// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package frame

import (
	"bytes"
	"testing"
)

// FuzzFrameDecode tests that Decoder.Decode never panics on arbitrary input.
// Seeds are drawn from the cross-compatibility vectors shared with C firmware.
func FuzzFrameDecode(f *testing.F) {
	// Seed with every cross-compatibility wire vector (valid encoded frames).
	for _, v := range crossCompatibilityVectors() {
		f.Add(v.wire)
	}

	// Seed with a few degenerate inputs that exercise early-exit paths.
	f.Add([]byte{})                   // empty
	f.Add([]byte{0xAA})               // partial sync
	f.Add([]byte{0xAA, 0x55})         // sync only, no header
	f.Add([]byte{0x00, 0x00})         // wrong sync word
	f.Add(make([]byte, MinFrameSize)) // minimum size, all zeros (bad sync)

	f.Fuzz(func(t *testing.T, data []byte) {
		decoder := NewDecoder()
		// Must not panic. Errors are expected for malformed input.
		_, _ = decoder.Decode(data)
	})
}

// FuzzStreamDecoder tests that StreamDecoder.Decode never panics on arbitrary
// byte streams, including garbage prefixes, truncated frames, and corrupted CRCs.
func FuzzStreamDecoder(f *testing.F) {
	// Seed with valid frames built by the same helper used in unit tests.
	seeds := []struct {
		seq     uint16
		flags   Flags
		payload []byte
	}{
		{0x0000, FlagNone, nil},
		{0x0001, FlagNone, []byte("Hello STAR")},
		{0x0002, FlagRequiresAck, []byte{}},
		{0x0003, FlagPriority, []byte{0xDE, 0xAD, 0xBE, 0xEF}},
		{0x0004, FlagFECEnabled, []byte{0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD}},
	}
	for _, s := range seeds {
		f.Add(buildTestFrame(s.seq, s.flags, s.payload))
	}

	// Seed with the cross-compatibility wire vectors as well.
	for _, v := range crossCompatibilityVectors() {
		f.Add(v.wire)
	}

	// Degenerate seeds.
	f.Add([]byte{})
	f.Add([]byte{0xAA, 0x55})
	f.Add([]byte{0xFF, 0xFF, 0xFF, 0xFF})

	f.Fuzz(func(t *testing.T, data []byte) {
		reader := bytes.NewReader(data)
		sd := NewStreamDecoder(reader)
		// Must not panic. EOF and decode errors are expected.
		_, _ = sd.Decode()
	})
}
