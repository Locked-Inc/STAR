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
type SocketTransport struct {
	mu     sync.RWMutex
	conn   net.Conn
	path   string
	isOpen bool
}

// NewSocketTransport creates a new SocketTransport with the given socket path.
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
		return nil
	}

	conn, err := net.DialTimeout("unix", s.path, DefaultSocketTimeout)
	if err != nil {
		return fmt.Errorf("failed to connect to socket %s: %w", s.path, err)
	}

	s.conn = conn
	s.isOpen = true
	return nil
}

// Transfer sends txData and receives the response.
//
// IMPORTANT: This simulates SPI full-duplex behavior where EXACTLY TransferSize
// bytes are exchanged in both directions, regardless of payload size.
// txData is padded to TransferSize if needed, and exactly TransferSize bytes
// are read back.
func (s *SocketTransport) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
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
		defer func() {
			_ = s.conn.SetDeadline(time.Time{})
		}()
	}

	// Pad txData to TransferSize (simulate SPI COPI line)
	txBuffer := make([]byte, TransferSize)
	copy(txBuffer, txData)

	// Write exactly TransferSize bytes
	n, err := s.conn.Write(txBuffer)
	if err != nil {
		return nil, fmt.Errorf("socket write failed: %w", err)
	}
	if n != TransferSize {
		return nil, fmt.Errorf("socket write incomplete: wrote %d bytes, expected %d", n, TransferSize)
	}

	// Read exactly TransferSize bytes (simulate SPI CIPO line)
	rxBuffer := make([]byte, TransferSize)
	n, err = io.ReadFull(s.conn, rxBuffer)
	if err != nil {
		return nil, fmt.Errorf("socket read failed: %w", err)
	}
	if n != TransferSize {
		return nil, fmt.Errorf("socket read incomplete: read %d bytes, expected %d", n, TransferSize)
	}

	return rxBuffer, nil
}

// Close closes the socket connection.
func (s *SocketTransport) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if !s.isOpen {
		return nil
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

// Send implements the legacy Transport interface.
// Pads data to TransferSize and sends, discarding the response.
func (s *SocketTransport) Send(data []byte) (int, error) {
	_, err := s.Transfer(context.Background(), data)
	if err != nil {
		return 0, err
	}
	return len(data), nil // Return original data length, not padded
}

// Receive implements the legacy Transport interface.
// Sends zeros and receives TransferSize bytes response.
// Returns up to maxLen bytes from the response.
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
