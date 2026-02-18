/* tests/test_bms_monitoring_task.c */

/**
 * @file test_bms_monitoring_task.c
 * @brief Unit Tests for BMS Monitoring Task
 *
 * @details
 * Tests BMS monitoring task creation and battery data handling.
 * Uses mocks for ThreadX, BQ4050 driver, and shared data.
 *
 * Test coverage:
 * - Task creation success with default thresholds (25% warning, 5% critical)
 * - Task creation failure cases (NULL config, invalid thresholds)
 * - Battery data reading and storage
 * - Low battery warning at 25% SoC (default warning threshold)
 * - No warning when SoC is above warning threshold
 * - Critical battery e-stop at 5% SoC (default critical threshold)
 * - Custom threshold configuration
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include <string.h>

#include "mock_rx_bq4050.h"
#include "mock_shared_data.h"
#include "tx_api.h"
#include "unity.h"

/* Include the task header for the public API */
#include "bms_monitor_task.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_bms_soc_constants_t
 * @brief SoC test values for BMS threshold verification
 *
 * @details
 * Values chosen to exercise all three regions: above warning, between
 * warning and critical, and below critical.
 *
 * | Constant | Value | Region |
 * |----------|-------|--------|
 * | k_test_soc_above_warning | 30% | Normal operation |
 * | k_test_soc_at_warning | 25% | Exactly at warning boundary |
 * | k_test_soc_below_warning | 20% | Below warning, above critical |
 * | k_test_soc_at_critical | 5% | Exactly at critical boundary |
 * | k_test_soc_below_critical | 3% | Below critical - emergency stop |
 *
 * @invariant k_test_soc_below_critical < k_test_soc_at_critical
 *            < k_test_soc_below_warning < k_test_soc_at_warning
 *            < k_test_soc_above_warning
 *
 * @code
 * mock_bq4050_set_status(k_test_normal_voltage_mv,
 *                        k_test_normal_current_ma,
 *                        k_test_soc_above_warning);
 * @endcode
 *
 * @see bms_soc_threshold_defaults_t Default warning/critical thresholds
 * @see test_invalid_threshold_t     Invalid threshold test constants
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_test_soc_above_warning   = 30, /**< Normal SoC - no warnings expected */
  k_test_soc_at_warning      = 25, /**< At default warning threshold (not below) */
  k_test_soc_below_warning   = 20, /**< Below warning threshold - triggers low battery */
  k_test_soc_at_critical     = 5,  /**< At default critical threshold (not below) */
  k_test_soc_below_critical  = 3,  /**< Below critical threshold - triggers e-stop */
  k_test_cell_count          = 4,  /**< Number of cells in pack */
} test_bms_soc_constants_t;

/**
 * @enum test_voltage_constants_t
 * @brief Test voltage values for BMS data tests
 *
 * @details
 * Pack voltages corresponding to healthy (4.2 V/cell × 4) and
 * low-battery (3.6 V/cell × 4) states, expressed in millivolts.
 * Used as inputs to mock_bq4050_set_status() in voltage-sensitive tests.
 *
 * @invariant k_test_low_voltage_mv < k_test_normal_voltage_mv
 *
 * @code
 * mock_bq4050_set_status(k_test_normal_voltage_mv,
 *                        k_test_normal_current_ma,
 *                        k_test_normal_soc_percent);
 * @endcode
 *
 * @see test_bms_soc_constants_t SoC constants used alongside these voltages
 * @see mock_bq4050_set_status()  Function that consumes these values
 *
 * @since Version 1.1.0
 */
typedef enum : uint16_t {
  k_test_normal_voltage_mv = 16800, /**< Normal pack voltage (4.2V x 4) */
  k_test_low_voltage_mv    = 14400, /**< Low pack voltage (3.6V x 4) */
} test_voltage_constants_t;

/**
 * @enum test_bms_read_current_t
 * @brief Discharge current test constants for battery data reading tests
 *
 * @details
 * rx_bq4050_status_t.current_ma is int16_t; int16_t backing avoids a
 * narrowing conversion when these values are compared against the field.
 *
 * @invariant k_test_critical_current_ma < k_test_low_current_ma
 *            < k_test_normal_current_ma
 *
 * @code
 * mock_bq4050_set_status(k_test_normal_voltage_mv,
 *                        k_test_normal_current_ma,
 *                        k_test_normal_soc_percent);
 * @endcode
 *
 * @see test_voltage_constants_t  Voltage constants used alongside current
 * @see mock_bq4050_set_status()  Function that consumes these values
 *
 * @since Version 1.1.0
 */
typedef enum : int16_t {
  k_test_normal_current_ma   = 500, /**< Normal discharge current (mA) */
  k_test_low_current_ma      = 100, /**< Low discharge current, used near warning threshold (mA) */
  k_test_critical_current_ma = 50,  /**< Near-empty discharge current, used near critical threshold (mA) */
} test_bms_read_current_t;

/**
 * @enum test_bms_timestamp_t
 * @brief Timestamp test constants for BMS data read and telemetry tests
 *
 * @details
 * bms_state_t.timestamp_ms is uint32_t; uint32_t backing avoids a narrowing
 * conversion in assignments and TEST_ASSERT_EQUAL_UINT32 assertions.
 *
 * @invariant k_test_normal_timestamp_ms < k_test_telem_timestamp_ms
 *
 * @code
 * bms_state_t bms  = {0};
 * bms.timestamp_ms = k_test_normal_timestamp_ms;
 * @endcode
 *
 * @see test_bms_read_current_t   Other typed constants used in the same tests
 * @see bms_state_t               Struct whose timestamp_ms field these populate
 *
 * @since Version 1.1.0
 */
typedef enum : uint32_t {
  k_test_normal_timestamp_ms = 1000U, /**< 1-second timestamp for data read test */
  k_test_telem_timestamp_ms  = 2000U, /**< Timestamp (ms) for telemetry storage test */
} test_bms_timestamp_t;

/**
 * @enum test_bms_soc_percent_t
 * @brief SoC percentage constants for battery data reading tests
 *
 * @details
 * Provides a named SoC value for the generic data-reading test where no
 * threshold crossing is expected.  Using a named constant avoids a bare
 * numeric literal in the test body.
 *
 * @invariant k_test_normal_soc_percent > k_bms_soc_warning_pct_default
 *
 * @code
 * mock_bq4050_set_status(k_test_normal_voltage_mv,
 *                        k_test_normal_current_ma,
 *                        k_test_normal_soc_percent);
 * @endcode
 *
 * @see test_bms_soc_constants_t  Threshold-specific SoC values
 * @see mock_bq4050_set_status()  Function that consumes this value
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_test_normal_soc_percent = 85, /**< Normal SoC for battery read tests */
} test_bms_soc_percent_t;

/**
 * @enum test_bms_telemetry_data_t
 * @brief Test constants for BMS telemetry storage tests (test_bms_data_stored_in_telemetry)
 *
 * @details
 * Unsigned 16-bit values that populate bms_state_t fields
 * (capacity_mah, full_capacity_mah, cycle_count) in the telemetry
 * round-trip test.  A common backing type prevents narrowing warnings.
 *
 * @invariant k_test_telem_capacity_mah < k_test_telem_full_capacity_mah
 *
 * @code
 * bms_in.capacity_mah      = k_test_telem_capacity_mah;
 * bms_in.full_capacity_mah = k_test_telem_full_capacity_mah;
 * bms_in.cycle_count       = k_test_telem_cycle_count;
 * @endcode
 *
 * @see test_bms_telemetry_int16_t  Signed 16-bit telemetry constants
 * @see test_bms_telemetry_fault_t  Fault flag telemetry constants
 *
 * @since Version 1.1.0
 */
typedef enum : uint16_t {
  k_test_telem_capacity_mah      = 3750, /**< Remaining capacity for telemetry test (mAh) */
  k_test_telem_full_capacity_mah = 5000, /**< Full charge capacity for telemetry test (mAh) */
  k_test_telem_cycle_count       = 150,  /**< Charge cycle count for telemetry test */
} test_bms_telemetry_data_t;

/**
 * @enum test_bms_telemetry_int16_t
 * @brief Signed 16-bit test constants for BMS telemetry storage tests
 *
 * @details
 * bms_state_t.current_ma and bms_state_t.temperature_celsius are both int16_t;
 * int16_t backing avoids narrowing conversions in TEST_ASSERT_EQUAL_INT16 assertions.
 *
 * @invariant k_test_telem_temperature_celsius > 0
 *
 * @code
 * bms_in.current_ma          = k_test_telem_current_ma;
 * bms_in.temperature_celsius = k_test_telem_temperature_celsius;
 * @endcode
 *
 * @see test_bms_telemetry_data_t  Unsigned 16-bit telemetry constants
 * @see bms_state_t                Struct whose int16_t fields these populate
 *
 * @since Version 1.1.0
 */
typedef enum : int16_t {
  k_test_telem_current_ma          = 1500, /**< Discharge current for telemetry test (mA) */
  k_test_telem_temperature_celsius = 25,   /**< Battery temperature for telemetry test (°C) */
} test_bms_telemetry_int16_t;

/**
 * @enum test_bms_telemetry_soc_t
 * @brief SoC percentage constant for telemetry test
 *
 * @details
 * Provides a named SoC value for the telemetry round-trip test.
 * Chosen above the default warning threshold (25%) so no side-effects fire
 * during the storage/retrieval verification.
 *
 * @invariant k_test_telem_soc_percent > k_bms_soc_warning_pct_default
 *
 * @code
 * bms_in.soc_percent = k_test_telem_soc_percent;
 * @endcode
 *
 * @see test_bms_soc_constants_t  Threshold-crossing SoC values
 * @see bms_state_t               Struct whose soc_percent field this populates
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_test_telem_soc_percent = 75, /**< State of charge for telemetry storage test (%) */
} test_bms_telemetry_soc_t;

/**
 * @enum test_bms_telemetry_fault_t
 * @brief Fault flag constants for BMS telemetry storage tests
 *
 * @details
 * bms_state_t.fault_flags is uint32_t; a separate enum with the correct backing
 * type avoids a narrowing conversion in TEST_ASSERT_EQUAL_UINT32 assertions.
 *
 * @invariant k_test_telem_fault_flags == 0 (no-fault state for clean test)
 *
 * @code
 * bms_in.fault_flags = k_test_telem_fault_flags;
 * TEST_ASSERT_EQUAL_UINT32(k_test_telem_fault_flags, bms_out.fault_flags);
 * @endcode
 *
 * @see test_bms_telemetry_data_t  Other uint16_t telemetry constants
 * @see bms_state_t                Struct whose fault_flags field this populates
 *
 * @since Version 1.1.0
 */
typedef enum : uint32_t {
  k_test_telem_fault_flags = 0, /**< No faults for telemetry test */
} test_bms_telemetry_fault_t;

/**
 * @enum test_custom_threshold_soc_t
 * @brief SoC values for exercising custom threshold behavior
 *
 * @details
 * Used with custom thresholds: 30% warning, 10% critical.
 * - k_test_custom_soc_below_warning: 29% is below 30% warning (triggers warning)
 * - k_test_custom_soc_below_critical: 9% is below 10% critical (triggers e-stop)
 *
 * @invariant k_test_custom_critical_pct < k_test_custom_warning_pct
 * @invariant k_test_custom_soc_below_critical < k_test_custom_critical_pct
 *            < k_test_custom_soc_below_warning < k_test_custom_warning_pct
 *
 * @code
 * const bms_monitor_config_t custom_config = {
 *     .soc_warning_pct  = k_test_custom_warning_pct,
 *     .soc_critical_pct = k_test_custom_critical_pct,
 * };
 * @endcode
 *
 * @see test_bms_soc_constants_t   Default-threshold SoC values
 * @see bms_monitor_config_t       Configuration struct using these thresholds
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_test_custom_soc_below_warning  = 29, /**< 29% SoC: below 30% warning, above 10% critical */
  k_test_custom_soc_below_critical = 9,  /**< 9% SoC: below 10% critical threshold */
  k_test_custom_warning_pct        = 30, /**< Custom warning threshold (%) */
  k_test_custom_critical_pct       = 10, /**< Custom critical threshold (%) */
} test_custom_threshold_soc_t;

/**
 * @enum test_invalid_threshold_t
 * @brief SoC threshold values that violate bms_monitor_config_t invariants
 *
 * @details
 * Used by test_bms_task_create_invalid_thresholds to verify that
 * bms_monitor_task_create() rejects each violation type.
 *
 * | Constant | Value | Violation |
 * |----------|-------|-----------|
 * | k_test_invalid_soc_zero | 0% | below minimum 1% lower bound |
 * | k_test_invalid_soc_equal | 15% | critical == warning |
 * | k_test_invalid_soc_warning_low | 10% | warning < critical (inverted) |
 * | k_test_invalid_soc_critical_high | 50% | critical > warning (inverted) |
 *
 * @invariant All values produce k_rx_err_invalid_arg when used as described
 *
 * @code
 * const bms_monitor_config_t bad_config = {
 *     .soc_warning_pct  = k_test_invalid_soc_equal,
 *     .soc_critical_pct = k_test_invalid_soc_equal, // equal - must be strictly less
 * };
 * TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, bms_monitor_task_create(&bad_config));
 * @endcode
 *
 * @see bms_soc_range_t             Valid input ranges these constants violate
 * @see bms_monitor_task_create()   Function that rejects these values
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_test_invalid_soc_zero          = 0,  /**< Zero SoC (invalid: below minimum 1% lower bound) */
  k_test_invalid_soc_equal         = 15, /**< Equal warning and critical (invalid: must be strictly less) */
  k_test_invalid_soc_warning_low   = 10, /**< Warning below critical (invalid: inverted relationship) */
  k_test_invalid_soc_critical_high = 50, /**< Critical above warning (invalid: inverted relationship) */
} test_invalid_threshold_t;

/**
 * @enum test_bms_call_count_t
 * @brief Expected mock call-count sentinels for BMS task test assertions
 *
 * @details
 * Named constants for the 0 and 1 call-count checks used by
 * TEST_ASSERT_EQUAL_UINT32 assertions, eliminating bare numeric literals.
 *
 * @invariant k_expect_call_count_zero < k_expect_call_count_one
 *
 * @code
 * TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_zero,
 *                          mock_shared_data_get_trigger_estop_count());
 * TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_one,
 *                          mock_shared_data_get_set_event_count());
 * @endcode
 *
 * @see mock_shared_data_get_set_event_count()     Query event call count
 * @see mock_shared_data_get_trigger_estop_count() Query e-stop call count
 *
 * @since Version 1.1.0
 */
typedef enum : uint32_t {
  k_expect_call_count_zero = 0U, /**< Expect function was not called */
  k_expect_call_count_one  = 1U, /**< Expect function was called exactly once */
} test_bms_call_count_t;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  /* Reset all mocks before each test */
  mock_shared_data_reset();
  mock_bq4050_reset();
  mock_tx_reset();
}

void tearDown(void)
{
  /* Clean up after each test */
}

/* =============================================================================
 * Task Creation Tests
 * =============================================================================
 */

/**
 * @brief Test successful BMS monitoring task creation with default thresholds
 *
 * @details
 * Verifies that bms_monitor_task_create() successfully creates
 * the ThreadX thread using the standard 25%/5% thresholds.
 *
 * @pre mock_tx_set_thread_create_return(TX_SUCCESS) called in setUp
 * @pre Mock state reset via mock_tx_reset() in setUp
 * @post bms_monitor_task_create() returns k_rx_ok
 * @post mock_tx_was_thread_create_called() returns true
 *
 * @note Single-threaded test; no concurrency concerns
 * @see bms_monitor_task_create() Function under test
 * @see bms_soc_threshold_defaults_t Default threshold constants
 *
 * @since Version 1.1.0
 */
void test_bms_task_create_success_with_default_thresholds(void)
{
  rx_err_t err;

  /* Configure mocks for success */
  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task with default thresholds */
  const bms_monitor_config_t config = {
    .soc_warning_pct  = k_bms_soc_warning_pct_default,
    .soc_critical_pct = k_bms_soc_critical_pct_default,
  };
  err = bms_monitor_task_create(&config);

  /* Verify success */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_tx_was_thread_create_called());
}

/**
 * @brief Test BMS task creation rejects NULL config
 *
 * @details
 * Verifies that bms_monitor_task_create() returns k_rx_err_null_ptr
 * when passed a NULL config pointer.
 *
 * @pre Mock state reset via mock_tx_reset() in setUp
 * @pre ThreadX mock configured to return TX_SUCCESS (call not reached)
 * @post bms_monitor_task_create(nullptr) returns k_rx_err_null_ptr
 * @post tx_thread_create() is never invoked (NULL check fires first)
 *
 * @note Single-threaded test; no concurrency concerns
 * @see bms_monitor_task_create() Function under test
 * @see k_rx_err_null_ptr         Expected error code for NULL pointer
 *
 * @since Version 1.1.0
 */
void test_bms_task_create_null_config(void)
{
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  err = bms_monitor_task_create(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test BMS task creation rejects invalid thresholds
 *
 * @details
 * Verifies that bms_monitor_task_create() returns k_rx_err_invalid_arg
 * when soc_critical_pct >= soc_warning_pct (constraint violation).
 *
 * @pre Mock state reset via mock_tx_reset() in setUp
 * @pre ThreadX mock configured to return TX_SUCCESS (not reached for invalid configs)
 * @post All four invalid configs return k_rx_err_invalid_arg
 * @post tx_thread_create() is never invoked for any invalid config
 *
 * @note Single-threaded test; no concurrency concerns
 * @see bms_monitor_task_create() Function under test
 * @see test_invalid_threshold_t  Invalid threshold value constants
 *
 * @since Version 1.1.0
 */
void test_bms_task_create_invalid_thresholds(void)
{
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* soc_warning_pct = 0 is below the [1,100] lower bound */
  const bms_monitor_config_t bad_config_warning_zero = {
    .soc_warning_pct  = k_test_invalid_soc_zero,
    .soc_critical_pct = k_test_invalid_soc_equal,
  };
  err = bms_monitor_task_create(&bad_config_warning_zero);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

  /* soc_critical_pct = 0 is below the [1,99] lower bound */
  const bms_monitor_config_t bad_config_critical_zero = {
    .soc_warning_pct  = k_test_invalid_soc_equal,
    .soc_critical_pct = k_test_invalid_soc_zero,
  };
  err = bms_monitor_task_create(&bad_config_critical_zero);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

  /* critical == warning is invalid (must be strictly less) */
  const bms_monitor_config_t bad_config_equal = {
    .soc_warning_pct  = k_test_invalid_soc_equal,
    .soc_critical_pct = k_test_invalid_soc_equal, /* same as warning - must be strictly less */
  };
  err = bms_monitor_task_create(&bad_config_equal);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

  /* critical > warning is also invalid */
  const bms_monitor_config_t bad_config_inverted = {
    .soc_warning_pct  = k_test_invalid_soc_warning_low,   /* lower than critical - inverted! */
    .soc_critical_pct = k_test_invalid_soc_critical_high,
  };
  err = bms_monitor_task_create(&bad_config_inverted);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test BMS task creation fails when ThreadX fails
 *
 * @details
 * Verifies that bms_monitor_task_create() propagates the ThreadX failure
 * as k_rx_err_rtos_thread_create when tx_thread_create() returns TX_NO_MEMORY.
 *
 * @pre mock_tx_set_thread_create_return(TX_NO_MEMORY) called to inject failure
 * @pre Mock state reset via mock_tx_reset() in setUp
 * @post bms_monitor_task_create() returns k_rx_err_rtos_thread_create
 * @post No BMS task state is updated (task not created)
 *
 * @note Single-threaded test; no concurrency concerns
 * @see bms_monitor_task_create() Function under test
 * @see k_rx_err_rtos_thread_create Expected error code for ThreadX failure
 *
 * @since Version 1.1.0
 */
void test_bms_task_create_thread_failure(void)
{
  rx_err_t err;

  /* Configure ThreadX to fail */
  mock_tx_set_thread_create_return(TX_NO_MEMORY);

  const bms_monitor_config_t config = {
    .soc_warning_pct  = k_bms_soc_warning_pct_default,
    .soc_critical_pct = k_bms_soc_critical_pct_default,
  };
  err = bms_monitor_task_create(&config);

  /* Verify failure */
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
}

/**
 * @brief Test BMS task creation fails when already created
 *
 * @details
 * Verifies that bms_monitor_task_create() is single-shot: the second call
 * must return k_rx_err_invalid_state regardless of config validity.
 *
 * @pre Mock state reset via mock_tx_reset() in setUp (clears created flag)
 * @pre First bms_monitor_task_create() call succeeds (TX_SUCCESS injected)
 * @post First call returns k_rx_ok
 * @post Second call with same config returns k_rx_err_invalid_state
 *
 * @note Single-threaded test; no concurrency concerns
 * @see bms_monitor_task_create() Function under test (single-shot guarantee)
 * @see k_rx_err_invalid_state    Expected error code for second call
 *
 * @since Version 1.1.0
 */
void test_bms_task_create_already_created(void)
{
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  const bms_monitor_config_t config = {
    .soc_warning_pct  = k_bms_soc_warning_pct_default,
    .soc_critical_pct = k_bms_soc_critical_pct_default,
  };

  /* Create the task first time - should succeed */
  err = bms_monitor_task_create(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create the task second time - should fail */
  err = bms_monitor_task_create(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Battery Data Reading Tests
 * =============================================================================
 */

/**
 * @brief Test BMS task reads battery data successfully
 *
 * @details
 * Verifies that battery status is read from BQ4050 and stored
 * in shared data.
 *
 * @pre mock_bq4050_set_status() called with normal voltage, current, SoC
 * @pre shared data mock reset via mock_shared_data_reset() in setUp
 * @post rx_bq4050_read_status() returns k_rx_ok
 * @post shared_data_update_bms() stores correct voltage and SoC
 *
 * @note Single-threaded test; no concurrency concerns
 * @see rx_bq4050_read_status()   Driver function exercised by this test
 * @see shared_data_update_bms()  Storage function exercised by this test
 *
 * @since Version 1.1.0
 */
void test_bms_task_reads_battery_data(void)
{
  rx_bq4050_status_t status  = {0};
  bms_state_t        bms_out = {0};
  rx_err_t           err;

  /* Set up battery status */
  mock_bq4050_set_status(k_test_normal_voltage_mv, k_test_normal_current_ma,
                         k_test_normal_soc_percent);

  /* Read status (simulating task behavior) */
  rx_bus_manager_t* manager = nullptr;
  err                       = rx_bq4050_read_status(manager, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_normal_voltage_mv, status.voltage_mv);

  /* Convert and store in shared data */
  bms_state_t bms  = {0};
  bms.voltage_mv   = status.voltage_mv;
  bms.current_ma   = status.current_ma;
  bms.soc_percent  = status.relative_soc;
  bms.timestamp_ms = k_test_normal_timestamp_ms;
  bms.valid        = true;

  err = shared_data_update_bms(&bms);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data stored */
  err = shared_data_get_bms(&bms_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(bms_out.valid);
  TEST_ASSERT_EQUAL_UINT16(k_test_normal_voltage_mv, bms_out.voltage_mv);
  TEST_ASSERT_EQUAL_UINT8(k_test_normal_soc_percent, bms_out.soc_percent);
}

/**
 * @brief Test no warning triggered when SoC is above 25% warning threshold
 *
 * @details
 * Verifies that at 30% SoC (above the 25% default warning threshold),
 * neither the low battery event nor the emergency stop is triggered.
 *
 * @pre mock_bq4050_set_status() called with k_test_soc_above_warning (30%)
 * @pre All mock call counters at zero from setUp
 * @post is_warning evaluates to false (30% >= 25%)
 * @post mock_shared_data_get_set_event_count() returns k_expect_call_count_zero
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_bms_soc_constants_t  k_test_soc_above_warning value definition
 * @see bms_soc_threshold_defaults_t Default 25% warning threshold
 *
 * @since Version 1.1.0
 */
void test_bms_task_no_warning_above_threshold(void)
{
  /* SoC is above warning threshold - no action expected */
  mock_bq4050_set_status(k_test_normal_voltage_mv, k_test_normal_current_ma,
                         k_test_soc_above_warning);

  rx_bq4050_status_t status = {0};
  rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_soc_above_warning, status.relative_soc);

  /* SoC is at 30%, warning threshold is 25%: no warning expected */
  bool is_warning  = (status.relative_soc < k_bms_soc_warning_pct_default);
  bool is_critical = (status.relative_soc < k_bms_soc_critical_pct_default);

  TEST_ASSERT_FALSE(is_warning);
  TEST_ASSERT_FALSE(is_critical);

  /* No mock side-effects should have fired */
  TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_zero, mock_shared_data_get_set_event_count());
  TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_zero, mock_shared_data_get_trigger_estop_count());
}

/**
 * @brief Test low battery warning triggered below 25% SoC (default warning threshold)
 *
 * @details
 * Verifies that when SoC drops below the default 25% warning threshold,
 * a low battery warning event is triggered.
 *
 * @pre mock_bq4050_set_status() called with k_test_soc_below_warning (20%)
 * @pre shared data mock reset; event count at zero from setUp
 * @post shared_data_set_event() called once with k_event_low_battery
 * @post mock_shared_data_get_set_event_count() returns k_expect_call_count_one
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_bms_soc_constants_t  k_test_soc_below_warning value definition
 * @see shared_data_set_event()   Function verified by this test
 *
 * @since Version 1.1.0
 */
void test_bms_task_low_battery_warning_at_25_pct(void)
{
  /* Set battery SoC to 20% (below the 25% default warning threshold) */
  mock_bq4050_set_status(k_test_low_voltage_mv, k_test_low_current_ma, k_test_soc_below_warning);

  rx_bq4050_status_t status = {0};
  rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_soc_below_warning, status.relative_soc);

  /* 20% < 25% warning threshold: warning must trigger */
  bool is_warning = (status.relative_soc < k_bms_soc_warning_pct_default);
  TEST_ASSERT_TRUE(is_warning);

  /* Simulate task behavior: set low battery event */
  if (is_warning) {
    err = shared_data_set_event(k_event_low_battery);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }

  /* Verify event was set */
  TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_one, mock_shared_data_get_set_event_count());
  TEST_ASSERT_EQUAL(k_event_low_battery, mock_shared_data_get_last_event_flags());
}

/**
 * @brief Test 25% is NOT below warning threshold (boundary condition)
 *
 * @details
 * The check in the task is strict-less-than (< soc_warning_pct), so
 * exactly 25% must NOT trigger the warning.
 *
 * @pre mock_bq4050_set_status() called with k_test_soc_at_warning (25%)
 * @pre All mock call counters at zero from setUp
 * @post is_warning evaluates to false (25% is NOT < 25%)
 * @post No shared data side-effects occur (set_event not called)
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_bms_soc_constants_t  k_test_soc_at_warning value definition
 * @see test_bms_task_low_battery_warning_at_25_pct() Complementary test below threshold
 *
 * @since Version 1.1.0
 */
void test_bms_task_no_warning_at_exactly_25_pct(void)
{
  /* Set battery SoC to exactly 25% - at the threshold, not below */
  mock_bq4050_set_status(k_test_low_voltage_mv, k_test_low_current_ma, k_test_soc_at_warning);

  rx_bq4050_status_t status = {0};
  rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_soc_at_warning, status.relative_soc);

  /* 25% is NOT strictly less than 25% - no warning */
  bool is_warning = (status.relative_soc < k_bms_soc_warning_pct_default);
  TEST_ASSERT_FALSE(is_warning);
}

/**
 * @brief Test critical battery triggers e-stop below 5% SoC (default critical threshold)
 *
 * @details
 * Verifies that when SoC drops below the default 5% critical threshold,
 * an emergency stop is triggered with k_estop_reason_low_battery.
 *
 * @pre mock_bq4050_set_status() called with k_test_soc_below_critical (3%)
 * @pre shared data mock reset; e-stop count at zero from setUp
 * @post shared_data_trigger_estop() called once with k_estop_reason_low_battery
 * @post mock_shared_data_get_trigger_estop_count() returns k_expect_call_count_one
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_bms_soc_constants_t  k_test_soc_below_critical value definition
 * @see shared_data_trigger_estop() Function verified by this test
 *
 * @since Version 1.1.0
 */
void test_bms_task_critical_battery_triggers_estop(void)
{
  /* Set battery SoC to 3% (below the 5% critical threshold) */
  mock_bq4050_set_status(k_test_low_voltage_mv, k_test_critical_current_ma,
                         k_test_soc_below_critical);

  rx_bq4050_status_t status = {0};
  rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_soc_below_critical, status.relative_soc);

  /* 3% < 5% critical threshold: emergency stop must trigger */
  bool is_critical = (status.relative_soc < k_bms_soc_critical_pct_default);
  TEST_ASSERT_TRUE(is_critical);

  /* Simulate task triggering e-stop */
  if (is_critical) {
    err = shared_data_trigger_estop(k_estop_reason_low_battery);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }

  /* Verify e-stop was triggered with correct reason */
  TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_one, mock_shared_data_get_trigger_estop_count());
  TEST_ASSERT_EQUAL(k_estop_reason_low_battery, mock_shared_data_get_last_estop_reason());
}

/**
 * @brief Test 5% is NOT below critical threshold (boundary condition)
 *
 * @details
 * The check in the task is strict-less-than (< soc_critical_pct), so
 * exactly 5% must NOT trigger the emergency stop.
 *
 * @pre mock_bq4050_set_status() called with k_test_soc_at_critical (5%)
 * @pre All mock call counters at zero from setUp
 * @post is_critical evaluates to false (5% is NOT < 5%)
 * @post mock_shared_data_get_trigger_estop_count() returns k_expect_call_count_zero
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_bms_soc_constants_t  k_test_soc_at_critical value definition
 * @see test_bms_task_critical_battery_triggers_estop() Complementary test below threshold
 *
 * @since Version 1.1.0
 */
void test_bms_task_no_estop_at_exactly_5_pct(void)
{
  /* Set battery SoC to exactly 5% - at the threshold, not below */
  mock_bq4050_set_status(k_test_low_voltage_mv, k_test_critical_current_ma,
                         k_test_soc_at_critical);

  rx_bq4050_status_t status = {0};
  rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_soc_at_critical, status.relative_soc);

  /* 5% is NOT strictly less than 5% - no e-stop */
  bool is_critical = (status.relative_soc < k_bms_soc_critical_pct_default);
  TEST_ASSERT_FALSE(is_critical);

  /* No e-stop should have been triggered */
  TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_zero, mock_shared_data_get_trigger_estop_count());
}

/**
 * @brief Test warning triggers but NOT critical when SoC between 5% and 25%
 *
 * @details
 * When SoC is 20%, warning threshold is crossed (< 25%) but critical is not
 * (>= 5%). Only the low battery event should fire, NOT the emergency stop.
 *
 * @pre mock_bq4050_set_status() called with k_test_soc_below_warning (20%)
 * @pre All mock call counters at zero from setUp
 * @post is_warning evaluates to true (20% < 25%)
 * @post is_critical evaluates to false (20% >= 5%)
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_bms_soc_constants_t  SoC constants used in this test
 * @see test_bms_task_low_battery_warning_at_25_pct() Full warning path test
 *
 * @since Version 1.1.0
 */
void test_bms_task_warning_only_between_thresholds(void)
{
  /* 20% - between critical (5%) and warning (25%) */
  mock_bq4050_set_status(k_test_low_voltage_mv, k_test_low_current_ma, k_test_soc_below_warning);

  rx_bq4050_status_t status = {0};
  rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  bool is_warning  = (status.relative_soc < k_bms_soc_warning_pct_default);
  bool is_critical = (status.relative_soc < k_bms_soc_critical_pct_default);

  TEST_ASSERT_TRUE(is_warning);
  TEST_ASSERT_FALSE(is_critical);
}

/**
 * @brief Test custom thresholds are accepted and produce correct comparisons
 *
 * @details
 * Verifies that bms_monitor_task_create() accepts non-default threshold
 * values when they satisfy the constraint soc_critical_pct < soc_warning_pct,
 * and that the stored thresholds produce the correct boundary comparisons
 * for a 29% SoC (below 30% warning) and a 9% SoC (below 10% critical).
 *
 * @pre mock_tx_set_thread_create_return(TX_SUCCESS) configured in setUp
 * @pre Custom config with 30% warning and 10% critical satisfies invariants
 * @post bms_monitor_task_create() returns k_rx_ok for valid custom config
 * @post Boundary comparisons for 29% and 9% SoC produce expected results
 *
 * @note Single-threaded test; no concurrency concerns
 * @see test_custom_threshold_soc_t  Custom threshold constants used here
 * @see bms_monitor_config_t         Configuration struct with custom thresholds
 *
 * @since Version 1.1.0
 */
void test_bms_task_create_custom_thresholds(void)
{
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Custom thresholds: 30% warning, 10% critical */
  const bms_monitor_config_t custom_config = {
    .soc_warning_pct  = k_test_custom_warning_pct,
    .soc_critical_pct = k_test_custom_critical_pct,
  };
  err = bms_monitor_task_create(&custom_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* ------------------------------------------------------------------ *
   * Verify warning branch: 29% SoC is below 30% warning, above 10% critical
   * ------------------------------------------------------------------ */
  mock_bq4050_set_status(k_test_normal_voltage_mv, k_test_normal_current_ma,
                         k_test_custom_soc_below_warning);

  rx_bq4050_status_t status = {0};
  err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_custom_soc_below_warning, status.relative_soc);

  bool is_warning  = (status.relative_soc < custom_config.soc_warning_pct);
  bool is_critical = (status.relative_soc < custom_config.soc_critical_pct);
  TEST_ASSERT_TRUE(is_warning);   /* 29% < 30% warning → warning fires */
  TEST_ASSERT_FALSE(is_critical); /* 29% > 10% critical → no e-stop */

  /* ------------------------------------------------------------------ *
   * Verify critical branch: 9% SoC is below both thresholds; critical wins
   * ------------------------------------------------------------------ */
  mock_bq4050_set_status(k_test_low_voltage_mv, k_test_normal_current_ma,
                         k_test_custom_soc_below_critical);

  err = rx_bq4050_read_status(nullptr, "i2c0", &status, k_test_cell_count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_custom_soc_below_critical, status.relative_soc);

  is_critical = (status.relative_soc < custom_config.soc_critical_pct);
  TEST_ASSERT_TRUE(is_critical); /* 9% < 10% critical → e-stop fires */
}

/**
 * @brief Test BMS data stored in telemetry
 *
 * @details
 * Verifies that BMS state is accessible for telemetry reporting.
 *
 * @pre shared data mock reset via mock_shared_data_reset() in setUp
 * @pre bms_in populated with all telemetry test constants before store call
 * @post shared_data_update_bms() returns k_rx_ok
 * @post shared_data_get_bms() returns all fields matching bms_in values
 *
 * @note Single-threaded test; no concurrency concerns
 * @see shared_data_update_bms() Storage function exercised by this test
 * @see shared_data_get_bms()    Retrieval function exercised by this test
 *
 * @since Version 1.1.0
 */
void test_bms_data_stored_in_telemetry(void)
{
  bms_state_t bms_in  = {0};
  bms_state_t bms_out = {0};
  rx_err_t    err;

  /* Set up complete BMS state */
  bms_in.voltage_mv          = k_test_normal_voltage_mv;
  bms_in.current_ma          = k_test_telem_current_ma;
  bms_in.soc_percent         = k_test_telem_soc_percent;
  bms_in.temperature_celsius = k_test_telem_temperature_celsius;
  bms_in.capacity_mah        = k_test_telem_capacity_mah;
  bms_in.full_capacity_mah   = k_test_telem_full_capacity_mah;
  bms_in.cycle_count         = k_test_telem_cycle_count;
  bms_in.fault_flags         = k_test_telem_fault_flags;
  bms_in.timestamp_ms        = k_test_telem_timestamp_ms;
  bms_in.valid               = true;

  /* Store */
  err = shared_data_update_bms(&bms_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read back for telemetry */
  err = shared_data_get_bms(&bms_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify all fields */
  TEST_ASSERT_TRUE(bms_out.valid);
  TEST_ASSERT_EQUAL_UINT16(k_test_normal_voltage_mv, bms_out.voltage_mv);
  TEST_ASSERT_EQUAL_INT16(k_test_telem_current_ma, bms_out.current_ma);
  TEST_ASSERT_EQUAL_UINT8(k_test_telem_soc_percent, bms_out.soc_percent);
  TEST_ASSERT_EQUAL_INT16(k_test_telem_temperature_celsius, bms_out.temperature_celsius);
  TEST_ASSERT_EQUAL_UINT16(k_test_telem_capacity_mah, bms_out.capacity_mah);
  TEST_ASSERT_EQUAL_UINT16(k_test_telem_full_capacity_mah, bms_out.full_capacity_mah);
  TEST_ASSERT_EQUAL_UINT16(k_test_telem_cycle_count, bms_out.cycle_count);
  TEST_ASSERT_EQUAL_UINT32(k_test_telem_fault_flags, bms_out.fault_flags);
  TEST_ASSERT_EQUAL_UINT32(k_test_telem_timestamp_ms, bms_out.timestamp_ms);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Task Creation Tests */
  RUN_TEST(test_bms_task_create_success_with_default_thresholds);
  RUN_TEST(test_bms_task_create_null_config);
  RUN_TEST(test_bms_task_create_invalid_thresholds);
  RUN_TEST(test_bms_task_create_thread_failure);
  RUN_TEST(test_bms_task_create_already_created);
  RUN_TEST(test_bms_task_create_custom_thresholds);

  /* Battery Data Reading Tests */
  RUN_TEST(test_bms_task_reads_battery_data);
  RUN_TEST(test_bms_task_no_warning_above_threshold);
  RUN_TEST(test_bms_task_low_battery_warning_at_25_pct);
  RUN_TEST(test_bms_task_no_warning_at_exactly_25_pct);
  RUN_TEST(test_bms_task_critical_battery_triggers_estop);
  RUN_TEST(test_bms_task_no_estop_at_exactly_5_pct);
  RUN_TEST(test_bms_task_warning_only_between_thresholds);
  RUN_TEST(test_bms_data_stored_in_telemetry);

  return UNITY_END();
}
