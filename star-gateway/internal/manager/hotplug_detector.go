package manager

import (
	"context"
	"log"
	"time"
)

// HotPlugDetector monitors for USB device add/remove events.
//
// On Linux, this uses inotify to watch /dev for ttyACM* device creation/removal.
// On other platforms, it falls back to polling.
//
// NOTE: This is a stub implementation for Phase 1.
// Full implementation with inotify will be added in Phase 2 (Issue #217).
type HotPlugDetector struct {
	pollInterval time.Duration
	vendorID     uint16
	productID    uint16
}

// NewHotPlugDetector creates a new HotPlugDetector.
func NewHotPlugDetector(pollInterval time.Duration, vid, pid uint16) *HotPlugDetector {
	if pollInterval <= 0 {
		panic("HotPlugDetector: pollInterval must be positive")
	}
	return &HotPlugDetector{
		pollInterval: pollInterval,
		vendorID:     vid,
		productID:    pid,
	}
}

// Run starts the hot-plug detection loop.
// It exits when ctx is cancelled.
func (hpd *HotPlugDetector) Run(ctx context.Context, eventHandler func(HotPlugEvent)) {
	ticker := time.NewTicker(hpd.pollInterval)
	defer ticker.Stop()

	log.Printf("HotPlugDetector started (VID=0x%04X PID=0x%04X, poll interval: %v)",
		hpd.vendorID, hpd.productID, hpd.pollInterval)

	for {
		select {
		case <-ctx.Done():
			log.Printf("HotPlugDetector stopped")
			return

		case <-ticker.C:
			// TODO: Implement actual device detection in Phase 2
			// For now, this is a no-op stub
		}
	}
}
