# STAR Project Makefile
# Wraps Docker commands for development tasks to simplify workflow.
# Standardizes build, format, and shell access without memorizing Docker flags.

IMAGE_NAME := star-ros2-dev
CONTAINER_NAME := star-dev
WORK_DIR := /workspaces/STAR
CURRENT_DIR := $(shell pwd)

.PHONY: help build-image build format shell up exec stop test proto-gen proto-gen-firmware proto-gen-go proto-gen-ros2 test-rx72n proto-check-nanopb-sync doxygen doxygen-pdf doxygen-clean

help:
	@echo "STAR Project Development Helper"
	@echo "-------------------------------"
	@echo "Targets:"
	@echo "  make build        - Build the ROS2 project (runs ./build-ros2.sh in Docker)"
	@echo "  make format       - Format code (runs ./scripts/format-ros2.sh in Docker)"
	@echo "  make shell        - Start an interactive shell in a new ephemeral container"
	@echo ""
	@echo "RX72N Firmware:"
	@echo "  make proto-gen-firmware - Generate nanopb protos for RX72N firmware"
	@echo "  make test-rx72n         - Run RX72N unit tests (regenerates protos first)"
	@echo "  make doxygen            - Generate Doxygen HTML docs for RX72N firmware"
	@echo "  make doxygen-pdf        - Generate Doxygen HTML + LaTeX PDF (requires lualatex)"
	@echo "  make doxygen-clean      - Remove generated Doxygen output"
	@echo "  Note: RX72N firmware is built in e2 studio, not via Makefile"
	@echo ""
	@echo "Protocol Buffers:"
	@echo "  make proto-gen    - Generate all proto code and setup Go modules (Go, TS, C/nanopb)"
	@echo "  make proto-check-nanopb-sync - Verify LidarScan nanopb bounds are in sync"
	@echo ""
	@echo "Persistent Container (optional):"
	@echo "  make up           - Start a persistent background container named '$(CONTAINER_NAME)'"
	@echo "  make exec         - Connect to the running persistent container"
	@echo "  make stop         - Stop and remove the persistent container"

# Build the Docker image (cached)
build-image:
	@echo "Building Docker image $(IMAGE_NAME)..."
	@docker build -t $(IMAGE_NAME) .

# Run the build script in an ephemeral container
build: build-image
	@echo "Running ROS2 build..."
	@docker run --rm -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) ./build-ros2.sh

# Run the format script in an ephemeral container
format: build-image
	@echo "Formatting ROS2 code..."
	@docker run --rm -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) ./scripts/format-ros2.sh

# Check formatting without modifying files (for CI/pre-commit)
check: build-image
	@echo "Checking ROS2 code formatting..."
	@docker run --rm -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) ./scripts/format-ros2.sh --check

# Start an ephemeral interactive shell
shell: build-image
	@echo "Starting ephemeral shell..."
	@docker run --rm -it -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) /bin/bash

# Start a persistent background container
up: build-image
	@if [ ! "$$(docker ps -q -f name=$(CONTAINER_NAME))" ]; then \
		if [ "$$(docker ps -aq -f name=$(CONTAINER_NAME))" ]; then \
			echo "Removing old stopped container..."; \
			docker rm $(CONTAINER_NAME); \
		fi; \
		echo "Starting $(CONTAINER_NAME)..."; \
		docker run -d -it --name $(CONTAINER_NAME) -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) /bin/bash; \
	else \
		echo "$(CONTAINER_NAME) is already running."; \
	fi

# Connect to the persistent container
exec:
	@echo "Connecting to $(CONTAINER_NAME)..."
	@docker exec -it $(CONTAINER_NAME) /bin/bash

# Stop the persistent container
stop:
	@echo "Stopping $(CONTAINER_NAME)..."
	@docker stop $(CONTAINER_NAME) || true
	@docker rm $(CONTAINER_NAME) || true

# ------------------------------------------------------------
# Protocol Buffer Generation (monorepo-wide)
# ------------------------------------------------------------

# Aggregate target: generate for all consumers (and verify nanopb option sync)
proto-gen: proto-gen-go proto-gen-ros2 proto-gen-firmware proto-check-nanopb-sync

# Generate code using buf for all configured plugins (runs from workspace root)
proto-gen-go:
	@echo "Generating protocol buffers (Go, TS, C, C++)..."
	@buf generate star-proto/proto
	@echo "✓ Code generated under star-proto/gen/"
	@echo "Setting up Go module for generated code..."
	@if [ ! -f star-proto/gen/go/go.mod ]; then \
		cd star-proto/gen/go && go mod init github.com/Locked-Inc/star-proto/gen/go; \
	fi
	@cd star-proto/gen/go && go mod tidy
	@go work sync
	@echo "✓ Go workspace synchronized"

# Placeholder for ROS2-specific generation (if distinct tooling is added)
proto-gen-ros2:
	@echo "ROS2 proto generation is handled via buf; no separate step."

# Copy nanopb outputs to firmware include directory.
# ui.pb.* is included because gateway_service.pb.h depends on ui.pb.h for
# OdometryData and LidarScan types used in ForwardTelemetryRequest.
proto-gen-firmware: proto-gen-go
	@echo "Preparing firmware nanopb headers/sources..."
	@mkdir -p e2-studio-star-rx72n-firmware/libs/rx_nanopb/inc/gen/star/v1
	@set -e; \
	dst=e2-studio-star-rx72n-firmware/libs/rx_nanopb/inc/gen/star/v1; \
	src_gen=star-proto/gen/nanopb/star/v1; \
	rm -f "$$dst"/*.pb.h "$$dst"/*.pb.c; \
	for header in "$$src_gen"/*.pb.h; do \
		base=$$(basename "$$header" .pb.h); \
		cp -v "$$src_gen/$$base.pb.h" "$$src_gen/$$base.pb.c" "$$dst/"; \
	done
	@echo "✓ Firmware protos updated"

# Test RX72N firmware (regenerates protos first)
test-rx72n: proto-gen-firmware
	@echo "Running RX72N unit tests..."
	@cd e2-studio-star-rx72n-firmware/tests && bash run_tests.sh

# Verify LidarScan nanopb max_count bounds are identical in ui.options and gateway_service.options.
# Fails with a clear SYNC ERROR message if any value diverges between the two files.
# Run this in CI after buf generate to enforce lockstep updates.
proto-check-nanopb-sync:
	@echo "Checking LidarScan nanopb bound sync across option files..."
	@cd star-proto && bash scripts/check_nanopb_sync.sh

# ------------------------------------------------------------
# Doxygen Documentation (RX72N firmware)
# ------------------------------------------------------------

# Generate HTML reference docs only
doxygen:
	@echo "Generating Doxygen HTML documentation..."
	@cd e2-studio-star-rx72n-firmware && doxygen Doxyfile
	@echo "Done: e2-studio-star-rx72n-firmware/docs/doxygen/html/index.html"

# Generate HTML + compile LaTeX to PDF
# Uses lualatex (no register limit) instead of pdflatex (hits eTex 32768 cap
# on our large codebase). Doxygen 1.16.1+ is required (installed from GitHub
# in the Dockerfile -- Ubuntu 24.04 apt ships 1.9.8 which uses broken tabu).
LATEX_DIR = e2-studio-star-rx72n-firmware/docs/doxygen/latex

doxygen-pdf:
	@echo "Generating Doxygen HTML + LaTeX..."
	@cd e2-studio-star-rx72n-firmware && doxygen Doxyfile
	@echo "Copying placeholder PDFs for any @msc blocks that failed to render..."
	@PLACEHOLDER=$$(ls $(LATEX_DIR)/inline_mscgraph_*.pdf 2>/dev/null | head -1); \
	 if [ -n "$$PLACEHOLDER" ]; then \
	   for eps in $(LATEX_DIR)/inline_mscgraph_*.eps; do \
	     pdf="$${eps%.eps}.pdf"; \
	     [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	   done; \
	   for tex in $(LATEX_DIR)/inline_mscgraph_*.tex; do \
	     pdf="$${tex%.tex}.pdf"; \
	     [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	   done; \
	 fi
	@echo "Compiling refman.pdf (latexmk + lualatex)..."
	@cd $(LATEX_DIR) && latexmk -lualatex -interaction=nonstopmode -f refman.tex
	@echo "Done: $(LATEX_DIR)/refman.pdf"

# Remove all generated Doxygen output
doxygen-clean:
	@echo "Removing Doxygen output..."
	@rm -rf e2-studio-star-rx72n-firmware/docs/doxygen
	@echo "Done."
