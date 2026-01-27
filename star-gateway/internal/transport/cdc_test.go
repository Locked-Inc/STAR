// Package transport tests for USB CDC transport.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"context"
	"testing"
	"time"
)

// TestDefaultCDCConfig verifies the default CDC configuration.
func TestDefaultCDCConfig(t *testing.T) {
	cfg := DefaultCDCConfig()

	if cfg.Device != DefaultCDCDevice {
		t.Errorf("expected device %s, got %s", DefaultCDCDevice, cfg.Device)
	}

	if cfg.BaudRate != DefaultBaudRate {
		t.Errorf("expected baud rate %d, got %d", DefaultBaudRate, cfg.BaudRate)
	}

	if cfg.Timeout != DefaultCDCTimeout {
		t.Errorf("expected timeout %v, got %v", DefaultCDCTimeout, cfg.Timeout)
	}

	if cfg.VID != RenesasVID {
		t.Errorf("expected VID 0x%04X, got 0x%04X", RenesasVID, cfg.VID)
	}

	if cfg.PID != RX72NPID {
		t.Errorf("expected PID 0x%04X, got 0x%04X", RX72NPID, cfg.PID)
	}
}

// TestNewCDCTransport verifies CDCTransport construction.
func TestNewCDCTransport(t *testing.T) {
	t.Run("WithConfig", func(t *testing.T) {
		cfg := &CDCConfig{
			Device:   "/dev/ttyACM1",
			BaudRate: 9600,
			Timeout:  50 * time.Millisecond,
			VID:      0x1234,
			PID:      0x5678,
		}

		cdc := NewCDCTransport(cfg)

		if cdc == nil {
			t.Fatal("expected non-nil CDCTransport")
		}

		if cdc.config.Device != cfg.Device {
			t.Errorf("expected device %s, got %s", cfg.Device, cdc.config.Device)
		}

		if cdc.config.BaudRate != cfg.BaudRate {
			t.Errorf("expected baud rate %d, got %d", cfg.BaudRate, cdc.config.BaudRate)
		}

		if cdc.IsOpen() {
			t.Error("expected transport to be closed initially")
		}
	})

	t.Run("WithNilConfig", func(t *testing.T) {
		cdc := NewCDCTransport(nil)

		if cdc == nil {
			t.Fatal("expected non-nil CDCTransport")
		}

		// Should use default config
		if cdc.config.Device != DefaultCDCDevice {
			t.Errorf("expected default device %s, got %s", DefaultCDCDevice, cdc.config.Device)
		}

		if cdc.config.BaudRate != DefaultBaudRate {
			t.Errorf("expected default baud rate %d, got %d", DefaultBaudRate, cdc.config.BaudRate)
		}
	})
}

// TestCDCTransportOpenClose tests opening and closing the CDC device.
// Note: This test will skip if /dev/ttyACM0 is not available.
func TestCDCTransportOpenClose(t *testing.T) {
	cfg := DefaultCDCConfig()
	cdc := NewCDCTransport(cfg)

	// Try to open - may fail if device not present
	err := cdc.Open()
	if err != nil {
		t.Skipf("Skipping test: CDC device not available: %v", err)
		return
	}

	// Verify open state
	if !cdc.IsOpen() {
		t.Error("expected transport to be open after Open()")
	}

	// Try opening again - should be idempotent
	if err := cdc.Open(); err != nil {
		t.Errorf("second Open() should succeed: %v", err)
	}

	// Close the device
	if err := cdc.Close(); err != nil {
		t.Errorf("Close() failed: %v", err)
	}

	// Verify closed state
	if cdc.IsOpen() {
		t.Error("expected transport to be closed after Close()")
	}

	// Try closing again - should be idempotent
	if err := cdc.Close(); err != nil {
		t.Errorf("second Close() should succeed: %v", err)
	}
}

// TestCDCTransportOperationsWhenClosed verifies operations fail when device is closed.
func TestCDCTransportOperationsWhenClosed(t *testing.T) {
	cdc := NewCDCTransport(nil)

	// Send should fail
	_, err := cdc.Send([]byte{0x01, 0x02, 0x03})
	if err != ErrDeviceNotOpen {
		t.Errorf("expected ErrDeviceNotOpen, got %v", err)
	}

	// Receive should fail
	_, err = cdc.Receive(10)
	if err != ErrDeviceNotOpen {
		t.Errorf("expected ErrDeviceNotOpen, got %v", err)
	}

	// Transfer should fail
	ctx := context.Background()
	_, err = cdc.Transfer(ctx, []byte{0x01, 0x02, 0x03})
	if err != ErrDeviceNotOpen {
		t.Errorf("expected ErrDeviceNotOpen, got %v", err)
	}
}

// TestCDCTransportContextCancellation verifies context handling.
func TestCDCTransportContextCancellation(t *testing.T) {
	// Create a transport (doesn't need to be open for context checks)
	cdc := NewCDCTransport(nil)

	t.Run("CancelledContext", func(t *testing.T) {
		ctx, cancel := context.WithCancel(context.Background())
		cancel() // Cancel immediately

		_, err := cdc.Transfer(ctx, []byte{0x01, 0x02})
		if err != context.Canceled {
			t.Errorf("expected context.Canceled, got %v", err)
		}
	})

	t.Run("DeadlineExceeded", func(t *testing.T) {
		ctx, cancel := context.WithDeadline(context.Background(), time.Now().Add(-1*time.Second))
		defer cancel()

		_, err := cdc.Transfer(ctx, []byte{0x01, 0x02})
		if err != context.DeadlineExceeded {
			t.Errorf("expected context.DeadlineExceeded, got %v", err)
		}
	})
}

// TestCDCTransportConfig verifies Config() accessor.
func TestCDCTransportConfig(t *testing.T) {
	cfg := &CDCConfig{
		Device:   "/dev/ttyUSB0",
		BaudRate: 9600,
		Timeout:  200 * time.Millisecond,
		VID:      0xABCD,
		PID:      0x1234,
	}

	cdc := NewCDCTransport(cfg)
	gotCfg := cdc.Config()

	if gotCfg.Device != cfg.Device {
		t.Errorf("expected device %s, got %s", cfg.Device, gotCfg.Device)
	}

	if gotCfg.BaudRate != cfg.BaudRate {
		t.Errorf("expected baud rate %d, got %d", cfg.BaudRate, gotCfg.BaudRate)
	}

	if gotCfg.VID != cfg.VID {
		t.Errorf("expected VID 0x%04X, got 0x%04X", cfg.VID, gotCfg.VID)
	}

	if gotCfg.PID != cfg.PID {
		t.Errorf("expected PID 0x%04X, got 0x%04X", cfg.PID, gotCfg.PID)
	}
}

// TestCDCTransportAutoDetect tests auto-detection behavior.
// Note: This test will skip if no CDC devices are available.
func TestCDCTransportAutoDetect(t *testing.T) {
	cfg := &CDCConfig{
		Device:   "", // Empty device triggers auto-detection
		BaudRate: DefaultBaudRate,
		Timeout:  DefaultCDCTimeout,
		VID:      RenesasVID,
		PID:      RX72NPID,
	}

	cdc := NewCDCTransport(cfg)
	err := cdc.Open()

	if err != nil {
		// Auto-detection may fail if no matching device is found
		if err == ErrDeviceNotFound {
			t.Skipf("Skipping test: No CDC device found with VID=%04X PID=%04X", cfg.VID, cfg.PID)
			return
		}
		t.Skipf("Skipping test: Auto-detection failed: %v", err)
		return
	}

	defer cdc.Close()

	if !cdc.IsOpen() {
		t.Error("expected transport to be open after successful auto-detection")
	}
}

// TestCDCTransportSendReceive tests basic send/receive operations.
// Note: This test requires a loopback device or real hardware.
// It will skip if the device is not available.
func TestCDCTransportSendReceive(t *testing.T) {
	t.Skip("Skipping hardware-dependent test - requires loopback device or real RX72N")

	cfg := DefaultCDCConfig()
	cdc := NewCDCTransport(cfg)

	if err := cdc.Open(); err != nil {
		t.Skipf("Skipping test: CDC device not available: %v", err)
		return
	}
	defer cdc.Close()

	// Test data
	testData := []byte{0x55, 0xAA, 0x01, 0x02, 0x03, 0x04}

	// Send data
	n, err := cdc.Send(testData)
	if err != nil {
		t.Fatalf("Send() failed: %v", err)
	}
	if n != len(testData) {
		t.Errorf("expected to send %d bytes, sent %d", len(testData), n)
	}

	// Receive data (requires loopback or real device responding)
	rxData, err := cdc.Receive(len(testData))
	if err != nil {
		t.Fatalf("Receive() failed: %v", err)
	}

	if len(rxData) != len(testData) {
		t.Errorf("expected to receive %d bytes, got %d", len(testData), len(rxData))
	}

	// In a loopback setup, received data should match sent data
	for i := range testData {
		if i < len(rxData) && rxData[i] != testData[i] {
			t.Errorf("byte %d: expected 0x%02X, got 0x%02X", i, testData[i], rxData[i])
		}
	}
}

// TestCDCTransportTransfer tests full transfer operation.
// Note: This test requires a loopback device or real hardware.
func TestCDCTransportTransfer(t *testing.T) {
	t.Skip("Skipping hardware-dependent test - requires loopback device or real RX72N")

	cfg := DefaultCDCConfig()
	cdc := NewCDCTransport(cfg)

	if err := cdc.Open(); err != nil {
		t.Skipf("Skipping test: CDC device not available: %v", err)
		return
	}
	defer cdc.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
	defer cancel()

	testData := []byte{0x55, 0xAA, 0x01, 0x02, 0x03, 0x04}

	rxData, err := cdc.Transfer(ctx, testData)
	if err != nil {
		t.Fatalf("Transfer() failed: %v", err)
	}

	if len(rxData) != len(testData) {
		t.Errorf("expected to receive %d bytes, got %d", len(testData), len(rxData))
	}
}

// TestCDCTransportConcurrentAccess verifies thread safety.
func TestCDCTransportConcurrentAccess(t *testing.T) {
	cdc := NewCDCTransport(nil)

	// Concurrent IsOpen() calls should be safe
	done := make(chan bool, 10)
	for i := 0; i < 10; i++ {
		go func() {
			_ = cdc.IsOpen()
			done <- true
		}()
	}

	for i := 0; i < 10; i++ {
		<-done
	}

	// Concurrent Config() calls should be safe
	for i := 0; i < 10; i++ {
		go func() {
			_ = cdc.Config()
			done <- true
		}()
	}

	for i := 0; i < 10; i++ {
		<-done
	}
}
