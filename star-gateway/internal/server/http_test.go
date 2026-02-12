package server

import (
	"context"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

// newDiscardLogger creates a logger that discards all output (for testing).
func newDiscardLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}

// Unit Tests (No Network I/O)

func TestNewHTTPServer_ConfigValidation(t *testing.T) {
	logger := newDiscardLogger()

	tests := []struct {
		name      string
		config    *HTTPConfig
		expectErr bool
	}{
		{
			name: "ValidConfig",
			config: &HTTPConfig{
				ListenAddr: ":8080",
				Handler:    http.NotFoundHandler(),
			},
			expectErr: false,
		},
		{
			name: "InvalidConfig_MissingAddr",
			config: &HTTPConfig{
				Handler: http.NotFoundHandler(),
			},
			expectErr: true,
		},
		{
			name: "InvalidConfig_MissingHandler",
			config: &HTTPConfig{
				ListenAddr: ":8080",
			},
			expectErr: true,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			srv, err := NewHTTPServer(tc.config, logger)

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

func TestNewHTTPServer_HandlerWiring(t *testing.T) {
	handlerCalled := false
	expectedPath := "/test"

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		handlerCalled = true

		if r.URL.Path != expectedPath {
			t.Errorf("Expected path %s, got %s", expectedPath, r.URL.Path)
		}

		w.WriteHeader(http.StatusOK)
		w.Write([]byte("test response"))
	})

	config := &HTTPConfig{
		ListenAddr:   ":8080",
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		Handler:      handler,
	}

	logger := newDiscardLogger()
	srv, err := NewHTTPServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	// Test handler WITHOUT starting server (key benefit of two-phase construction)
	req := httptest.NewRequest("GET", expectedPath, nil)
	recorder := httptest.NewRecorder()

	srv.Handler.ServeHTTP(recorder, req)

	if !handlerCalled {
		t.Error("Handler was not called")
	}

	if recorder.Code != http.StatusOK {
		t.Errorf("Expected status 200, got %d", recorder.Code)
	}

	body := recorder.Body.String()
	if body != "test response" {
		t.Errorf("Expected body %q, got %q", "test response", body)
	}
}

func TestNewHTTPServer_Configuration(t *testing.T) {
	config := &HTTPConfig{
		ListenAddr:   "127.0.0.1:9999",
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 7 * time.Second,
		Handler:      http.NotFoundHandler(),
	}

	logger := newDiscardLogger()
	srv, err := NewHTTPServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	if srv.Addr != config.ListenAddr {
		t.Errorf("Expected Addr %s, got %s", config.ListenAddr, srv.Addr)
	}
	if srv.ReadTimeout != config.ReadTimeout {
		t.Errorf("Expected ReadTimeout %v, got %v", config.ReadTimeout, srv.ReadTimeout)
	}
	if srv.WriteTimeout != config.WriteTimeout {
		t.Errorf("Expected WriteTimeout %v, got %v", config.WriteTimeout, srv.WriteTimeout)
	}
	if srv.Handler == nil {
		t.Error("Expected Handler to be set, got nil")
	}
}

// Integration Tests (Requires Network)

func TestRunHTTPServer_Lifecycle(t *testing.T) {
	requestReceived := false

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		requestReceived = true
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("ok"))
	})

	config := &HTTPConfig{
		ListenAddr:   "127.0.0.1:0", // Random available port
		ReadTimeout:  1 * time.Second,
		WriteTimeout: 1 * time.Second,
		Handler:      handler,
	}

	logger := newDiscardLogger()
	srv, err := NewHTTPServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	errChan, err := RunHTTPServer(ctx, srv, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	// Give server time to start
	time.Sleep(50 * time.Millisecond)

	// Make HTTP request to verify server is listening
	// Note: srv.Addr is updated by RunHTTPServer to the actual bound address
	resp, err := http.Get("http://" + srv.Addr)
	if err != nil {
		t.Fatalf("Failed to connect to server: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Errorf("Expected status 200, got %d", resp.StatusCode)
	}

	body, _ := io.ReadAll(resp.Body)
	if string(body) != "ok" {
		t.Errorf("Expected body %q, got %q", "ok", string(body))
	}

	if !requestReceived {
		t.Error("Handler was not called")
	}

	// Trigger graceful shutdown
	cancel()

	// Wait for server to stop (with timeout)
	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Server stopped with error: %v", err)
		}
	case <-time.After(7 * time.Second): // httpShutdownTimeout + buffer
		t.Fatal("Server did not stop within timeout")
	}
}

func TestRunHTTPServer_InvalidPort(t *testing.T) {
	config := &HTTPConfig{
		ListenAddr: "invalid:99999", // Invalid port
		Handler:    http.NotFoundHandler(),
	}

	logger := newDiscardLogger()
	srv, _ := NewHTTPServer(config, logger)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Should fail immediately (port binding error)
	_, err := RunHTTPServer(ctx, srv, logger)
	if err == nil {
		t.Error("Expected error for invalid port, got nil")
	}
}

func TestRunHTTPServer_ShutdownWithoutRequests(t *testing.T) {
	config := &HTTPConfig{
		ListenAddr: "127.0.0.1:0",
		Handler:    http.NotFoundHandler(),
	}

	logger := newDiscardLogger()
	srv, _ := NewHTTPServer(config, logger)

	ctx, cancel := context.WithCancel(context.Background())

	errChan, err := RunHTTPServer(ctx, srv, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	// Give server time to start
	time.Sleep(50 * time.Millisecond)

	// Trigger shutdown immediately (no requests made)
	cancel()

	// Verify clean shutdown
	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Expected clean shutdown, got error: %v", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("Shutdown timed out")
	}
}
