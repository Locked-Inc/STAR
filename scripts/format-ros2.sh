#!/bin/bash
# ROS2 C++ Code Formatting Script
# Uses ament_uncrustify (ROS2's official formatter) instead of clang-format
# Usage: ./scripts/format-ros2.sh [options]

set -e  # Exit on any error
set +H  # Disable history expansion (fixes ! in if statements)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
CHECK_ONLY=false
VERBOSE=false
SKIP_GUARDS=false
ROS2_DIR="star-ros2/src"

# Print usage information
usage() {
    echo "ROS2 C++ Code Formatting Script"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -c, --check       Check formatting without making changes"
    echo "  -v, --verbose     Enable verbose output"
    echo "  --skip-guards     Skip header guard checking/fixing"
    echo "  -h, --help        Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0             # Format all ROS2 C++ files and fix header guards"
    echo "  $0 --check     # Check formatting without changes (CI mode)"
    echo "  $0 -v          # Format with verbose output"
    echo ""
    echo "Note: This script uses ament_uncrustify (ROS2 official formatter)."
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

# Check if ament_uncrustify is available
check_uncrustify() {
    if ! command -v ament_uncrustify &> /dev/null; then
        print_error "ament_uncrustify not found!"
        echo ""
        echo "Please install ROS2 and source the environment:"
        echo "  source /opt/ros/jazzy/setup.bash"
        echo ""
        echo "Or if using a devcontainer/workspace:"
        echo "  Ensure ROS2 is properly installed and sourced"
        exit 1
    fi

    if [ "$VERBOSE" = true ]; then
        print_status "Found ament_uncrustify"
    fi
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--check)
                CHECK_ONLY=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            --skip-guards)
                SKIP_GUARDS=true
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

# Find all ROS2 packages
find_ros2_packages() {
    local packages=()
    
    if [ ! -d "$ROS2_DIR" ]; then
        print_error "ROS2 source directory not found: $ROS2_DIR"
        exit 1
    fi
    
    # Find all package directories (those containing package.xml)
    while IFS= read -r -d '' pkg_xml; do
        local pkg_dir
        pkg_dir=$(dirname "$pkg_xml")
        packages+=("$pkg_dir")
    done < <(find "$ROS2_DIR" -name "package.xml" -type f \
        -not -path "*/build/*" \
        -not -path "*/install/*" \
        -not -path "*/log/*" \
        -print0 2>/dev/null)
    
    printf '%s\n' "${packages[@]}"
}

# Check formatting of a package
check_package_formatting() {
    local pkg_dir="$1"
    local pkg_name
    pkg_name=$(basename "$pkg_dir")
    
    if [ "$VERBOSE" = true ]; then
        print_status "Checking package: $pkg_name"
    fi
    
    # Run ament_uncrustify in check mode
    # Redirect stderr to stdout to capture all output
    local output
    if output=$(cd "$pkg_dir" && ament_uncrustify --reformat 2>&1); then
        if [ "$VERBOSE" = true ]; then
            print_success "Package $pkg_name: OK"
        fi
        return 0
    else
        # Check if output contains "Code style divergence"
        if echo "$output" | grep -q "Code style divergence"; then
            print_warning "Package $pkg_name: Code style issues found"
            if [ "$VERBOSE" = true ]; then
                echo "$output"
            fi
            return 1
        else
            # Some other error
            print_error "Package $pkg_name: ament_uncrustify failed"
            echo "$output"
            return 1
        fi
    fi
}

# Format a package
format_package() {
    local pkg_dir="$1"
    local pkg_name
    pkg_name=$(basename "$pkg_dir")
    
    if [ "$VERBOSE" = true ]; then
        print_status "Formatting package: $pkg_name"
    fi
    
    # Run ament_uncrustify with --reformat to fix files
    local output
    if output=$(cd "$pkg_dir" && ament_uncrustify --reformat 2>&1); then
        print_success "Formatted package: $pkg_name"
        return 0
    else
        print_error "Failed to format package: $pkg_name"
        echo "$output"
        return 1
    fi
}

# Get expected header guard for a file
# Pattern: PACKAGE__FILENAME_HPP_
get_expected_guard() {
    local file="$1"
    local rel_path="${file#$ROS2_DIR/}"
    local package_name="${rel_path%%/*}"
    local filename
    filename=$(basename "$file" .hpp)
    
    local package_upper
    package_upper=$(echo "$package_name" | tr '[:lower:]-' '[:upper:]_')
    local filename_upper
    filename_upper=$(echo "$filename" | tr '[:lower:]-' '[:upper:]_')
    
    echo "${package_upper}__${filename_upper}_HPP_"
}

# Check header guards
check_header_guards() {
    local issues_found=false
    
    print_status "Checking header guards..."
    
    while IFS= read -r -d '' file; do
        local expected_guard
        expected_guard=$(get_expected_guard "$file")
        local current_guard
        current_guard=$(grep -m1 "^#ifndef " "$file" 2>/dev/null | sed 's/#ifndef //' || true)
        
        if [ -z "$current_guard" ]; then
            if [ "$VERBOSE" = true ]; then
                print_warning "No header guard found in: $file"
            fi
            continue
        fi
        
        if [ "$current_guard" != "$expected_guard" ]; then
            if [ "$issues_found" = false ]; then
                echo ""
                print_warning "Header guard issues found:"
                issues_found=true
            fi
            echo "  $file"
            if [ "$VERBOSE" = true ]; then
                echo "    Current:  $current_guard"
                echo "    Expected: $expected_guard"
            fi
        fi
    done < <(find "$ROS2_DIR" -name "*.hpp" -type f \
        -not -path "*/build/*" \
        -not -path "*/install/*" \
        -not -path "*/log/*" \
        -print0 2>/dev/null)
    
    if [ "$issues_found" = true ]; then
        echo ""
        print_error "Header guard check failed!"
        echo "Run './scripts/format-ros2.sh' to fix header guards."
        return 1
    else
        print_success "All header guards are correct!"
        return 0
    fi
}

# Fix header guards
fix_header_guards() {
    local fixed_count=0
    
    print_status "Checking and fixing header guards..."
    
    while IFS= read -r -d '' file; do
        local expected_guard
        expected_guard=$(get_expected_guard "$file")
        local current_guard
        current_guard=$(grep -m1 "^#ifndef " "$file" 2>/dev/null | sed 's/#ifndef //' || true)
        
        if [ -z "$current_guard" ]; then
            continue
        fi
        
        if [ "$current_guard" != "$expected_guard" ]; then
            # Fix the guard (macOS compatible)
            if [[ "$OSTYPE" == "darwin"* ]]; then
                sed -i '' "s/#ifndef $current_guard/#ifndef $expected_guard/" "$file"
                sed -i '' "s/#define $current_guard/#define $expected_guard/" "$file"
                sed -i '' "s|// $current_guard|// $expected_guard|" "$file"
            else
                sed -i "s/#ifndef $current_guard/#ifndef $expected_guard/" "$file"
                sed -i "s/#define $current_guard/#define $expected_guard/" "$file"
                sed -i "s|// $current_guard|// $expected_guard|" "$file"
            fi
            ((fixed_count++))
            if [ "$VERBOSE" = true ]; then
                print_status "Fixed guard: $file"
            fi
        fi
    done < <(find "$ROS2_DIR" -name "*.hpp" -type f \
        -not -path "*/build/*" \
        -not -path "*/install/*" \
        -not -path "*/log/*" \
        -print0 2>/dev/null)
    
    if [ $fixed_count -gt 0 ]; then
        print_success "Fixed $fixed_count header guard(s)!"
    else
        print_success "All header guards were already correct!"
    fi
}

# Main execution
main() {
    # Change to project root directory
    cd "$(dirname "$0")/.."

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_uncrustify

    # Find ROS2 packages
    print_status "Searching for ROS2 packages in: $ROS2_DIR"
    IFS=$'\n' read -d '' -r -a packages < <(find_ros2_packages && printf '\0')

    if [ ${#packages[@]} -eq 0 ]; then
        print_warning "No ROS2 packages found in $ROS2_DIR!"
        exit 0
    fi

    if [ "$VERBOSE" = true ]; then
        print_status "Found ${#packages[@]} ROS2 package(s)"
    fi

    # Track overall success
    local exit_code=0

    # Execute formatting or check on each package
    if [ "$CHECK_ONLY" = true ]; then
        print_status "Checking code formatting (no changes will be made)..."
        for pkg in "${packages[@]}"; do
            check_package_formatting "$pkg" || exit_code=1
        done
        
        if [ "$SKIP_GUARDS" = false ]; then
            check_header_guards || exit_code=1
        fi
    else
        print_status "Formatting code..."
        for pkg in "${packages[@]}"; do
            format_package "$pkg" || exit_code=1
        done
        
        if [ "$SKIP_GUARDS" = false ]; then
            fix_header_guards
        fi
    fi

    exit $exit_code
}

# Run main function with all arguments
main "$@"
