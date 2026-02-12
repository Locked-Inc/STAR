// Package app encapsulates the main application logic for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package app

import (
	"context"
	"fmt"
	"log"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/link"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/server"
	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
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

type Servers struct {
	HTTPServer *http.Server
	GRPCServer *grpc.Server
}

// serviceSet holds initialized service instances for use in gRPC server registration.
// IMPORTANT: Make sure this struct is updated whenever new proto services are added or modified to ensure proper initialization and registration.
type serviceSet struct {
	motorControl  *service.MotorControlService
	telemetry     *service.TelemetryService
	battery       *service.BatteryService
	configuration *service.ConfigurationService
	firmware      *service.FirmwareService
}

// Run starts the STAR Gateway application with the given configuration.
//
// This is the main entry point that orchestrates the complete application lifecycle:
//  1. Logger initialization
//  2. Transport layer setup (SPI/USB with automatic failover)
//  3. Message dispatcher initialization
//  4. Service layer initialization (gRPC and HTTP/WebSocket)
//  5. Server startup (background goroutines)
//  6. Signal handling (graceful shutdown on SIGINT/SIGTERM)
//
// The function blocks until a shutdown signal is received or a fatal error occurs.
func Run(ctx context.Context, config Config) error {
	// Create structured logger
	logger := slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	}))

	logger.Info("Starting STAR Gateway")

	// Initialize transport manager with validation and failover
	tm, err := initTransportManager(ctx, config)
	if err != nil {
		return fmt.Errorf("transport manager initialization failed: %w", err)
	}
	defer tm.Stop()

	// Initialize message dispatcher
	// TODO: Implement initDispatcher to return dispatcher instance
	if err := initDispatcher(ctx); err != nil {
		return fmt.Errorf("dispatcher initialization failed: %w", err)
	}

	// TODO: Get dispatcher instance from initDispatcher (currently returns error only)
	// For now, pass nil dispatcher - services will need to handle this gracefully
	var disp dispatcher.Dispatcher = nil

	// Initialize service layer
	services, err := initServices(ctx, tm, disp, logger)
	if err != nil {
		return fmt.Errorf("services initialization failed: %w", err)
	}

	// Initialize servers struct
	servers := &Servers{}

	// Start gRPC server (non-blocking)
	if err := startGRPCServer(ctx, servers, services, logger); err != nil {
		return fmt.Errorf("gRPC server startup failed: %w", err)
	}

	// Start HTTP server (non-blocking)
	if err := startHTTPServer(ctx, servers, logger); err != nil {
		return fmt.Errorf("HTTP server startup failed: %w", err)
	}

	// Setup signal handling
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)

	// Block until signal received or context cancelled
	select {
	case sig := <-sigChan:
		logger.Info("Received shutdown signal", slog.String("signal", sig.String()))
	case <-ctx.Done():
		logger.Info("Context cancelled, shutting down")
	}

	// Graceful shutdown is automatic via context cancellation in RunHTTPServer/RunGRPCServer
	logger.Info("Gateway shutdown complete")
	return nil
}

// initTransportManager initializes the TransportManager based on the provided configuration.
// This function orchestrates the initialization process by validating configuration,
// creating the transport manager, initializing available transports, and starting the manager.
//
// CRITICAL: TransportManager owns the single SessionState instance. All transports must use
// tm.GetSessionState() to ensure sequence continuity during failover (prevents Handoff Problem).
func initTransportManager(ctx context.Context, appConfig Config) (*manager.TransportManager, error) {
	// Create manager config from app config
	mgrConfig := &manager.Config{
		Mode: appConfig.TransportMode,
	}

	// Validate config or fallback to default
	mgrConfig = validateOrUseDefault(mgrConfig)

	// Create transport manager (internally creates SessionState)
	tm := manager.NewTransportManager(mgrConfig)

	// Initialize transports using manager's SessionState (passed via tm)
	if err := initializeTransports(ctx, appConfig, tm); err != nil {
		return nil, err
	}

	// Validate required transports based on mode
	if err := validateTransportsRegistered(tm, appConfig.TransportMode); err != nil {
		return nil, err
	}

	// Start manager (selects initial transport, performs reset handshake)
	if err := tm.Start(ctx); err != nil {
		return nil, fmt.Errorf("failed to start transport manager: %w", err)
	}

	return tm, nil
}

// validateOrUseDefault validates the configuration and returns it if valid,
// or returns the default configuration if validation fails.
func validateOrUseDefault(config *manager.Config) *manager.Config {
	// Validate configuration
	if err := config.Validate(); err != nil {
		// TODO: Log validation failure and use default configuration
		log.Printf("Config validation failed: %v. Using default configuration.", err)
		return manager.DefaultConfig()
	}
	return config
}

// initializeTransports initializes all available transports based on configuration.
// This function attempts to initialize SPI/Socket (for simulation) and USB transports.
// Transport initialization is non-fatal unless explicitly required by TransportMode.
//
// CRITICAL: All transports MUST use the shared SessionState from TransportManager
// (via tm.GetSessionState()) to ensure sequence continuity during failover.
func initializeTransports(ctx context.Context, appConfig Config, tm *manager.TransportManager) error {
	// Get shared session state from manager (single source of truth)
	session := tm.GetSessionState()

	// SPI/Socket initialization (non-fatal unless force-spi mode)
	if appConfig.SimulationMode {
		// Simulation mode: use socket transport
		socketTransport, err := createSocketTransport(ctx, appConfig.SocketPath)
		if err != nil {
			log.Printf("Socket transport initialization failed: %v", err)
		} else {
			// Wrap socket in CDC link (socket protocol matches CDC)
			socketLink, err := link.NewCDCLink(socketTransport, session)
			if err != nil {
				log.Printf("Socket link creation failed: %v", err)
			} else {
				// Register as SPI transport (simulation mode uses socket instead of SPI)
				tm.RegisterTransport(manager.TransportNameSPI, socketLink, manager.PrioritySPI)
				log.Printf("Registered socket transport (simulation mode)")
			}
		}
	} else {
		// Production mode: use SPI transport
		spiTransport, err := createSPITransport(ctx)
		if err != nil {
			log.Printf("SPI transport initialization failed: %v", err)
		} else {
			// Wrap in SPI link layer with shared session state
			spiLink, err := createSPILink(ctx, spiTransport, session)
			if err != nil {
				log.Printf("SPI link creation failed: %v", err)
			} else {
				tm.RegisterTransport(manager.TransportNameSPI, spiLink, manager.PrioritySPI)
				log.Printf("Registered SPI transport")
			}
		}
	}

	// USB CDC initialization (non-fatal unless force-usb mode)
	usbLink, err := createUSBLink(ctx, session)
	if err != nil {
		log.Printf("USB CDC initialization failed: %v", err)
		// If force-usb mode, this is fatal
		if appConfig.TransportMode == manager.ModeForceUSB {
			return fmt.Errorf("force-USB mode requires USB transport: %w", err)
		}
	} else {
		tm.RegisterTransport(manager.TransportNameUSB, usbLink, manager.PriorityUSB)
		log.Printf("Registered USB transport")
	}

	return nil
}

// validateTransportsRegistered ensures that we have at least one valid transport
// registered with the TransportManager and that force-mode requirements are satisfied.
func validateTransportsRegistered(tm *manager.TransportManager, mode manager.TransportMode) error {
	// Get list of registered transports from manager
	available := tm.GetAvailableTransports()

	// Ensure at least one transport is available
	if len(available) == 0 {
		return fmt.Errorf("no transports available")
	}

	// Validate force-mode requirements
	if mode == manager.ModeForceUSB {
		hasUSB := false
		for _, name := range available {
			if name == manager.TransportNameUSB {
				hasUSB = true
				break
			}
		}
		if !hasUSB {
			return fmt.Errorf("force-USB mode enabled but USB transport unavailable")
		}
	}

	if mode == manager.ModeForceSPI {
		hasSPI := false
		for _, name := range available {
			if name == manager.TransportNameSPI {
				hasSPI = true
				break
			}
		}
		if !hasSPI {
			return fmt.Errorf("force-SPI mode enabled but SPI transport unavailable")
		}
	}

	return nil
}

// createSocketTransport creates a SocketTransport for simulation mode.
// The transport is opened and ready for use upon successful return.
func createSocketTransport(ctx context.Context, socketPath string) (transport.Device, error) {
	// TODO: Verify this function is correct and works properly
	// Create SocketTransport with the provided socket path
	socketTransport := transport.NewSocketTransport(socketPath)

	// Open the transport connection
	if err := socketTransport.Open(); err != nil {
		return nil, err
	}

	return socketTransport, nil
}

// createSPITransport creates an SPITransport for production mode.
// The transport is configured with appropriate device parameters and opened.
func createSPITransport(ctx context.Context) (transport.Device, error) {
	// TODO: Create SPITransport with the appropriate device parameters
	// (SPI bus, chip select, speed, mode)
	// TODO: Open the transport connection
	// TODO: Return the opened transport
	return nil, fmt.Errorf("SPI transport not yet implemented")
}

// createSPILink creates an SPILink using the provided SPI transport and shared session state.
// This function wraps the SPI transport with HARQ protocol support.
//
// Returns the SPI link (harq.HARQ) or an error if link creation fails.
func createSPILink(ctx context.Context, spiTransport transport.Device, session *manager.SessionState) (harq.HARQ, error) {
	// Create SPI link with default config (nil = use defaults)
	spiLink, err := link.NewSPILink(spiTransport, session, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create SPI link: %w", err)
	}

	return spiLink, nil
}

// createLegacySPILink creates a legacy ChaseCombining HARQ link.
// This is used as a fallback when the standard SPI link creation fails.
//
// NOTE: Legacy HARQ implementation is deprecated and not currently supported.
func createLegacySPILink(ctx context.Context, transport transport.Transport, appConfig Config) error {
	return fmt.Errorf("legacy SPI link not implemented")
}

// createUSBLink creates a USB CDC link using the provided session state.
// Returns the USB link (harq.HARQ) or an error if initialization fails.
//
// The CDC transport is opened and wrapped in a CDCLink with the shared session state.
// This ensures sequence continuity during transport switching (prevents Handoff Problem).
func createUSBLink(ctx context.Context, session *manager.SessionState) (harq.HARQ, error) {
	// Create CDC transport with default config
	cdcTransport := transport.NewCDCTransport(transport.DefaultCDCConfig())

	// Open the CDC device
	if err := cdcTransport.Open(); err != nil {
		return nil, fmt.Errorf("failed to open CDC transport: %w", err)
	}

	// Wrap in link layer with shared session state
	cdcLink, err := link.NewCDCLink(cdcTransport, session)
	if err != nil {
		cdcTransport.Close() // Cleanup on error
		return nil, fmt.Errorf("failed to create CDC link: %w", err)
	}

	return cdcLink, nil
}

// initDispatcher initializes the message dispatcher for routing protocol messages.
// The dispatcher handles incoming messages from transports and routes them to
// appropriate service handlers.
func initDispatcher(ctx context.Context) error {
	// TODO: Create message dispatcher

	// TODO: Register message handlers for different protocol message types

	// TODO: Return success or error
	return nil
}

// initServices initializes gRPC and gateway services.
// This sets up the service layer that handles business logic and API endpoints.
// initServices initializes all gRPC service implementations.
// Returns serviceSet containing initialized services for gRPC server registration.
func initServices(ctx context.Context, harqHandler harq.HARQ, disp dispatcher.Dispatcher, logger *slog.Logger) (*serviceSet, error) {
	return &serviceSet{
		motorControl:  service.NewMotorControlService(harqHandler, disp, logger),
		telemetry:     service.NewTelemetryService(ctx, harqHandler, disp, logger),
		battery:       service.NewBatteryService(ctx, harqHandler, disp, logger),
		configuration: service.NewConfigurationService(harqHandler, disp, logger),
		firmware:      service.NewFirmwareService(),
	}, nil
}

// startGRPCServer starts the gRPC server on the configured port.
// The server listens for incoming gRPC connections and serves registered services.
// registerGRPCServices registers all gRPC service implementations with the server.
// This function must be updated whenever new services are added to the protobuf definitions.
func registerGRPCServices(srv *grpc.Server, services *serviceSet) error {
	starv1.RegisterMotorControlServiceServer(srv, services.motorControl)
	starv1.RegisterTelemetryServiceServer(srv, services.telemetry)
	starv1.RegisterBatteryManagementServiceServer(srv, services.battery)
	starv1.RegisterConfigurationServiceServer(srv, services.configuration)
	starv1.RegisterFirmwareUpdateServiceServer(srv, services.firmware)
	return nil
}

// startGRPCServer starts the gRPC server on the configured port with registered services.
func startGRPCServer(ctx context.Context, servers *Servers, services *serviceSet, logger *slog.Logger) error {
	// Configure gRPC server with service registration callback
	config := &server.GRPCConfig{
		ListenAddr:     grpcListenPort,
		MaxMessageSize: grpcMaxMsgSize,
		ServiceRegistrar: func(srv *grpc.Server) error {
			return registerGRPCServices(srv, services)
		},
	}

	// Create server (registers services)
	grpcSrv, err := server.NewGRPCServer(config, logger)
	if err != nil {
		return fmt.Errorf("failed to create gRPC server: %w", err)
	}
	servers.GRPCServer = grpcSrv

	// Start server (non-blocking)
	errChan, err := server.RunGRPCServer(ctx, grpcSrv, grpcListenPort, logger)
	if err != nil {
		return fmt.Errorf("failed to start gRPC server: %w", err)
	}

	// Monitor for errors in background
	go func() {
		if err := <-errChan; err != nil {
			logger.Error("gRPC server error", slog.String("error", err.Error()))
		}
	}()

	return nil
}

// startHTTPServer starts the HTTP/WebSocket server on the configured port.
// The server handles HTTP requests and WebSocket connections for the UI.
func startHTTPServer(ctx context.Context, servers *Servers, logger *slog.Logger) error {
	// Create HTTP router
	mux := http.NewServeMux()

	// TODO: Register controller handler at /ws/controller
	// TODO: Register static file handler
	// TODO: Register API endpoints

	// Configure HTTP server
	config := &server.HTTPConfig{
		ListenAddr:   httpListenPort,
		ReadTimeout:  httpReadTimeout,
		WriteTimeout: httpWriteTimeout,
		Handler:      mux,
	}

	// Create server
	httpSrv, err := server.NewHTTPServer(config, logger)
	if err != nil {
		return fmt.Errorf("failed to create HTTP server: %w", err)
	}
	servers.HTTPServer = httpSrv

	// Start server (non-blocking)
	errChan, err := server.RunHTTPServer(ctx, httpSrv, logger)
	if err != nil {
		return fmt.Errorf("failed to start HTTP server: %w", err)
	}

	// Monitor for errors in background
	go func() {
		if err := <-errChan; err != nil {
			logger.Error("HTTP server error", slog.String("error", err.Error()))
		}
	}()

	return nil
}
