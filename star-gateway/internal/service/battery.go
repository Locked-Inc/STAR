// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// December 2025
package service

import (
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// BatteryService implements the BatteryManagementServiceServer interface.
// This service handles BQ7850 BMS monitoring for lithium-ion battery pack safety.
type BatteryService struct {
	starv1.UnimplementedBatteryManagementServiceServer
}

// NewBatteryService creates a new BatteryService.
func NewBatteryService() *BatteryService {
	return &BatteryService{}
}

// TODO: Implement GetBatteryState - Complete battery snapshot (cells, temps, SOC).
// func (s *BatteryService) GetBatteryState(ctx context.Context, req *starv1.GetBatteryStateRequest) (*starv1.GetBatteryStateResponse, error)

// TODO: Implement StreamBatteryState - Continuous battery monitoring.
// func (s *BatteryService) StreamBatteryState(req *starv1.StreamBatteryStateRequest, stream starv1.BatteryManagementService_StreamBatteryStateServer) error

// TODO: Implement GetProtectionThresholds - OV/UV/OC/OT protection limits.
// func (s *BatteryService) GetProtectionThresholds(ctx context.Context, req *starv1.GetProtectionThresholdsRequest) (*starv1.GetProtectionThresholdsResponse, error)

// TODO: Implement SetProtectionThresholds - Configure protection limits.
// func (s *BatteryService) SetProtectionThresholds(ctx context.Context, req *starv1.SetProtectionThresholdsRequest) (*starv1.SetProtectionThresholdsResponse, error)

// TODO: Implement EnableCellBalancing - Per-cell passive balancing control.
// func (s *BatteryService) EnableCellBalancing(ctx context.Context, req *starv1.EnableCellBalancingRequest) (*starv1.EnableCellBalancingResponse, error)

// TODO: Implement DisableCellBalancing - Disable cell balancing.
// func (s *BatteryService) DisableCellBalancing(ctx context.Context, req *starv1.DisableCellBalancingRequest) (*starv1.DisableCellBalancingResponse, error)

// TODO: Implement ControlFets - Charge/discharge FET enable/disable.
// func (s *BatteryService) ControlFets(ctx context.Context, req *starv1.ControlFetsRequest) (*starv1.ControlFetsResponse, error)

// TODO: Implement GetDeviceInfo - Manufacturer, serial, chemistry, versions.
// func (s *BatteryService) GetDeviceInfo(ctx context.Context, req *starv1.GetDeviceInfoRequest) (*starv1.GetDeviceInfoResponse, error)
