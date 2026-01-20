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

		// --- LOGIC SIMULATION HERE ---

		// 1. Deserialize the frame (mocking HARQ/Protobuf unwrapping)
		// frame := decodeFrame(rxData)

		// 2. Determine response based on input
		// responseFrame := generateResponse(frame)

		// For demonstration, we echo back modified data
		// indicating "I am the Virtual RX72N"
		log.Printf("Received %d bytes: %x", n, rxData)

		// Simulate processing time (optional)
		// time.Sleep(1 * time.Millisecond)

		// 3. Send Telemetry Back
		// In real SPI, you send bytes continuously.
		// Here, we write back the response.
		txData := make([]byte, n)
		copy(txData, rxData)

		// Flip MSB of first byte to prove it's the simulator
		if len(txData) > 0 {
			txData[0] = SimulatorMarker
		}

		log.Printf("Sending %d bytes: %x", len(txData), txData)

		if _, err := conn.Write(txData); err != nil {
			log.Printf("Write error: %v", err)
			return
		}
	}
}
