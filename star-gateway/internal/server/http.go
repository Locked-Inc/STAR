package server

import (
	"context"
	"fmt"
	"log/slog"
	"net"
	"net/http"
	"time"
)

// httpShutdownTimeout is the maximum time allowed for HTTP server graceful shutdown.
const httpShutdownTimeout = 5 * time.Second

// NewHTTPServer creates a configured http.Server without starting it.
// This enables testing handler wiring without opening network ports.
//
// The server is configured with:
//   - Address from config.ListenAddr
//   - Handler from config.Handler (typically http.ServeMux)
//   - Timeouts from config (ReadTimeout, WriteTimeout)
//   - Error logger that uses slog
//
// Returns error if config validation fails.
func NewHTTPServer(config *HTTPConfig, logger *slog.Logger) (*http.Server, error) {
	if err := config.Validate(); err != nil {
		return nil, fmt.Errorf("invalid HTTP config: %w", err)
	}

	srv := &http.Server{
		Addr:         config.ListenAddr,
		Handler:      config.Handler,
		ReadTimeout:  config.ReadTimeout,
		WriteTimeout: config.WriteTimeout,
		ErrorLog:     slog.NewLogLogger(logger.Handler(), slog.LevelError),
	}

	return srv, nil
}

// RunHTTPServer starts the HTTP server in a background goroutine and returns immediately.
//
// The function:
//  1. Creates a TCP listener on srv.Addr
//  2. Starts server in background goroutine via srv.Serve()
//  3. Spawns a shutdown watcher goroutine that triggers graceful shutdown on ctx.Done()
//
// Returns:
//   - Error channel for async error reporting (buffered, size 1)
//   - Immediate error if listener creation fails
//
// Graceful shutdown:
//   - Triggered automatically when ctx is cancelled
//   - Calls srv.Shutdown() with httpShutdownTimeout (5 seconds)
//   - Allows in-flight requests to complete
//   - After timeout, forcefully closes connections
//
// The error channel receives:
//   - nil on clean shutdown
//   - error if srv.Serve() fails (excluding http.ErrServerClosed)
//
// Example:
//
//	ctx, cancel := context.WithCancel(context.Background())
//	defer cancel()
//
//	errChan, err := RunHTTPServer(ctx, httpSrv, logger)
//	if err != nil {
//	    return fmt.Errorf("failed to start: %w", err)
//	}
//
//	// Monitor for runtime errors
//	go func() {
//	    if err := <-errChan; err != nil {
//	        logger.Error("server error", slog.String("error", err.Error()))
//	    }
//	}()
//
//	// Trigger graceful shutdown
//	cancel()
func RunHTTPServer(ctx context.Context, srv *http.Server, logger *slog.Logger) (<-chan error, error) {
	// Create TCP listener first to catch port binding errors early
	listener, err := net.Listen("tcp", srv.Addr)
	if err != nil {
		return nil, fmt.Errorf("failed to listen on %s: %w", srv.Addr, err)
	}

	// Update srv.Addr to the actual bound address (important for :0 random ports)
	srv.Addr = listener.Addr().String()

	errChan := make(chan error, 1)

	// Start server in background goroutine
	go func() {
		logger.Info("HTTP server starting", slog.String("addr", srv.Addr))

		if err := srv.Serve(listener); err != nil && err != http.ErrServerClosed {
			logger.Error("HTTP server error", slog.String("error", err.Error()))
			errChan <- err
			return
		}

		logger.Info("HTTP server stopped")
		errChan <- nil
	}()

	// Spawn shutdown watcher goroutine
	go func() {
		<-ctx.Done()

		logger.Info("HTTP server shutting down")

		shutdownCtx, cancel := context.WithTimeout(context.Background(), httpShutdownTimeout)
		defer cancel()

		if err := srv.Shutdown(shutdownCtx); err != nil {
			logger.Error("HTTP server shutdown failed",
				slog.String("error", err.Error()),
				slog.Duration("timeout", httpShutdownTimeout))
		}
	}()

	return errChan, nil
}
