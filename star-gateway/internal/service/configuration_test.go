// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"errors"
	"math"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// createDefaultConfiguration creates a default system configuration for testing.
func createDefaultConfiguration() *starv1.SystemConfiguration {
	return &starv1.SystemConfiguration{
		MotorConfigs: []*starv1.MotorPidConfiguration{
			{MotorId: 0, PidConfig: createDefaultPidConfig()},
			{MotorId: 1, PidConfig: createDefaultPidConfig()},
			{MotorId: 2, PidConfig: createDefaultPidConfig()},
			{MotorId: 3, PidConfig: createDefaultPidConfig()},
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
			ThermalShutdownDeciCelsius: 800, // 80.0degC
			CellImbalanceThresholdMv:   100,
		},
		EncoderConfig: &starv1.EncoderConfiguration{
			EdgesPerRevolution: 1364,
			WheelDiameterM:     0.065,
			GearRatio:          1.0,
		},
		TimingConfig: &starv1.TimingConfiguration{
			MotorControlPeriodMs:   10,
			TelemetryPeriodMs:      100,
			CommunicationTimeoutMs: 500,
		},
		ConfigVersion: 1,
		ConfigCrc:     0,
	}
}

// createDefaultPidConfig creates a default PID configuration for testing.
func createDefaultPidConfig() *starv1.PidConfig {
	return &starv1.PidConfig{
		Kp:               0.286,
		Ki:               8.01,
		Kd:               0.0,
		OutputMinPercent: -100.0,
		OutputMaxPercent: 100.0,
		IntegralMin:      -100.0,
		IntegralMax:      100.0,
	}
}

func TestNewConfigurationService(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
}

func TestGetConfiguration_Success(t *testing.T) {
	config := createDefaultConfiguration()
	response := &starv1.GetConfigurationResponse{
		Configuration: config,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: responsePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.GetConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-config-1",
		},
	}

	resp, err := svc.GetConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.Header.RequestId != "test-config-1" {
		t.Errorf("Expected request_id 'test-config-1', got '%s'", resp.Header.RequestId)
	}

	if resp.Configuration == nil {
		t.Fatal("Expected non-nil configuration")
	}

	// Verify default configuration has 4 motors
	if len(resp.Configuration.MotorConfigs) != 4 {
		t.Errorf("Expected 4 motor configs, got %d", len(resp.Configuration.MotorConfigs))
	}
}

func TestGetConfiguration_HarqFailure(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return status.Error(codes.Unavailable, "harq send failure")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.GetConfigurationRequest{Header: &starv1.RequestHeader{RequestId: "test-config-harq"}}

	_, err := svc.GetConfiguration(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for HARQ failure")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.Unavailable {
		t.Errorf("Expected Unavailable code, got %v", st.Code())
	}
}

func TestGetConfiguration_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	_, err := svc.GetConfiguration(context.Background(), nil)
	if err == nil {
		t.Fatal("Expected error for nil request")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestSetConfiguration_ValidConfig(t *testing.T) {
	response := &starv1.SetConfigurationResponse{
		Header:           &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		ValidationResult: &starv1.ConfigValidationResult{Status: starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID},
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: responsePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()

	req := &starv1.SetConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-set-1",
		},
		Configuration: config,
		PersistToNvs:  true,
	}

	resp, err := svc.SetConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		t.Errorf("Expected VALID status, got %v", resp.ValidationResult.Status)
	}

	if resp.Header.Status != starv1.Status_STATUS_OK {
		t.Errorf("Expected STATUS_OK, got %v", resp.Header.Status)
	}
}

func TestSetConfiguration_InvalidPidGains(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set invalid kp (negative)
	config.MotorConfigs[0].PidConfig.Kp = -1.0

	req := &starv1.SetConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-set-invalid",
		},
		Configuration: config,
		PersistToNvs:  false,
	}

	resp, err := svc.SetConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}

	if len(resp.ValidationResult.Errors) == 0 {
		t.Error("Expected validation errors, got none")
	}

	if resp.Header.Status != starv1.Status_STATUS_INVALID_REQUEST {
		t.Errorf("Expected STATUS_INVALID_REQUEST, got %v", resp.Header.Status)
	}
}

func TestSetConfiguration_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	_, err := svc.SetConfiguration(context.Background(), nil)
	if err == nil {
		t.Fatal("Expected error for nil request")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestSetConfiguration_NilConfiguration(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SetConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-nil-config",
		},
		Configuration: nil,
	}

	_, err := svc.SetConfiguration(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for nil configuration")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestValidateConfiguration_ValidConfig(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-validate-1",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		t.Errorf("Expected VALID status, got %v", resp.ValidationResult.Status)
	}

	if len(resp.ValidationResult.Errors) != 0 {
		t.Errorf("Expected no validation errors, got %d", len(resp.ValidationResult.Errors))
	}
}

func TestValidateConfiguration_InvalidOutputLimits(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set invalid output limits (min >= max)
	config.MotorConfigs[0].PidConfig.OutputMinPercent = 50.0
	config.MotorConfigs[0].PidConfig.OutputMaxPercent = 50.0

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-validate-invalid",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}

	if len(resp.ValidationResult.Errors) == 0 {
		t.Error("Expected validation errors, got none")
	}

	// Check that error mentions output_min_percent
	foundError := false
	for _, err := range resp.ValidationResult.Errors {
		if err.FieldPath == "motor_configs[0].pid_config.output_min_percent" {
			foundError = true
			break
		}
	}
	if !foundError {
		t.Error("Expected error for output_min_percent field")
	}
}

func TestValidateConfiguration_InvalidSafetyThresholds(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set overcurrent threshold too high
	config.SafetyThresholds.OvercurrentThresholdMa = 25000

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-validate-safety",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}

	if len(resp.ValidationResult.Errors) == 0 {
		t.Error("Expected validation errors, got none")
	}
}

func TestValidateConfiguration_InvalidEncoderConfig(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set edges_per_revolution to 0 (invalid)
	config.EncoderConfig.EdgesPerRevolution = 0

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-validate-encoder",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestValidateConfiguration_InvalidTimingConfig(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set motor control period too high
	config.TimingConfig.MotorControlPeriodMs = 200

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-validate-timing",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestValidateConfiguration_NaNValues(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set kp to NaN
	config.MotorConfigs[0].PidConfig.Kp = math.NaN()

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-validate-nan",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestResetToDefaults_Success(t *testing.T) {
	config := createDefaultConfiguration()
	response := &starv1.ResetToDefaultsResponse{Configuration: config}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: responsePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.ResetToDefaultsRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-reset-1",
		},
		PersistToNvs: true,
	}

	resp, err := svc.ResetToDefaults(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.Configuration == nil {
		t.Fatal("Expected non-nil configuration")
	}

	// Verify default configuration
	if len(resp.Configuration.MotorConfigs) != 4 {
		t.Errorf("Expected 4 motor configs, got %d", len(resp.Configuration.MotorConfigs))
	}
}

func TestResetToDefaults_HarqFailure(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return status.Error(codes.Unavailable, "harq send failure")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.ResetToDefaultsRequest{Header: &starv1.RequestHeader{RequestId: "test-reset-harq"}}

	_, err := svc.ResetToDefaults(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for HARQ failure")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.Unavailable {
		t.Errorf("Expected Unavailable code, got %v", st.Code())
	}
}

func TestResetToDefaults_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	_, err := svc.ResetToDefaults(context.Background(), nil)
	if err == nil {
		t.Fatal("Expected error for nil request")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestSaveConfiguration_Success(t *testing.T) {
	response := &starv1.SaveConfigurationResponse{Saved: true, ConfigCrc: 1234}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: responsePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SaveConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-save-1",
		},
	}

	resp, err := svc.SaveConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if !resp.Saved {
		t.Error("Expected saved to be true")
	}

	if resp.ConfigCrc == 0 {
		t.Error("Expected non-zero CRC")
	}
}

func TestSaveConfiguration_HarqFailure(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return status.Error(codes.Unavailable, "harq send failure")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SaveConfigurationRequest{Header: &starv1.RequestHeader{RequestId: "test-save-harq"}}

	_, err := svc.SaveConfiguration(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for HARQ failure")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.Unavailable {
		t.Errorf("Expected Unavailable code, got %v", st.Code())
	}
}

func TestSaveConfiguration_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	_, err := svc.SaveConfiguration(context.Background(), nil)
	if err == nil {
		t.Fatal("Expected error for nil request")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestGetMotorPidConfig_Success(t *testing.T) {
	pidConfig := createDefaultPidConfig()
	response := &starv1.GetMotorPidConfigResponse{MotorId: 2, PidConfig: pidConfig}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: responsePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.GetMotorPidConfigRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-get-pid-1",
		},
		MotorId: 2,
	}

	resp, err := svc.GetMotorPidConfig(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.MotorId != 2 {
		t.Errorf("Expected motor_id 2, got %d", resp.MotorId)
	}

	if resp.PidConfig == nil {
		t.Fatal("Expected non-nil PID config")
	}
}

func TestGetMotorPidConfig_InvalidMotorId(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	testCases := []struct {
		name    string
		motorID int32
	}{
		{"Negative motor ID", -1},
		{"Motor ID too high", 5},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			req := &starv1.GetMotorPidConfigRequest{
				Header: &starv1.RequestHeader{
					RequestId: "test-invalid-motor",
				},
				MotorId: tc.motorID,
			}

			_, err := svc.GetMotorPidConfig(context.Background(), req)
			if err == nil {
				t.Fatal("Expected error for invalid motor_id")
			}

			st, ok := status.FromError(err)
			if !ok {
				t.Fatal("Expected gRPC status error")
			}

			if st.Code() != codes.InvalidArgument {
				t.Errorf("Expected InvalidArgument code, got %v", st.Code())
			}
		})
	}
}

func TestGetMotorPidConfig_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	_, err := svc.GetMotorPidConfig(context.Background(), nil)
	if err == nil {
		t.Fatal("Expected error for nil request")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestSetMotorPidConfig_Success(t *testing.T) {
	saveResponse := &starv1.SaveConfigurationResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		Saved:  true,
	}
	savePayload, err := proto.Marshal(saveResponse)
	if err != nil {
		t.Fatalf("failed to marshal save response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: savePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	pidConfig := createDefaultPidConfig()

	req := &starv1.SetMotorPidConfigRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-set-pid-1",
		},
		MotorId:      1,
		PidConfig:    pidConfig,
		PersistToNvs: true,
	}

	resp, err := svc.SetMotorPidConfig(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		t.Errorf("Expected VALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestSetMotorPidConfig_RuntimeUpdate(t *testing.T) {
	var sentPayloads [][]byte
	saveResponse := &starv1.SaveConfigurationResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		Saved:  true,
	}
	savePayload, err := proto.Marshal(saveResponse)
	if err != nil {
		t.Fatalf("failed to marshal save response: %v", err)
	}
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, data []byte, _ ...harq.Priority) error {
			sentPayloads = append(sentPayloads, data)
			return nil
		},
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: savePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	pidConfig := createDefaultPidConfig()

	req := &starv1.SetMotorPidConfigRequest{
		Header:       &starv1.RequestHeader{RequestId: "test-pid-runtime"},
		MotorId:      1,
		PidConfig:    pidConfig,
		PersistToNvs: true,
	}

	_, err = svc.SetMotorPidConfig(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if len(sentPayloads) != 2 {
		t.Fatalf("Expected 2 HARQ sends, got %d", len(sentPayloads))
	}

	var wireMsg starv1.WireMessage
	if err := proto.Unmarshal(sentPayloads[0], &wireMsg); err != nil {
		t.Fatalf("failed to unmarshal first payload: %v", err)
	}

	if wireMsg.GetPidConfig() == nil {
		t.Fatal("Expected first payload to contain PidConfig")
	}

	var saveReq starv1.SaveConfigurationRequest
	if err := proto.Unmarshal(sentPayloads[1], &saveReq); err != nil {
		t.Fatalf("failed to unmarshal second payload: %v", err)
	}

	if saveReq.Header == nil || saveReq.Header.RequestId != "test-pid-runtime" {
		t.Errorf("Expected save request header to match, got %+v", saveReq.Header)
	}
}

func TestSetMotorPidConfig_InvalidGains(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	pidConfig := createDefaultPidConfig()
	pidConfig.Ki = -1.0 // Invalid: ki must be >= 0

	req := &starv1.SetMotorPidConfigRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-set-pid-invalid",
		},
		MotorId:   0,
		PidConfig: pidConfig,
	}

	resp, err := svc.SetMotorPidConfig(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}

	if resp.Header.Status != starv1.Status_STATUS_INVALID_REQUEST {
		t.Errorf("Expected STATUS_INVALID_REQUEST, got %v", resp.Header.Status)
	}
}

func TestSetMotorPidConfig_ZeroKiValid(t *testing.T) {
	saveResponse := &starv1.SaveConfigurationResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		Saved:  true,
	}
	savePayload, err := proto.Marshal(saveResponse)
	if err != nil {
		t.Fatalf("failed to marshal save response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: savePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	pidConfig := createDefaultPidConfig()
	pidConfig.Ki = 0.0 // Valid: ki >= 0 is now allowed

	req := &starv1.SetMotorPidConfigRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-set-pid-zero-ki",
		},
		MotorId:      0,
		PidConfig:    pidConfig,
		PersistToNvs: true,
	}

	resp, err := svc.SetMotorPidConfig(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		t.Errorf("Expected VALID status for ki=0, got %v", resp.ValidationResult.Status)
	}
}

func TestSetMotorPidConfig_NilPidConfig(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SetMotorPidConfigRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-nil-pid",
		},
		MotorId:   0,
		PidConfig: nil,
	}

	_, err := svc.SetMotorPidConfig(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for nil pid_config")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestSetMotorPidConfig_InvalidMotorId(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	pidConfig := createDefaultPidConfig()

	req := &starv1.SetMotorPidConfigRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-invalid-motor",
		},
		MotorId:   10,
		PidConfig: pidConfig,
	}

	_, err := svc.SetMotorPidConfig(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for invalid motor_id")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestValidateConfiguration_TooManyMotors(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Add a 5th motor (invalid)
	config.MotorConfigs = append(config.MotorConfigs, &starv1.MotorPidConfiguration{
		MotorId:   4,
		PidConfig: createDefaultPidConfig(),
	})

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-too-many-motors",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}

	// Check for WRONG_ARRAY_SIZE error
	foundError := false
	for _, err := range resp.ValidationResult.Errors {
		if err.ErrorCode == starv1.ConfigErrorCode_CONFIG_ERROR_CODE_WRONG_ARRAY_SIZE {
			foundError = true
			break
		}
	}
	if !foundError {
		t.Error("Expected WRONG_ARRAY_SIZE error")
	}
}

func TestValidateConfiguration_MotorIdMismatch(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set motor_id to not match array index
	config.MotorConfigs[2].MotorId = 5

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-motor-id-mismatch",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestValidateConfiguration_IntegralLimitsMismatch(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set integral_min >= integral_max (invalid)
	config.MotorConfigs[0].PidConfig.IntegralMin = 100.0
	config.MotorConfigs[0].PidConfig.IntegralMax = 50.0

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-integral-limits",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestValidateConfiguration_InvalidWheelDiameter(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set wheel diameter to 0 (invalid)
	config.EncoderConfig.WheelDiameterM = 0.0

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-wheel-diameter",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}
}

func TestValidateConfiguration_AllTimingParametersInvalid(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	config := createDefaultConfiguration()
	// Set all timing parameters out of range
	config.TimingConfig.MotorControlPeriodMs = 0
	config.TimingConfig.TelemetryPeriodMs = 5
	config.TimingConfig.CommunicationTimeoutMs = 50

	req := &starv1.ValidateConfigurationRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-all-timing-invalid",
		},
		Configuration: config,
	}

	resp, err := svc.ValidateConfiguration(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID {
		t.Errorf("Expected INVALID status, got %v", resp.ValidationResult.Status)
	}

	// Should have 3 timing errors
	timingErrors := 0
	for _, err := range resp.ValidationResult.Errors {
		if err.FieldPath == "timing_config.motor_control_period_ms" ||
			err.FieldPath == "timing_config.telemetry_period_ms" ||
			err.FieldPath == "timing_config.communication_timeout_ms" {
			timingErrors++
		}
	}

	if timingErrors != 3 {
		t.Errorf("Expected 3 timing errors, got %d", timingErrors)
	}
}

func TestGetConfiguration_HarqReceiveError(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return nil, errors.New("receive timeout")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.GetConfigurationRequest{Header: &starv1.RequestHeader{RequestId: "test"}}
	_, err := svc.GetConfiguration(context.Background(), req)
	if status.Code(err) != codes.Unavailable {
		t.Errorf("Expected Unavailable, got %v", status.Code(err))
	}
}

func TestSetConfiguration_HarqSendError(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return errors.New("send failed")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SetConfigurationRequest{
		Header:        &starv1.RequestHeader{},
		Configuration: createDefaultConfiguration(),
	}
	_, err := svc.SetConfiguration(context.Background(), req)
	if status.Code(err) != codes.Unavailable {
		t.Errorf("Expected Unavailable, got %v", status.Code(err))
	}
}

func TestGetMotorPidConfig_UnmarshalError(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: []byte("garbage data"), Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.GetMotorPidConfigRequest{
		Header:  &starv1.RequestHeader{},
		MotorId: 0,
	}
	_, err := svc.GetMotorPidConfig(context.Background(), req)
	if status.Code(err) != codes.Unavailable {
		t.Errorf("Expected Unavailable error for unmarshal failure, got %v", status.Code(err))
	}
}

func TestSetMotorPidConfig_PersistFailure(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			// First send (PID config) succeeds
			return nil
		},
	}
	// Override SendFunc for the second call (Persist) to fail
	callCount := 0
	mockHARQ.SendFunc = func(_ context.Context, _ []byte, _ ...harq.Priority) error {
		callCount++
		if callCount == 2 {
			return errors.New("persist send failed")
		}
		return nil
	}

	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SetMotorPidConfigRequest{
		Header:       &starv1.RequestHeader{},
		MotorId:      0,
		PidConfig:    createDefaultPidConfig(),
		PersistToNvs: true,
	}

	_, err := svc.SetMotorPidConfig(context.Background(), req)
	if status.Code(err) != codes.Unavailable {
		t.Errorf("Expected Unavailable error for persist failure, got %v", status.Code(err))
	}
}

func TestSetMotorPidConfig_PersistResponseFailure(t *testing.T) {
	saveResponse := &starv1.SaveConfigurationResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_INVALID_REQUEST},
		Saved:  false,
	}
	savePayload, err := proto.Marshal(saveResponse)
	if err != nil {
		t.Fatalf("Failed to marshal save response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return nil
		},
		ReceiveFunc: func(_ context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: savePayload, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewConfigurationService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SetMotorPidConfigRequest{
		Header:       &starv1.RequestHeader{},
		MotorId:      0,
		PidConfig:    createDefaultPidConfig(),
		PersistToNvs: true,
	}

	_, err = svc.SetMotorPidConfig(context.Background(), req)
	if status.Code(err) != codes.Internal {
		t.Errorf("Expected Internal error for failed save response, got %v", status.Code(err))
	}
}
