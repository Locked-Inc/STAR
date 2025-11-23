/**
 * @file 105_bms_temperature.c
 * @brief Temperature monitoring and thermal management
 *
 * Demonstrates:
 * - Reading all temperature sensors
 * - Configuring temperature alerts
 * - Monitoring thermal trends
 * - Temperature-based decisions
 * - Thermal protection strategies
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_TEMPERATURE";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Temperature thresholds */
#define TEMP_CHARGE_MAX    (45.0f)  /* C */
#define TEMP_CHARGE_MIN    (0.0f)   /* C */
#define TEMP_DISCHARGE_MAX (60.0f)  /* C */
#define TEMP_DISCHARGE_MIN (-20.0f) /* C */
#define TEMP_WARNING       (50.0f)  /* C */

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/* Thermal history for trend analysis */
#define TEMP_HISTORY_SIZE (10)
static float temp_history[TEMP_HISTORY_SIZE];
static int temp_history_idx = 0;

/**
 * @brief Add temperature to history buffer
 */
static void add_temp_to_history(float temp_c)
{
  temp_history[temp_history_idx] = temp_c;
  temp_history_idx = (temp_history_idx + 1) % TEMP_HISTORY_SIZE;
}

/**
 * @brief Calculate temperature trend
 */
static float calculate_temp_trend(void)
{
  if (temp_history_idx < 2) {
    return 0.0f;  /* Not enough data */
  }

  int oldest_idx = temp_history_idx;
  int newest_idx = (temp_history_idx - 1 + TEMP_HISTORY_SIZE) % TEMP_HISTORY_SIZE;

  float delta = temp_history[newest_idx] - temp_history[oldest_idx];
  return delta / TEMP_HISTORY_SIZE;  /* Average change per sample */
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Temperature Monitoring ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "temp_demo", NULL, NULL);
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

  /* ===== READ ALL TEMPERATURE SENSORS ===== */
  ESP_LOGI(TAG, "===== Temperature Sensor Readings =====");

  bq7850_temp_data_t temps;
  ret = star_bms_bq7850_read_temperatures(&g_bms, &temps);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Active sensors: %d", temps.valid_sensors);

    for (int i = 0; i < temps.valid_sensors; i++) {
      float temp_c = star_bms_bq7850_convert_temp_to_celsius(temps.temp_c[i]);
      ESP_LOGI(TAG, "  Sensor %d: %.1f C (raw: %d)",
               i + 1, temp_c, temps.temp_c[i]);

      /* Determine sensor location (typical placement) */
      const char* location = "Unknown";
      if (i == 0) location = "Pack center";
      else if (i == 1) location = "FET/PCB";
      else if (i == 2) location = "External";

      ESP_LOGI(TAG, "    Location: %s", location);
    }

    /* Calculate min/max */
    int16_t min_t = 32767, max_t = -32768;
    for (int i = 0; i < temps.valid_sensors; i++) {
      if (temps.temp_c[i] < min_t) min_t = temps.temp_c[i];
      if (temps.temp_c[i] > max_t) max_t = temps.temp_c[i];
    }
    float min_c = star_bms_bq7850_convert_temp_to_celsius(min_t);
    float max_c = star_bms_bq7850_convert_temp_to_celsius(max_t);

    ESP_LOGI(TAG, "\nTemperature range:");
    ESP_LOGI(TAG, "  Minimum: %.1f C", min_c);
    ESP_LOGI(TAG, "  Maximum: %.1f C", max_c);
    ESP_LOGI(TAG, "  Delta: %.1f C", max_c - min_c);

    if (max_c - min_c > 10.0f) {
      ESP_LOGW(TAG, "  Large temperature gradient detected!");
    }
  } else {
    ESP_LOGE(TAG, "Failed to read temperatures: %s", esp_err_to_name(ret));
  }

  /* ===== PACK TEMPERATURE ===== */
  ESP_LOGI(TAG, "\n===== Pack Temperature =====");

  int16_t pack_temp;
  ret = star_bms_bq7850_read_temperature(&g_bms, &pack_temp);
  if (ret == ESP_OK) {
    float pack_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);
    ESP_LOGI(TAG, "Pack temperature: %.1f C", pack_c);

    /* Temperature classification */
    if (pack_c < -20.0f) {
      ESP_LOGW(TAG, "  Status: VERY COLD - Limited performance");
    } else if (pack_c < 0.0f) {
      ESP_LOGW(TAG, "  Status: COLD - No charging recommended");
    } else if (pack_c < 10.0f) {
      ESP_LOGI(TAG, "  Status: COOL - Reduced charge current recommended");
    } else if (pack_c < 45.0f) {
      ESP_LOGI(TAG, "  Status: NORMAL - Optimal operating range");
    } else if (pack_c < 60.0f) {
      ESP_LOGW(TAG, "  Status: WARM - Monitor closely");
    } else {
      ESP_LOGE(TAG, "  Status: HOT - Reduce current or shutdown");
    }
  }

  /* ===== TEMPERATURE-BASED CHARGING LIMITS ===== */
  ESP_LOGI(TAG, "\n===== Temperature-Based Charging Limits =====");

  if (ret == ESP_OK) {
    float pack_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);

    ESP_LOGI(TAG, "Charge limits based on temperature:");
    bool can_charge = true;

    if (pack_c < TEMP_CHARGE_MIN) {
      ESP_LOGW(TAG, "  Temperature too low for charging (%.1f C < %.1f C)",
               pack_c, TEMP_CHARGE_MIN);
      ESP_LOGW(TAG, "  Recommended: Disable charge FET");
      can_charge = false;
    } else if (pack_c > TEMP_CHARGE_MAX) {
      ESP_LOGW(TAG, "  Temperature too high for charging (%.1f C > %.1f C)",
               pack_c, TEMP_CHARGE_MAX);
      ESP_LOGW(TAG, "  Recommended: Disable charge FET or reduce current");
      can_charge = false;
    } else {
      ESP_LOGI(TAG, "  Temperature OK for charging (%.1f - %.1f C)",
               TEMP_CHARGE_MIN, TEMP_CHARGE_MAX);

      /* Calculate charge current derate */
      float charge_factor = 1.0f;
      if (pack_c < 10.0f) {
        charge_factor = 0.5f;  /* 50% current below 10C */
      } else if (pack_c > 40.0f) {
        charge_factor = 0.7f;  /* 70% current above 40C */
      }

      ESP_LOGI(TAG, "  Charge current factor: %.1f%%", charge_factor * 100.0f);
    }

    if (can_charge) {
      ESP_LOGI(TAG, "  Charging: ALLOWED");
    } else {
      ESP_LOGW(TAG, "  Charging: BLOCKED");
    }
  }

  /* ===== TEMPERATURE-BASED DISCHARGE LIMITS ===== */
  ESP_LOGI(TAG, "\n===== Temperature-Based Discharge Limits =====");

  if (ret == ESP_OK) {
    float pack_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);

    ESP_LOGI(TAG, "Discharge limits based on temperature:");
    bool can_discharge = true;

    if (pack_c < TEMP_DISCHARGE_MIN) {
      ESP_LOGW(TAG, "  Temperature too low for discharge (%.1f C < %.1f C)",
               pack_c, TEMP_DISCHARGE_MIN);
      ESP_LOGW(TAG, "  Recommended: Disable discharge FET or reduce current");
      can_discharge = false;
    } else if (pack_c > TEMP_DISCHARGE_MAX) {
      ESP_LOGW(TAG, "  Temperature too high for discharge (%.1f C > %.1f C)",
               pack_c, TEMP_DISCHARGE_MAX);
      ESP_LOGW(TAG, "  Recommended: Disable discharge FET");
      can_discharge = false;
    } else {
      ESP_LOGI(TAG, "  Temperature OK for discharge (%.1f - %.1f C)",
               TEMP_DISCHARGE_MIN, TEMP_DISCHARGE_MAX);
    }

    if (can_discharge) {
      ESP_LOGI(TAG, "  Discharging: ALLOWED");
    } else {
      ESP_LOGW(TAG, "  Discharging: BLOCKED");
    }
  }

  /* ===== THERMAL TREND MONITORING ===== */
  ESP_LOGI(TAG, "\n===== Thermal Trend Monitoring =====");
  ESP_LOGI(TAG, "Monitoring temperature changes over time...");

  for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
    ret = star_bms_bq7850_read_temperature(&g_bms, &pack_temp);
    if (ret == ESP_OK) {
      float pack_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);
      add_temp_to_history(pack_c);

      float trend = calculate_temp_trend();
      ESP_LOGI(TAG, "  [%d] %.1f C (trend: %+.2f C/sample)",
               i + 1, pack_c, trend);

      if (trend > 0.5f) {
        ESP_LOGW(TAG, "       Temperature rising rapidly!");
      } else if (trend < -0.5f) {
        ESP_LOGI(TAG, "       Temperature cooling");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  /* ===== THERMAL PROTECTION STRATEGY ===== */
  ESP_LOGI(TAG, "\n===== Thermal Protection Strategy =====");

  ret = star_bms_bq7850_read_temperature(&g_bms, &pack_temp);
  if (ret == ESP_OK) {
    float pack_c = star_bms_bq7850_convert_temp_to_celsius(pack_temp);
    float trend = calculate_temp_trend();

    ESP_LOGI(TAG, "Current: %.1f C, Trend: %+.2f C/sample", pack_c, trend);

    /* Implement thermal protection logic */
    if (pack_c > TEMP_WARNING || trend > 1.0f) {
      ESP_LOGW(TAG, "THERMAL PROTECTION ACTIVATED");

      bq7850_current_data_t current;
      if (star_bms_bq7850_read_current(&g_bms, &current) == ESP_OK) {
        if (current.current_ma > 0) {
          ESP_LOGW(TAG, "  Action: Reduce charge current");
        } else if (current.current_ma < 0) {
          ESP_LOGW(TAG, "  Action: Reduce discharge current");
        }
      }

      if (pack_c > TEMP_DISCHARGE_MAX) {
        ESP_LOGE(TAG, "  CRITICAL: Emergency shutdown recommended");
      }
    } else {
      ESP_LOGI(TAG, "Thermal conditions normal");
    }
  }

  /* ===== TEMPERATURE SENSOR DIAGNOSTICS ===== */
  ESP_LOGI(TAG, "\n===== Temperature Sensor Diagnostics =====");

  ret = star_bms_bq7850_read_temperatures(&g_bms, &temps);
  if (ret == ESP_OK) {
    bool sensor_ok = true;

    for (int i = 0; i < temps.valid_sensors; i++) {
      float temp_c = star_bms_bq7850_convert_temp_to_celsius(temps.temp_c[i]);

      /* Check for unrealistic readings */
      if (temp_c < -40.0f || temp_c > 100.0f) {
        ESP_LOGW(TAG, "  Sensor %d: Reading out of range (%.1f C)", i + 1, temp_c);
        sensor_ok = false;
      }

      /* Check for sensor disconnection (reading exactly 0K or -273.15C) */
      if (temps.temp_c[i] == 0) {
        ESP_LOGW(TAG, "  Sensor %d: May be disconnected", i + 1);
        sensor_ok = false;
      }
    }

    if (sensor_ok) {
      ESP_LOGI(TAG, "All temperature sensors functioning normally");
    } else {
      ESP_LOGW(TAG, "Temperature sensor issues detected - check connections");
    }
  }

  /* ===== THERMAL MANAGEMENT BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Thermal Management Best Practices =====");
  ESP_LOGI(TAG, "1. Monitor temperature during high-current operation");
  ESP_LOGI(TAG, "2. Derate charging/discharging at temperature extremes");
  ESP_LOGI(TAG, "3. Track temperature trends to predict thermal runaway");
  ESP_LOGI(TAG, "4. Place sensors at hottest locations (cells, FETs)");
  ESP_LOGI(TAG, "5. Large temperature gradients indicate poor thermal design");
  ESP_LOGI(TAG, "6. Never charge Li-Ion batteries below 0C");
  ESP_LOGI(TAG, "7. Implement hysteresis to prevent oscillation");
  ESP_LOGI(TAG, "8. Log thermal events for post-analysis");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nTemperature monitoring example complete");
}
