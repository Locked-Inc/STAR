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
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/proto"
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

	// Buffer for reading incoming data
	buf := make([]byte, 2048)

	// Sequence number for outgoing frames
	var sequenceNum uint16
	var timestamp int64

	// Main loop: receive and respond synchronously
	for {
		n, err := c.Read(buf)
		if err != nil {
			return
		}

		// Try to decode the incoming frame
		decodedFrame, decodeErr := decoder.Decode(buf[:n])

		// Check if this is a dummy read (all zeros)
		isDummyRead := n > 0 && isAllZeros(buf[:n])

		if decodeErr == nil && !isDummyRead {
			// Valid command frame received - could process it here if needed
			_ = decodedFrame
		}

		// Generate telemetry response
		telemetry := generateMockTelemetryData(timestamp)
		timestamp += 10000 // 10ms in microseconds

		// Wrap in WireMessage
		wireMsg := &starv1.WireMessage{
			Payload: &starv1.WireMessage_TelemetryData{
				TelemetryData: telemetry,
			},
		}

		// Marshal to protobuf
		payload, err := proto.Marshal(wireMsg)
		if err != nil {
			// Return zeros on error
			c.Write(make([]byte, n))
			continue
		}

		// Create frame
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

		// Encode frame
		encodedFrame, err := encoder.Encode(frameToSend)
		if err != nil {
			// Return zeros on error
			c.Write(make([]byte, n))
			continue
		}

		// Pad or truncate to match expected response length
		responseData := make([]byte, n)
		if len(encodedFrame) <= n {
			copy(responseData, encodedFrame)
		} else {
			copy(responseData, encodedFrame[:n])
		}

		c.Write(responseData)
	}
}

// isAllZeros checks if a byte slice contains only zeros.
func isAllZeros(data []byte) bool {
	for _, b := range data {
		if b != 0 {
			return false
		}
	}
	return true
}

// generateMockTelemetryData creates dummy telemetry data for testing.
func generateMockTelemetryData(timestampUs int64) *starv1.TelemetryData {
	return &starv1.TelemetryData{
		Imu: &starv1.ImuData{
			PitchRad:       0.01,
			RollRad:        -0.02,
			YawRad:         1.57,
			AccelXMps2:     0.1,
			AccelYMps2:     0.0,
			AccelZMps2:     9.81,
			GyroXRadPerS:   0.0,
			GyroYRadPerS:   0.0,
			GyroZRadPerS:   0.0,
		},
		BatteryPercent:     85.0,
		WifiSignalDbm:      -45,
		CpuUsagePercent:    25.5,
		TemperatureCelsius: 35.2,
		MotorLoadPercent:   15.0,
		TimestampUs:        timestampUs,
		EncoderFrontLeft: &starv1.EncoderData{
			MotorId:     0,
			Ticks:       1000,
			VelocityMps: 0.5,
			TimestampUs: timestampUs,
		},
		EncoderFrontRight: &starv1.EncoderData{
			MotorId:     1,
			Ticks:       1020,
			VelocityMps: 0.51,
			TimestampUs: timestampUs,
		},
		EncoderBackLeft: &starv1.EncoderData{
			MotorId:     2,
			Ticks:       990,
			VelocityMps: 0.49,
			TimestampUs: timestampUs,
		},
		EncoderBackRight: &starv1.EncoderData{
			MotorId:     3,
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

	// Wait for Gateway to initialize (TODO: implement proper health check polling)
	time.Sleep(2 * time.Second)

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

	gatewayClient := starv1.NewGatewayServiceClient(conn)
	telemetryClient := starv1.NewTelemetryServiceClient(conn)

	// 4. Test GetTelemetry (verify we receive valid telemetry data)
	telemetryReq := &starv1.GetTelemetryRequest{
		Header: &starv1.RequestHeader{RequestId: "e2e-telemetry-test"},
	}

	// Wait a bit for telemetry to be received by dispatcher
	time.Sleep(500 * time.Millisecond)

	telemetryResp, err := telemetryClient.GetTelemetry(context.Background(), telemetryReq)
	if err != nil {
		t.Fatalf("GetTelemetry failed: %v", err)
	}

	if telemetryResp == nil {
		t.Fatal("Expected non-nil telemetry response")
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

	resp, err := gatewayClient.GetTeleopCommand(context.Background(), req)
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
	case <-time.After(2 * time.Second):
		t.Fatal("Gateway did not shut down in time")
	}
}
