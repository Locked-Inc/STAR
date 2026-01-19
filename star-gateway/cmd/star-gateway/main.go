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
	var deviceTransport transport.Device
	simulationMode := os.Getenv("STAR_SIMULATION_MODE") == "true"

	if simulationMode {
		log.Println("⚠️  RUNNING IN SIMULATION MODE (Virtual RX72N)")
		log.Printf("    Connecting to socket: %s", transport.DefaultSocketPath)
		socketTransport := transport.NewSocketTransport(transport.DefaultSocketPath)
		if err := socketTransport.Open(); err != nil {
			return fmt.Errorf("failed to open socket transport: %w", err)
		}
		defer socketTransport.Close()
		deviceTransport = socketTransport
	} else {
		log.Println("🔌 Initializing SPI Transport")
		spiTransport := transport.NewSPITransport(transport.DefaultConfig())
		if err := spiTransport.Open(); err != nil {
			return fmt.Errorf("failed to open SPI transport: %w", err)
		}
		defer spiTransport.Close()
		deviceTransport = spiTransport
	}

	// Create a legacy transport adapter for HARQ (which still uses old interface)
	// Note: We'll need to update HARQ to use Device interface in the future
	var legacyTransport transport.Transport
	if spi, ok := deviceTransport.(*transport.SPITransport); ok {
		legacyTransport = spi
	} else if sock, ok := deviceTransport.(*transport.SocketTransport); ok {
		legacyTransport = sock
	} else {
		return fmt.Errorf("unknown transport type")
	}

	// ========================================
	// Layer 2: Frame Encoder/Decoder
	// ========================================
	log.Printf("Initializing frame encoder/decoder...")
	frameEncoder := frame.NewEncoder()
	frameDecoder := frame.NewDecoder()

	// ========================================
	// Layer 3: FEC Encoder/Decoder
	// ========================================
	log.Printf("Initializing FEC encoder/decoder...")
	fecEncoder := fec.NewConvolutionalEncoder()
	fecDecoder := fec.NewViterbiDecoder()

	// ========================================
	// Layer 4: HARQ Protocol Handler
	// ========================================
	log.Printf("Initializing HARQ handler...")
	harqHandler := harq.NewChaseCombining(
		harq.DefaultConfig(),
		legacyTransport,
		frameEncoder,
		frameDecoder,
		fecEncoder,
		fecDecoder,
	)

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
	msgDispatcher, err := dispatcher.NewDispatcher(harqHandler, logger, nil)
	if err != nil {
		return err
	}

	// Start dispatcher with cancellable context
	dispatcherCtx, dispatcherCancel := context.WithCancel(context.Background())
	if err := msgDispatcher.Start(dispatcherCtx); err != nil {
		dispatcherCancel()
		return err
	}
	defer func() {
		dispatcherCancel() // Trigger ctx.Done() in dispatcher's receive loop
		if err := msgDispatcher.Stop(); err != nil {
			log.Printf("Dispatcher shutdown error: %v", err)
		}
	}()

	// ========================================
	// Layer 5: gRPC Services
	// ========================================
	log.Printf("Initializing gRPC services...")

	// Create a context for service lifecycle management
	ctx := context.Background()

	// Shutdown registry for deterministic service cleanup (LIFO order)
	var shutdownRegistry []Shutdownable

	// Gateway service
	gatewaySvc := service.NewGatewayService()

	// Motor control service (with HARQ integration, Dispatcher, and Logger)
	motorSvc := service.NewMotorControlService(harqHandler, msgDispatcher, logger)

	// Telemetry service (with context, HARQ integration, Dispatcher, and Logger)
	telemetrySvc := service.NewTelemetryService(ctx, harqHandler, msgDispatcher, logger)
	shutdownRegistry = append(shutdownRegistry, telemetrySvc)

	// Configuration service (with HARQ integration, Dispatcher, and Logger)
	configSvc := service.NewConfigurationService(harqHandler, msgDispatcher, logger)

	// Battery management service (with context, HARQ integration, Dispatcher, and Logger)
	batterySvc := service.NewBatteryService(ctx, harqHandler, msgDispatcher, logger)
	shutdownRegistry = append(shutdownRegistry, batterySvc)

	// Create gRPC server
	grpcServer := grpc.NewServer(
		grpc.MaxRecvMsgSize(10*1024*1024), // 10MB
		grpc.MaxSendMsgSize(10*1024*1024),
	)

	// Register gRPC services
	starv1.RegisterGatewayServiceServer(grpcServer, gatewaySvc)
	starv1.RegisterMotorControlServiceServer(grpcServer, motorSvc)
	starv1.RegisterTelemetryServiceServer(grpcServer, telemetrySvc)
	starv1.RegisterConfigurationServiceServer(grpcServer, configSvc)
	starv1.RegisterBatteryManagementServiceServer(grpcServer, batterySvc)

	// Start gRPC listener
	grpcLis, err := net.Listen("tcp", ":50051")
	if err != nil {
		return err
	}

	// Error channel for goroutine failures
	errChan := make(chan error, 1)

	// Start gRPC server in goroutine
	go func() {
		log.Printf("gRPC server listening on :50051")
		if err := grpcServer.Serve(grpcLis); err != nil {
			errChan <- err
		}
	}()

	// Initialize controller handler with Gateway service
	ctrlHandler := controller.NewHandlerWithGateway(gatewaySvc)

	mux := http.NewServeMux()
	mux.HandleFunc("/ws/controller", ctrlHandler.ServeHTTP)

	// Add a health check
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		if _, err := w.Write([]byte("ok")); err != nil {
			log.Printf("health check write error: %v", err)
		}
	})

	server := &http.Server{
		Addr:         ":8080",
		Handler:      mux,
		ReadTimeout:  time.Second * 10,
		WriteTimeout: time.Second * 10,
	}

	// Start HTTP server in goroutine
	go func() {
		log.Printf("HTTP/WebSocket server starting on %s", server.Addr)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			errChan <- err
		}
	}()

	// Wait for interrupt or error
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt)

	select {
	case <-sigChan:
		log.Println("Interrupt received, shutting down...")
	case err := <-errChan:
		log.Printf("Server error: %v, shutting down...", err)
	}

	// Shutdown HTTP server
	ctx, cancel := context.WithTimeout(context.Background(), httpShutdownTimeout)
	defer cancel()

	if err := server.Shutdown(ctx); err != nil {
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

	// Transport cleanup (via defer at top of run)
	if simulationMode {
		log.Println("Closing socket transport...")
	} else {
		log.Println("Closing SPI transport...")
	}

	log.Println("All servers exited gracefully")
	return nil
}
