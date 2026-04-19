// Package frame defines the wire protocol frame structure for RPi5 <-> RX72N communication.
//
// Frame format:
//
//	[SYNC (2B, LE)][SEQ (2B, LE)][LEN (2B, LE)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B)]
//
// All multi-byte header fields (SYNC, SEQ, LEN) are little-endian by project convention.
// The CRC-32 uses the IEEE 802.3 polynomial and bit-ordering; the resulting 32-bit
// CRC value is serialized in little-endian to match the header field convention.
//
// STAR Project - Texas A&M University
// January 2026
package frame

import (
	"errors"
	"fmt"
)

// Frame structure constants.
const (
	// SyncWord is the frame synchronization marker (0x55AA).
	SyncWord uint16 = 0x55AA

	// SyncSize is the size of the sync word in bytes.
	SyncSize = 2

	// Header field sizes
	SeqSize   = 2
	LenSize   = 2
	TypeSize  = 1
	FlagsSize = 1

	// HeaderSize is the total header size (SEQ + LEN + TYPE + FLAGS).
	HeaderSize = SeqSize + LenSize + TypeSize + FlagsSize // 6 bytes

	// CRCSize is the size of the CRC-32 checksum in bytes.
	CRCSize = 4

	// MaxPayloadSize is the maximum payload size in bytes.
	MaxPayloadSize = 1024

	// MaxFrameSize is the maximum total frame size.
	MaxFrameSize = SyncSize + HeaderSize + MaxPayloadSize + CRCSize

	// MinFrameSize is the minimum frame size (empty payload).
	MinFrameSize = SyncSize + HeaderSize + CRCSize

	// EmptyPayloadLength is the length value for frames with no payload (ACK/NACK/PING).
	EmptyPayloadLength = 0
)

// Type indicates the type of frame being transmitted.
// Not serialized in Header struct, but part of the wire header.
type Type uint8

const (
	// Heartbeat and session reset frame types (Critical Fixes #4)
	FrameTypePing     Type = 0x00 // Heartbeat request
	FrameTypePong     Type = 0x01 // Heartbeat response
	FrameTypeResetAck Type = 0xFE // Session reset acknowledgment
	FrameTypeReset    Type = 0xFF // Session reset (synchronize sequences)

	// Standard frame types
	FrameTypeCommand  Type = 0x10 // Command frame
	FrameTypeResponse Type = 0x11 // Response frame
	FrameTypeAck      Type = 0x12 // Acknowledgment
	FrameTypeNack     Type = 0x13 // Negative acknowledgment

	// Firmware runtime log payload (ASCII bytes from rx_log_uart on the RX72N).
	// Multiplexed onto the same UART byte stream as command/response frames so
	// the frame sync word cannot collide with log text.
	FrameTypeLogMessage Type = 0x20

	// Legacy/Unknown
	FrameTypeUnknown Type = 0x14 // Unknown frame type
)

func (ft Type) String() string {
	switch ft {
	case FrameTypePing:
		return "PING"
	case FrameTypePong:
		return "PONG"
	case FrameTypeReset:
		return "RESET"
	case FrameTypeResetAck:
		return "RESET_ACK"
	case FrameTypeCommand:
		return "COMMAND"
	case FrameTypeResponse:
		return "RESPONSE"
	case FrameTypeAck:
		return "ACK"
	case FrameTypeNack:
		return "NACK"
	case FrameTypeLogMessage:
		return "LOG_MESSAGE"
	default:
		return "UNKNOWN"
	}
}

// Flags contains frame-level control flags.
type Flags uint8

const (
	FlagNone        Flags = 0x00
	FlagRequiresAck Flags = 0x01
	FlagRetransmit  Flags = 0x02
	FlagPriority    Flags = 0x04
	FlagFECEnabled  Flags = 0x08
	FlagSoftNACK    Flags = 0x10
)

// Header contains the frame header fields.
type Header struct {
	// Sequence is the frame sequence number.
	Sequence uint16

	// Length is the payload length in bytes.
	Length uint16

	// Flags contains frame control flags.
	Flags Flags
}

// Frame represents a complete communication frame.
type Frame struct {
	// Header contains the frame metadata.
	Header Header

	// Type indicates the frame type (derived from wire header).
	Type Type

	// Payload contains the data.
	Payload []byte

	// CRC is the CRC-32 checksum.
	CRC uint32

	// Retransmits indicates the number of retransmissions (metadata).
	Retransmits int

	// FECDecoded indicates if the frame was recovered using FEC (metadata).
	FECDecoded bool
}

// CRCError is returned by the stream decoder when CRC validation fails.
// It carries the parsed sequence number so callers can send a NACK.
type CRCError struct {
	Sequence uint16
}

func (e *CRCError) Error() string {
	return fmt.Sprintf("CRC validation failed for sequence %d", e.Sequence)
}

// Unwrap returns the underlying sentinel error (ErrInvalidCRC) to support
// Go 1.13+ error unwrapping with errors.Is and errors.As.
func (e *CRCError) Unwrap() error { return ErrInvalidCRC }

// Predefined errors.
var (
	ErrNotImplemented  = errors.New("not implemented")
	ErrInvalidSync     = errors.New("invalid sync word")
	ErrInvalidCRC      = errors.New("CRC validation failed")
	ErrPayloadTooLarge = errors.New("payload exceeds maximum size")
	ErrFrameTooShort   = errors.New("frame data too short")
)

// NewFrame creates a new Frame with the given type and payload.
func NewFrame(frameType Type, payload []byte) (*Frame, error) {
	if len(payload) > MaxPayloadSize {
		return nil, ErrPayloadTooLarge
	}

	return &Frame{
		Header: Header{
			Length: uint16(len(payload)),
			Flags:  FlagNone,
		},
		Type:    frameType,
		Payload: payload,
	}, nil
}

// EmptyPayload is a pre-allocated empty byte slice for frames with no payload (ACK/NACK/PING).
// Using this constant avoids repeated allocations of empty slices.
var EmptyPayload = []byte{}
