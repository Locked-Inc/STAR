// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// December 2025
package service

import (
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// TelemetryService implements the TelemetryServiceServer interface.
// This service handles real-time sensor data aggregation and system health monitoring.
type TelemetryService struct {
	starv1.UnimplementedTelemetryServiceServer
}

// NewTelemetryService creates a new TelemetryService.
func NewTelemetryService() *TelemetryService {
	return &TelemetryService{}
}

// TODO: Implement GetTelemetry - Snapshot of all sensor data.
// func (s *TelemetryService) GetTelemetry(ctx context.Context, req *starv1.GetTelemetryRequest) (*starv1.GetTelemetryResponse, error)

// TODO: Implement StreamTelemetry - Continuous sensor updates at 1-100Hz.
// func (s *TelemetryService) StreamTelemetry(req *starv1.StreamTelemetryRequest, stream starv1.TelemetryService_StreamTelemetryServer) error

// TODO: Implement GetSystemStatus - Firmware version, uptime, connection states.
// func (s *TelemetryService) GetSystemStatus(ctx context.Context, req *starv1.GetSystemStatusRequest) (*starv1.GetSystemStatusResponse, error)
