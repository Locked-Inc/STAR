/**
 * @file 017_bms_control.c
 * @brief BMS control operations example
 *
 * Demonstrates:
 * - Cell balancing control
 * - FET control (charge/discharge)
 * - Protection threshold configuration
 * - Software reset
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "BMS_CONTROL_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t s_manager;
static bq7850_handle_t    s_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== BMS Control Example ===\n");

  /* Initialize bus manager and BMS */
  ret = star_bus_manager_init(&s_manager, "control_demo", NULL, NULL);
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

  /* --- Cell Balancing --- */
  ESP_LOGI(s_tag, "\n--- Cell Balancing ---");

  /* Read current cell voltages */
  bq7850_cell_data_t cells;
  ret = star_bms_bq7850_read_cells(&s_bms, &cells);
  if (ret == ESP_OK) {
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

    ESP_LOGI(s_tag, "Cell voltages before balancing:");
    ESP_LOGI(s_tag, "  Min: %d mV (cell %d)", min_v, min_idx + 1);
    ESP_LOGI(s_tag, "  Max: %d mV (cell %d)", max_v, max_idx + 1);
    ESP_LOGI(s_tag, "  Delta: %d mV", max_v - min_v);
  }

  /* Enable balancing on cells that are too high */
  /* Bitmask: bit 0 = cell 1, bit 1 = cell 2, etc. */
  uint16_t balance_mask = 0;

  /* Example: Balance cells that are 10mV above minimum */
  uint16_t min_v = 0xFFFF;
  for (int i = 0; i < cells.valid_cells; i++) {
    if (cells.cell_mv[i] < min_v) min_v = cells.cell_mv[i];
  }
  for (int i = 0; i < cells.valid_cells; i++) {
    if (cells.cell_mv[i] > min_v + 10) {
      balance_mask |= (1 << i);
    }
  }

  ESP_LOGI(s_tag, "Balancing mask: 0x%04X", balance_mask);

  if (balance_mask != 0) {
    ret = star_bms_bq7850_enable_cell_balancing(&s_bms, balance_mask);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Cell balancing enabled");

      /* Check balancing status */
      uint16_t status;
      ret = star_bms_bq7850_get_balancing_status(&s_bms, &status);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Balancing status: 0x%04X", status);
        for (int i = 0; i < 16; i++) {
          if (status & (1 << i)) {
            ESP_LOGI(s_tag, "  Cell %d: Balancing", i + 1);
          }
        }
      }

      /* Let it balance for a bit */
      vTaskDelay(pdMS_TO_TICKS(1000));

      /* Disable balancing */
      ret = star_bms_bq7850_disable_cell_balancing(&s_bms);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Cell balancing disabled");
      }
    }
  } else {
    ESP_LOGI(s_tag, "No cells need balancing");
  }

  /* --- FET Control --- */
  ESP_LOGI(s_tag, "\n--- FET Control ---");

  ESP_LOGI(s_tag, "FET control:");
  ESP_LOGI(s_tag, "  true/false = Enable/Disable charge FET");
  ESP_LOGI(s_tag, "  true/false = Enable/Disable discharge FET");

  /* Enable both charge and discharge FETs */
  ret = star_bms_bq7850_control_fets(&s_bms, true, true);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Charge and discharge FETs enabled");
  }

  /* Disable discharge only (for storage) */
  ret = star_bms_bq7850_control_fets(&s_bms, true, false);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Discharge FET disabled (charge only mode)");
  }

  /* Re-enable both */
  star_bms_bq7850_control_fets(&s_bms, true, true);

  /* --- Protection Thresholds --- */
  ESP_LOGI(s_tag, "\n--- Protection Thresholds ---");

  /* Read current protection settings */
  bq7850_protection_t protection;
  ret = star_bms_bq7850_read_protection(&s_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Current protection settings:");
    ESP_LOGI(s_tag, "  Overvoltage: %d mV", protection.overvoltage_mv);
    ESP_LOGI(s_tag, "  Undervoltage: %d mV", protection.undervoltage_mv);
    ESP_LOGI(s_tag, "  Overcharge current: %d mA", protection.overcharge_ma);
    ESP_LOGI(s_tag, "  Overdischarge current: %d mA", protection.overdischarge_ma);
    ESP_LOGI(s_tag, "  Over-temperature: %d C", protection.overtemp_c);
    ESP_LOGI(s_tag, "  Under-temperature: %d C", protection.undertemp_c);
  }

  /* Modify protection settings */
  ESP_LOGI(s_tag, "\nModifying protection thresholds...");

  protection.overvoltage_mv = 4200;     /* 4.2V per cell */
  protection.undervoltage_mv = 2800;    /* 2.8V per cell */
  protection.overcharge_ma = 5000;      /* 5A charge limit */
  protection.overdischarge_ma = 10000;  /* 10A discharge limit */
  protection.overtemp_c = 60;           /* 60C max */
  protection.undertemp_c = 0;           /* 0C min */

  ret = star_bms_bq7850_write_protection(&s_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Protection thresholds updated");
  } else {
    ESP_LOGW(s_tag, "Failed to write protection: %s", esp_err_to_name(ret));
  }

  /* Verify new settings */
  ret = star_bms_bq7850_read_protection(&s_bms, &protection);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Verified new settings:");
    ESP_LOGI(s_tag, "  OV: %d mV, UV: %d mV",
             protection.overvoltage_mv, protection.undervoltage_mv);
    ESP_LOGI(s_tag, "  OC charge: %d mA, OC discharge: %d mA",
             protection.overcharge_ma, protection.overdischarge_ma);
    ESP_LOGI(s_tag, "  Temp: %d to %d C",
             protection.undertemp_c, protection.overtemp_c);
  }

  /* --- Software Reset --- */
  ESP_LOGI(s_tag, "\n--- Software Reset ---");
  ESP_LOGI(s_tag, "Performing software reset...");

  ret = star_bms_bq7850_reset(&s_bms);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Software reset successful");
    ESP_LOGI(s_tag, "Note: BMS may need re-initialization after reset");
  } else {
    ESP_LOGW(s_tag, "Software reset failed: %s", esp_err_to_name(ret));
  }

  /* --- Safety Notes --- */
  ESP_LOGI(s_tag, "\n--- Safety Notes ---");
  ESP_LOGI(s_tag, "IMPORTANT: Always verify protection settings before use!");
  ESP_LOGI(s_tag, "- Check voltage limits match your cell chemistry");
  ESP_LOGI(s_tag, "- Set current limits based on cell ratings");
  ESP_LOGI(s_tag, "- Temperature limits depend on application");
  ESP_LOGI(s_tag, "- Test FET control before deployment");
  ESP_LOGI(s_tag, "- Cell balancing should only be done during charging");

  /* Cleanup */
  star_bms_bq7850_deinit(&s_bms);
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nBMS control example complete");
}
