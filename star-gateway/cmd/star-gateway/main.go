package main

import (
	"context"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/controller"
	"github.com/Locked-Inc/STAR/star-gateway/internal/fec"
	"github.com/Locked-Inc/STAR/star-gateway/internal/frame"
	"github.com/Locked-Inc/STAR/star-gateway/internal/harq"
	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	"github.com/Locked-Inc/STAR/star-gateway/internal/transport"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/grpc"
)

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	// ========================================
	// Layer 1: SPI Transport
	// ========================================
	log.Printf("Initializing SPI transport...")
	spiTransport := transport.NewSPITransport(transport.DefaultConfig())
	if err := spiTransport.Open(); err != nil {
		log.Fatalf("Failed to open SPI transport: %v", err)
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
	// Layer 5: gRPC Services
	// ========================================
	log.Printf("Initializing gRPC services...")

	// Gateway service
	gatewaySvc := service.NewGatewayService()

	// Motor control service (with HARQ integration)
	motorSvc := service.NewMotorControlService(harqHandler)

	// Create gRPC server
	grpcServer := grpc.NewServer(
		grpc.MaxRecvMsgSize(10*1024*1024), // 10MB
		grpc.MaxSendMsgSize(10*1024*1024),
	)

	// Register gRPC services
	starv1.RegisterGatewayServiceServer(grpcServer, gatewaySvc)
	starv1.RegisterMotorControlServiceServer(grpcServer, motorSvc)

	// Start gRPC listener
	grpcLis, err := net.Listen("tcp", ":50051")
	if err != nil {
		log.Fatalf("Failed to listen on :50051: %v", err)
	}

	// Start gRPC server in goroutine
	go func() {
		log.Printf("gRPC server listening on :50051")
		if err := grpcServer.Serve(grpcLis); err != nil {
			log.Fatalf("gRPC server failed: %v", err)
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
			log.Fatalf("failed to listen and serve: %v", err)
		}
	}()

	// Wait for interrupt
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	<-c

	log.Println("Shutting down servers...")

	// Shutdown HTTP server
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := server.Shutdown(ctx); err != nil {
		log.Fatalf("HTTP server shutdown failed: %+v", err)
	}
	log.Println("HTTP server stopped")

	// Shutdown gRPC server
	log.Printf("Shutting down gRPC server...")
	grpcServer.GracefulStop()
	log.Println("gRPC server stopped")

	// SPI transport cleanup (via defer at top of main)
	log.Println("Closing SPI transport...")

	log.Println("All servers exited gracefully")
}
