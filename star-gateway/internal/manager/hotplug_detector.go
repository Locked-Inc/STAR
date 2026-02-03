package manager

import (
	"context"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/fsnotify/fsnotify"
)

// HotPlugDetector monitors for USB device add/remove events.
//
// On Linux, this uses inotify (via fsnotify) to watch /sys/bus/usb/devices
// for device add/remove events. When a new device appears or disappears,
// it triggers the registered callback with an appropriate HotPlugEvent.
//
// On other platforms, it falls back to polling /dev for ttyACM* devices.
//
// PHASE 3: Full implementation with inotify-based USB monitoring.
type HotPlugDetector struct {
	pollInterval time.Duration
	vendorID     uint16
	productID    uint16
	useInotify   bool // True on Linux, false elsewhere
}

// NewHotPlugDetector creates a new HotPlugDetector.
//
// On Linux, uses inotify for real-time USB device monitoring.
// On other platforms, uses polling at the specified interval.
func NewHotPlugDetector(pollInterval time.Duration, vid, pid uint16) *HotPlugDetector {
	if pollInterval <= 0 {
		panic("HotPlugDetector: pollInterval must be positive")
	}

	// Determine if we can use inotify (Linux only)
	useInotify := runtime.GOOS == "linux"

	return &HotPlugDetector{
		pollInterval: pollInterval,
		vendorID:     vid,
		productID:    pid,
		useInotify:   useInotify,
	}
}

// Run starts the hot-plug detection loop.
// It exits when ctx is cancelled.
//
// PHASE 3: Uses inotify on Linux, polling on other platforms.
func (hpd *HotPlugDetector) Run(ctx context.Context, eventHandler func(HotPlugEvent)) {
	if hpd.useInotify {
		log.Printf("HotPlugDetector started with inotify (VID=0x%04X PID=0x%04X)",
			hpd.vendorID, hpd.productID)
		hpd.runInotify(ctx, eventHandler)
	} else {
		log.Printf("HotPlugDetector started with polling (VID=0x%04X PID=0x%04X, interval: %v)",
			hpd.vendorID, hpd.productID, hpd.pollInterval)
		hpd.runPolling(ctx, eventHandler)
	}
}

// runInotify uses fsnotify to watch for USB device events (Linux only).
//
// Watches /sys/bus/usb/devices for device add/remove events.
// When a new device directory appears, checks if it matches our VID/PID.
func (hpd *HotPlugDetector) runInotify(ctx context.Context, eventHandler func(HotPlugEvent)) {
	const usbDevicesPath = "/sys/bus/usb/devices"

	// Create fsnotify watcher
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		log.Printf("ERROR: Failed to create fsnotify watcher: %v (falling back to polling)", err)
		hpd.runPolling(ctx, eventHandler)
		return
	}
	defer watcher.Close()

	// Watch the USB devices directory
	if err := watcher.Add(usbDevicesPath); err != nil {
		log.Printf("ERROR: Failed to watch %s: %v (falling back to polling)", usbDevicesPath, err)
		hpd.runPolling(ctx, eventHandler)
		return
	}

	log.Printf("Watching %s for USB device events", usbDevicesPath)

	for {
		select {
		case <-ctx.Done():
			log.Printf("HotPlugDetector stopped")
			return

		case event, ok := <-watcher.Events:
			if !ok {
				log.Printf("WARNING: Watcher events channel closed")
				return
			}

			// Handle Create and Remove events
			if event.Op&fsnotify.Create != 0 {
				// Device added
				if hpd.matchesTargetDevice(event.Name) {
					log.Printf("USB device added: %s", event.Name)
					eventHandler(HotPlugEvent{Action: "add", Device: event.Name})
				}
			} else if event.Op&fsnotify.Remove != 0 {
				// Device removed
				// Note: We can't check VID/PID after removal, so we trigger on any removal
				// The TransportManager will handle reconnection logic
				log.Printf("USB device removed: %s", event.Name)
				eventHandler(HotPlugEvent{Action: "remove", Device: event.Name})
			}

		case err, ok := <-watcher.Errors:
			if !ok {
				log.Printf("WARNING: Watcher errors channel closed")
				return
			}
			log.Printf("WARNING: Watcher error: %v", err)
		}
	}
}

// runPolling uses polling to detect USB device changes (fallback for non-Linux).
//
// Checks /dev for ttyACM* devices at regular intervals.
func (hpd *HotPlugDetector) runPolling(ctx context.Context, eventHandler func(HotPlugEvent)) {
	ticker := time.NewTicker(hpd.pollInterval)
	defer ticker.Stop()

	// Track last seen devices to detect add/remove
	lastSeen := make(map[string]bool)

	for {
		select {
		case <-ctx.Done():
			log.Printf("HotPlugDetector stopped")
			return

		case <-ticker.C:
			// Scan for ttyACM* devices in /dev
			currentDevices := hpd.scanDevices()

			// Detect additions
			for device := range currentDevices {
				if !lastSeen[device] {
					log.Printf("USB device added: %s", device)
					eventHandler(HotPlugEvent{Action: "add", Device: device})
				}
			}

			// Detect removals
			for device := range lastSeen {
				if !currentDevices[device] {
					log.Printf("USB device removed: %s", device)
					eventHandler(HotPlugEvent{Action: "remove", Device: device})
				}
			}

			lastSeen = currentDevices
		}
	}
}

// matchesTargetDevice checks if the given path is a USB device matching our VID/PID.
//
// Reads idVendor and idProduct from sysfs to verify the device.
func (hpd *HotPlugDetector) matchesTargetDevice(devicePath string) bool {
	// Extract device name (e.g., "1-1.2" from "/sys/bus/usb/devices/1-1.2")
	deviceName := filepath.Base(devicePath)

	// Skip non-device entries (like "usb1", "1-0:1.0", etc.)
	// Real device paths look like "1-1", "1-1.2", "2-3", etc.
	if !strings.Contains(deviceName, "-") || strings.Contains(deviceName, ":") {
		return false
	}

	// Read VID from sysfs
	vidPath := filepath.Join(devicePath, "idVendor")
	vidBytes, err := os.ReadFile(vidPath)
	if err != nil {
		// File doesn't exist or can't be read (not a USB device directory)
		return false
	}

	// Read PID from sysfs
	pidPath := filepath.Join(devicePath, "idProduct")
	pidBytes, err := os.ReadFile(pidPath)
	if err != nil {
		return false
	}

	// Parse VID/PID (format: "2341\n" for Arduino VID 0x2341)
	var vid, pid uint16
	_, err = fmt.Sscanf(strings.TrimSpace(string(vidBytes)), "%x", &vid)
	if err != nil {
		return false
	}
	_, err = fmt.Sscanf(strings.TrimSpace(string(pidBytes)), "%x", &pid)
	if err != nil {
		return false
	}

	// Check if it matches our target device
	return vid == hpd.vendorID && pid == hpd.productID
}

// scanDevices scans /dev for ttyACM* devices.
// Returns a map of device paths (for quick lookup).
func (hpd *HotPlugDetector) scanDevices() map[string]bool {
	devices := make(map[string]bool)

	pattern := "/dev/ttyACM*"
	matches, err := filepath.Glob(pattern)
	if err != nil {
		log.Printf("WARNING: Failed to scan %s: %v", pattern, err)
		return devices
	}

	for _, device := range matches {
		devices[device] = true
	}

	return devices
}
