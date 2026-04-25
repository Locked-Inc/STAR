// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Chase Combiner implementation for HARQ Type I.
//
// Chase Combining stores soft bits from failed transmission attempts and
// combines them element-wise (addition) before decoding. This improves
// the effective SNR with each retransmission.
//
// Example:
//
//	Transmission 1: soft bits S1 (decode fails)
//	Transmission 2: soft bits S2 (decode fails)
//	Combined: S_combined[i] = S1[i] + S2[i] (clamped to int16 range)
//	Decode: Viterbi(S_combined) -> success
//
// STAR Project - Texas A&M University
// December 2025
package fec

import "errors"

// Combiner error codes.
var (
	// ErrCombinerFull indicates max combining attempts reached.
	ErrCombinerFull = errors.New("fec: combiner full, max attempts reached")

	// ErrCombinerEmpty indicates no soft bits have been added.
	ErrCombinerEmpty = errors.New("fec: combiner empty")

	// ErrSoftBitsLengthMismatch indicates soft bits length doesn't match.
	ErrSoftBitsLengthMismatch = errors.New("fec: soft bits length mismatch")
)

// DefaultMaxCombines is the default maximum number of combining attempts.
const DefaultMaxCombines = 3

// ChaseCombiner implements soft bit combining for HARQ Type I.
type ChaseCombiner struct {
	// accumulated stores the sum of soft bits across all attempts.
	// Uses int16 to prevent overflow during accumulation.
	accumulated []int16

	// count is the number of transmissions combined so far.
	count int

	// maxCombines is the maximum number of combining attempts.
	maxCombines int

	// expectedLen is the expected length of soft bits (set by first Add).
	expectedLen int
}

// CombinerConfig holds configuration for the Chase Combiner.
type CombinerConfig struct {
	// MaxCombines is the maximum number of transmission attempts to combine.
	MaxCombines int
}

// NewChaseCombiner creates a new Chase Combiner with the given config.
func NewChaseCombiner(config *CombinerConfig) *ChaseCombiner {
	maxCombines := DefaultMaxCombines
	if config != nil && config.MaxCombines > 0 {
		maxCombines = config.MaxCombines
	}

	return &ChaseCombiner{
		maxCombines: maxCombines,
	}
}

// Add accumulates soft bits from a new transmission attempt.
// The first call sets the expected length for subsequent calls.
func (c *ChaseCombiner) Add(softBits []SoftBit) error {
	if len(softBits) == 0 {
		return ErrInvalidSoftBits
	}

	if c.count >= c.maxCombines {
		return ErrCombinerFull
	}

	// First transmission sets the expected length
	if c.count == 0 {
		c.expectedLen = len(softBits)
		c.accumulated = make([]int16, c.expectedLen)
	} else if len(softBits) != c.expectedLen {
		return ErrSoftBitsLengthMismatch
	}

	// Element-wise addition (accumulate)
	for i, soft := range softBits {
		c.accumulated[i] += int16(soft)
	}

	c.count++
	return nil
}

// Combined returns the accumulated soft bits ready for decoding.
// The accumulated values are scaled and clamped back to SoftBit range.
func (c *ChaseCombiner) Combined() []SoftBit {
	if c.count == 0 || len(c.accumulated) == 0 {
		return nil
	}

	result := make([]SoftBit, len(c.accumulated))

	for i, acc := range c.accumulated {
		// Clamp to SoftBit range [-127, +127]
		if acc > int16(SoftBitMax) {
			result[i] = SoftBitMax
		} else if acc < int16(SoftBitMin) {
			result[i] = SoftBitMin
		} else {
			result[i] = SoftBit(acc)
		}
	}

	return result
}

// Count returns the number of transmissions combined so far.
func (c *ChaseCombiner) Count() int {
	return c.count
}

// Reset clears the combining buffer for a new frame.
func (c *ChaseCombiner) Reset() {
	c.accumulated = nil
	c.count = 0
	c.expectedLen = 0
}

// CanAdd returns true if more transmissions can be combined.
func (c *ChaseCombiner) CanAdd() bool {
	return c.count < c.maxCombines
}

// MaxCombines returns the maximum number of combining attempts.
func (c *ChaseCombiner) MaxCombines() int {
	return c.maxCombines
}
