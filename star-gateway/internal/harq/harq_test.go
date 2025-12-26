// Package harq tests for HARQ protocol implementation.
//
// STAR Project - Texas A&M University
// December 2025
package harq

import (
	"errors"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
)

// ============================================================================
// Test Helpers
// ============================================================================

// createTestHARQ creates a StopAndWait with mock dependencies for testing.
// Uses legacy ARQ mode (FEC disabled) for backward compatibility.
func createTestHARQ(config *Config) (*StopAndWait, *MockTransport) {
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	harq := NewStopAndWait(config, mock, encoder, decoder)
	return harq, mock
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

func TestHARQConstants(t *testing.T) {
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
		{StateCombining, "COMBINING"},
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

func TestDefaultConfig(t *testing.T) {
	config := DefaultConfig()

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

	t.Run("FECEnabled", func(t *testing.T) {
		if !config.FECEnabled {
			t.Error("FECEnabled = false, want true (default)")
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
			harq := NewStopAndWait(tc.config, mock, encoder, decoder)

			if harq == nil {
				t.Fatal("expected HARQ, got nil")
			}
			if harq.Config().MaxRetries != tc.expectedRetries {
				t.Errorf("MaxRetries = %d, want %d", harq.Config().MaxRetries, tc.expectedRetries)
			}
			if harq.Config().Timeout != tc.expectedTimeout {
				t.Errorf("Timeout = %v, want %v", harq.Config().Timeout, tc.expectedTimeout)
			}
			// StopAndWait always has FEC disabled
			if harq.Config().FECEnabled {
				t.Error("FECEnabled = true, want false for StopAndWait")
			}
		})
	}
}

func TestStopAndWaitInitialState(t *testing.T) {
	harq, _ := createTestHARQ(nil)

	t.Run("State", func(t *testing.T) {
		if harq.GetState() != StateIdle {
			t.Errorf("GetState() = %v, want StateIdle", harq.GetState())
		}
	})

	t.Run("TxSequence", func(t *testing.T) {
		if harq.GetTxSequence() != 0 {
			t.Errorf("GetTxSequence() = %d, want 0", harq.GetTxSequence())
		}
	})

	t.Run("RxSequence", func(t *testing.T) {
		if harq.GetRxSequence() != 0 {
			t.Errorf("GetRxSequence() = %d, want 0", harq.GetRxSequence())
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
			harq, _ := createTestHARQ(nil)
			harq.SetStateForTesting(tc.setupState, tc.setupTxSeq, tc.setupRxSeq, tc.setupRetry)

			harq.Reset()

			if harq.GetState() != StateIdle {
				t.Errorf("GetState() = %v, want StateIdle after Reset", harq.GetState())
			}
			if harq.GetTxSequence() != 0 {
				t.Errorf("GetTxSequence() = %d, want 0 after Reset", harq.GetTxSequence())
			}
			if harq.GetRxSequence() != 0 {
				t.Errorf("GetRxSequence() = %d, want 0 after Reset", harq.GetRxSequence())
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
	harq, mock := createTestHARQ(nil)
	payload := []byte{0x01, 0x02, 0x03}

	// Queue ACK response with sequence 0
	mock.QueueResponse(createAckFrame(0))

	err := harq.Send(payload)

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if harq.GetState() != StateIdle {
		t.Errorf("GetState() = %v, want StateIdle", harq.GetState())
	}
	if harq.GetTxSequence() != 1 {
		t.Errorf("GetTxSequence() = %d, want 1", harq.GetTxSequence())
	}
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1", mock.GetSentCount())
	}
}

func TestSend_SequenceIncrement(t *testing.T) {
	harq, mock := createTestHARQ(nil)

	// Send three messages
	for i := 0; i < 3; i++ {
		mock.QueueResponse(createAckFrame(uint16(i)))
		err := harq.Send([]byte{byte(i)})
		if err != nil {
			t.Fatalf("Send() iteration %d error = %v", i, err)
		}
	}

	if harq.GetTxSequence() != 3 {
		t.Errorf("GetTxSequence() = %d, want 3", harq.GetTxSequence())
	}
}

func TestSend_TimeoutThenSuccess(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    5 * time.Millisecond, // Short timeout for test
	}
	harq, mock := createTestHARQ(config)

	// First attempt times out (no response queued), then ACK
	// We need to queue the ACK after a delay so first attempt times out
	go func() {
		time.Sleep(10 * time.Millisecond) // After first timeout
		mock.QueueResponse(createAckFrame(0))
	}()

	err := harq.Send([]byte{0x01})

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
	harq, mock := createTestHARQ(config)

	// Don't queue any responses - all attempts will timeout
	_ = mock // unused but needed for setup

	err := harq.Send([]byte{0x01})

	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Errorf("Send() error = %v, want ErrMaxRetriesExceeded", err)
	}
	if harq.GetState() != StateError {
		t.Errorf("GetState() = %v, want StateError", harq.GetState())
	}
}

func TestSend_NackThenAck(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    50 * time.Millisecond,
	}
	harq, mock := createTestHARQ(config)

	// First response is NACK, second is ACK
	mock.QueueResponses(
		createNackFrame(0),
		createAckFrame(0),
	)

	err := harq.Send([]byte{0x01})

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if mock.GetSentCount() != 2 {
		t.Errorf("sent count = %d, want 2", mock.GetSentCount())
	}
}

func TestSend_TransportError(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	mock.SetSendError(errors.New("transport failed"))

	err := harq.Send([]byte{0x01})

	if err == nil {
		t.Error("Send() error = nil, want error")
	}
	if harq.GetState() != StateError {
		t.Errorf("GetState() = %v, want StateError", harq.GetState())
	}
}

func TestSend_SequenceWraparound(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	harq.SetStateForTesting(StateIdle, DefaultSequenceMax, 0, 0)

	mock.QueueResponse(createAckFrame(DefaultSequenceMax))

	err := harq.Send([]byte{0x01})

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if harq.GetTxSequence() != 0 {
		t.Errorf("GetTxSequence() = %d, want 0 (wrapped)", harq.GetTxSequence())
	}
}

func TestSend_RetransmitFlag(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    5 * time.Millisecond,
	}
	harq, mock := createTestHARQ(config)

	// Queue ACK after delay to force retry
	go func() {
		time.Sleep(10 * time.Millisecond)
		mock.QueueResponse(createAckFrame(0))
	}()

	err := harq.Send([]byte{0x01})
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
	harq, _ := createTestHARQ(nil)
	harq.SetStateForTesting(StateError, 0, 0, 0)

	err := harq.Send([]byte{0x01})

	if !errors.Is(err, ErrInErrorState) {
		t.Errorf("Send() error = %v, want ErrInErrorState", err)
	}
}

func TestSend_NilTransport(t *testing.T) {
	harq := NewStopAndWait(nil, nil, frame.NewEncoder(), frame.NewDecoder())

	err := harq.Send([]byte{0x01})

	if !errors.Is(err, ErrTransportNil) {
		t.Errorf("Send() error = %v, want ErrTransportNil", err)
	}
}

func TestSend_NilEncoder(t *testing.T) {
	mock := NewMockTransport()
	harq := NewStopAndWait(nil, mock, nil, frame.NewDecoder())

	err := harq.Send([]byte{0x01})

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
	harq := NewStopAndWait(nil, mock, encoder, decoder)

	err := harq.Send([]byte{0x01})

	if err == nil {
		t.Fatal("Send() error = nil, want error")
	}
	if harq.GetState() != StateError {
		t.Errorf("GetState() = %v, want StateError", harq.GetState())
	}
}

func TestSend_PayloadTooLarge(t *testing.T) {
	harq, _ := createTestHARQ(nil)
	largePayload := make([]byte, frame.MaxPayloadSize+1)

	err := harq.Send(largePayload)

	if err == nil {
		t.Error("Send() error = nil, want error for large payload")
	}
}

func TestSend_ReceiveErrorInWaitForAck(t *testing.T) {
	config := &Config{
		MaxRetries: 0,
		Timeout:    50 * time.Millisecond,
	}
	harq, mock := createTestHARQ(config)

	// Trigger receive error during waitForAck
	mock.SetReceiveError(errors.New("receive failed"))

	err := harq.Send([]byte{0x01})

	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Errorf("Send() error = %v, want ErrMaxRetriesExceeded", err)
	}
}

func TestSendControlFrame_Errors(t *testing.T) {
	mock := NewMockTransport()

	t.Run("TransportNil", func(t *testing.T) {
		harq := NewStopAndWait(nil, nil, frame.NewEncoder(), frame.NewDecoder())
		// sendControlFrame is private, but we can trigger it via Receive()
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		_, err := harq.Receive()
		if !errors.Is(err, ErrTransportNil) {
			t.Errorf("Receive() error = %v, want ErrTransportNil", err)
		}
	})

	t.Run("EncoderNil", func(t *testing.T) {
		harq := NewStopAndWait(nil, mock, nil, frame.NewDecoder())
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		_, err := harq.Receive()
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
		harq := NewStopAndWait(nil, mock, encoder, frame.NewDecoder())
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		payload, err := harq.Receive()
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
	harq, mock := createTestHARQ(nil)
	expectedPayload := []byte{0x01, 0x02, 0x03}

	// Queue command frame with sequence 0
	mock.QueueResponse(createCommandFrame(0, expectedPayload))

	payload, err := harq.Receive()

	if err != nil {
		t.Errorf("Receive() error = %v, want nil", err)
	}
	if string(payload) != string(expectedPayload) {
		t.Errorf("Receive() payload = %v, want %v", payload, expectedPayload)
	}
	if harq.GetRxSequence() != 1 {
		t.Errorf("GetRxSequence() = %d, want 1", harq.GetRxSequence())
	}
	// Verify ACK was sent
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (ACK)", mock.GetSentCount())
	}
}

func TestReceive_DuplicateFrame(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	harq.SetStateForTesting(StateIdle, 0, 1, 0) // rxSequence = 1, so seq 0 is duplicate

	// Queue duplicate frame (seq 0 when expecting seq 1)
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))

	_, err := harq.Receive()

	if !errors.Is(err, ErrDuplicateFrame) {
		t.Errorf("Receive() error = %v, want ErrDuplicateFrame", err)
	}
	// ACK should still be sent for duplicate
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (ACK for duplicate)", mock.GetSentCount())
	}
}

func TestReceive_OutOfSequence(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	// Expecting seq 0, send seq 5
	mock.QueueResponse(createCommandFrame(5, []byte{0x01}))

	_, err := harq.Receive()

	if !errors.Is(err, ErrInvalidSequence) {
		t.Errorf("Receive() error = %v, want ErrInvalidSequence", err)
	}
	// NACK should be sent
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (NACK)", mock.GetSentCount())
	}
}

func TestReceive_CRCError(t *testing.T) {
	harq, mock := createTestHARQ(nil)

	// Queue corrupted frame (invalid CRC)
	corrupted := createCommandFrame(0, []byte{0x01})
	corrupted[len(corrupted)-1] ^= 0xFF // Corrupt CRC

	mock.QueueResponse(corrupted)

	_, err := harq.Receive()

	if !errors.Is(err, frame.ErrInvalidCRC) {
		t.Errorf("Receive() error = %v, want ErrInvalidCRC", err)
	}
	// NACK should be sent
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (NACK)", mock.GetSentCount())
	}
}

func TestReceive_SequenceIncrement(t *testing.T) {
	harq, mock := createTestHARQ(nil)

	// Receive three frames in sequence
	for i := 0; i < 3; i++ {
		mock.QueueResponse(createCommandFrame(uint16(i), []byte{byte(i)}))
		_, err := harq.Receive()
		if err != nil {
			t.Fatalf("Receive() iteration %d error = %v", i, err)
		}
	}

	if harq.GetRxSequence() != 3 {
		t.Errorf("GetRxSequence() = %d, want 3", harq.GetRxSequence())
	}
}

func TestReceive_SequenceWraparound(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	harq.SetStateForTesting(StateIdle, 0, DefaultSequenceMax, 0)

	mock.QueueResponse(createCommandFrame(DefaultSequenceMax, []byte{0x01}))

	_, err := harq.Receive()

	if err != nil {
		t.Errorf("Receive() error = %v, want nil", err)
	}
	if harq.GetRxSequence() != 0 {
		t.Errorf("GetRxSequence() = %d, want 0 (wrapped)", harq.GetRxSequence())
	}
}

func TestReceive_UnexpectedFrameType(t *testing.T) {
	harq, mock := createTestHARQ(nil)

	// Queue ACK frame instead of command/response
	mock.QueueResponse(createAckFrame(0))

	_, err := harq.Receive()

	if !errors.Is(err, ErrUnexpectedFrameType) {
		t.Errorf("Receive() error = %v, want ErrUnexpectedFrameType", err)
	}
}

func TestReceive_NilTransport(t *testing.T) {
	harq := NewStopAndWait(nil, nil, frame.NewEncoder(), frame.NewDecoder())

	_, err := harq.Receive()

	if !errors.Is(err, ErrTransportNil) {
		t.Errorf("Receive() error = %v, want ErrTransportNil", err)
	}
}

func TestReceive_NilDecoder(t *testing.T) {
	mock := NewMockTransport()
	harq := NewStopAndWait(nil, mock, frame.NewEncoder(), nil)

	_, err := harq.Receive()

	if !errors.Is(err, ErrDecoderNil) {
		t.Errorf("Receive() error = %v, want ErrDecoderNil", err)
	}
}

func TestReceive_TransportError(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	mock.SetReceiveError(errors.New("transport receive failed"))

	_, err := harq.Receive()

	if err == nil {
		t.Error("Receive() error = nil, want error")
	}
}

func TestReceive_AckError(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
	mock.SetSendError(errors.New("ack failed"))

	payload, err := harq.Receive()

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
	harq, mock := createTestHARQ(nil)
	// Out of sequence to trigger NACK
	mock.QueueResponse(createCommandFrame(5, []byte{0x01}))
	mock.SetSendError(errors.New("nack failed"))

	_, err := harq.Receive()

	// With best-effort NACK, send errors are ignored.
	// But ErrInvalidSequence is still returned for out-of-sequence frames.
	if !errors.Is(err, ErrInvalidSequence) {
		t.Errorf("Receive() error = %v, want ErrInvalidSequence", err)
	}
}

func TestReceive_DuplicateAckError(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	harq.SetStateForTesting(StateIdle, 0, 1, 0) // Expecting 1, seq 0 is duplicate
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
	mock.SetSendError(errors.New("ack failed"))

	_, err := harq.Receive()

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
	harq, mock := createTestHARQ(nil)

	// Send a command
	mock.QueueResponse(createAckFrame(0))
	err := harq.Send([]byte("hello"))
	if err != nil {
		t.Fatalf("Send() error = %v", err)
	}

	// Receive a response
	mock.QueueResponse(createCommandFrame(0, []byte("world")))
	payload, err := harq.Receive()
	if err != nil {
		t.Fatalf("Receive() error = %v", err)
	}
	if string(payload) != "world" {
		t.Errorf("payload = %s, want world", string(payload))
	}

	// Verify final state
	if harq.GetTxSequence() != 1 {
		t.Errorf("GetTxSequence() = %d, want 1", harq.GetTxSequence())
	}
	if harq.GetRxSequence() != 1 {
		t.Errorf("GetRxSequence() = %d, want 1", harq.GetRxSequence())
	}
}

func TestResetAfterError(t *testing.T) {
	config := &Config{
		MaxRetries: 0,
		Timeout:    1 * time.Millisecond,
	}
	harq, mock := createTestHARQ(config)

	// Force error state
	err := harq.Send([]byte{0x01})
	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Fatalf("expected ErrMaxRetriesExceeded, got %v", err)
	}

	// Reset and try again
	harq.Reset()
	mock.QueueResponse(createAckFrame(0))
	err = harq.Send([]byte{0x01})
	if err != nil {
		t.Errorf("Send() after Reset error = %v, want nil", err)
	}
}

// ============================================================================
// bytesToSoftBits Tests
// ============================================================================

func TestBytesToSoftBits(t *testing.T) {
	tests := []struct {
		name     string
		input    []byte
		expected []int8
	}{
		{
			name:     "all_zeros",
			input:    []byte{0x00},
			expected: []int8{-127, -127, -127, -127, -127, -127, -127, -127},
		},
		{
			name:     "all_ones",
			input:    []byte{0xFF},
			expected: []int8{127, 127, 127, 127, 127, 127, 127, 127},
		},
		{
			name:  "alternating",
			input: []byte{0xAA}, // 10101010
			expected: []int8{127, -127, 127, -127, 127, -127, 127, -127},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			softBits := bytesToSoftBits(tc.input)
			if len(softBits) != len(tc.expected) {
				t.Fatalf("bytesToSoftBits() len = %d, want %d", len(softBits), len(tc.expected))
			}
			for i, exp := range tc.expected {
				if int8(softBits[i]) != exp {
					t.Errorf("softBits[%d] = %d, want %d", i, softBits[i], exp)
				}
			}
		})
	}
}
