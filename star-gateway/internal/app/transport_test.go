// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package app tests for transport creation functions.
//
// STAR Project - Texas A&M University
// February 2026
package app

import (
	"context"
	"errors"
	"log/slog"
	"net"
	"os"
	"path/filepath"
	"slices"
	"testing"

	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/link"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

var errMockSPIOpen = errors.New("mock spi open failure")
var errMockSPILink = errors.New("mock spi link failure")

// mockSPIDevice is a minimal transport.Device mock for SPI-path unit tests.
// All operations (Transfer, Send, Receive) succeed unconditionally:
// Transfer ignores context cancellation, Receive allocates maxLen bytes without
// bounds checking. Not suitable for testing error paths or realistic behavior.
type mockSPIDevice struct {
	open bool
}

func (m *mockSPIDevice) Transfer(ctx context.Context, txData []byte) ([]byte, error) {
	return make([]byte, len(txData)), nil
}

func (m *mockSPIDevice) Send(data []byte) (int, error) {
	return len(data), nil
}

func (m *mockSPIDevice) Receive(maxLen int) ([]byte, error) {
	return make([]byte, maxLen), nil
}

func (m *mockSPIDevice) IsOpen() bool { return m.open }
func (m *mockSPIDevice) Open() error  { m.open = true; return nil }
func (m *mockSPIDevice) Close() error { m.open = false; return nil }

func skipIfNoSPIHardware(t *testing.T) {
	t.Helper()
	if _, err := os.Stat("/dev/spidev0.0"); os.IsNotExist(err) {
		t.Skip("Skipping SPI test: /dev/spidev0.0 not available")
	}
}

func TestCreateSPITransport_Success(t *testing.T) {
	skipIfNoSPIHardware(t)

	device, err := createSPITransport()
	if err != nil {
		t.Fatalf("createSPITransport() failed: %v", err)
	}
	defer device.Close()

	if !device.IsOpen() {
		t.Error("Expected transport to be open")
	}
}

func TestCreateSPITransport_UsesDefaultConfig(t *testing.T) {
	skipIfNoSPIHardware(t)

	spiTransport, err := createSPITransport()
	if err != nil {
		t.Fatalf("createSPITransport() failed: %v", err)
	}
	defer spiTransport.Close()

	spi, ok := spiTransport.(*transport.SPITransport)
	if !ok {
		t.Fatal("Expected *transport.SPITransport type")
	}

	cfg := spi.Config()
	want := transport.DefaultConfig()

	tests := []struct {
		name string
		got  any
		want any
	}{
		{name: "Device", got: cfg.Device, want: want.Device},
		{name: "SpeedHz", got: cfg.SpeedHz, want: want.SpeedHz},
		{name: "Mode", got: cfg.Mode, want: want.Mode},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.want {
				t.Errorf("%s = %v, want %v", tc.name, tc.got, tc.want)
			}
		})
	}
}

func TestCreateSocketTransport(t *testing.T) {
	tests := []struct {
		name       string
		path       string
		setupSock  bool
		expectErr  error
		expectOpen bool
	}{
		{
			name:       "Success",
			setupSock:  true,
			expectOpen: true,
		},
		{
			name:      "InvalidPath",
			path:      "/nonexistent/path/socket.sock",
			expectErr: os.ErrNotExist,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			socketPath := tc.path
			var listener net.Listener
			if tc.setupSock {
				tempDir := t.TempDir()
				socketPath = filepath.Join(tempDir, "test.sock")
				var err error
				listener, err = net.Listen("unix", socketPath)
				if err != nil {
					t.Fatalf("Failed to create test socket: %v", err)
				}
				defer listener.Close()
			}

			device, err := createSocketTransport(socketPath)
			if tc.expectErr != nil {
				if err == nil {
					t.Fatalf("Expected error for %s, got nil", tc.name)
				}
				if !errors.Is(err, tc.expectErr) {
					t.Fatalf("Expected error wrapping %v, got %v", tc.expectErr, err)
				}
				return
			}

			if err != nil {
				t.Fatalf("createSocketTransport() failed: %v", err)
			}
			defer device.Close()

			if device.IsOpen() != tc.expectOpen {
				t.Errorf("IsOpen() = %v, want %v", device.IsOpen(), tc.expectOpen)
			}
		})
	}
}

func TestCreateSPILink_WithValidTransport(t *testing.T) {
	tests := []struct {
		name             string
		needHardware     bool
		transportFactory func(t *testing.T) transport.Device
		sessionFactory   func(t *testing.T) *manager.SessionState
		expectError      bool
		expectedState    harq.State
	}{
		{
			name:         "valid",
			needHardware: true,
			transportFactory: func(t *testing.T) transport.Device {
				t.Helper()
				dev, err := createSPITransport()
				if err != nil {
					t.Fatalf("createSPITransport() failed: %v", err)
				}
				return dev
			},
			sessionFactory: func(t *testing.T) *manager.SessionState {
				t.Helper()
				return manager.NewSessionState()
			},
			expectError:   false,
			expectedState: harq.StateIdle,
		},
		{
			name:             "nil transport",
			needHardware:     false,
			transportFactory: nil,
			sessionFactory: func(t *testing.T) *manager.SessionState {
				t.Helper()
				return manager.NewSessionState()
			},
			expectError: true,
		},
		{
			name:         "nil session",
			needHardware: false,
			transportFactory: func(t *testing.T) transport.Device {
				t.Helper()
				return &mockSPIDevice{open: true}
			},
			sessionFactory: nil,
			expectError:    true,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.needHardware {
				skipIfNoSPIHardware(t)
			}

			var dev transport.Device
			if tc.transportFactory != nil {
				dev = tc.transportFactory(t)
				defer dev.Close()
			}

			var session *manager.SessionState
			if tc.sessionFactory != nil {
				session = tc.sessionFactory(t)
			}

			spiLink, err := createSPILink(dev, session)
			if tc.expectError {
				if err == nil {
					t.Fatal("expected error, got nil")
				}
				return
			}
			if err != nil {
				t.Fatalf("createSPILink() failed: %v", err)
			}

			if state := spiLink.GetState(); state != tc.expectedState {
				t.Errorf("Expected initial state %v, got %v", tc.expectedState, state)
			}
		})
	}
}

func TestInitializeTransports_SPIModeWithMockSPI(t *testing.T) {
	tests := []struct {
		name                string
		spiTransportFactory func() (transport.Device, error)
		spiLinkFactory      func(transport.Device, *manager.SessionState) (harq.HARQ, error)
		expectSPIRegistered bool
	}{
		{
			name: "SPIOpenFails",
			spiTransportFactory: func() (transport.Device, error) {
				return nil, errMockSPIOpen
			},
			spiLinkFactory: func(device transport.Device, session *manager.SessionState) (harq.HARQ, error) {
				return link.NewSPILink(device, session, nil /* no custom config */)
			},
			expectSPIRegistered: false,
		},
		{
			name: "SPILinkCreationFails",
			spiTransportFactory: func() (transport.Device, error) {
				return &mockSPIDevice{open: true}, nil
			},
			spiLinkFactory: func(device transport.Device, session *manager.SessionState) (harq.HARQ, error) {
				return nil, errMockSPILink
			},
			expectSPIRegistered: false,
		},
		{
			name: "SPIRegistersSuccessfully",
			spiTransportFactory: func() (transport.Device, error) {
				return &mockSPIDevice{open: true}, nil
			},
			spiLinkFactory: func(device transport.Device, session *manager.SessionState) (harq.HARQ, error) {
				return link.NewSPILink(device, session, nil /* no custom config */)
			},
			expectSPIRegistered: true,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			origCreateSPITransport := createSPITransportFn
			origCreateSPILink := createSPILinkFn
			defer func() {
				createSPITransportFn = origCreateSPITransport
				createSPILinkFn = origCreateSPILink
			}()

			createSPITransportFn = tc.spiTransportFactory
			createSPILinkFn = tc.spiLinkFactory

			config := Config{SimulationMode: false, TransportMode: manager.ModeForceSPI}
			mgrConfig := manager.DefaultConfig()
			mgrConfig.Mode = config.TransportMode
			tm := manager.NewTransportManager(mgrConfig)
			defer func() { _ = tm.Stop() }()

			if err := initializeTransports(config, tm, slog.Default()); err != nil {
				t.Fatalf("initializeTransports() failed unexpectedly: %v", err)
			}

			hasSPI := slices.Contains(tm.GetAvailableTransports(), manager.TransportNameSPI)
			if hasSPI != tc.expectSPIRegistered {
				t.Errorf("SPI registered = %v, want %v", hasSPI, tc.expectSPIRegistered)
			}
		})
	}
}

func TestInitializeTransports_SPIMode(t *testing.T) {
	skipIfNoSPIHardware(t)

	config := Config{
		SimulationMode: false,
		TransportMode:  manager.ModeForceSPI,
	}

	mgrConfig := manager.DefaultConfig()
	mgrConfig.Mode = config.TransportMode
	tm := manager.NewTransportManager(mgrConfig)
	defer func() { _ = tm.Stop() }()

	err := initializeTransports(config, tm, slog.Default())
	if err != nil {
		t.Fatalf("initializeTransports() failed: %v", err)
	}

	if !slices.Contains(tm.GetAvailableTransports(), manager.TransportNameSPI) {
		t.Error("Expected SPI transport to be registered")
	}
}
