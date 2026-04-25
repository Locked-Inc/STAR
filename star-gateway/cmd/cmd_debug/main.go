// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Combined send + raw dump tool.
// Sends RESET, then a VelocityCommand SetVelocityRequest, then dumps the
// raw stream for 4 s and prints any printable text fragments verbatim
// (so uart_debug_puts traces are visible) plus a hex dump of all bytes.
package main

import (
	"fmt"
	"io"
	"log"
	"strings"
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

	// No initial drain -- preserve early boot output for diagnostics.

	// RESET (hard-fail on write error so dead UART surfaces immediately)
	enc := frame.NewEncoder()
	rf := &frame.Frame{Type: frame.FrameTypeReset, Header: frame.Header{Sequence: 0}}
	rb, err := enc.Encode(rf)
	if err != nil {
		log.Fatalf("encode RESET: %v", err)
	}
	if _, werr := port.Write(rb); werr != nil {
		log.Fatalf("write RESET: %v", werr)
	}
	time.Sleep(150 * time.Millisecond)

	// VelocityCommand
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{RequestId: "dbg"},
		Command: &starv1.VelocityCommand{
			FrontLeftVelocityMps:  0.5,
			FrontRightVelocityMps: 0.5,
			BackLeftVelocityMps:   0.5,
			BackRightVelocityMps:  0.5,
			Sequence:              1,
			TimestampUs:           time.Now().UnixMicro(),
		},
	}
	body, _ := proto.Marshal(req)
	fmt.Printf(">>> velocity payload (%d B), sending at 5 Hz starting seq=0\n", len(body))

	/* Send velocity command at 200 ms intervals (5 Hz) starting at seq=0
	 * to match firmware session state after RESET. 200 ms keeps the
	 * comm-link watchdog fed (200 ms threshold). Run total 8 s. */
	go func() {
		seq := uint16(0)
		ticker := time.NewTicker(200 * time.Millisecond)
		defer ticker.Stop()
		end := time.Now().Add(8 * time.Second)
		for t := range ticker.C {
			if t.After(end) {
				return
			}
			req2 := proto.Clone(req).(*starv1.SetVelocityRequest)
			req2.Command.Sequence = uint32(seq)
			req2.Command.TimestampUs = t.UnixMicro()
			body2, _ := proto.Marshal(req2)
			f2 := &frame.Frame{
				Type: frame.FrameTypeCommand,
				Header: frame.Header{
					Sequence: seq, Length: uint16(len(body2)), Flags: frame.FlagNone,
				},
				Payload: body2,
			}
			enc3 := frame.NewEncoder()
			bs2, encErr := enc3.Encode(f2)
			if encErr != nil {
				log.Printf("encode seq=%d: %v", seq, encErr)
				return
			}
			if _, werr := port.Write(bs2); werr != nil {
				log.Printf("write seq=%d: %v", seq, werr)
				return
			}
			seq++
		}
	}()

	// Capture 9 s raw
	buf := make([]byte, 1<<19)
	tot := 0
	deadline := time.Now().Add(9 * time.Second)
	for time.Now().Before(deadline) && tot < len(buf) {
		n, err := port.Read(buf[tot:])
		if err != nil && err != io.EOF {
			fmt.Printf("!!! read error after %d bytes: %v\n", tot, err)
			break
		}
		tot += n
	}
	fmt.Printf("<<< captured %d bytes\n", tot)

	/* Decode framed LOG_MESSAGE frames cleanly (same logic as vel_test).
	 * Suppresses raw byte interleaving artifacts. */
	dec := frame.NewDecoder()
	typeCount := map[frame.Type]int{}
	logCount := 0
	suppressed := 0
	syncHits := 0
	decodeFailures := 0
	for i := 0; i < tot-1; i++ {
		if buf[i] != 0xAA || buf[i+1] != 0x55 {
			continue
		}
		syncHits++
		end := tot
		if i+1024 < end {
			end = i + 1024
		}
		f, err := dec.Decode(buf[i:end])
		if err != nil || f == nil {
			decodeFailures++
			continue
		}
		typeCount[f.Type]++
		if f.Type == frame.FrameTypeLogMessage {
			s := strings.TrimRight(string(f.Payload), "\r\n ")
			if strings.Contains(s, "IMU INT timeout") {
				suppressed++
				continue
			}
			logCount++
			fmt.Println("LOG:", s)
		}
	}
	fmt.Println("--- summary ---")
	for t, n := range typeCount {
		fmt.Printf("  %v: %d\n", t, n)
	}
	fmt.Printf("  decoded %d log lines (%d IMU-INT lines suppressed)\n",
		logCount, suppressed)
	fmt.Printf("  sync-word hits: %d, decode failures: %d\n",
		syncHits, decodeFailures)
}
