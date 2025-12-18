// Package frame provides frame decoding for the wire protocol.
//
// STAR Project - Texas A&M University
// December 2025
package frame

// Decoder defines the interface for frame decoding.
type Decoder interface {
	// Decode parses wire format bytes into a Frame.
	// Returns an error if the frame is malformed or CRC validation fails.
	Decode(data []byte) (*Frame, error)
}

// DefaultDecoder is a placeholder implementation of the Decoder interface.
// TODO: Implement actual decoding logic in a future PR.
type DefaultDecoder struct{}

// NewDecoder creates a new DefaultDecoder.
func NewDecoder() *DefaultDecoder {
	return &DefaultDecoder{}
}

// Decode parses wire format bytes into a Frame.
// TODO: Implement decoding logic:
//  1. Verify minimum frame length
//  2. Verify SYNC word (0x55AA)
//  3. Parse sequence number (uint16, little-endian)
//  4. Parse payload length (uint16, little-endian)
//  5. Parse frame type (uint8)
//  6. Parse flags (uint8)
//  7. Extract payload bytes
//  8. Parse CRC-32 from end of frame
//  9. Validate CRC-32 against header + payload
func (d *DefaultDecoder) Decode(data []byte) (*Frame, error) {
	return nil, ErrNotImplemented
}
