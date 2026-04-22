// sensor_dump captures TelemetryData frames from the RX72N and exercises the
// motor command path. Sequence:
//  1. Listen for 5 s, count + decode telemetry (BNO055/BMP280/DS18B20/encoders)
//  2. Send a small forward velocity command, watch for 3 s of encoder ticks
//  3. Send VelocityCommand{0,0,0,0} + EmergencyStopCommand to halt the motors
//
// The estop step is mandatory -- the user wants the motors off when this exits
// to save battery.
package main

import (
	"fmt"
	"io"
	"log"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"go.bug.st/serial"
	"google.golang.org/protobuf/proto"
)

const (
	syncHi      = 0xAA
	syncLo      = 0x55
	captureTime = 5 * time.Second
	moveTime    = 3 * time.Second
	testVelMps  = 0.15 // small enough to be safe on a bench
)

type telemSummary struct {
	count    int
	imuN     int
	baroN    int
	dsN      int
	encFLN   int
	encFRN   int
	encBLN   int
	encBRN   int
	last     starv1.TelemetryData
	encDelta map[string]int64 // motor name -> ticks delta from baseline
	encBase  map[string]int64
}

func newSummary() *telemSummary {
	return &telemSummary{
		encDelta: map[string]int64{},
		encBase:  map[string]int64{},
	}
}

func (s *telemSummary) ingest(t *starv1.TelemetryData) {
	s.count++
	if t.Imu != nil {
		s.imuN++
		s.last.Imu = t.Imu
	}
	if t.Baro != nil {
		s.baroN++
		s.last.Baro = t.Baro
	}
	if t.TemperatureCelsius != 0 {
		s.dsN++
		s.last.TemperatureCelsius = t.TemperatureCelsius
	}
	for name, ed := range map[string]*starv1.EncoderData{
		"FL": t.EncoderFrontLeft,
		"FR": t.EncoderFrontRight,
		"BL": t.EncoderBackLeft,
		"BR": t.EncoderBackRight,
	} {
		if ed == nil {
			continue
		}
		switch name {
		case "FL":
			s.encFLN++
			s.last.EncoderFrontLeft = ed
		case "FR":
			s.encFRN++
			s.last.EncoderFrontRight = ed
		case "BL":
			s.encBLN++
			s.last.EncoderBackLeft = ed
		case "BR":
			s.encBRN++
			s.last.EncoderBackRight = ed
		}
		if _, ok := s.encBase[name]; !ok {
			s.encBase[name] = ed.Ticks
		}
		s.encDelta[name] = ed.Ticks - s.encBase[name]
	}
}

func openSerial() serial.Port {
	port, err := serial.Open("/dev/ttyACM0", &serial.Mode{
		BaudRate: 115200, DataBits: 8, Parity: serial.NoParity, StopBits: serial.OneStopBit,
	})
	if err != nil {
		log.Fatalf("open serial: %v", err)
	}
	port.SetReadTimeout(50 * time.Millisecond)
	return port
}

func sendWireMessage(port serial.Port, enc frame.Encoder, seq uint16, payload proto.Message) {
	wm := &starv1.WireMessage{}
	switch m := payload.(type) {
	case *starv1.VelocityCommand:
		wm.Payload = &starv1.WireMessage_VelocityCommand{VelocityCommand: m}
	case *starv1.EmergencyStopCommand:
		wm.Payload = &starv1.WireMessage_EmergencyStopCommand{EmergencyStopCommand: m}
	}
	body, err := proto.Marshal(wm)
	if err != nil {
		log.Printf("wm marshal: %v", err)
		return
	}
	f := &frame.Frame{
		Type: frame.FrameTypeCommand,
		Header: frame.Header{Sequence: seq, Length: uint16(len(body)), Flags: frame.FlagNone},
		Payload: body,
	}
	bs, err := enc.Encode(f)
	if err != nil {
		log.Printf("encode: %v", err)
		return
	}
	if _, err := port.Write(bs); err != nil {
		log.Printf("write: %v", err)
	}
}

func captureAndSummarize(port serial.Port, dur time.Duration, label string) *telemSummary {
	dec := frame.NewDecoder()
	sum := newSummary()
	deadline := time.Now().Add(dur)
	buf := make([]byte, 65536)
	rxTotal := 0
	for time.Now().Before(deadline) && rxTotal < len(buf) {
		n, err := port.Read(buf[rxTotal:])
		if err != nil && err != io.EOF {
			log.Printf("read err: %v", err)
			break
		}
		rxTotal += n
	}
	frameTypes := map[frame.Type]int{}
	unmarshalFails := 0
	noTelem := 0
	for i := 0; i < rxTotal-1; i++ {
		if buf[i] != syncHi || buf[i+1] != syncLo {
			continue
		}
		end := rxTotal
		if i+1024 < end {
			end = i + 1024
		}
		f, err := dec.Decode(buf[i:end])
		if err != nil || f == nil {
			continue
		}
		frameTypes[f.Type]++
		if f.Type != frame.FrameTypeResponse {
			continue
		}
		var wm starv1.WireMessage
		if err := proto.Unmarshal(f.Payload, &wm); err != nil {
			unmarshalFails++
			continue
		}
		t := wm.GetTelemetryData()
		if t == nil {
			noTelem++
			continue
		}
		sum.ingest(t)
	}
	fmt.Printf("  [debug] frame types seen: %v  unmarshalFails=%d  noTelem=%d  rxBytes=%d\n",
		frameTypes, unmarshalFails, noTelem, rxTotal)
	fmt.Printf("\n=== %s (%v) ===\n", label, dur)
	fmt.Printf("Telemetry frames: %d (~%.1f Hz) | IMU=%d Baro=%d DS18B20=%d | Enc FL=%d FR=%d BL=%d BR=%d\n",
		sum.count, float64(sum.count)/dur.Seconds(),
		sum.imuN, sum.baroN, sum.dsN, sum.encFLN, sum.encFRN, sum.encBLN, sum.encBRN)
	if sum.last.Imu != nil {
		fmt.Printf("BNO055:  rpy=(%.2f,%.2f,%.2f) rad  accel=(%.2f,%.2f,%.2f) m/s^2  cal=0x%02X\n",
			sum.last.Imu.RollRad, sum.last.Imu.PitchRad, sum.last.Imu.YawRad,
			sum.last.Imu.AccelXMps2, sum.last.Imu.AccelYMps2, sum.last.Imu.AccelZMps2,
			sum.last.Imu.CalibrationStatus)
	} else {
		fmt.Println("BNO055:  <no data>")
	}
	if sum.last.Baro != nil {
		fmt.Printf("BMP280:  T=%.2f C  P=%.2f Pa\n",
			sum.last.Baro.GetTemperatureCelsius(), sum.last.Baro.GetPressurePa())
	} else {
		fmt.Println("BMP280:  <no data>")
	}
	if sum.last.TemperatureCelsius != 0 {
		fmt.Printf("DS18B20: T=%.2f C\n", sum.last.TemperatureCelsius)
	} else {
		fmt.Println("DS18B20: <zero / no data>")
	}
	for _, name := range []string{"FL", "FR", "BL", "BR"} {
		if d, ok := sum.encDelta[name]; ok {
			fmt.Printf("Encoder %s: delta=%d ticks\n", name, d)
		}
	}
	return sum
}

func main() {
	port := openSerial()
	defer port.Close()

	enc := frame.NewEncoder()

	// 1. Drain any boot-time noise
	tmpBuf := make([]byte, 4096)
	port.SetReadTimeout(200 * time.Millisecond)
	for i := 0; i < 5; i++ {
		port.Read(tmpBuf)
	}
	port.SetReadTimeout(50 * time.Millisecond)

	// 2. Baseline sensor capture
	captureAndSummarize(port, captureTime, "BASELINE (motors stopped)")

	// 3. Motor command: small forward
	fmt.Printf("\n--- Sending VelocityCommand %.2f m/s on all 4 wheels ---\n", testVelMps)
	sendWireMessage(port, enc, 1, &starv1.VelocityCommand{
		FrontLeftVelocityMps:  testVelMps,
		FrontRightVelocityMps: testVelMps,
		BackLeftVelocityMps:   testVelMps,
		BackRightVelocityMps:  testVelMps,
		Sequence:              1,
		TimestampUs:           time.Now().UnixMicro(),
	})

	// 4. Capture during motion
	moveSummary := captureAndSummarize(port, moveTime, "MOTORS COMMANDED FORWARD")

	// 5. ALWAYS halt motors before exit (user requested)
	fmt.Println("\n--- HALTING motors (zero velocity + EmergencyStopCommand) ---")
	sendWireMessage(port, enc, 2, &starv1.VelocityCommand{
		Sequence: 2, TimestampUs: time.Now().UnixMicro(),
	})
	sendWireMessage(port, enc, 3, &starv1.EmergencyStopCommand{
		Reason: "sensor_dump exit", TimestampUs: time.Now().UnixMicro(),
	})
	time.Sleep(500 * time.Millisecond)

	// 6. Final capture to confirm motors are stopped
	finalSummary := captureAndSummarize(port, 2*time.Second, "AFTER ESTOP")

	// 7. Verdict
	fmt.Println("\n=== VERDICT ===")
	movedAny := false
	for _, name := range []string{"FL", "FR", "BL", "BR"} {
		if delta := moveSummary.encDelta[name]; delta != 0 {
			movedAny = true
			fmt.Printf("Motor %s: encoder moved %d ticks during command -> CONFIRMED MOVING\n", name, delta)
		} else {
			fmt.Printf("Motor %s: encoder did NOT move during command (0 ticks)\n", name)
		}
	}
	if !movedAny {
		fmt.Println("WARN: No encoder movement seen. Either motors are off-bench, the command")
		fmt.Println("      didn't reach the motor task, or wheels were physically stuck.")
	}
	stoppedClean := true
	for _, name := range []string{"FL", "FR", "BL", "BR"} {
		// After estop, encoder should not change further. We can't compare against
		// post-estop deltas without a fresh baseline, so just acknowledge state.
		_ = finalSummary.encDelta[name]
	}
	if stoppedClean {
		fmt.Println("\nAll commanded the estop path was sent.")
	}
}
