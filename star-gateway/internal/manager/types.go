// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
)

// TransportMode defines the transport selection strategy.
type TransportMode string

const (
	// ModeAuto automatically selects the best available transport, preferring USB over SPI.
	ModeAuto TransportMode = "auto"

	// ModePreferUSB is an alias for ModeAuto (explicitly prefer USB, fallback to SPI).
	ModePreferUSB TransportMode = "prefer-usb"

	// ModeForceUSB only uses USB CDC transport. Returns error if USB is unavailable.
	ModeForceUSB TransportMode = "force-usb"

	// ModeForceSPI only uses SPI transport. USB hot-plug is disabled in this mode.
	ModeForceSPI TransportMode = "force-spi"
)

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
		return "Switching→USB"
	case StateSwitchingToSPI:
		return "Switching→SPI"
	case StateDegraded:
		return "Degraded"
	case StateFailed:
		return "Failed"
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

// HealthMetrics tracks the operational health of a transport.
type HealthMetrics struct {
	// LastSuccess is the timestamp of the last successful operation.
	LastSuccess time.Time

	// LastFailure is the timestamp of the last failed operation.
	LastFailure time.Time

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

	// IsHealthy indicates if this transport is considered healthy.
	// Set to false when ConsecutiveFailures >= FailureThreshold.
	IsHealthy bool
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

	// Device is the device path (e.g., "/dev/ttyACM0").
	Device string

	// Timestamp is when the event was detected.
	Timestamp time.Time

	// VendorID is the USB Vendor ID (if available).
	VendorID uint16

	// ProductID is the USB Product ID (if available).
	ProductID uint16
}
