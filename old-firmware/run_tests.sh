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
    echo -e "${GREEN}[OK] $1${NC}"
}

print_error() {
    echo -e "${RED}[FAIL] $1${NC}"
}

print_info() {
    echo -e "${YELLOW}[INFO] $1${NC}"
}

# Function to detect ESP32 device with retry
detect_esp32_device() {
    local max_attempts=3
    local attempt=1

    print_info "Detecting ESP32 device..." >&2

    while [ $attempt -le $max_attempts ]; do
        # Try to find ESP32 using esptool
        local devices=()

        # Check common serial ports
        for port in /dev/ttyUSB* /dev/ttyACM* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART*; do
            # Skip glob patterns that don't match any files
            if [[ "$port" == *"*"* ]]; then
                continue
            fi

            if [ -e "$port" ]; then
                # Check if we can access it and if it's an ESP32
                if timeout 5 python3 -m esptool --chip esp32 --port "$port" chip_id >/dev/null 2>&1; then
                    devices+=("$port")
                    print_success "Found ESP32 on $port" >&2
                else
                    # Check if port exists but we don't have permission
                    if [ ! -r "$port" ] || [ ! -w "$port" ]; then
                        print_info "Port $port exists but no read/write permission" >&2
                        print_info "Run: sudo chmod 666 $port" >&2
                    fi
                fi
            fi
        done

        if [ ${#devices[@]} -eq 0 ]; then
            if [ $attempt -lt $max_attempts ]; then
                print_info "No ESP32 found (attempt $attempt/$max_attempts). Retrying in 2 seconds..." >&2
                sleep 2
                attempt=$((attempt + 1))
            else
                print_error "No ESP32 device detected after $max_attempts attempts!" >&2
                echo "" >&2
                print_info "Available serial ports:" >&2
                ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null >&2 || echo "  (none found)" >&2
                echo "" >&2
                print_info "Troubleshooting steps:" >&2
                print_info "  1. Check if ESP32 is connected via USB" >&2
                print_info "  2. Check USB cable (some are power-only)" >&2
                print_info "  3. Try a different USB port" >&2
                print_info "  4. Check dmesg: dmesg | tail -20" >&2
                print_info "  5. Check permissions: sudo usermod -a -G dialout \$USER (requires logout)" >&2
                print_info "  6. Temporary fix: sudo chmod 666 /dev/ttyUSB*" >&2
                print_info "  7. Check if device is detected: lsusb | grep -i 'CP210\\|CH340\\|FTDI'" >&2
                return 1
            fi
        elif [ ${#devices[@]} -gt 1 ]; then
            print_info "Multiple ESP32 devices found:" >&2
            for i in "${!devices[@]}"; do
                echo "  $((i+1)). ${devices[$i]}" >&2
            done
            if [ -n "$ESP_PORT" ]; then
                print_info "Using ESP_PORT environment variable: $ESP_PORT" >&2
                echo "$ESP_PORT"
            else
                print_info "Using first device: ${devices[0]}" >&2
                print_info "Set ESP_PORT environment variable to choose a specific device" >&2
                echo "${devices[0]}"
            fi
            return 0
        else
            echo "${devices[0]}"
            return 0
        fi
    done
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

    # Detect ESP32 device
    local detected_port
    if [ -n "$ESP_PORT" ]; then
        print_info "Using ESP_PORT from environment: $ESP_PORT"
        detected_port="$ESP_PORT"
        # Verify it exists and is accessible
        if [ ! -e "$detected_port" ]; then
            print_error "Specified port $detected_port does not exist!"
            detected_port=$(detect_esp32_device)
            if [ $? -ne 0 ] || [ -z "$detected_port" ]; then
                return 1
            fi
        fi
    else
        # detect_esp32_device prints messages to stderr, port to stdout
        detected_port=$(detect_esp32_device)
        if [ $? -ne 0 ] || [ -z "$detected_port" ]; then
            return 1
        fi
    fi

    print_success "Using ESP32 on port: $detected_port"

    # Build test application
    print_info "Building test application..."
    idf.py build

    # Flash and monitor
    print_info "Flashing test application to ESP32..."
    idf.py -p "$detected_port" flash

    print_info "Resetting ESP32 and monitoring output..."

    # Reset the ESP32 to start fresh test run
    python3 -m esptool --chip esp32 --port "$detected_port" run >/dev/null 2>&1 || true

    sleep 1

    # Create temporary file for monitor output
    local monitor_output=$(mktemp)

    # Use Python to read serial output with timeout
    print_info "Monitoring test execution..."

    python3 - <<'EOF' "$detected_port" "$monitor_output" &
import serial
import sys
import time
import re

port = sys.argv[1]
output_file = sys.argv[2]

# Track failures and reboots
test_failed = False
panic_detected = False
reboot_count = 0
last_reboot_time = 0
consecutive_reboots_threshold = 2

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

                    # Check for test failures
                    if '[  FAILED  ]' in line or 'STAR_TEST: Test failed:' in line:
                        test_failed = True

                    # Check for fatal errors/panics
                    if any(marker in line for marker in [
                        "Guru Meditation Error",
                        "panic'ed",
                        "abort() was called",
                        "***ERROR***",
                        "Core dump",
                        "Unhandled exception"
                    ]):
                        panic_detected = True
                        print("\n*** FATAL ERROR DETECTED ***\n", file=sys.stderr)

                    # Check for reboots
                    if 'rst:0x' in line or 'ets Jun  8 2016' in line or 'ESP-IDF' in line and 'stage bootloader' in line:
                        current_time = time.time()
                        if current_time - last_reboot_time < 30:  # Reboot within 30 seconds
                            reboot_count += 1
                            print(f"\n*** REBOOT DETECTED ({reboot_count}/{consecutive_reboots_threshold}) ***\n", file=sys.stderr)

                            if reboot_count >= consecutive_reboots_threshold:
                                print("\n*** TOO MANY REBOOTS - STOPPING TEST ***\n", file=sys.stderr)
                                # Write failure marker
                                f.write("===TEST_EXECUTION_COMPLETE:FAIL===\n")
                                f.flush()
                                break
                        else:
                            reboot_count = 1
                        last_reboot_time = current_time

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
    local has_failures=0
    local has_panic=0
    local has_reboots=0

    if grep -q "===TEST_EXECUTION_COMPLETE:PASS===" "$monitor_output" 2>/dev/null; then
        test_result="PASS"
    elif grep -q "===TEST_EXECUTION_COMPLETE:FAIL===" "$monitor_output" 2>/dev/null; then
        test_result="FAIL"
    fi

    # Analyze the test output
    if grep -q "FAILED" "$monitor_output" 2>/dev/null; then
        has_failures=1
    fi
    if grep -q "Guru Meditation Error\|panic'ed" "$monitor_output" 2>/dev/null; then
        has_panic=1
    fi
    if grep -c "rst:0x" "$monitor_output" 2>/dev/null | awk '{if ($1 > 1) exit 0; else exit 1}'; then
        has_reboots=1
    fi

    # Generate summary
    echo ""
    print_header "Test Results Summary"

    if [ $has_failures -eq 1 ]; then
        local failed_count=$(grep -c "\[  FAILED  \]" "$monitor_output" 2>/dev/null || echo "0")
        print_error "Test Failures: $failed_count test(s) failed"
        echo ""
        print_info "Failed tests:"
        grep "\[  FAILED  \]" "$monitor_output" 2>/dev/null | grep -v "test_system_handler.c" | sed 's/^/  /' || true
    fi

    if [ $has_panic -eq 1 ]; then
        echo ""
        print_error "Fatal Error: Guru Meditation / Panic detected"
        print_info "Last panic location:"
        grep -A 5 "Guru Meditation Error" "$monitor_output" 2>/dev/null | head -10 | sed 's/^/  /' || true
    fi

    if [ $has_reboots -eq 1 ]; then
        local reboot_count=$(grep -c "rst:0x" "$monitor_output" 2>/dev/null || echo "0")
        echo ""
        print_error "Unexpected Reboots: $reboot_count reboot(s) detected"
    fi

    # Save detailed log
    local log_file="$SCRIPT_DIR/test_results_$(date +%Y%m%d_%H%M%S).log"
    cp "$monitor_output" "$log_file"
    print_info "Full test log saved to: $log_file"

    # Clean up
    rm -f "$monitor_output"

    cd "$SCRIPT_DIR"

    # Report results
    echo ""
    if [ "$test_result" = "PASS" ]; then
        print_success "All tests passed!"
        return 0
    elif [ "$test_result" = "FAIL" ]; then
        print_error "Tests FAILED!"
        print_info "Review the log file for details"
        return 1
    else
        print_error "Test timeout or completion marker not detected!"
        print_info "The test may have crashed or hung"
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
