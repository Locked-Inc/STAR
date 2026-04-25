// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package manager

import (
	"bytes"
	"context"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
)

// readTimeout is the maximum time to wait for HARQ.Receive() to return data.
// This prevents indefinite blocking and allows the reader to fail fast if the
// transport is unresponsive.
const readTimeout = 100 * time.Millisecond

// HARQReader adapts a harq.HARQ interface to io.Reader.
// It buffers payloads received from HARQ and provides them as a byte stream.
//
// Architecture Note:
// This adapter exists because harq.HARQ.Receive() returns complete HARQ packets
// (which may contain multiple frames or partial frames), while StreamDecoder expects
// a continuous byte stream. The adapter bridges this impedance mismatch by:
//  1. Calling HARQ.Receive() to get packet payloads (blocking up to readTimeout)
//  2. Buffering the payload bytes
//  3. Exposing them as an io.Reader for StreamDecoder to parse frame boundaries
//
// Thread Safety: Read() is safe for concurrent calls but only one will make progress
// at a time due to internal mutex. This matches io.Reader contract.
type HARQReader struct {
	h      harq.HARQ
	buffer bytes.Buffer
	mu     sync.Mutex
}

// NewHARQReader creates a new adapter.
func NewHARQReader(h harq.HARQ) *HARQReader {
	if h == nil {
		return nil
	}
	return &HARQReader{
		h: h,
	}
}

// Read implements io.Reader.
func (r *HARQReader) Read(p []byte) (int, error) {
	// Fast path: read from buffer
	r.mu.Lock()
	if r.buffer.Len() > 0 {
		n, err := r.buffer.Read(p)
		r.mu.Unlock()
		return n, err
	}
	r.mu.Unlock()

	// Slow path: fetch new data
	// Release mutex before blocking call to prevent deadlock/blocking other readers
	// Use a timeout context for safety
	ctx, cancel := context.WithTimeout(context.Background(), readTimeout)
	defer cancel()

	result, err := r.h.Receive(ctx)

	r.mu.Lock()
	defer r.mu.Unlock()

	if err != nil {
		return 0, err
	}

	// Buffer the payload
	// TODO: Metadata (Retransmits, FECDecoded) from result is lost here.
	// We need a way to propagate this if StreamDecoder/Frame ever supports it.
	r.buffer.Write(result.Payload)

	// Satisfy read
	return r.buffer.Read(p)
}
