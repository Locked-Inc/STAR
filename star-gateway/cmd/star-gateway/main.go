package main

import (
	"context"
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

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	if err := run(); err != nil {
		log.Fatalf("Application error: %v", err)
	}
}

func run() error {
	// ========================================
	// Layer 1: SPI Transport
	// ========================================
	log.Printf("Initializing SPI transport...")
	spiTransport := transport.NewSPITransport(transport.DefaultConfig())
	if err := spiTransport.Open(); err != nil {
		return err
	}
	defer spiTransport.Close()

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
		spiTransport,
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

	// Gateway service
	gatewaySvc := service.NewGatewayService()

	// Motor control service (with HARQ integration, Dispatcher, and Logger)
	motorSvc := service.NewMotorControlService(harqHandler, msgDispatcher, logger)

	// Telemetry service (with context, HARQ integration, Dispatcher, and Logger)
	telemetrySvc := service.NewTelemetryService(ctx, harqHandler, msgDispatcher, logger)

	// Create gRPC server
	grpcServer := grpc.NewServer(
		grpc.MaxRecvMsgSize(10*1024*1024), // 10MB
		grpc.MaxSendMsgSize(10*1024*1024),
	)

	// Register gRPC services
	starv1.RegisterGatewayServiceServer(grpcServer, gatewaySvc)
	starv1.RegisterMotorControlServiceServer(grpcServer, motorSvc)
	starv1.RegisterTelemetryServiceServer(grpcServer, telemetrySvc)

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

	// Shutdown services
	log.Println("Shutting down services...")
	telemetrySvc.Shutdown()
	log.Println("Services stopped")

	// SPI transport cleanup (via defer at top of run)
	log.Println("Closing SPI transport...")

	log.Println("All servers exited gracefully")
	return nil
}
