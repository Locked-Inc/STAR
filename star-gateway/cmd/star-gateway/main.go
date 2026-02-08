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
	config.TransportMode = manager.TransportMode(os.Getenv("TRANSPORT_MODE"))

	// Check if the provided transport mode is valid otherwise set to default (ModeAuto)
	if !config.TransportMode.IsValid() {
		log.Printf("Invalid TRANSPORT_MODE %q, defaulting to %q", config.TransportMode, manager.ModeAuto)
		config.TransportMode = manager.ModeAuto
	}

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
