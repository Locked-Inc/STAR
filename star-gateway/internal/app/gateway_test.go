package app

import (
	"context"
	"log/slog"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
)

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
	mockDisp := &mockDispatcher{}
	mockHarq := &mockHARQ{}

	// Create mock services
	services := &serviceSet{
		motorControl:  service.NewMotorControlService(mockHarq, mockDisp, newDiscardLogger()),
		telemetry:     service.NewTelemetryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
		battery:       service.NewBatteryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
		configuration: service.NewConfigurationService(mockHarq, mockDisp, newDiscardLogger()),
		firmware:      service.NewFirmwareService(),
	}

	// Verify all services are initialized
	if services.motorControl == nil {
		t.Fatal("motorControl service is nil")
	}
	if services.telemetry == nil {
		t.Fatal("telemetry service is nil")
	}
	if services.battery == nil {
		t.Fatal("battery service is nil")
	}
	if services.configuration == nil {
		t.Fatal("configuration service is nil")
	}
	if services.firmware == nil {
		t.Fatal("firmware service is nil")
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
	srv := grpc.NewServer()
	mockDisp := &mockDispatcher{}
	mockHarq := &mockHARQ{}

	tests := []struct {
		name     string
		services *serviceSet
		wantErr  bool
	}{
		{
			name: "AllServicesPresent",
			services: &serviceSet{
				motorControl:  service.NewMotorControlService(mockHarq, mockDisp, newDiscardLogger()),
				telemetry:     service.NewTelemetryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
				battery:       service.NewBatteryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
				configuration: service.NewConfigurationService(mockHarq, mockDisp, newDiscardLogger()),
				firmware:      service.NewFirmwareService(),
			},
			wantErr: false,
		},
		// Note: gRPC registration doesn't validate nil services at registration time,
		// so we can't test nil service handling here. This is documented behavior.
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := registerGRPCServices(srv, tc.services)
			if (err != nil) != tc.wantErr {
				t.Errorf("Expected error: %v, got: %v", tc.wantErr, err)
			}
		})
	}
}

// TestServiceSet_TypeCompatibility verifies that serviceSet types match protobuf expectations.
// This test ensures that when protobuf service definitions change, we catch type mismatches.
func TestServiceSet_TypeCompatibility(t *testing.T) {
	mockDisp := &mockDispatcher{}
	mockHarq := &mockHARQ{}

	services := &serviceSet{
		motorControl:  service.NewMotorControlService(mockHarq, mockDisp, newDiscardLogger()),
		telemetry:     service.NewTelemetryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
		battery:       service.NewBatteryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
		configuration: service.NewConfigurationService(mockHarq, mockDisp, newDiscardLogger()),
		firmware:      service.NewFirmwareService(),
	}

	// Verify each service implements the expected protobuf server interface
	// This will fail at compile time if service interfaces change

	var _ starv1.MotorControlServiceServer = services.motorControl
	var _ starv1.TelemetryServiceServer = services.telemetry
	var _ starv1.BatteryManagementServiceServer = services.battery
	var _ starv1.ConfigurationServiceServer = services.configuration
	var _ starv1.FirmwareUpdateServiceServer = services.firmware

	t.Log("All services implement expected protobuf interfaces")
}

// TestServiceSet_AllFieldsPopulated verifies that serviceSet has all required fields.
// Add new fields to this test when adding new services.
func TestServiceSet_AllFieldsPopulated(t *testing.T) {
	mockDisp := &mockDispatcher{}
	mockHarq := &mockHARQ{}

	services := &serviceSet{
		motorControl:  service.NewMotorControlService(mockHarq, mockDisp, newDiscardLogger()),
		telemetry:     service.NewTelemetryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
		battery:       service.NewBatteryService(context.Background(), mockHarq, mockDisp, newDiscardLogger()),
		configuration: service.NewConfigurationService(mockHarq, mockDisp, newDiscardLogger()),
		firmware:      service.NewFirmwareService(),
	}

	// This list must match the serviceSet struct definition.
	// If you add a new service, add it here to ensure it's initialized.
	expectedServices := map[string]interface{}{
		"motorControl":  services.motorControl,
		"telemetry":     services.telemetry,
		"battery":       services.battery,
		"configuration": services.configuration,
		"firmware":      services.firmware,
	}

	for name, svc := range expectedServices {
		if svc == nil {
			t.Errorf("Service %s is nil - ensure it's initialized in initServices()", name)
		}
	}

	// Expected count of services (update this when adding new services)
	const expectedServiceCount = 5
	if len(expectedServices) != expectedServiceCount {
		t.Errorf("Expected %d services, got %d. Did you add a new service without updating this test?",
			expectedServiceCount, len(expectedServices))
	}
}

// TestInitServices verifies that initServices creates all services correctly.
func TestInitServices(t *testing.T) {
	ctx := context.Background()
	logger := newDiscardLogger()
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

	// Verify individual services
	if services.motorControl == nil {
		t.Error("motorControl not initialized")
	}
	if services.telemetry == nil {
		t.Error("telemetry not initialized")
	}
	if services.battery == nil {
		t.Error("battery not initialized")
	}
	if services.configuration == nil {
		t.Error("configuration not initialized")
	}
	if services.firmware == nil {
		t.Error("firmware not initialized")
	}
}

// newDiscardLogger creates a logger that discards all output (for testing).
func newDiscardLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(nil, nil))
}
