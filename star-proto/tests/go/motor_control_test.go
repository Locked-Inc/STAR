// Package starproto_test provides serialization and functionality tests for STAR Protocol Buffer messages.
// This file tests motor control commands including velocity commands for 4-motor differential drive.
package starproto_test

import (
	"testing"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// TestVelocityCommand_4Motors verifies that VelocityCommand supports 4 independent motors
func TestVelocityCommand_4Motors(t *testing.T) {
	cmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps:  1.5,
		BackLeftVelocityMps:   1.5,
		FrontRightVelocityMps: 1.0,
		BackRightVelocityMps:  1.0,
		Sequence:              42,
		TimestampUs:           1000000,
	}

	// Serialize
	data, err := proto.Marshal(cmd)
	if err != nil {
		t.Fatalf("Failed to marshal VelocityCommand: %v", err)
	}

	// Verify compact encoding (4 motors should still be small)
	if len(data) > 100 {
		t.Errorf("Encoded size %d bytes is too large (expected < 100)", len(data))
	}

	// Deserialize
	decoded := &starv1.VelocityCommand{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Failed to unmarshal VelocityCommand: %v", err)
	}

	// Verify all 4 motor fields
	if decoded.FrontLeftVelocityMps != 1.5 {
		t.Errorf("Front left velocity = %f, want 1.5", decoded.FrontLeftVelocityMps)
	}
	if decoded.BackLeftVelocityMps != 1.5 {
		t.Errorf("Back left velocity = %f, want 1.5", decoded.BackLeftVelocityMps)
	}
	if decoded.FrontRightVelocityMps != 1.0 {
		t.Errorf("Front right velocity = %f, want 1.0", decoded.FrontRightVelocityMps)
	}
	if decoded.BackRightVelocityMps != 1.0 {
		t.Errorf("Back right velocity = %f, want 1.0", decoded.BackRightVelocityMps)
	}
	if decoded.Sequence != 42 {
		t.Errorf("Sequence = %d, want 42", decoded.Sequence)
	}
	if decoded.TimestampUs != 1000000 {
		t.Errorf("TimestampUs = %d, want 1000000", decoded.TimestampUs)
	}
}

// TestVelocityCommand_DifferentialDrive tests differential drive mode
// where front/back left = left side, front/back right = right side
func TestVelocityCommand_DifferentialDrive(t *testing.T) {
	leftVel := 1.5
	rightVel := 1.0

	cmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps:  leftVel,  // Left front
		BackLeftVelocityMps:   leftVel,  // Left rear
		FrontRightVelocityMps: rightVel, // Right front
		BackRightVelocityMps:  rightVel, // Right rear
		Sequence:              1,
	}

	// Serialize and deserialize
	data, err := proto.Marshal(cmd)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	decoded := &starv1.VelocityCommand{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	// Verify differential drive pairing
	if decoded.FrontLeftVelocityMps != decoded.BackLeftVelocityMps {
		t.Error("Left motors (front & back) should have same velocity")
	}
	if decoded.FrontRightVelocityMps != decoded.BackRightVelocityMps {
		t.Error("Right motors (front & back) should have same velocity")
	}
}

// TestVelocityCommand_VelocityRange tests valid velocity range (±2.0 m/s)
func TestVelocityCommand_VelocityRange(t *testing.T) {
	testCases := []struct {
		name     string
		velocity float64
		valid    bool
	}{
		{"Zero velocity", 0.0, true},
		{"Max forward", 2.0, true},
		{"Max reverse", -2.0, true},
		{"Within range positive", 1.5, true},
		{"Within range negative", -1.5, true},
		{"Exceeds max", 2.5, false},
		{"Exceeds min", -2.5, false},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			cmd := &starv1.VelocityCommand{
				FrontLeftVelocityMps:  tc.velocity,
				BackLeftVelocityMps:   tc.velocity,
				FrontRightVelocityMps: tc.velocity,
				BackRightVelocityMps:  tc.velocity,
			}

			data, err := proto.Marshal(cmd)
			if err != nil {
				t.Fatalf("Marshal failed: %v", err)
			}

			decoded := &starv1.VelocityCommand{}
			if err := proto.Unmarshal(data, decoded); err != nil {
				t.Fatalf("Unmarshal failed: %v", err)
			}

			// Protocol buffers will serialize any value, but application
			// should validate and clamp to ±2.0 m/s range
			if tc.valid {
				if decoded.FrontLeftVelocityMps != tc.velocity {
					t.Errorf("Velocity = %f, want %f", decoded.FrontLeftVelocityMps, tc.velocity)
				}
			}
		})
	}
}

// TestEncoderData_4Motors verifies EncoderData supports motor IDs 0-3
func TestEncoderData_4Motors(t *testing.T) {
	testCases := []struct {
		motorID     int32
		ticks       int64
		velocityMps float64
	}{
		{0, 1000, 1.5},
		{1, 2000, 1.2},
		{2, 1500, 1.0},
		{3, 1800, 0.8},
	}

	for _, tc := range testCases {
		encoder := &starv1.EncoderData{
			MotorId:     tc.motorID,
			Ticks:       tc.ticks,
			VelocityMps: tc.velocityMps,
			TimestampUs: 1000000,
		}

		// Serialize
		data, err := proto.Marshal(encoder)
		if err != nil {
			t.Fatalf("Motor %d: Marshal failed: %v", tc.motorID, err)
		}

		// Deserialize
		decoded := &starv1.EncoderData{}
		if err := proto.Unmarshal(data, decoded); err != nil {
			t.Fatalf("Motor %d: Unmarshal failed: %v", tc.motorID, err)
		}

		// Verify fields
		if decoded.MotorId != tc.motorID {
			t.Errorf("MotorId = %d, want %d", decoded.MotorId, tc.motorID)
		}
		if decoded.Ticks != tc.ticks {
			t.Errorf("Ticks = %d, want %d", decoded.Ticks, tc.ticks)
		}
		if decoded.VelocityMps != tc.velocityMps {
			t.Errorf("VelocityMps = %f, want %f", decoded.VelocityMps, tc.velocityMps)
		}
	}
}

// TestMotorStatus_4Motors verifies MotorStatus supports motor IDs 0-3
func TestMotorStatus_4Motors(t *testing.T) {
	t.Skip("MotorStatus message removed - unused in firmware")
}

// TestMotorPowerCommand_4Motors verifies direct motor power control
func TestMotorPowerCommand_4Motors(t *testing.T) {
	t.Skip("MotorPowerCommand message removed - unused in firmware")
}

// TestSetVelocityRequest_Roundtrip tests complete request/response flow
func TestSetVelocityRequest_Roundtrip(t *testing.T) {
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-request-123",
		},
		Command: &starv1.VelocityCommand{
			FrontLeftVelocityMps:  1.5,
			BackLeftVelocityMps:   1.5,
			FrontRightVelocityMps: 1.0,
			BackRightVelocityMps:  1.0,
			Sequence:              1,
			TimestampUs:           1000000,
		},
	}

	data, err := proto.Marshal(req)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	decoded := &starv1.SetVelocityRequest{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	if decoded.Header.RequestId != "test-request-123" {
		t.Errorf("RequestId = %s, want test-request-123", decoded.Header.RequestId)
	}
	if decoded.Command.FrontLeftVelocityMps != 1.5 {
		t.Errorf("Front left velocity = %f, want 1.5", decoded.Command.FrontLeftVelocityMps)
	}
}

// TestEmergencyStopRequest verifies E-STOP request encoding
func TestEmergencyStopRequest(t *testing.T) {
	req := &starv1.EmergencyStopRequest{
		Header: &starv1.RequestHeader{
			RequestId: "estop-request-1",
		},
		Reason: "User initiated emergency stop",
	}

	data, err := proto.Marshal(req)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	decoded := &starv1.EmergencyStopRequest{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	if decoded.Reason != "User initiated emergency stop" {
		t.Errorf("Reason = %s, want 'User initiated emergency stop'", decoded.Reason)
	}
}
