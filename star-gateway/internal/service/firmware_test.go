// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package service

import (
	"testing"
)

func TestNewFirmwareService(t *testing.T) {
	svc := NewFirmwareService()
	if svc == nil {
		t.Fatal("Expected non-nil service")
	}
}
