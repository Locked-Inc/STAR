// Package transport provides the SPI transport layer for RPi5 <-> ESP32 communication.
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
	"errors"
	"time"
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

// Transport defines the interface for low-level communication.
type Transport interface {
	// Send transmits data over the transport.
	// Returns the number of bytes sent and any error.
	Send(data []byte) (int, error)

	// Receive reads data from the transport.
	// Returns the received data and any error.
	Receive(maxLen int) ([]byte, error)

	// Transfer performs a full-duplex SPI transfer.
	// Sends txData while simultaneously receiving data.
	// Returns the received data and any error.
	Transfer(txData []byte) ([]byte, error)

	// Close releases transport resources.
	Close() error
}

// SPIConfig holds SPI configuration parameters.
type SPIConfig struct {
	// Device is the SPI device path (e.g., "/dev/spidev0.0").
	Device string

	// SpeedHz is the SPI clock speed in Hz.
	SpeedHz uint32

	// Mode is the SPI mode (0-3).
	Mode uint8

	// BitsPerWord is the word size in bits.
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

// SPITransport is a placeholder implementation of the Transport interface.
// TODO: Implement actual SPI communication using periph.io or spidev in a future PR.
type SPITransport struct {
	config *SPIConfig
	isOpen bool
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
// TODO: Implement device initialization:
//  1. Open /dev/spidevX.Y
//  2. Configure mode, speed, bits per word
//  3. Set up DMA if available
func (s *SPITransport) Open() error {
	return ErrNotImplemented
}

// Send transmits data over SPI.
// TODO: Implement send logic using ioctl SPI_IOC_MESSAGE.
func (s *SPITransport) Send(data []byte) (int, error) {
	if !s.isOpen {
		return 0, ErrDeviceNotOpen
	}
	return 0, ErrNotImplemented
}

// Receive reads data from SPI.
// TODO: Implement receive logic.
func (s *SPITransport) Receive(maxLen int) ([]byte, error) {
	if !s.isOpen {
		return nil, ErrDeviceNotOpen
	}
	return nil, ErrNotImplemented
}

// Transfer performs full-duplex SPI transfer.
// TODO: Implement full-duplex transfer using ioctl SPI_IOC_MESSAGE.
func (s *SPITransport) Transfer(txData []byte) ([]byte, error) {
	if !s.isOpen {
		return nil, ErrDeviceNotOpen
	}
	return nil, ErrNotImplemented
}

// Close releases the SPI device.
// TODO: Implement device cleanup.
func (s *SPITransport) Close() error {
	if !s.isOpen {
		return nil
	}
	return ErrNotImplemented
}

// Config returns the current SPI configuration.
func (s *SPITransport) Config() *SPIConfig {
	return s.config
}

// IsOpen returns whether the device is currently open.
func (s *SPITransport) IsOpen() bool {
	return s.isOpen
}
