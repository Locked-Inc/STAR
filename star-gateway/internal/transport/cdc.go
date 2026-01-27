// Package transport provides the USB CDC transport layer for RPi5 <-> RX72N communication.
//
// The Raspberry Pi 5 communicates with the RX72N over USB CDC (Communications Device Class),
// which appears as a virtual serial port (e.g., /dev/ttyACM0). This provides a simpler
// interface than SPI and includes built-in flow control and reliability.
//
// Reference: docs/sections/01_nanopb_protocol.tex
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"go.bug.st/serial"
)

// CDC configuration constants.
const (
	// DefaultCDCDevice is the default CDC device path on Linux.
	// RX72N typically appears as /dev/ttyACM0.
	DefaultCDCDevice = "/dev/ttyACM0"

	// DefaultBaudRate is the baud rate for CDC communication.
	// Note: For USB CDC, baud rate is often ignored by the hardware but
	// required by the serial library.
	DefaultBaudRate = 115200

	// DefaultCDCTimeout is the default read/write timeout for CDC operations.
	DefaultCDCTimeout = 100 * time.Millisecond

	// RenesasVID is the Renesas USB Vendor ID.
	RenesasVID = 0x045B

	// RX72NPID is the RX72N USB Product ID.
	// TODO: Verify this value once RX72N firmware team confirms.
	RX72NPID = 0x0235
)

// CDCConfig holds USB CDC configuration parameters.
type CDCConfig struct {
	// Device is the CDC device path (e.g., "/dev/ttyACM0").
	// If empty, auto-detection will be attempted using VID/PID.
	Device string

	// BaudRate is the baud rate for serial communication.
	// For USB CDC, this is often ignored by hardware but required by the API.
	BaudRate int

	// Timeout is the read/write timeout.
	Timeout time.Duration

	// VID is the USB Vendor ID for auto-detection.
	// Set to 0 to disable VID filtering.
	VID uint16

	// PID is the USB Product ID for auto-detection.
	// Set to 0 to disable PID filtering.
	PID uint16
}

// DefaultCDCConfig returns the default CDC configuration.
func DefaultCDCConfig() *CDCConfig {
	return &CDCConfig{
		Device:   DefaultCDCDevice,
		BaudRate: DefaultBaudRate,
		Timeout:  DefaultCDCTimeout,
		VID:      RenesasVID,
		PID:      RX72NPID,
	}
}

// Predefined CDC-specific errors.
var (
	// ErrDeviceNotFound is returned when no matching CDC device is found.
	ErrDeviceNotFound = errors.New("cdc: device not found")

	// ErrMultipleDevices is returned when multiple matching devices are found.
	ErrMultipleDevices = errors.New("cdc: multiple matching devices found")

	// ErrReadTimeout is returned when a read operation times out.
	ErrReadTimeout = errors.New("cdc: read timeout")

	// ErrWriteTimeout is returned when a write operation times out.
	ErrWriteTimeout = errors.New("cdc: write timeout")
)

// CDCTransport implements the Device interface for USB CDC communication.
// It provides context-aware operations for serial communication with the RX72N.
//
// Unlike SPI (which is full-duplex), CDC is half-duplex:
//   - Send() writes data
//   - Receive() reads data
//   - Transfer() writes then reads (simulated full-duplex)
type CDCTransport struct {
	mu     sync.RWMutex // protects isOpen, port
	config *CDCConfig
	isOpen bool
	port   serial.Port
}

// NewCDCTransport creates a new CDCTransport with the given configuration.
// If config is nil, uses DefaultCDCConfig().
func NewCDCTransport(config *CDCConfig) *CDCTransport {
	if config == nil {
		config = DefaultCDCConfig()
	}
	return &CDCTransport{
		config: config,
		isOpen: false,
	}
}

// Open initializes the CDC device.
// If Device is empty in config, attempts to auto-detect using VID/PID.
func (c *CDCTransport) Open() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.isOpen {
		return nil
	}

	device := c.config.Device

	// Auto-detect device if not specified
	if device == "" {
		detected, err := c.autoDetect()
		if err != nil {
			return fmt.Errorf("failed to auto-detect CDC device: %w", err)
		}
		device = detected
	}

	// Open serial port
	mode := &serial.Mode{
		BaudRate: c.config.BaudRate,
		DataBits: 8,
		Parity:   serial.NoParity,
		StopBits: serial.OneStopBit,
	}

	port, err := serial.Open(device, mode)
	if err != nil {
		return fmt.Errorf("failed to open CDC device %s: %w", device, err)
	}

	// Set timeout
	if err := port.SetReadTimeout(c.config.Timeout); err != nil {
		port.Close()
		return fmt.Errorf("failed to set read timeout: %w", err)
	}

	c.port = port
	c.isOpen = true
	return nil
}

// autoDetect finds the first available CDC device.
//
// Note: VID/PID filtering is not implemented in the current version because
// go.bug.st/serial does not provide USB device enumeration. For production use,
// either specify the device path explicitly in config, or implement platform-specific
// VID/PID filtering using sysfs on Linux (/sys/class/tty/*/device/../../idVendor).
//
// This function returns the first available serial port, which is typically
// sufficient when only one RX72N is connected.
func (c *CDCTransport) autoDetect() (string, error) {
	ports, err := serial.GetPortsList()
	if err != nil {
		return "", fmt.Errorf("failed to enumerate ports: %w", err)
	}

	if len(ports) == 0 {
		return "", ErrDeviceNotFound
	}

	// Return first port (typically /dev/ttyACM0 on Linux)
	// TODO: Implement VID/PID filtering using platform-specific APIs if needed
	return ports[0], nil
}

// Send transmits data over CDC.
// Returns the number of bytes sent and any error.
func (c *CDCTransport) Send(data []byte) (int, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	if !c.isOpen {
		return 0, ErrDeviceNotOpen
	}

	n, err := c.port.Write(data)
	if err != nil {
		return n, fmt.Errorf("CDC send failed: %w", err)
	}

	return n, nil
}

// Receive reads data from CDC.
// Reads up to maxLen bytes from the serial port.
func (c *CDCTransport) Receive(maxLen int) ([]byte, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	if !c.isOpen {
		return nil, ErrDeviceNotOpen
	}

	buf := make([]byte, maxLen)
	n, err := c.port.Read(buf)
	if err != nil {
		return nil, fmt.Errorf("CDC receive failed: %w", err)
	}

	return buf[:n], nil
}

// Transfer performs a half-duplex transfer with context support.
// For CDC (half-duplex):
//  1. Sends txData
//  2. Waits for response
//  3. Reads response (same length as txData)
//
// The context deadline is checked before and during the transfer.
func (c *CDCTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	// Check context before transfer
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	c.mu.RLock()
	defer c.mu.RUnlock()

	if !c.isOpen {
		return nil, ErrDeviceNotOpen
	}

	// Send data
	n, err := c.port.Write(txData)
	if err != nil {
		return nil, fmt.Errorf("CDC transfer write failed: %w", err)
	}
	if n != len(txData) {
		return nil, fmt.Errorf("CDC transfer incomplete write: wrote %d of %d bytes", n, len(txData))
	}

	// Check context after write
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	// Read response (expect same length as transmitted data)
	rxBuf := make([]byte, len(txData))
	totalRead := 0

	// Read with context timeout
	deadline, hasDeadline := ctx.Deadline()
	if hasDeadline {
		timeout := time.Until(deadline)
		if timeout < 0 {
			return nil, context.DeadlineExceeded
		}
		if err := c.port.SetReadTimeout(timeout); err != nil {
			return nil, fmt.Errorf("failed to set read timeout: %w", err)
		}
	}

	for totalRead < len(rxBuf) {
		// Check context cancellation
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		default:
		}

		n, err := c.port.Read(rxBuf[totalRead:])
		if err != nil {
			return nil, fmt.Errorf("CDC transfer read failed: %w", err)
		}
		totalRead += n

		// If we got some data but not all, continue reading
		if totalRead < len(rxBuf) && n > 0 {
			continue
		}

		// If we got no data, timeout
		if n == 0 {
			return nil, ErrReadTimeout
		}
	}

	// Restore original timeout
	if hasDeadline {
		if err := c.port.SetReadTimeout(c.config.Timeout); err != nil {
			return nil, fmt.Errorf("failed to restore read timeout: %w", err)
		}
	}

	return rxBuf, nil
}

// Close releases the CDC device.
func (c *CDCTransport) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if !c.isOpen {
		return nil
	}

	if c.port != nil {
		if err := c.port.Close(); err != nil {
			return fmt.Errorf("failed to close CDC port: %w", err)
		}
	}

	c.isOpen = false
	c.port = nil
	return nil
}

// Config returns the current CDC configuration.
func (c *CDCTransport) Config() *CDCConfig {
	return c.config
}

// IsOpen returns whether the device is currently open.
func (c *CDCTransport) IsOpen() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.isOpen
}
