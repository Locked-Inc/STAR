// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package server

import (
	"context"
	"fmt"
	"log/slog"
	"net"
	"time"

	"google.golang.org/grpc"
)

// DefaultGracefulStopTimeout is the maximum time allowed for gRPC server graceful shutdown.
const DefaultGracefulStopTimeout = 5 * time.Second

// NewGRPCServer creates a configured grpc.Server and registers services.
//
// The server is configured with:
//   - MaxRecvMsgSize and MaxSendMsgSize from config.MaxMessageSize
//   - Services registered via config.ServiceRegistrar callback
//
// Returns error if:
//   - Config validation fails
//   - ServiceRegistrar callback returns error
//
// Example:
//
//	config := &GRPCConfig{
//	    ListenAddr:     ":50051",
//	    MaxMessageSize: 10 * 1024 * 1024,
//	    ServiceRegistrar: func(srv *grpc.Server) error {
//	        starv1.RegisterMotorControlServiceServer(srv, motorService)
//	        return nil
//	    },
//	}
//	grpcSrv, err := NewGRPCServer(config, logger)
func NewGRPCServer(config *GRPCConfig, logger *slog.Logger) (*grpc.Server, error) {
	if config == nil {
		return nil, fmt.Errorf("gRPC config cannot be nil")
	}
	if logger == nil {
		return nil, fmt.Errorf("logger cannot be nil")
	}
	if config.ServiceRegistrar == nil {
		return nil, fmt.Errorf("service registrar function is required")
	}

	if err := config.Validate(); err != nil {
		logger.Error("invalid gRPC config", slog.Any("error", err))
		return nil, fmt.Errorf("invalid gRPC config: %w", err)
	}

	var opts []grpc.ServerOption
	if config.MaxMessageSize > 0 {
		opts = append(opts,
			grpc.MaxRecvMsgSize(config.MaxMessageSize),
			grpc.MaxSendMsgSize(config.MaxMessageSize),
		)
	}

	srv := grpc.NewServer(opts...)

	// Register services via callback
	if err := config.ServiceRegistrar(srv); err != nil {
		logger.Error("failed to register gRPC services", slog.Any("error", err))
		return nil, fmt.Errorf("failed to register gRPC services: %w", err)
	}

	logger.Info("gRPC server created",
		slog.String("listen_addr", config.ListenAddr),
		slog.Int("max_message_size", config.MaxMessageSize))

	return srv, nil
}

// RunGRPCServer starts the gRPC server in a background goroutine and returns immediately.
//
// The function:
//  1. Creates a TCP listener on addr
//  2. Starts server in background goroutine via srv.Serve()
//  3. Spawns a shutdown watcher goroutine that triggers graceful shutdown on ctx.Done()
//
// Returns:
//   - Error channel for async error reporting (buffered, size 1)
//   - Immediate error if listener creation fails
//
// Graceful shutdown:
//   - Triggered automatically when ctx is cancelled
//   - Calls srv.GracefulStop() with grpcShutdownTimeout (5 seconds)
//   - Waits for in-flight RPCs to complete
//   - Falls back to srv.Stop() on timeout (forcefully terminates active RPCs)
//
// The error channel receives:
//   - nil on clean shutdown
//   - error if srv.Serve() fails
//
// Example:
//
//	ctx, cancel := context.WithCancel(context.Background())
//	defer cancel()
//
//	errChan, err := RunGRPCServer(ctx, grpcSrv, ":50051", logger)
//	if err != nil {
//	    return fmt.Errorf("failed to start: %w", err)
//	}
//
//	// Monitor for runtime errors
//	go func() {
//	    if err := <-errChan; err != nil {
//	        logger.Error("server error", slog.String("error", err.Error()))
//	    }
//	}()
//
//	// Trigger graceful shutdown
//	cancel()
func RunGRPCServer(ctx context.Context, srv *grpc.Server, addr string, logger *slog.Logger) (<-chan error, error) {
	if srv == nil {
		return nil, fmt.Errorf("gRPC server cannot be nil")
	}
	if logger == nil {
		return nil, fmt.Errorf("logger cannot be nil")
	}
	if addr == "" {
		return nil, fmt.Errorf("listen address cannot be empty")
	}

	// Create TCP listener first to catch port binding errors early
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return nil, fmt.Errorf("failed to listen on %s: %w", addr, err)
	}

	errChan := make(chan error, errChanBufSize)

	// Start server in background goroutine
	go func() {
		logger.Info("gRPC server starting", slog.String("addr", addr))

		if err := srv.Serve(listener); err != nil {
			logger.Error("gRPC server error", slog.Any("error", err))
			errChan <- err
			return
		}

		logger.Info("gRPC server stopped")
		errChan <- nil
	}()

	// Spawn shutdown watcher goroutine with timeout fallback
	go func() {
		<-ctx.Done()

		logger.Info("gRPC server shutting down")

		// Attempt graceful shutdown with timeout
		shutdownChan := make(chan struct{})
		go func() {
			srv.GracefulStop()
			close(shutdownChan)
		}()

		select {
		case <-shutdownChan:
			logger.Info("gRPC server gracefully stopped")
		case <-time.After(DefaultGracefulStopTimeout):
			logger.Warn("gRPC server graceful shutdown timed out, forcing stop",
				slog.Duration("timeout", DefaultGracefulStopTimeout))
			srv.Stop() // Forcefully terminate active RPCs
		}
	}()

	return errChan, nil
}
