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

	// Auto-detect BBB: scan all ttyACM* devices by VID:PID.
	// This is robust to minor-number changes after disconnect/reconnect cycles
	// (e.g., ttyACM0 -> ttyACM1) because discovery is by VID:PID, not by name.
	if config.TransportMode == manager.ModeAuto || simpleFlag {
		if device := findBBBDevice(); device != "" {
			config.TransportMode = manager.ModeSimpleUSB
			config.CDCDevice = device
			config.USBVID = bbbUSBVID
			config.USBPID = bbbUSBPID
			log.Printf("BeagleBone Blue detected at %s (VID:PID %04x:%04x), using simple-usb mode",
				device, bbbUSBVID, bbbUSBPID)
		} else if simpleFlag {
			log.Printf("WARNING: --simple flag set but no BBB detected (VID:PID %04x:%04x)",
				bbbUSBVID, bbbUSBPID)
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
