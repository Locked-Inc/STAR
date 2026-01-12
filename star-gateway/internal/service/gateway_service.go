package service

import (
	"context"
	"log"
	"sync"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/types/known/timestamppb"
)

// GatewayService implements the gRPC GatewayService for ROS2 ↔ UI bridging.
//
// Architecture:
//
//	ROS2 (C++) ↔ gRPC ↔ GatewayService (Go) ↔ WebSocket ↔ UI (TypeScript)
//
// Data flows:
//  1. Telemetry (ROS2 → UI):
//     ROS2 calls ForwardTelemetry() → cached → WebSocket streams to UI
//  2. Teleop (UI → ROS2):
//     UI sends via WebSocket → UpdateTeleopCommand() → ROS2 polls GetTeleopCommand()
//  3. PID Gains (UI → ROS2):
//     UI sends via WebSocket → SetPIDGains() → ROS2 → SPI → RX72N
type GatewayService struct {
	starv1.UnimplementedGatewayServiceServer

	// Telemetry cache (ROS2 → UI)
	telemetryMu          sync.RWMutex
	cachedSystemStatus   *starv1.SystemStatus
	cachedBatteryState   *starv1.BatteryState
	cachedTelemetry      *starv1.TelemetryData
	telemetryLastUpdated time.Time

	// Teleop command cache (UI → ROS2)
	teleopMu          sync.RWMutex
	cachedTeleop      *starv1.VelocityCommand
	teleopLastUpdated time.Time

	// Connected WebSocket clients count
	clientsMu      sync.RWMutex
	activeClients  int32
	clientCounters map[string]bool // Track unique client IDs

	// Constants
	teleopStalenessThreshold time.Duration
}

// NewGatewayService creates a new GatewayService instance.
func NewGatewayService() *GatewayService {
	return &GatewayService{
		clientCounters:           make(map[string]bool),
		teleopStalenessThreshold: 500 * time.Millisecond, // Safety timeout
	}
}

// ============================================================================
// gRPC Service Implementation
// ============================================================================

// ForwardTelemetry receives telemetry data from ROS2 and caches it for UI streaming.
//
// Called by ROS2 bridge node at 10 Hz. This is a non-blocking operation that
// simply caches the telemetry data for WebSocket handlers to retrieve.
func (s *GatewayService) ForwardTelemetry(
	ctx context.Context,
	req *starv1.ForwardTelemetryRequest,
) (*starv1.ForwardTelemetryResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Cache telemetry data with write lock
	s.telemetryMu.Lock()
	s.cachedSystemStatus = req.SystemStatus
	s.cachedBatteryState = req.BatteryState
	s.cachedTelemetry = req.Telemetry
	s.telemetryLastUpdated = time.Now()
	s.telemetryMu.Unlock()

	// Get active client count
	s.clientsMu.RLock()
	activeClients := s.activeClients
	s.clientsMu.RUnlock()

	// Log telemetry with nil-safe accessors
	mode := "unknown"
	if req.SystemStatus != nil {
		mode = req.SystemStatus.GetMode().String()
	}

	batteryPercent := 0.0
	if req.BatteryState != nil && req.BatteryState.GetSoc() != nil {
		batteryPercent = float64(req.BatteryState.GetSoc().GetRelativeSocPercent())
	}

	log.Printf("Telemetry forwarded: mode=%s, battery=%.1f%%, clients=%d",
		mode, batteryPercent, activeClients)

	// Build response header
	respHeader := &starv1.ResponseHeader{
		RequestId:       req.Header.GetRequestId(),
		ServerTimestamp: timestamppb.Now(),
		Status:          starv1.Status_STATUS_OK,
	}

	return &starv1.ForwardTelemetryResponse{
		Header:        respHeader,
		Cached:        true,
		ActiveClients: activeClients,
	}, nil
}

// GetTeleopCommand returns the latest teleop command for ROS2 to execute.
//
// Called by ROS2 bridge node at 50 Hz. Returns cached command with age in ms.
// ROS2 is responsible for checking staleness (rejects commands > 500ms old).
func (s *GatewayService) GetTeleopCommand(
	ctx context.Context,
	req *starv1.GetTeleopCommandRequest,
) (*starv1.GetTeleopCommandResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	// Read cached teleop command with read lock
	s.teleopMu.RLock()
	cachedCmd := s.cachedTeleop
	lastUpdated := s.teleopLastUpdated
	s.teleopMu.RUnlock()

	// Calculate command age
	var commandAgeMs int64
	var commandAvailable bool

	if cachedCmd != nil && !lastUpdated.IsZero() {
		commandAgeMs = time.Since(lastUpdated).Milliseconds()
		commandAvailable = commandAgeMs < s.teleopStalenessThreshold.Milliseconds()
	}

	// Return zero velocity if no command or stale
	var cmd *starv1.VelocityCommand
	if cachedCmd != nil && commandAvailable {
		cmd = cachedCmd
	} else {
		// Return zero velocity (safe default)
		cmd = &starv1.VelocityCommand{
			Motor_0VelocityMps: 0.0,
			Motor_1VelocityMps: 0.0,
			Sequence:           0,
			TimestampUs:        time.Now().UnixMicro(),
		}
	}

	// Build response header
	respHeader := &starv1.ResponseHeader{
		RequestId:       req.Header.GetRequestId(),
		ServerTimestamp: timestamppb.Now(),
		Status:          starv1.Status_STATUS_OK,
	}

	return &starv1.GetTeleopCommandResponse{
		Header:           respHeader,
		Command:          cmd,
		CommandAvailable: commandAvailable,
		CommandAgeMs:     commandAgeMs,
	}, nil
}

// SetPIDGains updates PID gains for motor velocity control.
//
// Called by Gateway when UI requests PID tuning. This is a placeholder implementation
// that returns success but doesn't actually forward to ROS2 yet (will be implemented
// when ROS2 PID service is available).
func (s *GatewayService) SetPIDGains(
	ctx context.Context,
	req *starv1.SetPIDGainsRequest,
) (*starv1.SetPIDGainsResponse, error) {
	if req == nil {
		return nil, status.Error(codes.InvalidArgument, "request cannot be nil")
	}

	if req.PidConfig == nil {
		return nil, status.Error(codes.InvalidArgument, "pid_config cannot be nil")
	}

	// Validate motor ID (-1 = both, 0 = left, 1 = right)
	motorID := req.MotorId
	if motorID < -1 || motorID > 1 {
		return nil, status.Errorf(codes.InvalidArgument,
			"motor_id must be -1 (both), 0 (left), or 1 (right), got: %d", motorID)
	}

	// Log the request
	log.Printf("SetPIDGains: motor=%d, kp=%.3f, ki=%.3f, kd=%.3f",
		motorID, req.PidConfig.Kp, req.PidConfig.Ki, req.PidConfig.Kd)

	// TODO: Forward to ROS2 service when available
	// For now, return success (placeholder)

	motorDesc := "both motors"
	if motorID == 0 {
		motorDesc = "left motor"
	} else if motorID == 1 {
		motorDesc = "right motor"
	}

	// Build response header
	respHeader := &starv1.ResponseHeader{
		RequestId:       req.Header.GetRequestId(),
		ServerTimestamp: timestamppb.Now(),
		Status:          starv1.Status_STATUS_OK,
	}

	return &starv1.SetPIDGainsResponse{
		Header:  respHeader,
		Success: true,
		Message: "PID gains update for " + motorDesc + " (placeholder - ROS2 integration pending)",
	}, nil
}

// ============================================================================
// Public Methods for WebSocket Handler Integration
// ============================================================================

// UpdateTeleopCommand updates the cached teleop command from UI.
//
// Called by WebSocket handler when UI sends a new controller input.
// This command will be polled by ROS2 via GetTeleopCommand().
func (s *GatewayService) UpdateTeleopCommand(cmd *starv1.VelocityCommand) {
	if cmd == nil {
		return
	}

	s.teleopMu.Lock()
	s.cachedTeleop = cmd
	s.teleopLastUpdated = time.Now()
	s.teleopMu.Unlock()

	log.Printf("Teleop updated: left=%.2fm/s, right=%.2fm/s",
		cmd.Motor_0VelocityMps, cmd.Motor_1VelocityMps)
}

// GetLatestTelemetry returns the cached telemetry data for UI streaming.
//
// Called by WebSocket handler to stream telemetry to UI clients.
// Returns nil if no telemetry has been received yet.
func (s *GatewayService) GetLatestTelemetry() (
	*starv1.SystemStatus,
	*starv1.BatteryState,
	*starv1.TelemetryData,
) {
	s.telemetryMu.RLock()
	defer s.telemetryMu.RUnlock()

	return s.cachedSystemStatus, s.cachedBatteryState, s.cachedTelemetry
}

// GetTelemetryAge returns how old the cached telemetry is.
//
// Useful for UI to display data freshness indicator.
func (s *GatewayService) GetTelemetryAge() time.Duration {
	s.telemetryMu.RLock()
	defer s.telemetryMu.RUnlock()

	if s.telemetryLastUpdated.IsZero() {
		return 0
	}

	return time.Since(s.telemetryLastUpdated)
}

// RegisterClient increments the active WebSocket client count.
//
// Called when a new WebSocket client connects.
func (s *GatewayService) RegisterClient(clientID string) {
	s.clientsMu.Lock()
	defer s.clientsMu.Unlock()

	if !s.clientCounters[clientID] {
		s.clientCounters[clientID] = true
		s.activeClients++
		log.Printf("Client registered: %s (total: %d)", clientID, s.activeClients)
	}
}

// UnregisterClient decrements the active WebSocket client count.
//
// Called when a WebSocket client disconnects.
func (s *GatewayService) UnregisterClient(clientID string) {
	s.clientsMu.Lock()
	defer s.clientsMu.Unlock()

	if s.clientCounters[clientID] {
		delete(s.clientCounters, clientID)
		s.activeClients--
		log.Printf("Client unregistered: %s (total: %d)", clientID, s.activeClients)
	}
}

// GetActiveClientCount returns the number of connected WebSocket clients.
func (s *GatewayService) GetActiveClientCount() int32 {
	s.clientsMu.RLock()
	defer s.clientsMu.RUnlock()

	return s.activeClients
}
