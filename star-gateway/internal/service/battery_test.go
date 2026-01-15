package service

import (
	"testing"
)

func TestNewBatteryService(t *testing.T) {
	svc := NewBatteryService()
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
}
