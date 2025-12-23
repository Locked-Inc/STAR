// Package arq provides a mock transport for testing ARQ operations.
//
// STAR Project - Texas A&M University
// December 2025
package arq

import (
	"sync"
	"time"

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

	m.mu.Lock()
	defer m.mu.Unlock()

	if m.receiveErr != nil {
		return nil, m.receiveErr
	}

	if len(m.receiveQueue) == 0 {
		// Block forever (caller should use timeout)
		m.mu.Unlock()
		select {} // Deliberate block - tests use timeout
	}

	data := m.receiveQueue[0]
	m.receiveQueue = m.receiveQueue[1:]

	return data, nil
}

// Transfer performs a full-duplex transfer (not used in ARQ tests).
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
}
