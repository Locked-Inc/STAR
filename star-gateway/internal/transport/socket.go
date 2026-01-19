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
	"net"
	"sync"
	"time"
)

// Default socket configuration.
const (
	// DefaultSocketPath is the Unix domain socket path for Virtual RX72N.
	DefaultSocketPath = "/tmp/star_rx72n.sock"

	// DefaultSocketTimeout is the default I/O timeout for socket operations.
	DefaultSocketTimeout = 100 * time.Millisecond
)

// SocketTransport implements the Device interface using Unix Domain Sockets.
// It connects to a Virtual RX72N simulator for Hardware-in-the-Loop testing.
type SocketTransport struct {
	mu   sync.RWMutex // protects conn, isOpen
	conn net.Conn
	path string
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
	conn, err := net.Dial("unix", s.path)
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
// or a specific frame size depending on your protocol.
func (s *SocketTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if !s.isOpen {
		return nil, ErrDeviceNotOpen
	}

	// Set deadline based on context
	if deadline, ok := ctx.Deadline(); ok {
		if err := s.conn.SetDeadline(deadline); err != nil {
			return nil, fmt.Errorf("failed to set deadline: %w", err)
		}
		// Reset deadline after operation
		defer s.conn.SetDeadline(time.Time{})
	}

	// 1. Send the command
	_, err := s.conn.Write(txData)
	if err != nil {
		return nil, fmt.Errorf("socket write failed: %w", err)
	}

	// 2. Read the response
	// SPI is synchronous. We expect exactly len(txData) bytes back
	// or a specific frame size depending on your protocol.
	rxBuffer := make([]byte, len(txData))
	_, err = s.conn.Read(rxBuffer)
	if err != nil {
		return nil, fmt.Errorf("socket read failed: %w", err)
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
// Delegates to Transfer with background context.
func (s *SocketTransport) Send(data []byte) (int, error) {
	_, err := s.Transfer(context.Background(), data)
	if err != nil {
		return 0, err
	}
	return len(data), nil
}

// Receive implements the legacy Transport interface (for compatibility).
// For sockets, we can't "receive only" like SPI dummy bytes, so this
// sends zeros and reads the response.
func (s *SocketTransport) Receive(maxLen int) ([]byte, error) {
	// Send dummy bytes (zeros) to trigger a response
	txBuf := make([]byte, maxLen)
	return s.Transfer(context.Background(), txBuf)
}
