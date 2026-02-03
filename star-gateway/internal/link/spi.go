// Package link provides link layer implementations for the STAR Gateway.
//
// SPILink implements a full HARQ (Hybrid ARQ) link layer for SPI transport.
// Unlike CDCLink (lightweight USB), SPILink uses:
//   - ACK/NACK handshake for reliability
//   - Retransmission (up to 3 attempts)
//   - Optional FEC (Forward Error Correction) with Viterbi decoder
//   - Optional Chase Combining for soft-bit accumulation
//   - Sequence tracking via shared SessionState (prevents "Handoff Problem")
//
// Critical Fixes Implemented:
//   - Fix #1: Shared SessionState prevents sequence resets during transport switching
//   - Fix #2: Smart drain logic via FailureType (ACK timeout vs hard IO error)
//   - Fix #5: sendMutex ensures atomic sequence+write (no out-of-order frames)
//
// STAR Project - Texas A&M University
// February 2026
package link

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

const (
	// ackTimeout is the timeout for waiting for ACK/NACK response.
	ackTimeout = 10 * time.Millisecond

	// maxRetries is the maximum number of transmission attempts.
	maxRetries = 3

	// spiSendTimeout is the timeout for sending a frame over SPI.
	spiSendTimeout = 50 * time.Millisecond

	// firstAttempt is the attempt number for the first transmission.
	firstAttempt = 1

	// emptyPayloadLength indicates an empty payload (used for ACK/NACK frames).
	emptyPayloadLength = 0

	// decodeSoftModeDefault is the default mode for FEC soft decoding.
	decodeSoftModeDefault = 0
)

// Predefined errors for SPI link operations.
var (
	ErrSPITransportNotInitialized = errors.New("SPI transport not initialized")
	ErrMaxRetriesExceeded         = errors.New("maximum retries exceeded")
	ErrACKTimeout                 = errors.New("ACK timeout")
)

// SPILinkConfig holds configuration parameters for SPILink.
type SPILinkConfig struct {
	// EnableFEC enables Forward Error Correction (Viterbi K=7, Rate 1/2).
	// Phase 1: false (disabled)
	// Phase 2: true (enabled)
	EnableFEC bool

	// MaxRetries overrides the default maximum retries (default: 3).
	MaxRetries int

	// ACKTimeout overrides the default ACK wait timeout (default: 10ms).
	ACKTimeout time.Duration
}

// DefaultSPILinkConfig returns the default configuration for SPILink.
func DefaultSPILinkConfig() *SPILinkConfig {
	return &SPILinkConfig{
		EnableFEC:  false, // Phase 1: Disabled
		MaxRetries: maxRetries,
		ACKTimeout: ackTimeout,
	}
}

// SPILink implements a full HARQ link layer for SPI transport.
//
// Pattern: Based on CDCLink structure but with HARQ additions:
//   - ACK/NACK handshake
//   - Retransmission logic
//   - FEC encoding/decoding (optional)
//   - Chase Combining for soft-bit accumulation (optional)
type SPILink struct {
	transport    transport.Device      // SPI transport wrapper
	encoder      frame.Encoder         // Frame encoding (SYNC + CRC32)
	decoder      *frame.StreamDecoder  // Frame decoding with validation
	sessionState *manager.SessionState // SHARED across all transports (CRITICAL FIX #1)

	// HARQ-specific components (differences from CDCLink)
	fecEncoder *fec.ConvolutionalEncoder // FEC encoder (Phase 2)
	fecDecoder *fec.ViterbiDecoder       // FEC decoder (Phase 2)
	combiner   *fec.ChaseCombiner        // Chase Combiner (Phase 2)

	sendMutex    sync.Mutex   // Serialize Send operations (CRITICAL FIX #5)
	receiveMutex sync.Mutex   // Serialize Receive operations (prevents decoder race)
	state        harq.State   // HARQ state machine
	stateMu      sync.RWMutex // Protects state field

	config *SPILinkConfig // Configuration
}

// NewSPILink creates a new SPI link layer with shared session state.
//
// CRITICAL: session parameter MUST be shared across all transports (USB and SPI)
// to prevent sequence desynchronization during transport switching.
//
// Parameters:
//   - t: SPI transport device
//   - session: Shared SessionState instance (from TransportManager.GetSessionState())
//   - config: Configuration (nil = use defaults)
//
// Returns:
//   - *SPILink: Initialized SPI link
//   - error: Initialization error (nil/invalid parameters)
func NewSPILink(t transport.Device, session *manager.SessionState, config *SPILinkConfig) (*SPILink, error) {
	if t == nil {
		return nil, fmt.Errorf("transport cannot be nil")
	}
	if session == nil {
		return nil, fmt.Errorf("session state cannot be nil")
	}

	if config == nil {
		config = DefaultSPILinkConfig()
	}

	// Apply defaults for zero values
	if config.MaxRetries <= 0 {
		config.MaxRetries = maxRetries
	}
	if config.ACKTimeout <= 0 {
		config.ACKTimeout = ackTimeout
	}

	// Create transport adapter for StreamDecoder
	adapter := newTransportAdapter(t)

	spiLink := &SPILink{
		transport:    t,
		encoder:      frame.NewEncoder(),
		decoder:      frame.NewStreamDecoder(adapter),
		sessionState: session, // Shared state prevents Handoff Problem
		state:        harq.StateIdle,
		config:       config,
	}

	// Initialize FEC components if enabled
	if config.EnableFEC {
		spiLink.fecEncoder = fec.NewConvolutionalEncoder()
		spiLink.fecDecoder = fec.NewViterbiDecoder()
		spiLink.combiner = fec.NewChaseCombiner(nil) // Use defaults (MaxCombines=3)
	}

	return spiLink, nil
}

// updateState updates the HARQ state machine state.
func (s *SPILink) updateState(newState harq.State) {
	s.stateMu.Lock()
	s.state = newState
	s.stateMu.Unlock()
}

// getState returns the current HARQ state.
func (s *SPILink) getState() harq.State {
	s.stateMu.RLock()
	defer s.stateMu.RUnlock()
	return s.state
}

// ============================================================================
// HARQ Interface Implementation (Send/Receive with ACK/NACK handshake)
// ============================================================================

// sendWithRetry is a private helper that implements the HARQ send logic with retransmission.
// This helper allows both Send() and SendWithType() to share the retry logic.
// Caller must hold s.sendMutex lock.
func (s *SPILink) sendWithRetry(ctx context.Context, data []byte, seq uint16, frameType frame.Type) error {
	// Prepare payload (with optional FEC encoding)
	payload := data
	flags := frame.FlagRequiresAck

	if s.config.EnableFEC {
		encoded, err := s.fecEncoder.Encode(data)
		if err != nil {
			return fmt.Errorf("FEC encode failed: %w", err)
		}
		payload = encoded
		flags |= frame.FlagFECEnabled
	}

	// Retry loop (max 3 attempts)
	maxAttempts := s.config.MaxRetries
	if maxAttempts <= 0 {
		maxAttempts = maxRetries
	}

	for attempt := firstAttempt; attempt <= maxAttempts; attempt++ {
		// Update state based on attempt
		if attempt == firstAttempt {
			s.updateState(harq.StateWaitingAck)
		} else {
			s.updateState(harq.StateRetransmitting)
			flags |= frame.FlagRetransmit
		}

		// Create frame
		f := &frame.Frame{
			Header: frame.Header{
				Sequence: seq,
				Length:   uint16(len(payload)),
				Flags:    flags,
			},
			Type:    frameType,
			Payload: payload,
		}

		// Encode frame (adds SYNC, CRC32)
		encodedFrame, err := s.encoder.Encode(f)
		if err != nil {
			return fmt.Errorf("frame encode failed: %w", err)
		}

		// Send frame with timeout protection
		if err := s.sendFrameWithTimeout(ctx, encodedFrame); err != nil {
			// Send failed - retry
			continue
		}

		// Wait for ACK/NACK
		ack, err := s.waitForAckNack(ctx, seq)
		if err != nil {
			// Timeout or error - retry
			continue
		}

		if ack {
			// ACK received - success
			s.updateState(harq.StateIdle)
			return nil
		}

		// NACK received - retry (loop continues)
	}

	// All retries exhausted
	s.updateState(harq.StateError)
	return ErrMaxRetriesExceeded
}

// Send implements harq.HARQ interface with ACK/NACK handshake and retransmission.
//
// Protocol Flow:
//  1. Lock sendMutex (serialize sends, prevent out-of-order)
//  2. Get next sequence from shared SessionState
//  3. Encode frame with FlagRequiresAck
//  4. Optional: FEC encode payload (if EnableFEC)
//  5. Send frame via transport
//  6. Wait for ACK/NACK (timeout: 10ms)
//  7. On NACK or timeout: Set FlagRetransmit, retry (max 3 total attempts)
//  8. On ACK: Return success
//  9. After 3 failures: Return ErrMaxRetriesExceeded
//
// Thread Safety: sendMutex ensures atomicity (sequence assignment + Send)
func (s *SPILink) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	// Check context before starting
	if ctx.Err() != nil {
		return ctx.Err()
	}

	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both NextTxSequence() AND transport.Send()
	s.sendMutex.Lock()
	defer s.sendMutex.Unlock()

	// Get next sequence from SHARED state (critical for transport switching)
	seq := s.sessionState.NextTxSequence()

	return s.sendWithRetry(ctx, data, seq, frame.FrameTypeCommand)
}

// SendWithType sends data with a specific frame type (PING, PONG, RESET, etc.).
// This enables sending control frames with appropriate types instead of always using COMMAND.
// Uses the full HARQ protocol (ACK/NACK, retransmission, optional FEC).
//
// Example usage:
//   err := link.SendWithType(ctx, []byte{}, frame.FrameTypePing)  // Send PING
//   err := link.SendWithType(ctx, payload, frame.FrameTypeReset) // Send RESET
//
// PHASE 3: Frame Type API - allows HeartbeatManager to send PING frames.
func (s *SPILink) SendWithType(ctx context.Context, data []byte, frameType frame.Type) error {
	// Check context before starting
	if ctx.Err() != nil {
		return ctx.Err()
	}

	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both NextTxSequence() AND transport.Send()
	s.sendMutex.Lock()
	defer s.sendMutex.Unlock()

	// Get next sequence from SHARED state (critical for transport switching)
	seq := s.sessionState.NextTxSequence()

	return s.sendWithRetry(ctx, data, seq, frameType)
}

// sendFrameWithTimeout sends a frame with timeout protection.
// Pattern: Same as CDCLink.sendWithTimeout() to prevent blocking on SPI errors.
//
// GOROUTINE LEAK RISK: If transport.Send() blocks indefinitely (e.g., hardware flow
// control asserted by RX72N and never released), the goroutine will leak even after
// timeout or context cancellation. This is acceptable because:
//  1. SPI hardware should honor flow control timeouts (OS level)
//  2. Leaked goroutines will complete when the transport errors or device resets
//  3. The buffered channel (size 1) prevents goroutine blocking on send
//
// Alternative fixes considered but rejected:
//   - Adding transport.SetWriteDeadline: Not all transport.Device implementations support it
//   - Tracking goroutines: Adds complexity without solving the root cause (blocked hardware)
func (s *SPILink) sendFrameWithTimeout(ctx context.Context, encodedFrame []byte) error {
	type sendResult struct {
		n   int
		err error
	}
	sendCh := make(chan sendResult, 1)

	go func() {
		n, err := s.transport.Send(encodedFrame)
		sendCh <- sendResult{n: n, err: err}
	}()

	// Wait for send completion or timeout
	select {
	case result := <-sendCh:
		if result.err != nil {
			return fmt.Errorf("transport send failed: %w", result.err)
		}
		// Check for short write (partial transmission)
		if result.n < len(encodedFrame) {
			return fmt.Errorf("short write: sent %d bytes, expected %d", result.n, len(encodedFrame))
		}
		return nil
	case <-time.After(spiSendTimeout):
		// NOTE: Goroutine may still be running if Send() is blocked
		return fmt.Errorf("transport send timeout after %v", spiSendTimeout)
	case <-ctx.Done():
		// NOTE: Goroutine may still be running if Send() is blocked
		return ctx.Err()
	}
}

// waitForAckNack waits for ACK/NACK response with timeout.
//
// CRITICAL FIX: Uses goroutine + select to make Decode() interruptible.
// Without this, a silent SPI line would block forever, ignoring the 10ms timeout.
//
// Returns:
//   - (true, nil): ACK received
//   - (false, nil): NACK received
//   - (false, error): Timeout or error
func (s *SPILink) waitForAckNack(ctx context.Context, expectedSeq uint16) (bool, error) {
	type decodeResult struct {
		frame *frame.Frame
		err   error
	}

	decodeCh := make(chan decodeResult, 1)

	// Decode in goroutine to make it interruptible
	go func() {
		f, err := s.decoder.Decode()
		decodeCh <- decodeResult{frame: f, err: err}
	}()

	// Wait for decode, timeout, or context cancellation
	select {
	case result := <-decodeCh:
		if result.err != nil {
			return false, fmt.Errorf("decode failed: %w", result.err)
		}

		f := result.frame

		// Validate frame type
		if f.Type == frame.FrameTypeAck {
			// Validate sequence matches
			if f.Header.Sequence == expectedSeq {
				return true, nil
			}
			return false, fmt.Errorf("ACK sequence mismatch: expected %d, got %d", expectedSeq, f.Header.Sequence)
		} else if f.Type == frame.FrameTypeNack {
			// Validate sequence matches
			if f.Header.Sequence == expectedSeq {
				return false, nil
			}
			return false, fmt.Errorf("NACK sequence mismatch: expected %d, got %d", expectedSeq, f.Header.Sequence)
		}

		return false, fmt.Errorf("expected ACK/NACK, got frame type %s", f.Type)

	case <-time.After(s.config.ACKTimeout):
		// NOTE: This leaves the decode goroutine running if the transport is blocked.
		// The goroutine will complete when data arrives or the transport errors.
		// This is acceptable since ACK/NACK frames are small (12 bytes) and the
		// decoder will discard unexpected frames in the next receive cycle.
		return false, ErrACKTimeout

	case <-ctx.Done():
		return false, ctx.Err()
	}
}

// Receive implements harq.HARQ interface with ACK/NACK response generation.
//
// Protocol Flow:
//  1. Decode frame via StreamDecoder (validates CRC32 automatically)
//  2. Validate sequence using shared SessionState
//  3. Optional: FEC decode payload (if FlagFECEnabled)
//  4. On success: Send ACK, return payload
//  5. On failure: Send NACK, return error
//
// Thread Safety: receiveMutex prevents concurrent calls that would race on s.decoder.
// The decoder maintains internal buffer state and is NOT safe for concurrent access.
func (s *SPILink) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	// Serialize Receive operations to prevent decoder race
	s.receiveMutex.Lock()
	defer s.receiveMutex.Unlock()

	// Check context
	if ctx.Err() != nil {
		return nil, ctx.Err()
	}

	if s.transport == nil {
		return nil, ErrSPITransportNotInitialized
	}

	// Decode next frame from stream
	f, err := s.decoder.Decode()
	if err != nil {
		return nil, fmt.Errorf("failed to decode frame: %w", err)
	}

	// Validate sequence using SHARED state (detects duplicates across transports)
	if !s.sessionState.ValidateRxSequence(f.Header.Sequence) {
		// Out of sequence - send NACK
		if err := s.sendNack(ctx, f.Header.Sequence); err != nil {
			return nil, fmt.Errorf("failed to send NACK for duplicate frame: %w", err)
		}
		return nil, harq.ErrDuplicateFrame
	}

	// Decode payload (with optional FEC decoding)
	payload := f.Payload
	fecDecoded := false

	if s.config.EnableFEC && (f.Header.Flags&frame.FlagFECEnabled) != 0 {
		// FEC decoding path (Phase 2)
		decoded, err := s.decodeFEC(f)
		if err != nil {
			// FEC decode failed - send NACK
			if nackErr := s.sendNack(ctx, f.Header.Sequence); nackErr != nil {
				return nil, fmt.Errorf("failed to send NACK for FEC decode failure: %w", nackErr)
			}
			return nil, fmt.Errorf("FEC decode failed: %w", err)
		}
		payload = decoded
		fecDecoded = true
	}

	// Success - send ACK
	if err := s.sendAck(ctx, f.Header.Sequence); err != nil {
		// ACK send failed - log but don't fail receive
		// (RX72N will timeout and retransmit)
		return nil, fmt.Errorf("ACK send failed: %w", err)
	}

	return &harq.ReceiveResult{
		Payload: payload,
		Metadata: harq.FrameMetadata{
			Sequence:    f.Header.Sequence,
			ReceivedAt:  time.Now(),
			Retransmits: 0, // TODO: Track retransmit count
			FECDecoded:  fecDecoded,
		},
	}, nil
}

// decodeFEC decodes FEC-encoded payload.
// Phase 2 implementation: Chase Combining for retransmissions.
func (s *SPILink) decodeFEC(f *frame.Frame) ([]byte, error) {
	// Convert bytes to soft bits
	softBits := bytesToSoftBits(f.Payload)

	// Check if retransmit (Phase 2: Chase Combining)
	isRetransmit := (f.Header.Flags & frame.FlagRetransmit) != 0

	var decoded []byte
	var err error

	if isRetransmit && s.combiner != nil {
		// Retransmission - add to combiner
		if err := s.combiner.Add(softBits); err != nil {
			return nil, fmt.Errorf("combiner add failed: %w", err)
		}

		// Decode combined soft bits
		combined := s.combiner.Combined()
		decoded, _, err = s.fecDecoder.DecodeSoft(combined, decodeSoftModeDefault)
	} else {
		// First attempt - direct decode
		decoded, _, err = s.fecDecoder.DecodeSoft(softBits, decodeSoftModeDefault)

		if err != nil && s.combiner != nil {
			// Decode failed - store for combining
			s.combiner.Reset()
			if addErr := s.combiner.Add(softBits); addErr != nil {
				return nil, fmt.Errorf("combiner add failed: %w", addErr)
			}
			return nil, err
		}
	}

	if err == nil && s.combiner != nil {
		// Success - reset combiner for next frame
		s.combiner.Reset()
	}

	return decoded, err
}

// sendAck sends an ACK frame for the specified sequence number.
func (s *SPILink) sendAck(ctx context.Context, seq uint16) error {
	ackFrame := &frame.Frame{
		Header: frame.Header{
			Sequence: seq,
			Length:   emptyPayloadLength,
			Flags:    frame.FlagNone,
		},
		Type:    frame.FrameTypeAck,
		Payload: []byte{},
	}

	encodedFrame, err := s.encoder.Encode(ackFrame)
	if err != nil {
		return fmt.Errorf("ACK encode failed: %w", err)
	}

	return s.sendFrameWithTimeout(ctx, encodedFrame)
}

// sendNack sends a NACK frame for the specified sequence number.
func (s *SPILink) sendNack(ctx context.Context, seq uint16) error {
	nackFrame := &frame.Frame{
		Header: frame.Header{
			Sequence: seq,
			Length:   emptyPayloadLength,
			Flags:    frame.FlagNone,
		},
		Type:    frame.FrameTypeNack,
		Payload: []byte{},
	}

	encodedFrame, err := s.encoder.Encode(nackFrame)
	if err != nil {
		return fmt.Errorf("NACK encode failed: %w", err)
	}

	return s.sendFrameWithTimeout(ctx, encodedFrame)
}

// ============================================================================
// HARQ Interface Methods (State, Sequence, Reset)
// ============================================================================

// GetState implements harq.HARQ interface.
// Returns current HARQ state machine state.
func (s *SPILink) GetState() harq.State {
	if s.transport == nil {
		return harq.StateError
	}
	if !s.transport.IsOpen() {
		return harq.StateError
	}
	return s.getState()
}

// GetTxSequence implements harq.HARQ interface.
// Delegates to shared SessionState.
func (s *SPILink) GetTxSequence() uint16 {
	if s.sessionState == nil {
		return 0
	}
	return s.sessionState.GetTxSequence()
}

// GetRxSequence implements harq.HARQ interface.
// Delegates to shared SessionState.
func (s *SPILink) GetRxSequence() uint16 {
	if s.sessionState == nil {
		return 0
	}
	return s.sessionState.GetRxSequence()
}

// Reset implements harq.HARQ interface.
// Delegates to shared SessionState (resets both TX and RX sequences to 0).
// Also resets FEC combiner if enabled.
func (s *SPILink) Reset() {
	if s.sessionState != nil {
		s.sessionState.Reset()
	}

	if s.combiner != nil {
		s.combiner.Reset()
	}

	s.updateState(harq.StateIdle)
}

// ============================================================================
// Helper Functions
// ============================================================================

// bytesToSoftBits converts bytes to soft bits for Viterbi decoder.
// Each bit is converted to confidence level: 1 → +127, 0 → -127.
func bytesToSoftBits(data []byte) []fec.SoftBit {
	softBits := make([]fec.SoftBit, len(data)*8)

	for byteIdx := 0; byteIdx < len(data); byteIdx++ {
		b := data[byteIdx]
		for bitIdx := 0; bitIdx < 8; bitIdx++ {
			bit := (b >> (7 - bitIdx)) & 0x01
			if bit == 1 {
				softBits[byteIdx*8+bitIdx] = fec.SoftBitMax // +127
			} else {
				softBits[byteIdx*8+bitIdx] = fec.SoftBitMin // -127
			}
		}
	}

	return softBits
}
