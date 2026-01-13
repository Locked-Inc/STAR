// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// December 2025
package service

import (
	"context"
	"log"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/timestamppb"
)

// MotorControlService implements the MotorControlServiceServer interface.
// This service handles low-latency differential drive control.
type MotorControlService struct {
	starv1.UnimplementedMotorControlServiceServer
	harqHandler harq.HARQ
}

// NewMotorControlService creates a new MotorControlService.
func NewMotorControlService(h harq.HARQ) *MotorControlService {
	return &MotorControlService{
		harqHandler: h,
	}
}

// SetVelocity sets wheel velocities for differential drive.
// It serializes the command and sends it via HARQ to the RX72N.
func (s *MotorControlService) SetVelocity(ctx context.Context, req *starv1.SetVelocityRequest) (*starv1.SetVelocityResponse, error) {
	if req == nil || req.Command == nil {
		return nil, status.Error(codes.InvalidArgument, "request or command cannot be nil")
	}

	// 1. Serialize the VelocityCommand
	payload, err := proto.Marshal(req.Command)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to marshal velocity command: %v", err)
	}

	// 2. Send via HARQ (blocks until ACK or timeout/max retries)
	// Note: We are sending the raw serialized VelocityCommand.
	// The RX72N must know how to decode this specific message.
	// TODO: Verify if a wrapper message (oneof) is required by the firmware.
	if err := s.harqHandler.Send(payload); err != nil {
		log.Printf("Failed to send velocity command: %v", err)
		return nil, status.Errorf(codes.Unavailable, "failed to send command to motor controller: %v", err)
	}

	// 3. Construct response
	// Note: We do not have the immediate motor status from the RX72N in the ACK.
	// The status is typically streamed back via StreamEncoders or a separate Telemetry message.
	// We return an empty status list for now, or the last known status if cached (not implemented here).
	return &starv1.SetVelocityResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       req.Header.GetRequestId(),
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		MotorStatus: []*starv1.MotorStatus{}, // Populated by telemetry stream usually
	}, nil
}

// EmergencyStop triggers an immediate stop.
func (s *MotorControlService) EmergencyStop(ctx context.Context, req *starv1.EmergencyStopRequest) (*starv1.EmergencyStopResponse, error) {
	// TODO: Implement a specific EmergencyStop message or flag in the protocol.
	// For now, we will send a zero velocity command as a fallback if no E-Stop frame exists.
	// Real implementation should likely send a high-priority frame.

	zeroCmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps:  0,
		FrontRightVelocityMps: 0,
		BackLeftVelocityMps:   0,
		BackRightVelocityMps:  0,
		Sequence:              0, // Should be incremented
		TimestampUs:           time.Now().UnixMicro(),
	}

	payload, err := proto.Marshal(zeroCmd)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to marshal estop command: %v", err)
	}

	if err := s.harqHandler.Send(payload); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to send estop: %v", err)
	}

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
// This is a blocking call that reads from the HARQ receiver in a loop.
//
// WARNING: This assumes this service has exclusive access to Receive().
// If GatewayService or other services also call Receive(), this will contend/steal frames.
// A centralized dispatcher is recommended for production.
func (s *MotorControlService) StreamEncoders(req *starv1.StreamEncodersRequest, stream starv1.MotorControlService_StreamEncodersServer) error {
	// TODO: Implement rate limiting based on req.RateHz

	for {
		// Check context cancellation
		if stream.Context().Err() != nil {
			return stream.Context().Err()
		}

		// Block waiting for data from RX72N
		data, err := s.harqHandler.Receive()
		if err != nil {
			log.Printf("StreamEncoders: receive error: %v", err)
			// Don't exit immediately on transient errors, but maybe backoff
			time.Sleep(10 * time.Millisecond)
			continue
		}

		// Attempt to decode as EncoderData
		// TODO: The received frame might be ANY message (Telemetry, Response, etc.).
		// We need to know the type. Since we don't have a wrapper, we try to unmarshal.
		// If the wire format uses a wrapper, we should decode that first.

		var encData starv1.EncoderData
		if err := proto.Unmarshal(data, &encData); err != nil {
			// Not encoder data? Ignore.
			continue
		}

		if err := stream.Send(&encData); err != nil {
			return err
		}
	}
}

// ControlStream handles bidirectional streaming.
func (s *MotorControlService) ControlStream(stream starv1.MotorControlService_ControlStreamServer) error {
	// Start a goroutine to send commands from client to RX72N
	go func() {
		for {
			cmd, err := stream.Recv()
			if err != nil {
				return
			}

			payload, err := proto.Marshal(cmd)
			if err != nil {
				continue
			}

			// Best effort send
			_ = s.harqHandler.Send(payload)
		}
	}()

	// Main loop reads from RX72N and sends to client
	for {
		if stream.Context().Err() != nil {
			return stream.Context().Err()
		}

		data, err := s.harqHandler.Receive()
		if err != nil {
			time.Sleep(10 * time.Millisecond)
			continue
		}

		var encData starv1.EncoderData
		if err := proto.Unmarshal(data, &encData); err == nil {
			if err := stream.Send(&encData); err != nil {
				return err
			}
		}
	}
}
