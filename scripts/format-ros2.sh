#!/bin/bash
# ROS2 C++ Code Formatting Script
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
EXTENSIONS=("*.cpp" "*.hpp")
DIRECTORIES=("src")
ROS2_DIR="star-ros2"

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

# Check if clang-format is installed
check_clang_format() {
    if ! command -v clang-format &> /dev/null; then
        print_error "clang-format not found!"
        echo ""
        echo "Please install clang-format:"
        echo "  macOS: brew install clang-format"
        echo "  Ubuntu/Debian: sudo apt-get install clang-format"
        echo "  Fedora: sudo dnf install clang"
        exit 1
    fi

    local version
    version=$(clang-format --version | head -n1)
    if [ "$VERBOSE" = true ]; then
        print_status "Found $version"
    fi
}

# Check if .clang-format file exists
check_clang_format_config() {
    if [ ! -f "$ROS2_DIR/.clang-format" ]; then
        print_error ".clang-format configuration file not found in $ROS2_DIR!"
        echo "Please ensure you're running this script from the project root directory."
        exit 1
    fi

    if [ "$VERBOSE" = true ]; then
        print_status "Found $ROS2_DIR/.clang-format configuration"
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

# Find all source files
find_source_files() {
    local files=()

    for dir in "${DIRECTORIES[@]}"; do
        local full_dir="$ROS2_DIR/$dir"
        if [ ! -d "$full_dir" ]; then
            if [ "$VERBOSE" = true ]; then
                print_warning "Directory '$full_dir' not found, skipping..."
            fi
            continue
        fi

        for ext in "${EXTENSIONS[@]}"; do
            # Use find to recursively find files with the extension
            # Exclude build/, install/, log/ directories
            while IFS= read -r -d '' file; do
                files+=("$file")
            done < <(find "$full_dir" -name "$ext" -type f \
                -not -path "*/build/*" \
                -not -path "*/install/*" \
                -not -path "*/log/*" \
                -print0 2>/dev/null)
        done
    done

    printf '%s\n' "${files[@]}"
}

# Check formatting of files
check_formatting() {
    local files=("$@")
    local issues_found=false

    print_status "Checking code formatting..."

    for file in "${files[@]}"; do
        if [ "$VERBOSE" = true ]; then
            echo "  Checking: $file"
        fi

        # Check if file would be changed by clang-format
        if ! clang-format --dry-run --Werror "$file" >/dev/null 2>&1; then
            if [ "$issues_found" = false ]; then
                echo ""
                print_warning "Formatting issues found in:"
                issues_found=true
            fi
            echo "  $file"
        fi
    done

    if [ "$issues_found" = true ]; then
        echo ""
        print_error "Code formatting check failed!"
        echo "Run './scripts/format-ros2.sh' to fix formatting issues."
        return 1
    else
        print_success "All files are properly formatted!"
        return 0
    fi
}

# Get expected header guard for a file
# Pattern: PACKAGE__FILENAME_HPP_
get_expected_guard() {
    local file="$1"
    local rel_path="${file#star-ros2/src/}"
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
    done < <(find "$ROS2_DIR/src" -name "*.hpp" -type f \
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
    done < <(find "$ROS2_DIR/src" -name "*.hpp" -type f \
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

# Format files
format_files() {
    local files=("$@")
    local formatted_count=0

    print_status "Formatting source files..."

    for file in "${files[@]}"; do
        if [ "$VERBOSE" = true ]; then
            echo "  Processing: $file"
        fi

        # Create a temporary file to compare
        local temp_file
        temp_file=$(mktemp)

        # Format the file to temp
        clang-format "$file" > "$temp_file" 2>&1 || {
            echo "ERROR: clang-format failed on $file" >&2
            rm "$temp_file"
            continue
        }

        # Check if file was changed
        if ! cmp -s "$file" "$temp_file" 2>/dev/null; then
            cp "$temp_file" "$file"
            ((formatted_count++))
            if [ "$VERBOSE" = true ]; then
                echo "    ✓ Formatted"
            fi
        elif [ "$VERBOSE" = true ]; then
            echo "    • No changes needed"
        fi

        rm "$temp_file"
    done

    if [ $formatted_count -gt 0 ]; then
        print_success "Formatted $formatted_count file(s)!"
    else
        print_success "All files were already properly formatted!"
    fi
}

# Main execution
main() {
    # Change to project root directory
    cd "$(dirname "$0")/.."

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_clang_format
    check_clang_format_config

    # Find source files
    print_status "Searching for C++ files in: $ROS2_DIR/${DIRECTORIES[*]}"
    IFS=$'\n' read -d '' -r -a source_files < <(find_source_files && printf '\0')

    if [ ${#source_files[@]} -eq 0 ]; then
        print_warning "No C++ source files found!"
        exit 0
    fi

    if [ "$VERBOSE" = true ]; then
        print_status "Found ${#source_files[@]} C++ file(s)"
    fi

    # Track overall success
    local exit_code=0

    # Execute formatting or check
    if [ "$CHECK_ONLY" = true ]; then
        check_formatting "${source_files[@]}" || exit_code=1
        if [ "$SKIP_GUARDS" = false ]; then
            check_header_guards || exit_code=1
        fi
    else
        format_files "${source_files[@]}"
        if [ "$SKIP_GUARDS" = false ]; then
            fix_header_guards
        fi
    fi

    exit $exit_code
}

# Run main function with all arguments
main "$@"
