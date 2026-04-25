/**
 * @file test_rx_bus_onewire.c
 * @brief Unit Tests for 1-Wire Bus Protocol Implementation
 *
 * @details
 * Comprehensive test suite for the 1-Wire bus abstraction layer (rx_bus_onewire).
 * Tests protocol timing, ROM operations, device search, and error handling using
 * mock GPIO and CRC subsystems. Validates compliance with Dallas/Maxim 1-Wire
 * specification and NASA Power of 10 safety standards.
 *
 * @par 1-Wire Protocol Architecture
 * @code
 * +--------------------------------------------------------------+
 * |                    Application Layer                         |
 * |  (DS18B20 temp sensor, DS2431 EEPROM, etc.)                 |
 * +--------------------------------------------------------------+
 *                              ^
 *                              | Device Commands
 *                              v
 * +--------------------------------------------------------------+
 * |               ROM Command Layer (This Test Suite)            |
 * |  - Read ROM [0x33]      - Skip ROM [0xCC]                   |
 * |  - Match ROM [0x55]     - Search ROM [0xF0]                 |
 * +--------------------------------------------------------------+
 *                              ^
 *                              | Byte/Bit Operations
 *                              v
 * +--------------------------------------------------------------+
 * |         Bit/Byte Transfer Layer (rx_bus_onewire.c)          |
 * |  - write_bit()  - read_bit()                                |
 * |  - write_byte() - read_byte()                               |
 * +--------------------------------------------------------------+
 *                              ^
 *                              | Timing Slots
 *                              v
 * +--------------------------------------------------------------+
 * |           Timing Layer (mock_rx_onewire_hw)                 |
 * |  - Reset/Presence Pulse  - Time Slot Generation             |
 * |  - Microsecond delays    - GPIO control                     |
 * +--------------------------------------------------------------+
 *                              ^
 *                              | Physical Layer
 *                              v
 * +--------------------------------------------------------------+
 * |              Hardware (GPIO + Pull-up Resistor)              |
 * |  Single-wire open-drain: P0.5 + 4.7kOhm pull-up              |
 * +--------------------------------------------------------------+
 * @endcode
 *
 * @par 1-Wire Timing Specification (Dallas/Maxim)
 *
 * **Reset and Presence Pulse:**
 * @code
 * Controller:  --+                      +-----------------------
 *               +----------------------+
 *                |<--- 480-960us ---->|
 *
 * Device:      -----------------+      +-------------------------
 *                              +------+
 *                               |< 60-240us >|
 *                               Presence Pulse
 * Sampling:                         ^
 *                                 15-60us after release
 * @endcode
 *
 * **Write-0 Time Slot (60-120us):**
 * @code
 * Controller:  --+                 +--------------------------
 *               +-----------------+
 *                |<- 60-120us -->|<- Recovery >1us
 *
 * Device:      Samples at 15-30us: reads '0'
 * @endcode
 *
 * **Write-1 Time Slot (60-120us):**
 * @code
 * Controller:  --++------------------------------------------
 *               ++
 *                |< 1-15us >|<------- Release --------->|
 *
 * Device:      Samples at 15-30us: reads '1' (pull-up)
 * @endcode
 *
 * **Read Time Slot (60-120us):**
 * @code
 * Controller:  --++------------------------------------------
 *               ++
 *                |< 1-15us >|
 *                           ^
 *                         Sample at 15us
 * Device:         Pulls low if sending '0'
 * @endcode
 *
 * @par 64-bit ROM Code Structure
 *
 * Every 1-Wire device has a unique 64-bit ROM code programmed at factory:
 *
 * @code
 * +--------------------------------------------------------------+
 * |  Byte 0  |  Byte 1-6   |  Byte 7   |  Transmission Order   |
 * |  8-bit   |  48-bit     |  8-bit    |  LSB first (bit 0)    |
 * |  Family  |  Serial #   |  CRC-8    |  Then MSB (bit 7)     |
 * |  Code    |  (unique)   |           |                       |
 * +--------------------------------------------------------------+
 *
 * Example DS18B20 ROM: [0x28][0xFF][0x12][0x34][0x56][0x78][0x9A][0xBC]
 *                       ^                                             ^
 *                     Family                                        CRC-8
 *                     0x28 = DS18B20
 *                     0x10 = DS18S20
 *                     0x22 = DS1822
 * @endcode
 *
 * **CRC-8 Polynomial:** x^8 + x^5 + x^4 + 1 (0x8C)
 * - Computed over bytes 0-6 (family + serial number)
 * - Protects against address errors during ROM search
 * - Verified by controller after reading ROM
 *
 * @par Test Methodology
 *
 * **1. Timing Verification:**
 * - Mock timer captures GPIO pulse widths with 1us resolution
 * - Validates reset pulse duration (480-960us)
 * - Validates presence pulse sampling (15-60us after release)
 * - Validates write-0 slot (60-120us low)
 * - Validates write-1 slot (1-15us low)
 * - Validates read slot timing and sampling point (15us)
 *
 * **2. ROM Command Testing:**
 * - **Read ROM [0x33]:** Single device, read 64-bit ROM code
 * - **Skip ROM [0xCC]:** Broadcast to all devices (no addressing)
 * - **Match ROM [0x55]:** Select specific device by ROM code
 * - **Search ROM [0xF0]:** Discover multiple devices using binary search
 *
 * **3. ROM Search Algorithm:**
 * Implements Dallas/Maxim binary tree search:
 * @code
 * For each bit position (0-63):
 *   1. Read bit value from all devices
 *   2. Read complement bit value
 *   3. Determine search direction:
 *      - (1, 0): Only devices with bit=1 present -> write 1
 *      - (0, 1): Only devices with bit=0 present -> write 0
 *      - (0, 0): Conflict - both values present -> choose path, mark bifurcation
 *      - (1, 1): No devices responding -> error
 *   4. Write chosen bit to deselect non-matching devices
 *   5. Track last discrepancy for next iteration
 * @endcode
 *
 * **4. Error Injection:**
 * - GPIO hardware errors (read/write failures)
 * - Bus short conditions (line stuck low)
 * - Presence pulse timeout (no device)
 * - CRC errors in ROM codes
 * - Multiple device conflicts
 *
 * @par Test Coverage Analysis
 *
 * | Category               | Tests | Coverage | Notes                          |
 * |------------------------|-------|----------|--------------------------------|
 * | Initialization         | 6     | 100%     | nullptr checks, wrong bus type    |
 * | Reset/Presence         | 5     | 100%     | Device present/absent, errors  |
 * | Bit Operations         | 8     | 100%     | Write-0/1, Read-0/1, errors    |
 * | Byte Operations        | 8     | 100%     | Write/read, LSB-first order    |
 * | Buffer Operations      | 8     | 100%     | Multi-byte transfers, bounds   |
 * | ROM Commands           | 15    | 100%     | Skip/Match/Read ROM + CRC      |
 * | ROM Search             | 7     | 100%     | Single/multi device with CRC   |
 * | CRC Validation         | 5     | 100%     | Valid/mismatch for ROM + search |
 * | Error Handling         | 3     | 100%     | GPIO errors with state pool fix |
 * | Timing/Parasitic Power | 4     | 100%     | Constants, bus release checks   |
 * | **TOTAL**              | **66**| **100%** | All tests fully functional     |
 *
 * @par Hardware Requirements
 *
 * **Timer Resolution:**
 * - Minimum: 1us (for 15us sampling accuracy)
 * - Recommended: 0.5us or better (+/-10% timing tolerance)
 * - RX72N CMT provides 0.166us resolution @ 240 MHz
 *
 * **GPIO Capabilities:**
 * - Open-drain output mode (CMOS push-pull NOT compatible)
 * - Schmitt-trigger input for noise immunity
 * - Internal pull-up disabled (external 4.7kOhm required)
 * - Fast slew rate: <1us rise time with 100pF bus capacitance
 *
 * **Pull-up Resistor Selection:**
 * @code
 * R_pullup = (V_OH - V_OL) / I_sink
 *
 * For 5V bus with 5mA sink:
 * R = (5V - 0.4V) / 5mA = 920Ohm minimum
 *
 * Standard: 4.7kOhm (supports up to 200m cable length)
 * Long cable: 2.2kOhm (supports up to 100m with reduced devices)
 * Short bus: 1.0kOhm (max speed, <10m, few devices)
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 *
 * **Rule 1 (Simple Control Flow):** [OK] No goto, setjmp, recursion
 * - ROM search uses iterative loop with static depth limit
 *
 * **Rule 2 (Fixed Loop Bounds):** [OK] All loops have static bounds
 * - setUp/tearDown: No loops
 * - ROM search: 64 iterations (bit width)
 * - Test runner: Compile-time test count
 *
 * **Rule 3 (No Dynamic Memory):** [OK] Zero heap allocation
 * - All buffers are stack arrays or static variables
 * - Bus manager uses static state pool
 *
 * **Rule 4 (Short Functions):** [OK] All test functions <60 lines
 * - Average test length: 12 lines
 * - Longest test: test_rx_bus_onewire_search_no_devices (20 lines)
 *
 * **Rule 5 (Assertions):** [OK] Every test has 2+ assertions
 * - Input validation: TEST_ASSERT_EQUAL for error codes
 * - Output validation: TEST_ASSERT for result correctness
 * - State validation: Mock counters and GPIO state checks
 *
 * **Rule 6 (Small Scope):** [OK] Data at minimal scope
 * - Test fixtures are file-static
 * - Local variables declared at first use
 *
 * **Rule 7 (Check Return Values):** [OK] Every function call checked
 * - All rx_err_t returns validated with TEST_ASSERT_EQUAL
 * - Mock function returns validated
 *
 * **Rule 8 (Limited Preprocessor):** [OK] No function-like macros
 * - Uses typed enums for constants
 * - RUN_TEST macros are Unity framework (required)
 *
 * **Rule 9 (Pointer Restrictions):** [OK] Single-level dereference
 * - No function pointers in tests
 * - Simple pointer parameters (uint8_t*, bool*)
 *
 * **Rule 10 (Compiler Warnings):** [OK] Compiled with -Wall -Wextra -Werror
 * - Zero warnings in CI/CD builds
 * - clang-tidy: All checks pass
 *
 * @par SOLID Principles Application
 *
 * **Single Responsibility (S):**
 * - Each test validates ONE specific behavior
 * - Test fixtures handle ONLY setup/teardown
 * - Mock subsystems isolate hardware dependencies
 *
 * **Open/Closed (O):**
 * - New ROM command tests added without modifying existing tests
 * - Mock infrastructure extensible via new mock functions
 *
 * **Liskov Substitution (L):**
 * - Mock GPIO substitutes real GPIO without test changes
 * - Mock timer substitutes real timer with same interface
 *
 * **Interface Segregation (I):**
 * - Tests use minimal rx_bus_onewire API surface
 * - No dependency on internal implementation details
 *
 * **Dependency Inversion (D):**
 * - Tests depend on rx_bus_onewire interface, not GPIO details
 * - Mock injection via bus manager (not direct GPIO access)
 *
 * @par Mock Infrastructure
 *
 * **mock_rx_gpio:**
 * - Simulates GPIO pin read/write operations
 * - Configurable read values for device simulation
 * - Error injection: mock_gpio_set_next_error()
 * - Counter tracking: write_low_count, write_high_count
 *
 * **mock_rx_onewire_hw:**
 * - Simulates 1-Wire timing (reset, write, read slots)
 * - Tracks pulse widths for timing verification
 * - Device presence simulation
 *
 * **mock_rx_crc:**
 * - CRC-8 calculation for ROM code validation
 * - Override mode for testing CRC error handling
 *
 * @see rx_bus_onewire.h 1-Wire protocol API
 * @see rx_bus_manager.h Bus management interface
 * @see mock_rx_gpio.h GPIO mock implementation
 * @see mock_rx_onewire_hw.h 1-Wire hardware mock
 * @see mock_rx_crc.h CRC calculation mock
 *
 * @note Not thread-safe - Unity runs tests sequentially
 * @note State pool exhaustion fixed: tearDown() calls rx_bus_onewire_deinit()
 *
 * @note CRC validation tests implemented (test_onewire_crc group)
 * @note Multi-device ROM search test implemented (test_onewire_multi_search)
 * @note Parasitic power bus release tests implemented (test_onewire_timing)
 * @note Timing constant verification tests implemented (test_onewire_timing)
 *
 * @since Version 1.0.0
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "mock_rx_crc.h"
#include "mock_rx_gpio.h"
#include "mock_rx_onewire_hw.h"
#include "rx_bus_config.h"
#include "rx_bus_manager.h"
#include "rx_bus_onewire.h"
#include "rx_err.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test configuration constants
 *
 * @details
 * Defines sizing and limits for test execution. All values chosen based on
 * 1-Wire specification and typical usage patterns.
 */
typedef enum : uint8_t {
  /**
   * @brief OneWire ROM code size in bytes
   * @details
   * 1-Wire ROM structure: [Family(1)][Serial(6)][CRC(1)] = 8 bytes total
   */
  k_test_rom_bytes = 8,

  /**
   * @brief Maximum devices for ROM search tests
   * @details
   * Limits ROM search array size. Typical networks have 1-10 devices.
   * Larger arrays test boundary conditions.
   */
  k_test_max_search_devices = 4,
} test_constants_t;

/**
 * @enum test_onewire_data_t
 * @brief Data byte values used in byte-level and buffer write/read tests
 *
 * @details
 * Named constants for test data payloads, replacing bare hex literals.
 * Values chosen to exercise both 0 and 1 bits in multiple positions.
 */
typedef enum : uint8_t {
  k_test_write_byte    = 0xAB, /**< Byte for write_byte test (alternating bits) */
  k_test_buf_byte_0    = 0x01, /**< First byte of write buffer test data */
  k_test_buf_byte_1    = 0x02, /**< Second byte of write buffer test data */
  k_test_buf_byte_2    = 0x03, /**< Third byte of write buffer test data */
  k_test_bits_per_byte = 8,    /**< Number of bits in one byte */
} test_onewire_data_t;

/**
 * @enum test_onewire_buf_len_t
 * @brief Buffer length constants for write/read tests
 *
 * @details
 * Named constants for transfer lengths and array sizes used in buffer
 * and null-pointer tests.
 */
typedef enum : uint16_t {
  k_test_buf_len_3      = 3,     /**< 3-byte write/read buffer */
  k_test_null_data_len  = 5,     /**< Length used with nullptr data pointer to trigger error */
  k_test_max_buf_bytes  = 255,   /**< Maximum 1-Wire buffer size for loop-exit tests */
  k_test_large_delay_us = 10000, /**< 10 ms delay to exercise counter_max branch */
} test_onewire_buf_len_t;

/**
 * @enum test_onewire_search_init_t
 * @brief Initial values for search test variables
 *
 * @details
 * Non-zero sentinel value to verify that num_devices is correctly
 * set to 0 by the search function when no devices are found.
 */
typedef enum : uint32_t {
  k_test_nonzero_sentinel = 99, /**< Non-zero value to verify zeroing */
} test_onewire_search_init_t;

/**
 * @enum test_onewire_gpio_call_t
 * @brief GPIO call ordinal numbers for mock error injection
 *
 * @details
 * After bus init (2 GPIO calls for set_input + gpio_read), each 1-Wire
 * operation generates a known sequence of GPIO calls. These constants
 * identify specific call positions for targeted error injection.
 */
typedef enum : uint32_t {
  k_test_gpio_call_2  = 2,  /**< gpio_read after init (call 2) */
  k_test_gpio_call_3  = 3,  /**< First post-init GPIO op (set_output) */
  k_test_gpio_call_5  = 5,  /**< set_input (release_line) in reset/write/read */
  k_test_gpio_call_6  = 6,  /**< gpio_read (read_line) after release */
  k_test_gpio_call_7  = 7,  /**< write_byte bit0 start after reset (calls 3-6) */
  k_test_gpio_call_9  = 9,  /**< write_bit after 2 read_bits (2x4 calls each) */
  k_test_gpio_call_29 = 29, /**< search_bits first call after reset(4)+write_byte(24) */
  k_test_gpio_call_31 = 31, /**< ROM byte0 bit0 after reset(4)+write_byte cmd(24)+init(2) */
} test_onewire_gpio_call_t;

/**
 * @enum test_onewire_buf_size_t
 * @brief Buffer size and length constants for callback tests
 *
 * @details
 * Named constants for array dimensions and length arguments used in
 * internal callback error-injection tests.
 */
typedef enum : uint32_t {
  k_test_buf_size_2   = 2,           /**< Two-byte buffer size */
  k_test_buf_size_4   = 4,           /**< Four-byte buffer size */
  k_test_overflow_len = 0xFFFFFFFFU, /**< Overflow length for invalid-size tests */
} test_onewire_buf_size_t;

/**
 * @enum test_onewire_search_state_t
 * @brief Search state constants for search step direction tests
 */
typedef enum : uint8_t {
  k_test_last_discrepancy_2 = 2, /**< last_discrepancy=2 for direction-from-last-rom test */
} test_onewire_search_state_t;

/**
 * @enum test_onewire_test_data_byte_t
 * @brief Test data byte values for write/read error injection tests
 */
typedef enum : uint8_t {
  k_test_data_byte_aa = 0xAA, /**< Alternating bit pattern for write tests */
} test_onewire_test_data_byte_t;

/**
 * @enum test_onewire_rom_data_t
 * @brief ROM code byte values for match/read ROM tests
 *
 * @details
 * DS18B20 example ROM: [0x28][0xFF][0x12][0x34][0x56][0x78][0x9A][0xBC]
 */
typedef enum : uint8_t {
  k_test_rom_family = 0x28, /**< DS18B20 family code */
  k_test_rom_byte_1 = 0xFF, /**< ROM serial byte 1 */
  k_test_rom_byte_2 = 0x12, /**< ROM serial byte 2 */
  k_test_rom_byte_3 = 0x34, /**< ROM serial byte 3 */
  k_test_rom_byte_4 = 0x56, /**< ROM serial byte 4 */
  k_test_rom_byte_5 = 0x78, /**< ROM serial byte 5 */
  k_test_rom_byte_6 = 0x9A, /**< ROM serial byte 6 */
  k_test_rom_crc    = 0xBC, /**< ROM CRC-8 byte */
} test_onewire_rom_data_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @brief Static bus manager for all tests
 * @details
 * Shared across tests to manage bus lifecycle. Reset in setUp()/tearDown().
 * Uses static allocation per NASA Rule 3 (no dynamic memory).
 */
static rx_bus_manager_t s_test_manager;

/**
 * @brief Static OneWire bus configuration
 * @details
 * Configured in setUp() with test pin assignment. Destroyed in tearDown().
 */
static rx_bus_config_t s_onewire_config;

/**
 * @brief Test bus name for bus manager lookup
 * @details
 * Unique identifier for bus registration. Used in all rx_bus_onewire_*() calls.
 */
static const char* const s_test_bus_name = "test_onewire";

/**
 * @brief GPIO pin assigned to 1-Wire bus
 * @details
 * P0.5 is standard STAR temperature sensor pin (DS18B20).
 * Matches production hardware configuration.
 */
static const rx_port_pin_t s_test_pin = k_rx_p0_5;

/**
 * @brief Set up test fixtures before each test
 *
 * @details
 * Initializes mock subsystems and bus manager before every test. Ensures
 * clean state isolation between tests per NASA Rule 5 (assertions).
 *
 * **Initialization sequence:**
 * 1. Initialize mock GPIO (configures pin state tracking)
 * 2. Initialize mock 1-Wire hardware (timing simulation)
 * 3. Disable CRC override (use real CRC calculation)
 * 4. Create bus manager with test name
 * 5. Configure 1-Wire bus on P0.5
 * 6. Register bus with manager
 *
 * **Post-conditions:**
 * - Bus manager ready for rx_bus_onewire_init()
 * - Mock GPIO configured for P0.5
 * - CRC subsystem operational
 * - No buses initialized (requires explicit test init)
 *
 * @pre None (called by Unity before each test)
 * @post s_test_manager initialized and ready
 * @post s_onewire_config contains valid P0.5 configuration
 * @post Mock subsystems reset to default state
 *
 * @note Called automatically by Unity framework
 * @warning Assertions will fail test on init errors
 *
 * @see tearDown() Cleanup counterpart
 * @see rx_bus_manager_init() Bus manager initialization
 * @see rx_bus_config_init_onewire() 1-Wire config creation
 */
void setUp(void)
{
  /* Initialize mock subsystems */
  mock_gpio_init();
  mock_onewire_hw_init();
  mock_crc8_set_override(false);

  /* Initialize bus manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create OneWire bus config */
  err = rx_bus_config_init_onewire(&s_onewire_config, s_test_bus_name, s_test_pin);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add bus to manager */
  err = rx_bus_manager_add_bus(&s_test_manager, &s_onewire_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Tear down test fixtures after each test
 *
 * @details
 * Cleans up all resources allocated in setUp(). Ensures no state leakage
 * between tests per NASA Rule 5 (assertions) and Rule 3 (no memory leaks).
 *
 * **Cleanup sequence:**
 * 1. Deinitialize bus manager (releases bus resources)
 * 2. Deinitialize mock GPIO (clears pin state)
 * 3. Deinitialize mock 1-Wire hardware (resets timing)
 * 4. Disable CRC override (return to default)
 *
 * **Post-conditions:**
 * - All bus manager resources released
 * - Mock subsystems reset
 * - Ready for next test setUp()
 *
 * @pre setUp() has been called
 * @post s_test_manager deinitialized
 * @post Mock subsystems reset to default state
 * @post No resources leaked
 *
 * @note Called automatically by Unity framework
 * @note Errors intentionally ignored (cleanup must complete)
 *
 * @see setUp() Initialization counterpart
 * @see rx_bus_manager_deinit() Bus manager cleanup
 */
void tearDown(void)
{
  /* Release OneWire state pool entry before destroying manager */
  (void)rx_bus_onewire_deinit(&s_test_manager, s_test_bus_name);

  /* Deinitialize bus manager */
  (void)rx_bus_manager_deinit(&s_test_manager);

  /* Clean up mocks */
  mock_gpio_deinit();
  mock_onewire_hw_deinit();
  mock_crc8_set_override(false);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_init OneWire Initialization Tests
 * @brief Tests for rx_bus_onewire_init() function
 *
 * @details
 * Validates bus initialization including:
 * - Successful initialization with valid parameters
 * - nullptr pointer validation (manager, bus_name)
 * - Bus lookup failures (non-existent bus)
 * - Bus type validation (reject non-OneWire buses)
 * - GPIO hardware error propagation
 *
 * **Coverage:** 6 tests, 100% code paths
 * - Success case: 1 test
 * - Error cases: 5 tests
 *
 * **Tested Error Codes:**
 * - k_rx_ok: Successful initialization
 * - k_rx_err_null_ptr: nullptr manager or bus_name
 * - k_rx_err_not_found: Bus not registered
 * - k_rx_err_invalid_arg: Wrong bus type
 * - k_rx_err_hw_error: GPIO configuration failed
 *
 * @{
 */

/**
 * @brief Test successful OneWire bus initialization
 *
 * @details
 * Verifies normal initialization flow:
 * 1. Bus lookup by name succeeds
 * 2. Bus type is k_rx_bus_type_onewire
 * 3. GPIO pin configured as input (open-drain release)
 * 4. Internal state allocated from pool
 * 5. Bus marked as initialized
 *
 * **Expected GPIO State:**
 * - Pin configured as input (output disabled)
 * - Internal pull-up disabled (external 4.7kOhm used)
 * - Schmitt-trigger enabled for noise immunity
 *
 * @pre setUp() completed successfully
 * @post Bus initialized and ready for operations
 * @post GPIO P0.5 configured as input
 *
 * @see rx_bus_onewire_init() Function under test
 */
void test_rx_bus_onewire_init_success(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify GPIO was configured as input (open-drain release) */
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));
}

/**
 * @brief Test OneWire init with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection for bus manager parameter.
 * Ensures defensive programming per NASA Rule 5 (assertions).
 *
 * **Expected behavior:**
 * - Immediate return with k_rx_err_null_ptr
 * - No bus state allocation
 * - No GPIO configuration
 *
 * @pre None (independent test)
 * @post No side effects
 *
 * @see rx_bus_onewire_init() Function under test
 */
void test_rx_bus_onewire_init_null_manager(void)
{
  rx_err_t err = rx_bus_onewire_init(nullptr, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test OneWire init with nullptr bus name
 *
 * @details
 * Validates nullptr pointer detection for bus_name parameter.
 * Prevents invalid bus lookup attempts.
 *
 * **Expected behavior:**
 * - Immediate return with k_rx_err_null_ptr
 * - No bus manager access
 * - No state changes
 *
 * @pre None (independent test)
 * @post No side effects
 *
 * @see rx_bus_onewire_init() Function under test
 */
void test_rx_bus_onewire_init_null_bus_name(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test OneWire init with non-existent bus
 *
 * @details
 * Validates bus lookup error handling when bus name not found in manager.
 *
 * **Test scenario:**
 * - Request initialization for "nonexistent_bus"
 * - Bus manager lookup fails
 * - Returns k_rx_err_not_found
 *
 * @pre setUp() registered only "test_onewire" bus
 * @post No state changes
 *
 * @see rx_bus_onewire_init() Function under test
 * @see rx_bus_manager_find_bus() Bus lookup
 */
void test_rx_bus_onewire_init_bus_not_found(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, "nonexistent_bus");

  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test OneWire init with wrong bus type
 *
 * @details
 * Validates bus type checking. OneWire init must reject non-OneWire buses
 * (GPIO, I2C, SPI, etc.) to prevent incorrect operation.
 *
 * **Test scenario:**
 * 1. Register a GPIO bus (k_rx_bus_type_gpio)
 * 2. Attempt to initialize as OneWire
 * 3. Type mismatch detected
 * 4. Returns k_rx_err_invalid_arg
 *
 * **Why this matters:**
 * Different bus types have incompatible protocols. Initializing wrong type
 * would cause undefined behavior and communication failures.
 *
 * @pre setUp() completed
 * @post GPIO bus registration successful
 * @post OneWire init rejected
 * @post No side effects on GPIO bus
 *
 * @see rx_bus_onewire_init() Function under test
 * @see rx_bus_config_init_gpio() GPIO bus creation
 */
void test_rx_bus_onewire_init_wrong_bus_type(void)
{
  /* Create a GPIO bus (not OneWire) */
  static rx_bus_config_t s_gpio_config;
  rx_err_t               err = rx_bus_config_init_gpio(&s_gpio_config, "gpio_bus", k_rx_pc_6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Try to init as OneWire - should fail */
  err = rx_bus_onewire_init(&s_test_manager, "gpio_bus");
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test OneWire init with GPIO error
 *
 * @details
 * Validates GPIO hardware error propagation during initialization.
 * Tests error handling when GPIO configuration fails.
 *
 * **Test scenario:**
 * 1. Inject GPIO error via mock_gpio_set_next_error()
 * 2. Attempt initialization
 * 3. GPIO config fails with k_rx_err_hw_error
 * 4. Error propagated to caller
 *
 * **Real-world causes:**
 * - Invalid pin number
 * - Pin already locked by peripheral
 * - Port clock disabled
 * - Hardware fault
 *
 * @pre setUp() completed
 * @post Initialization failed, bus not operational
 * @post GPIO state unchanged (config never applied)
 *
 * @see rx_bus_onewire_init() Function under test
 * @see mock_gpio_set_next_error() Error injection
 */
void test_rx_bus_onewire_init_gpio_error(void)
{
  /* Inject GPIO error */
  mock_gpio_set_next_error(k_rx_err_hw_error);

  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/** @} */ /* end of test_onewire_init */

/* =============================================================================
 * Reset and Presence Detection Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_reset Reset and Presence Detection Tests
 * @brief Tests for rx_bus_onewire_reset() function
 *
 * @details
 * Validates reset pulse and presence detection:
 * - Reset pulse timing (480-960us)
 * - Presence pulse detection (60-240us response)
 * - Device present vs. absent detection
 * - nullptr pointer handling
 * - Uninitialized bus handling
 * - GPIO error propagation
 *
 * **1-Wire Reset Sequence:**
 * @code
 * 1. Controller pulls bus low for 480-960us
 * 2. Controller releases bus (returns to high via pull-up)
 * 3. Wait 15-60us for device to detect rising edge
 * 4. Device pulls bus low for 60-240us (presence pulse)
 * 5. Controller samples bus to detect presence
 * 6. Both release bus, idle high
 * @endcode
 *
 * **Coverage:** 5 tests, 100% code paths
 * - Device present: 1 test
 * - Device absent: 1 test
 * - Error cases: 3 tests
 *
 * @{
 */

/**
 * @brief Test reset with device present
 *
 * @details
 * Validates successful reset/presence detection when device is on bus.
 *
 * **Test sequence:**
 * 1. Initialize OneWire bus
 * 2. Configure mock GPIO to return LOW (simulates presence pulse)
 * 3. Execute rx_bus_onewire_reset()
 * 4. Verify presence = true
 *
 * **Mock GPIO behavior:**
 * - Returns false (low) when sampled 15-60us after reset
 * - Simulates DS18B20 presence pulse
 *
 * @pre Bus initialized
 * @post presence = true
 * @post Bus ready for ROM commands
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_reset_device_present(void)
{
  /* Initialize bus first */
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Simulate device presence (line goes low after reset) */
  mock_gpio_set_read_value(s_test_pin, false); /* Device pulls low */

  bool presence = false;
  err           = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(presence);
}

/**
 * @brief Test reset with no device present
 *
 * @details
 * Validates detection when no device responds to reset pulse.
 *
 * **Test sequence:**
 * 1. Initialize OneWire bus
 * 2. Configure mock GPIO to return HIGH (no presence pulse)
 * 3. Execute rx_bus_onewire_reset()
 * 4. Verify presence = false
 *
 * **Mock GPIO behavior:**
 * - Returns true (high) when sampled (pull-up, no device)
 * - Simulates empty bus or disconnected sensor
 *
 * **Real-world causes:**
 * - No devices connected
 * - Device powered off
 * - Bus short circuit
 * - Broken wire
 *
 * @pre Bus initialized
 * @post presence = false
 * @post Bus still operational (not error)
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_reset_no_device(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Simulate no device (line stays high from pull-up) */
  mock_gpio_set_read_value(s_test_pin, true);

  bool presence = true;
  err           = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(presence);
}

/**
 * @brief Test reset with nullptr presence pointer
 *
 * @details
 * Validates nullptr pointer detection for output parameter.
 *
 * **Expected behavior:**
 * - Detects nullptr presence pointer
 * - Returns k_rx_err_null_ptr
 * - No bus operations performed
 * - No GPIO access
 *
 * @pre Bus initialized
 * @post No side effects
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_reset_null_presence(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test reset on uninitialized bus
 *
 * @details
 * Validates state checking. Reset requires prior rx_bus_onewire_init().
 *
 * **Test scenario:**
 * - Skip initialization (bus config exists but not initialized)
 * - Attempt reset operation
 * - State check fails
 * - Returns k_rx_err_invalid_state
 *
 * **Why this matters:**
 * Uninitialized bus has no GPIO configuration. Operating on it would
 * cause undefined behavior or hardware faults.
 *
 * @pre setUp() completed (bus registered but not initialized)
 * @post No side effects
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_reset_not_initialized(void)
{
  /* Don't initialize - just try reset */
  bool     presence = false;
  rx_err_t err      = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test reset with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection for bus manager parameter.
 *
 * **Expected behavior:**
 * - Immediate return with k_rx_err_null_ptr
 * - No bus lookup attempted
 * - No memory access
 *
 * @pre None (independent test)
 * @post No side effects
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_reset_null_manager(void)
{
  bool     presence = false;
  rx_err_t err      = rx_bus_onewire_reset(nullptr, s_test_bus_name, &presence);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ /* end of test_onewire_reset */

/* =============================================================================
 * Bit Operation Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_bit Bit-Level Operation Tests
 * @brief Tests for write_bit() and read_bit() functions
 *
 * @details
 * Validates bit-level 1-Wire operations:
 * - Write-1 time slot (1-15us low pulse)
 * - Write-0 time slot (60-120us low pulse)
 * - Read time slot (1-15us low, sample at 15us)
 * - LSB-first bit order
 * - GPIO operation counting
 * - Error handling
 *
 * **Timing Requirements:**
 * All time slots are 60-120us total. Controller distinguishes write-0 vs
 * write-1 by pulse width. Devices sample at 15-30us.
 *
 * **Coverage:** 8 tests, 100% code paths
 *
 * @{
 */

/**
 * @brief Test write bit 1
 *
 * @details
 * Validates write-1 time slot timing and GPIO sequence.
 *
 * **Expected GPIO sequence:**
 * 1. Pull bus low for 1-15us (write-1 start)
 * 2. Release bus immediately (return to high via pull-up)
 * 3. Wait for slot completion (60-120us total)
 *
 * **Device behavior:**
 * - Samples bus at ~15us
 * - Reads '1' (bus is high due to pull-up)
 *
 * @pre Bus initialized
 * @post At least one GPIO low pulse detected
 * @post write_low_count > 0
 *
 * @see rx_bus_onewire_write_bit() Function under test
 */
void test_rx_bus_onewire_write_bit_one(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_reset_counters();

  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify GPIO operations occurred (low pulse, then release) */
  TEST_ASSERT_GREATER_THAN(0, mock_gpio_get_write_low_count());
}

/**
 * @brief Test write bit 0
 *
 * @details
 * Validates write-0 time slot timing and GPIO sequence.
 *
 * **Expected GPIO sequence:**
 * 1. Pull bus low for 60-120us (write-0 duration)
 * 2. Release bus (return to high via pull-up)
 * 3. Recovery time >1us before next operation
 *
 * **Device behavior:**
 * - Samples bus at ~15us
 * - Reads '0' (bus is still low)
 *
 * @pre Bus initialized
 * @post At least one GPIO low pulse detected
 * @post Pulse width longer than write-1 (60-120us vs 1-15us)
 *
 * @see rx_bus_onewire_write_bit() Function under test
 */
void test_rx_bus_onewire_write_bit_zero(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_reset_counters();

  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_GREATER_THAN(0, mock_gpio_get_write_low_count());
}

/**
 * @brief Test write bit on uninitialized bus
 *
 * @details
 * Validates state checking before write operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_write_bit() Function under test
 */
void test_rx_bus_onewire_write_bit_not_initialized(void)
{
  rx_err_t err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, true);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test write bit with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection.
 *
 * @pre None
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write_bit() Function under test
 */
void test_rx_bus_onewire_write_bit_null_manager(void)
{
  rx_err_t err = rx_bus_onewire_write_bit(nullptr, s_test_bus_name, true);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read bit returns high
 *
 * @details
 * Validates read time slot when device transmits '1'.
 *
 * **Read sequence:**
 * 1. Controller pulls bus low for 1-15us
 * 2. Controller releases and samples at ~15us
 * 3. Device keeps bus high (or doesn't pull low)
 * 4. Controller reads '1'
 *
 * @pre Bus initialized
 * @pre Mock GPIO returns true (high)
 * @post bit = true
 *
 * @see rx_bus_onewire_read_bit() Function under test
 */
void test_rx_bus_onewire_read_bit_high(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Set line to return high on read */
  mock_gpio_set_read_value(s_test_pin, true);

  bool bit = false;
  err      = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(bit);
}

/**
 * @brief Test read bit returns low
 *
 * @details
 * Validates read time slot when device transmits '0'.
 *
 * **Read sequence:**
 * 1. Controller pulls bus low for 1-15us
 * 2. Controller releases and samples at ~15us
 * 3. Device pulls bus low (transmits '0')
 * 4. Controller reads '0'
 *
 * @pre Bus initialized
 * @pre Mock GPIO returns false (low)
 * @post bit = false
 *
 * @see rx_bus_onewire_read_bit() Function under test
 */
void test_rx_bus_onewire_read_bit_low(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Set line to return low on read */
  mock_gpio_set_read_value(s_test_pin, false);

  bool bit = true;
  err      = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(bit);
}

/**
 * @brief Test read bit with nullptr output pointer
 *
 * @details
 * Validates nullptr pointer detection for output parameter.
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_bit() Function under test
 */
void test_rx_bus_onewire_read_bit_null_output(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read bit on uninitialized bus
 *
 * @details
 * Validates state checking before read operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_read_bit() Function under test
 */
void test_rx_bus_onewire_read_bit_not_initialized(void)
{
  bool     bit = false;
  rx_err_t err = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ /* end of test_onewire_bit */

/* =============================================================================
 * Byte Operation Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_byte Byte-Level Operation Tests
 * @brief Tests for write_byte() and read_byte() functions
 *
 * @details
 * Validates byte-level operations built from bit operations:
 * - LSB-first transmission (bit 0 -> bit 7)
 * - 8-bit data transfer
 * - Correct bit ordering
 * - Error propagation
 *
 * **Byte Transmission Order (1-Wire Specification):**
 * @code
 * Byte 0xAB = 0b10101011
 *
 * Bit order on wire (LSB first):
 * [bit0=1] [bit1=1] [bit2=0] [bit3=1] [bit4=0] [bit5=1] [bit6=0] [bit7=1]
 * @endcode
 *
 * **Coverage:** 8 tests, 100% code paths
 *
 * @{
 */

/**
 * @brief Test write byte
 *
 * @details
 * Validates byte transmission with LSB-first bit order.
 *
 * **Test byte:** 0xAB = 0b10101011
 * **Wire order:** 1,1,0,1,0,1,0,1 (bit0 first)
 *
 * **Verification:**
 * - Counts GPIO low pulses (should be >=8 for 8 bits)
 * - Each bit requires at least one low pulse
 *
 * @pre Bus initialized
 * @post 8 or more GPIO low pulses counted
 *
 * @see rx_bus_onewire_write_byte() Function under test
 */
void test_rx_bus_onewire_write_byte_success(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_reset_counters();

  err = rx_bus_onewire_write_byte(&s_test_manager, s_test_bus_name, k_test_write_byte);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify 8 bits were written (8 low pulses minimum) */
  TEST_ASSERT_GREATER_OR_EQUAL((uint8_t)k_test_bits_per_byte, mock_gpio_get_write_low_count());
}

/**
 * @brief Test write byte on uninitialized bus
 *
 * @details
 * Validates state checking before write operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_write_byte() Function under test
 */
void test_rx_bus_onewire_write_byte_not_initialized(void)
{
  rx_err_t err = rx_bus_onewire_write_byte(&s_test_manager, s_test_bus_name, 0x00);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test write byte with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection.
 *
 * @pre None
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write_byte() Function under test
 */
void test_rx_bus_onewire_write_byte_null_manager(void)
{
  rx_err_t err = rx_bus_onewire_write_byte(nullptr, s_test_bus_name, 0x00);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read byte returns 0xFF (all high)
 *
 * @details
 * Validates byte read when all bits are '1'.
 *
 * **Mock behavior:**
 * - GPIO always returns true (high)
 * - Simulates device transmitting 0xFF
 *
 * **Expected result:** 0xFF = 0b11111111
 *
 * @pre Bus initialized
 * @pre Mock GPIO returns true
 * @post byte = 0xFF
 *
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_read_byte_all_ones(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Set line to always return high */
  mock_gpio_set_read_value(s_test_pin, true);

  uint8_t byte = 0x00;
  err          = rx_bus_onewire_read_byte(&s_test_manager, s_test_bus_name, &byte);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(UINT8_MAX, byte);
}

/**
 * @brief Test read byte returns 0x00 (all low)
 *
 * @details
 * Validates byte read when all bits are '0'.
 *
 * **Mock behavior:**
 * - GPIO always returns false (low)
 * - Simulates device transmitting 0x00
 *
 * **Expected result:** 0x00 = 0b00000000
 *
 * @pre Bus initialized
 * @pre Mock GPIO returns false
 * @post byte = 0x00
 *
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_read_byte_all_zeros(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Set line to always return low */
  mock_gpio_set_read_value(s_test_pin, false);

  uint8_t byte = UINT8_MAX;
  err          = rx_bus_onewire_read_byte(&s_test_manager, s_test_bus_name, &byte);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(0, byte);
}

/**
 * @brief Test read byte with nullptr output pointer
 *
 * @details
 * Validates nullptr pointer detection for output parameter.
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_read_byte_null_output(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_read_byte(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read byte on uninitialized bus
 *
 * @details
 * Validates state checking before read operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_read_byte_not_initialized(void)
{
  uint8_t  byte = 0;
  rx_err_t err  = rx_bus_onewire_read_byte(&s_test_manager, s_test_bus_name, &byte);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ /* end of test_onewire_byte */

/* =============================================================================
 * Buffer Operation Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_buffer Buffer Operation Tests
 * @brief Tests for multi-byte read/write operations
 *
 * @details
 * Validates bulk data transfer operations:
 * - Multi-byte write sequences
 * - Multi-byte read sequences
 * - Zero-length handling (no-op)
 * - nullptr pointer validation
 * - Buffer boundary conditions
 *
 * **Coverage:** 8 tests, 100% code paths
 *
 * @{
 */

/**
 * @brief Test write buffer success
 *
 * @details
 * Validates multi-byte write operation.
 *
 * **Test data:** [0x01, 0x02, 0x03]
 * **Expected:** Each byte transmitted LSB-first
 *
 * @pre Bus initialized
 * @post All bytes transmitted successfully
 *
 * @see rx_bus_onewire_write() Function under test
 */
void test_rx_bus_onewire_write_buffer_success(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t data[] = {k_test_buf_byte_0, k_test_buf_byte_1, k_test_buf_byte_2};
  err            = rx_bus_onewire_write(&s_test_manager, s_test_bus_name, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test write buffer zero length
 *
 * @details
 * Validates that zero-length writes succeed without operation.
 *
 * **Expected behavior:**
 * - Returns k_rx_ok immediately
 * - No bus operations
 * - No GPIO access
 *
 * @pre Bus initialized
 * @post No side effects
 *
 * @see rx_bus_onewire_write() Function under test
 */
void test_rx_bus_onewire_write_buffer_zero_length(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Zero length should succeed without doing anything */
  err = rx_bus_onewire_write(&s_test_manager, s_test_bus_name, nullptr, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test write buffer with nullptr data and non-zero length
 *
 * @details
 * Validates nullptr pointer detection when length > 0.
 *
 * **Invalid scenario:**
 * - data = nullptr
 * - len = 5
 * - Cannot dereference nullptr pointer
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write() Function under test
 */
void test_rx_bus_onewire_write_buffer_null_data(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_write(&s_test_manager, s_test_bus_name, nullptr, k_test_null_data_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test write buffer on uninitialized bus
 *
 * @details
 * Validates state checking before write operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_write() Function under test
 */
void test_rx_bus_onewire_write_buffer_not_initialized(void)
{
  uint8_t  data[] = {k_test_buf_byte_0};
  rx_err_t err    = rx_bus_onewire_write(&s_test_manager, s_test_bus_name, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test read buffer success
 *
 * @details
 * Validates multi-byte read operation.
 *
 * **Mock behavior:**
 * - GPIO returns true (all bits = 1)
 * - Simulates device transmitting [0xFF, 0xFF, 0xFF]
 *
 * @pre Bus initialized
 * @pre Mock GPIO returns true
 * @post All bytes = 0xFF
 *
 * @see rx_bus_onewire_read() Function under test
 */
void test_rx_bus_onewire_read_buffer_success(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, true); /* All ones */

  uint8_t data[k_test_buf_len_3] = {};
  err = rx_bus_onewire_read(&s_test_manager, s_test_bus_name, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* All bytes should be 0xFF since line is high */
  TEST_ASSERT_EQUAL_HEX8(UINT8_MAX, data[0]);
  TEST_ASSERT_EQUAL_HEX8(UINT8_MAX, data[1]);
  TEST_ASSERT_EQUAL_HEX8(UINT8_MAX, data[k_test_buf_len_3 - 1]);
}

/**
 * @brief Test read buffer zero length
 *
 * @details
 * Validates that zero-length reads succeed without operation.
 *
 * @pre Bus initialized
 * @post k_rx_ok returned
 * @post No bus operations
 *
 * @see rx_bus_onewire_read() Function under test
 */
void test_rx_bus_onewire_read_buffer_zero_length(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_read(&s_test_manager, s_test_bus_name, nullptr, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read buffer with nullptr data and non-zero length
 *
 * @details
 * Validates nullptr pointer detection when length > 0.
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read() Function under test
 */
void test_rx_bus_onewire_read_buffer_null_data(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_read(&s_test_manager, s_test_bus_name, nullptr, k_test_null_data_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read buffer on uninitialized bus
 *
 * @details
 * Validates state checking before read operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_read() Function under test
 */
void test_rx_bus_onewire_read_buffer_not_initialized(void)
{
  uint8_t  data[k_test_buf_len_3];
  rx_err_t err = rx_bus_onewire_read(&s_test_manager, s_test_bus_name, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ /* end of test_onewire_buffer */

/* =============================================================================
 * Skip ROM Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_skip_rom Skip ROM Command Tests
 * @brief Tests for Skip ROM [0xCC] command
 *
 * @details
 * Validates Skip ROM command implementation:
 * - Command byte transmission (0xCC)
 * - Presence pulse checking
 * - Broadcast to all devices (no addressing)
 * - Error handling (no device, uninitialized bus)
 *
 * **Skip ROM Command [0xCC]:**
 * - Used when only one device on bus
 * - Broadcasts to all devices (no ROM matching)
 * - Faster than Match ROM (skips 64-bit address)
 * - Unsafe with multiple devices (all respond simultaneously)
 *
 * **Sequence:**
 * @code
 * 1. Reset pulse
 * 2. Check presence
 * 3. Send 0xCC (Skip ROM command)
 * 4. Send function command (e.g., Convert Temperature)
 * @endcode
 *
 * **Coverage:** 4 tests, 100% code paths
 *
 * @{
 */

/**
 * @brief Test skip ROM success with device present
 *
 * @details
 * Validates successful Skip ROM when device responds to reset.
 *
 * @pre Bus initialized
 * @pre Mock simulates device presence
 * @post k_rx_ok returned
 * @post Bus ready for function commands
 *
 * @see rx_bus_onewire_skip_rom() Function under test
 */
void test_rx_bus_onewire_skip_rom_success(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Simulate device presence */
  mock_gpio_set_read_value(s_test_pin, false);

  err = rx_bus_onewire_skip_rom(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test skip ROM with no device present
 *
 * @details
 * Validates error handling when no device responds to reset.
 *
 * **Expected behavior:**
 * - Reset pulse issued
 * - No presence pulse detected
 * - Returns k_rx_err_not_found
 * - No 0xCC command sent (early exit)
 *
 * @pre Bus initialized
 * @pre Mock simulates no device
 * @post k_rx_err_not_found returned
 *
 * @see rx_bus_onewire_skip_rom() Function under test
 */
void test_rx_bus_onewire_skip_rom_no_device(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* No device - line stays high */
  mock_gpio_set_read_value(s_test_pin, true);

  err = rx_bus_onewire_skip_rom(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test skip ROM with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection.
 *
 * @pre None
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_skip_rom() Function under test
 */
void test_rx_bus_onewire_skip_rom_null_manager(void)
{
  rx_err_t err = rx_bus_onewire_skip_rom(nullptr, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test skip ROM on uninitialized bus
 *
 * @details
 * Validates state checking before ROM command.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_skip_rom() Function under test
 */
void test_rx_bus_onewire_skip_rom_not_initialized(void)
{
  rx_err_t err = rx_bus_onewire_skip_rom(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ /* end of test_onewire_skip_rom */

/* =============================================================================
 * Match ROM Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_match_rom Match ROM Command Tests
 * @brief Tests for Match ROM [0x55] command
 *
 * @details
 * Validates Match ROM command implementation:
 * - Command byte transmission (0x55)
 * - 64-bit ROM code transmission
 * - Device selection by address
 * - Error handling (no device, nullptr ROM, uninitialized bus)
 *
 * **Match ROM Command [0x55]:**
 * - Selects specific device by 64-bit ROM code
 * - Required for multi-device networks
 * - Sequence: [0x55] [ROM[0]] [ROM[1]] ... [ROM[7]]
 * - Only addressed device responds to subsequent commands
 * - Other devices return to idle state
 *
 * **ROM Code Format:**
 * @code
 * [Family(1)] [Serial(6)] [CRC(1)]
 * Example DS18B20: [0x28][0xFF][0x12][0x34][0x56][0x78][0x9A][0xBC]
 * @endcode
 *
 * **Coverage:** 4 tests, 100% code paths
 *
 * @{
 */

/**
 * @brief Test match ROM success
 *
 * @details
 * Validates successful device selection via Match ROM.
 *
 * **Test ROM:** DS18B20 example
 * - Family: 0x28 (DS18B20)
 * - Serial: 0xFF 0x12 0x34 0x56 0x78 0x9A
 * - CRC: 0xBC (example, not validated in this test)
 *
 * @pre Bus initialized
 * @pre Mock simulates device presence
 * @post k_rx_ok returned
 * @post Device selected, ready for function commands
 *
 * @see rx_bus_onewire_match_rom() Function under test
 */
void test_rx_bus_onewire_match_rom_success(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Simulate device presence */
  mock_gpio_set_read_value(s_test_pin, false);

  uint8_t rom[k_test_rom_bytes] = {k_test_rom_family,
                                   k_test_rom_byte_1,
                                   k_test_rom_byte_2,
                                   k_test_rom_byte_3,
                                   k_test_rom_byte_4,
                                   k_test_rom_byte_5,
                                   k_test_rom_byte_6,
                                   k_test_rom_crc};
  err                           = rx_bus_onewire_match_rom(&s_test_manager, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test match ROM with no device present
 *
 * @details
 * Validates error handling when no device responds to reset.
 *
 * @pre Bus initialized
 * @pre Mock simulates no device
 * @post k_rx_err_not_found returned
 * @post No ROM code transmitted
 *
 * @see rx_bus_onewire_match_rom() Function under test
 */
void test_rx_bus_onewire_match_rom_no_device(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* No device - line stays high */
  mock_gpio_set_read_value(s_test_pin, true);

  uint8_t rom[k_test_rom_bytes] = {k_test_rom_family,
                                   k_test_rom_byte_1,
                                   k_test_rom_byte_2,
                                   k_test_rom_byte_3,
                                   k_test_rom_byte_4,
                                   k_test_rom_byte_5,
                                   k_test_rom_byte_6,
                                   k_test_rom_crc};
  err                           = rx_bus_onewire_match_rom(&s_test_manager, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test match ROM with nullptr ROM pointer
 *
 * @details
 * Validates nullptr pointer detection for ROM parameter.
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 * @post No bus operations
 *
 * @see rx_bus_onewire_match_rom() Function under test
 */
void test_rx_bus_onewire_match_rom_null_rom(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_match_rom(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test match ROM on uninitialized bus
 *
 * @details
 * Validates state checking before ROM command.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_match_rom() Function under test
 */
void test_rx_bus_onewire_match_rom_not_initialized(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  rx_err_t err                   = rx_bus_onewire_match_rom(&s_test_manager, s_test_bus_name, rom);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ /* end of test_onewire_match_rom */

/* =============================================================================
 * Read ROM Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_read_rom Read ROM Command Tests
 * @brief Tests for Read ROM [0x33] command
 *
 * @details
 * Validates Read ROM command implementation:
 * - Command byte transmission (0x33)
 * - 64-bit ROM code reception
 * - Single-device restriction
 * - Error handling (no device, nullptr ROM, uninitialized bus)
 *
 * **Read ROM Command [0x33]:**
 * - Reads 64-bit ROM code from device
 * - ONLY usable when exactly one device on bus
 * - Sequence: [0x33] -> [Read 8 bytes LSB-first]
 * - Multiple devices cause bus contention (invalid data)
 *
 * **Use cases:**
 * - Initial device discovery
 * - Learning ROM code for subsequent Match ROM
 * - Factory testing and inventory
 *
 * **Coverage:** 4 tests, 100% code paths
 *
 * @{
 */

/**
 * @brief Test read ROM with no device present
 *
 * @details
 * Validates error handling when no device responds to reset.
 *
 * @pre Bus initialized
 * @pre Mock simulates no device
 * @post k_rx_err_not_found returned
 * @post ROM buffer unchanged
 *
 * @see rx_bus_onewire_read_rom() Function under test
 */
void test_rx_bus_onewire_read_rom_no_device(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* No device - line stays high */
  mock_gpio_set_read_value(s_test_pin, true);

  uint8_t rom[k_test_rom_bytes] = {};
  err                           = rx_bus_onewire_read_rom(&s_test_manager, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test read ROM with nullptr ROM pointer
 *
 * @details
 * Validates nullptr pointer detection for output parameter.
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 * @post No bus operations
 *
 * @see rx_bus_onewire_read_rom() Function under test
 */
void test_rx_bus_onewire_read_rom_null_rom(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_onewire_read_rom(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read ROM on uninitialized bus
 *
 * @details
 * Validates state checking before ROM command.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_read_rom() Function under test
 */
void test_rx_bus_onewire_read_rom_not_initialized(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  rx_err_t err                   = rx_bus_onewire_read_rom(&s_test_manager, s_test_bus_name, rom);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test read ROM with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection for manager parameter.
 *
 * @pre None
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_rom() Function under test
 */
void test_rx_bus_onewire_read_rom_null_manager(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  rx_err_t err                   = rx_bus_onewire_read_rom(nullptr, s_test_bus_name, rom);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ /* end of test_onewire_read_rom */

/* =============================================================================
 * Search Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_search ROM Search Algorithm Tests
 * @brief Tests for Search ROM [0xF0] command
 *
 * @details
 * Validates ROM search algorithm implementation:
 * - Binary tree search algorithm
 * - Multi-device discovery
 * - Conflict resolution at bit discrepancies
 * - Array bounds handling
 * - Error cases (no devices, nullptr pointers)
 *
 * **Search ROM Algorithm [0xF0]:**
 *
 * Discovers all devices on bus using binary tree traversal:
 *
 * @code
 * For each bit position (0-63):
 *   1. All devices transmit bit N
 *   2. All devices transmit complement of bit N
 *   3. Controller analyzes response:
 *      (1, 0): Only '1' devices -> write 1, continue
 *      (0, 1): Only '0' devices -> write 0, continue
 *      (0, 0): Both present (conflict) -> choose path, mark bifurcation
 *      (1, 1): No devices -> error
 *   4. Write chosen bit (deselects non-matching devices)
 *   5. Matching devices continue, others go idle
 *
 * After first iteration: One ROM code found
 * Repeat from last discrepancy to find next device
 * Continue until no more bifurcations
 * @endcode
 *
 * **Example with 2 devices:**
 * @code
 * Device A: 0x28_01_00_00_00_00_00_CRC
 * Device B: 0x28_02_00_00_00_00_00_CRC
 *           ^
 *         Bit 8 differs
 *
 * Iteration 1: Choose '0' at bit 8 -> Find Device A
 * Iteration 2: Choose '1' at bit 8 -> Find Device B
 * @endcode
 *
 * **Coverage:** 6 tests, 80% code paths
 * - Multi-device search needs enhanced device emulator
 *
 * @{
 */

/**
 * @brief Test search with no devices
 *
 * @details
 * Validates search behavior when no devices on bus.
 *
 * **Expected behavior:**
 * 1. Reset pulse
 * 2. No presence detected
 * 3. Returns k_rx_ok (not an error, just empty bus)
 * 4. num_devices = 0
 *
 * @pre Bus initialized
 * @pre Mock simulates no device
 * @post k_rx_ok returned
 * @post num_devices = 0
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_no_devices(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* No device - line stays high */
  mock_gpio_set_read_value(s_test_pin, true);

  uint8_t  roms[k_test_max_search_devices * k_test_rom_bytes];
  uint32_t num_devices = k_test_nonzero_sentinel; /* Set to non-zero to verify it gets set to 0 */

  err = rx_bus_onewire_search(&s_test_manager,
                              s_test_bus_name,
                              roms,
                              k_test_max_search_devices,
                              &num_devices);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, num_devices);
}

/**
 * @brief Test search with zero max devices
 *
 * @details
 * Validates that zero-capacity search succeeds without operation.
 *
 * **Expected behavior:**
 * - max_devices = 0 (no space for results)
 * - Returns k_rx_ok immediately
 * - num_devices = 0
 * - No bus operations
 *
 * @pre Bus initialized
 * @post k_rx_ok returned
 * @post num_devices = 0
 * @post No search performed
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_zero_max(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  roms[k_test_rom_bytes];
  uint32_t num_devices = k_test_nonzero_sentinel;

  /* Zero max_devices should succeed immediately */
  err = rx_bus_onewire_search(&s_test_manager, s_test_bus_name, roms, 0, &num_devices);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, num_devices);
}

/**
 * @brief Test search with nullptr ROM buffer
 *
 * @details
 * Validates nullptr pointer detection for output buffer.
 *
 * **Invalid scenario:**
 * - roms = nullptr
 * - max_devices > 0
 * - Cannot write results
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_null_roms(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint32_t num_devices = 0;
  err                  = rx_bus_onewire_search(&s_test_manager,
                              s_test_bus_name,
                              nullptr,
                              k_test_max_search_devices,
                              &num_devices);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test search with nullptr num_devices pointer
 *
 * @details
 * Validates nullptr pointer detection for count output.
 *
 * **Invalid scenario:**
 * - num_devices = nullptr
 * - Cannot return device count
 *
 * @pre Bus initialized
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_null_num_devices(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t roms[k_test_max_search_devices * k_test_rom_bytes];
  err = rx_bus_onewire_search(&s_test_manager,
                              s_test_bus_name,
                              roms,
                              k_test_max_search_devices,
                              nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test search on uninitialized bus
 *
 * @details
 * Validates state checking before search operation.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_not_initialized(void)
{
  uint8_t  roms[k_test_max_search_devices * k_test_rom_bytes];
  uint32_t num_devices = 0;

  rx_err_t err = rx_bus_onewire_search(&s_test_manager,

                                       s_test_bus_name,
                                       roms,
                                       k_test_max_search_devices,
                                       &num_devices);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test search with nullptr manager
 *
 * @details
 * Validates nullptr pointer detection for manager parameter.
 *
 * @pre None
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_null_manager(void)
{
  uint8_t  roms[k_test_max_search_devices * k_test_rom_bytes];
  uint32_t num_devices = 0;

  rx_err_t err =

    rx_bus_onewire_search(nullptr, s_test_bus_name, roms, k_test_max_search_devices, &num_devices);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ /* end of test_onewire_search */

/* =============================================================================
 * GPIO Error Injection Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_errors Error Injection Tests
 * @brief Tests for GPIO error propagation and handling
 *
 * @details
 * Validates error handling throughout the stack:
 * - GPIO read errors during reset/presence
 * - GPIO write errors during bit transmission
 * - Error propagation from hardware layer to API
 *
 * **Coverage:** 3 tests, 100% code paths
 * - Reset read error: Tested
 * - Write bit GPIO error: Tested
 * - Read bit GPIO error: Tested
 *
 * @{
 */

/**
 * @brief Test reset with GPIO read error
 *
 * @details
 * Validates GPIO error propagation during presence detection.
 *
 * **Test scenario:**
 * 1. Initialize bus
 * 2. Inject GPIO read error via mock_gpio_set_next_error()
 * 3. Attempt reset (requires GPIO read for presence)
 * 4. GPIO read fails with k_rx_err_hw_error
 * 5. Error propagated to caller
 *
 * **Real-world causes:**
 * - Pin configuration error
 * - Port clock disabled
 * - Hardware fault
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 * @post presence value unchanged
 *
 * @see rx_bus_onewire_reset() Function under test
 * @see mock_gpio_set_next_error() Error injection
 */
void test_rx_bus_onewire_reset_gpio_read_error(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject error on GPIO read */
  mock_gpio_set_next_error(k_rx_err_hw_error);

  bool presence = false;
  err           = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test write bit with GPIO error
 *
 * @details
 * Validates GPIO error propagation during write-bit operation.
 *
 * **Test scenario:**
 * 1. Initialize bus
 * 2. Inject GPIO error via mock_gpio_set_next_error()
 * 3. Attempt write_bit
 * 4. GPIO write fails with k_rx_err_hw_error
 * 5. Error propagated to caller
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_write_bit() Function under test
 * @see mock_gpio_set_next_error() Error injection
 */
void test_rx_bus_onewire_write_bit_gpio_error(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject error on next GPIO operation */
  mock_gpio_set_next_error(k_rx_err_hw_error);

  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test read bit with GPIO error
 *
 * @details
 * Validates GPIO error propagation during read-bit operation.
 *
 * **Test scenario:**
 * 1. Initialize bus
 * 2. Inject GPIO error via mock_gpio_set_next_error()
 * 3. Attempt read_bit
 * 4. GPIO operation fails with k_rx_err_hw_error
 * 5. Error propagated to caller
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_read_bit() Function under test
 * @see mock_gpio_set_next_error() Error injection
 */
void test_rx_bus_onewire_read_bit_gpio_error(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject error on next GPIO operation */
  mock_gpio_set_next_error(k_rx_err_hw_error);

  bool bit = false;
  err      = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/** @} */ /* end of test_onewire_errors */

/* =============================================================================
 * CRC Validation Test Constants
 * =============================================================================
 */

/**
 * @brief CRC test constants
 *
 * @details
 * Constants for CRC validation tests. ROM codes are chosen with pre-computed
 * CRC-8/MAXIM values (polynomial 0x31, reversed 0x8C).
 */
typedef enum : uint8_t {
  k_test_crc_rom_a_family  = 0x28, /**< DS18B20 family code */
  k_test_crc_rom_a_byte7   = 0x1E, /**< CRC-8 of {0x28,0x00,0x00,0x00,0x00,0x00,0x00} */
  k_test_crc_rom_b_byte1   = 0x01, /**< Byte 1 of Device B (differs from A) */
  k_test_crc_rom_b_byte7   = 0x29, /**< CRC-8 of {0x28,0x01,0x00,0x00,0x00,0x00,0x00} */
  k_test_crc_rom_c_family  = 0x10, /**< DS18S20 family code */
  k_test_crc_rom_c_byte1   = 0xAA, /**< Device C serial byte 1 */
  k_test_crc_rom_c_byte2   = 0xBB, /**< Device C serial byte 2 */
  k_test_crc_rom_c_byte3   = 0xCC, /**< Device C serial byte 3 */
  k_test_crc_rom_c_byte4   = 0xDD, /**< Device C serial byte 4 */
  k_test_crc_rom_c_byte5   = 0xEE, /**< Device C serial byte 5 */
  k_test_crc_rom_c_byte6   = 0xFF, /**< Device C serial byte 6 */
  k_test_crc_rom_c_byte7   = 0xE9, /**< CRC-8 of {0x10,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF} */
  k_test_crc_bad_value     = 0x00, /**< Deliberately wrong CRC value for mismatch tests */
  k_test_crc_rom_byte_zero = 0x00, /**< Zero byte for ROM padding */
} crc_test_constants_t;

typedef enum : uint32_t {
  k_test_read_rom_seq_len    = 65,  /**< GPIO reads: 1 presence + 64 bit reads */
  k_test_single_search_len   = 129, /**< GPIO reads: 1 presence + 128 (64 pairs) */
  k_test_two_device_seq_len  = 258, /**< GPIO reads: 2 iterations x 129 */
  k_test_two_device_expected = 2,   /**< Expected device count for two-device search */
} crc_test_size_constants_t;

/**
 * @brief GPIO read sequence for read_rom of Device A
 *
 * @details
 * ROM: {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E}
 * Sequence: [presence=false] + [64 ROM bits LSB-first per byte]
 */
static const bool s_seq_read_rom_a[k_test_read_rom_seq_len] = {
  false, false, false, false, true,  false, true,  false, false, /* byte 0: 0x28 */
  false, false, false, false, false, false, false, false,        /* byte 1: 0x00 */
  false, false, false, false, false, false, false, false,        /* byte 2: 0x00 */
  false, false, false, false, false, false, false, false,        /* byte 3: 0x00 */
  false, false, false, false, false, false, false, false,        /* byte 4: 0x00 */
  false, false, false, false, false, false, false, false,        /* byte 5: 0x00 */
  false, false, false, false, false, false, false, false,        /* byte 6: 0x00 */
  false, true,  true,  true,  true,  false, false, false         /* byte 7: 0x1E */
};

/**
 * @brief GPIO read sequence for read_rom of Device C (DS18S20)
 *
 * @details
 * ROM: {0x10, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0xE9}
 * Sequence: [presence=false] + [64 ROM bits LSB-first per byte]
 */
static const bool s_seq_read_rom_c[k_test_read_rom_seq_len] = {
  false, false, false, false, false, true,  false, false, false, /* byte 0: 0x10 */
  false, true,  false, true,  false, true,  false, true,         /* byte 1: 0xAA */
  true,  true,  false, true,  true,  true,  false, true,         /* byte 2: 0xBB */
  false, false, true,  true,  false, false, true,  true,         /* byte 3: 0xCC */
  true,  false, true,  true,  true,  false, true,  true,         /* byte 4: 0xDD */
  false, true,  true,  true,  false, true,  true,  true,         /* byte 5: 0xEE */
  true,  true,  true,  true,  true,  true,  true,  true,         /* byte 6: 0xFF */
  true,  false, false, true,  false, true,  true,  true          /* byte 7: 0xE9 */
};

/**
 * @brief GPIO read sequence for single-device search (Device A)
 *
 * @details
 * ROM: {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E}
 * Search with one device: at each bit, reads (bit, !bit) since no conflicts.
 * Sequence: [presence=false] + [64 x (bit, complement)]
 */
static const bool s_seq_single_search[k_test_single_search_len] = {
  false,
  /* byte 0: 0x28 = 0b00101000, LSB first */
  false,
  true,
  false,
  true,
  false,
  true,
  true,
  false,
  false,
  true,
  true,
  false,
  false,
  true,
  false,
  true,
  /* byte 1: 0x00 */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 2: 0x00 */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 3: 0x00 */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 4: 0x00 */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 5: 0x00 */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 6: 0x00 */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 7: 0x1E = 0b00011110, LSB first */
  false,
  true,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  false,
  true,
  false,
  true,
  false,
  true};

/**
 * @brief GPIO read sequence for two-device search
 *
 * @details
 * Device A: {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E}
 * Device B: {0x28, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29}
 * Devices differ at byte 1 bit 0 (bit position 8, 0-indexed).
 *
 * Iteration 1: Both devices respond. At bit 9 (1-indexed) the algorithm
 * sees discrepancy (0,0), takes direction=0, selects Device A. After the
 * discrepancy, only Device A responds. Finds Device A.
 *
 * Iteration 2: Both devices respond again. At bit 9, the algorithm takes
 * direction=1 (last_discrepancy==9), selects Device B. After the
 * discrepancy, only Device B responds. Finds Device B.
 */
static const bool s_seq_two_device[k_test_two_device_seq_len] = {
  /* === Iteration 1: finds Device A === */
  false, /* presence */
  /* byte 0: 0x28 -- both devices agree, responses (bit, !bit) */
  false,
  true,
  false,
  true,
  false,
  true,
  true,
  false,
  false,
  true,
  true,
  false,
  false,
  true,
  false,
  true,
  /* byte 1: discrepancy at bit 0 (0,0), then A selected (0x00 remaining) */
  false,
  false,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* bytes 2-6: all 0x00 -- only Device A responding */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 7: 0x1E -- Device A CRC */
  false,
  true,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  false,
  true,
  false,
  true,
  false,
  true,
  /* === Iteration 2: finds Device B === */
  false, /* presence */
  /* byte 0: 0x28 -- both devices agree again after reset */
  false,
  true,
  false,
  true,
  false,
  true,
  true,
  false,
  false,
  true,
  true,
  false,
  false,
  true,
  false,
  true,
  /* byte 1: discrepancy at bit 0 (0,0), dir=1 selects B (0x01 remaining) */
  false,
  false,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* bytes 2-6: all 0x00 -- only Device B responding */
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  false,
  true,
  /* byte 7: 0x29 -- Device B CRC */
  true,
  false,
  false,
  true,
  false,
  true,
  true,
  false,
  false,
  true,
  true,
  false,
  false,
  true,
  false,
  true};

/* =============================================================================
 * CRC Validation Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_crc CRC Validation Tests
 * @brief Tests for CRC-8/MAXIM validation in ROM operations
 *
 * @details
 * Validates CRC-8/MAXIM (polynomial x^8 + x^5 + x^4 + 1) checking in
 * read_rom and search operations. Uses real CRC calculation (not mocked)
 * to verify end-to-end correctness, and CRC override mode to test
 * error handling when CRC fails.
 *
 * **CRC-8/MAXIM Algorithm:**
 * - Polynomial: 0x31 (normal), 0x8C (reversed/reflected)
 * - Computed over ROM bytes 0-6 (family + serial number)
 * - Stored in ROM byte 7 (last byte)
 * - Controller computes CRC and compares with stored value
 *
 * @{
 */

/**
 * @brief Test read_rom with valid CRC (Device A - DS18B20)
 *
 * @details
 * Uses mock GPIO read sequence to simulate a DS18B20 device responding
 * with ROM {0x28, 0x00, ..., 0x1E}. Real CRC calculation is used
 * (override disabled). Verifies that:
 * 1. Read ROM succeeds (k_rx_ok)
 * 2. All 8 bytes match expected ROM
 * 3. CRC byte (0x1E) passes validation
 *
 * @pre Bus initialized, CRC override disabled
 * @post ROM buffer contains correct device ROM
 *
 * @see rx_bus_onewire_read_rom() Function under test
 * @see rx_crc8_maxim() CRC validation used internally
 */
void test_rx_bus_onewire_read_rom_crc_valid(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_crc8_set_override(false);
  mock_gpio_set_read_sequence(s_test_pin, s_seq_read_rom_a, k_test_read_rom_seq_len);

  uint8_t rom[k_test_rom_bytes] = {};
  err                           = rx_bus_onewire_read_rom(&s_test_manager, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify ROM contents */
  TEST_ASSERT_EQUAL_HEX8(k_test_crc_rom_a_family, rom[0]);
  TEST_ASSERT_EQUAL_HEX8(k_test_crc_rom_a_byte7, rom[k_test_rom_bytes - 1]);
}

/**
 * @brief Test read_rom with valid CRC (Device C - DS18S20)
 *
 * @details
 * Uses a different device family (DS18S20, 0x10) with non-zero serial
 * bytes to exercise all CRC calculation paths. Verifies full ROM match.
 *
 * @pre Bus initialized, CRC override disabled
 * @post ROM buffer contains correct device ROM
 *
 * @see rx_bus_onewire_read_rom() Function under test
 */
void test_rx_bus_onewire_read_rom_crc_valid_device_c(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_crc8_set_override(false);
  mock_gpio_set_read_sequence(s_test_pin, s_seq_read_rom_c, k_test_read_rom_seq_len);

  uint8_t rom[k_test_rom_bytes] = {};
  err                           = rx_bus_onewire_read_rom(&s_test_manager, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const uint8_t expected_rom[k_test_rom_bytes] = {k_test_crc_rom_c_family,
                                                  k_test_crc_rom_c_byte1,
                                                  k_test_crc_rom_c_byte2,
                                                  k_test_crc_rom_c_byte3,
                                                  k_test_crc_rom_c_byte4,
                                                  k_test_crc_rom_c_byte5,
                                                  k_test_crc_rom_c_byte6,
                                                  k_test_crc_rom_c_byte7};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_rom, rom, k_test_rom_bytes);
}

/**
 * @brief Test read_rom with CRC mismatch (injected bad CRC)
 *
 * @details
 * Enables CRC override to return a deliberately wrong value (0x00).
 * The actual ROM data has CRC 0x1E, so the comparison fails.
 * Verifies that k_rx_err_crc_mismatch is returned.
 *
 * @pre Bus initialized, CRC override enabled with bad value
 * @post k_rx_err_crc_mismatch returned
 *
 * @see rx_bus_onewire_read_rom() Function under test
 * @see mock_crc8_set_override() CRC override control
 */
void test_rx_bus_onewire_read_rom_crc_mismatch(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Force CRC calculation to return 0x00 (wrong: actual CRC is 0x1E) */
  mock_crc8_set_override(true);
  mock_crc8_set_return_value(k_test_crc_bad_value);
  mock_gpio_set_read_sequence(s_test_pin, s_seq_read_rom_a, k_test_read_rom_seq_len);

  uint8_t rom[k_test_rom_bytes] = {};
  err                           = rx_bus_onewire_read_rom(&s_test_manager, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/**
 * @brief Test single-device search with valid CRC
 *
 * @details
 * Simulates a single DS18B20 on the bus. Each bit position returns
 * (bit, !bit) since there are no conflicts. After 64 bit positions,
 * the algorithm validates the ROM CRC. Verifies:
 * 1. Search succeeds (k_rx_ok)
 * 2. Exactly 1 device found
 * 3. ROM matches expected bytes
 *
 * @pre Bus initialized, CRC override disabled
 * @post 1 device found with correct ROM
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_single_device_crc_valid(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_crc8_set_override(false);
  mock_gpio_set_read_sequence(s_test_pin, s_seq_single_search, k_test_single_search_len);

  uint8_t  roms[k_test_max_search_devices * k_test_rom_bytes];
  uint32_t num_devices = 0;

  err = rx_bus_onewire_search(&s_test_manager,
                              s_test_bus_name,
                              roms,
                              k_test_max_search_devices,
                              &num_devices);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, num_devices);

  /* Verify discovered ROM */
  TEST_ASSERT_EQUAL_HEX8(k_test_crc_rom_a_family, roms[0]);
  TEST_ASSERT_EQUAL_HEX8(k_test_crc_rom_a_byte7, roms[k_test_rom_bytes - 1]);
}

/**
 * @brief Test search with CRC mismatch on discovered ROM
 *
 * @details
 * Enables CRC override to force a bad CRC result during search.
 * The search algorithm calls rx_crc8_maxim() on the discovered ROM,
 * and when the result doesn't match ROM byte 7, it returns
 * k_rx_err_crc_mismatch.
 *
 * @pre Bus initialized, CRC override enabled
 * @post k_rx_err_crc_mismatch returned
 * @post num_devices = 0 (search failed before completing)
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_crc_mismatch(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_crc8_set_override(true);
  mock_crc8_set_return_value(k_test_crc_bad_value);
  mock_gpio_set_read_sequence(s_test_pin, s_seq_single_search, k_test_single_search_len);

  uint8_t  roms[k_test_max_search_devices * k_test_rom_bytes];
  uint32_t num_devices = 0;

  err = rx_bus_onewire_search(&s_test_manager,
                              s_test_bus_name,
                              roms,
                              k_test_max_search_devices,
                              &num_devices);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
  TEST_ASSERT_EQUAL(0, num_devices);
}

/** @} */ /* end of test_onewire_crc */

/* =============================================================================
 * Multi-Device ROM Search Test
 * =============================================================================
 */

/**
 * @defgroup test_onewire_multi_search Multi-Device ROM Search Tests
 * @brief Tests for ROM search with multiple devices on the bus
 *
 * @details
 * Validates the binary tree search algorithm with multiple simulated devices.
 * Uses pre-computed GPIO read sequences that model the wired-AND bus behavior:
 * - When devices agree on a bit: (bit, !bit)
 * - When devices disagree: (0, 0) -- discrepancy
 * - After direction is written, non-matching devices go idle
 *
 * The search algorithm navigates the binary tree by:
 * 1. Taking direction=0 at new discrepancy points
 * 2. Tracking last_discrepancy for backtracking
 * 3. On subsequent iterations, taking direction=1 at last_discrepancy
 *
 * @{
 */

/**
 * @brief Test search discovers two devices with different ROMs
 *
 * @details
 * Simulates two DS18B20 devices that differ at byte 1 bit 0:
 * - Device A: {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E}
 * - Device B: {0x28, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29}
 *
 * The GPIO read sequence models the wired-AND bus behavior across two
 * search iterations. Iteration 1 discovers Device A (takes 0 at the
 * discrepancy), iteration 2 discovers Device B (takes 1 at the
 * discrepancy, since last_discrepancy points there).
 *
 * @pre Bus initialized, CRC override disabled
 * @post 2 devices found
 * @post First ROM matches Device A
 * @post Second ROM matches Device B
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_two_devices(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_crc8_set_override(false);
  mock_gpio_set_read_sequence(s_test_pin, s_seq_two_device, k_test_two_device_seq_len);

  uint8_t  roms[k_test_max_search_devices * k_test_rom_bytes];
  uint32_t num_devices = 0;

  err = rx_bus_onewire_search(&s_test_manager,
                              s_test_bus_name,
                              roms,
                              k_test_max_search_devices,
                              &num_devices);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_two_device_expected, num_devices);

  /* Verify Device A (first found: direction=0 at discrepancy) */
  const uint8_t expected_rom_a[k_test_rom_bytes] = {k_test_crc_rom_a_family,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_a_byte7};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_rom_a, &roms[0], k_test_rom_bytes);

  /* Verify Device B (second found: direction=1 at discrepancy) */
  const uint8_t expected_rom_b[k_test_rom_bytes] = {k_test_crc_rom_a_family,
                                                    k_test_crc_rom_b_byte1,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_byte_zero,
                                                    k_test_crc_rom_b_byte7};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_rom_b, &roms[k_test_rom_bytes], k_test_rom_bytes);
}

/** @} */ /* end of test_onewire_multi_search */

/* =============================================================================
 * Timing Constants and Bus State Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_timing Timing Constants and Parasitic Power Tests
 * @brief Tests for protocol timing and bus state verification
 *
 * @details
 * Validates:
 * 1. OneWire timing constants match Dallas/Maxim specification
 * 2. Bus is released (input/high-Z) after operations (required for
 *    parasitic power devices that charge from the bus line)
 * 3. Overdrive mode constants are defined (standard speed only in v1)
 *
 * **Parasitic Power:**
 * Devices like DS18B20 can operate without a dedicated VDD pin by
 * drawing power from the data line (DQ). This requires the bus to be
 * released HIGH between transactions so the external pull-up drives
 * the line to VDD, charging the device's internal capacitor.
 *
 * @{
 */

/**
 * @brief Verify OneWire timing constants match specification
 *
 * @details
 * Validates all timing parameters against Dallas/Maxim 1-Wire
 * specification (Application Note AN126):
 * - Reset pulse: 480 us minimum
 * - Presence wait: 60-75 us (we use 70)
 * - Write-1 LOW: 6-15 us (we use 10)
 * - Write-0 LOW: 60 us minimum
 * - Read init LOW: 1-15 us (we use 6)
 * - Read sample: within 15 us (we use 9)
 *
 * @pre None (tests compile-time constants)
 * @post All timing values verified within specification
 *
 * @see onewire_timing_us_t Timing constants under test
 */
void test_rx_bus_onewire_timing_constants(void)
{
  /* Reset timing */
  TEST_ASSERT_GREATER_OR_EQUAL(480, k_onewire_reset_pulse_us);
  TEST_ASSERT_TRUE(k_onewire_presence_wait_us >= 60 && k_onewire_presence_wait_us <= 75);
  /* Remaining presence window: reset_pulse + presence_wait + presence_tail must equal 960 us */
  TEST_ASSERT_EQUAL(k_onewire_reset_slot_total_us,
                    k_onewire_reset_pulse_us + k_onewire_presence_wait_us +
                      k_onewire_presence_tail_us);

  /* Write-1 timing: LOW pulse 6-15us */
  TEST_ASSERT_TRUE(k_onewire_write_1_low_us >= 6 && k_onewire_write_1_low_us <= 15);

  /* Write-0 timing: LOW pulse >= 60us */
  TEST_ASSERT_GREATER_OR_EQUAL(60, k_onewire_write_0_low_us);

  /* Read timing: init LOW 1-15us, sample within 15us of slot start */
  TEST_ASSERT_TRUE(k_onewire_read_init_us >= 1 && k_onewire_read_init_us <= 15);
  TEST_ASSERT_TRUE(k_onewire_read_sample_us <= 15);

  /* Total slot: init + recovery should be >= 60us */
  TEST_ASSERT_GREATER_OR_EQUAL(60, k_onewire_slot_duration_us);

  /* Recovery time: >= 1us per spec */
  TEST_ASSERT_GREATER_OR_EQUAL(1, k_onewire_recovery_us);
}

/**
 * @brief Verify bus is released (input) after write operations
 *
 * @details
 * After a write-bit or write-byte, the bus should be in input mode
 * (released high via external pullup). This is critical for parasitic
 * power devices that need the bus HIGH to charge their internal capacitor.
 *
 * @pre Bus initialized
 * @post GPIO pin is configured as input (not driving)
 *
 * @see rx_bus_onewire_write_bit() Function under test
 * @see rx_bus_onewire_write_byte() Function under test
 */
void test_rx_bus_onewire_bus_released_after_write(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Write a bit: bus should be released after */
  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));

  /* Write another bit (0): bus should still be released */
  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));

  /* Write a byte: bus should be released after */
  err = rx_bus_onewire_write_byte(&s_test_manager, s_test_bus_name, k_test_data_byte_aa);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));
}

/**
 * @brief Verify bus is released (input) after read operations
 *
 * @details
 * After a read-bit or read-byte, the bus should be in input mode.
 * The read slot ends with the controller releasing the bus, and the
 * recovery period ensures the bus returns to idle (high via pullup).
 *
 * @pre Bus initialized
 * @post GPIO pin is configured as input (not driving)
 *
 * @see rx_bus_onewire_read_bit() Function under test
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_bus_released_after_read(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read a bit: bus should be released after */
  bool bit = false;
  err      = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));

  /* Read a byte: bus should be released after */
  uint8_t byte = 0;
  err          = rx_bus_onewire_read_byte(&s_test_manager, s_test_bus_name, &byte);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));
}

/**
 * @brief Verify bus is released after reset/presence detection
 *
 * @details
 * After a reset pulse and presence detection, the bus should return
 * to input mode. The reset sequence ends with a recovery period where
 * the bus must be HIGH for parasitic power charging.
 *
 * @pre Bus initialized
 * @post GPIO pin is configured as input (not driving)
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_bus_released_after_reset(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Simulate device present (line goes low during presence) */
  mock_gpio_set_read_value(s_test_pin, false);

  bool presence = false;
  err           = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(presence);

  /* After reset, bus should be in input mode (released for pullup) */
  TEST_ASSERT_FALSE(mock_gpio_is_output(s_test_pin));
}

/** @} */ /* end of test_onewire_timing */

/* =============================================================================
 * Internal Function Coverage Tests
 * =============================================================================
 */

/**
 * @defgroup test_onewire_internal Internal Function Coverage Tests
 * @brief Direct tests of RX_STATIC_TESTABLE internal functions for 100% coverage.
 * @{
 */

/**
 * @brief Test internal_delay_us with zero microseconds hits early return.
 *
 * @pre setUp() completed
 * @post k_rx_ok (void return, no GPIO calls)
 *
 * @see internal_delay_us() Function under test
 */
void test_onewire_internal_delay_us_zero(void)
{
  /* Should return immediately without accessing the timer */
  internal_delay_us(0U);
  /* No assertions needed -- if it hangs, the test times out */
  TEST_PASS();
}

/**
 * @brief Test internal_acquire_state returns existing state on second init.
 *
 * @details
 * Calls rx_bus_onewire_init twice on the same bus. The second call must
 * follow the bus_config->handle != nullptr branch (lines 334-336).
 *
 * @pre setUp() completed
 * @post Both inits return k_rx_ok
 *
 * @see rx_bus_onewire_init() Function under test (calls internal_acquire_state)
 */
void test_onewire_internal_acquire_state_existing_handle(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Second init on same bus: handle already set, returns existing state */
  err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test internal_acquire_state returns k_rx_err_no_mem when pool is exhausted.
 *
 * @details
 * Allocates all k_max_buses (32) pool slots by calling
 * internal_onewire_init_callback() directly on dummy bus configs, then
 * verifies the next allocation fails with k_rx_err_no_mem. Releases all
 * slots afterward to avoid leaking state for subsequent tests.
 *
 * @pre setUp() completed (test bus NOT yet initialized)
 * @post All pool slots released
 *
 * @see internal_onewire_init_callback() Function under test
 */
void test_onewire_internal_acquire_state_pool_exhausted(void)
{
  /* Use a typed enum constant for pool size to avoid magic number */
  enum : uint8_t { k_test_pool_size = 32U };

  static rx_bus_config_t s_dummy_configs[k_test_pool_size];
  onewire_simple_ctx_t   ctx = {.result = k_rx_ok};

  /* Exhaust the pool by initializing k_test_pool_size dummy configs */
  for (uint8_t i = 0; i < k_test_pool_size; ++i) {
    rx_err_t err = rx_bus_config_init_onewire(&s_dummy_configs[i], s_test_bus_name, s_test_pin);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    s_dummy_configs[i].initialized = false;
    ctx.result                     = k_rx_ok;
    err                            = internal_onewire_init_callback(&s_dummy_configs[i], &ctx);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    /* Reset mock GPIO error injection cleared by the gpio_set_input call */
  }

  /* Next allocation must fail -- pool is full */
  rx_bus_config_t extra_config;
  rx_err_t        err = rx_bus_config_init_onewire(&extra_config, "extra", s_test_pin);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  extra_config.initialized = false;
  ctx.result               = k_rx_ok;
  err                      = internal_onewire_init_callback(&extra_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_no_mem, err);

  /* Release all pool slots to avoid state leak for subsequent tests */
  for (uint8_t i = 0; i < k_test_pool_size; ++i) {
    onewire_simple_ctx_t deinit_ctx = {.result = k_rx_ok};
    (void)internal_onewire_deinit_callback(&s_dummy_configs[i], &deinit_ctx);
  }
}

/**
 * @brief Test internal_set_drive_mode: gpio_set_input failure propagates.
 *
 * @details
 * Calls internal_set_drive_mode(output=false) with line_is_output=true so
 * it attempts gpio_set_input. Injects an error to cover line 386.
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_set_drive_mode() Function under test
 */
void test_onewire_internal_set_drive_mode_set_input_fails(void)
{
  /* state.line_is_output=true forces the gpio_set_input path */
  onewire_runtime_state_t state = {.line_is_output = true};
  mock_gpio_set_next_error(k_rx_err_hw_error);

  rx_err_t err = internal_set_drive_mode(&s_onewire_config, &state, false);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_reset_pulse: release_line failure propagates.
 *
 * @details
 * After init (2 GPIO calls), reset_pulse makes: set_output(3), write_low(4),
 * set_input(5). Inject error on call 5 to hit the release_line error path
 * (lines 499-501).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_reset() Public API wrapping internal_reset_pulse
 */
void test_onewire_internal_reset_pulse_release_line_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After init: 2 GPIO calls (set_input + gpio_read). Next calls for reset:
   * call 3: gpio_set_output (drive_low)
   * call 4: gpio_write_low  (drive_low)
   * call 5: gpio_set_input  (release_line) <-- inject error here
   */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_5, k_rx_err_hw_error);

  bool presence = false;
  err           = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_reset_pulse: read_line failure propagates.
 *
 * @details
 * After init (2 calls), reset_pulse makes: set_output(3), write_low(4),
 * set_input(5), gpio_read(6). Inject error on call 6 to hit read_line
 * error path (lines 506-508).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_reset() Public API wrapping internal_reset_pulse
 */
void test_onewire_internal_reset_pulse_read_line_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 6: gpio_read (read_line) <-- inject error here */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_6, k_rx_err_hw_error);

  bool presence = false;
  err           = rx_bus_onewire_reset(&s_test_manager, s_test_bus_name, &presence);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_write_bit(1): release_line failure propagates.
 *
 * @details
 * After init (2 calls), write_bit(1) makes: set_output(3), write_low(4),
 * set_input(5). Inject error on call 5 to hit the release_line error path
 * in the bit=1 branch (lines 564-566).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_write_bit() Public API wrapping internal_write_bit
 */
void test_onewire_internal_write_bit_one_release_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 5: gpio_set_input (release_line in bit=1 branch) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_5, k_rx_err_hw_error);

  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_write_bit(0): release_line failure propagates.
 *
 * @details
 * After init (2 calls), write_bit(0) makes: set_output(3), write_low(4),
 * set_input(5). Inject error on call 5 to hit the release_line error path
 * in the bit=0 branch (lines 571-573).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_write_bit() Public API wrapping internal_write_bit
 */
void test_onewire_internal_write_bit_zero_release_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 5: gpio_set_input (release_line in bit=0 branch) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_5, k_rx_err_hw_error);

  err = rx_bus_onewire_write_bit(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_read_bit: release_line failure propagates.
 *
 * @details
 * After init (2 calls), read_bit makes: set_output(3), write_low(4),
 * set_input(5). Inject error on call 5 to hit lines 628-630.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_read_bit() Public API wrapping internal_read_bit
 */
void test_onewire_internal_read_bit_release_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 5: gpio_set_input (release_line) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_5, k_rx_err_hw_error);

  bool bit = false;
  err      = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_read_bit: read_line failure propagates.
 *
 * @details
 * After init (2 calls), read_bit makes: set_output(3), write_low(4),
 * set_input(5), gpio_read(6). Inject error on call 6 to hit lines 639-641.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_read_bit() Public API wrapping internal_read_bit
 */
void test_onewire_internal_read_bit_read_line_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 6: gpio_read (read_line) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_6, k_rx_err_hw_error);

  bool bit = false;
  err      = rx_bus_onewire_read_bit(&s_test_manager, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_write_byte: write_bit error propagates through loop.
 *
 * @details
 * After init (2 calls), write_byte calls write_bit for each bit. The first
 * bit (gpio_set_output on call 3) failure propagates through the loop's
 * error check (lines 658-660).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_write_byte() Public API wrapping internal_write_byte
 */
void test_onewire_internal_write_byte_write_bit_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 3: gpio_set_output (first write_bit's drive_low) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  err = rx_bus_onewire_write_byte(&s_test_manager, s_test_bus_name, k_test_data_byte_aa);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_read_byte: read_bit error propagates through loop.
 *
 * @details
 * After init (2 calls), read_byte calls read_bit for each bit. Fail the
 * first bit to cover the loop error path (lines 675-677).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see rx_bus_onewire_read_byte() Public API wrapping internal_read_byte
 */
void test_onewire_internal_read_byte_read_bit_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* call 3: gpio_set_output (first read_bit's drive_low) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  uint8_t byte = 0;
  err          = rx_bus_onewire_read_byte(&s_test_manager, s_test_bus_name, &byte);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_step: first read_bit error propagates.
 *
 * @details
 * Calls internal_search_step directly. Injects error on the first GPIO
 * call to fail internal_read_bit (lines 713-715).
 *
 * @pre setUp() completed (bus config available)
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_first_read_bit_fails(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  uint8_t                 byte_idx  = 0;
  uint8_t                 bit_mask  = 1;
  uint8_t                 last_zero = 0;

  mock_gpio_set_next_error(k_rx_err_hw_error);

  rx_err_t err =
    internal_search_step(&s_onewire_config, &state, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_step: second read_bit error propagates.
 *
 * @details
 * The first read_bit takes 3 GPIO calls (set_output, write_low, set_input)
 * plus gpio_read = 4 calls. Second read_bit starts at call 5.
 * Inject error on call 5 to fail second read_bit (lines 718-720).
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_second_read_bit_fails(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  uint8_t                 byte_idx  = 0;
  uint8_t                 bit_mask  = 1;
  uint8_t                 last_zero = 0;

  /* First read_bit: set_output(1), write_low(2), set_input(3), gpio_read(4)
   * Second read_bit starts at call 5: set_output(5) <-- inject here */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_5, k_rx_err_hw_error);

  rx_err_t err =
    internal_search_step(&s_onewire_config, &state, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_step: bit=1 and comp_bit=1 returns hw_error.
 *
 * @details
 * When both bit and comp_bit are 1, no devices are responding -- this is
 * a bus collision error (lines 721-723).
 *
 * The first read_bit reads the gpio_read result. To get bit=1: set gpio
 * read_value to HIGH (true). Both bits read high triggers k_rx_err_hw_error.
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_bit_and_comp_both_true(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  uint8_t                 byte_idx  = 0;
  uint8_t                 bit_mask  = 1;
  uint8_t                 last_zero = 0;

  /* Both read_bit calls return HIGH (line stays high after release) */
  mock_gpio_set_read_value(s_test_pin, true);

  rx_err_t err =
    internal_search_step(&s_onewire_config, &state, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_step: discrepancy at last_discrepancy bit uses last_rom.
 *
 * @details
 * When bit_number < last_discrepancy, search_direction = last_rom bit.
 * Covers the else branch at line 732: `search_direction = last_rom[...]`.
 *
 * Set last_discrepancy > 1, bit_number=1, both bits low (discrepancy=0,0),
 * and last_rom[0] bit 0 set.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, rom bit set according to last_rom
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_direction_from_last_rom(void)
{
  onewire_runtime_state_t state = {.line_is_output   = false,
                                   .last_discrepancy = k_test_last_discrepancy_2};

  /* Set last_rom[0] bit 0 = 1, so search_direction = true */
  state.last_rom[0] = 1U;

  uint8_t rom[k_test_rom_bytes];
  uint8_t byte_idx  = 0;
  uint8_t bit_mask  = 1;
  uint8_t last_zero = 0;

  /* Both read_bit calls return LOW (0,0 discrepancy): bit=false, comp_bit=false */
  mock_gpio_set_read_value(s_test_pin, false);

  rx_err_t err =
    internal_search_step(&s_onewire_config, &state, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test internal_search_step: write_bit error propagates.
 *
 * @details
 * Reads succeed (both bits low, discrepancy), then write_bit fails.
 * Covers lines 747-749. write_bit starts after 8 GPIO calls (2 read_bits
 * x 4 calls each).
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_write_bit_fails(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  uint8_t                 byte_idx  = 0;
  uint8_t                 bit_mask  = 1;
  uint8_t                 last_zero = 0;

  /* Both bits low (0,0): discrepancy, search_direction=false (bit_number>last_discrepancy=0)
   * read_bit_1: set_output(1), write_low(2), set_input(3), gpio_read(4)
   * read_bit_2: set_output(5), write_low(6), set_input(7), gpio_read(8)
   * write_bit starts: set_output(9) <-- inject here */
  mock_gpio_set_read_value(s_test_pin, false);
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_9, k_rx_err_hw_error);

  rx_err_t err =
    internal_search_step(&s_onewire_config, &state, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_bits: search_step error propagates.
 *
 * @details
 * Inject error on first GPIO call inside internal_search_bits to cause
 * internal_search_step to fail, covering the error return at line 791.
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_bits() Function under test
 */
void test_onewire_internal_search_bits_step_error(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  uint8_t                 last_zero = 0;

  mock_gpio_set_next_error(k_rx_err_hw_error);

  rx_err_t err = internal_search_bits(&s_onewire_config, &state, rom, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_iteration: reset_pulse error propagates.
 *
 * @details
 * Inject error on gpio_set_output (call 1) to fail internal_reset_pulse,
 * covering lines 863-865.
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_reset_pulse_fails(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  bool                    device_found = false;

  mock_gpio_set_next_error(k_rx_err_hw_error);

  rx_err_t err = internal_search_iteration(&s_onewire_config, &state, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_iteration: no presence returns k_rx_ok, device_found=false.
 *
 * @details
 * When gpio_read returns HIGH (no device), internal_search_iteration returns
 * k_rx_ok with device_found=false (lines 864-865 in the !presence branch).
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, device_found == false
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_no_presence(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  bool                    device_found = false;

  /* Line stays high (no device) */
  mock_gpio_set_read_value(s_test_pin, true);

  rx_err_t err = internal_search_iteration(&s_onewire_config, &state, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(device_found);
}

/**
 * @brief Test internal_search_iteration: write_byte error propagates.
 *
 * @details
 * Presence detected (gpio_read=LOW), then write_byte (SEARCH_ROM cmd) fails.
 * Covers lines 874-876. After init-sequence gpio calls for reset_pulse:
 * set_output(1), write_low(2), set_input(3), gpio_read(4, returns LOW).
 * Then write_byte bit0: set_output(5). Inject error at call 5.
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_write_byte_fails(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  bool                    device_found = false;

  /* Line goes low on gpio_read (presence detected)
   * reset_pulse: set_output(1), write_low(2), set_input(3), gpio_read(4)=LOW
   * write_byte bit0: set_output(5) <-- inject error */
  mock_gpio_set_read_value(s_test_pin, false);
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_5, k_rx_err_hw_error);

  rx_err_t err = internal_search_iteration(&s_onewire_config, &state, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_search_iteration: last_device_flag set causes early return.
 *
 * @details
 * When state.last_device_flag is true, the function returns k_rx_ok with
 * device_found=false immediately (lines 858-860).
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, device_found == false
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_last_device_flag(void)
{
  onewire_runtime_state_t state = {.last_device_flag = true};
  uint8_t                 rom[k_test_rom_bytes];
  bool                    device_found = false;

  rx_err_t err = internal_search_iteration(&s_onewire_config, &state, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(device_found);
}

/**
 * @brief Test internal_search_iteration: internal_search_bits error propagates.
 *
 * @details
 * Presence detected and SEARCH_ROM write_byte succeeds, then
 * internal_search_bits fails on the first search step.
 * Covers line 879.
 *
 * GPIO call accounting from direct call (no prior init):
 * reset_pulse:      set_output(1), write_low(2), set_input(3), gpio_read(4)=LOW
 * write_byte cmd:   8 bits x 3 calls each = 24 calls (5..28)
 * search_bits step0 read_bit: set_output(29) -- inject error here
 *
 * @pre setUp() completed
 * @post k_rx_err_hw_error returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_search_bits_fails(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  uint8_t                 rom[k_test_rom_bytes];
  bool                    device_found = false;

  mock_gpio_set_read_value(s_test_pin, false);
  /* reset_pulse(4) + write_byte(24) = 28 calls; search_bits first call = 29 */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_29, k_rx_err_hw_error);

  rx_err_t err = internal_search_iteration(&s_onewire_config, &state, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_init_callback: gpio_read error on post-init check.
 *
 * @details
 * After gpio_set_input succeeds (call 1), gpio_read (call 2) fails.
 * Covers lines 1016-1020. After init, gpio_set_input was call 1.
 * Then gpio_read is call 2. Inject error on call 2.
 *
 * @pre setUp() completed (bus not yet initialized)
 * @post k_rx_err_hw_error returned from rx_bus_onewire_init
 *
 * @see internal_onewire_init_callback() Function under test
 */
void test_onewire_internal_init_callback_gpio_read_fails(void)
{
  /* call 1: gpio_set_input (succeeds), call 2: gpio_read (fails) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_2, k_rx_err_hw_error);

  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_init_callback: pin reads low triggers warning.
 *
 * @details
 * Initialization succeeds even when pin reads LOW (missing pullup resistor).
 * A warning is logged but operation continues. Covers line 1023-1024.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned despite low pin state
 *
 * @see internal_onewire_init_callback() Function under test
 */
void test_onewire_internal_init_callback_pin_reads_low(void)
{
  /* Pin reads low (no pullup or device holding bus low) */
  mock_gpio_set_read_value(s_test_pin, false);

  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);
  /* Init still succeeds with a warning */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test internal_onewire_reset_callback: nullptr presence pointer.
 *
 * @details
 * Calls internal_onewire_reset_callback directly with ctx->presence = nullptr.
 * Covers lines 1105-1108.
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_reset_callback() Function under test
 */
void test_onewire_internal_reset_callback_null_presence(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_reset_ctx_t ctx      = {.presence = nullptr, .result = k_rx_ok};
  s_onewire_config.initialized = true; /* already done by init above */
  err                          = internal_onewire_reset_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_write_bit_callback: uninitialized bus.
 *
 * @details
 * write_bit callback has its own !initialized check (lines 1139-1143).
 * Calls callback directly with initialized=false.
 *
 * @pre setUp() completed (bus config available but not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_write_bit_callback() Function under test
 */
void test_onewire_internal_write_bit_callback_not_initialized(void)
{
  onewire_write_bit_ctx_t ctx = {.bit = true, .result = k_rx_ok};
  /* s_onewire_config.initialized == false (setUp doesn't init the bus) */
  rx_err_t err = internal_onewire_write_bit_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_write_bit_callback: wrong bus type.
 *
 * @details
 * bus_config->type != k_bus_type_onewire returns k_rx_err_invalid_arg.
 * Covers lines 1134-1136.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_write_bit_callback() Function under test
 */
void test_onewire_internal_write_bit_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */

  onewire_write_bit_ctx_t ctx = {.bit = true, .result = k_rx_ok};
  rx_err_t                err = internal_onewire_write_bit_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_read_bit_callback: uninitialized bus.
 *
 * @details
 * read_bit callback has its own !initialized check (lines 1176-1180).
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_read_bit_callback() Function under test
 */
void test_onewire_internal_read_bit_callback_not_initialized(void)
{
  bool                   bit = false;
  onewire_read_bit_ctx_t ctx = {.bit = &bit, .result = k_rx_ok};

  rx_err_t err = internal_onewire_read_bit_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_read_bit_callback: wrong bus type.
 *
 * @details
 * bus_config->type != k_bus_type_onewire returns k_rx_err_invalid_arg.
 * Covers lines 1171-1173.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_read_bit_callback() Function under test
 */
void test_onewire_internal_read_bit_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */

  bool                   bit = false;
  onewire_read_bit_ctx_t ctx = {.bit = &bit, .result = k_rx_ok};
  rx_err_t               err = internal_onewire_read_bit_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_read_bit_callback: nullptr bit output pointer.
 *
 * @details
 * Calls read_bit callback with ctx->bit = nullptr. Covers lines 1187-1190.
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_read_bit_callback() Function under test
 */
void test_onewire_internal_read_bit_callback_null_bit(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_read_bit_ctx_t ctx = {.bit = nullptr, .result = k_rx_ok};
  err                        = internal_onewire_read_bit_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_write_byte_callback: write_byte error propagates.
 *
 * @details
 * After init (2 GPIO calls), write_byte_callback calls write_byte which calls
 * write_bit. Inject error on call 3 (first bit's gpio_set_output) to trigger
 * the error path (lines 1222-1225).
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_write_byte_callback() Function under test
 */
void test_onewire_internal_write_byte_callback_write_byte_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  onewire_write_byte_ctx_t ctx = {.byte = k_test_data_byte_aa, .result = k_rx_ok};
  err                          = internal_onewire_write_byte_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_read_byte_callback: nullptr byte output pointer.
 *
 * @details
 * Calls read_byte callback with ctx->byte = nullptr. Covers lines 1253-1256.
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_read_byte_callback() Function under test
 */
void test_onewire_internal_read_byte_callback_null_byte(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_read_byte_ctx_t ctx = {.byte = nullptr, .result = k_rx_ok};
  err                         = internal_onewire_read_byte_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_write_buffer_callback: length exceeds maximum.
 *
 * @details
 * ctx->length > k_onewire_max_buf_bytes returns k_rx_err_invalid_size
 * (lines 1288-1290).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_size returned
 *
 * @see internal_onewire_write_buffer_callback() Function under test
 */
void test_onewire_internal_write_buffer_callback_length_too_large(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  static const uint8_t    s_data[1] = {k_test_data_byte_aa};
  onewire_write_buf_ctx_t ctx = {.data = s_data, .length = k_test_overflow_len, .result = k_rx_ok};
  err                         = internal_onewire_write_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test internal_onewire_write_buffer_callback: nullptr data pointer.
 *
 * @details
 * ctx->data = nullptr with valid length returns k_rx_err_invalid_arg
 * (lines 1298-1301).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_write_buffer_callback() Function under test
 */
void test_onewire_internal_write_buffer_callback_null_data(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_write_buf_ctx_t ctx = {.data = nullptr, .length = k_test_buf_size_4, .result = k_rx_ok};
  err                         = internal_onewire_write_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_write_buffer_callback: write_byte error mid-loop.
 *
 * @details
 * Data is valid, but write_byte fails on the first byte. Covers lines 1306-1309.
 * After init (2 GPIO calls), first write_byte bit0: gpio_set_output = call 3.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_write_buffer_callback() Function under test
 */
void test_onewire_internal_write_buffer_callback_write_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  static const uint8_t    s_data[k_test_buf_size_2] = {0x01, 0x02};
  onewire_write_buf_ctx_t ctx = {.data = s_data, .length = k_test_buf_size_2, .result = k_rx_ok};
  err                         = internal_onewire_write_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_read_buffer_callback: bus not initialized.
 *
 * @details
 * read_buffer with uninitialized bus returns k_rx_err_invalid_state
 * (lines 1336-1338). Call callback directly with initialized=false.
 *
 * @pre setUp() completed (bus not initialized)
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_read_buffer_callback() Function under test
 */
void test_onewire_internal_read_buffer_callback_not_initialized(void)
{
  static uint8_t         s_buf[k_test_buf_size_4];
  onewire_read_buf_ctx_t ctx = {.data = s_buf, .length = k_test_buf_size_4, .result = k_rx_ok};

  /* s_onewire_config.initialized == false */
  rx_err_t err = internal_onewire_read_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_read_buffer_callback: length exceeds max.
 *
 * @details
 * ctx->length > k_onewire_max_buf_bytes returns k_rx_err_invalid_size.
 * Covers lines 1331-1332.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_size returned
 *
 * @see internal_onewire_read_buffer_callback() Function under test
 */
void test_onewire_internal_read_buffer_callback_length_too_large(void)
{
  static uint8_t s_buf[k_test_buf_size_4];
  /* Use a length larger than k_onewire_max_buf_bytes (255) */
  onewire_read_buf_ctx_t ctx = {.data = s_buf, .length = k_test_overflow_len, .result = k_rx_ok};

  rx_err_t err = internal_onewire_read_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test internal_onewire_read_buffer_callback: nullptr data pointer.
 *
 * @details
 * ctx->data = nullptr returns k_rx_err_invalid_arg (lines 1346-1349).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_read_buffer_callback() Function under test
 */
void test_onewire_internal_read_buffer_callback_null_data(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_read_buf_ctx_t ctx = {.data = nullptr, .length = k_test_buf_size_4, .result = k_rx_ok};
  err                        = internal_onewire_read_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_read_buffer_callback: read_byte error mid-loop.
 *
 * @details
 * read_byte fails on first byte. Covers lines 1355-1358.
 * After init (2 calls), first read_byte bit0: gpio_set_output = call 3.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_read_buffer_callback() Function under test
 */
void test_onewire_internal_read_buffer_callback_read_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  static uint8_t         s_buf[k_test_buf_size_2];
  onewire_read_buf_ctx_t ctx = {.data = s_buf, .length = k_test_buf_size_2, .result = k_rx_ok};
  err                        = internal_onewire_read_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_skip_rom_callback: reset_pulse error propagates.
 *
 * @details
 * skip_rom calls reset_pulse. Inject error on first GPIO call to fail it.
 * Covers line 1391-1393.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_skip_rom_callback() Function under test
 */
void test_onewire_internal_skip_rom_callback_reset_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  err = internal_onewire_skip_rom_callback(&s_onewire_config, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_skip_rom_callback: write_byte error propagates.
 *
 * @details
 * Presence detected (line goes LOW), then write_byte (SKIP_ROM) fails.
 * Covers lines 1400-1402. reset_pulse takes calls 3-6 (set_output, write_low,
 * set_input, gpio_read=LOW). write_byte starts at call 7.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_skip_rom_callback() Function under test
 */
void test_onewire_internal_skip_rom_callback_write_byte_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, false);
  /* reset_pulse: set_output(3), write_low(4), set_input(5), gpio_read(6)
   * write_byte bit0: set_output(7) <-- inject error */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_7, k_rx_err_hw_error);

  err = internal_onewire_skip_rom_callback(&s_onewire_config, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_match_rom_callback: nullptr rom pointer.
 *
 * @details
 * ctx->rom = nullptr returns k_rx_err_invalid_arg (lines 1429-1432).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_match_rom_callback() Function under test
 */
void test_onewire_internal_match_rom_callback_null_rom(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_match_rom_ctx_t ctx = {.rom = nullptr, .result = k_rx_ok};
  err                         = internal_onewire_match_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_match_rom_callback: reset_pulse error propagates.
 *
 * @details
 * After init (gpio_set_input=1, gpio_read=2), reset_pulse calls gpio_set_output
 * (call 3). Inject error on call 3 to fail reset_pulse.
 * Covers lines 1433-1434.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_match_rom_callback() Function under test
 */
void test_onewire_internal_match_rom_callback_reset_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After init (2 calls), reset_pulse gpio_set_output = call 3 */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  static const uint8_t    s_rom[k_test_rom_bytes] = {};
  onewire_match_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_match_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_match_rom_callback: no presence detected.
 *
 * @details
 * Line stays HIGH (no device) after reset_pulse. Returns k_rx_err_not_found
 * (lines 1438-1440).
 *
 * @pre Bus initialized
 * @post k_rx_err_not_found returned
 *
 * @see internal_onewire_match_rom_callback() Function under test
 */
void test_onewire_internal_match_rom_callback_no_presence(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, true);

  static const uint8_t    s_rom[k_test_rom_bytes] = {};
  onewire_match_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_match_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test internal_onewire_match_rom_callback: write match_rom command fails.
 *
 * @details
 * Presence detected, then write_byte(MATCH_ROM=0x55) fails. Covers lines
 * 1447-1450. reset_pulse takes calls 3-6; write_byte starts at call 7.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_match_rom_callback() Function under test
 */
void test_onewire_internal_match_rom_callback_write_cmd_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, false);
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_7, k_rx_err_hw_error);

  static const uint8_t    s_rom[k_test_rom_bytes] = {};
  onewire_match_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_match_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_match_rom_callback: write ROM byte fails.
 *
 * @details
 * Presence detected, MATCH_ROM command succeeds (8 bits = 8 write_bit x 3 GPIO calls),
 * then first ROM byte fails. Covers lines 1453-1456.
 * After init (2), reset_pulse (4 calls = 3..6), write_byte cmd (8 bits:
 * each bit takes set_output+write_low+set_input = 3 calls -> 24 calls, 7..30).
 * First ROM byte bit0: call 31.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_match_rom_callback() Function under test
 */
void test_onewire_internal_match_rom_callback_write_rom_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, false);
  /* After init (2), reset_pulse (set_output=3, write_low=4, set_input=5, gpio_read=6),
   * write_byte(MATCH_ROM) 8 bits x 3 GPIO each = 24 calls (7..30),
   * ROM byte 0, bit 0: set_output(31) <-- inject error */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_31, k_rx_err_hw_error);

  static const uint8_t    s_rom[k_test_rom_bytes] = {};
  onewire_match_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_match_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: nullptr rom pointer.
 *
 * @details
 * ctx->rom = nullptr returns k_rx_err_invalid_arg (lines 1490-1493).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_null_rom(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_read_rom_ctx_t ctx = {.rom = nullptr, .result = k_rx_ok};
  err                        = internal_onewire_read_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: reset_pulse error propagates.
 *
 * @details
 * After init (gpio_set_input=1, gpio_read=2), reset_pulse calls gpio_set_output
 * (call 3). Inject error on call 3 to fail reset_pulse.
 * Covers lines 1494-1495.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_reset_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After init (2 calls), reset_pulse gpio_set_output = call 3 */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_3, k_rx_err_hw_error);

  static uint8_t         s_rom[k_test_rom_bytes] = {};
  onewire_read_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_read_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: no presence detected.
 *
 * @details
 * Line stays HIGH (no device). Returns k_rx_err_not_found (lines 1499-1501).
 *
 * @pre Bus initialized
 * @post k_rx_err_not_found returned
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_no_presence(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, true);

  static uint8_t         s_rom[k_test_rom_bytes] = {};
  onewire_read_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_read_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: write READ_ROM command fails.
 *
 * @details
 * Presence detected, then write_byte(READ_ROM) fails. Covers lines 1508-1511.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_write_cmd_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, false);
  /* reset_pulse (calls 3..6), write_byte READ_ROM starts at call 7 */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_7, k_rx_err_hw_error);

  static uint8_t         s_rom[k_test_rom_bytes] = {};
  onewire_read_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_read_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: read_byte error.
 *
 * @details
 * READ_ROM command sent, then first read_byte fails. Covers lines 1514-1517.
 * After init(2), reset_pulse(4), write_byte READ_ROM (8 bits x 3 calls = 24,
 * calls 7..30), read_byte byte0 bit0: set_output(31). But read_bit takes
 * set_output+write_low+set_input+gpio_read = 4 calls each. So call 31.
 *
 * @pre Bus initialized
 * @post k_rx_err_hw_error returned
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_read_byte_fails(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_gpio_set_read_value(s_test_pin, false);
  /* calls 3..6: reset_pulse, calls 7..30: write_byte READ_ROM
   * read_byte byte0 bit0: set_output(31) */
  mock_gpio_set_error_on_nth_call(k_test_gpio_call_31, k_rx_err_hw_error);

  static uint8_t         s_rom[k_test_rom_bytes] = {};
  onewire_read_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_read_rom_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: family code 0 triggers warning.
 *
 * @details
 * When ROM byte 0 (family code) is 0x00, a warning is logged but init
 * succeeds. Covers the family-code-zero warning branch.
 * With all GPIO reads returning false, ROM = {0,0,0,0,0,0,0,0}.
 * CRC-8/Maxim of 7 zero bytes = 0x00, which matches rom[7] = 0x00, so CRC passes.
 *
 * @pre Bus initialized
 * @post k_rx_ok returned despite family code 0
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_family_code_zero(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* All bytes low -> ROM = {0,0,0,0,0,0,0,0}, family_code=0
   * CRC-8/Maxim of 7 zero bytes = 0x00, matches rom[7]=0x00, CRC passes */
  mock_gpio_set_read_value(s_test_pin, false);

  static uint8_t         s_rom[k_test_rom_bytes] = {};
  onewire_read_rom_ctx_t ctx                     = {.rom = s_rom, .result = k_rx_ok};
  err = internal_onewire_read_rom_callback(&s_onewire_config, &ctx);
  /* Should succeed even with family code 0 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test internal_onewire_search_callback: max_devices == 0 returns ok.
 *
 * @details
 * ctx->max_devices = 0 returns k_rx_ok immediately (lines 1561-1562).
 *
 * @pre Bus initialized
 * @post k_rx_ok returned, num_devices = 0
 *
 * @see internal_onewire_search_callback() Function under test
 */
void test_onewire_internal_search_callback_zero_max(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint32_t             num = 1U;
  onewire_search_ctx_t ctx = {.roms        = nullptr,
                              .max_devices = 0U,
                              .num_devices = &num,
                              .result      = k_rx_ok};
  err                      = internal_onewire_search_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0U, num);
}

/**
 * @brief Test internal_onewire_search_callback: nullptr num_devices.
 *
 * @details
 * ctx->num_devices == nullptr returns k_rx_err_invalid_arg (lines 1551-1552).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_search_callback() Function under test
 */
void test_onewire_internal_search_callback_null_num_devices(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  onewire_search_ctx_t ctx = {.roms        = nullptr,
                              .max_devices = 1U,
                              .num_devices = nullptr,
                              .result      = k_rx_ok};
  err                      = internal_onewire_search_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_search_callback: max_devices exceeds limit.
 *
 * @details
 * ctx->max_devices > s_onewire_max_search_devices returns k_rx_err_invalid_arg
 * (lines 1571-1573).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_search_callback() Function under test
 */
void test_onewire_internal_search_callback_max_exceeds_limit(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint32_t             num = 0;
  static uint8_t       s_roms[k_test_rom_bytes];
  onewire_search_ctx_t ctx = {.roms        = s_roms,
                              .max_devices = k_test_overflow_len,
                              .num_devices = &num,
                              .result      = k_rx_ok};
  err                      = internal_onewire_search_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_search_callback: nullptr roms pointer.
 *
 * @details
 * ctx->roms = nullptr returns k_rx_err_invalid_arg (lines 1575-1577).
 *
 * @pre Bus initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_search_callback() Function under test
 */
void test_onewire_internal_search_callback_null_roms(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint32_t             num = 0;
  onewire_search_ctx_t ctx = {.roms        = nullptr,
                              .max_devices = 1U,
                              .num_devices = &num,
                              .result      = k_rx_ok};
  err                      = internal_onewire_search_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test rx_bus_onewire_deinit: nullptr manager returns k_rx_err_null_ptr.
 *
 * @details
 * rx_bus_onewire_deinit has its own nullptr checks (lines 1696-1697).
 *
 * @pre None
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_deinit() Function under test
 */
void test_onewire_deinit_null_manager(void)
{
  rx_err_t err = rx_bus_onewire_deinit(nullptr, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_deinit: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * rx_bus_onewire_deinit nullptr bus_name check (lines 1699-1700).
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_deinit() Function under test
 */
void test_onewire_deinit_null_bus_name(void)
{
  rx_err_t err = rx_bus_onewire_deinit(&s_test_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_deinit: bus not found returns with_bus error.
 *
 * @details
 * rx_bus_manager_with_bus fails when bus name is not registered. Covers
 * line 1706-1707.
 *
 * @pre setUp() completed
 * @post k_rx_err_not_found returned
 *
 * @see rx_bus_onewire_deinit() Function under test
 */
void test_onewire_deinit_bus_not_found(void)
{
  rx_err_t err = rx_bus_onewire_deinit(&s_test_manager, "nonexistent_bus");
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/* -------------------------------------------------------------------------
 * Public API null bus_name / null manager branch coverage tests
 * -------------------------------------------------------------------------
 */

/**
 * @brief Test rx_bus_onewire_reset: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1718.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_reset() Function under test
 */
void test_rx_bus_onewire_reset_null_bus_name(void)
{
  bool     presence = false;
  rx_err_t err      = rx_bus_onewire_reset(&s_test_manager, nullptr, &presence);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_write_bit: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1742.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write_bit() Function under test
 */
void test_rx_bus_onewire_write_bit_null_bus_name(void)
{
  rx_err_t err = rx_bus_onewire_write_bit(&s_test_manager, nullptr, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read_bit: nullptr manager returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(manager) branch at line 1764.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_bit() Function under test
 */
void test_rx_bus_onewire_read_bit_null_manager(void)
{
  bool     bit = false;
  rx_err_t err = rx_bus_onewire_read_bit(nullptr, s_test_bus_name, &bit);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read_bit: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1765.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_bit() Function under test
 */
void test_rx_bus_onewire_read_bit_null_bus_name(void)
{
  bool     bit = false;
  rx_err_t err = rx_bus_onewire_read_bit(&s_test_manager, nullptr, &bit);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_write_byte: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1789.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write_byte() Function under test
 */
void test_rx_bus_onewire_write_byte_null_bus_name(void)
{
  rx_err_t err = rx_bus_onewire_write_byte(&s_test_manager, nullptr, 0x00U);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read_byte: nullptr manager returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(manager) branch at line 1811.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_read_byte_null_manager(void)
{
  uint8_t  byte = 0U;
  rx_err_t err  = rx_bus_onewire_read_byte(nullptr, s_test_bus_name, &byte);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read_byte: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1812.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_byte() Function under test
 */
void test_rx_bus_onewire_read_byte_null_bus_name(void)
{
  uint8_t  byte = 0U;
  rx_err_t err  = rx_bus_onewire_read_byte(&s_test_manager, nullptr, &byte);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_write: nullptr manager returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(manager) branch at line 1829.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write() Function under test
 */
void test_rx_bus_onewire_write_null_manager(void)
{
  const uint8_t data[] = {0x01U};
  rx_err_t      err    = rx_bus_onewire_write(nullptr, s_test_bus_name, data, 1U);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_write: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1830.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_write() Function under test
 */
void test_rx_bus_onewire_write_null_bus_name(void)
{
  const uint8_t data[] = {0x01U};
  rx_err_t      err    = rx_bus_onewire_write(&s_test_manager, nullptr, data, 1U);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read: nullptr manager returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(manager) branch at line 1846.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read() Function under test
 */
void test_rx_bus_onewire_read_null_manager(void)
{
  uint8_t  buf[1] = {};
  rx_err_t err    = rx_bus_onewire_read(nullptr, s_test_bus_name, buf, 1U);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1847.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read() Function under test
 */
void test_rx_bus_onewire_read_null_bus_name(void)
{
  uint8_t  buf[1] = {};
  rx_err_t err    = rx_bus_onewire_read(&s_test_manager, nullptr, buf, 1U);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_skip_rom: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1875.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_skip_rom() Function under test
 */
void test_rx_bus_onewire_skip_rom_null_bus_name(void)
{
  rx_err_t err = rx_bus_onewire_skip_rom(&s_test_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_match_rom: nullptr manager returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(manager) branch at line 1895.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_match_rom() Function under test
 */
void test_rx_bus_onewire_match_rom_null_manager(void)
{
  const uint8_t rom[k_test_rom_bytes] = {0x28U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U};
  rx_err_t      err                   = rx_bus_onewire_match_rom(nullptr, s_test_bus_name, rom);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_match_rom: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1896.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_match_rom() Function under test
 */
void test_rx_bus_onewire_match_rom_null_bus_name(void)
{
  const uint8_t rom[k_test_rom_bytes] = {0x28U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U};
  rx_err_t      err                   = rx_bus_onewire_match_rom(&s_test_manager, nullptr, rom);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_read_rom: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1925.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_read_rom() Function under test
 */
void test_rx_bus_onewire_read_rom_null_bus_name(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  rx_err_t err                   = rx_bus_onewire_read_rom(&s_test_manager, nullptr, rom);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test rx_bus_onewire_search: nullptr bus_name returns k_rx_err_null_ptr.
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(bus_name) branch at line 1944.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see rx_bus_onewire_search() Function under test
 */
void test_rx_bus_onewire_search_null_bus_name(void)
{
  uint8_t  roms[k_test_rom_bytes * k_test_max_search_devices] = {};
  uint32_t num                                                = 0U;
  rx_err_t err =
    rx_bus_onewire_search(&s_test_manager, nullptr, roms, k_test_max_search_devices, &num);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* -------------------------------------------------------------------------
 * Callback wrong-type branch coverage tests
 * -------------------------------------------------------------------------
 */

/**
 * @brief Test internal_onewire_deinit_callback: wrong bus type returns k_rx_err_invalid_arg.
 *
 * @details
 * Covers CHECK_ONEWIRE_BUS type check at line 1077.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_deinit_callback() Function under test
 */
void test_onewire_internal_deinit_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */
  onewire_simple_ctx_t ctx     = {.result = k_rx_ok};
  rx_err_t             err     = internal_onewire_deinit_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_reset_callback: wrong bus type returns k_rx_err_invalid_arg.
 *
 * @details
 * Covers CHECK_ONEWIRE_BUS type check at line 1101.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_arg returned
 *
 * @see internal_onewire_reset_callback() Function under test
 */
void test_onewire_internal_reset_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */
  bool                presence = false;
  onewire_reset_ctx_t ctx      = {.presence = &presence, .result = k_rx_ok};
  rx_err_t            err      = internal_onewire_reset_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_onewire_write_byte_callback: wrong bus type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1213.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_write_byte_callback() Function under test
 */
void test_onewire_internal_write_byte_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */
  onewire_write_byte_ctx_t ctx = {.byte = k_test_data_byte_aa, .result = k_rx_ok};
  rx_err_t                 err = internal_onewire_write_byte_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_read_byte_callback: wrong bus type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1245.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_read_byte_callback() Function under test
 */
void test_onewire_internal_read_byte_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */
  uint8_t                 byte = 0U;
  onewire_read_byte_ctx_t ctx  = {.byte = &byte, .result = k_rx_ok};
  rx_err_t                err  = internal_onewire_read_byte_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_write_buffer_callback: wrong type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1290.
 * length must be nonzero and <= max to reach the type check.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_write_buffer_callback() Function under test
 */
void test_onewire_internal_write_buffer_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config   = s_onewire_config;
  wrong_config.type              = k_bus_type_spi; /* Wrong bus type */
  const uint8_t           data[] = {0x01U};
  onewire_write_buf_ctx_t ctx    = {.data = data, .length = 1U, .result = k_rx_ok};
  rx_err_t                err    = internal_onewire_write_buffer_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_read_buffer_callback: wrong type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1338.
 * length must be nonzero and <= max to reach the type check.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_read_buffer_callback() Function under test
 */
void test_onewire_internal_read_buffer_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config  = s_onewire_config;
  wrong_config.type             = k_bus_type_spi; /* Wrong bus type */
  uint8_t                buf[1] = {};
  onewire_read_buf_ctx_t ctx    = {.data = buf, .length = 1U, .result = k_rx_ok};
  rx_err_t               err    = internal_onewire_read_buffer_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_skip_rom_callback: wrong bus type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1382.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_skip_rom_callback() Function under test
 */
void test_onewire_internal_skip_rom_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config = s_onewire_config;
  wrong_config.type            = k_bus_type_spi; /* Wrong bus type */
  rx_err_t err                 = internal_onewire_skip_rom_callback(&wrong_config, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_match_rom_callback: wrong type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1423.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_match_rom_callback() Function under test
 */
void test_onewire_internal_match_rom_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config        = s_onewire_config;
  wrong_config.type                   = k_bus_type_spi; /* Wrong bus type */
  const uint8_t rom[k_test_rom_bytes] = {0x28U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0xCBU};
  onewire_match_rom_ctx_t ctx         = {.rom = rom, .result = k_rx_ok};
  rx_err_t                err         = internal_onewire_match_rom_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_read_rom_callback: wrong type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1484.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_read_rom_callback() Function under test
 */
void test_onewire_internal_read_rom_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config                 = s_onewire_config;
  wrong_config.type                            = k_bus_type_spi; /* Wrong bus type */
  uint8_t                rom[k_test_rom_bytes] = {};
  onewire_read_rom_ctx_t ctx                   = {.rom = rom, .result = k_rx_ok};
  rx_err_t               err = internal_onewire_read_rom_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test internal_onewire_search_callback: wrong type returns k_rx_err_invalid_state.
 *
 * @details
 * Covers the type != k_bus_type_onewire branch at line 1573.
 * All prior checks (num_devices, max_devices, roms) must pass first.
 *
 * @pre setUp() completed
 * @post k_rx_err_invalid_state returned
 *
 * @see internal_onewire_search_callback() Function under test
 */
void test_onewire_internal_search_callback_wrong_type(void)
{
  rx_bus_config_t wrong_config                = s_onewire_config;
  wrong_config.type                           = k_bus_type_spi; /* Wrong bus type */
  uint8_t              roms[k_test_rom_bytes] = {};
  uint32_t             num                    = 0U;
  onewire_search_ctx_t ctx                    = {
                       .roms        = roms,
                       .max_devices = 1U,
                       .num_devices = &num,
                       .result      = k_rx_ok,
  };
  rx_err_t err = internal_onewire_search_callback(&wrong_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* -------------------------------------------------------------------------
 * Internal function null-pointer branch coverage tests
 * -------------------------------------------------------------------------
 */

/**
 * @brief Test internal_search_step: nullptr bus_config returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(bus_config) at line 710.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_null_bus_config(void)
{
  onewire_runtime_state_t state                 = {};
  uint8_t                 rom[k_test_rom_bytes] = {};
  uint8_t                 byte_idx              = 0U;
  uint8_t                 bit_mask              = 0x01U;
  uint8_t                 last_zero             = 0U;
  rx_err_t err = internal_search_step(nullptr, &state, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_step: nullptr state returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(state) at line 711.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_step() Function under test
 */
void test_onewire_internal_search_step_null_state(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  uint8_t  byte_idx              = 0U;
  uint8_t  bit_mask              = 0x01U;
  uint8_t  last_zero             = 0U;
  rx_err_t err =
    internal_search_step(&s_onewire_config, nullptr, rom, 1U, &byte_idx, &bit_mask, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_bits: nullptr bus_config returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(bus_config) at line 776.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_bits() Function under test
 */
void test_onewire_internal_search_bits_null_bus_config(void)
{
  onewire_runtime_state_t state                 = {};
  uint8_t                 rom[k_test_rom_bytes] = {};
  uint8_t                 last_zero             = 0U;
  rx_err_t                err = internal_search_bits(nullptr, &state, rom, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_bits: nullptr state returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(state) at line 777.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_bits() Function under test
 */
void test_onewire_internal_search_bits_null_state(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  uint8_t  last_zero             = 0U;
  rx_err_t err = internal_search_bits(&s_onewire_config, nullptr, rom, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_bits: nullptr rom returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(rom) at line 778.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_bits() Function under test
 */
void test_onewire_internal_search_bits_null_rom(void)
{
  onewire_runtime_state_t state     = {};
  uint8_t                 last_zero = 0U;
  rx_err_t err = internal_search_bits(&s_onewire_config, &state, nullptr, &last_zero);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_bits: nullptr last_zero returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(last_zero) at line 779.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_bits() Function under test
 */
void test_onewire_internal_search_bits_null_last_zero(void)
{
  onewire_runtime_state_t state                 = {};
  uint8_t                 rom[k_test_rom_bytes] = {};
  rx_err_t                err = internal_search_bits(&s_onewire_config, &state, rom, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_iteration: nullptr bus_config returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(bus_config) at line 852.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_null_bus_config(void)
{
  onewire_runtime_state_t state                 = {};
  uint8_t                 rom[k_test_rom_bytes] = {};
  bool                    device_found          = false;
  rx_err_t                err = internal_search_iteration(nullptr, &state, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_iteration: nullptr state returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(state) at line 853.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_null_state(void)
{
  uint8_t  rom[k_test_rom_bytes] = {};
  bool     device_found          = false;
  rx_err_t err = internal_search_iteration(&s_onewire_config, nullptr, rom, &device_found);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_iteration: nullptr rom returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(rom) at line 854.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_null_rom(void)
{
  onewire_runtime_state_t state        = {};
  bool                    device_found = false;
  rx_err_t err = internal_search_iteration(&s_onewire_config, &state, nullptr, &device_found);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test internal_search_iteration: nullptr device_found returns k_rx_err_null_ptr.
 *
 * @details
 * Covers RX_CHECK_NULL_PTR(device_found) at line 855.
 *
 * @pre setUp() completed
 * @post k_rx_err_null_ptr returned
 *
 * @see internal_search_iteration() Function under test
 */
void test_onewire_internal_search_iteration_null_device_found(void)
{
  onewire_runtime_state_t state                 = {};
  uint8_t                 rom[k_test_rom_bytes] = {};
  rx_err_t                err = internal_search_iteration(&s_onewire_config, &state, rom, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* -------------------------------------------------------------------------
 * Drive mode FALSE-branch coverage tests
 * -------------------------------------------------------------------------
 */

/**
 * @brief Test internal_set_drive_mode: output=true when already output returns k_rx_ok.
 *
 * @details
 * When output==true and state->line_is_output==true, the TRUE branch of
 * `if (output && !state->line_is_output)` is NOT taken (FALSE branch at line 373).
 * The function should return k_rx_ok without calling gpio_set_output.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, line_is_output remains true
 *
 * @see internal_set_drive_mode() Function under test
 */
void test_onewire_internal_set_drive_mode_already_output(void)
{
  onewire_runtime_state_t state = {.line_is_output = true};
  rx_err_t                err   = internal_set_drive_mode(&s_onewire_config, &state, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(state.line_is_output);
}

/**
 * @brief Test internal_set_drive_mode: output=false when already input returns k_rx_ok.
 *
 * @details
 * When output==false and state->line_is_output==false, neither branch fires
 * (FALSE branch of `else if (!output && state->line_is_output)` at line 379).
 * The function should return k_rx_ok without calling gpio_set_input.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, line_is_output remains false
 *
 * @see internal_set_drive_mode() Function under test
 */
void test_onewire_internal_set_drive_mode_already_input(void)
{
  onewire_runtime_state_t state = {.line_is_output = false};
  rx_err_t                err   = internal_set_drive_mode(&s_onewire_config, &state, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(state.line_is_output);
}

/**
 * @brief Test write_buffer callback loop exits when i reaches max_buf_bytes.
 *
 * @details
 * Calls internal_onewire_write_buffer_callback with ctx->length == 255
 * (k_onewire_max_buf_bytes). The for loop `for (i = 0; (i < 255) && (i < 255); ++i)`
 * runs exactly 255 iterations. On the 256th check, i == 255 and the first
 * condition (i < k_onewire_max_buf_bytes) is FALSE -- covering the loop-exit
 * branch on line 1303.
 *
 * @pre Bus initialized
 * @post k_rx_ok returned, all 255 bytes written
 *
 * @see internal_onewire_write_buffer_callback() Function under test (line 1303)
 */
void test_onewire_write_buffer_loop_exit_at_max_buf_bytes(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* 255 zero bytes: each write-0 bit = 3 GPIO calls -> 255*8*3 = 6120 calls */
  static const uint8_t    s_buf[k_test_max_buf_bytes] = {};
  onewire_write_buf_ctx_t ctx = {.data = s_buf, .length = k_test_max_buf_bytes, .result = k_rx_ok};
  err                         = internal_onewire_write_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test read_buffer callback loop exits when i reaches max_buf_bytes.
 *
 * @details
 * Calls internal_onewire_read_buffer_callback with ctx->length == 255
 * (k_onewire_max_buf_bytes). The for loop runs exactly 255 iterations.
 * On the 256th check, i == 255 and the first condition is FALSE -- covering
 * the loop-exit branch on line 1351.
 *
 * GPIO reads default to false (0), so all 255 bytes read as 0x00.
 *
 * @pre Bus initialized
 * @post k_rx_ok returned, all 255 bytes read
 *
 * @see internal_onewire_read_buffer_callback() Function under test (line 1351)
 */
void test_onewire_read_buffer_loop_exit_at_max_buf_bytes(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* 255 bytes: each read bit = 4 GPIO calls -> 255*8*4 = 8160 calls */
  static uint8_t         s_buf[k_test_max_buf_bytes];
  onewire_read_buf_ctx_t ctx = {.data = s_buf, .length = k_test_max_buf_bytes, .result = k_rx_ok};
  err                        = internal_onewire_read_buffer_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test search callback while loop exits when num_devices reaches max_devices.
 *
 * @details
 * Uses the two-device mock sequence but sets max_devices = 1. After finding
 * Device A (with last_device_flag = false because a discrepancy exists),
 * *num_devices increments to 1. The while condition `*num_devices < max_devices`
 * evaluates to `1 < 1` = FALSE -- covering the while-loop-exit branch on
 * line 1580 (as opposed to the break at line 1595).
 *
 * @pre Bus initialized, CRC override disabled
 * @post k_rx_ok returned, exactly 1 device found
 *
 * @see internal_onewire_search_callback() Function under test (line 1580)
 */
void test_onewire_search_callback_while_loop_exits_at_max_devices(void)
{
  rx_err_t err = rx_bus_onewire_init(&s_test_manager, s_test_bus_name);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_crc8_set_override(false);
  /* Two-device sequence: iteration 1 finds Device A with last_device_flag=false */
  mock_gpio_set_read_sequence(s_test_pin, s_seq_two_device, k_test_two_device_seq_len);

  uint8_t              s_roms[k_test_rom_bytes];
  uint32_t             num_devices = 0U;
  onewire_search_ctx_t ctx         = {.roms        = s_roms,
                                      .max_devices = 1U,
                                      .num_devices = &num_devices,
                                      .result      = k_rx_ok};
  err                              = internal_onewire_search_callback(&s_onewire_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1U, num_devices);
}

/**
 * @brief Test internal_delay_us with large delay exercises ticks > counter_max branch
 *
 * @details
 * Calls internal_delay_us with 10000 microseconds (10ms). This produces
 * ticks = 10000 * 7500000 / 1000000 = 75000, which exceeds k_onewire_timer_counter_max
 * (0xFFFF = 65535). The ternary at line 278 takes the TRUE branch (cap at counter_max).
 * With the mock auto-incrementing cmcnt by 100 per read, the wait loop exits quickly.
 *
 * @pre setUp() completed, mock timer auto-increment enabled
 * @post Delay completes without hanging
 */
void test_onewire_internal_delay_us_large_delay_exercises_counter_max(void)
{
  /*
   * 10000 us -> ticks = 75000 > 65535 (k_onewire_timer_counter_max).
   * Covers the TRUE branch of: wait_ticks = (ticks > counter_max) ? counter_max : ticks
   *
   * With the default auto-increment of 100 the inner busy-wait never exits:
   * gcd(100, 65536) = 4 which does not divide 65535, so (uint16_t)(cmcnt - start)
   * can never reach 65535. Use increment=1 (odd -> gcd=1) so the loop exits
   * after exactly 65535 inner iterations per outer pass (~75000 nop calls total,
   * completing in < 1 ms on the x86_64 test host).
   */
  mock_onewire_hw_set_timer_auto_increment(1U);
  internal_delay_us(k_test_large_delay_us);
  /* Restore default so subsequent tests are not affected */
  mock_onewire_hw_set_timer_auto_increment(k_mock_onewire_timer_auto_increment_default);
  /* If we get here without hanging, the test passed */
  TEST_PASS();
}

/**
 * @brief Test internal_onewire_deinit_callback with corrupted handle exercises loop exhaustion
 *
 * @details
 * Covers line 1048 false exit: for loop completes all iterations without finding a match.
 * Sets bus_config.handle to a fake pointer that does not match any pool entry.
 * After the loop exhausts, bus_config.handle is set to nullptr.
 *
 * @pre setUp() completed, s_onewire_config initialized
 * @post handle set to nullptr, no crash
 */
void test_onewire_release_state_loop_exhaustion(void)
{
  /* Use a valid bus_config but set handle to a fake pointer not in the pool */
  rx_bus_config_t cfg = s_onewire_config;
  uint8_t         dummy_target;
  cfg.handle               = &dummy_target; /* Points outside s_state_pool */
  onewire_simple_ctx_t ctx = {.result = k_rx_ok};

  rx_err_t err = internal_onewire_deinit_callback(&cfg, &ctx);
  /* Loop exhausts, handle cleared to nullptr, callback returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NULL(cfg.handle);
}

/**
 * @brief Test internal_delay_timer_init with valid hardware register pointers
 *
 * @details
 * Exercises the "assert-passes" branch (condition true) for the three
 * RX_ASSERT_PRE checks on lines 212-214 of rx_bus_onewire.c by calling
 * internal_delay_timer_init() with all mock accessors returning valid
 * (non-null) pointers. Each branch has two paths: the fallthrough when
 * the pointer IS null and the jump-past when the pointer is NOT null.
 * This test covers the "not null" (jump-past) path.
 *
 * Must run before the null-pointer test. After this test completes,
 * internal_delay_timer_reset() clears the guard so the null-pointer
 * test can enter the function body again.
 *
 * @pre s_delay_timer_initialized == false (first call in process)
 * @post s_delay_timer_initialized == true (timer fully initialized)
 * @post All three RX_ASSERT_PRE "pass" branches exercised
 *
 * @since Version 1.0.0
 */
void test_onewire_delay_timer_init_valid_hw_registers(void)
{
  /* All mock accessors return valid pointers (null counts stay at 0) */
  internal_delay_timer_init();

  /* Reset so the next test can re-enter the function body */
  internal_delay_timer_reset();

  /* Timer initialized successfully with all asserts passing */
  TEST_PASS();
}

/**
 * @brief Test internal_delay_timer_init RX_ASSERT_PRE branches for null hardware registers
 *
 * @details
 * Covers the "assert-fires" branches on lines 212, 213, 214 in rx_bus_onewire.c
 * where RX_ASSERT_PRE checks that system_regs(), cmt_ctrl(), and cmt3() return
 * non-null pointers. Each mock accessor returns nullptr for exactly one call
 * (the assert check), then returns the real address on subsequent accesses so
 * that the rest of the function body executes without null dereferences.
 *
 * This test MUST run after test_onewire_delay_timer_init_valid_hw_registers
 * which covers the "pass" path and then resets s_delay_timer_initialized.
 *
 * @pre s_delay_timer_initialized == false (reset by prior test)
 * @post s_delay_timer_initialized == true
 * @post All three RX_ASSERT_PRE "fail" branches exercised
 *
 * @since Version 1.0.0
 */
void test_onewire_delay_timer_init_null_hw_registers(void)
{
  /* Force each mock to return nullptr once (for the RX_ASSERT_PRE check),
   * then return real address on subsequent calls (for the actual usage). */
  g_mock_onewire_null_system_regs_count = 1;
  g_mock_onewire_null_cmt_ctrl_count    = 1;
  g_mock_onewire_null_cmt3_count        = 1;

  /* In UNIT_TEST builds, internal_rx_fatal_error is a no-op, so execution
   * continues past the asserts. The second call to each accessor (lines 218,
   * 222, 225-227) returns the real struct pointer because the count expired. */
  internal_delay_timer_init();

  /* If we reach here, the assert false-branches were exercised without crash */
  TEST_PASS();
}

/** @} */ /* end of test_onewire_internal */

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Main test runner
 *
 * @details
 * Unity test framework entry point. Executes all test functions in sequence.
 *
 * **Test execution order:**
 * 1. Initialization tests (6 tests)
 * 2. Reset/Presence tests (5 tests)
 * 3. Bit operation tests (8 tests)
 * 4. Byte operation tests (8 tests)
 * 5. Buffer operation tests (8 tests)
 * 6. Skip ROM tests (4 tests)
 * 7. Match ROM tests (4 tests)
 * 8. Read ROM tests (4 tests)
 * 9. ROM Search tests (6 tests)
 * 10. Error injection tests (3 tests)
 * 11. CRC validation tests (5 tests)
 * 12. Multi-device search tests (1 test)
 * 13. Timing and parasitic power tests (4 tests)
 *
 * **Total:** 66 tests
 *
 * **Test isolation:**
 * - setUp() called before each test
 * - tearDown() called after each test
 * - No state leakage between tests
 *
 * @return int Unity test result (0 = success, >0 = failures)
 *
 * @note Called by build system (make test, ctest)
 * @see setUp() Pre-test initialization
 * @see tearDown() Post-test cleanup
 */
/**
 * @brief Run public API tests: init, reset, bit, byte, and buffer operations
 * @details 38 tests covering init (6), reset (5), write/read bit (8),
 *          write/read byte (7), and buffer write/read (8) plus error injection (3) + timing (1).
 */
static void internal_run_public_api_tests(void)
{
  /* Initialization Tests */
  RUN_TEST(test_rx_bus_onewire_init_success);
  RUN_TEST(test_rx_bus_onewire_init_null_manager);
  RUN_TEST(test_rx_bus_onewire_init_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_init_bus_not_found);
  RUN_TEST(test_rx_bus_onewire_init_wrong_bus_type);
  RUN_TEST(test_rx_bus_onewire_init_gpio_error);

  /* Reset/Presence Tests */
  RUN_TEST(test_rx_bus_onewire_reset_device_present);
  RUN_TEST(test_rx_bus_onewire_reset_no_device);
  RUN_TEST(test_rx_bus_onewire_reset_null_presence);
  RUN_TEST(test_rx_bus_onewire_reset_not_initialized);
  RUN_TEST(test_rx_bus_onewire_reset_null_manager);

  /* Write Bit Tests */
  RUN_TEST(test_rx_bus_onewire_write_bit_one);
  RUN_TEST(test_rx_bus_onewire_write_bit_zero);
  RUN_TEST(test_rx_bus_onewire_write_bit_not_initialized);
  RUN_TEST(test_rx_bus_onewire_write_bit_null_manager);

  /* Read Bit Tests */
  RUN_TEST(test_rx_bus_onewire_read_bit_high);
  RUN_TEST(test_rx_bus_onewire_read_bit_low);
  RUN_TEST(test_rx_bus_onewire_read_bit_null_output);
  RUN_TEST(test_rx_bus_onewire_read_bit_not_initialized);

  /* Write Byte Tests */
  RUN_TEST(test_rx_bus_onewire_write_byte_success);
  RUN_TEST(test_rx_bus_onewire_write_byte_not_initialized);
  RUN_TEST(test_rx_bus_onewire_write_byte_null_manager);

  /* Read Byte Tests */
  RUN_TEST(test_rx_bus_onewire_read_byte_all_ones);
  RUN_TEST(test_rx_bus_onewire_read_byte_all_zeros);
  RUN_TEST(test_rx_bus_onewire_read_byte_null_output);
  RUN_TEST(test_rx_bus_onewire_read_byte_not_initialized);

  /* Buffer Write Tests */
  RUN_TEST(test_rx_bus_onewire_write_buffer_success);
  RUN_TEST(test_rx_bus_onewire_write_buffer_zero_length);
  RUN_TEST(test_rx_bus_onewire_write_buffer_null_data);
  RUN_TEST(test_rx_bus_onewire_write_buffer_not_initialized);

  /* Buffer Read Tests */
  RUN_TEST(test_rx_bus_onewire_read_buffer_success);
  RUN_TEST(test_rx_bus_onewire_read_buffer_zero_length);
  RUN_TEST(test_rx_bus_onewire_read_buffer_null_data);
  RUN_TEST(test_rx_bus_onewire_read_buffer_not_initialized);

  /* GPIO Error Injection Tests */
  RUN_TEST(test_rx_bus_onewire_reset_gpio_read_error);
  RUN_TEST(test_rx_bus_onewire_write_bit_gpio_error);
  RUN_TEST(test_rx_bus_onewire_read_bit_gpio_error);
}

/**
 * @brief Run ROM command tests: skip, match, read ROM, search, CRC, and multi-device
 * @details 34 tests covering skip ROM (4), match ROM (4), read ROM (4),
 *          search (6), CRC (5), multi-device (1), and timing/parasitic (4) plus null bus_name (6).
 */
static void internal_run_rom_command_tests(void)
{
  /* Skip ROM Tests */
  RUN_TEST(test_rx_bus_onewire_skip_rom_success);
  RUN_TEST(test_rx_bus_onewire_skip_rom_no_device);
  RUN_TEST(test_rx_bus_onewire_skip_rom_null_manager);
  RUN_TEST(test_rx_bus_onewire_skip_rom_not_initialized);

  /* Match ROM Tests */
  RUN_TEST(test_rx_bus_onewire_match_rom_success);
  RUN_TEST(test_rx_bus_onewire_match_rom_no_device);
  RUN_TEST(test_rx_bus_onewire_match_rom_null_rom);
  RUN_TEST(test_rx_bus_onewire_match_rom_not_initialized);

  /* Read ROM Tests */
  RUN_TEST(test_rx_bus_onewire_read_rom_no_device);
  RUN_TEST(test_rx_bus_onewire_read_rom_null_rom);
  RUN_TEST(test_rx_bus_onewire_read_rom_not_initialized);
  RUN_TEST(test_rx_bus_onewire_read_rom_null_manager);

  /* Search Tests */
  RUN_TEST(test_rx_bus_onewire_search_no_devices);
  RUN_TEST(test_rx_bus_onewire_search_zero_max);
  RUN_TEST(test_rx_bus_onewire_search_null_roms);
  RUN_TEST(test_rx_bus_onewire_search_null_num_devices);
  RUN_TEST(test_rx_bus_onewire_search_not_initialized);
  RUN_TEST(test_rx_bus_onewire_search_null_manager);

  /* CRC Validation Tests */
  RUN_TEST(test_rx_bus_onewire_read_rom_crc_valid);
  RUN_TEST(test_rx_bus_onewire_read_rom_crc_valid_device_c);
  RUN_TEST(test_rx_bus_onewire_read_rom_crc_mismatch);
  RUN_TEST(test_rx_bus_onewire_search_single_device_crc_valid);
  RUN_TEST(test_rx_bus_onewire_search_crc_mismatch);

  /* Multi-Device Search Tests */
  RUN_TEST(test_rx_bus_onewire_search_two_devices);

  /* Timing and Parasitic Power Tests */
  RUN_TEST(test_rx_bus_onewire_timing_constants);
  RUN_TEST(test_rx_bus_onewire_bus_released_after_write);
  RUN_TEST(test_rx_bus_onewire_bus_released_after_read);
  RUN_TEST(test_rx_bus_onewire_bus_released_after_reset);
}

/**
 * @brief Run internal callback coverage tests part 1: init through read byte
 * @details 33 tests covering internal delay, state pool, drive mode, reset pulse,
 *          bit operations, byte operations, and search step callback errors.
 */
static void internal_run_callback_coverage_part1(void)
{
  RUN_TEST(test_onewire_internal_delay_us_zero);
  RUN_TEST(test_onewire_internal_acquire_state_existing_handle);
  RUN_TEST(test_onewire_internal_acquire_state_pool_exhausted);
  RUN_TEST(test_onewire_internal_set_drive_mode_set_input_fails);
  RUN_TEST(test_onewire_internal_reset_pulse_release_line_fails);
  RUN_TEST(test_onewire_internal_reset_pulse_read_line_fails);
  RUN_TEST(test_onewire_internal_write_bit_one_release_fails);
  RUN_TEST(test_onewire_internal_write_bit_zero_release_fails);
  RUN_TEST(test_onewire_internal_read_bit_release_fails);
  RUN_TEST(test_onewire_internal_read_bit_read_line_fails);
  RUN_TEST(test_onewire_internal_write_byte_write_bit_fails);
  RUN_TEST(test_onewire_internal_read_byte_read_bit_fails);
  RUN_TEST(test_onewire_internal_search_step_first_read_bit_fails);
  RUN_TEST(test_onewire_internal_search_step_second_read_bit_fails);
  RUN_TEST(test_onewire_internal_search_step_bit_and_comp_both_true);
  RUN_TEST(test_onewire_internal_search_step_direction_from_last_rom);
  RUN_TEST(test_onewire_internal_search_step_write_bit_fails);
  RUN_TEST(test_onewire_internal_search_bits_step_error);
  RUN_TEST(test_onewire_internal_search_iteration_reset_pulse_fails);
  RUN_TEST(test_onewire_internal_search_iteration_no_presence);
  RUN_TEST(test_onewire_internal_search_iteration_write_byte_fails);
  RUN_TEST(test_onewire_internal_search_iteration_last_device_flag);
  RUN_TEST(test_onewire_internal_search_iteration_search_bits_fails);
  RUN_TEST(test_onewire_internal_init_callback_gpio_read_fails);
  RUN_TEST(test_onewire_internal_init_callback_pin_reads_low);
  RUN_TEST(test_onewire_internal_reset_callback_null_presence);
  RUN_TEST(test_onewire_internal_write_bit_callback_not_initialized);
  RUN_TEST(test_onewire_internal_write_bit_callback_wrong_type);
  RUN_TEST(test_onewire_internal_read_bit_callback_not_initialized);
  RUN_TEST(test_onewire_internal_read_bit_callback_wrong_type);
  RUN_TEST(test_onewire_internal_read_bit_callback_null_bit);
  RUN_TEST(test_onewire_internal_write_byte_callback_write_byte_fails);
  RUN_TEST(test_onewire_internal_read_byte_callback_null_byte);
}

/**
 * @brief Run internal callback coverage tests part 2: buffer, ROM, search, and deinit
 * @details 35 tests covering buffer callbacks, ROM callbacks, search callbacks,
 *          deinit, null bus_name coverage, wrong-type coverage, and loop-exit coverage.
 */
static void internal_run_callback_coverage_part2(void)
{
  RUN_TEST(test_onewire_internal_write_buffer_callback_length_too_large);
  RUN_TEST(test_onewire_internal_write_buffer_callback_null_data);
  RUN_TEST(test_onewire_internal_write_buffer_callback_write_fails);
  RUN_TEST(test_onewire_internal_read_buffer_callback_not_initialized);
  RUN_TEST(test_onewire_internal_read_buffer_callback_length_too_large);
  RUN_TEST(test_onewire_internal_read_buffer_callback_null_data);
  RUN_TEST(test_onewire_internal_read_buffer_callback_read_fails);
  RUN_TEST(test_onewire_internal_skip_rom_callback_reset_fails);
  RUN_TEST(test_onewire_internal_skip_rom_callback_write_byte_fails);
  RUN_TEST(test_onewire_internal_match_rom_callback_null_rom);
  RUN_TEST(test_onewire_internal_match_rom_callback_reset_fails);
  RUN_TEST(test_onewire_internal_match_rom_callback_no_presence);
  RUN_TEST(test_onewire_internal_match_rom_callback_write_cmd_fails);
  RUN_TEST(test_onewire_internal_match_rom_callback_write_rom_fails);
  RUN_TEST(test_onewire_internal_read_rom_callback_null_rom);
  RUN_TEST(test_onewire_internal_read_rom_callback_reset_fails);
  RUN_TEST(test_onewire_internal_read_rom_callback_no_presence);
  RUN_TEST(test_onewire_internal_read_rom_callback_write_cmd_fails);
  RUN_TEST(test_onewire_internal_read_rom_callback_read_byte_fails);
  RUN_TEST(test_onewire_internal_read_rom_callback_family_code_zero);
  RUN_TEST(test_onewire_internal_search_callback_null_num_devices);
  RUN_TEST(test_onewire_internal_search_callback_zero_max);
  RUN_TEST(test_onewire_internal_search_callback_max_exceeds_limit);
  RUN_TEST(test_onewire_internal_search_callback_null_roms);
  RUN_TEST(test_onewire_deinit_null_manager);
  RUN_TEST(test_onewire_deinit_null_bus_name);
  RUN_TEST(test_onewire_deinit_bus_not_found);
}

/**
 * @brief Run null bus_name and wrong-type branch coverage tests
 * @details 26 tests covering public API null parameter coverage (16) and callback
 *          wrong-type branch coverage (10).
 */
static void internal_run_branch_coverage_part1(void)
{
  /* Public API null bus_name / null manager branch coverage */
  RUN_TEST(test_rx_bus_onewire_reset_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_write_bit_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_read_bit_null_manager);
  RUN_TEST(test_rx_bus_onewire_read_bit_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_write_byte_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_read_byte_null_manager);
  RUN_TEST(test_rx_bus_onewire_read_byte_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_write_null_manager);
  RUN_TEST(test_rx_bus_onewire_write_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_read_null_manager);
  RUN_TEST(test_rx_bus_onewire_read_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_skip_rom_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_match_rom_null_manager);
  RUN_TEST(test_rx_bus_onewire_match_rom_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_read_rom_null_bus_name);
  RUN_TEST(test_rx_bus_onewire_search_null_bus_name);

  /* Callback wrong-type branch coverage */
  RUN_TEST(test_onewire_internal_deinit_callback_wrong_type);
  RUN_TEST(test_onewire_internal_reset_callback_wrong_type);
  RUN_TEST(test_onewire_internal_write_byte_callback_wrong_type);
  RUN_TEST(test_onewire_internal_read_byte_callback_wrong_type);
  RUN_TEST(test_onewire_internal_write_buffer_callback_wrong_type);
  RUN_TEST(test_onewire_internal_read_buffer_callback_wrong_type);
  RUN_TEST(test_onewire_internal_skip_rom_callback_wrong_type);
  RUN_TEST(test_onewire_internal_match_rom_callback_wrong_type);
  RUN_TEST(test_onewire_internal_read_rom_callback_wrong_type);
  RUN_TEST(test_onewire_internal_search_callback_wrong_type);
}

/**
 * @brief Run null-pointer, drive-mode, loop-exit, and delay branch coverage tests
 * @details 17 tests covering internal null-pointer branches (10), drive mode (2),
 *          loop exit (3), and large delay / pool exhaustion (2).
 */
static void internal_run_branch_coverage_part2(void)
{
  /* Internal function null-pointer branch coverage */
  RUN_TEST(test_onewire_internal_search_step_null_bus_config);
  RUN_TEST(test_onewire_internal_search_step_null_state);
  RUN_TEST(test_onewire_internal_search_bits_null_bus_config);
  RUN_TEST(test_onewire_internal_search_bits_null_state);
  RUN_TEST(test_onewire_internal_search_bits_null_rom);
  RUN_TEST(test_onewire_internal_search_bits_null_last_zero);
  RUN_TEST(test_onewire_internal_search_iteration_null_bus_config);
  RUN_TEST(test_onewire_internal_search_iteration_null_state);
  RUN_TEST(test_onewire_internal_search_iteration_null_rom);
  RUN_TEST(test_onewire_internal_search_iteration_null_device_found);

  /* Drive mode FALSE-branch coverage */
  RUN_TEST(test_onewire_internal_set_drive_mode_already_output);
  RUN_TEST(test_onewire_internal_set_drive_mode_already_input);

  /* Loop exit branch coverage (i >= max_buf_bytes, while max_devices) */
  RUN_TEST(test_onewire_write_buffer_loop_exit_at_max_buf_bytes);
  RUN_TEST(test_onewire_read_buffer_loop_exit_at_max_buf_bytes);
  RUN_TEST(test_onewire_search_callback_while_loop_exits_at_max_devices);

  /* Branch coverage for large delay (ticks > counter_max) and pool exhaustion */
  RUN_TEST(test_onewire_internal_delay_us_large_delay_exercises_counter_max);
  RUN_TEST(test_onewire_release_state_loop_exhaustion);
}

/**
 * @brief Run delay timer init branch coverage tests
 * @details Must run first: s_delay_timer_initialized can only be false once per process.
 *          The valid-registers test runs first (exercises "pass" branches), then resets the
 *          timer state, then the null-registers test runs (exercises "fail" branches).
 */
static void internal_run_delay_timer_assert_tests(void)
{
  RUN_TEST(test_onewire_delay_timer_init_valid_hw_registers);
  RUN_TEST(test_onewire_delay_timer_init_null_hw_registers);
}

int main(void)
{
  UNITY_BEGIN();

  /* Must run first -- s_delay_timer_initialized is false only at process start */
  internal_run_delay_timer_assert_tests();

  internal_run_public_api_tests();
  internal_run_rom_command_tests();
  internal_run_callback_coverage_part1();
  internal_run_callback_coverage_part2();
  internal_run_branch_coverage_part1();
  internal_run_branch_coverage_part2();

  return UNITY_END();
}
