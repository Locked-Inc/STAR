# STAR Project Makefile
# Wraps Docker commands for development tasks to simplify workflow.
# Standardizes build, format, and shell access without memorizing Docker flags.

IMAGE_NAME := star-ros2-dev
CONTAINER_NAME := star-dev
WORK_DIR := /workspaces/STAR
CURRENT_DIR := $(shell pwd)

.PHONY: help build-image build format shell up exec stop test proto-gen proto-gen-firmware proto-gen-go proto-gen-ros2 build-rx72n test-rx72n clean-rx72n

help:
	@echo "STAR Project Development Helper"
	@echo "-------------------------------"
	@echo "Targets:"
	@echo "  make build        - Build the ROS2 project (runs ./build-ros2.sh in Docker)"
	@echo "  make format       - Format code (runs ./scripts/format-ros2.sh in Docker)"
	@echo "  make shell        - Start an interactive shell in a new ephemeral container"
	@echo ""
	@echo "RX72N Firmware:"
	@echo "  make build-rx72n  - Build RX72N firmware (regenerates protos first)"
	@echo "  make test-rx72n   - Run RX72N unit tests (regenerates protos first)"
	@echo ""
	@echo "Protocol Buffers:"
	@echo "  make proto-gen    - Generate all proto code (Go, TypeScript, C/nanopb)"
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
	@if [ ! "$$$(docker ps -q -f name=$(CONTAINER_NAME))" ]; then \
		if [ "$$$(docker ps -aq -f name=$(CONTAINER_NAME))" ]; then \
			echo "Removing old stopped container..."; \
			docker rm $(CONTAINER_NAME); \
		fi; \
		echo "Starting $(CONTAINER_NAME)..."; \
		docker run -d -it --name $(CONTAINER_NAME) -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) /bin/bash; \
	else 
		echo "$(CONTAINER_NAME) is already running."; 
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

# Aggregate target: generate for all consumers
proto-gen: proto-gen-go proto-gen-ros2 proto-gen-firmware

# Generate code using buf for all configured plugins (runs in star-proto)
proto-gen-go:
	@echo "Generating protocol buffers (Go, TS, C, C++)..."
	@cd star-proto && buf generate proto
	@echo "✓ Code generated under star-proto/gen/"

# Placeholder for ROS2-specific generation (if distinct tooling is added)
proto-gen-ros2:
	@echo "ROS2 proto generation is handled via buf; no separate step."

# Copy nanopb outputs to firmware include directory
proto-gen-firmware: proto-gen-go
	@echo "Preparing firmware nanopb headers/sources..."
	@mkdir -p star-rx72n-firmware/lib/rx_nanopb/inc/gen
	@cp -v star-proto/gen/nanopb/star/v1/*.pb.h star-proto/gen/nanopb/star/v1/*.pb.c star-rx72n-firmware/lib/rx_nanopb/inc/gen/
	@echo "✓ Firmware protos updated: star-rx72n-firmware/lib/rx_nanopb/inc/gen"

# Build RX72N firmware (regenerates protos first)
build-rx72n: proto-gen
	@echo "Building RX72N firmware..."
	@cd star-rx72n-firmware && bash build.sh

# Test RX72N firmware (regenerates protos first)
test-rx72n: proto-gen
	@echo "Running RX72N unit tests..."
	@cd star-rx72n-firmware/tests && bash run_tests.sh

# Clean RX72N build artifacts
clean-rx72n:
	@echo "Cleaning RX72N build directory..."
	@cd star-rx72n-firmware && bash clean.sh
	@echo "✓ RX72N build cleaned"
