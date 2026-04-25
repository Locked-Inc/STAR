// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport provides mock serial port for CDC testing without hardware.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"errors"
	"sync"
	"time"

	"go.bug.st/serial"
)

// mockSerialPort implements serial.Port interface for testing.
// Provides configurable behaviors for simulating various device conditions.
type mockSerialPort struct {
	mu sync.Mutex

	// State
	closed      bool
	readTimeout time.Duration

	// Data buffers
	rxBuffer []byte // Data to return on Read() calls
	txBuffer []byte // Data received via Write() calls (for verification)

	// Behavior configuration
	readBehavior   *mockReadBehavior
	writeBehavior  *mockWriteBehavior
	nextError      error // Error to return on next operation (deprecated)
	nextReadError  error // Error to return on next Read() call
	nextWriteError error // Error to return on next Write() call

	// Call tracking
	readCallCount  int
	writeCallCount int
	timeoutCalls   []time.Duration // Track all SetReadTimeout calls
}

// mockReadBehavior configures Read() operation behavior.
type mockReadBehavior struct {
	maxBytesPerRead int           // Maximum bytes to return per Read() call (0 = unlimited)
	latency         time.Duration // Simulated read latency
	returnZero      bool          // Return n=0 to simulate timeout
}

// mockWriteBehavior configures Write() operation behavior.
type mockWriteBehavior struct {
	maxBytesPerWrite int           // Maximum bytes to accept per Write() call (0 = unlimited)
	latency          time.Duration // Simulated write latency
	returnZero       bool          // Return n=0 to simulate write stall
}

// newMockSerialPort creates a new mock serial port with default behavior.
func newMockSerialPort() *mockSerialPort {
	return &mockSerialPort{
		readTimeout: DefaultCDCTimeout,
		readBehavior: &mockReadBehavior{
			maxBytesPerRead: 0, // Unlimited by default
		},
		writeBehavior: &mockWriteBehavior{
			maxBytesPerWrite: 0, // Unlimited by default
		},
		timeoutCalls: make([]time.Duration, 0),
	}
}

// Read implements serial.Port.Read.
// Reads up to len(b) bytes from the receive buffer.
func (m *mockSerialPort) Read(b []byte) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.closed {
		return 0, errors.New("mock serial: port closed")
	}

	m.readCallCount++

	// Check for injected read-specific error first
	if m.nextReadError != nil {
		err := m.nextReadError
		m.nextReadError = nil // Clear after use
		return 0, err
	}

	// Check for generic injected error (legacy)
	if m.nextError != nil {
		err := m.nextError
		m.nextError = nil // Clear after use
		return 0, err
	}

	// Simulate latency
	if m.readBehavior.latency > 0 {
		m.mu.Unlock()
		time.Sleep(m.readBehavior.latency)
		m.mu.Lock()

		// Re-check state after re-acquiring lock
		if m.closed {
			return 0, errors.New("mock serial: port closed during read")
		}
	}

	// Simulate timeout (return n=0)
	if m.readBehavior.returnZero {
		return 0, nil
	}

	// No data available - simulate timeout
	if len(m.rxBuffer) == 0 {
		return 0, nil
	}

	// Copy data from buffer to output
	maxBytes := len(b)
	if m.readBehavior.maxBytesPerRead > 0 && m.readBehavior.maxBytesPerRead < maxBytes {
		maxBytes = m.readBehavior.maxBytesPerRead
	}

	n := copy(b[:maxBytes], m.rxBuffer)
	m.rxBuffer = m.rxBuffer[n:] // Consume read bytes
	return n, nil
}

// Write implements serial.Port.Write.
// Writes data to internal buffer for verification.
func (m *mockSerialPort) Write(b []byte) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.closed {
		return 0, errors.New("mock serial: port closed")
	}

	m.writeCallCount++

	// Check for injected write-specific error first
	if m.nextWriteError != nil {
		err := m.nextWriteError
		m.nextWriteError = nil // Clear after use
		return 0, err
	}

	// Check for generic injected error (legacy)
	if m.nextError != nil {
		err := m.nextError
		m.nextError = nil // Clear after use
		return 0, err
	}

	// Simulate latency
	if m.writeBehavior.latency > 0 {
		m.mu.Unlock()
		time.Sleep(m.writeBehavior.latency)
		m.mu.Lock()
	}

	// Simulate write stall (return n=0)
	if m.writeBehavior.returnZero {
		return 0, nil
	}

	// Write data to buffer
	bytesToWrite := len(b)
	if m.writeBehavior.maxBytesPerWrite > 0 && m.writeBehavior.maxBytesPerWrite < bytesToWrite {
		bytesToWrite = m.writeBehavior.maxBytesPerWrite
	}

	m.txBuffer = append(m.txBuffer, b[:bytesToWrite]...)
	return bytesToWrite, nil
}

// Close implements serial.Port.Close.
func (m *mockSerialPort) Close() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.closed = true
	return nil
}

// SetReadTimeout implements serial.Port.SetReadTimeout.
func (m *mockSerialPort) SetReadTimeout(timeout time.Duration) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.closed {
		return errors.New("mock serial: port closed")
	}

	m.readTimeout = timeout
	m.timeoutCalls = append(m.timeoutCalls, timeout)
	return nil
}

// Stub methods to satisfy serial.Port interface
func (m *mockSerialPort) GetModemStatusBits() (*serial.ModemStatusBits, error) {
	return &serial.ModemStatusBits{}, nil
}

func (m *mockSerialPort) SetDTR(dtr bool) error {
	return nil
}

func (m *mockSerialPort) SetRTS(rts bool) error {
	return nil
}

func (m *mockSerialPort) Drain() error {
	return nil
}

func (m *mockSerialPort) ResetInputBuffer() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.rxBuffer = nil
	return nil
}

func (m *mockSerialPort) ResetOutputBuffer() error {
	return nil
}

func (m *mockSerialPort) Break(duration time.Duration) error {
	return nil
}

func (m *mockSerialPort) Reconfigure(options ...func(*serial.Mode) error) error {
	return nil
}

func (m *mockSerialPort) SetMode(mode *serial.Mode) error {
	// No-op for mock; accept any mode configuration
	return nil
}

// Helper methods for test configuration

// QueueReadData adds data to the receive buffer.
// Data will be returned by subsequent Read() calls.
func (m *mockSerialPort) QueueReadData(data []byte) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.rxBuffer = append(m.rxBuffer, data...)
}

// GetWrittenData returns all data written via Write() calls.
func (m *mockSerialPort) GetWrittenData() []byte {
	m.mu.Lock()
	defer m.mu.Unlock()
	return append([]byte(nil), m.txBuffer...)
}

// ClearWrittenData clears the write buffer.
func (m *mockSerialPort) ClearWrittenData() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.txBuffer = nil
}

// SetPartialReadMode configures Read() to return at most maxBytes per call.
// Use this to simulate partial reads.
func (m *mockSerialPort) SetPartialReadMode(maxBytes int) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.readBehavior.maxBytesPerRead = maxBytes
}

// SetPartialWriteMode configures Write() to accept at most maxBytes per call.
// Use this to simulate partial writes.
func (m *mockSerialPort) SetPartialWriteMode(maxBytes int) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.writeBehavior.maxBytesPerWrite = maxBytes
}

// SetReadLatency adds artificial delay to Read() calls.
func (m *mockSerialPort) SetReadLatency(latency time.Duration) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.readBehavior.latency = latency
}

// SetWriteLatency adds artificial delay to Write() calls.
func (m *mockSerialPort) SetWriteLatency(latency time.Duration) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.writeBehavior.latency = latency
}

// SetReadTimeoutBehavior configures Read() to always return n=0 (timeout simulation).
func (m *mockSerialPort) SetReadTimeoutBehavior(returnZero bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.readBehavior.returnZero = returnZero
}

// SetWriteStallBehavior configures Write() to always return n=0 (stall).
func (m *mockSerialPort) SetWriteStallBehavior(returnZero bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.writeBehavior.returnZero = returnZero
}

// SetNextError configures the next Read() or Write() to return an error.
func (m *mockSerialPort) SetNextError(err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.nextError = err
}

// SetNextReadError configures the next Read() to return an error.
func (m *mockSerialPort) SetNextReadError(err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.nextReadError = err
}

// SetNextWriteError configures the next Write() to return an error.
func (m *mockSerialPort) SetNextWriteError(err error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.nextWriteError = err
}

// GetReadCallCount returns the number of Read() calls.
func (m *mockSerialPort) GetReadCallCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.readCallCount
}

// GetWriteCallCount returns the number of Write() calls.
func (m *mockSerialPort) GetWriteCallCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.writeCallCount
}

// GetTimeoutCalls returns all SetReadTimeout() values in order.
func (m *mockSerialPort) GetTimeoutCalls() []time.Duration {
	m.mu.Lock()
	defer m.mu.Unlock()
	return append([]time.Duration(nil), m.timeoutCalls...)
}

// GetCurrentTimeout returns the current read timeout value.
func (m *mockSerialPort) GetCurrentTimeout() time.Duration {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.readTimeout
}

// newCDCTransportWithMock creates a CDCTransport with injected mock port.
// This bypasses serial.Open() for testing.
func newCDCTransportWithMock(config *CDCConfig, mock *mockSerialPort) *CDCTransport {
	if config == nil {
		config = DefaultCDCConfig()
	}

	cdc := &CDCTransport{
		config: config,
		port:   mock,
		isOpen: true, // Already "opened" with mock
	}

	// Mirror Open - aligning mock timeout with config
	mock.mu.Lock()
	mock.readTimeout = config.Timeout
	mock.timeoutCalls = append(mock.timeoutCalls, config.Timeout)
	mock.mu.Unlock()

	return cdc
}
