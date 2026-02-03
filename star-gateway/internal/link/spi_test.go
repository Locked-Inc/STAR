package link

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
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
}

func (m *mockSPITransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	if m.TransferFunc != nil {
		return m.TransferFunc(ctx, txData)
	}
	return make([]byte, len(txData)), nil
}

func (m *mockSPITransport) Send(data []byte) (int, error) {
	if m.SendFunc != nil {
		return m.SendFunc(data)
	}
	return len(data), nil
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

	if link.config.EnableFEC {
		t.Error("FEC should be disabled by default")
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

	// Send should succeed with ACK
	ctx := context.Background()
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

	// Mock Receive: Return valid data frame
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		dataFrame := &frame.Frame{
			Type: frame.FrameTypeCommand,
			Header: frame.Header{
				Sequence: testSequenceZero,
				Length:   uint16(len("test")),
				Flags:    testFlagsNone,
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

	nackSent := false

	// Mock Send: Capture NACK
	mockTransport.SendFunc = func(data []byte) (int, error) {
		decoder := frame.NewDecoder()
		nackFrame, err := decoder.Decode(data)
		if err != nil {
			return 0, err
		}

		if nackFrame.Type == frame.FrameTypeNack {
			nackSent = true
		}

		return len(data), nil
	}

	// Mock Receive: Return frame with sequence 0 (duplicate)
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		dataFrame := &frame.Frame{
			Type: frame.FrameTypeCommand,
			Header: frame.Header{
				Sequence: testSequenceZero, // Wrong sequence
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
		t.Fatal("Expected Receive to fail for invalid sequence")
	}

	if !errors.Is(err, harq.ErrDuplicateFrame) {
		t.Errorf("Expected ErrDuplicateFrame, got: %v", err)
	}

	if !nackSent {
		t.Error("Expected NACK to be sent")
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

	// Mock Receive: Return valid frame
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		dataFrame := &frame.Frame{
			Type: frame.FrameTypeCommand,
			Header: frame.Header{
				Sequence: testSequenceZero,
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

	// Should fail because ACK send failed
	if err == nil {
		t.Fatal("Expected Receive to fail when ACK send fails")
	}

	if sendCallCount != 1 {
		t.Errorf("Expected 1 send call (ACK), got %d", sendCallCount)
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
	// 0xFF = 11111111 → all +127
	for i := 0; i < 8; i++ {
		if softBits[i] != fec.SoftBitMax {
			t.Errorf("Bit %d of 0xFF: expected %d, got %d", i, fec.SoftBitMax, softBits[i])
		}
	}

	// 0x00 = 00000000 → all -127
	for i := 8; i < 16; i++ {
		if softBits[i] != fec.SoftBitMin {
			t.Errorf("Bit %d of 0x00: expected %d, got %d", i, fec.SoftBitMin, softBits[i])
		}
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

	if config.EnableFEC {
		t.Error("Expected FEC disabled by default")
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
// PHASE 3: SendWithType() Tests
// ============================================================================

// TestSPILink_SendWithType_Ping tests sending PING frames with correct type.
func TestSPILink_SendWithType_Ping(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	link, err := NewSPILink(mockTransport, session, &SPILinkConfig{
		EnableFEC:  false,
		MaxRetries: 3,
		ACKTimeout: 10 * time.Millisecond,
	})
	if err != nil {
		t.Fatalf("NewSPILink failed: %v", err)
	}

	ctx := context.Background()
	payload := []byte{0x00, 0x00, 0x00, 0x01} // PING counter

	// Capture sent frame
	var sentFrame []byte
	mockTransport.SendFunc = func(data []byte) (int, error) {
		sentFrame = make([]byte, len(data))
		copy(sentFrame, data)
		return len(data), nil
	}

	// Mock transport to return ACK
	ackReceived := false
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		if ackReceived {
			return nil, errors.New("no more data")
		}
		ackReceived = true

		// Create ACK frame for sequence 0
		ackFrame := &frame.Frame{
			Header: frame.Header{
				Sequence: 0,
				Length:   emptyPayloadLength,
				Flags:    frame.FlagNone,
			},
			Type:    frame.FrameTypeAck,
			Payload: []byte{},
		}
		encoder := frame.NewEncoder()
		ackData, _ := encoder.Encode(ackFrame)
		return ackData, nil
	}

	// Send PING frame
	err = link.SendWithType(ctx, payload, frame.FrameTypePing)
	if err != nil {
		t.Fatalf("SendWithType(PING) failed: %v", err)
	}

	// Verify frame was sent
	if sentFrame == nil {
		t.Fatal("No frame was sent")
	}

	// Decode and verify frame type
	decoder := frame.NewDecoder()
	f, err := decoder.Decode(sentFrame)
	if err != nil {
		t.Fatalf("Failed to decode frame: %v", err)
	}

	if f.Type != frame.FrameTypePing {
		t.Errorf("Frame type = %v, want FrameTypePing", f.Type)
	}

	// Verify FlagRequiresAck is set (SPILink always sets it)
	if (f.Header.Flags & frame.FlagRequiresAck) == 0 {
		t.Error("Expected FlagRequiresAck to be set")
	}
}

// TestSPILink_SendWithType_Reset tests sending RESET frames with correct type.
func TestSPILink_SendWithType_Reset(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	link, err := NewSPILink(mockTransport, session, &SPILinkConfig{
		EnableFEC:  false,
		MaxRetries: 3,
		ACKTimeout: 10 * time.Millisecond,
	})
	if err != nil {
		t.Fatalf("NewSPILink failed: %v", err)
	}

	ctx := context.Background()

	// Capture sent frame
	var sentFrame []byte
	mockTransport.SendFunc = func(data []byte) (int, error) {
		sentFrame = make([]byte, len(data))
		copy(sentFrame, data)
		return len(data), nil
	}

	// Mock transport to return ACK
	ackReceived := false
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		if ackReceived {
			return nil, errors.New("no more data")
		}
		ackReceived = true

		ackFrame := &frame.Frame{
			Header: frame.Header{
				Sequence: 0,
				Length:   emptyPayloadLength,
				Flags:    frame.FlagNone,
			},
			Type:    frame.FrameTypeAck,
			Payload: []byte{},
		}
		encoder := frame.NewEncoder()
		ackData, _ := encoder.Encode(ackFrame)
		return ackData, nil
	}

	// Send RESET frame (empty payload)
	err = link.SendWithType(ctx, []byte{}, frame.FrameTypeReset)
	if err != nil {
		t.Fatalf("SendWithType(RESET) failed: %v", err)
	}

	// Verify frame was sent
	if sentFrame == nil {
		t.Fatal("No frame was sent")
	}

	// Decode and verify frame type
	decoder := frame.NewDecoder()
	f, err := decoder.Decode(sentFrame)
	if err != nil {
		t.Fatalf("Failed to decode frame: %v", err)
	}

	if f.Type != frame.FrameTypeReset {
		t.Errorf("Frame type = %v, want FrameTypeReset", f.Type)
	}

	if len(f.Payload) != 0 {
		t.Errorf("RESET frame should have empty payload, got %d bytes", len(f.Payload))
	}
}

// TestSPILink_SendWithType_SequenceIncrement tests that sequence numbers increment
// correctly when using SendWithType() mixed with regular Send().
func TestSPILink_SendWithType_SequenceIncrement(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	link, err := NewSPILink(mockTransport, session, &SPILinkConfig{
		EnableFEC:  false,
		MaxRetries: 3,
		ACKTimeout: 10 * time.Millisecond,
	})
	if err != nil {
		t.Fatalf("NewSPILink failed: %v", err)
	}

	ctx := context.Background()
	decoder := frame.NewDecoder()

	// Capture all sent frames
	var sentFrames [][]byte
	mockTransport.SendFunc = func(data []byte) (int, error) {
		frameCopy := make([]byte, len(data))
		copy(frameCopy, data)
		sentFrames = append(sentFrames, frameCopy)
		return len(data), nil
	}

	// Mock transport to return ACKs for all frames
	ackCount := 0
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		if ackCount >= 3 {
			return nil, errors.New("no more data")
		}

		// Create ACK for the current sequence
		ackFrame := &frame.Frame{
			Header: frame.Header{
				Sequence: uint16(ackCount),
				Length:   emptyPayloadLength,
				Flags:    frame.FlagNone,
			},
			Type:    frame.FrameTypeAck,
			Payload: []byte{},
		}
		ackCount++

		encoder := frame.NewEncoder()
		ackData, _ := encoder.Encode(ackFrame)
		return ackData, nil
	}

	// Send regular COMMAND frame (seq=0)
	err = link.Send(ctx, []byte("command"))
	if err != nil {
		t.Fatalf("Send failed: %v", err)
	}

	// Send PING frame (seq=1)
	err = link.SendWithType(ctx, []byte{0x01}, frame.FrameTypePing)
	if err != nil {
		t.Fatalf("SendWithType(PING) failed: %v", err)
	}

	// Send another COMMAND frame (seq=2)
	err = link.Send(ctx, []byte("command2"))
	if err != nil {
		t.Fatalf("Second Send failed: %v", err)
	}

	// Verify 3 frames sent
	if len(sentFrames) != 3 {
		t.Fatalf("Expected 3 frames sent, got %d", len(sentFrames))
	}

	// Decode all frames and verify sequences
	for i, expectedSeq := range []uint16{0, 1, 2} {
		f, err := decoder.Decode(sentFrames[i])
		if err != nil {
			t.Fatalf("Failed to decode frame %d: %v", i, err)
		}

		if f.Header.Sequence != expectedSeq {
			t.Errorf("Frame %d: sequence = %d, want %d", i, f.Header.Sequence, expectedSeq)
		}
	}

	// Verify types
	f0, _ := decoder.Decode(sentFrames[0])
	if f0.Type != frame.FrameTypeCommand {
		t.Errorf("Frame 0: type = %v, want FrameTypeCommand", f0.Type)
	}

	f1, _ := decoder.Decode(sentFrames[1])
	if f1.Type != frame.FrameTypePing {
		t.Errorf("Frame 1: type = %v, want FrameTypePing", f1.Type)
	}

	f2, _ := decoder.Decode(sentFrames[2])
	if f2.Type != frame.FrameTypeCommand {
		t.Errorf("Frame 2: type = %v, want FrameTypeCommand", f2.Type)
	}
}

// TestSPILink_SendWithType_WithFEC tests SendWithType with FEC enabled.
func TestSPILink_SendWithType_WithFEC(t *testing.T) {
	mockTransport := &mockSPITransport{}
	session := manager.NewSessionState()
	link, err := NewSPILink(mockTransport, session, &SPILinkConfig{
		EnableFEC:  true, // Enable FEC
		MaxRetries: 3,
		ACKTimeout: 10 * time.Millisecond,
	})
	if err != nil {
		t.Fatalf("NewSPILink failed: %v", err)
	}

	ctx := context.Background()
	payload := []byte{0xAA, 0xBB, 0xCC, 0xDD}

	// Capture sent frame
	var sentFrame []byte
	mockTransport.SendFunc = func(data []byte) (int, error) {
		sentFrame = make([]byte, len(data))
		copy(sentFrame, data)
		return len(data), nil
	}

	// Mock transport to return ACK
	ackReceived := false
	mockTransport.ReceiveFunc = func(maxLen int) ([]byte, error) {
		if ackReceived {
			return nil, errors.New("no more data")
		}
		ackReceived = true

		ackFrame := &frame.Frame{
			Header: frame.Header{
				Sequence: 0,
				Length:   emptyPayloadLength,
				Flags:    frame.FlagNone,
			},
			Type:    frame.FrameTypeAck,
			Payload: []byte{},
		}
		encoder := frame.NewEncoder()
		ackData, _ := encoder.Encode(ackFrame)
		return ackData, nil
	}

	// Send PING frame with FEC enabled
	err = link.SendWithType(ctx, payload, frame.FrameTypePing)
	if err != nil {
		t.Fatalf("SendWithType(PING) with FEC failed: %v", err)
	}

	// Verify frame was sent
	if sentFrame == nil {
		t.Fatal("No frame was sent")
	}

	// Decode and verify FEC flag is set
	decoder := frame.NewDecoder()
	f, err := decoder.Decode(sentFrame)
	if err != nil {
		t.Fatalf("Failed to decode frame: %v", err)
	}

	if (f.Header.Flags & frame.FlagFECEnabled) == 0 {
		t.Error("Expected FlagFECEnabled to be set")
	}

	// Payload should be FEC-encoded (longer than original due to rate 1/2)
	if len(f.Payload) <= len(payload) {
		t.Errorf("FEC-encoded payload should be longer than original: got %d, original %d",
			len(f.Payload), len(payload))
	}
}
