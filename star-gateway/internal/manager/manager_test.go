// Package manager tests for transport management.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"encoding/binary"
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

func TestTransportManager_CheckValidTransportMode(t *testing.T) {
	tests := []struct {
		name          string
		transportmode TransportMode
		wantValid     bool
	}{
		{
			name:          "ModeAuto is valid",
			transportmode: ModeAuto,
			wantValid:     true,
		},
		{
			name:          "ModePreferUSB is valid",
			transportmode: ModePreferUSB,
			wantValid:     true,
		},
		{
			name:          "ModeForceUSB is valid",
			transportmode: ModeForceUSB,
			wantValid:     true,
		},
		{
			name:          "ModeForceSPI is valid",
			transportmode: ModeForceSPI,
			wantValid:     true,
		},
		{
			name:          "empty string defaults to auto",
			transportmode: "",
			wantValid:     false, // empty string is not valid, but should be handled by ParseTransportMode
		},
		{
			name:          "invalid mode is not valid",
			transportmode: "invalid",
			wantValid:     false,
		},
		{

			name:          "invalid numeral string",
			transportmode: "123",
			wantValid:     false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			config := DefaultConfig()
			config.Mode = tt.transportmode
			if got := config.Mode.IsValid(); got != tt.wantValid {
				t.Errorf("IsValid() = %v, want %v", got, tt.wantValid)
			}
		})
	}
}

func TestTransportManager_ParseTransportMode(t *testing.T) {
	tests := []struct {
		name     string
		input    string
		wantMode TransportMode
		wantErr  bool
	}{
		{
			name:     "valid mode auto",
			input:    "auto",
			wantMode: ModeAuto,
			wantErr:  false,
		},
		{
			name:     "valid mode prefer-usb",
			input:    "prefer-usb",
			wantMode: ModePreferUSB,
			wantErr:  false,
		},
		{
			name:     "valid mode force-usb",
			input:    "force-usb",
			wantMode: ModeForceUSB,
			wantErr:  false,
		},
		{
			name:     "valid mode force-spi",
			input:    "force-spi",
			wantMode: ModeForceSPI,
			wantErr:  false,
		},
		{
			name:     "invalid mode returns error",
			input:    "invalid",
			wantMode: ModeAuto,
			wantErr:  true,
		},
		{
			name:     "empty string returns error",
			input:    "",
			wantMode: ModeAuto,
			wantErr:  true,
		},
		{
			name:     "case-insensitive mode",
			input:    "PrEfEr-UsB",
			wantMode: ModePreferUSB,
			wantErr:  false,
		},
		{
			name:     "numerical string returns error",
			input:    "123",
			wantMode: ModeAuto,
			wantErr:  true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ParseTransportMode(tt.input)
			if (err != nil) != tt.wantErr {
				t.Fatalf("ParseTransportMode(%q) error = %v, wantErr %v", tt.input, err, tt.wantErr)
			}
			if !tt.wantErr && got != tt.wantMode {
				t.Errorf("ParseTransportMode(%q) = %v, want %v", tt.input, got, tt.wantMode)
			}
		})
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

	// Capture session ID from RESET frame to echo in RESET_ACK
	var capturedSessionID atomic.Uint32

	mockTransport := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			return nil
		},
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			// Capture session ID from RESET payload
			if frameType == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
				capturedSessionID.Store(binary.BigEndian.Uint32(data[:SessionIDPayloadSize]))
			}
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			count := receiveCount.Add(1)
			// First call is reset handshake, return RESET_ACK with matching session ID
			if count == 1 {
				sid := capturedSessionID.Load()
				payload := make([]byte, SessionIDPayloadSize)
				binary.BigEndian.PutUint32(payload, sid)
				return &harq.ReceiveResult{
					Payload: payload,
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

// Helper to create a TransportManager with mock transport set as active for
// testing performResetHandshake directly (without going through Start).
func newTestManagerWithMock(mock *testutil.MockHARQ) *TransportManager {
	config := DefaultConfig()
	tm := NewTransportManager(config)
	tm.RegisterTransport("mock", mock, 10)
	// activeTransport is set by RegisterTransport
	return tm
}

// Helper to build a RESET_ACK payload with a given session ID.
func makeResetAckPayload(sessionID uint32) []byte {
	payload := make([]byte, SessionIDPayloadSize)
	binary.BigEndian.PutUint32(payload, sessionID)
	return payload
}

func TestPerformResetHandshake_StaleFramesDrained(t *testing.T) {
	var discardedCount atomic.Int32
	var capturedSessionID atomic.Uint32

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			if frameType == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
				capturedSessionID.Store(binary.BigEndian.Uint32(data[:SessionIDPayloadSize]))
			}
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			count := discardedCount.Add(1)
			// First 5 calls return stale COMMAND frames
			if count <= 5 {
				return &harq.ReceiveResult{
					Payload: []byte("stale-data"),
					Metadata: harq.FrameMetadata{
						Type:     frame.FrameTypeCommand,
						Sequence: uint16(count + 100), // Old sequence numbers
					},
				}, nil
			}
			// 6th call: valid RESET_ACK with matching session ID
			sid := capturedSessionID.Load()
			return &harq.ReceiveResult{
				Payload: makeResetAckPayload(sid),
				Metadata: harq.FrameMetadata{
					Type:     frame.FrameTypeResetAck,
					Sequence: 0,
				},
			}, nil
		},
	}

	tm := newTestManagerWithMock(mock)
	ctx := context.Background()

	// Set non-zero sequences to verify they get reset
	tm.sessionState.NextTxSequence() // tx=0, advances to 1
	tm.sessionState.NextTxSequence() // tx=1, advances to 2

	err := tm.performResetHandshake(ctx)
	if err != nil {
		t.Fatalf("performResetHandshake failed: %v", err)
	}

	// Verify all 5 stale frames were drained (6 total receives, 5 stale + 1 RESET_ACK)
	if got := discardedCount.Load(); got != 6 {
		t.Errorf("total receive calls = %d, want 6 (5 stale + 1 RESET_ACK)", got)
	}

	// Verify sequences were reset to 0 after RESET_ACK
	if tx := tm.sessionState.GetTxSequence(); tx != 0 {
		t.Errorf("txSequence = %d, want 0 after reset", tx)
	}
	if rx := tm.sessionState.GetRxSequence(); rx != 0 {
		t.Errorf("rxSequence = %d, want 0 after reset", rx)
	}

	// Barrier should be deactivated
	if tm.sessionState.IsBarrierActive() {
		t.Error("barrier should be deactivated after successful handshake")
	}
}

func TestPerformResetHandshake_SessionIDMismatch(t *testing.T) {
	var capturedSessionID atomic.Uint32
	var receiveCount atomic.Int32

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			if frameType == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
				capturedSessionID.Store(binary.BigEndian.Uint32(data[:SessionIDPayloadSize]))
			}
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			count := receiveCount.Add(1)
			// First call: RESET_ACK with wrong session ID
			if count == 1 {
				return &harq.ReceiveResult{
					Payload: makeResetAckPayload(9999), // Wrong session ID
					Metadata: harq.FrameMetadata{
						Type:     frame.FrameTypeResetAck,
						Sequence: 0,
					},
				}, nil
			}
			// Second call: RESET_ACK with correct session ID
			sid := capturedSessionID.Load()
			return &harq.ReceiveResult{
				Payload: makeResetAckPayload(sid),
				Metadata: harq.FrameMetadata{
					Type:     frame.FrameTypeResetAck,
					Sequence: 0,
				},
			}, nil
		},
	}

	tm := newTestManagerWithMock(mock)
	ctx := context.Background()

	err := tm.performResetHandshake(ctx)
	if err != nil {
		t.Fatalf("performResetHandshake failed: %v", err)
	}

	// Should have received 2 frames (1 wrong + 1 correct)
	if got := receiveCount.Load(); got != 2 {
		t.Errorf("receive count = %d, want 2", got)
	}
}

func TestPerformResetHandshake_UsesFrameTypeReset(t *testing.T) {
	var sentFrameType atomic.Uint32

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			sentFrameType.Store(uint32(frameType))
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			// Return matching RESET_ACK (session ID = 1, first increment)
			return &harq.ReceiveResult{
				Payload: makeResetAckPayload(1),
				Metadata: harq.FrameMetadata{
					Type:     frame.FrameTypeResetAck,
					Sequence: 0,
				},
			}, nil
		},
	}

	tm := newTestManagerWithMock(mock)
	ctx := context.Background()

	err := tm.performResetHandshake(ctx)
	if err != nil {
		t.Fatalf("performResetHandshake failed: %v", err)
	}

	// Verify SendWithType was called with FrameTypeReset (not FrameTypeCommand)
	if got := frame.Type(sentFrameType.Load()); got != frame.FrameTypeReset {
		t.Errorf("sent frame type = %s, want RESET", got.String())
	}
}

func TestPerformResetHandshake_BarrierLifecycle(t *testing.T) {
	var barrierDuringReceive atomic.Bool

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			return nil, context.DeadlineExceeded
		},
	}

	tm := newTestManagerWithMock(mock)

	// Override ReceiveFunc to check barrier state during receive
	mock.ReceiveFunc = func(ctx context.Context) (*harq.ReceiveResult, error) {
		barrierDuringReceive.Store(tm.sessionState.IsBarrierActive())
		// Return matching RESET_ACK
		sid := tm.sessionState.GetSessionID()
		return &harq.ReceiveResult{
			Payload: makeResetAckPayload(sid),
			Metadata: harq.FrameMetadata{
				Type:     frame.FrameTypeResetAck,
				Sequence: 0,
			},
		}, nil
	}

	ctx := context.Background()
	err := tm.performResetHandshake(ctx)
	if err != nil {
		t.Fatalf("performResetHandshake failed: %v", err)
	}

	// Barrier should have been active during receive
	if !barrierDuringReceive.Load() {
		t.Error("barrier should be active during handshake receive")
	}

	// Barrier should be deactivated after completion
	if tm.sessionState.IsBarrierActive() {
		t.Error("barrier should be deactivated after handshake")
	}
}

func TestPerformResetHandshake_BarrierDeactivatedOnError(t *testing.T) {
	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			return errors.New("send failed")
		},
	}

	tm := newTestManagerWithMock(mock)
	ctx := context.Background()

	err := tm.performResetHandshake(ctx)
	if err == nil {
		t.Fatal("expected error from failed handshake")
	}

	// Barrier must be deactivated even on failure (via defer)
	if tm.sessionState.IsBarrierActive() {
		t.Error("barrier should be deactivated after failed handshake")
	}
}

func TestPerformResetHandshake_Idempotency(t *testing.T) {
	mock := &testutil.MockHARQ{}
	tm := newTestManagerWithMock(mock)

	// Manually activate barrier to simulate concurrent reset
	tm.sessionState.ActivateBarrier(1)

	ctx := context.Background()
	err := tm.performResetHandshake(ctx)
	if err == nil {
		t.Fatal("expected error when barrier already active")
	}

	if got := err.Error(); got != "reset already in progress (barrier active)" {
		t.Errorf("error = %q, want 'reset already in progress (barrier active)'", got)
	}

	// Clean up
	tm.sessionState.DeactivateBarrier()
}

func TestPerformResetHandshake_TimeoutRetry(t *testing.T) {
	var sendCount atomic.Int32
	var capturedSessionID atomic.Uint32

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			sendCount.Add(1)
			if frameType == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
				capturedSessionID.Store(binary.BigEndian.Uint32(data[:SessionIDPayloadSize]))
			}
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			attempt := sendCount.Load()
			// First two attempts: timeout
			if attempt <= 2 {
				<-ctx.Done()
				return nil, ctx.Err()
			}
			// Third attempt: success
			sid := capturedSessionID.Load()
			return &harq.ReceiveResult{
				Payload: makeResetAckPayload(sid),
				Metadata: harq.FrameMetadata{
					Type:     frame.FrameTypeResetAck,
					Sequence: 0,
				},
			}, nil
		},
	}

	tm := newTestManagerWithMock(mock)
	ctx := context.Background()

	err := tm.performResetHandshake(ctx)
	if err != nil {
		t.Fatalf("performResetHandshake failed: %v", err)
	}

	// Should have sent RESET 3 times (2 timeouts + 1 success)
	if got := sendCount.Load(); got != 3 {
		t.Errorf("send count = %d, want 3", got)
	}
}

func TestReceive_BarrierActive_DiscardsFrames(t *testing.T) {
	// Track receive calls to verify frames are being discarded
	callCount := atomic.Int32{}

	// Mock transport returns stale frames indefinitely (simulating FIFO backlog)
	mock := &testutil.MockHARQ{
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			callCount.Add(1)
			// Block briefly to simulate I/O, then return stale frame
			select {
			case <-ctx.Done():
				return nil, ctx.Err()
			case <-time.After(10 * time.Millisecond):
				return &harq.ReceiveResult{
					Payload: []byte("stale data"),
					Metadata: harq.FrameMetadata{
						Type:     frame.FrameTypeCommand,
						Sequence: 5,
					},
				}, nil
			}
		},
	}

	// Create manager with default config
	config := DefaultConfig()
	tm := NewTransportManager(config)
	tm.RegisterTransport("mock", mock, 10)

	// Initialize heartbeat manager (required by Receive code path)
	tm.heartbeat, _ = NewHeartbeatManager(
		50*time.Millisecond,
		200*time.Millisecond,
		func() {},
	)

	// Start manager context and machinery needed for Receive
	tm.ctx, tm.cancel = context.WithCancel(context.Background())
	defer tm.cancel()

	// Activate session barrier to simulate reset-in-progress state
	tm.sessionState.ActivateBarrier(1)
	defer tm.sessionState.DeactivateBarrier()

	// Receive should block and discard frames while barrier is active.
	// Use timeout context to verify it's discarding (not returning them).
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()

	result, err := tm.Receive(ctx)

	// Should timeout because barrier discards all frames
	if err != context.DeadlineExceeded {
		t.Errorf("Receive() error = %v, want context.DeadlineExceeded", err)
	}
	if result != nil {
		t.Errorf("Receive() returned non-nil result, want nil")
	}

	// Verify frames were actually received and discarded (not just no frames)
	if got := callCount.Load(); got < 2 {
		t.Errorf("mock Receive called %d times, want >= 2 (proves frames discarded)", got)
	}

	// Verify the transport was polled (call count > 0)
	if callCount.Load() == 0 {
		t.Error("expected at least one receive call, got none")
	}
}
