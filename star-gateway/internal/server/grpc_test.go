package server

import (
	"context"
	"testing"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// Mock service for testing service registration
type mockMotorService struct {
	starv1.UnimplementedMotorControlServiceServer
	setCalled bool
}

func (m *mockMotorService) SetVelocity(ctx context.Context, req *starv1.SetVelocityRequest) (*starv1.SetVelocityResponse, error) {
	m.setCalled = true
	return &starv1.SetVelocityResponse{
		Header: &starv1.ResponseHeader{
			Status: starv1.Status_STATUS_OK,
		},
	}, nil
}

// Unit Tests (No Network I/O)

func TestNewGRPCServer_ConfigValidation(t *testing.T) {
	logger := newDiscardLogger()

	tests := []struct {
		name      string
		config    *GRPCConfig
		expectErr bool
	}{
		{
			name: "ValidConfig",
			config: &GRPCConfig{
				ListenAddr:     ":50051",
				MaxMessageSize: 10 * 1024 * 1024,
				ServiceRegistrar: func(srv *grpc.Server) error {
					return nil
				},
			},
			expectErr: false,
		},
		{
			name: "InvalidConfig_MissingAddr",
			config: &GRPCConfig{
				MaxMessageSize: 10 * 1024 * 1024,
				ServiceRegistrar: func(srv *grpc.Server) error {
					return nil
				},
			},
			expectErr: true,
		},
		{
			name: "InvalidConfig_MissingRegistrar",
			config: &GRPCConfig{
				ListenAddr:     ":50051",
				MaxMessageSize: 10 * 1024 * 1024,
			},
			expectErr: true,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			srv, err := NewGRPCServer(tc.config, logger)

			if tc.expectErr {
				if err == nil {
					t.Error("Expected error, got nil")
				}
				if srv != nil {
					t.Error("Expected nil server on error, got non-nil")
				}
			} else {
				if err != nil {
					t.Errorf("Expected no error, got: %v", err)
				}
				if srv == nil {
					t.Error("Expected server, got nil")
				}
			}
		})
	}
}

func TestNewGRPCServer_ServiceRegistration(t *testing.T) {
	registrarCalled := false
	mockService := &mockMotorService{}

	config := &GRPCConfig{
		ListenAddr:     ":50051",
		MaxMessageSize: 10 * 1024 * 1024,
		ServiceRegistrar: func(srv *grpc.Server) error {
			registrarCalled = true
			starv1.RegisterMotorControlServiceServer(srv, mockService)
			return nil
		},
	}

	logger := newDiscardLogger()
	srv, err := NewGRPCServer(config, logger)

	if err != nil {
		t.Fatalf("Unexpected error: %v", err)
	}
	if !registrarCalled {
		t.Error("ServiceRegistrar callback was not called")
	}
	if srv == nil {
		t.Fatal("Expected server, got nil")
	}
}

func TestNewGRPCServer_RegistrationError(t *testing.T) {
	config := &GRPCConfig{
		ListenAddr:     ":50051",
		MaxMessageSize: 10 * 1024 * 1024,
		ServiceRegistrar: func(srv *grpc.Server) error {
			// Simulate registration error
			return grpc.ErrServerStopped
		},
	}

	logger := newDiscardLogger()
	srv, err := NewGRPCServer(config, logger)

	if err == nil {
		t.Error("Expected error from ServiceRegistrar, got nil")
	}
	if srv != nil {
		t.Error("Expected nil server on registration error, got non-nil")
	}
}

// Integration Tests (Requires Network)

func TestRunGRPCServer_Lifecycle(t *testing.T) {
	mockService := &mockMotorService{}

	config := &GRPCConfig{
		ListenAddr:     "127.0.0.1:0", // Random available port
		MaxMessageSize: 10 * 1024 * 1024,
		ServiceRegistrar: func(srv *grpc.Server) error {
			starv1.RegisterMotorControlServiceServer(srv, mockService)
			return nil
		},
	}

	logger := newDiscardLogger()
	srv, err := NewGRPCServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	addr := "127.0.0.1:15051"
	errChan, err := RunGRPCServer(ctx, srv, addr, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	// Give server time to start
	time.Sleep(100 * time.Millisecond)

	// Connect gRPC client
	conn, err := grpc.NewClient(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to dial: %v", err)
	}
	defer conn.Close()

	// Invoke service to verify registration
	client := starv1.NewMotorControlServiceClient(conn)
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test",
		},
		Command: &starv1.VelocityCommand{
			FrontLeftVelocityMps:  1.0,
			FrontRightVelocityMps: 1.0,
			BackLeftVelocityMps:   1.0,
			BackRightVelocityMps:  1.0,
		},
	}

	resp, err := client.SetVelocity(context.Background(), req)
	if err != nil {
		t.Fatalf("Failed to call SetVelocity: %v", err)
	}

	if resp.Header.Status != starv1.Status_STATUS_OK {
		t.Errorf("Expected STATUS_OK, got %v", resp.Header.Status)
	}

	if !mockService.setCalled {
		t.Error("Mock service method was not called")
	}

	// Trigger graceful shutdown
	cancel()

	// Wait for server to stop (with timeout)
	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Server stopped with error: %v", err)
		}
	case <-time.After(7 * time.Second): // grpcShutdownTimeout + buffer
		t.Fatal("Server did not stop within timeout")
	}
}

func TestRunGRPCServer_InvalidPort(t *testing.T) {
	config := &GRPCConfig{
		ListenAddr:     ":50051",
		MaxMessageSize: 10 * 1024 * 1024,
		ServiceRegistrar: func(srv *grpc.Server) error {
			return nil
		},
	}

	logger := newDiscardLogger()
	srv, _ := NewGRPCServer(config, logger)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Should fail immediately (port binding error)
	_, err := RunGRPCServer(ctx, srv, "invalid:99999", logger)
	if err == nil {
		t.Error("Expected error for invalid port, got nil")
	}
}

func TestRunGRPCServer_ShutdownWithoutRequests(t *testing.T) {
	config := &GRPCConfig{
		ListenAddr:     "127.0.0.1:0",
		MaxMessageSize: 10 * 1024 * 1024,
		ServiceRegistrar: func(srv *grpc.Server) error {
			starv1.RegisterMotorControlServiceServer(srv, &mockMotorService{})
			return nil
		},
	}

	logger := newDiscardLogger()
	srv, _ := NewGRPCServer(config, logger)

	ctx, cancel := context.WithCancel(context.Background())

	addr := "127.0.0.1:15052"
	errChan, err := RunGRPCServer(ctx, srv, addr, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	// Give server time to start
	time.Sleep(100 * time.Millisecond)

	// Trigger shutdown immediately (no requests made)
	cancel()

	// Verify clean shutdown
	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Expected clean shutdown, got error: %v", err)
		}
	case <-time.After(7 * time.Second):
		t.Fatal("Shutdown timed out")
	}
}
