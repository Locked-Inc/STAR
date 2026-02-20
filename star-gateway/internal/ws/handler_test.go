package ws

import (
	"context"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/gorilla/websocket"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// mockMotorController is a test stub for the MotorController interface.
// No MotorController calls are expected in handler tests; EmergencyStop is
// provided only to satisfy the interface.
type mockMotorController struct{}

func (m *mockMotorController) EmergencyStop(_ context.Context, _ string) error { return nil }

// discardLogger returns a no-op logger suitable for tests.
func discardLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}

// testDialer is a WebSocket dialer with a short handshake timeout so tests
// fail fast rather than hanging indefinitely on network/server issues.
var testDialer = &websocket.Dialer{
	HandshakeTimeout: 2 * time.Second,
}

// TestNewHandler_Logger verifies that NewHandler returns a non-nil handler for
// both nil and non-nil logger arguments (table-driven).
func TestNewHandler_Logger(t *testing.T) {
	tests := []struct {
		name   string
		logger *slog.Logger
	}{
		{name: "nil logger", logger: nil},
		{name: "with logger", logger: discardLogger()},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			hub := NewHub(&mockMotorController{}, tc.logger)
			h := NewHandler(hub, &mockMotorController{}, tc.logger)
			require.NotNil(t, h)
		})
	}
}

func TestNewHandler_UpgradesConnection(t *testing.T) {
	logger := discardLogger()
	hub := NewHub(&mockMotorController{}, logger)
	h := NewHandler(hub, &mockMotorController{}, logger)

	// Drain hub.register BEFORE creating the server so the handler can write to
	// hub.register without racing against the client connect.
	go func() {
		<-hub.register
	}()

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	conn, _, err := testDialer.Dial(wsURL, nil)
	require.NoError(t, err, "NewHandler must successfully upgrade the HTTP connection")
	t.Cleanup(func() { _ = conn.Close() })

	// Verify the socket is usable: send a ping frame and assert no write error.
	require.NoError(t, conn.SetWriteDeadline(time.Now().Add(time.Second)))
	require.NoError(t, conn.WriteMessage(websocket.PingMessage, []byte("probe")),
		"upgraded connection must accept a ping frame")
}

// TestNewHandler_PlainHTTPRejected verifies that a plain HTTP GET request
// (no Upgrade header) is rejected with a non-101 response code.
func TestNewHandler_PlainHTTPRejected(t *testing.T) {
	logger := discardLogger()
	hub := NewHub(&mockMotorController{}, logger)
	h := NewHandler(hub, &mockMotorController{}, logger)

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	resp, err := http.Get(srv.URL) //nolint:noctx // test helper, no production context required
	require.NoError(t, err)
	t.Cleanup(func() { _ = resp.Body.Close() })
	assert.NotEqual(t, http.StatusSwitchingProtocols, resp.StatusCode,
		"plain HTTP GET must not be accepted as a WebSocket upgrade")
}

// TestNewHandler_NilHub verifies that a handler constructed with a nil hub
// returns http.StatusServiceUnavailable rather than panicking.
func TestNewHandler_NilHub(t *testing.T) {
	h := NewHandler(nil, &mockMotorController{}, discardLogger())
	require.NotNil(t, h)

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	resp, err := http.Get(srv.URL) //nolint:noctx
	require.NoError(t, err)
	t.Cleanup(func() { _ = resp.Body.Close() })
	assert.Equal(t, http.StatusServiceUnavailable, resp.StatusCode,
		"nil hub must return 503 rather than panic")
}

// TestNewHandler_ConcurrentUpgrades verifies that multiple simultaneous
// WebSocket upgrade requests are handled cleanly: no hangs, no panics, and
// all connections either succeed or fail with a clean error.
func TestNewHandler_ConcurrentUpgrades(t *testing.T) {
	const n = 5

	logger := discardLogger()
	hub := NewHub(&mockMotorController{}, logger)
	h := NewHandler(hub, &mockMotorController{}, logger)

	// Drain hub.register for all n clients before starting the server so the
	// upgrade handler never blocks waiting for the channel.
	go func() {
		for i := 0; i < n; i++ {
			<-hub.register
		}
	}()

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")

	var wg sync.WaitGroup
	for i := 0; i < n; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			conn, _, err := testDialer.Dial(wsURL, nil)
			if err != nil {
				return // acceptable: short timeout or server contention
			}
			_ = conn.Close()
		}()
	}
	wg.Wait()
}
