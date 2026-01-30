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
)

const (
	// DefaultPingInterval is the interval for sending PING when idle.
	DefaultPingInterval = 50 * time.Millisecond

	// DefaultFailureTimeout is the timeout for declaring link dead.
	DefaultFailureTimeout = 200 * time.Millisecond

	// pingPayloadSize is the size of PING frame payload (4-byte counter).
	pingPayloadSize = 4

	// pingSendTimeout is the timeout for sending a PING frame.
	pingSendTimeout = 100 * time.Millisecond
)

// HeartbeatManager implements hybrid implicit/explicit connectivity detection.
//
// Hybrid Detection Strategy:
//   - Implicit: Updates LastSeen on ANY valid frame (telemetry, commands, ACKs)
//   - Explicit: Sends PING when idle >50ms
//   - Failure: Declares link dead if no frames >200ms
//
// Benefits:
//   - Minimal bandwidth overhead (only PING when idle)
//   - Fast failure detection (200ms vs 5s polling)
//   - No false positives from buffered OS data (counter prevents replay)
//
// Thread Safety: All methods use mutex protection for concurrent access.
type HeartbeatManager struct {
	mu             sync.Mutex
	lastSeen       time.Time
	pingCounter    uint32
	pingInterval   time.Duration // 50ms - send PING if idle
	failureTimeout time.Duration // 200ms - declare link dead
	onLinkFailed   func()        // Callback to trigger failover
}

// NewHeartbeatManager creates a new heartbeat manager with the specified timeouts.
//
// Parameters:
//   - pingInterval: How long to wait before sending explicit PING (recommended 50ms)
//   - failureTimeout: How long without frames before declaring failure (recommended 200ms)
//   - onFailure: Callback function to invoke when link is declared dead
//
// Returns an error if:
//   - pingInterval >= failureTimeout (no time for PING before failure)
//   - onFailure is nil (required for triggering failover)
//
// The pingInterval should be shorter than failureTimeout to allow for at least one
// PING attempt before failure. Recommended: pingInterval = failureTimeout / 4.
func NewHeartbeatManager(pingInterval, failureTimeout time.Duration, onFailure func()) (*HeartbeatManager, error) {
	// Validate inputs
	if pingInterval >= failureTimeout {
		return nil, fmt.Errorf("pingInterval (%v) must be less than failureTimeout (%v)", pingInterval, failureTimeout)
	}
	if onFailure == nil {
		return nil, fmt.Errorf("onFailure callback cannot be nil")
	}

	return &HeartbeatManager{
		lastSeen:       time.Now(),
		pingCounter:    0,
		pingInterval:   pingInterval,
		failureTimeout: failureTimeout,
		onLinkFailed:   onFailure,
	}, nil
}

// OnFrameReceived updates the LastSeen timestamp for implicit heartbeat detection.
//
// This method should be called by TransportManager.Receive() for EVERY valid frame,
// including:
//   - Telemetry frames
//   - Command responses
//   - ACK/NACK frames
//   - PONG frames (explicit heartbeat responses)
//
// By updating on any frame, we avoid sending unnecessary PINGs when the link is
// actively transferring data.
//
// Thread Safety: Uses mutex to protect lastSeen timestamp.
func (hm *HeartbeatManager) OnFrameReceived() {
	hm.mu.Lock()
	hm.lastSeen = time.Now()
	hm.mu.Unlock()
}

// Run starts the heartbeat monitoring loop as a background goroutine.
//
// This method:
//   1. Periodically checks elapsed time since lastSeen
//   2. Sends PING if idle > pingInterval (50ms)
//   3. Triggers failover if idle > failureTimeout (200ms)
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

	// Check every pingInterval to minimize latency
	ticker := time.NewTicker(hm.pingInterval)
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
// This is called periodically by Run() to:
//   1. Calculate elapsed time since lastSeen
//   2. Trigger failover if elapsed > failureTimeout (200ms)
//   3. Send PING if elapsed > pingInterval (50ms) and not yet failed
//
// The order matters: we check failure first to avoid sending PING on a dead link.
func (hm *HeartbeatManager) check(ctx context.Context, tm *TransportManager) {
	hm.mu.Lock()
	elapsed := time.Since(hm.lastSeen)
	hm.mu.Unlock()

	// Failure detection (highest priority)
	if elapsed > hm.failureTimeout {
		log.Printf("Heartbeat timeout (%v), triggering failover", elapsed)
		if hm.onLinkFailed != nil {
			hm.onLinkFailed()
		}
		return
	}

	// Explicit PING (only if idle but not yet failed)
	if elapsed > hm.pingInterval {
		hm.sendPing(ctx, tm)
	}
}

// sendPing sends a PING frame to the active transport.
//
// PING Frame Format:
//   - Type: FrameTypePing (0x00)
//   - Payload: 4-byte counter (big-endian uint32)
//   - Expected Response: PONG frame with same counter
//
// The counter prevents replay attacks (RX72N echoes the counter in PONG).
// If the PONG is received, OnFrameReceived() will be called automatically,
// updating lastSeen and resetting the heartbeat timer.
//
// Thread Safety: Increments pingCounter under mutex.
func (hm *HeartbeatManager) sendPing(ctx context.Context, tm *TransportManager) {
	hm.mu.Lock()
	counter := hm.pingCounter
	hm.pingCounter++
	hm.mu.Unlock()

	// Create PING payload (pingPayloadSize bytes, big-endian counter)
	payload := make([]byte, pingPayloadSize)
	binary.BigEndian.PutUint32(payload, counter)

	// Send PING via TransportManager (uses active transport - USB or SPI)
	// Note: We don't wait for PONG here - OnFrameReceived() will be called
	// when the PONG arrives, updating lastSeen
	pingCtx, cancel := context.WithTimeout(ctx, pingSendTimeout)
	defer cancel()

	if err := tm.Send(pingCtx, payload); err != nil {
		log.Printf("PING send failed: %v (counter=%d)", err, counter)
		// Don't update lastSeen - let the failure timeout trigger
	}
}
