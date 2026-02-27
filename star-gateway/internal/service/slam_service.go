package service

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
)

// SLAMService encapsulates SLAM-related operations exposed to HTTP handlers.
type SLAMService interface {
	Reset(ctx context.Context) error
}

// ros2Binary is the name of the ROS2 CLI tool invoked by ros2SLAMService.Reset
// to call a ROS2 service over the local DDS bus. Defined as a constant so it
// is easy to locate and update if the tool name changes (e.g., in a different
// ROS2 distro installation layout).
const ros2Binary = "ros2"

type ros2SLAMService struct {
	resetServicePath string
	resetServiceType string
}

func newROS2SLAMService(resetServicePath, resetServiceType string) (SLAMService, error) {
	resetServicePath = strings.TrimSpace(resetServicePath)
	resetServiceType = strings.TrimSpace(resetServiceType)
	if resetServicePath == "" || !strings.HasPrefix(resetServicePath, "/") {
		return nil, fmt.Errorf("invalid resetServicePath: must be non-empty and start with '/'")
	}
	if resetServiceType == "" || !strings.Contains(resetServiceType, "/") {
		return nil, fmt.Errorf("invalid resetServiceType: must be non-empty and contain '/'")
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
		"service",
		"call",
		s.resetServicePath,
		s.resetServiceType,
		"{}",
	).CombinedOutput()
	if err != nil {
		trimmedOutput := strings.TrimSpace(string(output))
		if trimmedOutput != "" {
			return fmt.Errorf("failed to call ros2 service reset: %w: output=%q", err, trimmedOutput)
		}
		return fmt.Errorf("failed to call ros2 service reset: %w", err)
	}
	return nil
}
