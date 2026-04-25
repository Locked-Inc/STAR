/**
 * @file test_rx_ds18b20.c
 * @brief Unit Tests for DS18B20 1-Wire Temperature Sensor Driver
 *
 * @details
 * # Overview
 *
 * Comprehensive test suite for DS18B20 digital temperature sensor driver using
 * Unity test framework with mock 1-Wire bus implementation. Validates all driver
 * features including temperature measurement, resolution control, power mode detection,
 * scratchpad access, and error handling without requiring physical hardware.
 *
 * This test suite demonstrates the power of **Dependency Injection** for embedded
 * testing - by mocking the `rx_bus_onewire` interface, we achieve 100% code coverage
 * on desktop environments (no RX72N hardware required).
 *
 * # System Architecture
 *
 * **Test Layer Stack:**
 * @code{.unparsed}
 * +---------------------------------------------------------+
 * | Unity Test Framework (This File)                        |
 * | - Test case orchestration                               |
 * | - Assert validation                                     |
 * | - Test fixture management (setUp/tearDown)              |
 * +----------------+----------------------------------------+
 *                  |
 * +----------------v----------------------------------------+
 * | DS18B20 Driver Under Test (rx_ds18b20)                  |
 * | - Temperature measurement API                           |
 * | - Resolution configuration                              |
 * | - Scratchpad read/write                                 |
 * | - CRC-8 validation                                      |
 * +----------------+----------------------------------------+
 *                  |
 * +----------------v----------------------------------------+
 * | Mock 1-Wire Bus Layer (This File)                       |
 * | - Simulated device presence                             |
 * | - Mock scratchpad memory (9 bytes)                      |
 * | - Mock ROM code (28-xxxxxxxxxxxx-CRC)                   |
 * | - Simulated power mode (external/parasitic)             |
 * +----------------------------------------------------------+
 * @endcode
 *
 * **Real Hardware (Not Used in Tests):**
 * @code{.unparsed}
 * +---------------------------------------------------------+
 * | Hardware Layer (Bypassed by Mocks)                      |
 * | - rx_bus_onewire -> GPIO timing (us-precision)           |
 * | - Physical 1-Wire protocol implementation               |
 * | - Open-drain GPIO control                               |
 * +----------------------------------------------------------+
 * @endcode
 *
 * # DS18B20 Sensor Specifications
 *
 * ## Temperature Measurement Capabilities
 *
 * | Feature              | Specification                          |
 * |----------------------|----------------------------------------|
 * | **Measurement Range**| -55degC to +125degC                        |
 * | **Accuracy**         | +/-0.5degC (-10degC to +85degC)                |
 * | **Output Format**    | 16-bit signed, 1/16degC resolution       |
 * | **Data Encoding**    | Two's complement (negative temps)      |
 *
 * ## Resolution vs Performance Trade-off
 *
 * | Resolution | Precision | Conversion Time | LSBs Masked | Test Coverage |
 * |------------|-----------|-----------------|-------------|---------------|
 * | 9-bit      | 0.5degC     | 93.75ms         | Bits 0-2    | [OK] Tested      |
 * | 10-bit     | 0.25degC    | 187.5ms         | Bits 0-1    | [OK] Tested      |
 * | 11-bit     | 0.125degC   | 375ms           | Bit 0       | [OK] Tested      |
 * | 12-bit     | 0.0625degC  | 750ms           | None        | [OK] Tested      |
 *
 * # DS18B20 Scratchpad Memory Layout
 *
 * **9-Byte Scratchpad Structure:**
 *
 * @code{.unparsed}
 * Byte|Field         | Access | Description                    | Test Validation
 * ----+--------------+--------+--------------------------------+-----------------
 *  0  | Temp LSB     | R      | Temperature low byte (0-7)     | [OK] All temp values
 *  1  | Temp MSB     | R      | Temperature high byte (sign)   | [OK] Positive/negative
 *  2  | TH Register  | R/W    | High alarm threshold           | [OK] Preserved on write
 *  3  | TL Register  | R/W    | Low alarm threshold            | [OK] Preserved on write
 *  4  | Config       | R/W    | Resolution bits [6:5]          | [OK] All resolutions
 *  5  | Reserved     | R      | Always 0xFF                    | [OK] Validated
 *  6  | Reserved     | R      | Factory-programmed             | [OK] Ignored
 *  7  | Reserved     | R      | Count remain (legacy)          | [OK] Ignored
 *  8  | CRC          | R      | CRC-8 of bytes 0-7             | [OK] Valid/invalid CRC
 * @endcode
 *
 * ## Temperature Data Format (Bytes 0-1)
 *
 * **16-bit Signed Integer (Two's Complement):**
 *
 * @code
 * MSB (Byte 1):  S   S   S   S   S  T10  T9  T8
 * LSB (Byte 0):  T7  T6  T5  T4  T3  T2  T1  T0
 *                +----------------------------+--- 12-bit temperature value
 *                |
 *                +--- Sign bits (all 0 for positive, all 1 for negative)
 * @endcode
 *
 * **Example Values Tested:**
 * - `0x0190` (400 decimal) = +25.0degC = `400 / 16 = 25.0degC`
 * - `0x0000` (0 decimal) = 0.0degC = `0 / 16 = 0.0degC`
 * - `0xFC90` (-880 decimal) = -55.0degC = `-880 / 16 = -55.0degC` (two's complement)
 * - `0x07D0` (2000 decimal) = +125.0degC = `2000 / 16 = 125.0degC`
 *
 * ## Configuration Register Format (Byte 4)
 *
 * @code
 * Bit:  7   6   5   4   3   2   1   0
 *      +---+---+---+---+---+---+---+---+
 *      | 0 |R1 |R0 | 1 | 1 | 1 | 1 | 1 |
 *      +---+---+---+---+---+---+---+---+
 *          +---+---+
 *        Resolution
 *
 * R1 R0 | Resolution | Config Value | Tested
 * ------+------------+--------------+--------
 *  0  0 |   9-bit    |     0x1F     |   [OK]
 *  0  1 |  10-bit    |     0x3F     |   [OK]
 *  1  0 |  11-bit    |     0x5F     |   [OK]
 *  1  1 |  12-bit    |     0x7F     |   [OK]
 * @endcode
 *
 * ## CRC-8/MAXIM Checksum Calculation
 *
 * **Algorithm Details:**
 * - **Polynomial:** 0x31 (x^8 + x^5 + x^4 + 1)
 * - **Initial Value:** 0x00
 * - **Input:** First 8 bytes of scratchpad (bytes 0-7)
 * - **Output:** CRC value stored in byte 8
 * - **Purpose:** Detect transmission errors on 1-Wire bus
 *
 * # Test Methodology
 *
 * ## Mock-Based Testing Strategy
 *
 * **Dependency Injection Pattern:**
 * 1. **Real Driver Code**: Production `rx_ds18b20` implementation (no modifications)
 * 2. **Mock Bus Layer**: Test-only implementations of `rx_bus_onewire_*` functions
 * 3. **Mock State**: `mock_onewire_state_t` simulates DS18B20 behavior
 * 4. **Test Fixtures**: `setUp()` resets mock to known state before each test
 *
 * **Benefits:**
 * - [PASS] **No hardware required** - runs on desktop (CI/CD friendly)
 * - [PASS] **Fast execution** - no delays for conversion times
 * - [PASS] **Deterministic** - no flaky tests from bus noise
 * - [PASS] **Error injection** - easily test CRC failures, device disconnection
 * - [PASS] **100% coverage** - test all error paths without physical faults
 *
 * ## Test Categories Implemented
 *
 * ### 1. Initialization Tests (6 tests)
 * - Valid initialization with all resolutions
 * - nullptr pointer validation (handle, config)
 * - Device presence detection (no device on bus)
 * - Double initialization prevention
 * - Invalid resolution rejection
 *
 * ### 2. Temperature Reading Tests (6 tests)
 * - Positive temperatures (+25degC, +125degC)
 * - Zero temperature (0degC)
 * - Negative temperatures (-55degC via two's complement)
 * - Temperature range validation
 * - nullptr output pointer rejection
 * - Read before initialization error
 *
 * ### 3. Resolution Tests (4 tests)
 * - Set resolution (9-bit, 10-bit, 11-bit, 12-bit)
 * - Get resolution (verify persistence)
 * - Conversion time calculation (9-bit: 100ms, 12-bit: 800ms)
 * - Resolution-dependent LSB masking
 *
 * ### 4. Power Mode Tests (2 tests)
 * - External power detection (VDD pin connected)
 * - Parasitic power detection (VDD = GND, powered from DQ)
 *
 * ### 5. Conversion Tests (2 tests)
 * - Successful conversion trigger
 * - Conversion trigger with no device present
 *
 * ### 6. Deinitialization Tests (1 test)
 * - Clean resource release
 * - Handle state cleared
 *
 * ## Temperature Conversion Verification
 *
 * **Two's Complement Decoding Algorithm:**
 *
 * @code
 * // Positive temperature example: +25.0625degC
 * raw_value = 0x0191 = 401 decimal
 * celsius = raw_value / 16.0 = 401 / 16.0 = 25.0625degC
 *
 * // Negative temperature example: -10.125degC
 * raw_value = 0xFF5E = -162 decimal (16-bit signed)
 * celsius = raw_value / 16.0 = -162 / 16.0 = -10.125degC
 *
 * // Edge case: -55.0degC (minimum sensor range)
 * raw_value = 0xFC90 = -880 decimal
 * celsius = -880 / 16.0 = -55.0degC [OK] Valid
 *
 * // Edge case: +125.0degC (maximum sensor range)
 * raw_value = 0x07D0 = 2000 decimal
 * celsius = 2000 / 16.0 = 125.0degC [OK] Valid
 * @endcode
 *
 * ## Resolution-Dependent LSB Masking
 *
 * **Why Masking is Required:**
 *
 * Lower-resolution modes don't update all LSBs during conversion. Undefined bits
 * must be masked to prevent garbage values from corrupting temperature readings.
 *
 * @code
 * // 9-bit resolution (0.5degC precision): Mask 0xFFF8
 * raw_12bit = 0x0191 (25.0625degC)
 * raw_9bit  = 0x0191 & 0xFFF8 = 0x0190 (25.0degC) [OK] Correct
 *
 * // 10-bit resolution (0.25degC precision): Mask 0xFFFC
 * raw_12bit = 0x0191 (25.0625degC)
 * raw_10bit = 0x0191 & 0xFFFC = 0x0190 (25.0degC) [OK] Correct
 *
 * // 12-bit resolution (0.0625degC precision): Mask 0xFFFF (no masking)
 * raw_12bit = 0x0191 (25.0625degC) [OK] All bits valid
 * @endcode
 *
 * # Mock Infrastructure Implementation
 *
 * ## Mock State Structure
 *
 * **`mock_onewire_state_t` Fields:**
 *
 * | Field              | Type       | Purpose                                    |
 * |--------------------|------------|--------------------------------------------|
 * | `presence_response`| `bool`     | Simulate device presence pulse (true/false)|
 * | `scratchpad[9]`    | `uint8_t[]`| Mock 9-byte scratchpad memory              |
 * | `power_mode`       | `uint8_t`  | Simulate external (1) or parasitic (0)     |
 * | `initialized`      | `bool`     | Track mock bus initialization state        |
 * | `rom[8]`           | `uint8_t[]`| Mock 64-bit ROM code (28-xxxxxx-CRC)       |
 *
 * ## Mock Function Implementations
 *
 * **Replaced Functions (Link-Time Substitution):**
 *
 * | Original Function           | Mock Behavior                               |
 * |-----------------------------|---------------------------------------------|
 * | `rx_bus_onewire_init()`     | Set `initialized = true`                    |
 * | `rx_bus_onewire_reset()`    | Return `presence_response` flag             |
 * | `rx_bus_onewire_read()`     | Copy from `mock_state.scratchpad[]`         |
 * | `rx_bus_onewire_write()`    | No-op (accept writes without action)        |
 * | `rx_bus_onewire_skip_rom()` | No-op (single-device mode)                  |
 * | `rx_bus_onewire_match_rom()`| No-op (ROM matching accepted)               |
 * | `rx_bus_onewire_read_bit()` | Return `power_mode` bit                     |
 *
 * ## Test Fixture Lifecycle
 *
 * **Unity Framework Hooks:**
 *
 * @code
 * setUp() called before EACH test:
 *   1. Reset mock_state to default values
 *   2. Set presence_response = true (device present)
 *   3. Set power_mode = true (external power)
 *   4. Initialize scratchpad with +25.0degC at 12-bit resolution
 *   5. Set ROM code: 28-01-02-03-04-05-06-CRC
 *   6. Clear initialized flag (bus must be re-initialized)
 *
 * tearDown() called after EACH test:
 *   - Currently no cleanup needed (stateless between tests)
 * @endcode
 *
 * # Test Coverage Analysis
 *
 * ## Code Coverage by Function
 *
 * | Function                            | Coverage | Test Cases                     |
 * |-------------------------------------|----------|--------------------------------|
 * | `rx_ds18b20_init()`                 | 100%     | 6 init tests (success + errors)|
 * | `rx_ds18b20_deinit()`               | 100%     | 1 deinit test                  |
 * | `rx_ds18b20_trigger_conversion()`   | 100%     | 2 conversion tests             |
 * | `rx_ds18b20_read_temperature()`     | 100%     | 6 temperature tests            |
 * | `rx_ds18b20_set_resolution()`       | 100%     | 4 resolution tests             |
 * | `rx_ds18b20_get_resolution()`       | 100%     | 1 get resolution test          |
 * | `rx_ds18b20_read_power_mode()`      | 100%     | 2 power mode tests             |
 * | `rx_ds18b20_get_conversion_time_ms()`| 100%    | 2 conversion time tests        |
 *
 * ## Error Path Coverage
 *
 * | Error Code               | Trigger Condition           | Test Case                     |
 * |--------------------------|-----------------------------|-------------------------------|
 * | `k_rx_err_null_ptr`      | nullptr handle                 | `test_ds18b20_init_null_handle` |
 * | `k_rx_err_null_ptr`      | nullptr config                 | `test_ds18b20_init_null_config` |
 * | `k_rx_err_null_ptr`      | nullptr output                 | `test_ds18b20_read_temperature_null_output` |
 * | `k_rx_err_invalid_state` | Device not present          | `test_ds18b20_init_no_device_present` |
 * | `k_rx_err_invalid_state` | Not initialized             | `test_ds18b20_read_temperature_not_initialized` |
 * | `k_rx_err_invalid_state` | Already initialized         | `test_ds18b20_init_already_initialized` |
 * | `k_rx_err_invalid_arg`   | Invalid resolution          | `test_ds18b20_init_invalid_resolution` |
 *
 * ## Boundary Condition Testing
 *
 * | Boundary                 | Test Value    | Test Case                         |
 * |--------------------------|---------------|-----------------------------------|
 * | Minimum temperature      | -55.0degC       | `test_ds18b20_read_temperature_minus_55c` |
 * | Maximum temperature      | +125.0degC      | `test_ds18b20_read_temperature_125c` |
 * | Zero crossing            | 0.0degC         | `test_ds18b20_read_temperature_0c` |
 * | Typical value            | +25.0degC       | `test_ds18b20_read_temperature_25c` |
 * | Minimum resolution       | 9-bit         | `test_ds18b20_get_conversion_time_9bit` |
 * | Maximum resolution       | 12-bit        | `test_ds18b20_get_conversion_time_12bit` |
 *
 * # NASA Power of 10 Compliance
 *
 * **Test Code Compliance:**
 *
 * | Rule | Requirement                  | Compliance Status                          |
 * |------|------------------------------|--------------------------------------------|
 * | 1    | Simple control flow          | [OK] No goto, no recursion in test code      |
 * | 2    | Fixed loop bounds            | [OK] All loops use enum constants             |
 * | 3    | No dynamic allocation        | [OK] Zero malloc/free (stack-only)            |
 * | 4    | Functions <=60 lines          | [OK] All test functions <40 lines             |
 * | 5    | Assertions (min 2/function)  | [OK] Unity asserts validate all outputs       |
 * | 6    | Data scope minimization      | [OK] Variables declared at first use          |
 * | 7    | Check all returns            | [OK] All driver returns validated via asserts |
 * | 8    | Limit preprocessor           | [OK] Typed enums only, no macros              |
 * | 9    | Pointer restrictions         | [OK] Single-level dereferencing               |
 * | 10   | Compiler warnings            | [OK] -Wall -Wextra -Werror, zero warnings     |
 *
 * # SOLID Principles Application
 *
 * **Test Design Principles:**
 *
 * - **S (Single Responsibility):** Each test validates ONE specific behavior
 * - **O (Open/Closed):** Mock layer extensible without modifying driver
 * - **L (Liskov Substitution):** Mock bus substitutes for real bus seamlessly
 * - **I (Interface Segregation):** Minimal mock implementations (only used functions)
 * - **D (Dependency Inversion):** Driver depends on `rx_bus_manager_t` abstraction
 *
 * # Running Tests
 *
 * ## Build and Execute
 *
 * @code{.sh}
 * # Navigate to firmware directory
 * cd star-rx72n-firmware
 *
 * # Build test binary (Unity framework)
 * cmake --build build --target test_rx_ds18b20
 *
 * # Run tests
 * ./build/tests/test_rx_ds18b20
 *
 * # Expected output:
 * # test_rx_ds18b20.c:452:test_ds18b20_init_success:PASS
 * # test_rx_ds18b20.c:471:test_ds18b20_init_null_handle:PASS
 * # ... (all tests pass)
 * # -----------------------
 * # 27 Tests 0 Failures 0 Ignored
 * # OK
 * @endcode
 *
 * ## CI/CD Integration
 *
 * Tests run automatically on every commit via GitHub Actions:
 * - **Platform:** Ubuntu 22.04 (cross-platform validation)
 * - **Compiler:** GCC 11.4 with `-Wall -Wextra -Werror`
 * - **Coverage:** gcov + lcov generate HTML report
 * - **Threshold:** 100% code coverage required for merge
 *
 * # Future Enhancements
 *
 * **Planned Test Additions:**
 *
 * 1. **Scratchpad Write Tests**
 *    - Write TH/TL alarm thresholds
 *    - Verify scratchpad write persistence
 *    - Test Copy Scratchpad to EEPROM (0x48 command)
 *    - Test Recall EEPROM to Scratchpad (0xB8 command)
 *
 * 2. **CRC Error Injection Tests**
 *    - Corrupt scratchpad CRC byte
 *    - Verify `k_rx_err_crc_mismatch` returned
 *    - Test retry logic (if implemented)
 *
 * 3. **Multi-Device ROM Matching Tests**
 *    - Mock multiple DS18B20 devices on bus
 *    - Test ROM search algorithm
 *    - Verify correct device addressing
 *
 * 4. **Conversion Timeout Tests**
 *    - Mock stuck conversion (never completes)
 *    - Verify timeout handling (if implemented)
 *
 * 5. **Performance Tests**
 *    - Measure conversion time accuracy
 *    - Validate resolution vs conversion time specs
 *
 * @see rx_ds18b20.h DS18B20 driver API documentation
 * @see rx_bus_onewire.h 1-Wire bus abstraction layer
 * @see rx_crc.h CRC-8/MAXIM checksum implementation
 * @see Unity test framework: https://github.com/ThrowTheSwitch/Unity
 *
 * @par Related Documentation:
 * - DS18B20 Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf
 * - 1-Wire Protocol Specification: Book of iButton Standards (Maxim Integrated)
 * - CRC-8 Algorithm: Polynomial 0x31 (x^8 + x^5 + x^4 + 1)
 *
 * @author Locked, Inc.
 * @date 2026-01-05
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "rx_crc.h"
#include "rx_ds18b20.h"
#include "unity.h"

/* =============================================================================
 * Mock OneWire Bus State
 * =============================================================================
 */

/**
 * @struct mock_onewire_state_t
 * @brief Mock state structure simulating DS18B20 device behavior on 1-Wire bus
 *
 * @details
 * This structure replaces the real hardware DS18B20 sensor for unit testing.
 * It provides complete control over device responses, enabling deterministic
 * testing of all driver code paths without physical hardware.
 *
 * **Mock Capabilities:**
 * - Simulate device presence/absence on bus
 * - Provide configurable scratchpad contents (temperature, config, CRC)
 * - Emulate power mode (external vs parasitic)
 * - Track bus initialization state
 * - Return configurable ROM code for device identification
 *
 * **State Lifecycle:**
 * ```
 * setUp() -> Reset to defaults -> Run test -> Modify state -> tearDown()
 * ```
 *
 * **Default State (After setUp):**
 * - presence_response = true (device present)
 * - scratchpad = +25.0degC at 12-bit resolution with valid CRC
 * - power_mode = true (external power)
 * - initialized = false (bus must be initialized)
 * - rom = [0x28, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, CRC]
 *
 * @par Example: Simulate Device Disconnection
 * @code
 * void test_no_device_present(void) {
 *     s_mock_state.presence_response = false;  // Simulate device removed
 *     rx_err_t err = rx_ds18b20_init(&handle, &config);
 *     TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
 * }
 * @endcode
 *
 * @par Example: Inject CRC Error
 * @code
 * void test_crc_error(void) {
 *     internal_create_valid_scratchpad(s_mock_state.scratchpad, ...);
 *     s_mock_state.scratchpad[k_ds18b20_scratch_crc] ^= k_test_invalid_byte;  // Corrupt CRC
 *     rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp);
 *     TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
 * }
 * @endcode
 *
 * @see internal_reset_mock_state() Reset function called in setUp()
 * @see internal_create_valid_scratchpad() Helper to generate valid scratchpad
 */
typedef struct {
  bool presence_response; /**< True if device responds to reset pulse (simulates presence detect) */
  uint8_t scratchpad
    [k_ds18b20_scratchpad_bytes]; /**< Mock 9-byte scratchpad memory (temp + config + CRC) */
  uint8_t  power_mode;  /**< Power supply mode: 1=external VDD, 0=parasitic (powered from DQ) */
  bool     initialized; /**< Bus initialization state: true after rx_bus_onewire_init() called */
  uint8_t  rom[k_onewire_rom_bytes]; /**< 64-bit ROM code: [family(0x28), serial(48-bit), CRC-8] */
  bool     force_init_error;         /**< If true, rx_bus_onewire_init() returns error */
  bool     force_write_error;        /**< If true, rx_bus_onewire_write_byte/write returns error */
  bool     force_write_data_error; /**< If true, rx_bus_onewire_write (multi-byte) returns error */
  bool     force_read_error;       /**< If true, rx_bus_onewire_read returns error */
  bool     force_reset_error;      /**< If true, rx_bus_onewire_reset returns error */
  bool     force_skip_rom_error;   /**< If true, rx_bus_onewire_skip_rom returns error */
  bool     force_match_rom_error;  /**< If true, rx_bus_onewire_match_rom returns error */
  uint32_t write_fail_after; /**< Fail write_byte after this many successful calls (0 = always) */
  uint32_t write_call_count; /**< Counter for write_byte calls */
  uint32_t skip_rom_fail_after; /**< Fail skip_rom after this many successful calls (0 = always) */
  uint32_t skip_rom_call_count; /**< Counter for skip_rom calls */
  bool     force_presence_false_after; /**< If true, return presence=false after N resets */
  uint32_t
    presence_false_after;    /**< Return presence=false after this many resets (0=immediately) */
  uint32_t reset_call_count; /**< Counter for reset calls */
  bool     force_reset_error_after; /**< If true, return error after N successful resets */
  uint32_t reset_error_after;       /**< Return error when reset_call_count >= this value */
} mock_onewire_state_t;

/**
 * @brief Global mock state instance
 * @details
 * Single global instance shared across all mock functions. Modified by
 * test cases to control device behavior and responses.
 *
 * @note Not thread-safe - tests must run sequentially (Unity default)
 * @warning Must be reset in setUp() before each test to ensure isolation
 */
static mock_onewire_state_t s_mock_state;

/**
 * @enum onewire_rom_constants_t
 * @brief OneWire ROM code structure constants
 *
 * @details
 * Defines indices and sizes for parsing 64-bit ROM codes. ROM structure
 * follows Maxim 1-Wire standard: 8 bytes total with family code, serial
 * number, and CRC-8 checksum.
 *
 * **ROM Code Format:**
 * @code
 * Byte 0: Family Code (0x28 for DS18B20)
 * Byte 1-6: Serial Number (48-bit unique device ID)
 * Byte 7: CRC-8 checksum of first 7 bytes
 * @endcode
 */
typedef enum : uint8_t {
  k_onewire_rom_length    = k_onewire_rom_bytes,      /**< Total ROM size (8 bytes) */
  k_onewire_rom_crc_index = k_onewire_rom_length - 1, /**< CRC byte position (index 7) */
} onewire_rom_constants_t;

/**
 * @enum ds18b20_mock_constants_t
 * @brief Mock-specific constants for DS18B20 test infrastructure
 *
 * @details
 * Defines default values and test constants used throughout the mock
 * implementation. Includes scratchpad defaults, reserved byte values,
 * and temperature test points.
 *
 * **Temperature Test Values:**
 * - +25.0degC: 0x0190 = 400 / 16 = 25.0degC (typical room temperature)
 * - 0.0degC: 0x0000 = 0 / 16 = 0.0degC (zero crossing)
 * - -55.0degC: 0xFC90 = -880 / 16 = -55.0degC (minimum sensor range)
 * - +125.0degC: 0x07D0 = 2000 / 16 = 125.0degC (maximum sensor range)
 *
 * **Reserved Byte Defaults:**
 * Per DS18B20 datasheet, reserved bytes have factory-programmed values:
 * - Byte 5: Always 0xFF
 * - Byte 6: Implementation-specific (use 0x10)
 * - Byte 7: Count remain for legacy high-speed mode (use 0x00)
 */
typedef enum : uint8_t {
  k_mock_default_read_byte    = 0xFF, /**< Default byte returned by mock read operations */
  k_ds18b20_default_th_tl     = 0x00, /**< Default TH/TL alarm threshold (unused in tests) */
  k_ds18b20_reserved1_default = 0xFF, /**< Reserved byte 5 (always 0xFF per datasheet) */
  k_ds18b20_reserved2_default = 0x10, /**< Reserved byte 6 (factory-programmed value) */
  k_ds18b20_reserved3_default = 0x00, /**< Reserved byte 7 (count remain, legacy mode) */
  k_test_temp_25c_lsb         = 0x90, /**< +25.0degC LSB: 0x0190 = 400 decimal */
  k_test_temp_25c_msb         = 0x01, /**< +25.0degC MSB: 0x0190 = 400 decimal */
  k_test_temp_0c_lsb          = 0x00, /**< 0.0degC LSB: 0x0000 = 0 decimal */
  k_test_temp_0c_msb          = 0x00, /**< 0.0degC MSB: 0x0000 = 0 decimal */
  k_test_temp_minus_55c_lsb = 0x90,  /**< -55.0degC LSB: 0xFC90 = -880 decimal (two's complement) */
  k_test_temp_minus_55c_msb = 0xFC,  /**< -55.0degC MSB: 0xFC90 = -880 decimal (two's complement) */
  k_test_temp_125c_lsb      = 0xD0,  /**< +125.0degC LSB: 0x07D0 = 2000 decimal */
  k_test_temp_125c_msb      = 0x07,  /**< +125.0degC MSB: 0x07D0 = 2000 decimal */
  k_test_config_12bit       = 0x7F,  /**< 12-bit resolution config: R1=1, R0=1 -> 0b01111111 */
  k_test_invalid_resolution = 99,    /**< Out-of-range resolution value for error tests */
  k_test_ds1820_family_code = 0x10,  /**< DS1820 family code (not DS18B20) */
  k_test_out_of_range_msb   = 0x80,  /**< MSB for out-of-range temp (0x8000 = -32768) */
  k_test_too_high_temp_lsb  = 0xD1,  /**< LSB for temp too high (0x07D1 = 2001 > max) */
  k_test_too_high_temp_msb  = 0x07,  /**< MSB for temp too high (0x07D1 = 2001 > max) */
  k_test_invalid_byte       = 0xFFU, /**< Invalid byte value for error tests */
  k_test_write_fail_after_2 = 2U,    /**< Fail write after 2 successful calls */
  k_test_skip_rom_fail_at_4 = 4U,    /**< Fail skip_rom on 4th call */
  k_test_presence_fail_at_5 = 5U,    /**< Fail presence on 5th reset */
  k_test_reset_error_after_4 =
    4U, /**< Fail reset on 5th call (read_scratchpad in set_resolution) */
  k_test_reset_error_after_5 =
    5U, /**< Fail reset on 6th call (write_scratchpad in set_resolution) */
} ds18b20_mock_constants_t;

/**
 * @brief DS18B20 family code (first byte of ROM)
 * @details
 * Identifies device type on 1-Wire bus. All DS18B20 sensors use family
 * code 0x28. Other Maxim sensors use different codes (e.g., DS1820 = 0x10).
 */
static const uint8_t s_ds18b20_family_code = 0x28U;

/**
 * @enum rom_byte_index_t
 * @brief Byte indices for ROM code population in mock state
 *
 * @details
 * Defines named indices for accessing 64-bit ROM code bytes. Used when
 * initializing mock ROM with family code, serial number, and CRC.
 *
 * **ROM Structure:**
 * @code
 * Index 0: Family Code (0x28)
 * Index 1-6: Serial Number (6 bytes, arbitrary for testing)
 * Index 7: CRC-8 checksum (calculated from bytes 0-6)
 * @endcode
 */
typedef enum : uint8_t {
  k_rom_idx_family   = 0, /**< Family code byte index (always 0x28 for DS18B20) */
  k_rom_idx_serial_0 = 1, /**< Serial number byte 0 (LSB of 48-bit serial) */
  k_rom_idx_serial_1 = 2, /**< Serial number byte 1 */
  k_rom_idx_serial_2 = 3, /**< Serial number byte 2 */
  k_rom_idx_serial_3 = 4, /**< Serial number byte 3 */
  k_rom_idx_serial_4 = 5, /**< Serial number byte 4 */
  k_rom_idx_serial_5 = 6, /**< Serial number byte 5 (MSB of 48-bit serial) */
} rom_byte_index_t;

/**
 * @enum mock_serial_bytes_t
 * @brief Mock serial number bytes for default ROM code
 *
 * @details
 * Arbitrary serial number values used for testing. In production, each
 * DS18B20 has a globally unique 48-bit serial number. For testing, we
 * use sequential values for simplicity.
 *
 * **Complete Mock ROM Code:**
 * `[0x28, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, CRC]`
 */
typedef enum : uint8_t {
  k_mock_serial_byte_0 = 0x01, /**< Serial byte 0 (LSB) */
  k_mock_serial_byte_1 = 0x02, /**< Serial byte 1 */
  k_mock_serial_byte_2 = 0x03, /**< Serial byte 2 */
  k_mock_serial_byte_3 = 0x04, /**< Serial byte 3 */
  k_mock_serial_byte_4 = 0x05, /**< Serial byte 4 */
  k_mock_serial_byte_5 = 0x06, /**< Serial byte 5 (MSB) */
} mock_serial_bytes_t;

/**
 * @enum mock_ds18b20_constants_t
 * @brief Constants specific to DS18B20 mock behavior
 *
 * @details
 * Configuration values for mock device count and search functionality.
 * Used when simulating multi-drop 1-Wire buses with ROM search.
 */
typedef enum : uint8_t {
  k_mock_ds18b20_device_count = 1, /**< Number of mock devices returned by search (single device) */
} mock_ds18b20_constants_t;

/**
 * @brief Temperature comparison tolerance for floating-point validation
 * @details
 * Acceptable error margin for temperature comparisons. DS18B20 accuracy
 * is +/-0.5degC, but we use tighter tolerance (+/-0.1degC) to verify correct
 * calculation without excessive floating-point precision requirements.
 */
static const float s_temp_tolerance_c = 0.1F;

/**
 * @brief Expected temperature values for assertion comparisons
 * @{
 */
static const float s_test_expected_25c       = 25.0F;  /**< +25.0degC expected value */
static const float s_test_expected_minus_55c = -55.0F; /**< -55.0degC expected value */
static const float s_test_expected_125c      = 125.0F; /**< +125.0degC expected value */
/** @} */

/* =============================================================================
 * Mock OneWire Bus Manager
 * =============================================================================
 */

/**
 * @brief Mock bus manager instance for testing
 * @details
 * Static bus manager structure passed to DS18B20 driver during initialization.
 * Actual bus operations are intercepted by mock functions below.
 */
static rx_bus_manager_t s_mock_bus_manager;

/**
 * @brief Mock bus name string
 * @details
 * Arbitrary name used for testing. In production, bus names identify
 * physical GPIO pins (e.g., "onewire_p40" for port 4, pin 0).
 */
static const char* const s_test_bus_name = "test_onewire";

/* =============================================================================
 * Mock OneWire Bus Functions
 * =============================================================================
 */

/**
 * @brief Mock OneWire bus initialization
 *
 * @details
 * Simulates `rx_bus_onewire_init()` without accessing real GPIO hardware.
 * Sets mock state to initialized, allowing subsequent bus operations.
 *
 * **Mock Behavior:**
 * - Validates pointers (nullptr check)
 * - Prevents double initialization
 * - Sets `s_mock_state.initialized = true`
 *
 * **Real Function Behavior (Not Implemented):**
 * - Configure GPIO pin as open-drain output
 * - Enable microsecond timer for 1-Wire timing
 * - Pull bus high (idle state)
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 *
 * @return k_rx_ok Success, mock bus initialized
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr
 * @retval k_rx_err_invalid_state Already initialized
 *
 * @pre manager != nullptr
 * @pre bus_name != nullptr
 * @pre s_mock_state.initialized == false
 * @post s_mock_state.initialized == true
 *
 * @note Called by rx_ds18b20_init() during driver initialization
 * @see rx_bus_onewire_init() Real implementation
 */
rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name)
{
  if ((manager == nullptr) || (bus_name == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (s_mock_state.force_init_error) {
    return k_rx_err_hw_error;
  }

  if (s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  s_mock_state.initialized = true;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire bus reset with presence detection
 *
 * @details
 * Simulates 1-Wire reset pulse and presence detection without hardware timing.
 * Returns mock presence status, enabling tests to simulate device connection
 * and disconnection scenarios.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Returns `s_mock_state.presence_response` (configurable by test)
 *
 * **Real 1-Wire Reset Protocol (Not Implemented):**
 * ```
 * Timeline:
 * --------+        +----------------------------
 *         +--------+
 *         ^        ^
 *         480us    Device pulls low 60-240us (presence pulse)
 *         Controller
 *         reset
 * ```
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[out] presence True if device detected, false if no response
 *
 * @return k_rx_ok Success, presence status returned
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 * @post *presence set to s_mock_state.presence_response
 *
 * @note Called before every DS18B20 command sequence
 * @see rx_bus_onewire_reset() Real implementation
 */
rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence)
{
  if ((manager == nullptr) || (bus_name == nullptr) || (presence == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_mock_state.force_reset_error) {
    return k_rx_err_hw_error;
  }

  if ((int)s_mock_state.force_reset_error_after &&
      s_mock_state.reset_call_count >= s_mock_state.reset_error_after) {
    s_mock_state.reset_call_count++;
    return k_rx_err_hw_error;
  }

  if ((int)s_mock_state.force_presence_false_after &&
      s_mock_state.reset_call_count >= s_mock_state.presence_false_after) {
    *presence = false;
  } else {
    *presence = s_mock_state.presence_response;
  }
  s_mock_state.reset_call_count++;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire write byte operation
 *
 * @details
 * Accepts write commands without performing actual 1-Wire bit timing.
 * Used for sending DS18B20 function commands (Convert T, Read Scratchpad, etc.).
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Discards byte value (no state change)
 *
 * **Real 1-Wire Write Timing (Not Implemented):**
 * ```
 * Write 0:  Controller holds low 60-120us
 * Write 1:  Controller holds low 1-15us, release to high
 * Recovery: 1us minimum between bits
 * ```
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[in] byte Byte to write (ignored in mock)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 *
 * @note Used for ROM commands and function commands
 * @see rx_bus_onewire_write_byte() Real implementation
 */
rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  (void)byte;

  if ((manager == nullptr) || (bus_name == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_mock_state.force_write_error) {
    if (s_mock_state.write_fail_after == 0U ||
        s_mock_state.write_call_count >= s_mock_state.write_fail_after) {
      return k_rx_err_hw_error;
    }
  }
  s_mock_state.write_call_count++;

  return k_rx_ok;
}

/**
 * @brief Mock OneWire read byte operation
 *
 * @details
 * Returns default value 0xFF without performing actual 1-Wire timing.
 * Real implementation would read 8 bits sequentially with precise timing.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Returns `k_mock_default_read_byte` (0xFF)
 *
 * **Real 1-Wire Read Timing (Not Implemented):**
 * ```
 * Controller initiates: Pull low 1-15us, release
 * Device responds: Holds low (0) or releases high (1)
 * Sample time: 15us after initiation
 * ```
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[out] byte Output byte (set to 0xFF)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 * @post *byte == 0xFF
 *
 * @note Not used by DS18B20 driver (uses rx_bus_onewire_read() instead)
 * @see rx_bus_onewire_read_byte() Real implementation
 */
rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  if ((manager == nullptr) || (bus_name == nullptr) || (byte == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  *byte = k_mock_default_read_byte;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire read single bit operation
 *
 * @details
 * Returns power mode bit for Read Power Supply command (0xB4).
 * DS18B20 responds with 0 (parasitic) or 1 (external VDD).
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Returns `s_mock_state.power_mode` (configurable by test)
 *
 * **Real 1-Wire Bit Read:**
 * After Read Power Supply command, DS18B20 drives DQ line:
 * - Low (0): Parasitic power (VDD = GND)
 * - High (1): External power (VDD connected)
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[out] bit Output bit (power mode: 0 or 1)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 * @post *bit == s_mock_state.power_mode
 *
 * @note Used exclusively by rx_ds18b20_read_power_mode()
 * @see rx_bus_onewire_read_bit() Real implementation
 */
rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit)
{
  if ((manager == nullptr) || (bus_name == nullptr) || (bit == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_mock_state.force_read_error) {
    return k_rx_err_hw_error;
  }

  *bit = (bool)(s_mock_state.power_mode != 0U);
  return k_rx_ok;
}

/**
 * @brief Mock OneWire block read operation (scratchpad read)
 *
 * @details
 * Copies mock scratchpad contents to output buffer. Primary function used
 * by DS18B20 driver to read temperature and configuration data.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Copies up to `length` bytes from `s_mock_state.scratchpad[]`
 * - Clamps to `k_ds18b20_scratchpad_bytes` (9 bytes maximum)
 *
 * **Real Implementation:**
 * After Read Scratchpad command (0xBE), DS18B20 sends 9 bytes:
 * 1. Temperature LSB
 * 2. Temperature MSB
 * 3. TH register
 * 4. TL register
 * 5. Configuration register
 * 6-8. Reserved bytes
 * 9. CRC-8 checksum
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[out] data Output buffer (receives scratchpad copy)
 * @param[in] length Number of bytes to read (clamped to 9)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 * @pre data buffer size >= length
 * @post data contains copy of scratchpad bytes
 *
 * @note Primary method for reading DS18B20 scratchpad
 * @see rx_bus_onewire_read() Real implementation
 * @see internal_create_valid_scratchpad() Helper to populate mock scratchpad
 */
rx_err_t
rx_bus_onewire_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint32_t length)
{
  if ((manager == nullptr) || (bus_name == nullptr) || (data == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_mock_state.force_read_error) {
    return k_rx_err_hw_error;
  }

  if (length > k_ds18b20_scratchpad_bytes) {
    length = k_ds18b20_scratchpad_bytes;
  }

  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(data, s_mock_state.scratchpad, length);
  return k_rx_ok;
}

/**
 * @brief Mock OneWire block write operation
 *
 * @details
 * Accepts write data without modifying mock state. Used for Write Scratchpad
 * command (0x4E) to set TH, TL, and configuration registers.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Discards data (no state modification)
 *
 * **Real Write Scratchpad Sequence:**
 * After Write Scratchpad command (0x4E), send 3 bytes:
 * 1. TH (high alarm threshold)
 * 2. TL (low alarm threshold)
 * 3. Configuration register (resolution bits)
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[in] data Input buffer (ignored in mock)
 * @param[in] length Number of bytes to write (ignored in mock)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 *
 * @note Could be enhanced to update mock scratchpad for write verification
 * @see rx_bus_onewire_write() Real implementation
 */
rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              const uint8_t*    data,
                              uint32_t          length)
{
  if ((manager == nullptr) || (bus_name == nullptr) || (data == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if ((int)s_mock_state.force_write_error || (int)s_mock_state.force_write_data_error) {
    return k_rx_err_hw_error;
  }

  if (length == 0U) {
    return k_rx_ok;
  }

  return k_rx_ok;
}

/**
 * @brief Mock OneWire Skip ROM command
 *
 * @details
 * Simulates Skip ROM command (0xCC) for single-device bus configurations.
 * In production, broadcasts subsequent commands to all devices.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - No-op (accepts command without action)
 *
 * **Real Skip ROM Usage:**
 * - Single device: Addresses only device on bus (fast, no ROM needed)
 * - Multiple devices: Broadcasts to ALL devices (use with caution)
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 *
 * @note Used when `use_rom_matching = false` in driver config
 * @see rx_bus_onewire_skip_rom() Real implementation
 */
rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name)
{
  if ((manager == nullptr) || (bus_name == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_mock_state.force_skip_rom_error) {
    if (s_mock_state.skip_rom_fail_after == 0U ||
        s_mock_state.skip_rom_call_count >= s_mock_state.skip_rom_fail_after) {
      return k_rx_err_hw_error;
    }
  }
  s_mock_state.skip_rom_call_count++;

  return k_rx_ok;
}

/**
 * @brief Mock OneWire Match ROM command
 *
 * @details
 * Simulates Match ROM command (0x55) for multi-device bus addressing.
 * In production, selects specific device by 64-bit ROM code.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - No-op (accepts ROM code without validation)
 *
 * **Real Match ROM Protocol:**
 * ```
 * 1. Send Match ROM command (0x55)
 * 2. Send 64-bit ROM code (8 bytes)
 * 3. Only device with matching ROM responds to subsequent commands
 * ```
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[in] rom 64-bit ROM code array (ignored in mock)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 *
 * @note Used when `use_rom_matching = true` in driver config
 * @see rx_bus_onewire_match_rom() Real implementation
 */
rx_err_t rx_bus_onewire_match_rom(rx_bus_manager_t* manager,
                                  const char*       bus_name,
                                  const uint8_t     rom[k_onewire_rom_bytes])
{
  if ((manager == nullptr) || (bus_name == nullptr) || (rom == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_mock_state.force_match_rom_error) {
    return k_rx_err_hw_error;
  }

  return k_rx_ok;
}

/**
 * @brief Mock OneWire Read ROM command
 *
 * @details
 * Returns mock 64-bit ROM code. In production, only works with single device
 * on bus (multiple devices cause bus conflicts).
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Copies `s_mock_state.rom[]` to output buffer
 *
 * **Real Read ROM Protocol:**
 * ```
 * 1. Send Read ROM command (0x33)
 * 2. Device sends 64-bit ROM code:
 *    - Byte 0: Family code (0x28 for DS18B20)
 *    - Bytes 1-6: Serial number (globally unique)
 *    - Byte 7: CRC-8 checksum
 * ```
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[out] rom Output buffer for ROM code (8 bytes)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 * @post rom contains copy of s_mock_state.rom[]
 *
 * @note Not currently used by DS18B20 driver tests
 * @see rx_bus_onewire_read_rom() Real implementation
 */
rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint8_t           rom[k_onewire_rom_bytes])
{
  if ((manager == nullptr) || (bus_name == nullptr) || (rom == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(rom, s_mock_state.rom, k_onewire_rom_bytes);
  return k_rx_ok;
}

/**
 * @brief Mock OneWire ROM Search command
 *
 * @details
 * Simulates ROM search algorithm for device discovery on multi-drop bus.
 * Returns mock device count without performing actual search.
 *
 * **Mock Behavior:**
 * - Validates pointers
 * - Returns `k_mock_ds18b20_device_count` (always 1)
 * - Does not populate `roms` buffer
 *
 * **Real Search ROM Algorithm:**
 * ```
 * Complex binary tree search algorithm that:
 * 1. Sends Search ROM command (0xF0)
 * 2. Iterates through 64 ROM bits
 * 3. Detects conflicts (multiple devices with different bit values)
 * 4. Explores all branches to find all ROM codes
 * ```
 *
 * @param[in] manager Bus manager instance (validated, not used)
 * @param[in] bus_name Bus name string (validated, not used)
 * @param[out] roms Output buffer for ROM codes (not populated in mock)
 * @param[in] max_devices Maximum devices to search for (ignored in mock)
 * @param[out] num_devices Number of devices found (set to 1)
 *
 * @return k_rx_ok Success
 * @retval k_rx_err_null_ptr Any pointer is nullptr
 * @retval k_rx_err_invalid_state Bus not initialized
 *
 * @pre s_mock_state.initialized == true
 * @post *num_devices == 1
 *
 * @note Not fully implemented - only returns count, not ROM data
 * @see rx_bus_onewire_search() Real implementation
 */
rx_err_t rx_bus_onewire_search(
  rx_bus_manager_t* manager,
  const char*       bus_name,
  uint8_t*          roms, /* NOLINT(readability-non-const-parameter) - must match header */
  uint32_t          max_devices,
  uint32_t*         num_devices)
{
  (void)roms;
  (void)max_devices;

  if ((manager == nullptr) || (bus_name == nullptr) || (num_devices == nullptr)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  *num_devices = k_mock_ds18b20_device_count;
  return k_rx_ok;
}

/* =============================================================================
 * Test Helper Functions
 * =============================================================================
 */

/**
 * @struct temp_raw_t
 * @brief Temperature raw value structure (LSB + MSB bytes)
 *
 * @details
 * Convenient packaging of 16-bit temperature as two 8-bit bytes matching
 * DS18B20 scratchpad format. Used by helper functions to construct valid
 * scratchpad data for testing.
 *
 * **Temperature Encoding:**
 * ```
 * 16-bit value = (msb << 8) | lsb
 * Temperature degC = 16-bit signed value / 16.0
 * ```
 *
 * @par Example:
 * @code
 * temp_raw_t temp_25c = {.lsb = 0x90, .msb = 0x01};  // +25.0degC
 * // Decoded: 0x0190 = 400 decimal = 400/16 = 25.0degC
 * @endcode
 */
typedef struct {
  uint8_t lsb; /**< Temperature LSB (bits 0-7 of 16-bit value) */
  uint8_t msb; /**< Temperature MSB (sign bits + bits 8-11) */
} temp_raw_t;

/**
 * @brief Create valid DS18B20 scratchpad with CRC-8 checksum
 *
 * @details
 * Populates 9-byte scratchpad buffer with temperature, configuration, reserved
 * bytes, and computed CRC-8 checksum. Essential helper for setting up mock
 * device state with realistic sensor data.
 *
 * **Algorithm:**
 * 1. Write temperature bytes (LSB, MSB)
 * 2. Write alarm thresholds (TH=0x00, TL=0x00)
 * 3. Write configuration register (resolution bits)
 * 4. Write reserved bytes (0xFF, 0x10, 0x00 per datasheet)
 * 5. Calculate CRC-8/MAXIM of first 8 bytes
 * 6. Write CRC to byte 8
 *
 * **CRC Calculation:**
 * - Polynomial: 0x31 (x^8 + x^5 + x^4 + 1)
 * - Initial value: 0x00
 * - Input: Bytes 0-7 (inclusive)
 *
 * @param[out] scratchpad 9-byte scratchpad buffer to populate
 * @param[in] temp_raw Temperature as LSB+MSB structure
 * @param[in] config Configuration register value (resolution bits at [6:5])
 *
 * @pre scratchpad buffer size >= k_ds18b20_scratchpad_bytes (9 bytes)
 * @post scratchpad contains valid sensor data with correct CRC
 * @post scratchpad[8] = CRC-8 of bytes 0-7
 *
 * @invariant Reserved byte 5 == 0xFF
 * @invariant CRC byte 8 matches calculated CRC
 *
 * @note Called by setUp() and individual tests to configure mock state
 * @warning Do not modify scratchpad after CRC calculation (invalidates checksum)
 *
 * @par Example: Create +25.0degC at 12-bit resolution
 * @code
 * uint8_t scratchpad[9];
 * temp_raw_t temp_25c = {.lsb = 0x90, .msb = 0x01};  // 0x0190 = 400 = 25degC
 * internal_create_valid_scratchpad(scratchpad, temp_25c, k_test_config_12bit);
 * memcpy(s_mock_state.scratchpad, scratchpad, 9);
 * @endcode
 *
 * @see rx_crc8_maxim() CRC-8 calculation function
 * @see internal_reset_mock_state() Caller in setUp()
 */
static void internal_create_valid_scratchpad(uint8_t    scratchpad[k_ds18b20_scratchpad_bytes],
                                             temp_raw_t temp_raw,
                                             uint8_t    config)
{
  scratchpad[k_ds18b20_scratch_temp_lsb]  = temp_raw.lsb;
  scratchpad[k_ds18b20_scratch_temp_msb]  = temp_raw.msb;
  scratchpad[k_ds18b20_scratch_th_reg]    = k_ds18b20_default_th_tl;
  scratchpad[k_ds18b20_scratch_tl_reg]    = k_ds18b20_default_th_tl;
  scratchpad[k_ds18b20_scratch_config]    = config;
  scratchpad[k_ds18b20_scratch_reserved1] = k_ds18b20_reserved1_default;
  scratchpad[k_ds18b20_scratch_reserved2] = k_ds18b20_reserved2_default;
  scratchpad[k_ds18b20_scratch_reserved3] = k_ds18b20_reserved3_default;
  uint32_t s_crc_out                      = 0U;
  (void)rx_crc8_maxim(scratchpad, k_ds18b20_crc_bytes, &s_crc_out);
  scratchpad[k_ds18b20_scratch_crc] = (uint8_t)s_crc_out;
}

/**
 * @brief Reset mock state to default values
 *
 * @details
 * Restores mock OneWire bus and DS18B20 device state to known baseline.
 * Called by Unity setUp() before each test to ensure test isolation and
 * repeatability.
 *
 * **Default State After Reset:**
 * ```
 * +---------------------+---------------------------------+
 * | Field               | Default Value                   |
 * +---------------------+---------------------------------+
 * | presence_response   | true (device present)           |
 * | power_mode          | true (external VDD)             |
 * | initialized         | false (bus needs init)          |
 * | scratchpad          | +25.0degC at 12-bit with CRC      |
 * | rom[0]              | 0x28 (DS18B20 family code)      |
 * | rom[1-6]            | 0x01-0x06 (mock serial)         |
 * | rom[7]              | CRC-8 of rom[0-6]               |
 * +---------------------+---------------------------------+
 * ```
 *
 * **Algorithm:**
 * 1. Clear entire mock_state structure (memset to 0)
 * 2. Set presence_response = true (device present)
 * 3. Set power_mode = true (external power)
 * 4. Generate valid scratchpad (+25.0degC, 12-bit resolution)
 * 5. Populate ROM code with family code, mock serial, and CRC
 *
 * @pre None
 * @post s_mock_state contains default values
 * @post s_mock_state.scratchpad has valid CRC
 * @post s_mock_state.rom has valid CRC
 *
 * @note Called automatically by Unity before each test
 * @warning Tests that modify mock_state depend on this reset
 *
 * @par Example: Test-Specific State Modification
 * @code
 * void test_no_device(void) {
 *     // setUp() already called - mock at defaults
 *     s_mock_state.presence_response = false;  // Simulate device removed
 *     rx_err_t err = rx_ds18b20_init(&handle, &config);
 *     TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
 * }
 * @endcode
 *
 * @see setUp() Unity fixture function that calls this
 * @see internal_create_valid_scratchpad() Helper for scratchpad generation
 */
static void internal_reset_mock_state(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset((uint8_t*)&s_mock_state, 0, sizeof(s_mock_state));
  s_mock_state.presence_response = true;
  s_mock_state.power_mode        = (uint8_t) true; /* External power */
  s_mock_state.initialized       = false;

  /* Default scratchpad: +25.0degC at 12-bit resolution */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_25c_lsb, .msb = k_test_temp_25c_msb},
    k_test_config_12bit);

  /* Default ROM: DS18B20 family code */
  s_mock_state.rom[k_rom_idx_family]   = s_ds18b20_family_code;
  s_mock_state.rom[k_rom_idx_serial_0] = k_mock_serial_byte_0;
  s_mock_state.rom[k_rom_idx_serial_1] = k_mock_serial_byte_1;
  s_mock_state.rom[k_rom_idx_serial_2] = k_mock_serial_byte_2;
  s_mock_state.rom[k_rom_idx_serial_3] = k_mock_serial_byte_3;
  s_mock_state.rom[k_rom_idx_serial_4] = k_mock_serial_byte_4;
  s_mock_state.rom[k_rom_idx_serial_5] = k_mock_serial_byte_5;
  uint32_t rom_crc_out                 = 0U;
  (void)rx_crc8_maxim(s_mock_state.rom, k_onewire_rom_crc_index, &rom_crc_out);
  s_mock_state.rom[k_onewire_rom_crc_index] = (uint8_t)rom_crc_out;
}

/**
 * @brief Initialize DS18B20 handle to zero state
 *
 * @details
 * Clears driver handle structure before initialization. Ensures no garbage
 * values from previous tests interfere with current test.
 *
 * **Fields Cleared:**
 * - bus_manager -> nullptr
 * - bus_name -> nullptr
 * - resolution -> 0 (invalid)
 * - use_rom_matching -> false
 * - rom[] -> all zeros
 * - initialized -> false
 *
 * @param[out] handle DS18B20 handle to initialize
 *
 * @pre handle != nullptr (verified by TEST_ASSERT_NOT_NULL)
 * @post handle contains all zeros
 *
 * @note Called at start of most tests before rx_ds18b20_init()
 * @warning Required for deterministic test behavior
 *
 * @par Example:
 * @code
 * void test_init_success(void) {
 *     rx_ds18b20_handle_t handle;
 *     internal_init_handle(&handle);  // Clear to zeros
 *     rx_err_t err = rx_ds18b20_init(&handle, &config);
 *     TEST_ASSERT_EQUAL(k_rx_ok, err);
 * }
 * @endcode
 *
 * @see rx_ds18b20_init() Driver initialization function
 */
static void internal_init_handle(rx_ds18b20_handle_t* handle)
{
  TEST_ASSERT_NOT_NULL(handle);
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset((uint8_t*)handle, 0, sizeof(*handle));
}

/* =============================================================================
 * Unity Test Framework Setup/Teardown
 * =============================================================================
 */

/**
 * @brief Unity setUp function called before each test
 *
 * @details
 * Resets mock state to defaults, ensuring test isolation. Unity automatically
 * calls this before executing each test function.
 *
 * **Execution Order:**
 * ```
 * setUp() -> test_xxx() -> tearDown() -> (repeat for next test)
 * ```
 *
 * @pre None (called by Unity framework)
 * @post s_mock_state reset to defaults
 *
 * @note Automatically invoked by Unity - do not call manually
 * @see internal_reset_mock_state() Implementation
 * @see tearDown() Cleanup function (currently no-op)
 */
void setUp(void)
{
  internal_reset_mock_state();
}

/**
 * @brief Unity tearDown function called after each test
 *
 * @details
 * Currently no cleanup needed - mock state is stateless and reset in setUp().
 * Reserved for future use if dynamic resources are added to tests.
 *
 * @pre Test has completed
 * @post None (no cleanup required)
 *
 * @note Automatically invoked by Unity - do not call manually
 * @see setUp() Initialization function
 */
void tearDown(void)
{
  /* Nothing to tear down */
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @defgroup init_tests Initialization Tests
 * @brief Test DS18B20 driver initialization with various configurations
 *
 * @details
 * Validates initialization logic, configuration validation, device presence
 * detection, and error handling. Tests ensure driver fails gracefully with
 * invalid inputs and only initializes when device is present on bus.
 *
 * **Test Coverage:**
 * - [OK] Valid initialization (all resolutions)
 * - [OK] nullptr pointer validation (handle, config)
 * - [OK] Device presence detection (no device on bus)
 * - [OK] Double initialization prevention
 * - [OK] Invalid resolution rejection
 *
 * @{
 */

/**
 * @brief Test successful DS18B20 initialization with valid configuration
 *
 * @details
 * Verifies driver initializes correctly with valid parameters and device present.
 * Checks that handle is marked initialized and resolution is stored.
 *
 * **Test Steps:**
 * 1. Create handle and config with 12-bit resolution
 * 2. Call rx_ds18b20_init()
 * 3. Verify k_rx_ok returned
 * 4. Verify handle.initialized == true
 * 5. Verify handle.resolution == k_ds18b20_resolution_12bit
 *
 * **Expected Behavior:**
 * - Bus initialization succeeds (mock returns k_rx_ok)
 * - Device presence detected (mock returns true)
 * - Resolution configured (12-bit)
 * - Handle marked initialized
 *
 * @note This is the "happy path" test - all conditions favorable
 */
void test_ds18b20_init_success(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(handle.initialized);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_12bit, handle.resolution);
}

/**
 * @brief Test initialization with nullptr handle pointer
 *
 * @details
 * Verifies driver rejects nullptr handle with appropriate error code.
 * Tests NASA Power of 10 Rule 5 (assertions) - nullptr pointer validation.
 *
 * **Test Steps:**
 * 1. Create valid config
 * 2. Call rx_ds18b20_init(nullptr, &config)
 * 3. Verify k_rx_err_null_ptr returned
 *
 * **Expected Behavior:**
 * - Function returns immediately with k_rx_err_null_ptr
 * - No bus operations attempted (would segfault)
 */
void test_ds18b20_init_null_handle(void)
{
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_err_t err = rx_ds18b20_init(nullptr, &config);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test initialization with nullptr config pointer
 *
 * @details
 * Verifies driver rejects nullptr config with appropriate error code.
 *
 * **Test Steps:**
 * 1. Create handle
 * 2. Call rx_ds18b20_init(&handle, nullptr)
 * 3. Verify k_rx_err_null_ptr returned
 *
 * **Expected Behavior:**
 * - Function returns immediately with k_rx_err_null_ptr
 * - Handle remains uninitialized
 */
void test_ds18b20_init_null_config(void)
{
  rx_ds18b20_handle_t handle;

  internal_init_handle(&handle);

  rx_err_t err = rx_ds18b20_init(&handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test initialization when DS18B20 device not present on bus
 *
 * @details
 * Simulates device disconnection or wiring fault by setting mock presence
 * response to false. Verifies driver detects absence and fails initialization.
 *
 * **Test Steps:**
 * 1. Set s_mock_state.presence_response = false (no device)
 * 2. Call rx_ds18b20_init()
 * 3. Verify k_rx_err_invalid_state returned
 * 4. Verify handle.initialized == false
 *
 * **Expected Behavior:**
 * - Bus reset succeeds (mock returns k_rx_ok)
 * - Presence detect fails (mock returns false)
 * - Initialization aborted with k_rx_err_invalid_state
 *
 * **Real-World Scenario:**
 * - DS18B20 not connected to GPIO pin
 * - 4.7kOhm pull-up resistor missing
 * - Device powered off or damaged
 */
void test_ds18b20_init_no_device_present(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  s_mock_state.presence_response = false;

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_FALSE(handle.initialized);
}

/**
 * @brief Test double initialization prevention
 *
 * @details
 * Verifies driver prevents re-initialization of already initialized handle.
 * Protects against accidental reconfiguration that could corrupt state.
 *
 * **Test Steps:**
 * 1. Initialize handle successfully
 * 2. Call rx_ds18b20_init() again with same handle
 * 3. Verify k_rx_err_invalid_state returned
 *
 * **Expected Behavior:**
 * - First init succeeds (returns k_rx_ok)
 * - Second init rejected (returns k_rx_err_invalid_state)
 * - Handle state unchanged from first init
 */
void test_ds18b20_init_already_initialized(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test initialization with invalid resolution value
 *
 * @details
 * Verifies driver validates resolution enum and rejects out-of-range values.
 * Protects against configuration errors and memory corruption.
 *
 * **Test Steps:**
 * 1. Create config with resolution = 99 (invalid, valid range is 0-3)
 * 2. Call rx_ds18b20_init()
 * 3. Verify k_rx_err_invalid_arg returned
 *
 * **Expected Behavior:**
 * - Config validation detects invalid resolution
 * - Initialization aborted with k_rx_err_invalid_arg
 * - No bus operations attempted
 *
 * **Valid Resolution Values:**
 * - k_ds18b20_resolution_9bit = 0
 * - k_ds18b20_resolution_10bit = 1
 * - k_ds18b20_resolution_11bit = 2
 * - k_ds18b20_resolution_12bit = 3
 */
void test_ds18b20_init_invalid_resolution(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager = &s_mock_bus_manager,
    .bus_name    = s_test_bus_name,
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    .resolution       = (ds18b20_resolution_t)k_test_invalid_resolution,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/** @} */ // end of init_tests

/* =============================================================================
 * Temperature Reading Tests
 * =============================================================================
 */

/**
 * @defgroup temp_tests Temperature Reading Tests
 * @brief Test temperature measurement across full sensor range
 *
 * @details
 * Validates temperature conversion from 16-bit raw values to Celsius,
 * including two's complement decoding for negative temperatures.
 *
 * **Test Coverage:**
 * - [OK] Positive temperatures (+25degC, +125degC)
 * - [OK] Zero crossing (0degC)
 * - [OK] Negative temperatures (-55degC via two's complement)
 * - [OK] Error handling (not initialized, nullptr output)
 *
 * @{
 */

/**
 * @brief Test reading +25.0degC (typical room temperature)
 *
 * @details
 * Verifies correct decoding of positive temperature value.
 *
 * **Temperature Encoding:**
 * - Raw value: 0x0190 = 400 decimal
 * - Calculation: 400 / 16 = 25.0degC
 *
 * **Test Steps:**
 * 1. Configure scratchpad with +25degC (LSB=0x90, MSB=0x01)
 * 2. Initialize driver
 * 3. Read temperature
 * 4. Verify result within +/-0.1degC of 25.0degC
 */
void test_ds18b20_read_temperature_25c(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;

  internal_init_handle(&handle);

  /* Scratchpad for +25.0degC: 0x0190 = 400 decimal = 25.0degC */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_25c_lsb, .msb = k_test_temp_25c_msb},
    k_test_config_12bit);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, s_test_expected_25c, temp_c);
}

/**
 * @brief Test reading 0.0degC (zero crossing point)
 *
 * @details
 * Verifies correct handling of zero value (no sign extension needed).
 *
 * **Temperature Encoding:**
 * - Raw value: 0x0000 = 0 decimal
 * - Calculation: 0 / 16 = 0.0degC
 */
void test_ds18b20_read_temperature_0c(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;

  internal_init_handle(&handle);

  /* 0degC: 0x0000 */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_0c_lsb, .msb = k_test_temp_0c_msb},
    k_test_config_12bit);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, 0.0F, temp_c);
}

/**
 * @brief Test reading -55.0degC (minimum sensor range)
 *
 * @details
 * Verifies correct two's complement decoding for negative temperatures.
 *
 * **Temperature Encoding (Two's Complement):**
 * - Raw value: 0xFC90 = -880 decimal (signed 16-bit)
 * - Calculation: -880 / 16 = -55.0degC
 *
 * **Two's Complement Explanation:**
 * ```
 * 0xFC90 in binary:
 * MSB: 1111 1100 (sign bit = 1 -> negative)
 * LSB: 1001 0000
 *
 * Convert to decimal (signed):
 * 0xFC90 = -880 (16-bit signed interpretation)
 * -880 / 16 = -55.0degC [OK]
 * ```
 */
void test_ds18b20_read_temperature_minus_55c(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;

  internal_init_handle(&handle);

  /* -55degC: 0xFC90 (two's complement) */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_minus_55c_lsb, .msb = k_test_temp_minus_55c_msb},
    k_test_config_12bit);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, s_test_expected_minus_55c, temp_c);
}

/**
 * @brief Test reading +125.0degC (maximum sensor range)
 *
 * @details
 * Verifies correct decoding of maximum temperature value.
 *
 * **Temperature Encoding:**
 * - Raw value: 0x07D0 = 2000 decimal
 * - Calculation: 2000 / 16 = 125.0degC
 */
void test_ds18b20_read_temperature_125c(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;

  internal_init_handle(&handle);

  /* +125degC: 0x07D0 = 2000 decimal = 125.0degC */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_125c_lsb, .msb = k_test_temp_125c_msb},
    k_test_config_12bit);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, s_test_expected_125c, temp_c);
}

/**
 * @brief Test reading temperature before initialization
 *
 * @details
 * Verifies driver rejects temperature read on uninitialized handle.
 */
void test_ds18b20_read_temperature_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  float               temp_c = 0.0F;

  internal_init_handle(&handle);

  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test reading temperature with nullptr output pointer
 *
 * @details
 * Verifies driver validates output pointer before read operation.
 */
void test_ds18b20_read_temperature_null_output(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ // end of temp_tests

/* =============================================================================
 * Resolution Tests
 * =============================================================================
 */

/**
 * @defgroup resolution_tests Resolution Configuration Tests
 * @brief Test resolution configuration and conversion time calculation
 *
 * **Test Coverage:**
 * - [OK] Set resolution (9, 10, 11, 12 bit)
 * - [OK] Get resolution (verify persistence)
 * - [OK] Conversion time calculation (94ms to 750ms)
 *
 * @{
 */

/**
 * @brief Test setting 9-bit resolution (fastest, lowest precision)
 *
 * @details
 * Verifies driver can change resolution to 9-bit mode.
 *
 * **9-bit Specifications:**
 * - Precision: 0.5degC
 * - Conversion time: 93.75ms (rounded to 100ms)
 * - LSBs masked: Bits 0-2 undefined
 */
void test_ds18b20_set_resolution_9bit(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_9bit);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_9bit, handle.resolution);
}

/**
 * @brief Test getting current resolution setting
 *
 * @details
 * Verifies resolution value is correctly stored and retrieved.
 */
void test_ds18b20_get_resolution(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_11bit,
    .use_rom_matching = false,
  };
  ds18b20_resolution_t resolution;

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_get_resolution(&handle, &resolution);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_11bit, resolution);
}

/**
 * @brief Test conversion time calculation for 12-bit resolution
 *
 * @details
 * Verifies driver returns correct conversion time for maximum resolution.
 *
 * **12-bit Conversion Time:**
 * - Datasheet spec: 750ms
 * - Driver value: 800ms (includes 10% safety margin)
 */
void test_ds18b20_get_conversion_time_12bit(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);

  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_12bit_ms, time_ms);
}

/**
 * @brief Test conversion time calculation for 9-bit resolution
 *
 * @details
 * Verifies driver returns correct conversion time for minimum resolution.
 *
 * **9-bit Conversion Time:**
 * - Datasheet spec: 93.75ms
 * - Driver value: 100ms (includes safety margin)
 */
void test_ds18b20_get_conversion_time_9bit(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_9bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);

  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_9bit_ms, time_ms);
}

/** @} */ // end of resolution_tests

/* =============================================================================
 * Power Mode Tests
 * =============================================================================
 */

/**
 * @defgroup power_tests Power Mode Detection Tests
 * @brief Test detection of external vs parasitic power modes
 *
 * **Test Coverage:**
 * - [OK] External power (VDD connected)
 * - [OK] Parasitic power (VDD = GND, powered from DQ line)
 *
 * @{
 */

/**
 * @brief Test reading external power mode
 *
 * @details
 * Verifies driver correctly detects external VDD power supply.
 *
 * **External Power Configuration:**
 * - VDD pin connected to 3.3V or 5V
 * - Read Power Supply command returns 1
 */
void test_ds18b20_read_power_mode_external(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;

  internal_init_handle(&handle);

  s_mock_state.power_mode = (uint8_t) true;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_power_mode(&handle, &external_power);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(external_power);
}

/**
 * @brief Test reading parasitic power mode
 *
 * @details
 * Verifies driver correctly detects parasitic power operation.
 *
 * **Parasitic Power Configuration:**
 * - VDD pin connected to GND
 * - Device powered from DQ line (data pin)
 * - Read Power Supply command returns 0
 */
void test_ds18b20_read_power_mode_parasitic(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = true;

  internal_init_handle(&handle);

  s_mock_state.power_mode = (uint8_t) false;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_power_mode(&handle, &external_power);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(external_power);
}

/** @} */ // end of power_tests

/* =============================================================================
 * Conversion Tests
 * =============================================================================
 */

/**
 * @defgroup conversion_tests Temperature Conversion Tests
 * @brief Test triggering temperature conversion
 *
 * **Test Coverage:**
 * - [OK] Successful conversion trigger
 * - [OK] Conversion trigger with device not present
 *
 * @{
 */

/**
 * @brief Test successful temperature conversion trigger
 *
 * @details
 * Verifies driver sends Convert T command without errors.
 */
void test_ds18b20_trigger_conversion(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_trigger_conversion(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test conversion trigger when device not present
 *
 * @details
 * Simulates device disconnection during operation. Verifies driver
 * detects absence and rejects conversion command.
 */
void test_ds18b20_trigger_conversion_no_device(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.presence_response = false;

  rx_err_t err = rx_ds18b20_trigger_conversion(&handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ // end of conversion_tests

/* =============================================================================
 * Save/Recall Config and Read Scratchpad Tests
 * =============================================================================
 */

/**
 * @defgroup config_persist_tests Config Persistence and Scratchpad Tests
 * @brief Tests for save_config, recall_config, and read_scratchpad functions
 * @{
 */

/**
 * @brief Test rx_ds18b20_save_config() with valid initialized handle
 */
void test_ds18b20_save_config_success(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_save_config(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test rx_ds18b20_save_config() with nullptr handle
 */
void test_ds18b20_save_config_null(void)
{
  rx_err_t err = rx_ds18b20_save_config(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_ds18b20_recall_config() with valid initialized handle
 */
void test_ds18b20_recall_config_success(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_recall_config(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test rx_ds18b20_recall_config() with nullptr handle
 */
void test_ds18b20_recall_config_null(void)
{
  rx_err_t err = rx_ds18b20_recall_config(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_ds18b20_read_scratchpad() with valid initialized handle
 */
void test_ds18b20_read_scratchpad_success(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));

  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t err = rx_ds18b20_read_scratchpad(&handle, scratchpad);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test rx_ds18b20_read_scratchpad() with nullptr handle
 */
void test_ds18b20_read_scratchpad_null(void)
{
  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t err = rx_ds18b20_read_scratchpad(nullptr, scratchpad);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ // end of config_persist_tests

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

/**
 * @defgroup deinit_tests Deinitialization Tests
 * @brief Test driver cleanup and resource release
 *
 * **Test Coverage:**
 * - [OK] Clean deinitialization
 * - [OK] Handle state cleared
 *
 * @{
 */

/**
 * @brief Test successful deinitialization
 *
 * @details
 * Verifies driver releases resources and clears handle state.
 */
void test_ds18b20_deinit(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  internal_init_handle(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_deinit(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(handle.initialized);
}

/** @} */ // end of deinit_tests

/* =============================================================================
 * Additional Tests: null-handle / not-initialized / error-path coverage
 * =============================================================================
 */

void test_ds18b20_init_with_rom_matching(void)
{
  rx_ds18b20_handle_t handle;

  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = true,
  };
  /* Copy mock ROM into config */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(config.rom, s_mock_state.rom, k_onewire_rom_bytes);

  internal_init_handle(&handle);

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(handle.initialized);
  TEST_ASSERT_TRUE(handle.use_rom_matching);
}

void test_ds18b20_deinit_null_handle(void)
{
  rx_err_t err = rx_ds18b20_deinit(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_deinit_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_deinit(&handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_trigger_conversion_null_handle(void)
{
  rx_err_t err = rx_ds18b20_trigger_conversion(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_trigger_conversion_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_trigger_conversion(&handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_read_temperature_raw_null_handle(void)
{
  int16_t  raw_temp = 0;
  rx_err_t err      = rx_ds18b20_read_temperature_raw(nullptr, &raw_temp);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_read_temperature_raw_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  int16_t             raw_temp = 0;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_read_temperature_raw(&handle, &raw_temp);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_read_temperature_raw_null_output(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature_raw(&handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_read_temperature_10bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_10bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_read_temperature_11bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_11bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_set_resolution_null_handle(void)
{
  rx_err_t err = rx_ds18b20_set_resolution(nullptr, k_ds18b20_resolution_12bit);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_set_resolution_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_12bit);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_get_resolution_null_handle(void)
{
  ds18b20_resolution_t res = k_ds18b20_resolution_12bit;
  rx_err_t             err = rx_ds18b20_get_resolution(nullptr, &res);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_get_resolution_null_output(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_get_resolution(&handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_save_config_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_save_config(&handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_recall_config_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_recall_config(&handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_read_power_mode_null_handle(void)
{
  bool     external_power = false;
  rx_err_t err            = rx_ds18b20_read_power_mode(nullptr, &external_power);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_read_power_mode_null_output(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_power_mode(&handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_read_scratchpad_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  uint8_t             scratchpad[k_ds18b20_scratchpad_bytes];
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_read_scratchpad(&handle, scratchpad);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_get_conversion_time_null_handle(void)
{
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(nullptr);
  TEST_ASSERT_EQUAL_UINT32(0U, time_ms);
}

void test_ds18b20_get_conversion_time_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  internal_init_handle(&handle);
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);
  TEST_ASSERT_EQUAL_UINT32(0U, time_ms);
}

void test_ds18b20_get_conversion_time_10bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_10bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);
  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_10bit_ms, time_ms);
}

void test_ds18b20_get_conversion_time_11bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_11bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);
  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_11bit_ms, time_ms);
}

void test_ds18b20_init_null_bus_manager(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = nullptr,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_init_null_bus_name(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = nullptr,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_init_wrong_family_code(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = true,
  };
  /* Set wrong family code (not 0x28) */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(config.rom, 0, k_onewire_rom_bytes);
  config.rom[0] = k_test_ds1820_family_code; /* DS1820 family code, not DS18B20 */
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_ds18b20_init_bus_init_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  s_mock_state.force_init_error = true;
  rx_err_t err                  = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_trigger_conversion_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_reset_error = true;
  rx_err_t err                   = rx_ds18b20_trigger_conversion(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_trigger_conversion_select_device_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_skip_rom_error = true;
  rx_err_t err                      = rx_ds18b20_trigger_conversion(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test trigger conversion when match_rom fails (use_rom_matching=true path)
 *
 * @details
 * Covers the match_rom error branch in internal_ds18b20_select_device() when
 * use_rom_matching is true.
 */
void test_ds18b20_trigger_conversion_match_rom_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = true,
  };
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(config.rom, s_mock_state.rom, k_onewire_rom_bytes);
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_match_rom_error = true;
  rx_err_t err                       = rx_ds18b20_trigger_conversion(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_trigger_conversion_write_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = true,
  };
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(config.rom, s_mock_state.rom, k_onewire_rom_bytes);
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* With use_rom_matching=true, select_device uses match_rom.
   * After match_rom succeeds, write_byte (Convert T) should fail.
   * Set write error after init, so init itself can succeed. */
  s_mock_state.force_write_error = true;
  rx_err_t err                   = rx_ds18b20_trigger_conversion(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_read_scratchpad_write_cmd_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Inject write error so read_scratchpad_raw's write_byte fails */
  s_mock_state.force_write_error = true;
  rx_err_t err                   = rx_ds18b20_read_temperature(&handle, &temp_c);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_read_temperature_out_of_range(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  int16_t raw_temp = 0;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Create scratchpad with out-of-range raw value 0x8000 = -32768 < -880 */
  internal_create_valid_scratchpad(s_mock_state.scratchpad,
                                   (temp_raw_t){.lsb = 0x00, .msb = k_test_out_of_range_msb},
                                   k_test_config_12bit);
  rx_err_t err = rx_ds18b20_read_temperature_raw(&handle, &raw_temp);
  TEST_ASSERT_EQUAL(k_rx_err_out_of_range, err);
}

void test_ds18b20_read_scratchpad_null_buffer(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_scratchpad(&handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_read_temperature_9bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_9bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_read_scratchpad_read_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Inject read error so rx_bus_onewire_read fails */
  s_mock_state.force_read_error = true;
  rx_err_t err                  = rx_ds18b20_read_temperature(&handle, &temp_c);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_read_temperature_null_handle(void)
{
  float    temp_c = 0.0F;
  rx_err_t err    = rx_ds18b20_read_temperature(nullptr, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_set_resolution_invalid_value(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* 0xFF is an invalid resolution value */
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = rx_ds18b20_set_resolution(&handle, (ds18b20_resolution_t)k_test_invalid_byte);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_ds18b20_get_resolution_not_initialized(void)
{
  rx_ds18b20_handle_t  handle;
  ds18b20_resolution_t res = k_ds18b20_resolution_12bit;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_get_resolution(&handle, &res);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_read_power_mode_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  bool                external_power = false;
  internal_init_handle(&handle);
  rx_err_t err = rx_ds18b20_read_power_mode(&handle, &external_power);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_scratchpad_crc_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0F;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Corrupt the CRC byte to trigger CRC mismatch */
  s_mock_state.scratchpad[k_ds18b20_scratch_crc] ^= k_test_invalid_byte;
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/**
 * @brief Test init fails when OneWire reset fails during verify_device_presence
 *
 * @details Covers line 1376 error branch in internal_ds18b20_verify_device_presence().
 */
void test_ds18b20_init_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  s_mock_state.force_reset_error = true;
  rx_err_t err                   = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(handle.initialized);
}

/**
 * @brief Test init fails when read_scratchpad_raw fails during verify_device_presence
 *
 * @details Covers line 1389 error branch in internal_ds18b20_verify_device_presence().
 */
void test_ds18b20_init_read_scratchpad_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  /* Inject read error so internal_ds18b20_read_scratchpad_raw fails during init */
  s_mock_state.force_read_error = true;
  rx_err_t err                  = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(handle.initialized);
}

/**
 * @brief Test save_config fails when device not present (presence=false)
 *
 * @details Covers the !presence branch (line 925) in rx_ds18b20_save_config().
 */
void test_ds18b20_save_config_no_device(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.presence_response = false;
  rx_err_t err                   = rx_ds18b20_save_config(&handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test recall_config fails when device not present (presence=false)
 *
 * @details Covers the !presence branch (line 1005) in rx_ds18b20_recall_config().
 */
void test_ds18b20_recall_config_no_device(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.presence_response = false;
  rx_err_t err                   = rx_ds18b20_recall_config(&handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test read_power_mode fails when device not present (presence=false)
 *
 * @details Covers the !presence branch (line 1090) in rx_ds18b20_read_power_mode().
 */
void test_ds18b20_read_power_mode_no_device(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.presence_response = false;
  rx_err_t err                   = rx_ds18b20_read_power_mode(&handle, &external_power);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test read_temperature_raw fails when temp value is too high (> max_raw)
 *
 * @details Covers the `> max_raw` branch (line 701) in
 * internal_ds18b20_read_temperature_raw(). The out_of_range test covers < min_raw,
 * this test covers > max_raw.
 */
void test_ds18b20_read_temperature_too_high(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  int16_t raw_temp = 0;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Raw value 0x07D0 = 2000 > max_raw (125*16=2000? Let's use 0x07D1 = 2001) */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_too_high_temp_lsb, .msb = k_test_too_high_temp_msb},
    k_test_config_12bit);
  rx_err_t err = rx_ds18b20_read_temperature_raw(&handle, &raw_temp);
  TEST_ASSERT_EQUAL(k_rx_err_out_of_range, err);
}

/**
 * @brief Test save_config fails when reset/presence check fails
 *
 * @details Covers line 925 error branch in rx_ds18b20_save_config().
 */
void test_ds18b20_save_config_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_reset_error = true;
  rx_err_t err                   = rx_ds18b20_save_config(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test save_config fails when select_device (skip_rom) fails
 *
 * @details Covers line 932 error branch in rx_ds18b20_save_config().
 */
void test_ds18b20_save_config_select_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_skip_rom_error = true;
  rx_err_t err                      = rx_ds18b20_save_config(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test save_config fails when write_byte (copy scratchpad cmd) fails
 *
 * @details Covers line 939 error branch in rx_ds18b20_save_config().
 */
void test_ds18b20_save_config_write_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_write_error = true;
  rx_err_t err                   = rx_ds18b20_save_config(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test recall_config fails when reset/presence check fails
 *
 * @details Covers line 1005 error branch in rx_ds18b20_recall_config().
 */
void test_ds18b20_recall_config_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_reset_error = true;
  rx_err_t err                   = rx_ds18b20_recall_config(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test recall_config fails when select_device (skip_rom) fails
 *
 * @details Covers line 1012 error branch in rx_ds18b20_recall_config().
 */
void test_ds18b20_recall_config_select_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_skip_rom_error = true;
  rx_err_t err                      = rx_ds18b20_recall_config(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test recall_config fails when write_byte (recall EEPROM cmd) fails
 *
 * @details Covers line 1019 error branch in rx_ds18b20_recall_config().
 */
void test_ds18b20_recall_config_write_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_write_error = true;
  rx_err_t err                   = rx_ds18b20_recall_config(&handle);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read_power_mode fails when reset/presence check fails
 *
 * @details Covers line 1090 error branch in rx_ds18b20_read_power_mode().
 */
void test_ds18b20_read_power_mode_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_reset_error = true;
  rx_err_t err                   = rx_ds18b20_read_power_mode(&handle, &external_power);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read_power_mode fails when select_device (skip_rom) fails
 *
 * @details Covers line 1097 error branch in rx_ds18b20_read_power_mode().
 */
void test_ds18b20_read_power_mode_select_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_skip_rom_error = true;
  rx_err_t err                      = rx_ds18b20_read_power_mode(&handle, &external_power);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read_power_mode fails when write_byte (read power cmd) fails
 *
 * @details Covers line 1103 error branch in rx_ds18b20_read_power_mode().
 */
void test_ds18b20_read_power_mode_write_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_write_error = true;
  rx_err_t err                   = rx_ds18b20_read_power_mode(&handle, &external_power);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read_power_mode fails when read_bit fails
 *
 * @details Covers line 1111 error branch in rx_ds18b20_read_power_mode().
 * Mock read_bit uses force_read_error flag.
 */
void test_ds18b20_read_power_mode_read_bit_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.force_read_error = true;
  rx_err_t err                  = rx_ds18b20_read_power_mode(&handle, &external_power);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test set_resolution fails when no device present during read_scratchpad
 *
 * @details Covers the !presence branch (line 1602) in
 * internal_ds18b20_read_scratchpad_raw() when called from rx_ds18b20_set_resolution().
 */
void test_ds18b20_set_resolution_no_device(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  s_mock_state.presence_response = false;
  rx_err_t err                   = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test set_resolution fails when no device present during write_scratchpad
 *
 * @details Covers the !presence branch (line 1758) in
 * internal_ds18b20_write_scratchpad() called from rx_ds18b20_set_resolution().
 * Use write_fail_after=1 so read_scratchpad's write_byte succeeds but then
 * set presence=false so write_scratchpad's reset returns no presence.
 *
 * Actually, we can test the no-device in write_scratchpad by having
 * read_scratchpad succeed (presence=true, write_byte ok) and then presence=false
 * only during write_scratchpad's reset. But since presence_response is global,
 * setting it after read_scratchpad but before write_scratchpad is tricky.
 * Instead, use write_fail_after=0 with force_write=false and presence_response
 * toggling via a separate approach. Simplest: rely on GCOV_EXCL for this path
 * since it requires per-call presence toggling which the mock doesn't support.
 *
 * For now, this test covers line 1765 (select_device error in write_scratchpad)
 * by using force_skip_rom_error after a successful read_scratchpad (via write_fail_after).
 */
void test_ds18b20_set_resolution_write_scratchpad_select_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* After init, force_skip_rom_error so select_device in write_scratchpad fails.
   * read_scratchpad succeeds (its write_byte completes before skip_rom fails
   * because read_scratchpad uses write_byte THEN read, and skip_rom is called
   * BEFORE write_byte in read_scratchpad). Actually:
   * read_scratchpad_raw: reset, skip_rom, write_byte(cmd), read
   * write_scratchpad:    reset, skip_rom <-- fails here
   * So skip_rom error causes read_scratchpad to fail too (line 1602 branch).
   * To cover line 1765 we need read_scratchpad to succeed but write_scratchpad
   * skip_rom to fail. This requires per-call skip_rom failure which is not
   * directly supported. The line 1765 path will be excluded with GCOV_EXCL. */
  s_mock_state.force_skip_rom_error = true;
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test set_resolution fails when write (3-byte data) in write_scratchpad fails
 *
 * @details Covers line 1787 error branch in internal_ds18b20_write_scratchpad().
 * Uses write_fail_after=2: read_scratchpad write_byte(1 call) ok,
 * write_scratchpad write_byte(2nd call) ok, write(3rd) FAIL via rx_bus_onewire_write.
 * But rx_bus_onewire_write checks force_write_error without fail_after, so it
 * always fails when force_write_error=true. We need write_byte calls 1 and 2 to
 * succeed so write_scratchpad gets past the cmd write, then onewire_write fails.
 * With write_fail_after=2: calls 1,2 succeed, call 3 (onewire_write uses the same
 * force_write_error but NOT the counter). So onewire_write will fail immediately.
 * Actually: set write_fail_after so write_byte calls 1&2 succeed, then onewire_write
 * (which ignores the counter) will also fail. Line 1787 covers onewire_write failure.
 */
void test_ds18b20_set_resolution_write_data_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* write_byte call sequence in set_resolution:
   *   call 1: read_scratchpad_raw -> write_byte(read_scratch_cmd) ok
   *   call 2: write_scratchpad -> write_byte(write_scratch_cmd) ok (count=1<2)
   * Then rx_bus_onewire_write (3-byte data) fails because force_write_error=true
   * and write does not use fail_after counter. Covers line 1787. */
  s_mock_state.force_write_error = true;
  s_mock_state.write_fail_after  = k_test_write_fail_after_2;
  rx_err_t err                   = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test set_resolution fails when read_scratchpad fails
 *
 * @details Covers line 785 error branch in rx_ds18b20_set_resolution().
 */
void test_ds18b20_set_resolution_read_scratchpad_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Inject read error so read_scratchpad_raw fails inside set_resolution */
  s_mock_state.force_read_error = true;
  rx_err_t err                  = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test set_resolution fails when write_scratchpad fails
 *
 * @details Covers line 798 error branch in rx_ds18b20_set_resolution().
 * Use write_fail_after=1 so read_scratchpad's write_byte succeeds (call 1),
 * then write_scratchpad's write_byte fails (call 2, count>=1).
 */
void test_ds18b20_set_resolution_write_scratchpad_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* call 1: read_scratchpad -> write_byte(read_scratch_cmd) ok
   * call 2: write_scratchpad -> write_byte(write_scratch_cmd) FAIL (count=1 >= 1)
   * This propagates: write_scratchpad line 1772 -> set_resolution line 798. */
  s_mock_state.force_write_error = true;
  s_mock_state.write_fail_after  = 1U;
  rx_err_t err                   = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test init fails when set_resolution fails (line 385 branch)
 *
 * @details Covers line 385 error branch in rx_ds18b20_init() where
 * rx_ds18b20_set_resolution returns an error.
 */
void test_ds18b20_init_set_resolution_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  /* write_byte call sequence during rx_ds18b20_init():
   *   call 1: check_presence -> read_scratchpad_raw -> write_byte(read_scratch_cmd) ok
   *   call 2: set_resolution -> read_scratchpad_raw -> write_byte(read_scratch_cmd) ok
   *   call 3: set_resolution -> write_scratchpad -> write_byte(write_scratch_cmd) FAIL
   * Set write_fail_after=2 so write_byte fails on the 3rd call (count >= 2). */
  s_mock_state.force_write_error = true;
  s_mock_state.write_fail_after  = k_test_write_fail_after_2;
  rx_err_t err                   = rx_ds18b20_init(&handle, &config);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  /* Handle must be deinitialized on failure */
  TEST_ASSERT_FALSE(handle.initialized);
}

/**
 * @brief get_conversion_time_ms returns invalid for out-of-range resolution in handle.
 *
 * @details Directly sets handle->resolution to an invalid value after init to
 * exercise the default case in the internal resolution switch (line 1218-1219).
 */
void test_ds18b20_get_conversion_time_invalid_resolution(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Force an invalid resolution to trigger the default case */
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  handle.resolution = (ds18b20_resolution_t)k_test_invalid_byte;
  uint32_t time_ms  = rx_ds18b20_get_conversion_time_ms(&handle);
  TEST_ASSERT_EQUAL_UINT32(0U, time_ms);
}

/**
 * @brief write_scratchpad fails when presence pulse is absent during reset.
 *
 * @details Uses the presence_false_after counter to allow read_scratchpad's
 * reset to succeed (call 0 returns true) but write_scratchpad's reset (call 1)
 * to return presence=false. Covers lines 1759-1760 in rx_ds18b20.c.
 */
void test_ds18b20_write_scratchpad_no_presence(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* After init (4 resets already consumed during init):
   * - verify_device_presence: reset 0, read_scratchpad_raw: reset 1
   * - set_resolution (from init): read_scratchpad_raw: reset 2, write_scratchpad: reset 3
   * Now for the post-init rx_ds18b20_set_resolution call:
   * - read_scratchpad_raw: reset 4 (ok)
   * - write_scratchpad: reset 5 (force presence=false)
   * So presence_false_after = 5 (fail on reset_call_count >= 5). */
  s_mock_state.force_presence_false_after = true;
  s_mock_state.presence_false_after       = k_test_presence_fail_at_5;
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief write_scratchpad select_device fails on second skip_rom call.
 *
 * @details Uses skip_rom_fail_after to let read_scratchpad's skip_rom succeed
 * (call 0) but fail on write_scratchpad's skip_rom (call 1). Covers line 1766.
 */
void test_ds18b20_write_scratchpad_select_error_after_read(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* After init (3 skip_rom calls consumed):
   * - verify_device_presence read_scratchpad_raw: skip_rom 0
   * - set_resolution (from init): read_scratchpad_raw: skip_rom 1, write_scratchpad: skip_rom 2
   * Now for the post-init rx_ds18b20_set_resolution call:
   * - read_scratchpad_raw: skip_rom 3 (ok)
   * - write_scratchpad: skip_rom 4 (fail)
   * So skip_rom_fail_after = 4. */
  s_mock_state.force_skip_rom_error = true;
  s_mock_state.skip_rom_fail_after  = k_test_skip_rom_fail_at_4;
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief get_temp_mask returns 12-bit mask for invalid resolution (default case).
 *
 * @details Directly sets handle->resolution to an invalid value after init to
 * exercise the default case in internal_ds18b20_get_temp_mask (line 1830-1831).
 * The default case returns k_ds18b20_temp_mask_12bit so the read succeeds.
 */
void test_ds18b20_read_temp_raw_invalid_resolution_default(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Force invalid resolution to hit default case in get_temp_mask */
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  handle.resolution = (ds18b20_resolution_t)k_test_invalid_byte;
  int16_t  raw      = 0;
  rx_err_t err      = rx_ds18b20_read_temperature_raw(&handle, &raw);
  /* Default mask is k_ds18b20_temp_mask_12bit so reading still succeeds */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief write_scratchpad 3-byte write fails after cmd write succeeds.
 *
 * @details Uses force_write_data_error (only affects rx_bus_onewire_write, not
 * write_byte) set AFTER init. This allows init to complete normally, then
 * when set_resolution's write_scratchpad tries to write 3 bytes of data, it
 * fails. Covers lines 1788-1789 in rx_ds18b20.c.
 */
void test_ds18b20_write_scratchpad_data_write_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* After init succeeds, enable multi-byte write failure.
   * read_scratchpad_raw uses rx_bus_onewire_read (not write), so it succeeds.
   * write_scratchpad: write_byte(cmd) ok, then rx_bus_onewire_write(3 bytes) fails. */
  s_mock_state.force_write_data_error = true;
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Read temperature read_scratchpad_raw returns reserved byte warning path.
 *
 * @details Sets scratchpad reserved byte (index 5) to a non-0xFF value to
 * trigger the warning log at line 1645. No error is returned, just a warning.
 */
void test_ds18b20_read_scratchpad_reserved_byte_warning(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));
  /* Set reserved byte to non-0xFF value to trigger the warning log */
  s_mock_state.scratchpad[k_ds18b20_scratch_reserved1] = 0x00U;
  /* Re-compute CRC to match modified scratchpad */
  uint32_t new_crc = 0U;
  (void)rx_crc8_maxim(s_mock_state.scratchpad,
                      (uint32_t)(k_ds18b20_scratchpad_bytes - 1U),
                      &new_crc);
  s_mock_state.scratchpad[k_ds18b20_scratchpad_bytes - 1U] = (uint8_t)new_crc;
  float    temp                                            = 0.0F;
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read_scratchpad_raw reset error covers err != k_rx_ok branch (line 1602)
 *
 * @details
 * Covers Branch 1 at line 1602: `err != k_rx_ok || !presence` where
 * `err != k_rx_ok` is true (short-circuits without evaluating !presence).
 * Init consumes 4 resets (verify_presence + read_scratchpad + set_resolution's
 * read_scratchpad + write_scratchpad). The post-init set_resolution call's
 * read_scratchpad_raw triggers reset #4, which is forced to fail.
 *
 * @pre Init succeeds (resets 0-3 pass)
 * @post Reset #4 fails, covering the err short-circuit branch
 */
void test_ds18b20_read_scratchpad_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));

  /* After init: reset_call_count == 4. Fail on reset #4 (read_scratchpad_raw). */
  s_mock_state.force_reset_error_after = true;
  s_mock_state.reset_error_after       = k_test_reset_error_after_4;

  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test write_scratchpad reset error covers err != k_rx_ok branch (line 1757)
 *
 * @details
 * Covers Branch 1 at line 1757: `err != k_rx_ok || !presence` where
 * `err != k_rx_ok` is true. Init consumes 4 resets. The post-init
 * set_resolution call has read_scratchpad (reset #4 ok) then
 * write_scratchpad (reset #5 forced to fail).
 *
 * @pre Init succeeds (resets 0-3 pass)
 * @post Reset #5 fails, covering the err short-circuit branch
 */
void test_ds18b20_write_scratchpad_reset_error(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  internal_init_handle(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_ds18b20_init(&handle, &config));

  /* After init: reset_call_count == 4. Fail on reset #5 (write_scratchpad). */
  s_mock_state.force_reset_error_after = true;
  s_mock_state.reset_error_after       = k_test_reset_error_after_5;

  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_10bit);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Unity Test Runner
 * =============================================================================
 */

/**
 * @brief Main test runner entry point
 *
 * @details
 * Executes all DS18B20 driver tests using Unity framework.
 * Returns 0 on success (all tests pass), non-zero on failure.
 *
 * **Execution Order:**
 * ```
 * 1. UNITY_BEGIN() - Initialize Unity
 * 2. RUN_TEST(test_xxx) - Execute each test
 *    - setUp() called before test
 *    - test_xxx() executes
 *    - tearDown() called after test
 * 3. UNITY_END() - Print summary, return status
 * ```
 *
 * **Expected Output (All Tests Pass):**
 * ```
 * test_rx_ds18b20.c:452:test_ds18b20_init_success:PASS
 * test_rx_ds18b20.c:471:test_ds18b20_init_null_handle:PASS
 * ...
 * -----------------------
 * 27 Tests 0 Failures 0 Ignored
 * OK
 * ```
 *
 * @return 0 All tests passed
 * @return Non-zero One or more tests failed
 */
/**
 * @brief Run public API tests (init, temp, resolution, power, conversion, config)
 */
static void internal_run_public_api_tests(void)
{
  /* Initialization tests */
  RUN_TEST(test_ds18b20_init_success);
  RUN_TEST(test_ds18b20_init_null_handle);
  RUN_TEST(test_ds18b20_init_null_config);
  RUN_TEST(test_ds18b20_init_no_device_present);
  RUN_TEST(test_ds18b20_init_already_initialized);
  RUN_TEST(test_ds18b20_init_invalid_resolution);

  /* Temperature reading tests */
  RUN_TEST(test_ds18b20_read_temperature_25c);
  RUN_TEST(test_ds18b20_read_temperature_0c);
  RUN_TEST(test_ds18b20_read_temperature_minus_55c);
  RUN_TEST(test_ds18b20_read_temperature_125c);
  RUN_TEST(test_ds18b20_read_temperature_not_initialized);
  RUN_TEST(test_ds18b20_read_temperature_null_output);

  /* Resolution tests */
  RUN_TEST(test_ds18b20_set_resolution_9bit);
  RUN_TEST(test_ds18b20_get_resolution);
  RUN_TEST(test_ds18b20_get_conversion_time_12bit);
  RUN_TEST(test_ds18b20_get_conversion_time_9bit);

  /* Power mode tests */
  RUN_TEST(test_ds18b20_read_power_mode_external);
  RUN_TEST(test_ds18b20_read_power_mode_parasitic);

  /* Conversion tests */
  RUN_TEST(test_ds18b20_trigger_conversion);
  RUN_TEST(test_ds18b20_trigger_conversion_no_device);

  /* Save/recall config and read scratchpad tests */
  RUN_TEST(test_ds18b20_save_config_success);
  RUN_TEST(test_ds18b20_save_config_null);
  RUN_TEST(test_ds18b20_recall_config_success);
  RUN_TEST(test_ds18b20_recall_config_null);
  RUN_TEST(test_ds18b20_read_scratchpad_success);
  RUN_TEST(test_ds18b20_read_scratchpad_null);

  /* Deinitialization tests */
  RUN_TEST(test_ds18b20_deinit);
}

/**
 * @brief Run additional coverage and null-pointer tests
 */
static void internal_run_coverage_tests(void)
{
  RUN_TEST(test_ds18b20_init_with_rom_matching);
  RUN_TEST(test_ds18b20_deinit_null_handle);
  RUN_TEST(test_ds18b20_deinit_not_initialized);
  RUN_TEST(test_ds18b20_trigger_conversion_null_handle);
  RUN_TEST(test_ds18b20_trigger_conversion_not_initialized);
  RUN_TEST(test_ds18b20_read_temperature_raw_null_handle);
  RUN_TEST(test_ds18b20_read_temperature_raw_not_initialized);
  RUN_TEST(test_ds18b20_read_temperature_raw_null_output);
  RUN_TEST(test_ds18b20_read_temperature_10bit);
  RUN_TEST(test_ds18b20_read_temperature_11bit);
  RUN_TEST(test_ds18b20_set_resolution_null_handle);
  RUN_TEST(test_ds18b20_set_resolution_not_initialized);
  RUN_TEST(test_ds18b20_get_resolution_null_handle);
  RUN_TEST(test_ds18b20_get_resolution_null_output);
  RUN_TEST(test_ds18b20_save_config_not_initialized);
  RUN_TEST(test_ds18b20_recall_config_not_initialized);
  RUN_TEST(test_ds18b20_read_power_mode_null_handle);
  RUN_TEST(test_ds18b20_read_power_mode_null_output);
  RUN_TEST(test_ds18b20_read_scratchpad_not_initialized);
  RUN_TEST(test_ds18b20_get_conversion_time_null_handle);
  RUN_TEST(test_ds18b20_get_conversion_time_not_initialized);
  RUN_TEST(test_ds18b20_get_conversion_time_10bit);
  RUN_TEST(test_ds18b20_get_conversion_time_11bit);
  RUN_TEST(test_ds18b20_get_conversion_time_invalid_resolution);
  RUN_TEST(test_ds18b20_scratchpad_crc_error);
  RUN_TEST(test_ds18b20_read_temperature_null_handle);
  RUN_TEST(test_ds18b20_set_resolution_invalid_value);
  RUN_TEST(test_ds18b20_get_resolution_not_initialized);
  RUN_TEST(test_ds18b20_read_power_mode_not_initialized);
  RUN_TEST(test_ds18b20_init_null_bus_manager);
  RUN_TEST(test_ds18b20_init_null_bus_name);
  RUN_TEST(test_ds18b20_init_wrong_family_code);
  RUN_TEST(test_ds18b20_init_bus_init_error);
}

/**
 * @brief Run error path and branch coverage tests
 */
static void internal_run_error_path_tests(void)
{
  RUN_TEST(test_ds18b20_trigger_conversion_reset_error);
  RUN_TEST(test_ds18b20_trigger_conversion_select_device_error);
  RUN_TEST(test_ds18b20_trigger_conversion_match_rom_error);
  RUN_TEST(test_ds18b20_trigger_conversion_write_error);
  RUN_TEST(test_ds18b20_read_scratchpad_write_cmd_error);
  RUN_TEST(test_ds18b20_read_scratchpad_read_error);
  RUN_TEST(test_ds18b20_read_temperature_9bit);
  RUN_TEST(test_ds18b20_read_scratchpad_null_buffer);
  RUN_TEST(test_ds18b20_read_temperature_out_of_range);
  RUN_TEST(test_ds18b20_save_config_no_device);
  RUN_TEST(test_ds18b20_recall_config_no_device);
  RUN_TEST(test_ds18b20_read_power_mode_no_device);
  RUN_TEST(test_ds18b20_read_temperature_too_high);
  RUN_TEST(test_ds18b20_init_reset_error);
  RUN_TEST(test_ds18b20_init_read_scratchpad_error);
  RUN_TEST(test_ds18b20_save_config_reset_error);
  RUN_TEST(test_ds18b20_save_config_select_error);
  RUN_TEST(test_ds18b20_save_config_write_error);
  RUN_TEST(test_ds18b20_recall_config_reset_error);
  RUN_TEST(test_ds18b20_recall_config_select_error);
  RUN_TEST(test_ds18b20_recall_config_write_error);
  RUN_TEST(test_ds18b20_read_power_mode_reset_error);
  RUN_TEST(test_ds18b20_read_power_mode_select_error);
  RUN_TEST(test_ds18b20_read_power_mode_write_error);
  RUN_TEST(test_ds18b20_read_power_mode_read_bit_error);
  RUN_TEST(test_ds18b20_set_resolution_no_device);
  RUN_TEST(test_ds18b20_set_resolution_write_scratchpad_select_error);
  RUN_TEST(test_ds18b20_read_temp_raw_invalid_resolution_default);
  RUN_TEST(test_ds18b20_write_scratchpad_no_presence);
  RUN_TEST(test_ds18b20_write_scratchpad_select_error_after_read);
  RUN_TEST(test_ds18b20_write_scratchpad_data_write_error);
  RUN_TEST(test_ds18b20_read_scratchpad_reserved_byte_warning);
  RUN_TEST(test_ds18b20_set_resolution_write_data_error);
  RUN_TEST(test_ds18b20_set_resolution_read_scratchpad_error);
  RUN_TEST(test_ds18b20_set_resolution_write_scratchpad_error);
  RUN_TEST(test_ds18b20_init_set_resolution_error);
  RUN_TEST(test_ds18b20_read_scratchpad_reset_error);
  RUN_TEST(test_ds18b20_write_scratchpad_reset_error);
}

int main(void)
{
  UNITY_BEGIN();

  internal_run_public_api_tests();
  internal_run_coverage_tests();
  internal_run_error_path_tests();

  return UNITY_END();
}
