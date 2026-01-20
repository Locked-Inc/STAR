// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// ============================================================================
// Test Constants
// ============================================================================

const (
	// testGoroutineStartTimeout is the maximum time to wait for background goroutines to start
	testGoroutineStartTimeout = 500 * time.Millisecond

	// Stream test timing constants
	testStreamTimeout       = 300 * time.Millisecond // Context timeout for stream tests
	testBatterySendInterval = 50 * time.Millisecond  // Interval for sending battery states
	testStreamRateHz        = 5                      // Stream rate in Hz (200ms per message)
	testStreamWaitTimeout   = 500 * time.Millisecond // Maximum wait for stream completion

	// Cancellation test timing constants
	testCancelDelay = 50 * time.Millisecond  // Delay before calling cancel()
	testCancelWait  = 200 * time.Millisecond // Maximum wait for cancellation to complete
)

// ============================================================================
// Test Helpers
// ============================================================================

// mockBatteryStreamServer implements BatteryManagementService_StreamBatteryStateServer for testing.
type mockBatteryStreamServer struct {
	grpc.ServerStream
	ctx      context.Context
	sentData []*starv1.BatteryState
	sendErr  error
}

func (m *mockBatteryStreamServer) Send(data *starv1.BatteryState) error {
	if m.sendErr != nil {
		return m.sendErr
	}
	m.sentData = append(m.sentData, data)
	return nil
}

func (m *mockBatteryStreamServer) Context() context.Context {
	return m.ctx
}

// createTestBatteryState creates a test battery state with realistic values.
func createTestBatteryState() *starv1.BatteryState {
	return &starv1.BatteryState{
		Cells: &starv1.CellData{
			CellMv:     []uint32{3700, 3705, 3698, 3702, 3710, 3695, 3700, 3708}, // 8 cells
			ValidCells: 8,
		},
		Temperatures: &starv1.TemperatureData{
			TempDeciCelsius: []int32{285, 290, 288}, // 28.5°C, 29.0°C, 28.8°C
			ValidSensors:    3,
		},
		Current: &starv1.CurrentData{
			CurrentMa:    -2450, // 2.45A discharge (negative)
			AvgCurrentMa: -2400, // 2.4A average
			VoltageMv:    29600, // 8S nominal (3.7V * 8)
		},
		Soc: &starv1.StateOfChargeData{
			RemainingCapacityMah: 3400,
			FullCapacityMah:      4000,
			DesignCapacityMah:    4000,
			RelativeSocPercent:   85,
			AbsoluteSocPercent:   85,
		},
		Status: &starv1.BatteryStatus{
			BatteryStatusRegister:   0,
			SafetyStatusRegister:    0,
			OperationStatusRegister: 0,
			AlarmWarningRegister:    0,
			Charging:                false,
			Discharging:             true,
			FullyCharged:            false,
		},
		TimestampUs: time.Now().UnixMicro(),
	}
}

// createValidProtectionThresholds creates valid protection thresholds for testing.
func createValidProtectionThresholds() *starv1.ProtectionThresholds {
	return &starv1.ProtectionThresholds{
		OvervoltageMv:        4250,  // 4.25V
		UndervoltageMv:       2800,  // 2.8V
		OverchargeMa:         5000,  // 5A
		OverdischargeMa:      10000, // 10A
		OvertempDeciCelsius:  600,   // 60°C
		UndertempDeciCelsius: -100,  // -10°C
	}
}

// createTestDeviceInfo creates test BMS device info.
func createTestDeviceInfo() *starv1.BmsDeviceInfo {
	return &starv1.BmsDeviceInfo{
		DeviceType:      1,      // BQ7850 device type
		FirmwareVersion: 0x0210, // v2.1.0 encoded
		HardwareVersion: 0x0100, // v1.0 encoded
		SerialNumber:    12345,  // Test serial
	}
}

// ============================================================================
// Constructor Tests
// ============================================================================

func TestNewBatteryService(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	ready := make(chan struct{})
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			ch := make(chan *starv1.WireMessage)
			// Signal that subscription happened (goroutine started)
			select {
			case ready <- struct{}{}:
			default:
			}
			return ch
		},
	}
	logger := testutil.NewDiscardLogger()

	ctx := context.Background()
	svc := NewBatteryService(ctx, mockHARQ, mockDispatcher, logger)
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
	defer svc.Shutdown()

	// Wait for background goroutine to start
	select {
	case <-ready:
		// Goroutine started successfully
	case <-time.After(testGoroutineStartTimeout):
		t.Fatal("Background goroutine did not start")
	}
}

// ============================================================================
// GetBatteryState Tests (3 tests)
// ============================================================================

func TestGetBatteryState_Success(t *testing.T) {
	testBattery := createTestBatteryState()
	response := &starv1.GetBatteryStateResponse{
		Header: &starv1.ResponseHeader{
			RequestId: "test-battery-1",
			Status:    starv1.Status_STATUS_OK,
		},
		State: testBattery,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.GetBatteryStateRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-battery-1",
		},
	}

	resp, err := svc.GetBatteryState(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.Header.RequestId != "test-battery-1" {
		t.Errorf("Expected request_id 'test-battery-1', got '%s'", resp.Header.RequestId)
	}

	if resp.State == nil {
		t.Fatal("Expected non-nil battery state")
	}

	if resp.State.Soc.RelativeSocPercent != testBattery.Soc.RelativeSocPercent {
		t.Errorf("Expected SOC %d%%, got %d%%", testBattery.Soc.RelativeSocPercent, resp.State.Soc.RelativeSocPercent)
	}
}

func TestGetBatteryState_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	_, err := svc.GetBatteryState(context.Background(), nil)
	if err == nil {
		t.Fatal("Expected error for nil request")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestGetBatteryState_HarqFailure(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(_ context.Context, _ []byte, _ ...harq.Priority) error {
			return status.Error(codes.Unavailable, "harq send failure")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.GetBatteryStateRequest{Header: &starv1.RequestHeader{RequestId: "test-harq"}}

	_, err := svc.GetBatteryState(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for HARQ failure")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.Unavailable {
		t.Errorf("Expected Unavailable code, got %v", st.Code())
	}
}

// ============================================================================
// StreamBatteryState Tests (4 tests)
// ============================================================================

func TestStreamBatteryState_ValidRate(t *testing.T) {
	testBattery := createTestBatteryState()

	mockHARQ := &testutil.MockHARQ{}
	batteryCh := make(chan *starv1.WireMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return batteryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	ctx, cancel := context.WithTimeout(context.Background(), testStreamTimeout)
	defer cancel()

	mockStream := &mockBatteryStreamServer{
		ctx: ctx,
	}

	req := &starv1.StreamBatteryStateRequest{
		Header: &starv1.RequestHeader{RequestId: "test-stream"},
		RateHz: testStreamRateHz,
	}

	// Start goroutine to continuously send battery states
	go func() {
		ticker := time.NewTicker(testBatterySendInterval)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				select {
				case batteryCh <- &starv1.WireMessage{
					Payload: &starv1.WireMessage_BatteryState{
						BatteryState: testBattery,
					},
				}:
				default:
					// Channel full, skip
				}
			}
		}
	}()

	// Run stream in goroutine
	errCh := make(chan error, 1)
	go func() {
		errCh <- svc.StreamBatteryState(req, mockStream)
	}()

	// Wait for stream to process
	select {
	case err := <-errCh:
		if err != nil && !errors.Is(err, context.DeadlineExceeded) && !errors.Is(err, context.Canceled) {
			t.Fatalf("Stream failed: %v", err)
		}
	case <-time.After(testStreamWaitTimeout):
		t.Fatal("Stream did not complete")
	}

	// Should have received at least one message
	if len(mockStream.sentData) < 1 {
		t.Errorf("Expected at least 1 message, got %d", len(mockStream.sentData))
	}
}

func TestStreamBatteryState_InvalidRate(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	batteryCh := make(chan *starv1.WireMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return batteryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	testBattery := createTestBatteryState()
	batteryCh <- &starv1.WireMessage{
		Payload: &starv1.WireMessage_BatteryState{
			BatteryState: testBattery,
		},
	}

	ctx, cancel := context.WithTimeout(context.Background(), testStreamTimeout)
	defer cancel()

	mockStream := &mockBatteryStreamServer{
		ctx: ctx,
	}

	// Test with invalid rate (15 Hz > MaxBatteryRateHz of 10)
	req := &starv1.StreamBatteryStateRequest{
		Header: &starv1.RequestHeader{RequestId: "test-invalid-rate"},
		RateHz: 15, // Invalid: max is 10 Hz
	}

	// Should use default rate instead
	errCh := make(chan error, 1)
	go func() {
		errCh <- svc.StreamBatteryState(req, mockStream)
	}()

	select {
	case err := <-errCh:
		// Should complete normally (uses default rate)
		if err != nil && !errors.Is(err, context.DeadlineExceeded) && !errors.Is(err, context.Canceled) {
			t.Fatalf("Stream failed: %v", err)
		}
	case <-time.After(testStreamWaitTimeout):
		t.Fatal("Stream did not complete")
	}
}

func TestStreamBatteryState_ContextCancellation(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	batteryCh := make(chan *starv1.WireMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return batteryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	ctx, cancel := context.WithCancel(context.Background())

	mockStream := &mockBatteryStreamServer{
		ctx: ctx,
	}

	req := &starv1.StreamBatteryStateRequest{
		Header: &starv1.RequestHeader{RequestId: "test-cancel"},
		RateHz: 1,
	}

	errCh := make(chan error, 1)
	go func() {
		errCh <- svc.StreamBatteryState(req, mockStream)
	}()

	// Cancel after a short delay
	time.Sleep(testCancelDelay)
	cancel()

	// Should return context.Canceled
	select {
	case err := <-errCh:
		if !errors.Is(err, context.Canceled) {
			t.Errorf("Expected context.Canceled, got: %v", err)
		}
	case <-time.After(testCancelWait):
		t.Fatal("Stream did not exit after context cancellation")
	}
}

func TestStreamBatteryState_MultipleClients(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	batteryCh := make(chan *starv1.WireMessage, 10)
	subscribeCount := 0
	var mu sync.Mutex

	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			mu.Lock()
			subscribeCount++
			mu.Unlock()
			return batteryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	testBattery := createTestBatteryState()
	batteryCh <- &starv1.WireMessage{
		Payload: &starv1.WireMessage_BatteryState{
			BatteryState: testBattery,
		},
	}

	ctx, cancel := context.WithTimeout(context.Background(), testStreamTimeout)
	defer cancel()

	// Create 3 concurrent clients
	numClients := 3
	errCh := make(chan error, numClients)

	for range numClients {
		mockStream := &mockBatteryStreamServer{
			ctx: ctx,
		}

		req := &starv1.StreamBatteryStateRequest{
			Header: &starv1.RequestHeader{RequestId: "test-multi"},
			RateHz: 5,
		}

		go func() {
			errCh <- svc.StreamBatteryState(req, mockStream)
		}()
	}

	// Wait for all clients to complete
	for range numClients {
		select {
		case err := <-errCh:
			if err != nil && !errors.Is(err, context.DeadlineExceeded) && !errors.Is(err, context.Canceled) {
				t.Fatalf("Stream failed: %v", err)
			}
		case <-time.After(testStreamWaitTimeout):
			t.Fatal("Not all streams completed")
		}
	}

	// Each client should have subscribed
	mu.Lock()
	if subscribeCount != numClients+1 { // +1 for the background cache goroutine
		t.Errorf("Expected %d subscriptions, got %d", numClients+1, subscribeCount)
	}
	mu.Unlock()
}

// ============================================================================
// GetProtectionThresholds Tests (2 tests)
// ============================================================================

func TestGetProtectionThresholds_Success(t *testing.T) {
	thresholds := createValidProtectionThresholds()
	response := &starv1.GetProtectionThresholdsResponse{
		Header:     &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		Thresholds: thresholds,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.GetProtectionThresholdsRequest{
		Header: &starv1.RequestHeader{RequestId: "test-thresholds"},
	}

	resp, err := svc.GetProtectionThresholds(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.Thresholds.OvervoltageMv != thresholds.OvervoltageMv {
		t.Errorf("Expected OV %d mV, got %d mV", thresholds.OvervoltageMv, resp.Thresholds.OvervoltageMv)
	}
}

func TestGetProtectionThresholds_HarqFailure(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return nil, errors.New("receive timeout")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.GetProtectionThresholdsRequest{Header: &starv1.RequestHeader{RequestId: "test-fail"}}

	_, err := svc.GetProtectionThresholds(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for HARQ failure")
	}
}

// ============================================================================
// SetProtectionThresholds Tests (3 tests)
// ============================================================================

func TestSetProtectionThresholds_Success(t *testing.T) {
	response := &starv1.SetProtectionThresholdsResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.SetProtectionThresholdsRequest{
		Header:     &starv1.RequestHeader{RequestId: "test-set-thresholds"},
		Thresholds: createValidProtectionThresholds(),
	}

	resp, err := svc.SetProtectionThresholds(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}
}

func TestSetProtectionThresholds_InvalidThresholds(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	// Test with OV < UV (invalid)
	invalidThresholds := createValidProtectionThresholds()
	invalidThresholds.OvervoltageMv = 2700
	invalidThresholds.UndervoltageMv = 2800

	req := &starv1.SetProtectionThresholdsRequest{
		Header:     &starv1.RequestHeader{RequestId: "test-invalid"},
		Thresholds: invalidThresholds,
	}

	_, err := svc.SetProtectionThresholds(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for invalid thresholds (OV < UV)")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

func TestSetProtectionThresholds_OutOfRange(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	// Test with OV out of range
	invalidThresholds := createValidProtectionThresholds()
	invalidThresholds.OvervoltageMv = 5000 // > maxOvervoltageMv (4350)

	req := &starv1.SetProtectionThresholdsRequest{
		Header:     &starv1.RequestHeader{RequestId: "test-out-of-range"},
		Thresholds: invalidThresholds,
	}

	_, err := svc.SetProtectionThresholds(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error for out of range overvoltage")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.InvalidArgument {
		t.Errorf("Expected InvalidArgument code, got %v", st.Code())
	}
}

// ============================================================================
// Cell Balancing Tests (4 tests)
// ============================================================================

func TestEnableCellBalancing_Success(t *testing.T) {
	response := &starv1.EnableCellBalancingResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.EnableCellBalancingRequest{
		Header:   &starv1.RequestHeader{RequestId: "test-balancing"},
		CellMask: 0x0F, // Balance cells 0-3 (0b00001111)
	}

	resp, err := svc.EnableCellBalancing(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}
}

func TestEnableCellBalancing_InvalidMasks(t *testing.T) {
	tests := []struct {
		name string
		mask uint32
	}{
		{
			name: "invalid mask (too large)",
			mask: 0x1FFFF, // 17 bits (> 16 cells max)
		},
		{
			name: "zero mask",
			mask: 0, // No cells selected
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			mockHARQ := &testutil.MockHARQ{}
			mockDispatcher := &testutil.MockDispatcher{
				SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
					return make(chan *starv1.WireMessage)
				},
			}
			logger := testutil.NewDiscardLogger()

			svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
			defer svc.Shutdown()

			req := &starv1.EnableCellBalancingRequest{
				Header:   &starv1.RequestHeader{RequestId: tc.name},
				CellMask: tc.mask,
			}

			_, err := svc.EnableCellBalancing(context.Background(), req)
			if err == nil {
				t.Fatal("Expected error for invalid cell mask")
			}

			st, ok := status.FromError(err)
			if !ok {
				t.Fatal("Expected gRPC status error")
			}

			if st.Code() != codes.InvalidArgument {
				t.Errorf("Expected InvalidArgument code, got %v", st.Code())
			}
		})
	}
}

func TestDisableCellBalancing_Success(t *testing.T) {
	response := &starv1.DisableCellBalancingResponse{
		Header: &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.DisableCellBalancingRequest{
		Header: &starv1.RequestHeader{RequestId: "test-disable"},
	}

	resp, err := svc.DisableCellBalancing(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}
}

func TestGetBalancingStatus_Success(t *testing.T) {
	response := &starv1.GetBalancingStatusResponse{
		Header:         &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		ActiveCellMask: 0x0F, // Cells 0-3 active
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.GetBalancingStatusRequest{
		Header: &starv1.RequestHeader{RequestId: "test-status"},
	}

	resp, err := svc.GetBalancingStatus(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ActiveCellMask != 0x0F {
		t.Errorf("Expected active cell mask 0x0F, got 0x%X", resp.ActiveCellMask)
	}
}

// ============================================================================
// FET Control Tests (2 tests)
// ============================================================================

func TestControlFets_Success(t *testing.T) {
	response := &starv1.ControlFetsResponse{
		Header:              &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		ChargeFetEnabled:    true,
		DischargeFetEnabled: true,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.ControlFetsRequest{
		Header:             &starv1.RequestHeader{RequestId: "test-fets"},
		EnableChargeFet:    true,
		EnableDischargeFet: true,
	}

	resp, err := svc.ControlFets(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if !resp.ChargeFetEnabled {
		t.Error("Expected charge FET to be enabled")
	}

	if !resp.DischargeFetEnabled {
		t.Error("Expected discharge FET to be enabled")
	}
}

func TestControlFets_StateVerification(t *testing.T) {
	// Test that response matches request for both enabled and disabled states
	response := &starv1.ControlFetsResponse{
		Header:              &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		ChargeFetEnabled:    false,
		DischargeFetEnabled: true,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.ControlFetsRequest{
		Header:             &starv1.RequestHeader{RequestId: "test-state"},
		EnableChargeFet:    false,
		EnableDischargeFet: true,
	}

	resp, err := svc.ControlFets(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.ChargeFetEnabled != req.EnableChargeFet {
		t.Errorf("Expected charge FET state to match request: %v != %v", resp.ChargeFetEnabled, req.EnableChargeFet)
	}

	if resp.DischargeFetEnabled != req.EnableDischargeFet {
		t.Errorf("Expected discharge FET state to match request: %v != %v", resp.DischargeFetEnabled, req.EnableDischargeFet)
	}
}

// ============================================================================
// Device Info & Reset Tests (2 tests)
// ============================================================================

func TestGetDeviceInfo_Success(t *testing.T) {
	deviceInfo := createTestDeviceInfo()
	response := &starv1.GetDeviceInfoResponse{
		Header:     &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		DeviceInfo: deviceInfo,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.GetDeviceInfoRequest{
		Header: &starv1.RequestHeader{RequestId: "test-device-info"},
	}

	resp, err := svc.GetDeviceInfo(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp.DeviceInfo.DeviceType != deviceInfo.DeviceType {
		t.Errorf("Expected device type %d, got %d", deviceInfo.DeviceType, resp.DeviceInfo.DeviceType)
	}

	if resp.DeviceInfo.FirmwareVersion != deviceInfo.FirmwareVersion {
		t.Errorf("Expected firmware version 0x%X, got 0x%X", deviceInfo.FirmwareVersion, resp.DeviceInfo.FirmwareVersion)
	}

	if resp.DeviceInfo.SerialNumber != deviceInfo.SerialNumber {
		t.Errorf("Expected serial number %d, got %d", deviceInfo.SerialNumber, resp.DeviceInfo.SerialNumber)
	}
}

func TestResetDevice_Success(t *testing.T) {
	response := &starv1.ResetDeviceResponse{
		Header:         &starv1.ResponseHeader{Status: starv1.Status_STATUS_OK},
		ResetInitiated: true,
	}
	responsePayload, err := proto.Marshal(response)
	if err != nil {
		t.Fatalf("failed to marshal response: %v", err)
	}

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(_ context.Context) ([]byte, error) {
			return responsePayload, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	req := &starv1.ResetDeviceRequest{
		Header: &starv1.RequestHeader{RequestId: "test-reset"},
	}

	resp, err := svc.ResetDevice(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if !resp.ResetInitiated {
		t.Error("Expected reset to be initiated")
	}
}

func TestSetProtectionThresholds_MultipleValidationErrors(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return make(chan *starv1.WireMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	tests := []struct {
		name       string
		modifyFunc func(*starv1.ProtectionThresholds)
	}{
		{"invalid overcharge", func(t *starv1.ProtectionThresholds) { t.OverchargeMa = 20000 }},
		{"invalid overdischarge", func(t *starv1.ProtectionThresholds) { t.OverdischargeMa = 40000 }},
		{"invalid overtemp", func(t *starv1.ProtectionThresholds) { t.OvertempDeciCelsius = 1500 }},
		{"invalid undertemp", func(t *starv1.ProtectionThresholds) { t.UndertempDeciCelsius = -1000 }},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			thresholds := createValidProtectionThresholds()
			tc.modifyFunc(thresholds)

			req := &starv1.SetProtectionThresholdsRequest{
				Header:     &starv1.RequestHeader{RequestId: "test"},
				Thresholds: thresholds,
			}

			_, err := svc.SetProtectionThresholds(context.Background(), req)
			if err == nil {
				t.Errorf("Expected error for %s", tc.name)
			}
			if status.Code(err) != codes.InvalidArgument {
				t.Errorf("Expected InvalidArgument for %s, got %v", tc.name, status.Code(err))
			}
		})
	}
}

func TestBatteryService_ReceiveStreamBatteryLoop_Error(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	batteryCh := make(chan *starv1.WireMessage)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(_ dispatcher.MessageType) <-chan *starv1.WireMessage {
			return batteryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewBatteryService(context.Background(), mockHARQ, mockDispatcher, logger)
	
	// Close channel to trigger error in receive loop
	close(batteryCh)
	
	// Wait a bit for goroutine to process
	time.Sleep(50 * time.Millisecond)
	
	svc.Shutdown()
}

