// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Viterbi decoder implementation for rate-1/2, K=7 convolutional code.
//
// This decoder supports both soft and hard decision decoding.
// Soft decision decoding provides approximately 2dB coding gain over hard decision.
//
// The decoder uses the add-compare-select (ACS) algorithm with traceback
// to find the maximum likelihood path through the trellis.
//
// STAR Project - Texas A&M University
// December 2025
package fec

const (
	// TracebackDepth is the number of trellis stages to trace back.
	// Rule of thumb: 5 * K for good performance.
	TracebackDepth = 5 * ConstraintLength // 35

	// MaxPathMetric is used to initialize path metrics.
	MaxPathMetric = 0x7FFFFFFF
)

// ViterbiDecoder implements soft Viterbi decoding for rate-1/2, K=7 code.
type ViterbiDecoder struct {
	// pathMetrics stores the accumulated path metric for each state.
	pathMetrics [NumStates]int

	// newPathMetrics is used during ACS to avoid overwriting.
	newPathMetrics [NumStates]int

	// survivors stores the survivor path decisions for traceback.
	// For each time step and each destination state, stores the predecessor's LSB.
	survivors []uint64

	// branchMetrics is a precomputed lookup table for branch metrics.
	// Index: [state][input_bit] -> [out0, out1]
	branchTable [NumStates][2][2]uint8
}

// NewViterbiDecoder creates a new Viterbi decoder.
func NewViterbiDecoder() *ViterbiDecoder {
	d := &ViterbiDecoder{}
	d.initBranchTable()
	return d
}

// initBranchTable precomputes the output bits for each state transition.
func (d *ViterbiDecoder) initBranchTable() {
	for state := 0; state < NumStates; state++ {
		for input := 0; input < 2; input++ {
			// Compute output bits for this transition
			combined := (uint8(input) << 6) | uint8(state)
			out0 := parity(combined & G1Octal)
			out1 := parity(combined & G2Octal)
			d.branchTable[state][input][0] = out0
			d.branchTable[state][input][1] = out1
		}
	}
}

// DecodeSoft decodes soft bits using the Viterbi algorithm.
// expectedOutputLen specifies the expected decoded data length in bytes.
// If 0, it's auto-calculated from the soft bits length.
// Returns the decoded data, final path metric, and any error.
func (d *ViterbiDecoder) DecodeSoft(softBits []SoftBit, expectedOutputLen int) ([]byte, int, error) {
	if len(softBits) == 0 || len(softBits)%2 != 0 {
		return nil, 0, ErrInvalidSoftBits
	}

	// Calculate actual number of symbols based on expected output
	var numSymbols int
	if expectedOutputLen > 0 {
		// expectedOutputLen * 8 = data bits
		// data bits + tail bits = total symbols
		numSymbols = expectedOutputLen*8 + TailBits
	} else {
		// Auto-detect: use all soft bits
		numSymbols = len(softBits) / 2
	}

	if numSymbols < TailBits {
		return nil, 0, ErrInvalidSoftBits
	}

	// Ensure we have enough soft bits
	if numSymbols*2 > len(softBits) {
		numSymbols = len(softBits) / 2
	}

	// Allocate survivors array
	d.survivors = make([]uint64, numSymbols)

	// Initialize path metrics: state 0 = 0, others = MaxPathMetric
	for i := range d.pathMetrics {
		d.pathMetrics[i] = MaxPathMetric
	}
	d.pathMetrics[0] = 0

	// Process each symbol pair through the trellis
	for t := 0; t < numSymbols; t++ {
		soft0 := softBits[t*2]
		soft1 := softBits[t*2+1]

		// Reset new path metrics
		for i := range d.newPathMetrics {
			d.newPathMetrics[i] = MaxPathMetric
		}

		// For each current state, compute transitions
		for state := 0; state < NumStates; state++ {
			if d.pathMetrics[state] == MaxPathMetric {
				continue
			}

			// Try both input bits (0 and 1)
			for input := 0; input < 2; input++ {
				// Get expected output bits for this transition
				exp0 := d.branchTable[state][input][0]
				exp1 := d.branchTable[state][input][1]

				// Compute branch metric using soft information
				branchMetric := d.computeBranchMetric(soft0, soft1, exp0, exp1)

				// Compute next state: shift right and insert input as MSB
				nextState := (state >> 1) | (input << (ConstraintLength - 2))

				// Compute new path metric
				newMetric := d.pathMetrics[state] + branchMetric

				// Compare and select
				if newMetric < d.newPathMetrics[nextState] {
					d.newPathMetrics[nextState] = newMetric
					// Store the LSB of the winning predecessor state (`state`) for the given `nextState`.
					// This bit-packed `survivors[t]` field holds the LSB of the winning
					// predecessor for each of the 64 possible destination states at this time step.
					// This is used during traceback to reconstruct the most likely path.
					d.survivors[t] &^= 1 << uint(nextState) // Clear the bit for this state
					if state&1 == 1 {
						d.survivors[t] |= 1 << uint(nextState) // Set the bit if predecessor's LSB is 1
					}
				}
			}
		}

		// Swap path metrics
		d.pathMetrics, d.newPathMetrics = d.newPathMetrics, d.pathMetrics
	}

	// Traceback from state 0 (due to tail bits flushing encoder to zero)
	decodedBits := make([]uint8, numSymbols)
	state := 0

	for t := numSymbols - 1; t >= 0; t-- {
		// The input bit is the MSB of the current state
		inputBit := uint8((state >> (ConstraintLength - 2)) & 1)
		decodedBits[t] = inputBit

		// Get predecessor's LSB from survivors
		predecessorLSB := int((d.survivors[t] >> state) & 1)

		// Compute predecessor state: shift left and insert the LSB
		state = ((state << 1) & (NumStates - 1)) | predecessorLSB
	}

	// Remove tail bits and pack into bytes
	dataBits := numSymbols - TailBits
	if dataBits <= 0 {
		return nil, 0, ErrDecodeFailed
	}

	outputBytes := (dataBits + 7) / 8
	output := make([]byte, outputBytes)

	for i := 0; i < dataBits; i++ {
		if decodedBits[i] == 1 {
			byteIdx := i / 8
			bitPos := 7 - (i % 8)
			output[byteIdx] |= 1 << bitPos
		}
	}

	return output, d.pathMetrics[0], nil
}

// computeBranchMetric calculates the branch metric for soft decoding.
// Uses correlation metric: higher correlation = lower metric (better).
func (d *ViterbiDecoder) computeBranchMetric(soft0, soft1 SoftBit, exp0, exp1 uint8) int {
	// Convert expected bits to soft values for correlation
	// Expected 0 -> -127, Expected 1 -> +127
	exp0Soft := int(SoftBitMin)
	if exp0 == 1 {
		exp0Soft = int(SoftBitMax)
	}
	exp1Soft := int(SoftBitMin)
	if exp1 == 1 {
		exp1Soft = int(SoftBitMax)
	}

	// Branch metric is negative correlation (lower is better)
	// Metric = -correlation = -(soft0*exp0Soft + soft1*exp1Soft)
	// We want to minimize, so negate the correlation
	correlation := int(soft0)*exp0Soft + int(soft1)*exp1Soft

	// Scale and negate: high correlation -> low metric
	// Add offset to keep metrics positive
	return 32768 - correlation
}

// DecodeHard decodes hard decision bits.
// Converts hard bits to maximum confidence soft bits and decodes.
// expectedOutputLen specifies the expected decoded data length in bytes (0 = auto-detect).
func (d *ViterbiDecoder) DecodeHard(data []byte, expectedOutputLen int) ([]byte, error) {
	if len(data) == 0 {
		return nil, ErrInvalidInput
	}

	// Convert hard bits to soft bits
	numBits := len(data) * 8
	softBits := make([]SoftBit, numBits)

	for i := 0; i < numBits; i++ {
		bit := getOutputBit(data, i)
		softBits[i] = HardToSoft(bit)
	}

	decoded, _, err := d.DecodeSoft(softBits, expectedOutputLen)
	return decoded, err
}

// InputLength returns the expected soft bits length for a given output length.
func (d *ViterbiDecoder) InputLength(outputLen int) int {
	if outputLen <= 0 {
		return 0
	}
	// Output = (inputBits + TailBits) * 2 encoded bits
	// So inputBits = outputBits/2 - TailBits
	// And inputLen = inputBits / 8
	inputBits := outputLen * 8
	encodedBits := (inputBits + TailBits) * 2
	return encodedBits
}
