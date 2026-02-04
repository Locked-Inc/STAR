package manager

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"
)

// TestNewHotPlugDetector_Valid tests creating a valid HotPlugDetector.
// Phase 3: inotify-based implementation ignores pollInterval (kept for API compatibility).
func TestNewHotPlugDetector_Valid(t *testing.T) {
	const pollInterval = time.Second

	tests := []struct {
		name             string
		vid              uint16
		pid              uint16
		wantTargetDevice string
		wantVendorID     uint16
		wantProductID    uint16
	}{
		{
			name:             "configured_vid_pid",
			vid:              0x045B,
			pid:              0x0235,
			wantTargetDevice: DefaultTargetDevice,
			wantVendorID:     0x045B,
			wantProductID:    0x0235,
		},
		{
			name:             "zero_vid_pid",
			vid:              0,
			pid:              0,
			wantTargetDevice: DefaultTargetDevice,
			wantVendorID:     0,
			wantProductID:    0,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			// pollInterval is ignored in inotify implementation but kept for API compatibility
			hpd := NewHotPlugDetector(pollInterval, tc.vid, tc.pid)
			if hpd == nil {
				t.Fatal("NewHotPlugDetector() returned nil")
			}

			if hpd.vendorID != tc.wantVendorID {
				t.Errorf("vendorID = 0x%04X, want 0x%04X", hpd.vendorID, tc.wantVendorID)
			}

			if hpd.productID != tc.wantProductID {
				t.Errorf("productID = 0x%04X, want 0x%04X", hpd.productID, tc.wantProductID)
			}

			if hpd.targetDevice != tc.wantTargetDevice {
				t.Errorf("targetDevice = %q, want %q", hpd.targetDevice, tc.wantTargetDevice)
			}
		})
	}
}

func TestHotPlugDetectorMatchesTargetDevice(t *testing.T) {
	const pollInterval = time.Second

	usbRoot := t.TempDir()
	deviceDir := filepath.Join(usbRoot, "1-2.3")
	ttyDir := filepath.Join(deviceDir, "1-2.3:1.0", "tty", DefaultTargetDevice)
	if err := os.MkdirAll(ttyDir, 0o755); err != nil {
		t.Fatalf("failed to create tty directory: %v", err)
	}

	if err := writeUSBIDFiles(deviceDir, 0x045B, 0x0235); err != nil {
		t.Fatalf("failed to write id files: %v", err)
	}

	hpd := NewHotPlugDetector(pollInterval, 0x045B, 0x0235)
	hpd.usbDevicesPath = usbRoot

	if !hpd.matchesTargetDevice(deviceDir) {
		t.Fatal("matchesTargetDevice returned false, want true")
	}

	hpd.productID = 0x9999
	if hpd.matchesTargetDevice(deviceDir) {
		t.Fatal("matchesTargetDevice returned true with mismatched PID")
	}
}

func TestHotPlugDetectorGetDevicePath(t *testing.T) {
	const pollInterval = time.Second

	sysfsRoot := t.TempDir()
	devRoot := t.TempDir()

	deviceDir := filepath.Join(sysfsRoot, "2-1.4")
	targetDir := filepath.Join(deviceDir, "2-1.4:1.0", "tty", DefaultTargetDevice)
	if err := os.MkdirAll(targetDir, 0o755); err != nil {
		t.Fatalf("failed to create target tty directory: %v", err)
	}

	hpd := NewHotPlugDetector(pollInterval, 0, 0)
	hpd.devRoot = devRoot

	got := hpd.getDevicePath(deviceDir)
	want := filepath.Join(devRoot, DefaultTargetDevice)
	if got != want {
		t.Fatalf("getDevicePath = %q, want %q", got, want)
	}

	missingTTYDir := filepath.Join(sysfsRoot, "3-2.1")
	if err := os.MkdirAll(missingTTYDir, 0o755); err != nil {
		t.Fatalf("failed to create device directory: %v", err)
	}

	got = hpd.getDevicePath(missingTTYDir)
	if got != want {
		t.Fatalf("getDevicePath fallback = %q, want %q", got, want)
	}
}

func TestHotPlugDetectorRunInotify(t *testing.T) {
	const pollInterval = time.Second
	const eventWait = time.Second

	usbRoot := t.TempDir()
	devRoot := t.TempDir()

	hpd := NewHotPlugDetector(pollInterval, 0, 0)
	hpd.usbDevicesPath = usbRoot
	hpd.devRoot = devRoot

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	eventCh := make(chan HotPlugEvent, 1)
	errCh := make(chan error, 1)

	go func() {
		errCh <- hpd.runInotify(ctx, func(event HotPlugEvent) {
			eventCh <- event
		})
	}()

	// Give the inotify watcher time to initialize before creating the device
	time.Sleep(100 * time.Millisecond)

	stagingDir, err := os.MkdirTemp("", "hpd-device-")
	if err != nil {
		t.Fatalf("failed to create staging dir: %v", err)
	}
	defer os.RemoveAll(stagingDir)

	deviceDir := filepath.Join(stagingDir, "1-9.1")
	ttyDir := filepath.Join(deviceDir, "1-9.1:1.0", "tty", DefaultTargetDevice)
	if err := os.MkdirAll(ttyDir, 0o755); err != nil {
		t.Fatalf("failed to create staging tty dir: %v", err)
	}

	finalDeviceDir := filepath.Join(usbRoot, "1-9.1")
	if err := os.Rename(deviceDir, finalDeviceDir); err != nil {
		t.Fatalf("failed to move device dir into usb root: %v", err)
	}

	select {
	case event := <-eventCh:
		if event.Action != "add" {
			t.Fatalf("event action = %q, want %q", event.Action, "add")
		}
		wantDevice := filepath.Join(devRoot, DefaultTargetDevice)
		if event.Device != wantDevice {
			t.Fatalf("event device = %q, want %q", event.Device, wantDevice)
		}
	case <-time.After(eventWait):
		t.Fatal("timed out waiting for hotplug event")
	}

	cancel()

	select {
	case err := <-errCh:
		if err != nil {
			t.Fatalf("runInotify returned error: %v", err)
		}
	case <-time.After(eventWait):
		t.Fatal("timed out waiting for runInotify to exit")
	}
}

func writeUSBIDFiles(deviceDir string, vid, pid uint16) error {
	if err := os.MkdirAll(deviceDir, 0o755); err != nil {
		return err
	}

	if err := os.WriteFile(filepath.Join(deviceDir, "idVendor"), []byte(fmt.Sprintf("%04x", vid)), 0o644); err != nil {
		return err
	}

	if err := os.WriteFile(filepath.Join(deviceDir, "idProduct"), []byte(fmt.Sprintf("%04x", pid)), 0o644); err != nil {
		return err
	}

	return nil
}
