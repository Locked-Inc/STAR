// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package dispatcher

import (
	"context"
	"errors"
	"log/slog"
	"os"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// Test timeout constants.
const (
	testHARQTimeout      = 50 * time.Millisecond
	testShortTimeout     = 100 * time.Millisecond
	testMediumTimeout    = 150 * time.Millisecond
	testLongTimeout      = 200 * time.Millisecond
	testDrainTimeout     = 10 * time.Millisecond
	testMockReceivePoll  = 10 * time.Millisecond
	testCancelWait       = 20 * time.Millisecond
	testBackpressureMsgs = 20
)

// MockHARQ is a mock implementation of the harq.HARQ interface for testing.
// Note: Kept local to avoid import cycle with testutil (which imports dispatcher for MockDispatcher).
type MockHARQ struct {
	ReceiveFunc  func(ctx context.Context) (*harq.ReceiveResult, error)
	SendFunc     func(ctx context.Context, data []byte, p ...harq.Priority) error
	GetStateFunc func() harq.State
	GetTxSeqFunc func() uint16
	GetRxSeqFunc func() uint16
	ResetFunc    func()
	receiveChan  chan *harq.ReceiveResult
	LastSentData []byte
}

// Compile-time assertion that MockHARQ implements harq.HARQ.
// This ensures the mock stays in sync with the interface and catches regressions.
var _ harq.HARQ = (*MockHARQ)(nil)

func (m *MockHARQ) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	m.LastSentData = data
	if m.SendFunc != nil {
		return m.SendFunc(ctx, data, p...)
	}
	return nil
}

func (m *MockHARQ) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	if m.ReceiveFunc != nil {
		return m.ReceiveFunc(ctx)
	}
	if m.receiveChan != nil {
		// Use a very short timeout to prevent blocking during shutdown
		ticker := time.NewTimer(testMockReceivePoll)
		defer ticker.Stop()

		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		case result, ok := <-m.receiveChan:
			if !ok {
				// Channel closed - return timeout
				return nil, harq.ErrTimeout
			}
			return result, nil
		case <-ticker.C:
			// Quick timeout allows dispatcher to check context
			return nil, harq.ErrTimeout
		}
	}
	// No func or chan configured -- block until context cancels.
	<-ctx.Done()
	return nil, ctx.Err()
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

func TestNewDispatcher(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stderr, nil))
	mockHARQ := &MockHARQ{}

	tests := []struct {
		name      string
		harq      harq.HARQ
		logger    *slog.Logger
		expectErr error
	}{
		{
			name:      "Success",
			harq:      mockHARQ,
			logger:    logger,
			expectErr: nil,
		},
		{
			name:      "NilHARQ",
			harq:      nil,
			logger:    logger,
			expectErr: ErrHARQNil,
		},
		{
			name:      "NilLogger",
			harq:      mockHARQ,
			logger:    nil,
			expectErr: ErrLoggerNil,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			d, err := NewDispatcher(tc.harq, tc.logger, nil)
			if !errors.Is(err, tc.expectErr) {
				t.Errorf("Expected error %v, got %v", tc.expectErr, err)
			}
			if tc.expectErr == nil && d == nil {
				t.Error("Expected dispatcher, got nil")
			}
		})
	}
}

func TestDispatcherSubscribeUnsubscribe(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stderr, nil))
	mockHARQ := &MockHARQ{}

	d, err := NewDispatcher(mockHARQ, logger, nil)
	if err != nil {
		t.Fatalf("Failed to create dispatcher: %v", err)
	}

	// Subscribe
	ch := d.Subscribe(MessageTypeTelemetryData)
	if ch == nil {
		t.Fatal("Expected channel, got nil")
	}

	// Unsubscribe
	d.Unsubscribe(MessageTypeTelemetryData, ch)

	// Verify channel is closed
	_, ok := <-ch
	if ok {
		t.Error("Expected channel to be closed")
	}
}

func TestDispatcherBasicRouting(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))

	// Create telemetry message
	telemetry := &starv1.TelemetryData{
		TimestampUs: time.Now().UnixMicro(),
	}

	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{TelemetryData: telemetry},
	}

	data, err := proto.Marshal(wrapper)
	if err != nil {
		t.Fatalf("Failed to marshal test data: %v", err)
	}

	// Create mock HARQ that returns the telemetry message
	receiveChan := make(chan *harq.ReceiveResult, 1)
	receiveChan <- &harq.ReceiveResult{
		Payload: data,
		Metadata: harq.FrameMetadata{
			Sequence:   0,
			FECDecoded: false,
		},
	}

	mockHARQ := &MockHARQ{
		receiveChan: receiveChan,
	}

	d, err := NewDispatcher(mockHARQ, logger, nil)
	if err != nil {
		t.Fatalf("Failed to create dispatcher: %v", err)
	}

	// Subscribe to telemetry
	telemetryCh := d.Subscribe(MessageTypeTelemetryData)

	// Start dispatcher
	ctx, cancel := context.WithTimeout(context.Background(), testLongTimeout)
	defer cancel()

	if err := d.Start(ctx); err != nil {
		t.Fatalf("Failed to start dispatcher: %v", err)
	}

	// Wait for message
	select {
	case msg := <-telemetryCh:
		if msg == nil || msg.WireMsg == nil {
			t.Fatal("Received nil message")
		}
		receivedTelem := msg.WireMsg.GetTelemetryData()
		if receivedTelem == nil {
			t.Fatal("Expected TelemetryData, got nil")
		}
	case <-time.After(testMediumTimeout):
		t.Fatal("Timeout waiting for message")
	}

	// Stop dispatcher
	if err := d.Stop(); err != nil {
		t.Errorf("Failed to stop dispatcher: %v", err)
	}

	if d.GetState() != StateStopped {
		t.Errorf("Expected state Stopped, got %v", d.GetState())
	}
}

func TestDispatcherMultipleSubscribers(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))

	telemetry := &starv1.TelemetryData{}
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{TelemetryData: telemetry},
	}
	data, err := proto.Marshal(wrapper)
	if err != nil {
		t.Fatalf("Failed to marshal test data: %v", err)
	}

	receiveChan := make(chan *harq.ReceiveResult, 1)
	receiveChan <- &harq.ReceiveResult{
		Payload:  data,
		Metadata: harq.FrameMetadata{Sequence: 0},
	}

	mockHARQ := &MockHARQ{receiveChan: receiveChan}

	d, _ := NewDispatcher(mockHARQ, logger, nil)

	// Subscribe two clients
	ch1 := d.Subscribe(MessageTypeTelemetryData)
	ch2 := d.Subscribe(MessageTypeTelemetryData)

	ctx, cancel := context.WithTimeout(context.Background(), testMediumTimeout)
	defer cancel()

	if err := d.Start(ctx); err != nil {
		t.Fatalf("Failed to start dispatcher: %v", err)
	}

	// Both should receive
	receivedCount := 0
	timeout := time.After(testShortTimeout)

	for receivedCount < 2 {
		select {
		case <-ch1:
			receivedCount++
		case <-ch2:
			receivedCount++
		case <-timeout:
			t.Fatalf("Timeout, only received %d/2 messages", receivedCount)
		}
	}

	if err := d.Stop(); err != nil {
		t.Errorf("Failed to stop dispatcher: %v", err)
	}
}

// TestDispatcherAllMessageTypes tests message routing for all message types.
func TestDispatcherAllMessageTypes(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))

	// Test each message type
	testCases := []struct {
		name    string
		msgType MessageType
		payload proto.Message
	}{
		{"VelocityCommand", MessageTypeVelocityCommand, &starv1.VelocityCommand{FrontLeftVelocityMps: 1.0}},
		{"EmergencyStopCommand", MessageTypeEmergencyStopCommand, &starv1.EmergencyStopCommand{Reason: "test"}},
		{"TelemetryData", MessageTypeTelemetryData, &starv1.TelemetryData{}},
		{"EncoderData", MessageTypeEncoderData, &starv1.EncoderData{MotorId: 0}},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			// Create WireMessage with correct oneof field
			var wrapper *starv1.WireMessage
			switch tc.msgType {
			case MessageTypeVelocityCommand:
				wrapper = &starv1.WireMessage{Payload: &starv1.WireMessage_VelocityCommand{VelocityCommand: tc.payload.(*starv1.VelocityCommand)}}
			case MessageTypeEmergencyStopCommand:
				wrapper = &starv1.WireMessage{Payload: &starv1.WireMessage_EmergencyStopCommand{EmergencyStopCommand: tc.payload.(*starv1.EmergencyStopCommand)}}
			case MessageTypeTelemetryData:
				wrapper = &starv1.WireMessage{Payload: &starv1.WireMessage_TelemetryData{TelemetryData: tc.payload.(*starv1.TelemetryData)}}
			case MessageTypeEncoderData:
				wrapper = &starv1.WireMessage{Payload: &starv1.WireMessage_EncoderData{EncoderData: tc.payload.(*starv1.EncoderData)}}
			}

			data, err := proto.Marshal(wrapper)
			if err != nil {
				t.Fatalf("Failed to marshal test data: %v", err)
			}
			mockHARQ := &MockHARQ{receiveChan: make(chan *harq.ReceiveResult, 1)}
			mockHARQ.receiveChan <- &harq.ReceiveResult{
				Payload:  data,
				Metadata: harq.FrameMetadata{Sequence: 0},
			}

			d, err := NewDispatcher(mockHARQ, logger, nil)
			if err != nil {
				t.Fatalf("Failed to create dispatcher: %v", err)
			}

			// Subscribe BEFORE starting to avoid race condition
			ch := d.Subscribe(tc.msgType)
			defer d.Unsubscribe(tc.msgType, ch)

			ctx, cancel := context.WithCancel(context.Background())
			// Don't defer cancel here, handle in cleanup

			if err := d.Start(ctx); err != nil {
				t.Fatalf("Failed to start dispatcher: %v", err)
			}
			defer func() {
				cancel()                   // Cancel FIRST
				time.Sleep(testCancelWait) // Give dispatcher time to see cancellation
				if err := d.Stop(); err != nil {
					t.Errorf("Failed to stop dispatcher: %v", err)
				}
			}()

			select {
			case msg := <-ch:
				if msg == nil {
					t.Error("Received nil message")
				}
			case <-time.After(testShortTimeout):
				t.Error("Timeout waiting for message")
			}
		})
	}
}

// TestDispatcherErrorHandling tests error handling in receive loop.
func TestDispatcherErrorHandling(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))

	t.Run("HARQ Error State", func(t *testing.T) {
		callCount := 0
		mockHARQ := &MockHARQ{
			ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
				callCount++
				if callCount == 1 {
					return nil, harq.ErrInErrorState
				}
				return nil, harq.ErrTimeout
			},
			ResetFunc: func() {},
		}

		d, _ := NewDispatcher(mockHARQ, logger, nil)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		if err := d.Start(ctx); err != nil {
			t.Fatalf("Failed to start dispatcher: %v", err)
		}
		time.Sleep(testHARQTimeout)
		cancel()
		if err := d.Stop(); err != nil {
			t.Errorf("Failed to stop dispatcher: %v", err)
		}

		if callCount < 2 {
			t.Error("Expected HARQ to be reset and retried")
		}
	})

	t.Run("Invalid WireMessage", func(t *testing.T) {
		mockHARQ := &MockHARQ{receiveChan: make(chan *harq.ReceiveResult, 1)}
		mockHARQ.receiveChan <- &harq.ReceiveResult{
			Payload:  []byte{0xFF, 0xFF, 0xFF}, // Invalid protobuf
			Metadata: harq.FrameMetadata{Sequence: 0},
		}

		d, _ := NewDispatcher(mockHARQ, logger, nil)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		if err := d.Start(ctx); err != nil {
			t.Fatalf("Failed to start dispatcher: %v", err)
		}
		time.Sleep(testHARQTimeout)
		cancel()
		if err := d.Stop(); err != nil {
			t.Errorf("Failed to stop dispatcher: %v", err)
		}
	})

	t.Run("Empty WireMessage", func(t *testing.T) {
		wrapper := &starv1.WireMessage{} // No payload set
		data, err := proto.Marshal(wrapper)
		if err != nil {
			t.Fatalf("Failed to marshal test data: %v", err)
		}

		mockHARQ := &MockHARQ{receiveChan: make(chan *harq.ReceiveResult, 1)}
		mockHARQ.receiveChan <- &harq.ReceiveResult{
			Payload:  data,
			Metadata: harq.FrameMetadata{Sequence: 0},
		}

		d, _ := NewDispatcher(mockHARQ, logger, nil)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		if err := d.Start(ctx); err != nil {
			t.Fatalf("Failed to start dispatcher: %v", err)
		}
		time.Sleep(testHARQTimeout)
		cancel()
		if err := d.Stop(); err != nil {
			t.Errorf("Failed to stop dispatcher: %v", err)
		}
	})
}

// TestDispatcherChannelBackpressure tests channel full scenario.
func TestDispatcherChannelBackpressure(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))

	telemetry := &starv1.TelemetryData{}
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{TelemetryData: telemetry},
	}
	data, err := proto.Marshal(wrapper)
	if err != nil {
		t.Fatalf("Failed to marshal test data: %v", err)
	}

	// Send testBackpressureMsgs messages rapidly to fill the DefaultSubscriberBufferSize channel
	mockHARQ := &MockHARQ{receiveChan: make(chan *harq.ReceiveResult, testBackpressureMsgs)}
	for i := 0; i < testBackpressureMsgs; i++ {
		mockHARQ.receiveChan <- &harq.ReceiveResult{
			Payload:  data,
			Metadata: harq.FrameMetadata{Sequence: uint16(i)},
		}
	}

	d, _ := NewDispatcher(mockHARQ, logger, nil)
	ctx, cancel := context.WithCancel(context.Background())
	// Don't defer cancel here, handle in cleanup

	if err := d.Start(ctx); err != nil {
		t.Fatalf("Failed to start dispatcher: %v", err)
	}
	defer func() {
		cancel()                   // Cancel FIRST
		time.Sleep(testCancelWait) // Give dispatcher time to see cancellation
		if err := d.Stop(); err != nil {
			t.Errorf("Failed to stop dispatcher: %v", err)
		}
	}()

	ch := d.Subscribe(MessageTypeTelemetryData)
	defer d.Unsubscribe(MessageTypeTelemetryData, ch)

	// Let dispatcher fill the channel
	time.Sleep(testShortTimeout)

	// Drain some messages
	received := 0
	for i := 0; i < 15; i++ {
		select {
		case <-ch:
			received++
		case <-time.After(testDrainTimeout):
			// Timeout is expected when queue is drained
		}
	}

	if received == 0 {
		t.Error("Expected to receive messages despite backpressure")
	}
}
