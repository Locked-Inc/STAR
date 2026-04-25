package service

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"log/slog"
	"sync"
	"testing"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// mockHub is a thread-safe test double for HubNotifier.
type mockHub struct {
	mu         sync.Mutex
	broadcasts []*starv1.STAREnvelope
	err        error // returned from every Broadcast call when non-nil
}

func (m *mockHub) Broadcast(ctx context.Context, env *starv1.STAREnvelope) error {
	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	m.broadcasts = append(m.broadcasts, env)
	return m.err
}

func (m *mockHub) count() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return len(m.broadcasts)
}

func (m *mockHub) at(i int) *starv1.STAREnvelope {
	m.mu.Lock()
	defer m.mu.Unlock()
	if i < 0 || i >= len(m.broadcasts) {
		panic(fmt.Sprintf("mockHub.at: index %d out of range [0, %d)", i, len(m.broadcasts)))
	}
	return m.broadcasts[i]
}

// TestForwardTelemetry_ConcurrentStress verifies that GatewayService handles
// simultaneous ForwardTelemetry calls from multiple goroutines without data
// races. It exercises the telemetryMu, hubMu, and hub access code paths
// concurrently to validate the locking strategy.
func TestForwardTelemetry_ConcurrentStress(t *testing.T) {
	const (
		goroutines      = 10
		callsPerRoutine = 100
	)

	svc := NewGatewayService()
	hub := &mockHub{}
	svc.SetHub(hub)

	var wg sync.WaitGroup
	wg.Add(goroutines)
	for g := range goroutines {
		go func() {
			defer wg.Done()
			for i := range callsPerRoutine {
				req := &starv1.ForwardTelemetryRequest{
					Header: &starv1.RequestHeader{
						RequestId: fmt.Sprintf("g%d-i%d", g, i),
					},
					SystemStatus: &starv1.SystemStatus{},
					Telemetry:    &starv1.TelemetryData{},
				}
				_, err := svc.ForwardTelemetry(context.Background(), req)
				if err != nil {
					t.Errorf("ForwardTelemetry(g=%d, i=%d) returned error: %v", g, i, err)
				}
			}
		}()
	}
	wg.Wait()

	// Each ForwardTelemetry call broadcasts 2 non-nil payloads
	// (SystemStatus, Telemetry), so expected count is goroutines * callsPerRoutine * 2.
	const payloadsPerCall = 2
	expected := goroutines * callsPerRoutine * payloadsPerCall
	if got := hub.count(); got != expected {
		t.Errorf("hub.count() = %d, want %d (goroutines=%d, calls=%d, payloads=%d)",
			got, expected, goroutines, callsPerRoutine, payloadsPerCall)
	}
}

func TestNewGatewayService(t *testing.T) {
	svc := NewGatewayService()
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}

	// Verify initialization
	if svc.clientCounters == nil {
		t.Error("Expected clientCounters map to be initialized")
	}

	if svc.teleopStalenessThreshold != 500*time.Millisecond {
		t.Errorf("Expected staleness threshold 500ms, got %v", svc.teleopStalenessThreshold)
	}
}

func TestForwardTelemetry(t *testing.T) {
	svc := NewGatewayService()
	ctx := context.Background()

	t.Run("Success", func(t *testing.T) {
		req := &starv1.ForwardTelemetryRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-123",
			},
			SystemStatus: &starv1.SystemStatus{},
			Telemetry:    &starv1.TelemetryData{},
		}

		resp, err := svc.ForwardTelemetry(ctx, req)
		if err != nil {
			t.Fatalf("Expected no error, got %v", err)
		}

		if resp.Header.RequestId != "test-123" {
			t.Errorf("Expected request_id 'test-123', got '%s'", resp.Header.RequestId)
		}

		if !resp.Cached {
			t.Error("Expected cached to be true")
		}

		if resp.ActiveClients != 0 {
			t.Errorf("Expected 0 active clients, got %d", resp.ActiveClients)
		}
	})

	t.Run("NilRequest", func(t *testing.T) {
		_, err := svc.ForwardTelemetry(ctx, nil)
		if err == nil {
			t.Fatal("Expected error for nil request")
		}

		st, ok := status.FromError(err)
		if !ok {
			t.Fatal("Expected gRPC status error")
		}

		if st.Code() != codes.InvalidArgument {
			t.Errorf("Expected InvalidArgument, got %v", st.Code())
		}
	})
}

func TestGetTeleopCommand(t *testing.T) {
	svc := NewGatewayService()
	ctx := context.Background()

	t.Run("NoCommand", func(t *testing.T) {
		req := &starv1.GetTeleopCommandRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-456",
			},
		}

		resp, err := svc.GetTeleopCommand(ctx, req)
		if err != nil {
			t.Fatalf("Expected no error, got %v", err)
		}

		if resp.CommandAvailable {
			t.Error("Expected command not available")
		}

		if resp.Command == nil {
			t.Fatal("Expected zero velocity command, got nil")
		}

		// Verify zero velocities
		if resp.Command.FrontLeftVelocityMps != 0.0 {
			t.Errorf("Expected zero velocity, got %.2f", resp.Command.FrontLeftVelocityMps)
		}
	})

	t.Run("WithFreshCommand", func(t *testing.T) {
		// Set a fresh command
		cmd := &starv1.VelocityCommand{
			FrontLeftVelocityMps:  1.0,
			FrontRightVelocityMps: 1.5,
			BackLeftVelocityMps:   0.8,
			BackRightVelocityMps:  1.2,
		}
		svc.UpdateTeleopCommand(cmd)

		req := &starv1.GetTeleopCommandRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-789",
			},
		}

		resp, err := svc.GetTeleopCommand(ctx, req)
		if err != nil {
			t.Fatalf("Expected no error, got %v", err)
		}

		if !resp.CommandAvailable {
			t.Error("Expected command to be available")
		}

		if resp.Command.FrontLeftVelocityMps != 1.0 {
			t.Errorf("Expected 1.0, got %.2f", resp.Command.FrontLeftVelocityMps)
		}

		if resp.CommandAgeMs >= 500 {
			t.Errorf("Expected command age < 500ms, got %dms", resp.CommandAgeMs)
		}
	})

	t.Run("WithStaleCommand", func(t *testing.T) {
		// Set a command in the past
		svc.teleopMu.Lock()
		svc.cachedTeleop = &starv1.VelocityCommand{
			FrontLeftVelocityMps: 2.0,
		}
		svc.teleopLastUpdated = time.Now().Add(-1 * time.Second) // 1 second old
		svc.teleopMu.Unlock()

		req := &starv1.GetTeleopCommandRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-stale",
			},
		}

		resp, err := svc.GetTeleopCommand(ctx, req)
		if err != nil {
			t.Fatalf("Expected no error, got %v", err)
		}

		if resp.CommandAvailable {
			t.Error("Expected command not available (stale)")
		}

		// Should return zero velocity
		if resp.Command.FrontLeftVelocityMps != 0.0 {
			t.Errorf("Expected zero velocity for stale command, got %.2f", resp.Command.FrontLeftVelocityMps)
		}
	})

	t.Run("NilRequest", func(t *testing.T) {
		_, err := svc.GetTeleopCommand(ctx, nil)
		if err == nil {
			t.Fatal("Expected error for nil request")
		}

		st, ok := status.FromError(err)
		if !ok {
			t.Fatal("Expected gRPC status error")
		}

		if st.Code() != codes.InvalidArgument {
			t.Errorf("Expected InvalidArgument, got %v", st.Code())
		}
	})
}

func TestSetPidGains(t *testing.T) {
	svc := NewGatewayService()
	ctx := context.Background()

	t.Run("Success", func(t *testing.T) {
		req := &starv1.SetPidGainsRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-pid",
			},
			MotorId: 0,
			PidConfig: &starv1.PidConfig{
				Kp: 1.0,
				Ki: 0.5,
				Kd: 0.1,
			},
		}

		resp, err := svc.SetPidGains(ctx, req)
		if err != nil {
			t.Fatalf("Expected no error, got %v", err)
		}

		if !resp.Success {
			t.Error("Expected success to be true")
		}
	})

	t.Run("NilRequest", func(t *testing.T) {
		_, err := svc.SetPidGains(ctx, nil)
		if err == nil {
			t.Fatal("Expected error for nil request")
		}

		st, ok := status.FromError(err)
		if !ok {
			t.Fatal("Expected gRPC status error")
		}

		if st.Code() != codes.InvalidArgument {
			t.Errorf("Expected InvalidArgument, got %v", st.Code())
		}
	})

	t.Run("NilPidConfig", func(t *testing.T) {
		req := &starv1.SetPidGainsRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-pid-nil",
			},
			MotorId:   0,
			PidConfig: nil,
		}

		_, err := svc.SetPidGains(ctx, req)
		if err == nil {
			t.Fatal("Expected error for nil pid_config")
		}

		st, ok := status.FromError(err)
		if !ok {
			t.Fatal("Expected gRPC status error")
		}

		if st.Code() != codes.InvalidArgument {
			t.Errorf("Expected InvalidArgument, got %v", st.Code())
		}
	})

	t.Run("InvalidMotorID", func(t *testing.T) {
		req := &starv1.SetPidGainsRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-pid-invalid",
			},
			MotorId: 5, // Invalid motor ID
			PidConfig: &starv1.PidConfig{
				Kp: 1.0,
				Ki: 0.5,
				Kd: 0.1,
			},
		}

		_, err := svc.SetPidGains(ctx, req)
		if err == nil {
			t.Fatal("Expected error for invalid motor ID")
		}

		st, ok := status.FromError(err)
		if !ok {
			t.Fatal("Expected gRPC status error")
		}

		if st.Code() != codes.InvalidArgument {
			t.Errorf("Expected InvalidArgument, got %v", st.Code())
		}
	})
}

func TestUpdateTeleopCommand(t *testing.T) {
	svc := NewGatewayService()

	cmd := &starv1.VelocityCommand{
		FrontLeftVelocityMps:  1.5,
		FrontRightVelocityMps: 2.0,
		BackLeftVelocityMps:   1.2,
		BackRightVelocityMps:  1.8,
	}

	svc.UpdateTeleopCommand(cmd)

	// Verify command was cached
	svc.teleopMu.RLock()
	cached := svc.cachedTeleop
	svc.teleopMu.RUnlock()

	if cached == nil {
		t.Fatal("Expected cached command, got nil")
	}

	if cached.FrontLeftVelocityMps != 1.5 {
		t.Errorf("Expected 1.5, got %.2f", cached.FrontLeftVelocityMps)
	}

	// Test nil command (should be no-op)
	svc.UpdateTeleopCommand(nil)
}

func TestGetLatestTelemetry(t *testing.T) {
	svc := NewGatewayService()

	// Initially should return nil
	status, telemetry := svc.GetLatestTelemetry()
	if status != nil || telemetry != nil {
		t.Error("Expected all nil telemetry initially")
	}

	// Set telemetry
	svc.telemetryMu.Lock()
	svc.cachedSystemStatus = &starv1.SystemStatus{}
	svc.cachedTelemetry = &starv1.TelemetryData{}
	svc.telemetryMu.Unlock()

	// Verify retrieval
	status, telemetry = svc.GetLatestTelemetry()
	if status == nil || telemetry == nil {
		t.Error("Expected non-nil telemetry after caching")
	}
}

func TestGetTelemetryAge(t *testing.T) {
	svc := NewGatewayService()

	// Initially should return 0
	age := svc.GetTelemetryAge()
	if age != 0 {
		t.Errorf("Expected age 0, got %v", age)
	}

	// Set telemetry timestamp
	svc.telemetryMu.Lock()
	svc.telemetryLastUpdated = time.Now()
	svc.telemetryMu.Unlock()

	// Wait a bit and check age
	time.Sleep(10 * time.Millisecond)
	age = svc.GetTelemetryAge()
	if age < 10*time.Millisecond {
		t.Errorf("Expected age >= 10ms, got %v", age)
	}
}

func TestClientRegistration(t *testing.T) {
	svc := NewGatewayService()

	// Initially no clients
	if svc.GetActiveClientCount() != 0 {
		t.Error("Expected 0 active clients initially")
	}

	// Register client
	svc.RegisterClient("client-1")
	if svc.GetActiveClientCount() != 1 {
		t.Errorf("Expected 1 active client, got %d", svc.GetActiveClientCount())
	}

	// Register another client
	svc.RegisterClient("client-2")
	if svc.GetActiveClientCount() != 2 {
		t.Errorf("Expected 2 active clients, got %d", svc.GetActiveClientCount())
	}

	// Register duplicate (should be no-op)
	svc.RegisterClient("client-1")
	if svc.GetActiveClientCount() != 2 {
		t.Errorf("Expected 2 active clients (duplicate ignored), got %d", svc.GetActiveClientCount())
	}

	// Unregister client
	svc.UnregisterClient("client-1")
	if svc.GetActiveClientCount() != 1 {
		t.Errorf("Expected 1 active client, got %d", svc.GetActiveClientCount())
	}

	// Unregister non-existent client (should be no-op)
	svc.UnregisterClient("client-999")
	if svc.GetActiveClientCount() != 1 {
		t.Errorf("Expected 1 active client (non-existent ignored), got %d", svc.GetActiveClientCount())
	}

	// Unregister last client
	svc.UnregisterClient("client-2")
	if svc.GetActiveClientCount() != 0 {
		t.Errorf("Expected 0 active clients, got %d", svc.GetActiveClientCount())
	}
}

// TestSetHub verifies that SetHub stores the notifier and ForwardTelemetry uses it.
func TestSetHub(t *testing.T) {
	svc := NewGatewayService()

	// hub must be nil before SetHub is called.
	if svc.Hub() != nil {
		t.Fatal("Expected hub to be nil before SetHub")
	}

	hub := &mockHub{}
	svc.SetHub(hub)

	if svc.Hub() != hub {
		t.Fatal("Expected hub to be set after SetHub")
	}
}

// TestForwardTelemetry_Hub_AllFields checks that ForwardTelemetry broadcasts one
// STAREnvelope for each of the five non-nil payload fields in the request.
func TestForwardTelemetry_Hub_AllFields(t *testing.T) {
	svc := NewGatewayService()
	hub := &mockHub{}
	svc.SetHub(hub)

	req := &starv1.ForwardTelemetryRequest{
		Header:       &starv1.RequestHeader{RequestId: "all-fields"},
		SystemStatus: &starv1.SystemStatus{},
		Telemetry:    &starv1.TelemetryData{},
		MotorStatus:  []*starv1.MotorStatus{{}, {}},
		Odometry:     &starv1.OdometryData{XM: 1.0, YM: 2.0},
		LidarScan:    &starv1.LidarScan{TimestampUs: 42},
	}

	if _, err := svc.ForwardTelemetry(context.Background(), req); err != nil {
		t.Fatalf("ForwardTelemetry returned unexpected error: %v", err)
	}

	// Verify each broadcast carries the expected payload type using explicit
	// type assertions instead of fragile string-based reflect comparisons.
	type payloadCheck struct {
		name  string
		match func(*starv1.STAREnvelope) bool
	}
	wantTypes := []payloadCheck{
		{"*starv1.STAREnvelope_System", func(e *starv1.STAREnvelope) bool { _, ok := e.Payload.(*starv1.STAREnvelope_System); return ok }},
		{"*starv1.STAREnvelope_Telemetry", func(e *starv1.STAREnvelope) bool { _, ok := e.Payload.(*starv1.STAREnvelope_Telemetry); return ok }},
		{"*starv1.STAREnvelope_Motors", func(e *starv1.STAREnvelope) bool { _, ok := e.Payload.(*starv1.STAREnvelope_Motors); return ok }},
		{"*starv1.STAREnvelope_Odometry", func(e *starv1.STAREnvelope) bool { _, ok := e.Payload.(*starv1.STAREnvelope_Odometry); return ok }},
		{"*starv1.STAREnvelope_Lidar", func(e *starv1.STAREnvelope) bool { _, ok := e.Payload.(*starv1.STAREnvelope_Lidar); return ok }},
	}
	gotCount := hub.count()
	if gotCount != len(wantTypes) {
		t.Errorf("Expected %d broadcasts (one per non-nil field), got %d", len(wantTypes), gotCount)
	}
	for i := 0; i < min(gotCount, len(wantTypes)); i++ {
		env := hub.at(i)
		if !wantTypes[i].match(env) {
			t.Errorf("broadcast[%d]: want %s, got %T", i, wantTypes[i].name, env.Payload)
		}
	}
}

// TestForwardTelemetry_Hub_PartialFields verifies only non-nil fields produce broadcasts.
func TestForwardTelemetry_Hub_PartialFields(t *testing.T) {
	tests := []struct {
		name           string
		req            *starv1.ForwardTelemetryRequest
		wantBroadcasts int
	}{
		{
			name: "SystemOnly",
			req: &starv1.ForwardTelemetryRequest{
				Header:       &starv1.RequestHeader{RequestId: "system-only"},
				SystemStatus: &starv1.SystemStatus{},
			},
			wantBroadcasts: 1,
		},
		{
			name: "OdometryOnly",
			req: &starv1.ForwardTelemetryRequest{
				Header:   &starv1.RequestHeader{RequestId: "odometry-only"},
				Odometry: &starv1.OdometryData{},
			},
			wantBroadcasts: 1,
		},
		{
			name: "AllNilPayloads",
			req: &starv1.ForwardTelemetryRequest{
				Header: &starv1.RequestHeader{RequestId: "empty"},
			},
			wantBroadcasts: 0,
		},
		{
			name: "MotorsOnly",
			req: &starv1.ForwardTelemetryRequest{
				Header:      &starv1.RequestHeader{RequestId: "motors-only"},
				MotorStatus: []*starv1.MotorStatus{{}},
			},
			wantBroadcasts: 1,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			svc := NewGatewayService()
			hub := &mockHub{}
			svc.SetHub(hub)

			if _, err := svc.ForwardTelemetry(context.Background(), tc.req); err != nil {
				t.Fatalf("ForwardTelemetry returned unexpected error: %v", err)
			}

			if hub.count() != tc.wantBroadcasts {
				t.Errorf("want %d broadcasts, got %d", tc.wantBroadcasts, hub.count())
			}
		})
	}
}

// TestForwardTelemetry_NilHub verifies ForwardTelemetry does not panic when no hub is set.
func TestForwardTelemetry_NilHub(t *testing.T) {
	svc := NewGatewayService() // hub is nil

	req := &starv1.ForwardTelemetryRequest{
		Header:       &starv1.RequestHeader{RequestId: "no-hub"},
		SystemStatus: &starv1.SystemStatus{},
		Telemetry:    &starv1.TelemetryData{},
		Odometry:     &starv1.OdometryData{},
		LidarScan:    &starv1.LidarScan{},
	}

	resp, err := svc.ForwardTelemetry(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error with nil hub, got %v", err)
	}
	if !resp.Cached {
		t.Error("Expected cached=true even without hub")
	}
}

// TestForwardTelemetry_HubBroadcastError verifies that hub channel saturation
// (Broadcast returning an error) does not fail the RPC -- the gateway drops the
// message and logs, but still returns a successful ForwardTelemetryResponse.
func TestForwardTelemetry_HubBroadcastError(t *testing.T) {
	svc := NewGatewayService()
	hub := &mockHub{err: errors.New("broadcast channel full")}
	svc.SetHub(hub)

	req := &starv1.ForwardTelemetryRequest{
		Header:       &starv1.RequestHeader{RequestId: "hub-error"},
		SystemStatus: &starv1.SystemStatus{},
	}

	resp, err := svc.ForwardTelemetry(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected RPC to succeed despite broadcast error, got %v", err)
	}
	if !resp.Cached {
		t.Error("Expected cached=true even when broadcast fails")
	}
	// Broadcast was still attempted for the one non-nil field.
	// expectedBroadcasts = 1 because only SystemStatus is non-nil in req.
	const expectedBroadcasts = 1
	if hub.count() != expectedBroadcasts {
		t.Errorf("Expected %d broadcast attempts, got %d", expectedBroadcasts, hub.count())
	}
}

// TestBroadcastEnvelope_Success verifies that broadcastEnvelope returns without
// logging when Broadcast succeeds (no error).
func TestBroadcastEnvelope_Success(t *testing.T) {
	hub := &mockHub{}
	env := &starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_System{System: &starv1.SystemStatus{}},
	}

	var buf bytes.Buffer
	logger := slog.New(slog.NewTextHandler(&buf, nil))

	broadcastEnvelope(context.Background(), logger, "system", hub, env)

	if buf.Len() != 0 {
		t.Errorf("expected no log output on success, got: %s", buf.String())
	}
	if hub.count() != 1 {
		t.Errorf("expected 1 broadcast, got %d", hub.count())
	}
}

// TestBroadcastEnvelope_ChannelSaturation verifies that broadcastEnvelope logs a
// warning when Broadcast returns an error (e.g. channel saturation) but does not panic.
func TestBroadcastEnvelope_ChannelSaturation(t *testing.T) {
	hub := &mockHub{err: errors.New("broadcast channel full")}
	env := &starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_System{System: &starv1.SystemStatus{}},
	}

	var buf bytes.Buffer
	logger := slog.New(slog.NewTextHandler(&buf, nil))

	broadcastEnvelope(context.Background(), logger, "system", hub, env)

	if !bytes.Contains(buf.Bytes(), []byte("broadcast dropped")) {
		t.Errorf("expected 'broadcast dropped' in log output, got: %s", buf.String())
	}
}

// TestBroadcastEnvelope_ContextCancellation verifies that broadcastEnvelope logs a
// warning when the context is already cancelled before Broadcast is called.
func TestBroadcastEnvelope_ContextCancellation(t *testing.T) {
	hub := &mockHub{}
	env := &starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Telemetry{Telemetry: &starv1.TelemetryData{}},
	}

	ctx, cancel := context.WithCancel(context.Background())
	cancel() // cancel immediately so Broadcast returns ctx.Err()

	var buf bytes.Buffer
	logger := slog.New(slog.NewTextHandler(&buf, nil))

	broadcastEnvelope(ctx, logger, "telemetry", hub, env)

	if !bytes.Contains(buf.Bytes(), []byte("broadcast dropped")) {
		t.Errorf("expected 'broadcast dropped' in log output, got: %s", buf.String())
	}
}

// TestForwardTelemetry_Motors_WrappedInList checks that repeated MotorStatus entries
// are correctly wrapped into a MotorStatusList before broadcasting.
func TestForwardTelemetry_Motors_WrappedInList(t *testing.T) {
	svc := NewGatewayService()
	hub := &mockHub{}
	svc.SetHub(hub)

	// expectedMotorCount matches the 4-motor drive configuration (2 front + 2 rear).
	const expectedMotorCount = 4
	motors := make([]*starv1.MotorStatus, expectedMotorCount)
	for i := range motors {
		motors[i] = &starv1.MotorStatus{}
	}
	req := &starv1.ForwardTelemetryRequest{
		Header:      &starv1.RequestHeader{RequestId: "motors-list"},
		MotorStatus: motors,
	}

	if _, err := svc.ForwardTelemetry(context.Background(), req); err != nil {
		t.Fatalf("Unexpected error: %v", err)
	}

	if hub.count() != 1 {
		t.Fatalf("Expected 1 broadcast for motors, got %d", hub.count())
	}

	env := hub.at(0)
	motorsPayload, ok := env.Payload.(*starv1.STAREnvelope_Motors)
	if !ok {
		t.Fatalf("Expected STAREnvelope_Motors payload, got %T", env.Payload)
	}
	if len(motorsPayload.Motors.Motors) != len(motors) {
		t.Errorf("Expected %d motors in MotorStatusList, got %d", len(motors), len(motorsPayload.Motors.Motors))
	}
}
