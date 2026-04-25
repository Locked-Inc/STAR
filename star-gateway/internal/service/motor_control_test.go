// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

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

func TestSetVelocity(t *testing.T) {
	tests := []struct {
		name          string
		req           *starv1.SetVelocityRequest
		mockSendErr   error
		expectedCode  codes.Code
		expectedVel   float64
		verifyPayload bool
	}{
		{
			name: "Success",
			req: &starv1.SetVelocityRequest{
				Header: &starv1.RequestHeader{RequestId: "req-1"},
				Command: &starv1.VelocityCommand{
					FrontLeftVelocityMps: 1.5,
					Sequence:             1,
				},
			},
			expectedCode:  codes.OK,
			expectedVel:   1.5,
			verifyPayload: true,
		},
		{
			name:          "NilRequest",
			req:           nil,
			expectedCode:  codes.InvalidArgument,
			verifyPayload: false,
		},
		{
			name: "NilCommand",
			req: &starv1.SetVelocityRequest{
				Header:  &starv1.RequestHeader{},
				Command: nil,
			},
			expectedCode:  codes.InvalidArgument,
			verifyPayload: false,
		},
		{
			name: "HarqSendFailure",
			req: &starv1.SetVelocityRequest{
				Command: &starv1.VelocityCommand{},
			},
			mockSendErr:   errors.New("transport error"),
			expectedCode:  codes.Unavailable,
			verifyPayload: false,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			mockHARQ := &testutil.MockHARQ{
				SendFunc: func(ctx context.Context, data []byte, _ ...harq.Priority) error {
					return tc.mockSendErr
				},
			}
			mockDispatcher := &testutil.MockDispatcher{}
			logger := testutil.NewDiscardLogger()
			svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

			resp, err := svc.SetVelocity(context.Background(), tc.req)

			if status.Code(err) != tc.expectedCode {
				t.Errorf("Expected status %v, got %v (err: %v)", tc.expectedCode, status.Code(err), err)
			}

			if tc.expectedCode == codes.OK {
				if resp == nil {
					t.Fatal("Expected response, got nil")
				}
				if resp.Header.Status != starv1.Status_STATUS_OK {
					t.Errorf("Expected header status OK, got %v", resp.Header.Status)
				}
			}

			if tc.verifyPayload {
				// Now we need to unwrap WireMessage to get VelocityCommand
				var wrapper starv1.WireMessage
				if err := proto.Unmarshal(mockHARQ.GetLastSentPayload(), &wrapper); err != nil {
					t.Fatalf("Failed to unmarshal wire message: %v", err)
				}
				sentCmd := wrapper.GetVelocityCommand()
				if sentCmd == nil {
					t.Fatal("Expected VelocityCommand in WireMessage")
				}
				if sentCmd.FrontLeftVelocityMps != tc.expectedVel {
					t.Errorf("Expected FL velocity %f, got %f", tc.expectedVel, sentCmd.FrontLeftVelocityMps)
				}
			}
		})
	}
}

func TestEmergencyStop(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()
	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	req := &starv1.EmergencyStopRequest{
		Header: &starv1.RequestHeader{RequestId: "estop-1"},
		Reason: "Test E-Stop",
	}

	resp, err := svc.EmergencyStop(context.Background(), req)
	if err != nil {
		t.Fatalf("EmergencyStop failed: %v", err)
	}

	if !resp.EstopEngaged {
		t.Error("Expected EstopEngaged to be true")
	}

	// Verify that EmergencyStopCommand was sent (wrapped in WireMessage)
	var wrapper starv1.WireMessage
	if err := proto.Unmarshal(mockHARQ.GetLastSentPayload(), &wrapper); err != nil {
		t.Fatalf("Failed to unmarshal wire message: %v", err)
	}

	estopCmd := wrapper.GetEmergencyStopCommand()
	if estopCmd == nil {
		t.Fatal("Expected EmergencyStopCommand in WireMessage")
	}

	if estopCmd.Reason != req.Reason {
		t.Errorf("Expected reason %q, got %q", req.Reason, estopCmd.Reason)
	}

	if !estopCmd.EngageHardwareStop {
		t.Error("Expected EngageHardwareStop to be true")
	}
}

// Mock stream for StreamEncoders
type mockStreamEncodersServer struct {
	grpc.ServerStream
	ctx       context.Context
	sentData  []*starv1.EncoderData
	sendError error
}

func (m *mockStreamEncodersServer) Context() context.Context {
	return m.ctx
}

func (m *mockStreamEncodersServer) Send(data *starv1.EncoderData) error {
	if m.sendError != nil {
		return m.sendError
	}
	m.sentData = append(m.sentData, data)
	return nil
}

func TestStreamEncoders(t *testing.T) {
	// Create a context that we can cancel to stop the stream loop
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Prepare sample encoder data
	expectedData := &starv1.EncoderData{
		MotorId:     0,
		Ticks:       100,
		VelocityMps: 0.5,
		TimestampUs: 123456,
	}

	// Create TelemetryData containing the encoder data
	// (The updated implementation expects TelemetryData messages from RX72N)
	telemetry := &starv1.TelemetryData{
		EncoderFrontLeft: expectedData,
	}

	// Wrap in WireMessage (as Dispatcher would do)
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: telemetry,
		},
	}

	// Create channel to send telemetry messages
	// telemetryChan := make(chan *dispatcher.DispatchedMessage, 10)

	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			ch := make(chan *dispatcher.DispatchedMessage, 10)
			go func() {
				for i := 0; i < 5; i++ {
					select {
					case ch <- &dispatcher.DispatchedMessage{WireMsg: wireMsg}:
					case <-ctx.Done():
						return
					}
					time.Sleep(5 * time.Millisecond)
				}
				close(ch)
			}()
			return ch
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)
	stream := &mockStreamEncodersServer{
		ctx: ctx,
	}

	// Start StreamEncoders in a goroutine (it's a blocking call)
	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.StreamEncoders(&starv1.StreamEncodersRequest{RateHz: 100}, stream) // Use high rate for fast test
	}()

	// Wait for data to be sent (ticker fires every 10ms at 100 Hz)
	time.Sleep(50 * time.Millisecond)

	// Cancel context to stop streaming
	cancel()

	// Wait for goroutine to exit
	select {
	case err := <-errChan:
		// Verify we exited with context canceled error
		if !errors.Is(err, context.Canceled) {
			t.Errorf("Expected context canceled error, got %v", err)
		}
	case <-time.After(100 * time.Millisecond):
		t.Fatal("StreamEncoders goroutine did not exit")
	}

	// Verify we received the data
	// With rate limiting, we should get at least 1 data item
	if len(stream.sentData) < 1 {
		t.Fatalf("Expected at least 1 sent data item, got %d", len(stream.sentData))
	}

	received := stream.sentData[0]
	if received.Ticks != expectedData.Ticks {
		t.Errorf("Expected ticks %d, got %d", expectedData.Ticks, received.Ticks)
	}
}

// Mock server stream for ControlStream
type mockControlStreamServer struct {
	grpc.ServerStream
	ctx       context.Context
	recvChan  chan *starv1.VelocityCommand
	sentData  []*starv1.EncoderData
	recvError error
	sendError error
	mu        sync.Mutex
}

func (m *mockControlStreamServer) Context() context.Context {
	return m.ctx
}

func (m *mockControlStreamServer) Recv() (*starv1.VelocityCommand, error) {
	if m.recvError != nil {
		return nil, m.recvError
	}
	select {
	case cmd := <-m.recvChan:
		return cmd, nil
	case <-m.ctx.Done():
		return nil, m.ctx.Err()
	}
}

func (m *mockControlStreamServer) Send(data *starv1.EncoderData) error {
	if m.sendError != nil {
		return m.sendError
	}
	m.mu.Lock()
	m.sentData = append(m.sentData, data)
	m.mu.Unlock()
	return nil
}

func TestControlStream_GoroutineCleanup(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Mock HARQ that returns telemetry data
	telemetry := &starv1.TelemetryData{
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       100,
			VelocityMps: 0.5,
			TimestampUs: 123456,
		},
	}

	// Wrap in WireMessage
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: telemetry,
		},
	}

	// Create channel to send telemetry messages
	telemetryChan := make(chan *dispatcher.DispatchedMessage, 10)

	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			// Send telemetry data continuously
			go func() {
				ticker := time.NewTicker(10 * time.Millisecond)
				defer ticker.Stop()
				for {
					select {
					case <-ctx.Done():
						return
					case <-ticker.C:
						select {
						case telemetryChan <- &dispatcher.DispatchedMessage{WireMsg: wireMsg}:
						case <-ctx.Done():
							return
						}
					}
				}
			}()
			return telemetryChan
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	// Create mock stream with command channel
	recvChan := make(chan *starv1.VelocityCommand)
	stream := &mockControlStreamServer{
		ctx:      ctx,
		recvChan: recvChan,
	}

	// Start ControlStream in goroutine
	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.ControlStream(stream)
	}()

	// Send a command
	recvChan <- &starv1.VelocityCommand{
		FrontLeftVelocityMps: 1.0,
	}

	// Wait for command to be processed
	time.Sleep(50 * time.Millisecond)

	// Cancel context to trigger cleanup
	cancel()

	// Verify goroutine exits cleanly
	select {
	case err := <-errChan:
		if !errors.Is(err, context.Canceled) {
			t.Errorf("Expected context.Canceled, got %v", err)
		}
	case <-time.After(100 * time.Millisecond):
		t.Fatal("ControlStream goroutine did not exit after context cancel")
	}

	// Verify we received encoder data
	stream.mu.Lock()
	dataCount := len(stream.sentData)
	stream.mu.Unlock()

	if dataCount == 0 {
		t.Error("Expected to receive encoder data")
	}
}

func TestControlStream_BasicFlow(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()

	// Mock HARQ that returns telemetry data
	telemetry := &starv1.TelemetryData{
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       100,
			VelocityMps: 0.5,
			TimestampUs: 123456,
		},
	}

	// Wrap in WireMessage
	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: telemetry,
		},
	}

	// Create channel to send telemetry messages
	telemetryChan := make(chan *dispatcher.DispatchedMessage, 10)

	var sendCalls int
	var mu sync.Mutex

	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, _ ...harq.Priority) error {
			mu.Lock()
			sendCalls++
			mu.Unlock()
			return nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{
		SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
			// Send telemetry data continuously
			go func() {
				ticker := time.NewTicker(10 * time.Millisecond)
				defer ticker.Stop()
				for {
					select {
					case <-ctx.Done():
						return
					case <-ticker.C:
						select {
						case telemetryChan <- &dispatcher.DispatchedMessage{WireMsg: wireMsg}:
						case <-ctx.Done():
							return
						}
					}
				}
			}()
			return telemetryChan
		},
	}
	logger := testutil.NewDiscardLogger()

	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	// Create mock stream with command channel
	recvChan := make(chan *starv1.VelocityCommand, 10)
	stream := &mockControlStreamServer{
		ctx:      ctx,
		recvChan: recvChan,
	}

	// Send velocity commands
	recvChan <- &starv1.VelocityCommand{
		FrontLeftVelocityMps: 1.0,
	}
	recvChan <- &starv1.VelocityCommand{
		FrontRightVelocityMps: 0.5,
	}

	// Start ControlStream
	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.ControlStream(stream)
	}()

	// Wait for timeout
	<-ctx.Done()

	// Wait for goroutine
	select {
	case <-errChan:
		// Expected timeout
	case <-time.After(100 * time.Millisecond):
		t.Fatal("ControlStream goroutine did not exit")
	}

	// Verify commands were sent
	mu.Lock()
	calls := sendCalls
	mu.Unlock()

	if calls < 2 {
		t.Errorf("Expected at least 2 Send calls, got %d", calls)
	}

	// Verify encoder data was received
	stream.mu.Lock()
	dataCount := len(stream.sentData)
	stream.mu.Unlock()

	if dataCount == 0 {
		t.Error("Expected to receive encoder data")
	}
}

func TestControlStream_HarqSendError(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()

	// Mock HARQ that fails to send
	mockHARQ := &testutil.MockHARQ{
		SendFunc: func(ctx context.Context, data []byte, _ ...harq.Priority) error {
			return errors.New("send failed")
		},
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			// Return error to avoid infinite loop
			time.Sleep(10 * time.Millisecond)
			return nil, errors.New("receive error")
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	recvChan := make(chan *starv1.VelocityCommand, 1)
	stream := &mockControlStreamServer{
		ctx:      ctx,
		recvChan: recvChan,
	}

	// Send command that will fail
	recvChan <- &starv1.VelocityCommand{
		FrontLeftVelocityMps: 1.0,
	}

	// Start ControlStream
	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.ControlStream(stream)
	}()

	// Wait for timeout (should handle error gracefully)
	<-ctx.Done()

	select {
	case <-errChan:
		// Expected - service should exit on context done
	case <-time.After(100 * time.Millisecond):
		t.Fatal("ControlStream did not exit after timeout")
	}
}

func TestControlStream_ClientSendError(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	telemetry := &starv1.TelemetryData{
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId: 0,
			Ticks:   100,
		},
	}
	marshaledData, _ := proto.Marshal(telemetry)

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: marshaledData, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	// Create stream that will return error on Recv
	recvChan := make(chan *starv1.VelocityCommand)
	stream := &mockControlStreamServer{
		ctx:       ctx,
		recvChan:  recvChan,
		recvError: errors.New("client recv error"),
	}

	errChan := make(chan error, 1)
	go func() {
		errChan <- svc.ControlStream(stream)
	}()

	// Should exit with error from Recv
	select {
	case err := <-errChan:
		if err == nil {
			t.Error("Expected error from client recv")
		}
	case <-time.After(100 * time.Millisecond):
		t.Fatal("ControlStream did not exit on recv error")
	}
}

func TestStreamEncoders_Concurrent(t *testing.T) {
	// Test multiple concurrent StreamEncoders clients
	telemetry := &starv1.TelemetryData{
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       100,
			VelocityMps: 0.5,
			TimestampUs: 123456,
		},
	}
	marshaledData, _ := proto.Marshal(telemetry)

	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: marshaledData, Metadata: harq.FrameMetadata{Sequence: 0}}, nil
		},
	}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()

	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	// Launch 5 concurrent clients
	var wg sync.WaitGroup
	for i := 0; i < 5; i++ {
		wg.Add(1)
		go func(clientID int) {
			defer wg.Done()

			ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
			defer cancel()

			stream := &mockStreamEncodersServer{ctx: ctx}
			err := svc.StreamEncoders(&starv1.StreamEncodersRequest{RateHz: 10}, stream)

			if err != nil && !errors.Is(err, context.DeadlineExceeded) {
				t.Errorf("Client %d: unexpected error: %v", clientID, err)
			}
		}(i)
	}

	wg.Wait()
}

func TestSetMotorPower(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()
	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	req := &starv1.SetMotorPowerRequest{}
	resp, err := svc.SetMotorPower(context.Background(), req)

	if status.Code(err) != codes.Unimplemented {
		t.Errorf("Expected Unimplemented, got %v", status.Code(err))
	}
	if resp != nil {
		t.Error("Expected nil response")
	}
}

func TestMotorControl_ValidateRateHz(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{}
	mockDispatcher := &testutil.MockDispatcher{}
	logger := testutil.NewDiscardLogger()
	svc := NewMotorControlService(mockHARQ, mockDispatcher, logger)

	// Valid rate
	rate := svc.validateRateHz(50)
	if rate != 50 {
		t.Errorf("Expected 50, got %v", rate)
	}

	// Rate too low (should return default 10)
	rate = svc.validateRateHz(0)
	if rate != 10 {
		t.Errorf("Expected 10, got %v", rate)
	}

	// Rate too high (should return default 10)
	rate = svc.validateRateHz(500)
	if rate != 10 {
		t.Errorf("Expected 10, got %v", rate)
	}
}
