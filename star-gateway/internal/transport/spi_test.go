// Package transport tests for SPI communication layer.
//
// STAR Project - Texas A&M University
// December 2025
package transport

import (
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

	_, err := transport.Transfer([]byte{0x01})
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
	rxData, err := transport.Transfer(txData)
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

	rxData, err := transport.Transfer(txData)
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
	for i := 0; i < 10; i++ {
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
		_, err = transport.Transfer(testData)
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
	rxData, err := transport.Transfer(txData)
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
	_, err = transport.Transfer(testData)
	if err != nil {
		t.Fatalf("Transfer with custom config failed: %v", err)
	}
}
