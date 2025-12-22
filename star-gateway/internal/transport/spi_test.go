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

	t.Run("Device", func(t *testing.T) {
		if config.Device != DefaultDevice {
			t.Errorf("Device = %s, want %s", config.Device, DefaultDevice)
		}
	})

	t.Run("SpeedHz", func(t *testing.T) {
		if config.SpeedHz != DefaultSpeedHz {
			t.Errorf("SpeedHz = %d, want %d", config.SpeedHz, DefaultSpeedHz)
		}
	})

	t.Run("Mode", func(t *testing.T) {
		if config.Mode != DefaultMode {
			t.Errorf("Mode = %d, want %d", config.Mode, DefaultMode)
		}
	})

	t.Run("BitsPerWord", func(t *testing.T) {
		if config.BitsPerWord != DefaultBitsPerWord {
			t.Errorf("BitsPerWord = %d, want %d", config.BitsPerWord, DefaultBitsPerWord)
		}
	})

	t.Run("Timeout", func(t *testing.T) {
		if config.Timeout != DefaultTimeout {
			t.Errorf("Timeout = %v, want %v", config.Timeout, DefaultTimeout)
		}
	})
}

// ============================================================================
// SPITransport Tests
// ============================================================================

func TestNewSPITransport(t *testing.T) {
	tests := []struct {
		name            string
		config          *SPIConfig
		expectedDevice  string
		expectedSpeedHz uint32
	}{
		{
			name:            "nil_config_uses_defaults",
			config:          nil,
			expectedDevice:  DefaultDevice,
			expectedSpeedHz: DefaultSpeedHz,
		},
		{
			name: "custom_config",
			config: &SPIConfig{
				Device:      "/dev/spidev1.0",
				SpeedHz:     5_000_000,
				Mode:        1,
				BitsPerWord: 8,
				Timeout:     200 * time.Millisecond,
			},
			expectedDevice:  "/dev/spidev1.0",
			expectedSpeedHz: 5_000_000,
		},
		{
			name: "alternate_device",
			config: &SPIConfig{
				Device:      "/dev/spidev0.1",
				SpeedHz:     1_000_000,
				Mode:        3,
				BitsPerWord: 8,
				Timeout:     50 * time.Millisecond,
			},
			expectedDevice:  "/dev/spidev0.1",
			expectedSpeedHz: 1_000_000,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			transport := NewSPITransport(tc.config)

			if transport == nil {
				t.Fatal("expected transport, got nil")
			}
			if transport.Config().Device != tc.expectedDevice {
				t.Errorf("Device = %s, want %s", transport.Config().Device, tc.expectedDevice)
			}
			if transport.Config().SpeedHz != tc.expectedSpeedHz {
				t.Errorf("SpeedHz = %d, want %d", transport.Config().SpeedHz, tc.expectedSpeedHz)
			}
		})
	}
}

func TestSPITransportIsOpen(t *testing.T) {
	transport := NewSPITransport(nil)
	if transport.IsOpen() {
		t.Error("expected IsOpen() = false for new transport")
	}
}

func TestSPITransportNotOpenErrors(t *testing.T) {
	tests := []struct {
		name      string
		operation string
	}{
		{"Send", "send"},
		{"Receive", "receive"},
		{"Transfer", "transfer"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			transport := NewSPITransport(nil)

			var err error
			switch tc.operation {
			case "send":
				_, err = transport.Send([]byte{0x01, 0x02})
			case "receive":
				_, err = transport.Receive(64)
			case "transfer":
				_, err = transport.Transfer([]byte{0x01, 0x02})
			}

			if err != ErrDeviceNotOpen {
				t.Errorf("%s() error = %v, want ErrDeviceNotOpen", tc.name, err)
			}
		})
	}
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
