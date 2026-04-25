// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package dispatcher

import (
	"testing"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// FuzzWireMessageUnmarshal tests that protobuf unmarshaling of WireMessage
// never panics on arbitrary input. Seeds are valid marshaled WireMessages
// covering every oneof variant used by the dispatcher.
func FuzzWireMessageUnmarshal(f *testing.F) {
	// Build seed corpus from each WireMessage payload variant.
	seedMessages := []*starv1.WireMessage{
		// TelemetryData
		{Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: &starv1.TelemetryData{TimestampUs: 1234567890},
		}},
		// VelocityCommand
		{Payload: &starv1.WireMessage_VelocityCommand{
			VelocityCommand: &starv1.VelocityCommand{FrontLeftVelocityMps: 1.5},
		}},
		// EmergencyStopCommand
		{Payload: &starv1.WireMessage_EmergencyStopCommand{
			EmergencyStopCommand: &starv1.EmergencyStopCommand{Reason: "test"},
		}},
		// EncoderData
		{Payload: &starv1.WireMessage_EncoderData{
			EncoderData: &starv1.EncoderData{MotorId: 0},
		}},
		// Empty WireMessage (no oneof set)
		{},
	}

	for _, msg := range seedMessages {
		data, err := proto.Marshal(msg)
		if err != nil {
			f.Fatalf("failed to marshal seed WireMessage: %v", err)
		}
		f.Add(data)
	}

	// Degenerate seeds.
	f.Add([]byte{})
	f.Add([]byte{0xFF, 0xFF, 0xFF})
	f.Add([]byte{0x08, 0x96, 0x01}) // valid varint field

	f.Fuzz(func(t *testing.T, data []byte) {
		msg := &starv1.WireMessage{}
		// Must not panic. Errors are expected for malformed protobuf.
		_ = proto.Unmarshal(data, msg)
	})
}
