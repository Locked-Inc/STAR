package starproto_test

import (
	"testing"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// TestVelocityCommand_4Motors verifies that VelocityCommand supports 4 independent motors
func TestVelocityCommand_4Motors(t *testing.T) {
	cmd := &starv1.VelocityCommand{
		Motor_0VelocityMps: 1.5,
		Motor_1VelocityMps: 1.5,
		Motor_2VelocityMps: 1.0,
		Motor_3VelocityMps: 1.0,
		Sequence:           42,
		TimestampUs:        1000000,
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
	if decoded.Motor_0VelocityMps != 1.5 {
		t.Errorf("Motor 0 velocity = %f, want 1.5", decoded.Motor_0VelocityMps)
	}
	if decoded.Motor_1VelocityMps != 1.5 {
		t.Errorf("Motor 1 velocity = %f, want 1.5", decoded.Motor_1VelocityMps)
	}
	if decoded.Motor_2VelocityMps != 1.0 {
		t.Errorf("Motor 2 velocity = %f, want 1.0", decoded.Motor_2VelocityMps)
	}
	if decoded.Motor_3VelocityMps != 1.0 {
		t.Errorf("Motor 3 velocity = %f, want 1.0", decoded.Motor_3VelocityMps)
	}
	if decoded.Sequence != 42 {
		t.Errorf("Sequence = %d, want 42", decoded.Sequence)
	}
	if decoded.TimestampUs != 1000000 {
		t.Errorf("TimestampUs = %d, want 1000000", decoded.TimestampUs)
	}
}

// TestVelocityCommand_DifferentialDrive tests differential drive mode
// where motors 0&1 = left side, motors 2&3 = right side
func TestVelocityCommand_DifferentialDrive(t *testing.T) {
	leftVel := 1.5
	rightVel := 1.0

	cmd := &starv1.VelocityCommand{
		Motor_0VelocityMps: leftVel,  // Left front
		Motor_1VelocityMps: leftVel,  // Left rear
		Motor_2VelocityMps: rightVel, // Right front
		Motor_3VelocityMps: rightVel, // Right rear
		Sequence:           1,
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
	if decoded.Motor_0VelocityMps != decoded.Motor_1VelocityMps {
		t.Error("Left motors (0 & 1) should have same velocity")
	}
	if decoded.Motor_2VelocityMps != decoded.Motor_3VelocityMps {
		t.Error("Right motors (2 & 3) should have same velocity")
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
				Motor_0VelocityMps: tc.velocity,
				Motor_1VelocityMps: tc.velocity,
				Motor_2VelocityMps: tc.velocity,
				Motor_3VelocityMps: tc.velocity,
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
				if decoded.Motor_0VelocityMps != tc.velocity {
					t.Errorf("Velocity = %f, want %f", decoded.Motor_0VelocityMps, tc.velocity)
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
	for motorID := int32(0); motorID < 4; motorID++ {
		status := &starv1.MotorStatus{
			MotorId:           motorID,
			DutyCyclePercent:  75.5,
			VelocityMps:       1.5,
			TargetVelocityMps: 1.5,
			TemperatureCelsius: 45.0,
			CurrentMa:         2500.0,
			FaultFlags:        0,
			State:             starv1.MotorState_MOTOR_STATE_RUNNING,
		}

		data, err := proto.Marshal(status)
		if err != nil {
			t.Fatalf("Motor %d: Marshal failed: %v", motorID, err)
		}

		decoded := &starv1.MotorStatus{}
		if err := proto.Unmarshal(data, decoded); err != nil {
			t.Fatalf("Motor %d: Unmarshal failed: %v", motorID, err)
		}

		if decoded.MotorId != motorID {
			t.Errorf("MotorId = %d, want %d", decoded.MotorId, motorID)
		}
	}
}

// TestMotorPowerCommand_4Motors verifies direct motor power control
func TestMotorPowerCommand_4Motors(t *testing.T) {
	testCases := []struct {
		motorID         int32
		dutyCyclePercent float64
	}{
		{0, 50.0},
		{1, -50.0},
		{2, 100.0},
		{3, -100.0},
	}

	for _, tc := range testCases {
		cmd := &starv1.MotorPowerCommand{
			MotorId:          tc.motorID,
			DutyCyclePercent: tc.dutyCyclePercent,
		}

		data, err := proto.Marshal(cmd)
		if err != nil {
			t.Fatalf("Motor %d: Marshal failed: %v", tc.motorID, err)
		}

		decoded := &starv1.MotorPowerCommand{}
		if err := proto.Unmarshal(data, decoded); err != nil {
			t.Fatalf("Motor %d: Unmarshal failed: %v", tc.motorID, err)
		}

		if decoded.MotorId != tc.motorID {
			t.Errorf("MotorId = %d, want %d", decoded.MotorId, tc.motorID)
		}
		if decoded.DutyCyclePercent != tc.dutyCyclePercent {
			t.Errorf("DutyCyclePercent = %f, want %f", decoded.DutyCyclePercent, tc.dutyCyclePercent)
		}
	}
}

// TestSetVelocityRequest_Roundtrip tests complete request/response flow
func TestSetVelocityRequest_Roundtrip(t *testing.T) {
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-request-123",
		},
		Command: &starv1.VelocityCommand{
			Motor_0VelocityMps: 1.5,
			Motor_1VelocityMps: 1.5,
			Motor_2VelocityMps: 1.0,
			Motor_3VelocityMps: 1.0,
			Sequence:           1,
			TimestampUs:        1000000,
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
	if decoded.Command.Motor_0VelocityMps != 1.5 {
		t.Errorf("Motor 0 velocity = %f, want 1.5", decoded.Command.Motor_0VelocityMps)
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
