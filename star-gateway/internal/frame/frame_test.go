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

			// Verify CRC-32 (big-endian)
			crcStart := len(encoded) - CRCSize
			receivedCRC := binary.BigEndian.Uint32(encoded[crcStart:])
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

			// Verify CRC is big-endian (all 4 bytes)
			crcStart := len(encoded) - CRCSize
			expectedCRC := computeCRC32(encoded[:crcStart])
			for i := range CRCSize {
				expectedByte := byte((expectedCRC >> (24 - 8*i)) & 0xFF)
				if encoded[crcStart+i] != expectedByte {
					t.Errorf("CRC byte %d at offset %d: got 0x%02X, want 0x%02X",
						i, crcStart+i, encoded[crcStart+i], expectedByte)
				}
			}
		})
	}
}
