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

// BeagleBone Blue USB gadget VID:PID (Linux Foundation Multifunction Composite Gadget).
const (
	bbbUSBVID uint16 = 0x1d6b
	bbbUSBPID uint16 = 0x0104
)

// Cypress CY7C65213 USB-UART bridge VID:PID -- the chip on the STAR PCB
// that connects the RX72N's SCI9 to the Pi5 via USB CDC-ACM. Same wire
// protocol as the BBB path (CRC-32 framed nanopb), just a different
// physical transport.
const (
	rx72nCypressVID uint16 = 0x04B4
	rx72nCypressPID uint16 = 0x0003
)

// findBBBDevice scans all ttyACM* devices in Linux sysfs for a BeagleBone Blue
// USB gadget (VID:PID 1d6b:0104) and returns its /dev path (e.g., /dev/ttyACM1).
//
// Using VID:PID-based discovery means the correct device is found regardless of
// which ttyACMN minor number the kernel assigned. After a disconnect/reconnect
// cycle the minor may change (e.g., ttyACM0 -> ttyACM1); this function handles
// that transparently.
//
// Returns empty string if the BBB is not connected.
func findBBBDevice() string {
	device, err := transport.FindCDCDevice(bbbUSBVID, bbbUSBPID)
	if err != nil {
		return ""
	}
	return device
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

	config := app.Config{}

	// Check for --simple CLI flag
	simpleFlag := false
	for _, arg := range os.Args[1:] {
		if arg == "--simple" {
			simpleFlag = true
		}
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

	// Auto-detect motor controller transport: prefer BBB (USB CDC composite),
	// fall back to RX72N's Cypress USB-UART bridge. Both speak the same
	// CRC-32 framed nanopb protocol over a CDC-ACM serial node.
	// STAR_CDC_DEVICE env var overrides everything: explicit /dev/ttyXXX path.
	if config.TransportMode == manager.ModeAuto || simpleFlag {
		if dev := os.Getenv("STAR_CDC_DEVICE"); dev != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = dev
			log.Printf("STAR_CDC_DEVICE override: using %s in simple-usb mode", dev)
		} else if device := findBBBDevice(); device != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = device
			config.USBVID = bbbUSBVID
			config.USBPID = bbbUSBPID
			log.Printf("BeagleBone Blue detected at %s (VID:PID %04x:%04x), using simple-usb mode",
				device, bbbUSBVID, bbbUSBPID)
		} else if device := findRX72NDevice(); device != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = device
			config.USBVID = rx72nCypressVID
			config.USBPID = rx72nCypressPID
			log.Printf("RX72N (Cypress USB-UART) detected at %s (VID:PID %04x:%04x), using simple-usb mode",
				device, rx72nCypressVID, rx72nCypressPID)
		} else if simpleFlag {
			log.Printf("WARNING: --simple flag set but no BBB or RX72N CDC detected")
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
