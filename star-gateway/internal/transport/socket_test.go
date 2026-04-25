// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport tests for Socket communication layer.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"context"
	"errors"
	"io"
	"net"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"
)

const (
	// SimulatorMarker is the expected first byte from Virtual RX72N (must match virtual_rx72n/main.go).
	SimulatorMarker = 0xFF

	// Test delays and timeouts.
	slowServerDelay          = 200 * time.Millisecond
	shortContextTimeout      = 50 * time.Millisecond
	transferContextTimeout   = 500 * time.Millisecond
	openCloseInterleaveDelay = 5 * time.Millisecond
)

// ============================================================================
// Constant Tests
// ============================================================================

func TestSocketConstants(t *testing.T) {
	t.Run("DefaultSocketPath", func(t *testing.T) {
		if DefaultSocketPath != "/tmp/star_rx72n.sock" {
			t.Errorf("DefaultSocketPath = %s, want /tmp/star_rx72n.sock", DefaultSocketPath)
		}
	})

	t.Run("DefaultSocketTimeout", func(t *testing.T) {
		if DefaultSocketTimeout != 100*time.Millisecond {
			t.Errorf("DefaultSocketTimeout = %v, want 100ms", DefaultSocketTimeout)
		}
	})
}

// ============================================================================
// SocketTransport Constructor Tests
// ============================================================================

func TestSocketTransport_New(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	if transport == nil {
		t.Fatal("Expected non-nil transport")
	}

	if transport.IsOpen() {
		t.Error("Expected transport to be closed initially")
	}

	if transport.Path() != "/tmp/test.sock" {
		t.Errorf("Expected path /tmp/test.sock, got %s", transport.Path())
	}
}

func TestSocketTransport_NewWithEmptyPath(t *testing.T) {
	transport := NewSocketTransport("")

	if transport == nil {
		t.Fatal("Expected non-nil transport with default path")
	}

	// Should use default path
	if transport.Path() != DefaultSocketPath {
		t.Errorf("Expected default path %s, got %s", DefaultSocketPath, transport.Path())
	}
}

// ============================================================================
// Error Handling Tests (without server)
// ============================================================================

func TestSocketTransport_OpenNoServer(t *testing.T) {
	transport := NewSocketTransport("/tmp/nonexistent_socket_12345.sock")

	err := transport.Open()
	if err == nil {
		t.Error("Expected error when connecting to non-existent socket")
		err := transport.Close()
		if err != nil {
			t.Errorf("Failed to close transport: %v", err)
		}
	}
}

func TestSocketTransport_TransferNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	_, err := transport.Transfer(context.Background(), []byte{0x01})
	if !errors.Is(err, ErrDeviceNotOpen) {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSocketTransport_SendNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	_, err := transport.Send([]byte{0x01, 0x02})
	if !errors.Is(err, ErrDeviceNotOpen) {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSocketTransport_ReceiveNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	_, err := transport.Receive(10)
	if !errors.Is(err, ErrDeviceNotOpen) {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSocketTransport_CloseNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	// Close when not open should be idempotent
	err := transport.Close()
	if err != nil {
		t.Errorf("Expected no error closing unopened transport, got %v", err)
	}
}

func TestSocketTransport_ContextCanceled(t *testing.T) {
	// Create a temporary listener so Open() succeeds
	// Use explicit short path for macOS socket length limits
	dir, err := os.MkdirTemp("/tmp", "star_socket_test")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	defer func(path string) {
		err := os.RemoveAll(path)
		if err != nil {
			t.Errorf("Failed to remove temp dir: %v", err)
		}
	}(dir)

	socketPath := filepath.Join(dir, "test.sock")
	ln, err := net.Listen("unix", socketPath)
	if err != nil {
		t.Fatalf("Failed to create temporary listener: %v", err)
	}
	defer func(ln net.Listener) {
		err := ln.Close()
		if err != nil {
			t.Errorf("Failed to close listener: %v", err)
		}
	}(ln)

	transport := NewSocketTransport(socketPath)
	if err := transport.Open(); err != nil {
		t.Fatalf("Failed to open transport: %v", err)
	}
	defer func(transport *SocketTransport) {
		err := transport.Close()
		if err != nil {
			t.Errorf("Failed to close transport: %v", err)
		}
	}(transport)

	// Create a canceled context
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	// Context error should be checked before operation
	_, err = transport.Transfer(ctx, []byte{0x01})
	if !errors.Is(err, context.Canceled) {
		t.Errorf("Expected context.Canceled, got %v", err)
	}
}

func TestSocketTransport_SendReceive(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	dir, err := os.MkdirTemp("/tmp", "star_socket_test_sr")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	defer func(path string) {
		err := os.RemoveAll(path)
		if err != nil {
			t.Errorf("Failed to remove temp dir: %v", err)
		}
	}(dir)

	socketPath := filepath.Join(dir, "test.sock")
	ln, err := net.Listen("unix", socketPath)
	if err != nil {
		t.Fatalf("Failed to create temporary listener: %v", err)
	}
	defer func(ln net.Listener) {
		err := ln.Close()
		if err != nil {
			t.Errorf("Failed to close listener: %v", err)
		}
	}(ln)

	go func() {
		conn, err := ln.Accept()
		if err != nil {
			return
		}
		defer func(conn net.Conn) {
			err := conn.Close()
			if err != nil {
				t.Errorf("Failed to close connection: %v", err)
			}
		}(conn)
		_, _ = io.Copy(conn, conn) // Echo
	}()

	transport := NewSocketTransport(socketPath)
	if err := transport.Open(); err != nil {
		t.Fatalf("Failed to open transport: %v", err)
	}
	defer func(transport *SocketTransport) {
		err := transport.Close()
		if err != nil {
			t.Errorf("Failed to close transport: %v", err)
		}
	}(transport)

	// Test Send
	n, err := transport.Send([]byte{0x01, 0x02})
	if err != nil {
		t.Errorf("Send failed: %v", err)
	}
	if n != 2 {
		t.Errorf("Expected 2 bytes sent, got %d", n)
	}

	// Test Receive
	data, err := transport.Receive(2)
	if err != nil {
		t.Errorf("Receive failed: %v", err)
	}
	if len(data) != 2 {
		t.Errorf("Expected 2 bytes received, got %d", len(data))
	}

	// Test Send error
	transport.Close()
	_, err = transport.Send([]byte{0x01})
	if err == nil {
		t.Error("Expected error on Send to closed transport")
	}
}

func TestSocketTransport_TransferErrors(t *testing.T) {
	dir, err := os.MkdirTemp("/tmp", "star_socket_test_err")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	defer os.RemoveAll(dir)

	socketPath := filepath.Join(dir, "test.sock")
	ln, err := net.Listen("unix", socketPath)
	if err != nil {
		t.Fatalf("Failed to create temporary listener: %v", err)
	}

	transport := NewSocketTransport(socketPath)
	if err := transport.Open(); err != nil {
		t.Fatalf("Failed to open transport: %v", err)
	}

	// Case 1: Write failure (server closed)
	ln.Close()
	transport.conn.Close() // Force close

	_, err = transport.Transfer(context.Background(), []byte{0x01})
	if err == nil {
		t.Error("Expected error on write to closed connection")
	}

	transport.Close()

	// Case 2: Read failure (incomplete)
	ln2, _ := net.Listen("unix", socketPath)
	defer func(ln2 net.Listener) {
		err := ln2.Close()
		if err != nil {
			t.Errorf("Failed to close listener: %v", err)
		}
	}(ln2)

	transport2 := NewSocketTransport(socketPath)
	if err := transport2.Open(); err != nil {
		t.Errorf("Failed to open transport2: %v", err)
	}
	defer transport2.Close()

	go func() {
		conn, _ := ln2.Accept()
		_, _ = conn.Read(make([]byte, 10))
		conn.Close() // Close without writing enough back
	}()

	_, err = transport2.Transfer(context.Background(), []byte{0x01, 0x02})
	if err == nil {
		t.Error("Expected error on incomplete read")
	}
}

// ============================================================================
// Integration Tests (require Virtual RX72N running)
// These are skipped with -short flag
// ============================================================================

// Note: To run these tests, first start the Virtual RX72N in another terminal:
//   go run ./cmd/virtual_rx72n/

func TestSocketTransport_WithVirtualRX72N(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	transport := NewSocketTransport(DefaultSocketPath)

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: Virtual RX72N not running: %v", err)
	}
	defer transport.Close()

	if !transport.IsOpen() {
		t.Error("Expected transport to be open")
	}

	// Test basic transfer
	txData := []byte{0x55, 0xAA, 0x01, 0x02}
	rxData, err := transport.Transfer(context.Background(), txData)
	if err != nil {
		t.Fatalf("Transfer failed: %v", err)
	}

	if len(rxData) != len(txData) {
		t.Errorf("Expected %d bytes, got %d", len(txData), len(rxData))
	}

	// Virtual RX72N should modify first byte to SimulatorMarker
	if rxData[0] != SimulatorMarker {
		t.Errorf("Expected first byte 0x%02X (simulator marker), got 0x%02X", SimulatorMarker, rxData[0])
	}

	// Rest should be echoed
	for i := 1; i < len(rxData); i++ {
		if rxData[i] != txData[i] {
			t.Errorf("Byte %d: expected 0x%02X, got 0x%02X", i, txData[i], rxData[i])
		}
	}
}

func TestSocketTransport_MultipleTransfers(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	transport := NewSocketTransport(DefaultSocketPath)

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: Virtual RX72N not running: %v", err)
	}
	defer func(transport *SocketTransport) {
		err := transport.Close()
		if err != nil {
			t.Errorf("Failed to close transport: %v", err)
		}
	}(transport)

	// Perform multiple transfers
	for i := 0; i < 10; i++ {
		testData := []byte{byte(i), 0xAA, 0x01, 0x02}

		rxData, err := transport.Transfer(context.Background(), testData)
		if err != nil {
			t.Fatalf("Transfer %d failed: %v", i, err)
		}

		if len(rxData) != len(testData) {
			t.Errorf("Transfer %d: expected %d bytes, got %d", i, len(testData), len(rxData))
		}
	}
}

func TestSocketTransport_LargeTransfer(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	transport := NewSocketTransport(DefaultSocketPath)

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: Virtual RX72N not running: %v", err)
	}
	defer transport.Close()

	// Test with larger data (1KB)
	txData := make([]byte, 1024)
	for i := range txData {
		txData[i] = byte(i % 256)
	}

	rxData, err := transport.Transfer(context.Background(), txData)
	if err != nil {
		t.Fatalf("Large transfer failed: %v", err)
	}

	if len(rxData) != len(txData) {
		t.Errorf("Expected %d bytes, got %d", len(txData), len(rxData))
	}

	// First byte should be modified to SimulatorMarker
	if rxData[0] != SimulatorMarker {
		t.Errorf("Expected first byte 0x%02X (simulator marker), got 0x%02X", SimulatorMarker, rxData[0])
	}
}

func TestSocketTransport_ContextTimeout(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create a temporary Unix socket listener that intentionally delays response
	// Use explicit short path for macOS socket length limits
	dir, err := os.MkdirTemp("/tmp", "star_socket_test")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	defer os.RemoveAll(dir)

	socketPath := filepath.Join(dir, "slow.sock")
	ln, err := net.Listen("unix", socketPath)
	if err != nil {
		t.Fatalf("Failed to create temporary listener: %v", err)
	}
	defer ln.Close()

	// Accept and handle one connection with an artificial delay to trigger timeout
	go func() {
		conn, err := ln.Accept()
		if err != nil {
			return
		}
		defer conn.Close()

		buf := make([]byte, 1024)
		n, err := conn.Read(buf)
		if err != nil {
			return
		}

		// Delay longer than the test context deadline to force a timeout
		time.Sleep(slowServerDelay)

		// Echo back the received bytes
		if _, err := conn.Write(buf[:n]); err != nil {
			return
		}
	}()

	transport := NewSocketTransport(socketPath)

	err = transport.Open()
	if err != nil {
		t.Fatalf("Failed to open transport to temporary listener: %v", err)
	}
	defer transport.Close()

	// Create a context with a short deadline that will expire while waiting for the response
	ctx, cancel := context.WithTimeout(context.Background(), shortContextTimeout)
	defer cancel()

	_, err = transport.Transfer(ctx, []byte{0x01, 0x02, 0x03})
	if err == nil {
		t.Error("Expected timeout error")
		return
	}

	// Accept either context.DeadlineExceeded or a wrapped net timeout
	var ne net.Error
	if !errors.Is(err, context.DeadlineExceeded) {
		if !errors.As(err, &ne) || !ne.Timeout() {
			t.Errorf("Expected timeout error, got: %v", err)
		}
	}
}

func TestSocketTransport_ConcurrentAccess(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	transport := NewSocketTransport(DefaultSocketPath)

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: Virtual RX72N not running: %v", err)
	}
	defer func(transport *SocketTransport) {
		err := transport.Close()
		if err != nil {
			t.Errorf("Failed to close transport: %v", err)
		}
	}(transport)

	// Parameters for concurrency
	const (
		numTransferers    = 8
		transfersPerGorun = 20
		numOpenClose      = 4
		openCloseIters    = 30
	)

	var wg sync.WaitGroup

	// Launch transfer goroutines
	for i := 0; i < numTransferers; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			for j := 0; j < transfersPerGorun; j++ {
				tx := []byte{byte(id), byte(j), 0xAA}
				ctx, cancel := context.WithTimeout(context.Background(), transferContextTimeout)
				rx, err := transport.Transfer(ctx, tx)
				cancel()
				if err != nil {
					// During concurrent Open/Close, ErrDeviceNotOpen or connection reset is expected
					if errors.Is(err, ErrDeviceNotOpen) || errors.Is(err, io.EOF) {
						continue
					}
					var netErr net.Error
					if errors.As(err, &netErr) {
						continue
					}
					t.Errorf("Transfer goroutine %d iteration %d: %v", id, j, err)
					return
				}
				if len(rx) != len(tx) {
					t.Errorf("Transfer goroutine %d iteration %d: expected %d bytes, got %d", id, j, len(tx), len(rx))
					return
				}
			}
		}(i)
	}

	// Launch concurrent Open/Close goroutines
	for i := 0; i < numOpenClose; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			for k := 0; k < openCloseIters; k++ {
				if id%2 == 0 {
					if err := transport.Close(); err != nil {
						// Ignore errors that are expected during concurrent close/transfer races
						var netErr net.Error
						if !errors.As(err, &netErr) && !errors.Is(err, io.EOF) {
							t.Errorf("Open/Close goroutine %d iteration %d: Close error: %v", id, k, err)
						}
					}
				} else {
					if err := transport.Open(); err != nil {
						// Ignore errors that are expected during concurrent open/transfer races
						var netErr net.Error
						if !errors.As(err, &netErr) {
							t.Errorf("Open/Close goroutine %d iteration %d: Open error: %v", id, k, err)
						}
					}
				}
				// Small sleep to interleave operations
				time.Sleep(openCloseInterleaveDelay)
			}
		}(i)
	}

	wg.Wait()
}
