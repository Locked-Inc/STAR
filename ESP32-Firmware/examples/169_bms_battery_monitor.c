/**
 * @file 169_bms_battery_monitor.c
 * @brief BQ7850 Battery Management System monitoring
 *
 * Demonstrates:
 * - Cell voltage monitoring
 * - Pack current measurement
 * - State of charge estimation
 * - Protection status
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bms_bq7850.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "BMS_MONITOR";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define I2C_CLK_SPEED   (100000)

void bms_monitor_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "BMS_Manager", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create and configure I2C/SMBus bus */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "bms_bus",
    I2C_NUM_0,
    BQ7850_DEFAULT_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  ret = star_bus_manager_add_bus(&manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add bus to manager");
    star_bus_manager_deinit(&manager);
    return;
  }

  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Initialize BMS */
  bq7850_handle_t bms;
  bq7850_config_t bms_config = {
    .num_cells = 4,
    .num_temp = 1,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage = 14800
  };

  ret = star_bms_bq7850_init(&bms, &manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init BMS: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "BQ7850 BMS initialized - %d cell configuration", bms_config.num_cells);

  /* Main monitoring loop */
  for (int i = 0; i < 100; i++) {
    /* Read complete battery state */
    bq7850_battery_state_t state;
    if (star_bms_bq7850_read_battery_state(&bms, &state) == ESP_OK) {
      ESP_LOGI(s_tag, "=== Battery Status ===");
      ESP_LOGI(s_tag, "Pack Voltage: %.2f V", state.cells.pack_mv / 1000.0f);
      ESP_LOGI(s_tag, "Pack Current: %.2f A", state.current.current_ma / 1000.0f);
      ESP_LOGI(s_tag, "Temperature: %.1f C", state.temps.avg_temp_c / 10.0f);
      ESP_LOGI(s_tag, "SOC: %d%%", state.soc.relative_soc);

      ESP_LOGI(s_tag, "Cell Voltages:");
      uint16_t min_v = 0xFFFF, max_v = 0;
      for (int c = 0; c < state.cells.valid_cells; c++) {
        ESP_LOGI(s_tag, "  Cell %d: %.3f V", c + 1, state.cells.cell_mv[c] / 1000.0f);
        if (state.cells.cell_mv[c] < min_v) min_v = state.cells.cell_mv[c];
        if (state.cells.cell_mv[c] > max_v) max_v = state.cells.cell_mv[c];
      }

      float cell_diff = (max_v - min_v) / 1000.0f;
      ESP_LOGI(s_tag, "Cell Imbalance: %.3f V", cell_diff);

      /* Check protection status */
      if (state.status.fault_active) {
        ESP_LOGW(s_tag, "Protection Active - Safety Status: 0x%04X", state.status.safety_status);
        if (state.status.safety_status & BQ7850_SAFETY_STATUS_COV) {
          ESP_LOGW(s_tag, "  - Cell Over Voltage");
        }
        if (state.status.safety_status & BQ7850_SAFETY_STATUS_CUV) {
          ESP_LOGW(s_tag, "  - Cell Under Voltage");
        }
        if (state.status.safety_status & BQ7850_SAFETY_STATUS_OCC) {
          ESP_LOGW(s_tag, "  - Over Current Charge");
        }
        if (state.status.safety_status & BQ7850_SAFETY_STATUS_OCD) {
          ESP_LOGW(s_tag, "  - Over Current Discharge");
        }
        if (state.status.safety_status & BQ7850_SAFETY_STATUS_OTC) {
          ESP_LOGW(s_tag, "  - Over Temperature Charge");
        }
        if (state.status.safety_status & BQ7850_SAFETY_STATUS_OTD) {
          ESP_LOGW(s_tag, "  - Over Temperature Discharge");
        }
      } else {
        ESP_LOGI(s_tag, "Protection: OK");
      }

      /* Charge/discharge status */
      if (state.status.charging) {
        ESP_LOGI(s_tag, "Status: CHARGING");
      } else if (state.status.discharging) {
        ESP_LOGI(s_tag, "Status: DISCHARGING");
      } else if (state.status.fully_charged) {
        ESP_LOGI(s_tag, "Status: FULLY CHARGED");
      } else {
        ESP_LOGI(s_tag, "Status: IDLE");
      }
    } else {
      ESP_LOGW(s_tag, "Failed to read battery state");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_bms_bq7850_deinit(&bms);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { bms_monitor_example(); }
