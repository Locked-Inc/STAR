// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package frame provides frame encoding for the wire protocol.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"encoding/binary"
)

// Encoder defines the interface for frame encoding.
type Encoder interface {
	// Encode serializes a Frame into wire format bytes.
	// Returns the encoded bytes including SYNC, header, payload, and CRC.
	Encode(frame *Frame) ([]byte, error)
}

// DefaultEncoder implements the Encoder interface.
type DefaultEncoder struct{}

// NewEncoder creates a new DefaultEncoder.
func NewEncoder() *DefaultEncoder {
	return &DefaultEncoder{}
}

// Encode serializes a Frame into wire format bytes.
//
// Wire format:
//
//	[SYNC (2B)][SEQ (2B)][LEN (2B)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B)]
//
// Header fields (SYNC, SEQ, LEN, TYPE, FLAGS) are encoded in little-endian
// by project choice for consistency across platforms.
//
// CRC-32 uses the IEEE 802.3 polynomial (0xEDB88320) with bit/byte ordering
// as defined by the standard. The CRC is calculated over SYNC + Header + Payload.
func (e *DefaultEncoder) Encode(frame *Frame) ([]byte, error) {
	// Validate payload size
	if len(frame.Payload) > MaxPayloadSize {
		return nil, ErrPayloadTooLarge
	}

	// Calculate total frame size
	frameSize := SyncSize + HeaderSize + len(frame.Payload) + CRCSize
	buf := make([]byte, frameSize)
	offset := 0

	// Write SYNC word (little-endian)
	binary.LittleEndian.PutUint16(buf[offset:], SyncWord)
	offset += SyncSize

	// Write SEQ (little-endian)
	binary.LittleEndian.PutUint16(buf[offset:], frame.Header.Sequence)
	offset += SeqSize

	// Write LEN (little-endian)
	binary.LittleEndian.PutUint16(buf[offset:], frame.Header.Length)
	offset += LenSize

	// Write TYPE (1 byte) - Explicit field
	buf[offset] = byte(frame.Type)
	offset += TypeSize

	// Write FLAGS (1 byte)
	buf[offset] = byte(frame.Header.Flags)
	offset += FlagsSize

	// Write PAYLOAD
	if len(frame.Payload) > 0 {
		copy(buf[offset:], frame.Payload)
		offset += len(frame.Payload)
	}

	// Calculate CRC-32 over SYNC + Header + Payload
	crcData := buf[:offset]
	crc := computeCRC32(crcData)

	// Write CRC-32 (little-endian, IEEE 802.3 LSB-first order)
	binary.LittleEndian.PutUint32(buf[offset:], crc)

	// Store CRC in frame for reference
	frame.CRC = crc

	return buf, nil
}
