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
	"net"
	"net/http"
	"os"
	"os/signal"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/controller"
	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/link"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
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

// Shutdownable defines the interface for services that require graceful shutdown.
type Shutdownable interface {
	Shutdown()
}

// Config holds the application configuration.
type Config struct {
	SimulationMode bool
	SocketPath     string
	// "auto", "prefer-usb", "force-usb", "force-spi"
	TransportMode manager.TransportMode
}

// Run starts the gateway application.
func Run(ctx context.Context, cfg Config) error {
	// ========================================
	// Layer 1: Transport (SPI or Socket for simulation)
	// ========================================
	deviceTransport, transportCleanup, err := initTransport(cfg)
	if err != nil {
		return err
	}
	// NOTE: transportCleanup is deferred later, after dispatcher init, to ensure
	// transport closes BEFORE dispatcher cleanup (to interrupt pending I/O)

	// ========================================
	// Layer 2-4: Protocol Stack (HARQ with TransportManager)
	// ========================================
	// All transport modes now use TransportManager for reliability features:
	// - Shared SessionState (prevents sequence resets during transport switching)
	// - Health monitoring (latency, failures, packet loss tracking)
	// - Heartbeat detection (PING/PONG for connectivity monitoring)
	// - Hot-plug support (automatic USB device detection)
	// - Reset handshake (three-way handshake with barrier for startup sync)
	// - Automatic failover (switches transports on failure)
	harqHandler, tmCleanup, err := initTransportManager(ctx, cfg, deviceTransport)
	if err != nil {
		return fmt.Errorf("failed to initialize HARQ: %w", err)
	}
	// NOTE: tmCleanup is deferred later, after dispatcher init

	// ========================================
	// Structured Logger
	// ========================================
	logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	}))

	// ========================================
	// Layer 4.5: Message Dispatcher
	// ========================================
	log.Printf("Initializing message dispatcher...")
	msgDispatcher, dispatcherCleanup, err := initDispatcher(ctx, harqHandler, logger)
	if err != nil {
		return err
	}
	defer dispatcherCleanup()

	// Defer TransportManager and Transport cleanup AFTER dispatcher cleanup to ensure correct shutdown order:
	// LIFO execution order (deferred first = executes last):
	// 1. Transport closes (deferred last, executes first) - socket closes, interrupts any pending I/O
	// 2. TransportManager stops (deferred second, executes second) - background goroutines exit
	// 3. Dispatcher stops (deferred first, executes last) - receive loop exits due to context cancel + closed transport
	if tmCleanup != nil {
		defer tmCleanup()
	}
	defer transportCleanup()

	// ========================================
	// Layer 5: gRPC Services
	// ========================================
	log.Printf("Initializing gRPC services...")
	// Use main context so services are automatically cancelled when main context is cancelled
	grpcServer, gatewaySvc, shutdownRegistry, err := initServices(ctx, harqHandler, msgDispatcher, logger)
	if err != nil {
		return err
	}

	// Error channel for goroutine failures
	errChan := make(chan error, 1)

	// Start gRPC server
	if err := startGRPCServer(grpcServer, errChan); err != nil {
		return err
	}

	// Start HTTP server
	httpServer := startHTTPServer(gatewaySvc, errChan)

	// Setup signal handling
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt)

	// Wait for context cancellation, interrupt, or error
	select {
	case <-ctx.Done():
		log.Println("Context cancelled, shutting down...")
	case <-sigChan:
		log.Println("Interrupt received, shutting down...")
	case err := <-errChan:
		log.Printf("Server error: %v, shutting down...", err)
	}

	// Shutdown servers and services
	shutdownServers(httpServer, grpcServer, shutdownRegistry)

	// Deferred cleanups will execute in reverse order (LIFO):
	// 1. Transport closes (interrupts any pending I/O in dispatcher receive loop)
	// 2. TransportManager stops (background goroutines exit)
	// 3. Dispatcher stops (receive loop exits due to context cancel + closed transport)
	log.Println("All servers exited gracefully")
	return nil
}

func initTransport(cfg Config) (transport.Device, func(), error) {
	var deviceTransport transport.Device

	cleanup := func() {
		log.Println("Transport cleanup starting...")
		if deviceTransport != nil {
			log.Printf("Closing transport (isOpen=%v)...", deviceTransport.IsOpen())
			if err := deviceTransport.Close(); err != nil {
				log.Printf("Transport close error: %v", err)
			} else {
				log.Println("Transport closed successfully")
			}
		} else {
			log.Println("Transport is nil, skipping close")
		}
	}

	if cfg.SimulationMode {
		socketPath := cfg.SocketPath
		if socketPath == "" {
			socketPath = transport.DefaultSocketPath
		}
		log.Println("WARNING: RUNNING IN SIMULATION MODE (Virtual RX72N)")
		log.Printf("    Connecting to socket: %s", socketPath)
		socketTransport := transport.NewSocketTransport(socketPath)
		if err := socketTransport.Open(); err != nil {
			return nil, nil, fmt.Errorf("failed to open socket transport: %w", err)
		}
		deviceTransport = socketTransport
	} else {
		log.Println("Initializing SPI Transport")
		spiTransport := transport.NewSPITransport(transport.DefaultConfig())
		if err := spiTransport.Open(); err != nil {
			return nil, nil, fmt.Errorf("failed to open SPI transport: %w", err)
		}
		deviceTransport = spiTransport
	}

	return deviceTransport, cleanup, nil
}

func initTransportManager(ctx context.Context, cfg Config, deviceTransport transport.Device) (harq.HARQ, func(), error) {
	tmConfig := manager.DefaultConfig()
	if cfg.TransportMode != "" {
		// Validate transport mode before using it
		if !cfg.TransportMode.IsValid() {
			return nil, nil, fmt.Errorf("invalid transport mode %q (valid: %s, %s, %s, %s)",
				cfg.TransportMode, manager.ModeAuto, manager.ModePreferUSB, manager.ModeForceUSB, manager.ModeForceSPI)
		}
		tmConfig.Mode = cfg.TransportMode
	}

	tm := manager.NewTransportManager(tmConfig)

	// Identify the transport type and register it
	var legacyTransport transport.Transport
	switch t := deviceTransport.(type) {
	case *transport.SPITransport:
		legacyTransport = t
	case *transport.SocketTransport:
		legacyTransport = t
	default:
		return nil, nil, fmt.Errorf("unknown transport type for TransportManager")
	}

	// Get shared session state from TransportManager (CRITICAL FIX #1)
	session := tm.GetSessionState()

	// Wrap SPI transport in a full HARQ/Link layer
	spiLink := createSPILink(legacyTransport, cfg, session)
	tm.RegisterTransport("spi", spiLink, manager.PrioritySPI)

	// Register USB CDC (if mode allows and device is available)
	// CRITICAL FIX #2: Smart drain logic enables fast failover on USB disconnect
	if tmConfig.Mode != manager.ModeForceSPI {
		if usbLink, err := createUSBLink(session); err == nil {
			tm.RegisterTransport("usb", usbLink, manager.PriorityUSB)
			log.Printf("USB CDC registered successfully")
		} else {
			log.Printf("USB CDC not available: %v (will use SPI only)", err)

			// In force-usb mode, return error if USB unavailable
			if tmConfig.Mode == manager.ModeForceUSB {
				return nil, nil, fmt.Errorf("USB CDC required by mode %s but unavailable: %w", tmConfig.Mode, err)
			}
		}
	}

	if err := tm.Start(ctx); err != nil {
		return nil, nil, err
	}

	cleanup := func() {
		if err := tm.Stop(); err != nil {
			log.Printf("TransportManager shutdown error: %v", err)
		}
	}

	return tm, cleanup, nil
}

// createSPILink wraps a transport in SPILink with full HARQ protocol.
// Phase 2: ACK/NACK handshake, retransmission, and FEC with Chase Combining.
// Uses shared SessionState to prevent sequence desynchronization during transport switching.
func createSPILink(t transport.Transport, cfg Config, session *manager.SessionState) harq.HARQ {
	// Type assert to Device interface (SPITransport implements both Transport and Device)
	deviceTransport, ok := t.(transport.Device)
	if !ok {
		// Fallback to legacy ChaseCombining for non-Device transports
		log.Printf("WARN: Transport doesn't implement Device interface, using legacy ChaseCombining")
		return createLegacySPILink(t, cfg)
	}

	// Create SPILink with shared SessionState (CRITICAL for transport switching)
	// Phase 2: Enable FEC + Chase Combining for ~10x BER improvement
	spiLink, err := link.NewSPILink(deviceTransport, session, &link.SPILinkConfig{
		EnableFEC:  true, // Phase 2: Enable FEC + Chase Combining
		MaxRetries: harq.DefaultMaxRetries,
		ACKTimeout: harq.DefaultTimeout,
	})
	if err != nil {
		log.Printf("ERROR: Failed to create SPILink: %v, falling back to legacy", err)
		return createLegacySPILink(t, cfg)
	}

	log.Printf("SPILink created successfully (Phase 2: ACK/NACK + FEC + Chase Combining)")
	return spiLink
}

// createLegacySPILink creates the legacy ChaseCombining HARQ for fallback.
// This is kept for compatibility during migration.
func createLegacySPILink(t transport.Transport, cfg Config) harq.HARQ {
	frameEncoder := frame.NewEncoder()
	frameDecoder := frame.NewDecoder()
	fecEncoder := fec.NewConvolutionalEncoder()
	fecDecoder := fec.NewViterbiDecoder()

	harqConfig := harq.DefaultConfig()
	if cfg.SimulationMode {
		// Relax timeout for simulation environment
		harqConfig.Timeout = simulationHARQTimeout
	}

	log.Printf("WARN: Using legacy ChaseCombining HARQ for SPI")
	return harq.NewChaseCombining(
		harqConfig,
		t,
		frameEncoder,
		frameDecoder,
		fecEncoder,
		fecDecoder,
	)
}

// createUSBLink creates a lightweight CDC link with shared session state.
// CRITICAL FIX #1: Uses shared SessionState to prevent sequence desynchronization.
// CRITICAL FIX #7: sendWithTimeout prevents blocked writes on USB disconnect.
func createUSBLink(session *manager.SessionState) (harq.HARQ, error) {
	// Create USB CDC transport with default configuration
	cdcConfig := transport.DefaultCDCConfig()
	cdcTransport := transport.NewCDCTransport(cdcConfig)

	// Open the USB CDC device (/dev/ttyACM0)
	if err := cdcTransport.Open(); err != nil {
		return nil, fmt.Errorf("failed to open USB CDC: %w", err)
	}

	// Wrap in lightweight CDCLink with SHARED session state (critical!)
	// Both USB and SPI MUST share the same SessionState to prevent
	// sequence resets during transport switching (Handoff Problem).
	cdcLink, err := link.NewCDCLink(cdcTransport, session)
	if err != nil {
		cdcTransport.Close()
		return nil, fmt.Errorf("failed to create CDC link: %w", err)
	}

	return cdcLink, nil
}

func initDispatcher(ctx context.Context, harqHandler harq.HARQ, logger *slog.Logger) (dispatcher.Dispatcher, func(), error) {
	msgDispatcher, err := dispatcher.NewDispatcher(harqHandler, logger, nil)
	if err != nil {
		return nil, nil, err
	}

	dispatcherCtx, dispatcherCancel := context.WithCancel(ctx)
	if err := msgDispatcher.Start(dispatcherCtx); err != nil {
		dispatcherCancel()
		return nil, nil, err
	}

	cleanup := func() {
		dispatcherCancel() // Trigger ctx.Done() in dispatcher's receive loop
		if err := msgDispatcher.Stop(); err != nil {
			log.Printf("Dispatcher shutdown error: %v", err)
		}
	}

	return msgDispatcher, cleanup, nil
}

func initServices(ctx context.Context, harqHandler harq.HARQ, msgDispatcher dispatcher.Dispatcher, logger *slog.Logger) (*grpc.Server, *service.GatewayService, []Shutdownable, error) {
	var shutdownRegistry []Shutdownable

	gatewaySvc := service.NewGatewayService()
	motorSvc := service.NewMotorControlService(harqHandler, msgDispatcher, logger)

	telemetrySvc := service.NewTelemetryService(ctx, harqHandler, msgDispatcher, logger)
	shutdownRegistry = append(shutdownRegistry, telemetrySvc)

	configSvc := service.NewConfigurationService(harqHandler, msgDispatcher, logger)

	batterySvc := service.NewBatteryService(ctx, harqHandler, msgDispatcher, logger)
	shutdownRegistry = append(shutdownRegistry, batterySvc)

	firmwareSvc := service.NewFirmwareService()

	grpcServer := grpc.NewServer(
		grpc.MaxRecvMsgSize(grpcMaxMsgSize),
		grpc.MaxSendMsgSize(grpcMaxMsgSize),
	)

	starv1.RegisterGatewayServiceServer(grpcServer, gatewaySvc)
	starv1.RegisterMotorControlServiceServer(grpcServer, motorSvc)
	// TelemetryService gRPC removed - firmware operates in push mode only (20Hz unsolicited)
	starv1.RegisterConfigurationServiceServer(grpcServer, configSvc)
	starv1.RegisterBatteryManagementServiceServer(grpcServer, batterySvc)
	starv1.RegisterFirmwareUpdateServiceServer(grpcServer, firmwareSvc)

	return grpcServer, gatewaySvc, shutdownRegistry, nil
}

func startGRPCServer(grpcServer *grpc.Server, errChan chan<- error) error {
	grpcLis, err := net.Listen("tcp", grpcListenPort)
	if err != nil {
		return fmt.Errorf("failed to create gRPC listener: %w", err)
	}

	go func() {
		log.Printf("gRPC server listening on %s", grpcListenPort)
		if err := grpcServer.Serve(grpcLis); err != nil {
			errChan <- err
		}
	}()
	return nil
}

func startHTTPServer(gatewaySvc *service.GatewayService, errChan chan<- error) *http.Server {
	ctrlHandler := controller.NewHandlerWithGateway(gatewaySvc)

	mux := http.NewServeMux()
	mux.HandleFunc("/ws/controller", ctrlHandler.ServeHTTP)

	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		if _, err := w.Write([]byte("ok")); err != nil {
			log.Printf("health check write error: %v", err)
		}
	})

	server := &http.Server{
		Addr:         httpListenPort,
		Handler:      mux,
		ReadTimeout:  httpReadTimeout,
		WriteTimeout: httpWriteTimeout,
	}

	go func() {
		log.Printf("HTTP/WebSocket server starting on %s", server.Addr)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			errChan <- err
		}
	}()

	return server
}

func shutdownServers(httpServer *http.Server, grpcServer *grpc.Server, shutdownRegistry []Shutdownable) {
	// 1. Shutdown services FIRST (stops new HARQ operations)
	log.Println("Shutting down services...")
	for i := len(shutdownRegistry) - 1; i >= 0; i-- {
		shutdownRegistry[i].Shutdown()
	}
	log.Println("Services stopped")

	// 2. Then shutdown HTTP server (stops accepting requests)
	ctx, cancel := context.WithTimeout(context.Background(), httpShutdownTimeout)
	defer cancel()
	if err := httpServer.Shutdown(ctx); err != nil {
		log.Printf("HTTP server shutdown error: %v", err)
	} else {
		log.Println("HTTP server stopped")
	}

	// 3. Finally shutdown gRPC server
	log.Printf("Shutting down gRPC server...")
	stopped := make(chan struct{})
	go func() {
		grpcServer.GracefulStop()
		close(stopped)
	}()

	select {
	case <-stopped:
		log.Println("gRPC server stopped gracefully")
	case <-time.After(grpcShutdownTimeout):
		log.Println("gRPC server graceful shutdown timed out, forcing stop")
		grpcServer.Stop()
	}
}
