// Package arq implements the Automatic Repeat reQuest (ARQ) protocol
// for reliable frame delivery over the SPI transport.
//
// The ARQ mechanism uses Stop-and-Wait with the following properties:
//   - 16-bit sequence numbers with wraparound
//   - ACK/NACK response frames
//   - Configurable retry count and timeout
//   - Duplicate frame detection
//
// Reference: docs/sections/01_nanopb_protocol.tex (Layer 3: ARQ)
//
// STAR Project - Texas A&M University
// December 2025
package arq

import (
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// ARQ protocol constants from specification.
const (
	// DefaultMaxRetries is the maximum number of transmission attempts.
	DefaultMaxRetries = 3

	// DefaultTimeout is the timeout waiting for ACK/NACK (8-10ms per spec).
	DefaultTimeout = 10 * time.Millisecond

	// DefaultSequenceMax is the maximum sequence number before wraparound.
	DefaultSequenceMax = 0xFFFF
)

// State represents the ARQ state machine state.
type State uint8

const (
	// StateIdle indicates ready to send or receive.
	StateIdle State = iota

	// StateWaitingAck indicates waiting for acknowledgment.
	StateWaitingAck

	// StateRetransmitting indicates retransmitting a frame.
	StateRetransmitting

	// StateError indicates an error state (max retries exceeded).
	StateError
)

// String returns the string representation of a State.
func (s State) String() string {
	switch s {
	case StateIdle:
		return "IDLE"
	case StateWaitingAck:
		return "WAITING_ACK"
	case StateRetransmitting:
		return "RETRANSMITTING"
	case StateError:
		return "ERROR"
	default:
		return "UNKNOWN"
	}
}

// ARQ defines the interface for the ARQ protocol handler.
type ARQ interface {
	// Send transmits data with ARQ reliability.
	// Handles retransmission on timeout or NACK.
	// Returns an error if max retries exceeded.
	Send(data []byte) error

	// Receive waits for data with ARQ handling.
	// Sends ACK on successful receipt, NACK on error.
	// Returns the received data and any error.
	Receive() ([]byte, error)

	// GetState returns the current ARQ state.
	GetState() State

	// GetTxSequence returns the current TX sequence number.
	GetTxSequence() uint16

	// GetRxSequence returns the expected RX sequence number.
	GetRxSequence() uint16

	// Reset resets the ARQ state machine.
	Reset()
}

// Config holds ARQ configuration parameters.
type Config struct {
	// MaxRetries is the maximum number of transmission attempts.
	MaxRetries int

	// Timeout is the timeout waiting for ACK/NACK.
	Timeout time.Duration
}

// DefaultARQConfig returns the default ARQ configuration.
func DefaultARQConfig() *Config {
	return &Config{
		MaxRetries: DefaultMaxRetries,
		Timeout:    DefaultTimeout,
	}
}

// Predefined errors for ARQ operations.
var (
	// ErrNotImplemented is returned by placeholder implementations.
	ErrNotImplemented = errors.New("arq: not implemented")

	// ErrMaxRetriesExceeded is returned when transmission fails after max retries.
	ErrMaxRetriesExceeded = errors.New("arq: max retries exceeded")

	// ErrTimeout is returned when waiting for ACK times out.
	ErrTimeout = errors.New("arq: timeout waiting for acknowledgment")

	// ErrInvalidSequence is returned when sequence number is invalid.
	ErrInvalidSequence = errors.New("arq: invalid sequence number")

	// ErrDuplicateFrame is returned when a duplicate frame is detected.
	ErrDuplicateFrame = errors.New("arq: duplicate frame detected")

	// ErrTransportNil is returned when transport is nil.
	ErrTransportNil = errors.New("arq: transport is nil")

	// ErrEncoderNil is returned when encoder is nil.
	ErrEncoderNil = errors.New("arq: encoder is nil")

	// ErrDecoderNil is returned when decoder is nil.
	ErrDecoderNil = errors.New("arq: decoder is nil")

	// ErrUnexpectedFrameType is returned for unexpected frame types.
	ErrUnexpectedFrameType = errors.New("arq: unexpected frame type")

	// ErrInErrorState is returned when ARQ is in error state.
	ErrInErrorState = errors.New("arq: in error state, call Reset() first")
)

// StopAndWait implements the ARQ interface using Stop-and-Wait protocol.
type StopAndWait struct {
	config     *Config
	state      State
	txSequence uint16
	rxSequence uint16
	retryCount int

	// Dependencies (injected)
	transport transport.Transport
	encoder   frame.Encoder
	decoder   frame.Decoder

	// Thread safety
	mu sync.Mutex

	// For retransmission
	pendingFrame *frame.Frame
}

// NewStopAndWait creates a new Stop-and-Wait ARQ handler.
// If config is nil, uses DefaultARQConfig().
// The transport, encoder, and decoder are required dependencies.
func NewStopAndWait(
	config *Config,
	t transport.Transport,
	encoder frame.Encoder,
	decoder frame.Decoder,
) *StopAndWait {
	if config == nil {
		config = DefaultARQConfig()
	}
	return &StopAndWait{
		config:       config,
		state:        StateIdle,
		txSequence:   0,
		rxSequence:   0,
		retryCount:   0,
		transport:    t,
		encoder:      encoder,
		decoder:      decoder,
		pendingFrame: nil,
	}
}

// Send transmits data with ARQ reliability.
// Wraps data in a command frame and waits for ACK.
// Retransmits on timeout or NACK up to MaxRetries.
func (s *StopAndWait) Send(data []byte) error {
	s.mu.Lock()

	// Validate dependencies
	if s.transport == nil {
		s.mu.Unlock()
		return ErrTransportNil
	}
	if s.encoder == nil {
		s.mu.Unlock()
		return ErrEncoderNil
	}

	// Check if in error state
	if s.state == StateError {
		s.mu.Unlock()
		return ErrInErrorState
	}

	// Create command frame
	f, err := frame.NewFrame(frame.FrameTypeCommand, data)
	if err != nil {
		s.mu.Unlock()
		return fmt.Errorf("arq: failed to create frame: %w", err)
	}
	f.Header.Sequence = s.txSequence
	f.Header.Flags = frame.FlagRequiresAck

	s.pendingFrame = f
	s.state = StateWaitingAck
	s.retryCount = 0
	currentSeq := s.txSequence
	s.mu.Unlock()

	// Retry loop
	for attempt := 0; attempt <= s.config.MaxRetries; attempt++ {
		s.mu.Lock()
		if attempt > 0 {
			s.pendingFrame.Header.Flags |= frame.FlagRetransmit
			s.state = StateRetransmitting
			s.retryCount = attempt
		}
		frameToSend := s.pendingFrame
		s.mu.Unlock()

		// Encode frame
		encoded, err := s.encoder.Encode(frameToSend)
		if err != nil {
			s.setErrorState()
			return fmt.Errorf("arq: failed to encode frame: %w", err)
		}

		// Send via transport
		_, err = s.transport.Send(encoded)
		if err != nil {
			s.setErrorState()
			return fmt.Errorf("arq: transport send failed: %w", err)
		}

		// Wait for ACK/NACK with timeout
		ackFrame, err := s.waitForAck()
		if errors.Is(err, ErrTimeout) {
			// Timeout - will retry
			continue
		}
		if err != nil {
			// Decode error or transport error - will retry
			continue
		}

		// Process response
		if ackFrame.Header.Type == frame.FrameTypeAck {
			if ackFrame.Header.Sequence == currentSeq {
				// Success
				s.mu.Lock()
				s.txSequence = incrementSequence(s.txSequence)
				s.state = StateIdle
				s.pendingFrame = nil
				s.retryCount = 0
				s.mu.Unlock()
				return nil
			}
			// Wrong sequence - ignore and continue waiting
		}

		if ackFrame.Header.Type == frame.FrameTypeNack {
			// NACK received - will retry
			continue
		}

		// Unexpected frame type - ignore and treat as timeout
	}

	// Max retries exceeded
	s.setErrorState()
	return ErrMaxRetriesExceeded
}

// Receive waits for data with ARQ handling.
// Validates CRC and sequence number, sends ACK for valid frames.
func (s *StopAndWait) Receive() ([]byte, error) {
	// Validate dependencies
	if s.transport == nil {
		return nil, ErrTransportNil
	}
	if s.decoder == nil {
		return nil, ErrDecoderNil
	}
	if s.encoder == nil {
		return nil, ErrEncoderNil
	}

	// Receive raw data from transport
	data, err := s.transport.Receive(frame.MaxFrameSize)
	if err != nil {
		return nil, fmt.Errorf("arq: transport receive failed: %w", err)
	}

	// Decode frame (CRC validated by decoder)
	f, err := s.decoder.Decode(data)
	if err != nil {
		// CRC error or malformed frame - send NACK (best-effort)
		s.mu.Lock()
		seq := s.rxSequence
		s.mu.Unlock()
		_ = s.sendNack(seq) // Best-effort, ignore errors
		return nil, err
	}

	// Only process command/response frames
	if f.Header.Type != frame.FrameTypeCommand &&
		f.Header.Type != frame.FrameTypeResponse {
		return nil, ErrUnexpectedFrameType
	}

	s.mu.Lock()

	receivedSeq := f.Header.Sequence
	expectedSeq := s.rxSequence
	previousSeq := decrementSequence(s.rxSequence)

	if receivedSeq == expectedSeq {
		// New valid frame - increment sequence and prepare payload
		s.rxSequence = incrementSequence(s.rxSequence)
		payload := f.Payload
		s.mu.Unlock() // Release lock before I/O

		// Best-effort ACK. If this fails, the sender will retransmit,
		// and we'll correctly handle it as a duplicate.
		_ = s.sendAck(receivedSeq)
		return payload, nil
	}

	if receivedSeq == previousSeq {
		// Duplicate frame
		s.mu.Unlock()              // Release lock before I/O
		_ = s.sendAck(receivedSeq) // Best-effort
		return nil, ErrDuplicateFrame
	}

	// Out of sequence frame
	s.mu.Unlock()               // Release lock before I/O
	_ = s.sendNack(receivedSeq) // Best-effort
	return nil, ErrInvalidSequence
}

// GetState returns the current ARQ state.
func (s *StopAndWait) GetState() State {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.state
}

// GetTxSequence returns the current TX sequence number.
func (s *StopAndWait) GetTxSequence() uint16 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.txSequence
}

// GetRxSequence returns the expected RX sequence number.
func (s *StopAndWait) GetRxSequence() uint16 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.rxSequence
}

// Reset resets the ARQ state machine to initial state.
func (s *StopAndWait) Reset() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.state = StateIdle
	s.txSequence = 0
	s.rxSequence = 0
	s.retryCount = 0
	s.pendingFrame = nil
}

// Config returns the current ARQ configuration.
func (s *StopAndWait) Config() *Config {
	return s.config
}

// SetStateForTesting sets internal state for testing purposes.
// This method should only be used in tests to simulate various states.
func (s *StopAndWait) SetStateForTesting(state State, txSeq, rxSeq uint16, retryCount int) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.state = state
	s.txSequence = txSeq
	s.rxSequence = rxSeq
	s.retryCount = retryCount
}

// waitForAck waits for an ACK/NACK response with timeout.
func (s *StopAndWait) waitForAck() (*frame.Frame, error) {
	type result struct {
		frame *frame.Frame
		err   error
	}

	ch := make(chan result, 1)

	go func() {
		data, err := s.transport.Receive(frame.MaxFrameSize)
		if err != nil {
			ch <- result{nil, err}
			return
		}
		f, err := s.decoder.Decode(data)
		ch <- result{f, err}
	}()

	select {
	case r := <-ch:
		return r.frame, r.err
	case <-time.After(s.config.Timeout):
		return nil, ErrTimeout
	}
}

// sendNack sends a NACK frame.
func (s *StopAndWait) sendNack(seq uint16) error {
	return s.sendControlFrame(frame.FrameTypeNack, seq)
}

// sendAck sends an ACK frame.
func (s *StopAndWait) sendAck(seq uint16) error {
	return s.sendControlFrame(frame.FrameTypeAck, seq)
}

// sendControlFrame sends an ACK or NACK frame.
func (s *StopAndWait) sendControlFrame(frameType frame.FrameType, seq uint16) error {
	if s.transport == nil {
		return ErrTransportNil
	}
	if s.encoder == nil {
		return ErrEncoderNil
	}

	f, err := frame.NewFrame(frameType, nil)
	if err != nil {
		return err
	}
	f.Header.Sequence = seq

	encoded, err := s.encoder.Encode(f)
	if err != nil {
		return err
	}

	_, err = s.transport.Send(encoded)
	return err
}

// setErrorState transitions to error state (thread-safe).
func (s *StopAndWait) setErrorState() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.state = StateError
}

// incrementSequence increments a sequence number with wraparound.
func incrementSequence(seq uint16) uint16 {
	if seq == DefaultSequenceMax {
		return 0
	}
	return seq + 1
}

// decrementSequence decrements a sequence number with wraparound.
func decrementSequence(seq uint16) uint16 {
	if seq == 0 {
		return DefaultSequenceMax
	}
	return seq - 1
}
