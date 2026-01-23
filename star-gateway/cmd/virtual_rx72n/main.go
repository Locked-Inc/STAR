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
	"errors"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
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
	MaxFrameSize = 2048

	// SimulatorMarker is the value written to the first byte to prove simulator processed the frame.
	SimulatorMarker = 0xFF

	// SignalBufferSize is the buffer size for OS signal channel.
	SignalBufferSize = 1

	// readDeadlineTimeout is the timeout for socket read operations.
	readDeadlineTimeout = 1 * time.Second
)

func main() {
	// Cleanup old socket file if it exists
	if err := os.Remove(SocketPath); err != nil && !os.IsNotExist(err) {
		log.Printf("Warning: failed to remove old socket: %v", err)
	}

	// Create Unix domain socket listener
	listener, err := net.Listen("unix", SocketPath)
	if err != nil {
		log.Fatalf("Failed to create virtual device: %v", err)
	}
	defer listener.Close()

	log.Println("Virtual RX72N Started. Waiting for Gateway...")
	log.Printf("   Socket: %s", SocketPath)
	log.Printf("   Max Frame Size: %d bytes", MaxFrameSize)

	// Channel to signal graceful shutdown
	done := make(chan struct{})

	// Handle Ctrl+C gracefully
	sigChan := make(chan os.Signal, SignalBufferSize)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-sigChan
		log.Println("\nShutting down Virtual RX72N...")
		// Signal main loop to stop
		close(done)
		// Close listener to unblock Accept()
		listener.Close()
		if err := os.Remove(SocketPath); err != nil {
			log.Printf("Warning: failed to remove socket: %v", err)
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

			log.Printf("Connection error: %v", err)
			continue
		}
		log.Printf("Gateway connected from %v", conn.RemoteAddr())

		// Handle each connection in a goroutine
		go handleConnection(conn)
	}
}

// handleConnection processes messages from a single Gateway connection.
func handleConnection(conn net.Conn) {
	defer conn.Close()
	defer log.Println("Gateway disconnected")

	buffer := make([]byte, MaxFrameSize)

	for {
		// Read data from Gateway
		// Set a read deadline (readDeadlineTimeout) to avoid indefinite blocking if the peer stalls.
		// Use a reasonable timeout so the simulator can continue serving other
		// peers or handle periodic events.
		if err := conn.SetReadDeadline(time.Now().Add(readDeadlineTimeout)); err != nil {
			log.Printf("Failed to set read deadline: %v", err)
			// Continue without deadline if setting failed
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
			log.Printf("Read error: %v", err)
			return
		}

		rxData := buffer[:n]

		// --- FRAME PARSING AND RESPONSE GENERATION ---

		// 1. Decode the frame
		decoder := frame.NewDecoder()
		decodedFrame, err := decoder.Decode(rxData)
		if err != nil {
			log.Printf("Frame decode error: %v", err)
			continue
		}

		log.Printf("Received frame: seq=%d, type=%s, flags=%d, payload_len=%d",
			decodedFrame.Header.Sequence, decodedFrame.Header.Type.String(),
			decodedFrame.Header.Flags, len(decodedFrame.Payload))

		// 2. Parse the protobuf payload
		var wireMsg starv1.WireMessage
		if err := proto.Unmarshal(decodedFrame.Payload, &wireMsg); err != nil {
			log.Printf("Protobuf unmarshal error: %v", err)
			continue
		}

		// 3. Process the command and generate response
		responseMsg := processCommand(&wireMsg)

		// 4. Encode the response
		responsePayload, err := proto.Marshal(responseMsg)
		if err != nil {
			log.Printf("Protobuf marshal error: %v", err)
			continue
		}

		// 5. Encode the response frame (increment sequence)
		encoder := frame.NewEncoder()
		responseFrame := &frame.Frame{
			Header: frame.Header{
				Sequence: decodedFrame.Header.Sequence + 1,
				Length:   uint16(len(responsePayload)),
				Type:     frame.FrameTypeResponse,
				Flags:    frame.FlagNone,
			},
			Payload: responsePayload,
		}

		txData, err := encoder.Encode(responseFrame)
		if err != nil {
			log.Printf("Frame encode error: %v", err)
			continue
		}

		log.Printf("Sending frame: seq=%d, type=%s, payload_len=%d",
			responseFrame.Header.Sequence, responseFrame.Header.Type.String(), len(responsePayload))

		// 6. Send the response
		if _, err := conn.Write(txData); err != nil {
			log.Printf("Write error: %v", err)
			return
		}
	}
}

// processCommand handles incoming commands and generates appropriate responses.
func processCommand(wireMsg *starv1.WireMessage) *starv1.WireMessage {
	// Check which type of command was received
	switch payload := wireMsg.Payload.(type) {
	case *starv1.WireMessage_VelocityCommand:
		log.Printf("VelocityCommand: FL=%.2f, FR=%.2f, BL=%.2f, BR=%.2f m/s",
			payload.VelocityCommand.FrontLeftVelocityMps,
			payload.VelocityCommand.FrontRightVelocityMps,
			payload.VelocityCommand.BackLeftVelocityMps,
			payload.VelocityCommand.BackRightVelocityMps)

		// Generate telemetry response with simulated data
		return generateTelemetryResponse()

	case *starv1.WireMessage_EmergencyStopCommand:
		log.Printf("EmergencyStopCommand received: %s", payload.EmergencyStopCommand.Reason)
		return generateEmergencyStopResponse()

	default:
		log.Printf("Unknown command type, generating default telemetry")
		return generateTelemetryResponse()
	}
}

// generateTelemetryResponse creates a realistic telemetry data response.
func generateTelemetryResponse() *starv1.WireMessage {
	return &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: &starv1.TelemetryData{
				TimestampUs:        time.Now().UnixMicro(),
				BatteryPercent:     85.0, // 85% state of charge
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
			},
		},
	}
}

// generateEmergencyStopResponse creates a telemetry response indicating emergency stop.
func generateEmergencyStopResponse() *starv1.WireMessage {
	return &starv1.WireMessage{
		Payload: &starv1.WireMessage_TelemetryData{
			TelemetryData: &starv1.TelemetryData{
				TimestampUs:        time.Now().UnixMicro(),
				BatteryPercent:     85.0,
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
