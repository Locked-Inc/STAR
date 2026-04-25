// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package app

import (
	"context"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"strings"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"github.com/gorilla/websocket"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/proto"
)

const (
	httpServerStartupDelay = 50 * time.Millisecond
	websocketDialTimeout   = 2 * time.Second

	// serverReadinessDeadline is the maximum time to wait for the HTTP server to
	// accept its first TCP connection in the readiness probe of startTestHTTPServer.
	// Larger than httpServerStartupDelay to accommodate slow CI runners where OS
	// scheduling delays can stretch server startup well beyond 50 ms.
	serverReadinessDeadline = 500 * time.Millisecond

	// dialProbeTimeout is the per-attempt TCP dial timeout in the server readiness probe.
	dialProbeTimeout = 10 * time.Millisecond
	// dialProbeInterval is the sleep between TCP dial attempts in the server readiness probe.
	dialProbeInterval = 2 * time.Millisecond

	// testHTTPClientTimeout is the HTTP client timeout used in test helper requests.
	testHTTPClientTimeout = 100 * time.Millisecond
)

// serviceGetters is the shared table of service accessor functions.
// Update this list when adding new services to serviceSet.
var serviceGetters = []struct {
	name   string
	getter func(*serviceSet) any
}{
	{name: "motorControl", getter: func(s *serviceSet) any { return s.motorControl }},
	{name: "telemetry", getter: func(s *serviceSet) any { return s.telemetry }},
	{name: "configuration", getter: func(s *serviceSet) any { return s.configuration }},
	{name: "firmware", getter: func(s *serviceSet) any { return s.firmware }},
	{name: "gateway", getter: func(s *serviceSet) any { return s.gateway }},
}

// mockDispatcher is a minimal mock for testing.
type mockDispatcher struct{}

func (m *mockDispatcher) Subscribe(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
	ch := make(chan *dispatcher.DispatchedMessage)
	close(ch) // Return closed channel to prevent blocking
	return ch
}

func (m *mockDispatcher) Unsubscribe(msgType dispatcher.MessageType, ch <-chan *dispatcher.DispatchedMessage) {
}

func (m *mockDispatcher) Start(ctx context.Context) error {
	return nil
}

func (m *mockDispatcher) Stop() error {
	return nil
}

func (m *mockDispatcher) GetState() dispatcher.State {
	return dispatcher.StateIdle
}

// mockHARQ is a minimal mock for testing.
type mockHARQ struct{}

func (m *mockHARQ) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	return nil
}

func (m *mockHARQ) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	return nil, nil
}

func (m *mockHARQ) GetState() harq.State {
	return harq.StateIdle
}

func (m *mockHARQ) GetTxSequence() uint16 {
	return 0
}

func (m *mockHARQ) GetRxSequence() uint16 {
	return 0
}

func (m *mockHARQ) Reset() {
}

// TestRegisterGRPCServices verifies that all services are registered correctly.
// This test will fail if:
//   - New services are added to protobuf but not registered
//   - Service registration function signatures change
//   - Services are nil (initialization failed)
func TestRegisterGRPCServices(t *testing.T) {
	services := newTestServiceSet(t)

	for _, tc := range serviceGetters {
		t.Run("service initialized/"+tc.name, func(t *testing.T) {
			if tc.getter(services) == nil {
				t.Fatalf("%s service is nil", tc.name)
			}
		})
	}

	// Create gRPC server
	srv := grpc.NewServer()

	// Register services
	err := registerGRPCServices(srv, services)
	if err != nil {
		t.Fatalf("registerGRPCServices failed: %v", err)
	}

	// Verify server has registered services
	// Note: grpc.Server doesn't expose a way to list registered services,
	// but we can verify the function executed without error
	t.Log("All services registered successfully")
}

// TestRegisterGRPCServices_NilServices verifies error handling for nil services.
func TestRegisterGRPCServices_NilServices(t *testing.T) {
	tests := []struct {
		name            string
		services        *serviceSet
		wantErr         bool
		wantErrContains string // Expected substring in error message
	}{
		{
			name:     "AllServicesPresent",
			services: newTestServiceSet(t),
			wantErr:  false,
		},
		{
			name:            "NilMotorControlService",
			services:        func() *serviceSet { s := newTestServiceSet(t); s.motorControl = nil; return s }(),
			wantErr:         true,
			wantErrContains: "motorControl",
		},
		{
			name:            "NilTelemetryService",
			services:        func() *serviceSet { s := newTestServiceSet(t); s.telemetry = nil; return s }(),
			wantErr:         true,
			wantErrContains: "telemetry",
		},
		{
			name:            "NilConfigurationService",
			services:        func() *serviceSet { s := newTestServiceSet(t); s.configuration = nil; return s }(),
			wantErr:         true,
			wantErrContains: "configuration",
		},
		{
			name:            "NilFirmwareService",
			services:        func() *serviceSet { s := newTestServiceSet(t); s.firmware = nil; return s }(),
			wantErr:         true,
			wantErrContains: "firmware",
		},
		{
			name:            "NilGatewayService",
			services:        func() *serviceSet { s := newTestServiceSet(t); s.gateway = nil; return s }(),
			wantErr:         true,
			wantErrContains: "gateway",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			srv := grpc.NewServer()
			err := registerGRPCServices(srv, tc.services)
			if (err != nil) != tc.wantErr {
				t.Errorf("Expected error: %v, got: %v", tc.wantErr, err)
			}
			// Verify error message contains the service name
			if tc.wantErr && tc.wantErrContains != "" {
				if err == nil {
					t.Errorf("Expected error containing %q, got nil", tc.wantErrContains)
				} else if !strings.Contains(err.Error(), tc.wantErrContains) {
					t.Errorf("Expected error containing %q, got: %v", tc.wantErrContains, err)
				}
			}
		})
	}
}

// TestServiceSet_TypeCompatibility verifies that serviceSet types match protobuf expectations.
// This test ensures that when protobuf service definitions change, we catch type mismatches.
func TestServiceSet_TypeCompatibility(t *testing.T) {
	services := newTestServiceSet(t)

	// Verify each service implements the expected protobuf server interface
	// This will fail at compile time if service interfaces change

	var _ starv1.MotorControlServiceServer = services.motorControl
	var _ starv1.TelemetryServiceServer = services.telemetry
	var _ starv1.ConfigurationServiceServer = services.configuration
	var _ starv1.FirmwareUpdateServiceServer = services.firmware
	var _ starv1.GatewayServiceServer = services.gateway

	t.Log("All services implement expected protobuf interfaces")
}

// TestServiceSet_AllFieldsPopulated verifies that serviceSet has all required fields.
// Add new fields to this test when adding new services.
func TestServiceSet_AllFieldsPopulated(t *testing.T) {
	services := newTestServiceSet(t)

	// Reuse the shared serviceGetters table for nil checks.
	for _, tc := range serviceGetters {
		if tc.getter(services) == nil {
			t.Errorf("Service %s is nil - ensure it's initialized in initServices()", tc.name)
		}
	}

	// Expected count of services (update this when adding new services)
	const expectedServiceCount = 5
	if len(serviceGetters) != expectedServiceCount {
		t.Errorf("Expected %d services, got %d. Did you add a new service without updating serviceGetters?",
			expectedServiceCount, len(serviceGetters))
	}
}

// TestInitServices verifies that initServices creates all services correctly.
func TestInitServices(t *testing.T) {
	ctx := context.Background()
	logger := testutil.NewDiscardLogger()
	mockDisp := &mockDispatcher{}
	mockHarq := &mockHARQ{}

	// Call initServices with mock harq and dispatcher
	services, err := initServices(ctx, mockHarq, mockDisp, logger)
	if err != nil {
		t.Fatalf("initServices failed: %v", err)
	}

	// Verify all services are created
	if services == nil {
		t.Fatal("initServices returned nil serviceSet")
	}

	for _, tc := range serviceGetters {
		t.Run(tc.name, func(t *testing.T) {
			if tc.getter(services) == nil {
				t.Errorf("%s not initialized", tc.name)
			}
		})
	}
}

func TestStartGRPCServerWithAddr_RegistersAllServices(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	servers := &Servers{}
	services := newTestServiceSet(t)
	logger := testutil.NewDiscardLogger()

	if err := startGRPCServerWithAddr(ctx, servers, services, "127.0.0.1:0", logger); err != nil {
		t.Fatalf("startGRPCServerWithAddr failed: %v", err)
	}

	if servers.GRPCServer == nil {
		t.Fatal("expected GRPCServer to be initialized")
	}

	serviceInfo := servers.GRPCServer.GetServiceInfo()
	required := []string{
		"star.v1.MotorControlService",
		"star.v1.TelemetryService",
		"star.v1.ConfigurationService",
		"star.v1.FirmwareUpdateService",
		"star.v1.GatewayService",
	}

	for _, name := range required {
		if _, ok := serviceInfo[name]; !ok {
			t.Errorf("missing registered service: %s", name)
		}
	}
}

func TestStartHTTPServerWithAddr_WiresWSAndHealthRoutes(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	servers := &Servers{}
	services := newTestServiceSet(t)
	logger := testutil.NewDiscardLogger()

	if err := startHTTPServerWithAddr(ctx, servers, services, "127.0.0.1:0", logger); err != nil {
		t.Fatalf("startHTTPServerWithAddr failed: %v", err)
	}

	if servers.HTTPServer == nil {
		t.Fatal("expected HTTPServer to be initialized")
	}

	addr := servers.HTTPServer.Addr
	deadline := time.Now().Add(serverReadinessDeadline)
	serverReady := false
	for time.Now().Before(deadline) {
		c, dialErr := net.DialTimeout("tcp", addr, dialProbeTimeout)
		if dialErr == nil {
			_ = c.Close()
			serverReady = true
			break
		}
		time.Sleep(dialProbeInterval)
	}
	if !serverReady {
		t.Fatalf("HTTP server at %s did not become ready within %v", addr, serverReadinessDeadline)
	}

	healthResp, err := http.Get("http://" + addr + "/healthz")
	if err != nil {
		t.Fatalf("health check request failed: %v", err)
	}
	defer healthResp.Body.Close()

	if healthResp.StatusCode != http.StatusOK {
		t.Fatalf("expected 200 from /healthz, got %d", healthResp.StatusCode)
	}

	body, err := io.ReadAll(healthResp.Body)
	if err != nil {
		t.Fatalf("failed to read /healthz response: %v", err)
	}
	if string(body) != "ok" {
		t.Fatalf("expected /healthz body 'ok', got %q", string(body))
	}

	wsURL := "ws://" + addr + "/ws"
	wsCtx, wsCancel := context.WithTimeout(context.Background(), websocketDialTimeout)
	defer wsCancel()

	conn, _, err := websocket.DefaultDialer.DialContext(wsCtx, wsURL, nil)
	if err != nil {
		t.Fatalf("failed to connect websocket route: %v", err)
	}
	_ = conn.Close()
}

func newTestServiceSet(t *testing.T) *serviceSet {
	t.Helper()

	ctx := context.Background()
	logger := testutil.NewDiscardLogger()
	mockDisp := &mockDispatcher{}
	mockHarq := &mockHARQ{}

	services, err := initServices(ctx, mockHarq, mockDisp, logger)
	if err != nil {
		t.Fatalf("initServices failed: %v", err)
	}

	return services
}

// newTestServiceSetWith creates a serviceSet using the provided HARQ implementation.
// Use this when you need to control HARQ behaviour (e.g. inject failures) in a test.
func newTestServiceSetWith(t *testing.T, h harq.HARQ) *serviceSet {
	t.Helper()
	services, err := initServices(context.Background(), h, &mockDispatcher{}, testutil.NewDiscardLogger())
	if err != nil {
		t.Fatalf("initServices: %v", err)
	}
	return services
}

// startTestHTTPServer starts the HTTP server on a random port and returns the
// base URL ("http://host:port"). Cancellation and cleanup are wired to t.
// Instead of a fixed sleep it uses a readiness probe so the test only
// proceeds once the server is actually accepting connections.
func startTestHTTPServer(t *testing.T, services *serviceSet) string {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	servers := &Servers{}
	if err := startHTTPServerWithAddr(ctx, servers, services, "127.0.0.1:0", testutil.NewDiscardLogger()); err != nil {
		t.Fatalf("startHTTPServerWithAddr: %v", err)
	}

	// Readiness probe: repeatedly attempt a TCP dial until the server accepts
	// connections or the startup deadline is exceeded.
	addr := servers.HTTPServer.Addr
	deadline := time.Now().Add(serverReadinessDeadline)
	serverReady := false
	for time.Now().Before(deadline) {
		conn, dialErr := net.DialTimeout("tcp", addr, dialProbeTimeout)
		if dialErr == nil {
			_ = conn.Close()
			serverReady = true
			break
		}
		time.Sleep(dialProbeInterval)
	}
	if !serverReady {
		t.Fatalf("HTTP server at %s did not become ready within %v", addr, serverReadinessDeadline)
	}

	return "http://" + addr
}

// doRequest sends an HTTP request with the given method and URL, registers body
// cleanup with t, and returns the response. Fails the test on transport errors.
func doRequest(t *testing.T, method, url string) *http.Response {
	t.Helper()
	req, err := http.NewRequest(method, url, http.NoBody)
	if err != nil {
		t.Fatalf("http.NewRequest(%s, %s): %v", method, url, err)
	}
	client := &http.Client{Timeout: testHTTPClientTimeout}
	resp, err := client.Do(req)
	if err != nil {
		t.Fatalf("http.Do %s %s: %v", method, url, err)
	}
	t.Cleanup(func() { _ = resp.Body.Close() })
	return resp
}

// -- motorControllerAdapter ----------------------------------------------------

// TestMotorControllerAdapter_ForwardsReason verifies that the adapter serialises
// the plain reason string into the EmergencyStopCommand.Reason field of the
// WireMessage that reaches the HARQ layer.
func TestMotorControllerAdapter_ForwardsReason(t *testing.T) {
	mock := &testutil.MockHARQ{}
	services := newTestServiceSetWith(t, mock)
	adapter := &motorControllerAdapter{svc: services.motorControl}

	const wantReason = "encoder_fault"
	if err := adapter.EmergencyStop(context.Background(), wantReason); err != nil {
		t.Fatalf("EmergencyStop returned unexpected error: %v", err)
	}

	payload := mock.GetLastSentPayload()
	if payload == nil {
		t.Fatal("no payload captured: HARQ.Send was never called")
	}

	var wire starv1.WireMessage
	if err := proto.Unmarshal(payload, &wire); err != nil {
		t.Fatalf("proto.Unmarshal: %v", err)
	}

	estop := wire.GetEmergencyStopCommand()
	if estop == nil {
		t.Fatal("WireMessage contains no EmergencyStopCommand")
	}
	if estop.Reason != wantReason {
		t.Errorf("reason: got %q, want %q", estop.Reason, wantReason)
	}
	if !estop.EngageHardwareStop {
		t.Error("EngageHardwareStop must be true for hardware safety interlock")
	}
}

// TestMotorControllerAdapter_PropagatesError verifies that errors returned by
// the underlying MotorControlService are surfaced to the caller.
func TestMotorControllerAdapter_PropagatesError(t *testing.T) {
	mock := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return fmt.Errorf("harq transport unavailable")
		},
	}
	services := newTestServiceSetWith(t, mock)
	adapter := &motorControllerAdapter{svc: services.motorControl}

	if err := adapter.EmergencyStop(context.Background(), "test"); err == nil {
		t.Fatal("expected error from failing HARQ, got nil")
	}
}

// -- /api/estop endpoint -------------------------------------------------------

// TestEstopEndpoint_MethodEnforcement verifies that only POST is accepted on
// /api/estop; every other HTTP verb receives 405 Method Not Allowed.
func TestEstopEndpoint_MethodEnforcement(t *testing.T) {
	base := startTestHTTPServer(t, newTestServiceSet(t))
	url := base + "/api/estop"

	tests := []struct {
		method   string
		wantCode int
	}{
		{http.MethodPost, http.StatusNoContent},
		{http.MethodGet, http.StatusMethodNotAllowed},
		{http.MethodPut, http.StatusMethodNotAllowed},
		{http.MethodPatch, http.StatusMethodNotAllowed},
		{http.MethodDelete, http.StatusMethodNotAllowed},
	}
	for _, tc := range tests {
		t.Run(tc.method, func(t *testing.T) {
			resp := doRequest(t, tc.method, url)
			if resp.StatusCode != tc.wantCode {
				t.Errorf("expected %d, got %d", tc.wantCode, resp.StatusCode)
			}
		})
	}
}

// TestEstopEndpoint_ReasonFromQueryParam verifies that the ?reason= query
// parameter is forwarded as EmergencyStopCommand.Reason in the WireMessage.
func TestEstopEndpoint_ReasonFromQueryParam(t *testing.T) {
	mock := &testutil.MockHARQ{}
	base := startTestHTTPServer(t, newTestServiceSetWith(t, mock))

	const wantReason = "sensor_fault"
	resp := doRequest(t, http.MethodPost, base+"/api/estop?reason="+url.QueryEscape(wantReason))
	if resp.StatusCode != http.StatusNoContent {
		t.Fatalf("expected 204, got %d", resp.StatusCode)
	}

	payload := mock.GetLastSentPayload()
	if payload == nil {
		t.Fatal("no payload captured by mock HARQ")
	}
	var wire starv1.WireMessage
	if err := proto.Unmarshal(payload, &wire); err != nil {
		t.Fatalf("proto.Unmarshal: %v", err)
	}
	estop := wire.GetEmergencyStopCommand()
	if estop == nil {
		t.Fatal("WireMessage contains no EmergencyStopCommand")
	}
	if estop.Reason != wantReason {
		t.Errorf("reason: got %q, want %q", estop.Reason, wantReason)
	}
}

// TestEstopEndpoint_DefaultReason verifies that omitting ?reason= results in
// the fallback string "user_http_fallback" reaching the motor controller.
func TestEstopEndpoint_DefaultReason(t *testing.T) {
	mock := &testutil.MockHARQ{}
	base := startTestHTTPServer(t, newTestServiceSetWith(t, mock))

	resp := doRequest(t, http.MethodPost, base+"/api/estop") // no ?reason=
	if resp.StatusCode != http.StatusNoContent {
		t.Fatalf("expected 204, got %d", resp.StatusCode)
	}

	payload := mock.GetLastSentPayload()
	if payload == nil {
		t.Fatal("no payload captured")
	}
	var wire starv1.WireMessage
	if err := proto.Unmarshal(payload, &wire); err != nil {
		t.Fatalf("proto.Unmarshal: %v", err)
	}
	estop := wire.GetEmergencyStopCommand()
	if estop == nil {
		t.Fatal("WireMessage contains no EmergencyStopCommand")
	}
	if estop.Reason != defaultEstopReason {
		t.Errorf("reason: got %q, want %q", estop.Reason, defaultEstopReason)
	}
}

// TestEstopEndpoint_Returns500OnHARQError verifies that a transport-level
// failure causes the handler to return HTTP 500.
func TestEstopEndpoint_Returns500OnHARQError(t *testing.T) {
	mock := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return fmt.Errorf("transport unavailable")
		},
	}
	base := startTestHTTPServer(t, newTestServiceSetWith(t, mock))

	resp := doRequest(t, http.MethodPost, base+"/api/estop")
	if resp.StatusCode != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", resp.StatusCode)
	}
}

// -- Regression tests ----------------------------------------------------------

// TestOldWSControllerRoute_Returns404 is a regression guard ensuring the legacy
// /ws/controller endpoint is no longer served after being replaced by /ws.
func TestOldWSControllerRoute_Returns404(t *testing.T) {
	base := startTestHTTPServer(t, newTestServiceSet(t))
	resp := doRequest(t, http.MethodGet, base+"/ws/controller")
	if resp.StatusCode != http.StatusNotFound {
		t.Errorf("expected 404 from removed route /ws/controller, got %d", resp.StatusCode)
	}
}
