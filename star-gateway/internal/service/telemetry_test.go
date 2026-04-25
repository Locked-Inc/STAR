// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"errors"
	"io"
	"sync"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
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
		WifiSignalDbm:      -45,
		CpuUsagePercent:    32.1,
		TemperatureCelsius: 28.5,
		MotorLoadPercent:   50.0,
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
	testTelemetry := createTestTelemetryData()

	mockHARQ := &testutil.MockHARQ{}
	telemetryCh := make(chan *dispatcher.DispatchedMessage, 1)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return telemetryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	// Send telemetry data to cache
	telemetryCh <- &dispatcher.DispatchedMessage{
		WireMsg: &starv1.WireMessage{
			Payload: &starv1.WireMessage_TelemetryData{
				TelemetryData: testTelemetry,
			},
		},
		Metadata: nil,
	}

	// Wait for cache to update
	time.Sleep(10 * time.Millisecond)

	req := &starv1.GetTelemetryRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-1",
		},
	}

	resp, err := svc.GetTelemetry(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.Header.RequestId != "test-1" {
		t.Errorf("Expected request_id 'test-1', got '%s'", resp.Header.RequestId)
	}

	if resp.Telemetry == nil {
		t.Fatal("Expected non-nil telemetry")
	}
}

func TestGetTelemetry_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return make(chan *dispatcher.DispatchedMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	_, err := svc.GetTelemetry(context.Background(), nil)
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

func TestGetTelemetry_NoDataAvailable(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return make(chan *dispatcher.DispatchedMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	req := &starv1.GetTelemetryRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-1",
		},
	}

	_, err := svc.GetTelemetry(context.Background(), req)
	if err == nil {
		t.Fatal("Expected error when no data available")
	}

	st, ok := status.FromError(err)
	if !ok {
		t.Fatal("Expected gRPC status error")
	}

	if st.Code() != codes.Unavailable {
		t.Errorf("Expected Unavailable code, got %v", st.Code())
	}
}

func TestStreamTelemetry_ValidRate(t *testing.T) {
	testTelemetry := createTestTelemetryData()

	mockHARQ := &testutil.MockHARQ{}
	telemetryCh := make(chan *dispatcher.DispatchedMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return telemetryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	stream := &mockTelemetryStreamServer{ctx: ctx}

	req := &starv1.StreamTelemetryRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-stream-1",
		},
		RateHz: 20, // 20 Hz (50ms interval)
	}

	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.StreamTelemetry(req, stream)
	}()

	// Send telemetry data
	for i := 0; i < 3; i++ {
		telemetryCh <- &dispatcher.DispatchedMessage{
			WireMsg: &starv1.WireMessage{
				Payload: &starv1.WireMessage_TelemetryData{
					TelemetryData: testTelemetry,
				},
			},
			Metadata: nil,
		}
		time.Sleep(20 * time.Millisecond)
	}

	// Wait for some data to be sent
	time.Sleep(100 * time.Millisecond)

	// Cancel context to stop streaming
	cancel()

	// Wait for goroutine to exit
	select {
	case err := <-errChan:
		if err != context.Canceled && !errors.Is(err, context.Canceled) {
			t.Errorf("Expected context.Canceled error, got: %v", err)
		}
	case <-time.After(200 * time.Millisecond):
		t.Fatal("Streaming goroutine did not exit")
	}

	// Verify we received some telemetry
	if len(stream.sentData) == 0 {
		t.Error("Expected to receive telemetry data, got none")
	}
}

func TestStreamTelemetry_InvalidRate(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	telemetryCh := make(chan *dispatcher.DispatchedMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return telemetryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	testCases := []struct {
		name   string
		rateHz int32
	}{
		{"Zero rate", 0},
		{"Negative rate", -10},
		{"Too high rate", 150},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
			defer cancel()

			stream := &mockTelemetryStreamServer{ctx: ctx}

			req := &starv1.StreamTelemetryRequest{
				Header: &starv1.RequestHeader{
					RequestId: "test-stream",
				},
				RateHz: tc.rateHz,
			}

			// Should default to 10 Hz and not error
			errChan := make(chan error, 1)
			go func() {
				errChan <- svc.StreamTelemetry(req, stream)
			}()

			// Wait for context timeout
			select {
			case err := <-errChan:
				if err != context.DeadlineExceeded && !errors.Is(err, context.DeadlineExceeded) {
					t.Errorf("Expected context.DeadlineExceeded, got: %v", err)
				}
			case <-time.After(200 * time.Millisecond):
				t.Fatal("Streaming goroutine did not exit")
			}
		})
	}
}

func TestStreamTelemetry_ContextCancellation(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	telemetryCh := make(chan *dispatcher.DispatchedMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return telemetryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	ctx, cancel := context.WithCancel(context.Background())
	stream := &mockTelemetryStreamServer{ctx: ctx}

	req := &starv1.StreamTelemetryRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-cancel",
		},
		RateHz: 10,
	}

	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.StreamTelemetry(req, stream)
	}()

	// Wait a bit then cancel
	time.Sleep(50 * time.Millisecond)
	cancel()

	select {
	case err := <-errChan:
		if err != context.Canceled && !errors.Is(err, context.Canceled) {
			t.Errorf("Expected context.Canceled, got: %v", err)
		}
	case <-time.After(200 * time.Millisecond):
		t.Fatal("Streaming goroutine did not exit after context cancellation")
	}
}

func TestStreamTelemetry_SendError(t *testing.T) {
	testTelemetry := createTestTelemetryData()

	mockHARQ := &testutil.MockHARQ{}
	telemetryCh := make(chan *dispatcher.DispatchedMessage, 10)
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return telemetryCh
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	stream := &mockTelemetryStreamServer{
		ctx:     ctx,
		sendErr: io.ErrClosedPipe,
	}

	req := &starv1.StreamTelemetryRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-error",
		},
		RateHz: 50, // 50 Hz = 20ms interval for faster test
	}

	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.StreamTelemetry(req, stream)
	}()

	// Give stream time to start and subscribe
	time.Sleep(20 * time.Millisecond)

	// Send telemetry data multiple times to ensure it gets sent
	for i := 0; i < 5; i++ {
		telemetryCh <- &dispatcher.DispatchedMessage{
			WireMsg: &starv1.WireMessage{
				Payload: &starv1.WireMessage_TelemetryData{
					TelemetryData: testTelemetry,
				},
			},
			Metadata: nil,
		}
		time.Sleep(10 * time.Millisecond)
	}

	// Wait for error to be returned
	select {
	case err := <-errChan:
		if err == nil {
			t.Fatal("Expected error when stream.Send fails")
		}
		if !errors.Is(err, io.ErrClosedPipe) {
			t.Errorf("Expected io.ErrClosedPipe, got: %v", err)
		}
	case <-time.After(500 * time.Millisecond):
		t.Fatal("Streaming goroutine did not return error")
	}
}

func TestGetSystemStatus_Success(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return make(chan *dispatcher.DispatchedMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	req := &starv1.GetSystemStatusRequest{
		Header: &starv1.RequestHeader{
			RequestId: "test-status-1",
		},
	}

	resp, err := svc.GetSystemStatus(context.Background(), req)
	if err != nil {
		t.Fatalf("Expected no error, got: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	if resp.Header.RequestId != "test-status-1" {
		t.Errorf("Expected request_id 'test-status-1', got '%s'", resp.Header.RequestId)
	}

	if resp.Status == nil {
		t.Fatal("Expected non-nil status")
	}

	// Verify mock status fields
	if resp.Status.FirmwareVersion != "v0.1.0-dev" {
		t.Errorf("Expected firmware version 'v0.1.0-dev', got '%s'", resp.Status.FirmwareVersion)
	}
}

func TestGetSystemStatus_NilRequest(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return make(chan *dispatcher.DispatchedMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	_, err := svc.GetSystemStatus(context.Background(), nil)
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

func TestValidateRateHz(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			return make(chan *dispatcher.DispatchedMessage)
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewTelemetryService(context.Background(), mockHARQ, mockDispatcher, logger)

	testCases := []struct {
		name     string
		input    int32
		expected int32
	}{
		{"Valid rate 10 Hz", 10, 10},
		{"Valid rate 1 Hz", 1, 1},
		{"Valid rate 100 Hz", 100, 100},
		{"Valid rate 50 Hz", 50, 50},
		{"Zero defaults to 10", 0, 10},
		{"Negative defaults to 10", -5, 10},
		{"Too high defaults to 10 (150)", 150, 10},
		{"Too high defaults to 10 (101)", 101, 10},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			result := svc.validateRateHz(tc.input)
			if result != tc.expected {
				t.Errorf("Expected %d Hz, got %d Hz", tc.expected, result)
			}
		})
	}
}

func TestStreamTelemetry_MultipleClients(t *testing.T) {
	testTelemetry := createTestTelemetryData()

	mockHARQ := &testutil.MockHARQ{}

	// Track all subscriber channels for broadcasting
	var subscribersMutex sync.Mutex
	var subscribers []chan *dispatcher.DispatchedMessage

	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			// Each subscription gets its own channel
			ch := make(chan *dispatcher.DispatchedMessage, 10)
			subscribersMutex.Lock()
			subscribers = append(subscribers, ch)
			subscribersMutex.Unlock()
			return ch
		},
	}
	logger := testutil.NewDiscardLogger()

	ctx := context.Background()
	svc := NewTelemetryService(ctx, mockHARQ, mockDispatcher, logger)
	defer svc.Shutdown()

	// Create two concurrent clients
	ctx1, cancel1 := context.WithCancel(context.Background())
	defer cancel1()
	stream1 := &mockTelemetryStreamServer{ctx: ctx1}

	ctx2, cancel2 := context.WithCancel(context.Background())
	defer cancel2()
	stream2 := &mockTelemetryStreamServer{ctx: ctx2}

	req1 := &starv1.StreamTelemetryRequest{
		Header: &starv1.RequestHeader{RequestId: "client-1"},
		RateHz: 10,
	}

	req2 := &starv1.StreamTelemetryRequest{
		Header: &starv1.RequestHeader{RequestId: "client-2"},
		RateHz: 10,
	}

	errChan1 := make(chan error, 1)
	errChan2 := make(chan error, 1)

	go func() {
		errChan1 <- svc.StreamTelemetry(req1, stream1)
	}()

	go func() {
		errChan2 <- svc.StreamTelemetry(req2, stream2)
	}()

	// Give goroutines time to subscribe
	time.Sleep(50 * time.Millisecond)

	// Broadcast telemetry data to all subscribers
	msg := &dispatcher.DispatchedMessage{
		WireMsg: &starv1.WireMessage{
			Payload: &starv1.WireMessage_TelemetryData{
				TelemetryData: testTelemetry,
			},
		},
		Metadata: nil,
	}

	subscribersMutex.Lock()
	for _, ch := range subscribers {
		select {
		case ch <- msg:
		default:
		}
	}
	subscribersMutex.Unlock()

	time.Sleep(150 * time.Millisecond)

	// Cancel both contexts
	cancel1()
	cancel2()

	// Wait for both to exit
	select {
	case <-errChan1:
	case <-time.After(200 * time.Millisecond):
		t.Fatal("Client 1 did not exit")
	}

	select {
	case <-errChan2:
	case <-time.After(200 * time.Millisecond):
		t.Fatal("Client 2 did not exit")
	}

	// Close all subscriber channels to prevent goroutine leaks
	subscribersMutex.Lock()
	for _, ch := range subscribers {
		close(ch)
	}
	subscribersMutex.Unlock()

	// Both clients should have received data
	if len(stream1.sentData) == 0 {
		t.Error("Client 1 received no data")
	}
	if len(stream2.sentData) == 0 {
		t.Error("Client 2 received no data")
	}
}
