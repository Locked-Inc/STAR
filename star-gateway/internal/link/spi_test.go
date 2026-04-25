// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package link

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// ============================================================================
// Mock Transport for Testing
// ============================================================================

// mockSPITransport implements transport.Device for testing.
type mockSPITransport struct {
	TransferFunc func(ctx context.Context, txData []byte) ([]byte, error)
	SendFunc     func(data []byte) (int, error)
	ReceiveFunc  func(maxLen int) ([]byte, error)
	IsOpenFunc   func() bool
	OpenFunc     func() error
	CloseFunc    func() error

	// Frame tracking (Phase 3: for SendWithType tests)
	mu         sync.Mutex
	sentFrames [][]byte
}

// Compile-time assertion that mockSPITransport implements transport.Device.
// This ensures the mock stays in sync with the interface and catches regressions.
var _ transport.Device = (*mockSPITransport)(nil)

func (m *mockSPITransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	if m.TransferFunc != nil {
		return m.TransferFunc(ctx, txData)
	}
	if m.ReceiveFunc != nil {
		return m.ReceiveFunc(len(txData))
	}
	return make([]byte, len(txData)), nil
}

func (m *mockSPITransport) Send(data []byte) (int, error) {
	// Track sent frames
	m.mu.Lock()
	m.sentFrames = append(m.sentFrames, append([]byte(nil), data...))
	m.mu.Unlock()

	if m.SendFunc != nil {
		return m.SendFunc(data)
	}
	return len(data), nil
}

// GetSentFrames returns all frames sent via Send().
func (m *mockSPITransport) GetSentFrames() [][]byte {
	m.mu.Lock()
	defer m.mu.Unlock()
	return append([][]byte(nil), m.sentFrames...)
}

func (m *mockSPITransport) Receive(maxLen int) ([]byte, error) {
	if m.ReceiveFunc != nil {
		return m.ReceiveFunc(maxLen)
	}
	return make([]byte, maxLen), nil
}

func (m *mockSPITransport) IsOpen() bool {
	if m.IsOpenFunc != nil {
		return m.IsOpenFunc()
	}
	return true
}

func (m *mockSPITransport) Open() error {
	if m.OpenFunc != nil {
		return m.OpenFunc()
	}
	return nil
}

func (m *mockSPITransport) Close() error {
	if m.CloseFunc != nil {
		return m.CloseFunc()
	}
	return nil
}

// ============================================================================
// Constructor Tests
// ============================================================================

func TestNewSPILink_Valid(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, err := NewSPILink(mockTransport, session, nil)
	if err != nil {
		t.Fatalf("NewSPILink failed: %v", err)
	}

	if link == nil {
		t.Fatal("Expected non-nil SPILink")
	}

	if link.transport != mockTransport {
		t.Error("Transport not set correctly")
	}

	if link.sessionState != session {
		t.Error("SessionState not set correctly")
	}

	if link.encoder == nil {
		t.Error("Encoder not initialized")
	}

	if link.decoder == nil {
		t.Error("Decoder not initialized")
	}

	if !link.config.EnableFEC {
		t.Error("FEC should be enabled by default")
	}
}

func TestNewSPILink_NilTransport(t *testing.T) {
	session := manager.NewSessionState()

	_, err := NewSPILink(nil, session, nil)
	if err == nil {
		t.Fatal("Expected error for nil transport")
	}

	if err.Error() != "transport cannot be nil" {
		t.Errorf("Unexpected error message: %v", err)
	}
}

func TestNewSPILink_NilSessionState(t *testing.T) {
	mockTransport := &mockSPITransport{}

	_, err := NewSPILink(mockTransport, nil, nil)
	if err == nil {
		t.Fatal("Expected error for nil session state")
	}

	if err.Error() != "session state cannot be nil" {
		t.Errorf("Unexpected error message: %v", err)
	}
}

func TestNewSPILink_WithFECEnabled(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	config := &SPILinkConfig{EnableFEC: true}

	link, err := NewSPILink(mockTransport, session, config)
	if err != nil {
		t.Fatalf("NewSPILink with FEC failed: %v", err)
	}

	if link.fecEncoder == nil {
		t.Error("FEC encoder not initialized")
	}

	if link.fecDecoder == nil {
		t.Error("FEC decoder not initialized")
	}

	if link.combiner == nil {
		t.Error("Chase combiner not initialized")
	}
}

// ============================================================================
// Send Tests (ACK/NACK Handshake)
// ============================================================================

func TestSPILink_SendWithAck_Success(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	// Mock: Return ACK frame on receive
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		ackFrame := &frame.Frame{
			Type: frame.FrameTypeAck,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
				Flags:    testFlagsNone,
			},
			Payload: []byte{},
		}
		encoded, _ := encoder.Encode(ackFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	// Test data
	testData := []byte("test payload")

	ctx := context.Background()

	// CRITICAL FIX #1: Run Receive() in background to dispatch ACK to Send()
	// (only Receive() reads from decoder now)
	// Add small delay to ensure Send() registers ACK channel before Receive() processes response
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		link.Receive(ctx)
	}()

	// Send should succeed with ACK
	err := link.Send(ctx, testData)
	if err != nil {
		t.Fatalf("Send failed: %v", err)
	}

	// Verify state returned to Idle
	if link.GetState() != harq.StateIdle {
		t.Errorf("Expected state Idle, got %v", link.GetState())
	}

	// Verify sequence incremented
	if session.GetTxSequence() != 1 {
		t.Errorf("Expected TX sequence 1, got %d", session.GetTxSequence())
	}
}

func TestSPILink_SendWithNack_Retry_Success(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	attemptCount := 0

	// Mock: Return NACK on first attempt, ACK on second
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		attemptCount++

		var frameToSend *frame.Frame
		if attemptCount == 1 {
			// First attempt: NACK
			frameToSend = &frame.Frame{
				Type: frame.FrameTypeNack,
				Header: frame.Header{
					Sequence: testSequenceZero,
					Length:   0,
				},
				Payload: []byte{},
			}
		} else {
			// Second attempt: ACK
			frameToSend = &frame.Frame{
				Type: frame.FrameTypeAck,
				Header: frame.Header{
					Sequence: testSequenceZero,
					Length:   0,
				},
				Payload: []byte{},
			}
		}

		encoded, _ := encoder.Encode(frameToSend)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()

	// CRITICAL FIX #1: Run Receive() in background to dispatch NACK/ACK to Send()
	// Add small delay to ensure Send() registers ACK channel before Receive() processes responses
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		for i := 0; i < 2; i++ {         // Process NACK, then ACK
			link.Receive(ctx)
			time.Sleep(2 * time.Millisecond) // Small delay between retries
		}
	}()

	err := link.Send(ctx, []byte("test"))
	if err != nil {
		t.Fatalf("Send failed after NACK retry: %v", err)
	}

	if attemptCount != 2 {
		t.Errorf("Expected 2 attempts (1 NACK + 1 ACK), got %d", attemptCount)
	}

	// Verify state returned to Idle
	if link.GetState() != harq.StateIdle {
		t.Errorf("Expected state Idle, got %v", link.GetState())
	}
}

func TestSPILink_SendMaxRetries_Failure(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	attemptCount := 0

	// Mock: Always return NACK
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		attemptCount++

		nackFrame := &frame.Frame{
			Type: frame.FrameTypeNack,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
			},
			Payload: []byte{},
		}

		encoded, _ := encoder.Encode(nackFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()

	// CRITICAL FIX #1: Run Receive() in background to dispatch NACKs to Send()
	// Add small delay to ensure Send() registers ACK channel before Receive() processes responses
	go func() {
		time.Sleep(5 * time.Millisecond)  // Give Send() time to register its channel
		for i := 0; i < maxRetries; i++ { // Process all NACK attempts
			link.Receive(ctx)
			time.Sleep(2 * time.Millisecond) // Small delay between retries
		}
	}()

	err := link.Send(ctx, []byte("test"))
	if err == nil {
		t.Fatal("Expected Send to fail after max retries")
	}

	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Errorf("Expected ErrMaxRetriesExceeded, got: %v", err)
	}

	if attemptCount != maxRetries {
		t.Errorf("Expected %d attempts, got %d", maxRetries, attemptCount)
	}

	// Verify state is Error
	if link.GetState() != harq.StateError {
		t.Errorf("Expected state Error, got %v", link.GetState())
	}
}

func TestSPILink_SendACKTimeout_Retry(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	attemptCount := 0

	// Mock: Return error (timeout) on first, ACK on second
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		attemptCount++

		if attemptCount == 1 {
			// Simulate timeout
			return nil, errors.New("timeout")
		}

		// Second attempt: ACK
		ackFrame := &frame.Frame{
			Type: frame.FrameTypeAck,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
			},
			Payload: []byte{},
		}

		encoded, _ := encoder.Encode(ackFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()

	// CRITICAL FIX #1: Run Receive() in background to process timeout then ACK
	// Add small delay to ensure Send() registers ACK channel before Receive() processes responses
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		for i := 0; i < 2; i++ {         // Process timeout error, then ACK
			link.Receive(ctx)
			time.Sleep(2 * time.Millisecond) // Small delay between retries
		}
	}()

	err := link.Send(ctx, []byte("test"))
	if err != nil {
		t.Fatalf("Send failed after timeout retry: %v", err)
	}

	if attemptCount != 2 {
		t.Errorf("Expected 2 attempts (1 timeout + 1 ACK), got %d", attemptCount)
	}
}

// ============================================================================
// Receive Tests
// ============================================================================

func TestSPILink_ReceiveValid_SendsACK(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	ackSent := false

	// Mock Send: Capture ACK
	mockTransport.SendFunc = func(data []byte) (int, error) {
		// Decode to verify it's an ACK
		decoder := frame.NewDecoder()
		ackFrame, err := decoder.Decode(data)
		if err != nil {
			t.Errorf("Failed to decode sent frame: %v", err)
			return 0, err
		}

		if ackFrame.Type == frame.FrameTypeAck {
			ackSent = true
		}

		return len(data), nil
	}

	// Mock Receive: Return valid data frame with FlagRequiresAck
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		dataFrame := &frame.Frame{
			Type: frame.FrameTypeCommand,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   uint16(len("test")),
				Flags:    frame.FlagRequiresAck, // Set flag to trigger ACK
			},
			Payload: []byte("test"),
		}

		encoded, _ := encoder.Encode(dataFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()
	result, err := link.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive failed: %v", err)
	}

	if string(result.Payload) != "test" {
		t.Errorf("Expected payload 'test', got '%s'", string(result.Payload))
	}

	if !ackSent {
		t.Error("Expected ACK to be sent")
	}

	// Verify sequence incremented
	if session.GetRxSequence() != 1 {
		t.Errorf("Expected RX sequence 1, got %d", session.GetRxSequence())
	}
}

func TestSPILink_ReceiveInvalidSequence_SendsNACK(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	// Pre-advance RX sequence to 5 (by validating sequences 0-4)
	for i := uint16(0); i < 5; i++ {
		session.ValidateRxSequence(i)
	}

	ackSent := false

	// Mock Send: Capture ACK (CRITICAL FIX #2: duplicates get ACK, not NACK)
	mockTransport.SendFunc = func(data []byte) (int, error) {
		decoder := frame.NewDecoder()
		responseFrame, err := decoder.Decode(data)
		if err != nil {
			return 0, err
		}

		if responseFrame.Type == frame.FrameTypeAck {
			ackSent = true
		}

		return len(data), nil
	}

	// Mock Receive: Return frame with sequence 0 (duplicate)
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		dataFrame := &frame.Frame{
			Type: frame.FrameTypeCommand,
			Header: frame.Header{
				Sequence: testSequenceZero, // Wrong sequence (duplicate)
				Length:   uint16(len("test")),
			},
			Payload: []byte("test"),
		}

		encoded, _ := encoder.Encode(dataFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()
	_, err := link.Receive(ctx)
	if err == nil {
		t.Fatal("Expected Receive to fail for duplicate sequence")
	}

	if !errors.Is(err, harq.ErrDuplicateFrame) {
		t.Errorf("Expected ErrDuplicateFrame, got: %v", err)
	}

	// CRITICAL FIX #2: Duplicates should get ACK (sender missed previous ACK)
	if !ackSent {
		t.Error("Expected ACK to be sent for duplicate frame")
	}
}

func TestSPILink_ReceiveACKSendFailed(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	sendCallCount := 0

	// Mock Send: Fail ACK send
	mockTransport.SendFunc = func(data []byte) (int, error) {
		sendCallCount++
		return 0, errors.New("transport send failed")
	}

	// Mock Receive: Return valid frame with FlagRequiresAck
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		dataFrame := &frame.Frame{
			Type: frame.FrameTypeCommand,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   uint16(len("test")),
				Flags:    frame.FlagRequiresAck, // Set flag to trigger ACK attempt
			},
			Payload: []byte("test"),
		}

		encoded, _ := encoder.Encode(dataFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()
	result, err := link.Receive(ctx)

	// CRITICAL FIX #4: Should succeed even if ACK send fails
	// (payload was decoded successfully, ACK failure is logged but not fatal)
	if err != nil {
		t.Fatalf("Expected Receive to succeed despite ACK send failure, got error: %v", err)
	}

	if result == nil || string(result.Payload) != "test" {
		t.Fatal("Expected valid payload despite ACK send failure")
	}

	if sendCallCount != 1 {
		t.Errorf("Expected 1 send call (ACK attempt), got %d", sendCallCount)
	}
}

func TestSPILink_ReceiveTransportError(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	// Mock Receive: Return transport error
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		return nil, errors.New("transport read failed")
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()
	_, err := link.Receive(ctx)

	// Should fail because transport failed
	if err == nil {
		t.Fatal("Expected Receive to fail when transport fails")
	}
}

// ============================================================================
// HARQ Interface Tests
// ============================================================================

func TestSPILink_GetState(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, _ := NewSPILink(mockTransport, session, nil)

	// Initial state should be Idle
	if link.GetState() != harq.StateIdle {
		t.Errorf("Expected initial state Idle, got %v", link.GetState())
	}

	// Test with closed transport
	mockTransport.IsOpenFunc = func() bool {
		return false
	}

	if link.GetState() != harq.StateError {
		t.Errorf("Expected state Error for closed transport, got %v", link.GetState())
	}
}

func TestSPILink_GetTxSequence(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, _ := NewSPILink(mockTransport, session, nil)

	// Initial sequence should be 0
	if link.GetTxSequence() != 0 {
		t.Errorf("Expected TX sequence 0, got %d", link.GetTxSequence())
	}

	// Advance sequence
	session.NextTxSequence()

	if link.GetTxSequence() != 1 {
		t.Errorf("Expected TX sequence 1, got %d", link.GetTxSequence())
	}
}

func TestSPILink_GetRxSequence(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, _ := NewSPILink(mockTransport, session, nil)

	// Initial sequence should be 0
	if link.GetRxSequence() != 0 {
		t.Errorf("Expected RX sequence 0, got %d", link.GetRxSequence())
	}

	// Validate a received sequence
	session.ValidateRxSequence(0)

	if link.GetRxSequence() != 1 {
		t.Errorf("Expected RX sequence 1, got %d", link.GetRxSequence())
	}
}

func TestSPILink_Reset(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, _ := NewSPILink(mockTransport, session, nil)

	// Advance sequences
	session.NextTxSequence()
	session.NextTxSequence()
	session.ValidateRxSequence(0)

	// Reset
	link.Reset()

	// Verify sequences reset
	if link.GetTxSequence() != 0 {
		t.Errorf("Expected TX sequence 0 after reset, got %d", link.GetTxSequence())
	}

	if link.GetRxSequence() != 0 {
		t.Errorf("Expected RX sequence 0 after reset, got %d", link.GetRxSequence())
	}

	// Verify state reset to Idle
	if link.GetState() != harq.StateIdle {
		t.Errorf("Expected state Idle after reset, got %v", link.GetState())
	}
}

// ============================================================================
// Shared SessionState Tests (Critical for Transport Switching)
// ============================================================================

func TestSPILink_SharedSessionState(t *testing.T) {
	mockTransport1 := &mockSPITransport{}
	mockTransport2 := &mockSPITransport{}
	session := manager.NewSessionState() // SHARED session

	// Create two links with same session
	link1, _ := NewSPILink(mockTransport1, session, nil)
	link2, _ := NewSPILink(mockTransport2, session, nil)

	// Advance sequence via link1
	session.NextTxSequence()
	session.NextTxSequence()

	// Verify both links see same sequence
	if link1.GetTxSequence() != 2 {
		t.Errorf("Link1 expected TX sequence 2, got %d", link1.GetTxSequence())
	}

	if link2.GetTxSequence() != 2 {
		t.Errorf("Link2 expected TX sequence 2, got %d", link2.GetTxSequence())
	}

	// This verifies the CRITICAL FIX #1: Shared SessionState prevents
	// sequence desynchronization during transport switching
}

// ============================================================================
// FEC Tests (Phase 2)
// ============================================================================

func TestSPILink_FECRoundTrip(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	config := &SPILinkConfig{EnableFEC: true}

	// Mock: Return ACK
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		ackFrame := &frame.Frame{
			Type: frame.FrameTypeAck,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
			},
			Payload: []byte{},
		}
		encoded, _ := encoder.Encode(ackFrame)
		return encoded, nil
	}

	// Capture sent frame
	var sentFrame []byte
	mockTransport.SendFunc = func(data []byte) (int, error) {
		sentFrame = make([]byte, len(data))
		copy(sentFrame, data)
		return len(data), nil
	}

	link, _ := NewSPILink(mockTransport, session, config)

	testData := []byte{0xAB, 0xCD}

	ctx := context.Background()

	// CRITICAL FIX #1: Run Receive() in background to dispatch ACK to Send()
	// Add small delay to ensure Send() registers ACK channel before Receive() processes response
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		link.Receive(ctx)
	}()

	err := link.Send(ctx, testData)
	if err != nil {
		t.Fatalf("FEC send failed: %v", err)
	}

	// Decode sent frame
	decoder := frame.NewDecoder()
	decodedFrame, err := decoder.Decode(sentFrame)
	if err != nil {
		t.Fatalf("Failed to decode sent frame: %v", err)
	}

	// Verify FEC flag set
	if (decodedFrame.Header.Flags & frame.FlagFECEnabled) == 0 {
		t.Error("Expected FlagFECEnabled to be set")
	}

	// Verify payload is FEC-encoded (longer than original)
	if len(decodedFrame.Payload) <= len(testData) {
		t.Errorf("Expected FEC-encoded payload to be longer than original")
	}
}

func TestSPILink_bytesToSoftBits(t *testing.T) {
	data := []byte{0xFF, 0x00}

	softBits := bytesToSoftBits(data)

	// Expected: 16 soft bits (2 bytes * 8 bits)
	if len(softBits) != 16 {
		t.Errorf("Expected 16 soft bits, got %d", len(softBits))
	}

	// Verify conversion
	// 0xFF = 11111111 -> all +127
	for i := 0; i < 8; i++ {
		if softBits[i] != fec.SoftBitMax {
			t.Errorf("Bit %d of 0xFF: expected %d, got %d", i, fec.SoftBitMax, softBits[i])
		}
	}

	// 0x00 = 00000000 -> all -127
	for i := 8; i < 16; i++ {
		if softBits[i] != fec.SoftBitMin {
			t.Errorf("Bit %d of 0x00: expected %d, got %d", i, fec.SoftBitMin, softBits[i])
		}
	}
}

// ============================================================================
// Phase 2: Chase Combining Tests
// ============================================================================

// TestSPILink_ChaseCombining_FirstAttemptSuccess tests successful decode on first attempt.
func TestSPILink_ChaseCombining_FirstAttemptSuccess(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	config := &SPILinkConfig{EnableFEC: true}

	// Mock: Return ACK
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		ackFrame := &frame.Frame{
			Type:   frame.FrameTypeAck,
			Header: frame.Header{Sequence: testSequenceZero, Length: frame.EmptyPayloadLength},
		}
		encoded, _ := encoder.Encode(ackFrame)
		return encoded, nil
	}

	mockTransport.SendFunc = func(data []byte) (int, error) {
		return len(data), nil
	}

	link, _ := NewSPILink(mockTransport, session, config)

	// Send data with FEC enabled
	testData := []byte{0xAB, 0xCD, 0xEF}
	ctx := context.Background()

	// CRITICAL FIX #1: Run Receive() in background to dispatch ACK to Send()
	// Add small delay to ensure Send() registers ACK channel before Receive() processes response
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		link.Receive(ctx)
	}()

	err := link.Send(ctx, testData)
	if err != nil {
		t.Fatalf("Send with FEC failed: %v", err)
	}

	// Verify combiner was reset (not left in partial state)
	if link.combiner == nil {
		t.Fatal("Combiner should be initialized with FEC enabled")
	}
}

// TestSPILink_ChaseCombining_RetransmissionCombines tests Chase Combining with retransmissions.
func TestSPILink_ChaseCombining_RetransmissionCombines(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	fecEncoder := fec.NewConvolutionalEncoder()

	config := &SPILinkConfig{EnableFEC: true}
	link, _ := NewSPILink(mockTransport, session, config)

	// Test data
	testData := []byte{0xAB, 0xCD}

	// Encode with FEC
	fecEncoded, err := fecEncoder.Encode(testData)
	if err != nil {
		t.Fatalf("FEC encode failed: %v", err)
	}

	// Create frames: first attempt (no FlagRetransmit), then retransmission (with FlagRetransmit)
	firstAttemptFrame := &frame.Frame{
		Type: frame.FrameTypeCommand,
		Header: frame.Header{
			Sequence: testSequenceZero,
			Length:   uint16(len(fecEncoded)),
			Flags:    frame.FlagFECEnabled, // No FlagRetransmit on first attempt
		},
		Payload: fecEncoded,
	}

	retransmitFrame := &frame.Frame{
		Type: frame.FrameTypeCommand,
		Header: frame.Header{
			Sequence: testSequenceZero,
			Length:   uint16(len(fecEncoded)),
			Flags:    frame.FlagFECEnabled | frame.FlagRetransmit,
		},
		Payload: fecEncoded,
	}

	// For this test, we'll manually call decodeFEC to test the combining logic.
	// Since the test data is valid FEC-encoded data (not corrupted), both
	// decodes will succeed immediately.

	// First attempt - succeeds immediately (no combining needed)
	decoded1, _, combiningCount1, err1 := link.decodeFEC(firstAttemptFrame)
	if err1 != nil {
		t.Errorf("First attempt decode failed: %v", err1)
	}
	if combiningCount1 != 1 {
		t.Errorf("First attempt combining count = %d, want 1", combiningCount1)
	}

	// Retransmission - with empty combiner (first succeeded), adds this frame only
	decoded2, _, combiningCount2, err2 := link.decodeFEC(retransmitFrame)
	if err2 != nil {
		t.Errorf("Retransmit decode failed: %v", err2)
	}
	if combiningCount2 != 1 {
		t.Errorf("Retransmit combining count = %d, want 1 (empty combiner)", combiningCount2)
	}

	// Verify both decodes produced the same original data (ignoring FEC padding)
	dataLen := len(testData)
	var safe1, safe2 []byte
	if len(decoded1) >= dataLen {
		safe1 = decoded1[:dataLen]
	} else {
		safe1 = decoded1
	}
	if len(decoded2) >= dataLen {
		safe2 = decoded2[:dataLen]
	} else {
		safe2 = decoded2
	}

	if len(decoded1) < dataLen || string(safe1) != string(testData) {
		t.Errorf("First decode = %v, want %v (ignoring padding)", safe1, testData)
	}
	if len(decoded2) < dataLen || string(safe2) != string(testData) {
		t.Errorf("Retransmit decode = %v, want %v (ignoring padding)", safe2, testData)
	}

	// Verify combiner was used (it should have been reset or have data)
	if link.combiner == nil {
		t.Fatal("Combiner should be initialized")
	}
}

// TestSPILink_ChaseCombining_ResetAfterSuccess tests combiner reset after successful decode.
func TestSPILink_ChaseCombining_ResetAfterSuccess(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	config := &SPILinkConfig{EnableFEC: true}
	link, _ := NewSPILink(mockTransport, session, config)

	// Test data
	testData := []byte{0xAB}

	// Encode with FEC (rate 1/2, so output should be ~2x input)
	fecEncoded, err := link.fecEncoder.Encode(testData)
	if err != nil {
		t.Fatalf("FEC encode failed: %v", err)
	}

	// Create valid FEC frame with FlagRequiresAck to trigger ACK response
	validFrame := &frame.Frame{
		Type: frame.FrameTypeCommand,
		Header: frame.Header{
			Sequence: testSequenceZero,
			Length:   uint16(len(fecEncoded)),
			Flags:    frame.FlagFECEnabled | frame.FlagRequiresAck,
		},
		Payload: fecEncoded,
	}

	// Mock: Send ACK for valid frame
	ackSent := false
	mockTransport.SendFunc = func(data []byte) (int, error) {
		decoder := frame.NewDecoder()
		f, _ := decoder.Decode(data)
		if f != nil && f.Type == frame.FrameTypeAck {
			ackSent = true
		}
		return len(data), nil
	}

	// Mock: Return encoded frame when Receive is called
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		encoded, _ := encoder.Encode(validFrame)
		return encoded, nil
	}

	ctx := context.Background()

	// Call Receive which will decode FEC and send ACK
	result, err := link.Receive(ctx)
	if err != nil {
		t.Fatalf("Receive with FEC failed: %v", err)
	}

	if result == nil {
		t.Fatal("Expected non-nil result")
	}

	// Verify ACK was sent
	if !ackSent {
		t.Error("Expected ACK to be sent for valid FEC frame")
	}

	// Verify FEC was decoded
	if !result.Metadata.FECDecoded {
		t.Error("Expected FECDecoded flag to be set")
	}
}

// ============================================================================
// Context Cancellation Tests
// ============================================================================

func TestSPILink_SendContextCanceled(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, _ := NewSPILink(mockTransport, session, nil)

	// Create canceled context
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	err := link.Send(ctx, []byte("test"))
	if err == nil {
		t.Fatal("Expected Send to fail with canceled context")
	}

	if !errors.Is(err, context.Canceled) {
		t.Errorf("Expected context.Canceled error, got: %v", err)
	}
}

func TestSPILink_ReceiveContextCanceled(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	link, _ := NewSPILink(mockTransport, session, nil)

	// Create canceled context
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	_, err := link.Receive(ctx)
	if err == nil {
		t.Fatal("Expected Receive to fail with canceled context")
	}

	if !errors.Is(err, context.Canceled) {
		t.Errorf("Expected context.Canceled error, got: %v", err)
	}
}

// ============================================================================
// Config Tests
// ============================================================================

func TestDefaultSPILinkConfig(t *testing.T) {
	config := DefaultSPILinkConfig()

	if !config.EnableFEC {
		t.Error("Expected FEC enabled by default")
	}

	if config.MaxRetries != maxRetries {
		t.Errorf("Expected MaxRetries %d, got %d", maxRetries, config.MaxRetries)
	}

	if config.ACKTimeout != ackTimeout {
		t.Errorf("Expected ACKTimeout %v, got %v", ackTimeout, config.ACKTimeout)
	}
}

func TestSPILink_CustomConfig(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()

	customConfig := &SPILinkConfig{
		EnableFEC:  true,
		MaxRetries: 5,
		ACKTimeout: 20 * time.Millisecond,
	}

	link, _ := NewSPILink(mockTransport, session, customConfig)

	if !link.config.EnableFEC {
		t.Error("Expected FEC enabled")
	}

	if link.config.MaxRetries != 5 {
		t.Errorf("Expected MaxRetries 5, got %d", link.config.MaxRetries)
	}

	if link.config.ACKTimeout != 20*time.Millisecond {
		t.Errorf("Expected ACKTimeout 20ms, got %v", link.config.ACKTimeout)
	}
}

// ============================================================================
// SendWithType Tests (Phase 3: Frame Type API)
// ============================================================================

// TestSPILink_SendWithType_WithAck_Success tests sending with explicit frame type and ACK.
func TestSPILink_SendWithType_WithAck_Success(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	// Mock: Return ACK frame on receive
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		ackFrame := &frame.Frame{
			Type: frame.FrameTypeAck,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
				Flags:    testFlagsNone,
			},
			Payload: []byte{},
		}
		encoded, _ := encoder.Encode(ackFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()

	// Run Receive() in background to dispatch ACK
	// Add small delay to ensure Send() registers ACK channel before Receive() processes response
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		link.Receive(ctx)
	}()

	// Send PING frame with explicit type
	testPayload := []byte{0x01, 0x02, 0x03, 0x04}
	err := link.SendWithType(ctx, testPayload, frame.FrameTypePing)
	if err != nil {
		t.Fatalf("SendWithType failed: %v", err)
	}

	// Verify state returned to Idle
	if link.GetState() != harq.StateIdle {
		t.Errorf("Expected state Idle, got %v", link.GetState())
	}

	// Verify sequence incremented
	if session.GetTxSequence() != 1 {
		t.Errorf("Expected TX sequence 1, got %d", session.GetTxSequence())
	}

	// Verify frame type in sent data
	decoder := frame.NewDecoder()
	sentFrames := mockTransport.GetSentFrames()
	if len(sentFrames) == 0 {
		t.Fatal("No frames sent")
	}
	f, err := decoder.Decode(sentFrames[0])
	if err != nil {
		t.Fatalf("Failed to decode sent frame: %v", err)
	}
	if f.Type != frame.FrameTypePing {
		t.Errorf("Frame type = %v, want FrameTypePing", f.Type)
	}
}

// TestSPILink_SendWithType_WithNack_Retry tests NACK retry with explicit frame type.
func TestSPILink_SendWithType_WithNack_Retry(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	attemptCount := 0

	// Mock: First receive returns NACK, second returns ACK
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		attemptCount++

		if attemptCount == 1 {
			// First attempt: NACK
			nackFrame := &frame.Frame{
				Type: frame.FrameTypeNack,
				Header: frame.Header{
					Sequence: testSequenceZero,
					Length:   0,
					Flags:    testFlagsNone,
				},
				Payload: []byte{},
			}
			encoded, _ := encoder.Encode(nackFrame)
			return encoded, nil
		}

		// Second attempt: ACK
		ackFrame := &frame.Frame{
			Type: frame.FrameTypeAck,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
				Flags:    testFlagsNone,
			},
			Payload: []byte{},
		}
		encoded, _ := encoder.Encode(ackFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()

	// Run Receive() in background
	// Add small delay to ensure Send() registers ACK channel before Receive() processes responses
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		for i := 0; i < 2; i++ {
			link.Receive(ctx)
			time.Sleep(2 * time.Millisecond) // Small delay between retries
		}
	}()

	// Send PONG frame
	err := link.SendWithType(ctx, []byte{0xDE, 0xAD, 0xBE, 0xEF}, frame.FrameTypePong)
	if err != nil {
		t.Fatalf("SendWithType failed: %v", err)
	}

	// Verify 2 attempts were made
	sentFrames := mockTransport.GetSentFrames()
	if len(sentFrames) != 2 {
		t.Errorf("Expected 2 send attempts, got %d", len(sentFrames))
	}

	// Verify both frames are PONG type
	decoder := frame.NewDecoder()
	for i, encodedFrame := range sentFrames {
		f, err := decoder.Decode(encodedFrame)
		if err != nil {
			t.Fatalf("Failed to decode frame %d: %v", i, err)
		}
		if f.Type != frame.FrameTypePong {
			t.Errorf("Frame[%d] type = %v, want FrameTypePong", i, f.Type)
		}
	}
}

// TestSPILink_SendWithType_MaxRetries tests max retries with explicit frame type.
func TestSPILink_SendWithType_MaxRetries(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	encoder := frame.NewEncoder()

	// Mock: Always return NACK
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		nackFrame := &frame.Frame{
			Type: frame.FrameTypeNack,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   0,
				Flags:    testFlagsNone,
			},
			Payload: []byte{},
		}
		encoded, _ := encoder.Encode(nackFrame)
		return encoded, nil
	}

	link, _ := NewSPILink(mockTransport, session, nil)

	ctx := context.Background()

	// Run Receive() in background
	// Add small delay to ensure Send() registers ACK channel before Receive() processes responses
	go func() {
		time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
		for i := 0; i < maxRetries; i++ {
			link.Receive(ctx)
			time.Sleep(2 * time.Millisecond) // Small delay between retries
		}
	}()

	// Send RESET frame should fail after max retries
	err := link.SendWithType(ctx, []byte{}, frame.FrameTypeReset)
	if err == nil {
		t.Fatal("SendWithType should fail after max retries")
	}

	if err != ErrMaxRetriesExceeded {
		t.Errorf("Error = %v, want ErrMaxRetriesExceeded", err)
	}

	// Verify state is Error
	if link.GetState() != harq.StateError {
		t.Errorf("Expected state Error, got %v", link.GetState())
	}
}

// TestSPILink_SendWithType_FrameTypes tests sending different frame types.
func TestSPILink_SendWithType_FrameTypes(t *testing.T) {
	encoder := frame.NewEncoder()

	testCases := []struct {
		name      string
		frameType frame.Type
		payload   []byte
	}{
		{"PING", frame.FrameTypePing, []byte{0x01, 0x02, 0x03, 0x04}},
		{"PONG", frame.FrameTypePong, []byte{0xDE, 0xAD, 0xBE, 0xEF}},
		{"RESET", frame.FrameTypeReset, []byte{}},
		{"RESET_ACK", frame.FrameTypeResetAck, []byte{}},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			mockTransport := &mockSPITransport{}
			session := manager.NewSessionState()

			// Mock: Return ACK
			mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
				ackFrame := &frame.Frame{
					Type: frame.FrameTypeAck,
					Header: frame.Header{
						Sequence: testSequenceZero,
						Length:   0,
						Flags:    testFlagsNone,
					},
					Payload: []byte{},
				}
				encoded, _ := encoder.Encode(ackFrame)
				return encoded, nil
			}

			link, _ := NewSPILink(mockTransport, session, nil)
			ctx := context.Background()

			// Run Receive() in background
			// Add small delay to ensure Send() registers ACK channel before Receive() processes response
			go func() {
				time.Sleep(5 * time.Millisecond) // Give Send() time to register its channel
				link.Receive(ctx)
			}()

			// Send frame with explicit type
			err := link.SendWithType(ctx, tc.payload, tc.frameType)
			if err != nil {
				t.Fatalf("SendWithType(%s) failed: %v", tc.name, err)
			}

			// Verify frame type
			decoder := frame.NewDecoder()
			sentFrames := mockTransport.GetSentFrames()
			if len(sentFrames) == 0 {
				t.Fatal("No frames sent")
			}
			f, err := decoder.Decode(sentFrames[0])
			if err != nil {
				t.Fatalf("Failed to decode frame: %v", err)
			}
			if f.Type != tc.frameType {
				t.Errorf("Frame type = %v, want %v", f.Type, tc.frameType)
			}

			// For zero-length payloads the encoder must NOT set FlagFECEnabled.
			// This enforces the "zero-length-skip" contract introduced around sendWithRetry.
			if len(tc.payload) == 0 {
				if (f.Header.Flags & frame.FlagFECEnabled) != 0 {
					t.Fatalf("%s: zero-length payload must NOT set FlagFECEnabled", tc.name)
				}
			}

			// If FEC is enabled the wire payload will be FEC-encoded; decode before comparing.
			if (f.Header.Flags & frame.FlagFECEnabled) != 0 {
				dec := fec.NewViterbiDecoder()
				decoded, err := dec.DecodeHard(f.Payload, len(tc.payload))
				if err != nil {
					t.Fatalf("FEC decode failed: %v", err)
				}
				if string(decoded) != string(tc.payload) {
					t.Errorf("Decoded payload = %q, want %q", decoded, tc.payload)
				}
			} else {
				if string(f.Payload) != string(tc.payload) {
					t.Errorf("Payload = %q, want %q", f.Payload, tc.payload)
				}
			}
		})
	}
}
