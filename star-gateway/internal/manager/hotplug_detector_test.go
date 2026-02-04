package manager

import (
	"testing"
	"time"
)

// TestNewHotPlugDetector_Valid tests creating a valid HotPlugDetector.
// Phase 3: inotify-based implementation ignores pollInterval (kept for API compatibility).
func TestNewHotPlugDetector_Valid(t *testing.T) {
	const (
		testVID = 0x045B
		testPID = 0x0235
	)

	// pollInterval is ignored in inotify implementation but kept for API compatibility
	hpd := NewHotPlugDetector(1*time.Second, testVID, testPID)
	if hpd == nil {
		t.Fatal("NewHotPlugDetector() returned nil")
	}

	if hpd.vendorID != testVID {
		t.Errorf("vendorID = 0x%04X, want 0x%04X", hpd.vendorID, testVID)
	}

	if hpd.productID != testPID {
		t.Errorf("productID = 0x%04X, want 0x%04X", hpd.productID, testPID)
	}

	if hpd.targetDevice != "ttyACM0" {
		t.Errorf("targetDevice = %q, want \"ttyACM0\"", hpd.targetDevice)
	}
}
