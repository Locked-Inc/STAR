#!/usr/bin/env bash
# esp32-firmware/run_tests.sh

#
# run_tests.sh - Run all tests for STAR ESP32 firmware
#
# Usage:
#   ./run_tests.sh [target|host|coverage|all]
#
#   target   - Run tests on ESP32 hardware (requires connected device)
#   host     - Run tests on host machine (Linux/Mac, no hardware needed)
#   coverage - Run tests with code coverage (host only)
#   all      - Run both target and host tests
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_APP_DIR="$SCRIPT_DIR/test_app"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

# Function to run tests on ESP32 hardware
run_target_tests() {
    print_header "Running Tests on ESP32 Hardware"

    cd "$TEST_APP_DIR"

    # Check if IDF is sourced
    if [ -z "$IDF_PATH" ]; then
        print_info "Sourcing ESP-IDF environment..."
        source ~/esp/esp-idf/export.sh
    fi

    # Build test application
    print_info "Building test application..."
    idf.py build

    # Check if device is connected
    if [ ! -e "/dev/ttyUSB0" ] && [ ! -e "/dev/ttyUSB1" ]; then
        print_error "No ESP32 device found on /dev/ttyUSB0 or /dev/ttyUSB1"
        return 1
    fi

    # Flash and monitor
    print_info "Flashing test application to ESP32..."
    idf.py -p ${ESP_PORT:-/dev/ttyUSB1} flash

    print_info "Resetting ESP32 and monitoring output..."

    # Reset the ESP32 to start fresh test run
    python3 -m esptool --chip esp32 --port ${ESP_PORT:-/dev/ttyUSB1} run >/dev/null 2>&1 || true

    sleep 1

    # Create temporary file for monitor output
    local monitor_output=$(mktemp)

    # Use Python to read serial output with timeout
    print_info "Monitoring test execution..."

    python3 - <<'EOF' "${ESP_PORT:-/dev/ttyUSB1}" "$monitor_output" &
import serial
import sys
import time

port = sys.argv[1]
output_file = sys.argv[2]

try:
    ser = serial.Serial(port, 115200, timeout=1)
    with open(output_file, 'w') as f:
        start_time = time.time()
        timeout = 300  # 5 minutes

        while (time.time() - start_time) < timeout:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='replace')
                    print(line, end='', flush=True)
                    f.write(line)
                    f.flush()

                    # Check for completion markers
                    if '===TEST_EXECUTION_COMPLETE:' in line:
                        time.sleep(1)  # Wait for any remaining output
                        # Read remaining output
                        for _ in range(10):
                            if ser.in_waiting > 0:
                                remaining = ser.readline().decode('utf-8', errors='replace')
                                print(remaining, end='', flush=True)
                                f.write(remaining)
                                f.flush()
                            else:
                                time.sleep(0.1)
                        break
                except Exception as e:
                    pass
            else:
                time.sleep(0.1)

    ser.close()
except Exception as e:
    print(f"Serial error: {e}", file=sys.stderr)
    sys.exit(1)
EOF
    local monitor_pid=$!

    # Wait for monitor to finish
    wait $monitor_pid 2>/dev/null || true

    # Give it a moment to finish writing
    sleep 1

    # Check results from the output file
    local test_result=""
    if grep -q "===TEST_EXECUTION_COMPLETE:PASS===" "$monitor_output" 2>/dev/null; then
        test_result="PASS"
    elif grep -q "===TEST_EXECUTION_COMPLETE:FAIL===" "$monitor_output" 2>/dev/null; then
        test_result="FAIL"
    fi

    # Clean up
    rm -f "$monitor_output"

    cd "$SCRIPT_DIR"

    # Report results
    echo ""
    if [ "$test_result" = "PASS" ]; then
        print_success "All tests passed!"
        return 0
    elif [ "$test_result" = "FAIL" ]; then
        print_error "Some tests failed!"
        return 1
    else
        print_error "Test timeout or completion marker not detected!"
        return 1
    fi
}

# Function to run tests on host
run_host_tests() {
    print_header "Running Tests on Host (Linux Build)"

    print_info "Host-based testing not yet configured"
    print_info "This requires ESP-IDF Linux target support"
    print_info "Coming soon..."

    # TODO: Implement host-based testing
    # cd "$TEST_APP_DIR"
    # idf.py --preview set-target linux
    # idf.py build
    # ./build/star_test_app.elf

    return 0
}

# Function to run tests with coverage
run_coverage_tests() {
    print_header "Running Tests with Code Coverage"

    print_info "Coverage testing not yet configured"
    print_info "This requires gcov/lcov integration"
    print_info "Coming soon..."

    # TODO: Implement coverage testing
    # cd "$TEST_APP_DIR"
    # idf.py --preview set-target linux
    # idf.py -D CMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage" build
    # ./build/star_test_app.elf
    # lcov --capture --directory . --output-file coverage.info
    # genhtml coverage.info --output-directory coverage_html

    return 0
}

# Main script
main() {
    local test_mode="${1:-target}"

    print_header "STAR ESP32 Firmware Test Runner"

    case "$test_mode" in
        target)
            run_target_tests
            ;;
        host)
            run_host_tests
            ;;
        coverage)
            run_coverage_tests
            ;;
        all)
            run_target_tests
            run_host_tests
            ;;
        *)
            echo "Usage: $0 [target|host|coverage|all]"
            echo ""
            echo "  target   - Run tests on ESP32 hardware (default)"
            echo "  host     - Run tests on host machine (no hardware)"
            echo "  coverage - Run tests with code coverage"
            echo "  all      - Run all test modes"
            exit 1
            ;;
    esac

    print_header "Test Run Complete"
}

main "$@"
