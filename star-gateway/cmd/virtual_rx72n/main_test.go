package main

import (
	"testing"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

func TestGenerateTelemetryResponse(t *testing.T) {
	// Test normal generation
	resp := generateTelemetryResponse()

	if resp == nil {
		t.Fatal("generateTelemetryResponse returned nil")
	}

	telemetry := resp.GetTelemetryData()
	if telemetry == nil {
		t.Fatal("Payload is not TelemetryData")
	}

	if telemetry.BatteryPercent != 85.0 {
		t.Errorf("BatteryPercent = %f, want 85.0", telemetry.BatteryPercent)
	}
}

func TestProcessCommand(t *testing.T) {
	// Test VelocityCommand
	cmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps: 1.0,
		Sequence:             1,
	}
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_VelocityCommand{
			VelocityCommand: cmd,
		},
	}

	resp := processCommand(wireMsg)
	if resp == nil {
		t.Fatal("processCommand returned nil for VelocityCommand")
	}
	if resp.GetTelemetryData() == nil {
		t.Error("Expected TelemetryData response for VelocityCommand")
	}

	// Test EmergencyStopCommand
	estop := &starv1.EmergencyStopCommand{
		Reason: "Test",
	}
	wireMsgEstop := &starv1.WireMessage{
		Payload: &starv1.WireMessage_EmergencyStopCommand{
			EmergencyStopCommand: estop,
		},
	}

	respEstop := processCommand(wireMsgEstop)
	if respEstop == nil {
		t.Fatal("processCommand returned nil for EmergencyStopCommand")
	}
	if respEstop.GetTelemetryData() == nil {
		t.Error("Expected TelemetryData response for EmergencyStopCommand")
	}

	// Test unknown message (default case)
	emptyMsg := &starv1.WireMessage{}
	respUnknown := processCommand(emptyMsg)
	if respUnknown == nil {
		t.Fatal("processCommand returned nil for unknown message")
	}
	if respUnknown.GetTelemetryData() == nil {
		t.Error("Expected TelemetryData response for unknown message")
	}
}
