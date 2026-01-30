// Package link tests for CDC link implementation.
//
// STAR Project - Texas A&M University
// January 2026
package link

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
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
	if len(frames) != 1 {
		t.Fatalf("Expected 1 frame sent, got %d", len(frames))
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
	if f.Header.Sequence != 0 {
		t.Errorf("First sequence should be 0, got %d", f.Header.Sequence)
	}

	// Send another frame, verify sequence increments
	err = link.Send(ctx, []byte("second"))
	if err != nil {
		t.Fatalf("Second Send failed: %v", err)
	}

	frames = transport.GetSentFrames()
	if len(frames) != 2 {
		t.Fatalf("Expected 2 frames sent, got %d", len(frames))
	}

	f2, err := decoder.Decode(frames[1])
	if err != nil {
		t.Fatalf("Failed to decode second frame: %v", err)
	}

	if f2.Header.Sequence != 1 {
		t.Errorf("Second sequence should be 1, got %d", f2.Header.Sequence)
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

		if f.Header.Sequence != uint16(i) {
			t.Errorf("Frame %d has sequence %d, expected %d", i, f.Header.Sequence, i)
		}
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

	t.Run("ContextCanceled", func(t *testing.T) {
		ctx, cancel := context.WithCancel(context.Background())
		cancel() // Cancel immediately

		err := link.Send(ctx, []byte("test"))
		if !errors.Is(err, context.Canceled) {
			t.Errorf("Expected context.Canceled, got %v", err)
		}
	})

	t.Run("ContextTimeout", func(t *testing.T) {
		ctx, cancel := context.WithTimeout(context.Background(), 1*time.Millisecond)
		defer cancel()

		// Block transport to trigger timeout
		transport.sendErr = errors.New("simulated block")

		err := link.Send(ctx, []byte("test"))
		if err == nil {
			t.Error("Expected error due to context timeout or send error")
		}

		// Reset transport
		transport.sendErr = nil
	})
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
			Sequence: 0,
			Length:   4,
			Flags:    0,
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
		Header:  frame.Header{Sequence: 0, Length: 1, Flags: 0},
		Type:    frame.FrameTypeCommand,
		Payload: []byte("0"),
	}
	enc0, _ := encoder.Encode(f0)
	transport.QueueReceive(enc0)

	_, err = link.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive seq 0 failed: %v", err)
	}

	// Skip sequence 1, send sequence 2 (gap = 1, should be accepted)
	f2 := &frame.Frame{
		Header:  frame.Header{Sequence: 2, Length: 1, Flags: 0},
		Type:    frame.FrameTypeCommand,
		Payload: []byte("2"),
	}
	enc2, _ := encoder.Encode(f2)
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
	if seq := link.GetTxSequence(); seq != 0 {
		t.Errorf("Initial TX sequence = %d, want 0", seq)
	}
	if seq := link.GetRxSequence(); seq != 0 {
		t.Errorf("Initial RX sequence = %d, want 0", seq)
	}

	// Send a frame, TX sequence should increment
	ctx := context.Background()
	link.Send(ctx, []byte("test"))

	if seq := link.GetTxSequence(); seq != 1 {
		t.Errorf("TX sequence after Send = %d, want 1", seq)
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
	link.Send(ctx, []byte("1"))
	link.Send(ctx, []byte("2"))
	link.Send(ctx, []byte("3"))

	// Verify sequence is non-zero
	if seq := link.GetTxSequence(); seq != 3 {
		t.Fatalf("TX sequence = %d, want 3", seq)
	}

	// Reset
	link.Reset()

	// Verify sequence is back to 0
	if seq := link.GetTxSequence(); seq != 0 {
		t.Errorf("TX sequence after Reset = %d, want 0", seq)
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
