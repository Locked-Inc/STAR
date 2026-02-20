// configuration_test.go - Configuration Protobuf Serialization Tests
// Verifies NVS configuration proto serialization/deserialization.
//
// Using Go table-driven tests following standard patterns.
//
// STAR Project - Texas A&M University
// December 2025

package starproto_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// ============================================================================
// SafetyThresholds Tests
// ============================================================================

func TestSafetyThresholdsRoundTrip(t *testing.T) {
	tests := []struct {
		name                 string
		overcurrentMa        uint32
		thermalShutdownDeciC int32
		cellImbalanceMv      uint32
		desc                 string
	}{
		{"standard", 5000, 600, 100, "Standard safety thresholds"},
		{"conservative", 2000, 500, 50, "Conservative thresholds"},
		{"high_current", 15000, 700, 200, "High-current motor thresholds"},
		{"sensitive", 3000, 550, 30, "Sensitive thresholds"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			thresholds := &starv1.SafetyThresholds{
				OvercurrentThresholdMa:     tc.overcurrentMa,
				ThermalShutdownDeciCelsius: tc.thermalShutdownDeciC,
				CellImbalanceThresholdMv:   tc.cellImbalanceMv,
			}

			bytes, err := proto.Marshal(thresholds)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.SafetyThresholds{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.overcurrentMa, parsed.OvercurrentThresholdMa, tc.desc)
			assert.Equal(t, tc.thermalShutdownDeciC, parsed.ThermalShutdownDeciCelsius, tc.desc)
			assert.Equal(t, tc.cellImbalanceMv, parsed.CellImbalanceThresholdMv, tc.desc)
		})
	}
}

func TestOvercurrentRange(t *testing.T) {
	tests := []struct {
		name string
		val  uint32
	}{
		{"min_valid", 1000},
		{"mid_valid", 5000},
		{"high_valid", 10000},
		{"max_valid", 20000},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			thresholds := &starv1.SafetyThresholds{
				OvercurrentThresholdMa: tc.val,
			}

			bytes, err := proto.Marshal(thresholds)
			require.NoError(t, err)

			parsed := &starv1.SafetyThresholds{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.True(t, parsed.OvercurrentThresholdMa >= 1000 && parsed.OvercurrentThresholdMa <= 20000)
			assert.Equal(t, tc.val, parsed.OvercurrentThresholdMa)
		})
	}
}

// ============================================================================
// TimingConfiguration Tests
// ============================================================================

func TestTimingConfigurationRoundTrip(t *testing.T) {
	tests := []struct {
		name              string
		motorPeriodMs     uint32
		telemetryPeriodMs uint32
		commTimeoutMs     uint32
		desc              string
	}{
		{"high_freq", 1, 10, 100, "1kHz motor, 100Hz telemetry"},
		{"standard", 10, 100, 500, "100Hz motor, 10Hz telemetry"},
		{"low_power", 50, 500, 2000, "20Hz motor, 2Hz telemetry"},
		{"balanced", 20, 200, 1000, "50Hz motor, 5Hz telemetry"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			timing := &starv1.TimingConfiguration{
				MotorControlPeriodMs:   tc.motorPeriodMs,
				TelemetryPeriodMs:      tc.telemetryPeriodMs,
				CommunicationTimeoutMs: tc.commTimeoutMs,
			}

			bytes, err := proto.Marshal(timing)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.TimingConfiguration{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.motorPeriodMs, parsed.MotorControlPeriodMs, tc.desc)
			assert.Equal(t, tc.telemetryPeriodMs, parsed.TelemetryPeriodMs, tc.desc)
			assert.Equal(t, tc.commTimeoutMs, parsed.CommunicationTimeoutMs, tc.desc)
		})
	}
}

func TestControlFrequencyCalculation(t *testing.T) {
	timing := &starv1.TimingConfiguration{
		MotorControlPeriodMs: 10, // 100 Hz
	}

	bytes, err := proto.Marshal(timing)
	require.NoError(t, err)

	parsed := &starv1.TimingConfiguration{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	// Verify frequency calculation: freq = 1000 / period_ms
	expectedHz := 1000 / parsed.MotorControlPeriodMs
	assert.Equal(t, uint32(100), expectedHz)
}

// ============================================================================
// EncoderConfiguration Tests
// ============================================================================

func TestEncoderConfigurationRoundTrip(t *testing.T) {
	tests := []struct {
		name           string
		edgesPerRev    uint32
		wheelDiameterM float64
		gearRatio      float64
	}{
		{"low_res", 500, 0.065, 34.02},
		{"medium_res", 1000, 0.065, 34.02},
		{"high_res", 2000, 0.065, 34.02},
		{"ultra_res", 4000, 0.065, 34.02},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			encoder := &starv1.EncoderConfiguration{
				EdgesPerRevolution: tc.edgesPerRev,
				WheelDiameterM:     tc.wheelDiameterM,
				GearRatio:          tc.gearRatio,
			}

			bytes, err := proto.Marshal(encoder)
			require.NoError(t, err)

			parsed := &starv1.EncoderConfiguration{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.edgesPerRev, parsed.EdgesPerRevolution)
			assert.InDelta(t, tc.wheelDiameterM, parsed.WheelDiameterM, 0.0001)
			assert.InDelta(t, tc.gearRatio, parsed.GearRatio, 0.0001)
		})
	}
}

// ============================================================================
// ConfigValidationStatus Enum Tests
// ============================================================================

func TestConfigValidationStatusZeroValue(t *testing.T) {
	assert.Equal(t, int32(0), int32(starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_UNKNOWN))
}

func TestAllConfigValidationStatusValues(t *testing.T) {
	statuses := []starv1.ConfigValidationStatus{
		starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_UNKNOWN,
		starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID,
		starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID,
		starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_WARNING,
		starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_CRC_MISMATCH,
		starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VERSION_MISMATCH,
	}

	for _, status := range statuses {
		t.Run(status.String(), func(t *testing.T) {
			result := &starv1.ConfigValidationResult{
				Status: status,
			}

			bytes, err := proto.Marshal(result)
			require.NoError(t, err)

			parsed := &starv1.ConfigValidationResult{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, status, parsed.Status)
		})
	}
}

// ============================================================================
// ConfigErrorCode Enum Tests
// ============================================================================

func TestConfigErrorCodeZeroValue(t *testing.T) {
	assert.Equal(t, int32(0), int32(starv1.ConfigErrorCode_CONFIG_ERROR_CODE_UNKNOWN))
}

func TestAllConfigErrorCodeValues(t *testing.T) {
	codes := []starv1.ConfigErrorCode{
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_UNKNOWN,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_INVALID_NUMBER,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_MISSING_FIELD,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_WRONG_ARRAY_SIZE,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_CRC_FAILED,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_UNSUPPORTED_VERSION,
		starv1.ConfigErrorCode_CONFIG_ERROR_CODE_STORAGE_ERROR,
	}

	for _, code := range codes {
		t.Run(code.String(), func(t *testing.T) {
			errInfo := &starv1.ConfigValidationError{
				ErrorCode: code,
			}

			bytes, err := proto.Marshal(errInfo)
			require.NoError(t, err)

			parsed := &starv1.ConfigValidationError{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, code, parsed.ErrorCode)
		})
	}
}

// ============================================================================
// ConfigValidationError Tests
// ============================================================================

func TestConfigValidationErrorRoundTrip(t *testing.T) {
	tests := []struct {
		name      string
		fieldPath string
		errorCode starv1.ConfigErrorCode
		message   string
		desc      string
	}{
		{
			"kp_too_high",
			"motor_configs[0].pid_config.kp",
			starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			"Kp exceeds maximum of 10.0",
			"PID gain validation error",
		},
		{
			"missing_field",
			"safety_thresholds.overcurrent_threshold_ma",
			starv1.ConfigErrorCode_CONFIG_ERROR_CODE_MISSING_FIELD,
			"Required field is missing",
			"Missing required field",
		},
		{
			"invalid_number",
			"encoder_config.gear_ratio",
			starv1.ConfigErrorCode_CONFIG_ERROR_CODE_INVALID_NUMBER,
			"Value is NaN",
			"Invalid number error",
		},
		{
			"wrong_array_size",
			"motor_configs",
			starv1.ConfigErrorCode_CONFIG_ERROR_CODE_WRONG_ARRAY_SIZE,
			"Expected 4 motors, got 2",
			"Array size validation error",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			errInfo := &starv1.ConfigValidationError{
				FieldPath: tc.fieldPath,
				ErrorCode: tc.errorCode,
				Message:   tc.message,
			}

			bytes, err := proto.Marshal(errInfo)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.ConfigValidationError{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.fieldPath, parsed.FieldPath, tc.desc)
			assert.Equal(t, tc.errorCode, parsed.ErrorCode, tc.desc)
			assert.Equal(t, tc.message, parsed.Message, tc.desc)
		})
	}
}

func TestMultipleValidationErrors(t *testing.T) {
	result := &starv1.ConfigValidationResult{
		Status: starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID,
		Errors: []*starv1.ConfigValidationError{
			{
				FieldPath: "motor_configs[0].pid_config.kp",
				ErrorCode: starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
				Message:   "Kp exceeds maximum",
			},
			{
				FieldPath: "safety_thresholds.overcurrent_threshold_ma",
				ErrorCode: starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
				Message:   "Overcurrent below minimum",
			},
		},
	}

	bytes, err := proto.Marshal(result)
	require.NoError(t, err)

	parsed := &starv1.ConfigValidationResult{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, 2, len(parsed.Errors))
	assert.Equal(t, starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID, parsed.Status)
}

// ============================================================================
// Complete SystemConfiguration Tests
// ============================================================================

func TestCompleteSystemConfigurationRoundTrip(t *testing.T) {
	config := &starv1.SystemConfiguration{
		MotorConfigs: []*starv1.MotorPidConfiguration{
			{MotorId: 0, PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
			{MotorId: 1, PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
			{MotorId: 2, PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
			{MotorId: 3, PidConfig: &starv1.PidConfig{Kp: 0.286, Ki: 8.01, Kd: 0.0}},
		},
		CurrentCalibration: &starv1.CurrentSensorCalibration{
			OffsetMa:    []float64{0.0, 0.0, 0.0, 0.0},
			ScaleFactor: []float64{1.0, 1.0, 1.0, 1.0},
		},
		TemperatureCalibration: &starv1.TemperatureCalibration{
			OffsetCelsius: 0.0,
		},
		SafetyThresholds: &starv1.SafetyThresholds{
			OvercurrentThresholdMa:     5000,
			ThermalShutdownDeciCelsius: 600,
			CellImbalanceThresholdMv:   100,
		},
		EncoderConfig: &starv1.EncoderConfiguration{
			EdgesPerRevolution: 2000,
			WheelDiameterM:     0.065,
			GearRatio:          34.02,
		},
		TimingConfig: &starv1.TimingConfiguration{
			MotorControlPeriodMs:   10,
			TelemetryPeriodMs:      100,
			CommunicationTimeoutMs: 500,
		},
		ConfigVersion: 1,
		ConfigCrc:     0x12345678,
	}

	bytes, err := proto.Marshal(config)
	require.NoError(t, err)
	require.Less(t, len(bytes), 1000, "Complete config should be < 1KB")

	parsed := &starv1.SystemConfiguration{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, 4, len(parsed.MotorConfigs), "Should have 4 motor configs")
	assert.InDelta(t, 0.286, parsed.MotorConfigs[0].PidConfig.Kp, 0.001, "Motor 0 Kp")
	assert.Equal(t, uint32(5000), parsed.SafetyThresholds.OvercurrentThresholdMa, "Overcurrent")
	assert.Equal(t, uint32(10), parsed.TimingConfig.MotorControlPeriodMs, "Motor period")
	assert.Equal(t, uint32(1), parsed.ConfigVersion, "Config version")
	assert.Equal(t, uint32(0x12345678), parsed.ConfigCrc, "Config CRC")
}

// ============================================================================
// Request/Response Pattern Tests
// ============================================================================

func TestGetSetConfigurationPattern(t *testing.T) {
	// Get request
	getReq := &starv1.GetConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId:     "get-config-001",
			ClientVersion: "1.0.0",
		},
	}

	getReqBytes, err := proto.Marshal(getReq)
	require.NoError(t, err)

	parsedGetReq := &starv1.GetConfigurationRequest{}
	err = proto.Unmarshal(getReqBytes, parsedGetReq)
	require.NoError(t, err)
	assert.Equal(t, "get-config-001", parsedGetReq.Header.RequestId)

	// Set request with validation
	setReq := &starv1.SetConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId:     "set-config-001",
			ClientVersion: "1.0.0",
		},
		Configuration: &starv1.SystemConfiguration{
			ConfigVersion: 1,
		},
		PersistToNvs: true,
	}

	setReqBytes, err := proto.Marshal(setReq)
	require.NoError(t, err)

	parsedSetReq := &starv1.SetConfigurationRequest{}
	err = proto.Unmarshal(setReqBytes, parsedSetReq)
	require.NoError(t, err)
	assert.True(t, parsedSetReq.PersistToNvs)
}
