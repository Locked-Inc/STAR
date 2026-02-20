// Package ws provides WebSocket support for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"io"
	"log/slog"
	"sync/atomic"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

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

// Broadcast is called by GatewayService. Non-blocking — drops if buffer full.
func (h *Hub) Broadcast(env *starv1.STAREnvelope) error {
	select {
	case h.broadcast <- env:
		return nil
	default:
		h.logger.Warn("hub broadcast channel full, dropping message", slog.Uint64("seq", h.seq.Load()))
		return nil
	}
}

func (h *Hub) Run() {
	var t typeThrottleState

	for {
		select {
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
		}
	}
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
		if now.Sub(t.lidar) < lidarInterval {
			return
		}
		t.lidar = now
		h.stampAndFanOut(env, now)

	// ── Per-type throttle for remaining sensor data ──
	case *starv1.STAREnvelope_Telemetry:
		if now.Sub(t.telemetry) < telemetryInterval {
			return
		}
		t.telemetry = now
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Motors:
		if now.Sub(t.motor) < telemetryInterval {
			return
		}
		t.motor = now
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Battery:
		if now.Sub(t.battery) < telemetryInterval {
			return
		}
		t.battery = now
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_Odometry:
		if now.Sub(t.odometry) < telemetryInterval {
			return
		}
		t.odometry = now
		h.stampAndFanOut(env, now)

	case *starv1.STAREnvelope_System:
		if now.Sub(t.system) < telemetryInterval {
			return
		}
		t.system = now
		h.stampAndFanOut(env, now)
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
