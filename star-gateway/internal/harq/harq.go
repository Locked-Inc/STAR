// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

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
	"context"
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

// Priority represents the priority level of a HARQ transmission.
type Priority uint8

const (
	PriorityEmergency Priority = 0
	PriorityHigh      Priority = 1
	PriorityNormal    Priority = 2
)

// Protocol wire format values for priority levels
const (
	PriorityValueEmergency uint8 = 0
	PriorityValueHigh      uint8 = 1
	PriorityValueNormal    uint8 = 2
)

// FrameMetadata contains frame-level information for diagnostics.
type FrameMetadata struct {
	// Frame identification
	Sequence   uint16     // Frame sequence number
	Type       frame.Type // Frame type (PING, PONG, COMMAND, etc.)
	ReceivedAt time.Time  // When frame was received

	// Transmission reliability
	Retransmits int  // Actual retransmit count from FlagRetransmit
	FECDecoded  bool // Whether FEC decoding was used

	// Quality metrics
	PathMetric     int // Viterbi decoder path metric (lower = better SNR)
	CombiningCount int // Chase combining attempts (1-3)
}

// ReceiveResult bundles payload with frame metadata.
type ReceiveResult struct {
	Payload  []byte
	Metadata FrameMetadata
}

// HARQ defines the interface for the HARQ protocol handler.
type HARQ interface {
	// Send transmits data with HARQ reliability.
	// Applies FEC encoding before transmission.
	// Handles retransmission on timeout or NACK.
	// Returns an error if max retries exceeded.
	Send(ctx context.Context, data []byte, p ...Priority) error

	// Receive waits for data with HARQ handling.
	// Applies FEC decoding and soft bit combining on retransmissions.
	// Sends ACK on successful decode, NACK on error.
	// Returns the decoded data with frame metadata and any error.
	Receive(ctx context.Context) (*ReceiveResult, error)

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

	// ErrNilContext is returned when a nil context is provided.
	ErrNilContext = errors.New("harq: context is nil")
)

// ChaseCombining implements the HARQ interface using Chase Combining (Type I).
type ChaseCombining struct {
	config     *Config
	state      State
	txSequence uint16
	rxSequence uint16
	retryCount int

	// Dependencies (injected)
	transport    transport.Device
	encoder      frame.Encoder
	decoder      frame.Decoder
	fecEncoder   fec.Encoder
	fecDecoder   fec.Decoder
	softCombiner *fec.ChaseCombiner

	// Thread safety
	mu sync.Mutex

	// For retransmission
	pendingFrame   *frame.Frame
	pendingEncoded []byte // FEC-encoded payload for retransmission
	expectedLen    int    // Expected decoded length for current frame

	// Full-duplex SPI: Store response received during transmit
	lastRxData []byte
}

// NewChaseCombining creates a new Chase Combining HARQ handler.
// If config is nil, uses DefaultConfig().
// The transport, frame encoder/decoder, and FEC encoder/decoder are required dependencies.
func NewChaseCombining(
	config *Config,
	t transport.Device,
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
// Priority defaults to PriorityNormal if not specified.
func (h *ChaseCombining) Send(ctx context.Context, data []byte, p ...Priority) error {
	if ctx == nil {
		return ErrNilContext
	}
	if err := ctx.Err(); err != nil {
		return err
	}

	// Extract priority with default
	priority := PriorityNormal
	if len(p) > 0 {
		priority = p[0]
	}

	currentSeq, err := h.prepareSend(data, priority)
	if err != nil {
		return err
	}

	// Retry loop (Chase Combining: same encoded frame each time)
	for attempt := 0; attempt <= h.config.MaxRetries; attempt++ {
		frameToSend := h.frameForAttempt(attempt)
		if err := h.transmitFrame(ctx, frameToSend); err != nil {
			return err
		}

		ackFrame, retry, err := h.waitForAckOrRetry(ctx)
		if err != nil {
			return err
		}
		if retry {
			continue
		}
		if h.processAckFrame(ackFrame, currentSeq) {
			return nil
		}
	}

	// Max retries exceeded
	h.setErrorState()
	return ErrMaxRetriesExceeded
}

// Receive waits for data with HARQ handling.
// On receive, attempts FEC decode. On failure, stores soft bits and waits
// for retransmission. Combines soft bits from multiple transmissions.
// Validates CRC and sequence number, sends ACK for valid frames.
func (h *ChaseCombining) Receive(ctx context.Context) (*ReceiveResult, error) {
	if ctx == nil {
		return nil, ErrNilContext
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	if err := h.validateReceiveDependencies(); err != nil {
		return nil, err
	}

	rcvCtx, cancel := h.receiveContext(ctx)
	if cancel != nil {
		defer cancel()
	}

	f, err := h.receiveFrame(rcvCtx)

	if err != nil {
		return nil, err
	}

	result, err := h.handleReceivedFrame(f)
	return result, err
}

func (h *ChaseCombining) validateSendDependencies() error {
	if h.transport == nil {
		return ErrTransportNil
	}
	if h.encoder == nil {
		return ErrEncoderNil
	}
	if h.config.FECEnabled && h.fecEncoder == nil {
		return ErrFECEncoderNil
	}
	if h.state == StateError {
		return ErrInErrorState
	}
	return nil
}

func (h *ChaseCombining) validateReceiveDependencies() error {
	h.mu.Lock()
	defer h.mu.Unlock()

	if h.state == StateError {
		return ErrInErrorState
	}
	if h.transport == nil {
		return ErrTransportNil
	}
	if h.decoder == nil {
		return ErrDecoderNil
	}
	if h.config.FECEnabled && h.fecDecoder == nil {
		return ErrFECDecoderNil
	}
	return nil
}

func (h *ChaseCombining) prepareSend(data []byte, priority Priority) (uint16, error) {
	h.mu.Lock()
	if err := h.validateSendDependencies(); err != nil {
		h.mu.Unlock()
		return 0, err
	}

	payload := data
	if h.config.FECEnabled {
		var err error
		payload, err = h.fecEncoder.Encode(data)
		if err != nil {
			h.mu.Unlock()
			return 0, fmt.Errorf("harq: FEC encode failed: %w", err)
		}
	}

	f, err := frame.NewFrame(frame.FrameTypeCommand, payload)
	if err != nil {
		h.mu.Unlock()
		return 0, fmt.Errorf("harq: failed to create frame: %w", err)
	}
	f.Header.Sequence = h.txSequence
	f.Header.Flags = frame.FlagRequiresAck
	if priority != PriorityNormal {
		f.Header.Flags |= frame.FlagPriority
	}
	if h.config.FECEnabled {
		f.Header.Flags |= frame.FlagFECEnabled
	}

	h.pendingFrame = f
	h.pendingEncoded = payload
	h.state = StateWaitingAck
	h.retryCount = 0
	currentSeq := h.txSequence
	h.mu.Unlock()
	return currentSeq, nil
}

func (h *ChaseCombining) frameForAttempt(attempt int) *frame.Frame {
	h.mu.Lock()
	if attempt > 0 {
		h.pendingFrame.Header.Flags |= frame.FlagRetransmit
		h.state = StateRetransmitting
		h.retryCount = attempt
	}
	frameToSend := h.pendingFrame
	h.mu.Unlock()
	return frameToSend
}

func (h *ChaseCombining) transmitFrame(ctx context.Context, frameToSend *frame.Frame) error {
	encoded, err := h.encoder.Encode(frameToSend)
	if err != nil {
		h.setErrorState()
		return fmt.Errorf("harq: failed to encode frame: %w", err)
	}

	// Full-duplex SPI: Send command and receive response atomically
	// The RX72N will send its telemetry frame back during our command transmission
	rxData, err := h.transport.Transfer(ctx, encoded)
	if err != nil {
		h.setErrorState()
		return fmt.Errorf("harq: transport transfer failed: %w", err)
	}

	// Store the received data for waitForAck to process
	// Only store non-empty responses for data transactions
	if len(rxData) > 0 {
		h.mu.Lock()
		h.lastRxData = rxData
		h.mu.Unlock()
	}

	return nil
}

func (h *ChaseCombining) waitForAckOrRetry(ctx context.Context) (*frame.Frame, bool, error) {
	ackCtx, cancel := context.WithTimeout(ctx, h.config.Timeout)
	ackFrame, err := h.waitForAck(ackCtx)
	cancel()
	if errors.Is(err, ErrTimeout) {
		if ctx.Err() != nil {
			return nil, false, ctx.Err()
		}
		return nil, true, nil
	}
	if err != nil {
		if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
			return nil, false, err
		}
		return nil, true, nil
	}

	return ackFrame, false, nil
}

func (h *ChaseCombining) processAckFrame(ackFrame *frame.Frame, currentSeq uint16) bool {
	if ackFrame.Type == frame.FrameTypeAck && ackFrame.Header.Sequence == currentSeq {
		h.mu.Lock()
		h.txSequence = incrementSequence(h.txSequence)
		h.state = StateIdle
		h.pendingFrame = nil
		h.pendingEncoded = nil
		h.retryCount = 0
		h.mu.Unlock()
		return true
	}
	if ackFrame.Type == frame.FrameTypeNack {
		return false
	}
	return false
}

func (h *ChaseCombining) receiveContext(ctx context.Context) (context.Context, context.CancelFunc) {
	if _, ok := ctx.Deadline(); !ok && h.config != nil && h.config.Timeout > 0 {
		return context.WithTimeout(ctx, h.config.Timeout) // 10ms timeout
	}
	return ctx, nil
}

func (h *ChaseCombining) receiveFrame(ctx context.Context) (*frame.Frame, error) {
	// Check context BEFORE attempting any I/O
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	// Full-duplex SPI: Check if we have a stored response from previous Transfer()
	h.mu.Lock()
	data := h.lastRxData
	h.lastRxData = nil
	h.mu.Unlock()

	// If no stored data, perform a new receive
	if data == nil {
		if h.transport == nil {
			return nil, ErrTransportNil
		}

		// Check context again RIGHT before blocking call
		if err := ctx.Err(); err != nil {
			return nil, err
		}

		// Use Transfer with zero payload (equivalent to Receive) to pass context
		zeros := make([]byte, frame.MaxFrameSize)
		var err error
		data, err = h.transport.Transfer(ctx, zeros)
		if err != nil {
			// Check context FIRST before interpreting error
			if ctxErr := ctx.Err(); ctxErr != nil {
				return nil, ctxErr
			}
			if errors.Is(ctx.Err(), context.DeadlineExceeded) {
				return nil, ErrTimeout
			}
			if errors.Is(ctx.Err(), context.Canceled) {
				return nil, ctx.Err()
			}
			return nil, fmt.Errorf("harq: transport receive failed: %w", err)
		}
	}

	// Trust the decoder to validate - it already checks sync word and CRC
	// Decode the frame
	f, err := h.decoder.Decode(data)
	if err != nil {
		// Decode errors include invalid sync word, CRC failures, etc.
		// Return error and let caller (handleReceivedFrame) decide whether to NACK
		return nil, err
	}

	if f.Type != frame.FrameTypeCommand && f.Type != frame.FrameTypeResponse {
		return nil, ErrUnexpectedFrameType
	}

	return f, nil
}

func (h *ChaseCombining) handleReceivedFrame(f *frame.Frame) (*ReceiveResult, error) {
	h.mu.Lock()
	receivedSeq := f.Header.Sequence
	expectedSeq := h.rxSequence
	previousSeq := decrementSequence(h.rxSequence)
	h.mu.Unlock()

	if receivedSeq == expectedSeq {
		return h.handleExpectedFrame(f)
	}

	if receivedSeq == previousSeq {
		// Duplicate frame - send ACK but return error
		_ = h.sendAck(receivedSeq) // Best-effort
		return nil, ErrDuplicateFrame
	}

	// Out of sequence - send NACK and return error
	_ = h.sendNack(receivedSeq) // Best-effort
	return nil, ErrInvalidSequence
}

func (h *ChaseCombining) handleExpectedFrame(f *frame.Frame) (*ReceiveResult, error) {
	var decoded []byte
	fecDecoded := false
	pathMetric := 0     // Path metric for FEC decoder confidence
	combiningCount := 1 // Number of combining attempts (default 1)

	if h.config.FECEnabled && (f.Header.Flags&frame.FlagFECEnabled) != 0 {
		if h.fecDecoder == nil {
			h.mu.Lock()
			h.state = StateError
			h.mu.Unlock()
			_ = h.sendNack(f.Header.Sequence) // Best-effort - ignore error
			return nil, ErrFECDecoderNil
		}
		softBits := bytesToSoftBits(f.Payload)
		if err := h.softCombiner.Add(softBits); err != nil {
			h.mu.Lock()
			h.state = StateError
			h.mu.Unlock()
			_ = h.sendNack(f.Header.Sequence) // Best-effort - ignore error
			return nil, err
		}

		var err error
		decoded, pathMetric, err = h.fecDecoder.DecodeSoft(h.softCombiner.Combined(), h.expectedLen)
		if err != nil {
			h.mu.Lock()
			h.state = StateCombining
			h.mu.Unlock()
			_ = h.sendNack(f.Header.Sequence) // Best-effort - ignore error
			return nil, ErrDecodeFailed
		}

		fecDecoded = true
		// Capture combining count BEFORE Reset()
		combiningCount = h.softCombiner.Count()
		h.softCombiner.Reset()
	} else {
		decoded = f.Payload
	}

	// Update state BEFORE sending ACK (ensures we return success even if ACK fails)
	h.mu.Lock()
	h.rxSequence = incrementSequence(h.rxSequence)
	h.state = StateIdle
	h.mu.Unlock()

	// Build metadata for diagnostics
	metadata := FrameMetadata{
		Sequence:       f.Header.Sequence,
		Type:           f.Type,
		ReceivedAt:     time.Now(),
		Retransmits:    extractRetransmitCount(f),
		FECDecoded:     fecDecoded,
		PathMetric:     pathMetric,
		CombiningCount: combiningCount,
	}

	result := &ReceiveResult{
		Payload:  decoded,
		Metadata: metadata,
	}

	// Send ACK as best-effort AFTER building result
	// This ensures we return success even if ACK send fails
	_ = h.sendAck(f.Header.Sequence)

	return result, nil
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

// GetExpectedLenForTesting returns the expected decoded length for testing.
func (h *ChaseCombining) GetExpectedLenForTesting() int {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.expectedLen
}

// waitForAck waits for an ACK/NACK response with timeout.
func (h *ChaseCombining) waitForAck(ctx context.Context) (*frame.Frame, error) {
	if ctx == nil {
		return nil, ErrNilContext
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	// Full-duplex SPI: Use the response already received during transmitFrame()
	// In real SPI, the RX72N sent its response simultaneously with our command
	h.mu.Lock()
	data := h.lastRxData
	h.lastRxData = nil // Clear after consuming
	h.mu.Unlock()

	if data == nil {
		// Fallback to legacy behavior if no stored data
		zeros := make([]byte, frame.MaxFrameSize)
		var err error
		data, err = h.transport.Transfer(ctx, zeros)
		if err != nil {
			errCtx := ctx.Err()
			switch {
			case errors.Is(errCtx, context.DeadlineExceeded):
				return nil, ErrTimeout
			case errors.Is(errCtx, context.Canceled):
				return nil, errCtx
			default:
			}
			return nil, err
		}
	}

	if errCtx := ctx.Err(); errCtx != nil {
		switch {
		case errors.Is(errCtx, context.DeadlineExceeded):
			return nil, ErrTimeout
		case errors.Is(errCtx, context.Canceled):
			return nil, errCtx
		default:
			return nil, errCtx
		}
	}

	f, err := h.decoder.Decode(data)
	return f, err
}

// sendNack sends a NACK frame.
func (h *ChaseCombining) sendNack(seq uint16) error {
	// Use a short timeout for control frames to avoid blocking
	ctx, cancel := context.WithTimeout(context.Background(), h.config.Timeout)
	defer cancel()
	return h.sendControlFrame(ctx, frame.FrameTypeNack, seq)
}

func (h *ChaseCombining) sendAck(seq uint16) error {
	ctx, cancel := context.WithTimeout(context.Background(), h.config.Timeout)
	defer cancel()
	err := h.sendControlFrame(ctx, frame.FrameTypeAck, seq)
	return err
}

func (h *ChaseCombining) sendControlFrame(ctx context.Context, frameType frame.Type, seq uint16) error {
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

	// Full-duplex SPI: Send ACK/NACK
	// Control frames are fire-and-forget in HARQ protocol - we don't store the response
	_, err = h.transport.Transfer(ctx, encoded)
	return err // Best-effort: caller ignores errors with `_`
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

// extractRetransmitCount extracts the retransmit flag from frame header.
// Returns 1 if FlagRetransmit is set, 0 otherwise.
func extractRetransmitCount(f *frame.Frame) int {
	if (f.Header.Flags & frame.FlagRetransmit) != 0 {
		return 1
	}
	return 0
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
