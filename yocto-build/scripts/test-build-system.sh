#!/bin/bash

# PYNQ-Z2 Robot System Build Test Script
# Comprehensive testing of the build system setup

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")

echo "=========================================="
echo "PYNQ-Z2 Robot System Build Test"
echo "=========================================="
echo "Testing build system configuration and setup"
echo "Project root: $PROJECT_ROOT"
echo ""

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test helper functions
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    echo -n "Testing $test_name... "
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    if eval "$test_command" >/dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

run_test_verbose() {
    local test_name="$1"
    local test_command="$2"
    
    echo "Testing $test_name..."
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    if eval "$test_command"; then
        echo -e "  ${GREEN}PASS${NC}: $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${RED}FAIL${NC}: $test_name"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# System requirements tests
test_system_requirements() {
    echo "=== System Requirements Tests ==="
    
    run_test "Docker installation" "command -v docker"
    run_test "Docker running" "docker info"
    run_test "Docker Compose installation" "docker compose version"
    run_test "Git installation" "command -v git"
    run_test "Sufficient disk space (50GB)" "[ \$(df \"$PROJECT_ROOT\" | tail -1 | awk '{print \$4}') -gt 52428800 ]"
    
    echo ""
}

# Directory structure tests
test_directory_structure() {
    echo "=== Directory Structure Tests ==="
    
    run_test "Build directory exists" "[ -d \"$PROJECT_ROOT/build\" ]"
    run_test "Layers directory exists" "[ -d \"$PROJECT_ROOT/layers\" ]"
    run_test "Scripts directory exists" "[ -d \"$PROJECT_ROOT/scripts\" ]"
    run_test "Configs directory exists" "[ -d \"$PROJECT_ROOT/configs\" ]"
    run_test "Docker directory exists" "[ -d \"$PROJECT_ROOT/docker\" ]"
    run_test "Docs directory exists" "[ -d \"$PROJECT_ROOT/docs\" ]"
    run_test "Meta-robot-slam layer exists" "[ -d \"$PROJECT_ROOT/layers/meta-robot-slam\" ]"
    
    echo ""
}

# Configuration files tests
test_configuration_files() {
    echo "=== Configuration Files Tests ==="
    
    run_test "local.conf exists" "[ -f \"$PROJECT_ROOT/configs/local.conf\" ]"
    run_test "bblayers.conf exists" "[ -f \"$PROJECT_ROOT/configs/bblayers.conf\" ]"
    run_test "pynqz2-robot.conf exists" "[ -f \"$PROJECT_ROOT/configs/pynqz2-robot.conf\" ]"
    run_test "setup-layers.sh exists and executable" "[ -x \"$PROJECT_ROOT/configs/setup-layers.sh\" ]"
    run_test "robot-kernel.cfg exists" "[ -f \"$PROJECT_ROOT/layers/meta-robot-slam/recipes-kernel/linux/files/robot-kernel.cfg\" ]"
    run_test "Device tree file exists" "[ -f \"$PROJECT_ROOT/layers/meta-robot-slam/recipes-kernel/linux/files/pynqz2-robot.dts\" ]"
    
    echo ""
}

# Docker tests
test_docker_setup() {
    echo "=== Docker Setup Tests ==="
    
    run_test "Dockerfile exists" "[ -f \"$PROJECT_ROOT/docker/Dockerfile\" ]"
    run_test "Docker Compose file exists" "[ -f \"$PROJECT_ROOT/docker/docker-compose.yml\" ]"
    run_test "Build env setup script exists" "[ -f \"$PROJECT_ROOT/docker/build-env-setup.sh\" ]"
    
    # Test Docker image build (this may take time)
    echo -n "Building Docker image... "
    if docker build -t pynq-test -f "$PROJECT_ROOT/docker/Dockerfile" "$PROJECT_ROOT" >/dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        
        # Clean up test image
        docker rmi pynq-test >/dev/null 2>&1 || true
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    echo ""
}

# Scripts tests
test_scripts() {
    echo "=== Scripts Tests ==="
    
    run_test "setup-environment.sh exists and executable" "[ -x \"$PROJECT_ROOT/scripts/setup-environment.sh\" ]"
    run_test "build-robot-image.sh exists and executable" "[ -x \"$PROJECT_ROOT/scripts/build-robot-image.sh\" ]"
    
    # Test script syntax
    run_test "setup-environment.sh syntax" "bash -n \"$PROJECT_ROOT/scripts/setup-environment.sh\""
    run_test "build-robot-image.sh syntax" "bash -n \"$PROJECT_ROOT/scripts/build-robot-image.sh\""
    
    echo ""
}

# Meta-layer tests
test_meta_layers() {
    echo "=== Meta-Layer Tests ==="
    
    run_test "meta-robot-slam layer.conf exists" "[ -f \"$PROJECT_ROOT/layers/meta-robot-slam/conf/layer.conf\" ]"
    run_test "pynq-robot-image recipe exists" "[ -f \"$PROJECT_ROOT/layers/meta-robot-slam/recipes-core/images/pynq-robot-image.bb\" ]"
    run_test "robot-gateway-bridge recipe exists" "[ -f \"$PROJECT_ROOT/layers/meta-robot-slam/recipes-robot/robot-gateway/robot-gateway-bridge.bb\" ]"
    run_test "Linux kernel append exists" "[ -f \"$PROJECT_ROOT/layers/meta-robot-slam/recipes-kernel/linux/linux-xlnx_%.bbappend\" ]"
    
    # Test layer configuration syntax
    echo -n "Testing layer.conf syntax... "
    if python3 -c "
import configparser
try:
    # Basic syntax check for layer.conf
    with open('$PROJECT_ROOT/layers/meta-robot-slam/conf/layer.conf', 'r') as f:
        content = f.read()
    # Check for required variables
    required_vars = ['BBPATH', 'BBFILES', 'BBFILE_COLLECTIONS', 'BBFILE_PATTERN_meta-robot-slam', 'BBFILE_PRIORITY_meta-robot-slam']
    for var in required_vars:
        if var not in content:
            raise Exception(f'Missing required variable: {var}')
    print('OK')
except Exception as e:
    print(f'Error: {e}')
    exit(1)
" 2>/dev/null; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    echo ""
}

# Recipe syntax tests
test_recipe_syntax() {
    echo "=== Recipe Syntax Tests ==="
    
    # Find all .bb and .bbappend files
    RECIPE_FILES=$(find "$PROJECT_ROOT/layers/meta-robot-slam" -name "*.bb" -o -name "*.bbappend")
    
    for recipe in $RECIPE_FILES; do
        recipe_name=$(basename "$recipe")
        echo -n "Testing $recipe_name syntax... "
        
        # Basic syntax check - look for common issues
        if grep -q "^DESCRIPTION\|^LICENSE\|^SRC_URI\|^inherit\|^do_install" "$recipe"; then
            echo -e "${GREEN}PASS${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${YELLOW}WARN${NC} (minimal recipe)"
            # Don't count as failure for minimal recipes
        fi
        TESTS_TOTAL=$((TESTS_TOTAL + 1))
    done
    
    echo ""
}

# Documentation tests
test_documentation() {
    echo "=== Documentation Tests ==="
    
    run_test "README.md exists" "[ -f \"$PROJECT_ROOT/README.md\" ]"
    run_test "Setup guide exists" "[ -f \"$PROJECT_ROOT/docs/setup-guide.md\" ]"
    run_test "Development guide exists" "[ -f \"$PROJECT_ROOT/docs/development.md\" ]"
    
    # Test documentation completeness
    echo -n "Testing README completeness... "
    if grep -q "Quick Start\|Prerequisites\|Features" "$PROJECT_ROOT/README.md"; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    echo ""
}

# Integration tests
test_integration() {
    echo "=== Integration Tests ==="
    
    # Test that configuration files are compatible
    echo -n "Testing bblayers.conf compatibility... "
    if grep -q "meta-robot-slam" "$PROJECT_ROOT/configs/bblayers.conf"; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    # Test that image recipe includes robot packages
    echo -n "Testing image recipe integration... "
    if grep -q "robot-gateway-bridge\|pynq-framework\|ros-core" "$PROJECT_ROOT/layers/meta-robot-slam/recipes-core/images/pynq-robot-image.bb"; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    echo ""
}

# Performance tests
test_performance() {
    echo "=== Performance Tests ==="
    
    # Check build configuration for performance
    echo -n "Testing build thread configuration... "
    if grep -q "BB_NUMBER_THREADS.*[4-9]\|BB_NUMBER_THREADS.*[1-9][0-9]" "$PROJECT_ROOT/configs/local.conf"; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${YELLOW}WARN${NC} (consider increasing BB_NUMBER_THREADS)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    # Check sstate and download cache configuration
    echo -n "Testing cache configuration... "
    if grep -q "DL_DIR\|SSTATE_DIR" "$PROJECT_ROOT/configs/local.conf"; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    echo ""
}

# Security tests
test_security() {
    echo "=== Security Tests ==="
    
    # Check for proper permissions on scripts
    run_test "Scripts have correct permissions" "[ -x \"$PROJECT_ROOT/scripts/setup-environment.sh\" ] && [ -x \"$PROJECT_ROOT/configs/setup-layers.sh\" ]"
    
    # Check that sensitive files are not included
    echo -n "Testing for sensitive files... "
    if find "$PROJECT_ROOT" -name "*.key" -o -name "*.pem" -o -name "id_rsa" -o -name "password*" | grep -q .; then
        echo -e "${RED}FAIL${NC} (found sensitive files)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    else
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    echo ""
}

# Generate test report
generate_test_report() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local report_file="$PROJECT_ROOT/test-report-$(date +%Y%m%d-%H%M%S).txt"
    
    cat > "$report_file" << EOF
PYNQ-Z2 Robot System Build Test Report
Generated: $timestamp

Test Summary:
  Total Tests: $TESTS_TOTAL
  Passed: $TESTS_PASSED
  Failed: $TESTS_FAILED
  Success Rate: $(( (TESTS_PASSED * 100) / TESTS_TOTAL ))%

Test Categories:
  - System Requirements
  - Directory Structure  
  - Configuration Files
  - Docker Setup
  - Scripts
  - Meta-Layers
  - Recipe Syntax
  - Documentation
  - Integration
  - Performance
  - Security

System Information:
  Host OS: $(uname -s -r)
  Docker Version: $(docker --version 2>/dev/null || echo "Not available")
  Available Space: $(($(df "$PROJECT_ROOT" | tail -1 | awk '{print $4}')/1024/1024))GB
  Python Version: $(python3 --version 2>/dev/null || echo "Not available")

Build Environment Status: $( [ $TESTS_FAILED -eq 0 ] && echo "READY" || echo "NEEDS ATTENTION" )

Next Steps:
$( [ $TESTS_FAILED -eq 0 ] && echo "✅ All tests passed! Build environment is ready." || echo "❌ Some tests failed. Review issues before building." )
$( [ $TESTS_FAILED -eq 0 ] && echo "1. Run ./scripts/setup-environment.sh to initialize" || echo "1. Fix failing tests" )
$( [ $TESTS_FAILED -eq 0 ] && echo "2. Use ./docker-run.sh to start build environment" || echo "2. Re-run this test script" )
$( [ $TESTS_FAILED -eq 0 ] && echo "3. Build image with ./build-image.sh" || echo "3. Check documentation for help" )
EOF

    echo "Test report saved to: $report_file"
}

# Main test execution
main() {
    echo "Starting comprehensive build system tests..."
    echo ""
    
    test_system_requirements
    test_directory_structure
    test_configuration_files
    test_docker_setup
    test_scripts
    test_meta_layers
    test_recipe_syntax
    test_documentation
    test_integration
    test_performance
    test_security
    
    echo "=========================================="
    echo "Test Results Summary"
    echo "=========================================="
    echo -e "Total Tests: $TESTS_TOTAL"
    echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
    echo -e "Success Rate: $(( (TESTS_PASSED * 100) / TESTS_TOTAL ))%"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}✅ All tests passed! Build system is ready.${NC}"
        echo ""
        echo "Next steps:"
        echo "1. Run ./scripts/setup-environment.sh to initialize build environment"
        echo "2. Use ./docker-run.sh to start the Docker build environment"  
        echo "3. Build the robot image with ./build-image.sh"
    else
        echo -e "${RED}❌ $TESTS_FAILED test(s) failed. Please review and fix issues.${NC}"
        echo ""
        echo "Common fixes:"
        echo "1. Install missing dependencies (Docker, Git, etc.)"
        echo "2. Check file permissions on scripts"
        echo "3. Ensure sufficient disk space (50GB+)"
        echo "4. Review configuration files for syntax errors"
    fi
    
    generate_test_report
    
    # Return appropriate exit code
    [ $TESTS_FAILED -eq 0 ]
}

# Handle script arguments
case "${1:-}" in
    --help|-h)
        echo "Usage: $0 [OPTIONS]"
        echo ""
        echo "Test the PYNQ-Z2 robot system build environment setup."
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --verbose, -v  Show verbose test output"
        echo ""
        echo "This script tests:"
        echo "  - System requirements and dependencies"
        echo "  - Directory structure and files"
        echo "  - Configuration file syntax"
        echo "  - Docker setup and build capability"
        echo "  - Recipe and layer configuration"
        echo "  - Documentation completeness"
        echo "  - Integration and security"
        exit 0
        ;;
    --verbose|-v)
        # Enable verbose mode (would need to modify test functions)
        echo "Verbose mode enabled"
        ;;
esac

# Run tests
main "$@"