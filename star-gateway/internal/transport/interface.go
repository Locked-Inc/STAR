// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport provides the low-level communication interfaces for
// the Gateway. This includes both real hardware (SPI) and simulation (Unix socket).
//
// STAR Project - Texas A&M University
// January 2026
package transport

import "context"

// Device represents the low-level connection (SPI or Mock/Socket).
// This interface enables dependency injection for testing and simulation,
// allowing the Gateway to communicate with either real hardware or a
// Virtual RX72N simulator without code changes.
//
// Device is an alias for Transport with the addition of context-aware operations.
// Both SPITransport and SocketTransport implement this interface.
type Device interface {
	// Transfer sends data and receives response simultaneously (Full Duplex).
	// This matches the native SPI operation mode where txData is sent while
	// rxData is received in a single atomic operation.
	//
	// The context allows for timeout control and cancellation.
	// Returns the received data and any error encountered.
	Transfer(ctx context.Context, txData []byte) ([]byte, error)

	// Open initializes the device connection.
	// Must be called before any Transfer operations.
	Open() error

	// Close releases the device resources.
	// Should be called when done with the device.
	Close() error

	// Receive reads up to len bytes from the device.
	Receive(len int) ([]byte, error)

	// Send writes data to the device.
	Send(data []byte) (int, error)

	// IsOpen returns whether the device is currently connected.
	IsOpen() bool
}
