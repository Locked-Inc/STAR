// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Virtual RX72N - Hardware-in-the-Loop Simulator
//
// This program simulates the RX72N motor controller firmware for testing
// and development without physical hardware. It listens on a Unix domain
// socket and responds to Gateway commands.
//
// Usage:
//
//	go run ./cmd/virtual_rx72n/
//
// The simulator will:
//  1. Create a Unix socket at /tmp/star_rx72n.sock
//  2. Listen for connections from the Gateway
//  3. Echo back modified data to prove it's the simulator
//  4. Handle Ctrl+C gracefully
//
// STAR Project - Texas A&M University
// January 2026
package main

import (
	"encoding/binary"
	"errors"
	"io"
	"log/slog"
	"net"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

const (
	// SocketPath is the Unix domain socket path for Virtual RX72N.
	SocketPath = "/tmp/star_rx72n.sock"

	// MaxFrameSize is the maximum expected frame size (must match frame.MaxPayloadSize + overhead).
	MaxFrameSize = frame.MaxFrameSize

	// SimulatorMarker is the value written to the first byte to prove simulator processed the frame.
	SimulatorMarker = 0xFF

	// SignalBufferSize is the buffer size for OS signal channel.
	SignalBufferSize = 1

	// readDeadlineTimeout is the timeout for socket read operations.
	readDeadlineTimeout = 1 * time.Second

	// PingPayloadSize is the expected payload size for PING frames (4-byte little-endian counter).
	PingPayloadSize = 4

	// DefaultSimLatencyMs is the default simulated processing latency in milliseconds.
	DefaultSimLatencyMs = 10

	// MinSimLatencyMs is the minimum allowed simulated latency (0 disables latency).
	MinSimLatencyMs = 0

	// MaxSimLatencyMs is the maximum allowed simulated latency in milliseconds.
	MaxSimLatencyMs = 1000

	// SeqMask is the bitmask for 16-bit sequence number wraparound.
	SeqMask = 0xFFFF

	// EnvSimLatencyMs is the environment variable name for configurable latency.
	EnvSimLatencyMs = "SIM_LATENCY_MS"

	// DefaultFrontLeftDistanceM is the simulated front-left HC-SR04 distance in metres.
	// Represents a clear, unobstructed sensor reading within the valid 0.02-4.00 m range.
	DefaultFrontLeftDistanceM = 0.85

	// DefaultFrontRightDistanceM is the simulated front-right HC-SR04 distance in metres.
	// Represents a clear, unobstructed sensor reading within the valid 0.02-4.00 m range.
	DefaultFrontRightDistanceM = 0.92

	// DefaultBackLeftDistanceM is the simulated back-left HC-SR04 distance in metres.
	// Represents a closer-but-clear rear reading within the valid 0.02-4.00 m range.
	DefaultBackLeftDistanceM = 0.45

	// DefaultBackRightDistanceM is the simulated back-right HC-SR04 distance in metres.
	// Represents a closer-but-clear rear reading within the valid 0.02-4.00 m range.
	DefaultBackRightDistanceM = 0.38

	// ObstacleDetectionThresholdM is the distance threshold in metres below which any_obstacle
	// is set to true (30 cm, matching the firmware HC-SR04 detection logic).
	ObstacleDetectionThresholdM = 0.30
)

// simulatorSession tracks session state for the virtual RX72N simulator.
// The txSeq counter is used for response frames and resets to 0 on RESET.
type simulatorSession struct {
	txSeq uint16
}

// nextSeq returns the current txSeq and increments it with wraparound.
func (s *simulatorSession) nextSeq() uint16 {
	seq := s.txSeq
	s.txSeq = uint16((uint32(s.txSeq) + 1) & SeqMask)
	return seq
}

// reset resets the session sequence counter to 0.
func (s *simulatorSession) reset() {
	s.txSeq = 0
}

// frameResponse represents zero or more frames to send back to the gateway.
type frameResponse struct {
	frames []*frame.Frame
}

// parseSimLatency reads the SIM_LATENCY_MS environment variable and returns
// the configured latency duration. Returns DefaultSimLatencyMs if unset or invalid.
// Valid range: MinSimLatencyMs (0) to MaxSimLatencyMs (1000).
func parseSimLatency() time.Duration {
	envVal := os.Getenv(EnvSimLatencyMs)
	if envVal == "" {
		return time.Duration(DefaultSimLatencyMs) * time.Millisecond
	}

	ms, err := strconv.Atoi(envVal)
	if err != nil || ms < MinSimLatencyMs || ms > MaxSimLatencyMs {
		slog.Warn("invalid sim latency env var, using default",
			"name", EnvSimLatencyMs,
			"value", envVal,
			"min_ms", MinSimLatencyMs,
			"max_ms", MaxSimLatencyMs,
			"default_ms", DefaultSimLatencyMs)
		return time.Duration(DefaultSimLatencyMs) * time.Millisecond
	}

	return time.Duration(ms) * time.Millisecond
}

func main() {
	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	})))

	// Parse simulation latency (once at startup, not per-frame)
	simLatency := parseSimLatency()

	// Cleanup old socket file if it exists
	if err := os.Remove(SocketPath); err != nil && !os.IsNotExist(err) {
		slog.Warn("failed to remove old socket", "err", err)
	}

	// Create Unix domain socket listener
	listener, err := net.Listen("unix", SocketPath)
	if err != nil {
		slog.Error("Failed to create virtual device", "err", err)
		os.Exit(1)
	}
	defer listener.Close()

	slog.Info("Virtual RX72N Started. Waiting for Gateway...")
	slog.Info("simulator config",
		"socket", SocketPath,
		"max_frame_size", MaxFrameSize,
		"simulated_latency", simLatency)

	// Channel to signal graceful shutdown
	done := make(chan struct{})

	// Handle Ctrl+C gracefully
	sigChan := make(chan os.Signal, SignalBufferSize)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-sigChan
		slog.Info("Shutting down Virtual RX72N...")
		// Signal main loop to stop
		close(done)
		// Close listener to unblock Accept()
		listener.Close()
		if err := os.Remove(SocketPath); err != nil {
			slog.Warn("failed to remove socket", "err", err)
		}
	}()

	// Accept connections in a loop
	for {
		// Check if we are shutting down
		select {
		case <-done:
			return
		default:
		}

		conn, err := listener.Accept()
		if err != nil {
			// Check for shutdown-related errors
			select {
			case <-done:
				return
			default:
			}

			// If unexpected error, verify if it's a closed network connection
			if errors.Is(err, net.ErrClosed) {
				return
			}

			slog.Info("Connection error", "err", err)
			continue
		}
		slog.Info("Gateway connected", "remote_addr", conn.RemoteAddr())

		// Handle each connection in a goroutine
		go handleConnection(conn, simLatency)
	}
}

// handleConnection processes messages from a single Gateway connection.
func handleConnection(conn net.Conn, latency time.Duration) {
	defer conn.Close()
	defer slog.Info("Gateway disconnected")

	buffer := make([]byte, MaxFrameSize)
	session := &simulatorSession{}
	encoder := frame.NewEncoder()
	decoder := frame.NewDecoder()

	for {
		// Read data from Gateway
		// Set a read deadline (readDeadlineTimeout) to avoid indefinite blocking if the peer stalls.
		if err := conn.SetReadDeadline(time.Now().Add(readDeadlineTimeout)); err != nil {
			slog.Info("Failed to set read deadline", "err", err)
		}

		n, err := conn.Read(buffer)
		if err != nil {
			// Treat timeouts as transient; continue waiting for data.
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				continue
			}
			// EOF is expected when connection closes
			if errors.Is(err, io.EOF) {
				return
			}
			slog.Info("Read error", "err", err)
			return
		}

		rxData := buffer[:n]

		// Decode the frame
		decodedFrame, err := decoder.Decode(rxData)
		if err != nil {
			slog.Info("Frame decode error", "err", err)
			continue
		}

		slog.Info("Received frame",
			"seq", decodedFrame.Header.Sequence,
			"type", decodedFrame.Type.String(),
			"flags", decodedFrame.Header.Flags,
			"payload_len", len(decodedFrame.Payload))

		// Process frame through extracted handler
		resp := handleFrame(decodedFrame, session, latency)

		// Send all response frames
		for _, respFrame := range resp.frames {
			txData, err := encoder.Encode(respFrame)
			if err != nil {
				slog.Info("Frame encode error", "err", err)
				continue
			}

			slog.Info("Sending frame",
				"seq", respFrame.Header.Sequence,
				"type", respFrame.Type.String(),
				"payload_len", len(respFrame.Payload))

			if _, err := conn.Write(txData); err != nil {
				slog.Info("Write error", "err", err)
				return
			}
		}
	}
}

// processCommand handles incoming commands and generates appropriate responses.
func processCommand(wireMsg *starv1.WireMessage) *starv1.WireMessage {
	// Check which type of command was received
	switch payload := wireMsg.Payload.(type) {
	case *starv1.WireMessage_VelocityCommand:
		slog.Info("VelocityCommand",
			"fl_mps", payload.VelocityCommand.FrontLeftVelocityMps,
			"fr_mps", payload.VelocityCommand.FrontRightVelocityMps,
			"bl_mps", payload.VelocityCommand.BackLeftVelocityMps,
			"br_mps", payload.VelocityCommand.BackRightVelocityMps)

		// Generate telemetry response with simulated data
		return generateTelemetryResponse()

	case *starv1.WireMessage_EmergencyStopCommand:
		slog.Info("EmergencyStopCommand received",
			"reason", payload.EmergencyStopCommand.Reason)
		return generateEmergencyStopResponse()

	default:
		slog.Info("Unknown command type, generating default telemetry")
		return generateTelemetryResponse()
	}
}

// generateTelemetryResponse creates a realistic telemetry data response.
func generateTelemetryResponse() *starv1.WireMessage {
	return &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: &starv1.TelemetryData{
				TimestampUs:        time.Now().UnixMicro(),
				CpuUsagePercent:    32.1, // Light CPU load
				TemperatureCelsius: 28.5, // Room temperature
				MotorLoadPercent:   50.0, // Medium motor load
				WifiSignalDbm:      -45,  // Strong WiFi signal
				Imu: &starv1.ImuData{
					PitchRad:   0.01,  // Nearly level
					RollRad:    -0.02, // Slight roll
					YawRad:     0.0,
					AccelXMps2: 0.05,
					AccelYMps2: -0.02,
					AccelZMps2: 9.81, // Gravity
				},
				Obstacle: &starv1.ObstacleData{
					DistanceFrontLeftM:  DefaultFrontLeftDistanceM,
					DistanceFrontRightM: DefaultFrontRightDistanceM,
					DistanceBackLeftM:   DefaultBackLeftDistanceM,
					DistanceBackRightM:  DefaultBackRightDistanceM,
					AnyObstacle:         false, // All readings exceed ObstacleDetectionThresholdM
					DetectedMask:        0,     // No per-sensor detection bits set
				},
			},
		},
	}
}

// handleFrame processes a decoded frame and returns response frames.
// Control frame handling (PING/PONG, RESET/RESET_ACK) happens BEFORE any
// protobuf unmarshalling, since control frames have non-protobuf payloads.
func handleFrame(decoded *frame.Frame, session *simulatorSession, latency time.Duration) frameResponse {
	var responses []*frame.Frame

	// Phase 1: ACK if required (before any other processing)
	if (decoded.Header.Flags & frame.FlagRequiresAck) != 0 {
		ackFrame := &frame.Frame{
			Header: frame.Header{
				Sequence: decoded.Header.Sequence, // Echo received sequence
				Length:   0,
				Flags:    frame.FlagNone,
			},
			Type:    frame.FrameTypeAck,
			Payload: []byte{},
		}
		responses = append(responses, ackFrame)
	}

	// Phase 2: Control frame dispatch (BEFORE protobuf unmarshal)
	switch decoded.Type {
	case frame.FrameTypePing:
		return handlePing(decoded, session, latency, responses)

	case frame.FrameTypeReset:
		return handleReset(decoded, session, latency, responses)

	case frame.FrameTypeCommand:
		return handleCommand(decoded, session, responses)

	default:
		slog.Info("Unhandled frame type", "type", decoded.Type.String())
		return frameResponse{frames: responses}
	}
}

// handlePing responds to a PING frame with a PONG containing the same counter.
// Applies simulated latency before generating the response.
func handlePing(decoded *frame.Frame, session *simulatorSession, latency time.Duration, existing []*frame.Frame) frameResponse {
	if len(decoded.Payload) != PingPayloadSize {
		slog.Info("PING payload size mismatch",
			"got", len(decoded.Payload),
			"expected", PingPayloadSize)
		return frameResponse{frames: existing}
	}

	// Simulate processing latency (USB buffer delay)
	if latency > 0 {
		time.Sleep(latency)
	}

	counter := binary.LittleEndian.Uint32(decoded.Payload)
	slog.Info("PING received, sending PONG", "counter", counter)

	// Echo the same 4-byte counter in PONG
	pongPayload := make([]byte, PingPayloadSize)
	binary.LittleEndian.PutUint32(pongPayload, counter)

	pongFrame := &frame.Frame{
		Header: frame.Header{
			Sequence: session.nextSeq(),
			Length:   PingPayloadSize,
			Flags:    frame.FlagNone,
		},
		Type:    frame.FrameTypePong,
		Payload: pongPayload,
	}

	return frameResponse{frames: append(existing, pongFrame)}
}

// sessionIDPayloadSize is the expected payload size for session ID in RESET/RESET_ACK frames.
// Must match manager.SessionIDPayloadSize.
const sessionIDPayloadSize = 4

// handleReset responds to a RESET frame with RESET_ACK and resets session state.
// If the RESET frame carries a session ID (4-byte little-endian payload), it is echoed
// back in the RESET_ACK payload for the gateway to match the handshake.
// RESET_ACK is built with the current sequence, then session state is reset.
// Applies simulated latency before generating the response.
func handleReset(decoded *frame.Frame, session *simulatorSession, latency time.Duration, existing []*frame.Frame) frameResponse {
	// Simulate processing latency (USB buffer delay)
	if latency > 0 {
		time.Sleep(latency)
	}

	// Extract and echo session ID from RESET payload.
	// Only accept either an empty payload (no session ID) or exactly one session ID
	// of size `sessionIDPayloadSize`. Any other length is a protocol error.
	var resetAckPayload []byte
	if len(decoded.Payload) == 0 {
		slog.Info("RESET received (no session ID), sending RESET_ACK")
		resetAckPayload = []byte{}
	} else if len(decoded.Payload) == sessionIDPayloadSize {
		sessionID := binary.LittleEndian.Uint32(decoded.Payload[:sessionIDPayloadSize])
		slog.Info("RESET received, sending RESET_ACK", "session", sessionID)
		resetAckPayload = make([]byte, sessionIDPayloadSize)
		binary.LittleEndian.PutUint32(resetAckPayload, sessionID)
	} else {
		slog.Info("RESET payload length protocol error; dropping frame",
			"got", len(decoded.Payload),
			"expected_alt1", 0,
			"expected_alt2", sessionIDPayloadSize)
		return frameResponse{frames: existing}
	}

	// Build RESET_ACK with current sequence BEFORE resetting
	resetAckFrame := &frame.Frame{
		Header: frame.Header{
			Sequence: session.nextSeq(),
			Length:   uint16(len(resetAckPayload)),
			Flags:    frame.FlagNone,
		},
		Type:    frame.FrameTypeResetAck,
		Payload: resetAckPayload,
	}

	// Reset session state AFTER building RESET_ACK
	session.reset()

	return frameResponse{frames: append(existing, resetAckFrame)}
}

// handleCommand processes a COMMAND frame by unmarshalling protobuf and generating a response.
func handleCommand(decoded *frame.Frame, session *simulatorSession, existing []*frame.Frame) frameResponse {
	var wireMsg starv1.WireMessage
	if err := proto.Unmarshal(decoded.Payload, &wireMsg); err != nil {
		slog.Info("Protobuf unmarshal error", "err", err)
		return frameResponse{frames: existing}
	}

	responseMsg := processCommand(&wireMsg)

	responsePayload, err := proto.Marshal(responseMsg)
	if err != nil {
		slog.Info("Protobuf marshal error", "err", err)
		return frameResponse{frames: existing}
	}

	// Use frame.NewFrame to ensure payload size validation
	responseFrame, err := frame.NewFrame(frame.FrameTypeResponse, responsePayload)
	if err != nil {
		slog.Info("Failed to create response frame", "err", err)
		return frameResponse{frames: existing}
	}

	responseFrame.Header.Sequence = session.nextSeq()
	responseFrame.Header.Flags = frame.FlagNone

	return frameResponse{frames: append(existing, responseFrame)}
}

// generateEmergencyStopResponse creates a telemetry response indicating emergency stop.
func generateEmergencyStopResponse() *starv1.WireMessage {
	return &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: &starv1.TelemetryData{
				TimestampUs:        time.Now().UnixMicro(),
				CpuUsagePercent:    15.0, // CPU drops after motors stop
				TemperatureCelsius: 28.5,
				MotorLoadPercent:   0.0, // Motors stopped
				WifiSignalDbm:      -45,
				Imu: &starv1.ImuData{
					PitchRad:   0.0,
					RollRad:    0.0,
					YawRad:     0.0,
					AccelXMps2: 0.0,
					AccelYMps2: 0.0,
					AccelZMps2: 9.81, // Gravity
				},
			},
		},
	}
}
