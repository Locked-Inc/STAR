// Virtual RX72N - Hardware-in-the-Loop Simulator
//
// This program simulates the RX72N motor controller firmware for testing
// and development without physical hardware. It listens on a Unix domain
// socket and responds to Gateway commands with valid protocol frames.
//
// Protocol Implementation:
//  - Decodes incoming frames using frame.Decoder
//  - Validates sync word (0x55AA) and CRC-32
//  - Parses WireMessage protobuf payloads
//  - Recognizes VelocityCommand messages
//  - Generates valid TelemetryData at 100Hz
//  - Encodes outgoing frames with proper HARQ framing
//
// Usage:
//
//	go run ./cmd/virtual_rx72n/
//
// The simulator will:
//  1. Create a Unix socket at /tmp/star_rx72n.sock
//  2. Listen for connections from the Gateway
//  3. Parse incoming command frames
//  4. Send valid telemetry frames at 100Hz
//  5. Handle Ctrl+C gracefully
//
// STAR Project - Texas A&M University
// January 2026
package main

import (
	"errors"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

const (
	// SocketPath is the Unix domain socket path for Virtual RX72N.
	SocketPath = "/tmp/star_rx72n.sock"

	// MaxFrameSize is the maximum expected frame size (must match frame.MaxFrameSize).
	MaxFrameSize = frame.MaxFrameSize

	// SignalBufferSize is the buffer size for OS signal channel.
	SignalBufferSize = 1

	// readDeadlineTimeout is the timeout for socket read operations.
	readDeadlineTimeout = 100 * time.Millisecond

	// telemetryInterval is the interval for sending telemetry updates (100Hz).
	telemetryInterval = 10 * time.Millisecond

	// dummyBatteryVoltage is the simulated battery voltage in volts.
	dummyBatteryVoltage = 24.5

	// dummyMotorCurrent is the simulated motor current in milliamps.
	dummyMotorCurrent = 1500.0

	// dummyBatterySOC is the simulated battery state of charge in percent.
	dummyBatterySOC = 85
)

func main() {
	// Cleanup old socket file if it exists
	if err := os.Remove(SocketPath); err != nil && !os.IsNotExist(err) {
		log.Printf("Warning: failed to remove old socket: %v", err)
	}

	// Create Unix domain socket listener
	listener, err := net.Listen("unix", SocketPath)
	if err != nil {
		log.Fatalf("Failed to create virtual device: %v", err)
	}
	defer listener.Close()

	log.Println("Virtual RX72N Started. Waiting for Gateway...")
	log.Printf("   Socket: %s", SocketPath)
	log.Printf("   Max Frame Size: %d bytes", MaxFrameSize)

	// Channel to signal graceful shutdown
	done := make(chan struct{})

	// Handle Ctrl+C gracefully
	sigChan := make(chan os.Signal, SignalBufferSize)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-sigChan
		log.Println("\nShutting down Virtual RX72N...")
		// Signal main loop to stop
		close(done)
		// Close listener to unblock Accept()
		listener.Close()
		if err := os.Remove(SocketPath); err != nil {
			log.Printf("Warning: failed to remove socket: %v", err)
		}
	}()

	// Accept connections in a loop
	for {
		// Check if we are shutting down
		select {
		case <-done:
			return
		default:
		}

		conn, err := listener.Accept()
		if err != nil {
			// Check for shutdown-related errors
			select {
			case <-done:
				return
			default:
			}

			// If unexpected error, verify if it's a closed network connection
			if errors.Is(err, net.ErrClosed) {
				return
			}

			log.Printf("Connection error: %v", err)
			continue
		}
		log.Printf("Gateway connected from %v", conn.RemoteAddr())

		// Handle each connection in a goroutine
		go handleConnection(conn)
	}
}

// handleConnection processes messages from a single Gateway connection.
// It uses a synchronous request/response pattern to match SPI full-duplex behavior.
func handleConnection(conn net.Conn) {
	defer conn.Close()
	defer log.Println("Gateway disconnected")

	// Create frame encoder/decoder
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()

	// Buffer for reading incoming data
	buffer := make([]byte, MaxFrameSize)

	// Sequence number for outgoing frames
	var sequenceNum uint16
	var timestamp int64

	// Main loop: receive and respond synchronously
	for {
		// Set a read deadline to avoid indefinite blocking
		if err := conn.SetReadDeadline(time.Now().Add(time.Second)); err != nil {
			log.Printf("Failed to set read deadline: %v", err)
		}

		n, err := conn.Read(buffer)
		if err != nil {
			// Treat timeouts as transient; continue waiting for data
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				continue
			}
			// EOF is expected when connection closes
			if errors.Is(err, io.EOF) {
				return
			}
			log.Printf("Read error: %v", err)
			return
		}

		rxData := buffer[:n]

		// Try to decode the incoming frame
		decodedFrame, decodeErr := decoder.Decode(rxData)
		
		// Check if this is a dummy read (all zeros) - indicates Gateway is polling for telemetry
		isDummyRead := n > 0 && isAllZeros(rxData)

		if decodeErr == nil && !isDummyRead {
			// Valid command frame received - process it
			log.Printf("Received valid frame: SEQ=%d TYPE=%s LEN=%d",
				decodedFrame.Header.Sequence,
				decodedFrame.Header.Type.String(),
				decodedFrame.Header.Length)

			// Parse the protobuf payload
			var wireMsg starv1.WireMessage
			if err := proto.Unmarshal(decodedFrame.Payload, &wireMsg); err == nil {
				processWireMessage(&wireMsg)
			}
		} else if isDummyRead {
			log.Printf("Received dummy read (%d bytes), sending telemetry SEQ=%d", n, sequenceNum)
		}

		// Generate and send telemetry response
		// Must respond with exactly n bytes to match SPI full-duplex behavior
		responseData := generateTelemetryResponse(encoder, &sequenceNum, &timestamp, n)
		
		if _, err := conn.Write(responseData); err != nil {
			log.Printf("Write error: %v", err)
			return
		}
	}
}

// isAllZeros checks if a byte slice contains only zeros.
func isAllZeros(data []byte) bool {
	for _, b := range data {
		if b != 0 {
			return false
		}
	}
	return true
}

// generateTelemetryResponse creates a telemetry frame and pads/truncates to exactly responseLen bytes.
func generateTelemetryResponse(encoder *frame.DefaultEncoder, seqNum *uint16, timestamp *int64, responseLen int) []byte {
	// Generate dummy telemetry data
	telemetry := generateTelemetryData(*timestamp)
	*timestamp += int64(telemetryInterval.Microseconds())

	// Wrap in WireMessage
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: telemetry,
		},
	}

	// Marshal to protobuf
	payload, err := proto.Marshal(wireMsg)
	if err != nil {
		log.Printf("ERROR: Telemetry marshal failed: %v", err)
		// Return zeros on error
		return make([]byte, responseLen)
	}

	// Create frame
	frameToSend := &frame.Frame{
		Header: frame.Header{
			Sequence: *seqNum,
			Length:   uint16(len(payload)),
			Type:     frame.FrameTypeResponse,
			Flags:    frame.FlagNone,
		},
		Payload: payload,
	}
	
	log.Printf("Generating telemetry frame: SEQ=%d, payload_len=%d", *seqNum, len(payload))
	*seqNum++

	// Encode frame
	encodedFrame, err := encoder.Encode(frameToSend)
	if err != nil {
		log.Printf("ERROR: Frame encode failed: %v", err)
		// Return zeros on error
		return make([]byte, responseLen)
	}

	log.Printf("Encoded frame size: %d bytes, response_len=%d", len(encodedFrame), responseLen)

	// Pad or truncate to match expected response length
	if len(encodedFrame) < responseLen {
		// Pad with zeros
		padded := make([]byte, responseLen)
		copy(padded, encodedFrame)
		return padded
	} else if len(encodedFrame) > responseLen {
		// Truncate (should not happen in practice)
		log.Printf("WARNING: Frame too large (%d > %d), truncating", len(encodedFrame), responseLen)
		return encodedFrame[:responseLen]
	}
	
	return encodedFrame
}

// processWireMessage handles different types of WireMessage payloads.
func processWireMessage(wireMsg *starv1.WireMessage) {
	switch msg := wireMsg.Payload.(type) {
	case *starv1.WireMessage_VelocityCommand:
		cmd := msg.VelocityCommand
		log.Printf("VelocityCommand received: FL=%.3f m/s, FR=%.3f m/s, BL=%.3f m/s, BR=%.3f m/s, SEQ=%d",
			cmd.FrontLeftVelocityMps,
			cmd.FrontRightVelocityMps,
			cmd.BackLeftVelocityMps,
			cmd.BackRightVelocityMps,
			cmd.Sequence)

	case *starv1.WireMessage_EmergencyStopCommand:
		cmd := msg.EmergencyStopCommand
		log.Printf("EmergencyStopCommand received: reason=%s, hardware_stop=%v",
			cmd.Reason,
			cmd.EngageHardwareStop)

	case *starv1.WireMessage_MotorPowerCommand:
		cmd := msg.MotorPowerCommand
		log.Printf("MotorPowerCommand received: motor_id=%d, duty_cycle=%.1f%%",
			cmd.MotorId,
			cmd.DutyCyclePercent)

	default:
		// Silently ignore unknown messages or dummy reads
	}
}

// generateTelemetryData creates dummy telemetry data for simulation.
func generateTelemetryData(timestampUs int64) *starv1.TelemetryData {
	return &starv1.TelemetryData{
		Imu: &starv1.ImuData{
			PitchRad:       0.01,
			RollRad:        -0.02,
			YawRad:         1.57,
			AccelXMps2:     0.1,
			AccelYMps2:     0.0,
			AccelZMps2:     9.81,
			GyroXRadPerS:   0.0,
			GyroYRadPerS:   0.0,
			GyroZRadPerS:   0.0,
		},
		BatteryPercent:     float64(dummyBatterySOC),
		WifiSignalDbm:      -45,
		CpuUsagePercent:    25.5,
		TemperatureCelsius: 35.2,
		MotorLoadPercent:   15.0,
		TimestampUs:        timestampUs,
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       1000,
			VelocityMps: 0.5,
			TimestampUs: timestampUs,
		},
		EncoderFrontRight: &starv1.EncoderData{
			MotorId:     1,
			Ticks:       1020,
			VelocityMps: 0.51,
			TimestampUs: timestampUs,
		},
		EncoderBackLeft: &starv1.EncoderData{
			MotorId:     2,
			Ticks:       990,
			VelocityMps: 0.49,
			TimestampUs: timestampUs,
		},
		EncoderBackRight: &starv1.EncoderData{
			MotorId:     3,
			Ticks:       1010,
			VelocityMps: 0.50,
			TimestampUs: timestampUs,
		},
		EmergencyStop:      false,
		FaultFlags:         0,
		BatteryVoltageV:    dummyBatteryVoltage,
		BatterySocPercent:  dummyBatterySOC,
	}
}
