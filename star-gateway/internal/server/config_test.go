// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package server

import (
	"net/http"
	"testing"
	"time"

	"google.golang.org/grpc"
)

const (
	defaultListenAddr   = ":8080"
	loopbackListenAddr  = "127.0.0.1:8080"
	defaultReadTimeout  = 10 * time.Second
	defaultWriteTimeout = 10 * time.Second
	negativeTimeout     = -1 * time.Second
)

func mockRegistrar(srv *grpc.Server) error {
	return nil
}

func checkValidationError(t *testing.T, err error, expectErr bool, expectedMsg string) {
	t.Helper()
	if expectErr {
		if err == nil {
			t.Errorf("Expected error containing %q, got nil", expectedMsg)
			return
		}
		if err.Error() != expectedMsg {
			t.Errorf("Expected error %q, got %q", expectedMsg, err.Error())
		}
		return
	}

	if err != nil {
		t.Errorf("Expected no error, got: %v", err)
	}
}

func TestHTTPConfig_Validate(t *testing.T) {
	tests := []struct {
		name      string
		config    *HTTPConfig
		expectErr bool
		errMsg    string
	}{
		{
			name: "ValidConfig",
			config: &HTTPConfig{
				ListenAddr:   defaultListenAddr,
				ReadTimeout:  defaultReadTimeout,
				WriteTimeout: defaultWriteTimeout,
				Handler:      http.NotFoundHandler(),
			},
			expectErr: false,
		},
		{
			name: "ValidConfigWithZeroTimeouts",
			config: &HTTPConfig{
				ListenAddr: loopbackListenAddr,
				Handler:    http.NotFoundHandler(),
			},
			expectErr: false,
		},
		{
			name: "MissingListenAddr",
			config: &HTTPConfig{
				Handler: http.NotFoundHandler(),
			},
			expectErr: true,
			errMsg:    "listen address is required",
		},
		{
			name: "MissingHandler",
			config: &HTTPConfig{
				ListenAddr: defaultListenAddr,
			},
			expectErr: true,
			errMsg:    "handler is required (typically http.ServeMux)",
		},
		{
			name: "NegativeReadTimeout",
			config: &HTTPConfig{
				ListenAddr:  defaultListenAddr,
				ReadTimeout: negativeTimeout,
				Handler:     http.NotFoundHandler(),
			},
			expectErr: true,
			errMsg:    "read timeout must be >= 0",
		},
		{
			name: "NegativeWriteTimeout",
			config: &HTTPConfig{
				ListenAddr:   defaultListenAddr,
				WriteTimeout: negativeTimeout,
				Handler:      http.NotFoundHandler(),
			},
			expectErr: true,
			errMsg:    "write timeout must be >= 0",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := tc.config.Validate()
			checkValidationError(t, err, tc.expectErr, tc.errMsg)
		})
	}
}

func TestDefaultHTTPConfig(t *testing.T) {
	config := DefaultHTTPConfig()

	tests := []struct {
		name string
		got  any
		want any
	}{
		{name: "ListenAddr", got: config.ListenAddr, want: DefaultListenAddr},
		{name: "ReadTimeout", got: config.ReadTimeout, want: DefaultReadTimeout},
		{name: "WriteTimeout", got: config.WriteTimeout, want: DefaultWriteTimeout},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.want {
				t.Errorf("%s = %v, want %v", tc.name, tc.got, tc.want)
			}
		})
	}

	config.Handler = http.NotFoundHandler()
	if err := config.Validate(); err != nil {
		t.Errorf("Default config should be valid after setting handler: %v", err)
	}
}

func TestGRPCConfig_Validate(t *testing.T) {
	tests := []struct {
		name      string
		config    *GRPCConfig
		expectErr bool
		errMsg    string
	}{
		{
			name: "ValidConfig",
			config: &GRPCConfig{
				ListenAddr:       DefaultGRPCListenAddr,
				MaxMessageSize:   DefaultGRPCMaxMessageSize,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: false,
		},
		{
			name: "ZeroMaxMessageSize",
			config: &GRPCConfig{
				ListenAddr:       "127.0.0.1:50051",
				MaxMessageSize:   0,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: true,
			errMsg:    "max message size must be > 0",
		},
		{
			name: "MissingListenAddr",
			config: &GRPCConfig{
				MaxMessageSize:   DefaultGRPCMaxMessageSize,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: true,
			errMsg:    "listen address is required",
		},
		{
			name: "MissingServiceRegistrar",
			config: &GRPCConfig{
				ListenAddr:     DefaultGRPCListenAddr,
				MaxMessageSize: DefaultGRPCMaxMessageSize,
			},
			expectErr: true,
			errMsg:    "service registrar function is required",
		},
		{
			name: "NegativeMaxMessageSize",
			config: &GRPCConfig{
				ListenAddr:       DefaultGRPCListenAddr,
				MaxMessageSize:   -1,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: true,
			errMsg:    "max message size must be > 0",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := tc.config.Validate()
			checkValidationError(t, err, tc.expectErr, tc.errMsg)
		})
	}
}

func TestDefaultGRPCConfig(t *testing.T) {
	config := DefaultGRPCConfig()

	tests := []struct {
		name string
		got  any
		want any
	}{
		{name: "ListenAddr", got: config.ListenAddr, want: DefaultGRPCListenAddr},
		{name: "MaxMessageSize", got: config.MaxMessageSize, want: DefaultGRPCMaxMessageSize},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.want {
				t.Errorf("%s = %v, want %v", tc.name, tc.got, tc.want)
			}
		})
	}

	config.ServiceRegistrar = mockRegistrar
	if err := config.Validate(); err != nil {
		t.Errorf("Default config should be valid after setting registrar: %v", err)
	}
}
