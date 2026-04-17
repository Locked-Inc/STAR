// Package transport - USB-UART-bridge device discovery via Linux sysfs.
//
// The CY7C65213 USB-UART bridge registers as cypress_m8 and shows up in sysfs
// as /sys/class/tty/ttyUSB*. On disconnect / reconnect the kernel may assign a
// different minor (ttyUSB0 -> ttyUSB1), so hard-coding /dev/ttyUSB0 breaks
// silently after a reboot or hub cycle. FindCDCDevice scans every ttyUSB*
// entry and matches by VID:PID to return whichever /dev path the bridge
// currently occupies.
//
// STAR Project - Texas A&M University
package transport

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// FindCDCDevice scans Linux sysfs for a USB-UART bridge matching the given
// VID:PID and returns its /dev path (e.g., /dev/ttyUSB1).
//
// Returns ErrDeviceNotFound if no matching device is present.
func FindCDCDevice(vid, pid uint16) (string, error) {
	return findCDCDeviceIn("/sys/class/tty", "/dev", vid, pid)
}

// findCDCDeviceIn is the testable core of FindCDCDevice with injectable paths.
func findCDCDeviceIn(sysfsClassTTY, devRoot string, vid, pid uint16) (string, error) {
	pattern := filepath.Join(sysfsClassTTY, "ttyUSB*", "device")
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return "", fmt.Errorf("usb_detect: glob %s: %w", pattern, err)
	}
	if len(matches) == 0 {
		return "", ErrDeviceNotFound
	}
	sort.Strings(matches) // deterministic order: ttyUSB0 before ttyUSB1

	for _, deviceLink := range matches {
		resolved, err := filepath.EvalSymlinks(deviceLink)
		if err != nil {
			continue
		}

		foundVID, foundPID, err := readUSBIDFiles(resolved)
		if err != nil {
			// USB interface dirs don't have idVendor; walk up to USB device dir.
			foundVID, foundPID, err = readUSBIDFiles(filepath.Dir(resolved))
			if err != nil {
				continue
			}
		}

		if foundVID == vid && foundPID == pid {
			// e.g. /sys/class/tty/ttyUSB1/device -> ttyUSB1 -> /dev/ttyUSB1
			ttyName := filepath.Base(filepath.Dir(deviceLink))
			return filepath.Join(devRoot, ttyName), nil
		}
	}
	return "", ErrDeviceNotFound
}

// readUSBIDFiles reads idVendor and idProduct hex files from a sysfs directory.
func readUSBIDFiles(sysfsDir string) (uint16, uint16, error) {
	vid, err := readHexUSBFile(filepath.Join(sysfsDir, "idVendor"))
	if err != nil {
		return 0, 0, err
	}
	pid, err := readHexUSBFile(filepath.Join(sysfsDir, "idProduct"))
	if err != nil {
		return 0, 0, err
	}
	return vid, pid, nil
}

func readHexUSBFile(path string) (uint16, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return 0, err
	}
	parsed, err := strconv.ParseUint(strings.TrimSpace(string(data)), 16, 16)
	if err != nil {
		return 0, fmt.Errorf("parse hex in %s: %w", path, err)
	}
	return uint16(parsed), nil
}
