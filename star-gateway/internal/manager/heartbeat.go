// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"encoding/binary"
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
)

const (
	// DefaultPingInterval is the interval for sending explicit PING when the link is idle.
	// This is a rare idle-link probe; the implicit timeout (DefaultFailureTimeout)
	// is the primary detection mechanism — any valid frame resets that timer.
	DefaultPingInterval = 1 * time.Second

	// DefaultFailureTimeout is the timeout for declaring link dead.
	// Any valid frame (data or control) resets this timer via OnFrameReceived().
	DefaultFailureTimeout = 200 * time.Millisecond

	// pingPayloadSize is the size of PING frame payload (4-byte counter).
	pingPayloadSize = 4

	// pingSendTimeout is the timeout for sending a PING frame.
	pingSendTimeout = 100 * time.Millisecond

	// checkIntervalDivisor determines how often the heartbeat loop checks for timeouts.
	// With failureTimeout/4, worst-case detection latency is failureTimeout + one check interval.
	// For 200ms timeout: check every 50ms, worst-case detection ~250ms (well within 1000ms WDT).
	checkIntervalDivisor = 4

	// minCheckInterval prevents excessively fast polling for very small failureTimeout values.
	minCheckInterval = 10 * time.Millisecond
)

// WDT Timing Coordination
//
// The RX72N hardware Watchdog Timer (WDT) has an approximate timeout of 1 second.
// Our software heartbeat MUST detect failure significantly faster to prevent
// unintended hardware reboots.
//
// Timing budget (dual detection strategy):
//
//	WDT timeout:       ~1000ms (hardware, configured in RX72N firmware)
//	Implicit timeout:    200ms (any frame resets timer via OnFrameReceived)
//	Check interval:       50ms (failureTimeout / 4, for timely detection)
//	PING interval:      1000ms (1 Hz, idle-link probe — rare when data is flowing)
//	Miss threshold:    1 PING  (single unanswered PING triggers failover)
//	Worst-case detect:  ~250ms (implicit timeout + one check interval)
//	Safety margin:        ~4x  (250ms << 1000ms WDT)
//
// How it works:
//
//	Active link: Data frames flow constantly, resetting the implicit timer.
//	             PING never fires because the link is never idle for 1s.
//	Idle link:   After 200ms of silence, implicit timeout triggers failover.
//	             PING at 1s is a secondary probe for very-slow-data links.
//
// WARNING: Do NOT increase DefaultFailureTimeout above 500ms without first
// verifying the RX72N WDT timeout. The implicit timeout is the critical path.
//
// To verify RX72N WDT timeout:
//  1. Check star-rx72n-firmware WDT configuration registers (WDTCR)
//  2. WDT clock source and divider determine actual timeout
//  3. Typical: PCLKB/8192 with 16-bit counter ~ 1.09s at 60MHz PCLKB
const (
	// DefaultConsecutiveMissThreshold is the number of consecutive unanswered PINGs
	// before triggering failover via the explicit (counter-based) path.
	// With 1s PING interval, the implicit timeout (200ms) fires first in practice.
	DefaultConsecutiveMissThreshold = 1

	// wdtTimeoutApproxMs documents the approximate RX72N WDT timeout in milliseconds.
	wdtTimeoutApproxMs = 1000

	// wdtSafetyFactor is the minimum ratio of WDT timeout to Gateway failure detection.
	// With DefaultFailureTimeout=200ms and checkInterval=50ms: worst-case ~250ms.
	// 1000ms / 200ms = 5 (best case), 1000ms / 250ms = 4 (worst case).
	// We use the best-case ratio (failureTimeout-based) since the check interval
	// only adds one extra period of delay.
	wdtSafetyFactor = 5
)

// HeartbeatManager implements hybrid implicit/explicit connectivity detection.
//
// Hybrid Detection Strategy:
//   - Implicit: Updates LastSeen on ANY valid frame (telemetry, commands, ACKs)
//   - Explicit: Sends PING when idle >1s (rare idle-link probe)
//   - Failure: Declares link dead if no frames >200ms (implicit timeout fires first)
//
// Benefits:
//   - Minimal bandwidth overhead (only PING when idle)
//   - Fast failure detection (200ms vs 5s polling)
//   - No false positives from buffered OS data (counter prevents replay)
//
// Thread Safety: All methods use mutex protection for concurrent access.
type HeartbeatManager struct {
	mu               sync.Mutex
	lastSeen         time.Time
	pingCounter      uint32
	failureTriggered bool          // Prevents repeated failover triggers
	pingInterval     time.Duration // 1s - send PING if idle
	failureTimeout   time.Duration // 200ms - declare link dead
	onLinkFailed     func()        // Callback to trigger failover

	// Explicit PONG validation state.
	// These fields separate "any frame arrived" (implicit) from
	// "correct PONG arrived" (explicit heartbeat success).
	lastValidPong     time.Time // Timestamp of last PONG with matching counter
	lastPingSent      uint32    // Counter value of most recently sent PING
	pendingPing       bool      // True after PING sent, false after valid PONG
	consecutiveMisses uint32    // Unanswered PINGs; reset on valid PONG match
}

// NewHeartbeatManager creates a new heartbeat manager with the specified timeouts.
//
// Parameters:
//   - pingInterval: How long the link must be idle before sending explicit PING (recommended 1s)
//   - failureTimeout: How long without any frames before declaring failure (recommended 200ms)
//   - onFailure: Callback function to invoke when link is declared dead
//
// Returns an error if:
//   - pingInterval or failureTimeout is <= 0
//   - onFailure is nil (required for triggering failover)
//
// Note: pingInterval may be greater than failureTimeout. In the dual-detection model,
// the implicit timeout (failureTimeout) is the primary detection mechanism — any valid
// frame resets the timer. PING is a rare idle-link probe that fires only when the link
// has been silent for pingInterval.
func NewHeartbeatManager(pingInterval, failureTimeout time.Duration, onFailure func()) (*HeartbeatManager, error) {
	// Validate inputs
	if pingInterval <= 0 {
		return nil, fmt.Errorf("pingInterval must be > 0, got %v", pingInterval)
	}
	if failureTimeout <= 0 {
		return nil, fmt.Errorf("failureTimeout must be > 0, got %v", failureTimeout)
	}
	if onFailure == nil {
		return nil, fmt.Errorf("onFailure callback cannot be nil")
	}

	now := time.Now()
	return &HeartbeatManager{
		lastSeen:       now,
		lastValidPong:  now,
		pingCounter:    0,
		pingInterval:   pingInterval,
		failureTimeout: failureTimeout,
		onLinkFailed:   onFailure,
	}, nil
}

// OnFrameReceived updates the LastSeen timestamp for implicit heartbeat detection.
//
// This method should be called by TransportManager.Receive() for valid non-PONG frames,
// including:
//   - PING frames (from remote side)
//   - Command frames
//   - Response frames
//
// PONG frames should use [OnPongReceived] for explicit counter validation.
//
// By updating on any frame, we avoid sending unnecessary PINGs when the link is
// actively transferring data. Also clears the failureTriggered flag to allow
// future failover if the link fails again.
//
// Thread Safety: Uses mutex to protect lastSeen timestamp and failureTriggered flag.
func (hm *HeartbeatManager) OnFrameReceived() {
	hm.mu.Lock()
	hm.lastSeen = time.Now()
	hm.failureTriggered = false // Clear failure flag (link recovered)
	hm.mu.Unlock()
}

// OnPongReceived validates a PONG payload against the last sent PING counter.
//
// This performs both implicit and explicit heartbeat detection:
//   - Implicit: Always updates lastSeen and clears failureTriggered (link is alive)
//   - Explicit: Validates the 4-byte counter matches lastPingSent
//
// On counter match:
//   - Resets consecutiveMisses to 0
//   - Updates lastValidPong timestamp
//   - Clears pendingPing flag
//
// On counter mismatch (stale/replayed PONG):
//   - Logs a warning with both counters
//   - Does NOT reset consecutiveMisses (link may be degraded)
//   - lastSeen is still updated (link is physically alive)
//
// Returns true if the counter matched, false otherwise.
//
// Thread Safety: Uses mutex to protect all state fields.
func (hm *HeartbeatManager) OnPongReceived(payload []byte) bool {
	hm.mu.Lock()
	defer hm.mu.Unlock()

	// Always update lastSeen for implicit activity detection
	hm.lastSeen = time.Now()
	hm.failureTriggered = false

	// Validate payload length
	if len(payload) != pingPayloadSize {
		log.Printf("WARN: PONG payload length %d, expected %d",
			len(payload), pingPayloadSize)
		return false
	}

	// Extract counter from PONG payload
	receivedCounter := binary.BigEndian.Uint32(payload)

	// Validate counter matches last sent PING
	if receivedCounter != hm.lastPingSent {
		log.Printf("WARN: PONG counter mismatch: received %d, expected %d (stale or replayed)",
			receivedCounter, hm.lastPingSent)
		return false
	}

	// Valid PONG: reset consecutive misses and update explicit heartbeat state
	hm.consecutiveMisses = 0
	hm.lastValidPong = time.Now()
	hm.pendingPing = false

	return true
}

// Run starts the heartbeat monitoring loop as a background goroutine.
//
// This method:
//  1. Periodically checks elapsed time since lastSeen (every failureTimeout/4)
//  2. Triggers failover if idle > failureTimeout (200ms)
//  3. Sends PING if idle > pingInterval (1s) and link not yet failed
//
// The check interval is derived from failureTimeout to ensure timely detection.
// With 200ms timeout and check every 50ms, worst-case detection is ~250ms.
//
// The loop runs until ctx is cancelled. It should be started as a goroutine:
//
//	go heartbeat.Run(ctx, transportManager)
//
// Thread Safety: Safe to call concurrently with OnFrameReceived().
func (hm *HeartbeatManager) Run(ctx context.Context, tm *TransportManager) {
	// Validate TransportManager is not nil (prevents panic in sendPing)
	if tm == nil {
		log.Printf("ERROR: HeartbeatManager.Run called with nil TransportManager, exiting")
		return
	}

	// Check at a fraction of failureTimeout for timely detection.
	// With failureTimeout=200ms and checkInterval=50ms, worst-case detection
	// is ~250ms, well within the ~1000ms WDT budget.
	checkInterval := hm.failureTimeout / checkIntervalDivisor
	if checkInterval < minCheckInterval {
		checkInterval = minCheckInterval
	}
	ticker := time.NewTicker(checkInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			hm.check(ctx, tm)
		}
	}
}

// check performs the heartbeat check logic.
//
// This is called periodically by Run() (every failureTimeout/4) to:
//  1. Calculate elapsed time since lastSeen
//  2. Trigger failover if elapsed > failureTimeout (200ms) - ONCE per failure
//  3. Track consecutive PONG misses and trigger failover at threshold
//  4. Send PING if elapsed > pingInterval (1s) and not yet failed
//
// Two independent failover triggers exist (defense-in-depth):
//   - Time-based: No frames at all for failureTimeout (200ms)
//   - Counter-based: DefaultConsecutiveMissThreshold unanswered PINGs
//
// The failureTriggered flag prevents repeated failover attempts for the same failure.
// It's cleared when OnFrameReceived() or OnPongReceived() is called (link recovered).
//
// The order matters: we check failure first to avoid sending PING on a dead link.
func (hm *HeartbeatManager) check(ctx context.Context, tm *TransportManager) {
	hm.mu.Lock()
	elapsed := time.Since(hm.lastSeen)
	alreadyTriggered := hm.failureTriggered
	pending := hm.pendingPing
	hm.mu.Unlock()

	// Time-based failure detection (highest priority)
	// Only trigger once per failure to prevent spamming attemptFailover()
	if elapsed > hm.failureTimeout && !alreadyTriggered {
		log.Printf("Heartbeat timeout (%v), triggering failover", elapsed)

		// Set flag BEFORE calling onLinkFailed to prevent race
		hm.mu.Lock()
		hm.failureTriggered = true
		hm.mu.Unlock()

		if hm.onLinkFailed != nil {
			hm.onLinkFailed()
		}
		return
	}

	// Explicit PING (only if idle but not yet failed)
	if elapsed > hm.pingInterval && !alreadyTriggered {
		// Before sending new PING, check if previous PING was answered
		if pending {
			hm.mu.Lock()
			hm.consecutiveMisses++
			currentMisses := hm.consecutiveMisses
			lastSent := hm.lastPingSent // capture under lock to avoid race
			hm.mu.Unlock()

			log.Printf("PONG miss: counter=%d unanswered, consecutive_misses=%d/%d",
				lastSent, currentMisses, DefaultConsecutiveMissThreshold)

			// Counter-based failure detection
			if currentMisses >= DefaultConsecutiveMissThreshold {
				log.Printf("Consecutive PONG misses (%d >= %d), triggering failover",
					currentMisses, DefaultConsecutiveMissThreshold)

				hm.mu.Lock()
				hm.failureTriggered = true
				hm.consecutiveMisses = 0 // reset to prevent re-trigger storm on recovery
				hm.pendingPing = false
				hm.mu.Unlock()

				if hm.onLinkFailed != nil {
					hm.onLinkFailed()
				}
				return
			}
		}

		hm.sendPing(ctx, tm)
	}
}

// sendPing sends a PING frame to the active transport.
//
// PING Frame Format:
//   - Type: FrameTypePing (0x00)
//   - Payload: 4-byte counter (big-endian uint32)
//   - Expected Response: PONG frame with same counter (validated by OnPongReceived)
//
// The counter enables replay detection: the RX72N echoes the counter in PONG,
// and OnPongReceived validates that it matches lastPingSent.
//
// After sending, pendingPing is set to true and lastPingSent records the counter.
// If OnPongReceived is not called before the next check(), the pending PING
// counts as a consecutive miss.
//
// Thread Safety: Increments pingCounter and sets pendingPing under mutex.
func (hm *HeartbeatManager) sendPing(ctx context.Context, tm *TransportManager) {
	hm.mu.Lock()
	counter := hm.pingCounter
	hm.pingCounter++
	hm.mu.Unlock()

	// Create PING payload (pingPayloadSize bytes, big-endian counter)
	payload := make([]byte, pingPayloadSize)
	binary.BigEndian.PutUint32(payload, counter)

	// Send PING via TransportManager with explicit frame type
	// Note: We don't wait for PONG here - OnPongReceived() will be called
	// when the PONG arrives via TransportManager.Receive()
	pingCtx, cancel := context.WithTimeout(ctx, pingSendTimeout)
	defer cancel()

	if err := tm.SendWithType(pingCtx, payload, frame.FrameTypePing); err != nil {
		log.Printf("PING send failed: %v (counter=%d)", err, counter)
		// Don't set pendingPing - failed sends should not count as misses
		return
	}

	// Only mark as pending after successful send
	hm.mu.Lock()
	hm.lastPingSent = counter
	hm.pendingPing = true
	hm.mu.Unlock()
}
