// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

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
	"log"
	"sync"
	"sync/atomic"
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

	// decodeSoftModeDefault is the default mode for FEC soft decoding.
	decodeSoftModeDefault = 0

	// spiSendChanBuffer is the buffer size for send result channel (prevents blocking).
	spiSendChanBuffer = 1

	// Soft-bit conversion constants
	bitWidth = 8    // Bits per byte
	msbShift = 7    // Shift amount to extract MSB
	bitMask  = 0x01 // Mask to extract single bit

	// Frame metadata defaults
	defaultPathMetric     = 0 // Default path metric when FEC is not used
	defaultCombiningCount = 1 // Default combining count (single decode attempt)

	// Retransmit count extraction constants
	retransmitCountSet   = 1 // Frame is a retransmission
	retransmitCountUnset = 0 // Frame is first transmission
)

type contextTransportAdapter struct {
	transport transport.Device
	ctx       atomic.Value
	mu        sync.Mutex
	pending   []byte
}

func newContextTransportAdapter(t transport.Device) *contextTransportAdapter {
	return &contextTransportAdapter{transport: t}
}

func (a *contextTransportAdapter) SetContext(ctx context.Context) {
	a.ctx.Store(ctx)
}

func (a *contextTransportAdapter) Read(p []byte) (int, error) {
	if len(p) == 0 {
		return 0, nil
	}

	a.mu.Lock()
	defer a.mu.Unlock()

	if len(a.pending) > 0 {
		n := copy(p, a.pending)
		a.pending = a.pending[n:]
		return n, nil
	}

	ctx := context.Background()
	if stored := a.ctx.Load(); stored != nil {
		if storedCtx, ok := stored.(context.Context); ok && storedCtx != nil {
			ctx = storedCtx
		}
	}

	txBuf := make([]byte, len(p))
	rxBuf, err := a.transport.Transfer(ctx, txBuf)
	n := copy(p, rxBuf)
	if len(rxBuf) > n {
		if cap(a.pending) < len(rxBuf)-n {
			a.pending = make([]byte, 0, len(rxBuf)-n)
		} else {
			a.pending = a.pending[:0]
		}
		a.pending = append(a.pending, rxBuf[n:]...)
	}
	if err != nil {
		return n, err
	}
	return n, nil
}

// Predefined errors for SPI link operations.
var (
	ErrSPITransportNotInitialized = errors.New("SPI transport not initialized")
	ErrMaxRetriesExceeded         = errors.New("maximum retries exceeded")
	ErrACKTimeout                 = errors.New("ACK timeout")
)

// SPILinkConfig holds configuration parameters for SPILink.
type SPILinkConfig struct {
	// EnableFEC enables Forward Error Correction (Viterbi K=7, Rate 1/2).
	// Default: Enable FEC by default to improve reliability on noisy SPI links.
	EnableFEC bool

	// MaxRetries overrides the default maximum retries (default: 3).
	MaxRetries int

	// ACKTimeout overrides the default ACK wait timeout (default: 10ms).
	ACKTimeout time.Duration
}

// DefaultSPILinkConfig returns the default configuration for SPILink.
func DefaultSPILinkConfig() *SPILinkConfig {
	return &SPILinkConfig{
		EnableFEC:  true, // Enabled by default
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
	sessionState *manager.SessionState // SHARED across all transports
	adapter      *contextTransportAdapter

	// HARQ-specific components (differences from CDCLink)
	fecEncoder *fec.ConvolutionalEncoder // FEC encoder (Phase 2)
	fecDecoder *fec.ViterbiDecoder       // FEC decoder (Phase 2)
	combiner   *fec.ChaseCombiner        // Chase Combiner (Phase 2)

	// ACK dispatch system
	// Maps sequence number to ACK result channel
	// When Send() waits for ACK, it registers a channel here
	// When Receive() gets ACK/NACK, it dispatches to registered channel
	// This ensures ONLY Receive() reads from decoder (prevents race)
	ackChannels map[uint16]chan bool // true=ACK, false=NACK
	ackMu       sync.Mutex           // Protects ackChannels map

	sendMutex    sync.Mutex   // Serialize Send operations
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
	adapter := newContextTransportAdapter(t)

	spiLink := &SPILink{
		transport:    t,
		encoder:      frame.NewEncoder(),
		decoder:      frame.NewStreamDecoder(adapter),
		sessionState: session, // Shared state prevents Handoff Problem
		adapter:      adapter,
		state:        harq.StateIdle,
		config:       config,
		ackChannels:  make(map[uint16]chan bool), // Initialize ACK dispatch map
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
// ACK Dispatch System
// ============================================================================

// registerAckChannel registers a channel to receive ACK/NACK for the given sequence.
// Returns the channel that will receive true (ACK) or false (NACK).
func (s *SPILink) registerAckChannel(seq uint16) chan bool {
	s.ackMu.Lock()
	defer s.ackMu.Unlock()

	ch := make(chan bool, 1) // Buffered to prevent blocking
	s.ackChannels[seq] = ch
	return ch
}

// unregisterAckChannel removes the ACK channel for the given sequence.
func (s *SPILink) unregisterAckChannel(seq uint16) {
	s.ackMu.Lock()
	defer s.ackMu.Unlock()

	delete(s.ackChannels, seq)
}

// dispatchAck dispatches an ACK/NACK result to the waiting Send() goroutine.
// Returns true if a channel was registered for this sequence (dispatched),
// false if no one was waiting (unexpected ACK/NACK).
func (s *SPILink) dispatchAck(seq uint16, isAck bool) bool {
	s.ackMu.Lock()
	ch, exists := s.ackChannels[seq]
	s.ackMu.Unlock()

	if !exists {
		return false // No one waiting for this sequence
	}

	// Send result to channel (non-blocking due to buffer)
	select {
	case ch <- isAck:
		return true
	default:
		// Channel full or closed, shouldn't happen
		return false
	}
}

// ============================================================================
// HARQ Interface Implementation (Send/Receive with ACK/NACK handshake)
// ============================================================================

// sendWithRetry is a private helper that implements the core retry logic with ACK/NACK handshake.
// Used by both Send() and SendWithType() to avoid code duplication.
//
// Caller must hold s.sendMutex lock.
//
// Protocol Flow:
//  1. Register ACK channel
//  2. Optional: FEC encode payload (if EnableFEC)
//  3. Retry loop (max 3 attempts):
//     a. Check context cancellation
//     b. Create frame with provided frameType
//     c. Encode frame (adds SYNC, CRC32)
//     d. Send via transport
//     e. Wait for ACK/NACK on registered channel (timeout: 10ms)
//     f. On NACK or timeout: Set FlagRetransmit, retry
//     g. On ACK: Return success
//  4. After 3 failures: Return ErrMaxRetriesExceeded
func (s *SPILink) sendWithRetry(ctx context.Context, data []byte, seq uint16, frameType frame.Type) error {
	// Check context before starting
	if ctx.Err() != nil {
		return ctx.Err()
	}

	// Register ACK channel
	// Only Receive() will read from decoder and dispatch to this channel
	ackCh := s.registerAckChannel(seq)
	defer s.unregisterAckChannel(seq)

	// Prepare payload (with optional FEC encoding)
	payload := data
	flags := frame.FlagRequiresAck

	// Only apply FEC encoding for non-empty payloads. Control frames (empty)
	// must not be passed to the convolutional encoder (it rejects empty data).
	if s.config.EnableFEC && len(data) > 0 {
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
		// CRITICAL FIX: Check context cancellation in retry loop
		if ctx.Err() != nil {
			s.updateState(harq.StateIdle)
			return ctx.Err()
		}

		// Update state based on attempt
		if attempt == firstAttempt {
			s.updateState(harq.StateWaitingAck)
		} else {
			s.updateState(harq.StateRetransmitting)
			flags |= frame.FlagRetransmit
		}

		// Create frame with provided frameType
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
			// Send failed - check context before retry
			if ctx.Err() != nil {
				s.updateState(harq.StateIdle)
				return ctx.Err()
			}
			continue
		}

		// Wait for ACK/NACK on registered channel (Receive() will dispatch)
		select {
		case isAck := <-ackCh:
			if isAck {
				// ACK received - success
				s.updateState(harq.StateIdle)
				return nil
			}
			// NACK received - retry (loop continues)

		case <-time.After(s.config.ACKTimeout):
			// Timeout - retry (loop continues)

		case <-ctx.Done():
			// Context cancelled
			s.updateState(harq.StateIdle)
			return ctx.Err()
		}
	}

	// All retries exhausted
	s.updateState(harq.StateError)
	return ErrMaxRetriesExceeded
}

// Send implements harq.HARQ interface with ACK/NACK handshake and retransmission.
// Uses frame.FrameTypeCommand for all data frames.
//
// Thread Safety: sendMutex ensures atomicity (sequence assignment + Send)
func (s *SPILink) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both NextTxSequence() AND transport.Send()
	s.sendMutex.Lock()
	defer s.sendMutex.Unlock()

	// Get next sequence from SHARED state (critical for transport switching)
	seq := s.sessionState.NextTxSequence()

	// Delegate to sendWithRetry helper
	return s.sendWithRetry(ctx, data, seq, frame.FrameTypeCommand)
}

// SendWithType sends data with explicit frame type (PING, PONG, RESET, etc.).
// This allows explicit control over the frame type for protocol messages.
// Phase 3: Frame Type API implementation.
//
// Thread Safety: sendMutex ensures atomicity (sequence assignment + Send)
func (s *SPILink) SendWithType(ctx context.Context, data []byte, frameType frame.Type) error {
	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both NextTxSequence() AND transport.Send()
	s.sendMutex.Lock()
	defer s.sendMutex.Unlock()

	// Get next sequence from SHARED state (critical for transport switching)
	seq := s.sessionState.NextTxSequence()

	// Delegate to sendWithRetry helper
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
	sendCh := make(chan sendResult, spiSendChanBuffer)

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
// func (s *SPILink) waitForAckNack(ctx context.Context, expectedSeq uint16) (bool, error) {
// 	type decodeResult struct {
// 		frame *frame.Frame
// 		err   error
// 	}

// 	decodeCh := make(chan decodeResult, 1)

// 	// Decode in goroutine to make it interruptible
// 	go func() {
// 		f, err := s.decoder.Decode()
// 		decodeCh <- decodeResult{frame: f, err: err}
// 	}()

// 	// Wait for decode, timeout, or context cancellation
// 	select {
// 	case result := <-decodeCh:
// 		if result.err != nil {
// 			return false, fmt.Errorf("decode failed: %w", result.err)
// 		}

// 		f := result.frame

// 		// Validate frame type
// 		if f.Type == frame.FrameTypeAck {
// 			// Validate sequence matches
// 			if f.Header.Sequence == expectedSeq {
// 				return true, nil
// 			}
// 			return false, fmt.Errorf("ACK sequence mismatch: expected %d, got %d", expectedSeq, f.Header.Sequence)
// 		} else if f.Type == frame.FrameTypeNack {
// 			// Validate sequence matches
// 			if f.Header.Sequence == expectedSeq {
// 				return false, nil
// 			}
// 			return false, fmt.Errorf("NACK sequence mismatch: expected %d, got %d", expectedSeq, f.Header.Sequence)
// 		}

// 		return false, fmt.Errorf("expected ACK/NACK, got frame type %s", f.Type)

// 	case <-time.After(s.config.ACKTimeout):
// 		// NOTE: This leaves the decode goroutine running if the transport is blocked.
// 		// The goroutine will complete when data arrives or the transport errors.
// 		// This is acceptable since ACK/NACK frames are small (12 bytes) and the
// 		// decoder will discard unexpected frames in the next receive cycle.
// 		return false, ErrACKTimeout

// 	case <-ctx.Done():
// 		return false, ctx.Err()
// 	}
// }

// Receive implements harq.HARQ interface with ACK/NACK response generation.
//
// Protocol Flow:
//  1. Decode frame via StreamDecoder (validates CRC32 automatically)
//  2. CRITICAL FIX: Check if ACK/NACK frame - dispatch to Send() if so
//  3. Validate sequence using shared SessionState
//  4. CRITICAL FIX: On duplicate - send ACK (not NACK) to stop retransmit loop
//  5. Optional: FEC decode payload (if FlagFECEnabled)
//  6. On success: Send ACK if required, return payload
//  7. CRITICAL FIX: Don't fail if ACK send fails (just log)
//  8. On failure: Send NACK, return error
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

	if s.adapter != nil {
		s.adapter.SetContext(ctx)
	}

	// Decode next frame from stream
	// CRITICAL: Only Receive() reads from decoder (Split Reader fix)
	// CRC failures trigger NACK + retry (up to MaxRetries). All other errors are fatal.
	var f *frame.Frame
	crcRetries := 0 // Per-call retry budget; resets for each Receive() invocation
	for {
		var decodeErr error
		f, decodeErr = s.decoder.Decode()
		if decodeErr == nil {
			break
		}
		var crcErr *frame.CRCError
		if errors.As(decodeErr, &crcErr) {
			crcRetries++
			// Send NACK so sender retransmits, per HARQ receive path step 5.
			// Skip NACK send if context is already cancelled.
			if ctx.Err() == nil {
				if nackErr := s.sendNack(ctx, crcErr.Sequence); nackErr != nil {
					log.Printf("WARN: Failed to send NACK for CRC failure seq=%d: %v", crcErr.Sequence, nackErr)
				}
			}
			if crcRetries > s.config.MaxRetries {
				return nil, fmt.Errorf("exceeded CRC retry limit after %d attempts for seq=%d", crcRetries-1, crcErr.Sequence)
			}
			// Check after sendNack in case context was cancelled during the send.
			if ctxErr := ctx.Err(); ctxErr != nil {
				return nil, ctxErr
			}
			continue
		}
		return nil, fmt.Errorf("failed to decode frame: %w", decodeErr)
	}

	// Check if this is an ACK/NACK control frame
	// If so, dispatch to waiting Send() goroutine and return
	if f.Type == frame.FrameTypeAck || f.Type == frame.FrameTypeNack {
		isAck := (f.Type == frame.FrameTypeAck)
		s.dispatchAck(f.Header.Sequence, isAck)
		// Return nil to indicate control frame handled (not data)
		return nil, nil
	}

	// Validate sequence using SHARED state (detects duplicates across transports)
	if !s.sessionState.ValidateRxSequence(f.Header.Sequence) {
		// Duplicate detected - sender likely missed our previous ACK
		// Send ACK (not NACK) to stop retransmission loop
		// Skip ACK resend if context is cancelled (graceful shutdown in progress)
		if ctx.Err() == nil {
			if err := s.sendAck(ctx, f.Header.Sequence); err != nil {
				log.Printf("WARN: Failed to resend ACK for duplicate seq=%d: %v", f.Header.Sequence, err)
			}
		}
		return nil, harq.ErrDuplicateFrame
	}

	// Decode payload (with optional FEC decoding)
	payload := f.Payload
	fecDecoded := false
	pathMetric := defaultPathMetric         // Path metric for FEC decoder confidence
	combiningCount := defaultCombiningCount // Number of combining attempts

	if s.config.EnableFEC && (f.Header.Flags&frame.FlagFECEnabled) != 0 {
		// FEC decoding path (Phase 2)
		decoded, metric, count, err := s.decodeFEC(f)
		if err != nil {
			// FEC decode failed - send NACK
			// Skip NACK send if context is cancelled
			if ctx.Err() == nil {
				if nackErr := s.sendNack(ctx, f.Header.Sequence); nackErr != nil {
					log.Printf("WARN: Failed to send NACK for FEC decode failure seq=%d: %v", f.Header.Sequence, nackErr)
				}
			}
			return nil, fmt.Errorf("FEC decode failed: %w", err)
		}
		payload = decoded
		pathMetric = metric
		combiningCount = count
		fecDecoded = true
	}

	// Success - send ACK if frame requires it
	if (f.Header.Flags & frame.FlagRequiresAck) != 0 {
		// Skip ACK send if context is cancelled (graceful shutdown in progress)
		if ctx.Err() == nil {
			if err := s.sendAck(ctx, f.Header.Sequence); err != nil {
				// Don't fail receive if ACK send fails
				// We successfully decoded the payload, just log the error
				// Sender will timeout and retransmit, we'll send ACK again
				log.Printf("WARN: ACK send failed for seq=%d: %v (payload decoded successfully)", f.Header.Sequence, err)
			}
		}
	}

	return &harq.ReceiveResult{
		Payload: payload,
		Metadata: harq.FrameMetadata{
			Sequence:       f.Header.Sequence,
			Type:           f.Type,
			ReceivedAt:     time.Now(),
			Retransmits:    extractRetransmitCount(f.Header.Flags),
			FECDecoded:     fecDecoded,
			PathMetric:     pathMetric,
			CombiningCount: combiningCount,
		},
	}, nil
}

// decodeFEC decodes FEC-encoded payload.
// Phase 2 implementation: Chase Combining for retransmissions.
// Returns decoded data, path metric (lower is better), combining count, and error.
func (s *SPILink) decodeFEC(f *frame.Frame) ([]byte, int, int, error) {
	// Convert bytes to soft bits
	softBits := bytesToSoftBits(f.Payload)

	// Check if retransmit (Phase 2: Chase Combining)
	isRetransmit := (f.Header.Flags & frame.FlagRetransmit) != 0

	var decoded []byte
	var pathMetric int
	var err error
	combiningCount := defaultCombiningCount

	if isRetransmit && s.combiner != nil {
		// Retransmission - add to combiner
		err = s.combiner.Add(softBits)
		if err != nil {
			combiningCount = s.combiner.Count() // Capture actual count before returning
			return nil, defaultPathMetric, combiningCount, fmt.Errorf("combiner add failed: %w", err)
		}

		// Decode combined soft bits
		combined := s.combiner.Combined()
		decoded, pathMetric, err = s.fecDecoder.DecodeSoft(combined, decodeSoftModeDefault)
		combiningCount = s.combiner.Count()

		// Reset combiner on success
		if err == nil {
			s.combiner.Reset()
		}
	} else {
		// First attempt - direct decode
		decoded, pathMetric, err = s.fecDecoder.DecodeSoft(softBits, decodeSoftModeDefault)

		if err != nil && s.combiner != nil {
			// Decode failed - store for combining
			s.combiner.Reset()
			if addErr := s.combiner.Add(softBits); addErr != nil {
				return nil, defaultPathMetric, combiningCount, fmt.Errorf("combiner add failed: %w", addErr)
			}
			combiningCount = s.combiner.Count()
			return nil, pathMetric, combiningCount, err
		}
		// First attempt success: combiningCount remains at defaultCombiningCount (1)
	}

	return decoded, pathMetric, combiningCount, err
}

// sendAck sends an ACK frame for the specified sequence number.
func (s *SPILink) sendAck(ctx context.Context, seq uint16) error {
	ackFrame := &frame.Frame{
		Header: frame.Header{
			Sequence: seq,
			Length:   frame.EmptyPayloadLength,
			Flags:    frame.FlagNone,
		},
		Type:    frame.FrameTypeAck,
		Payload: frame.EmptyPayload,
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
			Length:   frame.EmptyPayloadLength,
			Flags:    frame.FlagNone,
		},
		Type:    frame.FrameTypeNack,
		Payload: frame.EmptyPayload,
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
// Each bit is converted to confidence level: 1 -> +127, 0 -> -127.
func bytesToSoftBits(data []byte) []fec.SoftBit {
	softBits := make([]fec.SoftBit, len(data)*bitWidth)

	for byteIdx := 0; byteIdx < len(data); byteIdx++ {
		b := data[byteIdx]
		for bitIdx := 0; bitIdx < bitWidth; bitIdx++ {
			bit := (b >> (msbShift - bitIdx)) & bitMask
			if bit == bitMask {
				softBits[byteIdx*bitWidth+bitIdx] = fec.SoftBitMax // +127
			} else {
				softBits[byteIdx*bitWidth+bitIdx] = fec.SoftBitMin // -127
			}
		}
	}

	return softBits
}

// extractRetransmitCount extracts the retransmit flag from frame header.
// Returns retransmitCountSet if FlagRetransmit is set, retransmitCountUnset otherwise.
func extractRetransmitCount(flags frame.Flags) int {
	if (flags & frame.FlagRetransmit) != 0 {
		return retransmitCountSet
	}
	return retransmitCountUnset
}
