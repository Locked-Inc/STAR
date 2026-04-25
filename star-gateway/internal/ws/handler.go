// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package ws provides the WebSocket transport layer for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"io"
	"log/slog"
	"net/http"
	"net/url"

	"github.com/gorilla/websocket"
)

const (
	// defaultReadBufferSize is the initial read buffer size for WebSocket upgrade negotiation.
	defaultReadBufferSize = 1024
	// defaultWriteBufferSize is the initial write buffer size for WebSocket upgrade negotiation.
	defaultWriteBufferSize = 1024
)

// makeCheckOrigin returns a CheckOrigin function for the WebSocket upgrader.
// When allowInsecure is true, all origins are permitted (useful for local dev/test).
// Otherwise, a same-origin check is performed: the Origin header must match the
// request Host. Requests with no Origin header (e.g., non-browser / CLI clients)
// are always permitted.
func makeCheckOrigin(allowInsecure bool) func(r *http.Request) bool {
	if allowInsecure {
		return func(_ *http.Request) bool { return true }
	}
	return func(r *http.Request) bool {
		origin := r.Header.Get("Origin")
		if origin == "" {
			// No Origin header -- non-browser client (e.g., CLI, firmware). Allow.
			return true
		}
		parsed, err := url.Parse(origin)
		if err != nil {
			// Malformed Origin -- reject.
			return false
		}
		// Compare the parsed origin host (host[:port]) to the request Host.
		return parsed.Host == r.Host
	}
}

// NewHandler returns an http.Handler that upgrades each request to a WebSocket
// connection and wires it into hub.
//
// allowInsecureWebsocket controls origin checking for incoming upgrade requests:
// when true, all origins are permitted -- useful for local-network or development
// deployments where requests arrive from a different port or host. When false, a
// same-origin policy is enforced -- the Origin header must match the request Host.
// Pass false for production deployments; pass true only for local/dev environments.
//
// Panics at construction time if hub or motorService is nil:
//   - A nil hub has no client set and no event loop, so registration would block
//     indefinitely and routing would be unreachable.
//   - A nil motorService would cause a nil-pointer dereference in Client.readPump
//     on e-stop frames.
func NewHandler(hub *Hub, motorService MotorController, logger *slog.Logger, allowInsecureWebsocket bool) http.Handler {
	if hub == nil {
		panic("ws.NewHandler: hub must not be nil")
	}
	if motorService == nil {
		panic("ws.NewHandler: motorService must not be nil")
	}
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(io.Discard, nil))
	}
	upgrader := websocket.Upgrader{
		ReadBufferSize:  defaultReadBufferSize,
		WriteBufferSize: defaultWriteBufferSize,
		CheckOrigin:     makeCheckOrigin(allowInsecureWebsocket),
	}
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		conn, err := upgrader.Upgrade(w, r, nil)
		if err != nil {
			logger.Error("websocket upgrade failed", slog.Any("err", err))
			return
		}
		client := NewClient(hub, conn, motorService, logger)
		hub.register <- client
		go client.writePump()
		go client.readPump()
		// Handler returns immediately. readPump and writePump own the connection.
	})
}
