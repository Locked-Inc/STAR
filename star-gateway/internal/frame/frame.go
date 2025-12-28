// Package frame defines the wire protocol frame structure for RPi5 <-> ESP32 communication.
//
// Frame format (from docs/sections/01_nanopb_protocol.tex):
//
//	[SYNC (2B)][SEQ (2B)][LEN (2B)][TYPE (1B)][FLAGS (1B)][PAYLOAD (0-1KB)][CRC-32 (4B)]
//
// STAR Project - Texas A&M University
// December 2025
package frame

import "errors"

// Frame structure constants derived from docs/sections/01_nanopb_protocol.tex.
const (
	// SyncWord is the frame synchronization marker (0x55AA).
	SyncWord uint16 = 0x55AA

	// SyncSize is the size of the sync word in bytes.
	SyncSize = 2

	// SeqSize is the size of the sequence number field in bytes.
	SeqSize = 2

	// LenSize is the size of the payload length field in bytes.
	LenSize = 2

	// TypeSize is the size of the frame type field in bytes.
	TypeSize = 1

	// FlagsSize is the size of the flags field in bytes.
	FlagsSize = 1

	// HeaderSize is the total header size (SEQ + LEN + TYPE + FLAGS).
	// Note: SYNC is not included as it's handled separately during framing.
	HeaderSize = SeqSize + LenSize + TypeSize + FlagsSize // 6 bytes

	// CRCSize is the size of the CRC-32 checksum in bytes.
	CRCSize = 4

	// MaxPayloadSize is the maximum payload size in bytes.
	MaxPayloadSize = 1024

	// MaxFrameSize is the maximum total frame size.
	MaxFrameSize = SyncSize + HeaderSize + MaxPayloadSize + CRCSize // 1036 bytes

	// MinFrameSize is the minimum frame size (empty payload).
	MinFrameSize = SyncSize + HeaderSize + CRCSize // 12 bytes
)

// FrameType indicates the type of frame being transmitted.
type FrameType uint8

const (
	// FrameTypeUnknown is the zero value (invalid frame type).
	FrameTypeUnknown FrameType = iota

	// FrameTypeCommand is a command frame from controller to peripheral.
	FrameTypeCommand

	// FrameTypeResponse is a response frame from peripheral to controller.
	FrameTypeResponse

	// FrameTypeAck acknowledges successful receipt of a frame.
	FrameTypeAck

	// FrameTypeNack indicates frame reception failure (CRC error, etc.).
	FrameTypeNack
)

// String returns the string representation of a FrameType.
func (ft FrameType) String() string {
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

// FrameFlags contains frame-level control flags.
type FrameFlags uint8

const (
	// FlagNone indicates no flags set.
	FlagNone FrameFlags = 0x00

	// FlagRequiresAck indicates the frame requires acknowledgment.
	FlagRequiresAck FrameFlags = 0x01

	// FlagRetransmit indicates this frame is a retransmission.
	FlagRetransmit FrameFlags = 0x02

	// FlagPriority indicates high-priority frame (e.g., emergency stop).
	FlagPriority FrameFlags = 0x04

	// FlagFECEnabled indicates the payload is FEC-encoded (HARQ).
	FlagFECEnabled FrameFlags = 0x08

	// FlagSoftNACK indicates NACK with soft bit information (HARQ).
	FlagSoftNACK FrameFlags = 0x10
)

// Header contains the frame header fields.
type Header struct {
	// Sequence is the frame sequence number for ARQ.
	Sequence uint16

	// Length is the payload length in bytes.
	Length uint16

	// Type is the frame type.
	Type FrameType

	// Flags contains frame control flags.
	Flags FrameFlags
}

// Frame represents a complete communication frame.
type Frame struct {
	// Header contains the frame metadata.
	Header Header

	// Payload contains the protobuf-serialized message data.
	Payload []byte

	// CRC is the IEEE 802.3 CRC-32 checksum.
	CRC uint32
}

// Predefined errors for frame operations.
var (
	// ErrNotImplemented is returned by placeholder implementations.
	ErrNotImplemented = errors.New("not implemented")

	// ErrInvalidSync is returned when sync word doesn't match.
	ErrInvalidSync = errors.New("invalid sync word")

	// ErrInvalidCRC is returned when CRC validation fails.
	ErrInvalidCRC = errors.New("CRC validation failed")

	// ErrPayloadTooLarge is returned when payload exceeds MaxPayloadSize.
	ErrPayloadTooLarge = errors.New("payload exceeds maximum size")

	// ErrFrameTooShort is returned when frame data is too short.
	ErrFrameTooShort = errors.New("frame data too short")
)

// NewFrame creates a new Frame with the given type and payload.
// Returns an error if the payload exceeds MaxPayloadSize.
func NewFrame(frameType FrameType, payload []byte) (*Frame, error) {
	if len(payload) > MaxPayloadSize {
		return nil, ErrPayloadTooLarge
	}

	return &Frame{
		Header: Header{
			Type:   frameType,
			Length: uint16(len(payload)),
			Flags:  FlagNone,
		},
		Payload: payload,
	}, nil
}
