package manager

import (
	"testing"
	"time"
)

func TestNewHotPlugDetector_PanicOnInvalidInterval(t *testing.T) {
	tests := []struct {
		name     string
		interval time.Duration
	}{
		{"ZeroInterval", 0},
		{"NegativeInterval", -1 * time.Second},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			defer func() {
				if r := recover(); r == nil {
					t.Errorf("NewHotPlugDetector() did not panic for interval %v", tt.interval)
				}
			}()
			NewHotPlugDetector(tt.interval, 0x1234, 0x5678)
		})
	}
}

func TestNewHotPlugDetector_ValidInterval(t *testing.T) {
	interval := 1 * time.Second
	hpd := NewHotPlugDetector(interval, 0x1234, 0x5678)
	if hpd == nil {
		t.Fatalf("NewHotPlugDetector() returned nil for valid interval")
	}
	if hpd.pollInterval != interval {
		t.Errorf("expected pollInterval %v, got %v", interval, hpd.pollInterval)
	}
}
