// Package arq tests for ARQ protocol implementation.
//
// STAR Project - Texas A&M University
// December 2025
package arq

import (
	"errors"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
)

// ============================================================================
// Test Helpers
// ============================================================================

// createTestARQ creates a StopAndWait with mock dependencies for testing.
func createTestARQ(config *Config) (*StopAndWait, *MockTransport) {
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	arq := NewStopAndWait(config, mock, encoder, decoder)
	return arq, mock
}

// createAckFrame creates an encoded ACK frame for the given sequence.
func createAckFrame(seq uint16) []byte {
	encoder := frame.NewEncoder()
	f, _ := frame.NewFrame(frame.FrameTypeAck, nil)
	f.Header.Sequence = seq
	encoded, _ := encoder.Encode(f)
	return encoded
}

// createNackFrame creates an encoded NACK frame for the given sequence.
func createNackFrame(seq uint16) []byte {
	encoder := frame.NewEncoder()
	f, _ := frame.NewFrame(frame.FrameTypeNack, nil)
	f.Header.Sequence = seq
	encoded, _ := encoder.Encode(f)
	return encoded
}

// createCommandFrame creates an encoded command frame.
func createCommandFrame(seq uint16, payload []byte) []byte {
	encoder := frame.NewEncoder()
	f, _ := frame.NewFrame(frame.FrameTypeCommand, payload)
	f.Header.Sequence = seq
	encoded, _ := encoder.Encode(f)
	return encoded
}

// ============================================================================
// Constant Tests
// ============================================================================

func TestARQConstants(t *testing.T) {
	t.Run("DefaultMaxRetries", func(t *testing.T) {
		if DefaultMaxRetries != 3 {
			t.Errorf("DefaultMaxRetries = %d, want 3", DefaultMaxRetries)
		}
	})

	t.Run("DefaultTimeout", func(t *testing.T) {
		if DefaultTimeout != 10*time.Millisecond {
			t.Errorf("DefaultTimeout = %v, want 10ms", DefaultTimeout)
		}
	})

	t.Run("DefaultSequenceMax", func(t *testing.T) {
		if DefaultSequenceMax != 0xFFFF {
			t.Errorf("DefaultSequenceMax = %d, want 65535", DefaultSequenceMax)
		}
	})
}

func TestStateString(t *testing.T) {
	tests := []struct {
		state    State
		expected string
	}{
		{StateIdle, "IDLE"},
		{StateWaitingAck, "WAITING_ACK"},
		{StateRetransmitting, "RETRANSMITTING"},
		{StateError, "ERROR"},
		{State(99), "UNKNOWN"},
	}

	for _, tc := range tests {
		t.Run(tc.expected, func(t *testing.T) {
			if got := tc.state.String(); got != tc.expected {
				t.Errorf("State.String() = %s, want %s", got, tc.expected)
			}
		})
	}
}

// ============================================================================
// Config Tests
// ============================================================================

func TestDefaultARQConfig(t *testing.T) {
	config := DefaultARQConfig()

	t.Run("MaxRetries", func(t *testing.T) {
		if config.MaxRetries != DefaultMaxRetries {
			t.Errorf("MaxRetries = %d, want %d", config.MaxRetries, DefaultMaxRetries)
		}
	})

	t.Run("Timeout", func(t *testing.T) {
		if config.Timeout != DefaultTimeout {
			t.Errorf("Timeout = %v, want %v", config.Timeout, DefaultTimeout)
		}
	})
}

// ============================================================================
// StopAndWait Constructor Tests
// ============================================================================

func TestNewStopAndWait(t *testing.T) {
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()

	tests := []struct {
		name            string
		config          *Config
		expectedRetries int
		expectedTimeout time.Duration
	}{
		{
			name:            "nil_config_uses_defaults",
			config:          nil,
			expectedRetries: DefaultMaxRetries,
			expectedTimeout: DefaultTimeout,
		},
		{
			name: "custom_config",
			config: &Config{
				MaxRetries: 5,
				Timeout:    20 * time.Millisecond,
			},
			expectedRetries: 5,
			expectedTimeout: 20 * time.Millisecond,
		},
		{
			name: "zero_retries",
			config: &Config{
				MaxRetries: 0,
				Timeout:    5 * time.Millisecond,
			},
			expectedRetries: 0,
			expectedTimeout: 5 * time.Millisecond,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			arq := NewStopAndWait(tc.config, mock, encoder, decoder)

			if arq == nil {
				t.Fatal("expected ARQ, got nil")
			}
			if arq.Config().MaxRetries != tc.expectedRetries {
				t.Errorf("MaxRetries = %d, want %d", arq.Config().MaxRetries, tc.expectedRetries)
			}
			if arq.Config().Timeout != tc.expectedTimeout {
				t.Errorf("Timeout = %v, want %v", arq.Config().Timeout, tc.expectedTimeout)
			}
		})
	}
}

func TestStopAndWaitInitialState(t *testing.T) {
	arq, _ := createTestARQ(nil)

	t.Run("State", func(t *testing.T) {
		if arq.GetState() != StateIdle {
			t.Errorf("GetState() = %v, want StateIdle", arq.GetState())
		}
	})

	t.Run("TxSequence", func(t *testing.T) {
		if arq.GetTxSequence() != 0 {
			t.Errorf("GetTxSequence() = %d, want 0", arq.GetTxSequence())
		}
	})

	t.Run("RxSequence", func(t *testing.T) {
		if arq.GetRxSequence() != 0 {
			t.Errorf("GetRxSequence() = %d, want 0", arq.GetRxSequence())
		}
	})
}

func TestStopAndWaitReset(t *testing.T) {
	tests := []struct {
		name       string
		setupState State
		setupTxSeq uint16
		setupRxSeq uint16
		setupRetry int
	}{
		{"from_error_state", StateError, 100, 50, 3},
		{"from_waiting_ack", StateWaitingAck, 255, 128, 1},
		{"from_retransmitting", StateRetransmitting, 1000, 999, 2},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			arq, _ := createTestARQ(nil)
			arq.SetStateForTesting(tc.setupState, tc.setupTxSeq, tc.setupRxSeq, tc.setupRetry)

			arq.Reset()

			if arq.GetState() != StateIdle {
				t.Errorf("GetState() = %v, want StateIdle after Reset", arq.GetState())
			}
			if arq.GetTxSequence() != 0 {
				t.Errorf("GetTxSequence() = %d, want 0 after Reset", arq.GetTxSequence())
			}
			if arq.GetRxSequence() != 0 {
				t.Errorf("GetRxSequence() = %d, want 0 after Reset", arq.GetRxSequence())
			}
		})
	}
}

// ============================================================================
// Sequence Number Tests
// ============================================================================

func TestIncrementSequence(t *testing.T) {
	tests := []struct {
		name     string
		input    uint16
		expected uint16
	}{
		{"zero", 0, 1},
		{"normal", 100, 101},
		{"large_value", 10000, 10001},
		{"before_max", DefaultSequenceMax - 1, DefaultSequenceMax},
		{"wraparound_at_max", DefaultSequenceMax, 0},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := incrementSequence(tc.input)
			if got != tc.expected {
				t.Errorf("incrementSequence(%d) = %d, want %d", tc.input, got, tc.expected)
			}
		})
	}
}

func TestDecrementSequence(t *testing.T) {
	tests := []struct {
		name     string
		input    uint16
		expected uint16
	}{
		{"normal", 100, 99},
		{"one", 1, 0},
		{"wraparound_at_zero", 0, DefaultSequenceMax},
		{"from_max", DefaultSequenceMax, DefaultSequenceMax - 1},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := decrementSequence(tc.input)
			if got != tc.expected {
				t.Errorf("decrementSequence(%d) = %d, want %d", tc.input, got, tc.expected)
			}
		})
	}
}

// ============================================================================
// Send() Tests
// ============================================================================

func TestSend_Success(t *testing.T) {
	arq, mock := createTestARQ(nil)
	payload := []byte{0x01, 0x02, 0x03}

	// Queue ACK response with sequence 0
	mock.QueueResponse(createAckFrame(0))

	err := arq.Send(payload)

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if arq.GetState() != StateIdle {
		t.Errorf("GetState() = %v, want StateIdle", arq.GetState())
	}
	if arq.GetTxSequence() != 1 {
		t.Errorf("GetTxSequence() = %d, want 1", arq.GetTxSequence())
	}
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1", mock.GetSentCount())
	}
}

func TestSend_SequenceIncrement(t *testing.T) {
	arq, mock := createTestARQ(nil)

	// Send three messages
	for i := 0; i < 3; i++ {
		mock.QueueResponse(createAckFrame(uint16(i)))
		err := arq.Send([]byte{byte(i)})
		if err != nil {
			t.Fatalf("Send() iteration %d error = %v", i, err)
		}
	}

	if arq.GetTxSequence() != 3 {
		t.Errorf("GetTxSequence() = %d, want 3", arq.GetTxSequence())
	}
}

func TestSend_TimeoutThenSuccess(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    5 * time.Millisecond, // Short timeout for test
	}
	arq, mock := createTestARQ(config)

	// First attempt times out (no response queued), then ACK
	// We need to queue the ACK after a delay so first attempt times out
	go func() {
		time.Sleep(10 * time.Millisecond) // After first timeout
		mock.QueueResponse(createAckFrame(0))
	}()

	err := arq.Send([]byte{0x01})

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	// Should have sent twice (first attempt + retry)
	if mock.GetSentCount() < 2 {
		t.Errorf("sent count = %d, want >= 2", mock.GetSentCount())
	}
}

func TestSend_MaxRetriesExceeded(t *testing.T) {
	config := &Config{
		MaxRetries: 2,
		Timeout:    1 * time.Millisecond, // Very short timeout
	}
	arq, mock := createTestARQ(config)

	// Don't queue any responses - all attempts will timeout
	_ = mock // unused but needed for setup

	err := arq.Send([]byte{0x01})

	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Errorf("Send() error = %v, want ErrMaxRetriesExceeded", err)
	}
	if arq.GetState() != StateError {
		t.Errorf("GetState() = %v, want StateError", arq.GetState())
	}
}

func TestSend_NackThenAck(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    50 * time.Millisecond,
	}
	arq, mock := createTestARQ(config)

	// First response is NACK, second is ACK
	mock.QueueResponses(
		createNackFrame(0),
		createAckFrame(0),
	)

	err := arq.Send([]byte{0x01})

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if mock.GetSentCount() != 2 {
		t.Errorf("sent count = %d, want 2", mock.GetSentCount())
	}
}

func TestSend_TransportError(t *testing.T) {
	arq, mock := createTestARQ(nil)
	mock.SetSendError(errors.New("transport failed"))

	err := arq.Send([]byte{0x01})

	if err == nil {
		t.Error("Send() error = nil, want error")
	}
	if arq.GetState() != StateError {
		t.Errorf("GetState() = %v, want StateError", arq.GetState())
	}
}

func TestSend_SequenceWraparound(t *testing.T) {
	arq, mock := createTestARQ(nil)
	arq.SetStateForTesting(StateIdle, DefaultSequenceMax, 0, 0)

	mock.QueueResponse(createAckFrame(DefaultSequenceMax))

	err := arq.Send([]byte{0x01})

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if arq.GetTxSequence() != 0 {
		t.Errorf("GetTxSequence() = %d, want 0 (wrapped)", arq.GetTxSequence())
	}
}

func TestSend_RetransmitFlag(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    5 * time.Millisecond,
	}
	arq, mock := createTestARQ(config)

	// Queue ACK after delay to force retry
	go func() {
		time.Sleep(10 * time.Millisecond)
		mock.QueueResponse(createAckFrame(0))
	}()

	err := arq.Send([]byte{0x01})
	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}

	// Verify retransmit flag is set on second frame
	sentData := mock.GetSentData()
	if len(sentData) < 2 {
		t.Fatalf("expected at least 2 sent frames, got %d", len(sentData))
	}

	// Decode second frame and check flags
	decoder := frame.NewDecoder()
	f, err := decoder.Decode(sentData[1])
	if err != nil {
		t.Fatalf("failed to decode second frame: %v", err)
	}
	if f.Header.Flags&frame.FlagRetransmit == 0 {
		t.Error("second frame should have FlagRetransmit set")
	}
}

func TestSend_ErrorStateBlocks(t *testing.T) {
	arq, _ := createTestARQ(nil)
	arq.SetStateForTesting(StateError, 0, 0, 0)

	err := arq.Send([]byte{0x01})

	if !errors.Is(err, ErrInErrorState) {
		t.Errorf("Send() error = %v, want ErrInErrorState", err)
	}
}

func TestSend_NilTransport(t *testing.T) {
	arq := NewStopAndWait(nil, nil, frame.NewEncoder(), frame.NewDecoder())

	err := arq.Send([]byte{0x01})

	if !errors.Is(err, ErrTransportNil) {
		t.Errorf("Send() error = %v, want ErrTransportNil", err)
	}
}

func TestSend_NilEncoder(t *testing.T) {
	mock := NewMockTransport()
	arq := NewStopAndWait(nil, mock, nil, frame.NewDecoder())

	err := arq.Send([]byte{0x01})

	if !errors.Is(err, ErrEncoderNil) {
		t.Errorf("Send() error = %v, want ErrEncoderNil", err)
	}
}

func TestSend_EncodeError(t *testing.T) {
	mock := NewMockTransport()
	encoder := &MockEncoder{
		encodeFn: func(f *frame.Frame) ([]byte, error) {
			return nil, errors.New("encode failed")
		},
	}
	decoder := frame.NewDecoder()
	arq := NewStopAndWait(nil, mock, encoder, decoder)

	err := arq.Send([]byte{0x01})

	if err == nil {
		t.Fatal("Send() error = nil, want error")
	}
	if arq.GetState() != StateError {
		t.Errorf("GetState() = %v, want StateError", arq.GetState())
	}
}

func TestSend_PayloadTooLarge(t *testing.T) {
	arq, _ := createTestARQ(nil)
	largePayload := make([]byte, frame.MaxPayloadSize+1)

	err := arq.Send(largePayload)

	if err == nil {
		t.Error("Send() error = nil, want error for large payload")
	}
}

func TestSend_ReceiveErrorInWaitForAck(t *testing.T) {
	config := &Config{
		MaxRetries: 0,
		Timeout:    50 * time.Millisecond,
	}
	arq, mock := createTestARQ(config)

	// Trigger receive error during waitForAck
	mock.SetReceiveError(errors.New("receive failed"))

	err := arq.Send([]byte{0x01})

	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Errorf("Send() error = %v, want ErrMaxRetriesExceeded", err)
	}
}

func TestSendControlFrame_Errors(t *testing.T) {
	mock := NewMockTransport()

	t.Run("TransportNil", func(t *testing.T) {
		arq := NewStopAndWait(nil, nil, frame.NewEncoder(), frame.NewDecoder())
		// sendControlFrame is private, but we can trigger it via Receive()
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		_, err := arq.Receive()
		if !errors.Is(err, ErrTransportNil) {
			t.Errorf("Receive() error = %v, want ErrTransportNil", err)
		}
	})

	t.Run("EncoderNil", func(t *testing.T) {
		arq := NewStopAndWait(nil, mock, nil, frame.NewDecoder())
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		_, err := arq.Receive()
		if !errors.Is(err, ErrEncoderNil) {
			t.Errorf("Receive() error = %v, want ErrEncoderNil", err)
		}
	})

	t.Run("NewFrameError", func(t *testing.T) {
		// NewFrame fails if payload > MaxPayloadSize.
		// Control frames have nil payload, so this branch is hard to hit via sendControlFrame.
		// However, we can use a mock encoder to simulate the next error branch.
	})

	t.Run("EncodeError", func(t *testing.T) {
		encoder := &MockEncoder{
			encodeFn: func(f *frame.Frame) ([]byte, error) {
				return nil, errors.New("encode failed")
			},
		}
		arq := NewStopAndWait(nil, mock, encoder, frame.NewDecoder())
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		payload, err := arq.Receive()
		// With best-effort ACK, encoding errors are ignored.
		// The payload should still be returned successfully.
		if err != nil {
			t.Errorf("Receive() error = %v, want nil (ACK errors are best-effort)", err)
		}
		if payload == nil {
			t.Error("Receive() payload = nil, want payload despite ACK encode failure")
		}
	})
}

// ============================================================================
// Receive() Tests
// ============================================================================

func TestReceive_Success(t *testing.T) {
	arq, mock := createTestARQ(nil)
	expectedPayload := []byte{0x01, 0x02, 0x03}

	// Queue command frame with sequence 0
	mock.QueueResponse(createCommandFrame(0, expectedPayload))

	payload, err := arq.Receive()

	if err != nil {
		t.Errorf("Receive() error = %v, want nil", err)
	}
	if string(payload) != string(expectedPayload) {
		t.Errorf("Receive() payload = %v, want %v", payload, expectedPayload)
	}
	if arq.GetRxSequence() != 1 {
		t.Errorf("GetRxSequence() = %d, want 1", arq.GetRxSequence())
	}
	// Verify ACK was sent
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (ACK)", mock.GetSentCount())
	}
}

func TestReceive_DuplicateFrame(t *testing.T) {
	arq, mock := createTestARQ(nil)
	arq.SetStateForTesting(StateIdle, 0, 1, 0) // rxSequence = 1, so seq 0 is duplicate

	// Queue duplicate frame (seq 0 when expecting seq 1)
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))

	_, err := arq.Receive()

	if !errors.Is(err, ErrDuplicateFrame) {
		t.Errorf("Receive() error = %v, want ErrDuplicateFrame", err)
	}
	// ACK should still be sent for duplicate
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (ACK for duplicate)", mock.GetSentCount())
	}
}

func TestReceive_OutOfSequence(t *testing.T) {
	arq, mock := createTestARQ(nil)
	// Expecting seq 0, send seq 5
	mock.QueueResponse(createCommandFrame(5, []byte{0x01}))

	_, err := arq.Receive()

	if !errors.Is(err, ErrInvalidSequence) {
		t.Errorf("Receive() error = %v, want ErrInvalidSequence", err)
	}
	// NACK should be sent
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (NACK)", mock.GetSentCount())
	}
}

func TestReceive_CRCError(t *testing.T) {
	arq, mock := createTestARQ(nil)

	// Queue corrupted frame (invalid CRC)
	corrupted := createCommandFrame(0, []byte{0x01})
	corrupted[len(corrupted)-1] ^= 0xFF // Corrupt CRC

	mock.QueueResponse(corrupted)

	_, err := arq.Receive()

	if !errors.Is(err, frame.ErrInvalidCRC) {
		t.Errorf("Receive() error = %v, want ErrInvalidCRC", err)
	}
	// NACK should be sent
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (NACK)", mock.GetSentCount())
	}
}

func TestReceive_SequenceIncrement(t *testing.T) {
	arq, mock := createTestARQ(nil)

	// Receive three frames in sequence
	for i := 0; i < 3; i++ {
		mock.QueueResponse(createCommandFrame(uint16(i), []byte{byte(i)}))
		_, err := arq.Receive()
		if err != nil {
			t.Fatalf("Receive() iteration %d error = %v", i, err)
		}
	}

	if arq.GetRxSequence() != 3 {
		t.Errorf("GetRxSequence() = %d, want 3", arq.GetRxSequence())
	}
}

func TestReceive_SequenceWraparound(t *testing.T) {
	arq, mock := createTestARQ(nil)
	arq.SetStateForTesting(StateIdle, 0, DefaultSequenceMax, 0)

	mock.QueueResponse(createCommandFrame(DefaultSequenceMax, []byte{0x01}))

	_, err := arq.Receive()

	if err != nil {
		t.Errorf("Receive() error = %v, want nil", err)
	}
	if arq.GetRxSequence() != 0 {
		t.Errorf("GetRxSequence() = %d, want 0 (wrapped)", arq.GetRxSequence())
	}
}

func TestReceive_UnexpectedFrameType(t *testing.T) {
	arq, mock := createTestARQ(nil)

	// Queue ACK frame instead of command/response
	mock.QueueResponse(createAckFrame(0))

	_, err := arq.Receive()

	if !errors.Is(err, ErrUnexpectedFrameType) {
		t.Errorf("Receive() error = %v, want ErrUnexpectedFrameType", err)
	}
}

func TestReceive_NilTransport(t *testing.T) {
	arq := NewStopAndWait(nil, nil, frame.NewEncoder(), frame.NewDecoder())

	_, err := arq.Receive()

	if !errors.Is(err, ErrTransportNil) {
		t.Errorf("Receive() error = %v, want ErrTransportNil", err)
	}
}

func TestReceive_NilDecoder(t *testing.T) {
	mock := NewMockTransport()
	arq := NewStopAndWait(nil, mock, frame.NewEncoder(), nil)

	_, err := arq.Receive()

	if !errors.Is(err, ErrDecoderNil) {
		t.Errorf("Receive() error = %v, want ErrDecoderNil", err)
	}
}

func TestReceive_TransportError(t *testing.T) {
	arq, mock := createTestARQ(nil)
	mock.SetReceiveError(errors.New("transport receive failed"))

	_, err := arq.Receive()

	if err == nil {
		t.Error("Receive() error = nil, want error")
	}
}

func TestReceive_AckError(t *testing.T) {
	arq, mock := createTestARQ(nil)
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
	mock.SetSendError(errors.New("ack failed"))

	payload, err := arq.Receive()

	// With best-effort ACK, send errors are ignored.
	// The payload should still be returned successfully.
	if err != nil {
		t.Errorf("Receive() error = %v, want nil (ACK errors are best-effort)", err)
	}
	if payload == nil {
		t.Error("Receive() payload = nil, want payload despite ACK send failure")
	}
}

func TestReceive_NackError(t *testing.T) {
	arq, mock := createTestARQ(nil)
	// Out of sequence to trigger NACK
	mock.QueueResponse(createCommandFrame(5, []byte{0x01}))
	mock.SetSendError(errors.New("nack failed"))

	_, err := arq.Receive()

	// With best-effort NACK, send errors are ignored.
	// But ErrInvalidSequence is still returned for out-of-sequence frames.
	if !errors.Is(err, ErrInvalidSequence) {
		t.Errorf("Receive() error = %v, want ErrInvalidSequence", err)
	}
}

func TestReceive_DuplicateAckError(t *testing.T) {
	arq, mock := createTestARQ(nil)
	arq.SetStateForTesting(StateIdle, 0, 1, 0) // Expecting 1, seq 0 is duplicate
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
	mock.SetSendError(errors.New("ack failed"))

	_, err := arq.Receive()

	// With best-effort ACK, send errors are ignored.
	// But ErrDuplicateFrame is still returned for duplicate frames.
	if !errors.Is(err, ErrDuplicateFrame) {
		t.Errorf("Receive() error = %v, want ErrDuplicateFrame", err)
	}
}

// ============================================================================
// Integration Tests
// ============================================================================

func TestSendReceive_FullRoundTrip(t *testing.T) {
	// Simulate a bidirectional exchange
	arq, mock := createTestARQ(nil)

	// Send a command
	mock.QueueResponse(createAckFrame(0))
	err := arq.Send([]byte("hello"))
	if err != nil {
		t.Fatalf("Send() error = %v", err)
	}

	// Receive a response
	mock.QueueResponse(createCommandFrame(0, []byte("world")))
	payload, err := arq.Receive()
	if err != nil {
		t.Fatalf("Receive() error = %v", err)
	}
	if string(payload) != "world" {
		t.Errorf("payload = %s, want world", string(payload))
	}

	// Verify final state
	if arq.GetTxSequence() != 1 {
		t.Errorf("GetTxSequence() = %d, want 1", arq.GetTxSequence())
	}
	if arq.GetRxSequence() != 1 {
		t.Errorf("GetRxSequence() = %d, want 1", arq.GetRxSequence())
	}
}

func TestResetAfterError(t *testing.T) {
	config := &Config{
		MaxRetries: 0,
		Timeout:    1 * time.Millisecond,
	}
	arq, mock := createTestARQ(config)

	// Force error state
	err := arq.Send([]byte{0x01})
	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Fatalf("expected ErrMaxRetriesExceeded, got %v", err)
	}

	// Reset and try again
	arq.Reset()
	mock.QueueResponse(createAckFrame(0))
	err = arq.Send([]byte{0x01})
	if err != nil {
		t.Errorf("Send() after Reset error = %v, want nil", err)
	}
}
