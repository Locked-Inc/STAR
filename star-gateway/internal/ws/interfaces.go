// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package ws provides the WebSocket transport layer for the STAR Gateway's L5
// (application/session) service layer.
//
// Architecture role:
// ws sits between the HTTP server (net/http) and the GatewayService (internal/service).
// It manages the lifecycle of individual WebSocket connections (Client), co-ordinates
// fan-out to all connected UI clients (Hub), and serialises/deserialises binary
// Protobuf STAREnvelope frames. GatewayService communicates with the hub exclusively
// through the HubNotifier interface so ws can be replaced or mocked in tests without
// touching service logic.
//
// Thread-safety contract for implementations and consumers:
//   - Hub.Broadcast and Hub.Run are safe to call concurrently from different goroutines;
//     Broadcast enqueues via a channel and never accesses the clients map directly.
//   - Hub.register and Hub.unregister channels are the ONLY external entry-points for
//     modifying the clients map; all mutations happen inside Run's goroutine.
//   - Client.writePump is the ONLY goroutine that writes to the WebSocket connection;
//     external code must not call conn.WriteMessage directly.
//   - MotorController.EmergencyStop may be called from Client.readPump while the
//     connection is alive; implementations must be safe for concurrent calls.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	"context"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// HubNotifier is the minimal interface GatewayService uses to push messages to the WebSocket hub.
type HubNotifier interface {
	Broadcast(ctx context.Context, env *starv1.STAREnvelope) error
}

// MotorController is the interface for motor safety operations.
// Implementations receive a context for cancellation and tracing; they may ignore
// cancellation but should propagate it to any downstream call that supports context.
type MotorController interface {
	EmergencyStop(ctx context.Context, reason string) error
}
