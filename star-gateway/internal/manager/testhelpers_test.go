// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package manager

import (
	"testing"
	"time"
)

// TestHealthCheckInterval is the shared health check interval for tests.
// Unifies testHealthCheckInterval (health_monitor_test.go) and
// testHealthCheckIntervalFast (manager_test.go) into a single constant.
const TestHealthCheckInterval = 50 * time.Millisecond

// pollForActiveTransport polls tm.GetActiveTransport() until it matches the
// expected transport name or timeout occurs.
//
// This helper replaces duplicated polling loops across multiple tests
// (TestTransportManager_Failover, TestTransportManager_RegisterAndSelect,
// TestTransportManager_FailbackAfterRecovery) with a single reusable function.
//
// Parameters:
//   - t: Testing instance
//   - tm: TransportManager to poll
//   - want: Expected active transport name
//   - timeout: Maximum time to wait for the transport to become active
//
// The function uses transportPollInterval for the polling interval.
func pollForActiveTransport(t *testing.T, tm *TransportManager, want string, timeout time.Duration) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if tm.GetActiveTransport() == want {
			return
		}
		time.Sleep(transportPollInterval)
	}
	t.Errorf("Expected active transport %q, got %q after %v timeout", want, tm.GetActiveTransport(), timeout)
}
