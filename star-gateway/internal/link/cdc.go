// Package link provides link layer implementations for the STAR Gateway.
//
// CDCLink implements a simple framing layer for USB CDC transport.
// Unlike SPI, USB CDC is inherently reliable, so CDCLink does NOT implement:
//   - Retransmission logic (USB handles packet errors)
//   - ACK/NACK frames (not needed)
//   - Forward Error Correction (USB has built-in error detection)
//   - Sequence tracking (managed by LinkManager)
//
// STAR Project - Texas A&M University
// January 2026
package link

import (
	"context"
	"errors"
	"fmt"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
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

// CDCLink implements a simple framing layer for USB CDC transport.
// Sequences are managed externally by LinkManager.
type CDCLink struct {
	transport transport.Device
	encoder   frame.Encoder
	decoder   *frame.StreamDecoder
}

// NewCDCLink creates a new CDC link layer.
func NewCDCLink(t transport.Device) (*CDCLink, error) {
	if t == nil {
		return nil, fmt.Errorf("transport cannot be nil")
	}

	adapter := newTransportAdapter(t)

	return &CDCLink{
		transport: t,
		encoder:   frame.NewEncoder(),
		decoder:   frame.NewStreamDecoder(adapter),
	}, nil
}

// Predefined errors.
var (
	ErrCDCTransportNotInitialized = errors.New("CDC transport not initialized")
)

// SendWithSeq sends data with the specified sequence number and flags.
func (c *CDCLink) SendWithSeq(ctx context.Context, data []byte, seq uint16, flags frame.Flags) error {
	// Check context
	if ctx.Err() != nil {
		return ctx.Err()
	}

	if c.transport == nil {
		return ErrCDCTransportNotInitialized
	}

	// Create frame
	// Create frame with provided sequence
	frame := &frame.Frame{
		Header: frame.Header{
			Sequence: seq,
			Length:   uint16(len(data)),
			Flags:    flags,
		},
		Type:    frame.FrameTypeCommand,
		Payload: data,
	}

	// Encode frame (adds SYNC, header, CRC32)
	encodedFrame, err := c.encoder.Encode(frame)
	if err != nil {
		return fmt.Errorf("failed to encode frame: %w", err)
	}

	// Write to transport
	_, err = c.transport.Send(encodedFrame)
	if err != nil {
		return fmt.Errorf("transport send failed: %w", err)
	}

	return nil
}
