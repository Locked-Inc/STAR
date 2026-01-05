package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/controller"
)

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	// Initialize controller handler
	ctrlHandler := controller.NewHandler()

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
	
		// Start server in goroutine
		go func() {
			log.Printf("STAR Gateway starting on %s", server.Addr)
			if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
				log.Fatalf("failed to listen and serve: %v", err)
			}
		}()
	
			// Wait for interrupt
			c := make(chan os.Signal, 1)
			signal.Notify(c, os.Interrupt)
			<-c
		
			log.Println("Shutting down server...")
		
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()
		
			if err := server.Shutdown(ctx); err != nil {
				log.Fatalf("Server shutdown failed: %+v", err)
			}
			log.Println("Server exited gracefully")
		}
		