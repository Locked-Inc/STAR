// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Convolutional encoder implementation for rate-1/2, K=7 code.
//
// Generator polynomials (NASA standard):
//   - G1 = 171 octal = 0b1111001 = 121 decimal
//   - G2 = 133 octal = 0b1011011 = 91 decimal
//
// The encoder uses a 6-bit shift register (K-1 = 6 memory elements).
// For each input bit, two output bits are generated.
//
// STAR Project - Texas A&M University
// December 2025
package fec

// Convolutional encoder constants.
const (
	// ConstraintLength K=7 means 6 memory elements (2^6 = 64 states).
	ConstraintLength = 7

	// NumStates is 2^(K-1) = 64 states in the trellis.
	NumStates = 1 << (ConstraintLength - 1)

	// G1Octal is the first generator polynomial (171 octal).
	G1Octal = 0171 // 0b1111001 = 121

	// G2Octal is the second generator polynomial (133 octal).
	G2Octal = 0133 // 0b1011011 = 91

	// TailBits is the number of zero bits to flush the encoder state.
	TailBits = ConstraintLength - 1
)

// ConvolutionalEncoder implements a rate-1/2, K=7 convolutional encoder.
type ConvolutionalEncoder struct {
	state uint8 // 6-bit shift register state
}

// NewConvolutionalEncoder creates a new convolutional encoder.
func NewConvolutionalEncoder() *ConvolutionalEncoder {
	return &ConvolutionalEncoder{state: 0}
}

// Encode applies convolutional encoding to the input data.
// For each input bit, two output bits are generated.
// Tail bits are appended to flush the encoder to the zero state.
//
// Output length = (inputLen * 8 + TailBits) * 2 bits, packed into bytes.
func (e *ConvolutionalEncoder) Encode(data []byte) ([]byte, error) {
	if len(data) == 0 {
		return nil, ErrInvalidInput
	}

	// Reset encoder state
	e.state = 0

	// Calculate output size: (input bits + tail bits) * 2 output bits per input
	inputBits := len(data) * 8
	totalInputBits := inputBits + TailBits
	totalOutputBits := totalInputBits * 2
	outputBytes := (totalOutputBits + 7) / 8

	output := make([]byte, outputBytes)
	outBitIdx := 0

	// Encode each input bit
	for _, b := range data {
		for i := 7; i >= 0; i-- {
			inputBit := (b >> i) & 1
			out0, out1 := e.encodeBit(inputBit)

			// Pack output bits
			setOutputBit(output, outBitIdx, out0)
			outBitIdx++
			setOutputBit(output, outBitIdx, out1)
			outBitIdx++
		}
	}

	// Append tail bits (zeros) to flush encoder to zero state
	for i := 0; i < TailBits; i++ {
		out0, out1 := e.encodeBit(0)
		setOutputBit(output, outBitIdx, out0)
		outBitIdx++
		setOutputBit(output, outBitIdx, out1)
		outBitIdx++
	}

	return output, nil
}

// encodeBit encodes a single bit and updates the encoder state.
// Returns two output bits (G1 and G2 polynomial outputs).
func (e *ConvolutionalEncoder) encodeBit(input uint8) (uint8, uint8) {
	// Shift in the new bit (input is MSB of the combined state)
	combined := (uint8(input) << 6) | e.state

	// Calculate output bits using generator polynomials
	out0 := parity(combined & G1Octal)
	out1 := parity(combined & G2Octal)

	// Update state (shift right, new bit enters from left)
	e.state = combined >> 1

	return out0, out1
}

// Rate returns the code rate (0.5 for rate-1/2).
func (e *ConvolutionalEncoder) Rate() float64 {
	return 0.5
}

// OutputLength returns the encoded output length in bytes for a given input length.
func (e *ConvolutionalEncoder) OutputLength(inputLen int) int {
	if inputLen <= 0 {
		return 0
	}
	inputBits := inputLen * 8
	totalInputBits := inputBits + TailBits
	totalOutputBits := totalInputBits * 2
	return (totalOutputBits + 7) / 8
}

// Reset resets the encoder state to zero.
func (e *ConvolutionalEncoder) Reset() {
	e.state = 0
}

// parity calculates the parity (XOR of all bits) of a byte.
func parity(x uint8) uint8 {
	x ^= x >> 4
	x ^= x >> 2
	x ^= x >> 1
	return x & 1
}

// setOutputBit sets a bit at the specified index in the output byte slice.
func setOutputBit(output []byte, bitIdx int, value uint8) {
	byteIdx := bitIdx / 8
	bitPos := 7 - (bitIdx % 8) // MSB first
	if value != 0 {
		output[byteIdx] |= 1 << bitPos
	}
}

// getOutputBit gets a bit at the specified index from a byte slice.
func getOutputBit(data []byte, bitIdx int) uint8 {
	byteIdx := bitIdx / 8
	bitPos := 7 - (bitIdx % 8) // MSB first
	return (data[byteIdx] >> bitPos) & 1
}
