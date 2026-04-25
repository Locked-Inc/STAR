// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package manager

import (
	"context"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/fsnotify/fsnotify"
)

// HotPlugDetector monitors for USB device add/remove events.
//
// On Linux, this uses inotify via fsnotify to watch /sys/bus/usb/devices for
// device creation/removal. This provides instant detection (<50ms) compared to
// polling-based approaches.
//
// Device matching is VID:PID-based, not name-based. After a disconnect/reconnect
// cycle the kernel may assign a different ttyUSBN minor number; matching by
// VID:PID ensures the correct device is always identified.
type HotPlugDetector struct {
	vendorID          uint16
	productID         uint16
	usbDevicesPath    string
	sysfsClassTTYPath string
	devRoot           string
}

const (
	defaultUSBDevicesPath = "/sys/bus/usb/devices"
	defaultSysfsClassTTY  = "/sys/class/tty"
	defaultDevRoot        = "/dev"
)

// NewHotPlugDetector creates a new HotPlugDetector.
// The pollInterval parameter is ignored in the inotify implementation but kept
// for API compatibility.
func NewHotPlugDetector(pollInterval time.Duration, vid, pid uint16) *HotPlugDetector {
	return &HotPlugDetector{
		vendorID:          vid,
		productID:         pid,
		usbDevicesPath:    defaultUSBDevicesPath,
		sysfsClassTTYPath: defaultSysfsClassTTY,
		devRoot:           defaultDevRoot,
	}
}

// Run starts the hot-plug detection loop using inotify.
// It watches /sys/bus/usb/devices for Create/Remove events and calls eventHandler
// when the target device is added or removed.
//
// On non-Linux platforms or if fsnotify fails, it falls back to a no-op (graceful degradation).
func (hpd *HotPlugDetector) Run(ctx context.Context, eventHandler func(HotPlugEvent)) {
	log.Printf("HotPlugDetector started (VID=0x%04X PID=0x%04X)",
		hpd.vendorID, hpd.productID)

	// Try inotify-based detection
	if err := hpd.runInotify(ctx, eventHandler); err != nil {
		log.Printf("HotPlugDetector inotify failed: %v (falling back to no-op)", err)
		// Fallback: block on context cancellation
		<-ctx.Done()
	}

	log.Printf("HotPlugDetector stopped")
}

// runInotify implements the core inotify-based detection logic.
// Watches /sys/bus/usb/devices for USB device add/remove events.
func (hpd *HotPlugDetector) runInotify(ctx context.Context, eventHandler func(HotPlugEvent)) error {
	usbDevicesPath := hpd.usbDevicesDir()

	// Verify path exists (Linux-only)
	if _, err := os.Stat(usbDevicesPath); os.IsNotExist(err) {
		return fmt.Errorf("USB devices path not found (not Linux?): %s", usbDevicesPath)
	}

	// Create fsnotify watcher
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		return fmt.Errorf("failed to create fsnotify watcher: %w", err)
	}
	defer watcher.Close()

	// Add watch on USB devices directory
	if err := watcher.Add(usbDevicesPath); err != nil {
		return fmt.Errorf("failed to watch %s: %w", usbDevicesPath, err)
	}

	log.Printf("HotPlugDetector watching %s for USB events", usbDevicesPath)

	for {
		select {
		case <-ctx.Done():
			return nil

		case event, ok := <-watcher.Events:
			if !ok {
				return fmt.Errorf("fsnotify watcher closed unexpectedly")
			}

			// Handle Create/Rename (device added) and Remove (device removed) events
			if event.Op&(fsnotify.Create|fsnotify.Rename) != 0 {
				if hpd.matchesTargetDevice(event.Name) {
					devicePath := hpd.getDevicePath(event.Name)
					log.Printf("HotPlugDetector: USB device added: %s", devicePath)
					eventHandler(HotPlugEvent{
						Action:    "add",
						Device:    devicePath,
						Timestamp: time.Now(),
					})
				}
			} else if event.Op&fsnotify.Remove != 0 {
				if hpd.matchesTargetDevice(event.Name) {
					devicePath := hpd.getDevicePath(event.Name)
					log.Printf("HotPlugDetector: USB device removed: %s", devicePath)
					eventHandler(HotPlugEvent{
						Action:    "remove",
						Device:    devicePath,
						Timestamp: time.Now(),
					})
				}
			}

		case err, ok := <-watcher.Errors:
			if !ok {
				return fmt.Errorf("fsnotify error channel closed")
			}
			log.Printf("HotPlugDetector fsnotify error: %v", err)
			// Continue watching despite errors
		}
	}
}

// matchesTargetDevice checks if the sysfs path corresponds to the target USB device.
//
// Matching is done in two steps:
//  1. The sysfs entry must expose at least one ttyUSB* interface (CDC ACM class).
//  2. When VID:PID are non-zero, the device's USB identifiers must match.
//
// This approach is device-name-agnostic: it correctly handles ttyUSB0, ttyUSB1,
// etc. that appear after disconnect/reconnect cycles.
func (hpd *HotPlugDetector) matchesTargetDevice(sysfsPath string) bool {
	if sysfsPath == "" {
		return false
	}

	ttyNames := hpd.findTTYNames(sysfsPath)

	// Require at least one CDC ACM interface (any ttyUSB* suffix).
	hasCDCACM := false
	for _, name := range ttyNames {
		if strings.HasPrefix(name, "ttyUSB") {
			hasCDCACM = true
			break
		}
	}
	if !hasCDCACM {
		return false
	}

	if hpd.vendorID == 0 && hpd.productID == 0 {
		return true
	}

	vid, pid, err := hpd.readDeviceVIDPID(sysfsPath, ttyNames)
	if err != nil {
		return false
	}

	return vid == hpd.vendorID && pid == hpd.productID
}

// getDevicePath converts a sysfs path to a /dev device path.
//
// Returns the first ttyUSB* device found in the sysfs entry, allowing correct
// identification of ttyUSB0, ttyUSB1, etc. after disconnect/reconnect cycles.
// Falls back to the first available tty device if no ttyUSB* is found.
func (hpd *HotPlugDetector) getDevicePath(sysfsPath string) string {
	devRoot := hpd.devRootDir()
	ttyNames := hpd.findTTYNames(sysfsPath)

	// Prefer any ttyUSB* (CDC ACM) device.
	for _, name := range ttyNames {
		if strings.HasPrefix(name, "ttyUSB") {
			return filepath.Join(devRoot, name)
		}
	}

	if len(ttyNames) > 0 {
		return filepath.Join(devRoot, ttyNames[0])
	}

	return ""
}

func (hpd *HotPlugDetector) usbDevicesDir() string {
	if hpd.usbDevicesPath != "" {
		return hpd.usbDevicesPath
	}
	return defaultUSBDevicesPath
}

func (hpd *HotPlugDetector) sysfsClassTTYDir() string {
	if hpd.sysfsClassTTYPath != "" {
		return hpd.sysfsClassTTYPath
	}
	return defaultSysfsClassTTY
}

func (hpd *HotPlugDetector) devRootDir() string {
	if hpd.devRoot != "" {
		return hpd.devRoot
	}
	return defaultDevRoot
}

func (hpd *HotPlugDetector) findTTYNames(sysfsPath string) []string {
	if sysfsPath == "" {
		return nil
	}

	ttyDirs := []string{filepath.Join(sysfsPath, "tty")}
	if matches, err := filepath.Glob(filepath.Join(sysfsPath, "*", "tty")); err == nil {
		ttyDirs = append(ttyDirs, matches...)
	}

	names := make(map[string]struct{})
	for _, ttyDir := range ttyDirs {
		entries, err := os.ReadDir(ttyDir)
		if err != nil {
			continue
		}
		for _, entry := range entries {
			name := entry.Name()
			if name == "" {
				continue
			}
			names[name] = struct{}{}
		}
	}

	if len(names) == 0 {
		return nil
	}

	result := make([]string, 0, len(names))
	for name := range names {
		result = append(result, name)
	}
	sort.Strings(result)
	return result
}

func (hpd *HotPlugDetector) readDeviceVIDPID(sysfsPath string, ttyNames []string) (uint16, uint16, error) {
	if vid, pid, err := readVIDPIDFromPath(sysfsPath); err == nil {
		return vid, pid, nil
	}

	parent := filepath.Dir(sysfsPath)
	if parent != sysfsPath {
		if vid, pid, err := readVIDPIDFromPath(parent); err == nil {
			return vid, pid, nil
		}
	}

	if resolved, err := filepath.EvalSymlinks(filepath.Join(sysfsPath, "device")); err == nil {
		if vid, pid, err := readVIDPIDFromPath(resolved); err == nil {
			return vid, pid, nil
		}
	}

	for _, ttyName := range ttyNames {
		devicePath := filepath.Join(hpd.sysfsClassTTYDir(), ttyName, "device")
		resolved, err := filepath.EvalSymlinks(devicePath)
		if err != nil {
			continue
		}
		if vid, pid, err := readVIDPIDFromPath(resolved); err == nil {
			return vid, pid, nil
		}
		parent := filepath.Dir(resolved)
		if parent != resolved {
			if vid, pid, err := readVIDPIDFromPath(parent); err == nil {
				return vid, pid, nil
			}
		}
	}

	return 0, 0, fmt.Errorf("idVendor/idProduct not found for %s", sysfsPath)
}

func readVIDPIDFromPath(path string) (uint16, uint16, error) {
	vid, err := readHexFile(filepath.Join(path, "idVendor"))
	if err != nil {
		return 0, 0, err
	}

	pid, err := readHexFile(filepath.Join(path, "idProduct"))
	if err != nil {
		return 0, 0, err
	}

	return vid, pid, nil
}

func readHexFile(path string) (uint16, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return 0, err
	}

	value := strings.TrimSpace(string(data))
	parsed, err := strconv.ParseUint(value, 16, 16)
	if err != nil {
		return 0, err
	}

	return uint16(parsed), nil
}
