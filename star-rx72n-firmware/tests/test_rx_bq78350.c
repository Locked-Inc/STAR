/* tests/test_rx_bq78350.c */

/**
 * @file test_rx_bq78350.c
 * @brief Unit Tests for BQ78350-R1A Battery Fuel Gauge Driver
 * @details
 * Comprehensive tests for the BQ78350 driver including:
 * - Initialization tests with device type verification
 * - Voltage reading (pack and up to 16 cells)
 * - Current reading (instantaneous and average)
 * - State of charge (relative and absolute)
 * - Temperature reading with conversion to Celsius
 * - Capacity and cycle count reading
 * - Bulk telemetry read
 * - Device information read (manufacturer, serial, versions)
 * - Manufacturer access commands
 * - Error handling (NULL pointers, NACK, timeout, CRC)
 * - Data type conversions and edge cases
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdio.h>
#include <string.h>

#include "mocks/mock_rx_bus_smbus.h"
#include "rx_bq78350.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief SBS register addresses (matching rx_bq78350.h)
 */
typedef enum {
  k_test_sbs_manufacturer_access      = 0x00,
  k_test_sbs_temperature              = 0x08,
  k_test_sbs_voltage                  = 0x09,
  k_test_sbs_current                  = 0x0A,
  k_test_sbs_average_current          = 0x0B,
  k_test_sbs_relative_state_of_charge = 0x0D,
  k_test_sbs_absolute_state_of_charge = 0x0E,
  k_test_sbs_remaining_capacity       = 0x0F,
  k_test_sbs_full_charge_capacity     = 0x10,
  k_test_sbs_battery_status           = 0x16,
  k_test_sbs_cycle_count              = 0x17,
  k_test_sbs_design_capacity          = 0x18,
  k_test_sbs_design_voltage           = 0x19,
  k_test_sbs_serial_number            = 0x1C,
  k_test_sbs_manufacturer_name        = 0x20,
  k_test_sbs_device_name              = 0x21,
  k_test_sbs_device_chemistry         = 0x22,
  k_test_sbs_cell_voltage_4           = 0x3C,
  k_test_sbs_cell_voltage_3           = 0x3D,
  k_test_sbs_cell_voltage_2           = 0x3E,
  k_test_sbs_cell_voltage_1           = 0x3F,
} test_sbs_commands_t;

/**
 * @brief Manufacturer access subcommands
 */
typedef enum {
  k_test_mfg_device_type      = 0x0001,
  k_test_mfg_firmware_version = 0x0002,
  k_test_mfg_hardware_version = 0x0003,
  k_test_mfg_reset            = 0x0041,
} test_mfg_commands_t;

/**
 * @brief Test helper constants
 */
typedef enum {
  k_test_typical_voltage_mv  = 59200, /**< Typical 16S pack voltage (mV) */
  k_test_cell_voltage_mv     = 3700,  /**< Typical cell voltage (mV) */
  k_test_charging_ma         = 5000,  /**< Typical charging current (mA) */
  k_test_discharging_ma      = -8000, /**< Typical discharging current (mA) */
  k_test_temp_25c_0_1k       = 2981,  /**< 25.0 deg C in 0.1K units */
  k_test_temp_0c_0_1k        = 2731,  /**< 0.0 deg C in 0.1K units */
  k_test_temp_neg10c_0_1k    = 2631,  /**< -10.0 deg C in 0.1K units */
  k_test_soc_full            = 100,   /**< Full state of charge (%) */
  k_test_soc_half            = 50,    /**< Half state of charge (%) */
  k_test_capacity_full_mah   = 10000, /**< Full capacity (mAh) */
  k_test_capacity_half_mah   = 5000,  /**< Half capacity (mAh) */
  k_test_cycle_count         = 250,   /**< Cycle count */
  k_test_serial_number       = 0x1234, /**< Serial number */
  k_test_device_type         = 0x7835, /**< Device type for BQ78350 */
  k_test_firmware_version    = 0x0102, /**< Firmware version 1.2 */
  k_test_hardware_version    = 0x0001, /**< Hardware version 0.1 */
  k_test_design_voltage_mv   = 59200, /**< Design voltage (16S @ 3.7V) */
  k_test_num_cells_4s        = 4,     /**< 4-cell battery */
  k_test_num_cells_16s       = 16,    /**< 16-cell battery */
} test_values_t;

/* =============================================================================
 * Test Helpers
 * =============================================================================
 */

static rx_bus_manager_t s_manager;
static const char*      s_bus_name = "smbus_bq78350";

void setUp(void)
{
  /* Setup is done in test_setup() called by each test */
}

void tearDown(void)
{
  /* Cleanup if needed */
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
 * @brief Set up typical battery register values for bulk tests
 */
static void setup_typical_battery_values(void)
{
  /* Pack voltage and current */
  mock_smbus_set_word_response(k_test_sbs_voltage, k_test_typical_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_current, (uint16_t)k_test_charging_ma);
  mock_smbus_set_word_response(k_test_sbs_average_current, (uint16_t)k_test_charging_ma);

  /* State of charge */
  mock_smbus_set_word_response(k_test_sbs_relative_state_of_charge, k_test_soc_full);
  mock_smbus_set_word_response(k_test_sbs_absolute_state_of_charge, k_test_soc_half);

  /* Temperature */
  mock_smbus_set_word_response(k_test_sbs_temperature, k_test_temp_25c_0_1k);

  /* Capacity */
  mock_smbus_set_word_response(k_test_sbs_remaining_capacity, k_test_capacity_half_mah);
  mock_smbus_set_word_response(k_test_sbs_full_charge_capacity, k_test_capacity_full_mah);

  /* Status and cycle count */
  mock_smbus_set_word_response(k_test_sbs_battery_status, 0);
  mock_smbus_set_word_response(k_test_sbs_cycle_count, k_test_cycle_count);

  /* Cell voltages (4S for testing) */
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_1, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_2, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_3, k_test_cell_voltage_mv);
  mock_smbus_set_word_response(k_test_sbs_cell_voltage_4, k_test_cell_voltage_mv);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

static void test_init_success(void)
{
  test_setup();
  mock_smbus_set_initialized(false);

  /* BQ78350 init reads device type via manufacturer access */
  mock_smbus_set_word_response(k_test_sbs_manufacturer_access, k_test_device_type);

  rx_bq78350_handle_t handle;
  rx_bq78350_config_t config = {.bus_name = s_bus_name};

  rx_err_t err = rx_bq78350_init(&s_manager, &handle, &config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(handle.initialized);
  TEST_ASSERT_EQUAL_STRING(s_bus_name, handle.bus_name);
  TEST_ASSERT_TRUE(mock_smbus_was_init_called());
}

static void test_init_null_manager(void)
{
  test_setup();

  rx_bq78350_handle_t handle;
  rx_bq78350_config_t config = {.bus_name = s_bus_name};

  rx_err_t err = rx_bq78350_init(NULL, &handle, &config);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_init_null_handle(void)
{
  test_setup();

  rx_bq78350_config_t config = {.bus_name = s_bus_name};

  rx_err_t err = rx_bq78350_init(&s_manager, NULL, &config);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_init_null_config(void)
{
  test_setup();

  rx_bq78350_handle_t handle;

  rx_err_t err = rx_bq78350_init(&s_manager, &handle, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_init_device_not_responding(void)
{
  test_setup();
  mock_smbus_set_initialized(false);

  /* Simulate device timeout */
  mock_smbus_set_command_error(k_test_sbs_manufacturer_access, k_rx_err_timeout);

  rx_bq78350_handle_t handle;
  rx_bq78350_config_t config = {.bus_name = s_bus_name};

  rx_err_t err = rx_bq78350_init(&s_manager, &handle, &config);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Telemetry Reading Tests
 * =============================================================================
 */

static void test_read_telemetry_success(void)
{
  test_setup();
  setup_typical_battery_values();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  rx_bq78350_telemetry_t telemetry;

  rx_err_t err = rx_bq78350_read_telemetry(&handle, &telemetry);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify all telemetry fields */
  TEST_ASSERT_EQUAL(k_test_temp_25c_0_1k, telemetry.temperature_01k);
  TEST_ASSERT_EQUAL(k_test_typical_voltage_mv, telemetry.voltage_mv);
  TEST_ASSERT_EQUAL(k_test_charging_ma, telemetry.current_ma);
  TEST_ASSERT_EQUAL(k_test_charging_ma, telemetry.avg_current_ma);
  TEST_ASSERT_EQUAL(k_test_soc_full, telemetry.relative_soc_pct);
  TEST_ASSERT_EQUAL(k_test_soc_half, telemetry.absolute_soc_pct);
  TEST_ASSERT_EQUAL(k_test_capacity_half_mah, telemetry.remaining_mah);
  TEST_ASSERT_EQUAL(k_test_capacity_full_mah, telemetry.full_capacity_mah);
  TEST_ASSERT_EQUAL(0, telemetry.battery_status);
  TEST_ASSERT_EQUAL(k_test_cycle_count, telemetry.cycle_count);
}

static void test_read_telemetry_null_handle(void)
{
  test_setup();

  rx_bq78350_telemetry_t telemetry;

  rx_err_t err = rx_bq78350_read_telemetry(NULL, &telemetry);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_telemetry_null_data(void)
{
  test_setup();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};

  rx_err_t err = rx_bq78350_read_telemetry(&handle, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_read_telemetry_not_initialized(void)
{
  test_setup();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = false};
  rx_bq78350_telemetry_t telemetry;

  rx_err_t err = rx_bq78350_read_telemetry(&handle, &telemetry);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Cell Voltage Reading Tests
 * =============================================================================
 */

static void test_read_cell_voltages_4s(void)
{
  test_setup();
  setup_typical_battery_values();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  rx_bq78350_cell_voltages_t cells;

  rx_err_t err = rx_bq78350_read_cell_voltages(&handle, &cells, k_test_num_cells_4s);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_num_cells_4s, cells.num_cells);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mv, cells.cell_voltage_mv[0]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mv, cells.cell_voltage_mv[1]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mv, cells.cell_voltage_mv[2]);
  TEST_ASSERT_EQUAL(k_test_cell_voltage_mv, cells.cell_voltage_mv[3]);
}

static void test_read_cell_voltages_invalid_num_cells_zero(void)
{
  test_setup();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  rx_bq78350_cell_voltages_t cells;

  rx_err_t err = rx_bq78350_read_cell_voltages(&handle, &cells, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_read_cell_voltages_invalid_num_cells_too_many(void)
{
  test_setup();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  rx_bq78350_cell_voltages_t cells;

  rx_err_t err = rx_bq78350_read_cell_voltages(&handle, &cells, 17); /* Max is 16 */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Device Information Tests
 * =============================================================================
 */

static void test_read_device_info_success(void)
{
  test_setup();

  /* Set up string responses (block reads) */
  mock_smbus_set_block_response(k_test_sbs_manufacturer_name, (const uint8_t*)"Texas Instruments", 17);
  mock_smbus_set_block_response(k_test_sbs_device_name, (const uint8_t*)"BQ78350-R1A", 11);
  mock_smbus_set_block_response(k_test_sbs_device_chemistry, (const uint8_t*)"LION", 4);

  /* Set up word responses */
  mock_smbus_set_word_response(k_test_sbs_serial_number, k_test_serial_number);
  mock_smbus_set_word_response(k_test_sbs_design_capacity, k_test_capacity_full_mah);
  mock_smbus_set_word_response(k_test_sbs_design_voltage, k_test_design_voltage_mv);

  /* Manufacturer access responses */
  mock_smbus_set_word_response(k_test_sbs_manufacturer_access, k_test_firmware_version);
  mock_smbus_set_word_response(k_test_sbs_manufacturer_access, k_test_hardware_version);

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  rx_bq78350_device_info_t info;

  rx_err_t err = rx_bq78350_read_device_info(&handle, &info);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_STRING("Texas Instruments", info.manufacturer_name);
  TEST_ASSERT_EQUAL_STRING("BQ78350-R1A", info.device_name);
  TEST_ASSERT_EQUAL_STRING("LION", info.chemistry);
  TEST_ASSERT_EQUAL(k_test_serial_number, info.serial_number);
  TEST_ASSERT_EQUAL(k_test_capacity_full_mah, info.design_capacity_mah);
  TEST_ASSERT_EQUAL(k_test_design_voltage_mv, info.design_voltage_mv);
}

/* =============================================================================
 * Temperature Conversion Tests
 * =============================================================================
 */

static void test_temp_to_celsius_25c(void)
{
  int16_t celsius = rx_bq78350_temp_to_celsius(k_test_temp_25c_0_1k);
  TEST_ASSERT_EQUAL(25, celsius);
}

static void test_temp_to_celsius_0c(void)
{
  int16_t celsius = rx_bq78350_temp_to_celsius(k_test_temp_0c_0_1k);
  TEST_ASSERT_EQUAL(0, celsius);
}

static void test_temp_to_celsius_neg10c(void)
{
  int16_t celsius = rx_bq78350_temp_to_celsius(k_test_temp_neg10c_0_1k);
  TEST_ASSERT_EQUAL(-10, celsius);
}

static void test_celsius_to_temp_25c(void)
{
  int16_t temp_01k = rx_bq78350_celsius_to_temp(25);
  TEST_ASSERT_EQUAL(k_test_temp_25c_0_1k, temp_01k);
}

static void test_celsius_to_temp_0c(void)
{
  int16_t temp_01k = rx_bq78350_celsius_to_temp(0);
  TEST_ASSERT_EQUAL(k_test_temp_0c_0_1k, temp_01k);
}

static void test_celsius_to_temp_neg10c(void)
{
  int16_t temp_01k = rx_bq78350_celsius_to_temp(-10);
  TEST_ASSERT_EQUAL(k_test_temp_neg10c_0_1k, temp_01k);
}

/* =============================================================================
 * Manufacturer Access Tests
 * =============================================================================
 */

static void test_manufacturer_access_device_type(void)
{
  test_setup();

  mock_smbus_set_word_response(k_test_sbs_manufacturer_access, k_test_device_type);

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  uint16_t device_type;

  rx_err_t err = rx_bq78350_manufacturer_access(&handle, k_test_mfg_device_type, &device_type);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_device_type, device_type);
}

static void test_manufacturer_access_firmware_version(void)
{
  test_setup();

  mock_smbus_set_word_response(k_test_sbs_manufacturer_access, k_test_firmware_version);

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  uint16_t fw_version;

  rx_err_t err = rx_bq78350_manufacturer_access(&handle, k_test_mfg_firmware_version, &fw_version);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_firmware_version, fw_version);
}

/* =============================================================================
 * Error Handling Tests
 * =============================================================================
 */

static void test_read_word_smbus_timeout(void)
{
  test_setup();

  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_timeout);

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  uint16_t voltage;

  rx_err_t err = rx_bq78350_read_word(&handle, k_test_sbs_voltage, &voltage);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

static void test_read_word_smbus_nack(void)
{
  test_setup();

  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_nack);

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  uint16_t voltage;

  rx_err_t err = rx_bq78350_read_word(&handle, k_test_sbs_voltage, &voltage);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

static void test_read_word_crc_error(void)
{
  test_setup();

  mock_smbus_set_command_error(k_test_sbs_voltage, k_rx_err_crc_mismatch);

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};
  uint16_t voltage;

  rx_err_t err = rx_bq78350_read_word(&handle, k_test_sbs_voltage, &voltage);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/* =============================================================================
 * Reset Command Test
 * =============================================================================
 */

static void test_reset_success(void)
{
  test_setup();

  rx_bq78350_handle_t handle = {.bus_manager = &s_manager, .bus_name = s_bus_name, .initialized = true};

  rx_err_t err = rx_bq78350_reset(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_reset_null_handle(void)
{
  test_setup();

  rx_err_t err = rx_bq78350_reset(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
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
  RUN_TEST(test_init_null_handle);
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_device_not_responding);

  /* Telemetry tests */
  RUN_TEST(test_read_telemetry_success);
  RUN_TEST(test_read_telemetry_null_handle);
  RUN_TEST(test_read_telemetry_null_data);
  RUN_TEST(test_read_telemetry_not_initialized);

  /* Cell voltage tests */
  RUN_TEST(test_read_cell_voltages_4s);
  RUN_TEST(test_read_cell_voltages_invalid_num_cells_zero);
  RUN_TEST(test_read_cell_voltages_invalid_num_cells_too_many);

  /* Device info tests */
  RUN_TEST(test_read_device_info_success);

  /* Temperature conversion tests */
  RUN_TEST(test_temp_to_celsius_25c);
  RUN_TEST(test_temp_to_celsius_0c);
  RUN_TEST(test_temp_to_celsius_neg10c);
  RUN_TEST(test_celsius_to_temp_25c);
  RUN_TEST(test_celsius_to_temp_0c);
  RUN_TEST(test_celsius_to_temp_neg10c);

  /* Manufacturer access tests */
  RUN_TEST(test_manufacturer_access_device_type);
  RUN_TEST(test_manufacturer_access_firmware_version);

  /* Error handling tests */
  RUN_TEST(test_read_word_smbus_timeout);
  RUN_TEST(test_read_word_smbus_nack);
  RUN_TEST(test_read_word_crc_error);

  /* Reset command tests */
  RUN_TEST(test_reset_success);
  RUN_TEST(test_reset_null_handle);

  return UNITY_END();
}
