// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package ws

import (
	"context"
	"errors"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"github.com/gorilla/websocket"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

// mockMotorController is a test stub for the MotorController interface.
// Set emergencyStopErr to inject a failure from EmergencyStop.
// Set onEmergencyStop to a non-nil channel to receive a signal (via close)
// when EmergencyStop is first called -- useful for deterministic synchronisation
// instead of time.Sleep in tests.
type mockMotorController struct {
	emergencyStopErr error
	onEmergencyStop  chan struct{} // closed on first EmergencyStop call when non-nil
	closeOnce        sync.Once
}

func (m *mockMotorController) EmergencyStop(_ context.Context, _ string) error {
	if m.onEmergencyStop != nil {
		m.closeOnce.Do(func() { close(m.onEmergencyStop) })
	}
	return m.emergencyStopErr
}

// discardLogger returns a no-op logger suitable for tests.
func discardLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}

const (
	// testHandshakeTimeout is the WebSocket handshake timeout for the test dialer.
	// Short enough to fail fast on issues without hanging the test suite.
	testHandshakeTimeout = 2 * time.Second

	// pingWriteDeadline is the write deadline used in test frames (ping, etc.).
	pingWriteDeadline = time.Second

	// estopProcessingWait is the maximum time to wait for a readPump to process
	// an e-stop frame and invoke EmergencyStop before the test times out.
	estopProcessingWait = 500 * time.Millisecond
)

// testDialer is a WebSocket dialer with a short handshake timeout so tests
// fail fast rather than hanging indefinitely on network/server issues.
var testDialer = &websocket.Dialer{
	HandshakeTimeout: testHandshakeTimeout,
}

// drainHubRegister receives count clients from hub.register, discarding each,
// with an overall timeout. It is called in a separate goroutine before starting
// the test HTTP server so that handler goroutines can write to hub.register
// without blocking. If the timeout expires before receiving count clients, it
// reports a test error via tb.Errorf.
func drainHubRegister(tb testing.TB, hub *Hub, count int, timeout time.Duration) {
	timer := time.NewTimer(timeout)
	defer timer.Stop()
	received := 0
	for received < count {
		select {
		case <-hub.register:
			received++
		case <-timer.C:
			tb.Errorf("drainHubRegister timed out after %v: received %d/%d clients from Hub.register",
				timeout, received, count)
			return
		}
	}
}

// TestNewHandler_Logger verifies that NewHandler returns a non-nil handler for
// both nil and non-nil logger arguments (table-driven).
func TestNewHandler_Logger(t *testing.T) {
	t.Parallel()
	tests := []struct {
		name   string
		logger *slog.Logger
	}{
		{name: "nil logger", logger: nil},
		{name: "with logger", logger: discardLogger()},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			t.Parallel()
			hub := NewHub(tc.logger)
			h := NewHandler(hub, &mockMotorController{}, tc.logger, true)
			require.NotNil(t, h)
		})
	}
}

func TestNewHandler_UpgradesConnection(t *testing.T) {
	t.Parallel()
	logger := discardLogger()
	hub := NewHub(logger)
	h := NewHandler(hub, &mockMotorController{}, logger, true)

	// Drain hub.register BEFORE creating the server so the handler can write to
	// hub.register without racing against the client connect.
	go drainHubRegister(t, hub, 1, testHandshakeTimeout)

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	conn, _, err := testDialer.Dial(wsURL, nil)
	require.NoError(t, err, "NewHandler must successfully upgrade the HTTP connection")
	t.Cleanup(func() { _ = conn.Close() })

	// Verify the socket is usable: send a ping frame and assert no write error.
	require.NoError(t, conn.SetWriteDeadline(time.Now().Add(pingWriteDeadline)))
	require.NoError(t, conn.WriteMessage(websocket.PingMessage, []byte("probe")),
		"upgraded connection must accept a ping frame")
}

// TestNewHandler_PlainHTTPRejected verifies that a plain HTTP GET request
// (no Upgrade header) is rejected with a non-101 response code.
func TestNewHandler_PlainHTTPRejected(t *testing.T) {
	t.Parallel()
	logger := discardLogger()
	hub := NewHub(logger)
	h := NewHandler(hub, &mockMotorController{}, logger, true)

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	resp, err := http.Get(srv.URL) //nolint:noctx // test helper, no production context required
	require.NoError(t, err)
	t.Cleanup(func() { _ = resp.Body.Close() })
	assert.Equal(t, http.StatusBadRequest, resp.StatusCode,
		"plain HTTP GET must be rejected with 400 Bad Request, not upgraded")
}

// TestNewHandler_NilHub verifies that constructing a handler with a nil hub
// panics immediately at construction time rather than silently allowing a
// runtime nil-pointer dereference during request handling.
func TestNewHandler_NilHub(t *testing.T) {
	t.Parallel()
	require.Panics(t, func() {
		_ = NewHandler(nil, &mockMotorController{}, discardLogger(), false)
	}, "NewHandler must panic at construction when hub is nil")
}

// TestNewHandler_NilMotorController verifies that constructing a handler with a
// nil motor controller panics immediately at construction time rather than
// silently allowing a nil-pointer dereference in Client.readPump when an
// e-stop frame arrives.
func TestNewHandler_NilMotorController(t *testing.T) {
	t.Parallel()
	logger := discardLogger()
	hub := NewHub(logger)
	require.Panics(t, func() {
		_ = NewHandler(hub, nil, logger, false)
	}, "NewHandler must panic at construction when motorService is nil")
}

// TestNewHandler_ConcurrentUpgrades verifies that multiple simultaneous
// WebSocket upgrade requests are handled cleanly: no hangs, no panics, and
// all connections either succeed or fail with a clean error.
func TestNewHandler_ConcurrentUpgrades(t *testing.T) {
	t.Parallel()
	const (
		concurrentClientCount = 5
		// majorityThreshold is the minimum number of successful upgrades required
		// for this test to pass. Using a named constant makes the intent clear and
		// avoids repeating the arithmetic in both the condition and the failure msg.
		majorityThreshold = concurrentClientCount / 2
	)

	logger := discardLogger()
	hub := NewHub(logger)
	h := NewHandler(hub, &mockMotorController{}, logger, true)

	// Drain hub.register for up to concurrentClientCount clients. The helper guards against
	// blocking indefinitely if fewer than concurrentClientCount dials succeed.
	go drainHubRegister(t, hub, concurrentClientCount, testHandshakeTimeout)

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")

	var (
		wg        sync.WaitGroup
		successes atomic.Int32
		failures  atomic.Int32
	)
	for i := 0; i < concurrentClientCount; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			conn, _, err := testDialer.Dial(wsURL, nil)
			if err != nil {
				failures.Add(1)
				t.Logf("dial failed (acceptable under contention): %v", err)
				return
			}
			successes.Add(1)
			_ = conn.Close()
		}()
	}
	wg.Wait()
	t.Logf("concurrent upgrades: %d succeeded, %d failed", successes.Load(), failures.Load())
	if successes.Load() <= int32(majorityThreshold) {
		t.Fatalf("expected a majority (>%d) of concurrent upgrades to succeed, got %d out of %d",
			majorityThreshold, successes.Load(), concurrentClientCount)
	}
}

// TestEstopScenarios verifies that both success and injected-error EmergencyStop
// calls keep the client connection open and allow the hub to continue operating.
func TestEstopScenarios(t *testing.T) {
	t.Parallel()

	testCases := []struct {
		name             string
		emergencyStopErr error
		reason           string
		writeExpectation string
		pingExpectation  string
	}{
		{
			name:             "injected error",
			emergencyStopErr: errors.New("injected failure"),
			reason:           "test-injected-failure",
			writeExpectation: "write must succeed even when motor controller will return an error",
			pingExpectation:  "connection must remain open after EmergencyStop returns an injected error",
		},
		{
			name:             "success",
			emergencyStopErr: nil,
			reason:           "test-success",
			writeExpectation: "write must succeed",
			pingExpectation:  "connection must remain open after successful EmergencyStop",
		},
	}

	for _, testCase := range testCases {
		testCase := testCase
		t.Run(testCase.name, func(t *testing.T) {
			logger := discardLogger()
			mock := &mockMotorController{
				emergencyStopErr: testCase.emergencyStopErr,
				onEmergencyStop:  make(chan struct{}),
			}
			hub := NewHub(logger)
			h := NewHandler(hub, mock, logger, true)

			ctx, cancel := context.WithCancel(context.Background())
			t.Cleanup(cancel)
			go hub.Run(ctx)

			srv := httptest.NewServer(h)
			t.Cleanup(srv.Close)

			wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
			conn, _, err := testDialer.Dial(wsURL, nil)
			require.NoError(t, err, "dial must succeed")
			t.Cleanup(func() { _ = conn.Close() })

			// Build and send an e-stop envelope for this scenario.
			env := &starv1.STAREnvelope{
				Payload: &starv1.STAREnvelope_Estop{
					Estop: &starv1.EStopCommand{Reason: testCase.reason},
				},
			}
			data, err := proto.Marshal(env)
			require.NoError(t, err)

			require.NoError(t, conn.SetWriteDeadline(time.Now().Add(pingWriteDeadline)))
			require.NoError(t, conn.WriteMessage(websocket.BinaryMessage, data),
				testCase.writeExpectation)

			// Wait deterministically for readPump to invoke EmergencyStop.
			select {
			case <-mock.onEmergencyStop:
				// EmergencyStop was called -- proceed.
			case <-time.After(estopProcessingWait):
				t.Fatal("timed out waiting for EmergencyStop to be called")
			}

			// Connection must remain open after EmergencyStop.
			require.NoError(t, conn.SetWriteDeadline(time.Now().Add(pingWriteDeadline)))
			require.NoError(t, conn.WriteMessage(websocket.PingMessage, nil),
				testCase.pingExpectation)
		})
	}
}
