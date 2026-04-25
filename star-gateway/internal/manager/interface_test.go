// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package manager tests for interface compliance.
//
// This file verifies that the manager package correctly uses the harq.HARQ
// interface (not any legacy Transport interface) to prevent regressions.
//
// STAR Project - Texas A&M University
// February 2026
package manager

import (
	"bytes"
	"context"
	"encoding/binary"
	"sync/atomic"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
)

const (
	testPriorityUSB = 100
	testPrioritySPI = 50
	testTxSeqValue  = 42
	testRxSeqValue  = 24
	testTwoItems    = 2
	testZeroValue   = 0

	// firstReceiveCall is the receive invocation count that triggers the reset ACK response.
	firstReceiveCall = 1

	// resetAckSequence is the sequence number used in RESET_ACK frames.
	resetAckSequence = 0
)

// newResetCapableMock creates a MockHARQ that handles the reset handshake so
// tm.Start() completes instantly instead of waiting for ACK timeout x retries.
// Extra funcs (e.g. GetStateFunc, GetTxSeqFunc) can be set on the returned mock
// after creation.
func newResetCapableMock() *testutil.MockHARQ {
	var capturedSessionID atomic.Uint32
	var receiveCount atomic.Int32

	m := &testutil.MockHARQ{
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
	}

	m.SendWithTypeFunc = func(ctx context.Context, data []byte, ft frame.Type) error {
		if ft == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
			capturedSessionID.Store(binary.LittleEndian.Uint32(data[:SessionIDPayloadSize]))
		}
		return nil
	}

	m.ReceiveFunc = func(ctx context.Context) (*harq.ReceiveResult, error) {
		count := receiveCount.Add(1)
		if count == firstReceiveCall {
			// First receive: return RESET_ACK with matching session ID
			sid := capturedSessionID.Load()
			payload := make([]byte, SessionIDPayloadSize)
			binary.LittleEndian.PutUint32(payload, sid)
			return &harq.ReceiveResult{
				Payload: payload,
				Metadata: harq.FrameMetadata{
					Type:     frame.FrameTypeResetAck,
					Sequence: resetAckSequence,
				},
			}, nil
		}
		// Subsequent receives: block until context cancels
		<-ctx.Done()
		return nil, ctx.Err()
	}

	return m
}

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
	tm.RegisterTransport("usb", mockHARQ, testPriorityUSB)
	tm.RegisterTransport("spi", mockHARQ, testPrioritySPI)

	// Verify transports were registered
	transports := tm.GetAvailableTransports()
	if len(transports) != testTwoItems {
		t.Errorf("Expected %d transports registered, got %d", testTwoItems, len(transports))
	}
}

// TestTransportManager_MethodsReturnHARQTypes verifies that TransportManager
// methods return types from the harq package (harq.State, harq.ReceiveResult).
func TestTransportManager_MethodsReturnHARQTypes(t *testing.T) {
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	// Create mock with known return values and reset handshake support
	mockHARQ := newResetCapableMock()
	mockHARQ.GetTxSeqFunc = func() uint16 { return testTxSeqValue }
	mockHARQ.GetRxSeqFunc = func() uint16 { return testRxSeqValue }

	tm.RegisterTransport("usb", mockHARQ, testPriorityUSB)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() failed: %v", err)
	}
	defer tm.Stop()

	// Verify GetState returns harq.State type
	state := tm.GetState()
	if state != harq.StateIdle {
		t.Errorf("GetState() = %v, want %v", state, harq.StateIdle)
	}

	// Verify GetTxSequence returns uint16 matching harq interface
	txSeq := tm.GetTxSequence()
	if txSeq != testTxSeqValue {
		t.Errorf("GetTxSequence() = %d, want %d", txSeq, testTxSeqValue)
	}

	// Verify GetRxSequence returns uint16 matching harq interface
	rxSeq := tm.GetRxSequence()
	if rxSeq != testRxSeqValue {
		t.Errorf("GetRxSequence() = %d, want %d", rxSeq, testRxSeqValue)
	}
}

// TestTransportManager_SendAcceptsHARQPriority verifies that the Send method
// accepts harq.Priority parameters from the harq package.
func TestTransportManager_SendAcceptsHARQPriority(t *testing.T) {
	tests := []struct {
		name     string
		priority harq.Priority
	}{
		{name: "PriorityHigh", priority: harq.PriorityHigh},
		{name: "PriorityNormal", priority: harq.PriorityNormal},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			config := DefaultConfig()
			config.Mode = ModeAuto
			tm := NewTransportManager(config)

			sendCalled := false
			mockHARQ := newResetCapableMock()
			mockHARQ.SendFunc = func(ctx context.Context, data []byte, p ...harq.Priority) error {
				sendCalled = true
				if len(p) == 0 {
					t.Error("expected priority to be forwarded")
				} else if p[0] != tc.priority {
					t.Errorf("forwarded priority = %v, want %v", p[0], tc.priority)
				}
				return nil
			}

			tm.RegisterTransport("usb", mockHARQ, testPriorityUSB)

			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()

			if err := tm.Start(ctx); err != nil {
				t.Fatalf("Start() failed: %v", err)
			}
			defer tm.Stop()

			if err := tm.Send(ctx, []byte("test"), tc.priority); err != nil {
				t.Fatalf("Send() failed for %s: %v", tc.name, err)
			}

			if !sendCalled {
				t.Errorf("Send() did not call underlying HARQ mock for %s", tc.name)
			}
		})
	}
}

// TestTransportManager_ReceiveReturnsHARQResult verifies that Receive
// returns *harq.ReceiveResult from the harq package.
func TestTransportManager_ReceiveReturnsHARQResult(t *testing.T) {
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	expectedPayload := []byte("test payload")
	var receiveCount atomic.Int32
	var capturedSessionID atomic.Uint32

	mockHARQ := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, ft frame.Type) error {
			if ft == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
				capturedSessionID.Store(binary.LittleEndian.Uint32(data[:SessionIDPayloadSize]))
			}
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			count := receiveCount.Add(1)
			if count == firstReceiveCall {
				// First receive: RESET_ACK for handshake
				sid := capturedSessionID.Load()
				p := make([]byte, SessionIDPayloadSize)
				binary.LittleEndian.PutUint32(p, sid)
				return &harq.ReceiveResult{
					Payload:  p,
					Metadata: harq.FrameMetadata{Type: frame.FrameTypeResetAck},
				}, nil
			}
			// Second receive: the actual test payload (must be a data frame type)
			return &harq.ReceiveResult{
				Payload:  expectedPayload,
				Metadata: harq.FrameMetadata{Type: frame.FrameTypeResponse},
			}, nil
		},
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
	}

	tm.RegisterTransport("usb", mockHARQ, testPriorityUSB)

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

	// Verify payload matches
	if !bytes.Equal(result.Payload, expectedPayload) {
		t.Errorf("Received payload = %s, want %s", result.Payload, expectedPayload)
	}
}

// TestTransportManager_UsesHARQInterface verifies that TransportManager
// uses the harq.HARQ interface (not the old Transport interface).
// This test prevents regressions after removing the legacy Transport interface.
func TestTransportManager_UsesHARQInterface(t *testing.T) {
	// Create a reset-capable mock that implements harq.HARQ
	mockHARQ := newResetCapableMock()
	mockHARQ.GetTxSeqFunc = func() uint16 { return testZeroValue }
	mockHARQ.GetRxSeqFunc = func() uint16 { return testZeroValue }

	// Verify mock implements harq.HARQ
	var _ harq.HARQ = mockHARQ

	// Create config and manager
	config := DefaultConfig()
	config.Mode = ModeAuto
	tm := NewTransportManager(config)

	// Register the transport - this verifies TransportManager accepts harq.HARQ interface
	tm.RegisterTransport("usb", mockHARQ, testPriorityUSB)

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
