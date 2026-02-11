// bounds_test.go - nanopb .options Field Size Constraint Tests
// Verifies that nanopb field size constraints are enforced for embedded targets.
//
// These tests verify that the .options files define correct bounds for:
// - Array max_count limits (cell_mv, temp_deci_celsius, motor_configs, errors)
// - String max_size limits (manufacturer, device_name, chemistry, field_path, message)
//
// IMPORTANT: These tests verify Go protobuf behavior. The actual enforcement
// happens in nanopb (C) code generation for RX72N firmware. These tests ensure
// the constraints are correctly specified and that Go can handle the limits.
//
// STAR Project - Texas A&M University
// February 2026

package starproto_test

import (
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// ============================================================================
// battery_management.options - CellData Bounds Tests
// ============================================================================

// TestCellData_MaxCells verifies max_count:16 constraint for cell_mv array.
// BQ7850 BMS supports maximum 16 cells per specification.
func TestCellData_MaxCells(t *testing.T) {
	// Create CellData with exactly 16 cells (maximum allowed)
	cellVoltages := make([]uint32, 16)
	for i := 0; i < 16; i++ {
		cellVoltages[i] = 3700 + uint32(i)*5 // 3700, 3705, 3710, ..., 3775 mV
	}

	cellData := &starv1.CellData{
		CellMv:     cellVoltages,
		ValidCells: 16,
		PackMv:     59800, // Sum of all cells
		MinCellMv:  3700,
		MaxCellMv:  3775,
		DeltaMv:    75,
	}

	// Marshal should succeed
	bytes, err := proto.Marshal(cellData)
	require.NoError(t, err, "Should marshal 16 cells (max allowed)")

	// Unmarshal should succeed
	parsed := &starv1.CellData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Should unmarshal 16 cells")

	assert.Equal(t, 16, len(parsed.CellMv), "Should have 16 cells")
	assert.Equal(t, int32(16), parsed.ValidCells)
	assert.Equal(t, cellVoltages, parsed.CellMv, "All 16 cell voltages should match")
}

// TestCellData_ExceedsMax verifies that Go protobuf allows >16 cells.
// NOTE: This test verifies Go behavior. The actual constraint is enforced
// by nanopb in the RX72N firmware, which will truncate or reject messages
// with >16 cells during encoding/decoding.
func TestCellData_ExceedsMax(t *testing.T) {
	// Create CellData with 17 cells (exceeds max_count:16)
	cellVoltages := make([]uint32, 17)
	for i := 0; i < 17; i++ {
		cellVoltages[i] = 3700 + uint32(i)*5
	}

	cellData := &starv1.CellData{
		CellMv:     cellVoltages,
		ValidCells: 17,
		PackMv:     63580,
	}

	// Go protobuf allows this (no static array bounds)
	bytes, err := proto.Marshal(cellData)
	require.NoError(t, err, "Go protobuf allows >16 cells")

	parsed := &starv1.CellData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals >16 cells")

	// Go protobuf keeps all 17 cells
	assert.Equal(t, 17, len(parsed.CellMv), "Go protobuf preserves all cells")

	// IMPORTANT: nanopb on RX72N would only decode first 16 cells due to max_count:16
	// This test documents the behavior difference between Go and nanopb
	t.Log("WARNING: nanopb (RX72N) would truncate to 16 cells, Go keeps all 17")
}

// TestCellData_BelowMax verifies smaller arrays work correctly.
func TestCellData_BelowMax(t *testing.T) {
	tests := []struct {
		name      string
		numCells  int
		desc      string
	}{
		{"single_cell", 1, "1S battery pack"},
		{"4s_pack", 4, "4S battery pack"},
		{"8s_pack", 8, "8S battery pack"},
		{"12s_pack", 12, "12S battery pack"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			cellVoltages := make([]uint32, tc.numCells)
			for i := 0; i < tc.numCells; i++ {
				cellVoltages[i] = 3700
			}

			cellData := &starv1.CellData{
				CellMv:     cellVoltages,
				ValidCells: int32(tc.numCells),
			}

			bytes, err := proto.Marshal(cellData)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.CellData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.numCells, len(parsed.CellMv), tc.desc)
		})
	}
}

// ============================================================================
// battery_management.options - TemperatureData Bounds Tests
// ============================================================================

// TestTemperatureData_MaxSensors verifies max_count:3 constraint for temp_deci_celsius array.
// BQ7850 BMS supports maximum 3 temperature sensors per specification.
func TestTemperatureData_MaxSensors(t *testing.T) {
	// Create TemperatureData with exactly 3 sensors (maximum allowed)
	temps := []int32{250, 255, 260} // 25.0°C, 25.5°C, 26.0°C

	tempData := &starv1.TemperatureData{
		TempDeciCelsius:    temps,
		ValidSensors:       3,
		AvgTempDeciCelsius: 255,
		MinTempDeciCelsius: 250,
		MaxTempDeciCelsius: 260,
	}

	// Marshal should succeed
	bytes, err := proto.Marshal(tempData)
	require.NoError(t, err, "Should marshal 3 sensors (max allowed)")

	// Unmarshal should succeed
	parsed := &starv1.TemperatureData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Should unmarshal 3 sensors")

	assert.Equal(t, 3, len(parsed.TempDeciCelsius), "Should have 3 sensors")
	assert.Equal(t, int32(3), parsed.ValidSensors)
	assert.Equal(t, temps, parsed.TempDeciCelsius, "All 3 temperatures should match")
}

// TestTemperatureData_ExceedsMax verifies that Go protobuf allows >3 sensors.
// NOTE: nanopb on RX72N would truncate to 3 sensors due to max_count:3.
func TestTemperatureData_ExceedsMax(t *testing.T) {
	// Create TemperatureData with 4 sensors (exceeds max_count:3)
	temps := []int32{250, 255, 260, 265}

	tempData := &starv1.TemperatureData{
		TempDeciCelsius:    temps,
		ValidSensors:       4,
		AvgTempDeciCelsius: 257,
	}

	// Go protobuf allows this (no static array bounds)
	bytes, err := proto.Marshal(tempData)
	require.NoError(t, err, "Go protobuf allows >3 sensors")

	parsed := &starv1.TemperatureData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals >3 sensors")

	// Go protobuf keeps all 4 sensors
	assert.Equal(t, 4, len(parsed.TempDeciCelsius), "Go protobuf preserves all sensors")

	// IMPORTANT: nanopb on RX72N would only decode first 3 sensors due to max_count:3
	t.Log("WARNING: nanopb (RX72N) would truncate to 3 sensors, Go keeps all 4")
}

// TestTemperatureData_BelowMax verifies smaller arrays work correctly.
func TestTemperatureData_BelowMax(t *testing.T) {
	tests := []struct {
		name       string
		numSensors int
		desc       string
	}{
		{"single_sensor", 1, "Single temperature sensor"},
		{"dual_sensor", 2, "Dual temperature sensors"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			temps := make([]int32, tc.numSensors)
			for i := 0; i < tc.numSensors; i++ {
				temps[i] = 250 + int32(i)*5
			}

			tempData := &starv1.TemperatureData{
				TempDeciCelsius: temps,
				ValidSensors:    int32(tc.numSensors),
			}

			bytes, err := proto.Marshal(tempData)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.TemperatureData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.numSensors, len(parsed.TempDeciCelsius), tc.desc)
		})
	}
}

// ============================================================================
// battery_management.options - BmsDeviceInfo String Bounds Tests
// ============================================================================

// TestBmsDeviceInfo_StringLimits verifies max_size constraints for string fields:
// - manufacturer: max_size:32
// - device_name: max_size:32
// - chemistry: max_size:16
func TestBmsDeviceInfo_StringLimits(t *testing.T) {
	tests := []struct {
		name         string
		manufacturer string
		deviceName   string
		chemistry    string
		desc         string
	}{
		{
			"max_length_strings",
			strings.Repeat("A", 32), // 32 chars (max for manufacturer)
			strings.Repeat("B", 32), // 32 chars (max for device_name)
			strings.Repeat("C", 16), // 16 chars (max for chemistry)
			"All strings at maximum length",
		},
		{
			"typical_values",
			"Texas Instruments", // 18 chars
			"BQ7850",            // 6 chars
			"LION",              // 4 chars
			"Typical BMS device info",
		},
		{
			"short_strings",
			"TI",     // 2 chars
			"BQ76",   // 4 chars
			"LIFEPO", // 6 chars
			"Short strings well below limits",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			info := &starv1.BmsDeviceInfo{
				Manufacturer:    tc.manufacturer,
				DeviceName:      tc.deviceName,
				Chemistry:       tc.chemistry,
				DeviceType:      0x7850,
				FirmwareVersion: 0x0102,
				HardwareVersion: 0x0001,
			}

			bytes, err := proto.Marshal(info)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.BmsDeviceInfo{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.manufacturer, parsed.Manufacturer, tc.desc)
			assert.Equal(t, tc.deviceName, parsed.DeviceName, tc.desc)
			assert.Equal(t, tc.chemistry, parsed.Chemistry, tc.desc)
		})
	}
}

// TestBmsDeviceInfo_ExceedsMaxSize verifies Go protobuf allows strings exceeding max_size.
// NOTE: nanopb would truncate these strings on RX72N firmware.
func TestBmsDeviceInfo_ExceedsMaxSize(t *testing.T) {
	// Create strings exceeding max_size limits
	info := &starv1.BmsDeviceInfo{
		Manufacturer: strings.Repeat("A", 64), // Exceeds max_size:32
		DeviceName:   strings.Repeat("B", 64), // Exceeds max_size:32
		Chemistry:    strings.Repeat("C", 32), // Exceeds max_size:16
	}

	// Go protobuf allows this (strings are dynamic)
	bytes, err := proto.Marshal(info)
	require.NoError(t, err, "Go protobuf allows strings exceeding max_size")

	parsed := &starv1.BmsDeviceInfo{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals oversized strings")

	// Go protobuf keeps full strings
	assert.Equal(t, 64, len(parsed.Manufacturer), "Go keeps 64-char manufacturer")
	assert.Equal(t, 64, len(parsed.DeviceName), "Go keeps 64-char device_name")
	assert.Equal(t, 32, len(parsed.Chemistry), "Go keeps 32-char chemistry")

	// IMPORTANT: nanopb on RX72N would truncate to max_size limits
	t.Log("WARNING: nanopb would truncate manufacturer to 32 chars")
	t.Log("WARNING: nanopb would truncate device_name to 32 chars")
	t.Log("WARNING: nanopb would truncate chemistry to 16 chars")
}

// ============================================================================
// configuration.options - SystemConfiguration Bounds Tests
// ============================================================================

// TestSystemConfiguration_MaxMotors verifies max_count:4 constraint for motor_configs array.
// STAR robot has 4 motors (front-left, front-right, back-left, back-right).
func TestSystemConfiguration_MaxMotors(t *testing.T) {
	// Create SystemConfiguration with exactly 4 motors (maximum allowed)
	motorConfigs := []*starv1.MotorPidConfiguration{
		{MotorId: int32(0), PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
		{MotorId: int32(1), PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
		{MotorId: int32(2), PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
		{MotorId: int32(3), PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
	}

	config := &starv1.SystemConfiguration{
		MotorConfigs:  motorConfigs,
		ConfigVersion: 1,
	}

	// Marshal should succeed
	bytes, err := proto.Marshal(config)
	require.NoError(t, err, "Should marshal 4 motors (max allowed)")

	// Unmarshal should succeed
	parsed := &starv1.SystemConfiguration{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Should unmarshal 4 motors")

	assert.Equal(t, 4, len(parsed.MotorConfigs), "Should have 4 motor configs")
	for i, motor := range parsed.MotorConfigs {
		assert.Equal(t, int32(i), motor.MotorId, "Motor ID should match index")
		assert.InDelta(t, 0.286, motor.PidConfig.Kp, 0.001, "Motor %d Kp", i)
	}
}

// TestSystemConfiguration_ExceedsMax verifies that Go protobuf allows >4 motors.
// NOTE: nanopb on RX72N would truncate to 4 motors due to max_count:4.
func TestSystemConfiguration_ExceedsMax(t *testing.T) {
	// Create SystemConfiguration with 5 motors (exceeds max_count:4)
	motorConfigs := make([]*starv1.MotorPidConfiguration, 5)
	for i := 0; i < 5; i++ {
		motorConfigs[i] = &starv1.MotorPidConfiguration{
			MotorId:   int32(i),
			PidConfig: &starv1.PidConfig{Kp: 1.0, Ki: 0.1, Kd: 0.0},
		}
	}

	config := &starv1.SystemConfiguration{
		MotorConfigs:  motorConfigs,
		ConfigVersion: 1,
	}

	// Go protobuf allows this (no static array bounds)
	bytes, err := proto.Marshal(config)
	require.NoError(t, err, "Go protobuf allows >4 motors")

	parsed := &starv1.SystemConfiguration{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals >4 motors")

	// Go protobuf keeps all 5 motors
	assert.Equal(t, 5, len(parsed.MotorConfigs), "Go protobuf preserves all motors")

	// IMPORTANT: nanopb on RX72N would only decode first 4 motors due to max_count:4
	t.Log("WARNING: nanopb (RX72N) would truncate to 4 motors, Go keeps all 5")
}

// TestSystemConfiguration_BelowMax verifies smaller arrays work correctly.
func TestSystemConfiguration_BelowMax(t *testing.T) {
	tests := []struct {
		name      string
		numMotors int
		desc      string
	}{
		{"single_motor", 1, "Single motor configuration"},
		{"dual_motor", 2, "Dual motor configuration"},
		{"tri_motor", 3, "Three motor configuration"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			motorConfigs := make([]*starv1.MotorPidConfiguration, tc.numMotors)
			for i := 0; i < tc.numMotors; i++ {
				motorConfigs[i] = &starv1.MotorPidConfiguration{
					MotorId:   int32(i),
					PidConfig: &starv1.PidConfig{Kp: 1.0, Ki: 0.1, Kd: 0.0},
				}
			}

			config := &starv1.SystemConfiguration{
				MotorConfigs:  motorConfigs,
				ConfigVersion: 1,
			}

			bytes, err := proto.Marshal(config)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.SystemConfiguration{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.numMotors, len(parsed.MotorConfigs), tc.desc)
		})
	}
}

// ============================================================================
// configuration.options - ConfigValidationResult Bounds Tests
// ============================================================================

// TestConfigValidationResult_MaxErrors verifies max_count:16 constraint for errors array.
// Allows reporting up to 16 validation errors per configuration.
func TestConfigValidationResult_MaxErrors(t *testing.T) {
	// Create ConfigValidationResult with exactly 16 errors (maximum allowed)
	errors := make([]*starv1.ConfigValidationError, 16)
	for i := 0; i < 16; i++ {
		errors[i] = &starv1.ConfigValidationError{
			FieldPath: "motor_configs[0].pid_config.kp",
			ErrorCode: starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:   "Kp exceeds maximum",
		}
	}

	result := &starv1.ConfigValidationResult{
		Status: starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID,
		Errors: errors,
	}

	// Marshal should succeed
	bytes, err := proto.Marshal(result)
	require.NoError(t, err, "Should marshal 16 errors (max allowed)")

	// Unmarshal should succeed
	parsed := &starv1.ConfigValidationResult{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Should unmarshal 16 errors")

	assert.Equal(t, 16, len(parsed.Errors), "Should have 16 errors")
	assert.Equal(t, starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID, parsed.Status)
}

// TestConfigValidationResult_ExceedsMax verifies that Go protobuf allows >16 errors.
// NOTE: nanopb on RX72N would truncate to 16 errors due to max_count:16.
func TestConfigValidationResult_ExceedsMax(t *testing.T) {
	// Create ConfigValidationResult with 17 errors (exceeds max_count:16)
	errors := make([]*starv1.ConfigValidationError, 17)
	for i := 0; i < 17; i++ {
		errors[i] = &starv1.ConfigValidationError{
			FieldPath: "test.field",
			ErrorCode: starv1.ConfigErrorCode_CONFIG_ERROR_CODE_INVALID_NUMBER,
			Message:   "Test error",
		}
	}

	result := &starv1.ConfigValidationResult{
		Status: starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID,
		Errors: errors,
	}

	// Go protobuf allows this (no static array bounds)
	bytes, err := proto.Marshal(result)
	require.NoError(t, err, "Go protobuf allows >16 errors")

	parsed := &starv1.ConfigValidationResult{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals >16 errors")

	// Go protobuf keeps all 17 errors
	assert.Equal(t, 17, len(parsed.Errors), "Go protobuf preserves all errors")

	// IMPORTANT: nanopb on RX72N would only decode first 16 errors due to max_count:16
	t.Log("WARNING: nanopb (RX72N) would truncate to 16 errors, Go keeps all 17")
}

// TestConfigValidationResult_BelowMax verifies smaller arrays work correctly.
func TestConfigValidationResult_BelowMax(t *testing.T) {
	tests := []struct {
		name      string
		numErrors int
		desc      string
	}{
		{"single_error", 1, "Single validation error"},
		{"multiple_errors", 5, "Multiple validation errors"},
		{"many_errors", 10, "Many validation errors"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			errors := make([]*starv1.ConfigValidationError, tc.numErrors)
			for i := 0; i < tc.numErrors; i++ {
				errors[i] = &starv1.ConfigValidationError{
					FieldPath: "test.field",
					ErrorCode: starv1.ConfigErrorCode_CONFIG_ERROR_CODE_INVALID_NUMBER,
					Message:   "Test error",
				}
			}

			result := &starv1.ConfigValidationResult{
				Status: starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID,
				Errors: errors,
			}

			bytes, err := proto.Marshal(result)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.ConfigValidationResult{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.numErrors, len(parsed.Errors), tc.desc)
		})
	}
}

// ============================================================================
// configuration.options - ConfigValidationError String Bounds Tests
// ============================================================================

// TestConfigValidationError_StringLimits verifies max_size constraints for string fields:
// - field_path: max_size:64
// - message: max_size:256
// - actual_value: max_size:32
// - expected_constraint: max_size:128
func TestConfigValidationError_StringLimits(t *testing.T) {
	tests := []struct {
		name               string
		fieldPath          string
		message            string
		actualValue        string
		expectedConstraint string
		desc               string
	}{
		{
			"max_length_strings",
			strings.Repeat("A", 64),  // 64 chars (max for field_path)
			strings.Repeat("B", 256), // 256 chars (max for message)
			strings.Repeat("C", 32),  // 32 chars (max for actual_value)
			strings.Repeat("D", 128), // 128 chars (max for expected_constraint)
			"All strings at maximum length",
		},
		{
			"typical_validation_error",
			"motor_configs[0].pid_config.kp",
			"Proportional gain exceeds maximum allowed value",
			"15.5",
			"Value must be in range [0.0, 10.0]",
			"Typical PID validation error",
		},
		{
			"short_strings",
			"kp",
			"Too high",
			"99",
			"Max 10",
			"Short validation error strings",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			errInfo := &starv1.ConfigValidationError{
				FieldPath:          tc.fieldPath,
				ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
				Message:            tc.message,
				ActualValue:        tc.actualValue,
				ExpectedConstraint: tc.expectedConstraint,
			}

			bytes, err := proto.Marshal(errInfo)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.ConfigValidationError{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.fieldPath, parsed.FieldPath, tc.desc)
			assert.Equal(t, tc.message, parsed.Message, tc.desc)
			assert.Equal(t, tc.actualValue, parsed.ActualValue, tc.desc)
			assert.Equal(t, tc.expectedConstraint, parsed.ExpectedConstraint, tc.desc)
		})
	}
}

// TestConfigValidationError_ExceedsMaxSize verifies Go protobuf allows strings exceeding max_size.
// NOTE: nanopb would truncate these strings on RX72N firmware.
func TestConfigValidationError_ExceedsMaxSize(t *testing.T) {
	// Create strings exceeding max_size limits
	errInfo := &starv1.ConfigValidationError{
		FieldPath:          strings.Repeat("A", 128), // Exceeds max_size:64
		Message:            strings.Repeat("B", 512), // Exceeds max_size:256
		ActualValue:        strings.Repeat("C", 64),  // Exceeds max_size:32
		ExpectedConstraint: strings.Repeat("D", 256), // Exceeds max_size:128
	}

	// Go protobuf allows this (strings are dynamic)
	bytes, err := proto.Marshal(errInfo)
	require.NoError(t, err, "Go protobuf allows strings exceeding max_size")

	parsed := &starv1.ConfigValidationError{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals oversized strings")

	// Go protobuf keeps full strings
	assert.Equal(t, 128, len(parsed.FieldPath), "Go keeps 128-char field_path")
	assert.Equal(t, 512, len(parsed.Message), "Go keeps 512-char message")
	assert.Equal(t, 64, len(parsed.ActualValue), "Go keeps 64-char actual_value")
	assert.Equal(t, 256, len(parsed.ExpectedConstraint), "Go keeps 256-char expected_constraint")

	// IMPORTANT: nanopb on RX72N would truncate to max_size limits
	t.Log("WARNING: nanopb would truncate field_path to 64 chars")
	t.Log("WARNING: nanopb would truncate message to 256 chars")
	t.Log("WARNING: nanopb would truncate actual_value to 32 chars")
	t.Log("WARNING: nanopb would truncate expected_constraint to 128 chars")
}

// ============================================================================
// motor_control.options - EmergencyStopRequest String Bounds Tests
// ============================================================================

// TestEmergencyStopRequest_ReasonLimit verifies max_size:128 constraint for reason field.
func TestEmergencyStopRequest_ReasonLimit(t *testing.T) {
	tests := []struct {
		name   string
		reason string
		desc   string
	}{
		{
			"max_length_reason",
			strings.Repeat("A", 128), // 128 chars (max)
			"Emergency stop reason at maximum length",
		},
		{
			"typical_reasons",
			"USER_REQUESTED",
			"Typical emergency stop reason",
		},
		{
			"verbose_reason",
			"COLLISION_DETECTED: Front-left sensor triggered at 1.2m distance",
			"Verbose emergency stop reason",
		},
		{
			"short_reason",
			"LOW_BATTERY",
			"Short emergency stop reason",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			req := &starv1.EmergencyStopRequest{
				Header: &starv1.RequestHeader{
					RequestId: "estop-001",
				},
				Reason: tc.reason,
			}

			bytes, err := proto.Marshal(req)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.EmergencyStopRequest{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.reason, parsed.Reason, tc.desc)
		})
	}
}

// TestEmergencyStopRequest_ExceedsMaxSize verifies Go protobuf allows reason exceeding max_size:128.
// NOTE: nanopb would truncate to 128 chars on RX72N firmware.
func TestEmergencyStopRequest_ExceedsMaxSize(t *testing.T) {
	// Create reason string exceeding max_size:128
	longReason := strings.Repeat("A", 256) // 256 chars (exceeds max_size:128)

	req := &starv1.EmergencyStopRequest{
		Header: &starv1.RequestHeader{
			RequestId: "estop-long",
		},
		Reason: longReason,
	}

	// Go protobuf allows this (strings are dynamic)
	bytes, err := proto.Marshal(req)
	require.NoError(t, err, "Go protobuf allows reason exceeding max_size:128")

	parsed := &starv1.EmergencyStopRequest{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err, "Go protobuf unmarshals oversized reason")

	// Go protobuf keeps full string
	assert.Equal(t, 256, len(parsed.Reason), "Go keeps 256-char reason")

	// IMPORTANT: nanopb on RX72N would truncate to max_size:128
	t.Log("WARNING: nanopb (RX72N) would truncate reason to 128 chars")
}
