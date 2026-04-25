// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"fmt"
	"strings"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
)

// TransportMode defines the transport selection strategy.
type TransportMode string

const (
	// ModeAuto automatically selects the best available transport, preferring USB over SPI.
	ModeAuto TransportMode = "auto"

	// ModeForceUSB only uses USB CDC transport. Returns error if USB is unavailable.
	ModeForceUSB TransportMode = "force-usb"

	// ModeForceSPI only uses SPI transport. USB hot-plug is disabled in this mode.
	ModeForceSPI TransportMode = "force-spi"

	// ModeSimpleUSB uses USB CDC without HARQ overhead. USB is inherently reliable
	// (USB 2.0 CRC-16 + automatic retransmission at hardware level). Skips reset
	// handshake, disables strict sequence validation, and relaxes health monitoring.
	// CRC-32 frame validation is still performed as defense-in-depth. Wire protocol
	// is unchanged.
	ModeSimpleUSB TransportMode = "simple-usb"
)
// Error message format strings used by manager validation routines.
const (
	// InvalidTransportModeError is the format string returned by ParseTransportMode
	// when given an unrecognized mode string. Use with fmt.Errorf and the offending input.
	InvalidTransportModeError = "invalid transport mode: %q"
)

// IsValid checks if the TransportMode is one of the defined constants.
func (tm TransportMode) IsValid() bool {
	switch tm {
	case ModeAuto, ModeForceUSB, ModeForceSPI, ModeSimpleUSB:
		return true
	default:
		return false
	}
}

// ParseTransportMode converts a string to a TransportMode. Returns ModeAuto and an error if the input is invalid.
func ParseTransportMode(s string) (TransportMode, error) {
	// Normalize input to lowercase for case-insensitive matching
	mode := TransportMode(strings.ToLower(s))
	if !mode.IsValid() {
		return ModeAuto, fmt.Errorf(InvalidTransportModeError, s)
	}
	return mode, nil
}

// State represents the internal state of the TransportManager.
type State uint8

const (
	// StateInitializing indicates the manager is starting up and detecting transports.
	StateInitializing State = iota

	// StateActiveUSB indicates USB CDC is the active transport.
	StateActiveUSB

	// StateActiveSPI indicates SPI is the active transport.
	StateActiveSPI

	// StateSwitchingToUSB indicates a switch to USB is in progress.
	StateSwitchingToUSB

	// StateSwitchingToSPI indicates a switch to SPI is in progress.
	StateSwitchingToSPI

	// StateDegraded indicates the active transport is unhealthy but no better alternative exists.
	StateDegraded

	// StateFailed indicates no transports are available.
	StateFailed
)

// String returns a human-readable state name.
func (s State) String() string {
	switch s {
	case StateInitializing:
		return "Initializing"
	case StateActiveUSB:
		return "Active(USB)"
	case StateActiveSPI:
		return "Active(SPI)"
	case StateSwitchingToUSB:
		return "Switching->USB"
	case StateSwitchingToSPI:
		return "Switching->SPI"
	case StateDegraded:
		return "Degraded"
	case StateFailed:
		return "Failed"
	default:
		return "Unknown"
	}
}

// FailureType indicates why a transport switch was triggered.
// This determines whether the TransportManager should drain in-flight operations
// before switching (CRITICAL FIX #2: Smart Drain Logic).
type FailureType int

const (
	// FailureTypeGraceful indicates a graceful switch (config change, priority upgrade).
	// Drain in-flight operations before switching.
	FailureTypeGraceful FailureType = iota

	// FailureTypeTimeout indicates a heartbeat timeout (200ms without frames).
	// Drain in-flight operations before switching (safe to wait).
	FailureTypeTimeout

	// FailureTypeIOError indicates a hard IO error (write failed, device error).
	// Skip drain - device is non-responsive, don't waste time waiting.
	FailureTypeIOError

	// FailureTypeHealthCheck indicates a failed health probe.
	// Skip drain - transport already known to be unhealthy.
	FailureTypeHealthCheck

	// FailureTypeHotPlugRemove indicates USB device was physically unplugged.
	// Skip drain - device is gone, no point waiting for in-flight operations.
	FailureTypeHotPlugRemove
)

// String returns a human-readable failure type name.
func (ft FailureType) String() string {
	switch ft {
	case FailureTypeGraceful:
		return "Graceful"
	case FailureTypeTimeout:
		return "Timeout"
	case FailureTypeIOError:
		return "IOError"
	case FailureTypeHealthCheck:
		return "HealthCheck"
	case FailureTypeHotPlugRemove:
		return "HotPlugRemove"
	default:
		return "Unknown"
	}
}

// TransportWrapper wraps a transport with health metrics and metadata.
type TransportWrapper struct {
	// Name is the transport identifier ("spi" or "usb").
	Name string

	// Transport is the actual HARQ implementation.
	Transport harq.HARQ

	// Decoder is the streaming frame decoder.
	Decoder *frame.StreamDecoder

	// Health tracks operational metrics for this transport.
	Health *HealthMetrics

	// Priority determines selection order (higher = preferred).
	// USB typically has priority 10, SPI has priority 5.
	Priority int

	// Available indicates if this transport is currently usable.
	Available bool
}

const (
	// PriorityUSB is the default priority for USB transport (higher = preferred).
	PriorityUSB = 10

	// PrioritySPI is the default priority for SPI transport.
	PrioritySPI = 5
)

// packetLossWindowSize is the number of recent operations tracked for loss rate calculation.
const packetLossWindowSize = 100

// HealthMetrics tracks the operational health of a transport.
type HealthMetrics struct {
	// LastSuccess is the timestamp of the last successful operation.
	LastSuccess time.Time

	// LastFailure is the timestamp of the last failed operation.
	LastFailure time.Time

	// LastRecovery is the timestamp of the most recent unhealthy->healthy transition.
	// Used for failback hysteresis: transports must stay healthy for FailbackDamping
	// before being eligible for priority-based selection again.
	LastRecovery time.Time

	// ConsecutiveFailures counts failures since last success.
	// Resets to 0 on any successful operation.
	ConsecutiveFailures int

	// TotalSent is the cumulative count of successful Send operations.
	TotalSent uint64

	// TotalReceived is the cumulative count of successful Receive operations.
	TotalReceived uint64

	// TotalErrors is the cumulative count of all errors.
	TotalErrors uint64

	// AvgLatency is the moving average of operation latency.
	AvgLatency time.Duration

	// PacketLossRate is the fraction of failed operations in the sliding window [0.0, 1.0].
	PacketLossRate float64

	// IsHealthy indicates if this transport is considered healthy.
	// Set to false when ANY threshold is exceeded:
	//   - ConsecutiveFailures >= FailureThreshold
	//   - AvgLatency > HealthLatencyThreshold
	//   - PacketLossRate > HealthLossThreshold
	IsHealthy bool

	// lossWindow is a circular buffer tracking success (true) / failure (false)
	// for the most recent packetLossWindowSize operations.
	lossWindow [packetLossWindowSize]bool

	// lossWindowIdx is the next write position in lossWindow.
	lossWindowIdx int

	// lossWindowCount is the number of entries written (capped at packetLossWindowSize).
	lossWindowCount int
}

// NewHealthMetrics creates a new HealthMetrics instance with default values.
func NewHealthMetrics() *HealthMetrics {
	return &HealthMetrics{
		LastSuccess: time.Now(),
		IsHealthy:   true,
	}
}

// HotPlugEvent represents a USB device add/remove event.
type HotPlugEvent struct {
	// Action is either "add" or "remove".
	Action string

	// Device is the device path (e.g., "/dev/ttyUSB0").
	Device string

	// Timestamp is when the event was detected.
	Timestamp time.Time

	// VendorID is the USB Vendor ID (if available).
	VendorID uint16

	// ProductID is the USB Product ID (if available).
	ProductID uint16
}
