// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

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

	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
)

// Test timing constants to avoid magic numbers
const (
	// Heartbeat intervals (dual-detection model: pingInterval may be > failureTimeout)
	validPingInterval   = 50 * time.Millisecond
	validFailureTimeout = 200 * time.Millisecond
	largePingInterval   = 1 * time.Second // Production-like: PING > failure timeout
	largeFailureTimeout = 200 * time.Millisecond

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
			name:           "PingIntervalLargerThanFailure",
			pingInterval:   largePingInterval,
			failureTimeout: largeFailureTimeout,
			callback:       callback,
			wantErr:        false,
			errDescription: "dual-detection: pingInterval > failureTimeout is valid",
		},
		{
			name:           "ZeroPingInterval",
			pingInterval:   0,
			failureTimeout: validFailureTimeout,
			callback:       callback,
			wantErr:        true,
			errDescription: "zero pingInterval",
		},
		{
			name:           "ZeroFailureTimeout",
			pingInterval:   validPingInterval,
			failureTimeout: 0,
			callback:       callback,
			wantErr:        true,
			errDescription: "zero failureTimeout",
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
	binary.LittleEndian.PutUint32(payload, testPongCounter)

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
	binary.LittleEndian.PutUint32(payload, testWrongCounter)

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
// consecutive misses and eventually trigger failover via the counter-based path.
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

	// Register a mock transport so PING sends succeed (pendingPing gets set).
	// Without a transport, SendWithType returns "no active transport" and
	// pendingPing is never set (failed sends don't count as misses).
	tm := NewTransportManager(DefaultConfig())
	mockTransport := &testutil.MockHARQ{}
	tm.RegisterTransport("mock", mockTransport, 10)

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

	// After counter-based failover, consecutiveMisses is reset to 0 to prevent
	// re-trigger storms when OnFrameReceived clears failureTriggered on recovery.
	hm.mu.Lock()
	if hm.consecutiveMisses != 0 {
		t.Errorf("consecutiveMisses = %d, expected 0 (reset after failover)", hm.consecutiveMisses)
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
	binary.LittleEndian.PutUint32(payload, testPongCounter)

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

// minWDTSafetyFactor is the minimum acceptable ratio of WDT timeout to detection time.
const minWDTSafetyFactor = 3

// TestHeartbeatManager_WDTTimingVerification verifies the mathematical relationship
// between the implicit failure timeout, check interval, and the RX72N WDT timeout.
//
// Dual-detection model:
//
//	Primary: Implicit timeout (DefaultFailureTimeout = 200ms) -- any frame resets timer
//	Check interval: failureTimeout / checkIntervalDivisor = 50ms
//	Worst-case detection: failureTimeout + checkInterval = 250ms
//	Secondary: Explicit PING (DefaultPingInterval = 1s) -- rare idle-link probe
//	Safety margin = WDT timeout / detection time = 1000ms / 200ms = 5x (best case)
func TestHeartbeatManager_WDTTimingVerification(t *testing.T) {
	// Primary detection time is the implicit failure timeout
	gatewayDetectionMs := DefaultFailureTimeout.Milliseconds()
	actualSafetyFactor := int64(wdtTimeoutApproxMs) / gatewayDetectionMs

	// Safety factor must be >= minWDTSafetyFactor to avoid false WDT triggers
	if actualSafetyFactor < minWDTSafetyFactor {
		t.Errorf("WDT safety factor too low: %d (need >= %d). "+
			"Gateway detects in %dms, WDT fires at %dms",
			actualSafetyFactor, minWDTSafetyFactor, gatewayDetectionMs, wdtTimeoutApproxMs)
	}

	// Verify documented safety factor matches calculated value
	if actualSafetyFactor != int64(wdtSafetyFactor) {
		t.Errorf("wdtSafetyFactor constant (%d) does not match calculated value (%d)",
			wdtSafetyFactor, actualSafetyFactor)
	}

	// Verify failure timeout is well below WDT timeout (at least 2x margin)
	if DefaultFailureTimeout.Milliseconds() > int64(wdtTimeoutApproxMs)/2 {
		t.Errorf("DefaultFailureTimeout (%v) must be < WDT/2 (%dms)",
			DefaultFailureTimeout, wdtTimeoutApproxMs/2)
	}

	// Verify check interval is small enough for timely detection
	checkInterval := DefaultFailureTimeout / checkIntervalDivisor
	worstCaseDetection := DefaultFailureTimeout + checkInterval
	if worstCaseDetection.Milliseconds() >= int64(wdtTimeoutApproxMs) {
		t.Errorf("Worst-case detection (%v) must be < WDT timeout (%dms)",
			worstCaseDetection, wdtTimeoutApproxMs)
	}

	// Verify both durations are positive
	if DefaultPingInterval <= 0 {
		t.Error("DefaultPingInterval must be > 0")
	}
	if DefaultFailureTimeout <= 0 {
		t.Error("DefaultFailureTimeout must be > 0")
	}

	// Verify consecutive miss threshold is at least 1
	if DefaultConsecutiveMissThreshold < 1 {
		t.Errorf("DefaultConsecutiveMissThreshold must be >= 1, got %d",
			DefaultConsecutiveMissThreshold)
	}
}

// TestHeartbeatManager_PingTimerIndependentOfLastSeen verifies that the PING fires
// based on lastValidPong even when lastSeen is kept fresh by OnFrameReceived calls.
//
// This is the key property that enables zombie-link detection: stale OS-buffered frames
// (ACK/NACK) keep lastSeen fresh -- preventing the implicit 200ms timeout from firing --
// while lastValidPong ages independently, eventually triggering the PING. Without this
// independence, the counter-based path is permanently unreachable because lastSeen is
// always refreshed before implicitElapsed exceeds failureTimeout.
//
// Setup: lastSeen = now (fresh, no implicit timeout), lastValidPong = past (old, triggers
// PING), pendingPing = true (simulates a prior unanswered PING). A single check() call
// should increment consecutiveMisses and trigger counter-based failover, proving the PING
// condition used lastValidPong, not lastSeen.
func TestHeartbeatManager_PingTimerIndependentOfLastSeen(t *testing.T) {
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

	// Age lastValidPong well past pingInterval, but keep lastSeen fresh so the
	// implicit timeout cannot fire. With old code (elapsed = time.Since(lastSeen)),
	// neither block would trigger here: lastSeen is fresh (< failureTimeout) and
	// lastSeen is fresh (< pingInterval too). With the new code, pingElapsed is based
	// on lastValidPong and exceeds pingInterval independently.
	hm.mu.Lock()
	hm.lastValidPong = time.Now().Add(-5 * shortPingInterval) // far past pingInterval
	hm.lastSeen = time.Now()                                  // fresh: implicit timeout won't fire
	hm.pendingPing = true                                     // simulate prior unanswered PING
	hm.lastPingSent = testPongCounter
	hm.consecutiveMisses = DefaultConsecutiveMissThreshold - 1 // one more miss triggers failover
	hm.mu.Unlock()

	// No transport registered: sendPing will fail (if we reach it). But miss counting
	// occurs before sendPing, so the counter-based path still fires when pending=true.
	tm := NewTransportManager(DefaultConfig())

	hm.check(context.Background(), tm)

	// Counter-based failover should have triggered because pingElapsed > pingInterval
	// and pendingPing was true, incrementing consecutiveMisses to >= threshold.
	mu.Lock()
	triggered := failureCalled
	mu.Unlock()

	if !triggered {
		t.Error("PING timer (lastValidPong-based) should trigger counter-based failover " +
			"even when lastSeen is fresh -- proves pingElapsed is independent of lastSeen")
	}

	// consecutiveMisses is reset to 0 after counter-based failover.
	hm.mu.Lock()
	misses := hm.consecutiveMisses
	hm.mu.Unlock()

	if misses != 0 {
		t.Errorf("consecutiveMisses = %d after counter-based failover, want 0 (reset on trigger)", misses)
	}
}

// TestHeartbeatManager_ZombieLinkCounterDetection exercises the full zombie-link scenario
// end-to-end using the Run goroutine: continuous OnFrameReceived calls keep the implicit
// timeout from firing, but the PING timer (based on lastValidPong) ages independently,
// eventually triggering counter-based failover.
//
// A mock transport is registered so that PING sends succeed and pendingPing is set.
// After one PING interval passes without a PONG response, the next check sees
// pendingPing=true, increments consecutiveMisses to threshold, and calls onLinkFailed.
func TestHeartbeatManager_ZombieLinkCounterDetection(t *testing.T) {
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

	// Register a mock transport so PING sends succeed (pendingPing gets set).
	// This is required: failed sends do not set pendingPing, so misses would never
	// accumulate and counter-based failover would never trigger.
	tm := NewTransportManager(DefaultConfig())
	mockTransport := &testutil.MockHARQ{}
	tm.RegisterTransport("mock", mockTransport, 10)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go hm.Run(ctx, tm)

	// Continuously refresh lastSeen to prevent the implicit 200ms timeout from firing.
	// This simulates stale OS-buffered ACK/NACK frames arriving while the firmware is
	// actually dead. Without this, the implicit timeout would fire first and the test
	// would not isolate the zombie-link (counter-based) detection path.
	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()

	// Goroutine keeps lastSeen fresh while we wait for counter-based failover.
	refreshCtx, stopRefresh := context.WithCancel(context.Background())
	defer stopRefresh()
	go func() {
		ticker := time.NewTicker(shortPingInterval / 2)
		defer ticker.Stop()
		for {
			select {
			case <-refreshCtx.Done():
				return
			case <-ticker.C:
				hm.OnFrameReceived()
			}
		}
	}()

	select {
	case <-done:
		// Success: counter-based failover triggered despite continuous implicit-timer resets.
	case <-time.After(maxWaitForCallback):
		t.Error("Zombie-link counter-based failover not triggered within expected time; " +
			"PING timer must fire independently of lastSeen")
	}

	mu.Lock()
	if !failureCalled {
		t.Error("Failure callback should have been called via counter-based PING path")
	}
	mu.Unlock()
}
