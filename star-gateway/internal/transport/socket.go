// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport provides socket-based transport for HIL simulation.
package transport

import (
	"context"
	"fmt"
	"io"
	"net"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
)

// Socket transport configuration constants.
const (
	// DefaultSocketPath is the Unix domain socket path for Virtual RX72N.
	DefaultSocketPath = "/tmp/star_rx72n.sock"

	// DefaultSocketTimeout is the default I/O timeout for socket operations.
	DefaultSocketTimeout = 100 * time.Millisecond

	// TransferSize is the fixed transfer size for SPI simulation.
	// Both TX and RX must always be this exact size to match SPI behavior.
	TransferSize = frame.MaxFrameSize // 1036 bytes
)

// SocketTransport implements the Device interface using Unix Domain Sockets.
// It uses the "Unlocking" pattern to ensure thread-safe, non-blocking shutdowns.
type SocketTransport struct {
	mu   sync.RWMutex
	conn net.Conn
	path string
}

// NewSocketTransport creates a new SocketTransport with the given socket path.
func NewSocketTransport(socketPath string) *SocketTransport {
	if socketPath == "" {
		socketPath = DefaultSocketPath
	}
	return &SocketTransport{
		path: socketPath,
	}
}

// Open establishes a connection to the Virtual RX72N process via Unix socket.
func (s *SocketTransport) Open() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Reuse existing connection if open
	if s.conn != nil {
		return nil
	}

	conn, err := net.DialTimeout("unix", s.path, DefaultSocketTimeout)
	if err != nil {
		return fmt.Errorf("failed to connect to socket %s: %w", s.path, err)
	}

	s.conn = conn
	return nil
}

// Transfer sends txData and receives the response.
//
// Uses the "Unlocking" pattern: the mutex is held ONLY to retrieve the connection
// reference. The actual I/O is performed without the lock, allowing Close()
// to interrupt this operation immediately.
func (s *SocketTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	// 1. Critical Section: Retrieve connection snapshot
	s.mu.RLock()
	conn := s.conn
	s.mu.RUnlock()

	// 2. State Check
	if conn == nil {
		return nil, ErrDeviceNotOpen
	}

	// 3. Context Deadline Management
	// Monitor context cancellation to interrupt blocking I/O by forcing a deadline.
	monitorDone := make(chan struct{})
	defer close(monitorDone)

	go func() {
		select {
		case <-ctx.Done():
			// Force blocking I/O to return immediately
			_ = conn.SetDeadline(time.Now())
		case <-monitorDone:
			// Operation completed normally; clear deadline for future reuse
			_ = conn.SetDeadline(time.Time{})
		}
	}()

	// 4. Perform I/O (Unlocked)

	// Pad txData to TransferSize (simulate SPI COPI line)
	txBuffer := make([]byte, TransferSize)
	copy(txBuffer, txData)

	// Write exactly TransferSize bytes
	n, err := conn.Write(txBuffer)
	if err != nil {
		if ctx.Err() != nil {
			return nil, ctx.Err()
		}
		// If write failed because Close() was called, s.conn will be nil
		return nil, fmt.Errorf("socket write failed: %w", err)
	}
	if n != TransferSize {
		return nil, fmt.Errorf("socket write incomplete: wrote %d bytes, expected %d", n, TransferSize)
	}

	// Read exactly TransferSize bytes (simulate SPI CIPO line)
	rxBuffer := make([]byte, TransferSize)
	n, err = io.ReadFull(conn, rxBuffer)
	if err != nil {
		if ctx.Err() != nil {
			return nil, ctx.Err()
		}
		return nil, fmt.Errorf("socket read failed: %w", err)
	}
	if n != TransferSize {
		return nil, fmt.Errorf("socket read incomplete: read %d bytes, expected %d", n, TransferSize)
	}

	return rxBuffer, nil
}

// Close closes the socket connection.
//
// Uses the "Unlocking" pattern: sets s.conn to nil under lock, then calls
// conn.Close() outside the lock. This ensures pending Transfer() calls are
// interrupted immediately (returning net.ErrClosed) without waiting for a mutex.
func (s *SocketTransport) Close() error {
	s.mu.Lock()
	conn := s.conn
	s.conn = nil // "Lock out" new transfers immediately
	s.mu.Unlock()

	// Close connection outside mutex - this interrupts any pending I/O
	if conn != nil {
		if err := conn.Close(); err != nil {
			return fmt.Errorf("failed to close socket: %w", err)
		}
	}

	return nil
}

// IsOpen returns whether the socket is currently connected.
func (s *SocketTransport) IsOpen() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.conn != nil
}

// Path returns the Unix socket path.
func (s *SocketTransport) Path() string {
	return s.path
}

// Send implements the legacy Transport interface.
func (s *SocketTransport) Send(data []byte) (int, error) {
	_, err := s.Transfer(context.Background(), data)
	if err != nil {
		return 0, err
	}
	return len(data), nil
}

// Receive implements the legacy Transport interface.
func (s *SocketTransport) Receive(maxLen int) ([]byte, error) {
	txBuf := make([]byte, TransferSize)
	rxBuf, err := s.Transfer(context.Background(), txBuf)
	if err != nil {
		return nil, err
	}

	// Truncate to maxLen if specified
	if maxLen > 0 && len(rxBuf) > maxLen {
		return rxBuf[:maxLen], nil
	}

	return rxBuf, nil
}
