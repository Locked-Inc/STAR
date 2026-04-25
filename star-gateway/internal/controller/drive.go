// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package controller

import (
	"math"
)

// ArcadeDrive converts gamepad stick inputs to left/right motor velocities.
// linearVel: [-1.0, 1.0] (forward/backward)
// angularVel: [-1.0, 1.0] (right/left)
// Returns: (leftMotor, rightMotor) in range [-1.0, 1.0]
func ArcadeDrive(linearVel, angularVel float32) (float32, float32) {
	// Clamp inputs
	linearVel = clamp(linearVel, -1.0, 1.0)
	angularVel = clamp(angularVel, -1.0, 1.0)

	leftMotor := linearVel + angularVel
	rightMotor := linearVel - angularVel

	// Normalize if magnitude > 1.0 to preserve ratio
	maxMag := float32(math.Max(math.Abs(float64(leftMotor)), math.Abs(float64(rightMotor))))
	if maxMag > 1.0 {
		leftMotor /= maxMag
		rightMotor /= maxMag
	}

	return leftMotor, rightMotor
}

func clamp(v, min, max float32) float32 {
	if v < min {
		return min
	}
	if v > max {
		return max
	}
	return v
}
