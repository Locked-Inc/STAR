// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// December 2025
package service

import (
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// MotorControlService implements the MotorControlServiceServer interface.
// This service handles low-latency differential drive control.
type MotorControlService struct {
	starv1.UnimplementedMotorControlServiceServer
}

// NewMotorControlService creates a new MotorControlService.
func NewMotorControlService() *MotorControlService {
	return &MotorControlService{}
}

// TODO: Implement SetVelocity - Set wheel velocities for differential drive.
// func (s *MotorControlService) SetVelocity(ctx context.Context, req *starv1.SetVelocityRequest) (*starv1.SetVelocityResponse, error)

// TODO: Implement EmergencyStop - Immediate stop, priority over all commands.
// func (s *MotorControlService) EmergencyStop(ctx context.Context, req *starv1.EmergencyStopRequest) (*starv1.EmergencyStopResponse, error)

// TODO: Implement SetMotorPower - Direct PWM control (bypass PID, testing only).
// func (s *MotorControlService) SetMotorPower(ctx context.Context, req *starv1.SetMotorPowerRequest) (*starv1.SetMotorPowerResponse, error)

// TODO: Implement StreamEncoders - Server streams encoder readings at configured rate.
// func (s *MotorControlService) StreamEncoders(req *starv1.StreamEncodersRequest, stream starv1.MotorControlService_StreamEncodersServer) error

// TODO: Implement ControlStream - Bidirectional: velocity commands in, encoder feedback out.
// func (s *MotorControlService) ControlStream(stream starv1.MotorControlService_ControlStreamServer) error
