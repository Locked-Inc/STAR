# RX72N Firmware Unit Tests

This directory contains comprehensive unit tests for the RX72N firmware, covering all library modules and application tasks.

## Quick Start

```bash
cd tests
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure
```

## Test Infrastructure

- **35 independent test executables** - Each test suite runs separately
- **Unity v2.6.1 test framework** - Lightweight C unit testing framework (auto-fetched by CMake)
- **37 mock implementations** - Hardware abstraction for host testing (in `mocks/` directory)
- **CMake build system** - Cross-platform, parallel compilation support
- **Host target compilation** - Tests run on x86_64, not embedded RX72N

## Test Coverage

### Core Libraries
- `test_rx_crc8`, `test_rx_crc32` - CRC calculation (hardware and software)
- `test_rx_frame`, `test_rx_frame_ascii` - Protocol framing
- `test_rx_fec`, `test_rx_harq` - Forward error correction and hybrid ARQ
- `test_rx_pid` - PID control algorithm
- `test_rx_error_handler` - Error handling infrastructure
- `test_rx_pin_validator`, `test_rx_register_guard` - Safety mechanisms

### Hardware Abstraction
- `test_gpio_hal`, `test_uart_hal` - GPIO and UART drivers
- `test_rx_gptw_staggered`, `test_rx_mpc` - Timer and pin controller
- `test_rx_usb`, `test_rx_usb_multiport`, `test_rx_usb_comm` - USB CDC interface

### Communication
- `test_rx_comm_manager` - Multi-interface communication manager
- `test_rx_spi_comm` - SPI communication protocol
- `test_rx_bus_manager`, `test_rx_bus_onewire` - Bus abstraction layer
- `test_rx_nanopb` - Protocol Buffers integration

### Peripherals & Sensors
- `test_rx_hcsr04`, `test_rx_obstacle_detect` - Ultrasonic sensor and obstacle detection
- `test_rx_ds18b20` - Temperature sensor (1-Wire)
- `test_rx_encoder` - Motor quadrature encoder

### Motor Control
- `test_rx_motor` - Motor control abstraction

### Application Tasks
- `test_motor_control_task` - Motor control task (ThreadX)
- `test_communication_task` - Communication task
- `test_obstacle_detection_task` - Obstacle detection task
- `test_temperature_sensing_task` - Temperature sensing task
- `test_telemetry_aggregation_task` - Telemetry collection and reporting

## Running Individual Tests

```bash
cd build

# Run specific test
./test_rx_pid

# Run with verbose output
./test_rx_motor

# Run subset of tests matching pattern
ctest -R motor
```

## Build Options

### Clean build
```bash
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Debug vs Release
```bash
# Debug (default - includes debug symbols)
cmake ..

# Release (optimized, no debug symbols)
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Verbose compilation
```bash
make VERBOSE=1
```

## CI/CD Integration

Add to your CI pipeline (GitHub Actions, Jenkins, etc.):

```yaml
- name: Run RX72N unit tests
  run: |
    cd e2-studio-star-rx72n-firmware/tests
    mkdir build && cd build
    cmake ..
    make -j$(nproc)
    ctest --output-on-failure
```

## Test Structure

Each test file follows this pattern:

```c
#include "unity.h"
#include "module_under_test.h"

void setUp(void) {
    // Reset state before each test
}

void tearDown(void) {
    // Cleanup after each test
}

void test_module_function(void) {
    // Arrange
    int input = 5;

    // Act
    int result = function_under_test(input);

    // Assert
    TEST_ASSERT_EQUAL(expected, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_module_function);
    return UNITY_END();
}
```

## Mock System

Mocks provide host-compatible implementations of hardware-dependent code:

- **Override mechanism**: Mocks directory included FIRST in CMake, shadowing real hardware headers
- **Compile defines**: `UNIT_TEST`, `USE_MOCK_SCI_REGS`, `RX_CRC32_USE_SOFTWARE`, `MOCK_TX_THREAD_CREATE`
- **Sophisticated mocks**: UART with FIFO buffers, USB with packet tracking, motor with state machines

## Troubleshooting

### Build fails with "file not found"
- Check that `libs/` directory exists (not `lib/`)
- Verify all library paths in CMakeLists.txt use `../libs/` prefix

### Tests crash or segfault
- Ensure mocks are included FIRST (check include path order in CMakeLists.txt)
- Verify `UNIT_TEST` define is set
- Check that setUp/tearDown properly initialize/cleanup state

### Unity framework not found
- CMake should auto-fetch Unity from GitHub
- If offline, manually clone Unity to `tests/Unity/`
- Check internet connectivity and firewall settings

### Tests fail unexpectedly
- Verify firmware library code hasn't changed (tests and firmware must stay in sync)
- Check for uninitialized variables in tests
- Run individual test with verbose output for debugging

## Dependencies

- **CMake** 3.20+ (build system)
- **GCC** or **Clang** (host C compiler, not GNURX)
- **Git** (Unity framework fetch)
- **Python 3** (nanopb code generation)

## Development Workflow

1. **Edit firmware code** in e2-studio project (`libs/`, `src/`)
2. **Run tests** from terminal to verify changes:
   ```bash
   cd tests/build
   make -j$(nproc) && ctest
   ```
3. **Fix failures** by debugging individual tests
4. **Commit** when all tests pass

## Test Coverage Goals

Current coverage: ~85% of firmware code

Priority areas for additional tests:
- ThreadX RTOS integration (currently mocked)
- Smart Configurator generated code
- Interrupt handlers (hardware-specific)
- Flash programming operations

## Resources

- **Unity framework docs**: https://github.com/ThrowTheSwitch/Unity
- **CMake documentation**: https://cmake.org/documentation/
- **STAR firmware guide**: ../CLAUDE.md
- **RX72N verification**: ../RX72N_VERIFICATION_SUMMARY.md
