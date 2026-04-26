// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// ascii_test: send V lines at 10 Hz while reading E lines, single process.
package main

import (
	"bufio"
	"fmt"
	"log/slog"
	"os"
	"strings"
	"time"

	"go.bug.st/serial"
)

func main() {
	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	})))

	port, err := serial.Open("/dev/ttyACM0", &serial.Mode{
		BaudRate: 115200, DataBits: 8, Parity: serial.NoParity, StopBits: serial.OneStopBit,
	})
	if err != nil {
		slog.Error("open", "err", err)
		os.Exit(1)
	}
	defer port.Close()
	port.SetReadTimeout(50 * time.Millisecond)

	// Drain OS / kernel CDC buffer for 500 ms so the test sees only
	// post-write lines.
	drainBuf := make([]byte, 4096)
	deadlineDrain := time.Now().Add(500 * time.Millisecond)
	dropped := 0
	for time.Now().Before(deadlineDrain) {
		n, _ := port.Read(drainBuf)
		dropped += n
	}
	fmt.Printf("drained %d stale bytes\n", dropped)

	// Continuous V writer at 10 Hz with explicit error log
	stop := make(chan struct{})
	wcount := 0
	go func() {
		ticker := time.NewTicker(100 * time.Millisecond)
		defer ticker.Stop()
		for {
			select {
			case <-stop:
				return
			case <-ticker.C:
				n, werr := port.Write([]byte("V 0.40 0.40 0.40 0.40\n"))
				wcount++
				if werr != nil || n != 22 {
					fmt.Printf("WRITE #%d FAILED: n=%d err=%v\n", wcount, n, werr)
				} else if wcount == 1 || wcount%10 == 0 {
					fmt.Printf("WRITE #%d ok (%d bytes)\n", wcount, n)
				}
			}
		}
	}()

	// Read for 4 s, print all E and # lines
	r := bufio.NewReader(port)
	deadline := time.Now().Add(4 * time.Second)
	eCount := 0
	cCount := 0
	var firstE, lastE string
	for time.Now().Before(deadline) {
		line, err := r.ReadString('\n')
		if err != nil {
			continue
		}
		line = strings.TrimRight(line, "\r\n ")
		if strings.HasPrefix(line, "E ") {
			eCount++
			if firstE == "" {
				firstE = line
			}
			lastE = line
		} else if strings.HasPrefix(line, "#") {
			cCount++
			fmt.Println("CMT:", line)
		} else if line != "" {
			fmt.Println("OTHER:", line)
		}
	}

	close(stop)

	// Send stop
	port.Write([]byte("V 0 0 0 0\n"))

	fmt.Printf("\nE lines: %d  comment lines: %d\n", eCount, cCount)
	fmt.Println("first:", firstE)
	fmt.Println("last:", lastE)
}
