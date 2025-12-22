# Makefile for RPI Minimal OS
# Provides clean targets and build commands

BUILDROOT_VERSION := 2025.02
BUILDROOT_DIR := buildroot-$(BUILDROOT_VERSION)
BUILDROOT_URL := https://buildroot.org/downloads/buildroot-$(BUILDROOT_VERSION).tar.gz
PROJECT_DIR := $(shell pwd)
BUILD_DIR := $(PROJECT_DIR)/$(BUILDROOT_DIR)
OUTPUT_DIR := $(BUILD_DIR)/output
IMAGE := $(OUTPUT_DIR)/images/sdcard.img

.PHONY: all build clean distclean download configure help qemu qemu-build qemu-graphical qemu-ssh qemu-clean

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
	@cd $(BUILD_DIR) && $(MAKE) -j$$(nproc)
	@echo ""
	@echo "=== Build Complete ==="
	@echo "Output image: $(IMAGE)"
	@echo ""
	@echo "To flash to SD card:"
	@echo "  sudo dd if=$(IMAGE) of=/dev/sdX bs=4M status=progress conv=fsync"
	@echo ""
	@echo "Image includes: Systemd, ROS2 Jazzy, Python 3"

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

# QEMU Testing (uses generic ARM64 'virt' machine - Pi5/BCM2712 not yet supported)
# Use 'qemu' to just run, 'qemu-build' to build first
qemu:
	@chmod +x $(PROJECT_DIR)/scripts/qemu-test.sh
	@$(PROJECT_DIR)/scripts/qemu-test.sh run

qemu-build: build qemu

qemu-graphical:
	@chmod +x $(PROJECT_DIR)/scripts/qemu-test.sh
	@$(PROJECT_DIR)/scripts/qemu-test.sh graphical

qemu-ssh:
	@chmod +x $(PROJECT_DIR)/scripts/qemu-test.sh
	@$(PROJECT_DIR)/scripts/qemu-test.sh ssh

qemu-clean:
	@chmod +x $(PROJECT_DIR)/scripts/qemu-test.sh
	@$(PROJECT_DIR)/scripts/qemu-test.sh clean

# Help target
help:
	@echo "STAR Robot OS Build System"
	@echo ""
	@echo "Build targets:"
	@echo "  make build         - Download, configure, and build the image"
	@echo "  make clean         - Clean build artifacts (keeps downloads)"
	@echo "  make distclean     - Deep clean (removes all build artifacts and downloads)"
	@echo "  make mrproper      - Remove entire Buildroot directory"
	@echo "  make download      - Download Buildroot if not present"
	@echo "  make configure     - Configure Buildroot with external tree"
	@echo ""
	@echo "QEMU testing targets:"
	@echo "  make qemu           - Boot image in QEMU (serial console)"
	@echo "  make qemu-build     - Build then boot in QEMU"
	@echo "  make qemu-graphical - Boot with graphical display"
	@echo "  make qemu-ssh       - SSH into running QEMU instance"
	@echo "  make qemu-clean     - Remove QEMU working files"
	@echo ""
	@echo "Common workflows:"
	@echo "  make clean build      - Clean and rebuild"
	@echo "  make distclean build  - Full clean rebuild"
	@echo "  make mrproper build   - Complete fresh build"
	@echo "  make build qemu       - Build and test in QEMU"
	@echo ""
	@echo "QEMU environment variables:"
	@echo "  QEMU_MEMORY=4096 make qemu   - Set RAM (default: 2048MB)"
	@echo "  QEMU_CPUS=2 make qemu        - Set CPUs (default: 4)"
	@echo "  QEMU_SSH_PORT=3333 make qemu - Set SSH port (default: 2222)"
	@echo ""
	@echo "Note: QEMU uses generic ARM64 'virt' machine (Pi5/BCM2712 not supported)"
	@echo "      Good for testing systemd, services, userspace - not Pi5 hardware"
