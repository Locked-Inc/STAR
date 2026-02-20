// Package ws provides WebSocket support for the STAR Gateway.
//
// STAR Project - Texas A&M University
// February 2026
package ws

import (
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// HubNotifier is the minimal interface GatewayService uses to push messages to the WebSocket hub.
type HubNotifier interface {
	Broadcast(env *starv1.STAREnvelope) error
}

// MotorController is the interface for motor safety operations.
// NOTE: No context is provided to this interface since the operations are expected to be immediate and not require cancellation or timeouts.
type MotorController interface {
	EmergencyStop(reason string) error
}
