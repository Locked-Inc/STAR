// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"fmt"
	"log/slog"
	"math"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/timestamppb"
)

const (
	// MaxMotors is the maximum number of motors in the system.
	MaxMotors = 4

	// MinMotorID is the minimum valid motor ID.
	MinMotorID = 0

	// MaxMotorID is the maximum valid motor ID (0-indexed, 4 motors total).
	MaxMotorID = 3

	// Motor configuration validation constraints
	minPidGain       = 0.0
	minOutputPercent = -100.0
	maxOutputPercent = 100.0

	// Safety thresholds validation constraints
	minOvercurrentMa            = 1000
	maxOvercurrentMa            = 20000
	minThermalShutdownDeciC     = 500  // 50.0degC
	maxThermalShutdownDeciC     = 1000 // 100.0degC
	minCellImbalanceThresholdMv = 50
	maxCellImbalanceThresholdMv = 500

	// Encoder configuration validation constraints
	minEdgesPerRevolution = 1

	// Timing configuration validation constraints
	minMotorControlPeriodMs   = 1
	maxMotorControlPeriodMs   = 100
	minTelemetryPeriodMs      = 10
	maxTelemetryPeriodMs      = 1000
	minCommunicationTimeoutMs = 100
	maxCommunicationTimeoutMs = 5000
)

// ConfigurationService implements the ConfigurationServiceServer interface.
// This service handles runtime configuration management including NVS persistence.
//
// Architecture:
// - Uses WireMessage wrapper for protocol multiplexing
// - Uses Dispatcher for centralized message routing
// - Validates all configuration parameters before applying
// - Uses HARQ for reliable firmware communication
type ConfigurationService struct {
	starv1.UnimplementedConfigurationServiceServer
	harqHandler harq.HARQ
	dispatcher  dispatcher.Dispatcher
	logger      *slog.Logger
}

// NewConfigurationService creates a new ConfigurationService.
// Requires HARQ handler for sending requests, Dispatcher for receiving responses, and logger.
func NewConfigurationService(h harq.HARQ, d dispatcher.Dispatcher, logger *slog.Logger) *ConfigurationService {
	return &ConfigurationService{
		harqHandler: h,
		dispatcher:  d,
		logger:      logger,
	}
}

func (s *ConfigurationService) ensureResponseHeader(
	reqHeader *starv1.RequestHeader,
	respHeader **starv1.ResponseHeader,
	warnMsg string,
) {
	requestID := ""
	if reqHeader != nil {
		requestID = reqHeader.GetRequestId()
	}
	if *respHeader == nil {
		s.logger.Warn(warnMsg, "request_id", requestID)
		*respHeader = &starv1.ResponseHeader{
			RequestId:       requestID,
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		}
	}
}

func (s *ConfigurationService) sendAndReceive(
	ctx context.Context,
	req proto.Message,
	resp proto.Message,
	sendLogMsg string,
	sendErrMsg string,
	recvLogMsg string,
	recvErrMsg string,
) error {
	if err := s.sendProtoMessage(ctx, req); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return status.FromContextError(ctxErr).Err()
		}
		s.logger.Error(sendLogMsg, "error", err)
		return status.Errorf(codes.Unavailable, "%s: %v", sendErrMsg, err)
	}

	if err := s.receiveProtoMessage(ctx, resp); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return status.FromContextError(ctxErr).Err()
		}
		s.logger.Error(recvLogMsg, "error", err)
		return status.Errorf(codes.Unavailable, "%s: %v", recvErrMsg, err)
	}

	return nil
}

// GetConfiguration retrieves the current system configuration from the RX72N firmware.
// Sends the request via HARQ and receives the firmware response through the dispatcher.
func (s *ConfigurationService) GetConfiguration(ctx context.Context, req *starv1.GetConfigurationRequest) (*starv1.GetConfigurationResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.GetConfigurationResponse
	if err := s.sendAndReceive(
		ctx,
		req,
		&resp,
		"failed to send get configuration request",
		"failed to send get configuration request",
		"failed to receive configuration response",
		"failed to receive configuration response",
	); err != nil {
		return nil, err
	}

	s.ensureResponseHeader(req.Header, &resp.Header, "configuration response missing header")

	return &resp, nil
}

// SetConfiguration validates and applies new system configuration.
// Validates all parameters before applying. If persist_to_nvs is true, saves to NVS.
func (s *ConfigurationService) SetConfiguration(ctx context.Context, req *starv1.SetConfigurationRequest) (*starv1.SetConfigurationResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	if req.Configuration == nil {
		return nil, status.Error(codes.InvalidArgument, "configuration cannot be nil")
	}

	// Validate configuration
	validationResult := s.validateConfiguration(req.Configuration)

	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}

	// If validation failed, return error
	if validationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		return &starv1.SetConfigurationResponse{
			Header: &starv1.ResponseHeader{
				RequestId:       requestID,
				ServerTimestamp: timestamppb.Now(),
				Status:          starv1.Status_STATUS_INVALID_REQUEST,
			},
			ValidationResult: validationResult,
		}, nil
	}

	if err := s.sendProtoMessage(ctx, req); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return nil, status.FromContextError(ctxErr).Err()
		}
		s.logger.Error("failed to send set configuration request", "error", err)
		return nil, status.Errorf(codes.Unavailable, "failed to send configuration: %v", err)
	}

	var resp starv1.SetConfigurationResponse
	if err := s.receiveProtoMessage(ctx, &resp); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return nil, status.FromContextError(ctxErr).Err()
		}
		s.logger.Error("failed to receive set configuration response", "error", err)
		return nil, status.Errorf(codes.Unavailable, "failed to receive configuration response: %v", err)
	}

	s.ensureResponseHeader(req.Header, &resp.Header, "set configuration response missing header")

	if resp.Header.Status != starv1.Status_STATUS_OK {
		s.logger.Error("firmware rejected configuration", "status", resp.Header.Status, "response", &resp)
		return nil, status.Errorf(codes.Internal, "firmware rejected configuration: %s", resp.Header.Status.String())
	}

	if resp.ValidationResult != nil && resp.ValidationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		s.logger.Error("firmware validation failed", "status", resp.ValidationResult.Status, "response", &resp)
		return nil, status.Errorf(codes.Internal, "firmware validation failed: %s", resp.ValidationResult.Status.String())
	}

	if resp.ValidationResult == nil {
		resp.ValidationResult = validationResult
	}

	return &resp, nil
}

// ValidateConfiguration validates configuration parameters without applying them.
// Returns detailed validation results including any errors found.
func (s *ConfigurationService) ValidateConfiguration(_ context.Context, req *starv1.ValidateConfigurationRequest) (*starv1.ValidateConfigurationResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	if req.Configuration == nil {
		return nil, status.Error(codes.InvalidArgument, "configuration cannot be nil")
	}

	validationResult := s.validateConfiguration(req.Configuration)

	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}

	return &starv1.ValidateConfigurationResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       requestID,
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		ValidationResult: validationResult,
	}, nil
}

// ResetToDefaults resets configuration to factory defaults.
// If persist_to_nvs is true, saves defaults to NVS.
func (s *ConfigurationService) ResetToDefaults(ctx context.Context, req *starv1.ResetToDefaultsRequest) (*starv1.ResetToDefaultsResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.ResetToDefaultsResponse
	if err := s.sendAndReceive(
		ctx,
		req,
		&resp,
		"failed to send reset to defaults request",
		"failed to send reset request",
		"failed to receive reset to defaults response",
		"failed to receive reset response",
	); err != nil {
		return nil, err
	}

	s.ensureResponseHeader(req.Header, &resp.Header, "reset response missing header")

	return &resp, nil
}

// SaveConfiguration persists current in-memory configuration to NVS.
func (s *ConfigurationService) SaveConfiguration(ctx context.Context, req *starv1.SaveConfigurationRequest) (*starv1.SaveConfigurationResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.SaveConfigurationResponse
	if err := s.sendAndReceive(
		ctx,
		req,
		&resp,
		"failed to send save configuration request",
		"failed to send save request",
		"failed to receive save configuration response",
		"failed to receive save response",
	); err != nil {
		return nil, err
	}

	s.ensureResponseHeader(req.Header, &resp.Header, "save configuration response missing header")

	return &resp, nil
}

// GetMotorPidConfig retrieves PID configuration for a specific motor.
func (s *ConfigurationService) GetMotorPidConfig(ctx context.Context, req *starv1.GetMotorPidConfigRequest) (*starv1.GetMotorPidConfigResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Validate motor ID
	if req.MotorId < MinMotorID || req.MotorId > MaxMotorID {
		return nil, status.Errorf(codes.InvalidArgument, "motor_id must be between %d and %d", MinMotorID, MaxMotorID)
	}

	if err := s.sendProtoMessage(ctx, req); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return nil, status.FromContextError(ctxErr).Err()
		}
		s.logger.Error("failed to send get motor pid config request", "error", err)
		return nil, status.Errorf(codes.Unavailable, "failed to send get motor pid config request: %v", err)
	}

	var resp starv1.GetMotorPidConfigResponse
	if err := s.receiveProtoMessage(ctx, &resp); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return nil, status.FromContextError(ctxErr).Err()
		}
		s.logger.Error("failed to receive motor pid config response", "error", err)
		return nil, status.Errorf(codes.Unavailable, "failed to receive motor pid config response: %v", err)
	}

	s.ensureResponseHeader(req.Header, &resp.Header, "motor pid config response missing header")

	return &resp, nil
}

// SetMotorPidConfig updates PID configuration for a specific motor.
// Validates PID parameters before applying. If persist_to_nvs is true, saves to NVS.
func (s *ConfigurationService) SetMotorPidConfig(ctx context.Context, req *starv1.SetMotorPidConfigRequest) (*starv1.SetMotorPidConfigResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Validate motor ID
	if req.MotorId < MinMotorID || req.MotorId > MaxMotorID {
		return nil, status.Errorf(codes.InvalidArgument, "motor_id must be between %d and %d", MinMotorID, MaxMotorID)
	}

	if req.PidConfig == nil {
		return nil, status.Error(codes.InvalidArgument, "pid_config cannot be nil")
	}

	// Validate PID config
	validationResult := s.validatePidConfig(fmt.Sprintf("motor_configs[%d].pid_config", req.MotorId), req.PidConfig)

	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}

	// If validation failed, return error
	if validationResult.Status != starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID {
		return &starv1.SetMotorPidConfigResponse{
			Header: &starv1.ResponseHeader{
				RequestId:       requestID,
				ServerTimestamp: timestamppb.Now(),
				Status:          starv1.Status_STATUS_INVALID_REQUEST,
			},
			ValidationResult: validationResult,
		}, nil
	}

	// Wrap SetMotorPidConfigRequest in WireMessage
	// Note: PidConfig message currently lacks MotorId in the wire protocol definition.
	// This assumes the firmware applies this config to the appropriate motor or all motors,
	// or the protocol needs to be updated.
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_PidConfig{
			PidConfig: req.PidConfig,
		},
	}

	// Marshal WireMessage
	data, err := proto.Marshal(wireMsg)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to marshal wire message: %v", err)
	}

	// Send via HARQ
	if err := s.harqHandler.Send(ctx, data); err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return nil, status.FromContextError(ctxErr).Err()
		}
		s.logger.Error("failed to send PID config", "error", err)
		return nil, status.Errorf(codes.Internal, "failed to send configuration: %v", err)
	}

	if req.PersistToNvs {
		saveReq := &starv1.SaveConfigurationRequest{Header: req.Header}
		if err := s.sendProtoMessage(ctx, saveReq); err != nil {
			if ctxErr := ctx.Err(); ctxErr != nil {
				return nil, status.FromContextError(ctxErr).Err()
			}
			s.logger.Error("failed to persist configuration", "error", err)
			return nil, status.Errorf(codes.Unavailable, "failed to persist configuration: %v", err)
		}

		var saveResp starv1.SaveConfigurationResponse
		if err := s.receiveProtoMessage(ctx, &saveResp); err != nil {
			if ctxErr := ctx.Err(); ctxErr != nil {
				return nil, status.FromContextError(ctxErr).Err()
			}
			s.logger.Error("failed to receive persist configuration response", "error", err)
			return nil, status.Errorf(codes.Unavailable, "failed to persist configuration: %v", err)
		}

		if saveResp.Header == nil {
			requestID := ""
			if req.Header != nil {
				requestID = req.Header.GetRequestId()
			}
			s.logger.Warn("persist configuration response missing header", "request_id", requestID)
			saveResp.Header = &starv1.ResponseHeader{
				RequestId:       requestID,
				ServerTimestamp: timestamppb.Now(),
				Status:          starv1.Status_STATUS_OK,
			}
		}

		if saveResp.Header.Status != starv1.Status_STATUS_OK || !saveResp.Saved {
			s.logger.Error("firmware failed to persist configuration", "response", &saveResp)
			return nil, status.Errorf(codes.Internal, "failed to persist configuration")
		}
	}

	// TODO: Dispatch based response validation loops could be added here if the firmware sends Ack.
	// Current HARQ handles the Frame ACK, but not the Application Level ACK/Response (unless it sends a specific response message).

	return &starv1.SetMotorPidConfigResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       requestID,
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		ValidationResult: validationResult,
	}, nil
}

func (s *ConfigurationService) sendProtoMessage(ctx context.Context, msg proto.Message) error {
	if ctx == nil {
		ctx = context.Background()
	}
	if err := ctx.Err(); err != nil {
		return err
	}

	payload, err := proto.Marshal(msg)
	if err != nil {
		return err
	}

	return s.harqHandler.Send(ctx, payload)
}

func (s *ConfigurationService) receiveProtoMessage(ctx context.Context, target proto.Message) error {
	if ctx == nil {
		return status.Error(codes.InvalidArgument, "context cannot be nil")
	}
	if err := ctx.Err(); err != nil {
		return err
	}

	result, err := s.harqHandler.Receive(ctx)
	if err != nil {
		return err
	}
	if err := ctx.Err(); err != nil {
		return err
	}

	return proto.Unmarshal(result.Payload, target)
}

// validateConfiguration validates all configuration parameters.
// Returns validation result with detailed errors if any parameters are invalid.
func (s *ConfigurationService) validateConfiguration(config *starv1.SystemConfiguration) *starv1.ConfigValidationResult {
	var errors []*starv1.ConfigValidationError

	// Validate motor configurations
	if len(config.MotorConfigs) > MaxMotors {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "motor_configs",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_WRONG_ARRAY_SIZE,
			Message:            fmt.Sprintf("motor_configs array has %d elements, maximum is %d", len(config.MotorConfigs), MaxMotors),
			ActualValue:        fmt.Sprintf("%d", len(config.MotorConfigs)),
			ExpectedConstraint: fmt.Sprintf("<= %d", MaxMotors),
		})
	}

	for i, motorConfig := range config.MotorConfigs {
		if motorConfig == nil {
			errors = append(errors, &starv1.ConfigValidationError{
				FieldPath:          fmt.Sprintf("motor_configs[%d]", i),
				ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_MISSING_FIELD,
				Message:            "motor config cannot be nil",
				ActualValue:        "nil",
				ExpectedConstraint: "non-nil MotorPidConfiguration",
			})
			continue
		}

		// Validate motor_id matches array index
		if int(motorConfig.MotorId) != i {
			errors = append(errors, &starv1.ConfigValidationError{
				FieldPath:          fmt.Sprintf("motor_configs[%d].motor_id", i),
				ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
				Message:            fmt.Sprintf("motor_id %d does not match array index %d", motorConfig.MotorId, i),
				ActualValue:        fmt.Sprintf("%d", motorConfig.MotorId),
				ExpectedConstraint: fmt.Sprintf("%d", i),
			})
		}

		// Validate PID config
		if motorConfig.PidConfig != nil {
			pidResult := s.validatePidConfig(fmt.Sprintf("motor_configs[%d].pid_config", i), motorConfig.PidConfig)
			errors = append(errors, pidResult.Errors...)
		}
	}

	// Validate safety thresholds
	if config.SafetyThresholds != nil {
		errors = append(errors, s.validateSafetyThresholds(config.SafetyThresholds)...)
	}

	// Validate encoder configuration
	if config.EncoderConfig != nil {
		errors = append(errors, s.validateEncoderConfig(config.EncoderConfig)...)
	}

	// Validate timing configuration
	if config.TimingConfig != nil {
		errors = append(errors, s.validateTimingConfig(config.TimingConfig)...)
	}

	// Determine overall status
	var validationStatus starv1.ConfigValidationStatus
	if len(errors) == 0 {
		validationStatus = starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID
	} else {
		validationStatus = starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID
	}

	return &starv1.ConfigValidationResult{
		Status: validationStatus,
		Errors: errors,
	}
}

// validatePidConfig validates PID configuration parameters.
func (s *ConfigurationService) validatePidConfig(fieldPrefix string, config *starv1.PidConfig) *starv1.ConfigValidationResult {
	var errors []*starv1.ConfigValidationError

	// Validate kp > 0
	if config.Kp <= minPidGain || math.IsNaN(config.Kp) || math.IsInf(config.Kp, 0) {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.kp", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "kp must be greater than 0",
			ActualValue:        fmt.Sprintf("%f", config.Kp),
			ExpectedConstraint: "> 0",
		})
	}

	// Validate ki >= 0 (integral can be zero)
	if config.Ki < minPidGain || math.IsNaN(config.Ki) || math.IsInf(config.Ki, 0) {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.ki", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "ki must be greater than or equal to 0",
			ActualValue:        fmt.Sprintf("%f", config.Ki),
			ExpectedConstraint: ">= 0",
		})
	}

	// Validate kd >= 0 (derivative can be zero)
	if config.Kd < minPidGain || math.IsNaN(config.Kd) || math.IsInf(config.Kd, 0) {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.kd", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "kd must be >= 0",
			ActualValue:        fmt.Sprintf("%f", config.Kd),
			ExpectedConstraint: ">= 0",
		})
	}

	// Validate output limits (min < max)
	if config.OutputMinPercent >= config.OutputMaxPercent {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.output_min_percent", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:            "output_min_percent must be less than output_max_percent",
			ActualValue:        fmt.Sprintf("%f", config.OutputMinPercent),
			ExpectedConstraint: fmt.Sprintf("< %f", config.OutputMaxPercent),
		})
	}

	// Validate output limits range
	if config.OutputMinPercent < minOutputPercent {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.output_min_percent", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "output_min_percent out of range",
			ActualValue:        fmt.Sprintf("%f", config.OutputMinPercent),
			ExpectedConstraint: fmt.Sprintf(">= %f", minOutputPercent),
		})
	}

	if config.OutputMaxPercent > maxOutputPercent {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.output_max_percent", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:            "output_max_percent out of range",
			ActualValue:        fmt.Sprintf("%f", config.OutputMaxPercent),
			ExpectedConstraint: fmt.Sprintf("<= %f", maxOutputPercent),
		})
	}

	// Validate integral limits (min < max)
	if config.IntegralMin >= config.IntegralMax {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          fmt.Sprintf("%s.integral_min", fieldPrefix),
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:            "integral_min must be less than integral_max",
			ActualValue:        fmt.Sprintf("%f", config.IntegralMin),
			ExpectedConstraint: fmt.Sprintf("< %f", config.IntegralMax),
		})
	}

	var validationStatus starv1.ConfigValidationStatus
	if len(errors) == 0 {
		validationStatus = starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_VALID
	} else {
		validationStatus = starv1.ConfigValidationStatus_CONFIG_VALIDATION_STATUS_INVALID
	}

	return &starv1.ConfigValidationResult{
		Status: validationStatus,
		Errors: errors,
	}
}

// validateSafetyThresholds validates safety threshold parameters.
func (s *ConfigurationService) validateSafetyThresholds(thresholds *starv1.SafetyThresholds) []*starv1.ConfigValidationError {
	var errors []*starv1.ConfigValidationError

	// Validate overcurrent threshold
	if thresholds.OvercurrentThresholdMa < minOvercurrentMa || thresholds.OvercurrentThresholdMa > maxOvercurrentMa {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "safety_thresholds.overcurrent_threshold_ma",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "overcurrent_threshold_ma out of valid range",
			ActualValue:        fmt.Sprintf("%d", thresholds.OvercurrentThresholdMa),
			ExpectedConstraint: fmt.Sprintf("%d-%d mA", minOvercurrentMa, maxOvercurrentMa),
		})
	}

	// Validate thermal shutdown
	if thresholds.ThermalShutdownDeciCelsius < minThermalShutdownDeciC || thresholds.ThermalShutdownDeciCelsius > maxThermalShutdownDeciC {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "safety_thresholds.thermal_shutdown_deci_celsius",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "thermal_shutdown_deci_celsius out of valid range",
			ActualValue:        fmt.Sprintf("%d", thresholds.ThermalShutdownDeciCelsius),
			ExpectedConstraint: fmt.Sprintf("%d-%d deci-C (%.1f-%.1f degC)", minThermalShutdownDeciC, maxThermalShutdownDeciC, float64(minThermalShutdownDeciC)/10, float64(maxThermalShutdownDeciC)/10),
		})
	}

	// Validate cell imbalance threshold
	if thresholds.CellImbalanceThresholdMv < minCellImbalanceThresholdMv || thresholds.CellImbalanceThresholdMv > maxCellImbalanceThresholdMv {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "safety_thresholds.cell_imbalance_threshold_mv",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "cell_imbalance_threshold_mv out of valid range",
			ActualValue:        fmt.Sprintf("%d", thresholds.CellImbalanceThresholdMv),
			ExpectedConstraint: fmt.Sprintf("%d-%d mV", minCellImbalanceThresholdMv, maxCellImbalanceThresholdMv),
		})
	}

	return errors
}

// validateEncoderConfig validates encoder configuration parameters.
func (s *ConfigurationService) validateEncoderConfig(config *starv1.EncoderConfiguration) []*starv1.ConfigValidationError {
	var errors []*starv1.ConfigValidationError

	// Validate edges_per_revolution > 0
	if config.EdgesPerRevolution < minEdgesPerRevolution {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "encoder_config.edges_per_revolution",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "edges_per_revolution must be greater than 0",
			ActualValue:        fmt.Sprintf("%d", config.EdgesPerRevolution),
			ExpectedConstraint: fmt.Sprintf("> %d", minEdgesPerRevolution-1),
		})
	}

	// Validate wheel diameter > 0
	if config.WheelDiameterM <= 0 || math.IsNaN(config.WheelDiameterM) || math.IsInf(config.WheelDiameterM, 0) {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "encoder_config.wheel_diameter_m",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "wheel_diameter_m must be greater than 0",
			ActualValue:        fmt.Sprintf("%f", config.WheelDiameterM),
			ExpectedConstraint: "> 0",
		})
	}

	// Validate gear ratio > 0
	if config.GearRatio <= 0 || math.IsNaN(config.GearRatio) || math.IsInf(config.GearRatio, 0) {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "encoder_config.gear_ratio",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "gear_ratio must be greater than 0",
			ActualValue:        fmt.Sprintf("%f", config.GearRatio),
			ExpectedConstraint: "> 0",
		})
	}

	return errors
}

// validateTimingConfig validates timing configuration parameters.
func (s *ConfigurationService) validateTimingConfig(config *starv1.TimingConfiguration) []*starv1.ConfigValidationError {
	var errors []*starv1.ConfigValidationError

	// Validate motor control period
	if config.MotorControlPeriodMs < minMotorControlPeriodMs {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "timing_config.motor_control_period_ms",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "motor_control_period_ms is too low",
			ActualValue:        fmt.Sprintf("%d", config.MotorControlPeriodMs),
			ExpectedConstraint: fmt.Sprintf(">= %d ms", minMotorControlPeriodMs),
		})
	}
	if config.MotorControlPeriodMs > maxMotorControlPeriodMs {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "timing_config.motor_control_period_ms",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:            "motor_control_period_ms is too high",
			ActualValue:        fmt.Sprintf("%d", config.MotorControlPeriodMs),
			ExpectedConstraint: fmt.Sprintf("<= %d ms", maxMotorControlPeriodMs),
		})
	}

	// Validate telemetry period
	if config.TelemetryPeriodMs < minTelemetryPeriodMs {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "timing_config.telemetry_period_ms",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "telemetry_period_ms is too low",
			ActualValue:        fmt.Sprintf("%d", config.TelemetryPeriodMs),
			ExpectedConstraint: fmt.Sprintf(">= %d ms", minTelemetryPeriodMs),
		})
	}
	if config.TelemetryPeriodMs > maxTelemetryPeriodMs {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "timing_config.telemetry_period_ms",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:            "telemetry_period_ms is too high",
			ActualValue:        fmt.Sprintf("%d", config.TelemetryPeriodMs),
			ExpectedConstraint: fmt.Sprintf("<= %d ms", maxTelemetryPeriodMs),
		})
	}

	// Validate communication timeout
	if config.CommunicationTimeoutMs < minCommunicationTimeoutMs {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "timing_config.communication_timeout_ms",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_LOW,
			Message:            "communication_timeout_ms is too low",
			ActualValue:        fmt.Sprintf("%d", config.CommunicationTimeoutMs),
			ExpectedConstraint: fmt.Sprintf(">= %d ms", minCommunicationTimeoutMs),
		})
	}
	if config.CommunicationTimeoutMs > maxCommunicationTimeoutMs {
		errors = append(errors, &starv1.ConfigValidationError{
			FieldPath:          "timing_config.communication_timeout_ms",
			ErrorCode:          starv1.ConfigErrorCode_CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
			Message:            "communication_timeout_ms is too high",
			ActualValue:        fmt.Sprintf("%d", config.CommunicationTimeoutMs),
			ExpectedConstraint: fmt.Sprintf("<= %d ms", maxCommunicationTimeoutMs),
		})
	}

	return errors
}
