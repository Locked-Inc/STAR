package manager

import (
	"context"
	"encoding/binary"
	"log"
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
}

// NewHealthMonitor creates a new HealthMonitor with the given check interval.
func NewHealthMonitor(interval time.Duration) *HealthMonitor {
	if interval <= 0 {
		panic("HealthMonitor: interval must be positive")
	}
	return &HealthMonitor{
		interval: interval,
	}
}

// Run starts the health monitoring loop.
// It exits when ctx is cancelled.
func (hm *HealthMonitor) Run(ctx context.Context, tm *TransportManager) {
	ticker := time.NewTicker(hm.interval)
	defer ticker.Stop()

	log.Printf("HealthMonitor started (interval: %v)", hm.interval)

	for {
		select {
		case <-ctx.Done():
			log.Printf("HealthMonitor stopped")
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

		if healthy && !prevHealthy {
			log.Printf("Transport %s recovered (now healthy)", name)
		}
		tm.mu.Unlock()
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

// probeUSB checks if USB CDC device exists and is accessible.
//
// This is a non-intrusive check that:
//  1. Attempts to open the USB CDC device (/dev/ttyACM0)
//  2. Immediately closes it if successful
//
// Returns true if device is accessible, false otherwise.
// Does NOT interfere with active connections (only probes inactive transports).
func (hm *HealthMonitor) probeUSB() bool {
	// Use default USB CDC device path
	devicePath := transport.DefaultCDCDevice

	// Try to open the device (non-blocking)
	port, err := serial.Open(devicePath, &serial.Mode{
		BaudRate: transport.DefaultBaudRate,
	})
	if err != nil {
		// Device not available (unplugged, in use, permissions issue)
		return false
	}

	// Close immediately (we just wanted to check accessibility)
	if err := port.Close(); err != nil {
		log.Printf("WARNING: Failed to close USB probe port: %v", err)
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
	binary.BigEndian.PutUint32(payload, SPIProbeTestMarker)

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

	receivedCounter := binary.BigEndian.Uint32(result.Payload)
	return receivedCounter == SPIProbeTestMarker
}
