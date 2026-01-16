// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"fmt"
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

const (
	// MinBatteryRateHz is the minimum supported streaming rate in Hz (BMS hardware limited to 1-10 Hz).
	MinBatteryRateHz = 1
	// MaxBatteryRateHz is the maximum supported streaming rate in Hz.
	MaxBatteryRateHz = 10
	// DefaultBatteryRateHz is the default streaming rate in Hz.
	DefaultBatteryRateHz = 1

	// Protection threshold validation (Li-ion safe ranges)
	minOvervoltageMv   = 4200  // 4.20V
	maxOvervoltageMv   = 4350  // 4.35V
	minUndervoltageMv  = 2500  // 2.50V
	maxUndervoltageMv  = 3000  // 3.00V
	minOverchargeMa    = 1000  // 1A
	maxOverchargeMa    = 10000 // 10A
	minOverdischargeMa = 1000  // 1A
	maxOverdischargeMa = 20000 // 20A
	minOvertempDeciC   = 450   // 45.0°C
	maxOvertempDeciC   = 700   // 70.0°C
	minUndertempDeciC  = -200  // -20.0°C
	maxUndertempDeciC  = 0     // 0.0°C

	// Cell balancing validation (BQ7850 supports 16 cells max)
	maxCellMask = 0xFFFF // 16 bits

	// Battery state staleness detection
	nilWarningThreshold = 3 // Log warning after 3 consecutive nil readings
)

// batteryHolder provides thread-safe access to battery state data.
type batteryHolder struct {
	mu   sync.RWMutex
	data *starv1.BatteryState
}

// Load returns a defensive copy of the current battery state to prevent external mutation.
func (h *batteryHolder) Load() *starv1.BatteryState {
	h.mu.RLock()
	defer h.mu.RUnlock()
	if h.data == nil {
		return nil
	}
	// Return deep copy to prevent callers from mutating shared data
	cloned := proto.Clone(h.data)
	batteryState, ok := cloned.(*starv1.BatteryState)
	if !ok {
		return nil
	}
	return batteryState
}

// Store updates the battery state.
func (h *batteryHolder) Store(data *starv1.BatteryState) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.data = data
}

// BatteryService implements the BatteryManagementServiceServer interface.
// This service handles BQ7850 BMS monitoring for lithium-ion battery pack safety.
//
// Architecture:
// - Uses HARQ for reliable request/response communication
// - Uses Dispatcher for centralized message routing
// - Uses batteryHolder for thread-safe caching of latest battery state
// - Validates all safety-critical operations (thresholds, FET control)
type BatteryService struct {
	starv1.UnimplementedBatteryManagementServiceServer
	harqHandler   harq.HARQ
	dispatcher    dispatcher.Dispatcher
	logger        *slog.Logger
	latestBattery *batteryHolder
	ctx           context.Context
	cancel        context.CancelFunc
	wg            sync.WaitGroup
}

// NewBatteryService creates a new BatteryService.
// Requires a context for lifecycle management, HARQ handler for sending requests,
// Dispatcher for receiving battery state updates, and logger.
// Panics if any dependency is nil.
func NewBatteryService(ctx context.Context, h harq.HARQ, d dispatcher.Dispatcher, logger *slog.Logger) *BatteryService {
	// Validate dependencies
	if h == nil {
		panic("NewBatteryService: harq.HARQ cannot be nil")
	}
	if d == nil {
		panic("NewBatteryService: dispatcher.Dispatcher cannot be nil")
	}
	if logger == nil {
		panic("NewBatteryService: slog.Logger cannot be nil")
	}

	ctx, cancel := context.WithCancel(ctx)
	svc := &BatteryService{
		harqHandler:   h,
		dispatcher:    d,
		logger:        logger,
		latestBattery: &batteryHolder{},
		ctx:           ctx,
		cancel:        cancel,
	}

	// Start background goroutine to update cached battery state
	svc.wg.Add(1)
	go svc.updateBatteryCache()

	return svc
}

// updateBatteryCache continuously receives battery state from Dispatcher and updates cache.
// This goroutine runs until the service context is cancelled.
func (s *BatteryService) updateBatteryCache() {
	defer s.wg.Done()
	batteryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeBatteryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeBatteryData, batteryCh)

	for {
		select {
		case <-s.ctx.Done():
			return
		case wireMsg, ok := <-batteryCh:
			if !ok {
				return
			}
			if wireMsg == nil {
				s.logger.Warn("received nil wire message from dispatcher in battery cache update")
				continue
			}
			batteryState := wireMsg.GetBatteryState()
			if batteryState == nil {
				s.logger.Warn("received wire message with nil battery state in cache update")
				continue
			}
			s.latestBattery.Store(batteryState)
		}
	}
}

// Shutdown gracefully stops the BatteryService background goroutines.
func (s *BatteryService) Shutdown() {
	s.cancel()
	s.wg.Wait()
}

// sendProtoMessage marshals a protobuf message and sends it via HARQ.
func (s *BatteryService) sendProtoMessage(ctx context.Context, msg proto.Message) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		s.logger.Error("failed to marshal proto message", slog.String("error", err.Error()))
		return fmt.Errorf("marshal proto: %w", err)
	}

	if err := s.harqHandler.Send(ctx, data); err != nil {
		s.logger.Error("HARQ send failed", slog.String("error", err.Error()))
		return fmt.Errorf("HARQ send: %w", err)
	}

	return nil
}

// receiveProtoMessage receives data via HARQ and unmarshals into the target message.
func (s *BatteryService) receiveProtoMessage(ctx context.Context, target proto.Message) error {
	data, err := s.harqHandler.Receive(ctx)
	if err != nil {
		s.logger.Error("HARQ receive failed", slog.String("error", err.Error()))
		return fmt.Errorf("HARQ receive: %w", err)
	}

	if err := proto.Unmarshal(data, target); err != nil {
		s.logger.Error("failed to unmarshal proto message", slog.String("error", err.Error()))
		return fmt.Errorf("unmarshal proto: %w", err)
	}

	return nil
}

// ensureResponseHeader ensures response has valid header with request tracking.
func (s *BatteryService) ensureResponseHeader(
	reqHeader *starv1.RequestHeader,
	respHeader **starv1.ResponseHeader,
	warnMsg string,
) {
	requestID := ""
	if reqHeader != nil {
		requestID = reqHeader.GetRequestId()
	}
	if *respHeader == nil {
		s.logger.Warn(warnMsg, "request_id", requestID)
		*respHeader = &starv1.ResponseHeader{
			RequestId:       requestID,
			ServerTimestamp: timestamppb.Now(),
			Status:          starv1.Status_STATUS_OK,
		}
	}
}

// validateProtectionThresholds validates protection threshold ranges for Li-ion safety.
func (s *BatteryService) validateProtectionThresholds(t *starv1.ProtectionThresholds) error {
	if t == nil {
		return status.Error(codes.InvalidArgument, "thresholds cannot be nil")
	}

	// Overvoltage validation
	if t.OvervoltageMv < minOvervoltageMv || t.OvervoltageMv > maxOvervoltageMv {
		return status.Errorf(codes.InvalidArgument,
			"overvoltage_mv must be %d-%d mV, got %d",
			minOvervoltageMv, maxOvervoltageMv, t.OvervoltageMv)
	}

	// Undervoltage validation
	if t.UndervoltageMv < minUndervoltageMv || t.UndervoltageMv > maxUndervoltageMv {
		return status.Errorf(codes.InvalidArgument,
			"undervoltage_mv must be %d-%d mV, got %d",
			minUndervoltageMv, maxUndervoltageMv, t.UndervoltageMv)
	}

	// Ensure OV > UV
	if t.OvervoltageMv <= t.UndervoltageMv {
		return status.Errorf(codes.InvalidArgument,
			"overvoltage_mv (%d) must be greater than undervoltage_mv (%d)",
			t.OvervoltageMv, t.UndervoltageMv)
	}

	// Overcharge current validation
	if t.OverchargeMa < minOverchargeMa || t.OverchargeMa > maxOverchargeMa {
		return status.Errorf(codes.InvalidArgument,
			"overcharge_ma must be %d-%d mA, got %d",
			minOverchargeMa, maxOverchargeMa, t.OverchargeMa)
	}

	// Overdischarge current validation
	if t.OverdischargeMa < minOverdischargeMa || t.OverdischargeMa > maxOverdischargeMa {
		return status.Errorf(codes.InvalidArgument,
			"overdischarge_ma must be %d-%d mA, got %d",
			minOverdischargeMa, maxOverdischargeMa, t.OverdischargeMa)
	}

	// Overtemperature validation
	if t.OvertempDeciCelsius < minOvertempDeciC || t.OvertempDeciCelsius > maxOvertempDeciC {
		return status.Errorf(codes.InvalidArgument,
			"overtemp_deci_celsius must be %d-%d (%.1f-%.1f°C), got %d",
			minOvertempDeciC, maxOvertempDeciC,
			float64(minOvertempDeciC)/10.0, float64(maxOvertempDeciC)/10.0,
			t.OvertempDeciCelsius)
	}

	// Undertemperature validation
	if t.UndertempDeciCelsius < minUndertempDeciC || t.UndertempDeciCelsius > maxUndertempDeciC {
		return status.Errorf(codes.InvalidArgument,
			"undertemp_deci_celsius must be %d-%d (%.1f-%.1f°C), got %d",
			minUndertempDeciC, maxUndertempDeciC,
			float64(minUndertempDeciC)/10.0, float64(maxUndertempDeciC)/10.0,
			t.UndertempDeciCelsius)
	}

	// Ensure OT > UT
	if t.OvertempDeciCelsius <= t.UndertempDeciCelsius {
		return status.Errorf(codes.InvalidArgument,
			"overtemp_deci_celsius (%d) must be greater than undertemp_deci_celsius (%d)",
			t.OvertempDeciCelsius, t.UndertempDeciCelsius)
	}

	return nil
}

// validateCellMask validates cell balancing bitmask.
func (s *BatteryService) validateCellMask(mask uint32) error {
	if mask == 0 {
		return status.Error(codes.InvalidArgument, "cell_mask cannot be zero (no cells selected)")
	}

	if mask > maxCellMask {
		return status.Errorf(codes.InvalidArgument,
			"cell_mask 0x%X exceeds maximum 0x%X (16 cells max)",
			mask, maxCellMask)
	}

	return nil
}

// validateRateHz validates and normalizes the requested streaming rate.
// Valid range: MinBatteryRateHz-MaxBatteryRateHz, default DefaultBatteryRateHz.
func (s *BatteryService) validateRateHz(rateHz int32) int32 {
	if rateHz < MinBatteryRateHz || rateHz > MaxBatteryRateHz {
		s.logger.Warn("invalid battery rate, using default",
			"requested", rateHz,
			"default", DefaultBatteryRateHz,
			"min", MinBatteryRateHz,
			"max", MaxBatteryRateHz)
		return DefaultBatteryRateHz
	}
	return rateHz
}

// handleSimpleRequest sends a request via HARQ and receives the response with header validation.
func (s *BatteryService) handleSimpleRequest(
	ctx context.Context,
	req proto.Message,
	resp proto.Message,
	reqHeader *starv1.RequestHeader,
	respHeader **starv1.ResponseHeader,
	warnMsg string,
) error {
	if err := s.sendProtoMessage(ctx, req); err != nil {
		return status.Errorf(codes.Unavailable, "failed to send request: %v", err)
	}

	if err := s.receiveProtoMessage(ctx, resp); err != nil {
		return status.Errorf(codes.Unavailable, "failed to receive response: %v", err)
	}

	s.ensureResponseHeader(reqHeader, respHeader, warnMsg)
	return nil
}

// ============================================================================
// Simple Request/Response Methods (6 methods)
// ============================================================================

// GetBatteryState returns a complete snapshot of battery state.
func (s *BatteryService) GetBatteryState(ctx context.Context, req *starv1.GetBatteryStateRequest) (*starv1.GetBatteryStateResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.GetBatteryStateResponse
	if err := s.handleSimpleRequest(ctx, req, &resp, req.Header, &resp.Header, "battery state response missing header"); err != nil {
		return nil, err
	}

	return &resp, nil
}

// GetProtectionThresholds returns current OV/UV/OC/OT protection limits.
func (s *BatteryService) GetProtectionThresholds(ctx context.Context, req *starv1.GetProtectionThresholdsRequest) (*starv1.GetProtectionThresholdsResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.GetProtectionThresholdsResponse
	if err := s.handleSimpleRequest(ctx, req, &resp, req.Header, &resp.Header, "protection thresholds response missing header"); err != nil {
		return nil, err
	}

	return &resp, nil
}

// GetBalancingStatus returns current cell balancing status.
func (s *BatteryService) GetBalancingStatus(ctx context.Context, req *starv1.GetBalancingStatusRequest) (*starv1.GetBalancingStatusResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.GetBalancingStatusResponse
	if err := s.handleSimpleRequest(ctx, req, &resp, req.Header, &resp.Header, "balancing status response missing header"); err != nil {
		return nil, err
	}

	return &resp, nil
}

// GetDeviceInfo returns BMS device information (manufacturer, serial, versions).
func (s *BatteryService) GetDeviceInfo(ctx context.Context, req *starv1.GetDeviceInfoRequest) (*starv1.GetDeviceInfoResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.GetDeviceInfoResponse
	if err := s.handleSimpleRequest(ctx, req, &resp, req.Header, &resp.Header, "device info response missing header"); err != nil {
		return nil, err
	}

	return &resp, nil
}

// DisableCellBalancing disables cell balancing for all cells.
func (s *BatteryService) DisableCellBalancing(ctx context.Context, req *starv1.DisableCellBalancingRequest) (*starv1.DisableCellBalancingResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	var resp starv1.DisableCellBalancingResponse
	if err := s.handleSimpleRequest(ctx, req, &resp, req.Header, &resp.Header, "disable balancing response missing header"); err != nil {
		return nil, err
	}

	return &resp, nil
}

// ResetDevice resets the BMS device.
func (s *BatteryService) ResetDevice(ctx context.Context, req *starv1.ResetDeviceRequest) (*starv1.ResetDeviceResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Log safety-critical operation
	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}
	s.logger.Warn("BMS reset requested - safety critical operation",
		"request_id", requestID)

	// Send request via HARQ
	if err := s.sendProtoMessage(ctx, req); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to send request: %v", err)
	}

	// Receive response via HARQ
	var resp starv1.ResetDeviceResponse
	if err := s.receiveProtoMessage(ctx, &resp); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to receive response: %v", err)
	}

	// Ensure header exists
	s.ensureResponseHeader(req.Header, &resp.Header, "reset device response missing header")

	return &resp, nil
}

// ============================================================================
// Complex Methods with Validation (3 methods)
// ============================================================================

// SetProtectionThresholds configures OV/UV/OC/OT protection limits.
// This is a safety-critical operation - validates all thresholds before sending.
func (s *BatteryService) SetProtectionThresholds(ctx context.Context, req *starv1.SetProtectionThresholdsRequest) (*starv1.SetProtectionThresholdsResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Validate protection thresholds (safety-critical)
	if err := s.validateProtectionThresholds(req.Thresholds); err != nil {
		return nil, err
	}

	// Log safety-critical operation
	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}
	s.logger.Warn("protection thresholds update requested - safety critical operation",
		"request_id", requestID,
		"overvoltage_mv", req.Thresholds.OvervoltageMv,
		"undervoltage_mv", req.Thresholds.UndervoltageMv,
		"overcharge_ma", req.Thresholds.OverchargeMa,
		"overdischarge_ma", req.Thresholds.OverdischargeMa,
		"overtemp_deci_c", req.Thresholds.OvertempDeciCelsius,
		"undertemp_deci_c", req.Thresholds.UndertempDeciCelsius)

	// Send request via HARQ
	if err := s.sendProtoMessage(ctx, req); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to send request: %v", err)
	}

	// Receive response via HARQ
	var resp starv1.SetProtectionThresholdsResponse
	if err := s.receiveProtoMessage(ctx, &resp); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to receive response: %v", err)
	}

	// Ensure header exists
	s.ensureResponseHeader(req.Header, &resp.Header, "set thresholds response missing header")

	return &resp, nil
}

// EnableCellBalancing enables passive cell balancing for specified cells.
func (s *BatteryService) EnableCellBalancing(ctx context.Context, req *starv1.EnableCellBalancingRequest) (*starv1.EnableCellBalancingResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Validate cell mask
	if err := s.validateCellMask(req.CellMask); err != nil {
		return nil, err
	}

	// Log operation
	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}
	s.logger.Warn("cell balancing enable requested",
		"request_id", requestID,
		"cell_mask", fmt.Sprintf("0x%X", req.CellMask))

	// Send request via HARQ
	if err := s.sendProtoMessage(ctx, req); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to send request: %v", err)
	}

	// Receive response via HARQ
	var resp starv1.EnableCellBalancingResponse
	if err := s.receiveProtoMessage(ctx, &resp); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to receive response: %v", err)
	}

	// Ensure header exists
	s.ensureResponseHeader(req.Header, &resp.Header, "enable balancing response missing header")

	return &resp, nil
}

// ControlFets enables or disables charge and discharge FETs.
// This is a safety-critical operation - can disconnect battery from load.
func (s *BatteryService) ControlFets(ctx context.Context, req *starv1.ControlFetsRequest) (*starv1.ControlFetsResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Log safety-critical operation
	requestID := ""
	if req.Header != nil {
		requestID = req.Header.GetRequestId()
	}
	s.logger.Warn("FET control requested - safety critical operation",
		"request_id", requestID,
		"enable_charge_fet", req.EnableChargeFet,
		"enable_discharge_fet", req.EnableDischargeFet)

	// Send request via HARQ
	if err := s.sendProtoMessage(ctx, req); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to send request: %v", err)
	}

	// Receive response via HARQ
	var resp starv1.ControlFetsResponse
	if err := s.receiveProtoMessage(ctx, &resp); err != nil {
		return nil, status.Errorf(codes.Unavailable, "failed to receive response: %v", err)
	}

	// Ensure header exists
	s.ensureResponseHeader(req.Header, &resp.Header, "control fets response missing header")

	return &resp, nil
}

// ============================================================================
// Streaming Method (1 method)
// ============================================================================

// StreamBatteryState streams battery state at the requested rate (1-10 Hz).
// Uses Dispatcher to receive battery state and sends at controlled intervals.
func (s *BatteryService) StreamBatteryState(req *starv1.StreamBatteryStateRequest, stream starv1.BatteryManagementService_StreamBatteryStateServer) error {
	if req == nil {
		return status.Error(codes.InvalidArgument, "nil request")
	}

	// Validate rate
	rateHz := s.validateRateHz(req.RateHz)
	ticker := time.NewTicker(time.Second / time.Duration(rateHz))
	defer ticker.Stop()

	ctx, cancel := context.WithCancel(stream.Context())
	batteryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeBatteryData)

	holder := &batteryHolder{}
	var wg sync.WaitGroup

	// Start goroutine to receive battery state updates
	wg.Add(1)
	go func() {
		defer wg.Done()
		s.receiveStreamBatteryLoop(ctx, batteryCh, holder)
	}()

	// Main loop sends battery state at requested rate
	err := s.streamBatteryLoop(ctx, ticker, stream, holder)

	// Ensure receiver goroutine exits and channel is unsubscribed
	cancel()
	s.dispatcher.Unsubscribe(dispatcher.MessageTypeBatteryData, batteryCh)
	wg.Wait()

	return err
}

// receiveStreamBatteryLoop continuously receives battery state from Dispatcher and updates latest value.
func (s *BatteryService) receiveStreamBatteryLoop(ctx context.Context, batteryCh <-chan *starv1.WireMessage, holder *batteryHolder) {
	for {
		select {
		case <-ctx.Done():
			return
		case wireMsg, ok := <-batteryCh:
			if !ok {
				s.logger.Debug("battery subscription channel closed in stream")
				return
			}

			if wireMsg == nil {
				s.logger.Warn("received nil wire message from dispatcher in stream")
				continue
			}

			batteryState := wireMsg.GetBatteryState()
			if batteryState == nil {
				s.logger.Warn("received wire message with nil battery state in stream")
				continue
			}

			holder.Store(batteryState)
		}
	}
}

// streamBatteryLoop sends battery state data at the requested rate.
func (s *BatteryService) streamBatteryLoop(ctx context.Context, ticker *time.Ticker, stream starv1.BatteryManagementService_StreamBatteryStateServer, holder *batteryHolder) error {
	nilCount := 0

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			currentBattery := holder.Load()

			if currentBattery == nil {
				nilCount++
				if nilCount == nilWarningThreshold {
					s.logger.Warn("battery stream data stale or missing")
				}
				continue
			}

			nilCount = 0
			if err := stream.Send(currentBattery); err != nil {
				s.logger.Error("failed to send battery state", slog.String("error", err.Error()))
				return err
			}
		}
	}
}
