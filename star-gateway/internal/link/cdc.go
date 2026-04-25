// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package link provides link layer implementations for the STAR Gateway.
//
// CDCLink implements a lightweight link layer for USB CDC transport.
// Unlike SPI (which uses full HARQ with retransmissions and FEC), USB CDC
// relies on hardware-level reliability. CDCLink provides:
//   - CRC32 validation (end-to-end integrity)
//   - Sequence tracking via shared SessionState (prevents "Handoff Problem")
//   - Gap tolerance (accepts sequence gaps <10 frames for packet loss recovery)
//   - Send serialization via sendMutex (prevents out-of-order transmission)
//   - NO retransmission (failures propagate to TransportManager for failover)
//   - NO FEC encoding/decoding (USB handles error correction)
//   - NO ACK/NACK frames (USB provides implicit ACK)
//
// Critical Fixes Implemented:
//   - Fix #1: Shared SessionState prevents sequence resets during transport switching
//   - Fix #5: sendMutex ensures atomic sequence+write (no out-of-order frames)
//   - Fix #6: Gap tolerance allows recovery from rare USB packet loss
//   - Fix #7: Context timeout prevents blocked writes on USB disconnect
//
// STAR Project - Texas A&M University
// January 2026
package link

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

const (
	// Frame metadata defaults for CDC protocol (lightweight, no FEC/combining)
	cdcDefaultPathMetric     = 0     // Perfect metric (no FEC, hardware reliability)
	cdcDefaultCombiningCount = 1     // Single attempt (no Chase combining)
	cdcDefaultFECDecoded     = false // No FEC in CDC protocol
)

// transportAdapter bridges transport.Device to io.Reader for StreamDecoder.
type transportAdapter struct {
	transport transport.Device
}

// newTransportAdapter creates a new transport adapter.
func newTransportAdapter(t transport.Device) *transportAdapter {
	return &transportAdapter{transport: t}
}

// Read implements io.Reader by calling transport.Receive().
// Handles partial reads and timeouts correctly per io.Reader contract.
func (a *transportAdapter) Read(p []byte) (n int, err error) {
	data, err := a.transport.Receive(len(p))
	n = copy(p, data)

	// Process n > 0 BEFORE checking error
	// This handles partial reads before timeout
	if n > 0 && err == transport.ErrReadTimeout {
		return n, nil // Return partial data, keep reading
	}

	// Empty timeout is non-fatal for decoder
	if err == transport.ErrReadTimeout {
		return 0, nil // StreamDecoder will retry
	}

	// Fatal errors (disconnect, IO error)
	if err != nil {
		return n, err
	}

	return n, nil
}

// CDCLink implements a lightweight link layer for USB CDC transport.
// Unlike SPILink (full HARQ), CDCLink relies on USB hardware reliability.
// Sequences are managed by shared SessionState (prevents Handoff Problem).
type CDCLink struct {
	transport    transport.Device
	encoder      frame.Encoder
	decoder      *frame.StreamDecoder
	sessionState *manager.SessionState // CRITICAL FIX #1: Shared across all transports
	sendMutex    sync.Mutex            // CRITICAL FIX #5: Serialize Send operations
}

// NewCDCLink creates a new CDC link layer with shared session state.
// CRITICAL: session parameter MUST be shared across all transports (USB and SPI)
// to prevent sequence desynchronization during transport switching.
func NewCDCLink(t transport.Device, session *manager.SessionState) (*CDCLink, error) {
	if t == nil {
		return nil, fmt.Errorf("transport cannot be nil")
	}
	if session == nil {
		return nil, fmt.Errorf("session state cannot be nil")
	}

	adapter := newTransportAdapter(t)

	return &CDCLink{
		transport:    t,
		encoder:      frame.NewEncoder(),
		decoder:      frame.NewStreamDecoder(adapter),
		sessionState: session, // Shared state prevents Handoff Problem
	}, nil
}

// Predefined errors.
var (
	ErrCDCTransportNotInitialized = errors.New("CDC transport not initialized")
)

// sendWithTimeout sends an encoded frame with timeout protection.
// CRITICAL FIX #7: Prevents blocked writes on USB disconnect.
//
// This helper spawns a goroutine to call transport.Send() and waits on a buffered
// result channel. It selects between:
//   - Successful send completion
//   - Timeout (50ms) - returns error
//   - Context cancellation - returns ctx.Err()
//
// Note: If timeout or context cancellation occurs, the background goroutine may
// still be blocked in transport.Send(). For USB CDC, this is typically brief as
// the OS buffers the write, but in pathological cases (device stuck) the goroutine
// may leak. This is acceptable as USB disconnect will eventually unblock the syscall
// and the goroutine will complete.
func (c *CDCLink) sendWithTimeout(ctx context.Context, encodedFrame []byte) error {
	type sendResult struct {
		n   int
		err error
	}
	sendCh := make(chan sendResult, 1)

	go func() {
		n, err := c.transport.Send(encodedFrame)
		sendCh <- sendResult{n: n, err: err}
	}()

	// Wait for send completion or timeout
	const sendTimeout = 50 * time.Millisecond
	select {
	case result := <-sendCh:
		if result.err != nil {
			return fmt.Errorf("transport send failed: %w", result.err)
		}
		return nil
	case <-time.After(sendTimeout):
		return fmt.Errorf("transport send timeout after %v", sendTimeout)
	case <-ctx.Done():
		return ctx.Err()
	}
}

// buildAndSend is a private helper that encapsulates the common send logic.
// It performs context/transport checks, creates a frame, encodes it, and sends with timeout.
// Caller must hold c.sendMutex lock.
func (c *CDCLink) buildAndSend(ctx context.Context, data []byte, seq uint16, flags frame.Flags, frameType frame.Type) error {
	// Check context
	if ctx.Err() != nil {
		return ctx.Err()
	}

	if c.transport == nil {
		return ErrCDCTransportNotInitialized
	}

	// Create frame with provided sequence, flags, and type
	f := &frame.Frame{
		Header: frame.Header{
			Sequence: seq,
			Length:   uint16(len(data)),
			Flags:    flags,
		},
		Type:    frameType,
		Payload: data,
	}

	// Encode frame (adds SYNC, header, CRC32)
	encodedFrame, err := c.encoder.Encode(f)
	if err != nil {
		return fmt.Errorf("failed to encode frame: %w", err)
	}

	// CRITICAL FIX #7: Use timeout protection to prevent blocking on USB disconnect
	return c.sendWithTimeout(ctx, encodedFrame)
}

// SendWithSeq sends data with the specified sequence number and flags.
// CRITICAL FIX #5: sendMutex ensures atomic sequence+write (no out-of-order frames).
// CRITICAL FIX #7: Context timeout prevents blocked writes on USB disconnect.
func (c *CDCLink) SendWithSeq(ctx context.Context, data []byte, seq uint16, flags frame.Flags) error {
	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both sequence assignment AND transport.Send()
	c.sendMutex.Lock()
	defer c.sendMutex.Unlock()

	return c.buildAndSend(ctx, data, seq, flags, frame.FrameTypeCommand)
}

// ============================================================================
// HARQ Interface Implementation (Lightweight Protocol)
// ============================================================================

// Send implements harq.HARQ interface.
// Uses shared SessionState for sequence management (CRITICAL FIX #1).
// Serializes Send operations with sendMutex (CRITICAL FIX #5).
func (c *CDCLink) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both NextTxSequence() AND transport.Send()
	c.sendMutex.Lock()
	defer c.sendMutex.Unlock()

	// Get next sequence from SHARED state (critical for transport switching)
	seq := c.sessionState.NextTxSequence()

	// No RequiresAck, No FEC for lightweight CDC protocol
	return c.buildAndSend(ctx, data, seq, frame.FlagNone, frame.FrameTypeCommand)
}

// SendWithType sends data with a specific frame type (PING, PONG, RESET, etc.).
// This allows explicit control over the frame type for protocol messages.
// Phase 3: Frame Type API implementation.
func (c *CDCLink) SendWithType(ctx context.Context, data []byte, frameType frame.Type) error {
	// CRITICAL FIX #5: Serialize Send to prevent out-of-order transmission
	// Lock MUST cover both NextTxSequence() AND transport.Send()
	c.sendMutex.Lock()
	defer c.sendMutex.Unlock()

	// Get next sequence from SHARED state (critical for transport switching)
	seq := c.sessionState.NextTxSequence()

	// No RequiresAck, No FEC for lightweight CDC protocol
	return c.buildAndSend(ctx, data, seq, frame.FlagNone, frameType)
}

// Receive implements harq.HARQ interface.
// Validates CRC32 (decoder does this) and sequence using shared SessionState.
// CRITICAL FIX #6: Gap tolerance accepts sequence gaps <10 frames.
func (c *CDCLink) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	// Check context
	if ctx.Err() != nil {
		return nil, ctx.Err()
	}

	if c.transport == nil {
		return nil, ErrCDCTransportNotInitialized
	}

	// Decode next frame from stream
	f, err := c.decoder.Decode()
	if err != nil {
		return nil, fmt.Errorf("failed to decode frame: %w", err)
	}

	// Validate CRC32 (decoder already does this - if we got here, CRC is valid)
	// Drop frame if corrupt (NO retry request for USB)

	// Validate sequence on DATA frames only (COMMAND, RESPONSE).
	// Control frames (PING, PONG, RESET, RESET_ACK, ACK, NACK) bypass
	// sequence validation so they are delivered during reset handshakes
	// when the barrier is active.
	if f.Type == frame.FrameTypeCommand || f.Type == frame.FrameTypeResponse {
		if !c.sessionState.ValidateRxSequence(f.Header.Sequence) {
			return nil, harq.ErrDuplicateFrame
		}
	}

	return &harq.ReceiveResult{
		Payload: f.Payload,
		Metadata: harq.FrameMetadata{
			Sequence:       f.Header.Sequence,
			Type:           f.Type,
			ReceivedAt:     time.Now(),
			Retransmits:    extractRetransmitCount(f.Header.Flags),
			FECDecoded:     cdcDefaultFECDecoded,     // No FEC in lightweight CDC protocol
			PathMetric:     cdcDefaultPathMetric,     // No FEC = perfect metric
			CombiningCount: cdcDefaultCombiningCount, // No combining in CDC protocol
		},
	}, nil
}

// GetState implements harq.HARQ interface.
// CDCLink has no retransmission state machine, always returns Idle or Error.
func (c *CDCLink) GetState() harq.State {
	if c.transport == nil {
		return harq.StateError
	}
	if !c.transport.IsOpen() {
		return harq.StateError
	}
	return harq.StateIdle
}

// GetTxSequence implements harq.HARQ interface.
// Delegates to shared SessionState.
func (c *CDCLink) GetTxSequence() uint16 {
	if c.sessionState == nil {
		return 0
	}
	return c.sessionState.GetTxSequence()
}

// GetRxSequence implements harq.HARQ interface.
// Delegates to shared SessionState.
func (c *CDCLink) GetRxSequence() uint16 {
	if c.sessionState == nil {
		return 0
	}
	return c.sessionState.GetRxSequence()
}

// Reset implements harq.HARQ interface.
// Delegates to shared SessionState (resets both TX and RX sequences to 0).
// CRITICAL: This is called after receiving RESET_ACK from RX72N to synchronize
// sequences after Gateway restart (prevents restart deadlock - FIX #4).
func (c *CDCLink) Reset() {
	if c.sessionState != nil {
		c.sessionState.Reset()
	}
}
