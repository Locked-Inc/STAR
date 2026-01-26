package manager

import (
	"context"
	"log"
	"time"
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
// For now, this is a stub that returns true (assumes transport is healthy).
// In Phase 2, this will be enhanced to:
//   - Check if USB device exists and can be opened
//   - Perform quick SPI connectivity test
func (hm *HealthMonitor) probeTransport(wrapper *TransportWrapper) bool {
	// TODO: Implement actual probing logic in Phase 2
	// For USB: Check if /dev/ttyACM* exists and is accessible
	// For SPI: Quick ping/connectivity test

	// For now, assume all transports are potentially healthy
	return true
}
