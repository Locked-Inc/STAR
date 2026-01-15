/* tests/test_rx_bq4050.c */

/**
 * @file test_rx_bq4050.c
 * @brief Unit Tests for BQ4050 Battery Fuel Gauge Driver
 * @details
 * Comprehensive tests for the BQ4050 driver including:
 * - Initialization tests
 * - Voltage reading (pack and cell)
 * - Current reading (instantaneous and average)
 * - State of charge (relative and absolute)
 * - Temperature reading with conversion
 * - Capacity reading
 * - Bulk status read
 * - Error handling (NULL pointers, NACK, timeout, CRC)
 * - Data type conversions and edge cases
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdio.h>
#include <string.h>

#include "mocks/mock_rx_bus_smbus.h"
#include "rx_bq4050.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief SBS register addresses (same as in rx_bq4050.c)
 */
typedef enum {
  k_test_sbs_temperature              = 0x08,
  k_test_sbs_voltage                  = 0x09,
  k_test_sbs_current                  = 0x0A,
  k_test_sbs_average_current          = 0x0B,
  k_test_sbs_relative_state_of_charge = 0x0D,
  k_test_sbs_absolute_state_of_charge = 0x0E,
  k_test_sbs_remaining_capacity       = 0x0F,
  k_test_sbs_full_charge_capacity     = 0x10,
  k_test_sbs_run_time_to_empty        = 0x11,
  k_test_sbs_average_time_to_full     = 0x13,
  k_test_sbs_battery_status           = 0x16,
  k_test_sbs_cycle_count              = 0x17,
  k_test_sbs_design_capacity          = 0x18,
  k_test_sbs_cell_voltage_4           = 0x3C,
  k_test_sbs_cell_voltage_3           = 0x3D,
  k_test_sbs_cell_voltage_2           = 0x3E,
  k_test_sbs_cell_voltage_1           = 0x3F,
} test_sbs_commands_t;

/**
 * @brief Battery status flag bits (same as in rx_bq4050.c)
 */
typedef enum {
  k_test_status_remaining_capacity_alarm = (1 << 9),
  k_test_status_discharging              = (1 << 6),
  k_test_status_fully_charged            = (1 << 5),
  k_test_status_fully_discharged         = (1 << 4),
} test_status_flags_t;

/**
 * @brief Test helper constants
 */
typedef enum {
  k_test_typical_voltage_mv = 14800, /**< Typical 4S pack voltage (mV) */
  k_test_cell_voltage_mv    = 3700,  /**< Typical cell voltage (mV) */
  k_test_cell_voltage_high_mv   = 4200, /**< High cell voltage (mV) */
  k_test_cell_voltage_mid_hi_mv = 4150, /**< Mid-high cell voltage (mV) */
  k_test_cell_voltage_mid_lo_mv = 4100, /**< Mid-low cell voltage (mV) */
  k_test_cell_voltage_low_mv    = 4050, /**< Low cell voltage (mV) */
  k_test_charging_ma        = 2000,  /**< Typical charging current (mA) */
  k_test_discharging_ma     = -1500, /**< Typical discharging current (mA) */
  k_test_charging_current_2a = 2000, /**< Charging current (mA) */
  k_test_discharge_current   = -1500, /**< Discharging current (mA) */
  k_test_avg_current         = 1800, /**< Average current (mA) */
  k_test_temp_25c_0_1k      = 2981,  /**< 25.0 deg C in 0.1K units */
  k_test_temp_0c_0_1k       = 2731,  /**< 0.0 deg C in 0.1K units */
  k_test_temp_neg10c_0_1k   = 2631,  /**< -10.0 deg C in 0.1K units */
  k_test_soc_full           = 100,   /**< Full state of charge (%) */
  k_test_soc_half           = 50,    /**< Half state of charge (%) */
  k_test_soc_over_range     = 105,   /**< Over-range state of charge (%) */
  k_test_soc_way_over       = 200,   /**< Way over-range state of charge (%) */
  k_test_soc_abs            = 85,    /**< Absolute state of charge (%) */
  k_test_capacity_full_mah  = 5000,  /**< Full capacity (mAh) */
  k_test_capacity_half_mah  = 2500,  /**< Half capacity (mAh) */
  k_test_time_to_empty_min  = 120,   /**< Time to empty (min) */
  k_test_cycle_count        = 150,   /**< Cycle count */
} test_values_t;

/* =============================================================================
 * Test Helpers
 * =============================================================================
 */

static rx_bus_manager_t s_manager;
static const char*      s_bus_name = "smbus_fuel_gauge";

static void test_setup(void);

void setUp(void)
{
  test_setup();
}

void tearDown(void)
{
  /* No-op for these tests */
}

/**
 * @brief Set up test environment before each test
 */
static void test_setup(void)
{
  mock_smbus_reset();
  memset(&s_manager, 0, sizeof(s_manager));
  s_manager.tag = "TEST";

  /* Set default initialized state */
  mock_smbus_set_initialized(true);
}

/**
 * @brief Set up typical battery register values for bulk status tests
 */
static void setup_typical_battery_values(void)
{
  mock_smbus_set_word_response(k_test_sbs_voltage, k_test_typical_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_1, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_2, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_3, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_4, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_current, (uint16_t)k_test_charging_ma);
  mock_smbus_set_word_response(k_test_sbs_average_current, (uint16_t)k_test_charging_ma);
  mock_smbus_set_word_response(k_test_sbs_relative_state_of_charge, k_test_soc_full);
  mock_smbus_set_word_response(k_test_sbs_absolute_state_of_charge, k_test_soc_half);
  mock_smbus_set_word_response(k_test_sbs_temperature, k_test_temp_25c_0_1k);
  mock_smbus_set_word_response(k_test_sbs_remaining_capacity, k_test_capacity_half_mah);
  mock_smbus_set_word_response(k_test_sbs_full_charge_capacity, k_test_capacity_full_mah);
  mock_smbus_set_word_response(k_test_sbs_design_capacity, k_test_capacity_full_mah);
  mock_smbus_set_word_response(k_test_sbs_cycle_count, k_test_cycle_count);
  mock_smbus_set_word_response(k_test_sbs_run_time_to_empty, k_test_time_to_empty_min);
  mock_smbus_set_word_response(k_test_sbs_average_time_to_full, 0xFFFF);
  mock_smbus_set_word_response(k_test_sbs_battery_status, 0);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

static void test_init_success(void)
{
  test_setup();
  mock_smbus_set_initialized(false);
  mock_smbus_set_word_response(k_test_sbs_voltage, k_test_typical_voltage_mv);

  rx_err_t err = rx_bq4050_init(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_MESSAGE(mock_smbus_was_init_called(), "SMBus init should be called");
}

static void test_init_null_manager(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_init(NULL, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_init_null_bus_name(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_init(&s_manager, NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_init_smbus_fail(void)
{
  test_setup();
  mock_smbus_set_initialized(false);
  mock_smbus_set_next_error(k_rx_err_hw_init_failed);

  rx_err_t err = rx_bq4050_init(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_hw_init_failed, err);
}

static void test_init_communication_fail(void)
{
  test_setup();
  mock_smbus_set_initialized(false);
  /* Init will succeed but voltage read will fail */
  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_nack);

  rx_err_t err = rx_bq4050_init(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/* =============================================================================
 * Voltage Reading Tests
 * =============================================================================
 */

static void test_read_voltage_success(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_voltage, k_test_typical_voltage_mv);

  uint16_t voltage_mv;
  rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_typical_voltage_mv, voltage_mv);
}

static void test_read_voltage_null_manager(void)
{
  test_setup();

  uint16_t voltage_mv;
  rx_err_t err = rx_bq4050_read_voltage(NULL, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_voltage_null_output(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_voltage_timeout(void)
{
  test_setup();
  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_timeout);

  uint16_t voltage_mv;
  rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Cell Voltage Reading Tests
 * =============================================================================
 */

static void test_read_cell_voltages_1_cell(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_1, k_test_cell_voltage_high_mv);

  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, 1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_high_mv, cell_voltages[0]);
}

static void test_read_cell_voltages_4_cells(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_1, k_test_cell_voltage_high_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_2, k_test_cell_voltage_mid_hi_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_3, k_test_cell_voltage_mid_lo_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_4, k_test_cell_voltage_low_mv);

  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, 4);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_high_mv, cell_voltages[0]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mid_hi_mv, cell_voltages[1]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mid_lo_mv, cell_voltages[2]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_low_mv, cell_voltages[3]);
}

static void test_read_cell_voltages_null_array(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, NULL, 1);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_cell_voltages_too_many_cells(void)
{
  test_setup();

  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, 5);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_read_cell_voltages_partial_failure(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_1, k_test_cell_voltage_high_mv);
  mock_smbus_set_command_error(k_test_sbs_cell_voltage_2, k_rx_err_crc_mismatch);

  uint16_t cell_voltages[k_bq4050_max_cells] = {0};
  rx_err_t err = rx_bq4050_read_cell_voltages(&s_manager, s_bus_name, cell_voltages, 2);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/* =============================================================================
 * Current Reading Tests
 * =============================================================================
 */

static void test_read_current_charging(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_current, (uint16_t)k_test_charging_current_2a);

  int16_t  current_ma;
  rx_err_t err = rx_bq4050_read_current(&s_manager, s_bus_name, &current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_charging_current_2a, current_ma);
}

static void test_read_current_discharging(void)
{
  test_setup();
  /* -1500 as unsigned 16-bit */
  mock_smbus_set_word_response(k_test_sbs_current, (uint16_t)k_test_discharge_current);

  int16_t  current_ma;
  rx_err_t err = rx_bq4050_read_current(&s_manager, s_bus_name, &current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_discharge_current, current_ma);
}

static void test_read_current_idle(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_current, 0);

  int16_t  current_ma;
  rx_err_t err = rx_bq4050_read_current(&s_manager, s_bus_name, &current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, current_ma);
}

static void test_read_current_null_output(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_current(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_average_current_success(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_average_current, (uint16_t)k_test_avg_current);

  int16_t  avg_current_ma;
  rx_err_t err = rx_bq4050_read_average_current(&s_manager, s_bus_name, &avg_current_ma);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_avg_current, avg_current_ma);
}

static void test_read_average_current_null_output(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_average_current(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * State of Charge Tests
 * =============================================================================
 */

static void test_read_relative_soc_full(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_relative_state_of_charge, k_test_soc_full);

  uint8_t  soc;
  rx_err_t err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_full, soc);
}

static void test_read_relative_soc_half(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_relative_state_of_charge, k_test_soc_half);

  uint8_t  soc;
  rx_err_t err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_half, soc);
}

static void test_read_relative_soc_empty(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_relative_state_of_charge, 0);

  uint8_t  soc;
  rx_err_t err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, soc);
}

static void test_read_relative_soc_clamped(void)
{
  test_setup();
  /* SBS allows values > 100% in some conditions */
  mock_smbus_set_word_response(k_test_sbs_relative_state_of_charge, k_test_soc_over_range);

  uint8_t  soc;
  rx_err_t err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(100, soc);
}

static void test_read_relative_soc_null_output(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_relative_soc(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_absolute_soc_success(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_absolute_state_of_charge, k_test_soc_abs);

  uint8_t  soc;
  rx_err_t err = rx_bq4050_read_absolute_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_soc_abs, soc);
}

static void test_read_absolute_soc_clamped(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_absolute_state_of_charge, k_test_soc_way_over);

  uint8_t  soc;
  rx_err_t err = rx_bq4050_read_absolute_soc(&s_manager, s_bus_name, &soc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(100, soc);
}

/* =============================================================================
 * Temperature Reading Tests
 * =============================================================================
 */

static void test_read_temperature_25c(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_temperature, k_test_temp_25c_0_1k);

  int16_t  temp_c;
  rx_err_t err = rx_bq4050_read_temperature(&s_manager, s_bus_name, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(25, temp_c);
}

static void test_read_temperature_0c(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_temperature, k_test_temp_0c_0_1k);

  int16_t  temp_c;
  rx_err_t err = rx_bq4050_read_temperature(&s_manager, s_bus_name, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, temp_c);
}

static void test_read_temperature_negative(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_temperature, k_test_temp_neg10c_0_1k);

  int16_t  temp_c;
  rx_err_t err = rx_bq4050_read_temperature(&s_manager, s_bus_name, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(-10, temp_c);
}

static void test_read_temperature_null_output(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_temperature(&s_manager, s_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * Capacity Reading Tests
 * =============================================================================
 */

static void test_read_capacity_success(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_remaining_capacity, k_test_capacity_half_mah);
  mock_smbus_set_word_response(k_test_sbs_full_charge_capacity, k_test_capacity_full_mah);

  uint16_t remaining_mah, full_mah;
  rx_err_t err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_capacity_half_mah, remaining_mah);
  TEST_ASSERT_EQUAL(k_test_capacity_full_mah, full_mah);
}

static void test_read_capacity_null_remaining(void)
{
  test_setup();

  uint16_t full_mah;
  rx_err_t err = rx_bq4050_read_capacity(&s_manager, s_bus_name, NULL, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_capacity_null_full(void)
{
  test_setup();

  uint16_t remaining_mah;
  rx_err_t err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_capacity_first_read_fail(void)
{
  test_setup();
  mock_smbus_set_command_error(k_test_sbs_remaining_capacity, k_rx_err_timeout);

  uint16_t remaining_mah, full_mah;
  rx_err_t err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

static void test_read_capacity_second_read_fail(void)
{
  test_setup();
  mock_smbus_set_word_response(k_test_sbs_remaining_capacity, 2500);
  mock_smbus_set_command_error(k_test_sbs_full_charge_capacity, k_rx_err_nack);

  uint16_t remaining_mah, full_mah;
  rx_err_t err = rx_bq4050_read_capacity(&s_manager, s_bus_name, &remaining_mah, &full_mah);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/* =============================================================================
 * Bulk Status Read Tests
 * =============================================================================
 */

static void test_read_status_success(void)
{
  test_setup();
  setup_typical_battery_values();

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 4);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_typical_voltage_mv, status.voltage_mv);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mv, status.cell_voltages_mv[0]);
  TEST_ASSERT_EQUAL(k_test_soc_full, status.relative_soc);
  TEST_ASSERT_EQUAL(25, status.temperature_c);
}

static void test_read_status_null_status(void)
{
  test_setup();

  rx_err_t err = rx_bq4050_read_status(&s_manager, s_bus_name, NULL, 4);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_status_too_many_cells(void)
{
  test_setup();

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 5);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_read_status_charging_flags(void)
{
  test_setup();
  setup_typical_battery_values();
  /* Clear discharging flag = charging */
  mock_smbus_set_word_response(k_test_sbs_battery_status, 0);

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_charging);
  TEST_ASSERT_TRUE(!status.is_fully_discharged);
}

static void test_read_status_discharging_flags(void)
{
  test_setup();
  setup_typical_battery_values();
  mock_smbus_set_word_response(k_test_sbs_battery_status, k_test_status_discharging);

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(!status.is_charging);
}

static void test_read_status_fully_charged_flags(void)
{
  test_setup();
  setup_typical_battery_values();
  mock_smbus_set_word_response(k_test_sbs_battery_status, k_test_status_fully_charged);

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_fully_charged);
}

static void test_read_status_fully_discharged_flags(void)
{
  test_setup();
  setup_typical_battery_values();
  mock_smbus_set_word_response(k_test_sbs_battery_status,
                               k_test_status_discharging | k_test_status_fully_discharged);

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_fully_discharged);
}

static void test_read_status_low_capacity_alarm(void)
{
  test_setup();
  setup_typical_battery_values();
  mock_smbus_set_word_response(k_test_sbs_battery_status,
                               k_test_status_discharging | k_test_status_remaining_capacity_alarm);

  rx_bq4050_status_t status = {0};
  rx_err_t           err    = rx_bq4050_read_status(&s_manager, s_bus_name, &status, 1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.is_low_capacity);
}

/* =============================================================================
 * Error Handling Tests
 * =============================================================================
 */

static void test_read_voltage_nack(void)
{
  test_setup();
  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_nack);

  uint16_t voltage_mv;
  rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

static void test_read_voltage_crc_mismatch(void)
{
  test_setup();
  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_crc_mismatch);

  uint16_t voltage_mv;
  rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

static void test_read_voltage_bus_not_initialized(void)
{
  test_setup();
  mock_smbus_set_initialized(false);

  uint16_t voltage_mv;
  rx_err_t err = rx_bq4050_read_voltage(&s_manager, s_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
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
