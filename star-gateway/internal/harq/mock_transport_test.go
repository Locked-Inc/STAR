// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package harq provides a mock transport for testing HARQ operations.
//
// STAR Project - Texas A&M University
// December 2025
package harq

import (
	"context"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// MockTransport implements transport.Device for testing.
type MockTransport struct {
	mu           sync.Mutex
	sendData     [][]byte      // Data sent via Send()
	receiveQueue [][]byte      // Data to return from Receive()
	sendErr      error         // Error to return from Send()
	sendErrAfter int           // Fail on this call count (1-based)
	receiveErr   error         // Error to return from Receive()
	receiveDelay time.Duration // Delay before Receive returns
	isOpen       bool
	readDeadline time.Time
}

// Verify MockTransport implements transport.Device.
var _ transport.Device = (*MockTransport)(nil)

// NewMockTransport creates a new MockTransport for testing.
func NewMockTransport() *MockTransport {
	return &MockTransport{
		sendData:     make([][]byte, 0),
		receiveQueue: make([][]byte, 0),
		isOpen:       true,
	}
}

// MockEncoder implements frame.Encoder for testing.
type MockEncoder struct {
	mu       sync.Mutex
	encodeFn func(frame *frame.Frame) ([]byte, error)
	calls    []*frame.Frame
}

// Verify MockEncoder implements frame.Encoder.
var _ frame.Encoder = (*MockEncoder)(nil)

func (m *MockEncoder) Encode(f *frame.Frame) ([]byte, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.calls = append(m.calls, f)
	if m.encodeFn != nil {
		return m.encodeFn(f)
	}
	// Default behavior: return empty bytes
	return []byte{}, nil
}

// MockDecoder implements frame.Decoder for testing.
type MockDecoder struct {
	mu       sync.Mutex
	decodeFn func(data []byte) (*frame.Frame, error)
	calls    [][]byte
}

// Verify MockDecoder implements frame.Decoder.
var _ frame.Decoder = (*MockDecoder)(nil)

func (m *MockDecoder) Decode(data []byte) (*frame.Frame, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.calls = append(m.calls, data)
	if m.decodeFn != nil {
		return m.decodeFn(data)
	}
	// Default behavior: return empty frame
	return &frame.Frame{}, nil
}

// Open marks the transport as open.
func (m *MockTransport) Open() error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.isOpen {
		return nil // Idempotent
	}

	m.isOpen = true
	return nil
}

// Send records the data and returns the configured error.
func (m *MockTransport) Send(data []byte) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.sendErr != nil {
		return 0, m.sendErr
	}

	// Make a copy to avoid mutation issues
	dataCopy := make([]byte, len(data))
	copy(dataCopy, data)
	m.sendData = append(m.sendData, dataCopy)

	return len(data), nil
}

// Receive returns the next queued response or the configured error.
func (m *MockTransport) Receive(maxLen int) ([]byte, error) {
	m.mu.Lock()
	delay := m.receiveDelay
	m.mu.Unlock()
	if delay > 0 {
		time.Sleep(delay)
	}

	for {
		m.mu.Lock()
		if m.receiveErr != nil {
			err := m.receiveErr
			m.mu.Unlock()
			return nil, err
		}

		if len(m.receiveQueue) > 0 {
			data := m.receiveQueue[0]
			m.receiveQueue = m.receiveQueue[1:]
			m.mu.Unlock()
			return data, nil
		}

		deadline := m.readDeadline
		m.mu.Unlock()

		if !deadline.IsZero() {
			wait := time.Until(deadline)
			if wait > 0 {
				time.Sleep(wait)
			}
			return nil, transport.ErrTimeout
		}

		// Block forever (caller should use timeout)
		select {}
	}
}

// Transfer performs a full-duplex transfer (send and receive atomically).
// This simulates SPI full-duplex behavior where TX and RX happen simultaneously.
// In real SPI, TX always succeeds, but RX might return zeros if peripheral isn't ready.
func (m *MockTransport) Transfer(ctx context.Context, data []byte) ([]byte, error) {
	m.mu.Lock()

	// Check for send error (simulates SPI hardware failure)
	// Respect sendErrAfter if set
	if m.sendErr != nil {
		shouldFail := true
		if m.sendErrAfter > 0 {
			if len(m.sendData)+1 != m.sendErrAfter {
				shouldFail = false
			}
		}
		if shouldFail {
			err := m.sendErr
			m.mu.Unlock()
			return nil, err
		}
	}

	// Simulate transmission delay
	if m.receiveDelay > 0 {
		m.mu.Unlock()
		time.Sleep(m.receiveDelay)
		m.mu.Lock()
	}

	// Record the sent data (TX always succeeds in SPI)
	dataCopy := make([]byte, len(data))
	copy(dataCopy, data)
	m.sendData = append(m.sendData, dataCopy)

	// Get the next queued response (simulates simultaneous RX)
	if len(m.receiveQueue) > 0 {
		rxData := m.receiveQueue[0]
		m.receiveQueue = m.receiveQueue[1:]
		m.mu.Unlock()
		return rxData, nil
	}

	m.mu.Unlock()

	// Return empty buffer (simulates SPI CIPO line with no data)
	// Real SPI always returns something - zeros if peripheral isn't ready
	emptyBuffer := make([]byte, len(data))
	return emptyBuffer, nil
}

// Close marks the transport as closed.
func (m *MockTransport) Close() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.isOpen = false
	return nil
}

// IsOpen returns whether the transport is currently open.
func (m *MockTransport) IsOpen() bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.isOpen
}

// SetReadDeadline sets a deadline for Receive in tests.
func (m *MockTransport) SetReadDeadline(deadline time.Time) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.readDeadline = deadline
	return nil
}

// QueueResponse adds a response to the receive queue.
func (m *MockTransport) QueueResponse(data []byte) {
	m.mu.Lock()
	defer m.mu.Unlock()

	dataCopy := make([]byte, len(data))
	copy(dataCopy, data)
	m.receiveQueue = append(m.receiveQueue, dataCopy)
}

// QueueResponses adds multiple responses to the receive queue.
func (m *MockTransport) QueueResponses(responses ...[]byte) {
	for _, resp := range responses {
		m.QueueResponse(resp)
	}
}

// GetSentData returns all data sent via Send().
func (m *MockTransport) GetSentData() [][]byte {
	m.mu.Lock()
	defer m.mu.Unlock()

	result := make([][]byte, len(m.sendData))
	for i, data := range m.sendData {
		dataCopy := make([]byte, len(data))
		copy(dataCopy, data)
		result[i] = dataCopy
	}
	return result
}

// GetSentCount returns the number of Send() calls.
func (m *MockTransport) GetSentCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return len(m.sendData)
}

// SetSendError sets the error to return from Send().
func (m *MockTransport) SetSendError(err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.sendErr = err
	m.sendErrAfter = 0
}

// SetSendErrorAfter sets the error to return from Send() on the Nth call (1-based).
func (m *MockTransport) SetSendErrorAfter(n int, err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.sendErr = err
	m.sendErrAfter = n
}

// SetReceiveError sets the error to return from Receive().
func (m *MockTransport) SetReceiveError(err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.receiveErr = err
}

// SetReceiveDelay sets the delay before Receive() returns.
func (m *MockTransport) SetReceiveDelay(delay time.Duration) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.receiveDelay = delay
}

// Reset clears all state for reuse.
func (m *MockTransport) Reset() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.sendData = make([][]byte, 0)
	m.receiveQueue = make([][]byte, 0)
	m.sendErr = nil
	m.sendErrAfter = 0
	m.receiveErr = nil
	m.receiveDelay = 0
	m.isOpen = true
	m.readDeadline = time.Time{}
}
