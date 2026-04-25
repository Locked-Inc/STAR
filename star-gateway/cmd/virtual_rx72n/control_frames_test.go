// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package main

import (
	"encoding/binary"
	"os"
	"strconv"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
)

// Test-only constants for control frame tests.
const (
	testPingCounter    uint32 = 42
	testAltPingCounter uint32 = 12345
	testSeqNumber      uint16 = 100
	testZeroLatency           = 0 * time.Millisecond
	testSmallLatency          = 5 * time.Millisecond
)

// newTestPingFrame creates a PING frame with the given counter for testing.
func newTestPingFrame(t *testing.T, counter uint32, seq uint16) *frame.Frame {
	t.Helper()
	payload := make([]byte, PingPayloadSize)
	binary.LittleEndian.PutUint32(payload, counter)
	f, err := frame.NewFrame(frame.FrameTypePing, payload)
	if err != nil {
		t.Fatalf("failed to create PING frame: %v", err)
	}
	f.Header.Sequence = seq
	return f
}

// newTestResetFrame creates a RESET frame for testing.
// If sessionID > 0, the payload carries the session ID as 4-byte little-endian.
// If sessionID == 0, the payload is empty.
func newTestResetFrame(t *testing.T, seq uint16, sessionID ...uint32) *frame.Frame {
	t.Helper()
	var payload []byte
	if len(sessionID) > 0 {
		payload = make([]byte, sessionIDPayloadSize)
		binary.LittleEndian.PutUint32(payload, sessionID[0])
	} else {
		payload = []byte{}
	}
	f, err := frame.NewFrame(frame.FrameTypeReset, payload)
	if err != nil {
		t.Fatalf("failed to create RESET frame: %v", err)
	}
	f.Header.Sequence = seq
	return f
}

func TestHandleFrame_PingPong(t *testing.T) {
	tests := []struct {
		name            string
		counter         uint32
		seq             uint16
		wantPongCounter uint32
	}{
		{"basic_ping", testPingCounter, 0, testPingCounter},
		{"large_counter", testAltPingCounter, testSeqNumber, testAltPingCounter},
		{"zero_counter", 0, 0, 0},
		{"max_counter", 0xFFFFFFFF, 0, 0xFFFFFFFF},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			session := &simulatorSession{}
			pingFrame := newTestPingFrame(t, tc.counter, tc.seq)

			resp := handleFrame(pingFrame, session, testZeroLatency)

			if len(resp.frames) != 1 {
				t.Fatalf("expected 1 response frame, got %d", len(resp.frames))
			}

			pong := resp.frames[0]
			if pong.Type != frame.FrameTypePong {
				t.Errorf("response type = %s, want PONG", pong.Type.String())
			}

			if len(pong.Payload) != PingPayloadSize {
				t.Fatalf("PONG payload size = %d, want %d", len(pong.Payload), PingPayloadSize)
			}

			gotCounter := binary.LittleEndian.Uint32(pong.Payload)
			if gotCounter != tc.wantPongCounter {
				t.Errorf("PONG counter = %d, want %d", gotCounter, tc.wantPongCounter)
			}
		})
	}
}

func TestHandleFrame_PingInvalidPayload(t *testing.T) {
	tests := []struct {
		name    string
		payload []byte
	}{
		{"empty_payload", []byte{}},
		{"short_payload", []byte{0x01, 0x02}},
		{"long_payload", []byte{0x01, 0x02, 0x03, 0x04, 0x05}},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			session := &simulatorSession{}
			f, err := frame.NewFrame(frame.FrameTypePing, tc.payload)
			if err != nil {
				t.Fatalf("failed to create frame: %v", err)
			}

			resp := handleFrame(f, session, testZeroLatency)

			if len(resp.frames) != 0 {
				t.Errorf("expected 0 response frames for invalid PING, got %d", len(resp.frames))
			}
		})
	}
}

func TestHandleFrame_ResetResetAck(t *testing.T) {
	session := &simulatorSession{}

	// Advance the session sequence to a non-zero value
	const advanceCount = 5
	for i := 0; i < advanceCount; i++ {
		session.nextSeq()
	}
	if session.txSeq != advanceCount {
		t.Fatalf("pre-condition: txSeq = %d, want %d", session.txSeq, advanceCount)
	}

	const testSessionID uint32 = 7

	// Send RESET with session ID
	resetFrame := newTestResetFrame(t, testSeqNumber, testSessionID)
	resp := handleFrame(resetFrame, session, testZeroLatency)

	if len(resp.frames) != 1 {
		t.Fatalf("expected 1 response frame, got %d", len(resp.frames))
	}

	resetAck := resp.frames[0]
	if resetAck.Type != frame.FrameTypeResetAck {
		t.Errorf("response type = %s, want RESET_ACK", resetAck.Type.String())
	}

	// RESET_ACK should echo the session ID (4-byte payload)
	if len(resetAck.Payload) != sessionIDPayloadSize {
		t.Fatalf("RESET_ACK payload len = %d, want %d", len(resetAck.Payload), sessionIDPayloadSize)
	}
	gotSessionID := binary.LittleEndian.Uint32(resetAck.Payload)
	if gotSessionID != testSessionID {
		t.Errorf("RESET_ACK session ID = %d, want %d", gotSessionID, testSessionID)
	}

	// RESET_ACK uses sequence from before reset (advanceCount)
	if resetAck.Header.Sequence != advanceCount {
		t.Errorf("RESET_ACK sequence = %d, want %d", resetAck.Header.Sequence, advanceCount)
	}

	// After RESET, session should be at 0
	if session.txSeq != 0 {
		t.Errorf("post-reset txSeq = %d, want 0", session.txSeq)
	}
}

func TestHandleFrame_ResetEmptyPayload(t *testing.T) {
	session := &simulatorSession{}

	// Send RESET with empty payload
	resetFrame := newTestResetFrame(t, 0)
	resp := handleFrame(resetFrame, session, testZeroLatency)

	if len(resp.frames) != 1 {
		t.Fatalf("expected 1 response frame, got %d", len(resp.frames))
	}

	resetAck := resp.frames[0]
	if resetAck.Type != frame.FrameTypeResetAck {
		t.Errorf("response type = %s, want RESET_ACK", resetAck.Type.String())
	}

	// Empty payload RESET should produce empty payload RESET_ACK
	if len(resetAck.Payload) != 0 {
		t.Errorf("RESET_ACK payload len = %d, want 0", len(resetAck.Payload))
	}

	// After RESET, session should be at 0
	if session.txSeq != 0 {
		t.Errorf("post-reset txSeq = %d, want 0", session.txSeq)
	}
}

func TestHandleFrame_ResetSessionIDEcho(t *testing.T) {
	tests := []struct {
		name      string
		sessionID uint32
	}{
		{"session_1", 1},
		{"session_42", 42},
		{"session_max", 0xFFFFFFFF},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			session := &simulatorSession{}
			resetFrame := newTestResetFrame(t, 0, tc.sessionID)
			resp := handleFrame(resetFrame, session, testZeroLatency)

			if len(resp.frames) != 1 {
				t.Fatalf("expected 1 response frame, got %d", len(resp.frames))
			}

			resetAck := resp.frames[0]
			if len(resetAck.Payload) != sessionIDPayloadSize {
				t.Fatalf("RESET_ACK payload len = %d, want %d", len(resetAck.Payload), sessionIDPayloadSize)
			}

			gotID := binary.LittleEndian.Uint32(resetAck.Payload)
			if gotID != tc.sessionID {
				t.Errorf("RESET_ACK session ID = %d, want %d", gotID, tc.sessionID)
			}
		})
	}
}

func TestHandleFrame_SequenceContinuity(t *testing.T) {
	session := &simulatorSession{}

	// Frame 1: PING (should use seq=0)
	pingFrame := newTestPingFrame(t, testPingCounter, 0)
	resp1 := handleFrame(pingFrame, session, testZeroLatency)
	if len(resp1.frames) != 1 {
		t.Fatalf("PING: expected 1 frame, got %d", len(resp1.frames))
	}
	if resp1.frames[0].Header.Sequence != 0 {
		t.Errorf("PING response seq = %d, want 0", resp1.frames[0].Header.Sequence)
	}

	// Frame 2: Another PING (should use seq=1)
	pingFrame2 := newTestPingFrame(t, testAltPingCounter, 1)
	resp2 := handleFrame(pingFrame2, session, testZeroLatency)
	if len(resp2.frames) != 1 {
		t.Fatalf("PING2: expected 1 frame, got %d", len(resp2.frames))
	}
	if resp2.frames[0].Header.Sequence != 1 {
		t.Errorf("PING2 response seq = %d, want 1", resp2.frames[0].Header.Sequence)
	}

	// Frame 3: RESET (RESET_ACK uses current seq=2, then resets to 0)
	resetFrame := newTestResetFrame(t, 2)
	resp3 := handleFrame(resetFrame, session, testZeroLatency)
	if len(resp3.frames) != 1 {
		t.Fatalf("RESET: expected 1 frame, got %d", len(resp3.frames))
	}
	if resp3.frames[0].Header.Sequence != 2 {
		t.Errorf("RESET_ACK seq = %d, want 2", resp3.frames[0].Header.Sequence)
	}

	// Frame 4: PING after RESET (should use seq=0 since session was reset)
	pingFrame3 := newTestPingFrame(t, testPingCounter, 3)
	resp4 := handleFrame(pingFrame3, session, testZeroLatency)
	if len(resp4.frames) != 1 {
		t.Fatalf("PING3: expected 1 frame, got %d", len(resp4.frames))
	}
	if resp4.frames[0].Header.Sequence != 0 {
		t.Errorf("PING3 response seq = %d, want 0", resp4.frames[0].Header.Sequence)
	}
}

func TestHandleFrame_PingWithAckFlag(t *testing.T) {
	session := &simulatorSession{}
	pingFrame := newTestPingFrame(t, testPingCounter, testSeqNumber)
	pingFrame.Header.Flags = frame.FlagRequiresAck

	resp := handleFrame(pingFrame, session, testZeroLatency)

	// Should get ACK + PONG (2 frames)
	if len(resp.frames) != 2 {
		t.Fatalf("expected 2 response frames, got %d", len(resp.frames))
	}

	// First frame: ACK
	if resp.frames[0].Type != frame.FrameTypeAck {
		t.Errorf("first frame type = %s, want ACK", resp.frames[0].Type.String())
	}
	if resp.frames[0].Header.Sequence != testSeqNumber {
		t.Errorf("ACK sequence = %d, want %d", resp.frames[0].Header.Sequence, testSeqNumber)
	}

	// Second frame: PONG
	if resp.frames[1].Type != frame.FrameTypePong {
		t.Errorf("second frame type = %s, want PONG", resp.frames[1].Type.String())
	}

	// PONG should use the initial session sequence (0)
	if resp.frames[1].Header.Sequence != 0 {
		t.Errorf("PONG sequence = %d, want 0", resp.frames[1].Header.Sequence)
	}

	gotCounter := binary.LittleEndian.Uint32(resp.frames[1].Payload)
	if gotCounter != testPingCounter {
		t.Errorf("PONG counter = %d, want %d", gotCounter, testPingCounter)
	}
}

func TestHandleFrame_LatencySimulation(t *testing.T) {
	tests := []struct {
		name       string
		frameFunc  func(*testing.T) *frame.Frame
		latency    time.Duration
		minElapsed time.Duration
	}{
		{
			name: "ping_with_latency",
			frameFunc: func(t *testing.T) *frame.Frame {
				return newTestPingFrame(t, testPingCounter, 0)
			},
			latency:    testSmallLatency,
			minElapsed: testSmallLatency,
		},
		{
			name: "reset_with_latency",
			frameFunc: func(t *testing.T) *frame.Frame {
				return newTestResetFrame(t, 0)
			},
			latency:    testSmallLatency,
			minElapsed: testSmallLatency,
		},
		{
			name: "ping_zero_latency",
			frameFunc: func(t *testing.T) *frame.Frame {
				return newTestPingFrame(t, testPingCounter, 0)
			},
			latency:    testZeroLatency,
			minElapsed: testZeroLatency,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			session := &simulatorSession{}
			f := tc.frameFunc(t)

			start := time.Now()
			resp := handleFrame(f, session, tc.latency)
			elapsed := time.Since(start)

			if len(resp.frames) == 0 {
				t.Fatal("expected at least 1 response frame")
			}

			if elapsed < tc.minElapsed {
				t.Errorf("elapsed %v < minimum %v (latency not applied)", elapsed, tc.minElapsed)
			}
		})
	}
}

func TestParseSimLatency(t *testing.T) {
	tests := []struct {
		name     string
		envValue string
		setEnv   bool
		want     time.Duration
	}{
		{"default_unset", "", false, time.Duration(DefaultSimLatencyMs) * time.Millisecond},
		{"zero", "0", true, 0},
		{"custom_50", "50", true, 50 * time.Millisecond},
		{"max_valid", strconv.Itoa(MaxSimLatencyMs), true, time.Duration(MaxSimLatencyMs) * time.Millisecond},
		{"invalid_negative", "-1", true, time.Duration(DefaultSimLatencyMs) * time.Millisecond},
		{"invalid_over_max", strconv.Itoa(MaxSimLatencyMs + 1), true, time.Duration(DefaultSimLatencyMs) * time.Millisecond},
		{"invalid_string", "abc", true, time.Duration(DefaultSimLatencyMs) * time.Millisecond},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.setEnv {
				t.Setenv(EnvSimLatencyMs, tc.envValue)
			} else {
				// Explicitly unset environment variable to ensure deterministic test
				os.Unsetenv(EnvSimLatencyMs)
				t.Cleanup(func() {
					// Cleanup will restore original env after test
				})
			}

			got := parseSimLatency()
			if got != tc.want {
				t.Errorf("parseSimLatency() = %v, want %v", got, tc.want)
			}
		})
	}
}

func TestSimulatorSession(t *testing.T) {
	t.Run("nextSeq_increments", func(t *testing.T) {
		session := &simulatorSession{}
		if seq := session.nextSeq(); seq != 0 {
			t.Errorf("first nextSeq = %d, want 0", seq)
		}
		if seq := session.nextSeq(); seq != 1 {
			t.Errorf("second nextSeq = %d, want 1", seq)
		}
	})

	t.Run("reset_clears_sequence", func(t *testing.T) {
		session := &simulatorSession{}
		session.nextSeq()
		session.nextSeq()
		session.reset()
		if seq := session.nextSeq(); seq != 0 {
			t.Errorf("nextSeq after reset = %d, want 0", seq)
		}
	})

	t.Run("wraparound", func(t *testing.T) {
		session := &simulatorSession{txSeq: SeqMask} // 0xFFFF
		seq := session.nextSeq()
		if seq != SeqMask {
			t.Errorf("pre-wrap seq = %d, want %d", seq, uint16(SeqMask))
		}
		seq = session.nextSeq()
		if seq != 0 {
			t.Errorf("post-wrap seq = %d, want 0", seq)
		}
	})
}
