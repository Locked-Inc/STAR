// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package transport tests for SPI communication layer.
//
// STAR Project - Texas A&M University
// December 2025
package transport

import (
	"context"
	"errors"
	"testing"
	"time"
)

// ============================================================================
// Constant Tests
// ============================================================================

func TestSPIConstants(t *testing.T) {
	t.Run("DefaultDevice", func(t *testing.T) {
		if DefaultDevice != "/dev/spidev0.0" {
			t.Errorf("DefaultDevice = %s, want /dev/spidev0.0", DefaultDevice)
		}
	})

	t.Run("DefaultSpeedHz", func(t *testing.T) {
		if DefaultSpeedHz != 10_000_000 {
			t.Errorf("DefaultSpeedHz = %d, want 10000000", DefaultSpeedHz)
		}
	})

	t.Run("DefaultMode", func(t *testing.T) {
		if DefaultMode != 0 {
			t.Errorf("DefaultMode = %d, want 0", DefaultMode)
		}
	})

	t.Run("DefaultBitsPerWord", func(t *testing.T) {
		if DefaultBitsPerWord != 8 {
			t.Errorf("DefaultBitsPerWord = %d, want 8", DefaultBitsPerWord)
		}
	})

	t.Run("DefaultTimeout", func(t *testing.T) {
		if DefaultTimeout != 100*time.Millisecond {
			t.Errorf("DefaultTimeout = %v, want 100ms", DefaultTimeout)
		}
	})
}

func TestDefaultConfig(t *testing.T) {
	config := DefaultConfig()

	if config == nil {
		t.Fatal("Expected non-nil default config")
	}

	// Verify default values
	if config.Device != DefaultDevice {
		t.Errorf("Expected device %s, got %s", DefaultDevice, config.Device)
	}
	if config.SpeedHz != DefaultSpeedHz {
		t.Errorf("Expected speed %d, got %d", DefaultSpeedHz, config.SpeedHz)
	}
	if config.Mode != DefaultMode {
		t.Errorf("Expected mode %d, got %d", DefaultMode, config.Mode)
	}
	if config.BitsPerWord != DefaultBitsPerWord {
		t.Errorf("Expected bits per word %d, got %d", DefaultBitsPerWord, config.BitsPerWord)
	}
	if config.Timeout != DefaultTimeout {
		t.Errorf("Expected timeout %v, got %v", DefaultTimeout, config.Timeout)
	}
}

// ============================================================================
// SPITransport Constructor Tests
// ============================================================================

func TestSPITransport_New(t *testing.T) {
	config := DefaultConfig()
	transport := NewSPITransport(config)

	if transport == nil {
		t.Fatal("Expected non-nil transport")
	}

	if transport.IsOpen() {
		t.Error("Expected transport to be closed initially")
	}
}

func TestSPITransport_NewWithNilConfig(t *testing.T) {
	transport := NewSPITransport(nil)

	if transport == nil {
		t.Fatal("Expected non-nil transport with default config")
	}

	// Should use default config
	config := transport.Config()
	if config.Device != DefaultDevice {
		t.Errorf("Expected default device %s, got %s", DefaultDevice, config.Device)
	}
	if config.SpeedHz != DefaultSpeedHz {
		t.Errorf("Expected default speed %d, got %d", DefaultSpeedHz, config.SpeedHz)
	}
}

func TestSPITransport_Config(t *testing.T) {
	config := &SPIConfig{
		Device:      "/dev/spidev0.0",
		SpeedHz:     1_000_000, // 1 MHz for test
		Mode:        0,
		BitsPerWord: 8,
		Timeout:     100 * time.Millisecond,
	}

	transport := NewSPITransport(config)
	gotConfig := transport.Config()

	if gotConfig.SpeedHz != 1_000_000 {
		t.Errorf("Expected SpeedHz 1000000, got %d", gotConfig.SpeedHz)
	}
	if gotConfig.Mode != 0 {
		t.Errorf("Expected Mode 0, got %d", gotConfig.Mode)
	}
	if gotConfig.BitsPerWord != 8 {
		t.Errorf("Expected BitsPerWord 8, got %d", gotConfig.BitsPerWord)
	}
	if gotConfig.Timeout != 100*time.Millisecond {
		t.Errorf("Expected Timeout 100ms, got %v", gotConfig.Timeout)
	}
}

// ============================================================================
// Error Handling Tests (without hardware)
// ============================================================================

func TestSPITransport_SendNotOpen(t *testing.T) {
	transport := NewSPITransport(DefaultConfig())

	_, err := transport.Send([]byte{0x01, 0x02})
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSPITransport_ReceiveNotOpen(t *testing.T) {
	transport := NewSPITransport(DefaultConfig())

	_, err := transport.Receive(10)
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSPITransport_TransferNotOpen(t *testing.T) {
	transport := NewSPITransport(DefaultConfig())

	_, err := transport.Transfer(context.Background(), []byte{0x01})
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

func TestSPITransport_CloseNotOpen(t *testing.T) {
	transport := NewSPITransport(DefaultConfig())

	// Close when not open should be idempotent
	err := transport.Close()
	if err != nil {
		t.Errorf("Expected no error closing unopened transport, got %v", err)
	}
}

func TestSPITransport_SetReadDeadline(t *testing.T) {
	transport := NewSPITransport(DefaultConfig())

	// Test setting a future deadline
	futureDeadline := time.Now().Add(1 * time.Hour)
	err := transport.SetReadDeadline(futureDeadline)
	if err != nil {
		t.Errorf("SetReadDeadline failed: %v", err)
	}

	// Test setting a past deadline
	pastDeadline := time.Now().Add(-1 * time.Hour)
	err = transport.SetReadDeadline(pastDeadline)
	if err != nil {
		t.Errorf("SetReadDeadline failed: %v", err)
	}

	// Test setting a zero deadline (no deadline)
	err = transport.SetReadDeadline(time.Time{})
	if err != nil {
		t.Errorf("SetReadDeadline failed: %v", err)
	}
}

func TestSPITransport_ReceiveWithExpiredDeadline(t *testing.T) {
	transport := NewSPITransport(DefaultConfig())

	// Set a deadline in the past
	pastDeadline := time.Now().Add(-1 * time.Second)
	err := transport.SetReadDeadline(pastDeadline)
	if err != nil {
		t.Fatalf("SetReadDeadline failed: %v", err)
	}

	// Attempt to receive - should fail with timeout even without opening device
	// Note: This still won't work because device is not open
	_, err = transport.Receive(10)
	// Should get ErrDeviceNotOpen first (checked before deadline)
	if err != ErrDeviceNotOpen {
		t.Errorf("Expected ErrDeviceNotOpen, got %v", err)
	}
}

// ============================================================================
// Hardware Integration Tests (skip with -short flag)
// These require actual /dev/spidev0.0 device on Raspberry Pi 5
// ============================================================================

func TestSPITransport_OpenClose(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	transport := NewSPITransport(DefaultConfig())

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
	}
	defer transport.Close()

	if !transport.IsOpen() {
		t.Error("Expected transport to be open")
	}

	// Test idempotent open
	err = transport.Open()
	if err != nil {
		t.Errorf("Expected idempotent Open() to succeed, got %v", err)
	}

	// Close
	err = transport.Close()
	if err != nil {
		t.Errorf("Close failed: %v", err)
	}

	if transport.IsOpen() {
		t.Error("Expected transport to be closed")
	}

	// Test idempotent close
	err = transport.Close()
	if err != nil {
		t.Errorf("Expected idempotent Close() to succeed, got %v", err)
	}
}

func TestSPITransport_SendReceive(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	transport := NewSPITransport(DefaultConfig())

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
	}
	defer transport.Close()

	// Test Send
	testData := []byte{0x55, 0xAA, 0x01, 0x02}
	n, err := transport.Send(testData)
	if err != nil {
		t.Fatalf("Send failed: %v", err)
	}
	if n != len(testData) {
		t.Errorf("Expected to send %d bytes, sent %d", len(testData), n)
	}

	// Test Receive
	rxData, err := transport.Receive(4)
	if err != nil {
		t.Fatalf("Receive failed: %v", err)
	}
	if len(rxData) != 4 {
		t.Errorf("Expected 4 bytes, got %d", len(rxData))
	}
}

func TestSPITransport_Transfer(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	transport := NewSPITransport(DefaultConfig())

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
	}
	defer transport.Close()

	// Test full-duplex transfer
	txData := []byte{0x55, 0xAA, 0x01, 0x02}
	rxData, err := transport.Transfer(context.Background(), txData)
	if err != nil {
		t.Fatalf("Transfer failed: %v", err)
	}
	if len(rxData) != len(txData) {
		t.Errorf("Expected %d bytes, got %d", len(txData), len(rxData))
	}

	// Verify rxData is not nil and has correct length
	if rxData == nil {
		t.Error("Expected non-nil rxData")
	}
}

func TestSPITransport_LargeTransfer(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	transport := NewSPITransport(DefaultConfig())

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
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
}

func TestSPITransport_MultipleOperations(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	transport := NewSPITransport(DefaultConfig())

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
	}
	defer transport.Close()

	// Perform multiple operations in sequence
	for i := range 10 {
		testData := []byte{byte(i), 0xAA, 0x01, 0x02}

		// Send
		_, err := transport.Send(testData)
		if err != nil {
			t.Fatalf("Send %d failed: %v", i, err)
		}

		// Receive
		_, err = transport.Receive(4)
		if err != nil {
			t.Fatalf("Receive %d failed: %v", i, err)
		}

		// Transfer
		_, err = transport.Transfer(context.Background(), testData)
		if err != nil {
			t.Fatalf("Transfer %d failed: %v", i, err)
		}
	}
}

func TestSPITransport_EmptyTransfer(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	transport := NewSPITransport(DefaultConfig())

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
	}
	defer transport.Close()

	// Test with empty data
	txData := []byte{}
	rxData, err := transport.Transfer(context.Background(), txData)
	if err != nil {
		t.Fatalf("Empty transfer failed: %v", err)
	}
	if len(rxData) != 0 {
		t.Errorf("Expected 0 bytes for empty transfer, got %d", len(rxData))
	}
}

func TestSPITransport_CustomConfig(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping hardware test in short mode")
	}

	// Test with custom configuration
	config := &SPIConfig{
		Device:      "/dev/spidev0.0",
		SpeedHz:     1_000_000, // 1 MHz
		Mode:        0,
		BitsPerWord: 8,
		Timeout:     50 * time.Millisecond,
	}

	transport := NewSPITransport(config)

	err := transport.Open()
	if err != nil {
		t.Skipf("Skipping test: SPI device not available: %v", err)
	}
	defer transport.Close()

	// Verify we can still perform transfers with custom config
	testData := []byte{0x01, 0x02, 0x03, 0x04}
	_, err = transport.Transfer(context.Background(), testData)
	if err != nil {
		t.Fatalf("Transfer with custom config failed: %v", err)
	}
}

// ============================================================================
// Mock-based tests (run without hardware)
// These tests use mocks to achieve full coverage without requiring /dev/spidev0.0
// ============================================================================

func TestSPITransport_MockSend_Success(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	testData := []byte{0x55, 0xAA, 0x01, 0x02}
	n, err := transport.Send(testData)
	if err != nil {
		t.Fatalf("Send failed: %v", err)
	}
	if n != len(testData) {
		t.Errorf("Expected to send %d bytes, got %d", len(testData), n)
	}

	// Verify mock received the data
	if mockPort.conn.txCallCount != 1 {
		t.Errorf("Expected 1 Tx call, got %d", mockPort.conn.txCallCount)
	}
}

func TestSPITransport_MockSend_Failure(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Inject error into the connection after it's created
	mockPort.conn.mu.Lock()
	mockPort.conn.txError = errors.New("SPI transmission error")
	mockPort.conn.mu.Unlock()

	testData := []byte{0x01, 0x02}
	_, err = transport.Send(testData)
	if err == nil {
		t.Error("Expected Send to fail with mock error")
	}
	if err.Error() != "SPI send failed: SPI transmission error" {
		t.Errorf("Unexpected error: %v", err)
	}
}

func TestSPITransport_MockReceive_Success(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Set mock response data
	mockPort.conn.rxData = []byte{0xDE, 0xAD, 0xBE, 0xEF}

	rxData, err := transport.Receive(4)
	if err != nil {
		t.Fatalf("Receive failed: %v", err)
	}
	if len(rxData) != 4 {
		t.Errorf("Expected 4 bytes, got %d", len(rxData))
	}
	// Check first byte matches mock data
	if rxData[0] != 0xDE {
		t.Errorf("Expected first byte 0xDE, got 0x%02X", rxData[0])
	}
}

func TestSPITransport_MockReceive_WithDeadline_NotExpired(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Set future deadline
	futureDeadline := time.Now().Add(1 * time.Hour)
	err = transport.SetReadDeadline(futureDeadline)
	if err != nil {
		t.Fatalf("SetReadDeadline failed: %v", err)
	}

	// Should succeed because deadline hasn't passed
	_, err = transport.Receive(10)
	if err != nil {
		t.Fatalf("Receive should succeed with future deadline: %v", err)
	}
}

func TestSPITransport_MockReceive_WithDeadline_Expired(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Set past deadline
	pastDeadline := time.Now().Add(-1 * time.Second)
	err = transport.SetReadDeadline(pastDeadline)
	if err != nil {
		t.Fatalf("SetReadDeadline failed: %v", err)
	}

	// Should fail with timeout
	_, err = transport.Receive(10)
	if err != ErrTimeout {
		t.Errorf("Expected ErrTimeout with expired deadline, got %v", err)
	}
}

func TestSPITransport_MockReceive_Failure(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Inject error into the connection after it's created
	mockPort.conn.mu.Lock()
	mockPort.conn.txError = errors.New("SPI receive error")
	mockPort.conn.mu.Unlock()

	_, err = transport.Receive(10)
	if err == nil {
		t.Error("Expected Receive to fail with mock error")
	}
	if err.Error() != "SPI receive failed: SPI receive error" {
		t.Errorf("Unexpected error: %v", err)
	}
}

func TestSPITransport_MockTransfer_Success(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	txData := []byte{0x01, 0x02, 0x03, 0x04}
	rxData, err := transport.Transfer(context.Background(), txData)
	if err != nil {
		t.Fatalf("Transfer failed: %v", err)
	}
	if len(rxData) != len(txData) {
		t.Errorf("Expected %d bytes, got %d", len(txData), len(rxData))
	}
}

func TestSPITransport_MockTransfer_ContextCanceled(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Create canceled context
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	txData := []byte{0x01, 0x02}
	_, err = transport.Transfer(ctx, txData)
	if err == nil {
		t.Error("Expected Transfer to fail with canceled context")
	}
	if !errors.Is(err, context.Canceled) {
		t.Errorf("Expected context.Canceled error, got %v", err)
	}
}

func TestSPITransport_MockTransfer_Failure(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Inject error into the connection after it's created
	mockPort.conn.mu.Lock()
	mockPort.conn.txError = errors.New("SPI transfer error")
	mockPort.conn.mu.Unlock()

	_, err = transport.Transfer(context.Background(), []byte{0x01})
	if err == nil {
		t.Error("Expected Transfer to fail with mock error")
	}
	if err.Error() != "SPI transfer failed: SPI transfer error" {
		t.Errorf("Unexpected error: %v", err)
	}
}

func TestSPITransport_MockClose_Success(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}

	if !transport.IsOpen() {
		t.Error("Expected transport to be open")
	}

	err = transport.Close()
	if err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	if transport.IsOpen() {
		t.Error("Expected transport to be closed")
	}

	// Verify port.Close() was called
	if mockPort.closeCount != 1 {
		t.Errorf("Expected 1 Close call, got %d", mockPort.closeCount)
	}

	// Verify idempotent close
	err = transport.Close()
	if err != nil {
		t.Errorf("Second Close should be idempotent, got error: %v", err)
	}
}

func TestSPITransport_MockClose_Failure(t *testing.T) {
	mockPort := newMockSPIPort()
	mockPort.closeError = errors.New("failed to close SPI port")

	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}

	err = transport.Close()
	if err == nil {
		t.Error("Expected Close to fail with mock error")
	}
	if !errors.Is(err, mockPort.closeError) {
		t.Errorf("Unexpected error: %v, want %v", err, mockPort.closeError)
	}
}

func TestSPITransport_MockOpen_ConnectFailure(t *testing.T) {
	mockPort := newMockSPIPort()
	mockPort.connectErr = errors.New("failed to configure SPI")

	_, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err == nil {
		t.Error("Expected newSPITransportWithMockPort to fail with connect error")
	}
	if !errors.Is(err, mockPort.connectErr) {
		t.Errorf("Expected connect error, got: %v", err)
	}
}

func TestSPITransport_MockMultipleOperations(t *testing.T) {
	const iterations = 5 // Number of Send/Receive/Transfer cycles

	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Perform multiple different operations
	for i := range iterations {
		testData := []byte{byte(i), 0xAA}

		// Send
		n, err := transport.Send(testData)
		if err != nil {
			t.Fatalf("Iteration %d: Send failed: %v", i, err)
		}
		if n != len(testData) {
			t.Errorf("Iteration %d: Expected to send %d bytes, got %d", i, len(testData), n)
		}

		// Receive
		rxData, err := transport.Receive(2)
		if err != nil {
			t.Fatalf("Iteration %d: Receive failed: %v", i, err)
		}
		if len(rxData) != 2 {
			t.Errorf("Iteration %d: Expected 2 bytes, got %d", i, len(rxData))
		}

		// Transfer
		txData := []byte{byte(i * 2), byte(i * 3)}
		rxData, err = transport.Transfer(context.Background(), txData)
		if err != nil {
			t.Fatalf("Iteration %d: Transfer failed: %v", i, err)
		}
		if len(rxData) != len(txData) {
			t.Errorf("Iteration %d: Expected %d bytes, got %d", i, len(txData), len(rxData))
		}
	}
}

func TestSPITransport_MockEmptyTransfer(t *testing.T) {
	mockPort := newMockSPIPort()
	transport, err := newSPITransportWithMockPort(DefaultConfig(), mockPort)
	if err != nil {
		t.Fatalf("Failed to create transport with mock: %v", err)
	}
	defer transport.Close()

	// Test with empty data
	txData := []byte{}
	rxData, err := transport.Transfer(context.Background(), txData)
	if err != nil {
		t.Fatalf("Empty transfer failed: %v", err)
	}
	if len(rxData) != 0 {
		t.Errorf("Expected 0 bytes for empty transfer, got %d", len(rxData))
	}

	// Test empty send
	n, err := transport.Send([]byte{})
	if err != nil {
		t.Fatalf("Empty send failed: %v", err)
	}
	if n != 0 {
		t.Errorf("Expected to send 0 bytes, got %d", n)
	}
}
