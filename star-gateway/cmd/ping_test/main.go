// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package main

import (
	"fmt"
	"io"
	"log/slog"
	"os"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"go.bug.st/serial"
)

func main() {
	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	})))

	port, err := serial.Open("/dev/ttyACM0", &serial.Mode{
		BaudRate: 115200,
		DataBits: 8,
		Parity:   serial.NoParity,
		StopBits: serial.OneStopBit,
	})
	if err != nil {
		slog.Error("open serial", "err", err)
		os.Exit(1)
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
		slog.Error("encode reset", "err", err)
		os.Exit(1)
	}
	fmt.Printf("Sending RESET (%d bytes): % X\n", len(resetBytes), resetBytes)
	if _, err := port.Write(resetBytes); err != nil {
		slog.Error("write reset", "err", err)
		os.Exit(1)
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
		slog.Error("encode ping", "err", err)
		os.Exit(1)
	}
	fmt.Printf("Sending PING (%d bytes): % X\n", len(pingBytes), pingBytes)

	if _, err := port.Write(pingBytes); err != nil {
		slog.Error("write", "err", err)
		os.Exit(1)
	}

	deadline := time.Now().Add(3 * time.Second)
	buf := make([]byte, 16384)
	rxTotal := 0
	for time.Now().Before(deadline) && rxTotal < len(buf) {
		n, err := port.Read(buf[rxTotal:])
		if err != nil && err != io.EOF {
			slog.Info("read err", "err", err)
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
