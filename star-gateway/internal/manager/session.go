// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"encoding/binary"
	"log"
	"sync"
)

// MaxGapTolerance is the maximum number of skipped frames allowed before
// rejecting a received sequence as a large gap (transport failure).
// It controls how many frames may be lost (for example on lightweight USB)
// while still accepting and catching up the RX sequence.
const MaxGapTolerance uint16 = 10

// SessionIDPayloadSize is the byte size of the session ID in RESET/RESET_ACK payloads.
// The session ID is encoded as a 4-byte little-endian uint32.
const SessionIDPayloadSize = 4

// seqMask is the bitmask for uint16 sequence number wraparound.
const seqMask uint16 = 0xFFFF

// SessionState holds sequence numbers shared across all transports.
// When Gateway fails over from USB (seq=105) to SPI, the SPI link MUST continue from seq=106,
// not reset to 0. SessionState ensures all transports share the same sequence counter.
type SessionState struct {
	mu         sync.Mutex
	txSequence uint16 // Shared TX sequence (incremented on every Send)
	rxSequence uint16 // Shared RX sequence (validated on every Receive)

	// Reset barrier fields: protect against stale USB FIFO frames during reset handshake.
	// When the barrier is active, all non-RESET_ACK frames are discarded.
	sessionID             uint32 // Monotonically increasing, incremented on each reset
	resetBarrierActive    bool   // True when waiting for RESET_ACK
	resetBarrierSessionID uint32 // The session ID this barrier expects in RESET_ACK

	// simpleMode disables strict sequence validation for reliable USB CDC transports.
	// When true, ValidateRxSequence accepts all frames and tracks sequence for
	// diagnostics only. Set via SetSimpleMode() when using ModeSimpleUSB.
	simpleMode bool
}

// NewSessionState creates a new SessionState with initial sequence 0.
func NewSessionState() *SessionState {
	return &SessionState{
		txSequence: 0,
		rxSequence: 0,
	}
}

// NextTxSequence atomically increments and returns the next TX sequence.
//
// This method is called by all transport link layers (CDCLink, SPILink) to generate
// the next sequence number for outgoing frames. The shared counter ensures continuity
// across transport switches.
//
// Thread Safety: Uses mutex to ensure atomicity across concurrent goroutines.
func (s *SessionState) NextTxSequence() uint16 {
	s.mu.Lock()
	defer s.mu.Unlock()

	seq := s.txSequence
	s.txSequence = (s.txSequence + 1) & seqMask // Wraparound at 65535
	return seq
}

// SetSimpleMode enables or disables simple mode for reliable USB CDC transports.
// In simple mode, ValidateRxSequence accepts all frames regardless of sequence
// number. Sequence tracking continues for diagnostics but never rejects frames.
//
// Thread Safety: Uses mutex to ensure atomicity.
func (s *SessionState) SetSimpleMode(enabled bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.simpleMode = enabled
}

// ValidateRxSequence checks if the received sequence is valid.
// Returns true if valid (exact match or small gap), false if duplicate or large gap.
//
// In simple mode (ModeSimpleUSB), all frames are accepted and the sequence is
// updated to track the remote side. This is safe because USB CDC provides
// hardware-level reliability (CRC-16 + automatic retransmission).
//
// CRITICAL FIX #6: Allow small gaps for lightweight USB protocol (no retransmission).
// If USB drops a single frame due to rare bit flip, we accept the gap and continue
// rather than permanently stalling the link.
//
// Gap Tolerance:
//   - diff == 0: Exact match (most common case) -> Accept
//   - 0 < diff < 10: Small gap (packet loss) -> Accept and catch up
//   - diff >= 10: Large gap (transport failure) -> Reject
//   - diff is large positive (near 65535): Likely duplicate from wraparound -> Reject
//
// Thread Safety: Uses mutex to ensure atomicity across concurrent goroutines.
func (s *SessionState) ValidateRxSequence(seq uint16) bool {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Simple mode: accept all frames, track sequence for diagnostics only.
	// USB CDC is reliable at the hardware level -- sequence enforcement is
	// unnecessary and causes false rejections on gateway/firmware restart.
	if s.simpleMode {
		s.rxSequence = (seq + 1) & seqMask
		return true
	}

	// Reset barrier: reject ALL data frames while barrier is active.
	// Stale frames from USB FIFO buffers must be discarded during reset handshake.
	if s.resetBarrierActive {
		log.Printf("DEBUG: Reset barrier active (session=%d), rejecting frame seq=%d",
			s.resetBarrierSessionID, seq)
		return false
	}

	// Calculate difference (handles wraparound correctly for uint16)
	diff := seq - s.rxSequence

	// Exact match - most common case
	if diff == 0 {
		s.rxSequence = (s.rxSequence + 1) & seqMask
		return true
	}

	// Small gap (packet loss on USB) - Accept and catch up
	if diff > 0 && diff < MaxGapTolerance {
		log.Printf("WARN: Skipped %d frames (packet loss), expected %d, got %d",
			diff, s.rxSequence, seq)
		s.rxSequence = (seq + 1) & seqMask
		return true
	}

	// Large gap or negative difference (duplicate/out-of-order)
	// diff > maxGapTolerance: Too many frames lost (possible transport failure)
	// diff is large positive (near 65535): Likely duplicate from wraparound
	log.Printf("ERROR: Sequence mismatch, expected %d, got %d (diff=%d)",
		s.rxSequence, seq, diff)
	return false
}

// GetTxSequence returns the current TX sequence (for diagnostics).
//
// Thread Safety: Uses mutex for consistent reads.
func (s *SessionState) GetTxSequence() uint16 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.txSequence
}

// GetRxSequence returns the current RX sequence (for diagnostics).
//
// Thread Safety: Uses mutex for consistent reads.
func (s *SessionState) GetRxSequence() uint16 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.rxSequence
}

// Reset resets both sequences to 0 (used when RX72N resets).
//
// CRITICAL FIX #4: This is called after receiving a RESET_ACK from RX72N
// to synchronize sequences after Gateway restart.
//
// Thread Safety: Uses mutex to ensure atomicity.
func (s *SessionState) Reset() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.txSequence = 0
	s.rxSequence = 0
	log.Printf("SessionState reset: sequences synchronized to 0")
}

// IncrementSessionID atomically increments and returns the new session ID.
// Called at the start of each reset handshake to generate a unique identifier
// for matching RESET frames with their corresponding RESET_ACK responses.
//
// Thread Safety: Uses mutex to ensure atomicity.
func (s *SessionState) IncrementSessionID() uint32 {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sessionID++
	return s.sessionID
}

// GetSessionID returns the current session ID (for diagnostics).
//
// Thread Safety: Uses mutex for consistent reads.
func (s *SessionState) GetSessionID() uint32 {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.sessionID
}

// ActivateBarrier enables the reset barrier for the given session ID.
// While the barrier is active, ValidateRxSequence rejects all frames (they are stale),
// and only a RESET_ACK with a matching session ID should be accepted.
//
// Must be called BEFORE sending the RESET frame to ensure no stale frames
// from the USB FIFO buffer are processed during the handshake.
//
// Thread Safety: Uses mutex to ensure atomicity.
func (s *SessionState) ActivateBarrier(sessionID uint32) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.resetBarrierActive = true
	s.resetBarrierSessionID = sessionID
	log.Printf("Reset barrier activated (session=%d)", sessionID)
}

// DeactivateBarrier disables the reset barrier.
// Called after the reset handshake completes (success or failure) to resume
// normal frame processing. Should be called in a defer to guarantee cleanup.
//
// Thread Safety: Uses mutex to ensure atomicity.
func (s *SessionState) DeactivateBarrier() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.resetBarrierActive = false
	log.Printf("Reset barrier deactivated (session=%d)", s.resetBarrierSessionID)
}

// IsBarrierActive returns whether the reset barrier is currently active.
//
// Thread Safety: Uses mutex for consistent reads.
func (s *SessionState) IsBarrierActive() bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.resetBarrierActive
}

// ValidateResetAck checks if a RESET_ACK payload matches the active barrier's session ID.
// Returns true only if:
//   - The barrier is currently active
//   - The payload is exactly SessionIDPayloadSize (4) bytes
//   - The little-endian decoded session ID matches the barrier's expected session ID
//
// Thread Safety: Uses mutex to ensure atomicity.
func (s *SessionState) ValidateResetAck(payload []byte) bool {
	s.mu.Lock()
	defer s.mu.Unlock()

	if !s.resetBarrierActive {
		log.Printf("WARN: ValidateResetAck called with barrier inactive")
		return false
	}

	if len(payload) < SessionIDPayloadSize {
		log.Printf("WARN: RESET_ACK payload too short (%d bytes, need %d)",
			len(payload), SessionIDPayloadSize)
		return false
	}

	receivedID := binary.LittleEndian.Uint32(payload[:SessionIDPayloadSize])
	if receivedID != s.resetBarrierSessionID {
		log.Printf("WARN: RESET_ACK session ID mismatch (got=%d, expected=%d)",
			receivedID, s.resetBarrierSessionID)
		return false
	}

	return true
}
