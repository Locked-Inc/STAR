// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"log"
	"sort"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
)

const (
	// inflightPollInterval is the interval for polling in-flight operations during drain.
	inflightPollInterval = 10 * time.Millisecond

	// TransportNameUSB is the identifier for USB CDC transport.
	TransportNameUSB = "usb"

	// TransportNameSPI is the identifier for SPI transport.
	TransportNameSPI = "spi"
)

// TransportManager manages multiple transports with automatic failover and health monitoring.
//
// It implements the harq.HARQ interface and acts as a transparent proxy to the active transport.
// The Dispatcher interacts with TransportManager exactly as it would with a single HARQ instance,
// while TransportManager handles all transport selection, health monitoring, and failover logic.
//
// Key features:
//   - Priority-based transport selection (USB=PriorityUSB, SPI=PrioritySPI)
//   - Health monitoring with configurable failure thresholds
//   - Automatic failover when active transport fails
//   - USB hot-plug detection (add/remove events)
//   - Zero-downtime transport switching with pause-drain-resume
//   - Thread-safe operations with fine-grained locking
//
// Thread Safety:
//   - All public methods are thread-safe
//   - Uses RWMutex for high read concurrency
//   - Operations lock prevents race conditions during switching
type TransportManager struct {
	// Configuration (immutable after creation)
	config *Config

	// State management
	mu                  sync.RWMutex
	state               State
	activeTransport     harq.HARQ
	activeTransportName string
	availableTransports map[string]*TransportWrapper

	// Operations control (prevents race conditions during switching)
	operationsMu    sync.Mutex
	operationsCond  *sync.Cond
	paused          bool
	inflightCounter int

	// Background routines lifecycle
	ctx    context.Context
	cancel context.CancelFunc
	wg     sync.WaitGroup

	// Components (initialized on demand)
	healthMonitor   *HealthMonitor
	hotPlugDetector *HotPlugDetector
	heartbeat       *HeartbeatManager

	// Session state (shared across all transports - CRITICAL FIX #1)
	sessionState *SessionState
}

// NewTransportManager creates a new TransportManager with the given configuration.
//
// If config is nil, DefaultConfig() is used.
// The manager is created in StateInitializing and must be started with Start().
//
// Example:
//
//	tm := manager.NewTransportManager(manager.DefaultConfig())
//	tm.RegisterTransport("spi", spiLink, manager.PrioritySPI)
//	tm.RegisterTransport("usb", usbLink, manager.PriorityUSB)
//	tm.Start(ctx)
func NewTransportManager(config *Config) *TransportManager {
	if config == nil {
		config = DefaultConfig()
	}

	tm := &TransportManager{
		config:              config,
		state:               StateInitializing,
		availableTransports: make(map[string]*TransportWrapper),
		sessionState:        NewSessionState(), // CRITICAL FIX #1: Shared across all transports
	}

	tm.operationsCond = sync.NewCond(&tm.operationsMu)

	// Health monitor is always created
	tm.healthMonitor = NewHealthMonitor(config.HealthCheckInterval)

	// Hot-plug detector only if enabled and not in force-spi mode
	if config.EnableHotPlug && config.Mode != ModeForceSPI {
		tm.hotPlugDetector = NewHotPlugDetector(config.HotPlugPollInterval, config.USBVID, config.USBPID)
	}

	// Heartbeat manager for hybrid implicit/explicit connectivity detection
	// Uses default intervals: DefaultPingInterval (50ms), DefaultFailureTimeout (200ms)
	// onFailure callback triggers automatic failover
	heartbeat, err := NewHeartbeatManager(
		DefaultPingInterval,   // 50ms - send PING if idle
		DefaultFailureTimeout, // 200ms - declare link dead if no frames
		func() {
			// Callback to trigger failover on heartbeat timeout
			log.Printf("Heartbeat failure detected, triggering failover")
			go tm.attemptFailover(FailureTypeTimeout)
		},
	)
	if err != nil {
		// This should never happen with valid constants, but handle gracefully
		log.Printf("FATAL: Failed to create HeartbeatManager: %v", err)
		panic(fmt.Sprintf("HeartbeatManager creation failed: %v", err))
	}
	tm.heartbeat = heartbeat

	return tm
}

// Start initializes and starts the TransportManager background routines.
//
// This method:
//  1. Starts the health monitor for inactive transports
//  2. Starts the hot-plug detector (if enabled)
//  3. Selects the initial active transport
//
// The provided context is used for graceful shutdown. Call Stop() to clean up.
//
// Returns an error if:
//   - No transports are available and mode is ModeForceUSB or ModeForceSPI
//   - Config validation fails
func (tm *TransportManager) Start(ctx context.Context) error {
	// Validate configuration
	if err := tm.config.Validate(); err != nil {
		return fmt.Errorf("transport manager config invalid: %w", err)
	}

	tm.mu.Lock()

	// Create cancellable context for background routines
	tm.ctx, tm.cancel = context.WithCancel(ctx)

	// Start health monitor
	tm.wg.Add(1)
	go func() {
		defer tm.wg.Done()
		tm.healthMonitor.Run(tm.ctx, tm)
	}()

	// Start hot-plug detector
	if tm.hotPlugDetector != nil {
		tm.wg.Add(1)
		go func() {
			defer tm.wg.Done()
			tm.hotPlugDetector.Run(tm.ctx, tm.handleHotPlugEvent)
		}()
	}

	// Start heartbeat manager (hybrid implicit/explicit detection)
	tm.wg.Add(1)
	go func() {
		defer tm.wg.Done()
		tm.heartbeat.Run(tm.ctx, tm)
	}()

	// Select initial transport
	best := tm.selectBestTransportLocked()
	if best != nil {
		tm.activeTransport = best.Transport
		tm.activeTransportName = best.Name
		targetState := tm.getActiveStateLocked(best.Name)
		transportName := best.Name
		transportPriority := best.Priority

		// CRITICAL: Release mutex before performResetHandshake to prevent deadlock
		// performResetHandshake calls Send/Receive which may acquire locks
		tm.mu.Unlock()

		// CRITICAL FIX #4: Send Reset handshake before entering Active state
		// This synchronizes sequence numbers with RX72N after Gateway restart
		if err := tm.performResetHandshake(tm.ctx); err != nil {
			log.Printf("WARNING: Reset handshake failed: %v (continuing anyway)", err)
			// Don't fail startup - RX72N might be rebooting
		}

		// Reacquire mutex to update state
		tm.mu.Lock()
		tm.state = targetState
		log.Printf("TransportManager started with %s transport (priority %d)", transportName, transportPriority)
		tm.mu.Unlock() // Release before return
	} else {
		tm.state = StateFailed
		// Check if this is acceptable based on mode
		if tm.config.Mode == ModeForceUSB || tm.config.Mode == ModeForceSPI {
			tm.mu.Unlock() // Release before error return
			return fmt.Errorf("no %s transport available (required by mode)", tm.config.Mode)
		}
		log.Printf("TransportManager started with no available transports (will retry)")
		tm.mu.Unlock() // Release before return
	}

	return nil
}

// performResetHandshake sends a RESET frame to RX72N and waits for RESET_ACK.
// CRITICAL FIX #4: Prevents restart deadlock when Gateway restarts but RX72N stays running.
//
// This implements a three-way handshake with a "reset barrier" to handle stale
// frames buffered in USB FIFOs:
//  1. Activate barrier (reject all non-RESET_ACK frames)
//  2. Send RESET with session ID payload
//  3. Drain stale frames until RESET_ACK with matching session ID arrives
//  4. Reset sequence counters only after valid RESET_ACK
//  5. Deactivate barrier
//
// The session ID (4-byte big-endian uint32) in the RESET payload is echoed back
// in the RESET_ACK, allowing the gateway to match ACKs to the correct reset request
// and reject stale RESET_ACKs from previous resets.
//
// Retries up to 3 times with backoff. Returns error if all retries fail.
// Non-fatal - caller should log warning and continue (RX72N might be rebooting).
func (tm *TransportManager) performResetHandshake(ctx context.Context) error {
	const (
		maxRetries = 3
		retryDelay = 100 * time.Millisecond
		ackTimeout = 500 * time.Millisecond
	)

	// Idempotency: if barrier is already active, another reset is in progress
	if tm.sessionState.IsBarrierActive() {
		return fmt.Errorf("reset already in progress (barrier active)")
	}

	// Type-assert to get SendWithType (fixes bug: was using Send which sends as FrameTypeCommand)
	type frameTypeSender interface {
		SendWithType(ctx context.Context, data []byte, frameType frame.Type) error
	}
	sender, ok := tm.activeTransport.(frameTypeSender)
	if !ok {
		return fmt.Errorf("active transport does not support SendWithType")
	}

	// Increment session ID and encode as 4-byte big-endian payload
	newSessionID := tm.sessionState.IncrementSessionID()
	resetPayload := make([]byte, SessionIDPayloadSize)
	binary.BigEndian.PutUint32(resetPayload, newSessionID)

	// Activate barrier BEFORE sending RESET to ensure stale frames are rejected
	tm.sessionState.ActivateBarrier(newSessionID)
	defer tm.sessionState.DeactivateBarrier()

	var lastErr error
	for attempt := 1; attempt <= maxRetries; attempt++ {
		log.Printf("Reset handshake attempt %d/%d (session=%d)...",
			attempt, maxRetries, newSessionID)

		// Send RESET with FrameTypeReset
		if err := sender.SendWithType(ctx, resetPayload, frame.FrameTypeReset); err != nil {
			lastErr = fmt.Errorf("send RESET failed: %w", err)
			log.Printf("Reset send failed (attempt %d): %v", attempt, err)
			time.Sleep(retryDelay)
			continue
		}

		// Drain stale frames until RESET_ACK with matching session ID
		ctxTimeout, cancel := context.WithTimeout(ctx, ackTimeout)
		err := tm.drainUntilResetAck(ctxTimeout, newSessionID)
		cancel()

		if err == nil {
			// Reset sequences AFTER valid RESET_ACK (not before)
			tm.sessionState.Reset()
			log.Printf("Reset handshake successful (session=%d, attempt=%d)",
				newSessionID, attempt)
			return nil
		}

		lastErr = err
		log.Printf("Reset handshake attempt %d failed: %v", attempt, err)
		time.Sleep(retryDelay)
	}

	return fmt.Errorf("reset handshake failed after %d attempts: %w", maxRetries, lastErr)
}

// drainUntilResetAck receives frames in a loop, discarding non-RESET_ACK frames
// (stale frames from USB FIFO buffers) until a RESET_ACK with a matching session ID
// arrives or the context times out.
func (tm *TransportManager) drainUntilResetAck(ctx context.Context, expectedSessionID uint32) error {
	for {
		if ctx.Err() != nil {
			return fmt.Errorf("timeout waiting for RESET_ACK: %w", ctx.Err())
		}

		result, err := tm.activeTransport.Receive(ctx)
		if err != nil {
			if ctx.Err() != nil {
				return fmt.Errorf("timeout waiting for RESET_ACK: %w", ctx.Err())
			}
			// Transient receive error, keep trying within timeout
			log.Printf("DEBUG: Reset handshake receive error (continuing): %v", err)
			continue
		}

		if result == nil {
			continue
		}

		// Non-RESET_ACK frames are stale -- discard and continue draining
		if result.Metadata.Type != frame.FrameTypeResetAck {
			log.Printf("DEBUG: Discarding stale frame during reset (type=%s, seq=%d)",
				result.Metadata.Type.String(), result.Metadata.Sequence)
			continue
		}

		// Got RESET_ACK -- validate session ID
		if !tm.sessionState.ValidateResetAck(result.Payload) {
			log.Printf("WARN: RESET_ACK with invalid/mismatched session ID, discarding")
			continue
		}

		// Valid RESET_ACK with matching session ID
		log.Printf("Received valid RESET_ACK (session=%d, seq=%d)",
			expectedSessionID, result.Metadata.Sequence)
		return nil
	}
}

// Stop gracefully shuts down the TransportManager.
//
// This method:
//  1. Cancels all background routines
//  2. Waits for all goroutines to exit
//  3. Resets all registered transports
//
// It is safe to call Stop multiple times.
func (tm *TransportManager) Stop() error {
	// Cancel background routines
	if tm.cancel != nil {
		tm.cancel()
	}

	// Wait for all goroutines
	tm.wg.Wait()

	tm.mu.Lock()
	defer tm.mu.Unlock()

	// Reset all transports
	for name, wrapper := range tm.availableTransports {
		if wrapper.Transport != nil {
			wrapper.Transport.Reset()
			log.Printf("Reset transport: %s", name)
		}
	}

	log.Printf("TransportManager stopped")
	return nil
}

// RegisterTransport adds a transport to the manager with the given priority.
//
// Higher priority transports are preferred (e.g., USB=10, SPI=5).
// If this transport has higher priority than the current active transport,
// an automatic switch will be attempted in the background.
//
// This method is thread-safe and can be called while the manager is running.
//
// Parameters:
//   - name: Transport identifier (e.g., "spi", "usb")
//   - transport: HARQ implementation
//   - priority: Selection priority (higher = preferred)
func (tm *TransportManager) RegisterTransport(name string, transport harq.HARQ, priority int) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	wrapper := &TransportWrapper{
		Name:      name,
		Transport: transport,
		Decoder:   frame.NewStreamDecoder(NewHARQReader(transport)),
		Health:    NewHealthMetrics(),
		Priority:  priority,
		Available: true,
	}

	tm.availableTransports[name] = wrapper
	log.Printf("Registered transport: %s (priority %d)", name, priority)

	// If this is higher priority than current, attempt switch
	if tm.activeTransport == nil {
		// No active transport, select this one immediately
		tm.activeTransport = wrapper.Transport
		tm.activeTransportName = wrapper.Name
		tm.state = tm.getActiveStateLocked(wrapper.Name)
		log.Printf("Activated initial transport: %s", name)
	} else if currentWrapper, exists := tm.availableTransports[tm.activeTransportName]; exists && priority > currentWrapper.Priority {
		// Higher priority transport available, switch in background
		go tm.attemptSwitch(wrapper)
	}
}

// Send proxies the Send operation to the active transport with health tracking.
//
// This method implements the harq.HARQ interface.
// It blocks if a transport switch is in progress, then resumes once switching completes.
//
// Returns an error if:
//   - No active transport is available (StateFailed)
//   - The active transport's Send operation fails
//
// Failed operations increment the failure counter. When the failure threshold is reached,
// an automatic failover is triggered in the background.
func (tm *TransportManager) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	if err := tm.validateFramePayload(frame.FrameTypeCommand, data); err != nil {
		return err
	}

	// Wait if switching is in progress
	tm.operationsMu.Lock()
	for tm.paused {
		tm.operationsCond.Wait()
	}
	tm.inflightCounter++
	tm.operationsMu.Unlock()

	// Decrement in-flight counter on exit
	defer func() {
		tm.operationsMu.Lock()
		tm.inflightCounter--
		tm.operationsMu.Unlock()
	}()

	// Get active transport (read lock only)
	tm.mu.RLock()
	active := tm.activeTransport
	activeName := tm.activeTransportName
	tm.mu.RUnlock()

	if active == nil {
		return errors.New("no active transport available")
	}

	// Execute send with latency tracking
	start := time.Now()
	err := active.Send(ctx, data, p...)
	latency := time.Since(start)

	// Record operation result
	tm.recordOperation(activeName, err, latency, true)

	return err
}

// SendWithType sends data with an explicit frame type (PING, PONG, RESET, etc.).
// This is a Phase 3 API for protocol messages that require specific frame types.
//
// If the active transport supports SendWithType(), it uses that method.
// Otherwise, it falls back to Send() (CDCLink and SPILink both support SendWithType).
//
// This method blocks if a transport switch is in progress, then resumes once switching completes.
func (tm *TransportManager) SendWithType(ctx context.Context, data []byte, frameType frame.Type) error {
	if err := tm.validateFramePayload(frameType, data); err != nil {
		return err
	}

	// Wait if switching is in progress
	tm.operationsMu.Lock()
	for tm.paused {
		tm.operationsCond.Wait()
	}
	tm.inflightCounter++
	tm.operationsMu.Unlock()

	// Decrement in-flight counter on exit
	defer func() {
		tm.operationsMu.Lock()
		tm.inflightCounter--
		tm.operationsMu.Unlock()
	}()

	// Get active transport (read lock only)
	tm.mu.RLock()
	active := tm.activeTransport
	activeName := tm.activeTransportName
	tm.mu.RUnlock()

	if active == nil {
		return errors.New("no active transport available")
	}

	// Type-assert to check if transport supports SendWithType
	type frameTypeSender interface {
		SendWithType(ctx context.Context, data []byte, frameType frame.Type) error
	}

	sender, ok := active.(frameTypeSender)
	if !ok {
		// Fallback: use Send() (shouldn't happen with CDCLink/SPILink)
		log.Printf("WARNING: Active transport %s does not support SendWithType, falling back to Send()", activeName)
		return tm.Send(ctx, data)
	}

	// Execute send with latency tracking
	start := time.Now()
	err := sender.SendWithType(ctx, data, frameType)
	latency := time.Since(start)

	// Record operation result
	tm.recordOperation(activeName, err, latency, true)

	return err
}

func (tm *TransportManager) validateFramePayload(frameType frame.Type, payload []byte) error {
	if !isAllowedFrameType(frameType) {
		return fmt.Errorf("invalid frame type: %v", frameType)
	}

	const minFramePayloadSize = 0
	const maxFramePayloadSize = frame.MaxPayloadSize

	if len(payload) < minFramePayloadSize {
		return fmt.Errorf("payload size %d is below minimum %d", len(payload), minFramePayloadSize)
	}
	if len(payload) > maxFramePayloadSize {
		return fmt.Errorf("payload size %d exceeds maximum %d", len(payload), maxFramePayloadSize)
	}

	return nil
}

func isAllowedFrameType(frameType frame.Type) bool {
	switch frameType {
	case frame.FrameTypePing,
		frame.FrameTypePong,
		frame.FrameTypeReset,
		frame.FrameTypeResetAck,
		frame.FrameTypeCommand,
		frame.FrameTypeResponse,
		frame.FrameTypeAck,
		frame.FrameTypeNack:
		return true
	default:
		return false
	}
}

// Receive proxies the Receive operation to the active transport with health tracking.
//
// This method implements the harq.HARQ interface.
// It blocks if a transport switch is in progress, then resumes once switching completes.
//
// Returns (*harq.ReceiveResult, error) from the active transport.
func (tm *TransportManager) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	// Wait if switching is in progress
	tm.operationsMu.Lock()
	for tm.paused {
		tm.operationsCond.Wait()
	}
	tm.inflightCounter++
	tm.operationsMu.Unlock()

	defer func() {
		tm.operationsMu.Lock()
		tm.inflightCounter--
		tm.operationsMu.Unlock()
	}()

	tm.mu.RLock()
	// Get wrapper to access Decoder
	activeWrapper := tm.availableTransports[tm.activeTransportName]
	activeName := tm.activeTransportName
	tm.mu.RUnlock()

	if activeWrapper == nil {
		return nil, errors.New("no active transport available")
	}

	// FIX: Check if Transport interface is nil to prevent panic
	if activeWrapper.Transport == nil {
		err := errors.New("active transport interface is nil")
		tm.recordOperation(activeName, err, 0, false)
		return nil, err
	}

	// Validate decoder
	if activeWrapper.Decoder == nil {
		err := errors.New("transport decoder not initialized")
		tm.recordOperation(activeName, err, 0, false)
		return nil, err
	}

	start := time.Now()

	// Use direct transport receive instead of StreamDecoder (double framing issue)
	result, err := activeWrapper.Transport.Receive(ctx)

	latency := time.Since(start)
	tm.recordOperation(activeName, err, latency, false)

	if err != nil {
		return nil, err
	}

	// FIX: Handle nil result (e.g. control frames handled internally by transport)
	if result == nil {
		return nil, nil
	}

	// Reset barrier check: reject stale frames during active reset handshake.
	// This prevents stale USB FIFO frames from reaching the dispatcher.
	// Returns (nil, nil) which the dispatcher treats as a transient timeout.
	if tm.sessionState.IsBarrierActive() {
		log.Printf("DEBUG: Barrier active, discarding frame (type=%s, seq=%d)",
			result.Metadata.Type.String(), result.Metadata.Sequence)
		return nil, nil
	}

	// Update heartbeat based on frame type
	// - PONG/PING: Explicit heartbeat frames (always update)
	// - COMMAND/RESPONSE: Application data (update to prevent unnecessary PINGs)
	// - ACK/NACK: Control frames (ignore - not meaningful for heartbeat)
	switch result.Metadata.Type {
	case frame.FrameTypePong, frame.FrameTypePing,
		frame.FrameTypeCommand, frame.FrameTypeResponse:
		tm.heartbeat.OnFrameReceived()
	case frame.FrameTypeAck, frame.FrameTypeNack:
		// Internal control frames - don't reset heartbeat timer
		log.Printf("Received control frame %s (seq=%d) - ignored for heartbeat",
			result.Metadata.Type.String(), result.Metadata.Sequence)
	}

	return result, nil
}

// GetState returns the current harq.State mapped from the internal State.
//
// Mapping:
//   - StateActiveUSB, StateActiveSPI → harq.StateIdle
//   - StateSwitching* → harq.StateRetransmitting
//   - StateFailed → harq.StateError
func (tm *TransportManager) GetState() harq.State {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	switch tm.state {
	case StateActiveUSB, StateActiveSPI:
		return harq.StateIdle
	case StateSwitchingToUSB, StateSwitchingToSPI:
		return harq.StateRetransmitting // Closest semantic match
	case StateFailed, StateDegraded:
		return harq.StateError
	default:
		return harq.StateIdle
	}
}

// GetTxSequence returns the TX sequence number from the active transport.
// Returns 0 if no transport is active.
func (tm *TransportManager) GetTxSequence() uint16 {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	if tm.activeTransport == nil {
		return 0
	}
	return tm.activeTransport.GetTxSequence()
}

// GetRxSequence returns the RX sequence number from the active transport.
// Returns 0 if no transport is active.
func (tm *TransportManager) GetRxSequence() uint16 {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	if tm.activeTransport == nil {
		return 0
	}
	return tm.activeTransport.GetRxSequence()
}

// Reset resets the active transport to a clean state.
// This is a pass-through to the active transport's Reset method.
func (tm *TransportManager) Reset() {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if tm.activeTransport != nil {
		tm.activeTransport.Reset()
		log.Printf("Reset active transport: %s", tm.activeTransportName)
	}
}

// GetActiveTransport returns the name of the currently active transport.
// Returns empty string if no transport is active.
func (tm *TransportManager) GetActiveTransport() string {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	return tm.activeTransportName
}

// GetAvailableTransports returns a list of currently available transport names.
// Only includes transports marked as Available.
func (tm *TransportManager) GetAvailableTransports() []string {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	names := make([]string, 0, len(tm.availableTransports))
	for name, wrapper := range tm.availableTransports {
		if wrapper.Available {
			names = append(names, name)
		}
	}
	return names
}

// ForceSwitch manually switches to a specific transport, bypassing priority logic.
//
// This method is useful for testing or manual override scenarios.
//
// Returns an error if:
//   - The specified transport is not registered
//   - The specified transport is not available
//   - The switch operation fails
func (tm *TransportManager) ForceSwitch(transportName string) error {
	tm.mu.Lock()
	wrapper, exists := tm.availableTransports[transportName]

	if !exists {
		tm.mu.Unlock()
		return fmt.Errorf("transport %q not registered", transportName)
	}

	if !wrapper.Available {
		tm.mu.Unlock()
		return fmt.Errorf("transport %q not available", transportName)
	}
	tm.mu.Unlock()

	log.Printf("Force switching to transport: %s", transportName)
	return tm.executeSwitch(wrapper, FailureTypeGraceful)
}

// GetHealthMetrics returns a copy of health metrics for all registered transports.
func (tm *TransportManager) GetHealthMetrics() map[string]*HealthMetrics {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	metrics := make(map[string]*HealthMetrics, len(tm.availableTransports))
	for name, wrapper := range tm.availableTransports {
		// Create a copy to prevent external mutation
		metricsCopy := *wrapper.Health
		metrics[name] = &metricsCopy
	}
	return metrics
}

// GetInternalState returns the current internal State (for debugging/testing).
func (tm *TransportManager) GetInternalState() State {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	return tm.state
}

// GetSessionState returns the shared SessionState instance.
// CRITICAL FIX #1: All transports (USB CDC and SPI) MUST use the same SessionState
// to prevent sequence desynchronization during transport switching (Handoff Problem).
//
// Usage in gateway.go:
//
//	tm := manager.NewTransportManager(config)
//	session := tm.GetSessionState()
//	cdcLink, _ := link.NewCDCLink(cdcTransport, session)
//	spiLink, _ := link.NewSPILink(spiTransport, session)
//	tm.RegisterTransport("usb", cdcLink, manager.PriorityUSB)
//	tm.RegisterTransport("spi", spiLink, manager.PrioritySPI)
func (tm *TransportManager) GetSessionState() *SessionState {
	return tm.sessionState
}

// =============================================================================
// Private Methods
// =============================================================================

// selectBestTransportLocked selects the best available transport based on priority.
// Caller must hold tm.mu lock.
func (tm *TransportManager) selectBestTransportLocked() *TransportWrapper {
	// Filter for available and healthy transports
	candidates := make([]*TransportWrapper, 0, len(tm.availableTransports))
	for _, wrapper := range tm.availableTransports {
		if wrapper.Available && wrapper.Health.IsHealthy {
			candidates = append(candidates, wrapper)
		}
	}

	if len(candidates) == 0 {
		return nil
	}

	// Sort by priority (descending)
	sort.Slice(candidates, func(i, j int) bool {
		return candidates[i].Priority > candidates[j].Priority
	})

	// Apply mode-specific filtering
	switch tm.config.Mode {
	case ModeForceUSB:
		for _, t := range candidates {
			if t.Name == TransportNameUSB {
				return t
			}
		}
		return nil // USB not available

	case ModeForceSPI:
		for _, t := range candidates {
			if t.Name == TransportNameSPI {
				return t
			}
		}
		return nil // SPI not available

	default: // ModeAuto, ModePreferUSB
		return candidates[0] // Highest priority
	}
}

// executeSwitch performs a transport switch with pause-drain-resume.
// executeSwitch performs the actual transport switch with smart drain logic.
// CRITICAL FIX #2: Skips drain for hard failures (USB disconnect, IO errors) to enable fast failover (<300ms).
func (tm *TransportManager) executeSwitch(target *TransportWrapper, failureType FailureType) error {
	if target == nil {
		return errors.New("target transport is nil")
	}

	tm.mu.Lock()
	oldName := tm.activeTransportName
	oldTransport := tm.activeTransport

	// Update state to switching
	tm.state = tm.getSwitchingStateLocked(target.Name)
	tm.mu.Unlock()

	log.Printf("Starting transport switch: %s → %s (failure type: %v)", oldName, target.Name, failureType)

	// Step 1: Pause new operations
	tm.pauseOperations()
	defer tm.resumeOperations() // Ensure resume even on error

	// Step 2: SMART DRAIN - skip if hard failure
	// CRITICAL FIX #2: Only drain for graceful switches and heartbeat timeouts.
	// Hard failures (USB disconnect, IO errors) skip drain for fast failover (<300ms).
	shouldDrain := (failureType == FailureTypeGraceful || failureType == FailureTypeTimeout)

	if shouldDrain {
		if err := tm.drainInflight(tm.config.SwitchTimeout); err != nil {
			log.Printf("WARNING: Drain timeout during switch, continuing anyway: %v", err)
			// Continue with switch despite timeout (better than staying on failed transport)
		}
	} else {
		log.Printf("Skipping drain for failure type: %v (hard failure, fast failover)", failureType)
	}

	// Step 3: Reset old transport
	if oldTransport != nil {
		oldTransport.Reset()
	}

	// Step 4: Activate new transport
	tm.mu.Lock()
	tm.activeTransport = target.Transport
	tm.activeTransportName = target.Name
	tm.state = tm.getActiveStateLocked(target.Name)
	tm.mu.Unlock()

	log.Printf("Transport switch completed: %s → %s", oldName, target.Name)
	return nil
}

// pauseOperations blocks new Send/Receive calls.
func (tm *TransportManager) pauseOperations() {
	tm.operationsMu.Lock()
	tm.paused = true
	tm.operationsMu.Unlock()
}

// resumeOperations allows new Send/Receive calls and wakes waiting goroutines.
func (tm *TransportManager) resumeOperations() {
	tm.operationsMu.Lock()
	tm.paused = false
	tm.operationsCond.Broadcast() // Wake all waiting goroutines
	tm.operationsMu.Unlock()
}

// drainInflight waits for all in-flight operations to complete or timeout.
func (tm *TransportManager) drainInflight(timeout time.Duration) error {
	deadline := time.Now().Add(timeout)

	for {
		tm.operationsMu.Lock()
		count := tm.inflightCounter
		tm.operationsMu.Unlock()

		if count == 0 {
			return nil // All drained
		}

		if time.Now().After(deadline) {
			return fmt.Errorf("drain timeout: %d operations still in-flight", count)
		}

		time.Sleep(inflightPollInterval)
	}
}

// recordOperation updates health metrics based on operation result.
func (tm *TransportManager) recordOperation(name string, err error, latency time.Duration, isSend bool) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	wrapper := tm.availableTransports[name]
	if wrapper == nil {
		return
	}

	if err != nil {
		// Record failure
		wrapper.Health.LastFailure = time.Now()
		wrapper.Health.ConsecutiveFailures++
		wrapper.Health.TotalErrors++

		// Check threshold
		if wrapper.Health.ConsecutiveFailures >= tm.config.FailureThreshold {
			wrapper.Health.IsHealthy = false
			wrapper.Available = false
			log.Printf("Transport %s marked unhealthy after %d failures", name, wrapper.Health.ConsecutiveFailures)

			// Trigger failover in background (consecutive IO errors)
			go tm.attemptFailover(FailureTypeIOError)
		}
	} else {
		// Record success
		wrapper.Health.LastSuccess = time.Now()
		wrapper.Health.ConsecutiveFailures = 0
		wrapper.Health.IsHealthy = true

		if isSend {
			wrapper.Health.TotalSent++
		} else {
			wrapper.Health.TotalReceived++
		}

		// Update moving average latency
		if wrapper.Health.AvgLatency == 0 {
			wrapper.Health.AvgLatency = latency
		} else {
			wrapper.Health.AvgLatency = (wrapper.Health.AvgLatency + latency) / 2
		}
	}
}

// attemptFailover tries to switch to the next best transport.
// The failureType determines whether to drain in-flight operations before switching.
func (tm *TransportManager) attemptFailover(failureType FailureType) {
	tm.mu.Lock()
	currentName := tm.activeTransportName
	nextBest := tm.selectBestTransportLocked()
	tm.mu.Unlock()

	if nextBest == nil {
		log.Printf("Failover failed: no healthy transports available")
		tm.mu.Lock()
		tm.state = StateFailed
		tm.mu.Unlock()
		return
	}

	if nextBest.Name == currentName {
		log.Printf("Failover: current transport %s is still best option (degraded mode)", currentName)
		tm.mu.Lock()
		tm.state = StateDegraded
		tm.mu.Unlock()
		return
	}

	log.Printf("Failover triggered: %s → %s (failure type: %v)", currentName, nextBest.Name, failureType)
	if err := tm.executeSwitch(nextBest, failureType); err != nil {
		log.Printf("Failover switch failed: %v", err)
		tm.mu.Lock()
		tm.state = StateDegraded
		tm.mu.Unlock()
	}
}

// attemptSwitch tries to switch to a specific transport (used for priority upgrades).
// Priority upgrades are graceful switches (drain in-flight operations).
func (tm *TransportManager) attemptSwitch(target *TransportWrapper) {
	if err := tm.executeSwitch(target, FailureTypeGraceful); err != nil {
		log.Printf("Priority-based switch failed: %v", err)
	}
}

// handleHotPlugEvent processes USB hot-plug add/remove events.
func (tm *TransportManager) handleHotPlugEvent(event HotPlugEvent) {
	log.Printf("Hot-plug event: %s %s (VID=0x%04X PID=0x%04X)", event.Action, event.Device, event.VendorID, event.ProductID)

	switch event.Action {
	case "add":
		// USB device added - will be registered by the main initialization code
		// This event is primarily informational; actual registration happens via RegisterTransport
		log.Printf("USB device connected: %s", event.Device)

	case "remove":
		tm.mu.Lock()
		// Check if this was the active transport
		if tm.activeTransportName == TransportNameUSB {
			log.Printf("Active USB transport disconnected, triggering failover")
			tm.mu.Unlock()
			go tm.attemptFailover(FailureTypeHotPlugRemove)
		} else {
			// Mark USB as unavailable
			if wrapper, exists := tm.availableTransports[TransportNameUSB]; exists {
				wrapper.Available = false
				wrapper.Health.IsHealthy = false
			}
			tm.mu.Unlock()
		}
	}
}

// getActiveStateLocked returns the appropriate active state for a transport name.
// Caller must hold tm.mu lock.
func (tm *TransportManager) getActiveStateLocked(name string) State {
	switch name {
	case TransportNameUSB:
		return StateActiveUSB
	case TransportNameSPI:
		return StateActiveSPI
	default:
		return StateInitializing
	}
}

// getSwitchingStateLocked returns the appropriate switching state for a target transport.
// Caller must hold tm.mu lock.
func (tm *TransportManager) getSwitchingStateLocked(targetName string) State {
	switch targetName {
	case TransportNameUSB:
		return StateSwitchingToUSB
	case TransportNameSPI:
		return StateSwitchingToSPI
	default:
		return StateInitializing
	}
}
