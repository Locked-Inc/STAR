package controller

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
	"nhooyr.io/websocket" //nolint:staticcheck
)

func TestWebSocketHandler_ProcessMessage(t *testing.T) {
	// 1. Setup Server
	handler := NewHandler()
	s := httptest.NewServer(http.HandlerFunc(handler.ServeHTTP))
	defer s.Close()

	// 2. Connect Client
	wsURL := "ws" + strings.TrimPrefix(s.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*5)
	defer cancel()

	//nolint:staticcheck
	c, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("failed to dial: %v", err)
	}
	defer func() {
		//nolint:staticcheck
		_ = c.Close(websocket.StatusInternalError, "internal error")
	}()

	// 3. Send ControllerState Message
	msg := &starv1.ControllerState{
		LinearVelNormalized:  1.0,
		AngularVelNormalized: 0.5,
		TimestampMs:  time.Now().UnixMilli(),
	}
	bytes, err := proto.Marshal(msg)
	if err != nil {
		t.Fatalf("failed to marshal: %v", err)
	}

	//nolint:staticcheck
	err = c.Write(ctx, websocket.MessageBinary, bytes)
	if err != nil {
		t.Fatalf("failed to write: %v", err)
	}

	// 4. Verify Side Effect (e.g., checking last received state)
	// We'll wait a bit for async processing
	time.Sleep(100 * time.Millisecond)

	lastState := handler.GetLastState()
	if lastState == nil {
		t.Fatal("handler did not process message")
		return
	}

	if lastState.LinearVelNormalized != 1.0 {
		t.Errorf("got LinearVel %v, want 1.0", lastState.LinearVelNormalized)
	}
	if lastState.AngularVelNormalized != 0.5 {
		t.Errorf("got AngularVel %v, want 0.5", lastState.AngularVelNormalized)
	}
}

func TestWebSocketHandler_DebugFlag(t *testing.T) {
	// 1. Setup Server
	handler := NewHandler()
	s := httptest.NewServer(http.HandlerFunc(handler.ServeHTTP))
	defer s.Close()

	// 2. Connect Client
	wsURL := "ws" + strings.TrimPrefix(s.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*5)
	defer cancel()

	//nolint:staticcheck
	c, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("failed to dial: %v", err)
	}
	defer func() {
		//nolint:staticcheck
		_ = c.Close(websocket.StatusInternalError, "internal error")
	}()

	// 3. Send Message with Debug = true
	msg := &starv1.ControllerState{
		LinearVelNormalized:  0.5,
		AngularVelNormalized: -0.5,
		TimestampMs:  time.Now().UnixMilli(),
		Debug:      true,
	}
	bytes, err := proto.Marshal(msg)
	if err != nil {
		t.Fatalf("failed to marshal: %v", err)
	}

	//nolint:staticcheck
	err = c.Write(ctx, websocket.MessageBinary, bytes)
	if err != nil {
		t.Fatalf("failed to write: %v", err)
	}

	// 4. Verify State
	time.Sleep(100 * time.Millisecond)

	lastState := handler.GetLastState()
	if lastState == nil {
		t.Fatal("handler did not process message")
		return
	}

	if !lastState.Debug {
		t.Error("expected Debug flag to be true, got false")
	}
}

func TestWebSocketHandler_Watchdog(t *testing.T) {
	handler := NewHandler()

	// Inject state directly to test watchdog logic without full WS overhead
	handler.mu.Lock()
	handler.lastState = &starv1.ControllerState{LinearVelNormalized: 1.0}
	handler.lastReceived = time.Now()
	handler.mu.Unlock()

	// 1. Check immediate access
	state := handler.GetSafeState() // Method we will implement
	if state.LinearVelNormalized != 1.0 {
		t.Errorf("Immediate: got %v, want 1.0", state.LinearVelNormalized)
	}

	// 2. Wait for timeout > 200ms
	time.Sleep(250 * time.Millisecond)

	// 3. Check watchdog trigger
	state = handler.GetSafeState()
	if state.LinearVelNormalized != 0.0 {
		t.Errorf("After timeout: got %v, want 0.0", state.LinearVelNormalized)
	}
}

func TestHandlerWithGateway(t *testing.T) {
	// 1. Setup
	gw := service.NewGatewayService()
	handler := NewHandlerWithGateway(gw)
	s := httptest.NewServer(http.HandlerFunc(handler.ServeHTTP))
	defer s.Close()

	// 2. Connect
	wsURL := "ws" + strings.TrimPrefix(s.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*5)
	defer cancel()

	//nolint:staticcheck
	c, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("failed to dial: %v", err)
	}
	defer func() {
		//nolint:staticcheck
		_ = c.Close(websocket.StatusNormalClosure, "")
	}()

	// 3. Send Message
	msg := &starv1.ControllerState{
		LinearVelNormalized:  1.0,
		AngularVelNormalized: 0.0,
		TimestampMs:  time.Now().UnixMilli(),
	}
	bytes, _ := proto.Marshal(msg)
	//nolint:staticcheck
	if err := c.Write(ctx, websocket.MessageBinary, bytes); err != nil {
		t.Fatalf("failed to write: %v", err)
	}

	// 4. Verify Gateway received the command
	time.Sleep(100 * time.Millisecond)
	resp, err := gw.GetTeleopCommand(context.Background(), &starv1.GetTeleopCommandRequest{
		Header: &starv1.RequestHeader{RequestId: "test-req"},
	})
	if err != nil {
		t.Fatalf("GetTeleopCommand failed: %v", err)
	}
	if resp == nil || resp.Command == nil {
		t.Fatal("gateway did not return command")
	}
	if resp.Command.FrontLeftVelocityMps != 1.0 {
		t.Errorf("expected 1.0 mps, got %v", resp.Command.FrontLeftVelocityMps)
	}
}
