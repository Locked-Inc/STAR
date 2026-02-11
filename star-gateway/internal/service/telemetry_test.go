// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
)

// mockTelemetryStreamServer implements TelemetryService_StreamTelemetryServer for testing.
type mockTelemetryStreamServer struct {
	grpc.ServerStream
	ctx      context.Context
	sentData []*starv1.TelemetryData
	sendErr  error
}

func (m *mockTelemetryStreamServer) Send(data *starv1.TelemetryData) error {
	if m.sendErr != nil {
		return m.sendErr
	}
	m.sentData = append(m.sentData, data)
	return nil
}

func (m *mockTelemetryStreamServer) Context() context.Context {
	return m.ctx
}

// Helper function to create test telemetry data
func createTestTelemetryData() *starv1.TelemetryData {
	return &starv1.TelemetryData{
		TimestampUs:        time.Now().UnixMicro(),
		FrameSequence:      1,
		BatterySocPercent:  85,
		BatteryVoltageV:    12.6,
		TemperatureCelsius: 28.5,
		EmergencyStop:      false,
		FaultFlags:         0,
	}
}

func TestNewTelemetryService(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return make(chan *dispatcher.DispatchedMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	ctx := context.Background()
	svc := NewTelemetryService(ctx, mockHARQ, mockDispatcher, logger)
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
	defer svc.Shutdown()

	// If we reach here, the background goroutine started successfully
	// (NewTelemetryService waits for initialization with internal timeout)
}

func TestGetTelemetry_Success(t *testing.T) {
	t.Skip("GetTelemetry RPC removed - firmware operates in push mode only")
}

func TestGetTelemetry_NilRequest(t *testing.T) {
	t.Skip("GetTelemetry RPC removed - firmware operates in push mode only")
}

func TestGetTelemetry_NoDataAvailable(t *testing.T) {
	t.Skip("GetTelemetry RPC removed - firmware operates in push mode only")
}

func TestStreamTelemetry_ValidRate(t *testing.T) {
	t.Skip("StreamTelemetry RPC removed - firmware operates in push mode only")
}

func TestStreamTelemetry_InvalidRate(t *testing.T) {
	t.Skip("StreamTelemetry RPC removed - firmware operates in push mode only")
}

func TestStreamTelemetry_ContextCancellation(t *testing.T) {
	t.Skip("StreamTelemetry RPC removed - firmware operates in push mode only")
}

func TestStreamTelemetry_SendError(t *testing.T) {
	t.Skip("StreamTelemetry RPC removed - firmware operates in push mode only")
}

func TestGetSystemStatus_Success(t *testing.T) {
	t.Skip("GetSystemStatus RPC removed - firmware operates in push mode only")
}

func TestGetSystemStatus_NilRequest(t *testing.T) {
	t.Skip("GetSystemStatus RPC removed - firmware operates in push mode only")
}

func TestValidateRateHz(t *testing.T) {
	t.Skip("validateRateHz method removed with StreamEncoders/StreamTelemetry")
}

func TestStreamTelemetry_MultipleClients(t *testing.T) {
	t.Skip("StreamTelemetry RPC removed - firmware operates in push mode only")
}
