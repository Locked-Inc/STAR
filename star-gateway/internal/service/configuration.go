// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// December 2025
package service

import (
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// ConfigurationService implements the ConfigurationServiceServer interface.
// This service handles runtime parameter management with NVS persistence.
type ConfigurationService struct {
	starv1.UnimplementedConfigurationServiceServer
}

// NewConfigurationService creates a new ConfigurationService.
func NewConfigurationService() *ConfigurationService {
	return &ConfigurationService{}
}

// TODO: Implement GetConfiguration - Read current system configuration.
// func (s *ConfigurationService) GetConfiguration(ctx context.Context, req *starv1.GetConfigurationRequest) (*starv1.GetConfigurationResponse, error)

// TODO: Implement SetConfiguration - Apply configuration (optional NVS persist).
// func (s *ConfigurationService) SetConfiguration(ctx context.Context, req *starv1.SetConfigurationRequest) (*starv1.SetConfigurationResponse, error)

// TODO: Implement ResetToDefaults - Restore factory defaults.
// func (s *ConfigurationService) ResetToDefaults(ctx context.Context, req *starv1.ResetToDefaultsRequest) (*starv1.ResetToDefaultsResponse, error)

// TODO: Implement ValidateConfiguration - Validate without applying.
// func (s *ConfigurationService) ValidateConfiguration(ctx context.Context, req *starv1.ValidateConfigurationRequest) (*starv1.ValidateConfigurationResponse, error)

// TODO: Implement SaveConfiguration - Persist in-memory config to NVS.
// func (s *ConfigurationService) SaveConfiguration(ctx context.Context, req *starv1.SaveConfigurationRequest) (*starv1.SaveConfigurationResponse, error)

// TODO: Implement GetMotorPidConfig - Per-motor PID tuning read.
// func (s *ConfigurationService) GetMotorPidConfig(ctx context.Context, req *starv1.GetMotorPidConfigRequest) (*starv1.GetMotorPidConfigResponse, error)

// TODO: Implement SetMotorPidConfig - Per-motor PID tuning write.
// func (s *ConfigurationService) SetMotorPidConfig(ctx context.Context, req *starv1.SetMotorPidConfigRequest) (*starv1.SetMotorPidConfigResponse, error)
