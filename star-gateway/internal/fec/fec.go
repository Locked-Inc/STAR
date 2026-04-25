// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package fec implements Forward Error Correction for the HARQ protocol.
//
// This package provides a rate-1/2 convolutional encoder (K=7) with soft
// Viterbi decoding, designed for Chase Combining HARQ where soft bits from
// multiple transmissions are accumulated before decoding.
//
// FEC Parameters:
//   - Rate: 1/2 (2 output bits per input bit)
//   - Constraint Length: K=7 (64 states)
//   - Generator Polynomials: G1=171o, G2=133o (NASA standard)
//
// Reference: docs/sections/01_nanopb_protocol.tex (Layer 3: HARQ)
//
// STAR Project - Texas A&M University
// December 2025
package fec

import "errors"

// FEC error codes.
var (
	// ErrInvalidInput indicates the input data is nil or empty.
	ErrInvalidInput = errors.New("fec: invalid input data")

	// ErrInvalidSoftBits indicates the soft bits array is malformed.
	ErrInvalidSoftBits = errors.New("fec: invalid soft bits")

	// ErrDecodeFailed indicates Viterbi decoding failed to produce valid output.
	ErrDecodeFailed = errors.New("fec: decode failed")

	// ErrLengthMismatch indicates encoded/decoded length doesn't match expected.
	ErrLengthMismatch = errors.New("fec: length mismatch")
)

// SoftBit represents a soft decision value for a received bit.
// Range: -127 to +127
//   - Negative values indicate a '0' bit (more negative = higher confidence)
//   - Positive values indicate a '1' bit (more positive = higher confidence)
//   - Zero indicates an erasure (no information)
//
// The magnitude represents confidence level (log-likelihood ratio approximation).
type SoftBit int8

const (
	// SoftBitMax is the maximum soft bit value (high confidence '1').
	SoftBitMax SoftBit = 127

	// SoftBitMin is the minimum soft bit value (high confidence '0').
	SoftBitMin SoftBit = -127

	// SoftBitErasure indicates no information about the bit.
	SoftBitErasure SoftBit = 0
)

// ToBit converts a SoftBit to a hard decision (0 or 1).
func (s SoftBit) ToBit() uint8 {
	if s >= 0 {
		return 1
	}
	return 0
}

// Confidence returns the absolute confidence level (0-127).
func (s SoftBit) Confidence() uint8 {
	if s < 0 {
		return uint8(-s)
	}
	return uint8(s)
}

// HardToSoft converts a hard bit (0 or 1) to maximum confidence soft bit.
func HardToSoft(bit uint8) SoftBit {
	if bit == 0 {
		return SoftBitMin
	}
	return SoftBitMax
}

// Encoder defines the interface for FEC encoding.
type Encoder interface {
	// Encode applies forward error correction to the input data.
	// Returns FEC-encoded data (larger than input due to redundancy).
	Encode(data []byte) ([]byte, error)

	// Rate returns the code rate as a fraction (e.g., 0.5 for rate-1/2).
	Rate() float64

	// OutputLength returns the encoded output length for a given input length.
	OutputLength(inputLen int) int
}

// Decoder defines the interface for FEC decoding.
type Decoder interface {
	// DecodeSoft decodes soft bits using maximum likelihood decoding.
	// expectedOutputLen is the expected decoded data length in bytes (0 = auto-detect).
	// Returns decoded data, path metric (lower is better), and any error.
	DecodeSoft(softBits []SoftBit, expectedOutputLen int) ([]byte, int, error)

	// DecodeHard decodes hard decision bits (for testing/comparison).
	// expectedOutputLen is the expected decoded data length in bytes (0 = auto-detect).
	// Returns decoded data and any error.
	DecodeHard(data []byte, expectedOutputLen int) ([]byte, error)

	// InputLength returns the expected soft bits length for a given output length.
	InputLength(outputLen int) int
}

// SoftCombiner accumulates soft bits from multiple transmission attempts.
// Used for Chase Combining in HARQ Type I.
type SoftCombiner interface {
	// Add accumulates soft bits from a new transmission attempt.
	// The combiner element-wise adds the new soft bits to the existing buffer.
	Add(softBits []SoftBit) error

	// Combined returns the accumulated soft bits ready for decoding.
	Combined() []SoftBit

	// Count returns the number of transmissions combined so far.
	Count() int

	// Reset clears the combining buffer for a new frame.
	Reset()
}
