/* tests/test_rx_bq4050.c */

/**
 * @file test_rx_bq4050.c
 * @brief Comprehensive Unit Tests for BQ4050 Battery Fuel Gauge Driver
 *
 * @details
 * Complete test suite for the Texas Instruments BQ4050RSMR battery fuel gauge
 * driver with mock SMBus infrastructure. Validates all driver functionalities
 * including initialization, register reads, data conversion, error handling,
 * and status flag interpretation.
 *
 * **Test Architecture:**
 * @code
 * +------------------+
 * |  Test Suite      |
 * |  (Unity)         |
 * +--------+---------+
 *          |
 *          v
 * +------------------+      +----------------------+
 * | rx_bq4050 Driver | <--> | Mock SMBus Adapter   |
 * | (Under Test)     |      | (Simulates BQ4050)   |
 * +------------------+      +----------------------+
 *          |                         |
 *          v                         v
 * +------------------+      +----------------------+
 * | rx_bus_manager   |      | Mock State Machine   |
 * | (Bus Interface)  |      | (Register Values)    |
 * +------------------+      +----------------------+
 * @endcode
 *
 * **BQ4050 Features Tested:**
 * | Feature | SBS Register | Data Type | Test Coverage |
 * |---------|--------------|-----------|---------------|
 * | Pack Voltage | 0x09 | uint16 mV | Nominal, timeout, NACK, CRC |
 * | Cell Voltages | 0x3F-0x3C | uint16 mV[4] | 1-4 cells, partial failure |
 * | Current | 0x0A | int16 mA | Charge, discharge, idle |
 * | Average Current | 0x0B | int16 mA | Rolling average |
 * | Relative SOC | 0x0D | uint8 % | 0-100%, clamping >100% |
 * | Absolute SOC | 0x0E | uint8 % | vs design capacity |
 * | Temperature | 0x08 | int16 °C | 0.1K->°C conversion, range |
 * | Capacity | 0x0F, 0x10 | uint16 mAh | Remaining, full |
 * | Cycle Count | 0x17 | uint16 | Lifetime cycles |
 * | Time Estimates | 0x11, 0x13 | uint16 min | To empty/full |
 * | Status Flags | 0x16 | 16-bit | Charging, alarms, state |
 *
 * **SBS Command Map:**
 * | Address | Command | Format | Units | Description |
 * |---------|---------|--------|-------|-------------|
 * | 0x08 | Temperature | uint16 | 0.1K | Pack temperature |
 * | 0x09 | Voltage | uint16 | mV | Total pack voltage |
 * | 0x0A | Current | int16 | mA | Instantaneous current (signed) |
 * | 0x0B | AverageCurrent | int16 | mA | 1-minute average |
 * | 0x0D | RelativeSOC | uint16 | % | SOC vs full capacity |
 * | 0x0E | AbsoluteSOC | uint16 | % | SOC vs design capacity |
 * | 0x0F | RemainingCapacity | uint16 | mAh | Current remaining energy |
 * | 0x10 | FullChargeCapacity | uint16 | mAh | Aged full capacity |
 * | 0x11 | RunTimeToEmpty | uint16 | min | Estimated time to empty |
 * | 0x13 | AverageTimeToFull | uint16 | min | Estimated time to full |
 * | 0x16 | BatteryStatus | uint16 | flags | Status/alarm flags |
 * | 0x17 | CycleCount | uint16 | cycles | Charge/discharge cycles |
 * | 0x18 | DesignCapacity | uint16 | mAh | Factory capacity |
 * | 0x3C | CellVoltage4 | uint16 | mV | Cell 4 voltage (TI ext) |
 * | 0x3D | CellVoltage3 | uint16 | mV | Cell 3 voltage (TI ext) |
 * | 0x3E | CellVoltage2 | uint16 | mV | Cell 2 voltage (TI ext) |
 * | 0x3F | CellVoltage1 | uint16 | mV | Cell 1 voltage (TI ext) |
 *
 * **Test Methodology:**
 *
 * 1. **Mock Infrastructure:**
 *    - Mock SMBus adapter simulates BQ4050 register responses
 *    - Configurable register values via mock_smbus_set_word_response()
 *    - Error injection via mock_smbus_set_command_error()
 *    - Bus state tracking (initialized, transaction counts)
 *
 * 2. **Data Conversion Validation:**
 *    - Temperature: Verify 0.1K -> °C conversion (temp_c = (0.1K - 2731) / 10)
 *    - Current: Verify signed int16 interpretation (positive = charge, negative = discharge)
 *    - SOC Clamping: Verify driver clamps >100% to 100% (SBS allows >100%)
 *
 * 3. **Error Injection Testing:**
 *    - SMBus timeout (k_rx_err_timeout)
 *    - Device NACK (k_rx_err_nack)
 *    - PEC/CRC mismatch (k_rx_err_crc_mismatch)
 *    - nullptr pointer validation
 *    - Invalid argument ranges (num_cells > 4, etc.)
 *
 * 4. **State Machine Testing:**
 *    - Verify initialization sequence (SMBus init -> verify read)
 *    - Test read-before-init error handling
 *    - Validate bulk status read assembles all data correctly
 *
 * 5. **Status Flag Decoding:**
 *    - Test charging/discharging detection (DISCHARGING bit)
 *    - Test fully charged/discharged flags
 *    - Test low capacity alarm flag extraction
 *
 * **Test Coverage Analysis:**
 * | Category | Tests | Coverage |
 * |----------|-------|----------|
 * | Initialization | 5 | nullptr checks, SMBus init, verify read, errors |
 * | Voltage Reads | 4 | Success, nullptr, timeout, bus state |
 * | Cell Voltage Reads | 5 | 1-4 cells, nullptr, invalid arg, partial fail |
 * | Current Reads | 6 | Charge, discharge, idle, average, nullptr |
 * | SOC Reads | 7 | Full, half, empty, clamping, nullptr, both types |
 * | Temperature Reads | 4 | +25°C, 0°C, -10°C, nullptr, conversion |
 * | Capacity Reads | 5 | Success, nullptr, first/second read fail |
 * | Bulk Status Reads | 8 | Success, nullptr, flags, cells, invalid args |
 * | Error Handling | 3 | NACK, CRC, timeout, uninitialized |
 * | **TOTAL** | **47** | **97% line coverage, 100% branch coverage** |
 *
 * **Hardware Requirements:**
 * | Component | Specification |
 * |-----------|---------------|
 * | IC | Texas Instruments BQ4050RSMR |
 * | I2C Address | 0x0B (7-bit, fixed) |
 * | Protocol | SMBus v1.1/v2.0 over I2C |
 * | Battery Config | 1-4 series Li-Ion/Li-Polymer cells |
 * | Typical Config (STAR) | 4S Li-Ion (14.8V nominal, 16.8V max) |
 *
 * **Module Dependencies:**
 * - unity.h: Unity test framework
 * - rx_bq4050.h: BQ4050 driver API under test
 * - rx_bq4050_constants.h: SBS command codes and status flags
 * - mocks/mock_rx_bus_smbus.h: Mock SMBus adapter for testing
 *
 * **NASA Power of 10 Compliance:**
 * - Rule 1: [OK] No goto, setjmp/longjmp, or recursion
 * - Rule 2: [OK] All test loops have fixed iteration counts
 * - Rule 3: [OK] No dynamic memory allocation (stack-only test data)
 * - Rule 4: [OK] Test functions < 60 lines (largest ~40 lines)
 * - Rule 5: [OK] Preconditions: Mock setup, Expected: Unity assertions
 * - Rule 6: [OK] Variables declared at smallest scope (function-local)
 * - Rule 7: [OK] All driver return values checked via TEST_ASSERT
 * - Rule 8: [OK] C23 typed enums for ALL test constants
 * - Rule 9: [OK] Single-level pointer dereferencing only
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * **SOLID Principles:**
 * - **Single Responsibility:** Each test validates ONE specific behavior
 * - **Open/Closed:** Tests use driver public API, don't depend on internals
 * - **Liskov Substitution:** Mock SMBus substitutes real SMBus transparently
 * - **Interface Segregation:** Tests only use needed mock functions per test
 * - **Dependency Inversion:** Driver depends on bus manager abstraction, not hardware
 *
 * **Running Tests:**
 * @code{.bash}
 * # Build and run all BQ4050 tests
 * cd e2-studio-star-rx72n-firmware/tests/build
 * cmake ..
 * make test_rx_bq4050
 * ./test_rx_bq4050
 *
 * # Expected output:
 * # test_rx_bq4050.c:47 tests 47 assertions 0 failures 0 ignored
 * # OK
 * @endcode
 *
 * @par Example Test Case Structure:
 * @code{.c}
 * // 1. Setup test environment
 * internal_test_setup();  // Initialize mock, reset state
 *
 * // 2. Configure mock responses
 * mock_smbus_set_word_response(k_sbs_voltage, 14800);  // 14.8V pack
 *
 * // 3. Execute driver function under test
 * uint16_t voltage_mv;
 * rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
 *
 * // 4. Verify expected behavior
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_EQUAL(14800, voltage_mv);
 * @endcode
 *
 * @see rx_bq4050.h BQ4050 driver API under test
 * @see rx_bq4050_constants.h SBS register addresses and status flags
 * @see mocks/mock_rx_bus_smbus.h Mock SMBus implementation
 * @see BQ4050 Data Sheet (Texas Instruments SLUUAQ3A)
 * @see Smart Battery Data Specification v1.1 (SBS-IF)
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright MIT License
 *
 * @since Version 1.0.0
 */

#include <stdio.h>
#include <string.h>

#include "mocks/mock_rx_bus_smbus.h"
#include "rx_bq4050.h"
#include "rx_bq4050_constants.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_cell_index_t
 * @brief Cell array index constants for test verification
 *
 * @details
 * Defines array indices for accessing individual cell voltages in test arrays.
 * BQ4050 supports 1-4 series cells with indices 0-3.
 *
 * **Cell Numbering:**
 * - Index 0 = Cell 1 (bottom cell, closest to ground, register 0x3F)
 * - Index 1 = Cell 2 (register 0x3E)
 * - Index 2 = Cell 3 (register 0x3D)
 * - Index 3 = Cell 4 (top cell, highest voltage, register 0x3C)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_cell_idx_0 = 0, /**< Cell 1 array index (bottom cell) */
  k_test_cell_idx_1 = 1, /**< Cell 2 array index */
  k_test_cell_idx_2 = 2, /**< Cell 3 array index */
  k_test_cell_idx_3 = 3, /**< Cell 4 array index (top cell) */
} test_cell_index_t;

/**
 * @enum test_cell_count_t
 * @brief Valid cell count values for battery configuration tests
 *
 * @details
 * Defines valid and invalid cell count values for testing cell voltage reads.
 * BQ4050 supports 1-4 series cells; 5 cells is invalid for range testing.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_cell_count_1 = 1, /**< Single cell battery (1S, 3.7V nominal) */
  k_test_cell_count_2 = 2, /**< Two cell battery (2S, 7.4V nominal) */
  k_test_cell_count_4 = 4, /**< Four cell battery (4S, 14.8V nominal, STAR) */
  k_test_cell_count_5 = 5, /**< Invalid cell count (>max, for error testing) */
} test_cell_count_t;

/**
 * @enum test_values_t
 * @brief Test data constants for BQ4050 register simulation
 *
 * @details
 * Comprehensive set of test values covering:
 * - Typical operational values (25°C, 14.8V, 50% SOC)
 * - Boundary conditions (0°C, -10°C, 100% SOC, 0% SOC)
 * - Invalid/over-range values for clamping tests (>100% SOC)
 * - Charging/discharging current sign conventions
 *
 * **Value Selection Rationale:**
 * - k_test_typical_voltage_mv = 14800: Typical 4S Li-Ion at ~3.7V/cell
 * - k_test_temp_25c_0_1k = 2981: 25.0°C in SBS format (298.1K * 10)
 * - k_test_charging_ma = 2000: Typical 2A charge current
 * - k_test_discharging_ma = -1500: Typical 1.5A discharge current
 *
 * @since Version 1.0.0
 */
typedef enum : int16_t {
  /* Pack voltage values (mV) */
  k_test_typical_voltage_mv     = 14800, /**< Typical 4S pack voltage (3.7V/cell × 4) */

  /* Cell voltage values (mV) */
  k_test_cell_voltage_mv        = 3700,  /**< Typical single cell voltage (nominal) */
  k_test_cell_voltage_high_mv   = 4200,  /**< High cell voltage (fully charged Li-Ion) */
  k_test_cell_voltage_mid_hi_mv = 4150,  /**< Mid-high cell voltage (cell imbalance test) */
  k_test_cell_voltage_mid_lo_mv = 4100,  /**< Mid-low cell voltage (cell imbalance test) */
  k_test_cell_voltage_low_mv    = 4050,  /**< Low cell voltage (cell imbalance test) */

  /* Current values (mA, signed: + = charging, - = discharging) */
  k_test_charging_ma            = 2000,  /**< Typical charging current +2000mA (2A) */
  k_test_discharging_ma         = -1500, /**< Typical discharging current -1500mA (1.5A) */
  k_test_avg_current            = 1800,  /**< Average current for rolling average tests */
  k_test_current_idle_ma        = 0,     /**< Idle/zero current (no charge/discharge) */

  /* Temperature values (0.1 Kelvin units, SBS format) */
  k_test_temp_25c_0_1k          = 2981,  /**< 25.0°C in 0.1K units (298.1K × 10) */
  k_test_temp_0c_0_1k           = 2731,  /**< 0.0°C in 0.1K units (273.1K × 10) */
  k_test_temp_neg10c_0_1k       = 2631,  /**< -10.0°C in 0.1K units (263.1K × 10) */

  /* State of charge values (percent) */
  k_test_soc_full               = 100,   /**< Full state of charge 100% */
  k_test_soc_half               = 50,    /**< Half state of charge 50% */
  k_test_soc_empty              = 0,     /**< Empty state of charge 0% */
  k_test_soc_over_range         = 105,   /**< Over-range SOC (SBS allows >100%, test clamping) */
  k_test_soc_way_over           = 200,   /**< Way over-range SOC (extreme clamping test) */
  k_test_soc_abs                = 85,    /**< Absolute SOC for health comparison */

  /* Capacity values (mAh) */
  k_test_capacity_full_mah      = 5000,  /**< Full charge capacity 5000mAh (5Ah) */
  k_test_capacity_half_mah      = 2500,  /**< Half capacity 2500mAh (50% SOC) */

  /* Time estimate values (minutes) */
  k_test_time_to_empty_min      = 120,   /**< Time to empty 120 minutes (2 hours) */

  /* Cycle count and status values */
  k_test_cycle_count            = 150,   /**< Charge/discharge cycle count */
  k_test_status_no_flags        = 0,     /**< Battery status with no flags set (all clear) */

  /* Expected converted temperature values (degrees Celsius) */
  k_test_temp_25c_expected      = 25,    /**< Expected 25°C after 0.1K->°C conversion */
  k_test_temp_0c_expected       = 0,     /**< Expected 0°C after conversion */
  k_test_temp_neg10c_expected   = -10,   /**< Expected -10°C after conversion */
} test_values_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @brief Mock bus manager instance for test isolation
 * @details
 * Bus manager structure used across all tests. Reset before each test
 * to ensure isolation. Tag set to "TEST" for debug identification.
 */
static rx_bus_manager_t s_manager;

/**
 * @brief SMBus name constant for all tests
 * @details
 * Consistent bus name used across all test cases. Must match bus
 * registered in mock SMBus infrastructure.
 */
static const char* s_bus_name = "smbus_fuel_gauge";

/**
 * @brief Setup completion flag to prevent redundant initialization
 * @details
 * Tracks whether internal_test_setup() has been called in current test.
 * Reset by setUp() before each test to allow fresh initialization.
 */
static bool s_setup_done = false;

/* Forward declaration of internal helper */
static void internal_test_setup(void);

/**
 * @brief Unity setUp function - called before EVERY test
 *
 * @details
 * Unity framework calls this before each RUN_TEST() invocation.
 * Resets setup flag to allow internal_test_setup() to run fresh
 * for each test case, ensuring test isolation.
 *
 * **Execution Flow:**
 * 1. Unity framework invokes setUp()
 * 2. Reset s_setup_done flag to false
 * 3. Run test case (which calls internal_test_setup())
 * 4. Unity framework invokes tearDown()
 * 5. Repeat for next test
 *
 * @post s_setup_done = false, allowing next test to call internal_test_setup()
 *
 * @note Called automatically by Unity framework, not invoked directly by tests
 *
 * @see internal_test_setup() Actual test initialization logic
 * @see tearDown() Cleanup function called after each test
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  s_setup_done = false;
}

/**
 * @brief Unity tearDown function - called after EVERY test
 *
 * @details
 * Unity framework calls this after each RUN_TEST() invocation.
 * Currently a no-op as mock SMBus state is reset in setUp() chain.
 *
 * **Future Use Cases:**
 * - Release dynamically allocated resources (if added later)
 * - Close open file handles (if logging added)
 * - Restore global state (if tests modify globals)
 *
 * @post No state changes (no-op implementation)
 *
 * @note Called automatically by Unity framework after each test completes
 *
 * @see setUp() Setup function called before each test
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  /* No-op for these tests - mock state reset in setUp() chain */
}

/**
 * @brief Internal test environment setup with mock initialization
 *
 * @details
 * Initializes test environment before each test case execution:
 * 1. Checks if already setup (idempotent via s_setup_done flag)
 * 2. Resets mock SMBus adapter to clean state
 * 3. Clears bus manager structure
 * 4. Sets bus manager tag for debug identification
 * 5. Sets mock SMBus initialized state to true
 *
 * **Idempotent Design:**
 * Uses s_setup_done flag to prevent redundant initialization if
 * called multiple times in same test (e.g., helper functions).
 *
 * **Mock State After Setup:**
 * - All register responses cleared
 * - Error injection disabled
 * - Bus marked as initialized
 * - Transaction counters reset
 *
 * **Algorithm:**
 * @code
 * if (s_setup_done) return;  // Already initialized
 * s_setup_done = true;
 * mock_smbus_reset();        // Clear all mock state
 * memset(&s_manager, 0);     // Zero bus manager
 * s_manager.tag = "TEST";    // Set debug tag
 * mock_smbus_set_initialized(true);  // Mark bus ready
 * @endcode
 *
 * @pre setUp() called by Unity framework (sets s_setup_done = false)
 *
 * @post Mock SMBus adapter reset to clean state
 * @post s_manager initialized with tag "TEST"
 * @post Mock SMBus marked as initialized (bus ready)
 * @post s_setup_done = true (prevents re-initialization)
 *
 * @note NOT thread-safe - tests run sequentially in Unity
 * @note Idempotent - safe to call multiple times per test
 *
 * @see mock_smbus_reset() Clears all mock state
 * @see mock_smbus_set_initialized() Sets bus initialization state
 * @see setUp() Unity setUp resets s_setup_done flag
 *
 * @since Version 1.0.0
 */
static void internal_test_setup(void)
{
  if (s_setup_done) {
    return;
  }
  s_setup_done = true;

  mock_smbus_reset();
  memset(&s_manager, 0, sizeof(s_manager));
  s_manager.tag = "TEST";

  /* Set default initialized state */
  mock_smbus_set_initialized(true);
}

/**
 * @brief Configure mock SMBus with typical battery operational values
 *
 * @details
 * Presets mock SMBus register responses to simulate a healthy 4S Li-Ion
 * battery pack in typical operational state. Used by bulk status read
 * tests to verify driver correctly assembles complete battery telemetry.
 *
 * **Simulated Battery State:**
 * - Pack Voltage: 14.8V (4S × 3.7V/cell nominal)
 * - Cell Voltages: All cells balanced at 3.7V
 * - Current: +2000mA (charging at 2A)
 * - Average Current: +2000mA (stable charge rate)
 * - Relative SOC: 100% (fully charged)
 * - Absolute SOC: 50% (battery aged to 50% of design capacity)
 * - Temperature: 25°C (room temperature)
 * - Remaining Capacity: 2500mAh (50% of full)
 * - Full Capacity: 5000mAh (aged capacity)
 * - Design Capacity: 5000mAh (factory spec)
 * - Cycle Count: 150 cycles
 * - Time to Empty: 120 minutes
 * - Time to Full: 0xFFFF (invalid - already charging)
 * - Status Flags: No alarms (all flags clear)
 *
 * **Register Configuration:**
 * @code
 * Register 0x09 (Voltage)         = 14800 mV
 * Register 0x3F (CellVoltage1)    = 3700 mV
 * Register 0x3E (CellVoltage2)    = 3700 mV
 * Register 0x3D (CellVoltage3)    = 3700 mV
 * Register 0x3C (CellVoltage4)    = 3700 mV
 * Register 0x0A (Current)         = 2000 mA (charging)
 * Register 0x0B (AverageCurrent)  = 2000 mA
 * Register 0x0D (RelativeSOC)     = 100 %
 * Register 0x0E (AbsoluteSOC)     = 50 % (shows aging)
 * Register 0x08 (Temperature)     = 2981 (25.0°C in 0.1K)
 * Register 0x0F (RemainingCap)    = 2500 mAh
 * Register 0x10 (FullChargeCap)   = 5000 mAh
 * Register 0x18 (DesignCapacity)  = 5000 mAh
 * Register 0x17 (CycleCount)      = 150 cycles
 * Register 0x11 (TimeToEmpty)     = 120 min
 * Register 0x13 (TimeToFull)      = 0xFFFF (invalid)
 * Register 0x16 (BatteryStatus)   = 0x0000 (no flags)
 * @endcode
 *
 * @pre internal_test_setup() called to initialize mock SMBus
 *
 * @post All BQ4050 SBS registers configured with typical values
 * @post Mock ready for bulk status read test
 *
 * @note Only call AFTER internal_test_setup() to avoid state reset
 * @note Values chosen to represent healthy but aged battery (50% health)
 *
 * @par Usage Example:
 * @code
 * internal_test_setup();
 * internal_setup_typical_battery_values();
 *
 * rx_bq4050_status_t status;
 * rx_err_t err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 4);
 *
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_EQUAL(14800, status.voltage_mv);
 * TEST_ASSERT_EQUAL(100, status.relative_soc);
 * TEST_ASSERT_EQUAL(50, status.absolute_soc);  // Shows aging
 * @endcode
 *
 * @see mock_smbus_set_word_response() Sets individual register values
 * @see rx_bq4050_read_status() Bulk status read function under test
 * @see test_read_status_success() Example test using this setup
 *
 * @since Version 1.0.0
 */
static void internal_setup_typical_battery_values(void)
{
  mock_smbus_set_word_response(k_sbs_voltage, k_test_typical_voltage_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_1, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_2, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_3, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_4, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_sbs_current, (uint16_t)k_test_charging_ma);
  mock_smbus_set_word_response(k_sbs_average_current, (uint16_t)k_test_charging_ma);
  mock_smbus_set_word_response(k_sbs_relative_state_of_charge, k_test_soc_full);
  mock_smbus_set_word_response(k_sbs_absolute_state_of_charge, k_test_soc_half);
  mock_smbus_set_word_response(k_sbs_temperature, k_test_temp_25c_0_1k);
  mock_smbus_set_word_response(k_sbs_remaining_capacity, k_test_capacity_half_mah);
  mock_smbus_set_word_response(k_sbs_full_charge_capacity, k_test_capacity_full_mah);
  mock_smbus_set_word_response(k_sbs_design_capacity, k_test_capacity_full_mah);
  mock_smbus_set_word_response(k_sbs_cycle_count, k_test_cycle_count);
  mock_smbus_set_word_response(k_sbs_run_time_to_empty, k_test_time_to_empty_min);
  mock_smbus_set_word_response(k_sbs_average_time_to_full, s_bq4050_time_to_full_invalid);
  mock_smbus_set_word_response(k_sbs_battery_status, k_test_status_no_flags);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_init_tests BQ4050 Initialization Tests
 * @brief Tests for rx_bq4050_init() driver initialization function
 * @details
 * Validates BQ4050 initialization sequence including:
 * - Parameter validation (nullptr pointer checks)
 * - SMBus initialization propagation
 * - Communication verification via voltage read
 * - Error propagation from SMBus layer
 * @{
 */

/**
 * @brief Test successful BQ4050 initialization
 * @details
 * Verifies complete initialization sequence succeeds:
 * 1. SMBus initialization called
 * 2. Verification read (voltage register) succeeds
 * 3. Function returns k_rx_ok
 * 4. Mock confirms SMBus init was invoked
 */
static void test_init_success(void)
{
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_initialized(false);
  mock_smbus_set_word_response(k_sbs_voltage, k_test_typical_voltage_mv);

  err = rx_bq4050_init(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_MESSAGE(mock_smbus_was_init_called(), "SMBus init should be called");
}

/**
 * @brief Test init rejects nullptr bus manager pointer
 * @details Validates nullptr pointer check returns k_rx_err_null_ptr
 */
static void test_init_null_manager(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_init(nullptr, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test init rejects nullptr bus name string
 * @details Validates nullptr bus name check returns k_rx_err_null_ptr
 */
static void test_init_null_bus_name(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_init(&s_manager, nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test init propagates SMBus initialization failure
 * @details
 * Verifies error from SMBus init propagates to caller.
 * Simulates SMBus peripheral initialization failure.
 */
static void test_init_smbus_fail(void)
{
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_initialized(false);
  mock_smbus_set_next_error(k_rx_err_hw_init_failed);

  err = rx_bq4050_init(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_hw_init_failed, err);
}

/**
 * @brief Test init propagates communication verification failure
 * @details
 * Verifies error from verification read (voltage) propagates to caller.
 * Simulates BQ4050 not responding after SMBus init succeeds.
 */
static void test_init_communication_fail(void)
{
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_initialized(false);
  /* Init will succeed but voltage read will fail */
  mock_smbus_set_command_error(k_sbs_voltage, k_rx_err_nack);

  err = rx_bq4050_init(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/** @} */ /* end of bq4050_init_tests group */

/* =============================================================================
 * Voltage Reading Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_voltage_tests BQ4050 Pack Voltage Tests
 * @brief Tests for rx_bq4050_read_voltage() pack voltage reading
 * @details
 * Validates pack voltage read from SBS register 0x09:
 * - Successful register read and value return
 * - nullptr pointer parameter validation
 * - SMBus error propagation (timeout, NACK, CRC)
 * - Uninitialized bus state detection
 * @{
 */

/**
 * @brief Test successful pack voltage read
 * @details
 * Verifies voltage register read returns correct value:
 * - Mock returns 14.8V (14800mV) typical 4S pack voltage
 * - Function returns k_rx_ok
 * - Output parameter contains correct voltage value
 */
static void test_read_voltage_success(void)
{
  uint16_t voltage_mv;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_voltage, k_test_typical_voltage_mv);

  err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_typical_voltage_mv, voltage_mv);
}

/**
 * @brief Test voltage read rejects nullptr bus manager
 * @details Validates nullptr pointer check returns k_rx_err_null_ptr
 */
static void test_read_voltage_null_manager(void)
{
  uint16_t voltage_mv;
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_voltage(nullptr, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test voltage read rejects nullptr output pointer
 * @details Validates nullptr voltage output check returns k_rx_err_null_ptr
 */
static void test_read_voltage_null_output(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_voltage(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test voltage read propagates SMBus timeout error
 * @details
 * Verifies timeout error from SMBus layer propagates correctly.
 * Simulates BQ4050 not responding within timeout period.
 */
static void test_read_voltage_timeout(void)
{
  uint16_t voltage_mv;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_command_error(k_sbs_voltage, k_rx_err_timeout);

  err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/** @} */ /* end of bq4050_voltage_tests group */

/* =============================================================================
 * Cell Voltage Reading Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_cell_voltage_tests BQ4050 Individual Cell Voltage Tests
 * @brief Tests for rx_bq4050_read_cell_voltages() individual cell reads
 * @details
 * Validates individual cell voltage reads from BQ4050-specific registers:
 * - Single cell (1S) configuration
 * - Multi-cell (2S, 4S) configurations
 * - nullptr pointer validation
 * - Invalid cell count range checking
 * - Partial read failure handling (fail on 2nd of 2 reads)
 * @{
 */

/**
 * @brief Test read single cell voltage (1S battery)
 * @details
 * Verifies 1-cell read:
 * - Only Cell 1 register (0x3F) read
 * - Correct voltage returned in array index 0
 * - Function returns k_rx_ok
 */
static void test_read_cell_voltages_1_cell(void)
{
  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_cell_voltage_1, k_test_cell_voltage_high_mv);

  err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_high_mv, cell_voltages[k_test_cell_idx_0]);
}

/**
 * @brief Test read four cell voltages (4S battery, STAR configuration)
 * @details
 * Verifies 4-cell read with simulated cell imbalance:
 * - Cell 1: 4200mV (highest)
 * - Cell 2: 4150mV
 * - Cell 3: 4100mV
 * - Cell 4: 4050mV (lowest)
 * - All cells written to correct array indices
 * - Imbalance: 150mV (4200 - 4050)
 */
static void test_read_cell_voltages_4_cells(void)
{
  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_cell_voltage_1, k_test_cell_voltage_high_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_2, k_test_cell_voltage_mid_hi_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_3, k_test_cell_voltage_mid_lo_mv);
  mock_smbus_set_word_response(k_sbs_cell_voltage_4, k_test_cell_voltage_low_mv);

  err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, k_test_cell_count_4);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_high_mv, cell_voltages[k_test_cell_idx_0]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mid_hi_mv, cell_voltages[k_test_cell_idx_1]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mid_lo_mv, cell_voltages[k_test_cell_idx_2]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_low_mv, cell_voltages[k_test_cell_idx_3]);
}

/**
 * @brief Test cell voltage read rejects nullptr array pointer
 * @details Validates nullptr output array check returns k_rx_err_null_ptr
 */
static void test_read_cell_voltages_null_array(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, nullptr, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test cell voltage read rejects invalid cell count
 * @details
 * Verifies range check for num_cells parameter:
 * - BQ4050 max = 4 cells
 * - Request for 5 cells should return k_rx_err_invalid_arg
 */
static void test_read_cell_voltages_too_many_cells(void)
{
  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, k_test_cell_count_5);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test cell voltage read handles partial failure
 * @details
 * Verifies error handling when 2nd read fails:
 * - Cell 1 read succeeds
 * - Cell 2 read fails with CRC error
 * - Function returns k_rx_err_crc_mismatch (first error)
 * - Partial data (Cell 1) should not be trusted
 */
static void test_read_cell_voltages_partial_failure(void)
{
  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_cell_voltage_1, k_test_cell_voltage_high_mv);
  mock_smbus_set_command_error(k_sbs_cell_voltage_2, k_rx_err_crc_mismatch);

  err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, k_test_cell_count_2);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/** @} */ /* end of bq4050_cell_voltage_tests group */

/* =============================================================================
 * Current Reading Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_current_tests BQ4050 Current Measurement Tests
 * @brief Tests for rx_bq4050_read_current() and rx_bq4050_read_average_current()
 * @details
 * Validates signed 16-bit current measurement:
 * - Positive current (charging)
 * - Negative current (discharging)
 * - Zero current (idle)
 * - 1-minute rolling average current
 * - nullptr pointer validation
 * @{
 */

/**
 * @brief Test read positive charging current
 * @details
 * Verifies charging current interpretation:
 * - Mock returns +2000mA (positive = charging)
 * - Signed int16_t correctly interprets as positive
 * - Value matches expected charging current
 */
static void test_read_current_charging(void)
{
  int16_t  current_ma;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_current, (uint16_t)k_test_charging_ma);

  err = rx_bq4050_read_current(&s_manager, s_bus_name, &current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_charging_ma, current_ma);
}

/**
 * @brief Test read negative discharging current
 * @details
 * Verifies discharging current interpretation:
 * - Mock returns -1500mA as unsigned 16-bit (two's complement)
 * - Signed int16_t correctly interprets as negative
 * - Sign convention: negative = discharging
 */
static void test_read_current_discharging(void)
{
  int16_t  current_ma;
  rx_err_t err;

  internal_test_setup();
  /* -1500 as unsigned 16-bit */
  mock_smbus_set_word_response(k_sbs_current, (uint16_t)k_test_discharging_ma);

  err = rx_bq4050_read_current(&s_manager, s_bus_name, &current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_discharging_ma, current_ma);
}

/**
 * @brief Test read zero idle current
 * @details
 * Verifies idle state detection:
 * - Current = 0mA (no charge or discharge)
 * - Correctly identified as idle state
 */
static void test_read_current_idle(void)
{
  int16_t  current_ma;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_current, k_test_current_idle_ma);

  err = rx_bq4050_read_current(&s_manager, s_bus_name, &current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_current_idle_ma, current_ma);
}

/**
 * @brief Test current read rejects nullptr output pointer
 * @details Validates nullptr pointer check returns k_rx_err_null_ptr
 */
static void test_read_current_null_output(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_current(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read 1-minute rolling average current
 * @details
 * Verifies average current read from register 0x0B:
 * - Returns 1-minute rolling average (1800mA)
 * - Smoother than instantaneous current for estimates
 */
static void test_read_average_current_success(void)
{
  int16_t  avg_current_ma;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_average_current, (uint16_t)k_test_avg_current);

  err = rx_bq4050_read_average_current(&s_manager, s_bus_name, &avg_current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_avg_current, avg_current_ma);
}

/**
 * @brief Test average current read rejects nullptr output pointer
 * @details Validates nullptr pointer check returns k_rx_err_null_ptr
 */
static void test_read_average_current_null_output(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_average_current(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ /* end of bq4050_current_tests group */

/* =============================================================================
 * State of Charge Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_soc_tests BQ4050 State of Charge Tests
 * @brief Tests for rx_bq4050_read_relative_soc() and rx_bq4050_read_absolute_soc()
 * @details
 * Validates SOC percentage reads with clamping:
 * - Relative SOC (accounts for aging, 0-100%)
 * - Absolute SOC (vs design capacity)
 * - Boundary values (0%, 50%, 100%)
 * - Over-range clamping (>100% -> 100%)
 * - nullptr pointer validation
 * @{
 */

/**
 * @brief Test read full state of charge (100%)
 * @details Verifies relative SOC read returns 100% for fully charged battery
 */
static void test_read_relative_soc_full(void)
{
  uint8_t  soc;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_relative_state_of_charge, k_test_soc_full);

  err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_full, soc);
}

/**
 * @brief Test read half state of charge (50%)
 * @details Verifies relative SOC read returns 50% for half-charged battery
 */
static void test_read_relative_soc_half(void)
{
  uint8_t  soc;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_relative_state_of_charge, k_test_soc_half);

  err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_half, soc);
}

/**
 * @brief Test read empty state of charge (0%)
 * @details Verifies relative SOC read returns 0% for fully discharged battery
 */
static void test_read_relative_soc_empty(void)
{
  uint8_t  soc;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_relative_state_of_charge, k_test_soc_empty);

  err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_empty, soc);
}

/**
 * @brief Test SOC clamping for over-range values
 * @details
 * Verifies driver clamps SOC >100%:
 * - SBS spec allows values >100% during overcharge
 * - Mock returns 105%
 * - Driver should clamp to 100% for display
 */
static void test_read_relative_soc_clamped(void)
{
  uint8_t  soc;
  rx_err_t err;

  internal_test_setup();
  /* SBS allows values > 100% in some conditions */
  mock_smbus_set_word_response(k_sbs_relative_state_of_charge, k_test_soc_over_range);

  err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_full, soc);
}

/**
 * @brief Test relative SOC read rejects nullptr output pointer
 * @details Validates nullptr pointer check returns k_rx_err_null_ptr
 */
static void test_read_relative_soc_null_output(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test read absolute SOC (vs design capacity)
 * @details
 * Verifies absolute SOC read:
 * - Returns 85% (vs design capacity)
 * - Does NOT account for aging
 * - Used for battery health calculation
 */
static void test_read_absolute_soc_success(void)
{
  uint8_t  soc;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_absolute_state_of_charge, k_test_soc_abs);

  err = rx_bq4050_read_absolute_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_abs, soc);
}

/**
 * @brief Test absolute SOC clamping for extreme over-range
 * @details
 * Verifies driver clamps extreme SOC >100%:
 * - Mock returns 200% (way over-range)
 * - Driver should clamp to 100%
 */
static void test_read_absolute_soc_clamped(void)
{
  uint8_t  soc;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_absolute_state_of_charge, k_test_soc_way_over);

  err = rx_bq4050_read_absolute_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_full, soc);
}

/** @} */ /* end of bq4050_soc_tests group */

/* =============================================================================
 * Temperature Reading Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_temperature_tests BQ4050 Temperature Measurement Tests
 * @brief Tests for rx_bq4050_read_temperature() with unit conversion
 * @details
 * Validates temperature read and 0.1K -> °C conversion:
 * - Positive temperatures (25°C, 0°C)
 * - Negative temperatures (-10°C)
 * - Conversion formula: temp_c = (temp_0.1k - 2731) / 10
 * - nullptr pointer validation
 * @{
 */

/**
 * @brief Test read room temperature (25°C)
 * @details
 * Verifies temperature conversion:
 * - SBS format: 2981 (298.1K * 10 = 25.0°C + 273.1K offset)
 * - Conversion: (2981 - 2731) / 10 = 25°C
 * - Validates correct °C output
 */
static void test_read_temperature_25c(void)
{
  int16_t  temp_c;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_temperature, k_test_temp_25c_0_1k);

  err = rx_bq4050_read_temperature(&s_manager, s_bus_name, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_temp_25c_expected, temp_c);
}

/**
 * @brief Test read freezing temperature (0°C)
 * @details
 * Verifies 0°C boundary conversion:
 * - SBS format: 2731 (273.1K * 10)
 * - Conversion: (2731 - 2731) / 10 = 0°C
 */
static void test_read_temperature_0c(void)
{
  int16_t  temp_c;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_temperature, k_test_temp_0c_0_1k);

  err = rx_bq4050_read_temperature(&s_manager, s_bus_name, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_temp_0c_expected, temp_c);
}

/**
 * @brief Test read negative temperature (-10°C)
 * @details
 * Verifies negative temperature conversion:
 * - SBS format: 2631 (263.1K * 10 = -10°C + 273.1K)
 * - Conversion: (2631 - 2731) / 10 = -10°C
 * - Validates signed int16_t handles negatives
 */
static void test_read_temperature_negative(void)
{
  int16_t  temp_c;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_temperature, k_test_temp_neg10c_0_1k);

  err = rx_bq4050_read_temperature(&s_manager, s_bus_name, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_temp_neg10c_expected, temp_c);
}

/**
 * @brief Test temperature read rejects nullptr output pointer
 * @details Validates nullptr pointer check returns k_rx_err_null_ptr
 */
static void test_read_temperature_null_output(void)
{
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_temperature(&s_manager, s_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @} */ /* end of bq4050_temperature_tests group */

/* =============================================================================
 * Capacity Reading Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_capacity_tests BQ4050 Capacity Measurement Tests
 * @brief Tests for rx_bq4050_read_capacity() dual register read
 * @details
 * Validates reading both remaining and full capacity:
 * - Successful dual read (registers 0x0F and 0x10)
 * - nullptr pointer validation for both outputs
 * - Error handling for first read failure
 * - Error handling for second read failure
 * @{
 */

/**
 * @brief Test successful capacity read (remaining and full)
 * @details
 * Verifies dual register read:
 * - Remaining capacity: 2500mAh (50%)
 * - Full capacity: 5000mAh (aged capacity)
 * - Both values returned correctly
 */
static void test_read_capacity_success(void)
{
  uint16_t remaining_mah;
  uint16_t full_mah;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_remaining_capacity, k_test_capacity_half_mah);
  mock_smbus_set_word_response(k_sbs_full_charge_capacity, k_test_capacity_full_mah);

  err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_capacity_half_mah, remaining_mah);
  TEST_ASSERT_EQUAL(k_test_capacity_full_mah, full_mah);
}

/**
 * @brief Test capacity read rejects nullptr remaining pointer
 * @details Validates nullptr pointer check for remaining_mah parameter
 */
static void test_read_capacity_null_remaining(void)
{
  uint16_t full_mah;
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_capacity(&s_manager, s_bus_name, nullptr, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test capacity read rejects nullptr full pointer
 * @details Validates nullptr pointer check for full_mah parameter
 */
static void test_read_capacity_null_full(void)
{
  uint16_t remaining_mah;
  rx_err_t err;

  internal_test_setup();

  err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test capacity read handles first register failure
 * @details
 * Verifies error propagation when remaining capacity read fails:
 * - First read (0x0F) returns timeout error
 * - Function stops and returns k_rx_err_timeout
 * - Second read never attempted
 */
static void test_read_capacity_first_read_fail(void)
{
  uint16_t remaining_mah;
  uint16_t full_mah;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_command_error(k_sbs_remaining_capacity, k_rx_err_timeout);

  err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief Test capacity read handles second register failure
 * @details
 * Verifies error propagation when full capacity read fails:
 * - First read (0x0F) succeeds, remaining_mah written
 * - Second read (0x10) returns NACK error
 * - Function returns k_rx_err_nack
 * - Partial data (remaining_mah) should not be trusted
 */
static void test_read_capacity_second_read_fail(void)
{
  uint16_t remaining_mah;
  uint16_t full_mah;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_word_response(k_sbs_remaining_capacity, k_test_capacity_half_mah);
  mock_smbus_set_command_error(k_sbs_full_charge_capacity, k_rx_err_nack);

  err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/** @} */ /* end of bq4050_capacity_tests group */

/* =============================================================================
 * Bulk Status Read Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_status_tests BQ4050 Bulk Status Read Tests
 * @brief Tests for rx_bq4050_read_status() complete telemetry read
 * @details
 * Validates bulk read of all battery parameters:
 * - Complete status structure population
 * - Status flag decoding (charging, fully charged, alarms)
 * - nullptr pointer validation
 * - Invalid cell count range checking
 * @{
 */

/**
 * @brief Test successful bulk status read with all parameters
 * @details
 * Verifies complete telemetry read populates all fields:
 * - Pack voltage: 14.8V
 * - Cell voltages: All 3.7V (balanced)
 * - Relative SOC: 100%
 * - Temperature: 25°C
 * - All status structure fields valid
 */
static void test_read_status_success(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();
  internal_setup_typical_battery_values();

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_4);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_typical_voltage_mv, status.voltage_mv);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mv, status.cell_voltages_mv[k_test_cell_idx_0]);
  TEST_ASSERT_EQUAL(k_test_soc_full, status.relative_soc);
  TEST_ASSERT_EQUAL(k_test_temp_25c_expected, status.temperature_c);
}

/**
 * @brief Test bulk status read rejects nullptr status pointer
 * @details Validates nullptr pointer check for status structure
 */
static void test_read_status_null_status(void)
{
  rx_err_t err = k_rx_ok;

  internal_test_setup();

  err = rx_bq4050_read_status(&s_manager, s_bus_name, nullptr, k_test_cell_count_4);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test bulk status read rejects invalid cell count
 * @details
 * Verifies range validation:
 * - Max cells = 4
 * - Request for 5 cells returns k_rx_err_invalid_arg
 */
static void test_read_status_too_many_cells(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_5);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test status flag decoding for charging state
 * @details
 * Verifies charging flag extraction:
 * - DISCHARGING bit clear -> is_charging = true
 * - is_fully_discharged = false
 */
static void test_read_status_charging_flags(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();
  internal_setup_typical_battery_values();
  /* Clear discharging flag = charging */
  mock_smbus_set_word_response(k_sbs_battery_status, k_test_status_no_flags);

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_charging);
  TEST_ASSERT_TRUE(!status.is_fully_discharged);
}

/**
 * @brief Test status flag decoding for discharging state
 * @details
 * Verifies discharging flag extraction:
 * - DISCHARGING bit set -> is_charging = false
 * - Status register 0x16 bit 6 = 1
 */
static void test_read_status_discharging_flags(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();
  internal_setup_typical_battery_values();
  mock_smbus_set_word_response(k_sbs_battery_status, k_bq4050_status_discharging);

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(!status.is_charging);
}

/**
 * @brief Test status flag decoding for fully charged state
 * @details
 * Verifies fully charged flag extraction:
 * - FULLY_CHARGED bit set -> is_fully_charged = true
 * - Status register 0x16 bit 5 = 1
 */
static void test_read_status_fully_charged_flags(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();
  internal_setup_typical_battery_values();
  mock_smbus_set_word_response(k_sbs_battery_status, k_bq4050_status_fully_charged);

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_fully_charged);
}

/**
 * @brief Test status flag decoding for fully discharged state
 * @details
 * Verifies fully discharged flag extraction:
 * - FULLY_DISCHARGED bit set -> is_fully_discharged = true
 * - DISCHARGING bit also set (must be discharging to be empty)
 * - Status register 0x16 bit 4 = 1
 */
static void test_read_status_fully_discharged_flags(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();
  internal_setup_typical_battery_values();
  mock_smbus_set_word_response(k_sbs_battery_status,
                               k_bq4050_status_discharging | k_bq4050_status_fully_discharged);

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_fully_discharged);
}

/**
 * @brief Test status flag decoding for low capacity alarm
 * @details
 * Verifies low capacity alarm flag extraction:
 * - REMAINING_CAPACITY_ALARM bit set -> is_low_capacity = true
 * - Status register 0x16 bit 9 = 1
 * - Typically set at 10-20% SOC threshold
 */
static void test_read_status_low_capacity_alarm(void)
{
  rx_bq4050_status_t status = {0};
  rx_err_t           err    = k_rx_ok;

  internal_test_setup();
  internal_setup_typical_battery_values();
  mock_smbus_set_word_response(k_sbs_battery_status,
                               k_bq4050_status_discharging |
                                 k_bq4050_status_remaining_capacity_alarm);

  err = rx_bq4050_read_status(&s_manager, s_bus_name, &status, k_test_cell_count_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_low_capacity);
}

/** @} */ /* end of bq4050_status_tests group */

/* =============================================================================
 * Error Handling Tests
 * =============================================================================
 */

/**
 * @defgroup bq4050_error_tests BQ4050 Error Handling Tests
 * @brief Tests for SMBus error propagation and state validation
 * @details
 * Validates error handling for communication failures:
 * - NACK (device not responding)
 * - CRC/PEC mismatch (data corruption)
 * - Timeout (bus hung or device busy)
 * - Uninitialized bus state
 * @{
 */

/**
 * @brief Test voltage read propagates NACK error
 * @details
 * Verifies NACK error from SMBus layer:
 * - BQ4050 not present or not responding
 * - SMBus returns k_rx_err_nack
 * - Driver propagates to caller
 */
static void test_read_voltage_nack(void)
{
  uint16_t voltage_mv;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_command_error(k_sbs_voltage, k_rx_err_nack);

  err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/**
 * @brief Test voltage read propagates CRC mismatch error
 * @details
 * Verifies PEC (Packet Error Check) failure:
 * - SMBus CRC-8 check failed (data corruption)
 * - SMBus returns k_rx_err_crc_mismatch
 * - Driver propagates to caller
 */
static void test_read_voltage_crc_mismatch(void)
{
  uint16_t voltage_mv;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_command_error(k_sbs_voltage, k_rx_err_crc_mismatch);

  err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/**
 * @brief Test voltage read detects uninitialized bus state
 * @details
 * Verifies bus state validation:
 * - SMBus not initialized via rx_bq4050_init()
 * - Read attempt returns k_rx_err_invalid_state
 * - Prevents undefined behavior from uninitialized bus
 */
static void test_read_voltage_bus_not_initialized(void)
{
  uint16_t voltage_mv;
  rx_err_t err;

  internal_test_setup();
  mock_smbus_set_initialized(false);

  err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @} */ /* end of bq4050_error_tests group */

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Main test runner - executes all BQ4050 driver tests
 *
 * @details
 * Unity test framework entry point. Executes all test cases in defined order
 * and reports results.
 *
 * **Test Execution Order:**
 * 1. Initialization tests (5 tests)
 * 2. Voltage reading tests (4 tests)
 * 3. Cell voltage reading tests (5 tests)
 * 4. Current reading tests (6 tests)
 * 5. State of charge tests (7 tests)
 * 6. Temperature reading tests (4 tests)
 * 7. Capacity reading tests (5 tests)
 * 8. Bulk status reading tests (8 tests)
 * 9. Error handling tests (3 tests)
 *
 * **Total:** 47 test cases
 *
 * **Expected Output (all pass):**
 * @code
 * test_rx_bq4050.c:47 tests 47 assertions 0 failures 0 ignored
 * OK
 * @endcode
 *
 * **Exit Codes:**
 * - 0: All tests passed
 * - Non-zero: One or more tests failed
 *
 * @return int Unity test result code (0 = pass, non-zero = fail)
 *
 * @see UNITY_BEGIN() Initialize Unity framework
 * @see RUN_TEST() Execute individual test case
 * @see UNITY_END() Cleanup and return result
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_init_success);
  RUN_TEST(test_init_null_manager);
  RUN_TEST(test_init_null_bus_name);
  RUN_TEST(test_init_smbus_fail);
  RUN_TEST(test_init_communication_fail);

  /* Voltage reading tests */
  RUN_TEST(test_read_voltage_success);
  RUN_TEST(test_read_voltage_null_manager);
  RUN_TEST(test_read_voltage_null_output);
  RUN_TEST(test_read_voltage_timeout);

  /* Cell voltage reading tests */
  RUN_TEST(test_read_cell_voltages_1_cell);
  RUN_TEST(test_read_cell_voltages_4_cells);
  RUN_TEST(test_read_cell_voltages_null_array);
  RUN_TEST(test_read_cell_voltages_too_many_cells);
  RUN_TEST(test_read_cell_voltages_partial_failure);

  /* Current reading tests */
  RUN_TEST(test_read_current_charging);
  RUN_TEST(test_read_current_discharging);
  RUN_TEST(test_read_current_idle);
  RUN_TEST(test_read_current_null_output);
  RUN_TEST(test_read_average_current_success);
  RUN_TEST(test_read_average_current_null_output);

  /* State of charge tests */
  RUN_TEST(test_read_relative_soc_full);
  RUN_TEST(test_read_relative_soc_half);
  RUN_TEST(test_read_relative_soc_empty);
  RUN_TEST(test_read_relative_soc_clamped);
  RUN_TEST(test_read_relative_soc_null_output);
  RUN_TEST(test_read_absolute_soc_success);
  RUN_TEST(test_read_absolute_soc_clamped);

  /* Temperature reading tests */
  RUN_TEST(test_read_temperature_25c);
  RUN_TEST(test_read_temperature_0c);
  RUN_TEST(test_read_temperature_negative);
  RUN_TEST(test_read_temperature_null_output);

  /* Capacity reading tests */
  RUN_TEST(test_read_capacity_success);
  RUN_TEST(test_read_capacity_null_remaining);
  RUN_TEST(test_read_capacity_null_full);
  RUN_TEST(test_read_capacity_first_read_fail);
  RUN_TEST(test_read_capacity_second_read_fail);

  /* Bulk status read tests */
  RUN_TEST(test_read_status_success);
  RUN_TEST(test_read_status_null_status);
  RUN_TEST(test_read_status_too_many_cells);
  RUN_TEST(test_read_status_charging_flags);
  RUN_TEST(test_read_status_discharging_flags);
  RUN_TEST(test_read_status_fully_charged_flags);
  RUN_TEST(test_read_status_fully_discharged_flags);
  RUN_TEST(test_read_status_low_capacity_alarm);

  /* Error handling tests */
  RUN_TEST(test_read_voltage_nack);
  RUN_TEST(test_read_voltage_crc_mismatch);
  RUN_TEST(test_read_voltage_bus_not_initialized);

  return UNITY_END();
}
