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
// Sends a zero velocity command to halt all motors.
//
// TODO (tracked in GitHub issue): Implement priority framing for E-Stop.
// The frame layer supports FlagPriority, but the HARQ interface doesn't expose it.
// EmergencyStop should use priority framing to ensure immediate processing by RX72N.
// Current implementation sends zero velocity as a regular command without priority.
func (s *MotorControlService) EmergencyStop(ctx context.Context, req *starv1.EmergencyStopRequest) (*starv1.EmergencyStopResponse, error) {
	// Create zero velocity command to stop all motors
	zeroCmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps:  0,
		FrontRightVelocityMps: 0,
		BackLeftVelocityMps:   0,
		BackRightVelocityMps:  0,
		Sequence:              0,
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
// A centralized dispatcher is recommended for production (see GitHub issue for MessageDispatcher).
func (s *MotorControlService) StreamEncoders(req *starv1.StreamEncodersRequest, stream starv1.MotorControlService_StreamEncodersServer) error {
	// Validate and set rate limiting
	rateHz := req.RateHz
	if rateHz <= 0 || rateHz > 100 {
		rateHz = 10 // Default 10 Hz
	}
	ticker := time.NewTicker(time.Second / time.Duration(rateHz))
	defer ticker.Stop()

	// Channel for latest telemetry data
	telemetryChan := make(chan *starv1.TelemetryData, 1)
	errChan := make(chan error, 1)
	ctx := stream.Context()

	// Goroutine to continuously receive telemetry from RX72N
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			default:
			}

			// Block waiting for data from RX72N
			data, err := s.harqHandler.Receive()
			if err != nil {
				log.Printf("StreamEncoders: receive error: %v", err)
				time.Sleep(10 * time.Millisecond)
				continue
			}

			// Unmarshal as TelemetryData (contains all encoder data)
			// The RX72N sends full TelemetryData messages which include encoder_front_left,
			// encoder_front_right, encoder_back_left, encoder_back_right fields.
			var telemetry starv1.TelemetryData
			if err := proto.Unmarshal(data, &telemetry); err != nil {
				log.Printf("StreamEncoders: failed to unmarshal TelemetryData: %v", err)
				continue
			}

			// Non-blocking send to update latest data
			select {
			case telemetryChan <- &telemetry:
			default:
				// Channel full, drop old data
				<-telemetryChan
				telemetryChan <- &telemetry
			}
		}
	}()

	// Main loop sends encoder data at requested rate
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case err := <-errChan:
			return err
		case <-ticker.C:
			// Send latest telemetry's encoder data
			select {
			case telemetry := <-telemetryChan:
				// Extract and stream individual encoder data from all four motors
				// The TelemetryData message contains EncoderData for each motor
				for _, encData := range []*starv1.EncoderData{
					telemetry.EncoderFrontLeft,
					telemetry.EncoderFrontRight,
					telemetry.EncoderBackLeft,
					telemetry.EncoderBackRight,
				} {
					if encData != nil && encData.TimestampUs > 0 {
						if err := stream.Send(encData); err != nil {
							return err
						}
					}
				}
				// Put telemetry back for next tick
				select {
				case telemetryChan <- telemetry:
				default:
				}
			default:
				// No data available yet, skip this tick
			}
		}
	}
}

// ControlStream handles bidirectional streaming.
// Client streams velocity commands in, server streams encoder feedback out.
func (s *MotorControlService) ControlStream(stream starv1.MotorControlService_ControlStreamServer) error {
	ctx := stream.Context()
	errChan := make(chan error, 1)

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

			payload, err := proto.Marshal(cmd)
			if err != nil {
				log.Printf("ControlStream: failed to marshal command: %v", err)
				continue
			}

			// Best effort send to RX72N
			if err := s.harqHandler.Send(payload); err != nil {
				log.Printf("ControlStream: failed to send command: %v", err)
			}
		}
	}()

	// Main loop reads from RX72N and sends encoder feedback to client
	for {
		select {
		case <-ctx.Done():
			// Wait a moment for send goroutine to finish
			time.Sleep(10 * time.Millisecond)
			return ctx.Err()
		case err := <-errChan:
			// Send goroutine encountered error
			return err
		default:
		}

		// Receive telemetry from RX72N
		data, err := s.harqHandler.Receive()
		if err != nil {
			time.Sleep(10 * time.Millisecond)
			continue
		}

		// Unmarshal as TelemetryData (contains all encoder data)
		var telemetry starv1.TelemetryData
		if err := proto.Unmarshal(data, &telemetry); err != nil {
			log.Printf("ControlStream: failed to unmarshal TelemetryData: %v", err)
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
					return err
				}
			}
		}
	}
}
