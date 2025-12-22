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

	t.Run("MaxRetries", func(t *testing.T) {
		if config.MaxRetries != DefaultMaxRetries {
			t.Errorf("MaxRetries = %d, want %d", config.MaxRetries, DefaultMaxRetries)
		}
	})

	t.Run("Timeout", func(t *testing.T) {
		if config.Timeout != DefaultTimeout {
			t.Errorf("Timeout = %v, want %v", config.Timeout, DefaultTimeout)
		}
	})
}

// ============================================================================
// StopAndWait Tests
// ============================================================================

func TestNewStopAndWait(t *testing.T) {
	tests := []struct {
		name              string
		config            *Config
		expectedRetries   int
		expectedTimeout   time.Duration
	}{
		{
			name:            "nil_config_uses_defaults",
			config:          nil,
			expectedRetries: DefaultMaxRetries,
			expectedTimeout: DefaultTimeout,
		},
		{
			name: "custom_config",
			config: &Config{
				MaxRetries: 5,
				Timeout:    20 * time.Millisecond,
			},
			expectedRetries: 5,
			expectedTimeout: 20 * time.Millisecond,
		},
		{
			name: "zero_retries",
			config: &Config{
				MaxRetries: 0,
				Timeout:    5 * time.Millisecond,
			},
			expectedRetries: 0,
			expectedTimeout: 5 * time.Millisecond,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			arq := NewStopAndWait(tc.config)

			if arq == nil {
				t.Fatal("expected ARQ, got nil")
			}
			if arq.Config().MaxRetries != tc.expectedRetries {
				t.Errorf("MaxRetries = %d, want %d", arq.Config().MaxRetries, tc.expectedRetries)
			}
			if arq.Config().Timeout != tc.expectedTimeout {
				t.Errorf("Timeout = %v, want %v", arq.Config().Timeout, tc.expectedTimeout)
			}
		})
	}
}

func TestStopAndWaitInitialState(t *testing.T) {
	arq := NewStopAndWait(nil)

	t.Run("State", func(t *testing.T) {
		if arq.GetState() != StateIdle {
			t.Errorf("GetState() = %v, want StateIdle", arq.GetState())
		}
	})

	t.Run("TxSequence", func(t *testing.T) {
		if arq.GetTxSequence() != 0 {
			t.Errorf("GetTxSequence() = %d, want 0", arq.GetTxSequence())
		}
	})

	t.Run("RxSequence", func(t *testing.T) {
		if arq.GetRxSequence() != 0 {
			t.Errorf("GetRxSequence() = %d, want 0", arq.GetRxSequence())
		}
	})
}

func TestStopAndWaitReset(t *testing.T) {
	tests := []struct {
		name       string
		setupState State
		setupTxSeq uint16
		setupRxSeq uint16
		setupRetry int
	}{
		{"from_error_state", StateError, 100, 50, 3},
		{"from_waiting_ack", StateWaitingAck, 255, 128, 1},
		{"from_retransmitting", StateRetransmitting, 1000, 999, 2},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			arq := NewStopAndWait(nil)
			arq.SetStateForTesting(tc.setupState, tc.setupTxSeq, tc.setupRxSeq, tc.setupRetry)

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
		})
	}
}

func TestStopAndWaitNotImplemented(t *testing.T) {
	tests := []struct {
		name      string
		operation string
	}{
		{"Send", "send"},
		{"Receive", "receive"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			arq := NewStopAndWait(nil)

			var err error
			switch tc.operation {
			case "send":
				err = arq.Send([]byte{0x01, 0x02, 0x03})
			case "receive":
				_, err = arq.Receive()
			}

			if err != ErrNotImplemented {
				t.Errorf("%s() error = %v, want ErrNotImplemented", tc.name, err)
			}
		})
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
		{"large_value", 10000, 10001},
		{"before_max", DefaultSequenceMax - 1, DefaultSequenceMax},
		{"wraparound_at_max", DefaultSequenceMax, 0},
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
