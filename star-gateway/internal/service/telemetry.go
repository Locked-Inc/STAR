// Package service implements the gRPC service handlers for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package service

import (
	"context"
	"log/slog"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

const (
	// MinRateHz is the minimum allowed streaming rate in Hz.
	MinRateHz = 1
	// MaxRateHz is the maximum allowed streaming rate in Hz.
	MaxRateHz = 100
	// DefaultRateHz is the default streaming rate when invalid rate is provided.
	DefaultRateHz = 10
)

// telemetryHolder provides thread-safe access to telemetry data.
type telemetryHolder struct {
	mu   sync.RWMutex
	data *starv1.TelemetryData
}

// Load returns the current telemetry data.
func (h *telemetryHolder) Load() *starv1.TelemetryData {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return h.data
}

// Store updates the telemetry data.
func (h *telemetryHolder) Store(data *starv1.TelemetryData) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.data = data
}

// TelemetryService handles real-time sensor data aggregation and caching.
// This service caches telemetry data from the RX72N firmware (transmitted at 20Hz unsolicited).
//
// Architecture:
// - Uses Dispatcher for centralized message routing
// - Uses telemetryHolder for thread-safe caching of latest telemetry
// - Background goroutine updates cached telemetry from dispatcher
//
// Note: gRPC TelemetryService was removed - firmware operates in push mode only.
// Telemetry data is transmitted unsolicited at 20Hz via RESPONSE frames.
type TelemetryService struct {
	// Removed: starv1.UnimplementedTelemetryServiceServer (service deleted from proto)
	harqHandler     harq.HARQ // Reserved for future use
	dispatcher      dispatcher.Dispatcher
	logger          *slog.Logger
	latestTelemetry *telemetryHolder
	ctx             context.Context
	cancel          context.CancelFunc
	wg              sync.WaitGroup
	started         chan struct{} // Signal when background goroutine initialized
}

// NewTelemetryService creates a new TelemetryService.
// Requires a context for lifecycle management, HARQ handler for sending requests,
// Dispatcher for receiving telemetry, and logger.
func NewTelemetryService(ctx context.Context, h harq.HARQ, d dispatcher.Dispatcher, logger *slog.Logger) *TelemetryService {
	ctx, cancel := context.WithCancel(ctx)
	svc := &TelemetryService{
		harqHandler:     h,
		dispatcher:      d,
		logger:          logger,
		latestTelemetry: &telemetryHolder{},
		ctx:             ctx,
		cancel:          cancel,
		started:         make(chan struct{}),
	}

	// Start background goroutine to update cached telemetry
	svc.wg.Add(1)
	go svc.updateTelemetryCache()

	// Wait for background goroutine to initialize (with timeout)
	select {
	case <-svc.started:
		// Goroutine initialized successfully
	case <-time.After(5 * time.Second):
		panic("telemetry service background goroutine failed to start within 5 seconds")
	}

	return svc
}

// updateTelemetryCache continuously receives telemetry from Dispatcher and updates cache.
// This goroutine runs until the service context is cancelled.
func (s *TelemetryService) updateTelemetryCache() {
	defer s.wg.Done()
	telemetryCh := s.dispatcher.Subscribe(dispatcher.MessageTypeTelemetryData)
	defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeTelemetryData, telemetryCh)

	// Signal that initialization is complete
	close(s.started)

	for {
		// Prioritize context cancellation by checking it first (non-blocking)
		select {
		case <-s.ctx.Done():
			return
		default:
		}

		select {
		case <-s.ctx.Done():
			return
		case dispMsg, ok := <-telemetryCh:
			if !ok {
				return
			}
			telemetry := dispMsg.WireMsg.GetTelemetryData()
			if telemetry == nil {
				s.logger.Warn("received wire message with nil telemetry data in cache update")
				continue
			}

			// Populate frame sequence from metadata
			if dispMsg.Metadata != nil {
				telemetry.FrameSequence = uint32(dispMsg.Metadata.Sequence)
			}

			s.latestTelemetry.Store(telemetry)
		}
	}
}

// Shutdown gracefully stops the TelemetryService background goroutines.
func (s *TelemetryService) Shutdown() {
	s.cancel()
	s.wg.Wait()
}
