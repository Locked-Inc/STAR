# STAR ESP32 Firmware Test Application

## What is test_app?

The `test_app` folder is a **standalone ESP-IDF application** specifically designed to run the complete test suite on ESP32 hardware. It's separate from your production firmware to keep test code isolated.

### Why a Separate Test Application?

1. **Hardware Testing**: Tests run on actual ESP32 hardware to verify:
   - FreeRTOS mutex operations and thread safety
   - Memory allocation in embedded environment
   - Hardware-specific behavior (GPIO, timers, etc.)

2. **Separate from Production**: Your main firmware is for production use. `test_app` is only for testing:
   - Flash `test_app` -> Run 100 tests -> Verify results
   - Flash production firmware -> Deploy to device

3. **Complete Test Coverage**: Contains all 100 comprehensive tests:
   - 30 error_handler tests
   - 35 pin_validator tests
   - 35 protocol tests

## Test Components

Current test coverage (100 tests total):

| Component | Tests | Description |
|-----------|-------|-------------|
| `star_error_handler` | 30 | Error tracking, retry logic, state management |
| `star_pin_validator` | 35 | GPIO pin conflict detection and validation |
| `pynq_wifi_bridge/protocol` | 35 | Protocol packet creation, parsing, and transport |

## Quick Start

### Build and Run All Tests

```bash
cd test_app

# Build test application
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB1 flash

# Monitor test output
idf.py -p /dev/ttyUSB1 monitor
```

### Or Use the Test Runner Script

```bash
cd test_app
./run_tests.sh
```

## Expected Output

When all tests pass:

```
====================================
   STAR Test Application Started
====================================

================================================================================
                      STAR Test Framework - Running Tests
================================================================================

[ RUN      ] [error_handler] init_with_valid_params
[       OK ] [error_handler] init_with_valid_params
[ RUN      ] [error_handler] record_error_increments_count
[       OK ] [error_handler] record_error_increments_count
...
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

## Testing Framework: STAR_TEST

This project uses **STAR_TEST**, a custom lightweight testing framework designed specifically for ESP32 embedded testing.

### Why STAR_TEST?

Unity test framework had compatibility issues with ESP32 constructor-based auto-registration, causing boot loops. STAR_TEST was created as a lightweight, embedded-friendly alternative with:

- Auto-registration using GCC constructor attributes
- No linker conflicts between multiple test files
- Minimal memory footprint
- ESP32-optimized design

### Available Assertions

```c
// Boolean assertions
STAR_ASSERT(condition)
STAR_ASSERT_TRUE(condition)
STAR_ASSERT_FALSE(condition)

// Equality assertions
STAR_ASSERT_EQUAL(expected, actual)
STAR_ASSERT_NOT_EQUAL(expected, actual)

// Pointer assertions
STAR_ASSERT_NULL(ptr)
STAR_ASSERT_NOT_NULL(ptr)

// String assertions
STAR_ASSERT_STR_EQUAL(expected, actual)
```

## Directory Structure

```
test_app/
+-- CMakeLists.txt              # Test app project config
+-- sdkconfig.defaults          # ESP32 configuration for testing
+-- run_tests.sh                # Helper script to run tests
+-- README.md                   # This file
+-- TESTING_GUIDE.md            # Detailed testing guide
+-- main/
    +-- CMakeLists.txt          # Main component config
    +-- test_main.c             # Test runner entry point
    +-- test_error_handler.c    # 30 error handler tests
    +-- test_pin_validator.c    # 35 pin validator tests
    +-- test_protocol.c         # 35 protocol tests
```

## Configuration

### sdkconfig.defaults

Key test configuration options:
- `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` - Increased stack for tests
- `CONFIG_HEAP_POISONING_COMPREHENSIVE=y` - Memory debugging
- Serial output at 115200 baud

### Timeout

Tests complete within 2 minutes on ESP32. The application continues running after tests finish.

## Troubleshooting

### ESP32 Not Detected

Check USB connection:
```bash
# Verify device
ls -la /dev/ttyUSB*

# Ensure you're in dialout group
sudo usermod -a -G dialout $USER
# (logout and login required)

# Try different port
idf.py -p /dev/ttyUSB0 monitor
```

### Out of Memory Errors

Increase heap/stack in `sdkconfig.defaults`:
```
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384
```

### Tests Hang or Timeout

- Check for deadlocks in mutex usage
- Verify FreeRTOS task priorities
- Enable watchdog warnings
- Try erasing flash: `idf.py erase-flash`

### Build Errors

```bash
# Clean rebuild
idf.py fullclean
idf.py build

# Verify ESP-IDF environment
. ~/esp/esp-idf/export.sh
```

## Adding New Tests

### 1. Create Test File

Add a new test file in `test_app/main/`:

```c
#include "star_test.h"
#include "your_component.h"

STAR_TEST_CASE(your_component, test_basic_functionality)
{
  int result = your_function(5);
  STAR_ASSERT_EQUAL(10, result);
}

STAR_TEST_CASE(your_component, test_error_handling)
{
  int result = your_function(-1);
  STAR_ASSERT_EQUAL(-1, result);
}

/* Register tests */
STAR_TEST_LIST_BEGIN()
  STAR_TEST_REF(your_component, test_basic_functionality)
  STAR_TEST_REF(your_component, test_error_handling)
STAR_TEST_LIST_END()
```

### 2. Update CMakeLists.txt

Add your test file to `test_app/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "test_main.c"
         "test_error_handler.c"
         "test_pin_validator.c"
         "test_protocol.c"
         "test_your_component.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES star_test star_error_handler star_pin_validator
                  pynq_wifi_bridge your_component esp_driver_gpio
)
```

### 3. Rebuild and Run

```bash
idf.py build
idf.py flash monitor
```

## Integration with CI/CD

This test app can be integrated into CI/CD pipelines:

```yaml
- name: Run ESP32 Hardware Tests
  run: |
    cd esp32-firmware/test_app
    idf.py build
    idf.py -p /dev/ttyUSB1 flash monitor | tee test_output.log
    grep "ALL TESTS PASSED" test_output.log
```

## Workflow: Development -> Testing -> Production

1. **Development**: Work on main firmware in `esp32-firmware/main/`
2. **Testing**:
   ```bash
   cd test_app
   idf.py flash monitor  # Run all 100 tests
   ```
3. **Production**: After tests pass
   ```bash
   cd ..  # Back to esp32-firmware root
   idf.py flash monitor  # Flash production firmware
   ```

## Notes

- Test execution order is not guaranteed (tests run as registered)
- Tests should be independent and self-contained
- Each test has setup/teardown via helper functions
- Production builds exclude all test code
- Tests auto-register using GCC constructor attributes

## Further Reading

- [TESTING_GUIDE.md](TESTING_GUIDE.md) - Detailed testing documentation
- [STAR_TEST Framework](../components/star_test/README.md) - Framework documentation
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
