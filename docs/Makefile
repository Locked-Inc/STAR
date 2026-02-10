# =============================================================================
# STAR Documentation Build System
# Compiles LaTeX source files to PDF
# =============================================================================

# Configuration
MAIN_TEX = star_documentation.tex
MAIN_PDF = star_documentation.pdf
SECTIONS_DIR = sections
PREAMBLE = preamble.sty

# LaTeX compiler settings
LATEX = pdflatex
LATEX_FLAGS = -interaction=nonstopmode -halt-on-error

# Find all section files
SECTIONS = $(wildcard $(SECTIONS_DIR)/*.tex)

# Build outputs to clean
BUILD_ARTIFACTS = *.aux *.log *.out *.toc *.lof *.lot *.fls *.fdb_latexmk *.synctex.gz

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean help watch

# Default target
all: $(MAIN_PDF)

# Build the PDF (requires two passes for TOC and cross-references)
$(MAIN_PDF): $(MAIN_TEX) $(PREAMBLE) $(SECTIONS)
	@echo "========================================"
	@echo "Building STAR Documentation PDF"
	@echo "========================================"
	@echo "First pass: Generating auxiliary files..."
	$(LATEX) $(LATEX_FLAGS) $(MAIN_TEX)
	@echo ""
	@echo "Second pass: Resolving references and TOC..."
	$(LATEX) $(LATEX_FLAGS) $(MAIN_TEX)
	@echo ""
	@echo "========================================"
	@echo "Build complete: $(MAIN_PDF)"
	@echo "========================================"

# Quick build (single pass, for draft preview)
draft: $(MAIN_TEX) $(PREAMBLE) $(SECTIONS)
	@echo "Building draft (single pass)..."
	$(LATEX) $(LATEX_FLAGS) $(MAIN_TEX)
	@echo "Draft complete: $(MAIN_PDF)"

# Clean build artifacts (keep PDF)
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(BUILD_ARTIFACTS)
	@echo "Clean complete (PDF preserved)"

# Clean everything including PDF
distclean: clean
	@echo "Removing PDF..."
	rm -f $(MAIN_PDF)
	@echo "Full clean complete"

# Watch for changes and rebuild automatically (requires inotifywait)
watch:
	@echo "Watching for changes (Ctrl+C to stop)..."
	@while true; do \
		inotifywait -e modify -r $(MAIN_TEX) $(PREAMBLE) $(SECTIONS_DIR)/ 2>/dev/null && \
		$(MAKE) --no-print-directory all; \
	done

# Display help information
help:
	@echo "STAR Documentation Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make          - Build PDF with full processing (default)"
	@echo "  make all      - Same as 'make'"
	@echo "  make draft    - Quick single-pass build for preview"
	@echo "  make clean    - Remove build artifacts (keep PDF)"
	@echo "  make distclean - Remove all generated files including PDF"
	@echo "  make watch    - Auto-rebuild on file changes (requires inotify-tools)"
	@echo "  make help     - Show this help message"
	@echo ""
	@echo "Input files:"
	@echo "  Main: $(MAIN_TEX)"
	@echo "  Preamble: $(PREAMBLE)"
	@echo "  Sections: $(SECTIONS_DIR)/*.tex"
	@echo ""
	@echo "Output:"
	@echo "  PDF: $(MAIN_PDF)"

# Prevent make from deleting intermediate files
.PRECIOUS: $(MAIN_PDF)
