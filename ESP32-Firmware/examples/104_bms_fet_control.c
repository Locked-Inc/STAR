/**
 * @file 104_bms_fet_control.c
 * @brief FET control patterns for charge/discharge management
 *
 * Demonstrates:
 * - Charge/discharge FET control
 * - Pre-charge FET management
 * - Emergency FET shutdown
 * - FET control patterns for different scenarios
 * - Safe FET sequencing
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_FET_CONTROL";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/**
 * @brief Display current FET states
 */
static void display_fet_states(uint8_t control)
{
  ESP_LOGI(TAG, "FET States:");
  ESP_LOGI(TAG, "  CHG  (Charge):     %s", (control & BQ7850_FET_CONTROL_CHG_FET) ? "ON" : "OFF");
  ESP_LOGI(TAG, "  DSG  (Discharge):  %s", (control & BQ7850_FET_CONTROL_DSG_FET) ? "ON" : "OFF");
  ESP_LOGI(TAG, "  PCHG (Pre-charge): %s", (control & BQ7850_FET_CONTROL_PCHG_FET) ? "ON" : "OFF");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 FET Control Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "fet_control_demo", NULL, NULL);
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

  /* ===== SCENARIO 1: NORMAL OPERATION ===== */
  ESP_LOGI(TAG, "===== Scenario 1: Normal Operation =====");
  ESP_LOGI(TAG, "Both charge and discharge FETs enabled");

  ret = star_bms_bq7850_control_fets(&g_bms, true, true);  /* charge_fet=true, discharge_fet=true */
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "FETs configured for normal operation");
    ESP_LOGI(TAG, "  CHG FET: ON");
    ESP_LOGI(TAG, "  DSG FET: ON");
  } else {
    ESP_LOGW(TAG, "Failed to control FETs: %s", esp_err_to_name(ret));
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

  /* ===== SCENARIO 2: STORAGE MODE ===== */
  ESP_LOGI(TAG, "\n===== Scenario 2: Storage Mode =====");
  ESP_LOGI(TAG, "Charge enabled, discharge disabled");
  ESP_LOGI(TAG, "Use case: Long-term storage with maintenance charging");

  ret = star_bms_bq7850_control_fets(&g_bms, true, false);  /* charge_fet=true, discharge_fet=false */
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "FETs configured for storage mode");
    ESP_LOGI(TAG, "  CHG FET: ON");
    ESP_LOGI(TAG, "  DSG FET: OFF");
    ESP_LOGI(TAG, "Battery can charge but not discharge");
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

  /* ===== SCENARIO 3: DISCHARGE ONLY MODE ===== */
  ESP_LOGI(TAG, "\n===== Scenario 3: Discharge Only Mode =====");
  ESP_LOGI(TAG, "Charge disabled, discharge enabled");
  ESP_LOGI(TAG, "Use case: UPS mode, prevent charging from faulty charger");

  ret = star_bms_bq7850_control_fets(&g_bms, false, true);  /* charge_fet=false, discharge_fet=true */
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "FETs configured for discharge-only mode");
    ESP_LOGI(TAG, "  CHG FET: OFF");
    ESP_LOGI(TAG, "  DSG FET: ON");
    ESP_LOGI(TAG, "Battery can discharge but not charge");
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

  /* ===== SCENARIO 4: EMERGENCY SHUTDOWN ===== */
  ESP_LOGI(TAG, "\n===== SCENARIO 4: Emergency Shutdown =====");
  ESP_LOGI(TAG, "All FETs disabled");
  ESP_LOGI(TAG, "Use case: Fault condition, safety shutdown");

  ret = star_bms_bq7850_control_fets(&g_bms, false, false);  /* charge_fet=false, discharge_fet=false */
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "EMERGENCY SHUTDOWN - All FETs disabled");
    ESP_LOGI(TAG, "  CHG FET: OFF");
    ESP_LOGI(TAG, "  DSG FET: OFF");
    ESP_LOGI(TAG, "Battery electrically disconnected");
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

  /* ===== SCENARIO 5: PRE-CHARGE SEQUENCE ===== */
  ESP_LOGI(TAG, "\n===== Scenario 5: Pre-Charge Sequence =====");
  ESP_LOGI(TAG, "Note: Pre-charge FET control not available in current API");
  ESP_LOGI(TAG, "Skipping pre-charge demo");

  vTaskDelay(pdMS_TO_TICKS(1000));

  /* ===== SCENARIO 6: FAULT RECOVERY ===== */
  ESP_LOGI(TAG, "\n===== Scenario 6: Fault Recovery =====");

  /* Check for faults */
  bq7850_status_t status;
  ret = star_bms_bq7850_read_status(&g_bms, &status);
  if (ret == ESP_OK) {
    bool fault_active = star_bms_bq7850_is_fault_active(&status);

    if (fault_active) {
      ESP_LOGW(TAG, "Fault detected - FETs should be disabled");
      ESP_LOGW(TAG, "Safety status: 0x%04X", status.safety_status);

      /* Emergency shutdown */
      star_bms_bq7850_control_fets(&g_bms, false, false);
      ESP_LOGW(TAG, "All FETs disabled for safety");

      /* Wait for fault to clear (in real system, check conditions) */
      ESP_LOGI(TAG, "Waiting for fault to clear...");
      vTaskDelay(pdMS_TO_TICKS(2000));

      /* Re-check */
      star_bms_bq7850_read_status(&g_bms, &status);
      fault_active = star_bms_bq7850_is_fault_active(&status);

      if (!fault_active) {
        ESP_LOGI(TAG, "Fault cleared - re-enabling FETs");
        star_bms_bq7850_control_fets(&g_bms, true, true);
        ESP_LOGI(TAG, "Normal operation restored");
      } else {
        ESP_LOGW(TAG, "Fault still active - FETs remain disabled");
      }
    } else {
      ESP_LOGI(TAG, "No faults detected - FETs can be enabled");
    }
  }

  /* ===== SCENARIO 7: SMART CHARGING CONTROL ===== */
  ESP_LOGI(TAG, "\n===== Scenario 7: Smart Charging Control =====");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current SOC: %d%%", soc.relative_soc);

    if (soc.relative_soc >= 100) {
      ESP_LOGI(TAG, "Battery full - disabling charge FET");
      star_bms_bq7850_control_fets(&g_bms, false, true);
      ESP_LOGI(TAG, "Charge terminated, discharge still available");
    } else if (soc.relative_soc <= 5) {
      ESP_LOGI(TAG, "Battery critically low - disabling discharge FET");
      star_bms_bq7850_control_fets(&g_bms, true, false);
      ESP_LOGI(TAG, "Discharge blocked, charge still available");
    } else {
      ESP_LOGI(TAG, "Battery SOC normal - both FETs enabled");
      star_bms_bq7850_control_fets(&g_bms, true, true);
    }
  }

  /* ===== SCENARIO 8: TEMPERATURE-BASED CONTROL ===== */
  ESP_LOGI(TAG, "\n===== Scenario 8: Temperature-Based Control =====");

  bq7850_temp_data_t temps;
  ret = star_bms_bq7850_read_temperatures(&g_bms, &temps);
  if (ret == ESP_OK) {
    /* Calculate max temp */
    int16_t max_temp = -32768;
    for (int i = 0; i < temps.valid_sensors; i++) {
      if (temps.temp_c[i] > max_temp) max_temp = temps.temp_c[i];
    }
    float max_temp_c = star_bms_bq7850_convert_temp_to_celsius(max_temp);
    ESP_LOGI(TAG, "Maximum temperature: %.1f C", max_temp_c);

    if (max_temp_c > 50.0f) {
      ESP_LOGW(TAG, "High temperature - disabling charge FET");
      star_bms_bq7850_control_fets(&g_bms, false, true);
      ESP_LOGW(TAG, "Charging disabled due to thermal protection");
    } else if (max_temp_c < 0.0f) {
      ESP_LOGW(TAG, "Low temperature - disabling charge FET");
      star_bms_bq7850_control_fets(&g_bms, false, true);
      ESP_LOGW(TAG, "Charging disabled (below freezing)");
    } else {
      ESP_LOGI(TAG, "Temperature normal - both FETs enabled");
      star_bms_bq7850_control_fets(&g_bms, true, true);
    }
  }

  /* ===== FET CONTROL BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== FET Control Best Practices =====");
  ESP_LOGI(TAG, "1. Always disable FETs on detection of protection faults");
  ESP_LOGI(TAG, "2. Use pre-charge sequence for high-capacitance loads");
  ESP_LOGI(TAG, "3. Implement hysteresis to prevent FET chattering");
  ESP_LOGI(TAG, "4. Monitor FET temperature (high current can cause heating)");
  ESP_LOGI(TAG, "5. Add delays between FET state changes (settling time)");
  ESP_LOGI(TAG, "6. Log all FET control events for diagnostics");
  ESP_LOGI(TAG, "7. Default to safe state (FETs off) on unknown conditions");
  ESP_LOGI(TAG, "8. Test emergency shutdown before deployment");

  /* Restore normal operation */
  ESP_LOGI(TAG, "\n===== Restoring Normal Operation =====");
  star_bms_bq7850_control_fets(&g_bms, true, true);
  ESP_LOGI(TAG, "  CHG FET: ON");
  ESP_LOGI(TAG, "  DSG FET: ON");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nFET control example complete");
}
