// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package manager

import (
	"context"
	"encoding/binary"
	"log/slog"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
	"go.bug.st/serial"
)

const (
	// SPIProbeTimeout is the timeout for SPI PING/PONG health probe.
	SPIProbeTimeout = 50 * time.Millisecond

	// SPIProbePayloadSize is the size of SPI probe payload (4-byte counter).
	SPIProbePayloadSize = 4

	// SPIProbeTestMarker is the test value sent in PING and expected in PONG.
	SPIProbeTestMarker = 0xDEADBEEF
)

// HealthMonitor periodically checks the health of inactive transports.
//
// It runs in a background goroutine and probes transports that are not currently active.
// This allows the TransportManager to detect when a previously failed transport has recovered.
type HealthMonitor struct {
	interval time.Duration
	usbVID   uint16
	usbPID   uint16
}

// NewHealthMonitor creates a new HealthMonitor with the given check interval and USB VID:PID.
//
// vid and pid are used by probeUSB to locate the current CDC ACM device via sysfs rather than
// assuming a fixed /dev/ttyUSB0 path. Pass zeros to fall back to the default device path.
func NewHealthMonitor(interval time.Duration, vid, pid uint16) *HealthMonitor {
	if interval <= 0 {
		panic("HealthMonitor: interval must be positive")
	}
	return &HealthMonitor{
		interval: interval,
		usbVID:   vid,
		usbPID:   pid,
	}
}

// Run starts the health monitoring loop.
// It exits when ctx is cancelled.
func (hm *HealthMonitor) Run(ctx context.Context, tm *TransportManager) {
	ticker := time.NewTicker(hm.interval)
	defer ticker.Stop()

	slog.Info("HealthMonitor started", "interval", hm.interval)

	for {
		select {
		case <-ctx.Done():
			slog.Info("HealthMonitor stopped")
			return

		case <-ticker.C:
			hm.checkTransports(tm)
		}
	}
}

// checkTransports probes all inactive transports.
func (hm *HealthMonitor) checkTransports(tm *TransportManager) {
	tm.mu.RLock()
	activeName := tm.activeTransportName
	transports := make(map[string]*TransportWrapper)
	for name, wrapper := range tm.availableTransports {
		if name != activeName {
			transports[name] = wrapper
		}
	}
	tm.mu.RUnlock()

	// Probe each inactive transport
	for name, wrapper := range transports {
		healthy := hm.probeTransport(wrapper)

		tm.mu.Lock()
		prevHealthy := wrapper.Health.IsHealthy
		wrapper.Health.IsHealthy = healthy
		wrapper.Available = healthy

		// If transport just recovered, update LastRecovery timestamp for failback damping
		// Detect Recovery: previously unhealthy and now healthy
		needFailover := false
		var recoveredName string
		var dampingDuration time.Duration
		if healthy && !prevHealthy {
			wrapper.Health.LastRecovery = time.Now()
			needFailover = true
			recoveredName = name
			dampingDuration = tm.config.FailbackDamping
		}
		tm.mu.Unlock()

		// Trigger failback asynchronously outside of lock to avoid blocking the monitor loop
		// while attemptFailover/executeSwitch pauseOperations and drainInflight
		if needFailover {
			slog.Info("transport recovered, triggering failover evaluation",
				"transport", recoveredName, "damping", dampingDuration)
			go tm.attemptFailover(FailureTypeGraceful)
		}
	}
}

// probeTransport performs a lightweight health check on a transport.
//
// This method dispatches to transport-specific probe implementations:
//   - USB: Checks if device exists and can be opened
//   - SPI: Sends PING and validates PONG response
//   - Other: Assumes healthy (for testing/mock transports)
//
// Returns true if the transport is healthy and available, false otherwise.
func (hm *HealthMonitor) probeTransport(wrapper *TransportWrapper) bool {
	switch wrapper.Name {
	case TransportNameUSB:
		return hm.probeUSB()
	case TransportNameSPI:
		return hm.probeSPI(wrapper)
	default:
		// For unknown/test transports, assume healthy
		// This allows tests to use mock transports without implementing probes
		return true
	}
}

// probeUSB checks if the USB CDC device exists and is accessible.
//
// This is a non-intrusive check that:
//  1. Locates the current ttyUSBN device via sysfs VID:PID scan (when configured)
//  2. Attempts to open the device and immediately closes it
//
// Using sysfs discovery (FindCDCDevice) means the probe succeeds even when the
// kernel assigned a different minor number after a disconnect/reconnect cycle
// (e.g., the device moved from /dev/ttyUSB0 to /dev/ttyUSB1).
//
// Returns true if device is accessible, false otherwise.
// Does NOT interfere with active connections (only probes inactive transports).
func (hm *HealthMonitor) probeUSB() bool {
	devicePath := transport.DefaultCDCDevice // fallback
	if hm.usbVID != 0 || hm.usbPID != 0 {
		if found, err := transport.FindCDCDevice(hm.usbVID, hm.usbPID); err == nil {
			devicePath = found
		}
	}

	port, err := serial.Open(devicePath, &serial.Mode{
		BaudRate: transport.DefaultBaudRate,
	})
	if err != nil {
		return false
	}

	if err := port.Close(); err != nil {
		slog.Warn("failed to close USB probe port", "error", err)
		return false
	}
	return true
}

// probeSPI performs a quick connectivity test on SPI transport.
//
// This method:
//  1. Sends a PING frame with a test payload (SPIProbeTestMarker)
//  2. Waits for PONG response (SPIProbeTimeout)
//  3. Validates that PONG echoes the same payload
//
// Returns true if PING/PONG exchange succeeds, false otherwise.
// Uses short timeout to avoid blocking health monitor for too long.
func (hm *HealthMonitor) probeSPI(wrapper *TransportWrapper) bool {
	// Use short timeout for probe (don't block health monitor)
	ctx, cancel := context.WithTimeout(context.Background(), SPIProbeTimeout)
	defer cancel()

	// Create PING payload with test marker
	payload := make([]byte, SPIProbePayloadSize)
	binary.LittleEndian.PutUint32(payload, SPIProbeTestMarker)

	// Send PING
	if err := wrapper.Transport.Send(ctx, payload); err != nil {
		// Send failed (transport not available)
		return false
	}

	// Wait for PONG
	result, err := wrapper.Transport.Receive(ctx)
	if err != nil {
		// Receive failed (no response or timeout)
		return false
	}

	// Validate PONG payload
	if len(result.Payload) != SPIProbePayloadSize {
		// Invalid response length
		return false
	}

	receivedCounter := binary.LittleEndian.Uint32(result.Payload)
	return receivedCounter == SPIProbeTestMarker
}
