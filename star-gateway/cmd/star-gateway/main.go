package main

import (
	"context"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/Locked-Inc/STAR/star-gateway/internal/app"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
)

// bbbUSBGadgetVID is the USB Vendor ID for Linux Foundation gadget devices.
// BeagleBone Blue in USB gadget mode uses VID 1d6b (Linux Foundation).
const bbbUSBGadgetVID = "1d6b"

// bbbUSBGadgetPID is the USB Product ID for Multifunction Composite Gadget.
const bbbUSBGadgetPID = "0104"

// detectBBB checks if a BeagleBone Blue USB gadget is connected on ttyACM0.
// Returns true if the device's VID:PID matches the BBB USB gadget (1d6b:0104).
func detectBBB() bool {
	// Find ttyACM0's USB device in sysfs
	matches, _ := filepath.Glob("/sys/class/tty/ttyACM0/device/../idVendor")
	if len(matches) == 0 {
		// Try alternative sysfs path
		matches, _ = filepath.Glob("/sys/bus/usb/devices/*/tty/ttyACM0/../../../idVendor")
	}
	if len(matches) == 0 {
		return false
	}

	vendorPath := matches[0]
	productPath := strings.Replace(vendorPath, "idVendor", "idProduct", 1)

	vid, err := os.ReadFile(vendorPath)
	if err != nil {
		return false
	}
	pid, err := os.ReadFile(productPath)
	if err != nil {
		return false
	}

	return strings.TrimSpace(string(vid)) == bbbUSBGadgetVID &&
		strings.TrimSpace(string(pid)) == bbbUSBGadgetPID
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

	// Auto-detect BBB: if mode is still auto and BBB gadget is on ttyACM0,
	// switch to simple-usb mode automatically.
	if config.TransportMode == manager.ModeAuto && detectBBB() {
		config.TransportMode = manager.ModeSimpleUSB
		log.Printf("BeagleBone Blue detected on ttyACM0 (VID:PID %s:%s), using simple-usb mode",
			bbbUSBGadgetVID, bbbUSBGadgetPID)
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
