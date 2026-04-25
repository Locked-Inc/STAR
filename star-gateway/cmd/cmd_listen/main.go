// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Just listen, no commands sent. Confirms baseline firmware liveness.
package main

import (
	"fmt"
	"io"
	"log"
	"strings"
	"time"

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

	buf := make([]byte, 1<<18)
	tot := 0
	deadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(deadline) && tot < len(buf) {
		n, err := port.Read(buf[tot:])
		if err != nil && err != io.EOF {
			fmt.Printf("!!! err after %d: %v\n", tot, err)
			break
		}
		tot += n
	}
	fmt.Printf("<<< captured %d bytes\n", tot)

	printable := func(b byte) bool {
		return (b >= 0x20 && b < 0x7F) || b == '\r' || b == '\n' || b == '\t'
	}
	var sb strings.Builder
	for i := 0; i < tot; i++ {
		if printable(buf[i]) {
			sb.WriteByte(buf[i])
		} else {
			sb.WriteByte('.')
		}
	}
	for _, line := range strings.Split(sb.String(), "\n") {
		line = strings.TrimRight(line, "\r ")
		if line == "" {
			continue
		}
		fmt.Println(line)
	}
}
