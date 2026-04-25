// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package manager

import (
	"encoding/binary"
	"testing"
)

func TestSessionState_IncrementSessionID(t *testing.T) {
	ss := NewSessionState()

	// Initial session ID is 0, first increment returns 1
	id1 := ss.IncrementSessionID()
	if id1 != 1 {
		t.Errorf("first IncrementSessionID = %d, want 1", id1)
	}

	id2 := ss.IncrementSessionID()
	if id2 != 2 {
		t.Errorf("second IncrementSessionID = %d, want 2", id2)
	}

	// GetSessionID should match last increment
	if got := ss.GetSessionID(); got != 2 {
		t.Errorf("GetSessionID = %d, want 2", got)
	}
}

func TestSessionState_BarrierActivateDeactivate(t *testing.T) {
	ss := NewSessionState()

	if ss.IsBarrierActive() {
		t.Error("barrier should be inactive initially")
	}

	ss.ActivateBarrier(5)
	if !ss.IsBarrierActive() {
		t.Error("barrier should be active after ActivateBarrier")
	}

	ss.DeactivateBarrier()
	if ss.IsBarrierActive() {
		t.Error("barrier should be inactive after DeactivateBarrier")
	}
}

func TestSessionState_Barrier_RejectsDataFrames(t *testing.T) {
	ss := NewSessionState()

	// Without barrier, sequence validation should pass
	if !ss.ValidateRxSequence(0) {
		t.Error("ValidateRxSequence should pass without barrier")
	}

	// Activate barrier
	ss.ActivateBarrier(1)

	// All sequence validations should fail while barrier is active
	if ss.ValidateRxSequence(1) {
		t.Error("ValidateRxSequence should reject during active barrier")
	}
	if ss.ValidateRxSequence(0) {
		t.Error("ValidateRxSequence should reject seq=0 during active barrier")
	}
	if ss.ValidateRxSequence(100) {
		t.Error("ValidateRxSequence should reject seq=100 during active barrier")
	}
}

func TestSessionState_Barrier_PassesAfterDeactivate(t *testing.T) {
	ss := NewSessionState()

	// Activate and deactivate barrier
	ss.ActivateBarrier(1)
	ss.DeactivateBarrier()

	// Sequence validation should work normally again
	// Note: rxSequence is still at 1 from the pre-barrier test, but we use
	// a fresh SessionState so it starts at 0
	if !ss.ValidateRxSequence(0) {
		t.Error("ValidateRxSequence should pass after barrier deactivated")
	}
}

func TestSessionState_ValidateResetAck_MatchingID(t *testing.T) {
	ss := NewSessionState()

	const sessionID uint32 = 42
	ss.ActivateBarrier(sessionID)

	payload := make([]byte, SessionIDPayloadSize)
	binary.LittleEndian.PutUint32(payload, sessionID)

	if !ss.ValidateResetAck(payload) {
		t.Error("ValidateResetAck should accept matching session ID")
	}
}

func TestSessionState_ValidateResetAck_MismatchID(t *testing.T) {
	ss := NewSessionState()

	ss.ActivateBarrier(42)

	payload := make([]byte, SessionIDPayloadSize)
	binary.LittleEndian.PutUint32(payload, 99) // Wrong session ID

	if ss.ValidateResetAck(payload) {
		t.Error("ValidateResetAck should reject mismatched session ID")
	}
}

func TestSessionState_ValidateResetAck_ShortPayload(t *testing.T) {
	ss := NewSessionState()

	ss.ActivateBarrier(1)

	// Empty payload
	if ss.ValidateResetAck([]byte{}) {
		t.Error("ValidateResetAck should reject empty payload")
	}

	// Too short (2 bytes instead of 4)
	if ss.ValidateResetAck([]byte{0x00, 0x01}) {
		t.Error("ValidateResetAck should reject 2-byte payload")
	}
}

func TestSessionState_ValidateResetAck_BarrierInactive(t *testing.T) {
	ss := NewSessionState()

	// Barrier not active
	payload := make([]byte, SessionIDPayloadSize)
	binary.LittleEndian.PutUint32(payload, 1)

	if ss.ValidateResetAck(payload) {
		t.Error("ValidateResetAck should reject when barrier is inactive")
	}
}

func TestSessionState_ValidateResetAck_LongerPayload(t *testing.T) {
	ss := NewSessionState()

	const sessionID uint32 = 7
	ss.ActivateBarrier(sessionID)

	// Payload longer than 4 bytes should still work (reads first 4)
	payload := make([]byte, 8)
	binary.LittleEndian.PutUint32(payload, sessionID)
	payload[4] = 0xFF // Extra bytes

	if !ss.ValidateResetAck(payload) {
		t.Error("ValidateResetAck should accept payload >= 4 bytes with matching ID")
	}
}
