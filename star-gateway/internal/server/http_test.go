// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package server

import (
	"context"
	"io"
	"net/http"
	"net/http/httptest"
	"sync/atomic"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
)

const (
	testReadWriteTimeout        = 10 * time.Second
	configReadTimeout           = 5 * time.Second
	configWriteTimeout          = 7 * time.Second
	lifecycleReadWriteTimeout   = 1 * time.Second // timeout for test server read/write ops
	testServerStartupWait       = 50 * time.Millisecond
	lifecycleShutdownTimeout    = 7 * time.Second // shutdown context deadline including buffer
	shutdownWaitWithoutRequests = 2 * time.Second

	// testHTTPListenAddr is used wherever a specific placeholder listen address is needed for config validation.
	testHTTPListenAddr = ":8080"

	// testDynamicListenAddr lets the OS pick a free port for real server lifecycle tests.
	testDynamicListenAddr = "127.0.0.1:0"

	// testHTTPRequestTimeout is the maximum duration for HTTP requests in tests.
	testHTTPRequestTimeout = 100 * time.Millisecond
)

func TestNewHTTPServer_ConfigValidation(t *testing.T) {
	logger := testutil.NewDiscardLogger()

	tests := []struct {
		name      string
		config    *HTTPConfig
		expectErr bool
	}{
		{
			name: "ValidConfig",
			config: &HTTPConfig{
				ListenAddr: testHTTPListenAddr,
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
				ListenAddr: testHTTPListenAddr,
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

func TestNewHTTPServer_HandlerWiring(t *testing.T) {
	tests := []struct {
		name                string
		requestPath         string
		expectedStatus      int
		expectedBody        string
		expectHandlerCalled bool
	}{
		{
			name:                "HandlerCalledForMatchingPath",
			requestPath:         "/test",
			expectedStatus:      http.StatusOK,
			expectedBody:        "test response",
			expectHandlerCalled: true,
		},
		{
			name:                "NotFoundForNonMatchingPath",
			requestPath:         "/not-found",
			expectedStatus:      http.StatusNotFound,
			expectedBody:        "",
			expectHandlerCalled: false,
		},
		{
			name:                "NotFoundForNestedPath",
			requestPath:         "/test/nested",
			expectedStatus:      http.StatusNotFound,
			expectedBody:        "",
			expectHandlerCalled: false,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			handlerCalled := false
			mux := http.NewServeMux()
			mux.HandleFunc("/test", func(w http.ResponseWriter, r *http.Request) {
				handlerCalled = true
				w.WriteHeader(http.StatusOK)
				if _, err := w.Write([]byte("test response")); err != nil {
					t.Fatalf("failed to write response: %v", err)
				}
			})

			config := &HTTPConfig{
				ListenAddr:   testHTTPListenAddr,
				ReadTimeout:  testReadWriteTimeout,
				WriteTimeout: testReadWriteTimeout,
				Handler:      mux,
			}

			logger := testutil.NewDiscardLogger()
			srv, err := NewHTTPServer(config, logger)
			if err != nil {
				t.Fatalf("Failed to create server: %v", err)
			}

			req := httptest.NewRequest("GET", tc.requestPath, nil)
			recorder := httptest.NewRecorder()
			srv.Handler.ServeHTTP(recorder, req)

			if handlerCalled != tc.expectHandlerCalled {
				t.Errorf("handlerCalled = %v, want %v", handlerCalled, tc.expectHandlerCalled)
			}
			if recorder.Code != tc.expectedStatus {
				t.Errorf("Expected status %d, got %d", tc.expectedStatus, recorder.Code)
			}
			if tc.expectedBody != "" {
				if body := recorder.Body.String(); body != tc.expectedBody {
					t.Errorf("Expected body %q, got %q", tc.expectedBody, body)
				}
			}
		})
	}
}

func TestNewHTTPServer_Configuration(t *testing.T) {
	config := &HTTPConfig{
		ListenAddr:   "127.0.0.1:9999",
		ReadTimeout:  configReadTimeout,
		WriteTimeout: configWriteTimeout,
		Handler:      http.NotFoundHandler(),
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewHTTPServer(config, logger)
	if err != nil {
		t.Fatalf("Failed to create server: %v", err)
	}

	if srv.Addr != config.ListenAddr {
		t.Errorf("Expected Addr %s, got %s", config.ListenAddr, srv.Addr)
	}
	if srv.ReadTimeout != configReadTimeout {
		t.Errorf("Expected ReadTimeout %v, got %v", configReadTimeout, srv.ReadTimeout)
	}
	if srv.WriteTimeout != configWriteTimeout {
		t.Errorf("Expected WriteTimeout %v, got %v", configWriteTimeout, srv.WriteTimeout)
	}
	if srv.Handler == nil {
		t.Error("Expected Handler to be set, got nil")
	}
}

func TestRunHTTPServer_Lifecycle(t *testing.T) {
	tests := []struct {
		name        string
		makeRequest bool
	}{
		{name: "WithRequest", makeRequest: true},
		{name: "WithoutRequest", makeRequest: false},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			var requestReceived atomic.Bool
			handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				requestReceived.Store(true)
				w.WriteHeader(http.StatusOK)
				if _, err := w.Write([]byte("ok")); err != nil {
					t.Errorf("failed to write response: %v", err)
				}
			})

			config := &HTTPConfig{
				ListenAddr:   testDynamicListenAddr,
				ReadTimeout:  lifecycleReadWriteTimeout,
				WriteTimeout: lifecycleReadWriteTimeout,
				Handler:      handler,
			}

			logger := testutil.NewDiscardLogger()
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

			time.Sleep(testServerStartupWait)

			if tc.makeRequest {
				client := &http.Client{Timeout: testHTTPRequestTimeout}
				resp, err := client.Get("http://" + srv.Addr)
				if err != nil {
					t.Fatalf("Failed to connect to server: %v", err)
				}
				defer resp.Body.Close()

				if resp.StatusCode != http.StatusOK {
					t.Errorf("Expected status 200, got %d", resp.StatusCode)
				}

				body, err := io.ReadAll(resp.Body)
				if err != nil {
					t.Fatalf("failed to read response body: %v", err)
				}
				if string(body) != "ok" {
					t.Errorf("Expected body %q, got %q", "ok", string(body))
				}

				if !requestReceived.Load() {
					t.Error("Handler was not called")
				}
			}

			cancel()

			select {
			case err := <-errChan:
				if err != nil {
					t.Errorf("Server stopped with error: %v", err)
				}
			case <-time.After(lifecycleShutdownTimeout):
				t.Fatal("Server did not stop within timeout")
			}
		})
	}
}

func TestRunHTTPServer_InvalidPort(t *testing.T) {
	config := &HTTPConfig{
		ListenAddr: "invalid:99999",
		Handler:    http.NotFoundHandler(),
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewHTTPServer(config, logger)
	if err != nil {
		t.Fatalf("NewHTTPServer failed: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	_, err = RunHTTPServer(ctx, srv, logger)
	if err == nil {
		t.Error("Expected error for invalid port, got nil")
	}
}

func TestRunHTTPServer_ShutdownWithoutRequests(t *testing.T) {
	config := &HTTPConfig{
		ListenAddr: testDynamicListenAddr,
		Handler:    http.NotFoundHandler(),
	}

	logger := testutil.NewDiscardLogger()
	srv, err := NewHTTPServer(config, logger)
	if err != nil {
		t.Fatalf("NewHTTPServer failed: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())

	errChan, err := RunHTTPServer(ctx, srv, logger)
	if err != nil {
		t.Fatalf("Failed to start server: %v", err)
	}

	time.Sleep(testServerStartupWait)
	cancel()

	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Expected clean shutdown, got error: %v", err)
		}
	case <-time.After(shutdownWaitWithoutRequests):
		t.Fatal("Shutdown timed out")
	}
}
