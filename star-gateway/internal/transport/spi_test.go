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

	if config.Device != DefaultDevice {
		t.Errorf("Device = %s, want %s", config.Device, DefaultDevice)
	}
	if config.SpeedHz != DefaultSpeedHz {
		t.Errorf("SpeedHz = %d, want %d", config.SpeedHz, DefaultSpeedHz)
	}
	if config.Mode != DefaultMode {
		t.Errorf("Mode = %d, want %d", config.Mode, DefaultMode)
	}
	if config.BitsPerWord != DefaultBitsPerWord {
		t.Errorf("BitsPerWord = %d, want %d", config.BitsPerWord, DefaultBitsPerWord)
	}
	if config.Timeout != DefaultTimeout {
		t.Errorf("Timeout = %v, want %v", config.Timeout, DefaultTimeout)
	}
}

// ============================================================================
// SPITransport Tests
// ============================================================================

func TestNewSPITransport(t *testing.T) {
	t.Run("with_nil_config", func(t *testing.T) {
		transport := NewSPITransport(nil)
		if transport == nil {
			t.Fatal("expected transport, got nil")
		}
		if transport.Config().Device != DefaultDevice {
			t.Errorf("Device = %s, want %s", transport.Config().Device, DefaultDevice)
		}
	})

	t.Run("with_custom_config", func(t *testing.T) {
		customConfig := &SPIConfig{
			Device:      "/dev/spidev1.0",
			SpeedHz:     5_000_000,
			Mode:        1,
			BitsPerWord: 8,
			Timeout:     200 * time.Millisecond,
		}
		transport := NewSPITransport(customConfig)
		if transport.Config().Device != "/dev/spidev1.0" {
			t.Errorf("Device = %s, want /dev/spidev1.0", transport.Config().Device)
		}
		if transport.Config().SpeedHz != 5_000_000 {
			t.Errorf("SpeedHz = %d, want 5000000", transport.Config().SpeedHz)
		}
	})
}

func TestSPITransportIsOpen(t *testing.T) {
	transport := NewSPITransport(nil)
	if transport.IsOpen() {
		t.Error("expected IsOpen() = false for new transport")
	}
}

func TestSPITransportNotOpenErrors(t *testing.T) {
	transport := NewSPITransport(nil)

	t.Run("send_not_open", func(t *testing.T) {
		_, err := transport.Send([]byte{0x01, 0x02})
		if err != ErrDeviceNotOpen {
			t.Errorf("Send() error = %v, want ErrDeviceNotOpen", err)
		}
	})

	t.Run("receive_not_open", func(t *testing.T) {
		_, err := transport.Receive(64)
		if err != ErrDeviceNotOpen {
			t.Errorf("Receive() error = %v, want ErrDeviceNotOpen", err)
		}
	})

	t.Run("transfer_not_open", func(t *testing.T) {
		_, err := transport.Transfer([]byte{0x01, 0x02})
		if err != ErrDeviceNotOpen {
			t.Errorf("Transfer() error = %v, want ErrDeviceNotOpen", err)
		}
	})
}

func TestSPITransportOpenNotImplemented(t *testing.T) {
	transport := NewSPITransport(nil)
	err := transport.Open()
	if err != ErrNotImplemented {
		t.Errorf("Open() error = %v, want ErrNotImplemented", err)
	}
}

func TestSPITransportCloseNotOpen(t *testing.T) {
	transport := NewSPITransport(nil)
	err := transport.Close()
	// Closing a non-open transport should succeed (no-op)
	if err != nil {
		t.Errorf("Close() error = %v, want nil", err)
	}
}

// ============================================================================
// TODO: Implementation Tests
// ============================================================================

// TODO: Add integration tests with actual SPI hardware once implementation is complete.
// func TestSPITransportIntegration(t *testing.T) {}

// TODO: Add loopback tests (requires COPI/CIPO connected) once implementation is complete.
// func TestSPILoopback(t *testing.T) {}

// TODO: Add timeout behavior tests once implementation is complete.
// func TestSPITimeout(t *testing.T) {}
