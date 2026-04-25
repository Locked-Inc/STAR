// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package manager ...
//
// STAR Project - Texas A&M University
// January 2026
package manager

import "time"

// Config holds the configuration for the TransportManager.
type Config struct {
	// Mode determines the transport selection strategy.
	// Valid values: "auto", "force-usb", "force-spi"
	Mode TransportMode

	// HealthCheckInterval is how often to probe inactive transports.
	// Default: 5 seconds
	HealthCheckInterval time.Duration

	// FailureThreshold is the number of consecutive failures before switching transports.
	// Default: 3
	FailureThreshold int

	// SwitchTimeout is the maximum time allowed for completing a transport switch.
	// Includes draining in-flight operations.
	// Default: 500ms
	SwitchTimeout time.Duration

	// EnableHotPlug enables USB hot-plug detection.
	// Automatically disabled when Mode is ModeForceSPI.
	// Default: true
	EnableHotPlug bool

	// HotPlugPollInterval is the polling interval for USB device detection.
	// Only used if inotify is unavailable (non-Linux systems).
	// Default: 500ms
	HotPlugPollInterval time.Duration

	// USBVID is the expected USB Vendor ID for the RX72N device.
	// Used for auto-detection. Set to 0 to disable VID filtering.
	// Default: 0x045B (Renesas)
	USBVID uint16

	// USBPID is the expected USB Product ID for the RX72N device.
	// Used for auto-detection. Set to 0 to disable PID filtering.
	// Default: 0x0235 (Update with actual RX72N PID)
	USBPID uint16

	// FailbackDamping is the minimum time a recovered transport must remain healthy
	// before it is eligible for priority-based selection again.
	// Prevents rapid oscillation between transports after recovery.
	// Default: 30s
	FailbackDamping time.Duration

	// HealthLatencyThreshold is the maximum average latency before a transport is
	// considered unhealthy. Zero disables latency-based health checks.
	// Default: 200ms
	HealthLatencyThreshold time.Duration

	// HealthLossThreshold is the maximum packet loss rate (0.0-1.0) before a transport
	// is considered unhealthy. Zero disables loss-based health checks.
	// Default: 0.1 (10%)
	HealthLossThreshold float64
}

// DefaultConfig returns a Config with production-ready default settings.
//
// Default behavior:
//   - Auto mode: prefer USB, fallback to SPI
//   - Health checks every 5 seconds
//   - Failover after 3 consecutive failures
//   - 500ms switch timeout
//   - Hot-plug detection enabled
//
// Note: USBPID should be updated with the actual RX72N Product ID once known.
func DefaultConfig() *Config {
	return &Config{
		Mode:                   ModeAuto,
		HealthCheckInterval:    5 * time.Second,
		FailureThreshold:       3,
		SwitchTimeout:          500 * time.Millisecond,
		EnableHotPlug:          true,
		HotPlugPollInterval:    500 * time.Millisecond,
		USBVID:                 0x045B, // Renesas vendor ID
		USBPID:                 0x0235, // TODO: Verify if we need to obtain actual RX72N PID, once we acquire it.
		FailbackDamping:        30 * time.Second,
		HealthLatencyThreshold: 200 * time.Millisecond,
		HealthLossThreshold:    0.1, // 10%
	}
}

// Validate checks if the configuration is valid.
func (c *Config) Validate() error {
	if c.HealthCheckInterval < 100*time.Millisecond {
		return &ConfigError{Field: "HealthCheckInterval", Reason: "must be >= 100ms"}
	}
	if c.FailureThreshold < 1 {
		return &ConfigError{Field: "FailureThreshold", Reason: "must be >= 1"}
	}
	if c.SwitchTimeout < 10*time.Millisecond {
		return &ConfigError{Field: "SwitchTimeout", Reason: "must be >= 10ms"}
	}
	if c.HotPlugPollInterval < 100*time.Millisecond {
		return &ConfigError{Field: "HotPlugPollInterval", Reason: "must be >= 100ms"}
	}

	switch c.Mode {
	case ModeAuto, ModeForceUSB, ModeForceSPI, ModeSimpleUSB:
		// Valid modes
	default:
		return &ConfigError{Field: "Mode", Reason: "invalid mode: " + string(c.Mode)}
	}

	if c.FailbackDamping < 0 {
		return &ConfigError{Field: "FailbackDamping", Reason: "must be >= 0"}
	}
	if c.HealthLatencyThreshold < 0 {
		return &ConfigError{Field: "HealthLatencyThreshold", Reason: "must be >= 0"}
	}
	if c.HealthLossThreshold < 0 || c.HealthLossThreshold > 1.0 {
		return &ConfigError{Field: "HealthLossThreshold", Reason: "must be in [0.0, 1.0]"}
	}

	return nil
}

// ConfigError represents a configuration validation error.
type ConfigError struct {
	Field  string
	Reason string
}

func (e *ConfigError) Error() string {
	return "config validation failed: " + e.Field + ": " + e.Reason
}
