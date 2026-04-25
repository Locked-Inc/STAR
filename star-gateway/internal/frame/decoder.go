// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package frame provides frame decoding for the wire protocol.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"encoding/binary"
)

// Decoder defines the interface for frame decoding.
type Decoder interface {
	// Decode parses wire format bytes into a Frame.
	// Returns an error if the frame is malformed or CRC validation fails.
	Decode(data []byte) (*Frame, error)
}

// DefaultDecoder implements the Decoder interface.
type DefaultDecoder struct{}

// NewDecoder creates a new DefaultDecoder.
func NewDecoder() *DefaultDecoder {
	return &DefaultDecoder{}
}

// Decode parses wire format bytes into a Frame.
//
// Wire format (all fields in little-endian per IEEE 802.3):
//
//	[SYNC (2B)][SEQ (2B)][LEN (2B)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B)]
//
// CRC-32 is validated over SYNC + Header + Payload.
func (d *DefaultDecoder) Decode(data []byte) (*Frame, error) {
	// Verify minimum frame length
	if len(data) < MinFrameSize {
		return nil, ErrFrameTooShort
	}

	offset := 0

	// Parse and verify SYNC word (little-endian)
	sync := binary.LittleEndian.Uint16(data[offset:])
	if sync != SyncWord {
		return nil, ErrInvalidSync
	}
	offset += SyncSize

	// Parse SEQ (little-endian)
	seq := binary.LittleEndian.Uint16(data[offset:])
	offset += SeqSize

	// Parse LEN (little-endian)
	payloadLen := binary.LittleEndian.Uint16(data[offset:])
	offset += LenSize

	// Validate payload length
	if payloadLen > MaxPayloadSize {
		return nil, ErrPayloadTooLarge
	}

	// Parse TYPE (1 byte)
	frameType := Type(data[offset])
	offset += TypeSize

	// Parse FLAGS (1 byte)
	flags := Flags(data[offset])
	offset += FlagsSize

	// Verify frame completeness
	expectedLen := SyncSize + HeaderSize + int(payloadLen) + CRCSize
	if len(data) < expectedLen {
		return nil, ErrFrameTooShort
	}

	// Extract payload
	payload := make([]byte, payloadLen)
	if payloadLen > 0 {
		copy(payload, data[offset:offset+int(payloadLen)])
	}
	offset += int(payloadLen)

	// Parse CRC-32 (little-endian, IEEE 802.3 LSB-first order)
	receivedCRC := binary.LittleEndian.Uint32(data[offset:])

	// Calculate expected CRC-32 over SYNC + Header + Payload
	crcData := data[:offset]
	if !verifyCRC32(crcData, receivedCRC) {
		return nil, ErrInvalidCRC
	}

	// Build and return frame
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
