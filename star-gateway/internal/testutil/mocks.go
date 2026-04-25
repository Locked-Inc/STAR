// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package testutil provides testing utilities and mocks for star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package testutil

import (
	"context"
	"io"
	"log/slog"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
)

const (
	// DefaultHARQTimeout is the default timeout for mock HARQ receive operations.
	DefaultHARQTimeout = 50 * time.Millisecond
)

// MockHARQ is a mock implementation of the harq.HARQ interface.
// Also implements the frameTypeSender interface (SendWithType) used by performResetHandshake.
type MockHARQ struct {
	mu               sync.Mutex
	SendFunc         func(ctx context.Context, data []byte, p ...harq.Priority) error
	SendWithTypeFunc func(ctx context.Context, data []byte, frameType frame.Type) error
	ReceiveFunc      func(ctx context.Context) (*harq.ReceiveResult, error)
	GetStateFunc     func() harq.State
	GetTxSeqFunc     func() uint16
	GetRxSeqFunc     func() uint16
	ResetFunc        func()
	LastSentPayload  []byte

	// ReceiveChan is an optional channel for simulating HARQ receive operations.
	// If set, Receive will pull from this channel instead of calling ReceiveFunc.
	ReceiveChan chan *harq.ReceiveResult
}

// Compile-time assertion that MockHARQ implements harq.HARQ.
// This ensures the mock stays in sync with the interface and catches regressions.
var _ harq.HARQ = (*MockHARQ)(nil)

func (m *MockHARQ) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	m.mu.Lock()
	// Defensive copy to prevent slice aliasing
	m.LastSentPayload = append([]byte(nil), data...)
	sendFunc := m.SendFunc
	m.mu.Unlock()

	if sendFunc != nil {
		return sendFunc(ctx, data, p...)
	}
	return nil
}

// SendWithType sends data with an explicit frame type.
// Used by performResetHandshake to send FrameTypeReset frames.
func (m *MockHARQ) SendWithType(ctx context.Context, data []byte, frameType frame.Type) error {
	m.mu.Lock()
	m.LastSentPayload = append([]byte(nil), data...)
	sendWithTypeFunc := m.SendWithTypeFunc
	m.mu.Unlock()

	if sendWithTypeFunc != nil {
		return sendWithTypeFunc(ctx, data, frameType)
	}
	// Default: delegate to Send
	return m.Send(ctx, data)
}

func (m *MockHARQ) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	if m.ReceiveFunc != nil {
		return m.ReceiveFunc(ctx)
	}
	if m.ReceiveChan != nil {
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		case result := <-m.ReceiveChan:
			return result, nil
		case <-time.After(DefaultHARQTimeout):
			return nil, harq.ErrTimeout
		}
	}
	// No func or chan configured -- block until context cancels so callers
	// (e.g. drainUntilResetAck) time out gracefully instead of tight-looping.
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
	m.mu.Lock()
	resetFunc := m.ResetFunc
	m.LastSentPayload = nil
	m.mu.Unlock()

	if resetFunc != nil {
		resetFunc()
	}
}

// GetLastSentPayload returns a copy of the last sent payload in a thread-safe manner.
func (m *MockHARQ) GetLastSentPayload() []byte {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.LastSentPayload == nil {
		return nil
	}
	result := make([]byte, len(m.LastSentPayload))
	copy(result, m.LastSentPayload)
	return result
}

// MockDispatcher is a mock implementation of the dispatcher.Dispatcher interface.
type MockDispatcher struct {
	SubscribeChan   chan *dispatcher.DispatchedMessage
	SubscribeFunc   func(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage
	UnsubscribeFunc func(msgType dispatcher.MessageType, ch <-chan *dispatcher.DispatchedMessage)
	StartFunc       func() error
	StopFunc        func() error
	GetStateFunc    func() dispatcher.State
}

// Compile-time assertion that MockDispatcher implements dispatcher.Dispatcher.
// This ensures the mock stays in sync with the interface and catches regressions.
var _ dispatcher.Dispatcher = (*MockDispatcher)(nil)

func (m *MockDispatcher) Subscribe(msgType dispatcher.MessageType) <-chan *dispatcher.DispatchedMessage {
	if m.SubscribeFunc != nil {
		return m.SubscribeFunc(msgType)
	}
	if m.SubscribeChan != nil {
		return m.SubscribeChan
	}
	// Return a channel that never sends to avoid blocking tests
	ch := make(chan *dispatcher.DispatchedMessage)
	return ch
}

func (m *MockDispatcher) Unsubscribe(msgType dispatcher.MessageType, ch <-chan *dispatcher.DispatchedMessage) {
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
