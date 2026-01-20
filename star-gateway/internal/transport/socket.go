// Package transport provides socket-based transport for HIL simulation.
//
// SocketTransport implements the Device interface using Unix Domain Sockets
// instead of SPI hardware. This enables Hardware-in-the-Loop (HIL) testing
// where the Gateway communicates with a Virtual RX72N simulator process.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"context"
	"fmt"
	"io"
	"net"
	"sync"
	"time"
)

// Socket transport configuration constants.
const (
	// DefaultSocketPath is the Unix domain socket path for Virtual RX72N.
	DefaultSocketPath = "/tmp/star_rx72n.sock"

	// DefaultSocketTimeout is the default I/O timeout for socket operations.
	DefaultSocketTimeout = 100 * time.Millisecond
)

// SocketTransport implements the Device interface using Unix Domain Sockets.
// It connects to a Virtual RX72N simulator for Hardware-in-the-Loop testing.
//
// This transport is a drop-in replacement for SPITransport, enabling the Gateway
// to run on development machines without physical hardware.
type SocketTransport struct {
	mu     sync.RWMutex // protects conn, isOpen
	conn   net.Conn
	path   string
	isOpen bool
}

// NewSocketTransport creates a new SocketTransport with the given socket path.
// If socketPath is empty, uses DefaultSocketPath.
func NewSocketTransport(socketPath string) *SocketTransport {
	if socketPath == "" {
		socketPath = DefaultSocketPath
	}
	return &SocketTransport{
		path:   socketPath,
		isOpen: false,
	}
}

// Open establishes a connection to the Virtual RX72N process via Unix socket.
func (s *SocketTransport) Open() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.isOpen {
		return nil // Idempotent
	}

	// Connect to the "Virtual RX72N" process
	conn, err := net.DialTimeout("unix", s.path, DefaultSocketTimeout)
	if err != nil {
		return fmt.Errorf("failed to connect to socket %s: %w", s.path, err)
	}

	s.conn = conn
	s.isOpen = true
	return nil
}

// Transfer sends txData and receives the response.
// The context deadline is respected for both read and write operations.
//
// NOTE: SPI is synchronous. We expect exactly len(txData) bytes back
// to match the full-duplex behavior of real SPI hardware.
func (s *SocketTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	// Check context cancellation before locking
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	s.mu.Lock()
	defer s.mu.Unlock()

	if !s.isOpen {
		return nil, ErrDeviceNotOpen
	}

	// Set deadline based on context
	if deadline, ok := ctx.Deadline(); ok {
		if err := s.conn.SetDeadline(deadline); err != nil {
			return nil, fmt.Errorf("failed to set socket deadline: %w", err)
		}
		// Reset deadline after operation to avoid affecting future calls
		defer func() {
			_ = s.conn.SetDeadline(time.Time{})
		}()
	}

	// 1. Send the command
	n, err := s.conn.Write(txData)
	if err != nil {
		return nil, fmt.Errorf("socket write failed: %w", err)
	}
	if n != len(txData) {
		return nil, fmt.Errorf("socket write incomplete: wrote %d bytes, expected %d", n, len(txData))
	}

	// 2. Read the response
	// SPI is synchronous - we expect exactly len(txData) bytes back
	rxBuffer := make([]byte, len(txData))
	n, err = io.ReadFull(s.conn, rxBuffer)
	if err != nil {
		return nil, fmt.Errorf("socket read failed: %w", err)
	}
	if n != len(txData) {
		return nil, fmt.Errorf("socket read incomplete: read %d bytes, expected %d", n, len(txData))
	}

	return rxBuffer, nil
}

// Close closes the socket connection.
func (s *SocketTransport) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if !s.isOpen {
		return nil // Idempotent
	}

	if s.conn != nil {
		if err := s.conn.Close(); err != nil {
			return fmt.Errorf("failed to close socket: %w", err)
		}
	}

	s.isOpen = false
	s.conn = nil
	return nil
}

// IsOpen returns whether the socket is currently connected.
func (s *SocketTransport) IsOpen() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.isOpen
}

// Path returns the Unix socket path.
func (s *SocketTransport) Path() string {
	return s.path
}

// Send implements the legacy Transport interface (for compatibility).
// Delegates to Transfer using context.Background() for compatibility with the
// legacy Transport interface.
//
// NOTE: Because this helper uses context.Background(), it does not provide a
// timeout or cancellation mechanism. If the underlying Transfer blocks, this
// call may block indefinitely. For production code prefer calling
// `Transfer` with an explicit context (with timeout or cancellation) to avoid
// hangs and to support cancellation/timeouts.
func (s *SocketTransport) Send(data []byte) (int, error) {
	_, err := s.Transfer(context.Background(), data)
	if err != nil {
		return 0, err
	}
	return len(data), nil
}

// Receive implements the legacy Transport interface (for compatibility).
// Delegates to Transfer using context.Background() and sends zeros to read a
// response (sockets cannot read-only like typical SPI).
//
// NOTE: This helper uses context.Background(), so it does not support timeouts
// or cancellation. If the underlying Transfer blocks, this call may block
// indefinitely. Prefer calling `Transfer` with an explicit context (with
// timeout or cancellation) in production to avoid hangs.
func (s *SocketTransport) Receive(maxLen int) ([]byte, error) {
	// Send dummy bytes (zeros) to trigger a response
	txBuf := make([]byte, maxLen)
	return s.Transfer(context.Background(), txBuf)
}
