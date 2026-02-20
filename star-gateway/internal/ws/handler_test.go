package ws

import (
	"io"
	"log/slog"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gorilla/websocket"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// mockMotorController is a test stub for the MotorController interface.
type mockMotorController struct{}

func (m *mockMotorController) EmergencyStop(_ string) error { return nil }

// discardLogger returns a no-op logger suitable for tests.
func discardLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}

func TestNewHandler_NilLogger(t *testing.T) {
	hub := NewHub(&mockMotorController{}, nil)
	h := NewHandler(hub, &mockMotorController{}, nil)
	assert.NotNil(t, h)
}

func TestNewHandler_WithLogger(t *testing.T) {
	logger := discardLogger()
	hub := NewHub(&mockMotorController{}, logger)
	h := NewHandler(hub, &mockMotorController{}, logger)
	require.NotNil(t, h)
}

func TestNewHandler_UpgradesConnection(t *testing.T) {
	logger := discardLogger()
	hub := NewHub(&mockMotorController{}, logger)
	h := NewHandler(hub, &mockMotorController{}, logger)

	// Drain hub.register so the handler does not block.
	go func() {
		<-hub.register
	}()

	srv := httptest.NewServer(h)
	t.Cleanup(srv.Close)

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	conn, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	require.NoError(t, err, "NewHandler must successfully upgrade the HTTP connection")
	t.Cleanup(func() { _ = conn.Close() })
}
