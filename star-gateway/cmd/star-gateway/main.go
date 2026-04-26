// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package main

import (
	"context"
	"log/slog"
	"os"
	"strconv"

	"github.com/Locked-Inc/STAR/star-gateway/internal/app"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// Cypress CY7C65213 USB-UART bridge VID:PID -- the chip on the STAR PCB
// that connects the RX72N's SCI9 to the Pi5 via USB CDC-ACM. CRC-32
// framed nanopb wire protocol.
const (
	rx72nCypressVID uint16 = 0x04B4
	rx72nCypressPID uint16 = 0x0003
)

// requireLocalhostEnv, when set to a truthy value (or when the
// --require-localhost flag is passed), forces the gateway to bind only
// to 127.0.0.1 / ::1. Any non-loopback bind address (including 0.0.0.0
// and "") is refused at startup. This is a defence-in-depth measure for
// the no-TLS / no-auth deployment posture documented in
// docs/PI_DEPLOYMENT.md "Secrets and Environment Variables".
const requireLocalhostEnv = "STAR_REQUIRE_LOCALHOST"

// printSecurityBanner emits a startup banner so anyone running the
// gateway is immediately aware of the plaintext / no-auth posture.
// See audit finding F-01: transport security is a follow-up PR; this
// banner is the interim mitigation.
func printSecurityBanner() {
	slog.Info("================================================================")
	slog.Info("WARNING: gateway listens in plaintext with no auth -- LAN-only")
	slog.Info("         deployments only. No TLS, no client authentication.")
	slog.Info("         Set env var to refuse non-loopback binds",
		"env_var", requireLocalhostEnv)
	slog.Info("================================================================")
}

// findRX72NDevice scans for the STAR PCB's Cypress USB-UART bridge that fronts
// the RX72N over SCI9. Returns the /dev path or empty string if not present.
func findRX72NDevice() string {
	device, err := transport.FindCDCDevice(rx72nCypressVID, rx72nCypressPID)
	if err != nil {
		return ""
	}
	return device
}

func main() {
	// Configure structured logging for the gateway entrypoint. app.Run
	// later installs its own logger; this default covers any logging
	// before that point (banner, flag parsing, transport auto-detect).
	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	})))

	printSecurityBanner()

	config := app.Config{}

	// Check for CLI flags. We accept --simple (legacy) and
	// --require-localhost (audit F-01) here. Other flags are still
	// surfaced to app.Run via env or future config.
	simpleFlag := false
	requireLocalhostFlag := false
	for _, arg := range os.Args[1:] {
		switch arg {
		case "--simple":
			simpleFlag = true
		case "--require-localhost":
			requireLocalhostFlag = true
		}
	}

	// Resolve --require-localhost / STAR_REQUIRE_LOCALHOST. The flag and
	// env var are equivalent; either is sufficient.
	if requireLocalhostFlag {
		config.RequireLocalhost = true
	} else if val, ok := os.LookupEnv(requireLocalhostEnv); ok {
		parsed, err := strconv.ParseBool(val)
		if err != nil {
			slog.Error("invalid env var",
				"name", requireLocalhostEnv, "value", val, "err", err)
			os.Exit(1)
		}
		config.RequireLocalhost = parsed
	}
	if config.RequireLocalhost {
		slog.Info("require-localhost enabled: refusing non-loopback binds")
	}

	if simpleFlag {
		config.TransportMode = manager.ModeSimpleUSB
		slog.Info("Simple USB mode enabled via --simple flag")
	} else {
		// Get transport mode from environment variable, default to auto if not set or invalid
		mode, err := manager.ParseTransportMode(os.Getenv("TRANSPORT_MODE"))
		if err != nil {
			slog.Warn("transport mode parse failed; defaulting to auto", "err", err)
		}
		config.TransportMode = mode
	}

	// Auto-detect motor controller transport: scan for RX72N's Cypress USB-UART
	// bridge. CRC-32 framed nanopb protocol over a CDC-ACM serial node.
	// STAR_CDC_DEVICE env var overrides everything: explicit /dev/ttyXXX path.
	if config.TransportMode == manager.ModeAuto || simpleFlag {
		if dev := os.Getenv("STAR_CDC_DEVICE"); dev != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = dev
			slog.Info("STAR_CDC_DEVICE override: using simple-usb mode", "device", dev)
		} else if device := findRX72NDevice(); device != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = device
			config.USBVID = rx72nCypressVID
			config.USBPID = rx72nCypressPID
			slog.Info("RX72N (Cypress USB-UART) detected, using simple-usb mode",
				"device", device,
				"vid", rx72nCypressVID,
				"pid", rx72nCypressPID)
		} else if simpleFlag {
			slog.Warn("--simple flag set but no RX72N CDC detected")
		}
	}

	if val, ok := os.LookupEnv("STAR_SIMULATION_MODE"); ok {
		var err error
		config.SimulationMode, err = strconv.ParseBool(val)
		if err != nil {
			slog.Error("invalid STAR_SIMULATION_MODE", "value", val, "err", err)
			os.Exit(1)
		}
	}

	if err := app.Run(context.Background(), config); err != nil {
		slog.Error("Application error", "err", err)
		os.Exit(1)
	}
}
