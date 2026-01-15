package service

import (
	"testing"
)

func TestNewTelemetryService(t *testing.T) {
	svc := NewTelemetryService()
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
}
