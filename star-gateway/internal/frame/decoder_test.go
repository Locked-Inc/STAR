// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package frame tests for wire protocol frame structures.
//
// STAR Project - Texas A&M University
// January 2026
package frame

import (
	"encoding/binary"
	"errors"
	"testing"
)

// Test-only constants (avoid magic numbers in tests)
const (
	UnknownType = Type(99)
)

// ============================================================================
// Decoder Tests
// ============================================================================

func TestDecoder_Decode(t *testing.T) {
	encoder := NewEncoder()
	decoder := NewDecoder()

	// Helper to create a valid encoded frame
	createEncodedFrame := func(t *testing.T, ft Type, seq uint16, flags Flags, payload []byte) []byte {
		frame, err := NewFrame(ft, payload)
		if err != nil {
			t.Fatalf("NewFrame failed: %v", err)
		}
		frame.Header.Sequence = seq
		frame.Header.Flags = flags
		encoded, err := encoder.Encode(frame)
		if err != nil {
			t.Fatalf("Encode failed: %v", err)
		}
		return encoded
	}

	tests := []struct {
		name        string
		input       []byte
		expectError error
		validate    func(*testing.T, *Frame)
	}{
		{
			name:  "valid_frame_with_payload",
			input: createEncodedFrame(t, FrameTypeCommand, 123, FlagRequiresAck, []byte("hello")),
			validate: func(t *testing.T, f *Frame) {
				if f.Type != FrameTypeCommand {
					t.Errorf("Type = %v, want %v", f.Type, FrameTypeCommand)
				}
				if f.Header.Sequence != 123 {
					t.Errorf("Sequence = %d, want 123", f.Header.Sequence)
				}
				if f.Header.Flags != FlagRequiresAck {
					t.Errorf("Flags = %v, want %v", f.Header.Flags, FlagRequiresAck)
				}
				if string(f.Payload) != "hello" {
					t.Errorf("Payload = %s, want hello", string(f.Payload))
				}
			},
		},
		{
			name:  "valid_frame_empty_payload",
			input: createEncodedFrame(t, FrameTypeAck, 456, FlagNone, []byte{}),
			validate: func(t *testing.T, f *Frame) {
				if f.Type != FrameTypeAck {
					t.Errorf("Type = %v, want %v", f.Type, FrameTypeAck)
				}
				if len(f.Payload) != 0 {
					t.Errorf("Payload len = %d, want 0", len(f.Payload))
				}
			},
		},
		{
			name:        "frame_too_short",
			input:       make([]byte, MinFrameSize-1),
			expectError: ErrFrameTooShort,
		},
		{
			name: "invalid_sync_word",
			input: func() []byte {
				data := createEncodedFrame(t, FrameTypeCommand, 1, FlagNone, []byte{})
				data[0] = 0x00 // Corrupt sync word
				return data
			}(),
			expectError: ErrInvalidSync,
		},
		{
			name: "payload_too_large_in_header",
			input: func() []byte {
				data := createEncodedFrame(t, FrameTypeCommand, 1, FlagNone, []byte{})
				// Set length field to MaxPayloadSize + 1
				binary.LittleEndian.PutUint16(data[4:6], uint16(MaxPayloadSize+1))
				return data
			}(),
			expectError: ErrPayloadTooLarge,
		},
		{
			name: "incomplete_frame",
			input: func() []byte {
				data := createEncodedFrame(t, FrameTypeCommand, 1, FlagNone, []byte("data"))
				return data[:len(data)-1] // Cut off last byte
			}(),
			expectError: ErrFrameTooShort,
		},
		{
			name: "invalid_crc",
			input: func() []byte {
				data := createEncodedFrame(t, FrameTypeCommand, 1, FlagNone, []byte("data"))
				// Corrupt the CRC (last 4 bytes)
				data[len(data)-1] ^= 0xFF
				return data
			}(),
			expectError: ErrInvalidCRC,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			frame, err := decoder.Decode(tc.input)
			if tc.expectError != nil {
				if !errors.Is(err, tc.expectError) {
					t.Errorf("Decode error = %v, want %v", err, tc.expectError)
				}
			} else {
				if err != nil {
					t.Fatalf("Decode unexpected error: %v", err)
				}
				if tc.validate != nil {
					tc.validate(t, frame)
				}
			}
		})
	}
}

// ============================================================================
// Helper Tests (Type String, NewFrame Errors)
// ============================================================================

func TestType_String(t *testing.T) {
	tests := []struct {
		t        Type
		expected string
	}{
		{FrameTypeCommand, "COMMAND"},
		{FrameTypeResponse, "RESPONSE"},
		{FrameTypeAck, "ACK"},
		{FrameTypeNack, "NACK"},
		{FrameTypeUnknown, "UNKNOWN"},
		{UnknownType, "UNKNOWN"},
	}

	for _, tc := range tests {
		if s := tc.t.String(); s != tc.expected {
			t.Errorf("Type(%d).String() = %s, want %s", tc.t, s, tc.expected)
		}
	}
}

func TestNewFrame_Error(t *testing.T) {
	// Payload too large
	largePayload := make([]byte, MaxPayloadSize+1)
	_, err := NewFrame(FrameTypeCommand, largePayload)
	if !errors.Is(err, ErrPayloadTooLarge) {
		t.Errorf("NewFrame with large payload error = %v, want %v", err, ErrPayloadTooLarge)
	}
}
