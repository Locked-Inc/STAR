package main

import (
	"context"
	"fmt"
	"log"
	"log/slog"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/controller"
	"github.com/Locked-Inc/STAR/star-gateway/internal/dispatcher"
	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
)

const (
	// httpShutdownTimeout is the maximum time allowed for HTTP server graceful shutdown.
	httpShutdownTimeout = 5 * time.Second

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
)

// Shutdownable defines the interface for services that require graceful shutdown.
type Shutdownable interface {
	Shutdown()
}

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	if err := run(); err != nil {
		log.Fatalf("Application error: %v", err)
	}
}

func run() error {
	// ========================================
	// Layer 1: Transport (SPI or Socket for simulation)
	// ========================================
	deviceTransport, transportCleanup, err := initTransport()
	if err != nil {
		return err
	}
	defer transportCleanup()

	// ========================================
	// Layer 2-4: Protocol Stack (HARQ)
	// ========================================
	harqHandler, err := initHARQ(deviceTransport)
	if err != nil {
		return err
	}

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
	msgDispatcher, dispatcherCleanup, err := initDispatcher(harqHandler, logger)
	if err != nil {
		return err
	}
	defer dispatcherCleanup()

	// ========================================
	// Layer 5: gRPC Services
	// ========================================
	log.Printf("Initializing gRPC services...")
	grpcServer, gatewaySvc, shutdownRegistry, err := initServices(context.Background(), harqHandler, msgDispatcher, logger)
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

	// Wait for interrupt or error
	waitForShutdown(errChan)

	// Shutdown servers and services
	shutdownServers(httpServer, grpcServer, shutdownRegistry)

	// Transport cleanup is handled by deferred close functions above
	log.Println("All servers exited gracefully")
	return nil
}

func initTransport() (transport.Device, func(), error) {
	var deviceTransport transport.Device
	var simulationMode bool

	if val, ok := os.LookupEnv("STAR_SIMULATION_MODE"); ok {
		var err error
		simulationMode, err = strconv.ParseBool(val)
		if err != nil {
			return nil, nil, fmt.Errorf("invalid STAR_SIMULATION_MODE %q: %w", val, err)
		}
	}

	cleanup := func() {
		if deviceTransport != nil {
			if err := deviceTransport.Close(); err != nil {
				log.Printf("Transport close error: %v", err)
			} else {
				log.Println("Transport closed")
			}
		}
	}

	if simulationMode {
		log.Println("WARNING: RUNNING IN SIMULATION MODE (Virtual RX72N)")
		log.Printf("    Connecting to socket: %s", transport.DefaultSocketPath)
		socketTransport := transport.NewSocketTransport(transport.DefaultSocketPath)
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

func initHARQ(deviceTransport transport.Device) (harq.HARQ, error) {
	// Create a legacy transport adapter for HARQ
	var legacyTransport transport.Transport
	switch t := deviceTransport.(type) {
	case *transport.SPITransport:
		legacyTransport = t
	case *transport.SocketTransport:
		legacyTransport = t
	default:
		return nil, fmt.Errorf("unknown transport type")
	}

	log.Printf("Initializing frame encoder/decoder...")
	frameEncoder := frame.NewEncoder()
	frameDecoder := frame.NewDecoder()

	log.Printf("Initializing FEC encoder/decoder...")
	fecEncoder := fec.NewConvolutionalEncoder()
	fecDecoder := fec.NewViterbiDecoder()

	log.Printf("Initializing HARQ handler...")
	return harq.NewChaseCombining(
		harq.DefaultConfig(),
		legacyTransport,
		frameEncoder,
		frameDecoder,
		fecEncoder,
		fecDecoder,
	), nil
}

func initDispatcher(harqHandler harq.HARQ, logger *slog.Logger) (dispatcher.Dispatcher, func(), error) {
	msgDispatcher, err := dispatcher.NewDispatcher(harqHandler, logger, nil)
	if err != nil {
		return nil, nil, err
	}

	dispatcherCtx, dispatcherCancel := context.WithCancel(context.Background())
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

	grpcServer := grpc.NewServer(
		grpc.MaxRecvMsgSize(grpcMaxMsgSize),
		grpc.MaxSendMsgSize(grpcMaxMsgSize),
	)

	starv1.RegisterGatewayServiceServer(grpcServer, gatewaySvc)
	starv1.RegisterMotorControlServiceServer(grpcServer, motorSvc)
	starv1.RegisterTelemetryServiceServer(grpcServer, telemetrySvc)
	starv1.RegisterConfigurationServiceServer(grpcServer, configSvc)
	starv1.RegisterBatteryManagementServiceServer(grpcServer, batterySvc)

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

func waitForShutdown(errChan <-chan error) {
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt)

	select {
	case <-sigChan:
		log.Println("Interrupt received, shutting down...")
	case err := <-errChan:
		log.Printf("Server error: %v, shutting down...", err)
	}
}

func shutdownServers(httpServer *http.Server, grpcServer *grpc.Server, shutdownRegistry []Shutdownable) {
	// Shutdown HTTP server
	ctx, cancel := context.WithTimeout(context.Background(), httpShutdownTimeout)
	defer cancel()

	if err := httpServer.Shutdown(ctx); err != nil {
		log.Printf("HTTP server shutdown error: %v", err)
	} else {
		log.Println("HTTP server stopped")
	}

	// Shutdown gRPC server
	log.Printf("Shutting down gRPC server...")
	grpcServer.GracefulStop()
	log.Println("gRPC server stopped")

	// Shutdown services in reverse order (LIFO)
	log.Println("Shutting down services...")
	for i := len(shutdownRegistry) - 1; i >= 0; i-- {
		shutdownRegistry[i].Shutdown()
	}
	log.Println("Services stopped")
}
