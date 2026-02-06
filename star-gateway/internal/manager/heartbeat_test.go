// Package manager tests for heartbeat management.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"encoding/binary"
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

	// PONG validation test constants
	testPongCounter    uint32 = 42
	testWrongCounter   uint32 = 99
	testPongPayloadLen        = 4
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

// --- PONG Validation Tests ---

// TestHeartbeatManager_PongValidation_MatchingCounter tests that a PONG with the
// correct counter resets consecutiveMisses, clears pendingPing, and returns true.
func TestHeartbeatManager_PongValidation_MatchingCounter(t *testing.T) {
	hm, err := NewHeartbeatManager(validPingInterval, validFailureTimeout, func() {})
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Simulate sending a PING (set lastPingSent and pendingPing)
	hm.mu.Lock()
	hm.lastPingSent = testPongCounter
	hm.pendingPing = true
	hm.consecutiveMisses = 2 // simulate prior misses
	hm.mu.Unlock()

	// Create matching PONG payload
	payload := make([]byte, testPongPayloadLen)
	binary.BigEndian.PutUint32(payload, testPongCounter)

	// Validate PONG
	matched := hm.OnPongReceived(payload)
	if !matched {
		t.Error("OnPongReceived should return true for matching counter")
	}

	// Verify state was reset
	hm.mu.Lock()
	defer hm.mu.Unlock()

	if hm.consecutiveMisses != 0 {
		t.Errorf("consecutiveMisses = %d, want 0", hm.consecutiveMisses)
	}
	if hm.pendingPing {
		t.Error("pendingPing should be false after valid PONG")
	}
	if hm.lastValidPong.IsZero() {
		t.Error("lastValidPong should be set")
	}
}

// TestHeartbeatManager_PongValidation_MismatchedCounter tests that a PONG with a
// wrong counter returns false and does NOT reset consecutiveMisses.
func TestHeartbeatManager_PongValidation_MismatchedCounter(t *testing.T) {
	hm, err := NewHeartbeatManager(validPingInterval, validFailureTimeout, func() {})
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Simulate sending a PING
	hm.mu.Lock()
	hm.lastPingSent = testPongCounter
	hm.pendingPing = true
	hm.consecutiveMisses = 2
	hm.mu.Unlock()

	// Create PONG with wrong counter
	payload := make([]byte, testPongPayloadLen)
	binary.BigEndian.PutUint32(payload, testWrongCounter)

	matched := hm.OnPongReceived(payload)
	if matched {
		t.Error("OnPongReceived should return false for mismatched counter")
	}

	// Verify consecutiveMisses NOT reset
	hm.mu.Lock()
	defer hm.mu.Unlock()

	if hm.consecutiveMisses != 2 {
		t.Errorf("consecutiveMisses = %d, want 2 (should not reset on mismatch)", hm.consecutiveMisses)
	}
	if !hm.pendingPing {
		t.Error("pendingPing should remain true on mismatch")
	}
}

// TestHeartbeatManager_PongValidation_WrongLength tests that PONG payloads with
// incorrect lengths are rejected.
func TestHeartbeatManager_PongValidation_WrongLength(t *testing.T) {
	hm, err := NewHeartbeatManager(validPingInterval, validFailureTimeout, func() {})
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	tests := []struct {
		name    string
		payload []byte
	}{
		{"TooShort", []byte{0x01, 0x02}},
		{"Empty", []byte{}},
		{"TooLong", make([]byte, 8)},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			matched := hm.OnPongReceived(tt.payload)
			if matched {
				t.Errorf("OnPongReceived should return false for %s payload (len=%d)",
					tt.name, len(tt.payload))
			}
		})
	}
}

// TestHeartbeatManager_ImplicitActivityPreventTimeout tests that non-PONG frames
// (via OnFrameReceived) still prevent timeout even without valid PONG responses.
func TestHeartbeatManager_ImplicitActivityPreventTimeout(t *testing.T) {
	failureCalled := false
	var mu sync.Mutex

	hm, err := NewHeartbeatManager(shortPingInterval, shortFailureTimeout, func() {
		mu.Lock()
		failureCalled = true
		mu.Unlock()
	})
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	tm := NewTransportManager(DefaultConfig())
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go hm.Run(ctx, tm)

	// Continuously send implicit frames (simulating active COMMAND/RESPONSE traffic)
	// for longer than failureTimeout
	for i := 0; i < 10; i++ {
		time.Sleep(shortPingInterval / 2)
		hm.OnFrameReceived()
	}

	mu.Lock()
	if failureCalled {
		t.Error("Failure should not trigger when implicit frames keep arriving")
	}
	mu.Unlock()
}

// TestHeartbeatManager_ConsecutiveMissCounter tests that unanswered PINGs accumulate
// consecutive misses and eventually trigger failover.
func TestHeartbeatManager_ConsecutiveMissCounter(t *testing.T) {
	failureCalled := false
	var mu sync.Mutex
	var wg sync.WaitGroup

	wg.Add(1)
	hm, err := NewHeartbeatManager(shortPingInterval, shortFailureTimeout, func() {
		mu.Lock()
		defer mu.Unlock()
		if !failureCalled {
			failureCalled = true
			wg.Done()
		}
	})
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	tm := NewTransportManager(DefaultConfig())
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go hm.Run(ctx, tm)

	// Wait for failover (caused by consecutive misses or timeout)
	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()

	select {
	case <-done:
		// Success: failover triggered
	case <-time.After(maxWaitForCallback):
		t.Error("Failover not triggered within expected time from consecutive misses")
	}

	// Verify consecutive misses accumulated
	hm.mu.Lock()
	if hm.consecutiveMisses < 1 {
		t.Errorf("consecutiveMisses = %d, expected >= 1", hm.consecutiveMisses)
	}
	hm.mu.Unlock()
}

// TestHeartbeatManager_ConsecutiveMissReset tests that a valid PONG resets the
// consecutive miss counter even after multiple prior misses.
func TestHeartbeatManager_ConsecutiveMissReset(t *testing.T) {
	hm, err := NewHeartbeatManager(validPingInterval, validFailureTimeout, func() {})
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Simulate 3 consecutive misses
	hm.mu.Lock()
	hm.consecutiveMisses = 3
	hm.pendingPing = true
	hm.lastPingSent = testPongCounter
	hm.mu.Unlock()

	// Send valid PONG that matches
	payload := make([]byte, testPongPayloadLen)
	binary.BigEndian.PutUint32(payload, testPongCounter)

	matched := hm.OnPongReceived(payload)
	if !matched {
		t.Fatal("OnPongReceived should return true for matching counter")
	}

	hm.mu.Lock()
	if hm.consecutiveMisses != 0 {
		t.Errorf("consecutiveMisses = %d, want 0 after valid PONG", hm.consecutiveMisses)
	}
	hm.mu.Unlock()
}

// TestHeartbeatManager_WDTTimingVerification verifies the mathematical relationship
// between PING interval, consecutive miss threshold, and the RX72N WDT timeout.
// This test ensures the timing constants provide adequate safety margin.
func TestHeartbeatManager_WDTTimingVerification(t *testing.T) {
	// Gateway detection time = PING interval * consecutive miss threshold
	gatewayDetectionMs := DefaultPingInterval.Milliseconds() * int64(DefaultConsecutiveMissThreshold)
	actualSafetyFactor := int64(wdtTimeoutApproxMs) / gatewayDetectionMs

	// Safety factor must be >= 3 to avoid false WDT triggers
	if actualSafetyFactor < 3 {
		t.Errorf("WDT safety factor too low: %d (need >= 3). "+
			"Gateway detects in %dms, WDT fires at %dms",
			actualSafetyFactor, gatewayDetectionMs, wdtTimeoutApproxMs)
	}

	// Verify documented safety factor matches calculated value
	if actualSafetyFactor != int64(wdtSafetyFactor) {
		t.Errorf("wdtSafetyFactor constant (%d) does not match calculated value (%d)",
			wdtSafetyFactor, actualSafetyFactor)
	}

	// Verify PING interval < failure timeout (existing invariant)
	if DefaultPingInterval >= DefaultFailureTimeout {
		t.Errorf("DefaultPingInterval (%v) must be < DefaultFailureTimeout (%v)",
			DefaultPingInterval, DefaultFailureTimeout)
	}

	// Verify consecutive miss detection aligns with failure timeout
	// (4 misses * 50ms = 200ms == DefaultFailureTimeout)
	expectedDetectionTime := DefaultPingInterval * time.Duration(DefaultConsecutiveMissThreshold)
	if expectedDetectionTime != DefaultFailureTimeout {
		t.Logf("NOTE: Consecutive miss detection time (%v) differs from failure timeout (%v). "+
			"Both paths exist for defense-in-depth.", expectedDetectionTime, DefaultFailureTimeout)
	}
}
