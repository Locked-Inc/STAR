// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package server

import (
	"context"
	"fmt"
	"net"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/credentials/insecure"
)

const (
	defaultTestListenAddr     = ":50051"
	defaultTestMaxMessageSize = 10 * 1024 * 1024
	testVelocityMps           = 1.0
	testVelocityIncrementMps  = 0.01
	testServerStartupDeadline = 2 * time.Second
	testPollInterval          = 10 * time.Millisecond
	testShutdownWait          = 5 * time.Second
	testConcurrentRequests    = 16
)

// Mock service for testing service registration.
type mockMotorService struct {
	starv1.UnimplementedMotorControlServiceServer
	setCalled atomic.Bool
}

func (m *mockMotorService) SetVelocity(ctx context.Context, req *starv1.SetVelocityRequest) (*starv1.SetVelocityResponse, error) {
	m.setCalled.Store(true)
	return &starv1.SetVelocityResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
	}, nil
}

func getTestListenAddr(t *testing.T) string {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("failed to reserve test listen addr: %v", err)
	}
	addr := listener.Addr().String()
	_ = listener.Close()
	return addr
}

func waitForGRPCReady(addr string) error {
	deadline := time.Now().Add(testServerStartupDeadline)
	conn, err := grpc.NewClient(addr,
		grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return fmt.Errorf("failed to create gRPC client: %w", err)
	}
	defer conn.Close()

	conn.Connect()
	for time.Now().Before(deadline) {
		state := conn.GetState()
		if state == connectivity.Ready {
			return nil
		}
		// Check for terminal failure states
		if state == connectivity.Shutdown {
			return fmt.Errorf("gRPC connection in terminal state: %v", state)
		}
		func() {
			ctx, cancel := context.WithTimeout(context.Background(), testPollInterval)
			defer cancel()
			conn.WaitForStateChange(ctx, state)
		}()
		// Continue polling until overall deadline expires (don't exit on poll timeout)
	}
	return fmt.Errorf("gRPC server did not become ready within %v", testServerStartupDeadline)
}

func TestNewGRPCServer_ConfigValidation(t *testing.T) {
	logger := testutil.NewDiscardLogger()

	tests := []struct {
		name      string
		config    *GRPCConfig
		expectErr bool
	}{
		{
			name: "ValidConfig",
			config: &GRPCConfig{
				ListenAddr:     defaultTestListenAddr,
				MaxMessageSize: defaultTestMaxMessageSize,
				ServiceRegistrar: func(srv *grpc.Server) error {
					return nil
				},
			},
			expectErr: false,
		},
		{
			name: "InvalidConfig_MissingAddr",
			config: &GRPCConfig{
				MaxMessageSize: defaultTestMaxMessageSize,
				ServiceRegistrar: func(srv *grpc.Server) error {
					return nil
				},
			},
			expectErr: true,
		},
		{
			name: "InvalidConfig_MissingRegistrar",
			config: &GRPCConfig{
				ListenAddr:     defaultTestListenAddr,
				MaxMessageSize: defaultTestMaxMessageSize,
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
				return
			}

			if err != nil {
				t.Errorf("Expected no error, got: %v", err)
			}
			if srv == nil {
				t.Error("Expected server, got nil")
			}
		})
	}
}

func TestNewGRPCServer_ServiceRegistration(t *testing.T) {
	registrarCalled := false
	mockService := &mockMotorService{}
	config := &GRPCConfig{
		ListenAddr:     defaultTestListenAddr,
		MaxMessageSize: defaultTestMaxMessageSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			registrarCalled = true
			starv1.RegisterMotorControlServiceServer(srv, mockService)
			return nil
		},
	}

	logger := testutil.NewDiscardLogger()
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
		ListenAddr:     defaultTestListenAddr,
		MaxMessageSize: defaultTestMaxMessageSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			return grpc.ErrServerStopped
		},
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewGRPCServer(config, logger)
	if err == nil {
		t.Error("Expected error from ServiceRegistrar, got nil")
	}
	if srv != nil {
		t.Error("Expected nil server on registration error, got non-nil")
	}
}

func TestRunGRPCServer_Lifecycle(t *testing.T) {
	mockService := &mockMotorService{}
	addr := getTestListenAddr(t)
	config := &GRPCConfig{
		ListenAddr:     addr,
		MaxMessageSize: defaultTestMaxMessageSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			starv1.RegisterMotorControlServiceServer(srv, mockService)
			return nil
		},
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewGRPCServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	errChan, err := RunGRPCServer(ctx, srv, config.ListenAddr, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	if err := waitForGRPCReady(config.ListenAddr); err != nil {
		t.Fatal(err)
	}

	conn, err := grpc.NewClient(config.ListenAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to dial: %v", err)
	}
	defer conn.Close()

	client := starv1.NewMotorControlServiceClient(conn)
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{RequestId: "test"},
		Command: &starv1.VelocityCommand{
			FrontLeftVelocityMps:  testVelocityMps,
			FrontRightVelocityMps: testVelocityMps,
			BackLeftVelocityMps:   testVelocityMps,
			BackRightVelocityMps:  testVelocityMps,
		},
	}

	resp, err := client.SetVelocity(context.Background(), req)
	if err != nil {
		t.Fatalf("Failed to call SetVelocity: %v", err)
	}
	if resp.Header.Status != starv1.Status_STATUS_OK {
		t.Errorf("Expected STATUS_OK, got %v", resp.Header.Status)
	}
	if !mockService.setCalled.Load() {
		t.Error("Mock service method was not called")
	}

	cancel()

	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Server stopped with error: %v", err)
		}
	case <-time.After(testShutdownWait):
		t.Fatal("Server did not stop within timeout")
	}
}

func TestSetVelocityConcurrent(t *testing.T) {
	mockService := &mockMotorService{}
	addr := getTestListenAddr(t)
	config := &GRPCConfig{
		ListenAddr:     addr,
		MaxMessageSize: defaultTestMaxMessageSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			starv1.RegisterMotorControlServiceServer(srv, mockService)
			return nil
		},
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewGRPCServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	errChan, err := RunGRPCServer(ctx, srv, config.ListenAddr, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}
	defer func() {
		cancel()
		select {
		case err := <-errChan:
			if err != nil {
				t.Logf("server shutdown error from errChan: %v", err)
			}
		case <-time.After(testShutdownWait):
			t.Log("server shutdown did not complete within timeout")
		}
	}()

	if err := waitForGRPCReady(config.ListenAddr); err != nil {
		t.Fatal(err)
	}

	conn, err := grpc.NewClient(config.ListenAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to dial: %v", err)
	}
	defer conn.Close()
	client := starv1.NewMotorControlServiceClient(conn)

	var wg sync.WaitGroup
	errCh := make(chan error, testConcurrentRequests)

	for i := 0; i < testConcurrentRequests; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			velocity := testVelocityMps + float64(i)*testVelocityIncrementMps
			req := &starv1.SetVelocityRequest{
				Header: &starv1.RequestHeader{RequestId: fmt.Sprintf("req-%d", i)},
				Command: &starv1.VelocityCommand{
					FrontLeftVelocityMps:  velocity,
					FrontRightVelocityMps: velocity,
					BackLeftVelocityMps:   velocity,
					BackRightVelocityMps:  velocity,
				},
			}
			resp, err := client.SetVelocity(context.Background(), req)
			if err != nil {
				errCh <- err
				return
			}
			if resp == nil || resp.Header == nil || resp.Header.Status != starv1.Status_STATUS_OK {
				errCh <- fmt.Errorf("unexpected response for request %d", i)
			}
		}(i)
	}

	wg.Wait()
	close(errCh)
	for err := range errCh {
		t.Errorf("concurrent SetVelocity failed: %v", err)
	}

	if !mockService.setCalled.Load() {
		t.Error("expected at least one SetVelocity invocation")
	}
}

func TestRunGRPCServer_InvalidPort(t *testing.T) {
	config := &GRPCConfig{
		ListenAddr:     defaultTestListenAddr,
		MaxMessageSize: defaultTestMaxMessageSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			return nil
		},
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewGRPCServer(config, logger)
	if err != nil {
		t.Fatalf("NewGRPCServer failed: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	_, err = RunGRPCServer(ctx, srv, "invalid:99999", logger)
	if err == nil {
		t.Error("Expected error for invalid port, got nil")
	}
}

func TestRunGRPCServer_ShutdownWithoutRequests(t *testing.T) {
	addr := getTestListenAddr(t)
	config := &GRPCConfig{
		ListenAddr:     addr,
		MaxMessageSize: defaultTestMaxMessageSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			starv1.RegisterMotorControlServiceServer(srv, &mockMotorService{})
			return nil
		},
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewGRPCServer(config, logger)
	if err != nil {
		t.Fatalf("NewGRPCServer failed: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())

	errChan, err := RunGRPCServer(ctx, srv, config.ListenAddr, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	if err := waitForGRPCReady(config.ListenAddr); err != nil {
		t.Fatal(err)
	}

	cancel()

	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Expected clean shutdown, got error: %v", err)
		}
	case <-time.After(testShutdownWait):
		t.Fatal("Shutdown timed out")
	}
}
