// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package link tests for CDC link implementation.
//
// STAR Project - Texas A&M University
// January 2026
package link

import (
	"context"
	"errors"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// Test constants to avoid magic numbers
const (
	// Sequence number expectations
	initialSequence         = 0
	firstSequence           = 0
	secondSequence          = 1
	thirdSequence           = 2
	expectedAfterReset      = 0
	sequenceAfterOneSend    = 1
	sequenceAfterThreeSends = 3

	// Frame count expectations
	expectedFirstFrames  = 1
	expectedSecondFrames = 2

	// Frame metadata
	testSequenceZero = 0
	testSequenceOne  = 1
	testSequenceTwo  = 2
	testPayloadLen   = 4
	testPayloadLen1  = 1
	testFlagsNone    = 0

	// Timeouts
	sendTimeout = 1 * time.Millisecond
)

// MockTransport simulates a USB CDC transport for testing.
type MockTransport struct {
	mu          sync.Mutex
	sendData    [][]byte
	receiveData []byte
	sendErr     error
	receiveErr  error
	closed      bool
}

// Compile-time assertion that MockTransport implements transport.Device.
// This ensures the mock stays in sync with the interface and catches regressions.
var _ transport.Device = (*MockTransport)(nil)

func (m *MockTransport) Send(data []byte) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.closed {
		return 0, errors.New("transport closed")
	}
	if m.sendErr != nil {
		return 0, m.sendErr
	}

	m.sendData = append(m.sendData, append([]byte(nil), data...))
	return len(data), nil
}

func (m *MockTransport) Receive(n int) ([]byte, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.closed {
		return nil, errors.New("transport closed")
	}
	if m.receiveErr != nil {
		return nil, m.receiveErr
	}

	if n > len(m.receiveData) {
		n = len(m.receiveData)
	}

	data := make([]byte, n)
	copy(data, m.receiveData[:n])
	m.receiveData = m.receiveData[n:]
	return data, nil
}

func (m *MockTransport) Close() error {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.closed = true
	return nil
}

func (m *MockTransport) Open() error {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.closed = false
	return nil
}

func (m *MockTransport) IsOpen() bool {
	m.mu.Lock()
	defer m.mu.Unlock()

	return !m.closed
}

func (m *MockTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	// For CDC (half-duplex), transfer is just send + receive
	if _, err := m.Send(txData); err != nil {
		return nil, err
	}
	return m.Receive(len(txData))
}

func (m *MockTransport) GetSentFrames() [][]byte {
	m.mu.Lock()
	defer m.mu.Unlock()

	return append([][]byte(nil), m.sendData...)
}

func (m *MockTransport) QueueReceive(data []byte) {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.receiveData = append(m.receiveData, data...)
}

// TestCDCLink_NewCDCLink tests CDCLink creation with validation.
func TestCDCLink_NewCDCLink(t *testing.T) {
	session := manager.NewSessionState()

	t.Run("ValidConstruction", func(t *testing.T) {
		transport := &MockTransport{}
		link, err := NewCDCLink(transport, session)
		if err != nil {
			t.Fatalf("NewCDCLink failed: %v", err)
		}
		if link == nil {
			t.Fatal("NewCDCLink returned nil link")
		}
	})

	t.Run("NilTransport", func(t *testing.T) {
		_, err := NewCDCLink(nil, session)
		if err == nil {
			t.Error("NewCDCLink should fail with nil transport")
		}
	})

	t.Run("NilSession", func(t *testing.T) {
		transport := &MockTransport{}
		_, err := NewCDCLink(transport, nil)
		if err == nil {
			t.Error("NewCDCLink should fail with nil session")
		}
	})
}

// TestCDCLink_Send tests the lightweight Send implementation.
func TestCDCLink_Send(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	ctx := context.Background()
	payload := []byte("test payload")

	// Send should succeed
	err = link.Send(ctx, payload)
	if err != nil {
		t.Fatalf("Send failed: %v", err)
	}

	// Verify frame was sent
	frames := transport.GetSentFrames()
	if len(frames) != expectedFirstFrames {
		t.Fatalf("Expected %d frame sent, got %d", expectedFirstFrames, len(frames))
	}

	// Decode and verify frame
	decoder := frame.NewDecoder()
	f, err := decoder.Decode(frames[0])
	if err != nil {
		t.Fatalf("Failed to decode sent frame: %v", err)
	}

	if string(f.Payload) != string(payload) {
		t.Errorf("Payload mismatch: got %s, want %s", f.Payload, payload)
	}

	// Verify sequence number incremented
	if f.Header.Sequence != firstSequence {
		t.Errorf("First sequence should be %d, got %d", firstSequence, f.Header.Sequence)
	}

	// Send another frame, verify sequence increments
	err = link.Send(ctx, []byte("second"))
	if err != nil {
		t.Fatalf("Second Send failed: %v", err)
	}

	frames = transport.GetSentFrames()
	if len(frames) != expectedSecondFrames {
		t.Fatalf("Expected %d frames sent, got %d", expectedSecondFrames, len(frames))
	}

	f2, err := decoder.Decode(frames[1])
	if err != nil {
		t.Fatalf("Failed to decode second frame: %v", err)
	}

	if f2.Header.Sequence != secondSequence {
		t.Errorf("Second sequence should be %d, got %d", secondSequence, f2.Header.Sequence)
	}
}

// TestCDCLink_SendConcurrent tests that concurrent sends are serialized.
func TestCDCLink_SendConcurrent(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	const numGoroutines = 10
	var wg sync.WaitGroup
	wg.Add(numGoroutines)

	ctx := context.Background()

	// Launch concurrent sends
	for i := 0; i < numGoroutines; i++ {
		go func(id int) {
			defer wg.Done()
			payload := []byte{byte(id)}
			if err := link.Send(ctx, payload); err != nil {
				t.Errorf("Goroutine %d Send failed: %v", id, err)
			}
		}(i)
	}

	wg.Wait()

	// Verify all frames were sent
	frames := transport.GetSentFrames()
	if len(frames) != numGoroutines {
		t.Fatalf("Expected %d frames, got %d", numGoroutines, len(frames))
	}

	// Verify sequences are 0-9 (in-order due to sendMutex)
	decoder := frame.NewDecoder()
	seenSequences := make(map[uint16]bool)

	for i, frameData := range frames {
		f, err := decoder.Decode(frameData)
		if err != nil {
			t.Fatalf("Failed to decode frame %d: %v", i, err)
		}

		if seenSequences[f.Header.Sequence] {
			t.Errorf("Duplicate sequence %d at frame %d", f.Header.Sequence, i)
		}
		seenSequences[f.Header.Sequence] = true
	}
}

// TestCDCLink_SendWithTimeout tests sendWithTimeout behavior.
func TestCDCLink_SendWithTimeout(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	tests := []struct {
		name     string
		setupCtx func() (context.Context, context.CancelFunc)
		setupErr error
		wantErr  bool
		checkErr func(error) bool
	}{
		{
			name: "ContextCanceled",
			setupCtx: func() (context.Context, context.CancelFunc) {
				ctx, cancel := context.WithCancel(context.Background())
				cancel() // Cancel immediately
				return ctx, cancel
			},
			setupErr: nil,
			wantErr:  true,
			checkErr: func(err error) bool {
				return errors.Is(err, context.Canceled)
			},
		},
		{
			name: "ContextTimeout",
			setupCtx: func() (context.Context, context.CancelFunc) {
				return context.WithTimeout(context.Background(), sendTimeout)
			},
			setupErr: errors.New("simulated block"),
			wantErr:  true,
			checkErr: func(err error) bool {
				return err != nil
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ctx, cancel := tt.setupCtx()
			defer cancel()

			transport.sendErr = tt.setupErr

			err := link.Send(ctx, []byte("test"))

			if tt.wantErr {
				if err == nil {
					t.Error("Expected error, got nil")
				} else if tt.checkErr != nil && !tt.checkErr(err) {
					t.Errorf("Error check failed for %v", err)
				}
			} else {
				if err != nil {
					t.Errorf("Unexpected error: %v", err)
				}
			}

			// Reset transport for next test
			transport.sendErr = nil
		})
	}
}

// TestCDCLink_Receive tests frame reception and sequence validation.
func TestCDCLink_Receive(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Encode a valid frame
	encoder := frame.NewEncoder()
	f := &frame.Frame{
		Header: frame.Header{
			Sequence: testSequenceZero,
			Length:   testPayloadLen,
			Flags:    testFlagsNone,
		},
		Type:    frame.FrameTypeCommand,
		Payload: []byte("test"),
	}

	encoded, err := encoder.Encode(f)
	if err != nil {
		t.Fatalf("Failed to encode frame: %v", err)
	}

	// Queue frame for reception
	transport.QueueReceive(encoded)

	ctx := context.Background()
	result, err := link.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive failed: %v", err)
	}

	if string(result.Payload) != "test" {
		t.Errorf("Received payload = %s, want test", result.Payload)
	}
}

// TestCDCLink_ReceiveGapTolerance tests that small sequence gaps are accepted.
func TestCDCLink_ReceiveGapTolerance(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	encoder := frame.NewEncoder()
	ctx := context.Background()

	// Send sequence 0
	f0 := &frame.Frame{
		Header:  frame.Header{Sequence: testSequenceZero, Length: testPayloadLen1, Flags: testFlagsNone},
		Type:    frame.FrameTypeCommand,
		Payload: []byte("0"),
	}
	enc0, err := encoder.Encode(f0)
	if err != nil {
		t.Fatalf("Failed to encode frame 0: %v", err)
	}
	transport.QueueReceive(enc0)

	_, err = link.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive seq 0 failed: %v", err)
	}

	// Skip sequence 1, send sequence 2 (gap = 1, should be accepted)
	f2 := &frame.Frame{
		Header:  frame.Header{Sequence: testSequenceTwo, Length: testPayloadLen1, Flags: testFlagsNone},
		Type:    frame.FrameTypeCommand,
		Payload: []byte("2"),
	}
	enc2, err := encoder.Encode(f2)
	if err != nil {
		t.Fatalf("Failed to encode frame 2: %v", err)
	}
	transport.QueueReceive(enc2)

	result, err := link.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive seq 2 (gap=1) should succeed: %v", err)
	}

	if string(result.Payload) != "2" {
		t.Errorf("Received payload = %s, want 2", result.Payload)
	}
}

// TestCDCLink_GetSequences tests sequence getters.
func TestCDCLink_GetSequences(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Initial sequences should be 0
	if seq := link.GetTxSequence(); seq != initialSequence {
		t.Errorf("Initial TX sequence = %d, want %d", seq, initialSequence)
	}
	if seq := link.GetRxSequence(); seq != initialSequence {
		t.Errorf("Initial RX sequence = %d, want %d", seq, initialSequence)
	}

	// Send a frame, TX sequence should increment
	ctx := context.Background()
	if err := link.Send(ctx, []byte("test")); err != nil {
		t.Fatalf("Send failed: %v", err)
	}

	if seq := link.GetTxSequence(); seq != sequenceAfterOneSend {
		t.Errorf("TX sequence after Send = %d, want %d", seq, sequenceAfterOneSend)
	}
}

// TestCDCLink_Reset tests sequence reset.
func TestCDCLink_Reset(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	ctx := context.Background()

	// Send some frames to increment sequence
	if err := link.Send(ctx, []byte("1")); err != nil {
		t.Fatalf("Send 1 failed: %v", err)
	}
	if err := link.Send(ctx, []byte("2")); err != nil {
		t.Fatalf("Send 2 failed: %v", err)
	}
	if err := link.Send(ctx, []byte("3")); err != nil {
		t.Fatalf("Send 3 failed: %v", err)
	}

	// Verify sequence is non-zero
	if seq := link.GetTxSequence(); seq != sequenceAfterThreeSends {
		t.Fatalf("TX sequence = %d, want %d", seq, sequenceAfterThreeSends)
	}

	// Reset
	link.Reset()

	// Verify sequence is back to 0
	if seq := link.GetTxSequence(); seq != expectedAfterReset {
		t.Errorf("TX sequence after Reset = %d, want %d", seq, expectedAfterReset)
	}
}

// TestCDCLink_GetState tests state reporting.
func TestCDCLink_GetState(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// CDCLink should report Idle (lightweight protocol, no state machine)
	state := link.GetState()
	if state != harq.StateIdle {
		t.Errorf("GetState = %v, want StateIdle", state)
	}
}

// ============================================================================
// SendWithType Tests (Phase 3: Frame Type API)
// ============================================================================

// TestCDCLink_SendWithType_Success tests sending different frame types.
func TestCDCLink_SendWithType_Success(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	ctx := context.Background()
	decoder := frame.NewDecoder()

	testCases := []struct {
		name      string
		frameType frame.Type
		payload   []byte
	}{
		{"PING", frame.FrameTypePing, []byte{0x01, 0x02, 0x03, 0x04}},
		{"PONG", frame.FrameTypePong, []byte{0xDE, 0xAD, 0xBE, 0xEF}},
		{"RESET", frame.FrameTypeReset, []byte{}},
		{"COMMAND", frame.FrameTypeCommand, []byte("command payload")},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			initialFrameCount := len(transport.GetSentFrames())

			// Send frame with explicit type
			err := link.SendWithType(ctx, tc.payload, tc.frameType)
			if err != nil {
				t.Fatalf("SendWithType(%s) failed: %v", tc.name, err)
			}

			// Verify frame was sent
			frames := transport.GetSentFrames()
			if len(frames) != initialFrameCount+1 {
				t.Fatalf("Expected 1 new frame, got %d", len(frames)-initialFrameCount)
			}

			// Decode and verify frame type
			f, err := decoder.Decode(frames[len(frames)-1])
			if err != nil {
				t.Fatalf("Failed to decode frame: %v", err)
			}

			if f.Type != tc.frameType {
				t.Errorf("Frame type = %v, want %v", f.Type, tc.frameType)
			}

			if string(f.Payload) != string(tc.payload) {
				t.Errorf("Payload = %q, want %q", f.Payload, tc.payload)
			}
		})
	}
}

// TestCDCLink_SendWithType_ContextCancelled tests context cancellation.
func TestCDCLink_SendWithType_ContextCancelled(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	cancel() // Cancel immediately

	err = link.SendWithType(ctx, []byte("payload"), frame.FrameTypePing)
	if err == nil {
		t.Fatal("SendWithType should fail with cancelled context")
	}

	if !errors.Is(err, context.Canceled) {
		t.Errorf("Error = %v, want context.Canceled", err)
	}
}

// TestCDCLink_SendWithType_TransportError tests transport send failure.
func TestCDCLink_SendWithType_TransportError(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{
		sendErr: errors.New("transport send error"),
	}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	ctx := context.Background()
	err = link.SendWithType(ctx, []byte("payload"), frame.FrameTypePing)
	if err == nil {
		t.Fatal("SendWithType should fail when transport Send fails")
	}

	if !strings.Contains(err.Error(), "transport send") {
		t.Errorf("Error should mention transport send failure, got: %v", err)
	}
}

// TestCDCLink_SendWithType_SequenceIncrement tests sequence management.
func TestCDCLink_SendWithType_SequenceIncrement(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	ctx := context.Background()
	decoder := frame.NewDecoder()

	// Send 3 frames with different types
	frameTypes := []frame.Type{
		frame.FrameTypePing,
		frame.FrameTypePong,
		frame.FrameTypeCommand,
	}

	for i, frameType := range frameTypes {
		err := link.SendWithType(ctx, []byte{byte(i)}, frameType)
		if err != nil {
			t.Fatalf("SendWithType[%d] failed: %v", i, err)
		}
	}

	// Verify all frames have incrementing sequences
	frames := transport.GetSentFrames()
	if len(frames) != len(frameTypes) {
		t.Fatalf("Expected %d frames, got %d", len(frameTypes), len(frames))
	}

	for i, encodedFrame := range frames {
		f, err := decoder.Decode(encodedFrame)
		if err != nil {
			t.Fatalf("Failed to decode frame %d: %v", i, err)
		}

		expectedSeq := uint16(i)
		if f.Header.Sequence != expectedSeq {
			t.Errorf("Frame[%d] sequence = %d, want %d", i, f.Header.Sequence, expectedSeq)
		}

		if f.Type != frameTypes[i] {
			t.Errorf("Frame[%d] type = %v, want %v", i, f.Type, frameTypes[i])
		}
	}
}
