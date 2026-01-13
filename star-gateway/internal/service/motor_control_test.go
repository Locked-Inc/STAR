package service

import (
	"context"
	"errors"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// MockHARQ is a mock implementation of the harq.HARQ interface.
type MockHARQ struct {
	SendFunc        func(data []byte) error
	ReceiveFunc     func() ([]byte, error)
	GetStateFunc    func() harq.State
	GetTxSeqFunc    func() uint16
	GetRxSeqFunc    func() uint16
	ResetFunc       func()
	LastSentPayload []byte
}

func (m *MockHARQ) Send(data []byte) error {
	m.LastSentPayload = data
	if m.SendFunc != nil {
		return m.SendFunc(data)
	}
	return nil
}

func (m *MockHARQ) Receive() ([]byte, error) {
	if m.ReceiveFunc != nil {
		return m.ReceiveFunc()
	}
	return nil, errors.New("receive not implemented")
}

func (m *MockHARQ) GetState() harq.State {
	if m.GetStateFunc != nil {
		return m.GetStateFunc()
	}
	return harq.StateIdle
}

func (m *MockHARQ) GetTxSequence() uint16 {
	if m.GetTxSeqFunc != nil {
		return m.GetTxSeqFunc()
	}
	return 0
}

func (m *MockHARQ) GetRxSequence() uint16 {
	if m.GetRxSeqFunc != nil {
		return m.GetRxSeqFunc()
	}
	return 0
}

func (m *MockHARQ) Reset() {
	if m.ResetFunc != nil {
		m.ResetFunc()
	}
}

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
			mockHARQ := &MockHARQ{
				SendFunc: func(data []byte) error {
					return tc.mockSendErr
				},
			}
			svc := NewMotorControlService(mockHARQ)

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
				var sentCmd starv1.VelocityCommand
				if err := proto.Unmarshal(mockHARQ.LastSentPayload, &sentCmd); err != nil {
					t.Fatalf("Failed to unmarshal payload: %v", err)
				}
				if sentCmd.FrontLeftVelocityMps != tc.expectedVel {
					t.Errorf("Expected FL velocity %f, got %f", tc.expectedVel, sentCmd.FrontLeftVelocityMps)
				}
			}
		})
	}
}

func TestEmergencyStop(t *testing.T) {
	mockHARQ := &MockHARQ{}
	svc := NewMotorControlService(mockHARQ)

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

	// Verify that a zero velocity command was sent
	var sentCmd starv1.VelocityCommand
	if err := proto.Unmarshal(mockHARQ.LastSentPayload, &sentCmd); err != nil {
		t.Fatalf("Failed to unmarshal sent payload: %v", err)
	}

	if sentCmd.FrontLeftVelocityMps != 0 || sentCmd.FrontRightVelocityMps != 0 ||
		sentCmd.BackLeftVelocityMps != 0 || sentCmd.BackRightVelocityMps != 0 {
		t.Error("Expected all velocities to be 0 for E-Stop")
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
	marshaledData, _ := proto.Marshal(expectedData)

	callCount := 0
	mockHARQ := &MockHARQ{
		ReceiveFunc: func() ([]byte, error) {
			callCount++
			if callCount == 1 {
				// First call: return valid data
				return marshaledData, nil
			}
			// Second call: cancel context and return error to unblock waiting logic
			// The error forces the loop to check context again or sleep
			cancel()
			return nil, errors.New("end of stream")
		},
	}

	svc := NewMotorControlService(mockHARQ)
	stream := &mockStreamEncodersServer{
		ctx: ctx,
	}

	// Call StreamEncoders (this will block until context cancel)
	err := svc.StreamEncoders(&starv1.StreamEncodersRequest{}, stream)

	// Verify we exited with context canceled error
	if !errors.Is(err, context.Canceled) {
		t.Errorf("Expected context canceled error, got %v", err)
	}

	// Verify we received the data
	if len(stream.sentData) != 1 {
		t.Fatalf("Expected 1 sent data item, got %d", len(stream.sentData))
	}

	received := stream.sentData[0]
	if received.Ticks != expectedData.Ticks {
		t.Errorf("Expected ticks %d, got %d", expectedData.Ticks, received.Ticks)
	}
}
