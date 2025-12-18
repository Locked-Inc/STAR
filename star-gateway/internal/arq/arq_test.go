// Package arq tests for ARQ protocol implementation.
//
// STAR Project - Texas A&M University
// December 2025
package arq

import (
	"testing"
	"time"
)

// ============================================================================
// Constant Tests
// ============================================================================

func TestARQConstants(t *testing.T) {
	t.Run("DefaultMaxRetries", func(t *testing.T) {
		if DefaultMaxRetries != 3 {
			t.Errorf("DefaultMaxRetries = %d, want 3", DefaultMaxRetries)
		}
	})

	t.Run("DefaultTimeout", func(t *testing.T) {
		if DefaultTimeout != 10*time.Millisecond {
			t.Errorf("DefaultTimeout = %v, want 10ms", DefaultTimeout)
		}
	})

	t.Run("DefaultSequenceMax", func(t *testing.T) {
		if DefaultSequenceMax != 0xFFFF {
			t.Errorf("DefaultSequenceMax = %d, want 65535", DefaultSequenceMax)
		}
	})
}

func TestStateString(t *testing.T) {
	tests := []struct {
		state    State
		expected string
	}{
		{StateIdle, "IDLE"},
		{StateWaitingAck, "WAITING_ACK"},
		{StateRetransmitting, "RETRANSMITTING"},
		{StateError, "ERROR"},
		{State(99), "UNKNOWN"},
	}

	for _, tc := range tests {
		t.Run(tc.expected, func(t *testing.T) {
			if got := tc.state.String(); got != tc.expected {
				t.Errorf("State.String() = %s, want %s", got, tc.expected)
			}
		})
	}
}

// ============================================================================
// Config Tests
// ============================================================================

func TestDefaultARQConfig(t *testing.T) {
	config := DefaultARQConfig()

	if config.MaxRetries != DefaultMaxRetries {
		t.Errorf("MaxRetries = %d, want %d", config.MaxRetries, DefaultMaxRetries)
	}
	if config.Timeout != DefaultTimeout {
		t.Errorf("Timeout = %v, want %v", config.Timeout, DefaultTimeout)
	}
}

// ============================================================================
// StopAndWait Tests
// ============================================================================

func TestNewStopAndWait(t *testing.T) {
	t.Run("with_nil_config", func(t *testing.T) {
		arq := NewStopAndWait(nil)
		if arq == nil {
			t.Fatal("expected ARQ, got nil")
		}
		if arq.Config().MaxRetries != DefaultMaxRetries {
			t.Errorf("MaxRetries = %d, want %d", arq.Config().MaxRetries, DefaultMaxRetries)
		}
	})

	t.Run("with_custom_config", func(t *testing.T) {
		customConfig := &Config{
			MaxRetries: 5,
			Timeout:    20 * time.Millisecond,
		}
		arq := NewStopAndWait(customConfig)
		if arq.Config().MaxRetries != 5 {
			t.Errorf("MaxRetries = %d, want 5", arq.Config().MaxRetries)
		}
		if arq.Config().Timeout != 20*time.Millisecond {
			t.Errorf("Timeout = %v, want 20ms", arq.Config().Timeout)
		}
	})
}

func TestStopAndWaitInitialState(t *testing.T) {
	arq := NewStopAndWait(nil)

	if arq.GetState() != StateIdle {
		t.Errorf("GetState() = %v, want StateIdle", arq.GetState())
	}
	if arq.GetTxSequence() != 0 {
		t.Errorf("GetTxSequence() = %d, want 0", arq.GetTxSequence())
	}
	if arq.GetRxSequence() != 0 {
		t.Errorf("GetRxSequence() = %d, want 0", arq.GetRxSequence())
	}
}

func TestStopAndWaitReset(t *testing.T) {
	arq := NewStopAndWait(nil)

	// Manually modify state (simulating usage)
	arq.state = StateError
	arq.txSequence = 100
	arq.rxSequence = 50
	arq.retryCount = 3

	arq.Reset()

	if arq.GetState() != StateIdle {
		t.Errorf("GetState() = %v, want StateIdle after Reset", arq.GetState())
	}
	if arq.GetTxSequence() != 0 {
		t.Errorf("GetTxSequence() = %d, want 0 after Reset", arq.GetTxSequence())
	}
	if arq.GetRxSequence() != 0 {
		t.Errorf("GetRxSequence() = %d, want 0 after Reset", arq.GetRxSequence())
	}
}

func TestStopAndWaitSendNotImplemented(t *testing.T) {
	arq := NewStopAndWait(nil)
	err := arq.Send([]byte{0x01, 0x02, 0x03})
	if err != ErrNotImplemented {
		t.Errorf("Send() error = %v, want ErrNotImplemented", err)
	}
}

func TestStopAndWaitReceiveNotImplemented(t *testing.T) {
	arq := NewStopAndWait(nil)
	_, err := arq.Receive()
	if err != ErrNotImplemented {
		t.Errorf("Receive() error = %v, want ErrNotImplemented", err)
	}
}

// ============================================================================
// Sequence Number Tests
// ============================================================================

func TestIncrementSequence(t *testing.T) {
	tests := []struct {
		name     string
		input    uint16
		expected uint16
	}{
		{"zero", 0, 1},
		{"normal", 100, 101},
		{"before_max", DefaultSequenceMax - 1, DefaultSequenceMax},
		{"wraparound", DefaultSequenceMax, 0},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := incrementSequence(tc.input)
			if got != tc.expected {
				t.Errorf("incrementSequence(%d) = %d, want %d", tc.input, got, tc.expected)
			}
		})
	}
}

// ============================================================================
// TODO: Implementation Tests
// ============================================================================

// TODO: Add ACK/NACK handling tests once implementation is complete.
// func TestStopAndWaitAckHandling(t *testing.T) {}

// TODO: Add retry logic tests once implementation is complete.
// func TestStopAndWaitRetry(t *testing.T) {}

// TODO: Add duplicate detection tests once implementation is complete.
// func TestStopAndWaitDuplicateDetection(t *testing.T) {}

// TODO: Add timeout behavior tests once implementation is complete.
// func TestStopAndWaitTimeout(t *testing.T) {}

// TODO: Add sequence wraparound tests once implementation is complete.
// func TestStopAndWaitSequenceWraparound(t *testing.T) {}
