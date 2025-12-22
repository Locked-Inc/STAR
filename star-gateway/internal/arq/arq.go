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
	"time"
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
)

// StopAndWait is a placeholder implementation of the ARQ interface
// using Stop-and-Wait protocol.
// TODO: Implement actual ARQ logic in a future PR.
type StopAndWait struct {
	config     *Config
	state      State
	txSequence uint16
	rxSequence uint16
	retryCount int
}

// NewStopAndWait creates a new Stop-and-Wait ARQ handler.
// If config is nil, uses DefaultARQConfig().
func NewStopAndWait(config *Config) *StopAndWait {
	if config == nil {
		config = DefaultARQConfig()
	}
	return &StopAndWait{
		config:     config,
		state:      StateIdle,
		txSequence: 0,
		rxSequence: 0,
		retryCount: 0,
	}
}

// Send transmits data with ARQ reliability.
// TODO: Implement send logic:
//  1. Wrap data in frame with current TX sequence
//  2. Transmit frame
//  3. Wait for ACK/NACK with timeout
//  4. On ACK: increment sequence, return success
//  5. On NACK or timeout: retransmit up to MaxRetries
//  6. On max retries: return ErrMaxRetriesExceeded
func (s *StopAndWait) Send(data []byte) error {
	return ErrNotImplemented
}

// Receive waits for data with ARQ handling.
// TODO: Implement receive logic:
//  1. Wait for frame
//  2. Validate CRC
//  3. Check sequence number for duplicates
//  4. On valid new frame: send ACK, return data
//  5. On duplicate: send ACK (but don't return data again)
//  6. On CRC error: send NACK
func (s *StopAndWait) Receive() ([]byte, error) {
	return nil, ErrNotImplemented
}

// GetState returns the current ARQ state.
func (s *StopAndWait) GetState() State {
	return s.state
}

// GetTxSequence returns the current TX sequence number.
func (s *StopAndWait) GetTxSequence() uint16 {
	return s.txSequence
}

// GetRxSequence returns the expected RX sequence number.
func (s *StopAndWait) GetRxSequence() uint16 {
	return s.rxSequence
}

// Reset resets the ARQ state machine to initial state.
func (s *StopAndWait) Reset() {
	s.state = StateIdle
	s.txSequence = 0
	s.rxSequence = 0
	s.retryCount = 0
}

// Config returns the current ARQ configuration.
func (s *StopAndWait) Config() *Config {
	return s.config
}

// SetStateForTesting sets internal state for testing purposes.
// This method should only be used in tests to simulate various states.
func (s *StopAndWait) SetStateForTesting(state State, txSeq, rxSeq uint16, retryCount int) {
	s.state = state
	s.txSequence = txSeq
	s.rxSequence = rxSeq
	s.retryCount = retryCount
}

// incrementSequence increments a sequence number with wraparound.
func incrementSequence(seq uint16) uint16 {
	if seq == DefaultSequenceMax {
		return 0
	}
	return seq + 1
}
