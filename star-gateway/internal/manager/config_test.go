// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package manager tests for configuration validation.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"testing"
)

func TestConfig_Validate(t *testing.T) {
	tests := []struct {
		name      string
		config    *Config
		wantError bool
	}{
		{
			name:      "default_valid",
			config:    DefaultConfig(),
			wantError: false,
		},
		{
			name: "invalid_switch_timeout",
			config: func() *Config {
				c := DefaultConfig()
				c.SwitchTimeout = 0
				return c
			}(),
			wantError: true,
		},
		{
			name: "invalid_failure_threshold",
			config: func() *Config {
				c := DefaultConfig()
				c.FailureThreshold = 0
				return c
			}(),
			wantError: true,
		},
		{
			name: "invalid_health_check_interval",
			config: func() *Config {
				c := DefaultConfig()
				c.HealthCheckInterval = 0
				return c
			}(),
			wantError: true,
		},
		{
			name: "invalid_hot_plug_poll_interval",
			config: func() *Config {
				c := DefaultConfig()
				c.EnableHotPlug = true
				c.HotPlugPollInterval = 0
				return c
			}(),
			wantError: true,
		},
		{
			name: "invalid_mode",
			config: func() *Config {
				c := DefaultConfig()
				c.Mode = "invalid_mode"
				return c
			}(),
			wantError: true,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := tc.config.Validate()
			if (err != nil) != tc.wantError {
				t.Errorf("Validate() error = %v, wantError %v", err, tc.wantError)
			}
		})
	}
}

func TestConfig_Error(t *testing.T) {
	err := &ConfigError{Field: "TestField", Reason: "test reason"}
	expected := "config validation failed: TestField: test reason"
	if err.Error() != expected {
		t.Errorf("Error() = %s, want %s", err.Error(), expected)
	}
}
