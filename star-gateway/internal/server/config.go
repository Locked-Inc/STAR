// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package server

import (
	"fmt"
	"net/http"
	"time"

	"google.golang.org/grpc"
)

// Default server configuration values used by DefaultHTTPConfig and
// DefaultGRPCConfig. Override per-deployment as needed.
const (
	// DefaultListenAddr is the default HTTP listen address (":8080").
	DefaultListenAddr = ":8080"
	// DefaultReadTimeout is the default HTTP read timeout (10s).
	DefaultReadTimeout = 10 * time.Second
	// DefaultWriteTimeout is the default HTTP write timeout (10s).
	DefaultWriteTimeout = 10 * time.Second
	// DefaultGRPCListenAddr is the default gRPC listen address (":50051").
	DefaultGRPCListenAddr = ":50051"
	// DefaultGRPCMaxMessageSize is the default gRPC max message size (10 MiB).
	DefaultGRPCMaxMessageSize = 10 * 1024 * 1024
)

// HTTPConfig holds the configuration for HTTP server construction.
type HTTPConfig struct {
	// ListenAddr is the TCP address to listen on (e.g., ":8080", "127.0.0.1:8080").
	ListenAddr string

	// ReadTimeout is the maximum duration for reading the entire HTTP request.
	// Zero means no timeout.
	ReadTimeout time.Duration

	// WriteTimeout is the maximum duration before timing out writes of the response.
	// Zero means no timeout.
	WriteTimeout time.Duration

	// Handler is the HTTP request handler. Typically an http.ServeMux with registered routes.
	// This field is required and enables testability (can test handler without opening ports).
	Handler http.Handler
}

// Validate checks if the HTTPConfig is valid and returns an error if validation fails.
// This matches the existing config validation pattern (see manager.Config.Validate).
func (c *HTTPConfig) Validate() error {
	if c.ListenAddr == "" {
		return fmt.Errorf("listen address is required")
	}
	if c.Handler == nil {
		return fmt.Errorf("handler is required (typically http.ServeMux)")
	}
	if c.ReadTimeout < 0 {
		return fmt.Errorf("read timeout must be >= 0")
	}
	if c.WriteTimeout < 0 {
		return fmt.Errorf("write timeout must be >= 0")
	}
	return nil
}

// DefaultHTTPConfig returns an HTTPConfig with production-ready defaults.
// Handler must be set by the caller as it's application-specific.
func DefaultHTTPConfig() *HTTPConfig {
	return &HTTPConfig{
		ListenAddr:   DefaultListenAddr,
		ReadTimeout:  DefaultReadTimeout,
		WriteTimeout: DefaultWriteTimeout,
		// Handler must be set by caller
	}
}

// GRPCConfig holds the configuration for gRPC server construction.
type GRPCConfig struct {
	// ListenAddr is the TCP address to listen on (e.g., ":50051", "127.0.0.1:50051").
	ListenAddr string

	// MaxMessageSize is the maximum message size in bytes for both send and receive.
	// Typical value: 10 MB (10 * 1024 * 1024).
	MaxMessageSize int

	// ServiceRegistrar is a callback function that registers gRPC services with the server.
	// This enables type-safe, flexible service registration using generated protobuf functions.
	//
	// Example:
	//   ServiceRegistrar: func(srv *grpc.Server) error {
	//       starv1.RegisterMotorControlServiceServer(srv, motorService)
	//       starv1.RegisterTelemetryServiceServer(srv, telemetryService)
	//       return nil
	//   }
	//
	// This field is required.
	ServiceRegistrar func(*grpc.Server) error
}

// Validate checks if the GRPCConfig is valid and returns an error if validation fails.
func (c *GRPCConfig) Validate() error {
	if c.ListenAddr == "" {
		return fmt.Errorf("listen address is required")
	}
	if c.ServiceRegistrar == nil {
		return fmt.Errorf("service registrar function is required")
	}
	if c.MaxMessageSize <= 0 {
		return fmt.Errorf("max message size must be > 0")
	}
	return nil
}

// DefaultGRPCConfig returns a GRPCConfig with production-ready defaults.
// ServiceRegistrar must be set by the caller as it's application-specific.
func DefaultGRPCConfig() *GRPCConfig {
	return &GRPCConfig{
		ListenAddr:     DefaultGRPCListenAddr,
		MaxMessageSize: DefaultGRPCMaxMessageSize,
		// ServiceRegistrar must be set by caller
	}
}
