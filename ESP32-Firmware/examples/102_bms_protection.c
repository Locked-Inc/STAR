/**
 * @file 102_bms_protection.c
 * @brief BMS protection configuration and monitoring
 *
 * Demonstrates:
 * - Reading current protection settings
 * - Configuring overvoltage/undervoltage protection
 * - Setting overcurrent/undercurrent thresholds
 * - Configuring temperature protection limits
 * - Verifying protection threshold changes
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BMS_PROTECTION";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Protection Configuration ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "protection_demo", NULL, NULL);
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

  ESP_LOGI(TAG, "BMS initialized successfully");

  /* ===== READ CURRENT PROTECTION SETTINGS ===== */
  ESP_LOGI(TAG, "\n===== Current Protection Settings =====");

  bq7850_protection_t protection;
  ret = star_bms_bq7850_read_protection(&g_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Voltage Protection:");
    ESP_LOGI(TAG, "  Overvoltage threshold:  %d mV per cell", protection.overvoltage_mv);
    ESP_LOGI(TAG, "  Undervoltage threshold: %d mV per cell", protection.undervoltage_mv);
    ESP_LOGI(TAG, "  Safe voltage range: %d - %d mV",
             protection.undervoltage_mv, protection.overvoltage_mv);

    ESP_LOGI(TAG, "\nCurrent Protection:");
    ESP_LOGI(TAG, "  Overcharge current:     %d mA", protection.overcharge_ma);
    ESP_LOGI(TAG, "  Overdischarge current:  %d mA", protection.overdischarge_ma);

    ESP_LOGI(TAG, "\nTemperature Protection:");
    ESP_LOGI(TAG, "  Over-temperature:       %d C", protection.overtemp_c);
    ESP_LOGI(TAG, "  Under-temperature:      %d C", protection.undertemp_c);
    ESP_LOGI(TAG, "  Safe temp range: %d - %d C",
             protection.undertemp_c, protection.overtemp_c);
  } else {
    ESP_LOGW(TAG, "Failed to read protection settings: %s", esp_err_to_name(ret));
  }

  /* ===== CONFIGURE PROTECTION FOR LI-ION CELLS ===== */
  ESP_LOGI(TAG, "\n===== Configuring Li-Ion Protection =====");
  ESP_LOGI(TAG, "Setting conservative limits for Li-Ion cells (3.7V nominal)");

  /* Li-Ion typical protection values */
  protection.overvoltage_mv = 4200;     /* 4.2V max per cell */
  protection.undervoltage_mv = 2800;    /* 2.8V min per cell (conservative) */
  protection.overcharge_ma = 3000;      /* 3A charge limit */
  protection.overdischarge_ma = 6000;   /* 6A discharge limit */
  protection.overtemp_c = 60;           /* 60C max operating temp */
  protection.undertemp_c = 0;           /* 0C min charging temp */

  ESP_LOGI(TAG, "New protection settings:");
  ESP_LOGI(TAG, "  Voltage: %d - %d mV per cell",
           protection.undervoltage_mv, protection.overvoltage_mv);
  ESP_LOGI(TAG, "  Current: Charge %d mA, Discharge %d mA",
           protection.overcharge_ma, protection.overdischarge_ma);
  ESP_LOGI(TAG, "  Temperature: %d - %d C",
           protection.undertemp_c, protection.overtemp_c);

  ret = star_bms_bq7850_write_protection(&g_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Protection settings updated successfully");
  } else {
    ESP_LOGW(TAG, "Failed to write protection settings: %s", esp_err_to_name(ret));
    ESP_LOGW(TAG, "Device may be sealed - unseal required for configuration");
  }

  /* ===== VERIFY NEW SETTINGS ===== */
  ESP_LOGI(TAG, "\n===== Verifying New Settings =====");

  bq7850_protection_t verified;
  ret = star_bms_bq7850_read_protection(&g_bms, &verified);
  if (ret == ESP_OK) {
    bool match = true;

    if (verified.overvoltage_mv != protection.overvoltage_mv) {
      ESP_LOGW(TAG, "Overvoltage mismatch: expected %d, got %d",
               protection.overvoltage_mv, verified.overvoltage_mv);
      match = false;
    }

    if (verified.undervoltage_mv != protection.undervoltage_mv) {
      ESP_LOGW(TAG, "Undervoltage mismatch: expected %d, got %d",
               protection.undervoltage_mv, verified.undervoltage_mv);
      match = false;
    }

    if (verified.overcharge_ma != protection.overcharge_ma) {
      ESP_LOGW(TAG, "Overcharge current mismatch: expected %d, got %d",
               protection.overcharge_ma, verified.overcharge_ma);
      match = false;
    }

    if (verified.overdischarge_ma != protection.overdischarge_ma) {
      ESP_LOGW(TAG, "Overdischarge current mismatch: expected %d, got %d",
               protection.overdischarge_ma, verified.overdischarge_ma);
      match = false;
    }

    if (match) {
      ESP_LOGI(TAG, "All protection settings verified successfully");
    } else {
      ESP_LOGW(TAG, "Some settings could not be verified");
    }
  }

  /* ===== CONFIGURE PROTECTION FOR LIFEPO4 CELLS ===== */
  ESP_LOGI(TAG, "\n===== Alternative: LiFePO4 Protection =====");
  ESP_LOGI(TAG, "Example settings for LiFePO4 cells (3.2V nominal):");

  bq7850_protection_t lifepo4_protection = {
    .overvoltage_mv = 3650,     /* 3.65V max per cell */
    .undervoltage_mv = 2500,    /* 2.5V min per cell */
    .overcharge_ma = 5000,      /* 5A charge limit (higher than Li-Ion) */
    .overdischarge_ma = 10000,  /* 10A discharge limit */
    .overtemp_c = 60,           /* 60C max */
    .undertemp_c = -20          /* -20C min (LiFePO4 can handle colder temps) */
  };

  ESP_LOGI(TAG, "  Voltage: %d - %d mV per cell",
           lifepo4_protection.undervoltage_mv, lifepo4_protection.overvoltage_mv);
  ESP_LOGI(TAG, "  Current: Charge %d mA, Discharge %d mA",
           lifepo4_protection.overcharge_ma, lifepo4_protection.overdischarge_ma);
  ESP_LOGI(TAG, "  Temperature: %d - %d C",
           lifepo4_protection.undertemp_c, lifepo4_protection.overtemp_c);
  ESP_LOGI(TAG, "  (Not applied - example only)");

  /* ===== PROTECTION STATUS MONITORING ===== */
  ESP_LOGI(TAG, "\n===== Protection Status Monitoring =====");

  /* Read current battery state */
  bq7850_battery_state_t state;
  ret = star_bms_bq7850_read_battery_state(&g_bms, &state);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Current battery state:");
    ESP_LOGI(TAG, "  Pack voltage: %u mV (limit: %d - %d mV per cell)",
             state.cells.pack_mv,
             protection.undervoltage_mv, protection.overvoltage_mv);
    ESP_LOGI(TAG, "  Current: %d mA (limits: +%d / -%d mA)",
             state.current.current_ma,
             protection.overcharge_ma, protection.overdischarge_ma);
    ESP_LOGI(TAG, "  Temperature: %.1f C (limit: %d - %d C)",
             star_bms_bq7850_convert_temp_to_celsius(state.temps.temp_c[0]),
             protection.undertemp_c, protection.overtemp_c);

    /* Check for protection violations */
    bool violation = false;

    /* Check voltage (any cell) */
    for (int i = 0; i < state.cells.valid_cells; i++) {
      if (state.cells.cell_mv[i] > protection.overvoltage_mv) {
        ESP_LOGW(TAG, "  PROTECTION: Cell %d overvoltage! (%d mV > %d mV)",
                 i + 1, state.cells.cell_mv[i], protection.overvoltage_mv);
        violation = true;
      }
      if (state.cells.cell_mv[i] < protection.undervoltage_mv) {
        ESP_LOGW(TAG, "  PROTECTION: Cell %d undervoltage! (%d mV < %d mV)",
                 i + 1, state.cells.cell_mv[i], protection.undervoltage_mv);
        violation = true;
      }
    }

    /* Check current */
    if (state.current.current_ma > protection.overcharge_ma) {
      ESP_LOGW(TAG, "  PROTECTION: Overcharge current! (%d mA > %d mA)",
               state.current.current_ma, protection.overcharge_ma);
      violation = true;
    }
    if (state.current.current_ma < -protection.overdischarge_ma) {
      ESP_LOGW(TAG, "  PROTECTION: Overdischarge current! (%d mA < -%d mA)",
               state.current.current_ma, protection.overdischarge_ma);
      violation = true;
    }

    /* Check temperature */
    int16_t temp_c = state.temps.temp_c[0] / 10;  /* Convert 0.1C to C */
    if (temp_c > protection.overtemp_c) {
      ESP_LOGW(TAG, "  PROTECTION: Over-temperature! (%d C > %d C)",
               temp_c, protection.overtemp_c);
      violation = true;
    }
    if (temp_c < protection.undertemp_c) {
      ESP_LOGW(TAG, "  PROTECTION: Under-temperature! (%d C < %d C)",
               temp_c, protection.undertemp_c);
      violation = true;
    }

    if (!violation) {
      ESP_LOGI(TAG, "  All parameters within protection limits");
    }
  }

  /* ===== SAFETY RECOMMENDATIONS ===== */
  ESP_LOGI(TAG, "\n===== Safety Recommendations =====");
  ESP_LOGI(TAG, "IMPORTANT: Always follow these guidelines:");
  ESP_LOGI(TAG, "1. Match voltage limits to cell chemistry specifications");
  ESP_LOGI(TAG, "2. Set current limits based on cell C-rating");
  ESP_LOGI(TAG, "3. Temperature limits depend on cell datasheet and application");
  ESP_LOGI(TAG, "4. Add 10-20%% safety margin to manufacturer limits");
  ESP_LOGI(TAG, "5. Test protection triggers before deployment");
  ESP_LOGI(TAG, "6. Monitor protection events in production");
  ESP_LOGI(TAG, "7. Device must be unsealed to modify protection settings");
  ESP_LOGI(TAG, "8. Always verify settings after writing");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nProtection configuration example complete");
}
