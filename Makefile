# STAR Project Makefile
# Wraps Docker commands for development tasks to simplify workflow.
# Standardizes build, format, and shell access without memorizing Docker flags.

IMAGE_NAME := star-ros2-dev
CONTAINER_NAME := star-dev
WORK_DIR := /workspaces/STAR
CURRENT_DIR := $(shell pwd)

.PHONY: help build-image build format shell up exec stop test proto-gen proto-gen-firmware proto-gen-go proto-gen-ros2 test-rx72n proto-check-nanopb-sync doxygen doxygen-pdf doxygen-pdf-deps doxygen-clean build-rx72n build-rx72n-release format-rx72n check-rx72n

help:
	@echo "STAR Project Development Helper"
	@echo "-------------------------------"
	@echo "Targets:"
	@echo "  make build        - Build the ROS2 project (runs ./build-ros2.sh in Docker)"
	@echo "  make format       - Format code (runs ./scripts/format-ros2.sh in Docker)"
	@echo "  make shell        - Start an interactive shell in a new ephemeral container"
	@echo ""
	@echo "RX72N Firmware:"
	@echo "  make build-rx72n         - Build RX72N firmware (Debug, requires GNURX toolchain)"
	@echo "  make build-rx72n-release - Build RX72N firmware (Release, requires GNURX toolchain)"
	@echo "  make format-rx72n        - Auto-format firmware C/H files with clang-format"
	@echo "  make check-rx72n         - Check firmware formatting (exit 1 if any file differs)"
	@echo "  make proto-gen-firmware  - Generate nanopb protos for RX72N firmware"
	@echo "  make test-rx72n          - Run RX72N unit tests (regenerates protos first)"
	@echo "  make doxygen             - Generate Doxygen HTML docs for RX72N firmware"
	@echo "  make doxygen-pdf         - Generate Doxygen HTML + LaTeX PDF (requires lualatex)"
	@echo "  make doxygen-pdf-deps    - Install missing LaTeX packages for doxygen-pdf (varwidth, luaotfload)"
	@echo "  make doxygen-clean       - Remove generated Doxygen output"
	@echo "  Note: GNURX toolchain required for build targets (rx-elf-gcc)"
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
	@if docker ps -q -f name=^$(CONTAINER_NAME)$$ | grep -q .; then \
		echo "$(CONTAINER_NAME) is already running."; \
	else \
		docker rm -f $(CONTAINER_NAME) 2>/dev/null || true; \
		echo "Starting $(CONTAINER_NAME)..."; \
		docker run -d -it --name $(CONTAINER_NAME) -v "$(CURRENT_DIR):$(WORK_DIR)" -w $(WORK_DIR) $(IMAGE_NAME) /bin/bash; \
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

# Build RX72N firmware binary (Debug) - requires GNURX toolchain (rx-elf-gcc)
build-rx72n:
	@echo "Building RX72N firmware (Debug)..."
	@cd e2-studio-star-rx72n-firmware && bash build.sh debug

# Build RX72N firmware binary (Release) - requires GNURX toolchain (rx-elf-gcc)
build-rx72n-release:
	@echo "Building RX72N firmware (Release)..."
	@cd e2-studio-star-rx72n-firmware && bash build.sh release

# Auto-format firmware C/H files with clang-format (modifies files in place)
format-rx72n:
	@echo "Formatting RX72N firmware C/H files..."
	@bash $(FIRMWARE_DIR)/scripts/format_code.sh

# Check firmware formatting without modifying files (for CI/pre-commit)
check-rx72n:
	@echo "Checking RX72N firmware formatting..."
	@bash $(FIRMWARE_DIR)/scripts/format_code.sh --check

# Verify LidarScan nanopb max_count bounds are identical in ui.options and gateway_service.options.
# Fails with a clear SYNC ERROR message if any value diverges between the two files.
# Run this in CI after buf generate to enforce lockstep updates.
proto-check-nanopb-sync:
	@echo "Checking LidarScan nanopb bound sync across option files..."
	@cd star-proto && bash scripts/check_nanopb_sync.sh

# ------------------------------------------------------------
# Doxygen Documentation (RX72N firmware)
# ------------------------------------------------------------

FIRMWARE_DIR = e2-studio-star-rx72n-firmware
DOXYGEN_OUT  = $(FIRMWARE_DIR)/docs/doxygen
LATEX_DIR    = $(DOXYGEN_OUT)/latex

# Generate HTML reference docs only
doxygen:
	@echo "Generating Doxygen HTML documentation..."
	@mkdir -p $(DOXYGEN_OUT)
	@cd $(FIRMWARE_DIR) && doxygen Doxyfile
	@echo "Done: $(DOXYGEN_OUT)/html/index.html"

# Generate HTML + compile LaTeX to PDF
# Uses lualatex (no register limit) instead of pdflatex (hits eTex 32768 cap
# on our large codebase). Doxygen 1.16.1+ is required (installed from GitHub
# in the Dockerfile -- Ubuntu 24.04 apt ships 1.9.8 which uses broken tabu).
# Run 'make doxygen-pdf-deps' first if any required tools are missing.
#
# Note: CALL_GRAPH and CALLER_GRAPH are disabled for the PDF run only.
# The main Doxyfile keeps them enabled for HTML (SVG hover graphs).
# In a PDF they add ~3000 call/caller graph images and 20+ min compile time
# with no clickable benefit. Re-enable by editing the sed line below.
doxygen-pdf:
	@if ! kpsewhich varwidth.sty >/dev/null 2>&1 || ! kpsewhich luaotfload-main.lua >/dev/null 2>&1; then \
	  echo "ERROR: Missing LaTeX packages. Run: make doxygen-pdf-deps"; \
	  exit 1; \
	fi
	@if ! command -v epstopdf >/dev/null 2>&1; then \
	  echo "ERROR: epstopdf not found. Run: make doxygen-pdf-deps"; \
	  exit 1; \
	fi
	@echo "Generating Doxygen LaTeX (call/caller graphs disabled for PDF)..."
	@mkdir -p $(DOXYGEN_OUT)
	@tmp=$$(mktemp $(FIRMWARE_DIR)/Doxyfile.pdf.XXXXXX); \
	 sed -e 's/^CALL_GRAPH\s*=.*/CALL_GRAPH             = NO/' \
	     -e 's/^CALLER_GRAPH\s*=.*/CALLER_GRAPH           = NO/' \
	     -e 's/^GENERATE_HTML\s*=.*/GENERATE_HTML          = NO/' \
	     $(FIRMWARE_DIR)/Doxyfile > "$$tmp"; \
	 cd $(FIRMWARE_DIR) && doxygen "$$(basename $$tmp)"; \
	 rm -f "$$tmp"
	@echo "Converting mscgen EPS diagrams to PDF..."
	@for eps in $(LATEX_DIR)/inline_mscgraph_*.eps; do \
	  [ -f "$$eps" ] || continue; \
	  pdf="$${eps%.eps}.pdf"; \
	  [ -f "$$pdf" ] || epstopdf "$$eps" --outfile="$$pdf" 2>/dev/null || true; \
	done
	@echo "Copying placeholder PDFs for any @msc blocks that still have no PDF..."
	@PLACEHOLDER=$$(ls $(LATEX_DIR)/inline_mscgraph_*.pdf 2>/dev/null | head -1); \
	 if [ -z "$$PLACEHOLDER" ]; then \
	   PLACEHOLDER=$(LATEX_DIR)/inline_mscgraph_placeholder.pdf; \
	   printf '%%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj 2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj 3 0 obj<</Type/Page/MediaBox[0 0 200 100]/Parent 2 0 R>>endobj\nxref\n0 4\n0000000000 65535 f \n0000000009 00000 n \n0000000058 00000 n \n0000000115 00000 n \ntrailer<</Size 4/Root 1 0 R>>\nstartxref\n190\n%%%%EOF' > "$$PLACEHOLDER"; \
	 fi; \
	 for eps in $(LATEX_DIR)/inline_mscgraph_*.eps; do \
	   [ -f "$$eps" ] || continue; \
	   pdf="$${eps%.eps}.pdf"; \
	   [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	 done; \
	 for msc in $(LATEX_DIR)/inline_mscgraph_*.msc; do \
	   [ -f "$$msc" ] || continue; \
	   pdf="$${msc%.msc}.pdf"; \
	   [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	 done
	@echo "Compiling refman.pdf (latexmk + lualatex)..."
	@rm -f $(LATEX_DIR)/refman.fdb_latexmk $(LATEX_DIR)/refman.pdf
	@cd $(LATEX_DIR) && latexmk -lualatex -interaction=nonstopmode -f refman.tex
	@echo "Done: $(LATEX_DIR)/refman.pdf"

# Remove all generated Doxygen output
doxygen-clean:
	@echo "Removing Doxygen output..."
	@rm -rf $(DOXYGEN_OUT)
	@echo "Done."

# Install LaTeX packages required by doxygen-pdf that are not in the
# Dockerfile's base texlive set (texlive-luatex and texlive-latex-extra are
# not available as apt packages in this environment).
#
# Packages installed via tlmgr user mode (TeX Live 2023 historic archive):
#   varwidth, lualibs, lua-uni-algos, collection-latexextra, collection-luatex
# Packages installed manually from CTAN (non-relocatable, cannot use tlmgr):
#   luaotfload  -> ~/texmf/tex/luatex/luaotfload/
# Wrapper scripts:
#   epstopdf    -> /usr/local/bin/epstopdf (delegates to gs)
# Format rebuild:
#   fmtutil-user --byfmt lualatex  (fixes expl3 version mismatch after installs)
doxygen-pdf-deps:
	@echo "Installing LaTeX deps for doxygen-pdf..."
	@echo "  Configuring tlmgr user mode repository (TeX Live 2023 archive)..."
	@tlmgr init-usertree 2>/dev/null || true
	@tlmgr --usermode option repository https://ftp.math.utah.edu/pub/tex/historic/systems/texlive/2023/tlnet-final 2>/dev/null || true
	@echo "  Installing packages via tlmgr..."
	@tlmgr --usermode install \
	    varwidth lualibs lua-uni-algos \
	    collection-latexextra collection-luatex \
	    2>&1 | grep -E "(install:|running|Error)" || true
	@if ! kpsewhich luaotfload-main.lua >/dev/null 2>&1; then \
	  echo "  Downloading luaotfload from CTAN (non-relocatable, manual install)..."; \
	  tmp=$$(mktemp -d); \
	  curl -sL "https://mirrors.ctan.org/macros/luatex/generic/luaotfload.zip" -o "$$tmp/luaotfload.zip"; \
	  unzip -q "$$tmp/luaotfload.zip" -d "$$tmp"; \
	  mkdir -p ~/texmf/tex/luatex/luaotfload ~/texmf/tex/latex/luaotfload; \
	  cp "$$tmp/luaotfload/"*.lua ~/texmf/tex/luatex/luaotfload/; \
	  cp "$$tmp/luaotfload/"*.sty ~/texmf/tex/latex/luaotfload/; \
	  cp "$$tmp/luaotfload/"*.cnf ~/texmf/tex/luatex/luaotfload/; \
	  mktexlsr ~/texmf; \
	  rm -rf "$$tmp"; \
	else \
	  echo "  luaotfload-main.lua already present -- skipping"; \
	fi
	@if ! command -v epstopdf >/dev/null 2>&1; then \
	  bash scripts/install-epstopdf-wrapper.sh; \
	else \
	  echo "  epstopdf already present -- skipping"; \
	fi
	@echo "  Regenerating lualatex format (fixes expl3 version mismatch)..."
	@fmtutil-user --byfmt lualatex 2>&1 | grep -E "(INFO|Error|error)" | tail -3
	@echo "Done: LaTeX deps installed. Re-run 'make doxygen-pdf' to generate the PDF."

