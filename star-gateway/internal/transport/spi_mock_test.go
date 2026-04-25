// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport provides mock SPI implementations for testing.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"errors"
	"sync"

	conn "periph.io/x/conn/v3"
	"periph.io/x/conn/v3/physic"
	"periph.io/x/conn/v3/spi"
)

// mockSPIConn implements spi.Conn for testing.
type mockSPIConn struct {
	mu          sync.Mutex
	txData      []byte      // Last transmitted data
	rxData      []byte      // Data to return on next Tx
	txError     error       // Error to return on Tx
	txCallCount int         // Number of Tx calls
	closed      bool        // Whether connection is closed
	duplex      conn.Duplex // Duplex mode
	bits        int         // Bits per word
	maxHz       int64       // Max speed in Hz
}

// Tx implements spi.Conn.Tx - performs full-duplex transfer.
func (m *mockSPIConn) Tx(w, r []byte) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.closed {
		return errors.New("mock SPI: connection closed")
	}

	m.txCallCount++

	if m.txError != nil {
		return m.txError
	}

	// Store transmitted data for verification
	m.txData = make([]byte, len(w))
	copy(m.txData, w)

	// Return mock received data
	if len(r) > 0 {
		if len(m.rxData) > 0 {
			// Copy rxData to r buffer
			copy(r, m.rxData)
		} else {
			// Default: echo back the transmitted data
			copy(r, w)
		}
	}

	return nil
}

// Duplex implements spi.Conn.Duplex.
func (m *mockSPIConn) Duplex() conn.Duplex {
	return m.duplex
}

// Speed implements spi.Conn.Speed.
func (m *mockSPIConn) Speed(hz physic.Frequency) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.maxHz = int64(hz)
	return nil
}

// CLK implements spi.Conn.CLK.
func (m *mockSPIConn) CLK() physic.Frequency {
	m.mu.Lock()
	defer m.mu.Unlock()
	return physic.Frequency(m.maxHz)
}

// String implements spi.Conn.String.
func (m *mockSPIConn) String() string {
	return "mockSPIConn"
}

// TxPackets implements spi.Conn.TxPackets.
// For testing purposes, we don't use this method, so it's a stub.
func (m *mockSPIConn) TxPackets(p []spi.Packet) error {
	return errors.New("TxPackets not implemented in mock")
}

// mockSPIPort implements spi.PortCloser for testing.
type mockSPIPort struct {
	mu         sync.Mutex
	conn       *mockSPIConn
	closeError error
	closeCount int
	connectErr error
	closed     bool
}

// String implements spi.PortCloser.String.
func (m *mockSPIPort) String() string {
	return "mockSPIPort"
}

// Connect implements spi.PortCloser.Connect.
func (m *mockSPIPort) Connect(f physic.Frequency, mode spi.Mode, bits int) (spi.Conn, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.connectErr != nil {
		return nil, m.connectErr
	}

	m.conn = &mockSPIConn{
		duplex: conn.Full, // SPI is always full-duplex
		bits:   bits,
		maxHz:  int64(f),
	}
	return m.conn, nil
}

// LimitSpeed implements spi.PortCloser.LimitSpeed.
func (m *mockSPIPort) LimitSpeed(f physic.Frequency) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.conn != nil {
		m.conn.maxHz = int64(f)
	}
	return nil
}

// Close implements spi.PortCloser.Close.
func (m *mockSPIPort) Close() error {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.closeCount++
	m.closed = true

	if m.closeError != nil {
		return m.closeError
	}

	if m.conn != nil {
		m.conn.closed = true
	}

	return nil
}

// newMockSPIPort creates a new mock SPI port for testing.
func newMockSPIPort() *mockSPIPort {
	return &mockSPIPort{}
}

// newSPITransportWithMockPort creates an SPITransport with a mock port for testing.
// This allows testing SPI operations without requiring actual hardware.
func newSPITransportWithMockPort(config *SPIConfig, port *mockSPIPort) (*SPITransport, error) {
	if config == nil {
		config = DefaultConfig()
	}

	transport := &SPITransport{
		config: config,
		isOpen: false,
	}

	// Simulate opening with the mock port
	conn, err := port.Connect(
		physic.Frequency(config.SpeedHz)*physic.Hertz,
		spi.Mode(config.Mode),
		int(config.BitsPerWord),
	)
	if err != nil {
		return nil, err
	}

	transport.port = port
	transport.conn = conn
	transport.isOpen = true

	return transport, nil
}
