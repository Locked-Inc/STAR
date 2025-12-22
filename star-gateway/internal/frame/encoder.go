// Package frame provides frame encoding for the wire protocol.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"encoding/binary"
	"hash/crc32"
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
// Wire format (all multi-byte fields in network byte order / big-endian):
//
//	[SYNC (2B, BE)][SEQ (2B, BE)][LEN (2B, BE)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B, LE)]
//
// CRC-32 is calculated over SYNC + Header + Payload (IEEE 802.3 polynomial).
// CRC-32 is written in little-endian format to match IEEE 802.3 LSB-first transmission order.
func (e *DefaultEncoder) Encode(frame *Frame) ([]byte, error) {
	// Validate payload size
	if len(frame.Payload) > MaxPayloadSize {
		return nil, ErrPayloadTooLarge
	}

	// Calculate total frame size
	frameSize := SyncSize + HeaderSize + len(frame.Payload) + CRCSize
	buf := make([]byte, frameSize)
	offset := 0

	// Write SYNC word (big-endian)
	binary.BigEndian.PutUint16(buf[offset:], SyncWord)
	offset += SyncSize

	// Write SEQ (big-endian, network byte order per RFC 1700)
	binary.BigEndian.PutUint16(buf[offset:], frame.Header.Sequence)
	offset += SeqSize

	// Write LEN (big-endian, network byte order per RFC 1700)
	binary.BigEndian.PutUint16(buf[offset:], frame.Header.Length)
	offset += LenSize

	// Write TYPE (1 byte)
	buf[offset] = byte(frame.Header.Type)
	offset += TypeSize

	// Write FLAGS (1 byte)
	buf[offset] = byte(frame.Header.Flags)
	offset += FlagsSize

	// Write PAYLOAD
	if len(frame.Payload) > 0 {
		copy(buf[offset:], frame.Payload)
		offset += len(frame.Payload)
	}

	// Calculate CRC-32 over SYNC + Header + Payload (IEEE 802.3 polynomial)
	crcData := buf[:offset]
	crc := crc32.ChecksumIEEE(crcData)

	// Write CRC-32 (little-endian to match IEEE 802.3 LSB-first transmission order)
	binary.LittleEndian.PutUint32(buf[offset:], crc)

	// Store CRC in frame for reference
	frame.CRC = crc

	return buf, nil
}
