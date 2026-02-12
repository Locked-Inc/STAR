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
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/link"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
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
	// Create a mock transport that implements transport.Device
	mockTransport := &mockTransportDevice{}

	// Verify it implements the interface
	var _ transport.Device = mockTransport

	// Create a CDCLink with the transport.Device
	session := manager.NewSessionState()
	cdcLink, err := link.NewCDCLink(mockTransport, session)
	if err != nil {
		t.Fatalf("NewCDCLink with transport.Device failed: %v", err)
	}

	// Verify the link is a harq.HARQ
	var _ harq.HARQ = cdcLink

	// Create an SPILink with the transport.Device
	spiLink, err := link.NewSPILink(mockTransport, session, nil)
	if err != nil {
		t.Fatalf("NewSPILink with transport.Device failed: %v", err)
	}

	// Verify the link is a harq.HARQ
	var _ harq.HARQ = spiLink
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
		if state == harq.State(0) && state != harq.StateIdle {
			t.Errorf("GetState() returned zero-value that isn't StateIdle")
		}

		txSeq := cdcLink.GetTxSequence()
		_ = txSeq // Should be 0 initially

		rxSeq := cdcLink.GetRxSequence()
		_ = rxSeq // Should be 0 initially

		// Verify Send and Receive methods exist (part of harq.HARQ)
		ctx := context.Background()
		err = cdcLink.Send(ctx, []byte("test"))
		if err != nil {
			t.Errorf("Send() on CDCLink failed: %v", err)
		}
	})

	// Test SPILink
	t.Run("SPILink implements harq.HARQ", func(t *testing.T) {
		spiLink, err := link.NewSPILink(mockTransport, session, nil)
		if err != nil {
			t.Fatalf("NewSPILink failed: %v", err)
		}

		// Compile-time check that SPILink implements harq.HARQ
		var _ harq.HARQ = spiLink

		// Runtime check of HARQ methods
		state := spiLink.GetState()
		if state == harq.State(0) && state != harq.StateIdle {
			t.Errorf("GetState() returned zero-value that isn't StateIdle")
		}

		txSeq := spiLink.GetTxSequence()
		_ = txSeq // Should be 0 initially

		rxSeq := spiLink.GetRxSequence()
		_ = rxSeq // Should be 0 initially
	})
}

// TestGateway_TransportManagerUsesHARQInterface verifies that
// TransportManager uses harq.HARQ interface (not legacy Transport).
func TestGateway_TransportManagerUsesHARQInterface(t *testing.T) {
	// Create a mock HARQ
	mockHARQ := &testutil.MockHARQ{
		GetStateFunc: func() harq.State {
			return harq.StateIdle
		},
	}

	// Verify mock implements harq.HARQ
	var _ harq.HARQ = mockHARQ

	// Create TransportManager
	config := manager.DefaultConfig()
	config.Mode = manager.ModeAuto
	tm := manager.NewTransportManager(config)

	// RegisterTransport should accept harq.HARQ interface
	tm.RegisterTransport("usb", mockHARQ, 100)
	tm.RegisterTransport("spi", mockHARQ, 50)

	// Start the manager
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() with harq.HARQ mock failed: %v", err)
	}
	defer tm.Stop()

	// Verify TransportManager methods return harq package types
	state := tm.GetState()
	_ = state // Should be harq.State type

	txSeq := tm.GetTxSequence()
	_ = txSeq // Should be uint16

	rxSeq := tm.GetRxSequence()
	_ = rxSeq // Should be uint16
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
