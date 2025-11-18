# STAR ESP32 Firmware Testing Guide

This document describes the comprehensive testing infrastructure for the STAR ESP32 firmware project.

## Table of Contents

1. [Testing Philosophy](#testing-philosophy)
2. [Test Types](#test-types)
3. [Quick Start](#quick-start)
4. [Running Tests](#running-tests)
5. [Writing Tests](#writing-tests)
6. [Test Coverage](#test-coverage)
7. [Continuous Integration](#continuous-integration)
8. [Troubleshooting](#troubleshooting)

---

## Testing Philosophy

The STAR firmware project uses a **comprehensive testing strategy** with multiple layers:

- **Unit Tests**: Test individual functions and components in isolation
- **Integration Tests**: Test interactions between components
- **On-Target Tests**: Test on actual ESP32 hardware
- **Host-Based Tests**: Test on development machine (no hardware required)
- **Mock-Based Tests**: Test with mocked dependencies (WiFi, UART, etc.)

**Key Principles:**
- [x] Tests are **excluded from production builds** (zero overhead)
- [x] Tests run in `components/<component>/test/` directories
- [x] Unity framework for assertions
- [x] CMock for automatic mock generation
- [x] Code coverage tracking with gcov/lcov

---

## Test Types

### 1. Unit Tests

Test individual functions in isolation.

**Location**: `components/<component>/test/test_<component>.c`

**Example**:
```c
TEST_CASE("error_handler_init creates mutex", "[error_handler]")
{
  error_handler_t handler;
  esp_err_t ret = error_handler_init(&handler, 5, 1000, 10000, NULL, NULL);

  TEST_ASSERT_EQUAL(ESP_OK, ret);
  TEST_ASSERT_NOT_NULL(handler.mutex);
}
```

### 2. Integration Tests

Test multiple components working together.

**Location**: Same as unit tests, but test interactions.

**Example**:
```c
TEST_CASE("WiFi manager uses error handler for retries", "[integration]")
{
  // Test WiFi manager + error handler integration
}
```

### 3. On-Target Tests

Run tests on actual ESP32 hardware.

**Command**: `./run_tests.sh target`

**Advantages**:
- Tests real hardware behavior
- Tests FreeRTOS timing
- Tests actual GPIO/UART/WiFi
- Finds hardware-specific bugs

**Disadvantages**:
- Requires connected ESP32
- Slower than host tests
- Harder to debug

### 4. Host-Based Tests (Coming Soon)

Run tests on your development machine (Linux/Mac).

**Command**: `./run_tests.sh host`

**Advantages**:
- No hardware required
- Fast execution
- Easy debugging with gdb
- CI/CD friendly

**Disadvantages**:
- Can't test hardware peripherals
- Different timing behavior

---

## Quick Start

### Prerequisites

```bash
# ESP-IDF must be installed and sourced
source ~/esp/esp-idf/export.sh

# ESP32 connected via USB (for on-target tests)
ls /dev/ttyUSB*
```

### Run All Tests on ESP32

```bash
cd esp32-firmware
./run_tests.sh target
```

This will:
1. Build the test application
2. Flash it to ESP32
3. Run all tests
4. Display results

### Run Specific Component Tests

```bash
# Test only error handler
cd test_app
idf.py -DTEST_COMPONENTS="star_error_handler" build flash monitor

# Test only pin validator
idf.py -DTEST_COMPONENTS="star_pin_validator" build flash monitor
```

---

## Running Tests

### Option 1: Automated Test Runner (Recommended)

```bash
# Run on ESP32 hardware
./run_tests.sh target

# Run on host (coming soon)
./run_tests.sh host

# Run with coverage (coming soon)
./run_tests.sh coverage

# Run all test modes
./run_tests.sh all
```

### Option 2: Manual Test Execution

```bash
cd test_app

# Build test application
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB1 flash

# Monitor test output
idf.py -p /dev/ttyUSB1 monitor
```

### Option 3: Component-Specific Tests

```bash
cd test_app

# Test specific components
idf.py -DTEST_COMPONENTS="star_error_handler star_pin_validator" build

# Flash and run
idf.py -p /dev/ttyUSB1 flash monitor
```

---

## Writing Tests

### 1. Create Test File

Tests live in `components/<component>/test/` directory:

```
components/
+-- my_component/
    +-- include/
    +-- my_component.c
    +-- CMakeLists.txt
    +-- test/                        # <- Tests here
        +-- test_my_component.c
        +-- CMakeLists.txt
```

### 2. Write Test File

```c
/* components/my_component/test/test_my_component.c */

#include "unity.h"
#include "my_component.h"

/* Setup function (called before each test) */
void setUp(void)
{
  /* Initialize test state */
}

/* Teardown function (called after each test) */
void tearDown(void)
{
  /* Cleanup test state */
}

/* Test case */
TEST_CASE("my_component does something", "[my_component]")
{
  int result = my_function(42);

  TEST_ASSERT_EQUAL(42, result);
  TEST_ASSERT_NOT_NULL(some_pointer);
  TEST_ASSERT_TRUE(some_condition);
}

/* Another test case */
TEST_CASE("my_component handles errors", "[my_component][error]")
{
  esp_err_t ret = my_function_that_fails(NULL);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}
```

### 3. Create Test CMakeLists.txt

```cmake
# components/my_component/test/CMakeLists.txt

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES unity my_component)
```

### 4. Add Component to Test Build

Edit `test_app/CMakeLists.txt`:

```cmake
set(TEST_COMPONENTS "star_error_handler star_pin_validator my_component")
```

### 5. Build and Run

```bash
cd test_app
idf.py build flash monitor
```

---

## Test Assertions

Unity provides many assertion macros:

### Basic Assertions
```c
TEST_ASSERT(condition)
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_NOT_NULL(pointer)
```

### Equality Assertions
```c
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_UINT8(expected, actual)
TEST_ASSERT_EQUAL_HEX8(expected, actual)
TEST_ASSERT_EQUAL_STRING(expected, actual)
```

### Comparison Assertions
```c
TEST_ASSERT_LESS_THAN(threshold, actual)
TEST_ASSERT_GREATER_THAN(threshold, actual)
TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)
TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)
```

### Array/Memory Assertions
```c
TEST_ASSERT_EQUAL_MEMORY(expected, actual, length)
TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)
```

### Floating Point Assertions
```c
TEST_ASSERT_EQUAL_FLOAT(expected, actual)
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)
```

---

## Test Coverage

### Generate Coverage Report (Coming Soon)

```bash
# Run tests with coverage instrumentation
./run_tests.sh coverage

# View coverage report
open coverage_html/index.html
```

### Coverage Goals

- **Unit Tests**: >80% line coverage per component
- **Integration Tests**: >70% feature coverage
- **Critical Paths**: 100% coverage (error handling, security)

---

## Continuous Integration

### GitHub Actions (Coming Soon)

```yaml
# .github/workflows/tests.yml
name: Tests

on: [push, pull_request]

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
      - name: Run Host Tests
        run: ./run_tests.sh host

  hardware-tests:
    runs-on: self-hosted  # Requires ESP32 hardware
    steps:
      - uses: actions/checkout@v3
      - name: Run Target Tests
        run: ./run_tests.sh target
```

---

## Current Test Status

### [x] Completed Components

- **star_error_handler**: 15 unit tests
  - Initialization
  - Error recording
  - Exponential backoff
  - Retry logic
  - Reset functions
  - Thread safety

- **star_pin_validator**: 16 unit tests
  - Pin registration
  - Conflict detection
  - Shareable pins
  - Validation logic

- **star_wifi_bridge/protocol**: 20+ unit tests
  - Packet structure
  - Command codes
  - Payload encoding
  - Edge cases

### [IN PROGRESS] In Progress

- WiFi manager tests (with mocks)
- UART transport tests
- Command handler tests

### [PLANNED] Planned

- Host-based test execution
- Code coverage integration
- CMock setup for WiFi/UART
- Performance benchmarks
- Stress tests

---

## Troubleshooting

### Tests Won't Build

```bash
# Clean and rebuild
cd test_app
idf.py fullclean
idf.py build
```

### Tests Fail to Flash

```bash
# Check device connection
ls -la /dev/ttyUSB*

# Try different port
idf.py -p /dev/ttyUSB0 flash

# Check permissions
sudo usermod -a -G dialout $USER
# Log out and log back in
```

### Tests Timeout

```bash
# Increase timeout in sdkconfig
idf.py menuconfig
# -> Component config -> Unity test framework -> Timeout

# Or edit test_app/sdkconfig.defaults:
CONFIG_UNITY_TIMEOUT_MS=30000
```

### Memory Issues

```bash
# Increase main task stack
# Edit test_app/sdkconfig.defaults:
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384

# Enable heap tracing
CONFIG_HEAP_TRACING_STANDALONE=y
```

### Can't See Test Output

```bash
# Make sure monitor baud rate matches
idf.py -p /dev/ttyUSB1 -b 115200 monitor

# Reset device manually if needed
```

---

## Best Practices

1. **Write Tests First** (TDD)
   - Write test before implementing feature
   - Ensures code is testable
   - Documents expected behavior

2. **One Test, One Assertion** (mostly)
   - Each test should verify one thing
   - Easier to debug failures
   - Exception: Setup validation

3. **Use Descriptive Names**
   ```c
   // Good
   TEST_CASE("error_handler applies exponential backoff", "[error_handler]")

   // Bad
   TEST_CASE("test1", "[error_handler]")
   ```

4. **Test Edge Cases**
   - NULL pointers
   - Zero/negative values
   - Maximum values
   - Invalid inputs

5. **Keep Tests Independent**
   - Tests should not depend on each other
   - Use setUp/tearDown for cleanup
   - No global state between tests

6. **Mock External Dependencies**
   - Don't test WiFi stack (trust ESP-IDF)
   - Mock hardware peripherals
   - Focus on your logic

---

## Additional Resources

- [Unity Testing Framework](https://github.com/ThrowTheSwitch/Unity)
- [CMock Documentation](https://github.com/ThrowTheSwitch/CMock)
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
- [Test-Driven Development](https://en.wikipedia.org/wiki/Test-driven_development)

---

## Contributing

When adding new components, **always include tests**:

1. Create `components/<component>/test/` directory
2. Write unit tests for all functions
3. Add integration tests for interactions
4. Update test_app/CMakeLists.txt
5. Run `./run_tests.sh target` to verify
6. Ensure tests pass before committing

**Code without tests will not be merged.**

---

**Questions?** Open an issue or contact the maintainer.
