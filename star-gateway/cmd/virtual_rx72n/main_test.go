package main

import (
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

func TestIsAllZeros(t *testing.T) {
	tests := []struct {
		name     string
		input    []byte
		expected bool
	}{
		{"empty", []byte{}, true},
		{"all zeros", []byte{0, 0, 0, 0}, true},
		{"mixed", []byte{0, 0, 1, 0}, false},
		{"no zeros", []byte{1, 2, 3}, false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := isAllZeros(tt.input); got != tt.expected {
				t.Errorf("isAllZeros() = %v, want %v", got, tt.expected)
			}
		})
	}
}

func TestGenerateTelemetryData(t *testing.T) {
	timestamp := int64(123456789)
	data := generateTelemetryData(timestamp)

	if data == nil {
		t.Fatal("GenerateTelemetryData returned nil")
	}

	if data.TimestampUs != timestamp {
		t.Errorf("TimestampUs = %d, want %d", data.TimestampUs, timestamp)
	}

	if data.BatteryVoltageV != dummyBatteryVoltage {
		t.Errorf("BatteryVoltageV = %f, want %f", data.BatteryVoltageV, dummyBatteryVoltage)
	}

	if data.EncoderFrontLeft == nil {
		t.Error("EncoderFrontLeft is nil")
	}
}

func TestGenerateTelemetryResponse(t *testing.T) {
	encoder := frame.NewEncoder()
	seqNum := uint16(10)
	timestamp := int64(1000)
	responseLen := 256

	// Test normal generation
	resp := generateTelemetryResponse(encoder, &seqNum, &timestamp, responseLen)

	if len(resp) != responseLen {
		t.Errorf("Response length = %d, want %d", len(resp), responseLen)
	}

	if seqNum != 11 {
		t.Errorf("Sequence number not incremented, got %d want 11", seqNum)
	}

	if timestamp != 1000+telemetryInterval.Microseconds() {
		t.Errorf("Timestamp not incremented correctly")
	}

	// Verify we can decode it back
	decoder := frame.NewDecoder()
	decoded, err := decoder.Decode(resp)
	if err != nil {
		t.Fatalf("Failed to decode generated response: %v", err)
	}

	if decoded.Header.Sequence != 10 {
		t.Errorf("Decoded sequence = %d, want 10", decoded.Header.Sequence)
	}

	var wireMsg starv1.WireMessage
	if err := proto.Unmarshal(decoded.Payload, &wireMsg); err != nil {
		t.Fatalf("Failed to unmarshal payload: %v", err)
	}

	telemetry := wireMsg.GetTelemetryData()
	if telemetry == nil {
		t.Fatal("Payload is not TelemetryData")
	}
}

func TestProcessWireMessage(t *testing.T) {
	// Test VelocityCommand logging (doesn't panic)
	cmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps: 1.0,
		Sequence:             1,
	}
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_VelocityCommand{
			VelocityCommand: cmd,
		},
	}
	processWireMessage(wireMsg)

	// Test unknown message (doesn't panic)
	emptyMsg := &starv1.WireMessage{}
	processWireMessage(emptyMsg)
}
