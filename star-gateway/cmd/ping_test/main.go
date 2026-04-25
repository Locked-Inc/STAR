// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package main

import (
	"fmt"
	"io"
	"log"
	"os"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"go.bug.st/serial"
)

func main() {
	port, err := serial.Open("/dev/ttyACM0", &serial.Mode{
		BaudRate: 115200,
		DataBits: 8,
		Parity:   serial.NoParity,
		StopBits: serial.OneStopBit,
	})
	if err != nil {
		log.Fatalf("open serial: %v", err)
	}
	defer port.Close()
	port.SetReadTimeout(time.Millisecond * 100)

	enc := frame.NewEncoder()
	resetFrame := &frame.Frame{
		Type: frame.FrameTypeReset,
		Header: frame.Header{
			Sequence: 0,
			Length:   0,
			Flags:    frame.FlagNone,
		},
		Payload: nil,
	}
	resetBytes, err := enc.Encode(resetFrame)
	if err != nil {
		log.Fatalf("encode reset: %v", err)
	}
	fmt.Printf("Sending RESET (%d bytes): % X\n", len(resetBytes), resetBytes)
	if _, err := port.Write(resetBytes); err != nil {
		log.Fatalf("write reset: %v", err)
	}
	time.Sleep(100 * time.Millisecond)

	pingFrame := &frame.Frame{
		Type: frame.FrameTypePing,
		Header: frame.Header{
			Sequence: 1,
			Length:   0,
			Flags:    frame.FlagNone,
		},
		Payload: nil,
	}
	pingBytes, err := enc.Encode(pingFrame)
	if err != nil {
		log.Fatalf("encode ping: %v", err)
	}
	fmt.Printf("Sending PING (%d bytes): % X\n", len(pingBytes), pingBytes)

	if _, err := port.Write(pingBytes); err != nil {
		log.Fatalf("write: %v", err)
	}

	deadline := time.Now().Add(3 * time.Second)
	buf := make([]byte, 16384)
	rxTotal := 0
	for time.Now().Before(deadline) && rxTotal < len(buf) {
		n, err := port.Read(buf[rxTotal:])
		if err != nil && err != io.EOF {
			log.Printf("read err: %v", err)
			break
		}
		rxTotal += n
	}
	fmt.Printf("Received %d bytes total\n", rxTotal)

	dec := frame.NewDecoder()
	frameCount := map[frame.Type]int{}
	for i := 0; i < rxTotal-1; i++ {
		if buf[i] != 0xAA || buf[i+1] != 0x55 {
			continue
		}
		if i+12 > rxTotal {
			break
		}
		end := rxTotal
		if i+1024 < end {
			end = i + 1024
		}
		f, err := dec.Decode(buf[i:end])
		if err != nil {
			continue
		}
		if f != nil {
			frameCount[f.Type]++
			if f.Type != frame.FrameTypeResponse {
				fmt.Printf("INTERESTING FRAME @%d: type=%v seq=%d len=%d payload=%q\n",
					i, f.Type, f.Header.Sequence, f.Header.Length, string(f.Payload))
			}
		}
	}
	fmt.Println("Frame summary:")
	for t, n := range frameCount {
		fmt.Printf("  %v: %d\n", t, n)
	}
	if rxTotal == 0 {
		os.Exit(2)
	}
}
