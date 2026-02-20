// Package ws provides WebSocket support for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"context"
	"log/slog"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"github.com/gorilla/websocket"
	"google.golang.org/protobuf/proto"
)

type Client struct {
	hub          *Hub
	conn         *websocket.Conn
	send         chan []byte // buffered 32 — outbound frames
	motorService MotorController
	logger       *slog.Logger
}

const (
	writeWait = 10 * time.Second
	pongWait  = 60 * time.Second
	// pingPeriodRatio is the fraction of pongWait used for ping intervals.
	// Set below 1.0 to ensure pings arrive before the pong deadline expires.
	pingPeriodRatio = 9
	pingPeriodDenom = 10
	// maxMessageSize is the read-limit cap for inbound WebSocket frames.
	// The largest valid inbound message (~ControllerState) is ~100 bytes;
	// 4 KiB provides comfortable headroom while bounding per-connection
	// memory abuse from malformed or oversized frames.
	maxMessageSize = 4096
)

// writePump is the ONLY goroutine that ever calls WriteMessage on conn.
// All writes go through the send channel. No direct writes from other goroutines.
func (c *Client) writePump() {
	pingPeriod := (pongWait * pingPeriodRatio) / pingPeriodDenom

	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()
	for {
		select {
		case message, ok := <-c.send:
			// SetWriteDeadline before EVERY WriteMessage.
			if err := c.conn.SetWriteDeadline(time.Now().Add(writeWait)); err != nil {
				c.logger.Error("failed to set write deadline", "error", err)
				return
			}
			if !ok {
				// Hub closed the channel — send WS close frame and exit.
				if err := c.conn.WriteMessage(websocket.CloseMessage, []byte{}); err != nil {
					c.logger.Error("failed to write close message", "error", err)
				}
				return
			}
			if err := c.conn.WriteMessage(websocket.BinaryMessage, message); err != nil {
				c.logger.Error("failed to write binary message", "error", err)
				return
			}

		case <-ticker.C:
			// WriteControl permanently mutates WriteDeadline.
			if err := c.conn.SetWriteDeadline(time.Now().Add(writeWait)); err != nil {
				c.logger.Error("failed to set write deadline for ping", "error", err)
				return
			}
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				c.logger.Error("failed to write ping message", "error", err)
				return
			}
		}
	}
}

// readPump owns all reads from conn. Runs until error or close.
// E-stop is handled SYNCHRONOUSLY here — this is intentional.
// Joystick commands should not process while stop is propagating.
func (c *Client) readPump() {
	defer func() {
		c.hub.unregister <- c
		c.conn.Close()
	}()

	// Hard cap: see maxMessageSize for rationale.
	c.conn.SetReadLimit(maxMessageSize)

	if err := c.conn.SetReadDeadline(time.Now().Add(pongWait)); err != nil {
		c.logger.Error("failed to set initial read deadline", slog.String("err", err.Error()))
		return
	}
	c.conn.SetPongHandler(func(string) error {
		return c.conn.SetReadDeadline(time.Now().Add(pongWait))
	})

	for {
		_, data, err := c.conn.ReadMessage()
		if err != nil {
			// Covers: clean close, ping timeout, network drop, read limit exceeded.
			return
		}

		env := &starv1.STAREnvelope{}
		if err := proto.Unmarshal(data, env); err != nil {
			// Malformed frame — log and continue. Do not kill the connection.
			c.logger.Warn("bad envelope from client",
				slog.String("err", err.Error()),
				slog.Int("bytes", len(data)),
			)
			continue
		}

		// ── E-stop priority path ──
		if estop, ok := env.Payload.(*starv1.STAREnvelope_Estop); ok {
			// Use a short-lived context so a slow motor controller cannot
			// block readPump indefinitely. context.Background() is the root
			// so cancellation of connection context cannot abort an e-stop.
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			if err := c.motorService.EmergencyStop(ctx, estop.Estop.Reason); err != nil {
				c.logger.Error("emergency stop failed",
					slog.String("reason", estop.Estop.Reason),
					slog.String("err", err.Error()),
				)
			}
			cancel()
			// Forward for packet analyzer visibility (non-blocking)
			select {
			case c.hub.inbound <- env:
			default:
			}
			continue
		}

		// All other inbound messages
		select {
		case c.hub.inbound <- env:
		default:
			// Hub inbound full — drop. Never block readPump.
			c.logger.Debug("hub inbound channel full, dropping inbound envelope",
				slog.Uint64("seq", env.Seq),
			)
		}
	}
}
