// Package harq provides a mock transport for testing HARQ operations.
//
// STAR Project - Texas A&M University
// December 2025
package harq

import (
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// MockTransport implements transport.Transport for testing.
type MockTransport struct {
	mu           sync.Mutex
	sendData     [][]byte      // Data sent via Send()
	receiveQueue [][]byte      // Data to return from Receive()
	sendErr      error         // Error to return from Send()
	receiveErr   error         // Error to return from Receive()
	receiveDelay time.Duration // Delay before Receive returns
	isOpen       bool
	readDeadline time.Time
}

// Verify MockTransport implements transport.Transport.
var _ transport.Transport = (*MockTransport)(nil)

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
	if m.receiveDelay > 0 {
		time.Sleep(m.receiveDelay)
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

// Transfer performs a full-duplex transfer (not used in HARQ tests).
func (m *MockTransport) Transfer(txData []byte) ([]byte, error) {
	return nil, transport.ErrNotImplemented
}

// Close marks the transport as closed.
func (m *MockTransport) Close() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.isOpen = false
	return nil
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
	m.receiveErr = nil
	m.receiveDelay = 0
	m.isOpen = true
	m.readDeadline = time.Time{}
}
