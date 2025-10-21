# STAR Testing Guide

This guide explains the testing infrastructure for the STAR ESP32 firmware.

## Overview

The STAR project has **100 comprehensive tests** organized in a dedicated test application (`test_app`) that runs on ESP32 hardware.

### Test Coverage

| Component | Tests | Coverage |
|-----------|-------|----------|
| **star_error_handler** | 30 | Initialization, error recording, retry logic, state management, edge cases |
| **star_pin_validator** | 35 | Pin registration, sharing, validation, conflict detection, cleanup |
| **star_wifi_bridge/protocol** | 35 | Packet creation, parsing, response handling, transport layer |
| **Total** | **100** | Complete firmware component coverage |

## Testing Framework: STAR_TEST

The STAR project uses a custom lightweight testing framework called **STAR_TEST**, designed specifically for ESP32 embedded testing.

### Why STAR_TEST Instead of Unity?

Unity test framework was initially attempted but had compatibility issues with the ESP32 environment, particularly with constructor-based auto-registration causing boot loops. STAR_TEST was created as a lightweight, embedded-friendly alternative.

### Key Features

- **Auto-registration**: Tests automatically register using GCC constructor attributes
- **No linker conflicts**: Multiple test files coexist without naming collisions
- **Minimal footprint**: Optimized for embedded systems with limited memory
- **ESP32-optimized**: Designed specifically for ESP-IDF and FreeRTOS

## Quick Start

### Running All 100 Tests

The easiest way to run tests is using the provided script:

```bash
cd /home/bsikar/Documents/git/STAR/esp32-firmware/test_app
./run_tests.sh
```

Or manually:

```bash
cd test_app
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

Press `Ctrl+]` to exit the monitor.

### Expected Runtime

All 100 tests complete in approximately **1-2 minutes** on ESP32 hardware.

## Test Structure

### Architecture

```
test_app/                         # Standalone test application
+-- main/
|   +-- test_main.c              # Test runner entry point
|   +-- test_error_handler.c     # 30 tests for error_handler
|   +-- test_pin_validator.c     # 35 tests for pin_validator
|   +-- test_protocol.c          # 35 tests for protocol
+-- CMakeLists.txt               # Build configuration
+-- sdkconfig.defaults           # ESP32 test configuration
+-- run_tests.sh                 # Convenience script

components/
+-- star_test/                   # The test framework itself
|   +-- include/star_test.h      # Public API and macros
|   +-- star_test.c              # Implementation
|   +-- README.md                # Framework documentation
+-- star_error_handler/
|   +-- test/                    # Placeholder (tests moved to test_app)
+-- star_pin_validator/
|   +-- test/                    # Placeholder (tests moved to test_app)
+-- star_wifi_bridge/
    +-- test/                    # Placeholder (tests moved to test_app)
```

### Why test_app Directory?

All tests are in `test_app/main/` because:

1. **Centralized Test Runner**: Single application entry point runs all tests
2. **No Linker Conflicts**: With STAR_TEST's constructor-based registration, all test files can be in one place
3. **Easy to Run**: One command flashes and runs all 100 tests
4. **Isolated from Production**: Test code completely separate from production firmware

## Writing Tests

### 1. Basic Test Structure

```c
#include "star_test.h"
#include "your_component.h"

STAR_TEST_CASE(your_component, test_basic_functionality)
{
  // Arrange
  int input = 5;

  // Act
  int result = your_function(input);

  // Assert
  STAR_ASSERT_EQUAL(10, result);
}

STAR_TEST_CASE(your_component, test_error_handling)
{
  int result = your_function(-1);
  STAR_ASSERT_EQUAL(-1, result);
}
```

### 2. Register Your Tests

At the end of your test file:

```c
/* Register tests */
STAR_TEST_LIST_BEGIN()
  STAR_TEST_REF(your_component, test_basic_functionality)
  STAR_TEST_REF(your_component, test_error_handling)
STAR_TEST_LIST_END()
```

The `STAR_TEST_LIST_END()` macro automatically generates a constructor function that registers all tests when the module loads.

### 3. Add to Build

Update `test_app/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "test_main.c"
         "test_error_handler.c"
         "test_pin_validator.c"
         "test_protocol.c"
         "test_your_component.c"        # Add your test file
    INCLUDE_DIRS "."
    PRIV_REQUIRES star_test
                  star_error_handler
                  star_pin_validator
                  star_wifi_bridge
                  esp_driver_gpio
                  your_component            # Add your component
)
```

### 4. Test Helper Patterns

**Setup/Teardown with Helper Functions:**

```c
/* Helper to clean up after each test */
static void cleanup_component(void)
{
  component_deinit();
}

STAR_TEST_CASE(component, test_with_cleanup)
{
  cleanup_component();  // Ensure clean state

  // Test code here

  cleanup_component();  // Clean up after
}
```

**Mock Functions for Dependencies:**

```c
/* Mock transport for testing */
static uint8_t g_transport_buffer[2048];
static int g_transport_error = 0;

int transport_send(const uint8_t* data, uint16_t len)
{
  if (g_transport_error) {
    return -1;
  }
  memcpy(g_transport_buffer, data, len);
  return len;
}

static void reset_transport_mock(void)
{
  memset(g_transport_buffer, 0, sizeof(g_transport_buffer));
  g_transport_error = 0;
}
```

## Available Assertions

### Boolean Assertions

```c
STAR_ASSERT(condition)           // Assert condition is true
STAR_ASSERT_TRUE(condition)      // Alias for STAR_ASSERT
STAR_ASSERT_FALSE(condition)     // Assert condition is false
```

### Equality Assertions

```c
STAR_ASSERT_EQUAL(expected, actual)      // Assert values are equal
STAR_ASSERT_NOT_EQUAL(expected, actual)  // Assert values are not equal
```

### Pointer Assertions

```c
STAR_ASSERT_NULL(ptr)        // Assert pointer is NULL
STAR_ASSERT_NOT_NULL(ptr)    // Assert pointer is not NULL
```

### String Assertions

```c
STAR_ASSERT_STR_EQUAL(expected, actual)  // Assert strings are equal
```

### Example Usage

```c
STAR_TEST_CASE(example, comprehensive_test)
{
  // Setup
  error_handler_t handler;
  esp_err_t result = error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  // Assert initialization succeeded
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NOT_NULL(handler.mutex);
  STAR_ASSERT_FALSE(handler.error_state);
  STAR_ASSERT_EQUAL(0, handler.error_count);

  // Test functionality
  result = error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  STAR_ASSERT_EQUAL(ESP_FAIL, result);
  STAR_ASSERT_TRUE(handler.error_state);
  STAR_ASSERT_EQUAL(1, handler.error_count);

  // Cleanup
  error_handler_deinit(&handler);
}
```

## Test Output

### Successful Test Run

```
====================================
   STAR Test Application Started
====================================

================================================================================
                      STAR Test Framework - Running Tests
================================================================================

[ RUN      ] [error_handler] init_with_valid_params
[       OK ] [error_handler] init_with_valid_params
[ RUN      ] [error_handler] init_null_handler
[       OK ] [error_handler] init_null_handler
[ RUN      ] [error_handler] record_error_increments_count
[       OK ] [error_handler] record_error_increments_count

... (97 more tests)

[ RUN      ] [protocol] send_response_different_statuses
[       OK ] [protocol] send_response_different_statuses

================================================================================
                          STAR Test Results Summary
================================================================================
  Total Tests:  100
  Passed:       100
  Failed:       0
================================================================================

  [OK][OK][OK] ALL TESTS PASSED! [OK][OK][OK]

Test execution complete. System will continue running.
```

### Failed Test Example

```
[ RUN      ] [pin_validator] register_invalid_pin_too_high
[  FAILED  ] test_pin_validator.c:58: Expected 257, got 0
[  FAILED  ] [pin_validator] register_invalid_pin_too_high

================================================================================
                          STAR Test Results Summary
================================================================================
  Total Tests:  100
  Passed:       99
  Failed:       1
================================================================================

  SOME TESTS FAILED
```

## Test Categories

### Error Handler Tests (30 tests)

**File**: `test_app/main/test_error_handler.c`

Categories:
- Initialization tests (5) - Valid params, null checks, parameter validation
- Deinitialization tests (3) - Cleanup, null handlers, double-free
- Error recording tests (8) - Count tracking, state management, storage
- Retry logic tests (7) - Backoff, max retries, reset callbacks
- State reset tests (5) - Manual reset, post-recovery, state clearing
- Edge cases (2) - Stress testing, boundary conditions

### Pin Validator Tests (35 tests)

**File**: `test_app/main/test_pin_validator.c`

Categories:
- Basic registration (10) - Valid/invalid pins, boundary checks, description validation
- Pin sharing (10) - Shared pins, exclusive pins, conflict detection
- Validation (8) - Comprehensive pin state validation
- Cleanup (7) - Memory management, double-free prevention

### Protocol Tests (35 tests)

**File**: `test_app/main/test_protocol.c`

Categories:
- Packet creation (12) - Various payload sizes, command types, encoding
- Packet parsing (13) - Validation, error handling, malformed packets
- Protocol send (10) - Response handling, error codes, transport mocking

## Development Workflow

### 1. Write Production Code

```bash
cd esp32-firmware
# Edit components/your_component/your_component.c
```

### 2. Write Tests

```bash
# Add tests to test_app/main/test_your_component.c
```

### 3. Run Tests

```bash
cd test_app
idf.py build
idf.py flash monitor
```

### 4. Iterate

Fix any failures, re-run tests until all pass.

### 5. Deploy Production Firmware

```bash
cd ..  # Back to esp32-firmware root
idf.py flash monitor
```

## Troubleshooting

### ESP32 Not Detected

```bash
# Check device connection
ls -l /dev/ttyUSB*

# Ensure correct permissions
sudo usermod -a -G dialout $USER
# Logout and login required

# Try different port
idf.py -p /dev/ttyUSB0 monitor
```

### Tests Not Running

1. **Verify flash succeeded**: Look for "Hard resetting via RTS pin... Done"
2. **Check serial port**: Ensure `/dev/ttyUSB1` is correct
3. **Erase flash first**: `idf.py erase-flash && idf.py flash`
4. **Check constructor registration**: Tests should auto-register on boot

### Build Errors

```bash
# Clean rebuild
cd test_app
idf.py fullclean
idf.py build

# Verify ESP-IDF environment
. ~/esp/esp-idf/export.sh

# Check component dependencies in CMakeLists.txt
```

### Memory Issues

If tests fail with memory errors:

```bash
# Edit sdkconfig.defaults
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384  # Increase from 8192
CONFIG_HEAP_POISONING_COMPREHENSIVE=y   # Already enabled

# Rebuild
idf.py fullclean build
```

## Best Practices

### Test Independence

Each test should:
- Set up its own state
- Clean up after itself
- Not depend on other tests
- Not assume execution order

```c
STAR_TEST_CASE(component, independent_test)
{
  cleanup_component();  // Start clean

  // Test code

  cleanup_component();  // End clean
}
```

### Descriptive Test Names

Use clear, descriptive names:

```c
// Good
STAR_TEST_CASE(pin_validator, register_invalid_pin_too_high)
STAR_TEST_CASE(protocol, create_packet_with_max_payload)

// Bad
STAR_TEST_CASE(pin_validator, test1)
STAR_TEST_CASE(protocol, bad_input)
```

### Test One Thing

Each test should verify one specific behavior:

```c
// Good - Tests one specific failure mode
STAR_TEST_CASE(error_handler, init_with_null_handler)
{
  esp_err_t result = error_handler_init(NULL, 3, 100, 5000, NULL, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

// Bad - Tests multiple unrelated things
STAR_TEST_CASE(error_handler, everything)
{
  // Tests init, record, reset, etc. all in one test
}
```

### Use Helper Functions

Create helpers for common setup/teardown:

```c
static error_handler_t create_test_handler(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  return handler;
}

STAR_TEST_CASE(error_handler, test_with_helper)
{
  error_handler_t handler = create_test_handler();

  // Test code

  error_handler_deinit(&handler);
}
```

## Hardware Requirements

- **ESP32**: ESP32-D0WDQ6 or compatible
- **Flash**: Minimum 2MB
- **Connection**: USB for flashing and serial output
- **UART**: 115200 baud
- **Power**: USB power sufficient for tests

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
name: ESP32 Hardware Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: self-hosted  # Requires ESP32 connected to runner
    steps:
      - uses: actions/checkout@v3

      - name: Setup ESP-IDF
        run: |
          . ~/esp/esp-idf/export.sh

      - name: Build Tests
        run: |
          cd esp32-firmware/test_app
          idf.py build

      - name: Flash and Run Tests
        run: |
          idf.py -p /dev/ttyUSB1 flash monitor | tee test_output.log

      - name: Verify Results
        run: |
          grep "ALL TESTS PASSED" test_output.log
          if [ $? -ne 0 ]; then
            echo "Tests failed!"
            exit 1
          fi
```

## Further Reading

- [README.md](README.md) - Quick start and overview
- [STAR_TEST Framework](../components/star_test/README.md) - Framework internals
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
- [Test Examples](main/) - See actual test implementations

## Summary

The STAR project has **100 comprehensive tests** covering all major components. Tests run on actual ESP32 hardware using the custom STAR_TEST framework. All tests are in the `test_app` directory, which builds as a standalone application separate from production firmware.

**To run all 100 tests:**
```bash
cd test_app
./run_tests.sh
```

Happy testing!
