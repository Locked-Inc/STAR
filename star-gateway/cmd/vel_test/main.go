// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// vel_test: send a single VelocityCommand and dump all frames received in
// a ~4 s window. Useful for diagnosing the command path without the estop /
// telemetry parsing that sensor_dump does.
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

func main() {
	port, err := serial.Open("/dev/ttyACM0", &serial.Mode{
		BaudRate: 115200, DataBits: 8, Parity: serial.NoParity, StopBits: serial.OneStopBit,
	})
	if err != nil {
		log.Fatalf("open: %v", err)
	}
	defer port.Close()
	port.SetReadTimeout(50 * time.Millisecond)

	// Drain 500 ms of boot noise
	tmp := make([]byte, 4096)
	drainDeadline := time.Now().Add(500 * time.Millisecond)
	for time.Now().Before(drainDeadline) {
		port.Read(tmp)
	}

	// Send RESET first to sync session sequence
	enc0 := frame.NewEncoder()
	resetFrame := &frame.Frame{Type: frame.FrameTypeReset, Header: frame.Header{Sequence: 0}}
	if rbs, rerr := enc0.Encode(resetFrame); rerr == nil {
		port.Write(rbs)
	}
	time.Sleep(150 * time.Millisecond)

	// Firmware decodes the frame payload as SetVelocityRequest directly
	// (not wrapped in WireMessage). See comm_task.c internal_handle_velocity_command.
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{RequestId: "vel_test"},
		Command: &starv1.VelocityCommand{
			FrontLeftVelocityMps:  0.15,
			FrontRightVelocityMps: 0.15,
			BackLeftVelocityMps:   0.15,
			BackRightVelocityMps:  0.15,
			Sequence:              1,
			TimestampUs:           time.Now().UnixMicro(),
		},
	}
	body, err := proto.Marshal(req)
	if err != nil {
		log.Fatalf("marshal: %v", err)
	}
	fmt.Printf("VelocityCommand payload (%d bytes)\n", len(body))

	enc := frame.NewEncoder()
	f := &frame.Frame{
		Type:    frame.FrameTypeCommand,
		Header:  frame.Header{Sequence: 1, Length: uint16(len(body)), Flags: frame.FlagNone},
		Payload: body,
	}
	bs, err := enc.Encode(f)
	if err != nil {
		log.Fatalf("encode: %v", err)
	}
	fmt.Printf("Wire frame (%d bytes): % X\n", len(bs), bs[:min(len(bs), 32)])
	if _, err := port.Write(bs); err != nil {
		log.Fatalf("write: %v", err)
	}

	// Capture 4 s
	buf := make([]byte, 65536)
	rxTotal := 0
	deadline := time.Now().Add(4 * time.Second)
	for time.Now().Before(deadline) && rxTotal < len(buf) {
		n, err := port.Read(buf[rxTotal:])
		if err != nil && err != io.EOF {
			break
		}
		rxTotal += n
	}
	fmt.Printf("\nCaptured %d bytes over 4 s\n", rxTotal)

	dec := frame.NewDecoder()
	typeCount := map[frame.Type]int{}
	for i := 0; i < rxTotal-1; i++ {
		if buf[i] != 0xAA || buf[i+1] != 0x55 {
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
		typeCount[f.Type]++
		if f.Type == frame.FrameTypeLogMessage {
			s := string(f.Payload)
			// Only print non-routine logs
			if s != "" && !contains(s, "IMU INT timeout") && !contains(s, "MDBG: ctrl tick") && !contains(s, "[MOTOR] CMD:") && !contains(s, "[MOTOR] DUTY") {
				fmt.Printf("LOG: %s", s)
			}
		} else if f.Type != frame.FrameTypeResponse {
			fmt.Printf("FRAME type=%v seq=%d len=%d\n", f.Type, f.Header.Sequence, f.Header.Length)
		}
	}
	fmt.Println("\nFrame type counts:")
	for t, n := range typeCount {
		fmt.Printf("  %v: %d\n", t, n)
	}
}

func contains(s, sub string) bool {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}
