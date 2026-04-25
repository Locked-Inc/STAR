// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"log/slog"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/timestamppb"
)

// processStart anchors a monotonic clock used to populate proto
// `timestamp_us` fields on outbound commands. The wire convention is
// "microseconds since the sender started", NOT Unix epoch, so the value
// is comparable against the firmware's own boot-relative timestamps for
// gateway-side latency math without producing trillions-of-seconds garbage
// when subtracted from RX72N's tx_time_get()-based timestamps.
//
// Both gateway and ROS2 must use the same convention; see
// star-ros2/.../message_converter.cpp for the ROS2 equivalent.
var processStart = time.Now()

// monoMicrosSinceStart returns microseconds elapsed since this process
// started. Use it for proto `timestamp_us` fields on outbound commands
// (VelocityCommand, EmergencyStopCommand, etc.) so that round-trip
// latency math `now - cmd.timestamp_us` stays inside one monotonic
// reference frame and never mixes wall-clock with boot-time microseconds.
//
// Returns: int64 microseconds since processStart (always positive,
// monotonically increasing, ~1e8 after a few seconds; never trillions).
func monoMicrosSinceStart() int64 {
	return time.Since(processStart).Microseconds()
}

// MotorControlService implements the MotorControlServiceServer interface.
// This service handles low-latency differential drive control.
//
// Architecture:
// - Uses WireMessage wrapper for protocol multiplexing (solves ambiguity)
// - Uses Dispatcher for centralized message routing (solves concurrency race)
// - Uses structured logging (slog) for production observability
type MotorControlService struct {
	starv1.UnimplementedMotorControlServiceServer
	harqHandler harq.HARQ
	dispatcher  dispatcher.Dispatcher
	logger      *slog.Logger
}

// NewMotorControlService creates a new MotorControlService.
// Requires HARQ handler, Dispatcher for receiving telemetry, and logger.
func NewMotorControlService(h harq.HARQ, d dispatcher.Dispatcher, logger *slog.Logger) *MotorControlService {
	return &MotorControlService{
		harqHandler: h,
		dispatcher:  d,
		logger:      logger,
	}
}

// SetVelocity sets wheel velocities for differential drive.
// Wraps the command in WireMessage for protocol multiplexing and sends via HARQ.
func (s *MotorControlService) SetVelocity(ctx context.Context, req *starv1.SetVelocityRequest) (*starv1.SetVelocityResponse, error) {
	if req == nil || req.Command == nil {
		return nil, status.Error(codes.InvalidArgument, "request or command cannot be nil")
	}

	// 1. Wrap VelocityCommand in WireMessage for protocol multiplexing
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_VelocityCommand{
			VelocityCommand: req.Command,
		},
	}

	// 2. Serialize the WireMessage
	payload, err := proto.Marshal(wrapper)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to marshal velocity command: %v", err)
	}

	// 3. Send via HARQ (blocks until ACK or timeout/max retries)
	// Note: Using normal priority for standard velocity commands.
	if err := s.harqHandler.Send(ctx, payload, harq.PriorityNormal); err != nil {
		s.logger.Error("failed to send velocity command",
			slog.String("request_id", req.Header.GetRequestId()),
			slog.String("error", err.Error()))
		return nil, status.Errorf(codes.Unavailable, "failed to send command to motor controller: %v", err)
	}

	// 4. Construct response
	// Note: Motor status is streamed back via StreamEncoders/telemetry.
	return &starv1.SetVelocityResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       req.Header.GetRequestId(),
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		MotorStatus: []*starv1.MotorStatus{}, // Populated by telemetry stream
	}, nil
}

// EmergencyStop triggers an immediate stop using dedicated EmergencyStopCommand.
// This enables the RX72N to enter MOTOR_STATE_ESTOP and engage hardware safety features.
//
// TODO (tracked in GitHub issue): Implement priority framing for E-Stop.
// The frame layer supports FlagPriority, but the HARQ interface doesn't expose it.
// EmergencyStop should use priority framing to ensure immediate processing by RX72N.
func (s *MotorControlService) EmergencyStop(ctx context.Context, req *starv1.EmergencyStopRequest) (*starv1.EmergencyStopResponse, error) {
	// Create dedicated EmergencyStopCommand. timestamp_us uses our monotonic
	// since-start clock so round-trip latency math stays consistent with the
	// RX72N's boot-relative timestamps (see comment on monoMicrosSinceStart).
	estopCmd := &starv1.EmergencyStopCommand{
		Reason:             req.Reason,
		EngageHardwareStop: true, // Request firmware to enter MOTOR_STATE_ESTOP
		TimestampUs:        monoMicrosSinceStart(),
	}

	// Wrap in WireMessage for protocol multiplexing
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_EmergencyStopCommand{
			EmergencyStopCommand: estopCmd,
		},
	}

	payload, err := proto.Marshal(wrapper)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to marshal estop command: %v", err)
	}

	// Send via HARQ with emergency priority
	// Note: Using high priority for emergency stop commands.
	if err := s.harqHandler.Send(ctx, payload, harq.PriorityEmergency); err != nil {
		s.logger.Error("failed to send emergency stop command",
			slog.String("request_id", req.Header.GetRequestId()),
			slog.String("reason", req.Reason),
			slog.String("error", err.Error()))
		return nil, status.Errorf(codes.Unavailable, "failed to send estop: %v", err)
	}

	s.logger.Warn("emergency stop engaged",
		slog.String("request_id", req.Header.GetRequestId()),
		slog.String("reason", req.Reason))

	return &starv1.EmergencyStopResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       req.Header.GetRequestId(),
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		EstopEngaged: true,
	}, nil
}

// SetMotorPower sets raw motor power (bypass PID).
func (s *MotorControlService) SetMotorPower(ctx context.Context, req *starv1.SetMotorPowerRequest) (*starv1.SetMotorPowerResponse, error) {
	// TODO: Implement specific message for MotorPower if defined in proto/firmware.
	return nil, status.Error(codes.Unimplemented, "SetMotorPower not implemented")
}

// StreamEncoders streams encoder readings from the motor controller.
// Uses Dispatcher to receive TelemetryData messages without contention.
func (s *MotorControlService) StreamEncoders(req *starv1.StreamEncodersRequest, stream starv1.MotorControlService_StreamEncodersServer) error {
	rateHz := s.validateRateHz(req.RateHz)
	ticker := time.NewTicker(time.Second / time.Duration(rateHz))
	defer ticker.Stop()

	ctx := stream.Context()
	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	var latestTelemetry *starv1.TelemetryData
	var telemetryMutex sync.RWMutex

	// Start goroutine to receive telemetry updates
	go s.receiveTelemetryLoop(ctx, telemetryCh, &latestTelemetry, &telemetryMutex)

	// Main loop sends encoder data at requested rate
	return s.streamEncoderLoop(ctx, ticker, stream, &latestTelemetry, &telemetryMutex)
}

// validateRateHz validates and normalizes the requested streaming rate.
func (s *MotorControlService) validateRateHz(rateHz int32) int32 {
	if rateHz <= 0 || rateHz > 100 {
		return 10 // Default 10 Hz
	}
	return rateHz
}

// receiveTelemetryLoop continuously receives telemetry from Dispatcher and updates latest value.
func (s *MotorControlService) receiveTelemetryLoop(ctx context.Context, telemetryCh <-chan *dispatcher.DispatchedMessage, latestTelemetry **starv1.TelemetryData, mu *sync.RWMutex) {
	for {
		select {
		case <-ctx.Done():
			return
		case dispMsg, ok := <-telemetryCh:
			if !ok {
				s.logger.Debug("telemetry subscription channel closed")
				return
			}

			telemetry := dispMsg.WireMsg.GetTelemetryData()
			if telemetry == nil {
				s.logger.Warn("received wire message with nil telemetry data")
				continue
			}

			mu.Lock()
			*latestTelemetry = telemetry
			mu.Unlock()
		}
	}
}

// streamEncoderLoop sends encoder data at the requested rate.
func (s *MotorControlService) streamEncoderLoop(ctx context.Context, ticker *time.Ticker, stream starv1.MotorControlService_StreamEncodersServer, latestTelemetry **starv1.TelemetryData, mu *sync.RWMutex) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			mu.RLock()
			currentTelemetry := *latestTelemetry
			mu.RUnlock()

			if currentTelemetry != nil {
				if err := s.sendEncoderData(stream, currentTelemetry); err != nil {
					return err
				}
			}
		}
	}
}

// sendEncoderData sends encoder data from all four motors to the stream.
func (s *MotorControlService) sendEncoderData(stream starv1.MotorControlService_StreamEncodersServer, telemetry *starv1.TelemetryData) error {
	encoders := []*starv1.EncoderData{
		telemetry.EncoderFrontLeft,
		telemetry.EncoderFrontRight,
		telemetry.EncoderBackLeft,
		telemetry.EncoderBackRight,
	}

	for _, encData := range encoders {
		if encData != nil && encData.TimestampUs > 0 {
			if err := stream.Send(encData); err != nil {
				s.logger.Error("failed to send encoder data", slog.String("error", err.Error()))
				return err
			}
		}
	}
	return nil
}

// ControlStream handles bidirectional streaming.
// Client streams velocity commands in, server streams encoder feedback out.
// Uses Dispatcher to receive telemetry and wraps commands in WireMessage.
func (s *MotorControlService) ControlStream(stream starv1.MotorControlService_ControlStreamServer) error {
	ctx := stream.Context()
	errChan := make(chan error, 1)

	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	// Start goroutine to receive commands from client and send to RX72N
	go s.receiveCommandLoop(ctx, stream, errChan)

	// Main loop receives telemetry from Dispatcher and sends encoder feedback to client
	return s.sendTelemetryLoop(ctx, stream, telemetryCh, errChan)
}

// receiveCommandLoop receives velocity commands from client and forwards to RX72N via HARQ.
func (s *MotorControlService) receiveCommandLoop(ctx context.Context, stream starv1.MotorControlService_ControlStreamServer, errChan chan<- error) {
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		cmd, err := stream.Recv()
		if err != nil {
			select {
			case errChan <- err:
			default:
			}
			return
		}

		if err := s.forwardVelocityCommand(ctx, cmd); err != nil {
			s.logger.Warn("failed to forward command", slog.String("error", err.Error()))
		}
	}
}

// forwardVelocityCommand wraps and sends a velocity command to RX72N.
func (s *MotorControlService) forwardVelocityCommand(ctx context.Context, cmd *starv1.VelocityCommand) error {
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_VelocityCommand{
			VelocityCommand: cmd,
		},
	}

	payload, err := proto.Marshal(wrapper)
	if err != nil {
		s.logger.Error("failed to marshal velocity command", slog.String("error", err.Error()))
		return err
	}

	return s.harqHandler.Send(ctx, payload, harq.PriorityNormal)
}

// sendTelemetryLoop receives telemetry from Dispatcher and sends encoder feedback to client.
func (s *MotorControlService) sendTelemetryLoop(ctx context.Context, stream starv1.MotorControlService_ControlStreamServer, telemetryCh <-chan *dispatcher.DispatchedMessage, errChan <-chan error) error {
	for {
		select {
		case <-ctx.Done():
			time.Sleep(10 * time.Millisecond) // Wait for send goroutine
			return ctx.Err()
		case err := <-errChan:
			return err
		case dispMsg, ok := <-telemetryCh:
			if !ok {
				s.logger.Debug("telemetry subscription channel closed in control stream")
				return nil
			}

			telemetry := dispMsg.WireMsg.GetTelemetryData()
			if telemetry == nil {
				s.logger.Warn("received wire message with nil telemetry data in control stream")
				continue
			}

			if err := s.sendEncoderData(stream, telemetry); err != nil {
				return err
			}
		}
	}
}
