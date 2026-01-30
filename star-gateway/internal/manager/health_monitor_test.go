package manager

import (
	"context"
	"encoding/binary"
	"errors"
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

// MockTransportProbe allows control over Send/Receive behavior for probe testing
type MockTransportProbe struct {
	sendErr     error
	receiveErr  error
	receiveData []byte
}

func (m *MockTransportProbe) Send(ctx context.Context, data []byte, p ...harq.Priority) error {
	return m.sendErr
}

func (m *MockTransportProbe) Receive(ctx context.Context) (*harq.ReceiveResult, error) {
	if m.receiveErr != nil {
		return nil, m.receiveErr
	}
	return &harq.ReceiveResult{
		Payload: m.receiveData,
	}, nil
}

func (m *MockTransportProbe) GetState() harq.State  { return harq.StateIdle }
func (m *MockTransportProbe) GetTxSequence() uint16 { return 0 }
func (m *MockTransportProbe) GetRxSequence() uint16 { return 0 }
func (m *MockTransportProbe) Reset()                {}

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

// TestProbeTransport_SPI_Success tests successful SPI PING/PONG exchange
func TestProbeTransport_SPI_Success(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	// Create mock transport that returns valid PONG
	pongPayload := make([]byte, SPIProbePayloadSize)
	binary.BigEndian.PutUint32(pongPayload, SPIProbeTestMarker)

	mockTransport := &MockTransportProbe{
		receiveData: pongPayload,
	}

	wrapper := &TransportWrapper{
		Name:      TransportNameSPI,
		Transport: mockTransport,
		Priority:  PrioritySPI,
	}

	// Should return true (healthy)
	if !hm.probeTransport(wrapper) {
		t.Error("probeSPI should return true for valid PONG response")
	}
}

// TestProbeTransport_SPI_SendFails tests SPI probe when Send fails
func TestProbeTransport_SPI_SendFails(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	mockTransport := &MockTransportProbe{
		sendErr: errors.New("send failed"),
	}

	wrapper := &TransportWrapper{
		Name:      TransportNameSPI,
		Transport: mockTransport,
		Priority:  PrioritySPI,
	}

	// Should return false (unhealthy)
	if hm.probeTransport(wrapper) {
		t.Error("probeSPI should return false when Send fails")
	}
}

// TestProbeTransport_SPI_ReceiveTimeout tests SPI probe when Receive times out
func TestProbeTransport_SPI_ReceiveTimeout(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	mockTransport := &MockTransportProbe{
		receiveErr: context.DeadlineExceeded,
	}

	wrapper := &TransportWrapper{
		Name:      TransportNameSPI,
		Transport: mockTransport,
		Priority:  PrioritySPI,
	}

	// Should return false (unhealthy)
	if hm.probeTransport(wrapper) {
		t.Error("probeSPI should return false when Receive times out")
	}
}

// TestProbeTransport_SPI_InvalidPayloadLength tests SPI probe with wrong payload size
func TestProbeTransport_SPI_InvalidPayloadLength(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	// Return payload with wrong length (not SPIProbePayloadSize)
	mockTransport := &MockTransportProbe{
		receiveData: []byte{0x01, 0x02}, // Only 2 bytes instead of 4
	}

	wrapper := &TransportWrapper{
		Name:      TransportNameSPI,
		Transport: mockTransport,
		Priority:  PrioritySPI,
	}

	// Should return false (unhealthy)
	if hm.probeTransport(wrapper) {
		t.Error("probeSPI should return false for invalid payload length")
	}
}

// TestProbeTransport_SPI_WrongPayloadValue tests SPI probe with incorrect counter
func TestProbeTransport_SPI_WrongPayloadValue(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	// Return payload with wrong value (not SPIProbeTestMarker)
	wrongPayload := make([]byte, SPIProbePayloadSize)
	binary.BigEndian.PutUint32(wrongPayload, 0x12345678) // Wrong marker

	mockTransport := &MockTransportProbe{
		receiveData: wrongPayload,
	}

	wrapper := &TransportWrapper{
		Name:      TransportNameSPI,
		Transport: mockTransport,
		Priority:  PrioritySPI,
	}

	// Should return false (unhealthy)
	if hm.probeTransport(wrapper) {
		t.Error("probeSPI should return false for wrong payload value")
	}
}

// TestProbeTransport_USB tests USB probe dispatcher
//
// Note: Actual USB probing requires integration testing since probeUSB() calls
// serial.Open() directly without dependency injection. To make this unit-testable,
// we would need to refactor probeUSB to accept a USB interface abstraction.
//
// Current test verifies that probeTransport correctly dispatches to probeUSB.
func TestProbeTransport_USB(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	wrapper := &TransportWrapper{
		Name:      TransportNameUSB,
		Transport: &MockHARQ{},
		Priority:  PriorityUSB,
	}

	// This will call probeUSB() which attempts serial.Open()
	// Result depends on whether /dev/ttyACM0 exists on test system
	// We just verify it doesn't panic
	_ = hm.probeTransport(wrapper)
}

// TestProbeTransport_UnknownTransport tests default case for unknown transport types
func TestProbeTransport_UnknownTransport(t *testing.T) {
	hm := NewHealthMonitor(100 * time.Millisecond)

	wrapper := &TransportWrapper{
		Name:      "unknown-transport",
		Transport: &MockHARQ{},
		Priority:  5,
	}

	// Should return true (lenient for test/mock transports)
	if !hm.probeTransport(wrapper) {
		t.Error("probeTransport should return true for unknown transport types (test/mock support)")
	}
}
