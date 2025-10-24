# Makefile for RPI Minimal OS
# Provides clean targets and build commands

BUILDROOT_VERSION := 2025.02
BUILDROOT_DIR := buildroot-$(BUILDROOT_VERSION)
BUILDROOT_URL := https://buildroot.org/downloads/buildroot-$(BUILDROOT_VERSION).tar.gz
PROJECT_DIR := $(shell pwd)
BUILD_DIR := $(PROJECT_DIR)/$(BUILDROOT_DIR)
OUTPUT_DIR := $(BUILD_DIR)/output
IMAGE := $(OUTPUT_DIR)/images/sdcard.img

# Clean PATH for WSL compatibility
export PATH := /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

.PHONY: all build clean distclean download configure help

# Default target
all: build

# Download Buildroot if not present
download:
	@if [ ! -d "$(BUILD_DIR)" ]; then \
		echo "Downloading Buildroot $(BUILDROOT_VERSION)..."; \
		wget -O /tmp/buildroot.tar.gz "$(BUILDROOT_URL)"; \
		tar -xzf /tmp/buildroot.tar.gz -C "$(PROJECT_DIR)"; \
		rm /tmp/buildroot.tar.gz; \
	else \
		echo "Buildroot already downloaded."; \
	fi

# Configure Buildroot with external tree
configure: download
	@echo "Configuring Buildroot..."
	@cd $(BUILD_DIR) && $(MAKE) BR2_EXTERNAL="$(PROJECT_DIR)" rpi5_64_defconfig

# Build the image
build: configure
	@echo "Starting build..."
	@cd $(BUILD_DIR) && $(MAKE)
	@echo ""
	@echo "=== Build Complete ==="
	@echo "Output image: $(IMAGE)"
	@echo ""
	@echo "To flash to SD card:"
	@echo "  sudo dd if=$(IMAGE) of=/dev/sdX bs=4M status=progress conv=fsync"
	@echo ""
	@echo "Image includes: Systemd, OpenJDK 17, ROS2 Jazzy, Python 3"

# Clean build artifacts (keeps downloads)
clean:
	@echo "Cleaning build artifacts..."
	@if [ -d "$(BUILD_DIR)" ]; then \
		cd $(BUILD_DIR) && $(MAKE) clean; \
		echo "Build cleaned."; \
	else \
		echo "Nothing to clean - build directory doesn't exist."; \
	fi

# Deep clean - removes everything including downloads
distclean:
	@echo "Performing distclean (removes all build artifacts and downloads)..."
	@if [ -d "$(BUILD_DIR)" ]; then \
		cd $(BUILD_DIR) && $(MAKE) distclean; \
		echo "Distclean complete."; \
	else \
		echo "Nothing to clean - build directory doesn't exist."; \
	fi

# Remove entire buildroot directory
mrproper: distclean
	@echo "Removing Buildroot directory..."
	@rm -rf $(BUILD_DIR)
	@echo "Buildroot directory removed."

# Help target
help:
	@echo "RPI Minimal OS Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make build      - Download, configure, and build the image"
	@echo "  make clean      - Clean build artifacts (keeps downloads)"
	@echo "  make distclean  - Deep clean (removes all build artifacts and downloads)"
	@echo "  make mrproper   - Remove entire Buildroot directory"
	@echo "  make download   - Download Buildroot if not present"
	@echo "  make configure  - Configure Buildroot with external tree"
	@echo "  make help       - Show this help message"
	@echo ""
	@echo "Common workflows:"
	@echo "  make clean build     - Clean and rebuild"
	@echo "  make distclean build - Full clean rebuild"
	@echo "  make mrproper build  - Complete fresh build"
