// Package manager provides intelligent transport selection and failover for the STAR Gateway.
//
// STAR Project - Texas A&M University
// January 2026
package manager

import (
	"context"
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
	}

	tm.operationsCond = sync.NewCond(&tm.operationsMu)

	// Health monitor is always created
	tm.healthMonitor = NewHealthMonitor(config.HealthCheckInterval)

	// Hot-plug detector only if enabled and not in force-spi mode
	if config.EnableHotPlug && config.Mode != ModeForceSPI {
		tm.hotPlugDetector = NewHotPlugDetector(config.HotPlugPollInterval, config.USBVID, config.USBPID)
	}

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
	defer tm.mu.Unlock()

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

	// Select initial transport
	best := tm.selectBestTransportLocked()
	if best != nil {
		tm.activeTransport = best.Transport
		tm.activeTransportName = best.Name
		tm.state = tm.getActiveStateLocked(best.Name)
		log.Printf("TransportManager started with %s transport (priority %d)", best.Name, best.Priority)
	} else {
		tm.state = StateFailed
		// Check if this is acceptable based on mode
		if tm.config.Mode == ModeForceUSB || tm.config.Mode == ModeForceSPI {
			return fmt.Errorf("no %s transport available (required by mode)", tm.config.Mode)
		}
		log.Printf("TransportManager started with no available transports (will retry)")
	}

	return nil
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

	// Validate decoder
	if activeWrapper.Decoder == nil {
		err := errors.New("transport decoder not initialized")
		tm.recordOperation(activeName, err, 0, false)
		return nil, err
	}

	start := time.Now()

	// Use StreamDecoder instead of direct transport.Receive()
	decodedFrame, err := activeWrapper.Decoder.Decode()

	latency := time.Since(start)
	tm.recordOperation(activeName, err, latency, false)

	if err != nil {
		return nil, err
	}

	// Convert Frame to ReceiveResult
	result := &harq.ReceiveResult{
		Payload: decodedFrame.Payload,
		Metadata: harq.FrameMetadata{
			Sequence:   decodedFrame.Header.Sequence,
			ReceivedAt: time.Now(),
			// Propagate metadata from Frame fields
			Retransmits: decodedFrame.Retransmits,
			FECDecoded:  decodedFrame.FECDecoded,
		},
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
	return tm.executeSwitch(wrapper)
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
func (tm *TransportManager) executeSwitch(target *TransportWrapper) error {
	if target == nil {
		return errors.New("target transport is nil")
	}

	tm.mu.Lock()
	oldName := tm.activeTransportName
	oldTransport := tm.activeTransport

	// Update state to switching
	tm.state = tm.getSwitchingStateLocked(target.Name)
	tm.mu.Unlock()

	log.Printf("Starting transport switch: %s → %s", oldName, target.Name)

	// Step 1: Pause new operations
	tm.pauseOperations()
	defer tm.resumeOperations() // Ensure resume even on error

	// Step 2: Drain in-flight operations
	if err := tm.drainInflight(tm.config.SwitchTimeout); err != nil {
		log.Printf("WARNING: Drain timeout during switch, continuing anyway: %v", err)
		// Continue with switch despite timeout (better than staying on failed transport)
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

			// Trigger failover in background
			go tm.attemptFailover()
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
func (tm *TransportManager) attemptFailover() {
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

	log.Printf("Failover triggered: %s → %s", currentName, nextBest.Name)
	if err := tm.executeSwitch(nextBest); err != nil {
		log.Printf("Failover switch failed: %v", err)
		tm.mu.Lock()
		tm.state = StateDegraded
		tm.mu.Unlock()
	}
}

// attemptSwitch tries to switch to a specific transport (used for priority upgrades).
func (tm *TransportManager) attemptSwitch(target *TransportWrapper) {
	if err := tm.executeSwitch(target); err != nil {
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
			go tm.attemptFailover()
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
