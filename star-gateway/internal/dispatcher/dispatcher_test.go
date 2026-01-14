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

// MockHARQ is a mock implementation of the harq.HARQ interface for testing.
type MockHARQ struct {
	ReceiveFunc  func() ([]byte, error)
	SendFunc     func(data []byte) error
	GetStateFunc func() harq.State
	GetTxSeqFunc func() uint16
	GetRxSeqFunc func() uint16
	ResetFunc    func()
	receiveChan  chan []byte
	LastSentData []byte
}

func (m *MockHARQ) Send(data []byte) error {
	m.LastSentData = data
	if m.SendFunc != nil {
		return m.SendFunc(data)
	}
	return nil
}

func (m *MockHARQ) Receive() ([]byte, error) {
	if m.ReceiveFunc != nil {
		return m.ReceiveFunc()
	}
	if m.receiveChan != nil {
		select {
		case data := <-m.receiveChan:
			return data, nil
		case <-time.After(50 * time.Millisecond):
			return nil, harq.ErrTimeout
		}
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
		BatteryPercent: 85.0,
		TimestampUs:    time.Now().UnixMicro(),
	}

	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{TelemetryData: telemetry},
	}

	data, _ := proto.Marshal(wrapper)

	// Create mock HARQ that returns the telemetry message
	receiveChan := make(chan []byte, 1)
	receiveChan <- data

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
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()

	if err := d.Start(ctx); err != nil {
		t.Fatalf("Failed to start dispatcher: %v", err)
	}

	// Wait for message
	select {
	case msg := <-telemetryCh:
		if msg == nil {
			t.Fatal("Received nil message")
		}
		receivedTelem := msg.GetTelemetryData()
		if receivedTelem == nil {
			t.Fatal("Expected TelemetryData, got nil")
		}
		if receivedTelem.BatteryPercent != 85.0 {
			t.Errorf("Expected battery 85%%, got %.1f%%", receivedTelem.BatteryPercent)
		}
	case <-time.After(150 * time.Millisecond):
		t.Fatal("Timeout waiting for message")
	}

	// Stop dispatcher
	d.Stop()

	if d.GetState() != StateStopped {
		t.Errorf("Expected state Stopped, got %v", d.GetState())
	}
}

func TestDispatcherMultipleSubscribers(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))

	telemetry := &starv1.TelemetryData{BatteryPercent: 75.0}
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{TelemetryData: telemetry},
	}
	data, _ := proto.Marshal(wrapper)

	receiveChan := make(chan []byte, 1)
	receiveChan <- data

	mockHARQ := &MockHARQ{receiveChan: receiveChan}

	d, _ := NewDispatcher(mockHARQ, logger, nil)

	// Subscribe two clients
	ch1 := d.Subscribe(MessageTypeTelemetryData)
	ch2 := d.Subscribe(MessageTypeTelemetryData)

	ctx, cancel := context.WithTimeout(context.Background(), 150*time.Millisecond)
	defer cancel()

	d.Start(ctx)

	// Both should receive
	receivedCount := 0
	timeout := time.After(100 * time.Millisecond)

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

	d.Stop()
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
		{"BatteryStatus", MessageTypeBatteryStatus, &starv1.BatteryStatus{}},
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
			case MessageTypeBatteryStatus:
				wrapper = &starv1.WireMessage{Payload: &starv1.WireMessage_BatteryStatus{BatteryStatus: tc.payload.(*starv1.BatteryStatus)}}
			}

			data, _ := proto.Marshal(wrapper)
			mockHARQ := &MockHARQ{receiveChan: make(chan []byte, 1)}
			mockHARQ.receiveChan <- data

			d, err := NewDispatcher(mockHARQ, logger, nil)
			if err != nil {
				t.Fatalf("Failed to create dispatcher: %v", err)
			}

			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()

			if err := d.Start(ctx); err != nil {
				t.Fatalf("Failed to start dispatcher: %v", err)
			}
			defer d.Stop()

			ch := d.Subscribe(tc.msgType)
			defer d.Unsubscribe(tc.msgType, ch)

			select {
			case msg := <-ch:
				if msg == nil {
					t.Error("Received nil message")
				}
			case <-time.After(100 * time.Millisecond):
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
			ReceiveFunc: func() ([]byte, error) {
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

		d.Start(ctx)
		time.Sleep(50 * time.Millisecond)
		cancel()
		d.Stop()

		if callCount < 2 {
			t.Error("Expected HARQ to be reset and retried")
		}
	})

	t.Run("Invalid WireMessage", func(t *testing.T) {
		mockHARQ := &MockHARQ{receiveChan: make(chan []byte, 1)}
		mockHARQ.receiveChan <- []byte{0xFF, 0xFF, 0xFF} // Invalid protobuf

		d, _ := NewDispatcher(mockHARQ, logger, nil)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		d.Start(ctx)
		time.Sleep(50 * time.Millisecond)
		cancel()
		d.Stop()
	})

	t.Run("Empty WireMessage", func(t *testing.T) {
		wrapper := &starv1.WireMessage{} // No payload set
		data, _ := proto.Marshal(wrapper)

		mockHARQ := &MockHARQ{receiveChan: make(chan []byte, 1)}
		mockHARQ.receiveChan <- data

		d, _ := NewDispatcher(mockHARQ, logger, nil)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		d.Start(ctx)
		time.Sleep(50 * time.Millisecond)
		cancel()
		d.Stop()
	})
}

// TestDispatcherChannelBackpressure tests channel full scenario.
func TestDispatcherChannelBackpressure(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))

	telemetry := &starv1.TelemetryData{}
	wrapper := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{TelemetryData: telemetry},
	}
	data, _ := proto.Marshal(wrapper)

	// Send 20 messages rapidly to fill the 10-capacity channel
	mockHARQ := &MockHARQ{receiveChan: make(chan []byte, 20)}
	for i := 0; i < 20; i++ {
		mockHARQ.receiveChan <- data
	}

	d, _ := NewDispatcher(mockHARQ, logger, nil)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	d.Start(ctx)
	defer d.Stop()

	ch := d.Subscribe(MessageTypeTelemetryData)
	defer d.Unsubscribe(MessageTypeTelemetryData, ch)

	// Let dispatcher fill the channel
	time.Sleep(100 * time.Millisecond)

	// Drain some messages
	received := 0
	for i := 0; i < 15; i++ {
		select {
		case <-ch:
			received++
		case <-time.After(10 * time.Millisecond):
			break
		}
	}

	if received == 0 {
		t.Error("Expected to receive messages despite backpressure")
	}
}
