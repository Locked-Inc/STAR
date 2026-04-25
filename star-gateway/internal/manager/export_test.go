// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package manager

import "time"

// SetTransportHealthForTest is a test-only API that allows tests to manipulate
// transport health state without directly accessing internal fields.
//
// This method:
//   - Sets wrapper.Health.IsHealthy and wrapper.Available based on parameters
//   - When healthy = true:
//   - Resets wrapper.Health.ConsecutiveFailures = 0 (fresh recovery)
//   - Sets wrapper.Health.LastRecovery = time.Now() (match health_monitor.go semantics)
//   - When healthy = false:
//   - Preserves existing failure counter (simulates degraded state)
//
// This matches the recovery semantics in health_monitor.go (lines 90-92) where
// recovery detection sets IsHealthy, Available, ConsecutiveFailures=0, and LastRecovery.
//
// Returns false if the transport name is not registered.
//
// Example usage:
//
//	tm.SetTransportHealthForTest("primary", false, false)  // Mark as failed
//	hm.checkTransports(tm)                                 // Trigger recovery detection
//	pollForActiveTransport(t, tm, "primary", timeout)      // Verify failback
func (tm *TransportManager) SetTransportHealthForTest(name string, healthy, available bool) bool {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	wrapper, exists := tm.availableTransports[name]
	if !exists {
		return false
	}

	wrapper.Health.IsHealthy = healthy
	wrapper.Available = available

	if healthy {
		// Match recovery semantics from health_monitor.go
		wrapper.Health.ConsecutiveFailures = 0
		wrapper.Health.LastRecovery = time.Now()
	}

	return true
}
