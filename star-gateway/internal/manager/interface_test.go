// Package manager tests for interface compliance.
//
// This file verifies that the manager package correctly uses the harq.HARQ
// interface (not any legacy Transport interface) to prevent regressions.
//
// STAR Project - Texas A&M University
// February 2026
package manager

import (
	"context"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
)

// TestTransportManager_RegisterTransport_AcceptsHARQInterface verifies that
// RegisterTransport accepts the harq.HARQ interface type.
func TestTransportManager_RegisterTransport_AcceptsHARQInterface(t *testing.T) {
	config := DefaultConfig()
	tm := NewTransportManager(config)

	// Create a mock that implements harq.HARQ
	mockHARQ := &testutil.MockHARQ{
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
	}

	// Verify mock implements harq.HARQ
	var _ harq.HARQ = mockHARQ

	// This should compile and work - verifies RegisterTransport signature accepts harq.HARQ
	tm.RegisterTransport("usb", mockHARQ, 100)
	tm.RegisterTransport("spi", mockHARQ, 50)

	// Verify transports were registered
	transports := tm.GetAvailableTransports()
	if len(transports) != 2 {
		t.Errorf("Expected 2 transports registered, got %d", len(transports))
	}
}

// TestTransportManager_MethodsReturnHARQTypes verifies that TransportManager
// methods return types from the harq package (harq.State, harq.ReceiveResult).
func TestTransportManager_MethodsReturnHARQTypes(t *testing.T) {
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	// Create mock with known return values
	mockHARQ := &testutil.MockHARQ{
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
		GetTxSeqFunc: func() uint16 {
			return 42
		},
		GetRxSeqFunc: func() uint16 {
			return 24
		},
	}

	tm.RegisterTransport("usb", mockHARQ, 100)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() failed: %v", err)
	}
	defer tm.Stop()

	// Verify GetState returns harq.State type
	state := tm.GetState()
	_ = state // Should be harq.State type

	// Verify GetTxSequence returns uint16 matching harq interface
	txSeq := tm.GetTxSequence()
	if txSeq != 42 {
		t.Errorf("GetTxSequence() = %d, want 42", txSeq)
	}

	// Verify GetRxSequence returns uint16 matching harq interface
	rxSeq := tm.GetRxSequence()
	if rxSeq != 24 {
		t.Errorf("GetRxSequence() = %d, want 24", rxSeq)
	}
}

// TestTransportManager_SendAcceptsHARQPriority verifies that the Send method
// accepts harq.Priority parameters from the harq package.
func TestTransportManager_SendAcceptsHARQPriority(t *testing.T) {
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	sendCalled := false
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			sendCalled = true
			// Verify priority parameter type
			if len(p) > 0 {
				var _ harq.Priority = p[0]
			}
			return nil
		},
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
	}

	tm.RegisterTransport("usb", mockHARQ, 100)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() failed: %v", err)
	}
	defer tm.Stop()

	// Send with harq.PriorityHigh (from harq package, not legacy code)
	if err := tm.Send(ctx, []byte("test"), harq.PriorityHigh); err != nil {
		t.Fatalf("Send() with harq.PriorityHigh failed: %v", err)
	}

	if !sendCalled {
		t.Error("Send() did not call underlying HARQ mock")
	}
}

// TestTransportManager_ReceiveReturnsHARQResult verifies that Receive
// returns *harq.ReceiveResult from the harq package.
func TestTransportManager_ReceiveReturnsHARQResult(t *testing.T) {
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	expectedPayload := []byte("test payload")
	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			// Return harq.ReceiveResult type
			return &harq.ReceiveResult{
				Payload: expectedPayload,
			}, nil
		},
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
	}

	tm.RegisterTransport("usb", mockHARQ, 100)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() failed: %v", err)
	}
	defer tm.Stop()

	// Receive should return *harq.ReceiveResult
	result, err := tm.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive() failed: %v", err)
	}

	// Verify result type is *harq.ReceiveResult
	var _ *harq.ReceiveResult = result

	// Verify payload matches
	if string(result.Payload) != string(expectedPayload) {
		t.Errorf("Received payload = %s, want %s", result.Payload, expectedPayload)
	}
}

// TestTransportManager_UsesHARQInterface verifies that TransportManager
// uses the harq.HARQ interface (not the old Transport interface).
// This test prevents regressions after removing the legacy Transport interface.
func TestTransportManager_UsesHARQInterface(t *testing.T) {
	// Create a mock that implements harq.HARQ
	mockHARQ := &testutil.MockHARQ{
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
		GetTxSeqFunc: func() uint16 {
			return 0
		},
		GetRxSeqFunc: func() uint16 {
			return 0
		},
	}

	// Verify mock implements harq.HARQ
	var _ harq.HARQ = mockHARQ

	// Create config and manager
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	// Register the transport - this verifies TransportManager accepts harq.HARQ interface
	tm.RegisterTransport("usb", mockHARQ, 100)

	// Start the manager
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() with harq.HARQ mock failed: %v", err)
	}

	// Verify the mock is being used (state should match mock)
	if tm.GetState() != harq.StateIdle {
		t.Errorf("TransportManager state = %v, want StateIdle from mock", tm.GetState())
	}

	// Stop the manager
	if err := tm.Stop(); err != nil {
		t.Errorf("Stop() failed: %v", err)
	}
}
