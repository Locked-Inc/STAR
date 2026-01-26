// Package frame defines the wire protocol frame structure for RPi5 <-> ESP32 communication.
//
// Frame format:
//
//	[SYNC (2B)][SEQ (2B)][LEN (2B)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B)]
//
// STAR Project - Texas A&M University
// January 2026
package frame

import "errors"

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
)

// Type indicates the type of frame being transmitted.
// Not serialized in Header struct, but part of the wire header.
type Type uint8

const (
	FrameTypeUnknown  Type = 0
	FrameTypeCommand  Type = 1
	FrameTypeResponse Type = 2
	FrameTypeAck      Type = 3
	FrameTypeNack     Type = 4
)

func (ft Type) String() string {
	switch ft {
	case FrameTypeCommand:
		return "COMMAND"
	case FrameTypeResponse:
		return "RESPONSE"
	case FrameTypeAck:
		return "ACK"
	case FrameTypeNack:
		return "NACK"
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
