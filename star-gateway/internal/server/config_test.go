package server

import (
	"net/http"
	"testing"
	"time"

	"google.golang.org/grpc"
)

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
				ListenAddr:   ":8080",
				ReadTimeout:  10 * time.Second,
				WriteTimeout: 10 * time.Second,
				Handler:      http.NotFoundHandler(),
			},
			expectErr: false,
		},
		{
			name: "ValidConfigWithZeroTimeouts",
			config: &HTTPConfig{
				ListenAddr: "127.0.0.1:8080",
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
				ListenAddr: ":8080",
			},
			expectErr: true,
			errMsg:    "handler is required (typically http.ServeMux)",
		},
		{
			name: "NegativeReadTimeout",
			config: &HTTPConfig{
				ListenAddr:  ":8080",
				ReadTimeout: -1 * time.Second,
				Handler:     http.NotFoundHandler(),
			},
			expectErr: true,
			errMsg:    "read timeout must be >= 0",
		},
		{
			name: "NegativeWriteTimeout",
			config: &HTTPConfig{
				ListenAddr:   ":8080",
				WriteTimeout: -1 * time.Second,
				Handler:      http.NotFoundHandler(),
			},
			expectErr: true,
			errMsg:    "write timeout must be >= 0",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := tc.config.Validate()

			if tc.expectErr {
				if err == nil {
					t.Errorf("Expected error containing %q, got nil", tc.errMsg)
					return
				}
				if err.Error() != tc.errMsg {
					t.Errorf("Expected error %q, got %q", tc.errMsg, err.Error())
				}
			} else {
				if err != nil {
					t.Errorf("Expected no error, got: %v", err)
				}
			}
		})
	}
}

func TestDefaultHTTPConfig(t *testing.T) {
	config := DefaultHTTPConfig()

	if config.ListenAddr != ":8080" {
		t.Errorf("Expected default ListenAddr :8080, got %s", config.ListenAddr)
	}
	if config.ReadTimeout != 10*time.Second {
		t.Errorf("Expected default ReadTimeout 10s, got %v", config.ReadTimeout)
	}
	if config.WriteTimeout != 10*time.Second {
		t.Errorf("Expected default WriteTimeout 10s, got %v", config.WriteTimeout)
	}

	// Note: Handler is nil by default (must be set by caller)
	// Set handler and validate
	config.Handler = http.NotFoundHandler()
	if err := config.Validate(); err != nil {
		t.Errorf("Default config should be valid after setting handler: %v", err)
	}
}

func TestGRPCConfig_Validate(t *testing.T) {
	mockRegistrar := func(srv *grpc.Server) error {
		return nil
	}

	tests := []struct {
		name      string
		config    *GRPCConfig
		expectErr bool
		errMsg    string
	}{
		{
			name: "ValidConfig",
			config: &GRPCConfig{
				ListenAddr:       ":50051",
				MaxMessageSize:   10 * 1024 * 1024,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: false,
		},
		{
			name: "ValidConfigWithZeroMaxSize",
			config: &GRPCConfig{
				ListenAddr:       "127.0.0.1:50051",
				MaxMessageSize:   0,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: false,
		},
		{
			name: "MissingListenAddr",
			config: &GRPCConfig{
				MaxMessageSize:   10 * 1024 * 1024,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: true,
			errMsg:    "listen address is required",
		},
		{
			name: "MissingServiceRegistrar",
			config: &GRPCConfig{
				ListenAddr:     ":50051",
				MaxMessageSize: 10 * 1024 * 1024,
			},
			expectErr: true,
			errMsg:    "service registrar function is required",
		},
		{
			name: "NegativeMaxMessageSize",
			config: &GRPCConfig{
				ListenAddr:       ":50051",
				MaxMessageSize:   -1,
				ServiceRegistrar: mockRegistrar,
			},
			expectErr: true,
			errMsg:    "max message size must be >= 0",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := tc.config.Validate()

			if tc.expectErr {
				if err == nil {
					t.Errorf("Expected error containing %q, got nil", tc.errMsg)
					return
				}
				if err.Error() != tc.errMsg {
					t.Errorf("Expected error %q, got %q", tc.errMsg, err.Error())
				}
			} else {
				if err != nil {
					t.Errorf("Expected no error, got: %v", err)
				}
			}
		})
	}
}

func TestDefaultGRPCConfig(t *testing.T) {
	config := DefaultGRPCConfig()

	if config.ListenAddr != ":50051" {
		t.Errorf("Expected default ListenAddr :50051, got %s", config.ListenAddr)
	}
	if config.MaxMessageSize != 10*1024*1024 {
		t.Errorf("Expected default MaxMessageSize 10MB, got %d", config.MaxMessageSize)
	}

	// Note: ServiceRegistrar is nil by default (must be set by caller)
	// Set registrar and validate
	config.ServiceRegistrar = func(srv *grpc.Server) error { return nil }
	if err := config.Validate(); err != nil {
		t.Errorf("Default config should be valid after setting registrar: %v", err)
	}
}
