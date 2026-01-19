#!/bin/bash
# ROS2 Header Guard Fixer
# Converts header guards to ROS2 standard format:
#   PACKAGE__FILENAME_HPP_
# Usage: ./scripts/fix-header-guards.sh [--check]

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

CHECK_ONLY=false
ROS2_DIR="star-ros2/src"

print_status() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Parse args
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--check) CHECK_ONLY=true; shift ;;
        -h|--help)
            echo "Usage: $0 [--check]"
            echo "  --check  Check guards without modifying files (CI mode)"
            exit 0
            ;;
        *) print_error "Unknown option: $1"; exit 1 ;;
    esac
done

cd "$(dirname "$0")/.."

# Find all .hpp files in ROS2 packages
files_with_issues=()
fixed_count=0

while IFS= read -r -d '' file; do
    # Extract package name from path: star-ros2/src/PACKAGE/...
    rel_path="${file#star-ros2/src/}"
    package_name="${rel_path%%/*}"
    
    # Expected guard: PACKAGE__FILENAME_HPP_ (ROS2 standard double underscore)
    # Example: star_pkg/include/star_pkg/header.hpp -> STAR_PKG__HEADER_HPP_
    package=$(echo "$package_name" | tr '[:lower:]-' '[:upper:]_')
    filename=$(basename "$file" .hpp | tr '[:lower:]-' '[:upper:]_')
    expected_guard="${package}__${filename}_HPP_"
    
    # Read current guard from file
    current_guard=$(grep -m1 "^#ifndef " "$file" 2>/dev/null | sed 's/#ifndef //' || true)
    
    if [ -z "$current_guard" ]; then
        print_warning "No header guard found in: $file"
        continue
    fi
    
    # Check if guard matches expected pattern
    if [ "$current_guard" != "$expected_guard" ]; then
        if [ "$CHECK_ONLY" = true ]; then
            files_with_issues+=("$file")
            echo "  $file"
            echo "    Current:  $current_guard"
            echo "    Expected: $expected_guard"
        else
            # Fix the guard
            # macOS sed requires different syntax
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
            print_status "Fixed: $file"
        fi
    fi
done < <(find "$ROS2_DIR" -name "*.hpp" -type f \
    -not -path "*/build/*" \
    -not -path "*/install/*" \
    -not -path "*/log/*" \
    -print0 2>/dev/null)

if [ "$CHECK_ONLY" = true ]; then
    if [ ${#files_with_issues[@]} -gt 0 ]; then
        echo ""
        print_error "Found ${#files_with_issues[@]} file(s) with incorrect header guards!"
        echo "Run './scripts/fix-header-guards.sh' to fix them."
        exit 1
    else
        print_success "All header guards are correct!"
    fi
else
    if [ $fixed_count -gt 0 ]; then
        print_success "Fixed $fixed_count file(s)!"
    else
        print_success "All header guards were already correct!"
    fi
fi
