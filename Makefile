# STAR Project Makefile
# Wraps Docker commands for development tasks to simplify workflow.
# Standardizes build, format, and shell access without memorizing Docker flags.

IMAGE_NAME := star-ros2-dev
CONTAINER_NAME := star-dev
WORK_DIR := /workspaces/STAR
CURRENT_DIR := $(shell pwd)

.PHONY: help build-image build format shell up exec stop test proto-gen proto-gen-firmware proto-gen-go proto-gen-ros2 test-rx72n proto-check-nanopb-sync doxygen-html doxygen-pdfs doxygen-pdf-src doxygen-pdf-deps doxygen-clean build-rx72n build-rx72n-release format-rx72n check-rx72n

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

FIRMWARE_DIR := e2-studio-star-rx72n-firmware
DOXY_OUT     := $(FIRMWARE_DIR)/docs/doxygen

# Discover all libraries automatically -- new libs under libs/ are picked up
# without any Makefile changes.
LIBS         := $(notdir $(wildcard $(FIRMWARE_DIR)/libs/*))
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
	$(call build_doxy_pdf,src,STAR RX72N - Firmware Source,src,pdf/src)

# PDF per library -- e.g. make doxygen-pdf-rx_pid, make doxygen-pdf-threadx
# The % matches the lib name; INPUT is set to libs/<name>
doxygen-pdf-%:
	$(call build_doxy_pdf,$*,STAR RX72N - $* Library,libs/$*,pdf/$*)

# Helper: build one per-module PDF
# Usage: $(call build_doxy_pdf, name, project_title, input_path, out_subdir)
#   $(1) = short name     (e.g. rx_pid)
#   $(2) = project title  (e.g. STAR RX72N - rx_pid Library)
#   $(3) = INPUT path     relative to FIRMWARE_DIR (e.g. libs/rx_pid)
#   $(4) = output subdir  under DOXY_OUT            (e.g. pdf/rx_pid)
define build_doxy_pdf
	@if ! command -v xelatex >/dev/null 2>&1; then \
	  echo "ERROR: xelatex not found. Rebuild the Docker image (texlive-xetex is in Dockerfile)."; \
	  exit 1; \
	fi
	@if ! command -v epstopdf >/dev/null 2>&1; then \
	  echo "ERROR: epstopdf not found. Run: make doxygen-pdf-deps"; \
	  exit 1; \
	fi
	@echo "=== Building PDF: $(2) ==="
	@mkdir -p "$(DOXY_OUT)/$(4)"
	@tmp=$$(mktemp $(FIRMWARE_DIR)/Doxyfile.$(1).XXXXXX); \
	 printf '@INCLUDE = Doxyfile.pdf.base\nPROJECT_NAME = "$(2)"\nINPUT = $(3)\nOUTPUT_DIRECTORY = docs/doxygen/$(4)\nWARN_LOGFILE = docs/doxygen/$(4)/warnings.log\n' > "$$tmp"; \
	 cd $(FIRMWARE_DIR) && doxygen "$$(basename $$tmp)"; \
	 rm -f "$$tmp"
	@echo "  Converting mscgen EPS diagrams to PDF..."
	@for eps in $(DOXY_OUT)/$(4)/latex/inline_mscgraph_*.eps; do \
	  [ -f "$$eps" ] || continue; \
	  pdf="$${eps%.eps}.pdf"; \
	  [ -f "$$pdf" ] || epstopdf "$$eps" --outfile="$$pdf" 2>/dev/null || true; \
	done
	@echo "  Creating placeholder PDFs for any @msc blocks still missing a PDF..."
	@PLACEHOLDER=$$(ls $(DOXY_OUT)/$(4)/latex/inline_mscgraph_*.pdf 2>/dev/null | head -1); \
	 if [ -z "$$PLACEHOLDER" ]; then \
	   PLACEHOLDER=$(DOXY_OUT)/$(4)/latex/inline_mscgraph_placeholder.pdf; \
	   printf '%%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj 2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj 3 0 obj<</Type/Page/MediaBox[0 0 200 100]/Parent 2 0 R>>endobj\nxref\n0 4\n0000000000 65535 f \n0000000009 00000 n \n0000000058 00000 n \n0000000115 00000 n \ntrailer<</Size 4/Root 1 0 R>>\nstartxref\n190\n%%%%EOF' > "$$PLACEHOLDER"; \
	 fi; \
	 for eps in $(DOXY_OUT)/$(4)/latex/inline_mscgraph_*.eps; do \
	   [ -f "$$eps" ] || continue; \
	   pdf="$${eps%.eps}.pdf"; \
	   [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	 done; \
	 for msc in $(DOXY_OUT)/$(4)/latex/inline_mscgraph_*.msc; do \
	   [ -f "$$msc" ] || continue; \
	   pdf="$${msc%.msc}.pdf"; \
	   [ -f "$$pdf" ] || cp "$$PLACEHOLDER" "$$pdf"; \
	 done
	@echo "  Copying latex dir to tmpfs (/tmp) to avoid WSL2 9p I/O overhead..."
	@latex_abs=$$(readlink -f $(DOXY_OUT)/$(4)/latex); \
	 module_pdf="$${latex_abs%/latex}.pdf"; \
	 ramdisk=$$(mktemp -d /tmp/doxygen-pdf-$(1)-XXXXXX); \
	 cp -a "$$latex_abs"/. "$$ramdisk/"; \
	 rm -f "$$ramdisk/refman.fdb_latexmk" "$$ramdisk/refman.pdf" "$$ramdisk/refman.aux" "$$ramdisk/refman.toc" "$$ramdisk/refman.out"; \
	 echo "  tmpfs build dir: $$ramdisk ($$(du -sh $$latex_abs 2>/dev/null | cut -f1) copied)"; \
	 echo "  Deduplicating multiply-defined Doxygen labels..."; \
	 grep -rl 'label{doc-' "$$ramdisk" | xargs -r sed -i '/\\label{doc-[a-z-]*-members}/d'; \
	 echo "  Fixing refman.tex: \\\\+, \\\\_, and replacing helvet with TeX Gyre Heros for xelatex..."; \
	 sed -i 's/\\newcommand{\\+}{.*}/\\renewcommand{\\+}{}\n  \\renewcommand{\\_}{\\char"005F}/' "$$ramdisk/refman.tex"; \
	 sed -i 's/\\usepackage\[scaled=.90\]{helvet}/\\setsansfont[Scale=.90]{TeX Gyre Heros}/' "$$ramdisk/refman.tex"; \
	 sed -i 's/\\usepackage{doxygen}/\\usepackage{doxygen}\n\\usepackage[export]{adjustbox}\n\\let\\OrigIncludegraphics\\includegraphics\n\\renewcommand{\\includegraphics}[2][]{\\OrigIncludegraphics[max width=\\linewidth,max totalheight=0.8\\textheight,#1]{#2}}/' "$$ramdisk/refman.tex"; \
	 echo "  Fixing figure placement: [H] -> [htbp] to prevent figures overflowing page boundaries..."; \
	 grep -rl 'begin{figure}\[H\]' "$$ramdisk" | xargs -r sed -i 's/\\begin{figure}\[H\]/\\begin{figure}[htbp]/g'; \
	 cd "$$ramdisk" && xelatex -interaction=nonstopmode -no-pdf refman.tex; \
	 xelatex -interaction=nonstopmode -no-pdf refman.tex; \
	 xdvipdfmx -E -o refman.pdf refman.xdv; \
	 cp refman.log "$$latex_abs/refman.log"; \
	 if [ ! -f "$$ramdisk/refman.pdf" ]; then \
	   echo "ERROR: xdvipdfmx failed to produce refman.pdf -- see $(DOXY_OUT)/$(4)/latex/refman.log"; \
	   rm -rf "$$ramdisk"; \
	   exit 1; \
	 fi; \
	 cp "$$ramdisk/refman.pdf" "$$latex_abs/refman.pdf" || { echo "ERROR: failed to copy refman.pdf back to $$latex_abs"; rm -rf "$$ramdisk"; exit 1; }; \
	 cp "$$ramdisk/refman.pdf" "$$module_pdf" || { echo "ERROR: failed to write named PDF to $$module_pdf"; rm -rf "$$ramdisk"; exit 1; }; \
	 rm -rf "$$ramdisk"; \
	 echo "Done: $$module_pdf"
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
	  bash scripts/install-epstopdf-wrapper.sh; \
	else \
	  echo "  epstopdf already present -- skipping"; \
	fi
	@echo "  Regenerating xelatex format (fixes expl3 version mismatch)..."
	@fmtutil-user --byfmt xelatex 2>&1 | grep -E "(INFO|Error|error)" | tail -3
	@echo "Done: LaTeX deps installed. Re-run 'make doxygen-pdf-<name>' to generate a PDF."

