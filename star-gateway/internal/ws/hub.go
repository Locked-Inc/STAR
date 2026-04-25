// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package ws provides WebSocket support for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"context"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"sync"
	"sync/atomic"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// ErrBroadcastFull is returned by Broadcast when the hub's outbound channel is
// saturated and the envelope must be dropped to avoid blocking.
var ErrBroadcastFull = errors.New("ws: hub broadcast channel full")

// ErrInvalidEnvelope is returned by Broadcast when the caller passes a nil envelope.
var ErrInvalidEnvelope = errors.New("ws: nil envelope passed to Broadcast")

// InboundDispatcher receives envelopes forwarded inbound by WebSocket clients
// (all non-e-stop payloads read from the browser/UI). Implement this interface
// and register it with Hub.SetInboundDispatcher to route commands to the
// application layer instead of silently dropping them.
type InboundDispatcher interface {
	// Dispatch is called by the hub's event loop for every inbound envelope.
	// Implementations must be non-blocking or fast; the hub's main select loop
	// runs Dispatch synchronously and any stall delays all hub processing.
	Dispatch(ctx context.Context, env *starv1.STAREnvelope) error
}

// Hub maintains the set of active clients and broadcasts messages to the clients.
type Hub struct {
	clients    map[*Client]struct{}
	register   chan *Client
	unregister chan *Client
	broadcast  chan *starv1.STAREnvelope // buffered defaultChannelBuffer, from GatewayService
	inbound    chan *starv1.STAREnvelope // buffered defaultChannelBuffer, UI->GW messages

	inboundDispatcher InboundDispatcher // optional; nil means drop inbound envelopes

	startMu           sync.Mutex    // guards running flag and inboundDispatcher set-before-start
	running           atomic.Bool   // set to true when Run starts; guards SetInboundDispatcher
	seq               atomic.Uint64 // outbound envelopes only
	registeredClients atomic.Int32  // thread-safe count of active clients
	logger            *slog.Logger
}

// typeThrottleState holds the last-broadcast time per throttled message type.
// Named struct fields eliminate reflection and map allocations in the hot path.
type typeThrottleState struct {
	telemetry time.Time
	motor     time.Time
	odometry  time.Time
	system    time.Time
	lidar     time.Time
}

const (
	defaultChannelBuffer = 32
	// clientChanBufferSize is the buffer depth for client register/unregister channels.
	// A buffer of 1 prevents callers from blocking if hub.Run has not yet started.
	clientChanBufferSize = 1
	telemetryInterval    = 100 * time.Millisecond // <=10 Hz
	lidarInterval        = 400 * time.Millisecond // <=2.5 Hz
)

// NewHub creates a new Hub with the provided logger.
func NewHub(logger *slog.Logger) *Hub {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(io.Discard, nil))
	}
	return &Hub{
		clients:    make(map[*Client]struct{}),
		register:   make(chan *Client, clientChanBufferSize), // buffered so callers don't block if Run hasn't started yet
		unregister: make(chan *Client, clientChanBufferSize), // buffered so callers don't block if Run hasn't started yet
		broadcast:  make(chan *starv1.STAREnvelope, defaultChannelBuffer),
		inbound:    make(chan *starv1.STAREnvelope, defaultChannelBuffer),
		logger:     logger,
	}
}

// SetInboundDispatcher registers an InboundDispatcher that receives all inbound
// WebSocket envelopes (non-e-stop payloads forwarded by client readPumps).
// Must be called before Hub.Run starts. Panics if called after Run has started
// to enforce the "set-before-start" invariant and prevent data races.
func (h *Hub) SetInboundDispatcher(d InboundDispatcher) {
	h.startMu.Lock()
	defer h.startMu.Unlock()
	if h.running.Load() {
		panic("ws.Hub.SetInboundDispatcher: must be called before Hub.Run starts")
	}
	h.inboundDispatcher = d
}

// ClientCount returns the current number of active registered WebSocket clients.
// The value is read atomically and is safe to call from any goroutine without
// locking. Intended for use by metrics collection and health checks.
func (h *Hub) ClientCount() int {
	return int(h.registeredClients.Load())
}

// Broadcast pushes env to all connected clients. Non-blocking: if the hub's
// internal channel is full, the envelope is dropped and ErrBroadcastFull is
// returned so callers can decide whether to log or meter the drop. If ctx is
// already cancelled when Broadcast is called, ctx.Err() is returned immediately.
//
// Ownership transfer: the caller must not access or reuse env after calling
// Broadcast. The Hub's event loop (stampAndFanOut) mutates env.Seq and
// env.TimestampUs in-place before marshalling and fan-out.
func (h *Hub) Broadcast(ctx context.Context, env *starv1.STAREnvelope) error {
	if env == nil {
		h.logger.Warn("hub Broadcast called with nil envelope", slog.Uint64("seq", h.seq.Load()))
		return ErrInvalidEnvelope
	}
	select {
	case h.broadcast <- env:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	default:
		h.logger.Warn("hub broadcast channel full, dropping message", slog.Uint64("seq", h.seq.Load()))
		return ErrBroadcastFull
	}
}

// Run starts the hub's main event loop. It blocks until ctx is cancelled and
// must therefore be invoked in its own goroutine so callers do not block.
//
// Concurrency guarantees:
//   - Client registration and unregistration are safe from any goroutine via
//     the h.register / h.unregister channels; internal client state is never
//     accessed directly from outside Run.
//   - Outbound broadcasts are delivered from the h.broadcast channel; use
//     Broadcast to enqueue messages without racing with the event loop.
//   - When ctx is cancelled, Run closes every active client's send channel
//     before returning so writePump goroutines terminate cleanly.
func (h *Hub) Run(ctx context.Context) {
	// Signal that the hub is running; SetInboundDispatcher must not be called
	// after this point. Held briefly under startMu to synchronise with any
	// concurrent SetInboundDispatcher caller.
	h.startMu.Lock()
	h.running.Store(true)
	h.startMu.Unlock()
	var t typeThrottleState

	for {
		select {
		case <-ctx.Done():
			// Graceful shutdown: signal all writePumps to finish.
			for client := range h.clients {
				close(client.send)
				delete(h.clients, client)
				h.registeredClients.Add(-1)
			}
			return

		case client := <-h.register:
			h.clients[client] = struct{}{}
			h.registeredClients.Add(1)

		case client := <-h.unregister:
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				close(client.send)
				h.registeredClients.Add(-1)
			}

		case env := <-h.broadcast:
			now := time.Now()
			h.route(env, now, &t, telemetryInterval, lidarInterval)

		case env := <-h.inbound:
			// Inbound envelopes forwarded by client readPumps (all non-estop payloads).
			if h.inboundDispatcher != nil {
				if err := h.inboundDispatcher.Dispatch(ctx, env); err != nil {
					h.logger.Warn("hub: inboundDispatcher.Dispatch failed",
						slog.Uint64("seq", env.Seq),
						slog.String("type", fmt.Sprintf("%T", env.Payload)),
						slog.Any("error", err),
					)
				}
			} else {
				// No dispatcher wired -- drain the channel to prevent client backpressure.
				h.logger.Warn("hub: inbound envelope dropped (no dispatcher wired)",
					slog.Uint64("seq", env.Seq),
					slog.String("type", fmt.Sprintf("%T", env.Payload)),
				)
			}
		}
	}
}

// shouldThrottle reports whether an outbound message should be suppressed to
// enforce the given rate limit. When the message is allowed through, *lastSent
// is updated to now so subsequent calls measure from the correct baseline.
func shouldThrottle(lastSent *time.Time, now time.Time, interval time.Duration) bool {
	if now.Sub(*lastSent) < interval {
		return true
	}
	*lastSent = now
	return false
}

func (h *Hub) route(
	env *starv1.STAREnvelope,
	now time.Time,
	t *typeThrottleState,
	// telemetryRate and lidarRate are caller-supplied rate limits (not the package constants).
	// Pass the package-level telemetryInterval / lidarInterval at normal call sites.
	telemetryRate, lidarRate time.Duration,
) {
	switch env.Payload.(type) {
	// -- Alerts and E-stop: ALWAYS immediate, no throttle --
	case *starv1.STAREnvelope_Alert, *starv1.STAREnvelope_Estop:
		h.stampAndFanOut(env, now)

	// -- LiDAR: independent throttle (large packed payload) --
	case *starv1.STAREnvelope_Lidar:
		if shouldThrottle(&t.lidar, now, lidarRate) {
			return
		}
		h.stampAndFanOut(env, now)

	// -- Per-type throttle for remaining sensor data --
	case *starv1.STAREnvelope_Telemetry:
		if shouldThrottle(&t.telemetry, now, telemetryRate) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Motors:
		if shouldThrottle(&t.motor, now, telemetryRate) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Odometry:
		if shouldThrottle(&t.odometry, now, telemetryRate) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_System:
		if shouldThrottle(&t.system, now, telemetryRate) {
			return
		}
		h.stampAndFanOut(env, now)

	default:
		// Unknown payload variant -- log and discard. This can occur when a new
		// proto field is added and the gateway is not yet updated to handle it.
		h.logger.Warn("hub: dropping envelope with unrecognised payload type",
			slog.String("type", fmt.Sprintf("%T", env.Payload)),
			slog.Uint64("seq", env.Seq),
			slog.String("note", "Make sure the gateway is up to date with the latest proto definitions."),
		)
	}
}

// stampAndFanOut assigns seq + ts_ms, marshals ONCE, sends to all clients.
func (h *Hub) stampAndFanOut(env *starv1.STAREnvelope, now time.Time) {
	env.Seq = h.seq.Add(1)
	env.TsMs = now.UnixMilli()

	data, err := proto.Marshal(env)
	if err != nil {
		h.logger.Error("failed to marshal envelope for broadcast", slog.Any("error", err), slog.Uint64("seq", env.Seq))
		return
	}

	for client := range h.clients {
		select {
		case client.send <- data:
		default:
			// Client send buffer full -- drop this frame for this client.
			// Never disconnect, never block. Client catches up when foregrounded.
			h.logger.Debug("dropping frame for client (send buffer full)", slog.Uint64("seq", env.Seq))
		}
	}
}
