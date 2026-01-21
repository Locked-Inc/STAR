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
	"log"
	"net"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/app"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/proto"
)

const (
	// Telemetry interval for mock device response rate
	mockTelemetryInterval = 10 * time.Millisecond

	// Gateway initialization timeout
	gatewayStartupTimeout = 2 * time.Second

	// Time to allow telemetry to settle before assertions
	telemetrySettleTime = 500 * time.Millisecond

	// Timeout for individual gRPC requests
	grpcRequestTimeout = 5 * time.Second

	// Timeout for graceful gateway shutdown
	gatewayShutdownTimeout = 20 * time.Second

	// Telemetry retry interval
	telemetryRetryInterval = 100 * time.Millisecond

	// Mock telemetry IMU gravity constant
	standardGravityMps2 = 9.81

	// Mock telemetry motor IDs
	motorIDFrontLeft  = 0
	motorIDFrontRight = 1
	motorIDBackLeft   = 2
	motorIDBackRight  = 3
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

	// Create frame encoder/decoder
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()

	// Buffer for reading incoming data (one complete frame)
	buf := make([]byte, frame.MaxFrameSize)

	// Sequence number for outgoing frames
	var sequenceNum uint16
	var timestamp int64

	// Store last encoded frame and pending queue for retransmission
	var lastEncodedFrame []byte
	var txQueue []byte

	// Main loop: receive and respond synchronously
	for {
		// Read from the socket into a buffer
		n, err := c.Read(buf)
		if err != nil {
			return
		}

		// Try to decode the incoming frame
		decodedFrame, decodeErr := decoder.Decode(buf[:n])
		isNack := decodeErr == nil && decodedFrame.Header.Type == frame.FrameTypeNack

		// 1. Decision: Queue Old (NACK) vs Generate New
		if isNack {
			if len(lastEncodedFrame) > 0 {
				txQueue = lastEncodedFrame
			}
		} else if len(txQueue) == 0 {
			// Generate NEW telemetry
			telemetry := generateMockTelemetryData(timestamp)
			timestamp += mockTelemetryInterval.Microseconds()

			wireMsg := &starv1.WireMessage{
				Payload: &starv1.WireMessage_TelemetryData{
					TelemetryData: telemetry,
				},
			}

			payload, err := proto.Marshal(wireMsg)
			if err != nil {
				log.Printf("Failed to marshal telemetry: %v", err)
				c.Write(make([]byte, n))
				continue
			}

			frameToSend := &frame.Frame{
				Header: frame.Header{
					Sequence: sequenceNum,
					Length:   uint16(len(payload)),
					Type:     frame.FrameTypeResponse,
					Flags:    frame.FlagNone,
				},
				Payload: payload,
			}
			sequenceNum++

			encodedFrame, err := encoder.Encode(frameToSend)
			if err != nil {
				log.Printf("Failed to encode frame: %v", err)
				c.Write(make([]byte, n))
				continue
			}

			txQueue = encodedFrame
			lastEncodedFrame = encodedFrame
		}

		// 2. Transmission: Check bandwidth
		if len(txQueue) > 0 && len(txQueue) <= n {
			// Data fits in the read window (or is equal)
			out := make([]byte, n)
			copy(out, txQueue)
			if _, err := c.Write(out); err != nil {
				log.Printf("Failed to write response: %v", err)
				return
			}
			// Success: Clear queue
			txQueue = nil
		} else {
			// Data doesn't fit (e.g. NACK read window is small), or nothing to send.
			// Send Idle/Zeros. The queued data waits for a larger window (Dummy Read).
			if _, err := c.Write(make([]byte, n)); err != nil {
				log.Printf("Failed to write idle response: %v", err)
				return
			}
		}
	}
}

// generateMockTelemetryData creates dummy telemetry data for testing.
func generateMockTelemetryData(timestampUs int64) *starv1.TelemetryData {
	return &starv1.TelemetryData{
		Imu: &starv1.ImuData{
			PitchRad:     0.01,
			RollRad:      -0.02,
			YawRad:       1.57,
			AccelXMps2:   0.1,
			AccelYMps2:   0.0,
			AccelZMps2:   standardGravityMps2,
			GyroXRadPerS: 0.0,
			GyroYRadPerS: 0.0,
			GyroZRadPerS: 0.0,
		},
		BatteryPercent:     85.0,
		WifiSignalDbm:      -45,
		CpuUsagePercent:    25.5,
		TemperatureCelsius: 35.2,
		MotorLoadPercent:   15.0,
		TimestampUs:        timestampUs,
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     motorIDFrontLeft,
			Ticks:       1000,
			VelocityMps: 0.5,
			TimestampUs: timestampUs,
		},
		EncoderFrontRight: &starv1.EncoderData{
			MotorId:     motorIDFrontRight,
			Ticks:       1020,
			VelocityMps: 0.51,
			TimestampUs: timestampUs,
		},
		EncoderBackLeft: &starv1.EncoderData{
			MotorId:     motorIDBackLeft,
			Ticks:       990,
			VelocityMps: 0.49,
			TimestampUs: timestampUs,
		},
		EncoderBackRight: &starv1.EncoderData{
			MotorId:     motorIDBackRight,
			Ticks:       1010,
			VelocityMps: 0.50,
			TimestampUs: timestampUs,
		},
		EmergencyStop:     false,
		FaultFlags:        0,
		BatteryVoltageV:   24.5,
		BatterySocPercent: 85,
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

	// Wait for Gateway to initialize with polling health check
	deadline := time.Now().Add(gatewayStartupTimeout)
	var conn *grpc.ClientConn
	for time.Now().Before(deadline) {
		// Check if Gateway crashed early
		select {
		case crashErr := <-errChan:
			t.Fatalf("Gateway crashed: %v", crashErr)
		default:
		}

		// Attempt to connect to gRPC endpoint
		conn, err = grpc.NewClient("[::1]:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))

		if err == nil {
			// Connection successful, gateway is ready
			break
		}

		// Retry after a short delay
		time.Sleep(telemetrySettleTime)
	}

	if conn == nil {
		t.Fatalf("Failed to connect to Gateway within timeout: %v", err)
	}
	defer conn.Close()

	gatewayClient := starv1.NewGatewayServiceClient(conn)
	telemetryClient := starv1.NewTelemetryServiceClient(conn)

	// 4. Test GetTelemetry (verify we receive valid telemetry data)
	// Retry loop to allow time for first telemetry frame to arrive
	var telemetryResp *starv1.GetTelemetryResponse
	telemetryReq := &starv1.GetTelemetryRequest{
		Header: &starv1.RequestHeader{RequestId: "e2e-telemetry-test"},
	}

	deadline = time.Now().Add(grpcRequestTimeout)
	for {
		ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
		telemetryResp, err = telemetryClient.GetTelemetry(ctx, telemetryReq)
		cancel()

		if err == nil && telemetryResp != nil && telemetryResp.Telemetry != nil {
			break
		}

		if time.Now().After(deadline) {
			if err != nil {
				t.Fatalf("GetTelemetry failed after timeout: %v", err)
			} else {
				t.Fatal("GetTelemetry returned empty response after timeout")
			}
		}
		time.Sleep(telemetryRetryInterval)
	}

	if telemetryResp.Telemetry == nil {
		t.Fatal("Expected non-nil telemetry data")
	}

	// Verify telemetry contains expected simulator data
	telemetry := telemetryResp.Telemetry
	if telemetry.BatteryVoltageV == 0 {
		t.Error("Expected non-zero battery voltage")
	}
	if telemetry.BatteryPercent == 0 {
		t.Error("Expected non-zero battery percentage")
	}
	if telemetry.Imu == nil {
		t.Error("Expected non-nil IMU data")
	}
	if telemetry.EncoderFrontLeft == nil {
		t.Error("Expected non-nil front left encoder data")
	}

	t.Logf("GetTelemetry success: battery=%.1fV (%.0f%%), IMU pitch=%.3f rad",
		telemetry.BatteryVoltageV,
		telemetry.BatteryPercent,
		telemetry.Imu.PitchRad)

	// 5. Test GetTeleopCommand (verify connection)
	req := &starv1.GetTeleopCommandRequest{
		Header: &starv1.RequestHeader{RequestId: "e2e-test"},
	}

	teleopCtx, teleopCancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer teleopCancel()
	resp, err := gatewayClient.GetTeleopCommand(teleopCtx, req)
	if err != nil {
		t.Fatalf("GetTeleopCommand failed: %v", err)
	}

	if resp == nil {
		t.Fatal("Expected non-nil response")
	}

	t.Logf("GetTeleopCommand success: available=%v", resp.CommandAvailable)

	// 6. Test Shutdown
	cancel()
	select {
	case err := <-errChan:
		if err != nil {
			t.Errorf("Gateway shutdown error: %v", err)
		}
	case <-time.After(gatewayShutdownTimeout):
		t.Fatal("Gateway did not shut down in time")
	}
}
