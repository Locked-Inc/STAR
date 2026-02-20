// Package ws provides WebSocket support for the STAR Gateway.
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
	Broadcast(env *starv1.STAREnvelope) error
}

// MotorController is the interface for motor safety operations.
// Implementations receive a context for cancellation and tracing; they may ignore
// cancellation but should propagate it to any downstream call that supports context.
type MotorController interface {
	EmergencyStop(ctx context.Context, reason string) error
}
