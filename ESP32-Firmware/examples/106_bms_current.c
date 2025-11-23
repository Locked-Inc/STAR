/**
 * @file 106_bms_current.c
 * @brief Current measurement and integration
 *
 * Demonstrates:
 * - Reading instantaneous current
 * - Monitoring current accumulation
 * - Calculating charge/discharge totals
 * - Current-based protection
 * - Power calculations
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

static const char* TAG = "BMS_CURRENT";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED  (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/* Current integration variables */
static int32_t accumulated_mah_charge = 0;
static int32_t accumulated_mah_discharge = 0;
static int64_t last_sample_time_us = 0;

/**
 * @brief Get current time in microseconds
 */
static int64_t get_time_us(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}

/**
 * @brief Integrate current over time
 */
static void integrate_current(int16_t current_ma)
{
  int64_t now_us = get_time_us();

  if (last_sample_time_us != 0) {
    int64_t delta_us = now_us - last_sample_time_us;
    float delta_hours = delta_us / 3600000000.0f;

    /* mAh = mA * hours */
    float delta_mah = current_ma * delta_hours;

    if (current_ma > 0) {
      accumulated_mah_charge += (int32_t)delta_mah;
    } else {
      accumulated_mah_discharge += (int32_t)(-delta_mah);
    }
  }

  last_sample_time_us = now_us;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Current Measurement Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "current_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus manager");
    return;
  }

  /* Create and configure I2C/SMBus bus first */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "bms_bus",
    I2C_NUM_0,
    BQ7850_DEFAULT_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  ret = star_bus_manager_add_bus(&g_manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add bus to manager");
    star_bus_manager_deinit(&g_manager);
    return;
  }

  ret = star_bus_config_init(i2c_config, &g_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus");
    star_bus_manager_deinit(&g_manager);
    return;
  }

  /* Initialize BMS */
  bq7850_config_t bms_config = {
    .num_cells = 16,
    .num_temp = 3,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage = 14800
  };

  ret = star_bms_bq7850_init(&g_bms, &g_manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init BMS: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&g_manager);
    return;
  }

  ESP_LOGI(TAG, "BMS initialized successfully\n");

  /* ===== READ INSTANTANEOUS CURRENT ===== */
  ESP_LOGI(TAG, "===== Instantaneous Current =====");

  bq7850_current_data_t current;
  ret = star_bms_bq7850_read_current(&g_bms, &current);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current measurements:");
    ESP_LOGI(TAG, "  Instantaneous: %d mA", current.current_ma);
    ESP_LOGI(TAG, "  Average:       %d mA", current.avg_current_ma);
    ESP_LOGI(TAG, "  Voltage:       %d mV", current.voltage_mv);
    ESP_LOGI(TAG, "  Power:         %ld mW", (long)current.power_mw);

    /* Current direction */
    if (current.current_ma > 100) {
      ESP_LOGI(TAG, "  Direction: CHARGING (+%d mA)", current.current_ma);
    } else if (current.current_ma < -100) {
      ESP_LOGI(TAG, "  Direction: DISCHARGING (%d mA)", current.current_ma);
    } else {
      ESP_LOGI(TAG, "  Direction: IDLE (%.1f mA)", current.current_ma / 1000.0f);
    }

    /* Power in watts */
    float power_w = current.power_mw / 1000.0f;
    ESP_LOGI(TAG, "  Power: %.2f W", power_w);

    /* Calculate actual battery resistance */
    if (current.current_ma != 0) {
      /* This is a simplified calculation */
      ESP_LOGI(TAG, "\nBattery characteristics:");
      ESP_LOGI(TAG, "  Voltage: %.3f V", current.voltage_mv / 1000.0f);
      ESP_LOGI(TAG, "  Current: %.3f A", current.current_ma / 1000.0f);
      ESP_LOGI(TAG, "  C-rate: (depends on capacity)");
    }
  }

  /* ===== CURRENT STATISTICS ===== */
  ESP_LOGI(TAG, "\n===== Current Statistics (10 samples) =====");

  int16_t max_current = 0;
  int16_t min_current = 0;
  int32_t sum_current = 0;
  int count = 10;

  for (int i = 0; i < count; i++) {
    ret = star_bms_bq7850_read_current(&g_bms, &current);
    if (ret == ESP_OK) {
      if (i == 0) {
        max_current = current.current_ma;
        min_current = current.current_ma;
      } else {
        if (current.current_ma > max_current) max_current = current.current_ma;
        if (current.current_ma < min_current) min_current = current.current_ma;
      }
      sum_current += current.current_ma;

      ESP_LOGI(TAG, "  [%2d] %6d mA", i + 1, current.current_ma);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  int16_t avg_current = sum_current / count;
  ESP_LOGI(TAG, "\nStatistics:");
  ESP_LOGI(TAG, "  Average: %d mA", avg_current);
  ESP_LOGI(TAG, "  Maximum: %d mA", max_current);
  ESP_LOGI(TAG, "  Minimum: %d mA", min_current);
  ESP_LOGI(TAG, "  Range:   %d mA", max_current - min_current);

  /* ===== CURRENT INTEGRATION ===== */
  ESP_LOGI(TAG, "\n===== Current Integration =====");
  ESP_LOGI(TAG, "Integrating current over 10 seconds...");

  accumulated_mah_charge = 0;
  accumulated_mah_discharge = 0;
  last_sample_time_us = 0;

  for (int i = 0; i < 100; i++) {  /* 100 samples over 10 seconds */
    ret = star_bms_bq7850_read_current(&g_bms, &current);
    if (ret == ESP_OK) {
      integrate_current(current.current_ma);

      if (i % 10 == 0) {  /* Report every second */
        ESP_LOGI(TAG, "  [%2d s] Current: %6d mA, Charge: %6ld mAh, Discharge: %6ld mAh",
                 i / 10,
                 current.current_ma,
                 (long)accumulated_mah_charge,
                 (long)accumulated_mah_discharge);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(TAG, "\nIntegration results:");
  ESP_LOGI(TAG, "  Total charge:    %ld mAh", (long)accumulated_mah_charge);
  ESP_LOGI(TAG, "  Total discharge: %ld mAh", (long)accumulated_mah_discharge);
  ESP_LOGI(TAG, "  Net change:      %ld mAh",
           (long)(accumulated_mah_charge - accumulated_mah_discharge));

  /* ===== CURRENT-BASED PROTECTION ===== */
  ESP_LOGI(TAG, "\n===== Current-Based Protection =====");

  bq7850_protection_t protection;
  ret = star_bms_bq7850_read_protection(&g_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Protection thresholds:");
    ESP_LOGI(TAG, "  Overcharge:     %d mA", protection.overcharge_ma);
    ESP_LOGI(TAG, "  Overdischarge:  %d mA", protection.overdischarge_ma);

    ret = star_bms_bq7850_read_current(&g_bms, &current);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "\nCurrent status:");
      ESP_LOGI(TAG, "  Actual current: %d mA", current.current_ma);

      bool overcharge = (current.current_ma > protection.overcharge_ma);
      bool overdischarge = (current.current_ma < -protection.overdischarge_ma);

      if (overcharge) {
        ESP_LOGW(TAG, "  WARNING: Overcharge current!");
        ESP_LOGW(TAG, "  Action: Reduce charge current or disable charge FET");
      } else if (overdischarge) {
        ESP_LOGW(TAG, "  WARNING: Overdischarge current!");
        ESP_LOGW(TAG, "  Action: Reduce discharge current or disable discharge FET");
      } else {
        ESP_LOGI(TAG, "  Status: Current within safe limits");
      }

      /* Calculate headroom */
      int charge_headroom = protection.overcharge_ma - current.current_ma;
      int discharge_headroom = protection.overdischarge_ma + current.current_ma;

      ESP_LOGI(TAG, "\nCurrent headroom:");
      ESP_LOGI(TAG, "  Charge:    %d mA available", charge_headroom > 0 ? charge_headroom : 0);
      ESP_LOGI(TAG, "  Discharge: %d mA available", discharge_headroom > 0 ? discharge_headroom : 0);
    }
  }

  /* ===== C-RATE CALCULATION ===== */
  ESP_LOGI(TAG, "\n===== C-Rate Calculation =====");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    ret = star_bms_bq7850_read_current(&g_bms, &current);
    if (ret == ESP_OK) {
      uint16_t capacity_mah = soc.full_capacity_mah;
      float c_rate = (float)abs(current.current_ma) / capacity_mah;

      ESP_LOGI(TAG, "Battery capacity: %d mAh", capacity_mah);
      ESP_LOGI(TAG, "Current: %d mA", current.current_ma);
      ESP_LOGI(TAG, "C-Rate: %.2fC", c_rate);

      /* Interpret C-rate */
      if (c_rate < 0.2f) {
        ESP_LOGI(TAG, "  Characterization: Trickle/maintenance");
      } else if (c_rate < 0.5f) {
        ESP_LOGI(TAG, "  Characterization: Low rate");
      } else if (c_rate < 1.0f) {
        ESP_LOGI(TAG, "  Characterization: Standard rate");
      } else if (c_rate < 2.0f) {
        ESP_LOGI(TAG, "  Characterization: Fast rate");
      } else {
        ESP_LOGW(TAG, "  Characterization: Very high rate - monitor closely");
      }

      /* Estimate time remaining/to-full */
      if (current.current_ma < -100) {  /* Discharging */
        float hours_remaining = (float)soc.remaining_capacity_mah / (-current.current_ma);
        int minutes = (int)(hours_remaining * 60);
        ESP_LOGI(TAG, "  Estimated runtime: %d:%02d", minutes / 60, minutes % 60);
      } else if (current.current_ma > 100) {  /* Charging */
        int needed_mah = soc.full_capacity_mah - soc.remaining_capacity_mah;
        float hours_to_full = (float)needed_mah / current.current_ma;
        int minutes = (int)(hours_to_full * 60);
        ESP_LOGI(TAG, "  Estimated charge time: %d:%02d", minutes / 60, minutes % 60);
      }
    }
  }

  /* ===== POWER EFFICIENCY ===== */
  ESP_LOGI(TAG, "\n===== Power Efficiency =====");

  ret = star_bms_bq7850_read_current(&g_bms, &current);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current power:  %ld mW", (long)current.power_mw);
    ESP_LOGI(TAG, "Note: Design power calculation requires config data");
  }

  /* ===== CURRENT MONITORING BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Current Monitoring Best Practices =====");
  ESP_LOGI(TAG, "1. Sample current at consistent intervals for accurate integration");
  ESP_LOGI(TAG, "2. Use average current for long-term trend analysis");
  ESP_LOGI(TAG, "3. Monitor instantaneous current for protection");
  ESP_LOGI(TAG, "4. Calculate C-rate to assess battery stress");
  ESP_LOGI(TAG, "5. Track charge/discharge totals for cycle counting");
  ESP_LOGI(TAG, "6. Calibrate current sense resistor for accuracy");
  ESP_LOGI(TAG, "7. Account for self-discharge in long-term calculations");
  ESP_LOGI(TAG, "8. Log peak currents for battery health analysis");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nCurrent measurement example complete");
}
