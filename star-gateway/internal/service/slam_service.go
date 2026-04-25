// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package service

import (
	"context"
	"fmt"
	"os/exec"
	"regexp"
	"strings"
)

// servicePathRegex validates that a ROS2 service path starts with '/' and
// contains only letters, numbers, underscores, and forward slashes.
var servicePathRegex = regexp.MustCompile(`^/[a-zA-Z0-9_/]+$`)

// serviceTypeRegex validates that a ROS2 service type matches the
// package/srv/ServiceName pattern (letters, numbers, and underscores only).
var serviceTypeRegex = regexp.MustCompile(`^[a-zA-Z0-9_]+/srv/[a-zA-Z0-9_]+$`)

// SLAMService encapsulates SLAM-related operations exposed to HTTP handlers.
type SLAMService interface {
	Reset(ctx context.Context) error
}

// ros2Binary is the name of the ROS2 CLI tool invoked by ros2SLAMService.Reset
// to call a ROS2 service over the local DDS bus. Defined as a constant so it
// is easy to locate and update if the tool name changes (e.g., in a different
// ROS2 distro installation layout).
const ros2Binary = "ros2"

// ros2ServiceCmd, ros2CallCmd, and ros2EmptyPayload are the literal CLI
// sub-command arguments passed to ros2Binary when calling a ROS2 service.
// Defined as constants so they are easy to locate and update.
const (
	ros2ServiceCmd   = "service"
	ros2CallCmd      = "call"
	ros2EmptyPayload = "{}"
)

type ros2SLAMService struct {
	resetServicePath string
	resetServiceType string
}

func newROS2SLAMService(resetServicePath, resetServiceType string) (SLAMService, error) {
	resetServicePath = strings.TrimSpace(resetServicePath)
	resetServiceType = strings.TrimSpace(resetServiceType)
	if !servicePathRegex.MatchString(resetServicePath) {
		return nil, fmt.Errorf("invalid resetServicePath %q: must start with '/' and contain only letters, numbers, underscores, and forward slashes", resetServicePath)
	}
	if !serviceTypeRegex.MatchString(resetServiceType) {
		return nil, fmt.Errorf("invalid resetServiceType %q: must match package/srv/ServiceName pattern with only letters, numbers, and underscores", resetServiceType)
	}
	return &ros2SLAMService{
		resetServicePath: resetServicePath,
		resetServiceType: resetServiceType,
	}, nil
}

// newSLAMServiceFn is a test hook used by package tests to replace the default
// constructor for ros2-backed SLAM reset logic.
//
// Test safety requirements (same as app package transport hooks):
//   - Tests overriding this hook must NOT call t.Parallel().
//   - Tests must restore the original function before returning.
//   - This hook exists only to support tests and should not be used by
//     production call sites.
var newSLAMServiceFn = newROS2SLAMService

// NewROS2SLAMService creates a SLAM service that resets slam_toolbox via ros2 CLI.
func NewROS2SLAMService(resetServicePath, resetServiceType string) (SLAMService, error) {
	return newSLAMServiceFn(resetServicePath, resetServiceType)
}

func (s *ros2SLAMService) Reset(ctx context.Context) error {
	output, err := exec.CommandContext(
		ctx,
		ros2Binary,
		ros2ServiceCmd,
		ros2CallCmd,
		s.resetServicePath,
		s.resetServiceType,
		ros2EmptyPayload,
	).CombinedOutput()
	if err != nil {
		trimmedOutput := strings.TrimSpace(string(output))
		if trimmedOutput != "" {
			return fmt.Errorf("failed to call ros2 service reset: %w; output=%q", err, trimmedOutput)
		}
		return fmt.Errorf("failed to call ros2 service reset: %w", err)
	}
	return nil
}
