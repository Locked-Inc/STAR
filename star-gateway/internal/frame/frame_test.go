// Package frame tests for wire protocol frame structures.
//
// STAR Project - Texas A&M University
// December 2025
package frame

import (
	"bytes"
	"encoding/binary"
	"hash/crc32"
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
// Encoder Tests
// ============================================================================

func TestEncoderEncode(t *testing.T) {
	tests := []struct {
		name      string
		frameType FrameType
		seq       uint16
		flags     FrameFlags
		payload   []byte
	}{
		{"empty_payload", FrameTypeCommand, 0, FlagNone, []byte{}},
		{"small_payload", FrameTypeCommand, 1, FlagRequiresAck, []byte{0x01, 0x02, 0x03}},
		{"response_frame", FrameTypeResponse, 42, FlagNone, []byte("Hello, ESP32!")},
		{"ack_frame", FrameTypeAck, 100, FlagNone, []byte{}},
		{"nack_frame", FrameTypeNack, 255, FlagNone, []byte{}},
		{"priority_frame", FrameTypeCommand, 1000, FlagPriority, []byte{0xDE, 0xAD, 0xBE, 0xEF}},
		{"max_seq", FrameTypeCommand, 0xFFFF, FlagNone, []byte{0x01}},
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

			// Verify SEQ (big-endian)
			seq := binary.BigEndian.Uint16(encoded[2:4])
			if seq != tc.seq {
				t.Errorf("SEQ = %d, want %d", seq, tc.seq)
			}

			// Verify LEN (big-endian)
			length := binary.BigEndian.Uint16(encoded[4:6])
			if length != uint16(len(tc.payload)) {
				t.Errorf("LEN = %d, want %d", length, len(tc.payload))
			}

			// Verify TYPE
			if FrameType(encoded[6]) != tc.frameType {
				t.Errorf("TYPE = %d, want %d", encoded[6], tc.frameType)
			}

			// Verify FLAGS
			if FrameFlags(encoded[7]) != tc.flags {
				t.Errorf("FLAGS = 0x%02X, want 0x%02X", encoded[7], tc.flags)
			}

			// Verify payload
			payloadStart := SyncSize + HeaderSize
			payloadEnd := payloadStart + len(tc.payload)
			if len(tc.payload) > 0 && !bytes.Equal(encoded[payloadStart:payloadEnd], tc.payload) {
				t.Errorf("PAYLOAD mismatch")
			}

			// Verify CRC-32 (little-endian)
			crcStart := len(encoded) - CRCSize
			receivedCRC := binary.LittleEndian.Uint32(encoded[crcStart:])
			expectedCRC := crc32.ChecksumIEEE(encoded[:crcStart])
			if receivedCRC != expectedCRC {
				t.Errorf("CRC = 0x%08X, want 0x%08X", receivedCRC, expectedCRC)
			}
		})
	}
}

func TestEncoderErrors(t *testing.T) {
	tests := []struct {
		name        string
		frame       *Frame
		expectedErr error
	}{
		{
			name: "payload_too_large",
			frame: &Frame{
				Header:  Header{Type: FrameTypeCommand, Length: MaxPayloadSize + 1},
				Payload: make([]byte, MaxPayloadSize+1),
			},
			expectedErr: ErrPayloadTooLarge,
		},
	}

	encoder := NewEncoder()

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			_, err := encoder.Encode(tc.frame)
			if err != tc.expectedErr {
				t.Errorf("Encode() error = %v, want %v", err, tc.expectedErr)
			}
		})
	}
}

// ============================================================================
// Decoder Tests
// ============================================================================

func TestDecoderDecode(t *testing.T) {
	tests := []struct {
		name      string
		frameType FrameType
		seq       uint16
		flags     FrameFlags
		payload   []byte
	}{
		{"empty_payload", FrameTypeCommand, 0, FlagNone, []byte{}},
		{"small_payload", FrameTypeCommand, 1, FlagRequiresAck, []byte{0x01, 0x02, 0x03}},
		{"response_frame", FrameTypeResponse, 42, FlagNone, []byte("Hello, ESP32!")},
		{"ack_frame", FrameTypeAck, 100, FlagNone, []byte{}},
		{"nack_frame", FrameTypeNack, 255, FlagNone, []byte{}},
		{"priority_frame", FrameTypeCommand, 1000, FlagPriority, []byte{0xDE, 0xAD, 0xBE, 0xEF}},
	}

	encoder := NewEncoder()
	decoder := NewDecoder()

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			// Create and encode frame
			original, _ := NewFrame(tc.frameType, tc.payload)
			original.Header.Sequence = tc.seq
			original.Header.Flags = tc.flags

			encoded, err := encoder.Encode(original)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}

			// Decode frame
			decoded, err := decoder.Decode(encoded)
			if err != nil {
				t.Fatalf("Decode() error = %v", err)
			}

			// Verify decoded values
			if decoded.Header.Sequence != tc.seq {
				t.Errorf("Sequence = %d, want %d", decoded.Header.Sequence, tc.seq)
			}
			if decoded.Header.Length != uint16(len(tc.payload)) {
				t.Errorf("Length = %d, want %d", decoded.Header.Length, len(tc.payload))
			}
			if decoded.Header.Type != tc.frameType {
				t.Errorf("Type = %v, want %v", decoded.Header.Type, tc.frameType)
			}
			if decoded.Header.Flags != tc.flags {
				t.Errorf("Flags = 0x%02X, want 0x%02X", decoded.Header.Flags, tc.flags)
			}
			if !bytes.Equal(decoded.Payload, tc.payload) {
				t.Errorf("Payload mismatch: got %v, want %v", decoded.Payload, tc.payload)
			}
		})
	}
}

func TestDecoderErrors(t *testing.T) {
	tests := []struct {
		name        string
		data        []byte
		expectedErr error
	}{
		// Frame too short cases
		{"frame_too_short_empty", []byte{}, ErrFrameTooShort},
		{"frame_too_short_one_byte", []byte{0x55}, ErrFrameTooShort},
		{"frame_too_short_just_sync", []byte{0x55, 0xAA}, ErrFrameTooShort},
		{"frame_too_short_partial_header", []byte{0x55, 0xAA, 0x00, 0x00, 0x00}, ErrFrameTooShort},
		{"frame_too_short_missing_crc", []byte{0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00}, ErrFrameTooShort},

		// Invalid sync cases
		{"invalid_sync_swapped", buildFrameWithBadSync(0xAA, 0x55), ErrInvalidSync},
		{"invalid_sync_zero", buildFrameWithBadSync(0x00, 0x00), ErrInvalidSync},

		// Invalid CRC case
		{"corrupted_crc", buildFrameWithCorruptedCRC(), ErrInvalidCRC},

		// Payload too large case
		{"payload_len_exceeds_max", buildFrameWithOversizedLen(), ErrPayloadTooLarge},
	}

	decoder := NewDecoder()

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			_, err := decoder.Decode(tc.data)
			if err != tc.expectedErr {
				t.Errorf("Decode() error = %v, want %v", err, tc.expectedErr)
			}
		})
	}
}

// buildFrameWithBadSync creates a min-size frame with custom sync bytes.
func buildFrameWithBadSync(b0, b1 byte) []byte {
	data := make([]byte, MinFrameSize)
	data[0], data[1] = b0, b1
	return data
}

// buildFrameWithCorruptedCRC creates a valid frame then corrupts CRC.
func buildFrameWithCorruptedCRC() []byte {
	encoder := NewEncoder()
	frame, _ := NewFrame(FrameTypeCommand, []byte{0x01, 0x02, 0x03})
	encoded, _ := encoder.Encode(frame)
	encoded[len(encoded)-1] ^= 0xFF
	return encoded
}

// buildFrameWithOversizedLen creates a frame with LEN > MaxPayloadSize.
func buildFrameWithOversizedLen() []byte {
	data := make([]byte, MinFrameSize)
	binary.BigEndian.PutUint16(data[0:2], SyncWord)
	binary.BigEndian.PutUint16(data[4:6], MaxPayloadSize+1)
	data[6] = byte(FrameTypeCommand)
	return data
}

// ============================================================================
// Round-Trip Tests
// ============================================================================

func TestEncodeDecodeRoundTrip(t *testing.T) {
	encoder := NewEncoder()
	decoder := NewDecoder()

	tests := []struct {
		name      string
		frameType FrameType
		seq       uint16
		flags     FrameFlags
		payload   []byte
	}{
		{"command_with_data", FrameTypeCommand, 12345, FlagRequiresAck, []byte("motor command data")},
		{"response_with_sensor_data", FrameTypeResponse, 12346, FlagNone, []byte{0x00, 0x01, 0x02, 0x03, 0x04, 0x05}},
		{"empty_ack", FrameTypeAck, 1, FlagNone, []byte{}},
		{"retransmit_flag", FrameTypeCommand, 100, FlagRetransmit | FlagRequiresAck, []byte{0xFF}},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			// Create original frame
			original, err := NewFrame(tc.frameType, tc.payload)
			if err != nil {
				t.Fatalf("NewFrame() error = %v", err)
			}
			original.Header.Sequence = tc.seq
			original.Header.Flags = tc.flags

			// Encode
			encoded, err := encoder.Encode(original)
			if err != nil {
				t.Fatalf("Encode() error = %v", err)
			}

			// Decode
			decoded, err := decoder.Decode(encoded)
			if err != nil {
				t.Fatalf("Decode() error = %v", err)
			}

			// Verify all fields match
			if decoded.Header.Sequence != original.Header.Sequence {
				t.Errorf("Sequence: got %d, want %d", decoded.Header.Sequence, original.Header.Sequence)
			}
			if decoded.Header.Length != original.Header.Length {
				t.Errorf("Length: got %d, want %d", decoded.Header.Length, original.Header.Length)
			}
			if decoded.Header.Type != original.Header.Type {
				t.Errorf("Type: got %v, want %v", decoded.Header.Type, original.Header.Type)
			}
			if decoded.Header.Flags != original.Header.Flags {
				t.Errorf("Flags: got 0x%02X, want 0x%02X", decoded.Header.Flags, original.Header.Flags)
			}
			if !bytes.Equal(decoded.Payload, original.Payload) {
				t.Errorf("Payload: got %v, want %v", decoded.Payload, original.Payload)
			}
			if decoded.CRC != original.CRC {
				t.Errorf("CRC: got 0x%08X, want 0x%08X", decoded.CRC, original.CRC)
			}
		})
	}
}

// ============================================================================
// Byte Order Tests
// ============================================================================

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
				{offset: 0, expected: 0x55, desc: "SYNC high byte"},
				{offset: 1, expected: 0xAA, desc: "SYNC low byte"},
				{offset: 2, expected: 0x12, desc: "SEQ high byte (big-endian)"},
				{offset: 3, expected: 0x34, desc: "SEQ low byte (big-endian)"},
				{offset: 4, expected: 0x00, desc: "LEN high byte (big-endian)"},
				{offset: 5, expected: 0x01, desc: "LEN low byte (big-endian)"},
			},
		},
		{
			name:    "max_seq_value",
			seq:     0xFFFF,
			payload: []byte{},
			checks: []byteCheck{
				{offset: 2, expected: 0xFF, desc: "SEQ high byte at max"},
				{offset: 3, expected: 0xFF, desc: "SEQ low byte at max"},
				{offset: 4, expected: 0x00, desc: "LEN high byte (zero payload)"},
				{offset: 5, expected: 0x00, desc: "LEN low byte (zero payload)"},
			},
		},
		{
			name:    "zero_seq_value",
			seq:     0x0000,
			payload: []byte{0xAB, 0xCD},
			checks: []byteCheck{
				{offset: 2, expected: 0x00, desc: "SEQ high byte at zero"},
				{offset: 3, expected: 0x00, desc: "SEQ low byte at zero"},
				{offset: 4, expected: 0x00, desc: "LEN high byte"},
				{offset: 5, expected: 0x02, desc: "LEN low byte (2 bytes payload)"},
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

			// Verify CRC is little-endian (LSB first)
			crcStart := len(encoded) - CRCSize
			expectedCRC := crc32.ChecksumIEEE(encoded[:crcStart])
			if encoded[crcStart] != byte(expectedCRC&0xFF) {
				t.Errorf("CRC LSB: got 0x%02X, want 0x%02X", encoded[crcStart], byte(expectedCRC&0xFF))
			}
			if encoded[crcStart+1] != byte((expectedCRC>>8)&0xFF) {
				t.Errorf("CRC byte 1: got 0x%02X, want 0x%02X", encoded[crcStart+1], byte((expectedCRC>>8)&0xFF))
			}
		})
	}
}
