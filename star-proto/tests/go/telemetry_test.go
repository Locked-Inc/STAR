// Package starproto_test provides serialization and functionality tests for STAR Protocol Buffer messages.
// Tests verify RX72N telemetry encoding, battery monitoring, encoder data, and safety fields.
package starproto_test

import (
	"testing"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// TestTelemetryData_RX72N_EncoderFields verifies individual encoder fields with semantic naming
func TestTelemetryData_RX72N_EncoderFields(t *testing.T) {
	telem := &starv1.TelemetryData{
		TimestampUs: 1000000,
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       1000,
			VelocityMps: 1.5,
			TimestampUs: 1000000,
		},
		EncoderFrontRight: &starv1.EncoderData{
			MotorId:     1,
			Ticks:       1050,
			VelocityMps: 1.5,
			TimestampUs: 1000000,
		},
		EncoderBackLeft: &starv1.EncoderData{
			MotorId:     2,
			Ticks:       900,
			VelocityMps: 1.0,
			TimestampUs: 1000000,
		},
		EncoderBackRight: &starv1.EncoderData{
			MotorId:     3,
			Ticks:       950,
			VelocityMps: 1.0,
			TimestampUs: 1000000,
		},
		EmergencyStop: false,
		FaultFlags:    0,
	}

	// Serialize
	data, err := proto.Marshal(telem)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	// Verify reasonable size for embedded systems
	if len(data) > 256 {
		t.Errorf("Encoded size %d bytes exceeds 256 byte limit", len(data))
	}

	// Deserialize
	decoded := &starv1.TelemetryData{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	// Verify all 4 encoder fields
	if decoded.EncoderFrontLeft == nil || decoded.EncoderFrontLeft.MotorId != 0 {
		t.Error("Encoder 0 not decoded correctly")
	}
	if decoded.EncoderFrontRight == nil || decoded.EncoderFrontRight.MotorId != 1 {
		t.Error("Encoder 1 not decoded correctly")
	}
	if decoded.EncoderBackLeft == nil || decoded.EncoderBackLeft.MotorId != 2 {
		t.Error("Encoder 2 not decoded correctly")
	}
	if decoded.EncoderBackRight == nil || decoded.EncoderBackRight.MotorId != 3 {
		t.Error("Encoder 3 not decoded correctly")
	}

	// Verify encoder data
	if decoded.EncoderFrontLeft.Ticks != 1000 {
		t.Errorf("Encoder 0 ticks = %d, want 1000", decoded.EncoderFrontLeft.Ticks)
	}
	if decoded.EncoderFrontLeft.VelocityMps != 1.5 {
		t.Errorf("Encoder 0 velocity = %f, want 1.5", decoded.EncoderFrontLeft.VelocityMps)
	}
}

// TestTelemetryData_RX72N_BatteryFields verifies battery telemetry from BQ4050
func TestTelemetryData_RX72N_BatteryFields(t *testing.T) {
	telem := &starv1.TelemetryData{
		BatteryVoltageV:   12.6,
		BatterySocPercent: 85,
		TimestampUs:       1000000,
	}

	data, err := proto.Marshal(telem)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	decoded := &starv1.TelemetryData{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	// Verify battery fields
	if decoded.BatteryVoltageV != 12.6 {
		t.Errorf("BatteryVoltageV = %f, want 12.6", decoded.BatteryVoltageV)
	}
	if decoded.BatterySocPercent != 85 {
		t.Errorf("BatterySocPercent = %d, want 85", decoded.BatterySocPercent)
	}
}

// TestTelemetryData_RX72N_SafetyFields verifies emergency stop and fault flags
func TestTelemetryData_RX72N_SafetyFields(t *testing.T) {
	testCases := []struct {
		name          string
		emergencyStop bool
		faultFlags    uint32
	}{
		{"Normal operation", false, 0},
		{"Emergency stop active", true, 0},
		{"Motor 0 fault", false, 0x01},
		{"Motor 1 fault", false, 0x02},
		{"Motor 2 fault", false, 0x04},
		{"Motor 3 fault", false, 0x08},
		{"All motors fault", false, 0x0F},
		{"E-STOP with faults", true, 0x0F},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			telem := &starv1.TelemetryData{
				EmergencyStop: tc.emergencyStop,
				FaultFlags:    tc.faultFlags,
				TimestampUs:   1000000,
			}

			data, err := proto.Marshal(telem)
			if err != nil {
				t.Fatalf("Marshal failed: %v", err)
			}

			decoded := &starv1.TelemetryData{}
			if err := proto.Unmarshal(data, decoded); err != nil {
				t.Fatalf("Unmarshal failed: %v", err)
			}

			if decoded.EmergencyStop != tc.emergencyStop {
				t.Errorf("EmergencyStop = %v, want %v", decoded.EmergencyStop, tc.emergencyStop)
			}
			if decoded.FaultFlags != tc.faultFlags {
				t.Errorf("FaultFlags = 0x%X, want 0x%X", decoded.FaultFlags, tc.faultFlags)
			}
		})
	}
}

// TestTelemetryData_RX72N_TemperatureField verifies DS18B20 temperature
func TestTelemetryData_RX72N_TemperatureField(t *testing.T) {
	telem := &starv1.TelemetryData{
		TemperatureCelsius: 45.5,
		TimestampUs:        1000000,
	}

	data, err := proto.Marshal(telem)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	decoded := &starv1.TelemetryData{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	if decoded.TemperatureCelsius != 45.5 {
		t.Errorf("TemperatureCelsius = %f, want 45.5", decoded.TemperatureCelsius)
	}
}

// TestTelemetryData_Complete verifies full telemetry message with all RX72N fields
func TestTelemetryData_Complete(t *testing.T) {
	telem := &starv1.TelemetryData{
		// RX72N fields
		TimestampUs:        1000000,
		FrameSequence:      1,
		EmergencyStop:      false,
		FaultFlags:         0,
		BatteryVoltageV:    12.6,
		BatterySocPercent:  85,
		TemperatureCelsius: 45.0,
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       1000,
			VelocityMps: 1.5,
			TimestampUs: 1000000,
		},
		EncoderFrontRight: &starv1.EncoderData{
			MotorId:     1,
			Ticks:       1050,
			VelocityMps: 1.5,
			TimestampUs: 1000000,
		},
		EncoderBackLeft: &starv1.EncoderData{
			MotorId:     2,
			Ticks:       900,
			VelocityMps: 1.0,
			TimestampUs: 1000000,
		},
		EncoderBackRight: &starv1.EncoderData{
			MotorId:     3,
			Ticks:       950,
			VelocityMps: 1.0,
			TimestampUs: 1000000,
		},
	}

	// Serialize
	data, err := proto.Marshal(telem)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}

	// Verify reasonable size
	t.Logf("Complete telemetry message size: %d bytes", len(data))
	if len(data) > 512 {
		t.Errorf("Encoded size %d bytes exceeds 512 byte limit", len(data))
	}

	// Deserialize
	decoded := &starv1.TelemetryData{}
	if err := proto.Unmarshal(data, decoded); err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}

	// Verify critical fields
	if decoded.EmergencyStop != false {
		t.Error("EmergencyStop should be false")
	}
	if decoded.EncoderFrontLeft == nil {
		t.Fatal("EncoderFrontLeft should not be nil")
	}
	if decoded.EncoderFrontLeft.VelocityMps != 1.5 {
		t.Errorf("Encoder 0 velocity = %f, want 1.5", decoded.EncoderFrontLeft.VelocityMps)
	}
	if decoded.BatteryVoltageV != 12.6 {
		t.Errorf("BatteryVoltageV = %f, want 12.6", decoded.BatteryVoltageV)
	}
}

// TestTelemetryData_Streaming simulates high-frequency telemetry streaming (100 Hz)
func TestTelemetryData_Streaming(t *testing.T) {
	const numMessages = 100

	for i := int64(0); i < numMessages; i++ {
		telem := &starv1.TelemetryData{
			TimestampUs: i * 10000, // 10ms intervals (100 Hz)
			EncoderFrontLeft: &starv1.EncoderData{
				MotorId:     0,
				Ticks:       i * 10,
				VelocityMps: 1.5,
				TimestampUs: i * 10000,
			},
			EncoderFrontRight: &starv1.EncoderData{
				MotorId:     1,
				Ticks:       i * 10,
				VelocityMps: 1.5,
				TimestampUs: i * 10000,
			},
			EncoderBackLeft: &starv1.EncoderData{
				MotorId:     2,
				Ticks:       i * 8,
				VelocityMps: 1.0,
				TimestampUs: i * 10000,
			},
			EncoderBackRight: &starv1.EncoderData{
				MotorId:     3,
				Ticks:       i * 8,
				VelocityMps: 1.0,
				TimestampUs: i * 10000,
			},
			EmergencyStop:     false,
			FaultFlags:        0,
			BatteryVoltageV:   12.6,
			BatterySocPercent: 85,
		}

		data, err := proto.Marshal(telem)
		if err != nil {
			t.Fatalf("Message %d: Marshal failed: %v", i, err)
		}

		// Verify consistent encoding size
		if i == 0 {
			t.Logf("Telemetry message size: %d bytes", len(data))
		}
		if len(data) > 256 {
			t.Errorf("Message %d: Size %d bytes exceeds 256 byte limit", i, len(data))
		}

		// Spot check decoding every 10th message
		if i%10 == 0 {
			decoded := &starv1.TelemetryData{}
			if err := proto.Unmarshal(data, decoded); err != nil {
				t.Fatalf("Message %d: Unmarshal failed: %v", i, err)
			}

			if decoded.TimestampUs != i*10000 {
				t.Errorf("Message %d: TimestampUs = %d, want %d", i, decoded.TimestampUs, i*10000)
			}
		}
	}
}

// TestGetTelemetryResponse verifies telemetry service response
func TestGetTelemetryResponse(t *testing.T) {
	t.Skip("GetTelemetryResponse message removed - TelemetryService deleted")
}

// TestStreamTelemetryRequest verifies streaming request configuration
func TestStreamTelemetryRequest(t *testing.T) {
	t.Skip("StreamTelemetryRequest message removed - firmware operates in push mode only")
}
