/**
 * @file 016_bms_monitoring.c
 * @brief BMS current, power, and SOC monitoring example
 *
 * Demonstrates:
 * - Reading current measurements
 * - Reading power calculations
 * - Reading state of charge
 * - Reading battery status
 * - Continuous monitoring
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "BMS_MONITORING_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t s_manager;
static bq7850_handle_t    s_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== BMS Monitoring Example ===\n");

  /* Initialize bus manager and BMS */
  ret = star_bus_manager_init(&s_manager, "monitor_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

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
    .num_cells = 16,
    .num_temp = 3,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage = 14800
  };

  ret = star_bms_bq7850_init(&s_bms, &s_manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init BMS: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "BMS initialized");

  /* --- Read Current --- */
  ESP_LOGI(s_tag, "\n--- Current Measurements ---");

  bq7850_current_data_t current;
  ret = star_bms_bq7850_read_current(&s_bms, &current);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Instantaneous current: %d mA", current.current_ma);
    ESP_LOGI(s_tag, "Average current: %d mA", current.avg_current_ma);
    ESP_LOGI(s_tag, "Power: %d mW", current.power_mw);

    /* Interpret current direction */
    if (current.current_ma > 0) {
      ESP_LOGI(s_tag, "Status: Charging at %d mA", current.current_ma);
    } else if (current.current_ma < 0) {
      ESP_LOGI(s_tag, "Status: Discharging at %d mA", -current.current_ma);
    } else {
      ESP_LOGI(s_tag, "Status: Idle");
    }
  } else {
    ESP_LOGW(s_tag, "Failed to read current: %s", esp_err_to_name(ret));
  }

  /* --- Read State of Charge --- */
  ESP_LOGI(s_tag, "\n--- State of Charge ---");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&s_bms, &soc);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Relative SOC: %d%%", soc.relative_soc);
    ESP_LOGI(s_tag, "Absolute SOC: %d%%", soc.absolute_soc);
    ESP_LOGI(s_tag, "Remaining capacity: %d mAh", soc.remaining_capacity_mah);
    ESP_LOGI(s_tag, "Full charge capacity: %d mAh", soc.full_capacity_mah);
    ESP_LOGI(s_tag, "Cycle count: %d", soc.cycle_count);

    /* Calculate usage */
    int used = soc.full_capacity_mah - soc.remaining_capacity_mah;
    ESP_LOGI(s_tag, "Used: %d mAh", used);

    /* Estimate remaining time */
    if (current.current_ma < 0 && soc.remaining_capacity_mah > 0) {
      int minutes = (soc.remaining_capacity_mah * 60) / (-current.current_ma);
      ESP_LOGI(s_tag, "Est. time remaining: %d:%02d", minutes / 60, minutes % 60);
    } else if (current.current_ma > 0) {
      int to_full = soc.full_capacity_mah - soc.remaining_capacity_mah;
      int minutes = (to_full * 60) / current.current_ma;
      ESP_LOGI(s_tag, "Est. time to full: %d:%02d", minutes / 60, minutes % 60);
    }
  }

  /* --- Read Battery Status --- */
  ESP_LOGI(s_tag, "\n--- Battery Status Flags ---");

  bq7850_status_t status;
  ret = star_bms_bq7850_read_status(&s_bms, &status);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Battery status: 0x%04X", status.battery_status);
    ESP_LOGI(s_tag, "Safety status: 0x%04X", status.safety_status);
    ESP_LOGI(s_tag, "Operation status: 0x%04X", status.operation_status);

    /* Decode battery status flags */
    if (status.battery_status & BQ7850_BATTERY_STATUS_DSG) {
      ESP_LOGI(s_tag, "  [DSG] Discharging");
    }
    if (status.battery_status & BQ7850_BATTERY_STATUS_FC) {
      ESP_LOGI(s_tag, "  [FC] Fully charged");
    }
    if (status.battery_status & BQ7850_BATTERY_STATUS_FD) {
      ESP_LOGI(s_tag, "  [FD] Fully discharged");
    }
    if (status.battery_status & BQ7850_BATTERY_STATUS_INIT) {
      ESP_LOGI(s_tag, "  [INIT] Initialized");
    }

    /* Decode safety status flags */
    if (status.safety_status & BQ7850_SAFETY_STATUS_OCC) {
      ESP_LOGW(s_tag, "  [OCC] Overcurrent in charge!");
    }
    if (status.safety_status & BQ7850_SAFETY_STATUS_OCD) {
      ESP_LOGW(s_tag, "  [OCD] Overcurrent in discharge!");
    }
    if (status.safety_status & BQ7850_SAFETY_STATUS_COV) {
      ESP_LOGW(s_tag, "  [COV] Cell overvoltage!");
    }
    if (status.safety_status & BQ7850_SAFETY_STATUS_CUV) {
      ESP_LOGW(s_tag, "  [CUV] Cell undervoltage!");
    }
    if (status.safety_status & BQ7850_SAFETY_STATUS_OTC) {
      ESP_LOGW(s_tag, "  [OTC] Over-temperature in charge!");
    }
    if (status.safety_status & BQ7850_SAFETY_STATUS_OTD) {
      ESP_LOGW(s_tag, "  [OTD] Over-temperature in discharge!");
    }

    /* Status string */
    char status_str[128];
    star_bms_bq7850_status_to_string(&status, status_str, sizeof(status_str));
    ESP_LOGI(s_tag, "Status summary: %s", status_str);
  }

  /* --- Read Complete Battery State --- */
  ESP_LOGI(s_tag, "\n--- Complete Battery State ---");

  bq7850_battery_state_t state;
  ret = star_bms_bq7850_read_battery_state(&s_bms, &state);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Voltage: %u mV", state.cells.pack_mv);
    ESP_LOGI(s_tag, "Current: %d mA", state.current.current_ma);
    ESP_LOGI(s_tag, "Temperature: %.1f C",
             star_bms_bq7850_convert_temp_to_celsius(state.temps.avg_temp_c));
    ESP_LOGI(s_tag, "SOC: %d%%", state.soc.relative_soc);
    ESP_LOGI(s_tag, "Remaining: %d mAh", state.soc.remaining_capacity_mah);
  }

  /* --- Check for Faults --- */
  ESP_LOGI(s_tag, "\n--- Fault Check ---");

  bool fault_active = star_bms_bq7850_is_fault_active(&state.status);
  if (fault_active) {
    ESP_LOGW(s_tag, "FAULT ACTIVE - Check safety status!");
  } else {
    ESP_LOGI(s_tag, "No faults active");
  }

  /* --- Continuous Monitoring --- */
  ESP_LOGI(s_tag, "\n--- Continuous Monitoring (5 samples) ---");

  for (int i = 0; i < 5; i++) {
    ret = star_bms_bq7850_read_battery_state(&s_bms, &state);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "[%d] %.2fV %dmA %d%% %.1fC",
               i + 1,
               state.cells.pack_mv / 1000.0f,
               state.current.current_ma,
               state.soc.relative_soc,
               star_bms_bq7850_convert_temp_to_celsius(state.temps.avg_temp_c));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  /* Cleanup */
  star_bms_bq7850_deinit(&s_bms);
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nBMS monitoring example complete");
}
