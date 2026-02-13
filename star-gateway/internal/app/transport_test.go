// Package app tests for transport creation functions.
//
// STAR Project - Texas A&M University
// February 2026
package app

import (
	"context"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

// TestCreateSPITransport_Success verifies successful SPI transport creation and opening.
func TestCreateSPITransport_Success(t *testing.T) {
	// NOTE: This test requires /dev/spidev0.0 to exist
	// Skip if not on hardware
	if _, err := os.Stat("/dev/spidev0.0"); os.IsNotExist(err) {
		t.Skip("Skipping SPI test: /dev/spidev0.0 not available")
	}

	ctx := context.Background()

	device, err := createSPITransport(ctx)
	if err != nil {
		t.Fatalf("createSPITransport() failed: %v", err)
	}
	defer device.Close()

	// Verify interface compliance
	var _ transport.Device = device

	// Verify transport is open
	if !device.IsOpen() {
		t.Error("Expected transport to be open")
	}
}

// TestCreateSPITransport_ReturnsDeviceInterface verifies return type implements transport.Device.
func TestCreateSPITransport_ReturnsDeviceInterface(t *testing.T) {
	// Skip if not on hardware
	if _, err := os.Stat("/dev/spidev0.0"); os.IsNotExist(err) {
		t.Skip("Skipping SPI test: /dev/spidev0.0 not available")
	}

	ctx := context.Background()

	device, err := createSPITransport(ctx)
	if err != nil {
		t.Fatalf("createSPITransport() failed: %v", err)
	}
	defer device.Close()

	// Compile-time check: ensure return type is transport.Device
	var _ transport.Device = device

	// Runtime check: verify methods exist
	// Note: Go doesn't allow nil checks on methods directly, so we verify IsOpen instead
	if !device.IsOpen() {
		t.Error("Expected transport to be open (method verification)")
	}
}

// TestCreateSPITransport_UsesDefaultConfig verifies DefaultConfig() is used with correct parameters.
func TestCreateSPITransport_UsesDefaultConfig(t *testing.T) {
	// Skip if not on hardware
	if _, err := os.Stat("/dev/spidev0.0"); os.IsNotExist(err) {
		t.Skip("Skipping SPI test: /dev/spidev0.0 not available")
	}

	ctx := context.Background()

	spiTransport, err := createSPITransport(ctx)
	if err != nil {
		t.Fatalf("createSPITransport() failed: %v", err)
	}
	defer spiTransport.Close()

	// Type assert to access Config() method
	spi, ok := spiTransport.(*transport.SPITransport)
	if !ok {
		t.Fatal("Expected *transport.SPITransport type")
	}

	config := spi.Config()

	// Verify default values
	expectedConfig := transport.DefaultConfig()
	if config.Device != expectedConfig.Device {
		t.Errorf("Device = %s, want %s", config.Device, expectedConfig.Device)
	}
	if config.SpeedHz != expectedConfig.SpeedHz {
		t.Errorf("SpeedHz = %d, want %d", config.SpeedHz, expectedConfig.SpeedHz)
	}
	if config.Mode != expectedConfig.Mode {
		t.Errorf("Mode = %d, want %d", config.Mode, expectedConfig.Mode)
	}
}

// TestCreateSocketTransport_Success verifies successful socket transport creation.
func TestCreateSocketTransport_Success(t *testing.T) {
	// Create temporary socket path
	tempDir := t.TempDir()
	socketPath := filepath.Join(tempDir, "test.sock")

	// Start mock socket server
	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		t.Fatalf("Failed to create test socket: %v", err)
	}
	defer listener.Close()

	ctx := context.Background()

	device, err := createSocketTransport(ctx, socketPath)
	if err != nil {
		t.Fatalf("createSocketTransport() failed: %v", err)
	}
	defer device.Close()

	// Verify interface compliance
	var _ transport.Device = device

	// Verify transport is open
	if !device.IsOpen() {
		t.Error("Expected transport to be open")
	}
}

// TestCreateSocketTransport_InvalidPath verifies error handling for invalid socket path.
func TestCreateSocketTransport_InvalidPath(t *testing.T) {
	ctx := context.Background()

	// Use non-existent socket path
	_, err := createSocketTransport(ctx, "/nonexistent/path/socket.sock")
	if err == nil {
		t.Fatal("Expected error for invalid socket path, got nil")
	}

	// Verify error is wrapped
	if !strings.Contains(err.Error(), "failed to open socket transport") {
		t.Errorf("Error should be wrapped: %v", err)
	}
}

// TestCreateSocketTransport_ReturnsDeviceInterface verifies return type implements transport.Device.
func TestCreateSocketTransport_ReturnsDeviceInterface(t *testing.T) {
	tempDir := t.TempDir()
	socketPath := filepath.Join(tempDir, "test.sock")

	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		t.Fatalf("Failed to create test socket: %v", err)
	}
	defer listener.Close()

	ctx := context.Background()

	device, err := createSocketTransport(ctx, socketPath)
	if err != nil {
		t.Fatalf("createSocketTransport() failed: %v", err)
	}
	defer device.Close()

	// Compile-time check
	var _ transport.Device = device

	// Runtime check: verify transport is functional
	if !device.IsOpen() {
		t.Error("Expected transport to be open (method verification)")
	}
}

// TestCreateSPILink_WithValidTransport verifies createSPILink works with created transport.
func TestCreateSPILink_WithValidTransport(t *testing.T) {
	// Skip if not on hardware
	if _, err := os.Stat("/dev/spidev0.0"); os.IsNotExist(err) {
		t.Skip("Skipping SPI test: /dev/spidev0.0 not available")
	}

	ctx := context.Background()

	// Create transport
	spiTransport, err := createSPITransport(ctx)
	if err != nil {
		t.Fatalf("createSPITransport() failed: %v", err)
	}
	defer spiTransport.Close()

	// Create session
	session := manager.NewSessionState()

	// Create SPI link
	spiLink, err := createSPILink(spiTransport, session)
	if err != nil {
		t.Fatalf("createSPILink() failed: %v", err)
	}

	// Verify it implements harq.HARQ
	var _ harq.HARQ = spiLink

	// Verify methods exist and work
	state := spiLink.GetState()
	if state != harq.StateIdle {
		t.Errorf("Expected initial state Idle, got %v", state)
	}
}

// TestInitializeTransports_SPIMode verifies initializeTransports() uses createSPITransport correctly.
func TestInitializeTransports_SPIMode(t *testing.T) {
	// Skip if not on hardware
	if _, err := os.Stat("/dev/spidev0.0"); os.IsNotExist(err) {
		t.Skip("Skipping SPI test: /dev/spidev0.0 not available")
	}

	config := Config{
		SimulationMode: false, // Production mode
		TransportMode:  manager.ModeForceSPI,
	}

	mgrConfig := &manager.Config{
		Mode: config.TransportMode,
	}
	tm := manager.NewTransportManager(mgrConfig)

	ctx := context.Background()

	err := initializeTransports(ctx, config, tm)
	if err != nil {
		t.Fatalf("initializeTransports() failed: %v", err)
	}

	// Verify SPI transport was registered
	available := tm.GetAvailableTransports()
	hasSPI := false
	for _, name := range available {
		if name == manager.TransportNameSPI {
			hasSPI = true
			break
		}
	}

	if !hasSPI {
		t.Error("Expected SPI transport to be registered")
	}
}
