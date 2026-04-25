// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package app tests for interface compliance.
//
// This file verifies that the gateway application correctly uses the harq.HARQ
// and transport.Device interfaces (not any legacy interfaces) to prevent regressions.
//
// STAR Project - Texas A&M University
// February 2026
package app

import (
	"context"
	"encoding/binary"
	"sync/atomic"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/link"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// firstReceiveTrigger is the receive invocation count that triggers the reset ACK response.
const firstReceiveTrigger = 1

// newResetCapableAppMock creates a MockHARQ that handles the reset handshake so
// tm.Start() completes instantly instead of waiting for ACK timeout x retries.
func newResetCapableAppMock() *testutil.MockHARQ {
	var capturedSessionID atomic.Uint32
	var receiveCount atomic.Int32

	m := &testutil.MockHARQ{
		GetStateFunc: func() harq.State { return harq.StateIdle },
	}
	m.SendWithTypeFunc = func(ctx context.Context, data []byte, ft frame.Type) error {
		if ft == frame.FrameTypeReset && len(data) >= manager.SessionIDPayloadSize {
			capturedSessionID.Store(binary.LittleEndian.Uint32(data[:manager.SessionIDPayloadSize]))
		}
		return nil
	}
	m.ReceiveFunc = func(ctx context.Context) (*harq.ReceiveResult, error) {
		count := receiveCount.Add(1)
		if count == firstReceiveTrigger {
			sid := capturedSessionID.Load()
			p := make([]byte, manager.SessionIDPayloadSize)
			binary.LittleEndian.PutUint32(p, sid)
			return &harq.ReceiveResult{
				Payload:  p,
				Metadata: harq.FrameMetadata{Type: frame.FrameTypeResetAck},
			}, nil
		}
		<-ctx.Done()
		return nil, ctx.Err()
	}
	return m
}

const (
	usbPriority = 100
	spiPriority = 50
)

// mockTransportDevice is a minimal mock for testing transport.Device interface usage.
type mockTransportDevice struct {
	transport.Device
}

func (m *mockTransportDevice) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	return make([]byte, len(txData)), nil
}

func (m *mockTransportDevice) Send(data []byte) (int, error) {
	return len(data), nil
}

func (m *mockTransportDevice) Receive(maxLen int) ([]byte, error) {
	return make([]byte, maxLen), nil
}

func (m *mockTransportDevice) IsOpen() bool {
	return true
}

func (m *mockTransportDevice) Open() error {
	return nil
}

func (m *mockTransportDevice) Close() error {
	return nil
}

// Compile-time assertion that mockTransportDevice implements transport.Device.
var _ transport.Device = (*mockTransportDevice)(nil)

// TestGateway_UsesTransportDeviceInterface verifies that the gateway
// uses the transport.Device interface (not any legacy Transport interface).
func TestGateway_UsesTransportDeviceInterface(t *testing.T) {
	testCases := []struct {
		name        string
		constructor func(transport.Device, *manager.SessionState) (harq.HARQ, error)
	}{
		{
			name: "CDCLink",
			constructor: func(device transport.Device, session *manager.SessionState) (harq.HARQ, error) {
				return link.NewCDCLink(device, session)
			},
		},
		{
			name: "SPILink",
			constructor: func(device transport.Device, session *manager.SessionState) (harq.HARQ, error) {
				return link.NewSPILink(device, session, nil)
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			mockTransport := &mockTransportDevice{}
			session := manager.NewSessionState()
			_, err := tc.constructor(mockTransport, session)
			if err != nil {
				t.Fatalf("constructor failed: %v", err)
			}
		})
	}
}

// TestGateway_LinksImplementHARQInterface verifies that both CDCLink and
// SPILink implement the harq.HARQ interface.
func TestGateway_LinksImplementHARQInterface(t *testing.T) {
	mockTransport := &mockTransportDevice{}
	session := manager.NewSessionState()

	// Test CDCLink
	t.Run("CDCLink implements harq.HARQ", func(t *testing.T) {
		cdcLink, err := link.NewCDCLink(mockTransport, session)
		if err != nil {
			t.Fatalf("NewCDCLink failed: %v", err)
		}

		// Compile-time check that CDCLink implements harq.HARQ
		var _ harq.HARQ = cdcLink

		// Runtime check of HARQ methods
		state := cdcLink.GetState()
		if state != harq.StateIdle {
			t.Errorf("GetState() = %v, want %v", state, harq.StateIdle)
		}

		txSeq := cdcLink.GetTxSequence()
		if txSeq != 0 {
			t.Fatalf("cdcLink.GetTxSequence() = %d, want 0", txSeq)
		}

		rxSeq := cdcLink.GetRxSequence()
		if rxSeq != 0 {
			t.Fatalf("cdcLink.GetRxSequence() = %d, want 0", rxSeq)
		}

		// Verify Send and Receive methods exist (part of harq.HARQ)
		ctx := context.Background()
		err = cdcLink.Send(ctx, []byte("test"))
		if err != nil {
			t.Errorf("Send() on CDCLink failed: %v", err)
		}

		cancelledCtx, cancel := context.WithCancel(context.Background())
		cancel()
		if err := cdcLink.Send(cancelledCtx, []byte("test")); err == nil {
			t.Error("expected cdcLink.Send with canceled context to fail")
		}
	})

	t.Run("CDCLink nil transport returns error", func(t *testing.T) {
		_, err := link.NewCDCLink(nil, session)
		if err == nil {
			t.Fatal("expected error from link.NewCDCLink(nil, session)")
		}
	})

	// Test SPILink
	t.Run("SPILink implements harq.HARQ", func(t *testing.T) {
		spiSession := manager.NewSessionState()
		spiLink, err := link.NewSPILink(mockTransport, spiSession, nil)
		if err != nil {
			t.Fatalf("NewSPILink failed: %v", err)
		}

		// Compile-time check that SPILink implements harq.HARQ
		var _ harq.HARQ = spiLink

		// Runtime check of HARQ methods
		state := spiLink.GetState()
		if state != harq.StateIdle {
			t.Errorf("GetState() = %v, want %v", state, harq.StateIdle)
		}

		txSeq := spiLink.GetTxSequence()
		if txSeq != 0 {
			t.Fatalf("spiLink.GetTxSequence() = %d, want 0", txSeq)
		}

		rxSeq := spiLink.GetRxSequence()
		if rxSeq != 0 {
			t.Fatalf("spiLink.GetRxSequence() = %d, want 0", rxSeq)
		}
	})

	t.Run("SPILink nil args return errors", func(t *testing.T) {
		if _, err := link.NewSPILink(nil, session, nil); err == nil {
			t.Fatal("expected error from link.NewSPILink(nil, session, nil)")
		}
		if _, err := link.NewSPILink(mockTransport, nil, nil); err == nil {
			t.Fatal("expected error from link.NewSPILink(mockTransport, nil, nil)")
		}
	})
}

// TestGateway_TransportManagerUsesHARQInterface verifies that
// TransportManager uses harq.HARQ interface (not legacy Transport).
func TestGateway_TransportManagerUsesHARQInterface(t *testing.T) {
	// Create a reset-capable mock HARQ
	mockHARQ := newResetCapableAppMock()

	// Verify mock implements harq.HARQ
	var _ harq.HARQ = mockHARQ

	// Create TransportManager
	config := manager.DefaultConfig()
	config.Mode = manager.ModeAuto
	tm := manager.NewTransportManager(config)

	// RegisterTransport should accept harq.HARQ interface
	tm.RegisterTransport("usb", mockHARQ, usbPriority)
	tm.RegisterTransport("spi", mockHARQ, spiPriority)

	// Start the manager
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() with harq.HARQ mock failed: %v", err)
	}
	defer tm.Stop()

	// Verify TransportManager methods return harq package types
	state := tm.GetState()
	if state != harq.StateIdle {
		t.Errorf("GetState() = %v, want %v", state, harq.StateIdle)
	}

	txSeq := tm.GetTxSequence()
	if txSeq != 0 {
		t.Errorf("GetTxSequence() = %d, want 0", txSeq)
	}

	rxSeq := tm.GetRxSequence()
	if rxSeq != 0 {
		t.Errorf("GetRxSequence() = %d, want 0", rxSeq)
	}
}

// TestGateway_NoLegacyTransportReferences verifies that the gateway
// does not reference any legacy Transport interface.
// This is a compile-time check - if legacy Transport existed and was used,
// this test wouldn't compile after its removal.
func TestGateway_NoLegacyTransportReferences(t *testing.T) {
	// This test exists to document that we've removed the legacy Transport interface.
	// If you see a compile error here after making changes, you may have
	// reintroduced a legacy interface. Verify you're using transport.Device
	// and harq.HARQ interfaces only.

	// Create a transport.Device
	var device transport.Device = &mockTransportDevice{}

	// Verify it has the expected Device interface methods
	_ = device.Transfer
	_ = device.Send
	_ = device.Receive
	_ = device.IsOpen
	_ = device.Open
	_ = device.Close

	// Create a harq.HARQ
	var harqHandler harq.HARQ = &testutil.MockHARQ{}

	// Verify it has the expected HARQ interface methods
	_ = harqHandler.Send
	_ = harqHandler.Receive
	_ = harqHandler.GetState
	_ = harqHandler.GetTxSequence
	_ = harqHandler.GetRxSequence
	_ = harqHandler.Reset
}
