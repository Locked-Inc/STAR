// Package app encapsulates the main application logic for the star-gateway.
//
// STAR Project - Texas A&M University
// January 2026
package app

import (
	"context"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"sync"
	"syscall"
	"time"
	"unicode/utf8"

	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/link"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
	"github.com/Locked-Inc/STAR/star-gateway/internal/server"
	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
	"github.com/Locked-Inc/STAR/star-gateway/internal/ws"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
)

const (
	// wsStrictOriginEnv controls WebSocket strict origin validation.
	wsStrictOriginEnv = "WS_STRICT_ORIGIN"

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

	// defaultDispatcherReceiveInterval is the default polling cadence for dispatcher receive loop (100Hz).
	defaultDispatcherReceiveInterval = 10 * time.Millisecond

	// defaultDispatcherShutdownTimeout is the default graceful shutdown timeout for dispatcher stop.
	defaultDispatcherShutdownTimeout = 5 * time.Second

	// defaultEstopReason is the fallback reason string used when the HTTP /api/estop
	// endpoint is called without an explicit ?reason= query parameter.
	defaultEstopReason = "user_http_fallback"

	// maxEstopReasonLen caps the length of the sanitized emergency-stop reason
	// written to logs and forwarded to the motor controller. Prevents log
	// injection and unbounded memory usage from adversarially long query params.
	maxEstopReasonLen = 256

	// slamResetTimeout bounds SLAM reset request latency from the HTTP handler.
	slamResetTimeout = 5 * time.Second

	// slamResetCooldown is the minimum interval between successive SLAM reset
	// calls. Requests within this window are rejected with HTTP 429 to protect
	// slam_toolbox from rapid resets that could corrupt map state.
	slamResetCooldown = 10 * time.Second

	// slamToolboxResetServicePath is the ROS2 service path for slam_toolbox reset.
	slamToolboxResetServicePath = "/slam_toolbox/reset"

	// slamToolboxResetServiceType is the ROS2 service type for slam_toolbox reset.
	slamToolboxResetServiceType = "slam_toolbox/srv/Reset"
)

// Config holds the application configuration.
type Config struct {
	SimulationMode bool
	SocketPath     string
	// "auto", "force-usb", "force-spi"
	TransportMode manager.TransportMode

	// CDCDevice is the /dev path for the USB CDC serial port (e.g., /dev/ttyUSB1).
	// Leave empty to auto-detect via USBVID:USBPID sysfs scan.
	CDCDevice string

	// USBVID is the USB Vendor ID of the connected motor controller.
	// Used for VID:PID-based device discovery and health monitoring.
	// Set to 0 to disable (falls back to default /dev/ttyUSB0).
	USBVID uint16

	// USBPID is the USB Product ID of the connected motor controller.
	// Used for VID:PID-based device discovery and health monitoring.
	USBPID uint16
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
	configuration *service.ConfigurationService
	firmware      *service.FirmwareService
	gateway       *service.GatewayService
}

// Package-level variables which point to the real constructors. These are
// intentionally replaceable test hooks used by unit tests to inject mocks or
// test doubles. IMPORTANT: modifying these globals is NOT safe for tests that
// run in parallel -- any test that overrides them must NOT call t.Parallel()
// and should restore the original function when finished (or avoid concurrent
// replacement). The default implementations are `createSPITransport` and
// `createSPILink` respectively; look up those symbols to find the real logic.
var (
	createSPITransportFn = createSPITransport
	createSPILinkFn      = createSPILink
)

func (s *serviceSet) count() int {
	if s == nil {
		return 0
	}
	n := 0
	if s.motorControl != nil {
		n++
	}
	if s.telemetry != nil {
		n++
	}
	if s.configuration != nil {
		n++
	}
	if s.firmware != nil {
		n++
	}
	if s.gateway != nil {
		n++
	}
	return n
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

	// Create a derived cancellable context for the gateway run lifecycle so
	// subsystems (HTTP/gRPC servers, dispatcher) can observe cancellation when
	// we receive an OS signal and perform graceful shutdown.
	runCtx, cancel := context.WithCancel(ctx)
	defer cancel()

	// Initialize transport manager with validation and failover
	tm, err := initTransportManager(runCtx, config, logger)
	if err != nil {
		return fmt.Errorf("transport manager initialization failed: %w", err)
	}
	defer func() {
		if err := tm.Stop(); err != nil {
			logger.Error("failed to stop transport manager", slog.Any("error", err))
		}
	}()

	// Log initial transport selection for observability
	logger.Info("transport manager initialized",
		slog.String("mode", string(config.TransportMode)),
		slog.String("active_transport", tm.GetActiveTransport()),
		slog.Int("registered_count", len(tm.GetAvailableTransports())))

	// Initialize message dispatcher (starts receive loop, consumes control frames)
	disp, err := initDispatcher(ctx, tm, logger)
	if err != nil {
		return fmt.Errorf("dispatcher initialization failed: %w", err)
	}
	defer func() {
		if err := disp.Stop(); err != nil {
			logger.Error("failed to stop dispatcher", slog.Any("error", err))
		}
	}()

	// Initialize service layer
	services, err := initServices(ctx, tm, disp, logger)
	if err != nil {
		return fmt.Errorf("services initialization failed: %w", err)
	}

	logger.Info("services initialized with dispatcher",
		slog.Int("service_count", services.count()))

	// Initialize servers struct
	servers := &Servers{}

	// Start gRPC server (non-blocking) -- use derived runCtx so server goroutines
	// observe cancellation from the Run() signal handler.
	if err := startGRPCServer(runCtx, servers, services, logger); err != nil {
		return fmt.Errorf("gRPC server startup failed: %w", err)
	}

	// Start HTTP server (non-blocking)
	if err := startHTTPServer(runCtx, servers, services, logger); err != nil {
		return fmt.Errorf("HTTP server startup failed: %w", err)
	}

	// Setup signal handling
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)

	// Block until signal received or context cancelled
	select {
	case sig := <-sigChan:
		logger.Info("Received shutdown signal", slog.String("signal", sig.String()))
		// Notify derived context so background servers/loops can begin graceful shutdown.
		cancel()
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
func initTransportManager(ctx context.Context, appConfig Config, logger *slog.Logger) (*manager.TransportManager, error) {
	// Create manager config from app config
	mgrConfig := &manager.Config{
		Mode:   appConfig.TransportMode,
		USBVID: appConfig.USBVID,
		USBPID: appConfig.USBPID,
	}

	// Validate config or fallback to default.
	// Re-apply VID:PID after validation because validateOrUseDefault may replace
	// the entire config with defaults (which carry Renesas VID:PID), losing the
	// device-specific VID:PID we set for BBB or future motor controllers.
	mgrConfig = validateOrUseDefault(logger, mgrConfig)
	if appConfig.USBVID != 0 || appConfig.USBPID != 0 {
		mgrConfig.USBVID = appConfig.USBVID
		mgrConfig.USBPID = appConfig.USBPID
	}

	// Create transport manager (internally creates SessionState)
	tm := manager.NewTransportManager(mgrConfig)

	// Initialize transports using manager's SessionState (passed via tm)
	if err := initializeTransports(appConfig, tm, logger); err != nil {
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
func validateOrUseDefault(logger *slog.Logger, config *manager.Config) *manager.Config {
	if logger == nil {
		logger = slog.Default()
	}

	// Validate configuration
	if err := config.Validate(); err != nil {
		// Log validation failure and use default configuration, preserving the
		// requested transport mode so --simple / TRANSPORT_MODE is not lost.
		logger.Error("manager config validation failed, using defaults",
			slog.Any("error", err),
			slog.String("requested_mode", string(config.Mode)))
		defaults := manager.DefaultConfig()
		if config.Mode.IsValid() {
			defaults.Mode = config.Mode
		}
		return defaults
	}
	return config
}

// initializeTransports initializes all available transports based on configuration.
// This function attempts to initialize SPI/Socket (for simulation) and USB transports.
// In simulation mode, a socket transport replaces real hardware.
// For ModeSimpleUSB simulation, the socket is registered as USB transport.
//
// CRITICAL: All transports MUST use the shared SessionState from TransportManager
// (via tm.GetSessionState()) to ensure sequence continuity during failover.
func initializeTransports(appConfig Config, tm *manager.TransportManager, logger *slog.Logger) error {
	if logger == nil {
		logger = slog.Default()
	}

	// Get shared session state from manager (single source of truth)
	session := tm.GetSessionState()

	// Simple USB mode: skip SPI entirely -- only uses USB CDC.
	// In simulation mode, use socket transport registered as USB.
	if appConfig.TransportMode == manager.ModeSimpleUSB {
		if appConfig.SimulationMode {
			socketTransport, err := createSocketTransport(appConfig.SocketPath)
			if err != nil {
				return fmt.Errorf("simple-usb simulation requires socket transport: %w", err)
			}
			socketLink, err := link.NewCDCLink(socketTransport, session)
			if err != nil {
				if closeErr := socketTransport.Close(); closeErr != nil {
					logger.Error("failed to close socket after link error", slog.Any("error", closeErr))
				}
				return fmt.Errorf("simple-usb simulation link creation failed: %w", err)
			}
			tm.RegisterTransport(manager.TransportNameUSB, socketLink, manager.PriorityUSB)
			logger.Info("simple-usb simulation: socket registered as USB transport",
				slog.String("socket_path", appConfig.SocketPath))
			return nil // Socket replaces real USB; skip hardware init below
		}
		logger.Info("simple-usb mode: skipping SPI transport registration")
	} else if appConfig.SimulationMode {
		// Simulation mode: use socket transport
		socketTransport, err := createSocketTransport(appConfig.SocketPath)
		if err != nil {
			logger.Error("socket transport initialization failed",
				slog.Any("error", err),
				slog.String("socket_path", appConfig.SocketPath))
		} else {
			// Wrap socket in CDC link (socket protocol matches CDC)
			socketLink, err := link.NewCDCLink(socketTransport, session)
			if err != nil {
				if closeErr := socketTransport.Close(); closeErr != nil {
					logger.Error("failed to close socket transport after link error", slog.Any("error", closeErr))
				}
				logger.Error("socket link creation failed", slog.Any("error", err))
			} else {
				// Register as SPI transport (simulation mode uses socket instead of SPI)
				tm.RegisterTransport(manager.TransportNameSPI, socketLink, manager.PrioritySPI)
				logger.Info("registered socket transport", slog.String("mode", "simulation"))
			}
		}
	} else {
		// Production mode: use SPI transport
		spiTransport, err := createSPITransportFn()
		if err != nil {
			logger.Error("spi transport initialization failed", slog.Any("error", err))
		} else {
			// Wrap in SPI link layer with shared session state
			spiLink, err := createSPILinkFn(spiTransport, session)
			if err != nil {
				if closeErr := spiTransport.Close(); closeErr != nil {
					logger.Error("failed to close SPI transport after link error", slog.Any("error", closeErr))
				}
				logger.Error("spi link creation failed", slog.Any("error", err))
			} else {
				tm.RegisterTransport(manager.TransportNameSPI, spiLink, manager.PrioritySPI)
				logger.Info("registered transport", slog.String("name", manager.TransportNameSPI))
			}
		}
	}

	// USB CDC initialization (non-fatal unless force-usb mode)
	usbLink, err := createUSBLink(appConfig.CDCDevice, session)
	if err != nil {
		logger.Error("usb cdc initialization failed", slog.Any("error", err))
		// If force-usb or simple-usb mode, USB is required
		if appConfig.TransportMode == manager.ModeForceUSB || appConfig.TransportMode == manager.ModeSimpleUSB {
			logger.Error("USB mode requires usb transport", slog.String("mode", string(appConfig.TransportMode)))
			return fmt.Errorf("%s mode requires USB transport: %w", appConfig.TransportMode, err)
		}
	} else {
		tm.RegisterTransport(manager.TransportNameUSB, usbLink, manager.PriorityUSB)
		logger.Info("registered transport", slog.String("name", manager.TransportNameUSB))
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
		if !hasTransport(available, manager.TransportNameUSB) {
			return fmt.Errorf("force-USB mode enabled but USB transport unavailable")
		}
	}

	if mode == manager.ModeForceSPI {
		if !hasTransport(available, manager.TransportNameSPI) {
			return fmt.Errorf("force-SPI mode enabled but SPI transport unavailable")
		}
	}

	return nil
}

func hasTransport(available []string, name string) bool {
	for _, transportName := range available {
		if transportName == name {
			return true
		}
	}
	return false
}

// createSocketTransport creates a SocketTransport for simulation mode.
// The transport is opened and ready for use upon successful return.
func createSocketTransport(socketPath string) (transport.Device, error) {
	// Create SocketTransport with the provided socket path (simulation mode only)
	socketTransport := transport.NewSocketTransport(socketPath)

	// Open the transport connection
	if err := socketTransport.Open(); err != nil {
		return nil, fmt.Errorf("failed to open socket transport: %w", err)
	}

	return socketTransport, nil
}

// createSPITransport creates an SPITransport for production mode.
// The transport is configured with appropriate device parameters and opened.
func createSPITransport() (transport.Device, error) {
	// Create SPI transport with default configuration
	// DefaultConfig uses: /dev/spidev0.0, 10 MHz, Mode 0, 8 bits/word
	spiTransport := transport.NewSPITransport(transport.DefaultConfig())

	// Open the SPI device
	if err := spiTransport.Open(); err != nil {
		return nil, fmt.Errorf("failed to open SPI transport: %w", err)
	}

	return spiTransport, nil
}

// createSPILink creates an SPILink using the provided SPI transport and shared session state.
// This function wraps the SPI transport with HARQ protocol support.
//
// Returns the SPI link (harq.HARQ) or an error if link creation fails.
func createSPILink(spiTransport transport.Device, session *manager.SessionState) (harq.HARQ, error) {
	// Create SPI link with default config (nil = use defaults)
	spiLink, err := link.NewSPILink(spiTransport, session, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create SPI link: %w", err)
	}

	return spiLink, nil
}

// createUSBLink creates a USB CDC link using the provided session state.
// Returns the USB link (harq.HARQ) or an error if initialization fails.
//
// device is the /dev path for the CDC serial port (e.g., /dev/ttyUSB1). When
// empty, the CDCTransport auto-detects via VID:PID sysfs scan using the VID:PID
// in DefaultCDCConfig (Renesas). For BBB, always pass the detected device path.
//
// The CDC transport is opened and wrapped in a CDCLink with the shared session state.
// This ensures sequence continuity during transport switching (prevents Handoff Problem).
func createUSBLink(device string, session *manager.SessionState) (harq.HARQ, error) {
	cdcConfig := transport.DefaultCDCConfig()
	if device != "" {
		cdcConfig.Device = device
	}
	cdcTransport := transport.NewCDCTransport(cdcConfig)

	// Open the CDC device
	if err := cdcTransport.Open(); err != nil {
		return nil, fmt.Errorf("failed to open CDC transport: %w", err)
	}

	// Wrap in link layer with shared session state
	cdcLink, err := link.NewCDCLink(cdcTransport, session)
	if err != nil {
		_ = cdcTransport.Close() // Best-effort cleanup; error logged by caller
		return nil, fmt.Errorf("failed to create CDC link: %w", err)
	}

	return cdcLink, nil
}

// initDispatcher creates and starts the message dispatcher.
// The dispatcher owns the HARQ receive loop and routes messages to services.
// It consumes control frames (PING, PONG, RESET_ACK) internally per TRANSPORT_ARCHITECTURE.md.
func initDispatcher(ctx context.Context, tm harq.HARQ, logger *slog.Logger) (dispatcher.Dispatcher, error) {
	// Create dispatcher with TransportManager (which implements harq.HARQ interface)
	config := &dispatcher.Config{
		ShutdownTimeout: defaultDispatcherShutdownTimeout,
		ReceiveInterval: defaultDispatcherReceiveInterval, // 100Hz for SPI communication spec
	}

	disp, err := dispatcher.NewDispatcher(tm, logger, config)
	if err != nil {
		return nil, fmt.Errorf("failed to create dispatcher: %w", err)
	}

	// Start dispatcher receive loop (begins consuming messages from TransportManager)
	if err := disp.Start(ctx); err != nil {
		return nil, fmt.Errorf("failed to start dispatcher: %w", err)
	}

	logger.Info("dispatcher initialized and started",
		slog.Duration("receive_interval", config.ReceiveInterval),
		slog.Duration("shutdown_timeout", config.ShutdownTimeout))

	return disp, nil
}

// initServices initializes all gRPC service implementations.
// The tm parameter is the TransportManager (which implements harq.HARQ interface).
// Returns serviceSet containing initialized services for gRPC server registration.
func initServices(ctx context.Context, tm harq.HARQ, disp dispatcher.Dispatcher, logger *slog.Logger) (*serviceSet, error) {
	return &serviceSet{
		motorControl:  service.NewMotorControlService(tm, disp, logger),
		telemetry:     service.NewTelemetryService(ctx, tm, disp, logger),
		configuration: service.NewConfigurationService(tm, disp, logger),
		firmware:      service.NewFirmwareService(),
		gateway:       service.NewGatewayService(),
	}, nil
}

// startGRPCServer starts the gRPC server on the configured port.
// The server listens for incoming gRPC connections and serves registered services.
// registerGRPCServices registers all gRPC service implementations with the server.
// This function must be updated whenever new services are added to the protobuf definitions.
func registerGRPCServices(srv *grpc.Server, services *serviceSet) error {
	if srv == nil {
		return fmt.Errorf("gRPC server is nil")
	}
	if services == nil {
		return fmt.Errorf("service set is nil")
	}
	// Check each service individually to provide specific error messages
	if services.motorControl == nil {
		return fmt.Errorf("service set contains nil service: motorControl")
	}
	if services.telemetry == nil {
		return fmt.Errorf("service set contains nil service: telemetry")
	}
	if services.configuration == nil {
		return fmt.Errorf("service set contains nil service: configuration")
	}
	if services.firmware == nil {
		return fmt.Errorf("service set contains nil service: firmware")
	}
	if services.gateway == nil {
		return fmt.Errorf("service set contains nil service: gateway")
	}

	starv1.RegisterMotorControlServiceServer(srv, services.motorControl)
	starv1.RegisterTelemetryServiceServer(srv, services.telemetry)
	starv1.RegisterConfigurationServiceServer(srv, services.configuration)
	starv1.RegisterFirmwareUpdateServiceServer(srv, services.firmware)
	starv1.RegisterGatewayServiceServer(srv, services.gateway)
	return nil
}

// startGRPCServer starts the gRPC server on the configured port with registered services.
func startGRPCServer(ctx context.Context, servers *Servers, services *serviceSet, logger *slog.Logger) error {
	return startGRPCServerWithAddr(ctx, servers, services, grpcListenPort, logger)
}

// startGRPCServerWithAddr starts the gRPC server using the provided listen address.
//
// This helper allows deterministic tests by using random ports (":0") while keeping
// production startup logic in one place.
func startGRPCServerWithAddr(
	ctx context.Context,
	servers *Servers,
	services *serviceSet,
	listenAddr string,
	logger *slog.Logger,
) error {
	if servers == nil {
		return fmt.Errorf("servers container is nil")
	}
	if logger == nil {
		logger = slog.Default()
	}

	// Configure gRPC server with service registration callback
	config := &server.GRPCConfig{
		ListenAddr:     listenAddr,
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
	errChan, err := server.RunGRPCServer(ctx, grpcSrv, config.ListenAddr, logger)
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

// motorControllerAdapter adapts *service.MotorControlService to the ws.MotorController
// interface. The gRPC EmergencyStop method takes a full request struct, while the WebSocket
// hub interface only passes the plain reason string, so this thin adapter bridges the gap.
type motorControllerAdapter struct {
	svc *service.MotorControlService
}

func (a *motorControllerAdapter) EmergencyStop(ctx context.Context, reason string) error {
	if a == nil || a.svc == nil {
		return fmt.Errorf("motorControllerAdapter.EmergencyStop: adapter or svc is nil")
	}
	_, err := a.svc.EmergencyStop(ctx, &starv1.EmergencyStopRequest{Reason: reason})
	return err
}

// startHTTPServer starts the HTTP/WebSocket server on the configured port.
// The server handles HTTP requests and WebSocket connections for the UI.
func startHTTPServer(ctx context.Context, servers *Servers, services *serviceSet, logger *slog.Logger) error {
	return startHTTPServerWithAddr(ctx, servers, services, httpListenPort, logger)
}

// startHTTPServerWithAddr starts the HTTP server using the provided listen address.
//
// The HTTP layer exposes the following endpoints:
//   - /ws for the WebSocket hub (UI telemetry streaming, teleop input, and e-stop)
//   - /api/estop for a REST emergency stop fallback (POST ?reason=...)
//   - /healthz for liveness checks
//
// Safety requirement: services.motorControl must be non-nil. Both the /ws WebSocket
// e-stop path and the /api/estop REST fallback depend on it; starting the server
// without motor control would silently disable safety-critical stop paths. Callers
// must supply a valid MotorControlService or explicitly handle this degraded state
// before calling startHTTPServerWithAddr.
//
// UI static files are served by the dedicated UI service, not by the gateway.
func startHTTPServerWithAddr(
	ctx context.Context,
	servers *Servers,
	services *serviceSet,
	listenAddr string,
	logger *slog.Logger,
) error {
	if servers == nil {
		return fmt.Errorf("servers container is nil")
	}
	if services == nil {
		return fmt.Errorf("service set is nil")
	}
	if services.gateway == nil {
		return fmt.Errorf("gateway service is nil")
	}
	if services.motorControl == nil {
		// Both the WebSocket e-stop path and the /api/estop REST fallback require
		// motorControl; starting without it would silently disable safety-critical
		// stop paths and any call to EmergencyStop would panic.
		return fmt.Errorf("motorControl service is nil: cannot start HTTP server without motor control (e-stop paths would be disabled)")
	}
	if logger == nil {
		logger = slog.Default()
	}

	// internalCtx is a child of the caller-supplied ctx. Cancelling it shuts
	// down the HTTP server (and hub) even when the outer ctx is still live --
	// e.g. when the hub crashes and we must not serve /ws with a broken hub.
	// internalCtx is automatically cancelled when ctx is cancelled (parent
	// propagation), so no explicit defer is needed for the normal-flow cleanup.
	internalCtx, internalCancel := context.WithCancel(ctx)
	// No defer internalCancel() here!

	mux := http.NewServeMux()
	slamSvc, err := service.NewROS2SLAMService(slamToolboxResetServicePath, slamToolboxResetServiceType)
	if err != nil {
		internalCancel()
		return fmt.Errorf("failed to create SLAM service: %w", err)
	}
	if closer, ok := slamSvc.(io.Closer); ok {
		go func() {
			<-internalCtx.Done()
			if closeErr := closer.Close(); closeErr != nil {
				logger.Error("SLAM service Close() failed during shutdown",
					slog.Any("error", closeErr))
			}
		}()
	}

	// -- WebSocket hub setup -----------------------------------------------
	adapter := &motorControllerAdapter{svc: services.motorControl}
	hub := ws.NewHub(logger)

	hubErrChan := make(chan error, 1)
	go func() {
		var hubErr error
		defer func() {
			if r := recover(); r != nil {
				hubErr = fmt.Errorf("ws hub.Run panicked: %v", r)
			}
			if hubErr != nil {
				hubErrChan <- hubErr
			}
			close(hubErrChan)
		}()
		hub.Run(internalCtx)
	}()

	// Monitor hub; owns internalCancel so the HTTP server shuts down
	// if the hub dies, but NOT before the setup function returns.
	go func() {
		defer internalCancel() // <- lives here now
		if err, ok := <-hubErrChan; ok && err != nil {
			logger.Error("ws hub exited with error -- initiating HTTP server shutdown",
				slog.Any("error", err))
		} else {
			logger.Info("ws hub.Run exited cleanly")
		}
	}()

	services.gateway.SetHub(hub)

	// WebSocket origin checking: defaults to true (secure) for production.
	// Can be disabled via WS_STRICT_ORIGIN=false for development/testing only.
	//
	// SECURITY WARNING: Disabling origin checking allows cross-origin WebSocket
	// connections, which can enable CSRF attacks. Only disable if:
	// - Running in a trusted development environment
	// - Additional authentication/authorization is implemented at the application layer
	// - Network segmentation prevents unauthorized access
	strictOriginChecking := true
	if envVal := os.Getenv(wsStrictOriginEnv); envVal != "" {
		if envVal == "false" || envVal == "0" {
			strictOriginChecking = false
			logger.Warn(fmt.Sprintf("WebSocket strict origin checking DISABLED via %s environment variable", wsStrictOriginEnv),
				slog.String("compensating_controls_required", "ensure network segmentation and application-layer auth"))
		}
	}
	if !strictOriginChecking {
		logger.Error("SECURITY: WebSocket origin validation is disabled - vulnerable to CSRF attacks",
			slog.Bool("strict_origin_checking", false),
			slog.String("mitigation", fmt.Sprintf("set %s=true or remove environment variable", wsStrictOriginEnv)))
	}
	mux.Handle("/ws", ws.NewHandler(hub, adapter, logger, !strictOriginChecking))

	// REST fallback for emergency stop. The frontend sends the reason as a query
	// parameter (fetch('/api/estop?reason=...', { method: 'POST' })).
	// ADR-13: context.Background() is used so a dropped HTTP connection cannot
	// cancel the safety command before it reaches the motor controller.
	mux.HandleFunc("/api/estop", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		// Defensive nil-check: motorControl is validated at startup, but guard here
		// in case the handler is ever reached in a degraded/test scenario.
		if services.motorControl == nil {
			logger.Error("REST emergency stop: motorControl service unavailable")
			http.Error(w, "motor control unavailable", http.StatusServiceUnavailable)
			return
		}
		raw := r.URL.Query().Get("reason")
		// Sanitize the reason string: replace control characters (including
		// newlines and carriage returns) with a space to prevent log injection,
		// then truncate to maxEstopReasonLen.
		sanitizedReason := strings.Map(func(r rune) rune {
			if r < 0x20 || r == 0x7F {
				return ' '
			}
			return r
		}, raw)
		if len(sanitizedReason) > maxEstopReasonLen {
			byteCount := 0
			truncIdx := 0
			for i, r := range sanitizedReason {
				if byteCount+utf8.RuneLen(r) > maxEstopReasonLen {
					truncIdx = i
					break
				}
				byteCount += utf8.RuneLen(r)
				truncIdx = i + utf8.RuneLen(r)
			}
			sanitizedReason = sanitizedReason[:truncIdx]
		}
		sanitizedReason = strings.TrimSpace(sanitizedReason)
		if sanitizedReason == "" {
			sanitizedReason = defaultEstopReason
		}
		estopCtx, cancel := context.WithTimeout(context.Background(), ws.EstopTimeout)
		defer cancel()
		if _, err := services.motorControl.EmergencyStop(estopCtx,
			&starv1.EmergencyStopRequest{Reason: sanitizedReason}); err != nil {
			logger.Error("REST emergency stop failed",
				slog.Any("error", err),
				slog.String("reason", sanitizedReason))
			http.Error(w, "emergency stop failed", http.StatusInternalServerError)
			return
		}
		logger.Info("REST emergency stop succeeded", slog.String("reason", sanitizedReason))
		w.WriteHeader(http.StatusNoContent)
	})

	// SLAM reset endpoint -- calls slam_toolbox Reset service via ros2 CLI.
	// The gateway process inherits the ROS2 environment, so ros2 is on PATH.
	//
	// Rate-limited to 1 reset per slamResetCooldown to prevent disruption.
	// All invocations are audit-logged with caller identity and outcome.
	var (
		slamResetMu       sync.Mutex
		lastSlamResetTime time.Time
	)
	mux.HandleFunc("/api/slam/reset", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		remoteAddr := r.RemoteAddr
		forwardedFor := r.Header.Get("X-Forwarded-For")
		now := time.Now()

		// Rate-limit check: enforce cooldown between successive resets.
		slamResetMu.Lock()
		sinceLastReset := now.Sub(lastSlamResetTime)
		allowed := lastSlamResetTime.IsZero() || sinceLastReset >= slamResetCooldown
		if allowed {
			lastSlamResetTime = now
		}
		slamResetMu.Unlock()

		if !allowed {
			logger.Warn("SLAM reset denied: rate limit exceeded",
				slog.String("remote_addr", remoteAddr),
				slog.String("x_forwarded_for", forwardedFor),
				slog.Time("timestamp", now),
				slog.Duration("cooldown_remaining", slamResetCooldown-sinceLastReset),
				slog.String("outcome", "denied"))
			http.Error(w, fmt.Sprintf("rate limit exceeded: SLAM reset only allowed once per %s", slamResetCooldown), http.StatusTooManyRequests)
			return
		}

		// Use r.Context() so client disconnect/cancel aborts this non-safety SLAM
		// reset path. This intentionally differs from /api/estop, which uses
		// context.Background() per ADR-13 so safety commands cannot be cancelled by
		// HTTP client lifecycle.
		resetCtx, resetCancel := context.WithTimeout(r.Context(), slamResetTimeout)
		defer resetCancel()
		if err := slamSvc.Reset(resetCtx); err != nil {
			logger.Error("SLAM reset failed",
				slog.Any("error", err),
				slog.String("remote_addr", remoteAddr),
				slog.String("x_forwarded_for", forwardedFor),
				slog.Time("timestamp", now),
				slog.String("outcome", "error"))
			http.Error(w, "reset failed", http.StatusInternalServerError)
			return
		}
		logger.Info("SLAM reset succeeded",
			slog.String("remote_addr", remoteAddr),
			slog.String("x_forwarded_for", forwardedFor),
			slog.Time("timestamp", now),
			slog.String("outcome", "allowed"))
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(`{"status":"reset"}`))
	})

	// Lightweight liveness endpoint for orchestration and local debugging.
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("ok"))
	})

	// Configure HTTP server
	config := &server.HTTPConfig{
		ListenAddr:   listenAddr,
		ReadTimeout:  httpReadTimeout,
		WriteTimeout: httpWriteTimeout,
		Handler:      mux,
	}

	// Create server
	httpSrv, err := server.NewHTTPServer(config, logger)
	if err != nil {
		internalCancel() // prevent hub/monitor goroutine leak on early return
		return fmt.Errorf("failed to create HTTP server: %w", err)
	}
	servers.HTTPServer = httpSrv

	// Start server (non-blocking). Uses internalCtx so a hub failure (which
	// calls internalCancel) triggers graceful HTTP server shutdown.
	errChan, err := server.RunHTTPServer(internalCtx, httpSrv, logger)
	if err != nil {
		internalCancel() // prevent hub/monitor goroutine leak on early return
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
