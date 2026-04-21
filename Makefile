# STAR Project Makefile
# Wraps Docker commands for development tasks to simplify workflow.
# Standardizes build, format, and shell access without memorizing Docker flags.

IMAGE_NAME := star-ros2-dev
WORK_DIR := /workspaces/STAR
CURRENT_DIR := $(shell pwd)

.PHONY: help build-image build format shell test proto-gen proto-gen-firmware proto-gen-go proto-gen-ros2 test-rx72n coverage-rx72n proto-check-nanopb-sync doxygen-html doxygen-pdfs doxygen-pdf-src doxygen-pdf-deps doxygen-clean build-rx72n build-rx72n-release format-rx72n check-rx72n ci-rx72n devcontainer devcontainer-rebuild devcontainer-shell blinky build-blinky clean-blinky flash-blinky flash-blinky-sci motor0 motor0-forward motor0-reverse motor1 motor1-forward motor1-reverse motor2 motor2-forward motor2-reverse motor3 motor3-forward motor3-reverse motor-all motor-stop motor-clean none

help:
	@echo "STAR Project Development Helper"
	@echo "-------------------------------"
	@echo "Targets:"
	@echo "  make build        - Build the ROS2 project (runs ./build-ros2.sh in Docker)"
	@echo "  make format       - Format code (runs ./scripts/ros2/format-ros2.sh in Docker)"
	@echo "  make shell        - Start an interactive shell in a new ephemeral container"
	@echo ""
	@echo "RX72N Firmware:"
	@echo "  make blinky              - Clean build + flash bare-metal LED test via E2 Lite (one-shot)"
	@echo "  make build-blinky        - Build bare-metal P54 LED test (requires GNURX toolchain)"
	@echo "  make flash-blinky        - Flash blinky.elf via E2 Lite (builds first, FINE @ 250kbps)"
	@echo "  make flash-blinky-sci    - Flash blinky.elf via USB-UART SCI Boot Mode (SW4 Pin1=ON)"
	@echo "  make clean-blinky        - Remove blinky build artifacts"
	@echo "  make motor0              - Build + flash motor_spin_test, Motor 0 only, both directions"
	@echo "  make motor0-forward      - Build + flash, Motor 0 only, forward sweep (0..+100..0)"
	@echo "  make motor0-reverse      - Build + flash, Motor 0 only, reverse sweep (0..-100..0)"
	@echo "  make motor1              - Same for Motor 1 (both directions)"
	@echo "  make motor1-forward      - Motor 1 forward only"
	@echo "  make motor1-reverse      - Motor 1 reverse only"
	@echo "  make motor2              - Same for Motor 2 (both directions)"
	@echo "  make motor2-forward      - Motor 2 forward only"
	@echo "  make motor2-reverse      - Motor 2 reverse only"
	@echo "  make motor3              - Same for Motor 3 (both directions)"
	@echo "  make motor3-forward      - Motor 3 forward only"
	@echo "  make motor3-reverse      - Motor 3 reverse only"
	@echo "  make motor-all           - Build + flash motor_spin_test driving all 4 motors"
	@echo "  make motor-stop          - Build + flash with all motors disabled (DRVOFF held HIGH)"
	@echo "  make none                - Flash standalone software-standby firmware (~0.5 uA, wake on reset)"
	@echo "  make motor-clean         - Remove motor_spin_test build artifacts"
	@echo "  make build-rx72n         - Build RX72N firmware (Debug, requires GNURX toolchain)"
	@echo "  make build-rx72n-release - Build RX72N firmware (Release, requires GNURX toolchain)"
	@echo "  make format-rx72n        - Auto-format firmware C/H files with clang-format"
	@echo "  make check-rx72n         - Check firmware formatting (exit 1 if any file differs)"
	@echo "  make ci-rx72n            - Run full local CI pipeline (format, build, test, clang-tidy)"
	@echo "  make proto-gen-firmware  - Generate nanopb protos for RX72N firmware"
	@echo "  make test-rx72n          - Run RX72N unit tests and show coverage summary"
	@echo "  make coverage-rx72n      - Show coverage summary without rebuilding"
	@echo "  make doxygen-html        - Generate unified HTML docs (all features, all cross-links)"
	@echo "  make doxygen-pdf-src     - Generate PDF for firmware src/"
	@echo "  make doxygen-pdf-<lib>   - Generate PDF for a specific library (e.g. doxygen-pdf-rx_pid)"
	@echo "  make doxygen-pdfs        - Generate all per-module PDFs (default 4 parallel jobs)"
	@echo "                             Override: make doxygen-pdfs DOXY_PDF_JOBS=8"
	@echo "  make doxygen-pdf-deps    - Install missing LaTeX packages for PDF builds (varwidth, collection-latexextra)"
	@echo "  make doxygen-clean       - Remove generated Doxygen output"
	@echo "  Note: GNURX toolchain required for build targets (rx-elf-gcc)"
	@echo ""
	@echo "Protocol Buffers:"
	@echo "  make proto-gen    - Generate all proto code and setup Go modules (Go, TS, C/nanopb)"
	@echo "  make proto-check-nanopb-sync - Verify LidarScan nanopb bounds are in sync"
	@echo ""
	@echo "Dev Container (no VS Code required):"
	@echo "  make devcontainer         - Build and start the dev container"
	@echo "  make devcontainer-rebuild - Force rebuild (applies devcontainer.json changes)"
	@echo "  make devcontainer-shell   - Open a shell in the running dev container"

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
	@docker run --rm -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) ./scripts/ros2/format-ros2.sh

# Check formatting without modifying files (for CI/pre-commit)
check: build-image
	@echo "Checking ROS2 code formatting..."
	@docker run --rm -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) ./scripts/ros2/format-ros2.sh --check

# Start an ephemeral interactive shell
shell: build-image
	@echo "Starting ephemeral shell..."
	@docker run --rm -it -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) /bin/bash

# ------------------------------------------------------------
# Dev Container (runs without VS Code via devcontainer CLI)
#
# Prerequisites (Arch Linux):
#   sudo pacman -S nodejs npm docker docker-buildx
#   sudo systemctl enable --now docker
#   sudo usermod -aG docker $USER   # then log out and back in
#   sudo npm install -g @devcontainers/cli
#
# First run pulls osrf/ros:jazzy-desktop (~1 GB) and installs all
# devcontainer features -- expect 10-30 min. Subsequent runs use
# the cached image and start in seconds.
#
# DOCKER_BUILDKIT=1 is required because the Dockerfile uses
# --mount=type=cache which is a BuildKit-only feature.
# ------------------------------------------------------------

devcontainer:
	@echo "Starting dev container..."
	@DOCKER_BUILDKIT=1 devcontainer up --workspace-folder .

devcontainer-rebuild:
	@echo "Rebuilding dev container..."
	@DOCKER_BUILDKIT=1 devcontainer up --workspace-folder . --remove-existing-container

devcontainer-shell:
	@echo "Opening shell in dev container..."
	@devcontainer exec --workspace-folder . bash

# ------------------------------------------------------------
# Protocol Buffer Generation (monorepo-wide)
# ------------------------------------------------------------

# Aggregate target: generate for all consumers (and verify nanopb option sync)
proto-gen: proto-gen-go proto-gen-ros2 proto-gen-firmware proto-check-nanopb-sync

# Generate code using buf for all configured plugins (runs from workspace root)
proto-gen-go:
	@echo "Generating protocol buffers (Go, TS, C, C++)..."
	@buf generate star-proto/proto
	@echo "[PASS] Code generated under star-proto/gen/"
	@echo "Setting up Go module for generated code..."
	@if [ ! -f star-proto/gen/go/go.mod ]; then \
		cd star-proto/gen/go && go mod init github.com/Locked-Inc/star-proto/gen/go; \
	fi
	@cd star-proto/gen/go && go mod tidy
	@go work sync
	@echo "[PASS] Go workspace synchronized"

# Placeholder for ROS2-specific generation (if distinct tooling is added)
proto-gen-ros2:
	@echo "ROS2 proto generation is handled via buf; no separate step."

# Copy nanopb outputs to firmware include directory.
# ui.pb.* is included because gateway_service.pb.h depends on ui.pb.h for
# OdometryData and LidarScan types used in ForwardTelemetryRequest.
proto-gen-firmware: proto-gen-go
	@echo "Preparing firmware nanopb headers/sources..."
	@mkdir -p star-rx72n-firmware/libs/rx_nanopb/inc/gen/star/v1
	@set -e; \
	dst=star-rx72n-firmware/libs/rx_nanopb/inc/gen/star/v1; \
	src_gen=star-proto/gen/nanopb/star/v1; \
	rm -f "$$dst"/*.pb.h "$$dst"/*.pb.c; \
	for header in "$$src_gen"/*.pb.h; do \
		base=$$(basename "$$header" .pb.h); \
		cp -v "$$src_gen/$$base.pb.h" "$$src_gen/$$base.pb.c" "$$dst/"; \
	done
	@echo "[PASS] Firmware protos updated"

# Test RX72N firmware (regenerates protos first) and show coverage summary
test-rx72n: proto-gen-firmware
	@echo "Running RX72N unit tests..."
	@cd star-rx72n-firmware/tests && bash run_tests.sh
	@$(MAKE) --no-print-directory coverage-rx72n

# Show coverage summary for RX72N firmware (requires tests/build to exist)
coverage-rx72n:
	@echo ""
	@echo "=== RX72N Coverage Summary ==="
	@gcovr \
		--object-directory star-rx72n-firmware/tests/build \
		--filter 'star-rx72n-firmware/libs/' \
		--exclude 'star-rx72n-firmware/libs/threadx/' \
		--exclude 'star-rx72n-firmware/libs/rx_nanopb/nanopb/' \
		--exclude 'star-rx72n-firmware/libs/rx_nanopb/inc/gen/' \
		--exclude 'star-rx72n-firmware/libs/rx_hal/inc/rx72n_port_regs.h' \
		--exclude 'star-rx72n-firmware/libs/rx_hal/inc/rx_port_utils.h' \
		--exclude '.*/third_party/.*' \
		-j 1 \
		--print-summary \
		2>/dev/null || echo "(run 'make test-rx72n' first to generate coverage data)"

# Build RX72N firmware binary (Debug) - requires GNURX toolchain (rx-elf-gcc)
build-blinky:
	@echo "Building bare-metal blinky (P54 LED test, requires rx-elf-gcc)..."
	@PATH="$(GNURX_BIN):$$PATH" $(MAKE) -C star-rx72n-firmware/blinky

clean-blinky:
	@PATH="$(GNURX_BIN):$$PATH" $(MAKE) -C star-rx72n-firmware/blinky clean

# Flash blinky.elf to RX72N via E2 Lite (FINE interface, 250 kbps).
# Requires: E2 Lite plugged in, board externally powered, SW4 Pin1=OFF Pin2=OFF.
flash-blinky: build-blinky
	@PATH="$(GNURX_BIN):$$PATH" bash scripts/flash-rx72n.sh star-rx72n-firmware/blinky/blinky.elf e2lite

# Flash blinky.elf to RX72N via USB-UART SCI Boot Mode.
# Requires: USB-UART cable providing /dev/ttyACM0, SW4 Pin1=ON Pin2=OFF,
# and a power-cycle AFTER setting the switch so the MCU enters boot mode.
flash-blinky-sci: build-blinky
	@PATH="$(GNURX_BIN):$$PATH" bash scripts/flash-rx72n.sh star-rx72n-firmware/blinky/blinky.elf sci

# All-in-one alias matching the 'make motorN' / 'make none' pattern:
# clean build + flash via E2 Lite. Same end-state as
# 'make build-blinky flash-blinky'.
blinky: clean-blinky build-blinky flash-blinky
	@true

# ------------------------------------------------------------
# motor_spin_test -- open-loop 4-motor sweep on the production STAR PCB.
#   - 'make motorN' builds + flashes with only Motor N enabled (safe).
#   - 'make motor-all' builds + flashes all 4 motors at once.
# Each target does a clean build so the MOTOR_MASK macro reliably
# regenerates the ELF. Requires GNURX toolchain (rx-elf-gcc) and an
# E2 Lite probe attached for the flash step.
# ------------------------------------------------------------

MOTOR_TEST_DIR := star-rx72n-firmware/motor_spin_test
MOTOR_TEST_ELF := $(MOTOR_TEST_DIR)/motor_spin_test.elf
GNURX_BIN      := /opt/gnurx/bin

# Internal helper -- usage: $(call build_and_flash_motor,<MASK>,<DUTY_MIN>,<DUTY_MAX>,<label>)
# Prepends the GNURX toolchain to PATH so rx-elf-gcc / rx-elf-objcopy
# resolve without the user having to export PATH first.
define build_and_flash_motor
	@echo "==> Building motor_spin_test ($(4)) with MOTOR_MASK=$(1), duty=[$(2)..$(3)]..."
	@PATH="$(GNURX_BIN):$$PATH" $(MAKE) --no-print-directory -C $(MOTOR_TEST_DIR) clean
	@PATH="$(GNURX_BIN):$$PATH" $(MAKE) --no-print-directory -C $(MOTOR_TEST_DIR) \
	    MOTOR_MASK=$(1) MOTOR_DUTY_MIN=$(2) MOTOR_DUTY_MAX=$(3)
	@echo "==> Flashing $(4) via E2 Lite..."
	@PATH="$(GNURX_BIN):$$PATH" bash scripts/flash-rx72n.sh $(MOTOR_TEST_ELF) e2lite
endef

# Bidirectional sweep (-100 .. +100 .. -100)
motor0:
	$(call build_and_flash_motor,0x1,-100,100,Motor 0 bidirectional)
motor1:
	$(call build_and_flash_motor,0x2,-100,100,Motor 1 bidirectional)
motor2:
	$(call build_and_flash_motor,0x4,-100,100,Motor 2 bidirectional)
motor3:
	$(call build_and_flash_motor,0x8,-100,100,Motor 3 bidirectional)
motor-all:
	$(call build_and_flash_motor,0xF,-100,100,all 4 motors bidirectional)

# Forward-only sweep (0 .. +100 .. 0)
motor0-forward:
	$(call build_and_flash_motor,0x1,0,100,Motor 0 forward only)
motor1-forward:
	$(call build_and_flash_motor,0x2,0,100,Motor 1 forward only)
motor2-forward:
	$(call build_and_flash_motor,0x4,0,100,Motor 2 forward only)
motor3-forward:
	$(call build_and_flash_motor,0x8,0,100,Motor 3 forward only)

# Reverse-only sweep (0 .. -100 .. 0)
motor0-reverse:
	$(call build_and_flash_motor,0x1,-100,0,Motor 0 reverse only)
motor1-reverse:
	$(call build_and_flash_motor,0x2,-100,0,Motor 1 reverse only)
motor2-reverse:
	$(call build_and_flash_motor,0x4,-100,0,Motor 2 reverse only)
motor3-reverse:
	$(call build_and_flash_motor,0x8,-100,0,Motor 3 reverse only)

# All motors disabled (DRVOFF stays HIGH; safe stop)
motor-stop:
	$(call build_and_flash_motor,0x0,-100,100,all motors disabled)

# Park the chip in software standby -- lowest power "do nothing" state
# achievable without a hard reset. Uses a dedicated tiny firmware
# (none/main.c, ~74 bytes) that sets SBYCR.SSBY = 1 then issues WAIT.
# The MCU stops the CPU clock and most peripheral clocks; typical IDD
# is ~0.5 uA per the RX72N datasheet table 51.x. Wake requires the
# external reset button or a re-flash via E2 Lite.
none:
	@echo "==> Building 'none' firmware (software standby, no motors, no LEDs)..."
	@PATH="$(GNURX_BIN):$$PATH" $(MAKE) --no-print-directory -C star-rx72n-firmware/none clean
	@PATH="$(GNURX_BIN):$$PATH" $(MAKE) --no-print-directory -C star-rx72n-firmware/none
	@echo "==> Flashing 'none' firmware via E2 Lite..."
	@PATH="$(GNURX_BIN):$$PATH" bash scripts/flash-rx72n.sh star-rx72n-firmware/none/none.mot e2lite

motor-clean:
	@$(MAKE) --no-print-directory -C $(MOTOR_TEST_DIR) clean

build-rx72n:
	@echo "Building RX72N firmware (Debug)..."
	@cd star-rx72n-firmware && bash build.sh debug

# Build RX72N firmware binary (Release) - requires GNURX toolchain (rx-elf-gcc)
build-rx72n-release:
	@echo "Building RX72N firmware (Release)..."
	@cd star-rx72n-firmware && bash build.sh release

# Auto-format firmware C/H files with clang-format (modifies files in place)
format-rx72n:
	@echo "Formatting RX72N firmware C/H files..."
	@bash $(FIRMWARE_DIR)/scripts/format_code.sh

# Check firmware formatting without modifying files (for CI/pre-commit)
check-rx72n:
	@echo "Checking RX72N firmware formatting..."
	@bash $(FIRMWARE_DIR)/scripts/format_code.sh --check

# Run the full firmware CI pipeline locally (mirrors .github/workflows/firmware-unit-tests.yml).
# Steps: format check -> cross-compile -> unit tests -> clang-tidy
# Stops on first failure. Run before pushing to catch CI issues early.
ci-rx72n:
	@echo ""
	@echo "=========================================="
	@echo " RX72N Local CI Pipeline"
	@echo "=========================================="
	@echo ""
	@echo "[1/5] C23 pattern enforcement..."
	@c23_fail=0; \
	for f in $$(find $(FIRMWARE_DIR)/libs $(FIRMWARE_DIR)/src $(FIRMWARE_DIR)/tests \
	  -name '*.c' -o -name '*.h' | grep -v '/build/' | grep -v '/third_party/' | grep -v '/nanopb/'); do \
	  if grep -qn '_Static_assert' "$$f" 2>/dev/null; then \
	    echo "[FAIL] C11 _Static_assert in $$f (use static_assert)"; c23_fail=1; fi; \
	  if grep -qE '= \{0\}' "$$f" 2>/dev/null; then \
	    echo "[FAIL] C11 = {0} in $$f (use = {})"; c23_fail=1; fi; \
	  if grep -qn '#include <stdbool.h>' "$$f" 2>/dev/null; then \
	    echo "[FAIL] Unnecessary #include <stdbool.h> in $$f"; c23_fail=1; fi; \
	done; \
	if [ "$$c23_fail" -ne 0 ]; then \
	  echo ""; echo "Fix C11 patterns above, then retry."; exit 1; \
	fi
	@echo ""
	@echo "[2/5] clang-format check..."
	@bash $(FIRMWARE_DIR)/scripts/format_code.sh --check
	@echo ""
	@echo "[3/5] Cross-compile (GNURX)..."
	@cd $(FIRMWARE_DIR) && bash build.sh
	@echo ""
	@echo "[4/5] Unit tests..."
	@cd $(FIRMWARE_DIR)/tests && cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-18 2>&1 | tail -3
	@cmake --build $(FIRMWARE_DIR)/tests/build --parallel 2>&1 | tail -3
	@ctest --test-dir $(FIRMWARE_DIR)/tests/build --output-on-failure
	@echo ""
	@echo "[5/5] clang-tidy (SEI CERT C) -- CMake-integrated, mirrors CI exactly..."
	@cd $(FIRMWARE_DIR)/tests && cmake -B build/tidy \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_CLANG_TIDY=ON \
		-DCMAKE_C_COMPILER=clang-18 \
		-Wno-dev 2>&1 | tail -3
	@cmake --build $(FIRMWARE_DIR)/tests/build/tidy --parallel 2>&1
	@echo ""
	@echo "=========================================="
	@echo " [PASS] All CI checks passed!"
	@echo "=========================================="

# Verify LidarScan nanopb max_count bounds are identical in ui.options and gateway_service.options.
# Fails with a clear SYNC ERROR message if any value diverges between the two files.
# Run this in CI after buf generate to enforce lockstep updates.
proto-check-nanopb-sync:
	@echo "Checking LidarScan nanopb bound sync across option files..."
	@cd star-proto && bash scripts/check_nanopb_sync.sh

# ------------------------------------------------------------
# Doxygen Documentation (RX72N firmware)
# Two-mode build:
#   HTML  -- one unified run over src/ + libs/ (Doxyfile.main)
#   PDF   -- one run per module (Doxyfile.pdf.base + per-module INPUT override)
#
# Performance note: /workspaces is mounted over WSL2's 9p (Plan 9) bridge to
# the Windows host filesystem. xelatex opens every \includegraphics file on
# each pass; with many graph PDFs this causes thousands of 9p round-trips.
# Fix: copy each generated latex/ dir to /tmp (Linux tmpfs) before compiling,
# then copy refman.pdf back.  /tmp is kernel tmpfs -- no disk or 9p I/O.
# ------------------------------------------------------------

FIRMWARE_DIR := star-rx72n-firmware
DOXY_OUT     := $(FIRMWARE_DIR)/docs/doxygen

# Discover all libraries automatically -- new libs under libs/ are picked up
# without any Makefile changes.
LIBS         := $(notdir $(wildcard $(CURDIR)/$(FIRMWARE_DIR)/libs/*))
PDF_TARGETS  := doxygen-pdf-src $(addprefix doxygen-pdf-, $(LIBS))

# Generate unified HTML reference (all features, all cross-links)
# Uses Doxyfile.main: INPUT = src + libs, GENERATE_HTML = YES, GENERATE_LATEX = NO
doxygen-html:
	@echo "Generating Doxygen HTML documentation..."
	@mkdir -p $(DOXY_OUT)
	@cd $(FIRMWARE_DIR) && doxygen Doxyfile.main
	@echo "Done: $(DOXY_OUT)/html/index.html"

# Number of concurrent PDF jobs. Each job copies a latex dir to /tmp then runs xelatex
# twice + xdvipdfmx. On a 16 GB machine, 4 jobs is safe; raise on higher-memory hosts.
# Override at runtime: make doxygen-pdfs DOXY_PDF_JOBS=8
DOXY_PDF_JOBS ?= 4

# Build all per-module PDFs (parallel, capped to avoid OOM from simultaneous xelatex + cp)
doxygen-pdfs:
	$(MAKE) -j$(DOXY_PDF_JOBS) $(PDF_TARGETS)

# PDF for firmware src/
doxygen-pdf-src:
	$(call build_doxy_pdf,src,STAR RX72N - Firmware Source,src)

# PDF per library -- e.g. make doxygen-pdf-rx_pid, make doxygen-pdf-threadx
# The % matches the lib name; INPUT is set to libs/<name>
doxygen-pdf-%:
	$(call build_doxy_pdf,$*,STAR RX72N - $* Library,libs/$*)

# Helper: build one per-module PDF
# Usage: $(call build_doxy_pdf, name, project_title, input_path)
#   $(1) = short name     (e.g. rx_pid)
#   $(2) = project title  (e.g. STAR RX72N - rx_pid Library)
#   $(3) = INPUT path     relative to FIRMWARE_DIR (e.g. libs/rx_pid)
# Output PDF: $(DOXY_OUT)/pdf/$(1).pdf  (doxygen runs in /tmp, only PDF written to 9p)
define build_doxy_pdf
	@if ! command -v xelatex >/dev/null 2>&1; then \
	  echo "ERROR: xelatex not found. Rebuild the Docker image (texlive-xetex is in Dockerfile)."; \
	  exit 1; \
	fi
	@echo "=== Building PDF: $(2) ==="
	@mkdir -p "$(DOXY_OUT)/pdf"
	@ramdisk=$$(mktemp -d /tmp/doxygen-pdf-$(1)-XXXXXX); \
	 dest_pdf=$$(readlink -f $(DOXY_OUT)/pdf)/$(1).pdf; \
	 tmp=$$(mktemp $(FIRMWARE_DIR)/Doxyfile.$(1).XXXXXX); \
	 printf '@INCLUDE = Doxyfile.pdf.base\nPROJECT_NAME = "$(2)"\nINPUT = $(3)\nOUTPUT_DIRECTORY = %s\nWARN_LOGFILE = %s/warnings.log\n' "$$ramdisk" "$$ramdisk" > "$$tmp"; \
	 cd $(FIRMWARE_DIR) && doxygen "$$(basename $$tmp)"; \
	 rm -f "$$tmp"; \
	 echo "  tmpfs build dir: $$ramdisk"; \
	 echo "  Deduplicating multiply-defined Doxygen labels..."; \
	 grep -rl 'label{doc-' "$$ramdisk/latex" | xargs -r sed -i '/\\label{doc-[a-z-]*-members}/d'; \
	 echo "  Converting EPS graphs to properly-sized PDFs (gs respects BoundingBox)..."; \
	 for eps in "$$ramdisk/latex"/*.eps; do \
	   [ -f "$$eps" ] || continue; \
	   pdf="$${eps%.eps}.pdf"; \
	   [ -f "$$pdf" ] && continue; \
	   bb=$$(grep "^%%BoundingBox:" "$$eps" | grep -v atend | head -1); \
	   [ -z "$$bb" ] && continue; \
	   llx=$$(echo "$$bb" | awk '{print $$2}'); lly=$$(echo "$$bb" | awk '{print $$3}'); \
	   urx=$$(echo "$$bb" | awk '{print $$4}'); ury=$$(echo "$$bb" | awk '{print $$5}'); \
	   w=$$((urx - llx)); h=$$((ury - lly)); \
	   gs -q -dBATCH -dNOPAUSE -dSAFER -sDEVICE=pdfwrite \
	     -dFIXEDMEDIA -dDEVICEWIDTHPOINTS=$$w -dDEVICEHEIGHTPOINTS=$$h \
	     -sOutputFile=$$pdf \
	     -c "<</PageOffset [-$$llx -$$lly]>> setpagedevice" \
	     -f "$$eps" 2>/dev/null || true; \
	 done; \
	 echo "  Creating placeholder PDFs for any @msc blocks still missing a PDF..."; \
	 PLACEHOLDER=$$(ls "$$ramdisk/latex"/inline_mscgraph_*.pdf 2>/dev/null | head -1); \
	 if [ -z "$$PLACEHOLDER" ]; then \
	   PLACEHOLDER="$$ramdisk/latex/inline_mscgraph_placeholder.pdf"; \
	   printf '%%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj 2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj 3 0 obj<</Type/Page/MediaBox[0 0 200 100]/Parent 2 0 R>>endobj\nxref\n0 4\n0000000000 65535 f \n0000000009 00000 n \n0000000058 00000 n \n0000000115 00000 n \ntrailer<</Size 4/Root 1 0 R>>\nstartxref\n190\n%%%%EOF' > "$$PLACEHOLDER"; \
	 fi; \
	 for eps in "$$ramdisk/latex"/inline_mscgraph_*.eps; do \
	   [ -f "$$eps" ] || continue; \
	   pdf="$${eps%.eps}.pdf"; \
	   [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	 done; \
	 for msc in "$$ramdisk/latex"/inline_mscgraph_*.msc; do \
	   [ -f "$$msc" ] || continue; \
	   pdf="$${msc%.msc}.pdf"; \
	   [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	 done; \
	 echo "  Fixing refman.tex: \\\\+, \\\\_, and replacing helvet with TeX Gyre Heros for xelatex..."; \
	 sed -i 's/\\newcommand{\\+}{.*}/\\renewcommand{\\+}{}\n  \\renewcommand{\\_}{\\char95}/' "$$ramdisk/latex/refman.tex"; \
	 sed -i 's/\\usepackage\[scaled=.90\]{helvet}/\\setsansfont[Scale=.90]{TeX Gyre Heros}/' "$$ramdisk/latex/refman.tex"; \
	 sed -i 's/\\usepackage{doxygen}/\\usepackage{doxygen}\n\\usepackage[export]{adjustbox}/' "$$ramdisk/latex/refman.tex"; \
	 echo "  Replacing uncaptioned figure[H] envs with center blocks, removing nopagebreak, capping sizes..."; \
	 find "$$ramdisk/latex" -name "*.tex" ! -name "refman.tex" ! -name "doxygen.sty" | xargs -r perl -i -0pe 's/\\begin\{figure\}\[H\]\n\\begin\{center\}(.*?)\\end\{center\}\n\\end\{figure\}/\\begin{center}$$1\\end{center}/gs; s/\\nopagebreak//g'; \
	 find "$$ramdisk/latex" -name "*.tex" | xargs -r sed -i 's/\\includegraphics\[width=/\\includegraphics[max width=\\linewidth,max totalheight=.8\\textheight,width=/g'; \
	 cd "$$ramdisk/latex" && xelatex -interaction=nonstopmode -no-pdf refman.tex; \
	 xelatex -interaction=nonstopmode -no-pdf refman.tex; \
	 xdvipdfmx -E -o refman.pdf refman.xdv; \
	 if [ ! -f "$$ramdisk/latex/refman.pdf" ]; then \
	   echo "ERROR: xdvipdfmx failed to produce refman.pdf -- see $$ramdisk/latex/refman.log"; \
	   rm -rf "$$ramdisk"; \
	   exit 1; \
	 fi; \
	 cp "$$ramdisk/latex/refman.pdf" "$$dest_pdf" || { echo "ERROR: failed to write PDF to $$dest_pdf"; rm -rf "$$ramdisk"; exit 1; }; \
	 rm -rf "$$ramdisk"; \
	 echo "Done: $$dest_pdf"
endef

# Remove all generated Doxygen output (HTML + all per-module PDFs)
doxygen-clean:
	@echo "Removing Doxygen output..."
	@rm -rf $(DOXY_OUT)
	@echo "Done."

# Install LaTeX packages required by doxygen-pdf-* that are not in the
# Dockerfile's base texlive set. xelatex itself and fonts-urw-base35 are
# installed via the Dockerfile (rebuild the Docker image if missing).
#
# Packages installed via tlmgr user mode (TeX Live 2023 historic archive):
#   varwidth, collection-latexextra
# Wrapper scripts:
#   epstopdf  -> /usr/local/bin/epstopdf (delegates to gs)
# Format rebuild:
#   fmtutil-user --byfmt xelatex  (fixes expl3 version mismatch after installs)
doxygen-pdf-deps:
	@echo "Installing LaTeX deps for doxygen-pdf-*..."
	@if ! command -v xelatex >/dev/null 2>&1; then \
	  echo "ERROR: xelatex not found. Rebuild the Docker image (texlive-xetex is in Dockerfile)."; \
	  exit 1; \
	fi
	@echo "  Configuring tlmgr user mode repository (TeX Live 2023 archive)..."
	@tlmgr init-usertree 2>/dev/null || true
	@tlmgr --usermode option repository https://ftp.math.utah.edu/pub/tex/historic/systems/texlive/2023/tlnet-final 2>/dev/null || true
	@echo "  Installing packages via tlmgr..."
	@tlmgr --usermode install \
	    varwidth \
	    collection-latexextra \
	    2>&1 | grep -E "(install:|running|Error)" || true
	@if ! command -v epstopdf >/dev/null 2>&1; then \
	  bash scripts/devtools/install-epstopdf-wrapper.sh; \
	else \
	  echo "  epstopdf already present -- skipping"; \
	fi
	@echo "  Regenerating xelatex format (fixes expl3 version mismatch)..."
	@fmtutil-user --byfmt xelatex 2>&1 | grep -E "(INFO|Error|error)" | tail -3
	@echo "Done: LaTeX deps installed. Re-run 'make doxygen-pdf-<name>' to generate a PDF."

