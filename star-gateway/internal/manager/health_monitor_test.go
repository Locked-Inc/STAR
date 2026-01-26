package manager

import (
	"context"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
)

// MockHARQ for testing
type MockHARQ struct{}

func (m *MockHARQ) Send(ctx context.Context, data []byte, p ...harq.Priority) error { return nil }
func (m *MockHARQ) Receive(ctx context.Context) (*harq.ReceiveResult, error)        { return nil, nil }
func (m *MockHARQ) GetState() harq.State                                            { return harq.StateIdle }
func (m *MockHARQ) GetTxSequence() uint16                                           { return 0 }
func (m *MockHARQ) GetRxSequence() uint16                                           { return 0 }
func (m *MockHARQ) Reset()                                                          {}

func TestHealthMonitor_Recovery(t *testing.T) {
	// Setup TransportManager
	tm := NewTransportManager(DefaultConfig())
	mockTransport := &MockHARQ{}

	// Register a transport (it will be active)
	tm.RegisterTransport("test-transport", mockTransport, 10)

	// Create another transport that is inactive
	mockTransport2 := &MockHARQ{}
	tm.RegisterTransport("inactive-transport", mockTransport2, 5)

	// Verify initial state
	tm.mu.RLock()
	wrapper := tm.availableTransports["inactive-transport"]
	if !wrapper.Health.IsHealthy {
		t.Error("Transport should initially be healthy")
	}
	tm.mu.RUnlock()

	// Manually mark it as unhealthy
	tm.mu.Lock()
	wrapper.Health.IsHealthy = false
	wrapper.Available = false
	tm.mu.Unlock()

	// Create HealthMonitor
	hm := NewHealthMonitor(100 * time.Millisecond)

	// Run checkTransports directly (no need to wait for ticker in Run)
	// This function probes inactive transports. Since probeTransport returns true (stub),
	// it should recover the transport.
	hm.checkTransports(tm)

	// Verify recovery
	tm.mu.RLock()
	wrapper = tm.availableTransports["inactive-transport"]
	if !wrapper.Health.IsHealthy {
		t.Error("Transport should have recovered to healthy")
	}
	if !wrapper.Available {
		t.Error("Transport should be available after recovery")
	}
	tm.mu.RUnlock()
}
