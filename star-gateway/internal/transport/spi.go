// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport provides the SPI transport layer for RPi5 <-> RX72N communication.
//
// The Raspberry Pi 5 acts as the SPI controller, communicating with the RX72N
// peripheral at 10 MHz using SPI Mode 0.
//
// Reference: docs/sections/01_nanopb_protocol.tex
//
// STAR Project - Texas A&M University
// December 2025
package transport

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"periph.io/x/conn/v3/physic"
	"periph.io/x/conn/v3/spi"
	"periph.io/x/conn/v3/spi/spireg"
	"periph.io/x/host/v3"
)

// SPI configuration constants from hardware specification.
const (
	// DefaultDevice is the default SPI device path on Raspberry Pi 5.
	DefaultDevice = "/dev/spidev0.0"

	// DefaultSpeedHz is the SPI clock speed (10 MHz = 10 Mbps).
	DefaultSpeedHz = 10_000_000

	// DefaultMode is SPI Mode 0 (CPOL=0, CPHA=0).
	DefaultMode = 0

	// DefaultBitsPerWord is the word size in bits.
	DefaultBitsPerWord = 8

	// DefaultTimeout is the default operation timeout.
	DefaultTimeout = 100 * time.Millisecond
)

// SPIConfig holds SPI configuration parameters.
type SPIConfig struct {
	// Device is the SPI device path (e.g., "/dev/spidev0.0").
	Device string

	// SpeedHz is the SPI clock speed in Hz.
	SpeedHz uint32

	// Mode is the SPI mode (0-3).
	Mode uint8

	// BitsPerWord is the word size in bits (8 for our protocol).
	BitsPerWord uint8

	// Timeout is the operation timeout.
	Timeout time.Duration
}

// DefaultConfig returns the default SPI configuration.
func DefaultConfig() *SPIConfig {
	return &SPIConfig{
		Device:      DefaultDevice,
		SpeedHz:     DefaultSpeedHz,
		Mode:        DefaultMode,
		BitsPerWord: DefaultBitsPerWord,
		Timeout:     DefaultTimeout,
	}
}

// Predefined errors for transport operations.
var (
	// ErrNotImplemented is returned by placeholder implementations.
	ErrNotImplemented = errors.New("transport: not implemented")

	// ErrDeviceNotOpen is returned when operating on a closed device.
	ErrDeviceNotOpen = errors.New("transport: device not open")

	// ErrTimeout is returned when an operation times out.
	ErrTimeout = errors.New("transport: operation timed out")

	// ErrTransferFailed is returned when a transfer operation fails.
	ErrTransferFailed = errors.New("transport: transfer failed")
)

// SPITransport implements the Device interface using periph.io for SPI communication.
// The Raspberry Pi 5 acts as the SPI controller, communicating with the RX72N at 10 MHz.
type SPITransport struct {
	mu     sync.RWMutex // protects isOpen, conn, port
	config *SPIConfig
	isOpen bool
	conn   spi.Conn       // periph.io SPI connection
	port   spi.PortCloser // periph.io SPI port (for cleanup)

	readDeadline time.Time
}

// NewSPITransport creates a new SPITransport with the given configuration.
// If config is nil, uses DefaultConfig().
func NewSPITransport(config *SPIConfig) *SPITransport {
	if config == nil {
		config = DefaultConfig()
	}
	return &SPITransport{
		config: config,
		isOpen: false,
	}
}

// Open initializes the SPI device.
// Initializes periph.io host drivers, opens the SPI port, and configures it
// with the specified mode, speed, and bits per word.
func (s *SPITransport) Open() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.isOpen {
		return nil
	}

	// Initialize periph.io host drivers
	// This must be called once before using any periph.io drivers
	if _, err := host.Init(); err != nil {
		return fmt.Errorf("failed to initialize periph.io: %w", err)
	}

	// Open SPI port by device path (e.g., "/dev/spidev0.0")
	port, err := spireg.Open(s.config.Device)
	if err != nil {
		return fmt.Errorf("failed to open SPI port %s: %w", s.config.Device, err)
	}

	// Configure SPI connection with speed, mode, and bits per word
	conn, err := port.Connect(
		physic.Frequency(s.config.SpeedHz)*physic.Hertz,
		spi.Mode(s.config.Mode),
		int(s.config.BitsPerWord),
	)
	if err != nil {
		port.Close()
		return fmt.Errorf("failed to configure SPI: %w", err)
	}

	s.port = port
	s.conn = conn
	s.isOpen = true
	return nil
}

// Send transmits data over SPI.
// Performs a write-only operation by discarding the received data.
// SPI is inherently full-duplex, so we must read while writing.
func (s *SPITransport) Send(data []byte) (int, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if !s.isOpen {
		return 0, ErrDeviceNotOpen
	}

	// Write-only operation: allocate receive buffer but discard the data
	rxBuf := make([]byte, len(data))
	if err := s.conn.Tx(data, rxBuf); err != nil {
		return 0, fmt.Errorf("SPI send failed: %w", err)
	}

	return len(data), nil
}

// Receive reads data from SPI.
// Performs a read-only operation by sending dummy bytes (zeros).
// SPI is inherently full-duplex, so we must write while reading.
func (s *SPITransport) Receive(maxLen int) ([]byte, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if !s.isOpen {
		return nil, ErrDeviceNotOpen
	}
	if !s.readDeadline.IsZero() && time.Now().After(s.readDeadline) {
		return nil, ErrTimeout
	}

	// Read-only operation: send dummy bytes (zeros) while reading
	txBuf := make([]byte, maxLen)
	rxBuf := make([]byte, maxLen)

	if err := s.conn.Tx(txBuf, rxBuf); err != nil {
		return nil, fmt.Errorf("SPI receive failed: %w", err)
	}

	return rxBuf, nil
}

// SetReadDeadline sets a deadline for future Receive calls.
// Note: SPI transactions are synchronous and cannot be interrupted once started.
// The deadline is checked before each Receive.
func (s *SPITransport) SetReadDeadline(deadline time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.readDeadline = deadline
	return nil
}

// Transfer performs full-duplex SPI transfer with context support.
// Sends txData while simultaneously receiving data of the same length.
// This is the native SPI operation mode.
//
// The context deadline is checked before the transfer. Note that SPI
// transactions are synchronous and cannot be interrupted once started.
func (s *SPITransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	// Check context before transfer
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	if !s.isOpen {
		return nil, ErrDeviceNotOpen
	}

	if len(txData) == 0 {
		return []byte{}, nil
	}
	rxBuf := make([]byte, len(txData))
	if err := s.conn.Tx(txData, rxBuf); err != nil {
		return nil, fmt.Errorf("SPI transfer failed: %w", err)
	}

	return rxBuf, nil
}

// Close releases the SPI device.
// Closes the periph.io SPI port and marks the transport as closed.
func (s *SPITransport) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if !s.isOpen {
		return nil
	}

	if s.port != nil {
		if err := s.port.Close(); err != nil {
			return fmt.Errorf("failed to close SPI port: %w", err)
		}
	}

	s.isOpen = false
	s.conn = nil
	s.port = nil
	return nil
}

// Config returns the current SPI configuration.
func (s *SPITransport) Config() *SPIConfig {
	return s.config
}

// IsOpen returns whether the device is currently open.
func (s *SPITransport) IsOpen() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.isOpen
}
