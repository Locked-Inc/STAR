# TDA4VM Yocto Build System
# Simplified commands for building embedded Linux images

.PHONY: help setup build build-minimal build-default build-ros2 clean shell logs status \
        docker-build docker-up docker-down docker-logs menuconfig \
        flash-info build-info

# Default target
help: ## Show this help message
	@echo "TDA4VM Yocto Build System"
	@echo "========================="
	@echo ""
	@echo "Quick Start:"
	@echo "  make setup     - Set up Docker environment"
	@echo "  make build     - Build EdgeAI image (recommended)"
	@echo "  make build-ros2 - Build EdgeAI + ROS2 image"
	@echo "  make shell     - Enter development shell"
	@echo ""
	@echo "Available targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-15s %s\n", $$1, $$2}'

# Environment variables
DOCKER_COMPOSE := docker-compose
CONTAINER_NAME := tda4vm-yocto-builder
MACHINE := j721e-evm
IMAGE_EDGEAI := tisdk-edgeai-image
IMAGE_EDGEAI_ROS2 := tisdk-edgeai-ros2-image
IMAGE_DEFAULT := tisdk-default-image
IMAGE_MINIMAL := core-image-minimal

# Docker management
setup: ## Set up the Docker build environment
	@echo "🚀 Setting up TDA4VM Yocto build environment..."
	@echo "📋 Checking Docker installation..."
	@docker --version >/dev/null 2>&1 || (echo "❌ Docker not found. Please install Docker first." && exit 1)
	@docker-compose --version >/dev/null 2>&1 || (echo "❌ docker-compose not found. Please install docker-compose." && exit 1)
	@echo "🏗️  Building Docker image (this may take a while)..."
	$(DOCKER_COMPOSE) build --build-arg USER_ID=$$(id -u) --build-arg GROUP_ID=$$(id -g)
	@echo "✅ Setup complete! Run 'make shell' to enter build environment."

docker-build: ## Build the Docker image
	$(DOCKER_COMPOSE) build --build-arg USER_ID=$$(id -u) --build-arg GROUP_ID=$$(id -g)

docker-up: ## Start the Docker container
	$(DOCKER_COMPOSE) up -d

docker-down: ## Stop and remove the Docker container
	$(DOCKER_COMPOSE) down

docker-logs: ## Show Docker container logs
	$(DOCKER_COMPOSE) logs -f

# Build targets
build: docker-up ## Build the EdgeAI image for TDA4VM (recommended)
	@echo "🎯 Building EdgeAI image for TDA4VM..."
	$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
		cd build && \
		. conf/setenv && \
		MACHINE=$(MACHINE) bitbake $(IMAGE_EDGEAI)"
	@echo "✅ EdgeAI image build complete!"
	@make flash-info

build-default: docker-up ## Build the default SDK image
	@echo "🎯 Building default SDK image..."
	$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
		cd build && \
		. conf/setenv && \
		MACHINE=$(MACHINE) bitbake $(IMAGE_DEFAULT)"
	@echo "✅ Default image build complete!"
	@make flash-info

build-minimal: docker-up ## Build minimal image for testing
	@echo "🎯 Building minimal image..."
	$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
		cd build && \
		. conf/setenv && \
		MACHINE=$(MACHINE) bitbake $(IMAGE_MINIMAL)"
	@echo "✅ Minimal image build complete!"
	@make flash-info

build-ros2: docker-up ## Build EdgeAI image with ROS2 Jazzy support
	@echo "🤖 Building EdgeAI + ROS2 image..."
	$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
		cd build && \
		. conf/setenv && \
		MACHINE=$(MACHINE) bitbake $(IMAGE_EDGEAI_ROS2)"
	@echo "✅ EdgeAI + ROS2 image build complete!"
	@make flash-info

# Development targets
shell: docker-up ## Enter interactive development shell
	@echo "🐚 Starting development shell..."
	@echo "💡 Available images: $(IMAGE_EDGEAI), $(IMAGE_EDGEAI_ROS2), $(IMAGE_DEFAULT), $(IMAGE_MINIMAL)"
	@echo "💡 To build: MACHINE=$(MACHINE) bitbake <image-name>"
	$(DOCKER_COMPOSE) exec yocto-builder /bin/bash

menuconfig: docker-up ## Configure kernel options
	@echo "⚙️  Opening kernel menuconfig..."
	$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
		cd build && \
		. conf/setenv && \
		MACHINE=$(MACHINE) bitbake virtual/kernel -c menuconfig"

# Maintenance targets
clean: docker-up ## Clean build artifacts
	@echo "🧹 Cleaning build artifacts..."
	$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
		cd build && \
		. conf/setenv && \
		bitbake -c cleanall $(IMAGE_EDGEAI) $(IMAGE_DEFAULT) $(IMAGE_MINIMAL)"
	@echo "✅ Clean complete!"

clean-all: docker-up ## Clean everything including downloads
	@echo "🧹 Deep cleaning everything (downloads, sstate, tmp)..."
	@read -p "This will delete ALL build data. Continue? (y/N): " confirm; \
	if [ "$$confirm" = "y" ] || [ "$$confirm" = "Y" ]; then \
		$(DOCKER_COMPOSE) exec yocto-builder bash -c "\
			rm -rf build/arago-tmp-* downloads/* sstate-cache/*"; \
		echo "✅ Deep clean complete!"; \
	else \
		echo "❌ Cancelled."; \
	fi

# Information targets
status: ## Show build status and information
	@echo "📊 TDA4VM Build Environment Status"
	@echo "=================================="
	@echo "Machine: $(MACHINE)"
	@echo "Container: $(CONTAINER_NAME)"
	@echo "Images: EdgeAI ($(IMAGE_EDGEAI)), EdgeAI+ROS2 ($(IMAGE_EDGEAI_ROS2)), Default ($(IMAGE_DEFAULT)), Minimal ($(IMAGE_MINIMAL))"
	@echo ""
	@echo "🐳 Docker Status:"
	@$(DOCKER_COMPOSE) ps 2>/dev/null || echo "Container not running"
	@echo ""
	@if [ -d "build/arago-tmp-external-arm-glibc/deploy/images/$(MACHINE)" ]; then \
		echo "📦 Built Images:"; \
		ls -la "build/arago-tmp-external-arm-glibc/deploy/images/$(MACHINE)/"*.wic* 2>/dev/null | head -5 || echo "No images found"; \
	else \
		echo "📦 No built images found. Run 'make build' to create them."; \
	fi

build-info: ## Show information about built images
	@echo "📦 Built Images Information"
	@echo "=========================="
	@if [ -d "build/arago-tmp-external-arm-glibc/deploy/images/$(MACHINE)" ]; then \
		echo "Images in: build/arago-tmp-external-arm-glibc/deploy/images/$(MACHINE)/"; \
		ls -lh build/arago-tmp-external-arm-glibc/deploy/images/$(MACHINE)/*.wic* 2>/dev/null || echo "No .wic images found"; \
	else \
		echo "No images found. Run 'make build' first."; \
	fi

flash-info: ## Show SD card flashing instructions
	@echo ""
	@echo "💾 SD Card Flashing Instructions"
	@echo "================================"
	@echo "1. Find your built image:"
	@echo "   build/arago-tmp-external-arm-glibc/deploy/images/$(MACHINE)/"
	@echo ""
	@echo "2. Flash to SD card (Linux/macOS):"
	@echo "   sudo dd if=<image>.wic of=/dev/sdX bs=1M status=progress"
	@echo ""
	@echo "3. Or use balenaEtcher for a GUI option"
	@echo ""
	@echo "4. Insert SD card into TDA4VM board and boot"

# Advanced targets
logs: ## Show recent build logs
	@echo "📋 Recent build logs..."
	@if [ -f "build/arago-tmp-external-arm-glibc/log/cooker/$(MACHINE)/console-latest.log" ]; then \
		tail -50 "build/arago-tmp-external-arm-glibc/log/cooker/$(MACHINE)/console-latest.log"; \
	else \
		echo "No build logs found."; \
	fi

# Utility targets
reset-docker: ## Reset Docker environment completely
	@echo "🔄 Resetting Docker environment..."
	$(DOCKER_COMPOSE) down -v
	docker system prune -f
	@echo "✅ Docker environment reset. Run 'make setup' to rebuild."

# Version information
version: ## Show version information
	@echo "TDA4VM Yocto Build System"
	@echo "========================="
	@echo "SDK: TI Processor SDK Analytics 11.01.07.05"
	@echo "Yocto: Scarthgap"
	@echo "BitBake: 2.8.0"
	@echo "Target: J721E EVM (TDA4VM)"
	@echo "Date: $$(date)"