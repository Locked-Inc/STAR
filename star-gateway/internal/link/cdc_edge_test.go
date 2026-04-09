// Package link tests sequence edge cases for CDCLink.
//
// These tests verify behavior at protocol boundaries: uint16 wraparound,
// maximum gap tolerance, duplicate rejection, and simple mode permissiveness.
//
// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT
//
// STAR Project - Texas A&M University
// April 2026
package link

import (
	"context"
	"errors"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
)

// encodeTestFrame is a helper that creates and encodes a COMMAND frame.
func encodeTestFrame(t *testing.T, seq uint16, payload string) []byte {
	t.Helper()
	encoder := frame.NewEncoder()
	f := &frame.Frame{
		Header:  frame.Header{Sequence: seq, Length: uint16(len(payload)), Flags: 0},
		Type:    frame.FrameTypeCommand,
		Payload: []byte(payload),
	}
	data, err := encoder.Encode(f)
	if err != nil {
		t.Fatalf("Failed to encode frame seq=%d: %v", seq, err)
	}
	return data
}

// receiveOrFail calls Receive and fails the test on unexpected error.
func receiveOrFail(t *testing.T, link *CDCLink, transport *MockTransport, seq uint16, payload string) {
	t.Helper()
	transport.QueueReceive(encodeTestFrame(t, seq, payload))
	result, err := link.Receive(context.Background())
	if err != nil {
		t.Fatalf("Receive seq=%d failed: %v", seq, err)
	}
	if string(result.Payload) != payload {
		t.Errorf("Receive seq=%d payload = %q, want %q", seq, result.Payload, payload)
	}
}

// TestCDCLink_SequenceWraparound verifies uint16 wraparound at 65535 -> 0.
func TestCDCLink_SequenceWraparound(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Advance RX sequence to near wraparound by receiving frames directly.
	// Set session RX to 65534 by injecting frames at that sequence.
	// We need to get the session to expect seq=65534.
	// Simplest: use simple mode to accept any seq, then disable it.
	session.SetSimpleMode(true)
	transport.QueueReceive(encodeTestFrame(t, 65533, "a"))
	if _, err := link.Receive(context.Background()); err != nil {
		t.Fatalf("failed to prime RX sequence near wraparound: %v", err)
	}
	session.SetSimpleMode(false)
	// Now session expects seq=65534

	// Receive 65534 (exact match)
	receiveOrFail(t, link, transport, 65534, "b")

	// Receive 65535 (exact match)
	receiveOrFail(t, link, transport, 65535, "c")

	// Receive 0 (wraparound -- diff = 0 - 0 = 0, exact match after increment)
	receiveOrFail(t, link, transport, 0, "d")

	// Receive 1 (normal after wraparound)
	receiveOrFail(t, link, transport, 1, "e")

	t.Log("uint16 wraparound 65534 -> 65535 -> 0 -> 1: OK")
}

// TestCDCLink_GapAtMaxTolerance verifies gaps at the exact boundary.
func TestCDCLink_GapAtMaxTolerance(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Receive seq=0 (sets expected to 1)
	receiveOrFail(t, link, transport, 0, "a")

	// Gap of 8 (seq=9, expected=1, diff=8 < MaxGapTolerance=10) -> accepted
	receiveOrFail(t, link, transport, 9, "b")
	t.Log("Gap of 8 (within tolerance): accepted")

	// Now expected is 10. Gap of 9 (seq=19, diff=9 < 10) -> accepted
	receiveOrFail(t, link, transport, 19, "c")
	t.Log("Gap of 9 (within tolerance): accepted")
}

// TestCDCLink_GapExceedsTolerance verifies large gaps are rejected.
func TestCDCLink_GapExceedsTolerance(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Receive seq=0 (sets expected to 1)
	receiveOrFail(t, link, transport, 0, "a")

	// Gap of 10 (seq=11, expected=1, diff=10 >= MaxGapTolerance=10) -> rejected
	transport.QueueReceive(encodeTestFrame(t, 11, "b"))
	_, err = link.Receive(context.Background())
	if !errors.Is(err, harq.ErrDuplicateFrame) {
		t.Fatalf("Gap of 10 should be rejected, got err=%v", err)
	}
	t.Log("Gap of 10 (exceeds tolerance): rejected")
}

// TestCDCLink_DuplicateRejected verifies same sequence twice is rejected.
func TestCDCLink_DuplicateRejected(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Receive seq=0 (accepted, expected becomes 1)
	receiveOrFail(t, link, transport, 0, "a")

	// Receive seq=0 again (duplicate, diff = 0 - 1 = 65535, large gap -> rejected)
	transport.QueueReceive(encodeTestFrame(t, 0, "dup"))
	_, err = link.Receive(context.Background())
	if !errors.Is(err, harq.ErrDuplicateFrame) {
		t.Fatalf("Duplicate seq=0 should be rejected, got err=%v", err)
	}
	t.Log("Duplicate sequence: rejected")
}

// TestCDCLink_SimpleModeAcceptsAll verifies simple mode accepts any sequence.
func TestCDCLink_SimpleModeAcceptsAll(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	session.SetSimpleMode(true)

	// Receive out-of-order sequences -- all should be accepted
	receiveOrFail(t, link, transport, 42, "a")
	receiveOrFail(t, link, transport, 0, "b")   // backward jump
	receiveOrFail(t, link, transport, 100, "c") // large forward jump
	receiveOrFail(t, link, transport, 100, "d") // duplicate
	receiveOrFail(t, link, transport, 5, "e")   // backward jump

	t.Log("Simple mode: all sequences accepted regardless of order")
}

// TestCDCLink_ControlFramesBypassBarrier verifies RESET_ACK passes through
// during active reset barrier (critical for reset handshake).
func TestCDCLink_ControlFramesBypassBarrier(t *testing.T) {
	session := manager.NewSessionState()
	transport := &MockTransport{}
	link, err := NewCDCLink(transport, session)
	if err != nil {
		t.Fatalf("NewCDCLink failed: %v", err)
	}

	// Activate barrier (simulates reset handshake in progress)
	session.ActivateBarrier(1)

	// Data frame (COMMAND) should be rejected by barrier
	transport.QueueReceive(encodeTestFrame(t, 0, "data"))
	_, err = link.Receive(context.Background())
	if !errors.Is(err, harq.ErrDuplicateFrame) {
		t.Fatalf("COMMAND during barrier should be rejected, got err=%v", err)
	}

	// RESET_ACK frame should pass through barrier
	encoder := frame.NewEncoder()
	resetAck := &frame.Frame{
		Header:  frame.Header{Sequence: 0, Length: 4, Flags: 0},
		Type:    frame.FrameTypeResetAck,
		Payload: []byte{0x01, 0x00, 0x00, 0x00}, // session ID = 1
	}
	enc, err := encoder.Encode(resetAck)
	if err != nil {
		t.Fatalf("Failed to encode RESET_ACK: %v", err)
	}
	transport.QueueReceive(enc)

	result, err := link.Receive(context.Background())
	if err != nil {
		t.Fatalf("RESET_ACK during barrier should pass through: %v", err)
	}
	if result.Metadata.Type != frame.FrameTypeResetAck {
		t.Errorf("Expected RESET_ACK, got %s", result.Metadata.Type)
	}

	session.DeactivateBarrier()
	t.Log("Control frames bypass barrier: verified")
}
