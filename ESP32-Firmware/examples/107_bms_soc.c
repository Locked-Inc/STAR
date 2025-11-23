/**
 * @file 107_bms_soc.c
 * @brief State of Charge algorithms and monitoring
 *
 * Demonstrates:
 * - Reading SOC percentage
 * - Monitoring SOC changes
 * - Calculating remaining capacity
 * - SOC-based decisions
 * - SOC accuracy considerations
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_SOC";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/**
 * @brief Display SOC gauge
 */
static void display_soc_gauge(uint8_t soc_percent)
{
  const int bar_length = 20;
  int filled = (soc_percent * bar_length) / 100;

  ESP_LOGI(TAG, "  SOC Gauge: [");
  char gauge[bar_length + 1];
  for (int i = 0; i < bar_length; i++) {
    gauge[i] = (i < filled) ? '#' : '-';
  }
  gauge[bar_length] = '\0';
  ESP_LOGI(TAG, "  %s] %d%%", gauge, soc_percent);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 State of Charge Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "soc_demo", NULL, NULL);
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

  /* ===== READ STATE OF CHARGE ===== */
  ESP_LOGI(TAG, "===== State of Charge Information =====");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "SOC Data:");
    ESP_LOGI(TAG, "  Relative SOC:       %d%%", soc.relative_soc);
    ESP_LOGI(TAG, "  Absolute SOC:       %d%%", soc.absolute_soc);
    ESP_LOGI(TAG, "  Remaining capacity: %d mAh", soc.remaining_capacity_mah);
    ESP_LOGI(TAG, "  Full capacity:      %d mAh", soc.full_capacity_mah);
    ESP_LOGI(TAG, "  Cycle count:        %d", soc.cycle_count);

    /* Visual gauge */
    display_soc_gauge(soc.relative_soc);

    /* Calculate used capacity */
    int used_mah = soc.full_capacity_mah - soc.remaining_capacity_mah;
    ESP_LOGI(TAG, "\nCapacity breakdown:");
    ESP_LOGI(TAG, "  Used:      %d mAh (%d%%)", used_mah, 100 - soc.relative_soc);
    ESP_LOGI(TAG, "  Remaining: %d mAh (%d%%)", soc.remaining_capacity_mah, soc.relative_soc);

    /* SOC difference explanation */
    int soc_diff = soc.relative_soc - soc.absolute_soc;
    ESP_LOGI(TAG, "\nSOC Types:");
    ESP_LOGI(TAG, "  Relative SOC: Based on current full capacity");
    ESP_LOGI(TAG, "  Absolute SOC: Based on design capacity");
    ESP_LOGI(TAG, "  Difference: %d%% (battery aging indicator)", soc_diff);
  } else {
    ESP_LOGE(TAG, "Failed to read SOC: %s", esp_err_to_name(ret));
  }

  /* ===== SOC-BASED DECISIONS ===== */
  ESP_LOGI(TAG, "\n===== SOC-Based Decisions =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Battery status based on SOC:");

    if (soc.relative_soc >= 95) {
      ESP_LOGI(TAG, "  Status: FULL (>= 95%%)");
      ESP_LOGI(TAG, "  Actions: Terminate charging, enable balancing");
    } else if (soc.relative_soc >= 80) {
      ESP_LOGI(TAG, "  Status: HIGH (80-94%%)");
      ESP_LOGI(TAG, "  Actions: Normal operation");
    } else if (soc.relative_soc >= 20) {
      ESP_LOGI(TAG, "  Status: NORMAL (20-79%%)");
      ESP_LOGI(TAG, "  Actions: Normal operation");
    } else if (soc.relative_soc >= 10) {
      ESP_LOGW(TAG, "  Status: LOW (10-19%%)");
      ESP_LOGW(TAG, "  Actions: Low battery warning");
    } else if (soc.relative_soc >= 5) {
      ESP_LOGW(TAG, "  Status: CRITICAL (5-9%%)");
      ESP_LOGW(TAG, "  Actions: Critical battery warning, reduce power");
    } else {
      ESP_LOGE(TAG, "  Status: EMPTY (< 5%%)");
      ESP_LOGE(TAG, "  Actions: Immediate shutdown recommended");
    }
  }

  /* ===== TIME REMAINING ESTIMATION ===== */
  ESP_LOGI(TAG, "\n===== Time Remaining Estimation =====");

  bq7850_current_data_t current;
  ret = star_bms_bq7850_read_current(&g_bms, &current);
  if (ret == ESP_OK && soc.remaining_capacity_mah > 0) {
    ESP_LOGI(TAG, "Current: %d mA", current.current_ma);
    ESP_LOGI(TAG, "Remaining capacity: %d mAh", soc.remaining_capacity_mah);

    if (current.current_ma < -100) {  /* Discharging */
      float hours = (float)soc.remaining_capacity_mah / (-current.current_ma);
      int total_minutes = (int)(hours * 60);
      int hours_part = total_minutes / 60;
      int minutes_part = total_minutes % 60;

      ESP_LOGI(TAG, "Estimated runtime: %d hours %d minutes", hours_part, minutes_part);
      ESP_LOGI(TAG, "Note: Assumes constant discharge rate");
    } else if (current.current_ma > 100) {  /* Charging */
      int needed_mah = soc.full_capacity_mah - soc.remaining_capacity_mah;
      float hours = (float)needed_mah / current.current_ma;
      int total_minutes = (int)(hours * 60);
      int hours_part = total_minutes / 60;
      int minutes_part = total_minutes % 60;

      ESP_LOGI(TAG, "Estimated charge time: %d hours %d minutes", hours_part, minutes_part);
      ESP_LOGI(TAG, "Note: Assumes constant charge rate");
    } else {
      ESP_LOGI(TAG, "Battery idle - no time estimate available");
    }
  }

  /* ===== SOC MONITORING LOOP ===== */
  ESP_LOGI(TAG, "\n===== SOC Monitoring (10 samples) =====");

  uint8_t last_soc = 0;
  for (int i = 0; i < 10; i++) {
    ret = star_bms_bq7850_read_soc(&g_bms, &soc);
    if (ret == ESP_OK) {
      char change_indicator = ' ';
      if (i > 0) {
        if (soc.relative_soc > last_soc) change_indicator = '+';
        else if (soc.relative_soc < last_soc) change_indicator = '-';
      }

      ESP_LOGI(TAG, "  [%2d] SOC: %3d%% (%4d mAh) [%c]",
               i + 1,
               soc.relative_soc,
               soc.remaining_capacity_mah,
               change_indicator);

      last_soc = soc.relative_soc;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  /* ===== BATTERY HEALTH FROM SOC ===== */
  ESP_LOGI(TAG, "\n===== Battery Health Assessment =====");

  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Capacity comparison:");
    ESP_LOGI(TAG, "  Full capacity:   %d mAh", soc.full_capacity_mah);
    ESP_LOGI(TAG, "  Note: Design capacity requires config data (typically 5000 mAh)");

    ESP_LOGI(TAG, "\nCycle information:");
    ESP_LOGI(TAG, "  Cycle count: %d", soc.cycle_count);
  }

  /* ===== SOC ACCURACY FACTORS ===== */
  ESP_LOGI(TAG, "\n===== SOC Accuracy Factors =====");

  ESP_LOGI(TAG, "Factors affecting SOC accuracy:");
  ESP_LOGI(TAG, "1. Temperature (affects voltage and capacity)");
  ESP_LOGI(TAG, "2. Age of battery (capacity fade)");
  ESP_LOGI(TAG, "3. Discharge rate (Peukert effect)");
  ESP_LOGI(TAG, "4. Calibration state (needs periodic full cycle)");
  ESP_LOGI(TAG, "5. Relaxation time (voltage recovery after load)");

  /* Check temperature effect */
  bq7850_temp_data_t temps;
  if (star_bms_bq7850_read_temperatures(&g_bms, &temps) == ESP_OK) {
    float temp_c = star_bms_bq7850_convert_temp_to_celsius(temps.avg_temp_c);
    ESP_LOGI(TAG, "\nCurrent temperature: %.1f C", temp_c);

    if (temp_c < 0.0f) {
      ESP_LOGW(TAG, "  Cold temperature reduces available capacity");
      ESP_LOGW(TAG, "  SOC may appear lower than actual chemical charge");
    } else if (temp_c > 40.0f) {
      ESP_LOGW(TAG, "  High temperature affects SOC accuracy");
    } else {
      ESP_LOGI(TAG, "  Temperature is optimal for SOC accuracy");
    }
  }

  /* ===== SOC BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== SOC Monitoring Best Practices =====");
  ESP_LOGI(TAG, "1. Use relative SOC for user display (accounts for aging)");
  ESP_LOGI(TAG, "2. Use absolute SOC for capacity planning");
  ESP_LOGI(TAG, "3. Perform full charge/discharge cycle monthly for calibration");
  ESP_LOGI(TAG, "4. Don't rely on SOC for critical safety decisions");
  ESP_LOGI(TAG, "5. Consider temperature when interpreting SOC");
  ESP_LOGI(TAG, "6. Track SOC trends, not just absolute values");
  ESP_LOGI(TAG, "7. Implement SOC-based charging termination");
  ESP_LOGI(TAG, "8. Log SOC with battery events for analysis");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nState of charge example complete");
}
