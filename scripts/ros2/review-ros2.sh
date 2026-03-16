#!/bin/bash
# ROS2 C++ Code Review Automation Tool
# Usage: ./scripts/ros2/review-ros2.sh [options]

set -e
set +H

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
TARGET_PATH="star-ros2/src"
REPORT_FILE=""
VERBOSE=false
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0
MANUAL_CHECKS=0

# Issues array
declare -a ISSUES

# Print functions
print_status() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[[PASS]]${NC} $1"; }
print_failure() { echo -e "${RED}[[FAIL]]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[[WARN]]${NC} $1"; }

# Usage
usage() {
    echo "ROS2 C++ Code Review Automation Tool"
    echo ""
    echo "Usage: $0 [options] [path]"
    echo ""
    echo "Options:"
    echo "  -r, --report FILE  Write report to file"
    echo "  -v, --verbose      Enable verbose output"
    echo "  -h, --help         Show this help message"
    echo ""
    echo "Arguments:"
    echo "  path               Path to review (default: star-ros2/src)"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Review all ROS2 code"
    echo "  $0 star-ros2/src/star_spi_bridge    # Review specific package"
    echo "  $0 --report review.txt               # Generate report to file"
}

# Parse arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -r|--report)
                REPORT_FILE="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            -*)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
            *)
                TARGET_PATH="$1"
                shift
                ;;
        esac
    done
}

# Add issue to report
add_issue() {
    ISSUES+=("$1")
}

# Check 1: Naming Conventions (Lines 6-12 of checklist)
check_naming_conventions() {
    local category="NAMING CONVENTIONS"
    local category_passed=0
    local category_total=6

    print_status "Checking $category..."

    # Find all C++ files
    local cpp_files
    cpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o \( -name "*.cpp" -o -name "*.hpp" \) -print 2>/dev/null || true)

    if [ -z "$cpp_files" ]; then
        print_warning "$category: No C++ files found (manual review needed)"
        MANUAL_CHECKS=$((MANUAL_CHECKS + category_total))
        return 0
    fi

    # Check 1.1: Classes use CamelCase
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local bad_classes
    bad_classes=$(echo "$cpp_files" | xargs grep -Hn "^class [a-z_][a-z0-9_]*" 2>/dev/null || true)
    if [ -z "$bad_classes" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Classes not using CamelCase:"
        add_issue "$bad_classes"
    fi

    # Check 1.2: Methods use snake_case (detect camelCase methods)
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local camel_methods
    camel_methods=$(echo "$cpp_files" | xargs grep -Hn "  [a-z][a-zA-Z0-9]*[A-Z][a-zA-Z0-9]* *(" 2>/dev/null | grep -v "//" || true)
    if [ -z "$camel_methods" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Methods using camelCase (should be snake_case):"
        add_issue "$camel_methods"
    fi

    # Check 1.3: Member variables have trailing underscore
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local bad_members
    # Look for member variable declarations (exclude control flow statements)
    bad_members=$(echo "$cpp_files" | xargs grep -Hn "^  [a-z][a-z0-9_]* [a-z][a-z0-9_]*;" 2>/dev/null | grep -v "_;" | grep -vE "(return|if|for|while|switch|case|break|continue|throw|delete|new|const|static|typedef)" || true)
    if [ -z "$bad_members" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Member variables without trailing underscore:"
        add_issue "$bad_members"
    fi

    # Check 1.4: Check for /// comments (should be /** or /**<)
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local triple_slash
    triple_slash=$(echo "$cpp_files" | xargs grep -Hn "^ *///" 2>/dev/null || true)
    if [ -z "$triple_slash" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Using /// comments (should be /** or /**<):"
        add_issue "$triple_slash"
    fi

    # Remaining checks are harder to automate reliably
    MANUAL_CHECKS=$((MANUAL_CHECKS + 2))

    # Report category results
    if [ $category_passed -eq 4 ]; then
        print_success "$category ($category_passed/4 automated checks)"
    else
        print_failure "$category ($category_passed/4 automated checks)"
    fi
}

# Check 2: File Organization (Lines 14-19 of checklist)
check_file_organization() {
    local category="FILE ORGANIZATION"
    local category_passed=0
    local category_total=4

    print_status "Checking $category..."

    # Check 2.1: Headers use .hpp extension
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local dot_h_files
    dot_h_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o -name "*.h" -print 2>/dev/null || true)
    if [ -z "$dot_h_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Headers using .h extension (should be .hpp):"
        add_issue "$dot_h_files"
    fi

    # Check 2.2: Include guards follow pattern
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local hpp_files
    hpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o -name "*.hpp" -print 2>/dev/null || true)
    local bad_guards=0
    if [ -n "$hpp_files" ]; then
        while IFS= read -r file; do
            local rel_path="${file#star-ros2/src/}"
            local package_name="${rel_path%%/*}"
            local file_base
            file_base=$(basename "$rel_path")
            local file_stem="${file_base%.*}"
            local expected_guard
            expected_guard=$(printf '%s__%s_HPP_' "$package_name" "$file_stem" \
              | tr '[:lower:]' '[:upper:]' \
              | tr '-' '_')
            
            if grep -q "#pragma once" "$file" 2>/dev/null; then
                : # pragma once is the STAR-mandated header guard style (CLAUDE.md)
            elif ! grep -q "#ifndef $expected_guard" "$file" 2>/dev/null; then
                bad_guards=$((bad_guards + 1))
                add_issue "  - Incorrect include guard: $file (expected: $expected_guard)"
            fi
        done <<< "$hpp_files"
    fi
    if [ $bad_guards -eq 0 ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
    fi

    # Remaining checks require deeper analysis
    MANUAL_CHECKS=$((MANUAL_CHECKS + 2))

    if [ $category_passed -eq 2 ]; then
        print_success "$category ($category_passed/2 automated checks)"
    else
        print_failure "$category ($category_passed/2 automated checks)"
    fi
}

# Check 3: Formatting (Lines 21-28 of checklist)
check_formatting() {
    local category="FORMATTING"

    print_status "Checking $category..."

    # Check 3.1: clang-format check (requires ament_uncrustify from ROS2 environment)
    if ! command -v ament_uncrustify &> /dev/null; then
        print_warning "$category (skipped: ament_uncrustify not available)"
    else
        TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
        if ./scripts/ros2/format-ros2.sh --check > /dev/null 2>&1; then
            PASSED_CHECKS=$((PASSED_CHECKS + 1))
            print_success "$category (clang-format passes)"
        else
            FAILED_CHECKS=$((FAILED_CHECKS + 1))
            add_issue "  - clang-format check failed (run ./scripts/ros2/format-ros2.sh)"
            print_failure "$category (clang-format failed)"
        fi
    fi

    # Line length, indentation, braces - covered by clang-format
}

# Check 4: ROS2 Patterns (Lines 30-35 of checklist)
check_ros2_patterns() {
    local category="ROS2 PATTERNS"
    local category_passed=0

    print_status "Checking $category..."

    local cpp_files
    cpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o \( -name "*.cpp" -o -name "*.hpp" \) -print 2>/dev/null || true)

    if [ -z "$cpp_files" ]; then
        print_warning "$category: No C++ files found (manual review needed)"
        MANUAL_CHECKS=$((MANUAL_CHECKS + 4))
        return 0
    fi

    # Check 4.1: Nodes inherit from rclcpp::Node or rclcpp_lifecycle::LifecycleNode
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local node_inheritance
    node_inheritance=$(echo "$cpp_files" | xargs grep -H "class.*:.*public.*rclcpp" 2>/dev/null || true)
    if [ -n "$node_inheritance" ] || [ -z "$cpp_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        # No nodes found yet (infrastructure phase)
        MANUAL_CHECKS=$((MANUAL_CHECKS + 1))
    fi

    # Check 4.2: Check for SharedPtr usage
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local shared_ptr_usage
    shared_ptr_usage=$(echo "$cpp_files" | xargs grep -H "SharedPtr" 2>/dev/null || true)
    if [ -n "$shared_ptr_usage" ] || [ -z "$cpp_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        MANUAL_CHECKS=$((MANUAL_CHECKS + 1))
    fi

    # Remaining checks are contextual
    MANUAL_CHECKS=$((MANUAL_CHECKS + 2))

    if [ $category_passed -eq 2 ]; then
        print_success "$category ($category_passed/2 automated checks)"
    else
        print_warning "$category ($category_passed/2 automated checks, manual review needed)"
    fi
}

# Check 5: Error Handling (Lines 37-42 of checklist)
check_error_handling() {
    local category="ERROR HANDLING"
    local category_passed=0

    print_status "Checking $category..."

    local cpp_files
    cpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o -name "*.cpp" -print 2>/dev/null || true)

    if [ -z "$cpp_files" ]; then
        print_warning "$category: No C++ files found (manual review needed)"
        MANUAL_CHECKS=$((MANUAL_CHECKS + 4))
        return 0
    fi

    # Check 5.1: RCLCPP logging used (not printf/cout)
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local bad_logging
    bad_logging=$(echo "$cpp_files" | xargs grep -Hn "printf\|std::cout" 2>/dev/null | grep -v "//" || true)
    if [ -z "$bad_logging" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Using printf/cout instead of RCLCPP logging:"
        add_issue "$bad_logging"
    fi

    # Check 5.2: Exception handling present
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local has_try_catch
    has_try_catch=$(echo "$cpp_files" | xargs grep -l "try {" 2>/dev/null || true)
    if [ -n "$has_try_catch" ] || [ -z "$cpp_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        MANUAL_CHECKS=$((MANUAL_CHECKS + 1))
    fi

    # Remaining checks require semantic analysis
    MANUAL_CHECKS=$((MANUAL_CHECKS + 2))

    if [ $category_passed -eq 2 ]; then
        print_success "$category ($category_passed/2 automated checks)"
    else
        print_failure "$category ($category_passed/2 automated checks)"
    fi
}

# Check 6: Documentation (Lines 44-49 of checklist)
check_documentation() {
    local category="DOCUMENTATION"
    local category_passed=0

    print_status "Checking $category..."

    local hpp_files
    hpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o -name "*.hpp" -print 2>/dev/null || true)

    if [ -z "$hpp_files" ]; then
        print_warning "$category: No header files found (manual review needed)"
        MANUAL_CHECKS=$((MANUAL_CHECKS + 4))
        return 0
    fi

    # Check 6.1: Doxygen /** comments present
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local has_doxygen
    has_doxygen=$(echo "$hpp_files" | xargs grep -l "/\*\*" 2>/dev/null || true)
    if [ -n "$has_doxygen" ] || [ -z "$hpp_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - No Doxygen /** comments found in headers"
    fi

    # Check 6.2: @brief tags present
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local has_brief
    has_brief=$(echo "$hpp_files" | xargs grep -l "@brief" 2>/dev/null || true)
    if [ -n "$has_brief" ] || [ -z "$hpp_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        MANUAL_CHECKS=$((MANUAL_CHECKS + 1))
    fi

    # Remaining checks are qualitative
    MANUAL_CHECKS=$((MANUAL_CHECKS + 2))

    if [ $category_passed -eq 2 ]; then
        print_success "$category ($category_passed/2 automated checks)"
    else
        print_failure "$category ($category_passed/2 automated checks)"
    fi
}

# Check 7: Safety and Best Practices (Lines 51-57 of checklist)
check_safety_practices() {
    local category="SAFETY AND BEST PRACTICES"

    print_status "Checking $category..."

    local cpp_files
    cpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o -name "*.cpp" -print 2>/dev/null || true)

    if [ -z "$cpp_files" ]; then
        print_warning "$category: No C++ files found (manual review needed)"
        MANUAL_CHECKS=$((MANUAL_CHECKS + 5))
        return 0
    fi

    # Check 7.1: No manual delete (should use RAII)
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local manual_delete
    manual_delete=$(echo "$cpp_files" | xargs grep -Hn "delete " 2>/dev/null | grep -v "//" || true)
    if [ -z "$manual_delete" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Manual delete found (should use RAII/smart pointers):"
        add_issue "$manual_delete"
    fi

    # Remaining checks require control flow analysis
    MANUAL_CHECKS=$((MANUAL_CHECKS + 4))

    print_warning "$category (1/1 automated checks, rest require manual review)"
}

# Check 8: Testing (Lines 59-64 of checklist)
check_testing() {
    local category="TESTING"
    local category_passed=0

    print_status "Checking $category..."

    # Check 8.1: Test files exist
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local test_files
    test_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o \( -name "*_test.cpp" -o -name "test_*.cpp" \) -print 2>/dev/null || true)
    if [ -n "$test_files" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        MANUAL_CHECKS=$((MANUAL_CHECKS + 1))
    fi

    # Remaining checks require test execution and coverage analysis
    MANUAL_CHECKS=$((MANUAL_CHECKS + 3))

    print_warning "$category (tests exist check, coverage requires manual review)"
}

# Check 9: STAR-Specific (Lines 66-71 of checklist)
check_star_specific() {
    local category="STAR PROJECT SPECIFIC"
    local category_passed=0

    print_status "Checking $category..."

    local cpp_files
    cpp_files=$(find "$TARGET_PATH" -path "*/sllidar_ros2/*" -prune -o \( -name "*.cpp" -o -name "*.hpp" \) -print 2>/dev/null || true)

    if [ -z "$cpp_files" ]; then
        print_warning "$category: No C++ files found (manual review needed)"
        MANUAL_CHECKS=$((MANUAL_CHECKS + 4))
        return 0
    fi

    # Check 9.1: No malloc/new in critical paths (check for new keyword)
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local dynamic_alloc
    dynamic_alloc=$(echo "$cpp_files" | xargs grep -Hn "new " 2>/dev/null | grep -v "//" | grep -vE "^[^:]+:[0-9]+: *\*" || true)
    if [ -z "$dynamic_alloc" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        add_issue "  - Dynamic allocation found (prefer static allocation in critical paths):"
        add_issue "$dynamic_alloc"
    fi

    # Check 9.2: Constants defined as enums or const (check for #define with numbers)
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    local bad_macros
    bad_macros=$(echo "$cpp_files" | xargs grep -Hn "#define [A-Z_]* [0-9]" 2>/dev/null || true)
    if [ -z "$bad_macros" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        category_passed=$((category_passed + 1))
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - Macros used for constants (should be enums or const):"
        add_issue "$bad_macros"
    fi

    # Remaining checks are architectural
    MANUAL_CHECKS=$((MANUAL_CHECKS + 2))

    if [ $category_passed -eq 2 ]; then
        print_success "$category ($category_passed/2 automated checks)"
    else
        print_failure "$category ($category_passed/2 automated checks)"
    fi
}

# Check 10: CI/CD (Lines 73-78 of checklist)
check_ci_cd() {
    local category="CI/CD"

    print_status "Checking $category..."

    # These checks require actual build/test execution
    # Just verify workflow file exists
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    if [ -f ".github/workflows/ros2.yml" ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        print_success "$category (workflow file exists, build/test require CI execution)"
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        add_issue "  - .github/workflows/ros2.yml not found"
        print_failure "$category (workflow file missing)"
    fi

    MANUAL_CHECKS=$((MANUAL_CHECKS + 3))
}

# Generate report
generate_report() {
    local output=""

    output+="========================================\n"
    output+="ROS2 Code Review Report\n"
    output+="========================================\n\n"
    output+="Target: $TARGET_PATH\n"
    output+="Date: $(date '+%Y-%m-%d %H:%M:%S')\n\n"

    if [ ${#ISSUES[@]} -gt 0 ]; then
        output+="Issues Found:\n"
        for issue in "${ISSUES[@]}"; do
            output+="$issue\n"
        done
        output+="\n"
    fi

    output+="========================================\n"
    output+="SUMMARY\n"
    output+="========================================\n"
    output+="Automated checks: $PASSED_CHECKS/$TOTAL_CHECKS passed\n"
    output+="Manual review required: $MANUAL_CHECKS items\n"

    local percentage=0
    if [ $TOTAL_CHECKS -gt 0 ]; then
        percentage=$((PASSED_CHECKS * 100 / TOTAL_CHECKS))
    fi
    output+="Pass rate: $percentage%\n"

    if [ $FAILED_CHECKS -eq 0 ]; then
        output+="Status: [PASS] PASS\n"
    else
        output+="Status: [FAIL] FAIL ($FAILED_CHECKS issues)\n"
    fi
    output+="========================================\n"

    echo -e "$output"

    if [ -n "$REPORT_FILE" ]; then
        echo -e "$output" > "$REPORT_FILE"
        print_status "Report written to: $REPORT_FILE"
    fi
}

# Main
main() {
    # Change to project root directory (script lives at scripts/ros2/)
    cd "$(dirname "$0")/../.."

    parse_args "$@"

    print_status "Running ROS2 code review checks on: $TARGET_PATH"
    echo ""

    check_naming_conventions
    check_file_organization
    check_formatting
    check_ros2_patterns
    check_error_handling
    check_documentation
    check_safety_practices
    check_testing
    check_star_specific
    check_ci_cd

    echo ""
    generate_report

    # Exit with error if any checks failed
    if [ $FAILED_CHECKS -gt 0 ]; then
        exit 1
    fi
    exit 0
}

main "$@"
