// Package frame provides frame encoding for the wire protocol.
//
// STAR Project - Texas A&M University
// December 2025
package frame

// Encoder defines the interface for frame encoding.
type Encoder interface {
	// Encode serializes a Frame into wire format bytes.
	// Returns the encoded bytes including SYNC, header, payload, and CRC.
	Encode(frame *Frame) ([]byte, error)
}

// DefaultEncoder is a placeholder implementation of the Encoder interface.
// TODO: Implement actual encoding logic in a future PR.
type DefaultEncoder struct{}

// NewEncoder creates a new DefaultEncoder.
func NewEncoder() *DefaultEncoder {
	return &DefaultEncoder{}
}

// Encode serializes a Frame into wire format bytes.
// TODO: Implement encoding logic:
//  1. Write SYNC word (0x55AA)
//  2. Write sequence number (uint16, little-endian)
//  3. Write payload length (uint16, little-endian)
//  4. Write frame type (uint8)
//  5. Write flags (uint8)
//  6. Write payload bytes
//  7. Calculate and write CRC-32 (IEEE 802.3 polynomial)
func (e *DefaultEncoder) Encode(frame *Frame) ([]byte, error) {
	return nil, ErrNotImplemented
}
