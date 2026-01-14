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
	if err := s.harqHandler.Send(payload); err != nil {
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
	// Create dedicated EmergencyStopCommand
	estopCmd := &starv1.EmergencyStopCommand{
		Reason:             req.Reason,
		EngageHardwareStop: true, // Request firmware to enter MOTOR_STATE_ESTOP
		TimestampUs:        time.Now().UnixMicro(),
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

	if err := s.harqHandler.Send(payload); err != nil {
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
	// Validate and set rate limiting
	rateHz := req.RateHz
	if rateHz <= 0 || rateHz > 100 {
		rateHz = 10 // Default 10 Hz
	}
	ticker := time.NewTicker(time.Second / time.Duration(rateHz))
	defer ticker.Stop()

	ctx := stream.Context()

	// Subscribe to TelemetryData messages from Dispatcher
	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	// Channel for latest telemetry data
	var latestTelemetry *starv1.TelemetryData
	var telemetryMutex sync.RWMutex

	// Goroutine to continuously receive telemetry from Dispatcher
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case wireMsg, ok := <-telemetryCh:
				if !ok {
					// Channel closed by dispatcher
					s.logger.Debug("telemetry subscription channel closed")
					return
				}

				// Extract TelemetryData from WireMessage
				telemetry := wireMsg.GetTelemetryData()
				if telemetry == nil {
					s.logger.Warn("received wire message with nil telemetry data")
					continue
				}

				// Update latest telemetry with mutex protection
				telemetryMutex.Lock()
				latestTelemetry = telemetry
				telemetryMutex.Unlock()
			}
		}
	}()

	// Main loop sends encoder data at requested rate
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			// Send latest telemetry's encoder data if available
			telemetryMutex.RLock()
			currentTelemetry := latestTelemetry
			telemetryMutex.RUnlock()

			if currentTelemetry != nil {
				// Extract and stream individual encoder data from all four motors
				for _, encData := range []*starv1.EncoderData{
					currentTelemetry.EncoderFrontLeft,
					currentTelemetry.EncoderFrontRight,
					currentTelemetry.EncoderBackLeft,
					currentTelemetry.EncoderBackRight,
				} {
					if encData != nil && encData.TimestampUs > 0 {
						if err := stream.Send(encData); err != nil {
							s.logger.Error("failed to send encoder data",
								slog.String("error", err.Error()))
							return err
						}
					}
				}
			}
		}
	}
}

// ControlStream handles bidirectional streaming.
// Client streams velocity commands in, server streams encoder feedback out.
// Uses Dispatcher to receive telemetry and wraps commands in WireMessage.
func (s *MotorControlService) ControlStream(stream starv1.MotorControlService_ControlStreamServer) error {
	ctx := stream.Context()
	errChan := make(chan error, 1)

	// Subscribe to TelemetryData messages from Dispatcher
	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	// Goroutine to send commands from client to RX72N
	go func() {
		for {
			select {
			case <-ctx.Done():
				// Stream context cancelled, exit goroutine cleanly
				return
			default:
			}

			cmd, err := stream.Recv()
			if err != nil {
				// Stream closed or error receiving
				select {
				case errChan <- err:
				default:
				}
				return
			}

			// Wrap VelocityCommand in WireMessage for protocol multiplexing
			wrapper := &starv1.WireMessage{
				Payload: &starv1.WireMessage_VelocityCommand{
					VelocityCommand: cmd,
				},
			}

			payload, err := proto.Marshal(wrapper)
			if err != nil {
				s.logger.Error("failed to marshal velocity command in control stream",
					slog.String("error", err.Error()))
				continue
			}

			// Best effort send to RX72N
			if err := s.harqHandler.Send(payload); err != nil {
				s.logger.Warn("failed to send command in control stream",
					slog.String("error", err.Error()))
			}
		}
	}()

	// Main loop receives telemetry from Dispatcher and sends encoder feedback to client
	for {
		select {
		case <-ctx.Done():
			// Wait a moment for send goroutine to finish
			time.Sleep(10 * time.Millisecond)
			return ctx.Err()
		case err := <-errChan:
			// Send goroutine encountered error
			return err
		case wireMsg, ok := <-telemetryCh:
			if !ok {
				// Channel closed by dispatcher
				s.logger.Debug("telemetry subscription channel closed in control stream")
				return nil
			}

			// Extract TelemetryData from WireMessage
			telemetry := wireMsg.GetTelemetryData()
			if telemetry == nil {
				s.logger.Warn("received wire message with nil telemetry data in control stream")
				continue
			}

			// Send encoder data from all four motors to client
			for _, encData := range []*starv1.EncoderData{
				telemetry.EncoderFrontLeft,
				telemetry.EncoderFrontRight,
				telemetry.EncoderBackLeft,
				telemetry.EncoderBackRight,
			} {
				if encData != nil && encData.TimestampUs > 0 {
					if err := stream.Send(encData); err != nil {
						s.logger.Error("failed to send encoder data in control stream",
							slog.String("error", err.Error()))
						return err
					}
				}
			}
		}
	}
}
