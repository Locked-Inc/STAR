// Package harq implements Hybrid Automatic Repeat reQuest (HARQ) protocol
// for reliable frame delivery over the SPI transport.
//
// The HARQ mechanism uses Chase Combining (Type I) with the following properties:
//   - 16-bit sequence numbers with wraparound
//   - ACK/NACK response frames
//   - Configurable retry count and timeout
//   - Duplicate frame detection
//   - Forward Error Correction (FEC) with soft Viterbi decoding
//   - Soft bit combining across retransmissions
//
// Chase Combining stores soft bits from failed transmissions and combines
// them element-wise before re-attempting Viterbi decoding. This provides
// approximately 2dB coding gain per additional transmission.
//
// Reference: docs/sections/01_nanopb_protocol.tex (Layer 3: HARQ)
//
// STAR Project - Texas A&M University
// December 2025
package harq

import (
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// HARQ protocol constants from specification.
const (
	// DefaultMaxRetries is the maximum number of transmission attempts.
	DefaultMaxRetries = 3

	// DefaultTimeout is the timeout waiting for ACK/NACK (8-10ms per spec).
	DefaultTimeout = 10 * time.Millisecond

	// DefaultSequenceMax is the maximum sequence number before wraparound.
	DefaultSequenceMax = 0xFFFF
)

// State represents the HARQ state machine state.
type State uint8

const (
	// StateIdle indicates ready to send or receive.
	StateIdle State = iota

	// StateWaitingAck indicates waiting for acknowledgment.
	StateWaitingAck

	// StateRetransmitting indicates retransmitting a frame.
	StateRetransmitting

	// StateCombining indicates combining soft bits from retransmissions.
	StateCombining

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
	case StateCombining:
		return "COMBINING"
	case StateError:
		return "ERROR"
	default:
		return "UNKNOWN"
	}
}

// HARQ defines the interface for the HARQ protocol handler.
type HARQ interface {
	// Send transmits data with HARQ reliability.
	// Applies FEC encoding before transmission.
	// Handles retransmission on timeout or NACK.
	// Returns an error if max retries exceeded.
	Send(data []byte) error

	// Receive waits for data with HARQ handling.
	// Applies FEC decoding and soft bit combining on retransmissions.
	// Sends ACK on successful decode, NACK on error.
	// Returns the decoded data and any error.
	Receive() ([]byte, error)

	// GetState returns the current HARQ state.
	GetState() State

	// GetTxSequence returns the current TX sequence number.
	GetTxSequence() uint16

	// GetRxSequence returns the expected RX sequence number.
	GetRxSequence() uint16

	// Reset resets the HARQ state machine.
	Reset()
}

// Config holds HARQ configuration parameters.
type Config struct {
	// MaxRetries is the maximum number of transmission attempts.
	MaxRetries int

	// Timeout is the timeout waiting for ACK/NACK.
	Timeout time.Duration

	// FECEnabled enables Forward Error Correction.
	// When disabled, operates in legacy ARQ mode.
	FECEnabled bool
}

// DefaultConfig returns the default HARQ configuration.
func DefaultConfig() *Config {
	return &Config{
		MaxRetries: DefaultMaxRetries,
		Timeout:    DefaultTimeout,
		FECEnabled: true,
	}
}

// Predefined errors for HARQ operations.
var (
	// ErrNotImplemented is returned by placeholder implementations.
	ErrNotImplemented = errors.New("harq: not implemented")

	// ErrMaxRetriesExceeded is returned when transmission fails after max retries.
	ErrMaxRetriesExceeded = errors.New("harq: max retries exceeded")

	// ErrTimeout is returned when waiting for ACK times out.
	ErrTimeout = errors.New("harq: timeout waiting for acknowledgment")

	// ErrInvalidSequence is returned when sequence number is invalid.
	ErrInvalidSequence = errors.New("harq: invalid sequence number")

	// ErrDuplicateFrame is returned when a duplicate frame is detected.
	ErrDuplicateFrame = errors.New("harq: duplicate frame detected")

	// ErrTransportNil is returned when transport is nil.
	ErrTransportNil = errors.New("harq: transport is nil")

	// ErrEncoderNil is returned when encoder is nil.
	ErrEncoderNil = errors.New("harq: encoder is nil")

	// ErrDecoderNil is returned when decoder is nil.
	ErrDecoderNil = errors.New("harq: decoder is nil")

	// ErrFECEncoderNil is returned when FEC encoder is nil but FEC is enabled.
	ErrFECEncoderNil = errors.New("harq: FEC encoder is nil")

	// ErrFECDecoderNil is returned when FEC decoder is nil but FEC is enabled.
	ErrFECDecoderNil = errors.New("harq: FEC decoder is nil")

	// ErrUnexpectedFrameType is returned for unexpected frame types.
	ErrUnexpectedFrameType = errors.New("harq: unexpected frame type")

	// ErrInErrorState is returned when HARQ is in error state.
	ErrInErrorState = errors.New("harq: in error state, call Reset() first")

	// ErrDecodeFailed is returned when FEC decode fails after combining.
	ErrDecodeFailed = errors.New("harq: FEC decode failed after combining")
)

// ChaseCombining implements the HARQ interface using Chase Combining (Type I).
type ChaseCombining struct {
	config     *Config
	state      State
	txSequence uint16
	rxSequence uint16
	retryCount int

	// Dependencies (injected)
	transport   transport.Transport
	encoder     frame.Encoder
	decoder     frame.Decoder
	fecEncoder  fec.Encoder
	fecDecoder  fec.Decoder
	softCombiner *fec.ChaseCombiner

	// Thread safety
	mu sync.Mutex

	// For retransmission
	pendingFrame   *frame.Frame
	pendingEncoded []byte // FEC-encoded payload for retransmission
	expectedLen    int    // Expected decoded length for current frame
}

// NewChaseCombining creates a new Chase Combining HARQ handler.
// If config is nil, uses DefaultConfig().
// The transport, frame encoder/decoder, and FEC encoder/decoder are required dependencies.
func NewChaseCombining(
	config *Config,
	t transport.Transport,
	encoder frame.Encoder,
	decoder frame.Decoder,
	fecEncoder fec.Encoder,
	fecDecoder fec.Decoder,
) *ChaseCombining {
	if config == nil {
		config = DefaultConfig()
	}
	return &ChaseCombining{
		config:       config,
		state:        StateIdle,
		txSequence:   0,
		rxSequence:   0,
		retryCount:   0,
		transport:    t,
		encoder:      encoder,
		decoder:      decoder,
		fecEncoder:   fecEncoder,
		fecDecoder:   fecDecoder,
		softCombiner: fec.NewChaseCombiner(&fec.CombinerConfig{MaxCombines: config.MaxRetries + 1}),
		pendingFrame: nil,
	}
}

// Send transmits data with HARQ reliability.
// Applies FEC encoding, wraps in a command frame, and waits for ACK.
// Retransmits the same encoded frame on timeout or NACK up to MaxRetries.
func (h *ChaseCombining) Send(data []byte) error {
	h.mu.Lock()

	// Validate dependencies
	if h.transport == nil {
		h.mu.Unlock()
		return ErrTransportNil
	}
	if h.encoder == nil {
		h.mu.Unlock()
		return ErrEncoderNil
	}
	if h.config.FECEnabled && h.fecEncoder == nil {
		h.mu.Unlock()
		return ErrFECEncoderNil
	}

	// Check if in error state
	if h.state == StateError {
		h.mu.Unlock()
		return ErrInErrorState
	}

	// Apply FEC encoding if enabled
	payload := data
	if h.config.FECEnabled {
		var err error
		payload, err = h.fecEncoder.Encode(data)
		if err != nil {
			h.mu.Unlock()
			return fmt.Errorf("harq: FEC encode failed: %w", err)
		}
	}

	// Create command frame with FEC-encoded payload
	f, err := frame.NewFrame(frame.FrameTypeCommand, payload)
	if err != nil {
		h.mu.Unlock()
		return fmt.Errorf("harq: failed to create frame: %w", err)
	}
	f.Header.Sequence = h.txSequence
	f.Header.Flags = frame.FlagRequiresAck
	if h.config.FECEnabled {
		f.Header.Flags |= frame.FlagFECEnabled
	}

	h.pendingFrame = f
	h.pendingEncoded = payload
	h.state = StateWaitingAck
	h.retryCount = 0
	currentSeq := h.txSequence
	h.mu.Unlock()

	// Retry loop (Chase Combining: same encoded frame each time)
	for attempt := 0; attempt <= h.config.MaxRetries; attempt++ {
		h.mu.Lock()
		if attempt > 0 {
			h.pendingFrame.Header.Flags |= frame.FlagRetransmit
			h.state = StateRetransmitting
			h.retryCount = attempt
		}
		frameToSend := h.pendingFrame
		h.mu.Unlock()

		// Encode frame (framing layer, not FEC)
		encoded, err := h.encoder.Encode(frameToSend)
		if err != nil {
			h.setErrorState()
			return fmt.Errorf("harq: failed to encode frame: %w", err)
		}

		// Send via transport
		_, err = h.transport.Send(encoded)
		if err != nil {
			h.setErrorState()
			return fmt.Errorf("harq: transport send failed: %w", err)
		}

		// Wait for ACK/NACK with timeout
		ackFrame, err := h.waitForAck()
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
				h.mu.Lock()
				h.txSequence = incrementSequence(h.txSequence)
				h.state = StateIdle
				h.pendingFrame = nil
				h.pendingEncoded = nil
				h.retryCount = 0
				h.mu.Unlock()
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
	h.setErrorState()
	return ErrMaxRetriesExceeded
}

// Receive waits for data with HARQ handling.
// On receive, attempts FEC decode. On failure, stores soft bits and waits
// for retransmission. Combines soft bits from multiple transmissions.
// Validates CRC and sequence number, sends ACK for valid frames.
func (h *ChaseCombining) Receive() ([]byte, error) {
	// Validate dependencies
	if h.transport == nil {
		return nil, ErrTransportNil
	}
	if h.decoder == nil {
		return nil, ErrDecoderNil
	}
	if h.encoder == nil {
		return nil, ErrEncoderNil
	}
	if h.config.FECEnabled && h.fecDecoder == nil {
		return nil, ErrFECDecoderNil
	}

	// Receive raw data from transport
	data, err := h.transport.Receive(frame.MaxFrameSize)
	if err != nil {
		return nil, fmt.Errorf("harq: transport receive failed: %w", err)
	}

	// Decode frame (CRC validated by decoder)
	f, err := h.decoder.Decode(data)
	if err != nil {
		// CRC error or malformed frame - send NACK (best-effort)
		h.mu.Lock()
		seq := h.rxSequence
		h.mu.Unlock()
		_ = h.sendNack(seq) // Best-effort, ignore errors
		return nil, err
	}

	// Only process command/response frames
	if f.Header.Type != frame.FrameTypeCommand &&
		f.Header.Type != frame.FrameTypeResponse {
		return nil, ErrUnexpectedFrameType
	}

	h.mu.Lock()

	receivedSeq := f.Header.Sequence
	expectedSeq := h.rxSequence
	previousSeq := decrementSequence(h.rxSequence)

	if receivedSeq == expectedSeq {
		// New valid frame - attempt FEC decode
		var decoded []byte

		if h.config.FECEnabled && (f.Header.Flags&frame.FlagFECEnabled) != 0 {
			// Convert payload to soft bits and attempt decode
			softBits := bytesToSoftBits(f.Payload)

			// Add to combiner
			h.softCombiner.Add(softBits)

			// Try decode
			decoded, _, err = h.fecDecoder.DecodeSoft(h.softCombiner.Combined(), h.expectedLen)
			if err != nil {
				// Decode failed - need retransmission
				h.state = StateCombining
				h.mu.Unlock()
				_ = h.sendNack(receivedSeq)
				return nil, ErrDecodeFailed
			}

			// Success - reset combiner
			h.softCombiner.Reset()
		} else {
			// No FEC - use payload directly
			decoded = f.Payload
		}

		// Increment sequence and prepare payload
		h.rxSequence = incrementSequence(h.rxSequence)
		h.state = StateIdle
		h.mu.Unlock()

		// Best-effort ACK
		_ = h.sendAck(receivedSeq)
		return decoded, nil
	}

	if receivedSeq == previousSeq {
		// Duplicate frame detected - treat as true duplicate at the HARQ layer.
		//
		// NOTE: This branch is taken when the initial transmission was successfully
		// decoded (so we never entered StateCombining), the ACK was lost on the wire,
		// and the sender retransmits with previousSeq. We still send an ACK for
		// protocol correctness but return ErrDuplicateFrame so higher layers do not
		// deliver the same payload twice.
		//
		// StateCombining case: When a frame fails to decode (receivedSeq == expectedSeq),
		// we enter StateCombining but do NOT increment rxSequence. Retransmissions
		// will therefore have receivedSeq == expectedSeq (not previousSeq) and are
		// handled by the block above, which adds to the combiner and retries decode.
		h.mu.Unlock()
		_ = h.sendAck(receivedSeq)
		return nil, ErrDuplicateFrame
	}

	// Out of sequence frame
	h.mu.Unlock()
	_ = h.sendNack(receivedSeq)
	return nil, ErrInvalidSequence
}

// GetState returns the current HARQ state.
func (h *ChaseCombining) GetState() State {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.state
}

// GetTxSequence returns the current TX sequence number.
func (h *ChaseCombining) GetTxSequence() uint16 {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.txSequence
}

// GetRxSequence returns the expected RX sequence number.
func (h *ChaseCombining) GetRxSequence() uint16 {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.rxSequence
}

// Reset resets the HARQ state machine to initial state.
func (h *ChaseCombining) Reset() {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.state = StateIdle
	h.txSequence = 0
	h.rxSequence = 0
	h.retryCount = 0
	h.pendingFrame = nil
	h.pendingEncoded = nil
	h.expectedLen = 0
	h.softCombiner.Reset()
}

// Config returns the current HARQ configuration.
func (h *ChaseCombining) Config() *Config {
	return h.config
}

// SetStateForTesting sets internal state for testing purposes.
// This method should only be used in tests to simulate various states.
func (h *ChaseCombining) SetStateForTesting(state State, txSeq, rxSeq uint16, retryCount int) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.state = state
	h.txSequence = txSeq
	h.rxSequence = rxSeq
	h.retryCount = retryCount
}

// SetExpectedLenForTesting sets the expected decoded length for testing.
func (h *ChaseCombining) SetExpectedLenForTesting(expectedLen int) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.expectedLen = expectedLen
}

// waitForAck waits for an ACK/NACK response with timeout.
func (h *ChaseCombining) waitForAck() (*frame.Frame, error) {
	type result struct {
		frame *frame.Frame
		err   error
	}

	ch := make(chan result, 1)

	go func() {
		data, err := h.transport.Receive(frame.MaxFrameSize)
		if err != nil {
			ch <- result{nil, err}
			return
		}
		f, err := h.decoder.Decode(data)
		ch <- result{f, err}
	}()

	select {
	case r := <-ch:
		return r.frame, r.err
	case <-time.After(h.config.Timeout):
		return nil, ErrTimeout
	}
}

// sendNack sends a NACK frame.
func (h *ChaseCombining) sendNack(seq uint16) error {
	return h.sendControlFrame(frame.FrameTypeNack, seq)
}

// sendAck sends an ACK frame.
func (h *ChaseCombining) sendAck(seq uint16) error {
	return h.sendControlFrame(frame.FrameTypeAck, seq)
}

// sendControlFrame sends an ACK or NACK frame.
func (h *ChaseCombining) sendControlFrame(frameType frame.FrameType, seq uint16) error {
	if h.transport == nil {
		return ErrTransportNil
	}
	if h.encoder == nil {
		return ErrEncoderNil
	}

	f, err := frame.NewFrame(frameType, nil)
	if err != nil {
		return err
	}
	f.Header.Sequence = seq

	encoded, err := h.encoder.Encode(f)
	if err != nil {
		return err
	}

	_, err = h.transport.Send(encoded)
	return err
}

// setErrorState transitions to error state (thread-safe).
func (h *ChaseCombining) setErrorState() {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.state = StateError
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

// bytesToSoftBits converts bytes to maximum confidence soft bits.
// Each bit becomes +127 (for 1) or -127 (for 0).
func bytesToSoftBits(data []byte) []fec.SoftBit {
	numBits := len(data) * 8
	softBits := make([]fec.SoftBit, numBits)

	for i := 0; i < numBits; i++ {
		byteIdx := i / 8
		bitPos := 7 - (i % 8) // MSB first
		bit := (data[byteIdx] >> bitPos) & 1
		softBits[i] = fec.HardToSoft(bit)
	}

	return softBits
}

// =============================================================================
// Legacy ARQ Compatibility
// =============================================================================

// StopAndWait provides backward compatibility with the legacy ARQ interface.
// It wraps ChaseCombining with FEC disabled.
type StopAndWait struct {
	*ChaseCombining
}

// NewStopAndWait creates a new Stop-and-Wait ARQ handler (legacy compatibility).
// If config is nil, uses DefaultConfig() with FEC disabled.
// The transport, encoder, and decoder are required dependencies.
func NewStopAndWait(
	config *Config,
	t transport.Transport,
	encoder frame.Encoder,
	decoder frame.Decoder,
) *StopAndWait {
	if config == nil {
		config = &Config{
			MaxRetries: DefaultMaxRetries,
			Timeout:    DefaultTimeout,
			FECEnabled: false, // Legacy ARQ mode
		}
	} else {
		config.FECEnabled = false // Force legacy mode
	}
	return &StopAndWait{
		ChaseCombining: NewChaseCombining(config, t, encoder, decoder, nil, nil),
	}
}
