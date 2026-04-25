// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// December 2025
package service

import (
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// FirmwareService implements the FirmwareUpdateServiceServer interface.
// This service handles over-the-air (OTA) firmware updates with validation and rollback.
type FirmwareService struct {
	starv1.UnimplementedFirmwareUpdateServiceServer
}

// NewFirmwareService creates a new FirmwareService.
func NewFirmwareService() *FirmwareService {
	return &FirmwareService{}
}

// TODO: Implement BeginUpdate - Initialize OTA partition, set expected size/CRC.
// func (s *FirmwareService) BeginUpdate(ctx context.Context, req *starv1.BeginUpdateRequest) (*starv1.BeginUpdateResponse, error)

// TODO: Implement WriteChunk - Write single firmware chunk.
// func (s *FirmwareService) WriteChunk(ctx context.Context, req *starv1.WriteChunkRequest) (*starv1.WriteChunkResponse, error)

// TODO: Implement StreamChunks - Bulk chunk transfer (client streaming).
// func (s *FirmwareService) StreamChunks(stream starv1.FirmwareUpdateService_StreamChunksServer) error

// TODO: Implement FinalizeUpdate - Validate firmware, prepare for reboot.
// func (s *FirmwareService) FinalizeUpdate(ctx context.Context, req *starv1.FinalizeUpdateRequest) (*starv1.FinalizeUpdateResponse, error)

// TODO: Implement AbortUpdate - Cancel and cleanup.
// func (s *FirmwareService) AbortUpdate(ctx context.Context, req *starv1.AbortUpdateRequest) (*starv1.AbortUpdateResponse, error)

// TODO: Implement GetUpdateProgress - Transfer progress and state.
// func (s *FirmwareService) GetUpdateProgress(ctx context.Context, req *starv1.GetUpdateProgressRequest) (*starv1.GetUpdateProgressResponse, error)

// TODO: Implement StreamUpdateProgress - Stream progress updates.
// func (s *FirmwareService) StreamUpdateProgress(req *starv1.StreamUpdateProgressRequest, stream starv1.FirmwareUpdateService_StreamUpdateProgressServer) error

// TODO: Implement Reboot - Boot into new firmware.
// func (s *FirmwareService) Reboot(ctx context.Context, req *starv1.RebootRequest) (*starv1.RebootResponse, error)

// TODO: Implement Rollback - Revert to previous firmware.
// func (s *FirmwareService) Rollback(ctx context.Context, req *starv1.RollbackRequest) (*starv1.RollbackResponse, error)

// TODO: Implement MarkValid - Confirm firmware stability.
// func (s *FirmwareService) MarkValid(ctx context.Context, req *starv1.MarkValidRequest) (*starv1.MarkValidResponse, error)
