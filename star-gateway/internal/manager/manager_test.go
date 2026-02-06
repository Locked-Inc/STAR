// Package manager tests for transport management.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
)

// Test timing constants to avoid magic numbers and flakiness
const (
	transportSwitchTimeout = 200 * time.Millisecond
	transportPollInterval  = 10 * time.Millisecond
)

func TestTransportManager_Lifecycle(t *testing.T) {
	config := DefaultConfig()
	config.Mode = ModeAuto // ensure auto mode
	tm := NewTransportManager(config)

	// Start without transports should log but succeed (retry mode)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() failed: %v", err)
	}

	if tm.GetState() != harq.StateError {
		t.Errorf("Initial state should be Error (no transports), got %v", tm.GetState())
	}

	if err := tm.Stop(); err != nil {
		t.Errorf("Stop() failed: %v", err)
	}
}

func TestTransportManager_RegisterAndSelect(t *testing.T) {
	config := DefaultConfig()
	tm := NewTransportManager(config)

	mockUSB := &testutil.MockHARQ{}
	mockSPI := &testutil.MockHARQ{}

	// Register SPI (Priority 5)
	tm.RegisterTransport(TransportNameSPI, mockSPI, PrioritySPI)

	// Should select SPI immediately
	if tm.GetActiveTransport() != TransportNameSPI {
		t.Errorf("Expected SPI active, got %s", tm.GetActiveTransport())
	}

	// Register USB (Priority 10)
	tm.RegisterTransport(TransportNameUSB, mockUSB, PriorityUSB)

	// Should switch to USB (higher priority)
	// Poll until USB becomes active or timeout to avoid flakiness
	deadline := time.Now().Add(transportSwitchTimeout)
	for time.Now().Before(deadline) {
		if tm.GetActiveTransport() == TransportNameUSB {
			break
		}
		time.Sleep(transportPollInterval)
	}

	if tm.GetActiveTransport() != TransportNameUSB {
		t.Errorf("Expected USB active after registration, got %s", tm.GetActiveTransport())
	}
}

func TestTransportManager_SendReceive(t *testing.T) {
	config := DefaultConfig()
	tm := NewTransportManager(config)

	// Track call count to distinguish reset handshake from normal receive
	var receiveCount atomic.Int32

	mockTransport := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			count := receiveCount.Add(1)
			// First call is reset handshake, return RESET_ACK
			if count == 1 {
				return &harq.ReceiveResult{
					Payload: []byte{}, // Empty payload for RESET_ACK
					Metadata: harq.FrameMetadata{
						Type:     frame.FrameTypeResetAck,
						Sequence: 0,
					},
				}, nil
			}
			// Subsequent calls are normal receives
			return &harq.ReceiveResult{
				Payload: []byte("response"),
				Metadata: harq.FrameMetadata{
					Type:     frame.FrameTypeResponse,
					Sequence: uint16(count - 1),
				},
			}, nil
		},
	}

	tm.RegisterTransport("mock", mockTransport, 10)
	ctx := context.Background()
	if err := tm.Start(ctx); err != nil {
		t.Fatalf("Start() failed: %v", err)
	}
	defer tm.Stop()

	// Test Send
	if err := tm.Send(ctx, []byte("request")); err != nil {
		t.Errorf("Send() failed: %v", err)
	}

	// Test Receive
	res, err := tm.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive() failed: %v", err)
	}
	if string(res.Payload) != "response" {
		t.Errorf("Receive payload = %s, want response", string(res.Payload))
	}
}

func TestTransportManager_ForceSwitch(t *testing.T) {
	config := DefaultConfig()
	tm := NewTransportManager(config)

	mock1 := &testutil.MockHARQ{}
	mock2 := &testutil.MockHARQ{}

	tm.RegisterTransport("t1", mock1, 10)
	tm.RegisterTransport("t2", mock2, 5)

	// Initially t1 should be active
	if tm.GetActiveTransport() != "t1" {
		t.Fatalf("Initial transport should be t1")
	}

	// Force switch to t2
	if err := tm.ForceSwitch("t2"); err != nil {
		t.Fatalf("ForceSwitch failed: %v", err)
	}

	if tm.GetActiveTransport() != "t2" {
		t.Errorf("Active transport should be t2, got %s", tm.GetActiveTransport())
	}
}

func TestTransportManager_Failover(t *testing.T) {
	config := DefaultConfig()
	config.FailureThreshold = 1 // Fail immediately on error
	tm := NewTransportManager(config)

	// High priority transport that fails
	mockFaulty := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			return harq.ErrMaxRetriesExceeded
		},
	}

	// Backup transport
	mockBackup := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			return nil
		},
	}

	tm.RegisterTransport("faulty", mockFaulty, 10)
	tm.RegisterTransport("backup", mockBackup, 5)

	// Ensure faulty is initially selected
	if tm.GetActiveTransport() != "faulty" {
		t.Fatalf("Expected faulty transport initially")
	}

	// Trigger failure and assert Send returns an error for faulty transport
	if err := tm.Send(context.Background(), []byte("trigger")); err == nil {
		t.Fatalf("Expected Send to return error for faulty transport, got nil")
	} else if !errors.Is(err, harq.ErrMaxRetriesExceeded) {
		t.Fatalf("Send returned unexpected error: %v", err)
	}

	// Poll for failover to backup with a short timeout to avoid flakiness
	deadline := time.Now().Add(200 * time.Millisecond)
	for time.Now().Before(deadline) {
		if tm.GetActiveTransport() == "backup" {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	if tm.GetActiveTransport() != "backup" {
		t.Errorf("Expected failover to backup, got %s", tm.GetActiveTransport())
	}
}

func TestTransportManager_Getters(t *testing.T) {
	tm := NewTransportManager(DefaultConfig())

	mock := &testutil.MockHARQ{
		GetTxSeqFunc: func() uint16 { return 42 },
		GetRxSeqFunc: func() uint16 { return 24 },
		GetStateFunc: func() harq.State { return harq.StateIdle },
	}

	tm.RegisterTransport("mock", mock, 10)

	if seq := tm.GetTxSequence(); seq != 42 {
		t.Errorf("GetTxSequence = %d, want 42", seq)
	}

	if seq := tm.GetRxSequence(); seq != 24 {
		t.Errorf("GetRxSequence = %d, want 24", seq)
	}

	avail := tm.GetAvailableTransports()
	if len(avail) != 1 || avail[0] != "mock" {
		t.Errorf("GetAvailableTransports = %v, want [mock]", avail)
	}

	metrics := tm.GetHealthMetrics()
	if _, ok := metrics["mock"]; !ok {
		t.Error("GetHealthMetrics missing 'mock' entry")
	}
}
