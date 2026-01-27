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
	if config.TransportMode == "" {
		// Note: We are keeping SPI-only mode as default, add --transport-mode flag for opt-in USB support:
		// TODO: Change default to "auto" or "force-usb" once USB support is stable. And before testing on actual hardware.
		config.TransportMode = manager.ModeForceSPI // Default: no changes
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
