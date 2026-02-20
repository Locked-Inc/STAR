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
	"sync/atomic"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// ErrBroadcastFull is returned by Broadcast when the hub's outbound channel is
// saturated and the envelope must be dropped to avoid blocking.
var ErrBroadcastFull = errors.New("ws: hub broadcast channel full")

// Hub maintains the set of active clients and broadcasts messages to the clients.
type Hub struct {
	clients    map[*Client]struct{}
	register   chan *Client
	unregister chan *Client
	broadcast  chan *starv1.STAREnvelope // buffered 32, from GatewayService
	inbound    chan *starv1.STAREnvelope // buffered 32, UI→GW messages

	seq          atomic.Uint64 // outbound envelopes only
	motorService MotorController
	logger       *slog.Logger
}

// typeThrottleState holds the last-broadcast time per throttled message type.
// Named struct fields eliminate reflection and map allocations in the hot path.
type typeThrottleState struct {
	telemetry time.Time
	motor     time.Time
	battery   time.Time
	odometry  time.Time
	system    time.Time
	lidar     time.Time
}

const (
	defaultChannelBuffer = 32
	telemetryInterval    = 100 * time.Millisecond // <=10 Hz
	lidarInterval        = 400 * time.Millisecond // <=2.5 Hz
)

// NewHub creates a new Hub with the provided MotorController and logger.
func NewHub(motorService MotorController, logger *slog.Logger) *Hub {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(io.Discard, nil))
	}
	return &Hub{
		clients:      make(map[*Client]struct{}),
		register:     make(chan *Client),
		unregister:   make(chan *Client),
		broadcast:    make(chan *starv1.STAREnvelope, defaultChannelBuffer),
		inbound:      make(chan *starv1.STAREnvelope, defaultChannelBuffer),
		motorService: motorService,
		logger:       logger,
	}
}

// Broadcast pushes env to all connected clients. Non-blocking: if the hub's
// internal channel is full, the envelope is dropped and ErrBroadcastFull is
// returned so callers can decide whether to log or meter the drop.
func (h *Hub) Broadcast(env *starv1.STAREnvelope) error {
	select {
	case h.broadcast <- env:
		return nil
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
	var t typeThrottleState

	for {
		select {
		case <-ctx.Done():
			// Graceful shutdown: signal all writePumps to finish.
			for client := range h.clients {
				close(client.send)
				delete(h.clients, client)
			}
			return

		case client := <-h.register:
			h.clients[client] = struct{}{}

		case client := <-h.unregister:
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				close(client.send)
			}

		case env := <-h.broadcast:
			now := time.Now()
			h.route(env, now, &t, telemetryInterval, lidarInterval)

		case env := <-h.inbound:
			// Inbound envelopes forwarded by client readPumps (all non-estop payloads).
			// Consumed here to drain the channel and prevent client-side backpressure.
			// Future: dispatch to an application-level command handler.
			_ = env
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
	telemetryInterval, lidarInterval time.Duration,
) {
	switch env.Payload.(type) {
	// ── Alerts and E-stop: ALWAYS immediate, no throttle ──
	case *starv1.STAREnvelope_Alert, *starv1.STAREnvelope_Estop:
		h.stampAndFanOut(env, now)

	// ── LiDAR: independent throttle (large packed payload) ──
	case *starv1.STAREnvelope_Lidar:
		if shouldThrottle(&t.lidar, now, lidarInterval) {
			return
		}
		h.stampAndFanOut(env, now)

	// ── Per-type throttle for remaining sensor data ──
	case *starv1.STAREnvelope_Telemetry:
		if shouldThrottle(&t.telemetry, now, telemetryInterval) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Motors:
		if shouldThrottle(&t.motor, now, telemetryInterval) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Battery:
		if shouldThrottle(&t.battery, now, telemetryInterval) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Odometry:
		if shouldThrottle(&t.odometry, now, telemetryInterval) {
			return
		}
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_System:
		if shouldThrottle(&t.system, now, telemetryInterval) {
			return
		}
		h.stampAndFanOut(env, now)

	default:
		// Unknown payload variant — log and discard. This can occur when a new
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
		h.logger.Error("failed to marshal envelope for broadcast", slog.String("err", err.Error()), slog.Uint64("seq", env.Seq))
		return
	}

	for client := range h.clients {
		select {
		case client.send <- data:
		default:
			// Client send buffer full — drop this frame for this client.
			// Never disconnect, never block. Client catches up when foregrounded.
			h.logger.Debug("dropping frame for client (send buffer full)", slog.Uint64("seq", env.Seq))
		}
	}
}
