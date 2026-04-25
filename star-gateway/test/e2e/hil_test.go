// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

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
	"io"
	"log"
	"net"
	"os"
	"path/filepath"
	"sync/atomic"
	"testing"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/app"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
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
	// Set to 10s - adequate for clean shutdown now that duplicate wait logic is removed
	// If this timeout fires, it indicates a stuck resource (transport, dispatcher, etc.)
	gatewayShutdownTimeout = 10 * time.Second

	// Telemetry retry interval
	telemetryRetryInterval = 100 * time.Millisecond

	// Mock telemetry IMU gravity constant
	standardGravityMps2 = 9.81

	// Mock telemetry motor IDs
	motorIDFrontLeft  = 0
	motorIDFrontRight = 1
	motorIDBackLeft   = 2
	motorIDBackRight  = 3

	// DummyReadCheckLen is the minimum number of bytes inspected to detect
	// zero-filled dummy frames. Chosen to exceed typical header/frame size to
	// reliably detect zero-filled SPI transfers during tests.
	DummyReadCheckLen = 100

	// Connection initialization delay (allows socket connection to establish)
	connectionInitDelay = 50 * time.Millisecond

	// Sequence monitoring interval for diagnostics
	sequenceDiagnosticInterval = 100 * time.Millisecond

	// Sequence stuck threshold (fail if no change for this many checks)
	sequenceStuckThreshold = 10
)

// MockRX72N simulates the RX72N firmware behavior on a socket.
type MockRX72N struct {
	listener    net.Listener
	conns       []net.Conn
	done        chan struct{}
	lastSeqSent atomic.Uint32 // Track last sent sequence for diagnostics
	verbose     bool
}

func NewMockRX72N(socketPath string) (*MockRX72N, error) {
	// Cleanup old socket
	_ = os.Remove(socketPath)

	l, err := net.Listen("unix", socketPath)
	if err != nil {
		return nil, err
	}

	return &MockRX72N{
		listener:    l,
		done:        make(chan struct{}),
		lastSeqSent: atomic.Uint32{},
		verbose:     testing.Verbose(),
	}, nil
}

func (m *MockRX72N) debugLog(format string, args ...interface{}) {
	if m.verbose {
		log.Printf("MockRX72N: "+format, args...)
	}
}

// GetLastSequence returns the last sent sequence number for diagnostics.
func (m *MockRX72N) GetLastSequence() uint16 {
	return uint16(m.lastSeqSent.Load())
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

	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()
	buf := make([]byte, frame.MaxFrameSize)

	var sequenceNum uint16 = 0
	var timestamp int64
	var lastFrameToRetransmit *frame.Frame

	// Don't send initial frame proactively - wait for Gateway to poll first
	// This matches real SPI behavior where peripheral only responds to controller

	for {
		n, err := io.ReadFull(c, buf)
		if err != nil {
			if err != io.EOF {
				log.Printf("MockRX72N: Connection error: %v", err)
			}
			return
		}

		// Check if this is a dummy read (Gateway polling for data)
		if isDummyRead(buf[:n]) {
			// Send buffered frame, or generate fresh telemetry
			if lastFrameToRetransmit == nil {
				lastFrameToRetransmit = m.generateTelemetryFrame(sequenceNum, &timestamp)
				sequenceNum++
			}
			if err := m.sendFrame(c, encoder, lastFrameToRetransmit); err != nil {
				log.Printf("MockRX72N: Failed to send frame: %v", err)
				return
			}
			// Control frames (RESET_ACK, PONG) are one-shot: clear after sending
			// so the next dummy read generates telemetry instead of repeating.
			if lastFrameToRetransmit.Type != frame.FrameTypeResponse {
				lastFrameToRetransmit = nil
			}
			continue
		}

		// Attempt to decode the frame
		decodedFrame, decodeErr := decoder.Decode(buf[:n])

		if decodeErr != nil {
			m.debugLog("Decode error or unexpected frame, retransmitting")
			if lastFrameToRetransmit != nil {
				nextFrame := lastFrameToRetransmit
				nextFrame.Header.Flags |= frame.FlagRetransmit
				if err := m.sendFrame(c, encoder, nextFrame); err != nil {
					log.Printf("MockRX72N: Failed to send frame: %v", err)
					return
				}
			}
			continue
		}

		// Successfully decoded a frame
		var nextFrame *frame.Frame

		switch decodedFrame.Type {
		case frame.FrameTypeNack:
			nackedSeq := decodedFrame.Header.Sequence
			m.debugLog("Received NACK Seq=%d, retransmitting", nackedSeq)

			if lastFrameToRetransmit != nil && lastFrameToRetransmit.Header.Sequence == nackedSeq {
				nextFrame = lastFrameToRetransmit
				nextFrame.Header.Flags |= frame.FlagRetransmit
			} else {
				nextFrame = m.generateTelemetryFrame(nackedSeq, &timestamp)
			}

		case frame.FrameTypeAck:
			ackedSeq := decodedFrame.Header.Sequence
			m.debugLog("Received ACK Seq=%d", ackedSeq)

			// [PASS] FIXED: After ACK, clear the frame and WAIT for next request
			// Don't spontaneously send the next frame - this implements proper
			// Stop-and-Wait HARQ where Gateway (Controller) initiates requests
			// and MockRX72N (Peripheral) only responds when asked.
			if lastFrameToRetransmit != nil && ackedSeq == lastFrameToRetransmit.Header.Sequence {
				lastFrameToRetransmit = nil // Clear frame, wait for next dummy read
			}
			// Don't set nextFrame - let the next dummy read trigger new telemetry
			continue

		case frame.FrameTypePing:
			// Real firmware: send PONG echoing payload (rx_usb_comm.c:1130-1141).
			// In socket simulation, Send() consumes the Transfer response slot, so
			// the PONG sent here is discarded. Set as nextFrame (sent this cycle) but
			// don't persist in lastFrameToRetransmit -- the implicit heartbeat timer
			// (OnFrameReceived on any valid frame) is the primary liveness detector.
			nextFrame = &frame.Frame{
				Header: frame.Header{
					Sequence: sequenceNum,
					Length:   uint16(len(decodedFrame.Payload)),
					Flags:    frame.FlagNone,
				},
				Type:    frame.FrameTypePong,
				Payload: decodedFrame.Payload,
			}
			sequenceNum++
			m.debugLog("Prepared PONG for PING Seq=%d", decodedFrame.Header.Sequence)

		case frame.FrameTypeReset:
			// Real firmware: send RESET_ACK echoing session ID, reset session
			// (rx_usb_comm.c:1144-1156).
			//
			// Socket transport detail: SocketTransport.Send() calls Transfer()
			// which writes the RESET then reads the response -- and DISCARDS it.
			// So the RESET_ACK sent in THIS cycle is lost. By also setting
			// lastFrameToRetransmit, the RESET_ACK is re-sent on the NEXT dummy
			// read (triggered by drainUntilResetAck -> Receive -> Transfer(zeros)).
			nextFrame = &frame.Frame{
				Header: frame.Header{
					Sequence: sequenceNum,
					Length:   uint16(len(decodedFrame.Payload)),
					Flags:    frame.FlagNone,
				},
				Type:    frame.FrameTypeResetAck,
				Payload: decodedFrame.Payload,
			}
			lastFrameToRetransmit = nextFrame
			m.debugLog("Prepared RESET_ACK, resetting session (was seq=%d)", sequenceNum)
			sequenceNum = 0

		case frame.FrameTypePong, frame.FrameTypeResetAck:
			// Real firmware: consume silently (rx_usb_comm.c:1158-1161)
			m.debugLog("Consumed %s silently", decodedFrame.Type)
			continue

		case frame.FrameTypeCommand:
			// If the frame requires an ACK, send it first
			if (decodedFrame.Header.Flags & frame.FlagRequiresAck) != 0 {
				ackFrame := &frame.Frame{
					Header: frame.Header{
						Sequence: decodedFrame.Header.Sequence,
						Length:   0,
						Flags:    frame.FlagNone,
					},
					Type:    frame.FrameTypeAck,
					Payload: []byte{},
				}

				if err := m.sendFrame(c, encoder, ackFrame); err != nil {
					log.Printf("MockRX72N: Failed to send ACK: %v", err)
					return
				}
				m.debugLog("Sent ACK for Seq=%d", decodedFrame.Header.Sequence)
			}

			// Generate telemetry response
			nextFrame = m.generateTelemetryFrame(sequenceNum, &timestamp)
			lastFrameToRetransmit = nextFrame
			sequenceNum++

		default:
			m.debugLog("Unexpected frame type %s", decodedFrame.Type)
			if lastFrameToRetransmit != nil {
				nextFrame = lastFrameToRetransmit
				nextFrame.Header.Flags |= frame.FlagRetransmit
			} else {
				nextFrame = m.generateTelemetryFrame(sequenceNum, &timestamp)
				lastFrameToRetransmit = nextFrame
				sequenceNum++
			}
		}

		if nextFrame != nil {
			if err := m.sendFrame(c, encoder, nextFrame); err != nil {
				log.Printf("MockRX72N: Failed to send frame: %v", err)
				return
			}
		}
	}
}

// Keep isDummyRead as before
func isDummyRead(buf []byte) bool {
	checkLen := DummyReadCheckLen
	if len(buf) < checkLen {
		checkLen = len(buf)
	}

	for i := 0; i < checkLen; i++ {
		if buf[i] != 0 {
			return false
		}
	}
	return true
}

func (m *MockRX72N) sendFrame(c net.Conn, encoder frame.Encoder, f *frame.Frame) error {
	encoded, err := encoder.Encode(f)
	if err != nil {
		return err
	}

	// Always send full-sized buffer for SPI simulation
	out := make([]byte, frame.MaxFrameSize)
	copy(out, encoded)

	if _, err := c.Write(out); err != nil {
		return err
	}

	m.debugLog("Sent Seq=%d Type=%s (%d bytes)", f.Header.Sequence, f.Type.String(), len(encoded))

	// Update diagnostics
	m.lastSeqSent.Store(uint32(f.Header.Sequence))

	return nil
}

func (m *MockRX72N) generateTelemetryFrame(seq uint16, timestamp *int64) *frame.Frame {
	telemetry := generateMockTelemetryData(*timestamp)
	*timestamp += mockTelemetryInterval.Microseconds()

	wireMsg := &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: telemetry,
		},
	}

	payload, _ := proto.Marshal(wireMsg)

	f, _ := frame.NewFrame(frame.FrameTypeResponse, payload)
	f.Header.Sequence = seq
	return f
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
		EmergencyStop: false,
		FaultFlags:    0,
	}
}

func TestHIL_SimulatedIntegration(t *testing.T) {
	// 1. Setup Mock RX72N
	tempDir, err := os.MkdirTemp("", "star_e2e")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	t.Cleanup(func() { os.RemoveAll(tempDir) })

	socketPath := filepath.Join(tempDir, "rx72n.sock")
	mockDevice, err := NewMockRX72N(socketPath)
	if err != nil {
		t.Fatalf("Failed to create MockRX72N: %v", err)
	}
	mockDevice.Start()
	t.Cleanup(func() { mockDevice.Stop() })

	// Allow socket connection to establish before starting Gateway
	time.Sleep(connectionInitDelay)

	// 2. Start Gateway (in background)
	ctx, cancel := context.WithCancel(context.Background())

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
	t.Cleanup(func() { conn.Close() })

	// Ensure graceful shutdown with timeout
	// This cleanup runs AFTER the test completes (LIFO order)
	// Do NOT add duplicate shutdown logic in step 6 - rely solely on cleanup
	t.Cleanup(func() {
		t.Log("Cleanup: Triggering gateway shutdown...")
		cancel() // Signal shutdown to app.Run

		// Wait for app.Run to complete with timeout
		select {
		case err := <-errChan:
			if err != nil && err != context.Canceled {
				t.Errorf("Gateway shutdown error: %v", err)
			} else {
				t.Logf("Gateway shutdown completed gracefully")
			}
		case <-time.After(gatewayShutdownTimeout):
			t.Errorf("Gateway did not shut down in time (timeout: %v) - this indicates a stuck resource", gatewayShutdownTimeout)
		}
	})

	gatewayClient := starv1.NewGatewayServiceClient(conn)
	telemetryClient := starv1.NewTelemetryServiceClient(conn)

	// 3. Start sequence monitoring goroutine to detect ARQ desynchronization
	monitorDone := make(chan struct{})
	go func() {
		ticker := time.NewTicker(sequenceDiagnosticInterval)
		defer ticker.Stop()

		sameSeqCount := 0
		var lastSeq uint16

		for {
			select {
			case <-monitorDone:
				return
			case <-ticker.C:
				currentSeq := mockDevice.GetLastSequence()
				if currentSeq == lastSeq {
					sameSeqCount++
					if sameSeqCount >= sequenceStuckThreshold {
						log.Printf("WARNING: ARQ sequence stuck at %d for %d checks (potential desync)",
							currentSeq, sameSeqCount)
						// Don't fail immediately - could be normal during test setup
					}
				} else {
					if sameSeqCount > 0 {
						log.Printf("ARQ sequence progressed: %d -> %d (was stuck for %d checks)",
							lastSeq, currentSeq, sameSeqCount)
					}
					sameSeqCount = 0
				}
				lastSeq = currentSeq
			}
		}
	}()
	t.Cleanup(func() { close(monitorDone) })

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
	if telemetry.Imu == nil {
		t.Error("Expected non-nil IMU data")
	}
	if telemetry.EncoderFrontLeft == nil {
		t.Error("Expected non-nil front left encoder data")
	}

	t.Logf("GetTelemetry success: IMU pitch=%.3f rad", telemetry.Imu.PitchRad)

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
}

// TestHIL_SimpleUSBSimulation verifies that ModeSimpleUSB works end-to-end
// with the virtual RX72N over a socket transport. This exercises the code
// path where the gateway skips the RESET handshake, uses permissive sequence
// validation, and registers the socket as a USB transport.
func TestHIL_SimpleUSBSimulation(t *testing.T) {
	// 1. Setup Mock RX72N
	tempDir, err := os.MkdirTemp("", "star_e2e_simple_usb")
	if err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	t.Cleanup(func() { os.RemoveAll(tempDir) })

	socketPath := filepath.Join(tempDir, "rx72n.sock")
	mockDevice, err := NewMockRX72N(socketPath)
	if err != nil {
		t.Fatalf("Failed to create MockRX72N: %v", err)
	}
	mockDevice.Start()
	t.Cleanup(func() { mockDevice.Stop() })

	time.Sleep(connectionInitDelay)

	// 2. Start Gateway in simple-usb simulation mode
	ctx, cancel := context.WithCancel(context.Background())

	cfg := app.Config{
		SimulationMode: true,
		SocketPath:     socketPath,
		TransportMode:  manager.ModeSimpleUSB,
	}

	errChan := make(chan error, 1)
	startTime := time.Now()
	go func() {
		errChan <- app.Run(ctx, cfg)
	}()

	// 3. Wait for Gateway to initialize -- should be fast (no reset handshake)
	deadline := time.Now().Add(gatewayStartupTimeout)
	var conn *grpc.ClientConn
	for time.Now().Before(deadline) {
		select {
		case crashErr := <-errChan:
			t.Fatalf("Gateway crashed during startup: %v", crashErr)
		default:
		}

		conn, err = grpc.NewClient("[::1]:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
		if err == nil {
			break
		}
		time.Sleep(telemetryRetryInterval)
	}

	if conn == nil {
		t.Fatalf("Failed to connect to Gateway within timeout: %v", err)
	}
	t.Cleanup(func() { conn.Close() })

	startupDuration := time.Since(startTime)
	t.Logf("Gateway started in %v (simple-usb, no reset handshake)", startupDuration)

	// Graceful shutdown cleanup
	t.Cleanup(func() {
		t.Log("Cleanup: Triggering gateway shutdown...")
		cancel()
		select {
		case err := <-errChan:
			if err != nil && err != context.Canceled {
				t.Errorf("Gateway shutdown error: %v", err)
			} else {
				t.Logf("Gateway shutdown completed gracefully")
			}
		case <-time.After(gatewayShutdownTimeout):
			t.Errorf("Gateway did not shut down in time")
		}
	})

	telemetryClient := starv1.NewTelemetryServiceClient(conn)

	// 4. Verify telemetry flows through the simple-USB path
	var telemetryResp *starv1.GetTelemetryResponse
	telemetryReq := &starv1.GetTelemetryRequest{
		Header: &starv1.RequestHeader{RequestId: "simple-usb-test"},
	}

	deadline = time.Now().Add(grpcRequestTimeout)
	for {
		reqCtx, reqCancel := context.WithTimeout(context.Background(), 1*time.Second)
		telemetryResp, err = telemetryClient.GetTelemetry(reqCtx, telemetryReq)
		reqCancel()

		if err == nil && telemetryResp != nil && telemetryResp.Telemetry != nil {
			break
		}

		if time.Now().After(deadline) {
			if err != nil {
				t.Fatalf("GetTelemetry failed after timeout: %v", err)
			}
			t.Fatal("GetTelemetry returned empty response after timeout")
		}
		time.Sleep(telemetryRetryInterval)
	}

	// 5. Verify telemetry contains expected simulator data
	telemetry := telemetryResp.Telemetry
	if telemetry.Imu == nil {
		t.Fatal("Expected non-nil IMU data")
	}
	if telemetry.EncoderFrontLeft == nil {
		t.Fatal("Expected non-nil front left encoder data")
	}
	if telemetry.EncoderFrontRight == nil {
		t.Fatal("Expected non-nil front right encoder data")
	}

	t.Logf("Simple-USB telemetry OK: IMU accel_z=%.2f m/s^2, encoder_fl=%d ticks",
		telemetry.Imu.AccelZMps2, telemetry.EncoderFrontLeft.Ticks)
}
