// Package frame provides frame decoding for the wire protocol.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"encoding/binary"
	"hash/crc32"
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
// Wire format (all multi-byte fields in network byte order / big-endian):
//
//	[SYNC (2B, BE)][SEQ (2B, BE)][LEN (2B, BE)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B, LE)]
//
// CRC-32 is validated over SYNC + Header + Payload (IEEE 802.3 polynomial).
// CRC-32 is read in little-endian format to match IEEE 802.3 LSB-first transmission order.
func (d *DefaultDecoder) Decode(data []byte) (*Frame, error) {
	// Verify minimum frame length
	if len(data) < MinFrameSize {
		return nil, ErrFrameTooShort
	}

	offset := 0

	// Parse and verify SYNC word (big-endian)
	sync := binary.BigEndian.Uint16(data[offset:])
	if sync != SyncWord {
		return nil, ErrInvalidSync
	}
	offset += SyncSize

	// Parse SEQ (big-endian, network byte order per RFC 1700)
	seq := binary.BigEndian.Uint16(data[offset:])
	offset += SeqSize

	// Parse LEN (big-endian, network byte order per RFC 1700)
	payloadLen := binary.BigEndian.Uint16(data[offset:])
	offset += LenSize

	// Validate payload length
	if payloadLen > MaxPayloadSize {
		return nil, ErrPayloadTooLarge
	}

	// Parse TYPE (1 byte)
	frameType := FrameType(data[offset])
	offset += TypeSize

	// Parse FLAGS (1 byte)
	flags := FrameFlags(data[offset])
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

	// Parse CRC-32 (little-endian to match IEEE 802.3 LSB-first transmission order)
	receivedCRC := binary.LittleEndian.Uint32(data[offset:])

	// Calculate expected CRC-32 over SYNC + Header + Payload
	crcData := data[:offset]
	calculatedCRC := crc32.ChecksumIEEE(crcData)

	// Validate CRC
	if receivedCRC != calculatedCRC {
		return nil, ErrInvalidCRC
	}

	// Build and return frame
	return &Frame{
		Header: Header{
			Sequence: seq,
			Length:   payloadLen,
			Type:     frameType,
			Flags:    flags,
		},
		Payload: payload,
		CRC:     receivedCRC,
	}, nil
}
