/* Test BQ7850 BMS component with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <string.h>

#include "star_bms_bq7850.h"

void setUp(void) {}
void tearDown(void) {}

/* Configuration Tests */

void test_config_structure(void)
{
  bq7850_config_t config = {
    .num_cells       = 4,
    .num_temp        = 1,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 2500,
    .design_voltage  = 14800,
  };

  TEST_ASSERT_EQUAL(4, config.num_cells);
  TEST_ASSERT_EQUAL(1, config.num_temp);
  TEST_ASSERT_EQUAL(BQ7850_DEFAULT_ADDR, config.smbus_addr);
  TEST_ASSERT_EQUAL(2500, config.design_capacity);
  TEST_ASSERT_EQUAL(14800, config.design_voltage);
}

void test_invalid_cell_count(void)
{
  bq7850_config_t config = {
    .num_cells = 17, /* Over max */
  };

  TEST_ASSERT_GREATER_THAN(BQ7850_MAX_CELLS, config.num_cells);
}

/* Data Structure Tests */

void test_cell_data_structure(void)
{
  bq7850_cell_data_t cell_data;
  memset(&cell_data, 0, sizeof(cell_data));

  cell_data.cell_mv[0]  = 3700;
  cell_data.cell_mv[1]  = 3710;
  cell_data.valid_cells = 2;
  cell_data.pack_mv     = 7410;

  TEST_ASSERT_EQUAL(2, cell_data.valid_cells);
  TEST_ASSERT_EQUAL(3700, cell_data.cell_mv[0]);
  TEST_ASSERT_EQUAL(7410, cell_data.pack_mv);
}

void test_temp_data_structure(void)
{
  bq7850_temp_data_t temp_data;
  memset(&temp_data, 0, sizeof(temp_data));

  temp_data.temp_c[0]     = 250; /* 25.0C */
  temp_data.valid_sensors = 1;
  temp_data.avg_temp_c    = 250;

  TEST_ASSERT_EQUAL(1, temp_data.valid_sensors);
  TEST_ASSERT_EQUAL(250, temp_data.temp_c[0]);
}

void test_current_data_structure(void)
{
  bq7850_current_data_t current_data;
  memset(&current_data, 0, sizeof(current_data));

  current_data.current_ma = 1500;
  current_data.voltage_mv = 14800;
  current_data.power_mw   = 22200;

  TEST_ASSERT_EQUAL(1500, current_data.current_ma);
  TEST_ASSERT_EQUAL(14800, current_data.voltage_mv);
  TEST_ASSERT_EQUAL(22200, current_data.power_mw);
}

void test_soc_data_structure(void)
{
  bq7850_soc_data_t soc_data;
  memset(&soc_data, 0, sizeof(soc_data));

  soc_data.remaining_capacity_mah = 2000;
  soc_data.full_capacity_mah      = 2500;
  soc_data.relative_soc           = 80;
  soc_data.cycle_count            = 42;

  TEST_ASSERT_EQUAL(2000, soc_data.remaining_capacity_mah);
  TEST_ASSERT_EQUAL(80, soc_data.relative_soc);
  TEST_ASSERT_EQUAL(42, soc_data.cycle_count);
}

void test_status_flags(void)
{
  bq7850_status_t status;
  memset(&status, 0, sizeof(status));

  status.charging    = true;
  status.discharging = false;

  TEST_ASSERT_TRUE(status.charging);
  TEST_ASSERT_FALSE(status.discharging);
}

/* Register Constant Tests */

void test_register_addresses(void)
{
  TEST_ASSERT_EQUAL(0x00, BQ7850_CMD_MANUFACTURER_ACCESS);
  TEST_ASSERT_EQUAL(0x09, BQ7850_CMD_VOLTAGE);
  TEST_ASSERT_EQUAL(0x0A, BQ7850_CMD_CURRENT);
  TEST_ASSERT_EQUAL(0x0D, BQ7850_CMD_RELATIVE_STATE_CHARGE);
  TEST_ASSERT_EQUAL(0x16, BQ7850_CMD_BATTERY_STATUS);
}

void test_safety_status_flags(void)
{
  uint16_t safety_status = 0;

  safety_status |= BQ7850_SAFETY_STATUS_CUV;
  TEST_ASSERT_TRUE(safety_status & BQ7850_SAFETY_STATUS_CUV);

  safety_status |= BQ7850_SAFETY_STATUS_COV;
  TEST_ASSERT_TRUE(safety_status & BQ7850_SAFETY_STATUS_COV);

  safety_status &= ~BQ7850_SAFETY_STATUS_CUV;
  TEST_ASSERT_FALSE(safety_status & BQ7850_SAFETY_STATUS_CUV);
}

void test_battery_status_flags(void)
{
  uint16_t battery_status = 0;

  battery_status |= BQ7850_BATTERY_STATUS_DSG;
  TEST_ASSERT_TRUE(battery_status & BQ7850_BATTERY_STATUS_DSG);

  battery_status |= BQ7850_BATTERY_STATUS_FC;
  TEST_ASSERT_TRUE(battery_status & BQ7850_BATTERY_STATUS_FC);
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_config_structure);
  RUN_TEST(test_invalid_cell_count);
  RUN_TEST(test_cell_data_structure);
  RUN_TEST(test_temp_data_structure);
  RUN_TEST(test_current_data_structure);
  RUN_TEST(test_soc_data_structure);
  RUN_TEST(test_status_flags);
  RUN_TEST(test_register_addresses);
  RUN_TEST(test_safety_status_flags);
  RUN_TEST(test_battery_status_flags);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
