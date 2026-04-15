// Package transport - USB CDC device discovery via Linux sysfs.
//
// On disconnect and reconnect, the Linux kernel assigns new minor numbers to
// USB CDC ACM devices (e.g., ttyACM0 -> ttyACM1). Hardcoding /dev/ttyACM0
// breaks silently. FindCDCDevice scans all ttyACM* entries by USB VID:PID so
// the correct device is always found regardless of which minor was assigned.
//
// STAR Project - Texas A&M University
// April 2026
package transport

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// FindCDCDevice scans Linux sysfs for a USB CDC ACM device matching the given
// VID:PID and returns its /dev path (e.g., /dev/ttyACM1).
//
// On disconnect and reconnect the kernel may assign a different ttyACMN minor
// number. This function always finds the correct device regardless of minor.
//
// Returns ErrDeviceNotFound if no matching device is present.
func FindCDCDevice(vid, pid uint16) (string, error) {
	return findCDCDeviceIn("/sys/class/tty", "/dev", vid, pid)
}

// findCDCDeviceIn is the testable core of FindCDCDevice with injectable paths.
func findCDCDeviceIn(sysfsClassTTY, devRoot string, vid, pid uint16) (string, error) {
	pattern := filepath.Join(sysfsClassTTY, "ttyACM*", "device")
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return "", fmt.Errorf("usb_detect: glob %s: %w", pattern, err)
	}
	if len(matches) == 0 {
		return "", ErrDeviceNotFound
	}
	sort.Strings(matches) // deterministic order: ttyACM0 before ttyACM1

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
			// e.g. /sys/class/tty/ttyACM1/device -> ttyACM1 -> /dev/ttyACM1
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
