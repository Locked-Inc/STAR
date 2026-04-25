// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package ws provides WebSocket support for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"context"
	"log/slog"
	"sync"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"github.com/gorilla/websocket"
	"google.golang.org/protobuf/proto"
)

// Client represents a single connected WebSocket client managed by the Hub.
// Each Client owns two goroutines -- readPump (reads frames from the connection)
// and writePump (serialises frames onto the connection) -- and communicates with
// the Hub via the send channel and the hub.register / hub.unregister channels.
// motorService is called synchronously in readPump for e-stop frames.
// logger is used for structured diagnostic output; it is never nil.
type Client struct {
	hub          *Hub
	conn         *websocket.Conn
	send         chan []byte // buffered defaultChannelBuffer -- outbound frames
	motorService MotorController
	logger       *slog.Logger
	closeOnce    sync.Once // ensures conn.Close is called exactly once
}

const (
	writeWait = 10 * time.Second
	pongWait  = 60 * time.Second
	// pingPeriod is the interval between WebSocket ping frames sent to the client.
	// It must be strictly less than pongWait so that pings arrive before the pong
	// deadline expires. Set to 90 % of pongWait (54 s), matching the previous
	// ratio-based computation of (pongWait * 9) / 10.
	pingPeriod = (pongWait * 9) / 10
	// maxMessageSize is the read-limit cap for inbound WebSocket frames.
	// The largest valid inbound message (~ControllerState) is ~100 bytes;
	// 4 KiB provides comfortable headroom while bounding per-connection
	// memory abuse from malformed or oversized frames.
	maxMessageSize = 4096
	// EstopTimeout is the maximum time allowed for the motor-controller to
	// acknowledge an emergency stop before the context is cancelled. Using
	// context.Background() as the root ensures a dropped HTTP connection cannot
	// abort the safety command before it reaches the motor controller.
	// Exported so the REST /api/estop handler in the app package can share the
	// same safety-critical timeout without duplicating the constant.
	EstopTimeout = 5 * time.Second
)

// NewClient creates a properly initialised Client and registers it with the
// provided hub. It is the canonical constructor for external callers; the hub's
// handler (ws.NewHandler) uses this to ensure all unexported fields are set.
//
// Callers must start the read and write pump goroutines after construction:
//
//	go client.writePump()
//	client.readPump()
func NewClient(hub *Hub, conn *websocket.Conn, motorService MotorController, logger *slog.Logger) *Client {
	if hub == nil {
		panic("ws.NewClient: hub must not be nil")
	}
	if conn == nil {
		panic("ws.NewClient: conn must not be nil")
	}
	if motorService == nil {
		panic("ws.NewClient: motorService must not be nil")
	}
	if logger == nil {
		logger = slog.Default()
	}
	return &Client{
		hub:          hub,
		conn:         conn,
		send:         make(chan []byte, defaultChannelBuffer),
		motorService: motorService,
		logger:       logger,
	}
}

// closeConn closes the WebSocket connection exactly once regardless of how
// many goroutines call it concurrently (readPump and writePump both defer it).
func (c *Client) closeConn() {
	c.closeOnce.Do(func() {
		if err := c.conn.Close(); err != nil {
			c.logger.Debug("closeConn: error closing websocket connection", slog.Any("error", err))
		}
	})
}

// writePump is the ONLY goroutine that ever calls WriteMessage on conn.
// All writes go through the send channel. No direct writes from other goroutines.
func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.closeConn()
	}()
	for {
		select {
		case message, ok := <-c.send:
			// SetWriteDeadline before EVERY WriteMessage.
			if err := c.conn.SetWriteDeadline(time.Now().Add(writeWait)); err != nil {
				c.logger.Error("failed to set write deadline", slog.Any("error", err))
				return
			}
			if !ok {
				// Hub closed the channel -- send WS close frame and exit.
				if err := c.conn.WriteMessage(websocket.CloseMessage, []byte{}); err != nil {
					c.logger.Error("failed to write close message", slog.Any("error", err))
				}
				return
			}
			if err := c.conn.WriteMessage(websocket.BinaryMessage, message); err != nil {
				c.logger.Error("failed to write binary message", slog.Any("error", err))
				return
			}

		case <-ticker.C:
			// Use SetWriteDeadline + WriteMessage for ping instead of WriteControl.
			// WriteControl permanently mutates the connection write deadline (gorilla
			// issue #841 / ADR-17); SetWriteDeadline + WriteMessage keeps the deadline
			// scoped to this write only.
			if err := c.conn.SetWriteDeadline(time.Now().Add(writeWait)); err != nil {
				c.logger.Error("failed to set write deadline for ping", slog.Any("error", err))
				return
			}
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				c.logger.Error("failed to write ping", slog.Any("error", err))
				return
			}
		}
	}
}

// readPump owns all reads from conn. Runs until error or close.
// E-stop is handled SYNCHRONOUSLY here -- this is intentional.
// Joystick commands should not process while stop is propagating.
func (c *Client) readPump() {
	defer func() {
		// Bounded-blocking unregister: wait up to 5 s for the hub to accept the
		// client removal before giving up. A dropped non-blocking send would leave
		// the client in h.clients indefinitely, leaking the map entry and causing
		// stampAndFanOut to attempt writes to a permanently non-draining send channel.
		t := time.NewTimer(5 * time.Second)
		defer t.Stop()
		select {
		case c.hub.unregister <- c:
		case <-t.C:
			c.logger.Error("hub unregister channel blocked for 5s -- client leaked in hub")
		}
		c.closeConn()
	}()

	// Hard cap: see maxMessageSize for rationale.
	c.conn.SetReadLimit(maxMessageSize)

	if err := c.conn.SetReadDeadline(time.Now().Add(pongWait)); err != nil {
		c.logger.Error("failed to set initial read deadline", slog.Any("error", err))
		return
	}
	c.conn.SetPongHandler(func(string) error {
		return c.conn.SetReadDeadline(time.Now().Add(pongWait))
	})

	for {
		_, data, err := c.conn.ReadMessage()
		if err != nil {
			// Log unexpected (non-clean) disconnects at Debug level for diagnostics.
			if websocket.IsUnexpectedCloseError(err,
				websocket.CloseGoingAway,
				websocket.CloseNormalClosure,
				websocket.CloseNoStatusReceived,
			) {
				c.logger.Debug("readPump: unexpected close", slog.Any("error", err))
			}
			return
		}

		env := &starv1.STAREnvelope{}
		if err := proto.Unmarshal(data, env); err != nil {
			// Malformed frame -- log and continue. Do not kill the connection.
			c.logger.Warn("bad envelope from client",
				slog.Any("error", err),
				slog.Int("bytes", len(data)),
			)
			continue
		}

		// -- E-stop priority path --
		if estop, ok := env.Payload.(*starv1.STAREnvelope_Estop); ok {
			// Use a short-lived context so a slow motor controller cannot
			// block readPump indefinitely. context.Background() is the root
			// so cancellation of connection context cannot abort an e-stop.
			ctx, cancel := context.WithTimeout(context.Background(), EstopTimeout)
			if err := c.motorService.EmergencyStop(ctx, estop.Estop.Reason); err != nil {
				c.logger.Error("emergency stop failed",
					slog.String("reason", estop.Estop.Reason),
					slog.Any("error", err),
				)
			}
			cancel() // Release context resources immediately; do not defer inside a loop.
			// Forward for packet analyzer visibility (non-blocking)
			select {
			case c.hub.inbound <- env:
			default:
				c.logger.Warn("hub inbound full, dropped e-stop forward",
					slog.Uint64("seq", env.Seq),
					slog.String("reason", estop.Estop.Reason),
				)
			}
			continue
		}

		// All other inbound messages
		select {
		case c.hub.inbound <- env:
		default:
			// Hub inbound full -- drop. Never block readPump.
			c.logger.Debug("hub inbound channel full, dropping inbound envelope",
				slog.Uint64("seq", env.Seq),
			)
		}
	}
}
