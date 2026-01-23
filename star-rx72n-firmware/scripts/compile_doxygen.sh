#!/bin/bash
# RX72N STAR Firmware Doxygen Documentation Generator
# Usage: ./compile_doxygen.sh [options]

set -e  # Exit on any error

g_child_pid=0
g_log_file=""

# Graceful exit on interrupt
cleanup() {
    echo -e "\n\n${RED}[INTERRUPT] Script interrupted by user. Exiting gracefully...${NC}\n"

    if [[ -n "$g_child_pid" && "$g_child_pid" -ne 0 ]]; then
        kill -SIGTERM "$g_child_pid" 2>/dev/null
    fi

    # Check if there is a log file to display from a background process
    if [[ -n "$g_log_file" && -f "$g_log_file" ]]; then
        local output
        output=$(cat "$g_log_file")
        if [ -n "$output" ]; then
            echo -e "${YELLOW}--- Interrupted LaTeX Output ---${NC}\n$output\n${YELLOW}--------------------------------${NC}"
        fi
        rm -f "$g_log_file" # Clean up the log file
    fi

    # Clean up temporary Doxyfile if it exists
    if [[ -n "$TEMP_DOXYFILE" && -f "$TEMP_DOXYFILE" ]]; then
        rm -f "$TEMP_DOXYFILE"
    fi

    exit 130
}

trap cleanup SIGINT SIGTERM

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
OUTPUT_DIR="docs/doxygen"
CLEAN_FIRST=false
OPEN_BROWSER=false
VERBOSE=false
GENERATE_PDF=false
FORCE_REGENERATE=false
INCLUDE_THIRD_PARTY=false
CACHE_FILE=".doxygen_cache"
TEMP_DOXYFILE=""

# Print usage information
usage() {
    echo "RX72N STAR Firmware Doxygen Documentation Generator"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -o, --output DIR          Set output directory (default: docs/doxygen)"
    echo "  -c, --clean               Clean output directory before generation"
    echo "  -p, --pdf                 Generate PDF from LaTeX output"
    echo "  -b, --browser             Open documentation in browser after generation"
    echo "  -f, --force               Force regeneration (ignore cache)"
    echo "  -t, --include-third-party Include third-party libraries (ThreadX, nanopb)"
    echo "  -v, --verbose             Enable verbose output"
    echo "  -h, --help                Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                        # Generate docs in default location"
    echo "  $0 --clean                # Clean and regenerate docs"
    echo "  $0 --pdf                  # Generate HTML and PDF"
    echo "  $0 -cb                    # Clean, generate, and open in browser"
    echo "  $0 --include-third-party  # Include ThreadX and nanopb in docs"
    echo "  $0 -o custom/path         # Generate in custom directory"
}

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Doxygen is installed
check_doxygen() {
    if ! command -v doxygen &> /dev/null; then
        print_error "Doxygen not found!"
        echo ""
        echo "Please install Doxygen:"
        echo "  macOS: brew install doxygen"
        echo "  Ubuntu/Debian: sudo apt-get install doxygen"
        echo "  Fedora: sudo dnf install doxygen"
        exit 1
    fi
    
    local version
    version=$(doxygen --version 2>/dev/null || echo "unknown")
    if [ "$VERBOSE" = true ]; then
        print_status "Found Doxygen version $version"
    fi
}

# Check for PDF generation dependencies
check_pdf_deps() {
    if [ "$GENERATE_PDF" = true ]; then
        if ! command -v pdflatex &> /dev/null; then
            print_error "pdflatex not found, which is required for PDF generation."
            echo ""
            echo "Please install a LaTeX distribution:"
            echo "  macOS: brew install --cask mactex"
            echo "  Ubuntu/Debian: sudo apt-get install texlive-full"
            echo "  Fedora: sudo dnf install texlive-scheme-full"
            exit 1
        fi

        if ! command -v make &> /dev/null; then
            print_error "make not found, which is required for PDF generation."
            echo "Please install make."
            exit 1
        fi

        if [ "$VERBOSE" = true ]; then
            print_status "Found pdflatex for PDF generation"
        fi
    fi
}

# Check if Doxyfile exists
check_doxyfile() {
    if [ ! -f "Doxyfile" ]; then
        print_error "Doxyfile configuration not found!"
        echo "Please ensure you're running this script from the project root directory."
        echo "If no Doxyfile exists, create one with: doxygen -g"
        exit 1
    fi
    
    if [ "$VERBOSE" = true ]; then
        print_status "Found Doxyfile configuration"
    fi
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -c|--clean)
                CLEAN_FIRST=true
                shift
                ;;
            -p|--pdf)
                GENERATE_PDF=true
                shift
                ;;
            -b|--browser)
                OPEN_BROWSER=true
                shift
                ;;
            -f|--force)
                FORCE_REGENERATE=true
                shift
                ;;
            -t|--include-third-party)
                INCLUDE_THIRD_PARTY=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
}

# Clean output directory
clean_output() {
    if [ "$CLEAN_FIRST" = true ] && [ -d "$OUTPUT_DIR" ]; then
        print_status "Cleaning output directory: $OUTPUT_DIR"
        rm -rf "$OUTPUT_DIR"
        rm -f "$CACHE_FILE"
        if [ "$VERBOSE" = true ]; then
            print_success "Output directory and cache cleaned"
        fi
    fi
}

# Create output directory
create_output_dir() {
    if [ ! -d "$OUTPUT_DIR" ]; then
        mkdir -p "$OUTPUT_DIR"
        if [ "$VERBOSE" = true ]; then
            print_status "Created output directory: $OUTPUT_DIR"
        fi
    fi
}

# Compute hash of source files to detect changes
compute_source_hash() {
    # Hash all .h and .c files, Doxyfile, and other config
    {
        find include/ src/ tests/ lib/ -type f \( -name "*.h" -o -name "*.c" \) 2>/dev/null | sort | xargs md5sum 2>/dev/null
        md5sum Doxyfile 2>/dev/null
        # Include configuration flags in the hash to ensure cache is invalidated when options change
        echo "INCLUDE_THIRD_PARTY=${INCLUDE_THIRD_PARTY}"
        echo "GENERATE_PDF=${GENERATE_PDF}"
    } | md5sum | awk '{print $1}'
}

# Check if cache is valid
is_cache_valid() {
    if [ "$FORCE_REGENERATE" = true ]; then
        if [ "$VERBOSE" = true ]; then
            print_status "Force regeneration requested (--force)"
        fi
        return 1  # Cache invalid
    fi
    
    if [ ! -f "$CACHE_FILE" ] || [ ! -d "$OUTPUT_DIR/html" ]; then
        if [ "$VERBOSE" = true ]; then
            print_status "No cache found or output missing"
        fi
        return 1  # Cache invalid
    fi
    
    local cached_hash
    cached_hash=$(cat "$CACHE_FILE")
    local current_hash
    current_hash=$(compute_source_hash)
    
    if [ "$cached_hash" = "$current_hash" ]; then
        print_status "Source files unchanged, using cached documentation"
        return 0  # Cache valid
    else
        if [ "$VERBOSE" = true ]; then
            print_status "Source files changed. Regenerating documentation."
            echo "  Old hash: $cached_hash"
            echo "  New hash: $current_hash"
        fi
        return 1  # Cache invalid
    fi
}

# Update cache after successful generation
update_cache() {
    compute_source_hash > "$CACHE_FILE"
    if [ "$VERBOSE" = true ]; then
        print_status "Cache updated: $CACHE_FILE"
    fi
}

# Generate documentation
generate_docs() {
    # Check cache first
    if is_cache_valid; then
        return 0  # Skip generation, use cached output
    fi

    print_status "Generating Doxygen documentation..."

    if [ "$VERBOSE" = true ]; then
        print_status "Output will be generated in: $(realpath "$OUTPUT_DIR")"
    fi

    # Determine which Doxyfile to use
    local doxyfile_to_use="Doxyfile"

    # Create temporary Doxyfile if including third-party libraries
    if [ "$INCLUDE_THIRD_PARTY" = true ]; then
        print_status "Including third-party libraries (ThreadX, nanopb)"
        TEMP_DOXYFILE="Doxyfile.tmp"

        # Remove exclusion patterns for threadx and nanopb by replacing them with empty strings
        # We escape the * characters because they are regex metacharacters
        sed 's|\*/threadx/\*||g; s|\*/rx_nanopb/nanopb/\*||g' Doxyfile > "$TEMP_DOXYFILE"

        doxyfile_to_use="$TEMP_DOXYFILE"

        if [ "$VERBOSE" = true ]; then
            print_status "Created temporary Doxyfile without third-party exclusions"
        fi
    fi

    # Run Doxygen
    if [ "$VERBOSE" = true ]; then
        doxygen "$doxyfile_to_use"
    else
        doxygen "$doxyfile_to_use" 2>/dev/null
    fi

    local exit_code=$?

    # Clean up temporary Doxyfile
    if [ -n "$TEMP_DOXYFILE" ] && [ -f "$TEMP_DOXYFILE" ]; then
        rm -f "$TEMP_DOXYFILE"
        if [ "$VERBOSE" = true ]; then
            print_status "Cleaned up temporary Doxyfile"
        fi
    fi

    if [ $exit_code -eq 0 ]; then
        print_success "Documentation generated successfully!"
        update_cache
    else
        print_error "Doxygen generation failed with exit code $exit_code"
        return $exit_code
    fi
}

# Generate PDF from LaTeX
generate_pdf() {
    if [ "$GENERATE_PDF" = false ]; then
        return
    fi
    
    local latex_dir="$OUTPUT_DIR/latex"
    if [ ! -d "$latex_dir" ] || [ ! -f "$latex_dir/Makefile" ]; then
        print_warning "LaTeX output not found. Skipping PDF generation."
        print_warning "Ensure GENERATE_LATEX is enabled in Doxyfile."
        return
    fi
    
    print_status "Generating PDF from LaTeX output..."
    cd "$latex_dir"

    local exit_code
    local log_file_name="make_pdflatex.log"
    local output=""

    if [ "$VERBOSE" = true ]; then
        # Verbose: Run in foreground and pipe to tee to show output immediately
        # We use PIPESTATUS[0] to get make's exit code, not tee's
        print_status "Running make (output to terminal)..."
        make 2>&1 | tee "$log_file_name"
        exit_code=${PIPESTATUS[0]}
    else
        # Silent: Run in background and capture output
        g_log_file="$(pwd)/$log_file_name"
        make > "$log_file_name" 2>&1 &
        g_child_pid=$!

        # Wait for make to finish. The trap will handle Ctrl+C.
        # Disable set -e temporarily to capture exit code
        set +e
        wait "$g_child_pid"
        exit_code=$?
        set -e
        
        # Reset globals
        g_child_pid=0
        g_log_file=""
    fi
    
    cd - > /dev/null # Go back to original directory silently

    local log_path="$latex_dir/$log_file_name"
    if [ -f "$log_path" ]; then
        output=$(cat "$log_path")
        rm "$log_path"
    fi

    if [ $exit_code -eq 0 ] && [ -f "$latex_dir/refman.pdf" ]; then
        print_success "PDF generated successfully!"
    elif [ $exit_code -ne 130 ]; then # 130 is the exit code for being interrupted
        print_error "PDF generation failed with exit code $exit_code."
        if [ "$VERBOSE" = false ] && [ -n "$output" ]; then
            # Only print output here if we were silent (verbose already saw it)
            echo -e "${RED}--- LaTeX Output ---${NC}\n$output\n${RED}--------------------${NC}"
        fi
        print_error "Try running 'make' manually in '$latex_dir' to debug."
        return $exit_code
    fi
}

# Get the main HTML file path
get_main_html() {
    local html_file="$OUTPUT_DIR/html/index.html"
    if [ -f "$html_file" ]; then
        echo "$(realpath "$html_file")"
    else
        echo ""
    fi
}

# Get the main PDF file path
get_main_pdf() {
    local pdf_file="$OUTPUT_DIR/latex/refman.pdf"
    if [ -f "$pdf_file" ]; then
        echo "$(realpath "$pdf_file")"
    else
        echo ""
    fi
}


# Open documentation in browser
open_docs() {
    if [ "$OPEN_BROWSER" = true ]; then
        local html_file
        html_file=$(get_main_html)
        
        if [ -n "$html_file" ]; then
            print_status "Opening documentation in browser..."
            
            # Try different browser opening methods based on OS
            if command -v open &> /dev/null; then
                # macOS
                open "$html_file"
            elif command -v xdg-open &> /dev/null; then
                # Linux
                xdg-open "$html_file"
            elif command -v start &> /dev/null; then
                # Windows (Git Bash)
                start "$html_file"
            else
                print_warning "Could not automatically open browser"
                echo "Open this file manually: $html_file"
            fi
        else
            print_warning "Could not find generated HTML documentation"
        fi
    fi
}

# Display summary
show_summary() {
    echo ""
    print_success "Doxygen compilation completed!"
    
    local html_file
    html_file=$(get_main_html)
    
    local pdf_file
    pdf_file=$(get_main_pdf)
    
    if [ -n "$html_file" ] || [ -n "$pdf_file" ]; then
        echo ""
        echo "Documentation available at:"
        if [ -n "$html_file" ]; then
            echo "  HTML: $html_file"
        fi
        if [ -n "$pdf_file" ]; then
            echo "  PDF:  $pdf_file"
        fi
        echo ""
        echo "Quick access commands:"
        if [ -n "$html_file" ]; then
            echo "  Open in browser: open '$html_file'"
        fi
        if [ -n "$pdf_file" ]; then
            echo "  Open PDF:        open '$pdf_file'"
        fi
        echo "  View directory:  ls -la '$OUTPUT_DIR'"
    fi
    
    # Show directory size if verbose
    if [ "$VERBOSE" = true ] && [ -d "$OUTPUT_DIR" ]; then
        local size
        size=$(du -sh "$OUTPUT_DIR" 2>/dev/null | cut -f1 || echo "unknown")
        echo "  Total size:      $size"
    fi
}

# Main execution
main() {
    # Change to project root directory
    cd "$(dirname "$0")/.."
    
    # Parse command line arguments
    parse_args "$@"
    
    # Check prerequisites
    check_doxygen
    check_pdf_deps
    check_doxyfile
    
    # Prepare output directory
    clean_output
    create_output_dir
    
    # Generate documentation
    generate_docs
    
    # Generate PDF if requested
    generate_pdf
    
    # Open in browser if requested
    open_docs
    
    # Show summary
    show_summary
}

# Run main function with all arguments
main "$@"