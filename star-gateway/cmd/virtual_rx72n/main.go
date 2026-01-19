// Virtual RX72N - Hardware-in-the-Loop Simulator
//
// This program simulates the RX72N motor controller firmware for testing
// and development without physical hardware. It listens on a Unix domain
// socket and responds to Gateway commands.
//
// Usage:
//   go run ./cmd/virtual_rx72n/
//
// The simulator will:
//   1. Create a Unix socket at /tmp/star_rx72n.sock
//   2. Listen for connections from the Gateway
//   3. Echo back modified data to prove it's the simulator
//   4. Handle Ctrl+C gracefully
//
// STAR Project - Texas A&M University
// January 2026
package main

import (
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
)

const (
	// SocketPath is the Unix domain socket path.
	SocketPath = "/tmp/star_rx72n.sock"

	// MaxFrameSize is the maximum expected frame size.
	MaxFrameSize = 2048
)

func main() {
	// Cleanup old socket
	if err := os.Remove(SocketPath); err != nil && !os.IsNotExist(err) {
		log.Printf("Warning: failed to remove old socket: %v", err)
	}

	// Create Unix domain socket listener
	listener, err := net.Listen("unix", SocketPath)
	if err != nil {
		log.Fatalf("Failed to create virtual device: %v", err)
	}
	defer listener.Close()

	log.Println("🤖 Virtual RX72N Started. Waiting for Gateway...")
	log.Printf("   Socket: %s", SocketPath)

	// Handle Ctrl+C gracefully
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-sigChan
		log.Println("\n🛑 Shutting down Virtual RX72N...")
		listener.Close()
		os.Remove(SocketPath)
		os.Exit(0)
	}()

	// Accept connections in a loop
	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("Connection error: %v", err)
			continue
		}
		log.Printf("✅ Gateway connected from %v", conn.RemoteAddr())

		// Handle each connection in a goroutine
		go handleConnection(conn)
	}
}

// handleConnection processes messages from a single Gateway connection.
func handleConnection(conn net.Conn) {
	defer conn.Close()
	defer log.Println("❌ Gateway disconnected")

	buffer := make([]byte, MaxFrameSize)

	for {
		// Read data from Gateway
		n, err := conn.Read(buffer)
		if err != nil {
			if err.Error() == "EOF" {
				return // Connection closed normally
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

		// For demonstration, we just echo back modified data
		// indicating "I am a robot"
		log.Printf("📥 Received %d bytes: %x", n, rxData)

		// Simulate processing time (1ms)
		// time.Sleep(1 * time.Millisecond)

		// 3. Send Telemetry Back
		// In real SPI, you send bytes continuously.
		// Here, we write back the response.
		txData := make([]byte, n)
		copy(txData, rxData)

		// Example: Flip first byte to prove it's the simulator
		if len(txData) > 0 {
			txData[0] = 0xFF
		}

		log.Printf("📤 Sending %d bytes: %x", len(txData), txData)

		_, err = conn.Write(txData)
		if err != nil {
			log.Printf("Write error: %v", err)
			return
		}
	}
}
