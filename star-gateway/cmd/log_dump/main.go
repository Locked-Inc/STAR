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
		BaudRate: 115200, DataBits: 8, Parity: serial.NoParity, StopBits: serial.OneStopBit,
	})
	if err != nil {
		log.Fatalf("open: %v", err)
	}
	defer port.Close()
	port.SetReadTimeout(50 * time.Millisecond)

	dur := 12 * time.Second
	if len(os.Args) > 1 {
		fmt.Sscanf(os.Args[1], "%ds", &dur)
	}
	deadline := time.Now().Add(dur)
	buf := make([]byte, 262144)
	rxTotal := 0
	for time.Now().Before(deadline) && rxTotal < len(buf) {
		n, err := port.Read(buf[rxTotal:])
		if err != nil && err != io.EOF {
			break
		}
		rxTotal += n
	}
	fmt.Printf("Captured %d bytes\n", rxTotal)

	dec := frame.NewDecoder()
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
		if f.Type == frame.FrameTypeLogMessage {
			fmt.Printf("%s", string(f.Payload))
		}
	}
}
