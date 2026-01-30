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

// TestNewHeartbeatManager_Validation tests parameter validation.
func TestNewHeartbeatManager_Validation(t *testing.T) {
	callback := func() {}

	t.Run("ValidParameters", func(t *testing.T) {
		hm, err := NewHeartbeatManager(50*time.Millisecond, 200*time.Millisecond, callback)
		if err != nil {
			t.Fatalf("NewHeartbeatManager failed: %v", err)
		}
		if hm == nil {
			t.Fatal("NewHeartbeatManager returned nil")
		}
	})

	t.Run("PingIntervalTooLarge", func(t *testing.T) {
		_, err := NewHeartbeatManager(300*time.Millisecond, 200*time.Millisecond, callback)
		if err == nil {
			t.Error("NewHeartbeatManager should fail when pingInterval >= failureTimeout")
		}
	})

	t.Run("EqualIntervals", func(t *testing.T) {
		_, err := NewHeartbeatManager(100*time.Millisecond, 100*time.Millisecond, callback)
		if err == nil {
			t.Error("NewHeartbeatManager should fail when pingInterval == failureTimeout")
		}
	})

	t.Run("NilCallback", func(t *testing.T) {
		_, err := NewHeartbeatManager(50*time.Millisecond, 200*time.Millisecond, nil)
		if err == nil {
			t.Error("NewHeartbeatManager should fail with nil callback")
		}
	})
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

	hm, err := NewHeartbeatManager(10*time.Millisecond, 50*time.Millisecond, callback)
	if err != nil {
		t.Fatalf("NewHeartbeatManager failed: %v", err)
	}

	// Simulate frame reception (updates lastSeen)
	hm.OnFrameReceived()

	// Wait less than failure timeout
	time.Sleep(30 * time.Millisecond)

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

	hm, err := NewHeartbeatManager(10*time.Millisecond, 50*time.Millisecond, callback)
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
	case <-time.After(200 * time.Millisecond):
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

	hm, err := NewHeartbeatManager(10*time.Millisecond, 30*time.Millisecond, callback)
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
	time.Sleep(100 * time.Millisecond)

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

	hm, err := NewHeartbeatManager(10*time.Millisecond, 30*time.Millisecond, callback)
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
	time.Sleep(50 * time.Millisecond)

	mu.Lock()
	firstCount := callCount
	mu.Unlock()

	if firstCount != 1 {
		t.Fatalf("Expected 1 failure, got %d", firstCount)
	}

	// Simulate frame reception (recovery)
	hm.OnFrameReceived()

	// Wait for another timeout
	time.Sleep(50 * time.Millisecond)

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

	hm, err := NewHeartbeatManager(10*time.Millisecond, 50*time.Millisecond, callback)
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
	case <-time.After(100 * time.Millisecond):
		t.Error("Run did not return with nil TransportManager")
	}
}

// TestHeartbeatManager_ContextCancellation tests graceful shutdown.
func TestHeartbeatManager_ContextCancellation(t *testing.T) {
	callback := func() {}

	hm, err := NewHeartbeatManager(10*time.Millisecond, 50*time.Millisecond, callback)
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
	case <-time.After(100 * time.Millisecond):
		t.Error("Run did not exit after context cancellation")
	}
}
