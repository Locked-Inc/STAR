package service

import (
	"context"
	"testing"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

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
			BatteryState: &starv1.BatteryState{},
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

func TestSetPIDGains(t *testing.T) {
	svc := NewGatewayService()
	ctx := context.Background()

	t.Run("Success", func(t *testing.T) {
		req := &starv1.SetPIDGainsRequest{
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

		resp, err := svc.SetPIDGains(ctx, req)
		if err != nil {
			t.Fatalf("Expected no error, got %v", err)
		}

		if !resp.Success {
			t.Error("Expected success to be true")
		}
	})

	t.Run("NilRequest", func(t *testing.T) {
		_, err := svc.SetPIDGains(ctx, nil)
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
		req := &starv1.SetPIDGainsRequest{
			Header: &starv1.RequestHeader{
				RequestId: "test-pid-nil",
			},
			MotorId:   0,
			PidConfig: nil,
		}

		_, err := svc.SetPIDGains(ctx, req)
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
		req := &starv1.SetPIDGainsRequest{
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

		_, err := svc.SetPIDGains(ctx, req)
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
	status, battery, telemetry := svc.GetLatestTelemetry()
	if status != nil || battery != nil || telemetry != nil {
		t.Error("Expected all nil telemetry initially")
	}

	// Set telemetry
	svc.telemetryMu.Lock()
	svc.cachedSystemStatus = &starv1.SystemStatus{}
	svc.cachedBatteryState = &starv1.BatteryState{}
	svc.cachedTelemetry = &starv1.TelemetryData{}
	svc.telemetryMu.Unlock()

	// Verify retrieval
	status, battery, telemetry = svc.GetLatestTelemetry()
	if status == nil || battery == nil || telemetry == nil {
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
