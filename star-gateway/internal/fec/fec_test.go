// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// FEC package tests for convolutional encoder, Viterbi decoder, and Chase Combiner.
//
// STAR Project - Texas A&M University
// December 2025
package fec

import (
	"bytes"
	"testing"
)

// =============================================================================
// SoftBit Tests
// =============================================================================

func TestSoftBit_ToBit(t *testing.T) {
	tests := []struct {
		name     string
		soft     SoftBit
		expected uint8
	}{
		{"max positive", SoftBitMax, 1},
		{"positive", SoftBit(50), 1},
		{"zero (erasure)", SoftBitErasure, 1}, // Zero maps to 1 by convention
		{"negative", SoftBit(-50), 0},
		{"max negative", SoftBitMin, 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := tt.soft.ToBit()
			if got != tt.expected {
				t.Errorf("SoftBit(%d).ToBit() = %d, want %d", tt.soft, got, tt.expected)
			}
		})
	}
}

func TestSoftBit_Confidence(t *testing.T) {
	tests := []struct {
		name     string
		soft     SoftBit
		expected uint8
	}{
		{"max positive", SoftBitMax, 127},
		{"positive", SoftBit(50), 50},
		{"zero", SoftBitErasure, 0},
		{"negative", SoftBit(-50), 50},
		{"max negative", SoftBitMin, 127},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := tt.soft.Confidence()
			if got != tt.expected {
				t.Errorf("SoftBit(%d).Confidence() = %d, want %d", tt.soft, got, tt.expected)
			}
		})
	}
}

func TestHardToSoft(t *testing.T) {
	tests := []struct {
		bit      uint8
		expected SoftBit
	}{
		{0, SoftBitMin},
		{1, SoftBitMax},
	}

	for _, tt := range tests {
		got := HardToSoft(tt.bit)
		if got != tt.expected {
			t.Errorf("HardToSoft(%d) = %d, want %d", tt.bit, got, tt.expected)
		}
	}
}

// =============================================================================
// Convolutional Encoder Tests
// =============================================================================

func TestConvolutionalEncoder_Constants(t *testing.T) {
	if ConstraintLength != 7 {
		t.Errorf("ConstraintLength = %d, want 7", ConstraintLength)
	}
	if NumStates != 64 {
		t.Errorf("NumStates = %d, want 64", NumStates)
	}
	if TailBits != 6 {
		t.Errorf("TailBits = %d, want 6", TailBits)
	}
	if G1Octal != 0171 {
		t.Errorf("G1Octal = %o, want 171", G1Octal)
	}
	if G2Octal != 0133 {
		t.Errorf("G2Octal = %o, want 133", G2Octal)
	}
}

func TestConvolutionalEncoder_Rate(t *testing.T) {
	enc := NewConvolutionalEncoder()
	if enc.Rate() != 0.5 {
		t.Errorf("Rate() = %f, want 0.5", enc.Rate())
	}
}

func TestConvolutionalEncoder_OutputLength(t *testing.T) {
	enc := NewConvolutionalEncoder()

	tests := []struct {
		inputLen int
		expected int
	}{
		{0, 0},
		{1, 4},     // (8 + 6) * 2 = 28 bits -> ceil(28/8) = 4 bytes
		{10, 22},   // (80 + 6) * 2 = 172 bits -> ceil(172/8) = 22 bytes
		{100, 202}, // (800 + 6) * 2 = 1612 bits -> ceil(1612/8) = 202 bytes
	}

	for _, tt := range tests {
		got := enc.OutputLength(tt.inputLen)
		if got != tt.expected {
			t.Errorf("OutputLength(%d) = %d, want %d", tt.inputLen, got, tt.expected)
		}

		// Verify calculation formula for non-zero inputs
		if tt.inputLen > 0 {
			inputBits := tt.inputLen * 8
			totalInputBits := inputBits + TailBits
			totalOutputBits := totalInputBits * 2
			calculatedExpected := (totalOutputBits + 7) / 8
			if got != calculatedExpected {
				t.Errorf("OutputLength(%d) formula mismatch: got %d, calculated %d", tt.inputLen, got, calculatedExpected)
			}
		}
	}
}

func TestConvolutionalEncoder_EncodeNil(t *testing.T) {
	enc := NewConvolutionalEncoder()
	_, err := enc.Encode(nil)
	if err != ErrInvalidInput {
		t.Errorf("Encode(nil) error = %v, want ErrInvalidInput", err)
	}
}

func TestConvolutionalEncoder_EncodeEmpty(t *testing.T) {
	enc := NewConvolutionalEncoder()
	_, err := enc.Encode([]byte{})
	if err != ErrInvalidInput {
		t.Errorf("Encode([]) error = %v, want ErrInvalidInput", err)
	}
}

func TestConvolutionalEncoder_EncodeSingleByte(t *testing.T) {
	enc := NewConvolutionalEncoder()

	data := []byte{0xAB} // 10101011
	encoded, err := enc.Encode(data)
	if err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	expectedLen := enc.OutputLength(1)
	if len(encoded) != expectedLen {
		t.Errorf("Encode() len = %d, want %d", len(encoded), expectedLen)
	}

	// Verify encoder ends in zero state (due to tail bits)
	if enc.state != 0 {
		t.Errorf("encoder state after Encode = %d, want 0", enc.state)
	}
}

func TestConvolutionalEncoder_EncodeMultipleBytes(t *testing.T) {
	enc := NewConvolutionalEncoder()

	data := []byte{0x12, 0x34, 0x56, 0x78}
	encoded, err := enc.Encode(data)
	if err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	expectedLen := enc.OutputLength(4)
	if len(encoded) != expectedLen {
		t.Errorf("Encode() len = %d, want %d", len(encoded), expectedLen)
	}
}

func TestConvolutionalEncoder_Reset(t *testing.T) {
	enc := NewConvolutionalEncoder()

	// Encode something to change state
	if _, err := enc.Encode([]byte{0xFF}); err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	// After encode, state should be 0 due to tail bits
	if enc.state != 0 {
		t.Errorf("state after Encode = %d, want 0", enc.state)
	}

	// But if we manually set state and reset...
	enc.state = 42
	enc.Reset()
	if enc.state != 0 {
		t.Errorf("state after Reset = %d, want 0", enc.state)
	}
}

// =============================================================================
// Viterbi Decoder Tests
// =============================================================================

func TestViterbiDecoder_DecodeEmptySoftBits(t *testing.T) {
	dec := NewViterbiDecoder()

	_, _, err := dec.DecodeSoft(nil, 0)
	if err != ErrInvalidSoftBits {
		t.Errorf("DecodeSoft(nil) error = %v, want ErrInvalidSoftBits", err)
	}

	_, _, err = dec.DecodeSoft([]SoftBit{}, 0)
	if err != ErrInvalidSoftBits {
		t.Errorf("DecodeSoft([]) error = %v, want ErrInvalidSoftBits", err)
	}
}

func TestViterbiDecoder_DecodeOddLengthSoftBits(t *testing.T) {
	dec := NewViterbiDecoder()

	_, _, err := dec.DecodeSoft([]SoftBit{SoftBitMax}, 0)
	if err != ErrInvalidSoftBits {
		t.Errorf("DecodeSoft([1 element]) error = %v, want ErrInvalidSoftBits", err)
	}
}

func TestViterbiDecoder_DecodeHardEmpty(t *testing.T) {
	dec := NewViterbiDecoder()

	_, err := dec.DecodeHard(nil, 0)
	if err != ErrInvalidInput {
		t.Errorf("DecodeHard(nil) error = %v, want ErrInvalidInput", err)
	}
}

// =============================================================================
// Round-trip Tests (Encode -> Decode)
// =============================================================================

func TestEncodeDecodeRoundTrip_SingleByte(t *testing.T) {
	enc := NewConvolutionalEncoder()
	dec := NewViterbiDecoder()

	original := []byte{0x55} // 01010101

	// Encode
	encoded, err := enc.Encode(original)
	if err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	// Decode using hard decision
	decoded, err := dec.DecodeHard(encoded, len(original))
	if err != nil {
		t.Fatalf("DecodeHard() error = %v", err)
	}

	if !bytes.Equal(original, decoded) {
		t.Errorf("Round-trip failed: original=%v, decoded=%v", original, decoded)
	}
}

func TestEncodeDecodeRoundTrip_MultipleBytes(t *testing.T) {
	enc := NewConvolutionalEncoder()
	dec := NewViterbiDecoder()

	testCases := [][]byte{
		{0x00},
		{0xFF},
		{0x12, 0x34},
		{0xAB, 0xCD, 0xEF},
		{0x01, 0x02, 0x03, 0x04, 0x05},
	}

	for _, original := range testCases {
		encoded, err := enc.Encode(original)
		if err != nil {
			t.Fatalf("Encode(%v) error = %v", original, err)
		}

		decoded, err := dec.DecodeHard(encoded, len(original))
		if err != nil {
			t.Fatalf("DecodeHard() error = %v", err)
		}

		if !bytes.Equal(original, decoded) {
			t.Errorf("Round-trip failed for %v: decoded=%v", original, decoded)
		}
	}
}

func TestEncodeDecodeRoundTrip_AllZeros(t *testing.T) {
	enc := NewConvolutionalEncoder()
	dec := NewViterbiDecoder()

	original := make([]byte, 16)
	encoded, err := enc.Encode(original)
	if err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	decoded, err := dec.DecodeHard(encoded, len(original))
	if err != nil {
		t.Fatalf("DecodeHard() error = %v", err)
	}

	if !bytes.Equal(original, decoded) {
		t.Errorf("Round-trip failed for all zeros")
	}
}

func TestEncodeDecodeRoundTrip_AllOnes(t *testing.T) {
	enc := NewConvolutionalEncoder()
	dec := NewViterbiDecoder()

	original := bytes.Repeat([]byte{0xFF}, 16)
	encoded, err := enc.Encode(original)
	if err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	decoded, err := dec.DecodeHard(encoded, len(original))
	if err != nil {
		t.Fatalf("DecodeHard() error = %v", err)
	}

	if !bytes.Equal(original, decoded) {
		t.Errorf("Round-trip failed for all ones")
	}
}

// =============================================================================
// Chase Combiner Tests
// =============================================================================

func TestChaseCombiner_NewWithDefaultConfig(t *testing.T) {
	c := NewChaseCombiner(nil)
	if c.MaxCombines() != DefaultMaxCombines {
		t.Errorf("MaxCombines() = %d, want %d", c.MaxCombines(), DefaultMaxCombines)
	}
	if c.Count() != 0 {
		t.Errorf("Count() = %d, want 0", c.Count())
	}
}

func TestChaseCombiner_NewWithCustomConfig(t *testing.T) {
	c := NewChaseCombiner(&CombinerConfig{MaxCombines: 5})
	if c.MaxCombines() != 5 {
		t.Errorf("MaxCombines() = %d, want 5", c.MaxCombines())
	}
}

func TestChaseCombiner_AddEmpty(t *testing.T) {
	c := NewChaseCombiner(nil)
	err := c.Add(nil)
	if err != ErrInvalidSoftBits {
		t.Errorf("Add(nil) error = %v, want ErrInvalidSoftBits", err)
	}

	err = c.Add([]SoftBit{})
	if err != ErrInvalidSoftBits {
		t.Errorf("Add([]) error = %v, want ErrInvalidSoftBits", err)
	}
}

func TestChaseCombiner_AddSingleTransmission(t *testing.T) {
	c := NewChaseCombiner(nil)

	softBits := []SoftBit{SoftBitMax, SoftBitMin, SoftBit(50), SoftBit(-50)}
	err := c.Add(softBits)
	if err != nil {
		t.Fatalf("Add() error = %v", err)
	}

	if c.Count() != 1 {
		t.Errorf("Count() = %d, want 1", c.Count())
	}

	combined := c.Combined()
	if len(combined) != len(softBits) {
		t.Errorf("Combined() len = %d, want %d", len(combined), len(softBits))
	}

	// Single transmission: combined should equal original
	for i, s := range softBits {
		if combined[i] != s {
			t.Errorf("Combined()[%d] = %d, want %d", i, combined[i], s)
		}
	}
}

func TestChaseCombiner_AddMultipleTransmissions(t *testing.T) {
	c := NewChaseCombiner(nil)

	soft1 := []SoftBit{50, -50, 0, 100}
	soft2 := []SoftBit{50, -50, 0, 27}

	if err := c.Add(soft1); err != nil {
		t.Fatalf("Add(soft1) error = %v", err)
	}
	if err := c.Add(soft2); err != nil {
		t.Fatalf("Add(soft2) error = %v", err)
	}

	if c.Count() != 2 {
		t.Errorf("Count() = %d, want 2", c.Count())
	}

	combined := c.Combined()

	// Expected: element-wise sum, clamped
	expected := []SoftBit{100, -100, 0, 127} // last element: 100 + 27 = 127 (at SoftBitMax limit)
	for i, exp := range expected {
		if combined[i] != exp {
			t.Errorf("Combined()[%d] = %d, want %d", i, combined[i], exp)
		}
	}
}

func TestChaseCombiner_AddLengthMismatch(t *testing.T) {
	c := NewChaseCombiner(nil)

	if err := c.Add([]SoftBit{1, 2, 3, 4}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	err := c.Add([]SoftBit{1, 2, 3}) // Different length

	if err != ErrSoftBitsLengthMismatch {
		t.Errorf("Add() error = %v, want ErrSoftBitsLengthMismatch", err)
	}
}

func TestChaseCombiner_AddMaxExceeded(t *testing.T) {
	c := NewChaseCombiner(&CombinerConfig{MaxCombines: 2})

	if err := c.Add([]SoftBit{1, 2}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	if err := c.Add([]SoftBit{3, 4}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}

	err := c.Add([]SoftBit{5, 6})
	if err != ErrCombinerFull {
		t.Errorf("Add() error = %v, want ErrCombinerFull", err)
	}

	if c.Count() != 2 {
		t.Errorf("Count() = %d, want 2", c.Count())
	}
}

func TestChaseCombiner_Reset(t *testing.T) {
	c := NewChaseCombiner(nil)

	if err := c.Add([]SoftBit{1, 2, 3}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	if err := c.Add([]SoftBit{4, 5, 6}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}

	c.Reset()

	if c.Count() != 0 {
		t.Errorf("Count() after Reset = %d, want 0", c.Count())
	}

	combined := c.Combined()
	if combined != nil {
		t.Errorf("Combined() after Reset = %v, want nil", combined)
	}

	// Should be able to add again with different length
	err := c.Add([]SoftBit{10, 20})
	if err != nil {
		t.Errorf("Add() after Reset error = %v, want nil", err)
	}
}

func TestChaseCombiner_CanAdd(t *testing.T) {
	c := NewChaseCombiner(&CombinerConfig{MaxCombines: 2})

	if !c.CanAdd() {
		t.Errorf("CanAdd() = false before any adds")
	}

	if err := c.Add([]SoftBit{1}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	if !c.CanAdd() {
		t.Errorf("CanAdd() = false after 1 add")
	}

	if err := c.Add([]SoftBit{2}); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	if c.CanAdd() {
		t.Errorf("CanAdd() = true after max adds")
	}
}

func TestChaseCombiner_Clamping(t *testing.T) {
	c := NewChaseCombiner(&CombinerConfig{MaxCombines: 4})

	// Add max positive values repeatedly to test clamping
	maxSoft := []SoftBit{SoftBitMax, SoftBitMax}
	if err := c.Add(maxSoft); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	if err := c.Add(maxSoft); err != nil {
		t.Fatalf("Add() error = %v", err)
	}

	combined := c.Combined()

	// 127 + 127 = 254, should be clamped to 127
	if combined[0] != SoftBitMax {
		t.Errorf("Combined()[0] = %d, want %d (clamped)", combined[0], SoftBitMax)
	}
}

func TestChaseCombiner_NegativeClamping(t *testing.T) {
	c := NewChaseCombiner(&CombinerConfig{MaxCombines: 4})

	// Add min negative values repeatedly to test clamping
	minSoft := []SoftBit{SoftBitMin, SoftBitMin}
	if err := c.Add(minSoft); err != nil {
		t.Fatalf("Add() error = %v", err)
	}
	if err := c.Add(minSoft); err != nil {
		t.Fatalf("Add() error = %v", err)
	}

	combined := c.Combined()

	// -127 + -127 = -254, should be clamped to -127
	if combined[0] != SoftBitMin {
		t.Errorf("Combined()[0] = %d, want %d (clamped)", combined[0], SoftBitMin)
	}
}

// =============================================================================
// Integration: Chase Combining with Decode
// =============================================================================

func TestChaseCombining_ImprovesDecode(t *testing.T) {
	enc := NewConvolutionalEncoder()
	dec := NewViterbiDecoder()

	original := []byte{0xAB, 0xCD}

	// Encode
	encoded, err := enc.Encode(original)
	if err != nil {
		t.Fatalf("Encode() error = %v", err)
	}

	// Convert to soft bits with some noise (reduced confidence)
	numBits := len(encoded) * 8
	noisySoft1 := make([]SoftBit, numBits)
	noisySoft2 := make([]SoftBit, numBits)

	for i := 0; i < numBits; i++ {
		bit := getOutputBit(encoded, i)
		if bit == 1 {
			noisySoft1[i] = 40 // Reduced confidence
			noisySoft2[i] = 40 // Reduced confidence
		} else {
			noisySoft1[i] = -40 // Reduced confidence
			noisySoft2[i] = -40 // Reduced confidence
		}
	}

	// Combine soft bits
	combiner := NewChaseCombiner(nil)
	if err := combiner.Add(noisySoft1); err != nil {
		t.Fatalf("Add(noisySoft1) error = %v", err)
	}
	if err := combiner.Add(noisySoft2); err != nil {
		t.Fatalf("Add(noisySoft2) error = %v", err)
	}

	combined := combiner.Combined()

	// Combined should have higher confidence
	for i := 0; i < numBits; i++ {
		bit := getOutputBit(encoded, i)
		if bit == 1 {
			if combined[i] != 80 { // 40 + 40
				t.Errorf("combined[%d] = %d, want 80", i, combined[i])
			}
		} else {
			if combined[i] != -80 { // -40 + -40
				t.Errorf("combined[%d] = %d, want -80", i, combined[i])
			}
		}
	}

	// Decode combined
	decoded, _, err := dec.DecodeSoft(combined, len(original))
	if err != nil {
		t.Fatalf("DecodeSoft() error = %v", err)
	}

	if !bytes.Equal(original, decoded) {
		t.Errorf("Chase combining decode failed: original=%v, decoded=%v", original, decoded)
	}
}
