// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package dispatcher implements a centralized message router for the star-gateway.
//
// The Dispatcher solves the concurrency race condition where multiple services
// (MotorControlService, TelemetryService) all call HARQ.Receive()
// in parallel, causing frame stealing and deserialization errors.
//
// Architecture:
//  1. Dispatcher owns the single HARQ.Receive() loop
//  2. Services subscribe to specific message types
//  3. Dispatcher demultiplexes messages via WireMessage.oneof
//  4. Subscribed channels receive only their type
//
// STAR Project - Texas A&M University
// January 2026
package dispatcher

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// DispatchedMessage bundles a WireMessage with frame metadata for diagnostics.
// Exported so services can access the metadata.
type DispatchedMessage struct {
	// WireMsg is the decoded protobuf WireMessage payload.
	WireMsg *starv1.WireMessage
	// Metadata is the HARQ-layer frame metadata (sequence, retransmits, FEC flag).
	Metadata *harq.FrameMetadata
}

// Configuration constants.
const (
	// DefaultShutdownTimeout is the default maximum time to wait for graceful shutdown.
	DefaultShutdownTimeout = 5 * time.Second

	// DefaultSubscriberBufferSize is the default capacity for subscriber channels.
	DefaultSubscriberBufferSize = 100

	// DefaultReceiveInterval is the default interval between receive attempts (100Hz for SPI).
	// SPI communication spec requires 100Hz telemetry rate (10ms period).
	// USB transports can use faster intervals (e.g., 1ms) since they have built-in reliability.
	DefaultReceiveInterval = 10 * time.Millisecond

	// dispatchBackoff is the backoff duration used in the dispatcher loop to prevent busy-looping.
	dispatchBackoff time.Duration = 10 * time.Millisecond
)

// MessageType identifies a message type in the WireMessage union.
type MessageType uint32

const (
	// MessageTypeUnknown represents an unknown or unset message type.
	MessageTypeUnknown MessageType = iota

	// MessageTypeVelocityCommand is a VelocityCommand (motor control).
	MessageTypeVelocityCommand

	// MessageTypeEmergencyStopCommand is an EmergencyStopCommand.
	MessageTypeEmergencyStopCommand

	// MessageTypeMotorPowerCommand is a MotorPowerCommand.
	MessageTypeMotorPowerCommand

	// MessageTypeTelemetryData is a TelemetryData (sensor readings).
	MessageTypeTelemetryData

	// MessageTypeEncoderData is an EncoderData (encoder feedback).
	MessageTypeEncoderData

	// MessageTypePidConfig is a PidConfig (tuning parameters).
	MessageTypePidConfig
)

// Predefined errors for dispatcher operations.
var (
	// ErrDispatcherStopped is returned when attempting to use a stopped dispatcher.
	ErrDispatcherStopped = errors.New("dispatcher: dispatcher is stopped")

	// ErrHARQNil is returned when HARQ handler is nil.
	ErrHARQNil = errors.New("dispatcher: HARQ handler is nil")

	// ErrLoggerNil is returned when logger is nil.
	ErrLoggerNil = errors.New("dispatcher: logger is nil")
)

// Dispatcher defines the interface for centralized message routing.
type Dispatcher interface {
	// Subscribe registers a channel to receive messages of a specific type.
	// Returns a channel that will receive DispatchedMessage pointers.
	// The caller should drain this channel and call Unsubscribe when done.
	Subscribe(msgType MessageType) <-chan *DispatchedMessage

	// Unsubscribe removes a subscription.
	// Safe to call multiple times or with channels not subscribed.
	Unsubscribe(msgType MessageType, ch <-chan *DispatchedMessage)

	// Start begins the receive loop in a goroutine.
	// Returns immediately. The dispatcher will continue until Stop() is called
	// or the context is canceled.
	Start(ctx context.Context) error

	// Stop gracefully shuts down the dispatcher.
	// All subscribed channels will be closed.
	// Blocks until the receive loop has exited or timeout occurs.
	// Returns an error if shutdown times out.
	Stop() error

	// GetState returns the current dispatcher state (for testing/diagnostics).
	GetState() State
}

// State represents the dispatcher operational state.
type State uint8

const (
	// StateIdle indicates the dispatcher is idle (not started).
	StateIdle State = iota

	// StateRunning indicates the dispatcher is actively receiving messages.
	StateRunning

	// StateStopping indicates the dispatcher is shutting down.
	StateStopping

	// StateStopped indicates the dispatcher has stopped.
	StateStopped
)

// dispatcher implements Dispatcher interface.
type dispatcher struct {
	harq   harq.HARQ
	logger *slog.Logger

	mu              sync.RWMutex
	state           State
	subscribers     map[MessageType][]chan *DispatchedMessage
	stopCh          chan struct{}
	stoppedCh       chan struct{}
	shutdownTimeout time.Duration
	receiveInterval time.Duration
	cancel          context.CancelFunc
}

// Config holds dispatcher configuration parameters.
type Config struct {
	// ShutdownTimeout is the maximum time to wait for graceful shutdown.
	// If zero, defaults to DefaultShutdownTimeout.
	ShutdownTimeout time.Duration

	// ReceiveInterval is the interval between HARQ receive attempts.
	// If zero or negative, defaults to DefaultReceiveInterval (10ms for 100Hz SPI rate).
	// For USB transports, consider using 1ms for faster response.
	ReceiveInterval time.Duration
}

// NewDispatcher creates a new message dispatcher.
// The harq handler and logger are required.
func NewDispatcher(h harq.HARQ, logger *slog.Logger, cfg *Config) (Dispatcher, error) {
	if h == nil {
		return nil, ErrHARQNil
	}
	if logger == nil {
		return nil, ErrLoggerNil
	}

	if cfg == nil {
		cfg = &Config{}
	}

	shutdownTimeout := cfg.ShutdownTimeout
	if shutdownTimeout == 0 {
		shutdownTimeout = DefaultShutdownTimeout
	}

	receiveInterval := cfg.ReceiveInterval
	if receiveInterval <= 0 {
		receiveInterval = DefaultReceiveInterval
	}

	return &dispatcher{
		harq:            h,
		logger:          logger,
		state:           StateIdle,
		subscribers:     make(map[MessageType][]chan *DispatchedMessage),
		stopCh:          make(chan struct{}),
		stoppedCh:       make(chan struct{}),
		shutdownTimeout: shutdownTimeout,
		receiveInterval: receiveInterval,
	}, nil
}

// Subscribe registers a channel for a message type.
func (d *dispatcher) Subscribe(msgType MessageType) <-chan *DispatchedMessage {
	d.mu.Lock()
	defer d.mu.Unlock()

	// Create buffered channel to prevent blocking senders
	ch := make(chan *DispatchedMessage, DefaultSubscriberBufferSize)
	d.subscribers[msgType] = append(d.subscribers[msgType], ch)

	d.logger.Debug("subscriber registered", slog.String("message_type", fmt.Sprintf("%d", msgType)))

	return ch
}

// Unsubscribe removes a subscription.
func (d *dispatcher) Unsubscribe(msgType MessageType, ch <-chan *DispatchedMessage) {
	d.mu.Lock()
	defer d.mu.Unlock()

	subscribers, exists := d.subscribers[msgType]
	if !exists {
		return
	}

	// Remove the channel from subscribers list
	filtered := make([]chan *DispatchedMessage, 0, len(subscribers))
	for _, sub := range subscribers {
		if sub != ch {
			filtered = append(filtered, sub)
		} else {
			// Close the channel to signal completion
			close(sub)
		}
	}

	if len(filtered) == 0 {
		delete(d.subscribers, msgType)
	} else {
		d.subscribers[msgType] = filtered
	}

	d.logger.Debug("subscriber unregistered", slog.String("message_type", fmt.Sprintf("%d", msgType)))
}

// Start begins the receive loop.
func (d *dispatcher) Start(ctx context.Context) error {
	d.mu.Lock()
	if d.state != StateIdle {
		d.mu.Unlock()
		return fmt.Errorf("dispatcher: cannot start from state %v", d.state)
	}
	d.state = StateRunning
	ctx, cancel := context.WithCancel(ctx)
	d.cancel = cancel
	d.mu.Unlock()

	d.logger.Info("dispatcher starting")

	go d.receiveLoop(ctx)
	return nil
}

// Stop gracefully shuts down the dispatcher.
// Returns an error if the dispatcher doesn't stop within the configured timeout.
func (d *dispatcher) Stop() error {
	d.mu.Lock()
	currentState := d.state
	if d.state == StateRunning {
		d.state = StateStopping
		close(d.stopCh)
		if d.cancel != nil {
			d.cancel()
		}
	}
	d.mu.Unlock()

	// If never started, nothing to wait for
	if currentState == StateIdle {
		d.logger.Debug("dispatcher stop called but never started")
		return nil
	}

	// Wait for receive loop to exit with timeout protection
	select {
	case <-d.stoppedCh:
		d.logger.Info("dispatcher stopped gracefully")
		return nil
	case <-time.After(d.shutdownTimeout):
		d.logger.Error("dispatcher stop timeout - receive loop may be hung",
			slog.Duration("timeout", d.shutdownTimeout))
		return fmt.Errorf("dispatcher: stop timeout after %v", d.shutdownTimeout)
	}
}

// GetState returns the current dispatcher state.
func (d *dispatcher) GetState() State {
	d.mu.RLock()
	defer d.mu.RUnlock()
	return d.state
}

// receiveLoop runs in a goroutine and receives messages from HARQ.
func (d *dispatcher) receiveLoop(ctx context.Context) {
	defer func() {
		// Close all subscriber channels
		d.mu.Lock()
		for msgType := range d.subscribers {
			for _, ch := range d.subscribers[msgType] {
				close(ch)
			}
		}
		d.subscribers = make(map[MessageType][]chan *DispatchedMessage)
		d.state = StateStopped
		d.mu.Unlock()

		close(d.stoppedCh)
		d.logger.Debug("dispatcher receive loop exited")
	}()

	// Ticker for rate-limiting telemetry requests.
	// This prevents continuous polling and implements proper request/response pattern.
	// Default is 10ms (100Hz) for SPI. USB transports can configure faster intervals.
	ticker := time.NewTicker(d.receiveInterval)
	defer ticker.Stop()

	for {
		// Prioritize context cancellation by checking it first (non-blocking)
		select {
		case <-ctx.Done():
			return
		case <-d.stopCh:
			return
		default:
		}

		// Double-check cancellation before potentially blocking receive call
		select {
		case <-ctx.Done():
			return
		case <-d.stopCh:
			return
		default:
		}

		result, err := d.harq.Receive(ctx)
		if err == nil && result == nil {
			err = harq.ErrTimeout
		}
		if err != nil {
			// Exit on context cancellation
			if errors.Is(err, context.Canceled) {
				return // Just return, do not return ctx.Err()
			}
			// Log other errors and continue or break based on severity
			if errors.Is(err, harq.ErrTimeout) || errors.Is(err, harq.ErrDuplicateFrame) {
				// Transient errors - continue to ticker wait
			} else if errors.Is(err, harq.ErrInErrorState) {
				d.logger.Warn("HARQ in error state, resetting", slog.String("error", err.Error()))
				d.harq.Reset()
			} else {
				// Other errors (connection lost, max retries exceeded)
				d.logger.Error("HARQ receive error", slog.String("error", err.Error()))

				// Add a small backoff here to prevent busy-looping on hard failures
				// Use context-aware sleep to enable immediate shutdown on cancellation
				select {
				case <-ctx.Done():
					return
				case <-d.stopCh:
					return
				case <-time.After(dispatchBackoff):
					// Continue to ticker wait
				}
			}
		} else {
			// Check for cancellation before processing data
			select {
			case <-ctx.Done():
				d.logger.Debug("dispatcher context cancelled", slog.String("error", ctx.Err().Error()))
				return
			case <-d.stopCh:
				d.logger.Debug("dispatcher stop requested")
				return
			default:
			}

			// Unmarshal WireMessage
			var wrapper starv1.WireMessage
			if err := proto.Unmarshal(result.Payload, &wrapper); err != nil {
				d.logger.Warn("failed to unmarshal wire message", slog.String("error", err.Error()))
			} else {
				// Determine message type and dispatch
				msgType, _ := d.extractPayload(&wrapper)
				if msgType == MessageTypeUnknown {
					d.logger.Debug("received message with no payload set")
				} else {
					d.dispatchMessage(msgType, &wrapper, &result.Metadata)
				}
			}
		}

		// Wait for next interval (rate limiting) before next receive attempt
		select {
		case <-ctx.Done():
			return
		case <-d.stopCh:
			return
		case <-ticker.C:
			// Ticker fired - proceed to next iteration
		}
	}
}

// extractPayload extracts the message type and payload from a WireMessage.
func (d *dispatcher) extractPayload(msg *starv1.WireMessage) (MessageType, interface{}) {
	switch payload := msg.Payload.(type) {
	case *starv1.WireMessage_VelocityCommand:
		return MessageTypeVelocityCommand, payload.VelocityCommand
	case *starv1.WireMessage_EmergencyStopCommand:
		return MessageTypeEmergencyStopCommand, payload.EmergencyStopCommand
	case *starv1.WireMessage_MotorPowerCommand:
		return MessageTypeMotorPowerCommand, payload.MotorPowerCommand
	case *starv1.WireMessage_TelemetryData:
		return MessageTypeTelemetryData, payload.TelemetryData
	case *starv1.WireMessage_EncoderData:
		return MessageTypeEncoderData, payload.EncoderData
	case *starv1.WireMessage_PidConfig:
		return MessageTypePidConfig, payload.PidConfig
	default:
		return MessageTypeUnknown, nil
	}
}

// dispatchMessage sends a message to all subscribers of that type.
func (d *dispatcher) dispatchMessage(msgType MessageType, msg *starv1.WireMessage, metadata *harq.FrameMetadata) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	subscribers, exists := d.subscribers[msgType]

	if !exists || len(subscribers) == 0 {
		d.logger.Debug("no subscribers for message type", slog.String("message_type", fmt.Sprintf("%d", msgType)))
		return
	}

	// Bundle message with metadata
	dispMsg := &DispatchedMessage{
		WireMsg:  msg,
		Metadata: metadata,
	}

	// Send to all subscribers (non-blocking)
	for _, ch := range subscribers {
		select {
		case ch <- dispMsg:
			// Message sent
		default:
			// Channel full, drop oldest message to make room
			d.logger.Warn("subscriber channel full, dropping message",
				slog.String("message_type", fmt.Sprintf("%d", msgType)))
			select {
			case <-ch: // Drain one
			default:
			}
			// Try again
			select {
			case ch <- dispMsg:
			default:
				d.logger.Error("failed to send message after draining channel",
					slog.String("message_type", fmt.Sprintf("%d", msgType)))
			}
		}
	}
}
