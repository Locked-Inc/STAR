/**
 * @file 117_bms_diagnostics.c
 * @brief Comprehensive fault diagnosis and troubleshooting
 *
 * Demonstrates:
 * - Reading fault flags
 * - Diagnosing common issues
 * - Providing troubleshooting steps
 * - System health check
 * - Diagnostic reporting
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <string.h>

static const char* TAG = "BMS_DIAGNOSTICS";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/* Diagnostic result structure */
typedef struct {
  bool communication_ok;
  bool voltages_ok;
  bool current_ok;
  bool temperature_ok;
  bool balance_ok;
  bool protection_ok;
  bool capacity_ok;
  int  error_count;
  int  warning_count;
} diagnostic_result_t;

static diagnostic_result_t diag_result = {0};

/**
 * @brief Print diagnostic summary
 */
static void print_diagnostic_summary(void)
{
  ESP_LOGI(TAG, "\n+------------------------------------------+");
  ESP_LOGI(TAG, "|     BMS Diagnostic Summary               |");
  ESP_LOGI(TAG, "+------------------------------------------+");
  ESP_LOGI(TAG, "| Communication:    %-20s |",
           diag_result.communication_ok ? "OK" : "FAILED");
  ESP_LOGI(TAG, "| Voltages:         %-20s |",
           diag_result.voltages_ok ? "OK" : "ISSUES FOUND");
  ESP_LOGI(TAG, "| Current:          %-20s |",
           diag_result.current_ok ? "OK" : "ISSUES FOUND");
  ESP_LOGI(TAG, "| Temperature:      %-20s |",
           diag_result.temperature_ok ? "OK" : "ISSUES FOUND");
  ESP_LOGI(TAG, "| Cell Balance:     %-20s |",
           diag_result.balance_ok ? "OK" : "ISSUES FOUND");
  ESP_LOGI(TAG, "| Protection:       %-20s |",
           diag_result.protection_ok ? "OK" : "ISSUES FOUND");
  ESP_LOGI(TAG, "| Capacity:         %-20s |",
           diag_result.capacity_ok ? "OK" : "ISSUES FOUND");
  ESP_LOGI(TAG, "+------------------------------------------+");
  ESP_LOGI(TAG, "| Errors:   %-28d |", diag_result.error_count);
  ESP_LOGI(TAG, "| Warnings: %-28d |", diag_result.warning_count);
  ESP_LOGI(TAG, "+------------------------------------------+");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Comprehensive Diagnostics ===\n");

  /* Initialize result structure */
  memset(&diag_result, 0, sizeof(diag_result));

  /* ===== TEST 1: COMMUNICATION ===== */
  ESP_LOGI(TAG, "===== Test 1: Communication Check =====");

  ret = star_bus_manager_init(&g_manager, "diag_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Bus manager initialization FAILED");
    diag_result.error_count++;
    goto cleanup;
  }
  ESP_LOGI(TAG, "OK Bus manager initialized");

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
    ESP_LOGE(TAG, "X Failed to add bus to manager");
    diag_result.error_count++;
    goto cleanup;
  }

  ret = star_bus_config_init(i2c_config, &g_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Failed to init bus");
    diag_result.error_count++;
    goto cleanup;
  }

  bq7850_config_t bms_config = {
    .num_cells = 16,
    .num_temp = 3,
    .smbus_addr = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage = 14800
  };

  ret = star_bms_bq7850_init(&g_bms, &g_manager, "bms_bus", &bms_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X BMS initialization FAILED: %s", esp_err_to_name(ret));
    ESP_LOGE(TAG, "  Possible causes:");
    ESP_LOGE(TAG, "    - I2C bus wiring issue");
    ESP_LOGE(TAG, "    - Wrong I2C address");
    ESP_LOGE(TAG, "    - BMS not powered");
    ESP_LOGE(TAG, "    - Pull-up resistors missing");
    diag_result.error_count++;
    goto cleanup;
  }
  ESP_LOGI(TAG, "OK BMS communication established");

  /* Try to read device info to verify communication */
  bq7850_device_info_t info;
  ret = star_bms_bq7850_get_device_info(&g_bms, &info);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "X Failed to read device info");
    diag_result.warning_count++;
  } else {
    ESP_LOGI(TAG, "OK Device info read successfully");
    ESP_LOGI(TAG, "  Device Type: 0x%04X", info.device_type);
    ESP_LOGI(TAG, "  Firmware: v%d.%d", (info.fw_version >> 8), (info.fw_version & 0xFF));
    diag_result.communication_ok = true;
  }

  /* ===== TEST 2: VOLTAGE MEASUREMENTS ===== */
  ESP_LOGI(TAG, "\n===== Test 2: Voltage Diagnostics =====");

  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&g_bms, &cells);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Failed to read cell voltages");
    diag_result.error_count++;
  } else {
    diag_result.voltages_ok = true;

    ESP_LOGI(TAG, "Cell voltage analysis:");
    uint16_t min_v = cells.cell_mv[0], max_v = cells.cell_mv[0];
    int min_cell = 0, max_cell = 0;

    for (int i = 0; i < cells.valid_cells; i++) {
      ESP_LOGI(TAG, "  Cell %2d: %d mV", i + 1, cells.cell_mv[i]);

      if (cells.cell_mv[i] < min_v) { min_v = cells.cell_mv[i]; min_cell = i; }
      if (cells.cell_mv[i] > max_v) { max_v = cells.cell_mv[i]; max_cell = i; }

      /* Check for abnormal voltages */
      if (cells.cell_mv[i] < 2000) {
        ESP_LOGE(TAG, "    X Cell %d critically low (< 2.0V)", i + 1);
        ESP_LOGE(TAG, "      May be damaged or disconnected");
        diag_result.voltages_ok = false;
        diag_result.error_count++;
      } else if (cells.cell_mv[i] < 2500) {
        ESP_LOGW(TAG, "    ! Cell %d very low (< 2.5V)", i + 1);
        ESP_LOGW(TAG, "      Charge immediately");
        diag_result.warning_count++;
      } else if (cells.cell_mv[i] > 4300) {
        ESP_LOGE(TAG, "    X Cell %d overvoltage (> 4.3V)", i + 1);
        ESP_LOGE(TAG, "      Safety concern - check charger");
        diag_result.voltages_ok = false;
        diag_result.error_count++;
      }
    }

    ESP_LOGI(TAG, "\nVoltage statistics:");
    ESP_LOGI(TAG, "  Min: %d mV (cell %d)", min_v, min_cell + 1);
    ESP_LOGI(TAG, "  Max: %d mV (cell %d)", max_v, max_cell + 1);
    ESP_LOGI(TAG, "  Delta: %d mV", max_v - min_v);

    if (diag_result.voltages_ok) {
      ESP_LOGI(TAG, "OK All cell voltages within normal range");
    }
  }

  /* ===== TEST 3: CELL BALANCE ===== */
  ESP_LOGI(TAG, "\n===== Test 3: Cell Balance Diagnostic =====");

  if (ret == ESP_OK) {
    uint16_t min_v = cells.cell_mv[0], max_v = cells.cell_mv[0];
    for (int i = 1; i < cells.valid_cells; i++) {
      if (cells.cell_mv[i] < min_v) min_v = cells.cell_mv[i];
      if (cells.cell_mv[i] > max_v) max_v = cells.cell_mv[i];
    }
    int delta = max_v - min_v;

    if (delta < 10) {
      ESP_LOGI(TAG, "OK Excellent balance (< 10 mV)");
      diag_result.balance_ok = true;
    } else if (delta < 50) {
      ESP_LOGI(TAG, "OK Good balance (< 50 mV)");
      diag_result.balance_ok = true;
    } else if (delta < 100) {
      ESP_LOGW(TAG, "! Fair balance (%d mV)", delta);
      ESP_LOGW(TAG, "  Recommend: Enable cell balancing");
      diag_result.balance_ok = true;
      diag_result.warning_count++;
    } else {
      ESP_LOGE(TAG, "X Poor balance (%d mV)", delta);
      ESP_LOGE(TAG, "  Action required: Cell balancing needed");
      ESP_LOGE(TAG, "  May indicate failing cell");
      diag_result.balance_ok = false;
      diag_result.error_count++;
    }
  }

  /* ===== TEST 4: CURRENT MEASUREMENT ===== */
  ESP_LOGI(TAG, "\n===== Test 4: Current Diagnostic =====");

  bq7850_current_data_t current;
  ret = star_bms_bq7850_read_current(&g_bms, &current);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Failed to read current");
    diag_result.error_count++;
  } else {
    ESP_LOGI(TAG, "Current: %d mA", current.current_ma);
    ESP_LOGI(TAG, "Average: %d mA", current.avg_current_ma);

    diag_result.current_ok = true;

    /* Check for reasonable values */
    if (abs(current.current_ma) > 50000) {
      ESP_LOGW(TAG, "! Very high current reading - verify");
      diag_result.warning_count++;
    }

    if (diag_result.current_ok) {
      ESP_LOGI(TAG, "OK Current measurements appear valid");
    }
  }

  /* ===== TEST 5: TEMPERATURE ===== */
  ESP_LOGI(TAG, "\n===== Test 5: Temperature Diagnostic =====");

  bq7850_temp_data_t temps;
  ret = star_bms_bq7850_read_temperatures(&g_bms, &temps);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Failed to read temperatures");
    diag_result.error_count++;
  } else {
    diag_result.temperature_ok = true;

    for (int i = 0; i < temps.valid_sensors; i++) {
      float temp_c = temps.temp_c[i];
      ESP_LOGI(TAG, "  Sensor %d: %.1f C", i + 1, temp_c);

      if (temp_c < -40.0f || temp_c > 100.0f) {
        ESP_LOGE(TAG, "    X Temperature out of realistic range");
        ESP_LOGE(TAG, "      Sensor may be disconnected or faulty");
        diag_result.temperature_ok = false;
        diag_result.error_count++;
      } else if (temp_c > 60.0f) {
        ESP_LOGW(TAG, "    ! High temperature (> 60C)");
        diag_result.warning_count++;
      } else if (temp_c < 0.0f) {
        ESP_LOGW(TAG, "    ! Below freezing - no charging allowed");
        diag_result.warning_count++;
      }
    }

    if (diag_result.temperature_ok) {
      ESP_LOGI(TAG, "OK Temperature sensors functioning normally");
    }
  }

  /* ===== TEST 6: PROTECTION STATUS ===== */
  ESP_LOGI(TAG, "\n===== Test 6: Protection Status =====");

  bq7850_status_t status;
  ret = star_bms_bq7850_read_status(&g_bms, &status);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Failed to read protection status");
    diag_result.error_count++;
  } else {
    ESP_LOGI(TAG, "Safety Status: 0x%04X", status.safety_status);

    if (status.safety_status == 0) {
      ESP_LOGI(TAG, "OK No protection faults");
      diag_result.protection_ok = true;
    } else {
      ESP_LOGE(TAG, "X Protection faults detected:");
      diag_result.protection_ok = false;

      if (status.safety_status & BQ7850_SAFETY_STATUS_COV) {
        ESP_LOGE(TAG, "  - Cell Overvoltage");
        diag_result.error_count++;
      }
      if (status.safety_status & BQ7850_SAFETY_STATUS_CUV) {
        ESP_LOGE(TAG, "  - Cell Undervoltage");
        diag_result.error_count++;
      }
      if (status.safety_status & BQ7850_SAFETY_STATUS_OCC) {
        ESP_LOGE(TAG, "  - Overcurrent Charge");
        diag_result.error_count++;
      }
      if (status.safety_status & BQ7850_SAFETY_STATUS_OCD) {
        ESP_LOGE(TAG, "  - Overcurrent Discharge");
        diag_result.error_count++;
      }
      if (status.safety_status & BQ7850_SAFETY_STATUS_OTC) {
        ESP_LOGE(TAG, "  - Over-Temperature Charge");
        diag_result.error_count++;
      }
      if (status.safety_status & BQ7850_SAFETY_STATUS_OTD) {
        ESP_LOGE(TAG, "  - Over-Temperature Discharge");
        diag_result.error_count++;
      }
    }
  }

  /* ===== TEST 7: CAPACITY & SOC ===== */
  ESP_LOGI(TAG, "\n===== Test 7: Capacity Diagnostic =====");

  bq7850_soc_data_t soc;
  ret = star_bms_bq7850_read_soc(&g_bms, &soc);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "X Failed to read SOC data");
    diag_result.error_count++;
  } else {
    ESP_LOGI(TAG, "SOC: %d%%", soc.relative_soc);
    ESP_LOGI(TAG, "Full capacity: %d mAh", soc.full_capacity_mah);
    ESP_LOGI(TAG, "Remaining: %d mAh", soc.remaining_capacity_mah);
    ESP_LOGI(TAG, "Cycles: %d", soc.cycle_count);

    if (bms_config.design_capacity > 0) {
      float health = ((float)soc.full_capacity_mah / bms_config.design_capacity) * 100.0f;
      ESP_LOGI(TAG, "Battery health: %.1f%%", health);

      if (health >= 80.0f) {
        ESP_LOGI(TAG, "OK Capacity healthy (>= 80%%)");
        diag_result.capacity_ok = true;
      } else if (health >= 60.0f) {
        ESP_LOGW(TAG, "! Capacity degraded (60-80%%)");
        diag_result.capacity_ok = true;
        diag_result.warning_count++;
      } else {
        ESP_LOGE(TAG, "X Capacity severely degraded (< 60%%)");
        ESP_LOGE(TAG, "  Consider battery replacement");
        diag_result.capacity_ok = false;
        diag_result.error_count++;
      }
    }
  }

  /* ===== DIAGNOSTIC SUMMARY ===== */
  print_diagnostic_summary();

  /* ===== TROUBLESHOOTING GUIDE ===== */
  ESP_LOGI(TAG, "\n===== Troubleshooting Guide =====");

  if (!diag_result.communication_ok) {
    ESP_LOGI(TAG, "\nCommunication Issues:");
    ESP_LOGI(TAG, "1. Check I2C wiring (SDA, SCL, GND)");
    ESP_LOGI(TAG, "2. Verify pull-up resistors (2.2k-10k typical)");
    ESP_LOGI(TAG, "3. Check I2C address (default 0x08 for BQ7850)");
    ESP_LOGI(TAG, "4. Verify BMS power supply");
    ESP_LOGI(TAG, "5. Check for shorts or bad connections");
  }

  if (!diag_result.voltages_ok) {
    ESP_LOGI(TAG, "\nVoltage Issues:");
    ESP_LOGI(TAG, "1. Check cell connections");
    ESP_LOGI(TAG, "2. Verify no cells are damaged");
    ESP_LOGI(TAG, "3. Test individual cells with multimeter");
    ESP_LOGI(TAG, "4. Check for shorts in cell wiring");
    ESP_LOGI(TAG, "5. Verify BMS voltage sensing circuits");
  }

  if (!diag_result.balance_ok) {
    ESP_LOGI(TAG, "\nBalance Issues:");
    ESP_LOGI(TAG, "1. Enable cell balancing");
    ESP_LOGI(TAG, "2. Perform full charge/discharge cycle");
    ESP_LOGI(TAG, "3. Check for weak cell");
    ESP_LOGI(TAG, "4. Allow balancing time (hours)");
    ESP_LOGI(TAG, "5. Replace if one cell consistently low");
  }

  if (!diag_result.temperature_ok) {
    ESP_LOGI(TAG, "\nTemperature Issues:");
    ESP_LOGI(TAG, "1. Check thermistor connections");
    ESP_LOGI(TAG, "2. Verify thermistor values");
    ESP_LOGI(TAG, "3. Check for cooling system malfunction");
    ESP_LOGI(TAG, "4. Reduce current if overheating");
    ESP_LOGI(TAG, "5. Verify ambient temperature reasonable");
  }

  if (!diag_result.protection_ok) {
    ESP_LOGI(TAG, "\nProtection Faults:");
    ESP_LOGI(TAG, "1. Identify specific fault from status");
    ESP_LOGI(TAG, "2. Remove fault condition");
    ESP_LOGI(TAG, "3. Reset BMS if needed");
    ESP_LOGI(TAG, "4. Verify protection settings appropriate");
    ESP_LOGI(TAG, "5. Monitor after clearing fault");
  }

  /* ===== RECOMMENDATIONS ===== */
  ESP_LOGI(TAG, "\n===== Recommendations =====");

  if (diag_result.error_count == 0 && diag_result.warning_count == 0) {
    ESP_LOGI(TAG, "OK System health: EXCELLENT");
    ESP_LOGI(TAG, "  No action required");
  } else if (diag_result.error_count == 0) {
    ESP_LOGI(TAG, "! System health: GOOD");
    ESP_LOGI(TAG, "  Minor issues detected - monitor closely");
  } else if (diag_result.error_count < 3) {
    ESP_LOGW(TAG, "! System health: FAIR");
    ESP_LOGW(TAG, "  Issues require attention");
  } else {
    ESP_LOGE(TAG, "X System health: POOR");
    ESP_LOGE(TAG, "  Immediate action required");
  }

cleanup:
  /* Cleanup */
  if (diag_result.communication_ok) {
    star_bms_bq7850_deinit(&g_bms);
  }
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nDiagnostic scan complete");
}
