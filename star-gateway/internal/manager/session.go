// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"log"
	"sync"
)

// SessionState holds sequence numbers shared across all transports.
//
// CRITICAL FIX #1: This prevents the "Handoff Problem" where switching transports causes
// sequence resets, leading to duplicate frame rejection by RX72N.
//
// When Gateway fails over from USB (seq=105) to SPI, the SPI link MUST continue from seq=106,
// not reset to 0. SessionState ensures all transports share the same sequence counter.
//
// Thread Safety: All methods use mutex protection for concurrent Send/Receive operations.
type SessionState struct {
	mu         sync.Mutex
	txSequence uint16 // Shared TX sequence (incremented on every Send)
	rxSequence uint16 // Shared RX sequence (validated on every Receive)
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
	s.txSequence = (s.txSequence + 1) & 0xFFFF // Wraparound at 65535
	return seq
}

// ValidateRxSequence checks if the received sequence is valid.
// Returns true if valid (exact match or small gap), false if duplicate or large gap.
//
// CRITICAL FIX #6: Allow small gaps for lightweight USB protocol (no retransmission).
// If USB drops a single frame due to rare bit flip, we accept the gap and continue
// rather than permanently stalling the link.
//
// Gap Tolerance:
//   - diff == 0: Exact match (most common case) → Accept
//   - 0 < diff < 10: Small gap (packet loss) → Accept and catch up
//   - diff >= 10: Large gap (transport failure) → Reject
//   - diff is large positive (near 65535): Likely duplicate from wraparound → Reject
//
// Thread Safety: Uses mutex to ensure atomicity across concurrent goroutines.
func (s *SessionState) ValidateRxSequence(seq uint16) bool {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Calculate difference (handles wraparound correctly for uint16)
	diff := seq - s.rxSequence

	// Exact match - most common case
	if diff == 0 {
		s.rxSequence = (s.rxSequence + 1) & 0xFFFF
		return true
	}

	// Small gap (packet loss on USB) - Accept and catch up
	// Allow gaps up to 10 frames (configurable)
	const maxGapTolerance = 10
	if diff > 0 && diff < maxGapTolerance {
		log.Printf("WARN: Skipped %d frames (packet loss), expected %d, got %d",
			diff, s.rxSequence, seq)
		s.rxSequence = (seq + 1) & 0xFFFF
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
