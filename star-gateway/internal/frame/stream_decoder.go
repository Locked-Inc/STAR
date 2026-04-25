// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package frame provides a streaming decoder for the STAR protocol.
package frame

import (
	"bufio"
	"encoding/binary"
	"io"
	"sync"
	"time"
)

// Header field offsets within the header buffer
const (
	seqOffset   = 0
	lenOffset   = 2
	typeOffset  = 4
	flagsOffset = 5
)

// DecodeMetrics tracks decoder health statistics.
type DecodeMetrics struct {
	TotalFrames  uint64
	SyncErrors   uint64 // Bytes skipped while searching for sync
	CRCErrors    uint64 // Frames with invalid CRC
	LengthErrors uint64 // Frames with invalid length
	LastSuccess  time.Time
}

// StreamDecoder reads and decodes frames from a continuous stream.
// It handles synchronization loss, variable length payloads, and CRC validation.
//
// Thread Safety:
// StreamDecoder enforces single-reader semantics. Only one goroutine should call
// Decode() at a time. Concurrent calls will serialize via internal mutex.
// GetMetrics() may briefly block if Decode() is active, but this is typically
// negligible (<1ms) since metrics updates are fast.
//
// The mutex protects both buffer state and metrics to ensure consistency.
// This design trades some concurrency for simplicity and correctness.
type StreamDecoder struct {
	r       *bufio.Reader
	mu      sync.Mutex
	metrics DecodeMetrics
}

// NewStreamDecoder creates a new decoder reading from r.
func NewStreamDecoder(r io.Reader) *StreamDecoder {
	return &StreamDecoder{
		r: bufio.NewReaderSize(r, MaxFrameSize*2), // Double buffer size for efficiency
	}
}

// Decode blocks until a valid frame is read or an error occurs.
// It automatically handles resynchronization on stream errors.
func (d *StreamDecoder) Decode() (*Frame, error) {
	// Only lock for reading from the buffer/stream state, but we need
	// exclusive access to the reader `r` anyway.
	d.mu.Lock()
	defer d.mu.Unlock()

	for {
		// 1. Sync Search
		// Read 2 bytes for SyncWord.
		// Since we need to byte-scan if match fails, we'll read one byte, then another.
		// Optimized: Peek 2 bytes. If match, consume. If not, advance 1 byte.

		// Ensure we have at least 2 bytes (SyncSize)
		peekBuf, err := d.r.Peek(SyncSize)
		if err != nil {
			return nil, err
		}

		syncCandidate := binary.LittleEndian.Uint16(peekBuf)
		if syncCandidate != SyncWord {
			// Discard 1 byte and retry
			if _, err := d.r.Discard(1); err != nil {
				d.metrics.SyncErrors++
				return nil, err
			}
			d.metrics.SyncErrors++
			continue
		}

		// Sync found, consume it
		if _, err := d.r.Discard(SyncSize); err != nil {
			return nil, err
		}

		// 2. Read Header
		// Header: SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1)
		headerBuf, err := d.r.Peek(HeaderSize)
		if err != nil {
			// If EOF or error, return it
			return nil, err
		}

		// Parse Header
		seq := binary.LittleEndian.Uint16(headerBuf[seqOffset : seqOffset+2])
		payloadLen := binary.LittleEndian.Uint16(headerBuf[lenOffset : lenOffset+2])
		frameType := Type(headerBuf[typeOffset])
		flags := Flags(headerBuf[flagsOffset])

		// 3. Validate Header
		if payloadLen > MaxPayloadSize {
			d.metrics.LengthErrors++
			// Discard the peeked (but not yet consumed) header to prevent false resync.
			// Note: SYNC was already consumed above, so we only discard HeaderSize.
			if _, err := d.r.Discard(HeaderSize); err != nil {
				return nil, err
			}
			continue
		}

		// 4. Read Full Frame (Header + Payload + CRC)
		// We already consumed SYNC. We peeked Header.
		// We need to read Header + Payload + CRC.
		// Construct full buffer for CRC check: SYNC + Header + Payload.
		totalRemaining := HeaderSize + int(payloadLen) + CRCSize

		frameBuf := make([]byte, SyncSize+totalRemaining)
		binary.LittleEndian.PutUint16(frameBuf[0:], SyncWord) // Reconstruct SYNC

		// Read the rest
		_, err = io.ReadFull(d.r, frameBuf[SyncSize:])
		if err != nil {
			return nil, err
		}

		// 5. Validate CRC
		// CRC covers SYNC + Header + Payload
		crcData := frameBuf[:len(frameBuf)-CRCSize]
		receivedCRC := binary.LittleEndian.Uint32(frameBuf[len(frameBuf)-CRCSize:])

		if !verifyCRC32(crcData, receivedCRC) {
			d.metrics.CRCErrors++
			return nil, &CRCError{Sequence: seq}
		}

		// 6. Success
		d.metrics.TotalFrames++
		d.metrics.LastSuccess = time.Now()

		payload := make([]byte, payloadLen)
		copy(payload, frameBuf[SyncSize+HeaderSize:SyncSize+HeaderSize+int(payloadLen)])

		return &Frame{
			Header: Header{
				Sequence: seq,
				Length:   payloadLen,
				Flags:    flags,
			},
			Type:    frameType,
			Payload: payload,
			CRC:     receivedCRC,
		}, nil
	}
}

// GetMetrics returns the current health metrics.
func (d *StreamDecoder) GetMetrics() DecodeMetrics {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.metrics
}
