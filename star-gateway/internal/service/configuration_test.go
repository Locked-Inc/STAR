package service

import (
	"testing"
)

func TestNewConfigurationService(t *testing.T) {
	svc := NewConfigurationService()
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
}
