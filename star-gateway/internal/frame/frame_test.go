// Package frame tests for wire protocol frame structures.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"encoding/binary"
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
		{"MaxFrameSize", MaxFrameSize, 2 + 6 + 1024 + 4},
		{"MinFrameSize", MinFrameSize, 2 + 6 + 4},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.expected {
				t.Errorf("%s = %d, want %d", tc.name, tc.got, tc.expected)
			}
		})
	}
}

// ============================================================================
// Encoder Tests
// ============================================================================

func TestEncoderEncode(t *testing.T) {
	tests := []struct {
		name      string
		frameType Type
		seq       uint16
		flags     Flags
		payload   []byte
	}{
		{"empty_payload", FrameTypeCommand, 0, FlagNone, []byte{}},
		{"small_payload", FrameTypeCommand, 1, FlagRequiresAck, []byte{0x01, 0x02, 0x03}},
		{"priority_frame", FrameTypeCommand, 1000, FlagPriority, []byte{0xDE, 0xAD, 0xBE, 0xEF}},
	}

	encoder := NewEncoder()

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			frame, err := NewFrame(tc.frameType, tc.payload)
			if err != nil {
				t.Fatalf("failed to create frame: %v", err)
			}
			frame.Header.Sequence = tc.seq
			frame.Header.Flags = tc.flags

			encoded, err := encoder.Encode(frame)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}

			// Verify frame size
			expectedSize := SyncSize + HeaderSize + len(tc.payload) + CRCSize
			if len(encoded) != expectedSize {
				t.Errorf("encoded size = %d, want %d", len(encoded), expectedSize)
			}

			// Verify SYNC word (big-endian)
			sync := binary.BigEndian.Uint16(encoded[0:2])
			if sync != SyncWord {
				t.Errorf("SYNC = 0x%04X, want 0x%04X", sync, SyncWord)
			}

			// Verify CRC-32 (little-endian, IEEE 802.3)
			crcStart := len(encoded) - CRCSize
			receivedCRC := binary.LittleEndian.Uint32(encoded[crcStart:])
			expectedCRC := computeCRC32(encoded[:crcStart])
			if receivedCRC != expectedCRC {
				t.Errorf("CRC = 0x%08X, want 0x%08X", receivedCRC, expectedCRC)
			}
		})
	}
}

// byteCheck defines a single byte verification for byte order tests.
type byteCheck struct {
	offset   int
	expected byte
	desc     string
}

func TestEncoderByteOrder(t *testing.T) {
	tests := []struct {
		name    string
		seq     uint16
		payload []byte
		checks  []byteCheck
	}{
		{
			name:    "verify_all_header_fields",
			seq:     0x1234,
			payload: []byte{0x01},
			checks: []byteCheck{
				{offset: 0, expected: 0x55, desc: "SYNC high"},
				{offset: 1, expected: 0xAA, desc: "SYNC low"},
				{offset: 2, expected: 0x12, desc: "SEQ high"},
				{offset: 3, expected: 0x34, desc: "SEQ low"},
				{offset: 4, expected: 0x00, desc: "LEN high"},
				{offset: 5, expected: 0x01, desc: "LEN low"},
				{offset: 6, expected: 0x10, desc: "TYPE (Command)"},
				{offset: 7, expected: 0x00, desc: "FLAGS"},
			},
		},
	}

	encoder := NewEncoder()

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			frame, err := NewFrame(FrameTypeCommand, tc.payload)
			if err != nil {
				t.Fatalf("NewFrame() error = %v", err)
			}
			frame.Header.Sequence = tc.seq

			encoded, err := encoder.Encode(frame)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}

			// Verify byte positions
			for _, check := range tc.checks {
				if encoded[check.offset] != check.expected {
					t.Errorf("%s: got 0x%02X, want 0x%02X", check.desc, encoded[check.offset], check.expected)
				}
			}

			// Verify CRC is little-endian (IEEE 802.3 LSB-first, all 4 bytes)
			crcStart := len(encoded) - CRCSize
			expectedCRC := computeCRC32(encoded[:crcStart])
			for i := range CRCSize {
				expectedByte := byte((expectedCRC >> (8 * i)) & 0xFF)
				if encoded[crcStart+i] != expectedByte {
					t.Errorf("CRC byte %d at offset %d: got 0x%02X, want 0x%02X",
						i, crcStart+i, encoded[crcStart+i], expectedByte)
				}
			}
		})
	}
}

// ============================================================================
// Cross-Compatibility Test Vectors (Go ↔ C byte-exact)
//
// These vectors define the canonical wire encoding for every frame type.
// The C firmware tests (test_rx_frame.c) verify against the same hardcoded
// byte sequences. If either encoder changes, these tests catch the drift.
// ============================================================================

// crossVector defines a byte-exact wire encoding test case.
type crossVector struct {
	name      string
	frameType Type
	seq       uint16
	flags     Flags
	payload   []byte
	wire      []byte // Expected complete wire bytes (SYNC + header + payload + CRC-32 LE)
}

// crossCompatibilityVectors returns the canonical test vectors shared with C firmware.
// Any change to these vectors MUST be mirrored in test_rx_frame.c.
func crossCompatibilityVectors() []crossVector {
	return []crossVector{
		{
			name:      "PING_seq0_empty",
			frameType: FrameTypePing,
			seq:       0,
			flags:     FlagNone,
			payload:   nil,
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x00, // SEQ=0
				0x00, 0x00, // LEN=0
				0x00,                   // TYPE=PING
				0x00,                   // FLAGS=none
				0xEB, 0xAE, 0x3F, 0x9B, // CRC-32 LE = 0x9B3FAEEB
			},
		},
		{
			name:      "PONG_seq0_counter42",
			frameType: FrameTypePong,
			seq:       0,
			flags:     FlagNone,
			payload:   []byte{0x00, 0x00, 0x00, 0x2A}, // counter=42 big-endian
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x00, // SEQ=0
				0x00, 0x04, // LEN=4
				0x01,                   // TYPE=PONG
				0x00,                   // FLAGS=none
				0x00, 0x00, 0x00, 0x2A, // PAYLOAD
				0xDF, 0x37, 0x77, 0x28, // CRC-32 LE = 0x287737DF
			},
		},
		{
			name:      "COMMAND_seq1_TEST",
			frameType: FrameTypeCommand,
			seq:       1,
			flags:     FlagRequiresAck,
			payload:   []byte{0x54, 0x45, 0x53, 0x54}, // "TEST"
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x01, // SEQ=1
				0x00, 0x04, // LEN=4
				0x10,                   // TYPE=COMMAND
				0x01,                   // FLAGS=REQUIRES_ACK
				0x54, 0x45, 0x53, 0x54, // PAYLOAD="TEST"
				0x60, 0x5E, 0xF3, 0xDE, // CRC-32 LE = 0xDEF35E60
			},
		},
		{
			name:      "RESPONSE_seq1_OK",
			frameType: FrameTypeResponse,
			seq:       1,
			flags:     FlagNone,
			payload:   []byte{0x4F, 0x4B}, // "OK"
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x01, // SEQ=1
				0x00, 0x02, // LEN=2
				0x11,       // TYPE=RESPONSE
				0x00,       // FLAGS=none
				0x4F, 0x4B, // PAYLOAD="OK"
				0xF4, 0x8E, 0xC0, 0x6F, // CRC-32 LE = 0x6FC08EF4
			},
		},
		{
			name:      "ACK_seq1_empty",
			frameType: FrameTypeAck,
			seq:       1,
			flags:     FlagNone,
			payload:   nil,
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x01, // SEQ=1
				0x00, 0x00, // LEN=0
				0x12,                   // TYPE=ACK
				0x00,                   // FLAGS=none
				0x88, 0xF7, 0xAB, 0xDE, // CRC-32 LE = 0xDEABF788
			},
		},
		{
			name:      "NACK_seq1_empty",
			frameType: FrameTypeNack,
			seq:       1,
			flags:     FlagNone,
			payload:   nil,
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x01, // SEQ=1
				0x00, 0x00, // LEN=0
				0x13,                   // TYPE=NACK
				0x00,                   // FLAGS=none
				0xC9, 0xC6, 0xB0, 0xC7, // CRC-32 LE = 0xC7B0C6C9
			},
		},
		{
			name:      "RESET_seq0_empty",
			frameType: FrameTypeReset,
			seq:       0,
			flags:     FlagNone,
			payload:   nil,
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x00, // SEQ=0
				0x00, 0x00, // LEN=0
				0xFF,                   // TYPE=RESET
				0x00,                   // FLAGS=none
				0x99, 0x53, 0x1B, 0x08, // CRC-32 LE = 0x081B5399
			},
		},
		{
			name:      "RESET_ACK_seq0_empty",
			frameType: FrameTypeResetAck,
			seq:       0,
			flags:     FlagNone,
			payload:   nil,
			wire: []byte{
				0x55, 0xAA, // SYNC
				0x00, 0x00, // SEQ=0
				0x00, 0x00, // LEN=0
				0xFE,                   // TYPE=RESET_ACK
				0x00,                   // FLAGS=none
				0xD8, 0x62, 0x00, 0x11, // CRC-32 LE = 0x110062D8
			},
		},
	}
}

func TestCrossCompatibility_Encode(t *testing.T) {
	encoder := NewEncoder()

	for _, v := range crossCompatibilityVectors() {
		t.Run(v.name, func(t *testing.T) {
			payload := v.payload
			if payload == nil {
				payload = EmptyPayload
			}
			frame, err := NewFrame(v.frameType, payload)
			if err != nil {
				t.Fatalf("NewFrame() error = %v", err)
			}
			frame.Header.Sequence = v.seq
			frame.Header.Flags = v.flags

			encoded, err := encoder.Encode(frame)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}

			if len(encoded) != len(v.wire) {
				t.Fatalf("encoded length = %d, want %d", len(encoded), len(v.wire))
			}

			for i := range encoded {
				if encoded[i] != v.wire[i] {
					t.Errorf("byte[%d] = 0x%02X, want 0x%02X", i, encoded[i], v.wire[i])
				}
			}
		})
	}
}

func TestCrossCompatibility_Decode(t *testing.T) {
	decoder := NewDecoder()

	for _, v := range crossCompatibilityVectors() {
		t.Run(v.name, func(t *testing.T) {
			frame, err := decoder.Decode(v.wire)
			if err != nil {
				t.Fatalf("Decode() error = %v", err)
			}

			if frame.Header.Sequence != v.seq {
				t.Errorf("Sequence = %d, want %d", frame.Header.Sequence, v.seq)
			}
			if frame.Type != v.frameType {
				t.Errorf("Type = 0x%02X, want 0x%02X", frame.Type, v.frameType)
			}
			if frame.Header.Flags != v.flags {
				t.Errorf("Flags = 0x%02X, want 0x%02X", frame.Header.Flags, v.flags)
			}

			expectedPayload := v.payload
			if expectedPayload == nil {
				expectedPayload = []byte{}
			}
			if len(frame.Payload) != len(expectedPayload) {
				t.Fatalf("Payload length = %d, want %d", len(frame.Payload), len(expectedPayload))
			}
			for i := range expectedPayload {
				if frame.Payload[i] != expectedPayload[i] {
					t.Errorf("Payload[%d] = 0x%02X, want 0x%02X", i, frame.Payload[i], expectedPayload[i])
				}
			}
		})
	}
}

func TestCrossCompatibility_RoundTrip(t *testing.T) {
	encoder := NewEncoder()
	decoder := NewDecoder()

	for _, v := range crossCompatibilityVectors() {
		t.Run(v.name, func(t *testing.T) {
			// Decode the hardcoded wire bytes
			decoded, err := decoder.Decode(v.wire)
			if err != nil {
				t.Fatalf("Decode() error = %v", err)
			}

			// Re-encode and verify identical wire bytes
			reencoded, err := encoder.Encode(decoded)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}

			if len(reencoded) != len(v.wire) {
				t.Fatalf("re-encoded length = %d, want %d", len(reencoded), len(v.wire))
			}
			for i := range reencoded {
				if reencoded[i] != v.wire[i] {
					t.Errorf("re-encoded byte[%d] = 0x%02X, want 0x%02X", i, reencoded[i], v.wire[i])
				}
			}
		})
	}
}
