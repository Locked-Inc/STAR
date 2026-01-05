package controller

import (
	"testing"
)

func TestArcadeDrive(t *testing.T) {
	tests := []struct {
		name           string
		linear, angular float32
		wantLeft, wantRight float32
	}{
		{"Stop", 0, 0, 0, 0},
		{"Full Forward", 1.0, 0, 1.0, 1.0},
		{"Full Backward", -1.0, 0, -1.0, -1.0},
		{"Spin Right", 0, 1.0, 1.0, -1.0}, // Left forward, Right backward
		{"Spin Left", 0, -1.0, -1.0, 1.0}, // Left backward, Right forward
		{"Forward Right Turn", 0.5, 0.5, 1.0, 0.0},
		{"Forward Left Turn", 0.5, -0.5, 0.0, 1.0},
		{"Clamp Overflow", 1.0, 1.0, 1.0, 0.0}, // 2.0/2.0 -> 1.0, 0/2.0 -> 0
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			gotLeft, gotRight := ArcadeDrive(tt.linear, tt.angular)
			if !almostEqual(gotLeft, tt.wantLeft) || !almostEqual(gotRight, tt.wantRight) {
				t.Errorf("ArcadeDrive(%v, %v) = (%v, %v), want (%v, %v)",
					tt.linear, tt.angular, gotLeft, gotRight, tt.wantLeft, tt.wantRight)
			}
		})
	}
}

func almostEqual(a, b float32) bool {
	return (a-b) < 0.001 && (b-a) < 0.001
}
