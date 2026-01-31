// Package manager tests for heartbeat management.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"sync"
	"testing"
	"time"
)

// Test timing constants to avoid magic numbers
const (
	// Heartbeat intervals
	validPingInterval     = 50 * time.Millisecond
	validFailureTimeout   = 200 * time.Millisecond
	invalidPingInterval   = 300 * time.Millisecond
	invalidFailureTimeout = 200 * time.Millisecond
	equalInterval         = 100 * time.Millisecond

	// Test timeouts and waits
	shortPingInterval       = 10 * time.Millisecond
	shortFailureTimeout     = 50 * time.Millisecond
	veryShortFailureTimeout = 30 * time.Millisecond

	// Wait durations for test assertions
	waitBeforeTimeout     = 30 * time.Millisecond
	waitForMultipleCycles = 100 * time.Millisecond
	waitForFirstFailure   = 50 * time.Millisecond
	waitForSecondTimeout  = 50 * time.Millisecond
	maxWaitForCallback    = 200 * time.Millisecond
	maxWaitForExit        = 100 * time.Millisecond
)

// TestNewHeartbeatManager_Validation tests parameter validation.
func TestNewHeartbeatManager_Validation(t *testing.T) {
	callback := func() {}

	tests := []struct {
		name           string
		pingInterval   time.Duration
		failureTimeout time.Duration
		callback       func()
		wantErr        bool
		errDescription string
	}{
		{
			name:           "ValidParameters",
			pingInterval:   validPingInterval,
			failureTimeout: validFailureTimeout,
			callback:       callback,
			wantErr:        false,
		},
		{
			name:           "PingIntervalTooLarge",
			pingInterval:   invalidPingInterval,
			failureTimeout: invalidFailureTimeout,
			callback:       callback,
			wantErr:        true,
			errDescription: "pingInterval >= failureTimeout",
		},
		{
			name:           "EqualIntervals",
			pingInterval:   equalInterval,
			failureTimeout: equalInterval,
			callback:       callback,
			wantErr:        true,
			errDescription: "pingInterval == failureTimeout",
		},
		{
			name:           "NilCallback",
			pingInterval:   validPingInterval,
			failureTimeout: validFailureTimeout,
			callback:       nil,
			wantErr:        true,
			errDescription: "nil callback",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			hm, err := NewHeartbeatManager(tt.pingInterval, tt.failureTimeout, tt.callback)

			if tt.wantErr {
				if err == nil {
					t.Errorf("NewHeartbeatManager should fail with %s", tt.errDescription)
				}
			} else {
				if err != nil {
					t.Fatalf("NewHeartbeatManager failed: %v", err)
				}
				if hm == nil {
					t.Fatal("NewHeartbeatManager returned nil")
				}
			}
		})
	}
}

// TestHeartbeatManager_OnFrameReceived tests implicit heartbeat updates.
func TestHeartbeatManager_OnFrameReceived(t *testing.T) {
	failureCalled := false
	var mu sync.Mutex

	callback := func() {
		mu.Lock()
		failureCalled = true
		mu.Unlock()
	}

	hm, err := NewHeartbeatManager(shortPingInterval, shortFailureTimeout, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Simulate frame reception (updates lastSeen)
	hm.OnFrameReceived()

	// Wait less than failure timeout
	time.Sleep(waitBeforeTimeout)

	// Failure should NOT be called
	mu.Lock()
	if failureCalled {
		t.Error("Failure callback should not be called before timeout")
	}
	mu.Unlock()
}

// TestHeartbeatManager_TimeoutDetection tests failover trigger on timeout.
func TestHeartbeatManager_TimeoutDetection(t *testing.T) {
	failureCalled := false
	var mu sync.Mutex
	var wg sync.WaitGroup

	wg.Add(1)
	callback := func() {
		mu.Lock()
		defer mu.Unlock()
		if !failureCalled {
			failureCalled = true
			wg.Done()
		}
	}

	hm, err := NewHeartbeatManager(shortPingInterval, shortFailureTimeout, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Create mock TransportManager
	tm := NewTransportManager(DefaultConfig())

	// Start heartbeat monitor
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go hm.Run(ctx, tm)

	// Wait for timeout + buffer
	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()

	select {
	case <-done:
		// Success: callback was called
	case <-time.After(maxWaitForCallback):
		t.Error("Failure callback not called within expected time")
	}

	mu.Lock()
	if !failureCalled {
		t.Error("Failure callback should be called after timeout")
	}
	mu.Unlock()
}

// TestHeartbeatManager_FailureTriggeredOnce tests that failover is triggered only once per failure.
func TestHeartbeatManager_FailureTriggeredOnce(t *testing.T) {
	callCount := 0
	var mu sync.Mutex

	callback := func() {
		mu.Lock()
		callCount++
		mu.Unlock()
	}

	hm, err := NewHeartbeatManager(shortPingInterval, veryShortFailureTimeout, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Create mock TransportManager
	tm := NewTransportManager(DefaultConfig())

	// Start heartbeat monitor
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go hm.Run(ctx, tm)

	// Wait for multiple check cycles (should trigger only once)
	time.Sleep(waitForMultipleCycles)

	mu.Lock()
	if callCount != 1 {
		t.Errorf("Failure callback called %d times, expected 1", callCount)
	}
	mu.Unlock()
}

// TestHeartbeatManager_RecoveryAfterFailure tests that link can recover after failure.
func TestHeartbeatManager_RecoveryAfterFailure(t *testing.T) {
	callCount := 0
	var mu sync.Mutex

	callback := func() {
		mu.Lock()
		callCount++
		mu.Unlock()
	}

	hm, err := NewHeartbeatManager(shortPingInterval, veryShortFailureTimeout, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Create mock TransportManager
	tm := NewTransportManager(DefaultConfig())

	// Start heartbeat monitor
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go hm.Run(ctx, tm)

	// Wait for first failure
	time.Sleep(waitForFirstFailure)

	mu.Lock()
	firstCount := callCount
	mu.Unlock()

	if firstCount != 1 {
		t.Fatalf("Expected 1 failure, got %d", firstCount)
	}

	// Simulate frame reception (recovery)
	hm.OnFrameReceived()

	// Wait for another timeout
	time.Sleep(waitForSecondTimeout)

	mu.Lock()
	secondCount := callCount
	mu.Unlock()

	if secondCount != 2 {
		t.Errorf("Expected 2nd failure after recovery, got %d total", secondCount)
	}
}

// TestHeartbeatManager_NilTransportManager tests that Run handles nil TM gracefully.
func TestHeartbeatManager_NilTransportManager(t *testing.T) {
	callback := func() {
		t.Error("Callback should not be called with nil TransportManager")
	}

	hm, err := NewHeartbeatManager(shortPingInterval, shortFailureTimeout, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Run with nil TransportManager (should return immediately without panic)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	done := make(chan struct{})
	go func() {
		hm.Run(ctx, nil)
		close(done)
	}()

	select {
	case <-done:
		// Success: Run returned without panic
	case <-time.After(maxWaitForExit):
		t.Error("Run did not return with nil TransportManager")
	}
}

// TestHeartbeatManager_ContextCancellation tests graceful shutdown.
func TestHeartbeatManager_ContextCancellation(t *testing.T) {
	callback := func() {}

	hm, err := NewHeartbeatManager(shortPingInterval, shortFailureTimeout, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	tm := NewTransportManager(DefaultConfig())

	ctx, cancel := context.WithCancel(context.Background())

	done := make(chan struct{})
	go func() {
		hm.Run(ctx, tm)
		close(done)
	}()

	// Cancel context
	cancel()

	// Verify Run exits
	select {
	case <-done:
		// Success: Run exited
	case <-time.After(maxWaitForExit):
		t.Error("Run did not exit after context cancellation")
	}
}
