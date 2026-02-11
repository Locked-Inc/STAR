// Package app encapsulates the main application logic for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package app

import (
	"context"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
)

const (
	// httpShutdownTimeout is the maximum time allowed for HTTP server graceful shutdown.
	httpShutdownTimeout = 5 * time.Second

	// grpcShutdownTimeout is the maximum time allowed for gRPC server graceful shutdown.
	grpcShutdownTimeout = 5 * time.Second

	// grpcListenPort is the TCP port for gRPC services.
	grpcListenPort = ":50051"

	// httpListenPort is the TCP port for HTTP/WebSocket services.
	httpListenPort = ":8080"

	// httpReadTimeout is the maximum duration for reading the entire HTTP request.
	httpReadTimeout = 10 * time.Second

	// httpWriteTimeout is the maximum duration for writing the HTTP response.
	httpWriteTimeout = 10 * time.Second

	// grpcMaxMsgSize is the maximum message size for gRPC (10 MB).
	grpcMaxMsgSize = 10 * 1024 * 1024

	// simulationHARQTimeout is the timeout used for HARQ operations in simulation mode.
	simulationHARQTimeout time.Duration = 500 * time.Millisecond
)

// Config holds the application configuration.
type Config struct {
	SimulationMode bool
	SocketPath     string
	// "auto", "prefer-usb", "force-usb", "force-spi"
	TransportMode manager.TransportMode
}

// Run starts the STAR Gateway application with the given configuration.
func Run(ctx context.Context, config Config) error {
	// Create a logger
	// Initialize the transport manager
	// Init Dispatcher
	// init Services
	// Setup deferred cleanups LIFO
	// Start Servers (HTTP, gRPC)
	// Wait for signals
	// Shutdown servers gracefully
	return nil
}

// initTransportManager initializes the TransportManager based on the provided configuration.
func initTransportManager(ctx context.Context, config manager.Config) (*manager.TransportManager, error) {
	// Create transportmanager config
	tm := manager.NewTransportManager()
	// Create transportmanager
	// Get shared session state

	// SPI/Socket Initialization (non fatal)
	// If Simulation mode...
	// else (production)
	// Create SPITransposrt
	// success then create link
	// faiklure log warning set link as nil
	// regsiter if succesul

	// USB CDC INit (non fatal if not forced)
	// Create usb link
	// success then reigster trransposrt
	// failure then log warning, check if mode force usb is on then return error
	// else skip usb

	// Validate we have atelast one valid transport success
	// count reigstered tranpsorts
	// if 0 return no transposrt avaiable
	// If ModeForceUSB && !usbLink: return error
	// If ModeForceSPI && !spiLink: return error

	// tm.start

	// return invidiual clean up functions
	// tm clean up stop
	// usb clean up close
	// spi clean up close
	return nil, nil
}

// createSocketTransport creates a SocketTransport for simulation mode.
func createSocketTransport(ctx context.Context, socketPath string) (transport.Device, error) {
	// Create SocketTransport with the provided socket path
	// Open the transport connection
	// Return the transport and a cleanup function to close it
	return nil, nil
}

// createSPITransport creates an SPITransport for production mode.
func createSPITransport(ctx context.Context) (transport.Device, error) {
	// Ensure we use Legacy ChaseCombining HARQ
	// Create SPITransport with the appropriate device parameters
	// Open the transport connection
	// Return the transport and a cleanup function to close it
	return nil, nil
}

// createSPILink creates an SPILink using the provided SPI transport and shared session state.
func createSPILink(ctx context.Context, deviceTransport transport.Device, cfg manager.Config) error {
	// Type assert to transport.Transport
	// Create SPILink with session
	// Fallback to legacy ChaseCombining on error
	return nil
}

// createLegacySPILink creates a legacy ChaseCombining HARQ link.
func createLegacySPILink(ctx context.Context, transport transport.Transport, cfg manager.Config) error {
	// Legacy ChaseCombining HARQ
	return nil
}

// createUSBLink creates a USB CDC link using the provided session state.
func createUSBLink(ctx context.Context) error {
	// Create CDCTransport
	// cdcTransport.Open()
	// Create CDCLink with session
	// Return link or error
	return nil
}

// initDispatcher initializes the message dispatcher.
func initDispatcher(ctx context.Context) error {
	// Standard dispatcher initialization
	return nil
}

// initServices initializes gRPC and gateway services.
func initServices(ctx context.Context) error {
	// Standard service initialization
	return nil
}

// startGRPCServer starts the gRPC server.
func startGRPCServer(ctx context.Context) error {
	// Start gRPC listener
	return nil
}

// startHTTPServer starts the HTTP/WebSocket server.
func startHTTPServer(ctx context.Context) error {
	// Start HTTP/WebSocket server
	return nil
}

// shutdownServers gracefully shuts down HTTP and gRPC servers.
func shutdownServers(ctx context.Context) error {
	// Graceful shutdown sequence
	return nil
}
