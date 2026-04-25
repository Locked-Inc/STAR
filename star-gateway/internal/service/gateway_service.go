package service

import (
	"context"
	"log/slog"
	"sync"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/types/known/timestamppb"
)

// HubNotifier is the interface GatewayService uses to push STAREnvelope messages to the
// WebSocket hub. Defined here (not in ws) to avoid circular imports; ws.Hub satisfies it.
type HubNotifier interface {
	// Broadcast sends a STAREnvelope to the hub for distribution to connected UI clients.
	// ctx is used to detect cancellation; the call is non-blocking if the hub's
	// broadcast channel has capacity, and returns ctx.Err() if the context is
	// already cancelled. If the channel is full the message is dropped and
	// ErrBroadcastFull (from the ws package) is returned.
	Broadcast(ctx context.Context, env *starv1.STAREnvelope) error
}

// GatewayService implements the gRPC GatewayService for ROS2 <-> UI bridging.
//
// Architecture:
//
//	ROS2 (C++) <-> gRPC <-> GatewayService (Go) <-> WebSocket <-> UI (TypeScript)
//
// Data flows:
//  1. Telemetry (ROS2 -> UI):
//     ROS2 calls ForwardTelemetry() -> cached -> WebSocket streams to UI
//  2. Teleop (UI -> ROS2):
//     UI sends via WebSocket -> UpdateTeleopCommand() -> ROS2 polls GetTeleopCommand()
//  3. PID Gains (UI -> ROS2):
//     UI sends via WebSocket -> SetPidGains() -> ROS2 -> SPI -> RX72N
type GatewayService struct {
	starv1.UnimplementedGatewayServiceServer

	// Telemetry cache (ROS2 -> UI)
	telemetryMu          sync.RWMutex
	cachedSystemStatus   *starv1.SystemStatus
	cachedTelemetry      *starv1.TelemetryData
	telemetryLastUpdated time.Time

	// Teleop command cache (UI -> ROS2)
	teleopMu          sync.RWMutex
	cachedTeleop      *starv1.VelocityCommand
	teleopLastUpdated time.Time

	// Connected WebSocket clients count
	clientsMu      sync.RWMutex
	activeClients  int32
	clientCounters map[string]bool // Track unique client IDs

	// hub is the WebSocket hub used to push STAREnvelope messages to connected UI clients.
	// hubMu guards all reads and writes to hub so SetHub is safe to call before
	// server start and ForwardTelemetry is safe to call concurrently afterwards.
	hubMu sync.RWMutex
	hub   HubNotifier

	// Constants
	teleopStalenessThreshold time.Duration

	logger *slog.Logger
}

// NewGatewayService creates a new GatewayService instance.
func NewGatewayService() *GatewayService {
	return &GatewayService{
		clientCounters:           make(map[string]bool),
		teleopStalenessThreshold: 500 * time.Millisecond, // Safety timeout
		logger:                   slog.Default(),
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
	s.cachedTelemetry = req.Telemetry
	s.telemetryLastUpdated = time.Now()
	s.telemetryMu.Unlock()

	// Broadcast each non-nil payload individually so the hub's per-type throttle
	// logic fires independently for each message kind.
	s.hubMu.RLock()
	hub := s.hub
	s.hubMu.RUnlock()

	if hub != nil {
		if req.SystemStatus != nil {
			broadcastEnvelope(ctx, s.logger, "system", hub, &starv1.STAREnvelope{
				Payload: &starv1.STAREnvelope_System{System: req.SystemStatus},
			})
		}
		if req.Telemetry != nil {
			broadcastEnvelope(ctx, s.logger, "telemetry", hub, &starv1.STAREnvelope{
				Payload: &starv1.STAREnvelope_Telemetry{Telemetry: req.Telemetry},
			})
		}
		if req.MotorStatus != nil {
			broadcastEnvelope(ctx, s.logger, "motor", hub, &starv1.STAREnvelope{
				Payload: &starv1.STAREnvelope_Motors{
					Motors: &starv1.MotorStatusList{Motors: req.MotorStatus},
				},
			})
		}
		if req.Odometry != nil {
			broadcastEnvelope(ctx, s.logger, "odometry", hub, &starv1.STAREnvelope{
				Payload: &starv1.STAREnvelope_Odometry{Odometry: req.Odometry},
			})
		}
		if req.LidarScan != nil {
			broadcastEnvelope(ctx, s.logger, "lidar", hub, &starv1.STAREnvelope{
				Payload: &starv1.STAREnvelope_Lidar{Lidar: req.LidarScan},
			})
		}
	}

	// Get active client count
	s.clientsMu.RLock()
	activeClients := s.activeClients
	s.clientsMu.RUnlock()

	// Log telemetry with nil-safe accessors
	mode := "unknown"
	if req.SystemStatus != nil {
		mode = req.SystemStatus.GetMode().String()
	}

	s.logger.Info("Telemetry forwarded",
		slog.String("mode", mode),
		slog.Int("clients", int(activeClients)),
	)

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
		s.logger.Warn("Teleop watchdog triggered: no command available or stale",
			slog.Int64("age_ms", commandAgeMs),
			slog.Int64("threshold_ms", s.teleopStalenessThreshold.Milliseconds()),
		)
		cmd = &starv1.VelocityCommand{
			FrontLeftVelocityMps:  0.0,
			FrontRightVelocityMps: 0.0,
			BackLeftVelocityMps:   0.0,
			BackRightVelocityMps:  0.0,
			Sequence:              0,
			// monoMicrosSinceStart() lives in motor_control.go, same package;
			// keeps timestamp_us in our since-start epoch (not Unix wall-clock).
			TimestampUs: monoMicrosSinceStart(),
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

// SetPidGains updates PID gains for motor velocity control.
//
// Called by Gateway when UI requests PID tuning. This is a placeholder implementation
// that returns success but doesn't actually forward to ROS2 yet (will be implemented
// when ROS2 PID service is available).
func (s *GatewayService) SetPidGains(
	ctx context.Context,
	req *starv1.SetPidGainsRequest,
) (*starv1.SetPidGainsResponse, error) {
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
	s.logger.Info("SetPidGains",
		slog.Int("motor_id", int(motorID)),
		slog.Float64("kp", float64(req.PidConfig.Kp)),
		slog.Float64("ki", float64(req.PidConfig.Ki)),
		slog.Float64("kd", float64(req.PidConfig.Kd)),
	)

	// TODO: Forward to ROS2 service when available
	// For now, return success (placeholder)

	var motorDesc string
	switch motorID {
	case 0:
		motorDesc = "front left motor"
	case 1:
		motorDesc = "front right motor"
	case 2:
		motorDesc = "back left motor"
	case 3:
		motorDesc = "back right motor"
	default:
		motorDesc = "all motors"
	}

	// Build response header
	respHeader := &starv1.ResponseHeader{
		RequestId:       req.Header.GetRequestId(),
		ServerTimestamp: timestamppb.Now(),
		Status:          starv1.Status_STATUS_OK,
	}

	return &starv1.SetPidGainsResponse{
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

	s.logger.Info("Teleop updated",
		slog.Float64("front_left_mps", float64(cmd.FrontLeftVelocityMps)),
		slog.Float64("front_right_mps", float64(cmd.FrontRightVelocityMps)),
		slog.Float64("back_left_mps", float64(cmd.BackLeftVelocityMps)),
		slog.Float64("back_right_mps", float64(cmd.BackRightVelocityMps)),
	)
}

// GetLatestTelemetry returns the cached telemetry data for UI streaming.
//
// Called by WebSocket handler to stream telemetry to UI clients.
// Returns nil if no telemetry has been received yet.
func (s *GatewayService) GetLatestTelemetry() (
	*starv1.SystemStatus,
	*starv1.TelemetryData,
) {
	s.telemetryMu.RLock()
	defer s.telemetryMu.RUnlock()

	return s.cachedSystemStatus, s.cachedTelemetry
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
		s.logger.Info("Client registered", slog.String("client_id", clientID), slog.Int("total", int(s.activeClients)))
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
		s.logger.Info("Client unregistered", slog.String("client_id", clientID), slog.Int("total", int(s.activeClients)))
	}
}

// GetActiveClientCount returns the number of connected WebSocket clients.
func (s *GatewayService) GetActiveClientCount() int32 {
	s.clientsMu.RLock()
	defer s.clientsMu.RUnlock()

	return s.activeClients
}

// SetHub wires the WebSocket hub into the service so ForwardTelemetry can push
// STAREnvelope messages directly to connected UI clients.
//
// SetHub is safe to call concurrently with ForwardTelemetry and other hub
// readers because it synchronises via hubMu (Lock/RLock). It is intended to
// wire a HubNotifier and may also be used to replace s.hub at runtime.
func (s *GatewayService) SetHub(h HubNotifier) {
	s.hubMu.Lock()
	s.hub = h
	s.hubMu.Unlock()
}

// Hub returns the current HubNotifier under the read lock. Intended for use
// in tests that need to inspect the wired hub without accessing the unexported
// field directly, which would race with concurrent SetHub or ForwardTelemetry calls.
func (s *GatewayService) Hub() HubNotifier {
	s.hubMu.RLock()
	defer s.hubMu.RUnlock()
	return s.hub
}

// broadcastEnvelope sends a single STAREnvelope to hub and logs a warning on
// channel saturation. name is a short human-readable label used in log lines.
// ctx is forwarded to hub.Broadcast for cancellation propagation.
// The caller is responsible for reading s.hub under hubMu and passing a
// non-nil hub here, which avoids a redundant lock acquisition in the hot path.
func broadcastEnvelope(
	ctx context.Context,
	logger *slog.Logger,
	name string,
	hub HubNotifier,
	env *starv1.STAREnvelope,
) {
	if err := hub.Broadcast(ctx, env); err != nil {
		logger.Warn("ForwardTelemetry: broadcast dropped",
			slog.String("type", name),
			slog.Any("error", err),
		)
	}
}
