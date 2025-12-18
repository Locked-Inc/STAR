// Package frame tests for wire protocol frame structures.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"testing"
)

// ============================================================================
// Constant Tests
// ============================================================================

func TestFrameConstants(t *testing.T) {
	tests := []struct {
		name     string
		got      int
		expected int
	}{
		{"SyncWord", int(SyncWord), 0x55AA},
		{"SyncSize", SyncSize, 2},
		{"SeqSize", SeqSize, 2},
		{"LenSize", LenSize, 2},
		{"TypeSize", TypeSize, 1},
		{"FlagsSize", FlagsSize, 1},
		{"HeaderSize", HeaderSize, 6},
		{"CRCSize", CRCSize, 4},
		{"MaxPayloadSize", MaxPayloadSize, 1024},
		{"MaxFrameSize", MaxFrameSize, 1036},
		{"MinFrameSize", MinFrameSize, 12},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.expected {
				t.Errorf("%s = %d, want %d", tc.name, tc.got, tc.expected)
			}
		})
	}
}

func TestFrameTypeString(t *testing.T) {
	tests := []struct {
		frameType FrameType
		expected  string
	}{
		{FrameTypeUnknown, "UNKNOWN"},
		{FrameTypeCommand, "COMMAND"},
		{FrameTypeResponse, "RESPONSE"},
		{FrameTypeAck, "ACK"},
		{FrameTypeNack, "NACK"},
	}

	for _, tc := range tests {
		t.Run(tc.expected, func(t *testing.T) {
			if got := tc.frameType.String(); got != tc.expected {
				t.Errorf("FrameType.String() = %s, want %s", got, tc.expected)
			}
		})
	}
}

func TestFrameFlagValues(t *testing.T) {
	tests := []struct {
		name     string
		flag     FrameFlags
		expected uint8
	}{
		{"FlagNone", FlagNone, 0x00},
		{"FlagRequiresAck", FlagRequiresAck, 0x01},
		{"FlagRetransmit", FlagRetransmit, 0x02},
		{"FlagPriority", FlagPriority, 0x04},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if uint8(tc.flag) != tc.expected {
				t.Errorf("%s = 0x%02X, want 0x%02X", tc.name, tc.flag, tc.expected)
			}
		})
	}
}

// ============================================================================
// Frame Creation Tests
// ============================================================================

func TestNewFrame(t *testing.T) {
	tests := []struct {
		name        string
		frameType   FrameType
		payload     []byte
		expectError bool
	}{
		{"empty_payload", FrameTypeCommand, []byte{}, false},
		{"small_payload", FrameTypeCommand, make([]byte, 64), false},
		{"max_payload", FrameTypeResponse, make([]byte, MaxPayloadSize), false},
		{"oversized_payload", FrameTypeCommand, make([]byte, MaxPayloadSize+1), true},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			frame, err := NewFrame(tc.frameType, tc.payload)

			if tc.expectError {
				if err == nil {
					t.Error("expected error, got nil")
				}
				if err != ErrPayloadTooLarge {
					t.Errorf("expected ErrPayloadTooLarge, got %v", err)
				}
			} else {
				if err != nil {
					t.Errorf("unexpected error: %v", err)
				}
				if frame == nil {
					t.Fatal("expected frame, got nil")
				}
				if frame.Header.Type != tc.frameType {
					t.Errorf("Header.Type = %v, want %v", frame.Header.Type, tc.frameType)
				}
				if int(frame.Header.Length) != len(tc.payload) {
					t.Errorf("Header.Length = %d, want %d", frame.Header.Length, len(tc.payload))
				}
			}
		})
	}
}

// ============================================================================
// Encoder/Decoder Placeholder Tests
// ============================================================================

func TestEncoderNotImplemented(t *testing.T) {
	encoder := NewEncoder()
	frame, _ := NewFrame(FrameTypeCommand, []byte{0x01, 0x02, 0x03})

	_, err := encoder.Encode(frame)
	if err != ErrNotImplemented {
		t.Errorf("Encode() error = %v, want ErrNotImplemented", err)
	}
}

func TestDecoderNotImplemented(t *testing.T) {
	decoder := NewDecoder()

	_, err := decoder.Decode([]byte{0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00})
	if err != ErrNotImplemented {
		t.Errorf("Decode() error = %v, want ErrNotImplemented", err)
	}
}

// ============================================================================
// TODO: Encoder/Decoder Implementation Tests
// ============================================================================

// TODO: Add round-trip encode/decode tests once implementation is complete.
// func TestEncodeDecodeRoundTrip(t *testing.T) {}

// TODO: Add CRC validation tests once implementation is complete.
// func TestCRCValidation(t *testing.T) {}

// TODO: Add malformed frame detection tests once implementation is complete.
// func TestMalformedFrameDetection(t *testing.T) {}
