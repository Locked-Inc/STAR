/**
 * @file 015_bms_basic.c
 * @brief Basic BQ7850 BMS operations example
 *
 * Demonstrates:
 * - BMS initialization
 * - Reading device information
 * - Reading cell voltages
 * - Reading pack voltage
 * - Reading temperatures
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char *s_tag = "BMS_BASIC_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t s_manager;
static bq7850_handle_t    s_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Basic BQ7850 BMS Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "bms_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* --- Initialize BMS --- */
  ESP_LOGI(s_tag, "--- Initializing BMS ---");

  /* Create and add I2C/SMBus bus */
  star_bus_config_t* i2c_cfg = star_bus_config_create_i2c(
    "bms_bus", I2C_NUM_0, BQ7850_DEFAULT_ADDR,
    I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLK_SPEED
  );

  ret = star_bus_manager_add_bus(&s_manager, i2c_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add bus to manager");
    return;
  }

  ret = star_bus_config_init(i2c_cfg, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus");
    return;
  }

  /* Configure BMS */
  bq7850_config_t bms_config = {
    .num_cells = 16,  /* BQ7850 supports up to 16 cells */
    .num_temp = 3,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,  /* 5000 mAh */
    .design_voltage = 14800   /* 14.8V (16S * 3.7V nominal) */
  };

  ret = star_bms_bq7850_init(&s_bms, &s_manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init BMS: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "BMS initialized successfully");

  /* --- Read Device Information --- */
  ESP_LOGI(s_tag, "\n--- Device Information ---");

  bq7850_device_info_t info;
  ret = star_bms_bq7850_get_device_info(&s_bms, &info);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Device Type: 0x%04X", info.device_type);
    ESP_LOGI(s_tag, "Firmware Version: %d.%d", info.fw_version >> 8, info.fw_version & 0xFF);
    ESP_LOGI(s_tag, "Hardware Version: %d", info.hw_version);
    ESP_LOGI(s_tag, "Serial Number: %lu", (unsigned long)info.serial_number);
    ESP_LOGI(s_tag, "Manufacturer: %s", info.manufacturer);
    ESP_LOGI(s_tag, "Device Name: %s", info.device_name);
    ESP_LOGI(s_tag, "Chemistry: %s", info.chemistry);
  } else {
    ESP_LOGW(s_tag, "Failed to read device info: %s", esp_err_to_name(ret));
  }

  /* --- Read Cell Voltages --- */
  ESP_LOGI(s_tag, "\n--- Cell Voltages ---");

  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&s_bms, &cells);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Valid cells: %d", cells.valid_cells);
    ESP_LOGI(s_tag, "Pack voltage: %d mV", cells.pack_mv);

    /* Find min/max */
    uint16_t min_v = 0xFFFF, max_v = 0;
    int min_idx = -1, max_idx = -1;
    for (int i = 0; i < cells.valid_cells; i++) {
      if (cells.cell_mv[i] < min_v) {
        min_v = cells.cell_mv[i];
        min_idx = i;
      }
      if (cells.cell_mv[i] > max_v) {
        max_v = cells.cell_mv[i];
        max_idx = i;
      }
    }

    ESP_LOGI(s_tag, "Min voltage: %d mV (cell %d)", min_v, min_idx + 1);
    ESP_LOGI(s_tag, "Max voltage: %d mV (cell %d)", max_v, max_idx + 1);
    ESP_LOGI(s_tag, "Delta: %d mV", max_v - min_v);

    ESP_LOGI(s_tag, "\nIndividual cell voltages:");
    for (int i = 0; i < cells.valid_cells; i++) {
      ESP_LOGI(s_tag, "  Cell %2d: %d mV", i + 1, cells.cell_mv[i]);
    }
  } else {
    ESP_LOGW(s_tag, "Failed to read cells: %s", esp_err_to_name(ret));
  }

  /* Read single cell voltage */
  uint16_t cell_voltage;
  ret = star_bms_bq7850_read_cell_voltage(&s_bms, 0, &cell_voltage);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "\nCell 1 voltage: %d mV", cell_voltage);
  }

  /* --- Read Pack Voltage --- */
  ESP_LOGI(s_tag, "\n--- Pack Voltage ---");

  uint16_t pack_voltage;
  ret = star_bms_bq7850_read_pack_voltage(&s_bms, &pack_voltage);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pack voltage: %u mV (%.2f V)",
             pack_voltage, pack_voltage / 1000.0f);
  }

  /* --- Read Temperatures --- */
  ESP_LOGI(s_tag, "\n--- Temperatures ---");

  bq7850_temp_data_t temps;
  ret = star_bms_bq7850_read_temperatures(&s_bms, &temps);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Valid sensors: %d", temps.valid_sensors);
    ESP_LOGI(s_tag, "Average temp: %.1f C", star_bms_bq7850_convert_temp_to_celsius(temps.avg_temp_c));

    for (int i = 0; i < temps.valid_sensors; i++) {
      float celsius = star_bms_bq7850_convert_temp_to_celsius(temps.temp_c[i]);
      ESP_LOGI(s_tag, "  Sensor %d: %d (0.1C) = %.1f C", i + 1, temps.temp_c[i], celsius);
    }

    /* Find min/max */
    int16_t min_t = 32767, max_t = -32768;
    for (int i = 0; i < temps.valid_sensors; i++) {
      if (temps.temp_c[i] < min_t) min_t = temps.temp_c[i];
      if (temps.temp_c[i] > max_t) max_t = temps.temp_c[i];
    }
    ESP_LOGI(s_tag, "Min temp: %.1f C", star_bms_bq7850_convert_temp_to_celsius(min_t));
    ESP_LOGI(s_tag, "Max temp: %.1f C", star_bms_bq7850_convert_temp_to_celsius(max_t));
  }

  /* Read pack temperature */
  int16_t pack_temp;
  ret = star_bms_bq7850_read_temperature(&s_bms, &pack_temp);
  if (ret == ESP_OK) {
    float pack_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);
    ESP_LOGI(s_tag, "Pack temperature: %.1f C", pack_c);
  }

  /* --- Summary --- */
  ESP_LOGI(s_tag, "\n--- Summary ---");

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pack: %.2f V", pack_voltage / 1000.0f);

    /* Recalculate min/max for summary */
    uint16_t min_v = 0xFFFF, max_v = 0;
    for (int i = 0; i < cells.valid_cells; i++) {
      if (cells.cell_mv[i] < min_v) min_v = cells.cell_mv[i];
      if (cells.cell_mv[i] > max_v) max_v = cells.cell_mv[i];
    }
    ESP_LOGI(s_tag, "Cells: %d-%d mV (delta %d mV)", min_v, max_v, max_v - min_v);

    int16_t min_t = 32767, max_t = -32768;
    for (int i = 0; i < temps.valid_sensors; i++) {
      if (temps.temp_c[i] < min_t) min_t = temps.temp_c[i];
      if (temps.temp_c[i] > max_t) max_t = temps.temp_c[i];
    }
    ESP_LOGI(s_tag, "Temp range: %.1f - %.1f C",
             star_bms_bq7850_convert_temp_to_celsius(min_t),
             star_bms_bq7850_convert_temp_to_celsius(max_t));
  }

  /* Cleanup */
  star_bms_bq7850_deinit(&s_bms);
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nBasic BMS example complete");
}
