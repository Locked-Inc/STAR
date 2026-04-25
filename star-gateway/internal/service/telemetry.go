// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

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
	"google.golang.org/protobuf/types/known/timestamppb"
)

const (
	// MinRateHz is the minimum allowed streaming rate in Hz.
	MinRateHz = 1
	// MaxRateHz is the maximum allowed streaming rate in Hz.
	MaxRateHz = 100
	// DefaultRateHz is the default streaming rate when invalid rate is provided.
	DefaultRateHz = 10
)

// telemetryHolder provides thread-safe access to telemetry data.
type telemetryHolder struct {
	mu   sync.RWMutex
	data *starv1.TelemetryData
}

// Load returns the current telemetry data.
func (h *telemetryHolder) Load() *starv1.TelemetryData {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return h.data
}

// Store updates the telemetry data.
func (h *telemetryHolder) Store(data *starv1.TelemetryData) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.data = data
}

// TelemetryService implements the TelemetryServiceServer interface.
// This service handles real-time sensor data aggregation and system health monitoring.
//
// Architecture:
// - Uses WireMessage wrapper for protocol multiplexing
// - Uses Dispatcher for centralized message routing
// - Uses telemetryHolder for thread-safe caching of latest telemetry
// - Background goroutine updates cached telemetry from dispatcher
type TelemetryService struct {
	starv1.UnimplementedTelemetryServiceServer
	harqHandler     harq.HARQ // Reserved for future firmware/HARQ integration (GetSystemStatus, etc.)
	dispatcher      dispatcher.Dispatcher
	logger          *slog.Logger
	latestTelemetry *telemetryHolder
	ctx             context.Context
	cancel          context.CancelFunc
	wg              sync.WaitGroup
	started         chan struct{} // Signal when background goroutine initialized
}

// NewTelemetryService creates a new TelemetryService.
// Requires a context for lifecycle management, HARQ handler for sending requests,
// Dispatcher for receiving telemetry, and logger.
func NewTelemetryService(ctx context.Context, h harq.HARQ, d dispatcher.Dispatcher, logger *slog.Logger) *TelemetryService {
	ctx, cancel := context.WithCancel(ctx)
	svc := &TelemetryService{
		harqHandler:     h,
		dispatcher:      d,
		logger:          logger,
		latestTelemetry: &telemetryHolder{},
		ctx:             ctx,
		cancel:          cancel,
		started:         make(chan struct{}),
	}

	// Start background goroutine to update cached telemetry
	svc.wg.Add(1)
	go svc.updateTelemetryCache()

	// Wait for background goroutine to initialize (with timeout)
	select {
	case <-svc.started:
		// Goroutine initialized successfully
	case <-time.After(5 * time.Second):
		panic("telemetry service background goroutine failed to start within 5 seconds")
	}

	return svc
}

// updateTelemetryCache continuously receives telemetry from Dispatcher and updates cache.
// This goroutine runs until the service context is cancelled.
func (s *TelemetryService) updateTelemetryCache() {
	defer s.wg.Done()
	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	// Signal that initialization is complete
	close(s.started)

	for {
		// Prioritize context cancellation by checking it first (non-blocking)
		select {
		case <-s.ctx.Done():
			return
		default:
		}

		select {
		case <-s.ctx.Done():
			return
		case dispMsg, ok := <-telemetryCh:
			if !ok {
				return
			}
			telemetry := dispMsg.WireMsg.GetTelemetryData()
			if telemetry == nil {
				s.logger.Warn("received wire message with nil telemetry data in cache update")
				continue
			}

			// Populate frame sequence from metadata
			if dispMsg.Metadata != nil {
				telemetry.FrameSequence = uint32(dispMsg.Metadata.Sequence)
			}

			s.latestTelemetry.Store(telemetry)
		}
	}
}

// Shutdown gracefully stops the TelemetryService background goroutines.
func (s *TelemetryService) Shutdown() {
	s.cancel()
	s.wg.Wait()
}

// GetTelemetry returns a snapshot of the latest telemetry data.
// Uses cached telemetry updated by background goroutine for low latency.
func (s *TelemetryService) GetTelemetry(_ context.Context, req *starv1.GetTelemetryRequest) (*starv1.GetTelemetryResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	telemetry := s.latestTelemetry.Load()

	if telemetry == nil {
		return nil, status.Error(codes.Unavailable, "no telemetry data available")
	}

	// Safely extract request ID, handling nil Header
	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}

	return &starv1.GetTelemetryResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       requestID,
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		Telemetry: telemetry,
	}, nil
}

// StreamTelemetry streams telemetry data at the requested rate (1-100 Hz).
// Uses Dispatcher to receive telemetry and sends at controlled intervals.
func (s *TelemetryService) StreamTelemetry(req *starv1.StreamTelemetryRequest, stream starv1.TelemetryService_StreamTelemetryServer) error {
	// Validate rate
	rateHz := s.validateRateHz(req.RateHz)
	ticker := time.NewTicker(time.Second / time.Duration(rateHz))
	defer ticker.Stop()

	ctx := stream.Context()
	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	holder := &telemetryHolder{}

	// Start goroutine to receive telemetry updates
	go s.receiveStreamTelemetryLoop(ctx, telemetryCh, holder)

	// Main loop sends telemetry at requested rate
	return s.streamTelemetryLoop(ctx, ticker, stream, holder)
}

// validateRateHz validates and normalizes the requested streaming rate.
// Valid range: MinRateHz-MaxRateHz, default DefaultRateHz.
func (s *TelemetryService) validateRateHz(rateHz int32) int32 {
	if rateHz < MinRateHz || rateHz > MaxRateHz {
		return DefaultRateHz
	}
	return rateHz
}

// receiveStreamTelemetryLoop continuously receives telemetry from Dispatcher and updates latest value.
func (s *TelemetryService) receiveStreamTelemetryLoop(ctx context.Context, telemetryCh <-chan *dispatcher.DispatchedMessage, holder *telemetryHolder) {
	for {
		// Prioritize context cancellation by checking it first (non-blocking)
		select {
		case <-ctx.Done():
			return
		default:
		}

		select {
		case <-ctx.Done():
			return
		case dispMsg, ok := <-telemetryCh:
			if !ok {
				s.logger.Debug("telemetry subscription channel closed in stream")
				return
			}

			telemetry := dispMsg.WireMsg.GetTelemetryData()
			if telemetry == nil {
				s.logger.Warn("received wire message with nil telemetry data in stream")
				continue
			}

			// Populate frame sequence from metadata
			if dispMsg.Metadata != nil {
				telemetry.FrameSequence = uint32(dispMsg.Metadata.Sequence)
			}

			holder.Store(telemetry)
		}
	}
}

// streamTelemetryLoop sends telemetry data at the requested rate.
func (s *TelemetryService) streamTelemetryLoop(ctx context.Context, ticker *time.Ticker, stream starv1.TelemetryService_StreamTelemetryServer, holder *telemetryHolder) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			currentTelemetry := holder.Load()

			if currentTelemetry != nil {
				if err := stream.Send(currentTelemetry); err != nil {
					s.logger.Error("failed to send telemetry data", slog.String("error", err.Error()))
					return err
				}
			}
		}
	}
}

// GetSystemStatus requests system status from the RX72N firmware.
// TODO: Implement proper SystemStatusRequest message in wire.proto and send via HARQ.
// For now, returns mock status until firmware integration is complete.
func (s *TelemetryService) GetSystemStatus(_ context.Context, req *starv1.GetSystemStatusRequest) (*starv1.GetSystemStatusResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// TODO: When SystemStatusRequest is added to wire.proto:
	// 1. Wrap SystemStatusRequest in WireMessage
	// 2. Marshal and send via HARQ
	// 3. Subscribe to dispatcher for SystemStatus response
	// 4. Parse and return actual status

	// For now, return mock system status
	systemStatus := &starv1.SystemStatus{
		ConnectionStatus: starv1.ConnectionStatus_CONNECTION_STATUS_CONNECTED,
		Mode:             starv1.RobotMode_ROBOT_MODE_IDLE,
		Rx72NConnected:   true,
		LidarConnected:   false,
		RosConnected:     true,
		FirmwareVersion:  "v0.1.0-dev",
		UptimeS:          0,
	}

	// Safely extract request ID, handling nil Header
	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}

	return &starv1.GetSystemStatusResponse{
		Header: &starv1.ResponseHeader{
			RequestId:       requestID,
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		},
		Status: systemStatus,
	}, nil
}
