/**
 * @file 113_bms_safety.c
 * @brief Safety alerts and fault handling
 *
 * Demonstrates:
 * - Configuring safety alert thresholds
 * - Monitoring safety status flags
 * - Handling safety events
 * - Emergency shutdown procedures
 * - Safety system testing
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_SAFETY";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

/**
 * @brief Check and display all safety status flags
 */
static void check_safety_status(const bq7850_status_t* status)
{
  ESP_LOGI(TAG, "\nSafety Status Analysis:");

  bool any_fault = false;

  /* Cell voltage faults */
  if (status->safety_status & BQ7850_SAFETY_STATUS_COV) {
    ESP_LOGE(TAG, "  [CRITICAL] Cell Overvoltage detected!");
    ESP_LOGE(TAG, "    Action: Disable charging immediately");
    any_fault = true;
  }

  if (status->safety_status & BQ7850_SAFETY_STATUS_CUV) {
    ESP_LOGE(TAG, "  [CRITICAL] Cell Undervoltage detected!");
    ESP_LOGE(TAG, "    Action: Disable discharging immediately");
    any_fault = true;
  }

  /* Current faults */
  if (status->safety_status & BQ7850_SAFETY_STATUS_OCC) {
    ESP_LOGE(TAG, "  [CRITICAL] Overcurrent in Charge!");
    ESP_LOGE(TAG, "    Action: Reduce charge current or disable charge FET");
    any_fault = true;
  }

  if (status->safety_status & BQ7850_SAFETY_STATUS_OCD) {
    ESP_LOGE(TAG, "  [CRITICAL] Overcurrent in Discharge!");
    ESP_LOGE(TAG, "    Action: Reduce discharge current or disable discharge FET");
    any_fault = true;
  }

  /* Temperature faults */
  if (status->safety_status & BQ7850_SAFETY_STATUS_OTC) {
    ESP_LOGE(TAG, "  [CRITICAL] Over-Temperature during Charge!");
    ESP_LOGE(TAG, "    Action: Stop charging, allow cooling");
    any_fault = true;
  }

  if (status->safety_status & BQ7850_SAFETY_STATUS_OTD) {
    ESP_LOGE(TAG, "  [CRITICAL] Over-Temperature during Discharge!");
    ESP_LOGE(TAG, "    Action: Stop discharging, allow cooling");
    any_fault = true;
  }

  if (status->safety_status & BQ7850_SAFETY_STATUS_UTC) {
    ESP_LOGW(TAG, "  [WARNING] Under-Temperature during Charge!");
    ESP_LOGW(TAG, "    Action: Stop charging (Li-Ion cannot charge below 0C)");
    any_fault = true;
  }

  if (status->safety_status & BQ7850_SAFETY_STATUS_UTD) {
    ESP_LOGW(TAG, "  [WARNING] Under-Temperature during Discharge!");
    ESP_LOGW(TAG, "    Action: Reduce discharge current");
    any_fault = true;
  }

  if (!any_fault) {
    ESP_LOGI(TAG, "  [OK] All safety parameters within limits");
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Safety System Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "safety_demo", NULL, NULL);
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

  /* ===== SAFETY SYSTEM OVERVIEW ===== */
  ESP_LOGI(TAG, "===== BQ7850 Safety System =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Protection Features:");
  ESP_LOGI(TAG, "  1. Overvoltage Protection (OVP)");
  ESP_LOGI(TAG, "  2. Undervoltage Protection (UVP)");
  ESP_LOGI(TAG, "  3. Overcurrent in Charge (OCC)");
  ESP_LOGI(TAG, "  4. Overcurrent in Discharge (OCD)");
  ESP_LOGI(TAG, "  5. Over-Temperature Charge (OTC)");
  ESP_LOGI(TAG, "  6. Over-Temperature Discharge (OTD)");
  ESP_LOGI(TAG, "  7. Under-Temperature Charge (UTC)");
  ESP_LOGI(TAG, "  8. Under-Temperature Discharge (UTD)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Protection Actions:");
  ESP_LOGI(TAG, "  - Automatic FET control");
  ESP_LOGI(TAG, "  - Status flag setting");
  ESP_LOGI(TAG, "  - Alert pin assertion (if configured)");

  /* ===== READ CURRENT SAFETY STATUS ===== */
  ESP_LOGI(TAG, "\n===== Current Safety Status =====");

  bq7850_status_t status;
  ret = star_bms_bq7850_read_status(&g_bms, &status);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Status Registers:");
    ESP_LOGI(TAG, "  Battery Status:   0x%04X", status.battery_status);
    ESP_LOGI(TAG, "  Safety Status:    0x%04X", status.safety_status);
    ESP_LOGI(TAG, "  Operation Status: 0x%04X", status.operation_status);

    check_safety_status(&status);
  } else {
    ESP_LOGE(TAG, "Failed to read status: %s", esp_err_to_name(ret));
  }

  /* ===== CONFIGURE SAFETY THRESHOLDS ===== */
  ESP_LOGI(TAG, "\n===== Configure Safety Thresholds =====");

  bq7850_protection_t protection;
  ret = star_bms_bq7850_read_protection(&g_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current protection settings:");
    ESP_LOGI(TAG, "  Overvoltage:  %d mV per cell", protection.overvoltage_mv);
    ESP_LOGI(TAG, "  Undervoltage: %d mV per cell", protection.undervoltage_mv);
    ESP_LOGI(TAG, "  Overcharge current:    %d mA", protection.overcharge_ma);
    ESP_LOGI(TAG, "  Overdischarge current: %d mA", protection.overdischarge_ma);
    ESP_LOGI(TAG, "  Over-temperature:  %d C", protection.overtemp_c);
    ESP_LOGI(TAG, "  Under-temperature: %d C", protection.undertemp_c);

    /* Set conservative safety thresholds */
    ESP_LOGI(TAG, "\nSetting conservative safety thresholds...");

    protection.overvoltage_mv = 4200;     /* 4.2V max (Li-Ion) */
    protection.undervoltage_mv = 2800;    /* 2.8V min (conservative) */
    protection.overcharge_ma = 3000;      /* 3A charge limit */
    protection.overdischarge_ma = 6000;   /* 6A discharge limit */
    protection.overtemp_c = 55;           /* 55C max */
    protection.undertemp_c = 0;           /* 0C min for charging */

    ret = star_bms_bq7850_write_protection(&g_bms, &protection);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "Safety thresholds updated");
    } else {
      ESP_LOGW(TAG, "Failed to update thresholds (device may be sealed)");
    }
  }

  /* ===== MONITOR BATTERY PARAMETERS ===== */
  ESP_LOGI(TAG, "\n===== Monitor Battery Parameters =====");

  bq7850_battery_state_t state;
  ret = star_bms_bq7850_read_battery_state(&g_bms, &state);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current battery state:");
    ESP_LOGI(TAG, "  Pack voltage: %u mV", state.cells.pack_mv);
    ESP_LOGI(TAG, "  Current: %d mA", state.current.current_ma);
    ESP_LOGI(TAG, "  Temperature: %.1f C", state.temps.avg_temp_c);
    ESP_LOGI(TAG, "  SOC: %d%%", state.soc.relative_soc);

    /* Check each parameter against thresholds */
    ESP_LOGI(TAG, "\nParameter Safety Check:");

    /* Check cell voltages */
    bq7850_cell_data_t cells;
    if (star_bms_bq7850_read_cells(&g_bms, &cells) == ESP_OK) {
      bool voltage_ok = true;
      for (int i = 0; i < cells.valid_cells; i++) {
        if (cells.cell_mv[i] > protection.overvoltage_mv) {
          ESP_LOGW(TAG, "  Cell %d overvoltage: %d mV > %d mV",
                   i + 1, cells.cell_mv[i], protection.overvoltage_mv);
          voltage_ok = false;
        }
        if (cells.cell_mv[i] < protection.undervoltage_mv) {
          ESP_LOGW(TAG, "  Cell %d undervoltage: %d mV < %d mV",
                   i + 1, cells.cell_mv[i], protection.undervoltage_mv);
          voltage_ok = false;
        }
      }
      if (voltage_ok) {
        ESP_LOGI(TAG, "  Cell voltages: OK");
      }
    }

    /* Check current */
    if (abs(state.current.current_ma) < protection.overcharge_ma &&
        abs(state.current.current_ma) < protection.overdischarge_ma) {
      ESP_LOGI(TAG, "  Current: OK");
    } else {
      ESP_LOGW(TAG, "  Current: EXCEEDS LIMITS");
    }

    /* Check temperature */
    int16_t temp_c = (int16_t)state.temps.avg_temp_c;
    if (temp_c >= protection.undertemp_c && temp_c <= protection.overtemp_c) {
      ESP_LOGI(TAG, "  Temperature: OK");
    } else {
      ESP_LOGW(TAG, "  Temperature: OUT OF RANGE");
    }
  }

  /* ===== SAFETY EVENT HANDLING ===== */
  ESP_LOGI(TAG, "\n===== Safety Event Handling =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Recommended response to safety events:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Cell Overvoltage:");
  ESP_LOGI(TAG, "   - Immediately disable charge FET");
  ESP_LOGI(TAG, "   - Allow discharge to reduce voltage");
  ESP_LOGI(TAG, "   - Check for charger malfunction");
  ESP_LOGI(TAG, "   - Enable cell balancing");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Cell Undervoltage:");
  ESP_LOGI(TAG, "   - Immediately disable discharge FET");
  ESP_LOGI(TAG, "   - Prevent deep discharge damage");
  ESP_LOGI(TAG, "   - Charge at reduced current");
  ESP_LOGI(TAG, "   - Check for failing cells");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Overcurrent:");
  ESP_LOGI(TAG, "   - Disable appropriate FET");
  ESP_LOGI(TAG, "   - Check for short circuit");
  ESP_LOGI(TAG, "   - Verify load is appropriate");
  ESP_LOGI(TAG, "   - Allow cool-down period");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Temperature Fault:");
  ESP_LOGI(TAG, "   - Reduce or stop current flow");
  ESP_LOGI(TAG, "   - Allow thermal stabilization");
  ESP_LOGI(TAG, "   - Check cooling system");
  ESP_LOGI(TAG, "   - Verify temperature sensors");

  /* ===== CONTINUOUS SAFETY MONITORING ===== */
  ESP_LOGI(TAG, "\n===== Continuous Safety Monitoring =====");
  ESP_LOGI(TAG, "Monitoring safety status for 10 seconds...\n");

  for (int i = 0; i < 10; i++) {
    ret = star_bms_bq7850_read_status(&g_bms, &status);
    if (ret == ESP_OK) {
      bool fault = star_bms_bq7850_is_fault_active(&status);

      if (fault) {
        ESP_LOGE(TAG, "[%d] FAULT ACTIVE - Safety status: 0x%04X",
                 i, status.safety_status);
        check_safety_status(&status);

        /* Emergency shutdown procedure */
        ESP_LOGE(TAG, "Initiating emergency shutdown!");
        star_bms_bq7850_control_fets(&g_bms, false, false);  /* Disable all FETs */
        break;
      } else {
        ESP_LOGI(TAG, "[%d] Safety: OK (0x%04X)", i, status.safety_status);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  /* ===== RECOVERY PROCEDURE ===== */
  ESP_LOGI(TAG, "\n===== Fault Recovery Procedure =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "After fault condition clears:");
  ESP_LOGI(TAG, "1. Verify fault is actually cleared");
  ESP_LOGI(TAG, "2. Wait for settling time (2-5 minutes)");
  ESP_LOGI(TAG, "3. Check all parameters manually");
  ESP_LOGI(TAG, "4. Test at reduced current first");
  ESP_LOGI(TAG, "5. Gradually restore to normal operation");
  ESP_LOGI(TAG, "6. Monitor closely after recovery");
  ESP_LOGI(TAG, "7. Log incident for analysis");
  ESP_LOGI(TAG, "8. Investigate root cause");

  /* ===== SAFETY SYSTEM TESTING ===== */
  ESP_LOGI(TAG, "\n===== Safety System Testing =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Before deployment, test all safety features:");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "1. Overvoltage test:");
  ESP_LOGI(TAG, "   - Gradually increase voltage to threshold");
  ESP_LOGI(TAG, "   - Verify protection triggers");
  ESP_LOGI(TAG, "   - Confirm FET opens");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "2. Undervoltage test:");
  ESP_LOGI(TAG, "   - Discharge to threshold");
  ESP_LOGI(TAG, "   - Verify protection triggers");
  ESP_LOGI(TAG, "   - Confirm discharge stops");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "3. Overcurrent test:");
  ESP_LOGI(TAG, "   - Apply known overcurrent");
  ESP_LOGI(TAG, "   - Verify immediate response");
  ESP_LOGI(TAG, "   - Test both charge and discharge");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "4. Temperature test:");
  ESP_LOGI(TAG, "   - Use temperature chamber");
  ESP_LOGI(TAG, "   - Verify thresholds accurate");
  ESP_LOGI(TAG, "   - Test all sensors");

  /* ===== SAFETY BEST PRACTICES ===== */
  ESP_LOGI(TAG, "\n===== Safety Best Practices =====");
  ESP_LOGI(TAG, "1. Set thresholds with safety margin");
  ESP_LOGI(TAG, "2. Monitor continuously during operation");
  ESP_LOGI(TAG, "3. Implement redundant protection if critical");
  ESP_LOGI(TAG, "4. Log all safety events");
  ESP_LOGI(TAG, "5. Test safety system regularly");
  ESP_LOGI(TAG, "6. Have clear emergency procedures");
  ESP_LOGI(TAG, "7. Train users on safety warnings");
  ESP_LOGI(TAG, "8. Never bypass safety features");

  /* Restore normal FET operation */
  ESP_LOGI(TAG, "\n===== Restoring Normal Operation =====");
  star_bms_bq7850_control_fets(&g_bms, true, true);
  ESP_LOGI(TAG, "FETs re-enabled for normal operation");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nSafety system example complete");
}
