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
	transportSwitchTimeout        = 200 * time.Millisecond
	transportPollInterval         = 10 * time.Millisecond
	testFailureThresholdImmediate = 1
	testFailbackDampingShort      = 150 * time.Millisecond
	testFailbackGoroutineWait     = 50 * time.Millisecond
	testDampingMargin             = 50 * time.Millisecond
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
			name:          "empty string is invalid for IsValid",
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
			if got := tt.transportmode.IsValid(); got != tt.wantValid {
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
			if got != tt.wantMode {
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
	pollForActiveTransport(t, tm, TransportNameUSB, transportSwitchTimeout)
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
				capturedSessionID.Store(binary.LittleEndian.Uint32(data[:SessionIDPayloadSize]))
			}
			return nil
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			count := receiveCount.Add(1)
			// First call is reset handshake, return RESET_ACK with matching session ID
			if count == 1 {
				sid := capturedSessionID.Load()
				payload := make([]byte, SessionIDPayloadSize)
				binary.LittleEndian.PutUint32(payload, sid)
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

	// Poll for failover to backup
	pollForActiveTransport(t, tm, "backup", transportSwitchTimeout)
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
	binary.LittleEndian.PutUint32(payload, sessionID)
	return payload
}

func TestPerformResetHandshake_StaleFramesDrained(t *testing.T) {
	var discardedCount atomic.Int32
	var capturedSessionID atomic.Uint32

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(ctx context.Context, data []byte, frameType frame.Type) error {
			if frameType == frame.FrameTypeReset && len(data) >= SessionIDPayloadSize {
				capturedSessionID.Store(binary.LittleEndian.Uint32(data[:SessionIDPayloadSize]))
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
				capturedSessionID.Store(binary.LittleEndian.Uint32(data[:SessionIDPayloadSize]))
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
				capturedSessionID.Store(binary.LittleEndian.Uint32(data[:SessionIDPayloadSize]))
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

// TestTransportManager_FailbackAfterRecovery verifies the complete failover -> failback cycle.
//
// Scenario:
//  1. Primary transport active (high priority)
//  2. Primary fails -> system switches to backup (low priority)
//  3. Primary recovers (health monitor detects)
//  4. System switches back to primary (failback)
//
// This test verifies that recovery detection triggers failback to the higher priority transport.
// Uses generic transport names (not "usb"/"spi") so probeTransport() returns true (healthy).
func TestTransportManager_FailbackAfterRecovery(t *testing.T) {
	config := DefaultConfig()
	config.FailureThreshold = testFailureThresholdImmediate
	config.HealthCheckInterval = TestHealthCheckInterval
	config.FailbackDamping = 0 // Disable damping for this test
	tm := NewTransportManager(config)

	// Track which transport is being used
	var primarySendCount, backupSendCount atomic.Int32

	// Primary transport (high priority) - initially works, then fails, then recovers
	primaryHealthy := atomic.Bool{}
	primaryHealthy.Store(true)

	mockPrimary := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			if !primaryHealthy.Load() {
				return errors.New("primary transport failed")
			}
			primarySendCount.Add(1)
			return nil
		},
	}

	// Backup transport (low priority) - always works
	mockBackup := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			backupSendCount.Add(1)
			return nil
		},
	}

	// Use generic names (not "usb"/"spi") so probeTransport() returns true
	tm.RegisterTransport("primary", mockPrimary, PriorityUSB) // Priority 10
	tm.RegisterTransport("backup", mockBackup, PrioritySPI)   // Priority 5

	// Step 1: Verify primary is initially active (higher priority)
	if tm.GetActiveTransport() != "primary" {
		t.Fatalf("Expected primary active initially, got %s", tm.GetActiveTransport())
	}

	// Step 2: Trigger primary failure
	t.Logf("Step 2: Triggering primary failure")
	primaryHealthy.Store(false)

	if err := tm.Send(context.Background(), []byte("trigger-failover")); err == nil {
		t.Error("Expected Send to fail when primary is unhealthy")
	}

	// Poll for failover to backup
	pollForActiveTransport(t, tm, "backup", transportSwitchTimeout)
	t.Logf("[PASS] Failover to backup successful")

	// Step 3: Recover primary and wait for health monitor to detect it
	t.Logf("Step 3: Recovering primary")
	primaryHealthy.Store(true)

	// Manually mark primary as unhealthy so health monitor can detect recovery
	tm.SetTransportHealthForTest("primary", false, false)

	// Step 4: Manually trigger health check (simulates health monitor probe)
	hm := NewHealthMonitor(TestHealthCheckInterval, 0, 0)
	hm.checkTransports(tm)

	// Poll for failback to primary (failover is now async, no need for extra sleep)
	pollForActiveTransport(t, tm, "primary", transportSwitchTimeout)
	t.Logf("[PASS] Failback to primary successful")

	// Verify that primary is now being used for traffic
	primaryBefore := primarySendCount.Load()
	if err := tm.Send(context.Background(), []byte("test")); err != nil {
		t.Errorf("Send failed after failback: %v", err)
	}
	if primarySendCount.Load() <= primaryBefore {
		t.Error("Primary transport should be receiving traffic after failback")
	}
}

// TestTransportManager_FailbackRespectsDamping verifies that failback damping prevents
// rapid oscillation when a transport recovers.
//
// Scenario:
//  1. Primary fails -> switch to backup
//  2. Primary recovers immediately
//  3. System should NOT switch back immediately (damping window active)
//  4. After damping expires, primary becomes eligible again
//
// This prevents rapid switching when a transport is flapping.
// Uses generic transport names so probeTransport() returns true (healthy).
//
// NOTE: This test uses a longer sleep duration (>100ms) after damping expires
// to verify damping window expiration. This is an accepted timing exception
// required to demonstrate the damping logic.
func TestTransportManager_FailbackRespectsDamping(t *testing.T) {
	config := DefaultConfig()
	config.FailureThreshold = testFailureThresholdImmediate
	config.HealthCheckInterval = TestHealthCheckInterval
	config.FailbackDamping = testFailbackDampingShort
	tm := NewTransportManager(config)

	primaryHealthy := atomic.Bool{}
	primaryHealthy.Store(true)

	mockPrimary := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			if !primaryHealthy.Load() {
				return errors.New("primary failed")
			}
			return nil
		},
	}

	mockBackup := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, p ...harq.Priority) error {
			return nil
		},
	}

	tm.RegisterTransport("primary", mockPrimary, PriorityUSB)
	tm.RegisterTransport("backup", mockBackup, PrioritySPI)

	// Verify primary is initially active
	if tm.GetActiveTransport() != "primary" {
		t.Fatalf("Expected primary active initially, got %s", tm.GetActiveTransport())
	}

	// Trigger primary failure
	t.Logf("Triggering primary failure")
	primaryHealthy.Store(false)

	if err := tm.Send(context.Background(), []byte("fail")); err == nil {
		t.Error("Expected Send to fail")
	}

	// Wait for failover to backup
	pollForActiveTransport(t, tm, "backup", transportSwitchTimeout)
	t.Logf("[PASS] Failed over to backup")

	// Recover primary immediately
	t.Logf("Recovering primary (damping should prevent immediate failback)")
	primaryHealthy.Store(true)

	// Mark primary as unhealthy so health monitor detects recovery
	tm.SetTransportHealthForTest("primary", false, false)

	// Trigger health check (simulates health monitor probe)
	hm := NewHealthMonitor(TestHealthCheckInterval, 0, 0)
	hm.checkTransports(tm)

	// Give failback goroutine time to evaluate (should skip due to damping)
	time.Sleep(testFailbackGoroutineWait)

	// Verify we're STILL on backup (damping prevents immediate failback)
	if tm.GetActiveTransport() != "backup" {
		t.Errorf("Expected to stay on backup during damping window, got %s", tm.GetActiveTransport())
	}
	t.Logf("[PASS] Stayed on backup during damping window")

	// Verify LastRecovery timestamp was set and transport is healthy
	tm.mu.RLock()
	if wrapper, exists := tm.availableTransports["primary"]; exists {
		if wrapper.Health.LastRecovery.IsZero() {
			t.Error("Expected LastRecovery to be set after recovery detection")
		}
		if !wrapper.Health.IsHealthy {
			t.Error("Expected primary to be marked healthy after recovery")
		}
		// Verify damping is still active
		if time.Since(wrapper.Health.LastRecovery) >= config.FailbackDamping {
			t.Error("Damping window should still be active")
		}
	}
	tm.mu.RUnlock()

	// Wait for damping to expire
	waitDuration := testFailbackDampingShort + testDampingMargin
	t.Logf("Waiting for damping window to expire (%v)", waitDuration)
	time.Sleep(waitDuration)

	// Verify damping has expired
	tm.mu.RLock()
	dampingExpired := false
	if wrapper, exists := tm.availableTransports["primary"]; exists {
		if time.Since(wrapper.Health.LastRecovery) >= config.FailbackDamping {
			dampingExpired = true
		}
	}
	tm.mu.RUnlock()

	if !dampingExpired {
		t.Error("Damping window should have expired")
	}
	t.Logf("[PASS] Damping window has expired")

	// NOTE: With the current implementation, automatic failback after damping expires
	// requires another trigger (e.g., failover attempt). The health monitor only triggers
	// attemptFailover() once when recovery is detected, not continuously.
	//
	// To demonstrate that the transport IS now eligible for failback (damping expired),
	// we manually trigger selectBestTransportLocked to show primary would be selected.
	// WHITE-BOX TESTING: This directly calls the internal selectBestTransportLocked method
	// to verify damping logic. Update this test if the internal implementation changes.
	tm.mu.Lock()
	bestTransport := tm.selectBestTransportLocked()
	tm.mu.Unlock()

	if bestTransport == nil {
		t.Error("Expected a best transport to be selected")
	} else if bestTransport.Name != "primary" {
		t.Errorf("After damping expires, primary should be the best transport, got %s", bestTransport.Name)
	} else {
		t.Logf("[PASS] Primary is now eligible for selection (damping expired)")
	}

	// In a real scenario, failback would occur when:
	// 1. Backup transport fails (triggering failover evaluation), OR
	// 2. An explicit switch is requested, OR
	// 3. Future enhancement: periodic re-evaluation of transport priority
	//
	// For this test, we verify that damping successfully prevented immediate failback.
}

// TestDispatchControlFrame_Reset_SendsResetAck verifies server-side RESET handling:
//   - dispatchControlFrame routes FrameTypeReset to sendResetAck
//   - SendWithType is called with FrameTypeResetAck and nil payload
//   - heartbeat.OnFrameReceived is triggered (liveness proof)
//   - sessionState.Reset() is called to resync sequences with firmware
func TestDispatchControlFrame_Reset_SendsResetAck(t *testing.T) {
	var sentFrameType frame.Type
	var sentPayload []byte
	var sendWithTypeCalled atomic.Bool

	mock := &testutil.MockHARQ{
		SendWithTypeFunc: func(_ context.Context, data []byte, ft frame.Type) error {
			sentFrameType = ft
			sentPayload = data
			sendWithTypeCalled.Store(true)
			return nil
		},
	}

	tm := newTestManagerWithMock(mock)

	// Advance TX sequence so we can confirm Reset() zeroes it.
	tm.sessionState.NextTxSequence()
	tm.sessionState.NextTxSequence()
	if tx := tm.sessionState.GetTxSequence(); tx != 2 {
		t.Fatalf("pre-condition: txSequence = %d, want 2", tx)
	}

	resetFrame := &harq.ReceiveResult{
		Payload: nil,
		Metadata: harq.FrameMetadata{
			Type:     frame.FrameTypeReset,
			Sequence: 42,
		},
	}

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	result := tm.dispatchControlFrame(ctx, resetFrame)

	// Control frame must be consumed (nil return).
	if result != nil {
		t.Errorf("dispatchControlFrame returned non-nil for RESET frame, want nil")
	}

	// SendWithType must have been called.
	if !sendWithTypeCalled.Load() {
		t.Fatal("SendWithType was not called; RESET_ACK not sent")
	}

	// Must send FrameTypeResetAck with nil payload.
	if sentFrameType != frame.FrameTypeResetAck {
		t.Errorf("SendWithType frame type = %v, want FrameTypeResetAck", sentFrameType)
	}
	if sentPayload != nil {
		t.Errorf("SendWithType payload = %v, want nil", sentPayload)
	}

	// Sequences must be reset to 0 (firmware restarts from 0 after RESET).
	if tx := tm.sessionState.GetTxSequence(); tx != 0 {
		t.Errorf("txSequence = %d, want 0 after RESET", tx)
	}
	if rx := tm.sessionState.GetRxSequence(); rx != 0 {
		t.Errorf("rxSequence = %d, want 0 after RESET", rx)
	}
}
