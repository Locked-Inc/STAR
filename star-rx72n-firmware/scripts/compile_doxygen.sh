#!/bin/bash
# ESP32 STAR Firmware Doxygen Documentation Generator
# Usage: ./compile_doxygen.sh [options]

set -e  # Exit on any error

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

# Print usage information
usage() {
    echo "ESP32 STAR Firmware Doxygen Documentation Generator"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -o, --output DIR   Set output directory (default: docs/doxygen)"
    echo "  -c, --clean        Clean output directory before generation"
    echo "  -b, --browser      Open documentation in browser after generation"
    echo "  -v, --verbose      Enable verbose output"
    echo "  -h, --help         Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                 # Generate docs in default location"
    echo "  $0 --clean         # Clean and regenerate docs"
    echo "  $0 -cb             # Clean, generate, and open in browser"
    echo "  $0 -o custom/path  # Generate in custom directory"
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
            -b|--browser)
                OPEN_BROWSER=true
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
        if [ "$VERBOSE" = true ]; then
            print_success "Output directory cleaned"
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

# Generate documentation
generate_docs() {
    print_status "Generating Doxygen documentation..."
    
    if [ "$VERBOSE" = true ]; then
        print_status "Output will be generated in: $(realpath "$OUTPUT_DIR")"
    fi
    
    # Run Doxygen
    if [ "$VERBOSE" = true ]; then
        doxygen Doxyfile
    else
        doxygen Doxyfile 2>/dev/null
    fi
    
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        print_success "Documentation generated successfully!"
    else
        print_error "Doxygen generation failed with exit code $exit_code"
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
    
    if [ -n "$html_file" ]; then
        echo ""
        echo "Documentation available at:"
        echo "  HTML: $html_file"
        echo ""
        echo "Quick access commands:"
        echo "  Open in browser: open '$html_file'"
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
    check_doxyfile
    
    # Prepare output directory
    clean_output
    create_output_dir
    
    # Generate documentation
    generate_docs
    
    # Open in browser if requested
    open_docs
    
    # Show summary
    show_summary
}

# Run main function with all arguments
main "$@"