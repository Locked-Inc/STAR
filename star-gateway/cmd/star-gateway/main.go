package main

import (
	"context"
	"log"
	"os"
	"strconv"

	"github.com/Locked-Inc/STAR/star-gateway/internal/app"
	"github.com/Locked-Inc/STAR/star-gateway/internal/manager"
)

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	config := app.Config{}
	// Get transport mode from environment variable, default to auto if not set or invalid
	mode, err := manager.ParseTransportMode(os.Getenv("TRANSPORT_MODE"))
	if err != nil {
		log.Printf("Warning: %v. Defaulting to auto mode.", err)
	}
	config.TransportMode = mode

	if val, ok := os.LookupEnv("STAR_SIMULATION_MODE"); ok {
		var err error
		config.SimulationMode, err = strconv.ParseBool(val)
		if err != nil {
			log.Fatalf("invalid STAR_SIMULATION_MODE %q: %v", val, err)
		}
	}

	if err := app.Run(context.Background(), config); err != nil {
		log.Fatalf("Application error: %v", err)
	}
}
