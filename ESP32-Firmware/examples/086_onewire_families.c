/**
 * @file 086_onewire_families.c
 * @brief One-Wire Device Families Example
 *
 * Demonstrates:
 * - Working with different One-Wire device families
 * - DS18B20 temperature sensor operations
 * - DS2431 1K EEPROM operations
 * - DS2433 4K EEPROM operations
 * - DS2401/DS2411 silicon serial number
 * - Family code identification and device-specific commands
 * - CRC verification for each device type
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_FAMILIES";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS18B20 Temperature Sensor Commands */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBE)
#define DS18B20_CMD_WRITE_SCRATCHPAD (0x4E)
#define DS18B20_CMD_COPY_SCRATCHPAD (0x48)
#define DS18B20_CMD_RECALL_E2 (0xB8)

/* DS2431/DS2433 EEPROM Commands */
#define DS243X_CMD_WRITE_SCRATCHPAD (0x0F)
#define DS243X_CMD_READ_SCRATCHPAD (0xAA)
#define DS243X_CMD_COPY_SCRATCHPAD (0x55)
#define DS243X_CMD_READ_MEMORY (0xF0)

/* Device family names */
static const char *priv_get_family_name(uint8_t family)
{
  switch (family) {
    case STAR_ONEWIRE_FAMILY_DS18S20:
      return "DS18S20 (Temperature)";
    case STAR_ONEWIRE_FAMILY_DS18B20:
      return "DS18B20 (Temperature)";
    case STAR_ONEWIRE_FAMILY_DS1822:
      return "DS1822 (Temperature)";
    case STAR_ONEWIRE_FAMILY_DS2431:
      return "DS2431 (1K EEPROM)";
    case STAR_ONEWIRE_FAMILY_DS2433:
      return "DS2433 (4K EEPROM)";
    case STAR_ONEWIRE_FAMILY_DS2401:
      return "DS2401/DS2411/DS1990A (Serial/iButton)";
    default:
      return "Unknown";
  }
}

/* Read temperature from DS18B20 */
static esp_err_t priv_read_ds18b20_temperature(star_bus_manager_t *manager,
                                               const char         *bus_name,
                                               star_onewire_rom_t  rom,
                                               float              *temperature)
{
  esp_err_t ret;

  /* Start temperature conversion */
  uint8_t convert_cmd = DS18B20_CMD_CONVERT_T;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &convert_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for conversion (750ms for 12-bit resolution) */
  vTaskDelay(pdMS_TO_TICKS(750));

  /* Read scratchpad */
  uint8_t read_cmd = DS18B20_CMD_READ_SCRATCHPAD;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &read_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  uint8_t scratchpad[9];
  ret = star_bus_onewire_read_bytes(manager, bus_name, scratchpad, 9);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Verify CRC */
  uint8_t crc = star_bus_onewire_crc8(scratchpad, 8);
  if (crc != scratchpad[8]) {
    ESP_LOGE(s_tag, "DS18B20 CRC mismatch: expected 0x%02X, got 0x%02X", scratchpad[8], crc);
    return ESP_ERR_INVALID_CRC;
  }

  /* Calculate temperature */
  int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
  *temperature     = (float)raw_temp / 16.0f;

  return ESP_OK;
}

/* Read DS2431 EEPROM */
static esp_err_t priv_read_ds2431_memory(star_bus_manager_t *manager,
                                         const char         *bus_name,
                                         star_onewire_rom_t  rom,
                                         uint8_t             address,
                                         uint8_t            *data,
                                    size_t              length)
{
  esp_err_t ret;

  /* Send read memory command */
  uint8_t cmd[3] = {DS243X_CMD_READ_MEMORY, address & 0xFF, (address >> 8) & 0xFF};
  ret            = star_bus_onewire_write_bytes(manager, bus_name, rom, cmd, 3);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read data */
  ret = star_bus_onewire_read_bytes(manager, bus_name, data, length);
  return ret;
}

/* Write DS2431 EEPROM */
static esp_err_t priv_write_ds2431_memory(star_bus_manager_t *manager,
                                          const char         *bus_name,
                                          star_onewire_rom_t  rom,
                                          uint8_t             address,
                                          const uint8_t      *data,
                                     size_t              length)
{
  esp_err_t ret;

  /* DS2431 writes in 8-byte rows */
  if (length > 8) {
    length = 8;
  }

  /* Write to scratchpad */
  uint8_t cmd[3] = {DS243X_CMD_WRITE_SCRATCHPAD, address & 0xFF, (address >> 8) & 0xFF};
  ret            = star_bus_onewire_write_bytes(manager, bus_name, rom, cmd, 3);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, data, length);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read scratchpad for verification */
  uint8_t read_cmd = DS243X_CMD_READ_SCRATCHPAD;
  ret              = star_bus_onewire_write_bytes(manager, bus_name, rom, &read_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  uint8_t scratchpad[13];
  ret = star_bus_onewire_read_bytes(manager, bus_name, scratchpad, 13);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Copy scratchpad to memory */
  uint8_t copy_cmd[4] = {DS243X_CMD_COPY_SCRATCHPAD, scratchpad[0], scratchpad[1], scratchpad[2]};
  ret                 = star_bus_onewire_write_bytes(manager, bus_name, rom, copy_cmd, 4);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for copy to complete */
  vTaskDelay(pdMS_TO_TICKS(15));

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Device Families Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "onewire_families", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Configure One-Wire bus */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;
  config.use_parasitic_power   = false;
  config.use_strong_pullup     = false;

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire bus initialized on GPIO %d\n", ONEWIRE_GPIO);

  /* Search for all devices */
  ESP_LOGI(s_tag, "--- Searching for devices ---");

  star_onewire_rom_t rom_codes[STAR_ONEWIRE_MAX_DEVICES];
  size_t             device_count = STAR_ONEWIRE_MAX_DEVICES;

  ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &device_count);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Search failed: %s", esp_err_to_name(ret));
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu device(s)\n", device_count);

  /* Process each device by family */
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);
    uint8_t crc    = star_bus_onewire_get_crc(rom_codes[i]);

    ESP_LOGI(s_tag, "--- Device %zu ---", i);
    ESP_LOGI(s_tag, "ROM: 0x%016llX", (unsigned long long)rom_codes[i]);
    ESP_LOGI(s_tag, "Family: 0x%02X (%s)", family, priv_get_family_name(family));
    ESP_LOGI(s_tag, "CRC: 0x%02X", crc);

    /* Verify ROM CRC */
    bool crc_valid = star_bus_onewire_verify_rom(rom_codes[i]);
    ESP_LOGI(s_tag, "ROM CRC: %s", crc_valid ? "VALID" : "INVALID");

    if (!crc_valid) {
      ESP_LOGW(s_tag, "Skipping device with invalid CRC\n");
      continue;
    }

    /* Handle device based on family */
    switch (family) {
      case STAR_ONEWIRE_FAMILY_DS18B20:
      case STAR_ONEWIRE_FAMILY_DS18S20:
      case STAR_ONEWIRE_FAMILY_DS1822: {
        ESP_LOGI(s_tag, "Reading temperature...");
        float temperature;
        ret = priv_read_ds18b20_temperature(&manager, "onewire0", rom_codes[i], &temperature);
        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "Temperature: %.2f C (%.2f F)", temperature, temperature * 9.0f / 5.0f + 32.0f);
        } else {
          ESP_LOGE(s_tag, "Failed to read temperature: %s", esp_err_to_name(ret));
        }
        break;
      }

      case STAR_ONEWIRE_FAMILY_DS2431: {
        ESP_LOGI(s_tag, "DS2431 1K EEPROM operations");

        /* Read first 16 bytes */
        uint8_t read_data[16];
        ret = priv_read_ds2431_memory(&manager, "onewire0", rom_codes[i], 0x00, read_data, 16);
        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "First 16 bytes:");
          ESP_LOG_BUFFER_HEX_LEVEL(s_tag, read_data, 16, ESP_LOG_INFO);
        }

        /* Write test data */
        uint8_t write_data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
        ESP_LOGI(s_tag, "Writing test data to address 0x00...");
        ret = priv_write_ds2431_memory(&manager, "onewire0", rom_codes[i], 0x00, write_data, 8);
        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "Write successful");

          /* Read back to verify */
          uint8_t verify_data[8];
          ret = priv_read_ds2431_memory(&manager, "onewire0", rom_codes[i], 0x00, verify_data, 8);
          if (ret == ESP_OK) {
            bool match = (memcmp(write_data, verify_data, 8) == 0);
            ESP_LOGI(s_tag, "Verification: %s", match ? "PASS" : "FAIL");
          }
        } else {
          ESP_LOGE(s_tag, "Write failed: %s", esp_err_to_name(ret));
        }
        break;
      }

      case STAR_ONEWIRE_FAMILY_DS2433: {
        ESP_LOGI(s_tag, "DS2433 4K EEPROM operations");

        /* Read first page */
        uint8_t page_data[32];
        ret = priv_read_ds2431_memory(&manager, "onewire0", rom_codes[i], 0x00, page_data, 32);
        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "First page (32 bytes):");
          ESP_LOG_BUFFER_HEX_LEVEL(s_tag, page_data, 32, ESP_LOG_INFO);
        }
        break;
      }

      case STAR_ONEWIRE_FAMILY_DS2401: {
        /* Note: DS2401 and DS2411 share same family code 0x01 */
        ESP_LOGI(s_tag, "Silicon Serial Number (DS2401/DS2411)");

        /* Convert ROM to bytes for display */
        uint8_t rom_bytes[8];
        star_bus_onewire_rom_to_bytes(rom_codes[i], rom_bytes);

        ESP_LOGI(s_tag, "Serial Number: %02X:%02X:%02X:%02X:%02X:%02X",
                 rom_bytes[1],
                 rom_bytes[2],
                 rom_bytes[3],
                 rom_bytes[4],
                 rom_bytes[5],
                 rom_bytes[6]);
        break;
      }

      default:
        ESP_LOGI(s_tag, "Unsupported device family");
        break;
    }

    ESP_LOGI(s_tag, "");
  }

  /* Display statistics */
  ESP_LOGI(s_tag, "--- Bus Statistics ---");
  star_onewire_stats_t stats;
  ret = star_bus_onewire_get_stats(&manager, "onewire0", &stats);
  if (ret == ESP_OK) {
    star_bus_onewire_print_stats("onewire0", &stats);
  }

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nOne-Wire families example complete");
}
