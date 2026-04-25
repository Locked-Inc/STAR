// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package manager tests for HARQ reader.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"errors"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/testutil"
)

func TestHARQReader_Read(t *testing.T) {
	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: []byte("hello")}, nil
		},
	}

	reader := NewHARQReader(mockHARQ)
	buf := make([]byte, 10)

	// First read should get "hello"
	n, err := reader.Read(buf)
	if err != nil {
		t.Fatalf("Read failed: %v", err)
	}
	if n != 5 {
		t.Errorf("Read count = %d, want 5", n)
	}
	if string(buf[:n]) != "hello" {
		t.Errorf("Read data = %s, want hello", string(buf[:n]))
	}

	// Test EOF behavior or error handling if needed
	// Assuming Read blocks until data, so standard io.Reader behavior might not apply strictly
	// unless Receive returns error.

	mockHARQ.ReceiveFunc = func(ctx context.Context) (*harq.ReceiveResult, error) {
		return nil, errors.New("receive error")
	}

	_, err = reader.Read(buf)
	if err == nil {
		t.Error("Expected error from Read")
	}
}

func TestHARQReader_Read_Buffer(t *testing.T) {
	// Test reading from internal buffer when p is smaller than payload
	mockHARQ := &testutil.MockHARQ{
		ReceiveFunc: func(ctx context.Context) (*harq.ReceiveResult, error) {
			return &harq.ReceiveResult{Payload: []byte("helloworld")}, nil
		},
	}

	reader := NewHARQReader(mockHARQ)
	buf := make([]byte, 5)

	// First read: "hello"
	n, err := reader.Read(buf)
	if err != nil {
		t.Fatalf("Read 1 failed: %v", err)
	}
	if n != 5 {
		t.Errorf("Read 1 count = %d, want 5", n)
	}
	if string(buf[:n]) != "hello" {
		t.Errorf("Read 1 data = %s, want hello", string(buf[:n]))
	}

	// Second read: "world" (should come from buffer, not new Receive)
	callCount := 0
	mockHARQ.ReceiveFunc = func(ctx context.Context) (*harq.ReceiveResult, error) {
		callCount++
		return nil, errors.New("should not be called")
	}

	n, err = reader.Read(buf)
	if err != nil {
		t.Fatalf("Read 2 failed: %v", err)
	}
	if n != 5 {
		t.Errorf("Read 2 count = %d, want 5", n)
	}
	if string(buf[:n]) != "world" {
		t.Errorf("Read 2 data = %s, want world", string(buf[:n]))
	}

	if callCount > 0 {
		t.Error("Receive was called unexpectedly")
	}
}
