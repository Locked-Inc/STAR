// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package main

import (
	"context"
	"log"
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
	log.Printf("================================================================")
	log.Printf("WARNING: gateway listens in plaintext with no auth -- LAN-only")
	log.Printf("         deployments only. No TLS, no client authentication.")
	log.Printf("         Set %s=true to refuse non-loopback binds.",
		requireLocalhostEnv)
	log.Printf("================================================================")
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
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

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
			log.Fatalf("invalid %s=%q: %v", requireLocalhostEnv, val, err)
		}
		config.RequireLocalhost = parsed
	}
	if config.RequireLocalhost {
		log.Printf("require-localhost enabled: refusing non-loopback binds")
	}

	if simpleFlag {
		config.TransportMode = manager.ModeSimpleUSB
		log.Printf("Simple USB mode enabled via --simple flag")
	} else {
		// Get transport mode from environment variable, default to auto if not set or invalid
		mode, err := manager.ParseTransportMode(os.Getenv("TRANSPORT_MODE"))
		if err != nil {
			log.Printf("Warning: %v. Defaulting to auto mode.", err)
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
			log.Printf("STAR_CDC_DEVICE override: using %s in simple-usb mode", dev)
		} else if device := findRX72NDevice(); device != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = device
			config.USBVID = rx72nCypressVID
			config.USBPID = rx72nCypressPID
			log.Printf("RX72N (Cypress USB-UART) detected at %s (VID:PID %04x:%04x), using simple-usb mode",
				device, rx72nCypressVID, rx72nCypressPID)
		} else if simpleFlag {
			log.Printf("WARNING: --simple flag set but no RX72N CDC detected")
		}
	}

	if val, ok := os.LookupEnv("STAR_SIMULATION_MODE"); ok {
		var err error
		config.SimulationMode, err = strconv.ParseBool(val)
		if err != nil {
			log.Fatalf("invalid STAR_SIMULATION_MODE %q: %v", val, err)
		}
	}

	if err := app.Run(context.Background(), config); err != nil {
		log.Fatalf("Application error: %v", err)
	}
}
