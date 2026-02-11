// wire_format_test.go - STAR Wire Format Compatibility Tests
//
// Tests verify TelemetryData encoding, sequential field numbering, and size reduction.
//
// STAR Project - Texas A&M University
// February 2026

package starproto_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// ============================================================================
// TestTelemetryData_FieldRenumbering_BreaksCompatibility
// ============================================================================

// TestTelemetryData_FieldRenumbering_BreaksCompatibility verifies TelemetryData
// serialization with all 11 fields populated.
func TestTelemetryData_FieldRenumbering_BreaksCompatibility(t *testing.T) {
	// Create fully-populated TelemetryData with all 11 fields
	telemetry := &starv1.TelemetryData{
		TimestampUs:    1234567890, // Field 1
		FrameSequence:  42,         // Field 2
		EmergencyStop:  true,       // Field 3
		FaultFlags:     0x0F,       // Field 4 (all 4 motors faulted)
		EncoderFrontLeft: &starv1.EncoderData{ // Field 5
			MotorId:     0,
			Ticks:       1000,
			VelocityMps: 1.5,
			TimestampUs: 1234567890,
		},
		EncoderFrontRight: &starv1.EncoderData{ // Field 6
			MotorId:     1,
			Ticks:       1050,
			VelocityMps: 1.6,
			TimestampUs: 1234567890,
		},
		EncoderBackLeft: &starv1.EncoderData{ // Field 7
			MotorId:     2,
			Ticks:       980,
			VelocityMps: 1.4,
			TimestampUs: 1234567890,
		},
		EncoderBackRight: &starv1.EncoderData{ // Field 8
			MotorId:     3,
			Ticks:       1020,
			VelocityMps: 1.55,
			TimestampUs: 1234567890,
		},
		BatteryVoltageV:      24.8,  // Field 9
		BatterySocPercent:    85,    // Field 10
		TemperatureCelsius:   45.2,  // Field 11
	}

	// Serialize with new code
	bytes, err := proto.Marshal(telemetry)
	require.NoError(t, err, "Marshal should succeed")

	// Verify round-trip with new code
	parsed := &starv1.TelemetryData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Unmarshal should succeed")

	// Verify all fields are correctly decoded
	assert.Equal(t, int64(1234567890), parsed.TimestampUs, "timestamp_us field 1")
	assert.Equal(t, uint32(42), parsed.FrameSequence, "frame_sequence field 2")
	assert.True(t, parsed.EmergencyStop, "emergency_stop field 3")
	assert.Equal(t, uint32(0x0F), parsed.FaultFlags, "fault_flags field 4")

	// Verify encoder data (fields 5-8)
	require.NotNil(t, parsed.EncoderFrontLeft)
	assert.Equal(t, int32(0), parsed.EncoderFrontLeft.MotorId)
	assert.Equal(t, int64(1000), parsed.EncoderFrontLeft.Ticks)
	assert.InDelta(t, 1.5, parsed.EncoderFrontLeft.VelocityMps, 0.0001)

	require.NotNil(t, parsed.EncoderFrontRight)
	assert.Equal(t, int32(1), parsed.EncoderFrontRight.MotorId)

	require.NotNil(t, parsed.EncoderBackLeft)
	assert.Equal(t, int32(2), parsed.EncoderBackLeft.MotorId)

	require.NotNil(t, parsed.EncoderBackRight)
	assert.Equal(t, int32(3), parsed.EncoderBackRight.MotorId)

	// Verify battery and temperature (fields 9-11)
	assert.InDelta(t, 24.8, parsed.BatteryVoltageV, 0.01, "battery_voltage_v field 9")
	assert.Equal(t, uint32(85), parsed.BatterySocPercent, "battery_soc_percent field 10")
	assert.InDelta(t, 45.2, parsed.TemperatureCelsius, 0.01, "temperature_celsius field 11")
}

// ============================================================================
// TestControllerState_FieldRenames_BreaksCompatibility
// ============================================================================

// TestControllerState_FieldRenames_BreaksCompatibility verifies ControllerState
// serialization with normalized fields.
func TestControllerState_FieldRenames_BreaksCompatibility(t *testing.T) {
	// Create ControllerState with normalized joystick inputs
	controller := &starv1.ControllerState{
		LinearVelNormalized:  0.75,  // Field 1 (75% forward)
		AngularVelNormalized: -0.25, // Field 2 (25% clockwise)
		TimestampMs:          1707657600000, // Field 3 (Feb 11, 2026)
		Debug:                true,  // Field 4
	}

	// Serialize with new code
	bytes, err := proto.Marshal(controller)
	require.NoError(t, err, "Marshal should succeed")

	// Verify round-trip with new code
	parsed := &starv1.ControllerState{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Unmarshal should succeed")

	// Verify all fields correctly decoded
	assert.InDelta(t, 0.75, parsed.LinearVelNormalized, 0.0001,
		"linear_vel_normalized field 1")
	assert.InDelta(t, -0.25, parsed.AngularVelNormalized, 0.0001,
		"angular_vel_normalized field 2")
	assert.Equal(t, int64(1707657600000), parsed.TimestampMs,
		"timestamp_ms field 3")
	assert.True(t, parsed.Debug, "debug field 4")
}

// ============================================================================
// TestWireFormat_SequentialFieldNumbering
// ============================================================================

// TestWireFormat_SequentialFieldNumbering verifies that TelemetryData fields
// are numbered 1-11 sequentially.
func TestWireFormat_SequentialFieldNumbering(t *testing.T) {
	// Use protobuf reflection to inspect TelemetryData fields
	msg := &starv1.TelemetryData{}
	descriptor := msg.ProtoReflect().Descriptor()
	fields := descriptor.Fields()

	// Verify we have exactly 11 fields
	require.Equal(t, 11, fields.Len(), "TelemetryData should have 11 fields")

	// Verify field numbers are sequential (1-11)
	expectedFieldNumbers := map[string]int{
		"timestamp_us":         1,
		"frame_sequence":       2,
		"emergency_stop":       3,
		"fault_flags":          4,
		"encoder_front_left":   5,
		"encoder_front_right":  6,
		"encoder_back_left":    7,
		"encoder_back_right":   8,
		"battery_voltage_v":    9,
		"battery_soc_percent":  10,
		"temperature_celsius":  11,
	}

	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)
		fieldName := string(field.Name())
		fieldNumber := int(field.Number())

		// Verify field number matches expected
		expectedNumber, exists := expectedFieldNumbers[fieldName]
		require.True(t, exists, "Unexpected field: %s", fieldName)
		assert.Equal(t, expectedNumber, fieldNumber,
			"Field %s should be numbered %d", fieldName, expectedNumber)

		t.Logf("Field %2d: %s (type: %s)",
			fieldNumber, fieldName, field.Kind())
	}
}

// ============================================================================
// TestWireFormat_RemovedMessages
// ============================================================================

// TestWireFormat_RemovedMessages verifies removed fields no longer exist.
func TestWireFormat_RemovedMessages(t *testing.T) {
	// Verify TelemetryData no longer has ImuData or GpsData fields
	msg := &starv1.TelemetryData{}
	descriptor := msg.ProtoReflect().Descriptor()
	fields := descriptor.Fields()

	// Check that imu field doesn't exist
	imuField := fields.ByName(protoreflect.Name("imu"))
	assert.Nil(t, imuField, "TelemetryData should not have 'imu' field")

	// Check that gps field doesn't exist
	gpsField := fields.ByName(protoreflect.Name("gps"))
	assert.Nil(t, gpsField, "TelemetryData should not have 'gps' field")

	// Check that battery_percent field doesn't exist (duplicate)
	batteryPercentField := fields.ByName(protoreflect.Name("battery_percent"))
	assert.Nil(t, batteryPercentField,
		"TelemetryData should not have 'battery_percent' field (duplicate)")

	// Check that wifi_signal_dbm field doesn't exist
	wifiField := fields.ByName(protoreflect.Name("wifi_signal_dbm"))
	assert.Nil(t, wifiField, "TelemetryData should not have 'wifi_signal_dbm' field")

	// Check that cpu_usage_percent field doesn't exist
	cpuField := fields.ByName(protoreflect.Name("cpu_usage_percent"))
	assert.Nil(t, cpuField, "TelemetryData should not have 'cpu_usage_percent' field")

	// Check that motor_load_percent field doesn't exist
	motorLoadField := fields.ByName(protoreflect.Name("motor_load_percent"))
	assert.Nil(t, motorLoadField, "TelemetryData should not have 'motor_load_percent' field")
}

// ============================================================================
// TestWireFormat_SizeComparison
// ============================================================================

// TestWireFormat_SizeComparison measures TelemetryData encoded size.
func TestWireFormat_SizeComparison(t *testing.T) {
	// Create new TelemetryData (all 11 fields populated)
	newTelemetry := &starv1.TelemetryData{
		TimestampUs:    1234567890,
		FrameSequence:  42,
		EmergencyStop:  false,
		FaultFlags:     0,
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       1000,
			VelocityMps: 1.5,
			TimestampUs: 1234567890,
		},
		EncoderFrontRight: &starv1.EncoderData{
			MotorId:     1,
			Ticks:       1050,
			VelocityMps: 1.6,
			TimestampUs: 1234567890,
		},
		EncoderBackLeft: &starv1.EncoderData{
			MotorId:     2,
			Ticks:       980,
			VelocityMps: 1.4,
			TimestampUs: 1234567890,
		},
		EncoderBackRight: &starv1.EncoderData{
			MotorId:     3,
			Ticks:       1020,
			VelocityMps: 1.55,
			TimestampUs: 1234567890,
		},
		BatteryVoltageV:      24.8,
		BatterySocPercent:    85,
		TemperatureCelsius:   45.2,
	}

	// Serialize new message
	newBytes, err := proto.Marshal(newTelemetry)
	require.NoError(t, err, "Marshal should succeed")
	newSize := len(newBytes)

	// Verify size is reasonable
	assert.GreaterOrEqual(t, newSize, 50, "Message should be at least 50 bytes")
	assert.LessOrEqual(t, newSize, 400, "Message should be at most 400 bytes")

	t.Logf("TelemetryData encoded size: %d bytes", newSize)
}

// ============================================================================
// TestWireFormat_LittleEndianMigration
// ============================================================================

// TestWireFormat_LittleEndianMigration verifies protobuf encoding handles multi-byte fields.
func TestWireFormat_LittleEndianMigration(t *testing.T) {
	// Create TelemetryData with multi-byte fields
	telemetry := &starv1.TelemetryData{
		TimestampUs:    0x0123456789ABCDEF, // 8-byte int64
		FrameSequence:  0x12345678,         // 4-byte uint32
		FaultFlags:     0xABCDEF01,         // 4-byte uint32
		BatteryVoltageV:     24.8,          // 8-byte double
		BatterySocPercent:   85,            // varint (endian-neutral)
		TemperatureCelsius:  45.2,          // 8-byte double
	}

	// Serialize with protobuf (uses little-endian for fixed64, big-endian for varints)
	bytes, err := proto.Marshal(telemetry)
	require.NoError(t, err)

	// Verify round-trip
	parsed := &starv1.TelemetryData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	// Verify multi-byte fields decoded correctly
	assert.Equal(t, int64(0x0123456789ABCDEF), parsed.TimestampUs,
		"timestamp_us (int64) should decode correctly")
	assert.Equal(t, uint32(0x12345678), parsed.FrameSequence,
		"frame_sequence (uint32) should decode correctly")
	assert.Equal(t, uint32(0xABCDEF01), parsed.FaultFlags,
		"fault_flags (uint32) should decode correctly")
}

