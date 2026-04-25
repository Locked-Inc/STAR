// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport provides the UART-over-USB transport layer for RPi5 <-> RX72N communication.
//
// The RX72N's SCI9 UART (PB6/PB7, 921600 baud) is routed through a Cypress CY7C65213
// USB-UART bridge chip that presents itself on the Pi5 as a Cypress_M8 serial device
// at /dev/ttyUSB0. This single byte stream multiplexes:
//   - protobuf command / response / telemetry frames
//   - k_frame_type_log_message (0x20) frames carrying firmware runtime logs
//
// all wrapped in the same SYNC+SEQ+LEN+TYPE+FLAGS+PAYLOAD+CRC32 framing.
//
// The type name "CDC" is retained across this package for historical reasons: the
// CY7C65213 does present a USB CDC-class interface to the host. Earlier versions of
// the system talked to the RX72N's native USB peripheral (USB0); that path was removed
// because its firmware stack proved unreliable.
//
// Reference: docs/sections/01_nanopb_protocol.tex
//
// STAR Project - Texas A&M University
package transport

import (
	"context"
	"errors"
	"fmt"
	"log"
	"sync"
	"time"

	"go.bug.st/serial"
)

// CDC configuration constants.
const (
	// DefaultCDCDevice is the default tty path where the CY7C65213 bridge enumerates.
	// The Cypress cypress_m8 kernel driver registers as ttyUSB* (not ttyACM*).
	DefaultCDCDevice = "/dev/ttyUSB0"

	// DefaultBaudRate is the baud rate that the CY7C65213 bridge is configured for
	// on its UART side. The RX72N SCI9 driver is programmed to match.
	DefaultBaudRate = 921600

	// DefaultCDCTimeout is the default read/write timeout for CDC operations.
	DefaultCDCTimeout = 100 * time.Millisecond

	// CDCDataBits is the number of data bits for CDC serial communication.
	CDCDataBits = 8

	// CypressVID is the Cypress (now Infineon) USB Vendor ID for the CY7C65213 bridge.
	CypressVID = 0x04B4

	// CY7C65213PID is the default USB Product ID reported by the CY7C65213.
	CY7C65213PID = 0x0003
)

// CDCConfig holds USB CDC configuration parameters.
type CDCConfig struct {
	// Device is the CDC device path (e.g., "/dev/ttyUSB0").
	// If empty, auto-detection will be attempted using VID/PID.
	Device string

	// BaudRate is the baud rate for serial communication.
	// For USB CDC, this is often ignored by hardware but required by the API.
	BaudRate int

	// Timeout is the read/write timeout.
	Timeout time.Duration

	// VID is the USB Vendor ID for auto-detection.
	// Reserved for future use - currently unused by autoDetect().
	// The go.bug.st/serial library does not provide VID/PID filtering.
	VID uint16

	// PID is the USB Product ID for auto-detection.
	// Reserved for future use - currently unused by autoDetect().
	// The go.bug.st/serial library does not provide VID/PID filtering.
	PID uint16
}

// DefaultCDCConfig returns the default CDC configuration.
func DefaultCDCConfig() *CDCConfig {
	return &CDCConfig{
		Device:   DefaultCDCDevice,
		BaudRate: DefaultBaudRate,
		Timeout:  DefaultCDCTimeout,
		VID:      CypressVID,
		PID:      CY7C65213PID,
	}
}

// Predefined CDC-specific errors.
var (
	// ErrDeviceNotFound is returned when no matching CDC device is found.
	ErrDeviceNotFound = errors.New("cdc: device not found")

	// ErrReadTimeout is returned when a read operation times out.
	ErrReadTimeout = errors.New("cdc: read timeout")
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
	var allPorts []string

	// Auto-detect device if not specified
	if device == "" {
		detected, ports, err := c.autoDetect()
		if err != nil {
			return fmt.Errorf("failed to auto-detect CDC device: %w", err)
		}
		device = detected
		allPorts = ports
	}

	// Open serial port
	mode := &serial.Mode{
		BaudRate: c.config.BaudRate,
		DataBits: CDCDataBits,
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

	// Log successful auto-detection (after critical section completes)
	if len(allPorts) > 0 {
		if len(allPorts) > 1 {
			log.Printf("CDC: Multiple serial ports detected: %v", allPorts)
			log.Printf("CDC: Auto-selected first port: %s", device)
		} else {
			log.Printf("CDC: Auto-detected device: %s", device)
		}
	}

	return nil
}

// autoDetect finds the first available CDC device matching the configured VID:PID.
//
// When VID or PID is non-zero, sysfs-based discovery (FindCDCDevice) is used to
// locate the correct ttyUSBN device regardless of which minor number the kernel
// assigned. This is robust to disconnect/reconnect cycles where the minor number
// may change (e.g., ttyUSB0 -> ttyUSB1).
//
// When VID and PID are both zero, falls back to returning the first available
// serial port from the OS port list.
func (c *CDCTransport) autoDetect() (string, []string, error) {
	// Prefer VID:PID-based sysfs discovery when credentials are configured.
	if c.config.VID != 0 || c.config.PID != 0 {
		device, err := FindCDCDevice(c.config.VID, c.config.PID)
		if err == nil {
			return device, []string{device}, nil
		}
		// Fall through to port-list enumeration if sysfs lookup fails.
	}

	ports, err := serial.GetPortsList()
	if err != nil {
		return "", nil, fmt.Errorf("failed to enumerate ports: %w", err)
	}

	if len(ports) == 0 {
		return "", nil, ErrDeviceNotFound
	}

	// Return first port when VID:PID filtering is unavailable.
	return ports[0], ports, nil
}

// Send transmits data over CDC.
// Ensures the full buffer is sent by looping until all bytes are written.
// Returns the number of bytes sent and any error.
func (c *CDCTransport) Send(data []byte) (int, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if !c.isOpen {
		return 0, ErrDeviceNotOpen
	}

	totalWritten := 0
	for totalWritten < len(data) {
		n, err := c.port.Write(data[totalWritten:])
		if err != nil {
			return totalWritten, fmt.Errorf("CDC send failed after %d/%d bytes: %w", totalWritten, len(data), err)
		}
		// Detect write stall to prevent infinite loop
		if n == 0 {
			return totalWritten, fmt.Errorf("CDC send stalled after %d/%d bytes (no progress)", totalWritten, len(data))
		}
		totalWritten += n
	}

	return totalWritten, nil
}

// Receive reads data from CDC.
// Reads up to maxLen bytes from the serial port.
// Returns ErrReadTimeout if no data is available.
func (c *CDCTransport) Receive(maxLen int) ([]byte, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if !c.isOpen {
		return nil, ErrDeviceNotOpen
	}

	buf := make([]byte, maxLen)
	n, err := c.port.Read(buf)
	if err != nil {
		return nil, fmt.Errorf("CDC receive failed: %w", err)
	}

	// Detect timeout when no data is available
	if n == 0 {
		return nil, ErrReadTimeout
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
// Uses a write lock because SetReadTimeout mutates port state.
func (c *CDCTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	// Check context before transfer
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	c.mu.Lock()
	defer c.mu.Unlock()

	if !c.isOpen {
		return nil, ErrDeviceNotOpen
	}

	// Send data - ensure full buffer is written (consistent with Send())
	totalWritten := 0
	for totalWritten < len(txData) {
		n, err := c.port.Write(txData[totalWritten:])
		if err != nil {
			return nil, fmt.Errorf("CDC transfer write failed after %d/%d bytes: %w", totalWritten, len(txData), err)
		}
		// CRITICAL: Detect write stall to prevent infinite loop
		if n == 0 {
			return nil, fmt.Errorf("CDC transfer write stalled after %d/%d bytes (no progress)", totalWritten, len(txData))
		}
		totalWritten += n
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
		// Restore original timeout on all return paths
		defer func() {
			if err := c.port.SetReadTimeout(c.config.Timeout); err != nil {
				log.Printf("CDC: failed to restore read timeout: %v", err)
			}
		}()
	}

	for totalRead < len(rxBuf) {
		// Check context cancellation
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		default:
		}

		n, err := c.port.Read(rxBuf[totalRead:])
		totalRead += n

		if err != nil {
			return nil, fmt.Errorf("CDC transfer read failed after %d/%d bytes: %w", totalRead, len(rxBuf), err)
		}

		// If we got no data, timeout
		if n == 0 {
			return nil, fmt.Errorf("CDC transfer read timeout after %d/%d bytes: %w", totalRead, len(rxBuf), ErrReadTimeout)
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

// Config returns a copy of the current CDC configuration.
// Returns a copy to prevent external mutation of transport settings.
func (c *CDCTransport) Config() *CDCConfig {
	c.mu.RLock()
	defer c.mu.RUnlock()

	// Return a deep copy
	configCopy := *c.config
	return &configCopy
}

// IsOpen returns whether the device is currently open.
func (c *CDCTransport) IsOpen() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.isOpen
}
