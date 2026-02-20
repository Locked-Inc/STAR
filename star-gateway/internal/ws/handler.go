// Package ws provides the WebSocket transport layer for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"io"
	"log/slog"
	"net/http"

	"github.com/gorilla/websocket"
)

// upgrader configures the WebSocket upgrade from HTTP.
// CheckOrigin is permissive because STAR operates on a local private network.
var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(_ *http.Request) bool {
		return true // Local network deployment only. Restrict if deployed externally.
	},
}

// NewHandler returns an http.Handler that upgrades each request to a WebSocket
// connection and wires it into hub.
func NewHandler(hub *Hub, motorService MotorController, logger *slog.Logger) http.Handler {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(io.Discard, nil))
	}
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		conn, err := upgrader.Upgrade(w, r, nil)
		if err != nil {
			logger.Error("websocket upgrade failed", slog.String("err", err.Error()))
			return
		}
		client := &Client{
			hub:          hub,
			conn:         conn,
			send:         make(chan []byte, defaultChannelBuffer),
			motorService: motorService,
			logger:       logger,
		}
		hub.register <- client
		go client.writePump()
		go client.readPump()
		// Handler returns immediately. readPump and writePump own the connection.
	})
}
