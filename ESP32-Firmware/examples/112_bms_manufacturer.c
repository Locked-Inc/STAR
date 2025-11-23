/**
 * @file 112_bms_manufacturer.c
 * @brief Manufacturer data and device information
 *
 * Demonstrates:
 * - Reading manufacturer info block
 * - Device name and chemistry
 * - Serial number and date codes
 * - Hardware/firmware versions
 * - Design parameters
 */

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <string.h>

static const char* TAG = "BMS_MANUFACTURER";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t g_manager;
static bq7850_handle_t g_bms;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== BQ7850 Manufacturer Data Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "mfg_demo", NULL, NULL);
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

  /* ===== DEVICE IDENTIFICATION ===== */
  ESP_LOGI(TAG, "===== Device Identification =====");

  bq7850_device_info_t info;
  ret = star_bms_bq7850_get_device_info(&g_bms, &info);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "\nDevice Type:");
    ESP_LOGI(TAG, "  Device Type ID: 0x%04X", info.device_type);

    /* Decode device type */
    switch (info.device_type & 0xFF00) {
      case 0x7800:
        ESP_LOGI(TAG, "  Family: BQ78xx (Battery Monitor)");
        break;
      default:
        ESP_LOGI(TAG, "  Family: Unknown (0x%04X)", info.device_type);
        break;
    }

    ESP_LOGI(TAG, "  Model: BQ7850");
    ESP_LOGI(TAG, "  Description: 3-16 Series Cell Li-Ion Battery Monitor and Protector");

    /* Firmware Version */
    ESP_LOGI(TAG, "\nFirmware Information:");
    uint8_t fw_major = (info.fw_version >> 8) & 0xFF;
    uint8_t fw_minor = info.fw_version & 0xFF;
    ESP_LOGI(TAG, "  Version: %d.%d", fw_major, fw_minor);
    ESP_LOGI(TAG, "  Version (raw): 0x%04X", info.fw_version);

    /* Hardware Version */
    ESP_LOGI(TAG, "\nHardware Information:");
    ESP_LOGI(TAG, "  Hardware Revision: %d", info.hw_version);
    ESP_LOGI(TAG, "  Hardware Revision (hex): 0x%04X", info.hw_version);

  } else {
    ESP_LOGE(TAG, "Failed to read device info: %s", esp_err_to_name(ret));
  }

  /* ===== SERIAL NUMBER ===== */
  ESP_LOGI(TAG, "\n===== Serial Number =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Serial Number: %lu", (unsigned long)info.serial_number);
    ESP_LOGI(TAG, "Serial Number (hex): 0x%08lX", (unsigned long)info.serial_number);

    /* Decode serial number components (manufacturer specific) */
    /* This is an example - actual encoding varies by manufacturer */
    uint16_t year_code = (info.serial_number >> 24) & 0xFF;
    uint16_t week_code = (info.serial_number >> 16) & 0xFF;
    uint16_t unit_code = info.serial_number & 0xFFFF;

    ESP_LOGI(TAG, "\nSerial Number Decode (example):");
    ESP_LOGI(TAG, "  Year Code: %u", year_code);
    ESP_LOGI(TAG, "  Week Code: %u", week_code);
    ESP_LOGI(TAG, "  Unit Code: %u", unit_code);
    ESP_LOGI(TAG, "  Note: Actual encoding depends on manufacturer");
  }

  /* ===== MANUFACTURER NAME ===== */
  ESP_LOGI(TAG, "\n===== Manufacturer Information =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Manufacturer: %s", info.manufacturer);

    /* Check if manufacturer name is valid */
    if (strlen(info.manufacturer) == 0) {
      ESP_LOGW(TAG, "  (Manufacturer name not programmed)");
    } else {
      ESP_LOGI(TAG, "  Length: %d characters", strlen(info.manufacturer));
    }
  }

  /* ===== DEVICE NAME ===== */
  ESP_LOGI(TAG, "\n===== Device Name =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Device Name: %s", info.device_name);

    /* Check if device name is valid */
    if (strlen(info.device_name) == 0) {
      ESP_LOGW(TAG, "  (Device name not programmed)");
    } else {
      ESP_LOGI(TAG, "  Length: %d characters", strlen(info.device_name));

      /* Device name often includes capacity information */
      ESP_LOGI(TAG, "  Note: Device name may include:");
      ESP_LOGI(TAG, "    - Battery pack model number");
      ESP_LOGI(TAG, "    - Capacity rating");
      ESP_LOGI(TAG, "    - Application identifier");
    }
  }

  /* ===== BATTERY CHEMISTRY ===== */
  ESP_LOGI(TAG, "\n===== Battery Chemistry =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Chemistry: %s", info.chemistry);

    /* Common chemistry identifiers */
    if (strstr(info.chemistry, "LION") || strstr(info.chemistry, "Li-Ion")) {
      ESP_LOGI(TAG, "  Type: Lithium-Ion");
      ESP_LOGI(TAG, "  Nominal Voltage: 3.6-3.7V per cell");
      ESP_LOGI(TAG, "  Charge Voltage: 4.2V per cell");
      ESP_LOGI(TAG, "  Discharge Cutoff: 2.5-3.0V per cell");
    } else if (strstr(info.chemistry, "LiPo") || strstr(info.chemistry, "Li-Po")) {
      ESP_LOGI(TAG, "  Type: Lithium-Polymer");
      ESP_LOGI(TAG, "  Nominal Voltage: 3.7V per cell");
      ESP_LOGI(TAG, "  Similar to Li-Ion characteristics");
    } else if (strstr(info.chemistry, "LiFe") || strstr(info.chemistry, "LFP")) {
      ESP_LOGI(TAG, "  Type: Lithium Iron Phosphate (LiFePO4)");
      ESP_LOGI(TAG, "  Nominal Voltage: 3.2V per cell");
      ESP_LOGI(TAG, "  Charge Voltage: 3.6V per cell");
      ESP_LOGI(TAG, "  Discharge Cutoff: 2.0-2.5V per cell");
    } else if (strlen(info.chemistry) == 0) {
      ESP_LOGW(TAG, "  (Chemistry not programmed)");
    } else {
      ESP_LOGI(TAG, "  Type: %s", info.chemistry);
    }
  }

  /* ===== DESIGN PARAMETERS ===== */
  ESP_LOGI(TAG, "\n===== Design Parameters =====");

  ESP_LOGI(TAG, "Design Capacity: %d mAh", bms_config.design_capacity);
  ESP_LOGI(TAG, "  = %.2f Ah", bms_config.design_capacity / 1000.0f);
  ESP_LOGI(TAG, "  = %.3f kWh (at design voltage)",
           (bms_config.design_capacity / 1000.0f) * (bms_config.design_voltage / 1000.0f));

  ESP_LOGI(TAG, "\nDesign Voltage: %d mV", bms_config.design_voltage);
  ESP_LOGI(TAG, "  = %.2f V", bms_config.design_voltage / 1000.0f);

  /* Calculate implied cell count and cell voltage */
  if (bms_config.design_voltage > 0) {
    /* Assuming Li-Ion chemistry (3.6V nominal) */
    float cell_voltage = 3.6f;
    int estimated_cells = (int)((bms_config.design_voltage / 1000.0f) / cell_voltage + 0.5f);

    ESP_LOGI(TAG, "\nEstimated configuration:");
    ESP_LOGI(TAG, "  Cell count: ~%d cells in series", estimated_cells);
    ESP_LOGI(TAG, "  Per-cell voltage: ~%.2fV", (bms_config.design_voltage / 1000.0f) / estimated_cells);
  }

  /* Energy capacity */
  float energy_wh = (bms_config.design_capacity / 1000.0f) * (bms_config.design_voltage / 1000.0f);
  ESP_LOGI(TAG, "\nEnergy capacity:");
  ESP_LOGI(TAG, "  %.2f Wh", energy_wh);
  if (energy_wh >= 1000.0f) {
    ESP_LOGI(TAG, "  %.3f kWh", energy_wh / 1000.0f);
  }

  /* ===== MANUFACTURER DATA BLOCK ===== */
  ESP_LOGI(TAG, "\n===== Manufacturer Data Block =====");
  ESP_LOGI(TAG, "The manufacturer data block (register 0x23) contains:");
  ESP_LOGI(TAG, "  - Additional manufacturer information");
  ESP_LOGI(TAG, "  - Production date codes");
  ESP_LOGI(TAG, "  - Quality control data");
  ESP_LOGI(TAG, "  - Custom application data");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Note: Format is manufacturer-specific");
  ESP_LOGI(TAG, "Check manufacturer documentation for details");

  /* ===== COMPLETE DEVICE SUMMARY ===== */
  ESP_LOGI(TAG, "\n===== Device Summary =====");

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+------------------------------------------+");
    ESP_LOGI(TAG, "|         BQ7850 Device Summary            |");
    ESP_LOGI(TAG, "+------------------------------------------+");
    ESP_LOGI(TAG, "| Manufacturer: %-24s |", info.manufacturer);
    ESP_LOGI(TAG, "| Device Name:  %-24s |", info.device_name);
    ESP_LOGI(TAG, "| Chemistry:    %-24s |", info.chemistry);
    ESP_LOGI(TAG, "| Serial:       %-24lu |", (unsigned long)info.serial_number);
    ESP_LOGI(TAG, "+------------------------------------------+");
    ESP_LOGI(TAG, "| Capacity:     %-20d mAh |", bms_config.design_capacity);
    ESP_LOGI(TAG, "| Voltage:      %-20d mV  |", bms_config.design_voltage);
    ESP_LOGI(TAG, "| Firmware:     v%-22d |", info.fw_version);
    ESP_LOGI(TAG, "| Hardware:     Rev %-18d |", info.hw_version);
    ESP_LOGI(TAG, "+------------------------------------------+");
    ESP_LOGI(TAG, "");
  }

  /* ===== DATA SHEET INFORMATION ===== */
  ESP_LOGI(TAG, "===== BQ7850 Datasheet Information =====");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Key Specifications:");
  ESP_LOGI(TAG, "  - Cell Count: 3-16 series cells");
  ESP_LOGI(TAG, "  - Voltage Range: 9V - 65V");
  ESP_LOGI(TAG, "  - Current Measurement: +-100A (with appropriate sense resistor)");
  ESP_LOGI(TAG, "  - Voltage Accuracy: +-10mV (typical)");
  ESP_LOGI(TAG, "  - Current Accuracy: +-1%% (typical)");
  ESP_LOGI(TAG, "  - Temperature Sensors: Up to 3");
  ESP_LOGI(TAG, "  - Communication: I2C/SMBus");
  ESP_LOGI(TAG, "  - Protection Features: OV, UV, OC, OT, UT");
  ESP_LOGI(TAG, "  - Cell Balancing: Yes (passive)");

  /* ===== USAGE RECOMMENDATIONS ===== */
  ESP_LOGI(TAG, "\n===== Usage Recommendations =====");
  ESP_LOGI(TAG, "1. Record device information during production");
  ESP_LOGI(TAG, "2. Use serial number for traceability");
  ESP_LOGI(TAG, "3. Verify chemistry matches battery cells");
  ESP_LOGI(TAG, "4. Check firmware version for known issues");
  ESP_LOGI(TAG, "5. Document design parameters for reference");
  ESP_LOGI(TAG, "6. Store manufacturer data in external database");
  ESP_LOGI(TAG, "7. Use device name for application identification");
  ESP_LOGI(TAG, "8. Keep manufacturer contact information");

  /* Cleanup */
  star_bms_bq7850_deinit(&g_bms);
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nManufacturer data example complete");
}
