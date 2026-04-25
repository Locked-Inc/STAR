// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package server provides HTTP and gRPC server construction and lifecycle management.
//
// This package follows a two-phase construction pattern that separates server
// configuration from server startup:
//
//  1. Constructor phase: NewHTTPServer() / NewGRPCServer() create configured
//     servers without side effects (no network I/O, no goroutines).
//
//  2. Run phase: RunHTTPServer() / RunGRPCServer() start servers in background
//     goroutines with automatic graceful shutdown via context cancellation.
//
// # Two-Phase Construction Benefits
//
// This pattern enables:
//   - Testable handler/service wiring without opening network ports
//   - Inspecting server configuration before startup
//   - Separating config errors (constructor) from runtime errors (Run)
//   - Graceful shutdown via context cancellation (no manual shutdown calls)
//
// # HTTP Server Example
//
//	// Phase 1: Construct server with handler
//	mux := http.NewServeMux()
//	mux.HandleFunc("/health", healthHandler)
//
//	const defaultHTTPPort = ":8080"
//
//	config := &server.HTTPConfig{
//	    ListenAddr:   defaultHTTPPort,
//	    ReadTimeout:  DefaultReadTimeout,
//	    WriteTimeout: DefaultWriteTimeout,
//	    Handler:      mux,
//	}
//
//	httpSrv, err := server.NewHTTPServer(config, logger)
//	if err != nil {
//	    return fmt.Errorf("failed to create HTTP server: %w", err)
//	}
//
//	// Phase 2: Start server (non-blocking)
//	ctx, cancel := context.WithCancel(context.Background())
//	defer cancel()
//
//	errChan, err := server.RunHTTPServer(ctx, httpSrv, logger)
//	if err != nil {
//	    return fmt.Errorf("failed to start HTTP server: %w", err)
//	}
//
//	// Monitor for runtime errors
//	go func() {
//	    if err := <-errChan; err != nil {
//	        logger.Error("HTTP server error", slog.String("error", err.Error()))
//	    }
//	}()
//
//	// Graceful shutdown: cancel context
//	// (RunHTTPServer automatically shuts down when ctx is cancelled)
//	cancel()
//
// # gRPC Server Example
//
//	// Phase 1: Construct server with service registration
//	const bytesPerKiB = 1024
//	const bytesPerMiB = bytesPerKiB * 1024
//	const defaultMaxMessageMultiplier = 10
//	const defaultMaxMessageSize = defaultMaxMessageMultiplier * bytesPerMiB
//
//	config := &server.GRPCConfig{
//	    ListenAddr:     DefaultGRPCListenAddr,
//	    MaxMessageSize: defaultMaxMessageSize,
//	    ServiceRegistrar: func(srv *grpc.Server) error {
//	        starv1.RegisterMotorControlServiceServer(srv, motorService)
//	        starv1.RegisterTelemetryServiceServer(srv, telemetryService)
//	        return nil
//	    },
//	}
//
//	grpcSrv, err := server.NewGRPCServer(config, logger)
//	if err != nil {
//	    return fmt.Errorf("failed to create gRPC server: %w", err)
//	}
//
//	// Phase 2: Start server (non-blocking)
//	ctx, cancel := context.WithCancel(context.Background())
//	defer cancel()
//
//	errChan, err := server.RunGRPCServer(ctx, grpcSrv, logger)
//	if err != nil {
//	    return fmt.Errorf("failed to start gRPC server: %w", err)
//	}
//
//	// Monitor for runtime errors
//	go func() {
//	    if err := <-errChan; err != nil {
//	        logger.Error("gRPC server error", slog.String("error", err.Error()))
//	    }
//	}()
//
//	// Graceful shutdown: cancel context
//	// (RunGRPCServer automatically shuts down with 5s timeout fallback)
//	cancel()
//
// # Testing Without Network I/O
//
// The two-phase pattern enables testing handler wiring without opening ports:
//
//	// Test HTTP handler
//	config := &server.HTTPConfig{
//	    ListenAddr: ":8080",
//	    Handler:    myHandler,
//	}
//	httpSrv, _ := server.NewHTTPServer(config, testLogger)
//
//	req := httptest.NewRequest("GET", "/test", nil)
//	recorder := httptest.NewRecorder()
//	httpSrv.Handler.ServeHTTP(recorder, req)
//
//	// Assert on recorder.Code, recorder.Body
//
// # Graceful Shutdown
//
// Both RunHTTPServer and RunGRPCServer automatically handle graceful shutdown:
//
//   - HTTP: Calls Server.Shutdown() with DefaultShutdownTimeout
//   - gRPC: Calls GracefulStop() with DefaultGracefulStopTimeout, falls back to Stop()
//   - Shutdown is triggered automatically when the context passed to Run is cancelled
//   - No manual shutdown calls required
//
// STAR Project - Texas A&M University
// February 2026
package server
