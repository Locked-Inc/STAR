// Package transport tests for Socket communication layer.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"context"
	"testing"
	"time"
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
		transport.Close()
	}
}

func TestSocketTransport_TransferNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	_, err := transport.Transfer(context.Background(), []byte{0x01})
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSocketTransport_SendNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	_, err := transport.Send([]byte{0x01, 0x02})
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSocketTransport_ReceiveNotOpen(t *testing.T) {
	transport := NewSocketTransport("/tmp/test.sock")

	_, err := transport.Receive(10)
	if err != ErrDeviceNotOpen {
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
	transport := NewSocketTransport("/tmp/test.sock")

	// Create a canceled context
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	// Even if not open, context error should be checked
	// In this case, we'll get ErrDeviceNotOpen first since that check happens first
	_, err := transport.Transfer(ctx, []byte{0x01})
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
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

	// Virtual RX72N should modify first byte to 0xFF
	if rxData[0] != 0xFF {
		t.Errorf("Expected first byte 0xFF (simulator marker), got 0x%02X", rxData[0])
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
	defer transport.Close()

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

	// First byte should be modified
	if rxData[0] != 0xFF {
		t.Errorf("Expected first byte 0xFF, got 0x%02X", rxData[0])
	}
}

func TestSocketTransport_ContextTimeout(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	transport := NewSocketTransport(DefaultSocketPath)

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: Virtual RX72N not running: %v", err)
	}
	defer transport.Close()

	// Create context with very short timeout
	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Nanosecond)
	defer cancel()

	// Wait for timeout
	time.Sleep(10 * time.Millisecond)

	// Transfer should fail with timeout
	_, err = transport.Transfer(ctx, []byte{0x01, 0x02})
	if err == nil {
		t.Error("Expected timeout error")
	}
}
