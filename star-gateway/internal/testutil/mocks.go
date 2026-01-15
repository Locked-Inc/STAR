// Package testutil provides testing utilities and mocks for star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package testutil

import (
	"context"
	"errors"
	"io"
	"log/slog"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
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

// MockDispatcher is a mock implementation of the dispatcher.Dispatcher interface.
type MockDispatcher struct {
	SubscribeChan   chan *starv1.WireMessage
	SubscribeFunc   func(msgType dispatcher.MessageType) <-chan *starv1.WireMessage
	UnsubscribeFunc func(msgType dispatcher.MessageType, ch <-chan *starv1.WireMessage)
	StartFunc       func() error
	StopFunc        func() error
	GetStateFunc    func() dispatcher.State
}

func (m *MockDispatcher) Subscribe(msgType dispatcher.MessageType) <-chan *starv1.WireMessage {
	if m.SubscribeFunc != nil {
		return m.SubscribeFunc(msgType)
	}
	if m.SubscribeChan != nil {
		return m.SubscribeChan
	}
	// Return a channel that never sends to avoid blocking tests
	ch := make(chan *starv1.WireMessage)
	return ch
}

func (m *MockDispatcher) Unsubscribe(msgType dispatcher.MessageType, ch <-chan *starv1.WireMessage) {
	if m.UnsubscribeFunc != nil {
		m.UnsubscribeFunc(msgType, ch)
	}
}

func (m *MockDispatcher) Start(ctx context.Context) error {
	if m.StartFunc != nil {
		return m.StartFunc()
	}
	return nil
}

func (m *MockDispatcher) Stop() error {
	if m.StopFunc != nil {
		return m.StopFunc()
	}
	return nil // Return nil by default
}

func (m *MockDispatcher) GetState() dispatcher.State {
	if m.GetStateFunc != nil {
		return m.GetStateFunc()
	}
	return dispatcher.StateIdle
}

// NewDiscardLogger creates a logger that discards all output (for testing).
func NewDiscardLogger() *slog.Logger {
	// Use io.Discard to prevent nil pointer dereference
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}
