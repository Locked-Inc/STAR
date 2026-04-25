// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package harq tests for HARQ protocol implementation.
//
// STAR Project - Texas A&M University
// December 2025
package harq

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
)

// ============================================================================
// Test Helpers
// ============================================================================

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

func createTestHARQ(config *Config) (*ChaseCombining, *MockTransport) {
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	if config == nil {
		config = DefaultConfig()
	}

	var fecEncoder fec.Encoder
	var fecDecoder fec.Decoder
	if config.FECEnabled {
		fecEncoder = fec.NewConvolutionalEncoder()
		fecDecoder = fec.NewViterbiDecoder()
	}

	harq := NewChaseCombining(config, mock, encoder, decoder, fecEncoder, fecDecoder)
	return harq, mock
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
// ChaseCombining Constructor Tests
// ============================================================================

func TestNewChaseCombining(t *testing.T) {
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()

	const (
		testMaxRetries   = 5
		testTimeout      = 20 * time.Millisecond
		testZeroRetries  = 0
		testShortTimeout = 5 * time.Millisecond
		testFECEnabled   = true
	)

	tests := []struct {
		name               string
		config             *Config
		expectedRetries    int
		expectedTimeout    time.Duration
		expectedFECEnabled bool
	}{
		{
			name:               "nil_config_uses_defaults",
			config:             nil,
			expectedRetries:    DefaultMaxRetries,
			expectedTimeout:    DefaultTimeout,
			expectedFECEnabled: true,
		},
		{
			name: "custom_config",
			config: &Config{
				MaxRetries: testMaxRetries,
				Timeout:    testTimeout,
				FECEnabled: testFECEnabled,
			},
			expectedRetries:    testMaxRetries,
			expectedTimeout:    testTimeout,
			expectedFECEnabled: testFECEnabled,
		},
		{
			name: "zero_retries",
			config: &Config{
				MaxRetries: testZeroRetries,
				Timeout:    testShortTimeout,
				FECEnabled: false,
			},
			expectedRetries:    testZeroRetries,
			expectedTimeout:    testShortTimeout,
			expectedFECEnabled: false,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			harq := NewChaseCombining(
				tc.config,
				mock,
				encoder,
				decoder,
				fec.NewConvolutionalEncoder(),
				fec.NewViterbiDecoder(),
			)

			if harq == nil {
				t.Fatal("expected HARQ, got nil")
				return
			}
			if harq.Config().MaxRetries != tc.expectedRetries {
				t.Errorf("MaxRetries = %d, want %d", harq.Config().MaxRetries, tc.expectedRetries)
			}
			if harq.Config().Timeout != tc.expectedTimeout {
				t.Errorf("Timeout = %v, want %v", harq.Config().Timeout, tc.expectedTimeout)
			}
			if harq.Config().FECEnabled != tc.expectedFECEnabled {
				t.Errorf("FECEnabled = %v, want %v", harq.Config().FECEnabled, tc.expectedFECEnabled)
			}
		})
	}
}

func TestChaseCombiningInitialState(t *testing.T) {
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

func TestChaseCombiningReset(t *testing.T) {
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
	priority := PriorityNormal

	// Queue ACK response with sequence 0
	mock.QueueResponse(createAckFrame(0))

	err := harq.Send(context.Background(), payload, priority)

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
		err := harq.Send(context.Background(), []byte{byte(i)})
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
	mock.SetReceiveDelay(5 * time.Millisecond) // Ensure attempts take time

	// First attempt times out (no response queued), then ACK
	// We need to queue the ACK after a delay so first attempt times out
	go func() {
		time.Sleep(15 * time.Millisecond) // After first timeout (5ms) + delay (5ms)
		mock.QueueResponse(createAckFrame(0))
	}()

	err := harq.Send(context.Background(), []byte{0x01})

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

	err := harq.Send(context.Background(), []byte{0x01})

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

	err := harq.Send(context.Background(), []byte{0x01})

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

	err := harq.Send(context.Background(), []byte{0x01})

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

	err := harq.Send(context.Background(), []byte{0x01})

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
	mock.SetReceiveDelay(5 * time.Millisecond)

	// Queue ACK after delay to force retry
	go func() {
		time.Sleep(15 * time.Millisecond)
		mock.QueueResponse(createAckFrame(0))
	}()

	err := harq.Send(context.Background(), []byte{0x01})
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

	err := harq.Send(context.Background(), []byte{0x01})

	if !errors.Is(err, ErrInErrorState) {
		t.Errorf("Send() error = %v, want ErrInErrorState", err)
	}
}

func TestSend_NilTransport(t *testing.T) {
	harq := NewChaseCombining(
		nil,
		nil,
		frame.NewEncoder(),
		frame.NewDecoder(),
		fec.NewConvolutionalEncoder(),
		fec.NewViterbiDecoder(),
	)

	err := harq.Send(context.Background(), []byte{0x01})

	if !errors.Is(err, ErrTransportNil) {
		t.Errorf("Send() error = %v, want ErrTransportNil", err)
	}
}

func TestSend_NilEncoder(t *testing.T) {
	mock := NewMockTransport()
	harq := NewChaseCombining(
		nil,
		mock,
		nil,
		frame.NewDecoder(),
		fec.NewConvolutionalEncoder(),
		fec.NewViterbiDecoder(),
	)

	err := harq.Send(context.Background(), []byte{0x01})

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
	harq := NewChaseCombining(
		nil,
		mock,
		encoder,
		decoder,
		fec.NewConvolutionalEncoder(),
		fec.NewViterbiDecoder(),
	)

	err := harq.Send(context.Background(), []byte{0x01})

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

	err := harq.Send(context.Background(), largePayload)

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

	err := harq.Send(context.Background(), []byte{0x01})

	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Errorf("Send() error = %v, want ErrMaxRetriesExceeded", err)
	}
}

func TestSendControlFrame_Errors(t *testing.T) {
	mock := NewMockTransport()

	t.Run("TransportNil", func(t *testing.T) {
		harq := NewChaseCombining(
			nil,
			nil,
			frame.NewEncoder(),
			frame.NewDecoder(),
			fec.NewConvolutionalEncoder(),
			fec.NewViterbiDecoder(),
		)
		// sendControlFrame is private, but we can trigger it via Receive()
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		_, err := harq.Receive(context.Background())
		if !errors.Is(err, ErrTransportNil) {
			t.Errorf("Receive() error = %v, want ErrTransportNil", err)
		}
	})

	t.Run("EncoderNil", func(t *testing.T) {
		harq := NewChaseCombining(
			nil,
			mock,
			nil,
			frame.NewDecoder(),
			fec.NewConvolutionalEncoder(),
			fec.NewViterbiDecoder(),
		)
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		result, err := harq.Receive(context.Background())
		// With best-effort ACK, encoder nil errors are ignored.
		// The payload should still be returned successfully.
		if err != nil {
			t.Errorf("Receive() error = %v, want nil (ACK errors are best-effort)", err)
		}
		if result == nil {
			t.Error("Receive() result = nil, want result despite ACK encoder nil")
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
		harq := NewChaseCombining(
			nil,
			mock,
			encoder,
			frame.NewDecoder(),
			fec.NewConvolutionalEncoder(),
			fec.NewViterbiDecoder(),
		)
		mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
		payload, err := harq.Receive(context.Background())
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

	result, err := harq.Receive(context.Background())

	if err != nil {
		t.Errorf("Receive() error = %v, want nil", err)
	}
	if result == nil {
		t.Fatal("Receive() result = nil, want non-nil")
	}
	if string(result.Payload) != string(expectedPayload) {
		t.Errorf("Receive() payload = %v, want %v", result.Payload, expectedPayload)
	}
	if result.Metadata.Sequence != 0 {
		t.Errorf("Receive() metadata.Sequence = %d, want 0", result.Metadata.Sequence)
	}
	if harq.GetRxSequence() != 1 {
		t.Errorf("GetRxSequence() = %d, want 1", harq.GetRxSequence())
	}
	// Verify ACK was sent
	if mock.GetSentCount() != 2 {
		t.Errorf("sent count = %d, want 2 (Receive dummy + ACK)", mock.GetSentCount())
	}
}

func TestReceive_DuplicateFrame(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	harq.SetStateForTesting(StateIdle, 0, 1, 0) // rxSequence = 1, so seq 0 is duplicate

	// Queue duplicate frame (seq 0 when expecting seq 1)
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, ErrDuplicateFrame) {
		t.Errorf("Receive() error = %v, want ErrDuplicateFrame", err)
	}
	// ACK should still be sent for duplicate
	if mock.GetSentCount() != 2 {
		t.Errorf("sent count = %d, want 2 (Receive dummy + ACK for duplicate)", mock.GetSentCount())
	}
}

func TestReceive_OutOfSequence(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	// Expecting seq 0, send seq 5
	mock.QueueResponse(createCommandFrame(5, []byte{0x01}))

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, ErrInvalidSequence) {
		t.Errorf("Receive() error = %v, want ErrInvalidSequence", err)
	}
	// NACK should be sent
	if mock.GetSentCount() != 2 {
		t.Errorf("sent count = %d, want 2 (Receive dummy + NACK)", mock.GetSentCount())
	}
}

func TestReceive_CRCError(t *testing.T) {
	harq, mock := createTestHARQ(nil)

	// Queue corrupted frame (invalid CRC)
	corrupted := createCommandFrame(0, []byte{0x01})
	corrupted[len(corrupted)-1] ^= 0xFF // Corrupt CRC

	mock.QueueResponse(corrupted)

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, frame.ErrInvalidCRC) {
		t.Errorf("Receive() error = %v, want ErrInvalidCRC", err)
	}
	// NACK cannot be sent for CRC error (sequence unknown)
	if mock.GetSentCount() != 1 {
		t.Errorf("sent count = %d, want 1 (Receive dummy only)", mock.GetSentCount())
	}
}

func TestReceive_SequenceIncrement(t *testing.T) {
	harq, mock := createTestHARQ(nil)

	// Receive three frames in sequence
	for i := 0; i < 3; i++ {
		mock.QueueResponse(createCommandFrame(uint16(i), []byte{byte(i)}))
		_, err := harq.Receive(context.Background())
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

	_, err := harq.Receive(context.Background())

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

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, ErrUnexpectedFrameType) {
		t.Errorf("Receive() error = %v, want ErrUnexpectedFrameType", err)
	}
}

func TestReceive_NilTransport(t *testing.T) {
	harq := NewChaseCombining(
		nil,
		nil,
		frame.NewEncoder(),
		frame.NewDecoder(),
		fec.NewConvolutionalEncoder(),
		fec.NewViterbiDecoder(),
	)

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, ErrTransportNil) {
		t.Errorf("Receive() error = %v, want ErrTransportNil", err)
	}
}

func TestReceive_NilDecoder(t *testing.T) {
	mock := NewMockTransport()
	harq := NewChaseCombining(
		nil,
		mock,
		frame.NewEncoder(),
		nil,
		fec.NewConvolutionalEncoder(),
		fec.NewViterbiDecoder(),
	)

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, ErrDecoderNil) {
		t.Errorf("Receive() error = %v, want ErrDecoderNil", err)
	}
}

func TestReceive_TransportError(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	mock.SetReceiveError(errors.New("transport receive failed"))

	_, err := harq.Receive(context.Background())

	if err == nil {
		t.Error("Receive() error = nil, want error")
	}
}

func TestReceive_AckError(t *testing.T) {
	harq, mock := createTestHARQ(nil)
	mock.QueueResponse(createCommandFrame(0, []byte{0x01}))
	// Fail the 2nd transfer (ACK send), allow 1st (dummy receive)
	mock.SetSendErrorAfter(2, errors.New("ack failed"))

	payload, err := harq.Receive(context.Background())

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
	// Fail the 2nd transfer (NACK send)
	mock.SetSendErrorAfter(2, errors.New("nack failed"))

	_, err := harq.Receive(context.Background())

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
	// Fail the 2nd transfer (ACK send)
	mock.SetSendErrorAfter(2, errors.New("ack failed"))

	_, err := harq.Receive(context.Background())

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
	err := harq.Send(context.Background(), []byte("hello"))
	if err != nil {
		t.Fatalf("Send() error = %v", err)
	}

	// Receive a response
	mock.QueueResponse(createCommandFrame(0, []byte("world")))
	result, err := harq.Receive(context.Background())
	if err != nil {
		t.Fatalf("Receive() error = %v", err)
	}
	if result == nil {
		t.Fatal("Receive() result = nil, want non-nil")
	}
	if string(result.Payload) != "world" {
		t.Errorf("payload = %s, want world", string(result.Payload))
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
	err := harq.Send(context.Background(), []byte{0x01})
	if !errors.Is(err, ErrMaxRetriesExceeded) {
		t.Fatalf("expected ErrMaxRetriesExceeded, got %v", err)
	}

	// Reset and try again
	harq.Reset()
	mock.QueueResponse(createAckFrame(0))
	err = harq.Send(context.Background(), []byte{0x01})
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
			name:     "alternating",
			input:    []byte{0xAA}, // 10101010
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

// ============================================================================
// FEC-Enabled ChaseCombining Tests
// ============================================================================

// createFECCommandFrame creates an FEC-encoded command frame.
func createFECCommandFrame(seq uint16, fecEncodedPayload []byte) []byte {
	encoder := frame.NewEncoder()
	f, _ := frame.NewFrame(frame.FrameTypeCommand, fecEncodedPayload)
	f.Header.Sequence = seq
	f.Header.Flags = frame.FlagFECEnabled
	encoded, _ := encoder.Encode(f)
	return encoded
}

// MockFECEncoder implements fec.Encoder for testing.
type MockFECEncoder struct {
	encodeFn func(data []byte) ([]byte, error)
}

func (m *MockFECEncoder) Encode(data []byte) ([]byte, error) {
	if m.encodeFn != nil {
		return m.encodeFn(data)
	}
	// Default: double the data (simulating rate 1/2)
	result := make([]byte, len(data)*2)
	for i, b := range data {
		result[i*2] = b
		result[i*2+1] = ^b // Simple encoding for testing
	}
	return result, nil
}

func (m *MockFECEncoder) Rate() float64 {
	return 0.5
}

func (m *MockFECEncoder) OutputLength(inputLen int) int {
	return inputLen * 2
}

// MockFECDecoder implements fec.Decoder for testing.
type MockFECDecoder struct {
	decodeSoftFn     func(softBits []fec.SoftBit, expectedLen int) ([]byte, int, error)
	decodeHardFn     func(data []byte, expectedLen int) ([]byte, error)
	decodeCallCount  int
	failUntilCombine int // Number of decodes to fail before succeeding
}

func (m *MockFECDecoder) DecodeSoft(softBits []fec.SoftBit, expectedLen int) ([]byte, int, error) {
	m.decodeCallCount++
	if m.decodeSoftFn != nil {
		return m.decodeSoftFn(softBits, expectedLen)
	}
	// Default: fail if failUntilCombine > 0
	if m.decodeCallCount <= m.failUntilCombine {
		return nil, 0, fec.ErrDecodeFailed
	}
	// Success: return mock decoded data
	outputLen := len(softBits) / 16 // Rough approximation
	if outputLen == 0 {
		outputLen = 1
	}
	return make([]byte, outputLen), 0, nil
}

func (m *MockFECDecoder) DecodeHard(data []byte, expectedLen int) ([]byte, error) {
	if m.decodeHardFn != nil {
		return m.decodeHardFn(data, expectedLen)
	}
	return data[:len(data)/2], nil
}

func (m *MockFECDecoder) InputLength(outputLen int) int {
	return outputLen * 16
}

// createTestChaseCombining creates a ChaseCombining HARQ with mock FEC.
func createTestChaseCombining(config *Config, fecEncoder *MockFECEncoder, fecDecoder *MockFECDecoder) (*ChaseCombining, *MockTransport) {
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	if config == nil {
		config = &Config{
			MaxRetries: DefaultMaxRetries,
			Timeout:    DefaultTimeout,
			FECEnabled: true,
		}
	}
	harq := NewChaseCombining(config, mock, encoder, decoder, fecEncoder, fecDecoder)
	return harq, mock
}

func TestChaseCombining_FEC_FirstTrySuccess(t *testing.T) {
	fecEncoder := &MockFECEncoder{}
	fecDecoder := &MockFECDecoder{
		failUntilCombine: 0, // Success on first try
	}
	harq, mock := createTestChaseCombining(nil, fecEncoder, fecDecoder)

	// Queue ACK response
	mock.QueueResponse(createAckFrame(0))

	payload := []byte{0x01, 0x02, 0x03}
	err := harq.Send(context.Background(), payload)

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}
	if harq.GetState() != StateIdle {
		t.Errorf("GetState() = %v, want StateIdle", harq.GetState())
	}
	if harq.GetTxSequence() != 1 {
		t.Errorf("GetTxSequence() = %d, want 1", harq.GetTxSequence())
	}

	// Verify FEC encoding was applied
	sentData := mock.GetSentData()
	if len(sentData) != 1 {
		t.Fatalf("expected 1 sent frame, got %d", len(sentData))
	}

	// Decode frame and verify FEC flag
	frameDecoder := frame.NewDecoder()
	f, err := frameDecoder.Decode(sentData[0])
	if err != nil {
		t.Fatalf("failed to decode sent frame: %v", err)
	}
	if f.Header.Flags&frame.FlagFECEnabled == 0 {
		t.Error("sent frame should have FlagFECEnabled set")
	}
}

func TestChaseCombining_FEC_SuccessAfterCombining(t *testing.T) {
	fecEncoder := &MockFECEncoder{}
	fecDecoder := &MockFECDecoder{
		failUntilCombine: 1, // Fail first decode, succeed on second (after combining)
	}
	config := &Config{
		MaxRetries: 3,
		Timeout:    50 * time.Millisecond,
		FECEnabled: true,
	}
	harq, mock := createTestChaseCombining(config, fecEncoder, fecDecoder)

	// Create FEC-encoded payload for receive test
	fecEncodedPayload := []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06} // Mock FEC encoded

	// First frame (will fail to decode, triggers StateCombining)
	mock.QueueResponse(createFECCommandFrame(0, fecEncodedPayload))

	// First receive attempt - should fail and enter StateCombining
	_, err := harq.Receive(context.Background())
	if !errors.Is(err, ErrDecodeFailed) {
		t.Errorf("First Receive() error = %v, want ErrDecodeFailed", err)
	}
	if harq.GetState() != StateCombining {
		t.Errorf("GetState() = %v, want StateCombining", harq.GetState())
	}

	// Second frame (retransmission, combining should succeed)
	mock.QueueResponse(createFECCommandFrame(0, fecEncodedPayload))

	// Second receive - should succeed after combining
	payload, err := harq.Receive(context.Background())
	if err != nil {
		t.Errorf("Second Receive() error = %v, want nil", err)
	}
	if payload == nil {
		t.Error("payload should not be nil after successful combining")
	}
	if harq.GetState() != StateIdle {
		t.Errorf("GetState() = %v, want StateIdle after success", harq.GetState())
	}
	if harq.GetRxSequence() != 1 {
		t.Errorf("GetRxSequence() = %d, want 1", harq.GetRxSequence())
	}
}

func TestChaseCombining_FEC_FailureAfterMaxRetries(t *testing.T) {
	fecEncoder := &MockFECEncoder{}
	fecDecoder := &MockFECDecoder{
		failUntilCombine: 100, // Always fail
	}
	config := &Config{
		MaxRetries: 2,
		Timeout:    50 * time.Millisecond,
		FECEnabled: true,
	}
	harq, mock := createTestChaseCombining(config, fecEncoder, fecDecoder)

	fecEncodedPayload := []byte{0x01, 0x02, 0x03, 0x04}

	// Create an empty frame to serve as the response to NACK (peripheral sending nothing)
	emptyFrame := make([]byte, frame.MaxFrameSize)

	// Queue multiple frames (all will fail to decode)
	// For each Receive call, we need:
	// 1. The data frame (Receive -> Transfer)
	// 2. The NACK response (sendNack -> Transfer) - usually empty/zeros
	for i := 0; i < 3; i++ {
		mock.QueueResponse(createFECCommandFrame(0, fecEncodedPayload))
		mock.QueueResponse(emptyFrame)
	}

	// All receive attempts should fail
	for i := 0; i < 3; i++ {
		_, err := harq.Receive(context.Background())
		if !errors.Is(err, ErrDecodeFailed) {
			t.Errorf("Receive() attempt %d error = %v, want ErrDecodeFailed", i+1, err)
		}
	}

	// Should still be in combining state (waiting for more retries)
	if harq.GetState() != StateCombining {
		t.Errorf("GetState() = %v, want StateCombining", harq.GetState())
	}
}

func TestChaseCombining_FEC_Send_EncoderError(t *testing.T) {
	fecEncoder := &MockFECEncoder{
		encodeFn: func(data []byte) ([]byte, error) {
			return nil, errors.New("FEC encode failed")
		},
	}
	fecDecoder := &MockFECDecoder{}
	harq, _ := createTestChaseCombining(nil, fecEncoder, fecDecoder)

	err := harq.Send(context.Background(), []byte{0x01})

	if err == nil {
		t.Error("Send() error = nil, want FEC encode error")
	}
}

func TestChaseCombining_FEC_NilFECEncoder(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    DefaultTimeout,
		FECEnabled: true,
	}
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	harq := NewChaseCombining(config, mock, encoder, decoder, nil, &MockFECDecoder{})

	err := harq.Send(context.Background(), []byte{0x01})

	if !errors.Is(err, ErrFECEncoderNil) {
		t.Errorf("Send() error = %v, want ErrFECEncoderNil", err)
	}
}

func TestChaseCombining_FEC_NilFECDecoder(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    DefaultTimeout,
		FECEnabled: true,
	}
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	harq := NewChaseCombining(config, mock, encoder, decoder, &MockFECEncoder{}, nil)

	mock.QueueResponse(createFECCommandFrame(0, []byte{0x01, 0x02}))

	_, err := harq.Receive(context.Background())

	if !errors.Is(err, ErrFECDecoderNil) {
		t.Errorf("Receive() error = %v, want ErrFECDecoderNil", err)
	}
}

// ============================================================================
// Priority Flag Tests
// ============================================================================

func TestPriorityConstants(t *testing.T) {
	tests := []struct {
		name     string
		priority Priority
		expected uint8
	}{
		{"Emergency", PriorityEmergency, PriorityValueEmergency},
		{"High", PriorityHigh, PriorityValueHigh},
		{"Normal", PriorityNormal, PriorityValueNormal},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if uint8(tc.priority) != tc.expected {
				t.Errorf("Priority value = %d, want %d", uint8(tc.priority), tc.expected)
			}
		})
	}
}

func TestSend_PriorityEmergencyPreservedOnRetransmit(t *testing.T) {
	const (
		testMaxRetries   = 3
		testTimeout      = 5 * time.Millisecond
		testPollInterval = 1 * time.Millisecond
		testWaitTimeout  = 100 * time.Millisecond
	)

	config := &Config{
		MaxRetries: testMaxRetries,
		Timeout:    testTimeout,
	}
	harq, mock := createTestHARQ(config)
	mock.SetReceiveDelay(2 * time.Millisecond) // Ensure loops take time
	payload := []byte{0x01, 0x02, 0x03}

	// Use channel-based synchronization to deterministically trigger ACK after retransmit
	retransmitDetected := make(chan struct{})
	go func() {
		ticker := time.NewTicker(testPollInterval)
		defer ticker.Stop()
		defer close(retransmitDetected)

		timeout := time.After(testWaitTimeout)

		for {
			select {
			case <-ticker.C:
				sentData := mock.GetSentData()
				if len(sentData) >= 2 {
					mock.QueueResponse(createAckFrame(0))
					return
				}
			case <-timeout:
				t.Errorf("timeout waiting for retransmission after %v", testWaitTimeout)
				return
			}
		}
	}()

	// Send with PriorityEmergency
	err := harq.Send(context.Background(), payload, PriorityEmergency)

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}

	// Wait for retransmit detection to complete
	<-retransmitDetected

	// Verify priority flag is set on both original and retransmitted frames
	sentData := mock.GetSentData()
	if len(sentData) < 2 {
		t.Fatalf("expected at least 2 sent frames, got %d", len(sentData))
	}

	decoder := frame.NewDecoder()
	for i, data := range sentData {
		f, err := decoder.Decode(data)
		if err != nil {
			t.Fatalf("failed to decode frame %d: %v", i, err)
		}

		if f.Header.Flags&frame.FlagPriority == 0 {
			t.Errorf("Frame %d should have FlagPriority set for PriorityEmergency", i)
		}
	}
}

func TestSend_PriorityHighWithFEC(t *testing.T) {
	config := &Config{
		MaxRetries: 3,
		Timeout:    50 * time.Millisecond,
		FECEnabled: true,
	}
	mock := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	fecEncoder := &MockFECEncoder{}
	fecDecoder := &MockFECDecoder{}
	harq := NewChaseCombining(config, mock, encoder, decoder, fecEncoder, fecDecoder)

	mock.QueueResponse(createAckFrame(0))

	payload := []byte("high-priority-message")
	err := harq.Send(context.Background(), payload, PriorityHigh)

	if err != nil {
		t.Errorf("Send() error = %v, want nil", err)
	}

	sentData := mock.GetSentData()
	if len(sentData) == 0 {
		t.Fatal("expected at least 1 sent frame, got 0")
	}

	// Decode and verify priority flag is set along with FEC flag
	f, err := decoder.Decode(sentData[0])
	if err != nil {
		t.Fatalf("failed to decode frame: %v", err)
	}

	hasPriorityFlag := f.Header.Flags&frame.FlagPriority != 0
	hasFECFlag := f.Header.Flags&frame.FlagFECEnabled != 0

	if !hasPriorityFlag {
		t.Error("PriorityHigh should set FlagPriority")
	}
	if !hasFECFlag {
		t.Error("FEC enabled should set FlagFECEnabled")
	}
}

func TestSend_VariadicPriorityHandling(t *testing.T) {
	tests := []struct {
		name            string
		priority        []Priority
		expectedFlagSet bool
		description     string
	}{
		{
			name:            "no_priority_arg",
			priority:        []Priority{},
			expectedFlagSet: false,
			description:     "Should default to PriorityNormal and not set flag",
		},
		{
			name:            "single_priority_normal",
			priority:        []Priority{PriorityNormal},
			expectedFlagSet: false,
			description:     "PriorityNormal should not set flag",
		},
		{
			name:            "single_priority_high",
			priority:        []Priority{PriorityHigh},
			expectedFlagSet: true,
			description:     "PriorityHigh should set flag",
		},
		{
			name:            "single_priority_emergency",
			priority:        []Priority{PriorityEmergency},
			expectedFlagSet: true,
			description:     "PriorityEmergency should set flag",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			harq, mock := createTestHARQ(nil)
			mock.QueueResponse(createAckFrame(0))

			payload := []byte{0xFF}
			var err error
			if len(tc.priority) == 0 {
				err = harq.Send(context.Background(), payload)
			} else {
				err = harq.Send(context.Background(), payload, tc.priority[0])
			}

			if err != nil {
				t.Errorf("Send() error = %v, want nil", err)
			}

			sentData := mock.GetSentData()
			decoder := frame.NewDecoder()
			f, err := decoder.Decode(sentData[0])
			if err != nil {
				t.Fatalf("failed to decode frame: %v", err)
			}

			flagSet := f.Header.Flags&frame.FlagPriority != 0
			if flagSet != tc.expectedFlagSet {
				t.Errorf("%s: FlagPriority=%v, want=%v", tc.description, flagSet, tc.expectedFlagSet)
			}
		})
	}
}

// TestChaseCombining_SetExpectedLenForTesting tests the testing helper function.
func TestChaseCombining_SetExpectedLenForTesting(t *testing.T) {
	const testExpectedLen = 100

	mockTransport := NewMockTransport()
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	fecEncoder := &MockFECEncoder{}
	fecDecoder := &MockFECDecoder{}

	config := &Config{
		MaxRetries: 3,
		Timeout:    10 * time.Millisecond,
		FECEnabled: true,
	}

	harq := NewChaseCombining(config, mockTransport, encoder, decoder, fecEncoder, fecDecoder)

	// Test setting expected length
	harq.SetExpectedLenForTesting(testExpectedLen)

	// Verify the expected length was set correctly
	if gotLen := harq.GetExpectedLenForTesting(); gotLen != testExpectedLen {
		t.Fatalf("GetExpectedLenForTesting() = %d, want %d", gotLen, testExpectedLen)
	}
}
