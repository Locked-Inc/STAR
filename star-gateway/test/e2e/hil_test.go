// Package e2e_test implements End-to-End tests for the STAR Gateway.
//
// These tests verify the integration of the full Gateway stack:
// gRPC Services -> Message Dispatcher -> HARQ -> Transport -> Virtual RX72N.
//
// STAR Project - Texas A&M University
// January 2026
package e2e_test

import (
	"context"
	"net"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/app"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// MockRX72N simulates the RX72N firmware behavior on a socket.
type MockRX72N struct {
	listener net.Listener
	conns    []net.Conn
	done     chan struct{}
}

func NewMockRX72N(socketPath string) (*MockRX72N, error) {
	// Cleanup old socket
	_ = os.Remove(socketPath)

	l, err := net.Listen("unix", socketPath)
	if err != nil {
		return nil, err
	}

	return &MockRX72N{
		listener: l,
		done:     make(chan struct{}),
	}, nil
}

func (m *MockRX72N) Start() {
	go func() {
		for {
			conn, err := m.listener.Accept()
			if err != nil {
				select {
				case <-m.done:
					return
				default:
					// Log error?
				}
				continue
			}
			m.conns = append(m.conns, conn)
			go m.handleConnection(conn)
		}
	}()
}

func (m *MockRX72N) Stop() {
	close(m.done)
	m.listener.Close()
	for _, c := range m.conns {
		c.Close()
	}
}

func (m *MockRX72N) handleConnection(c net.Conn) {
	defer c.Close()
	buf := make([]byte, 2048)
	for {
		n, err := c.Read(buf)
		if err != nil {
			return
		}
		// Echo back with modification (Simulation Logic)
		// Set first byte to 0xFF to mimic Virtual RX72N behavior
		resp := make([]byte, n)
		copy(resp, buf[:n])
		if len(resp) > 0 {
			resp[0] = 0xFF
		}
		c.Write(resp)
	}
}

func TestHIL_SimulatedIntegration(t *testing.T) {
	// 1. Setup Mock RX72N
	tempDir, err := os.MkdirTemp("", "star_e2e")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	defer os.RemoveAll(tempDir)

	socketPath := filepath.Join(tempDir, "rx72n.sock")
	mockDevice, err := NewMockRX72N(socketPath)
	if err != nil {
		t.Fatalf("Failed to create MockRX72N: %v", err)
	}
	mockDevice.Start()
	defer mockDevice.Stop()

	// 2. Start Gateway (in background)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	cfg := app.Config{
		SimulationMode: true,
		SocketPath:     socketPath,
	}

	errChan := make(chan error, 1)
	go func() {
		errChan <- app.Run(ctx, cfg)
	}()

	// Wait for Gateway to start (simple sleep for now, ideally wait for port)
	time.Sleep(1 * time.Second)

	// Check if Gateway crashed early
	select {
	case err := <-errChan:
		t.Fatalf("Gateway crashed: %v", err)
	default:
	}

	// 3. Connect gRPC Client (Simulating ROS2 Bridge)
	conn, err := grpc.NewClient("localhost:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to connect to Gateway gRPC: %v", err)
	}
	defer conn.Close()

	client := starv1.NewGatewayServiceClient(conn)

	// 4. Test GetTeleopCommand (verify connection)
	// Even if RX72N doesn't send valid telemetry yet, we should be able to call Gateway
	req := &starv1.GetTeleopCommandRequest{
		Header: &starv1.RequestHeader{RequestId: "e2e-test"},
	}

	resp, err := client.GetTeleopCommand(context.Background(), req)
	if err != nil {
		t.Fatalf("GetTeleopCommand failed: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	t.Logf("GetTeleopCommand success: available=%v", resp.CommandAvailable)

	// 5. Test Shutdown
	cancel()
	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Gateway shutdown error: %v", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("Gateway did not shut down in time")
	}
}
