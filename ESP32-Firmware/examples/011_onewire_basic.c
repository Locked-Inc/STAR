/**
 * @file 011_onewire_basic.c
 * @brief Basic One-Wire operations example
 *
 * Demonstrates:
 * - One-Wire bus initialization
 * - Reset pulse and presence detection
 * - Reading/writing bits and bytes
 * - Device search
 * - ROM commands
 */

#include "star_bus_manager.h"
#include "star_bus_onewire.h"

#include <esp_log.h>

static const char *s_tag = "ONEWIRE_BASIC_EXAMPLE";

#define ONEWIRE_PIN   (GPIO_NUM_4)
#define BUS_NAME      "onewire0"

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Basic One-Wire Example ===\n");

  /* --- Initialize Bus Manager --- */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Default Configuration --- */
  ESP_LOGI(s_tag, "--- Configuration ---");

  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin = ONEWIRE_PIN;

  ESP_LOGI(s_tag, "Data pin: %d", config.gpio_pin);
  ESP_LOGI(s_tag, "Speed: %s", config.speed == STAR_ONEWIRE_SPEED_STANDARD ? "Standard" : "Overdrive");
  ESP_LOGI(s_tag, "Parasitic power: %s", config.use_parasitic_power ? "Yes" : "No");
  ESP_LOGI(s_tag, "Strong pullup: %s", config.use_strong_pullup ? "Yes" : "No");

  /* Initialize One-Wire bus */
  ret = star_bus_onewire_init(&manager, BUS_NAME, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire bus initialized");

  /* --- Reset and Presence Detection --- */
  ESP_LOGI(s_tag, "\n--- Reset and Presence ---");

  bool presence;
  ret = star_bus_onewire_reset(&manager, BUS_NAME, &presence);
  if (ret == ESP_OK) {
    if (presence) {
      ESP_LOGI(s_tag, "Device presence detected");
    } else {
      ESP_LOGW(s_tag, "No device presence detected");
    }
  } else {
    ESP_LOGE(s_tag, "Reset failed: %s", esp_err_to_name(ret));
  }

  /* --- Bit Operations --- */
  ESP_LOGI(s_tag, "\n--- Bit Operations ---");

  /* Write single bit */
  ret = star_bus_onewire_write_bit(&manager, BUS_NAME, 1);
  ESP_LOGI(s_tag, "Write bit 1: %s", esp_err_to_name(ret));

  ret = star_bus_onewire_write_bit(&manager, BUS_NAME, 0);
  ESP_LOGI(s_tag, "Write bit 0: %s", esp_err_to_name(ret));

  /* Read single bit */
  uint8_t bit;
  ret = star_bus_onewire_read_bit(&manager, BUS_NAME, &bit);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read bit: %d", bit);
  }

  /* --- Byte Operations --- */
  ESP_LOGI(s_tag, "\n--- Byte Operations ---");

  /* Reset bus first */
  star_bus_onewire_reset(&manager, BUS_NAME, &presence);

  /* Write byte */
  ret = star_bus_onewire_write_byte(&manager, BUS_NAME, 0xCC);  /* Skip ROM command */
  ESP_LOGI(s_tag, "Write byte 0xCC (Skip ROM): %s", esp_err_to_name(ret));

  /* Read byte */
  uint8_t byte;
  ret = star_bus_onewire_read_byte(&manager, BUS_NAME, &byte);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read byte: 0x%02X", byte);
  }

  /* --- Read ROM (Single Device) --- */
  ESP_LOGI(s_tag, "\n--- Read ROM ---");

  star_bus_onewire_reset(&manager, BUS_NAME, &presence);
  if (presence) {
    uint64_t rom;
    ret = star_bus_onewire_read_rom(&manager, BUS_NAME, &rom);
    if (ret == ESP_OK) {
      uint8_t family = star_bus_onewire_get_family(rom);
      uint8_t crc = star_bus_onewire_get_crc(rom);

      ESP_LOGI(s_tag, "ROM: 0x%016llX", (unsigned long long)rom);
      ESP_LOGI(s_tag, "Family code: 0x%02X", family);
      ESP_LOGI(s_tag, "CRC: 0x%02X", crc);

      /* Identify device type */
      switch (family) {
        case STAR_ONEWIRE_FAMILY_DS18S20:
          ESP_LOGI(s_tag, "Device: DS18S20 temperature sensor");
          break;
        case STAR_ONEWIRE_FAMILY_DS18B20:
          ESP_LOGI(s_tag, "Device: DS18B20 temperature sensor");
          break;
        case STAR_ONEWIRE_FAMILY_DS1822:
          ESP_LOGI(s_tag, "Device: DS1822 temperature sensor");
          break;
        case STAR_ONEWIRE_FAMILY_DS2431:
          ESP_LOGI(s_tag, "Device: DS2431 1K EEPROM");
          break;
        case STAR_ONEWIRE_FAMILY_DS2433:
          ESP_LOGI(s_tag, "Device: DS2433 4K EEPROM");
          break;
        default:
          ESP_LOGI(s_tag, "Device: Unknown family");
          break;
      }

      /* Verify CRC */
      bool crc_valid = star_bus_onewire_verify_rom(rom);
      ESP_LOGI(s_tag, "CRC valid: %s", crc_valid ? "Yes" : "No");
    }
  }

  /* --- Device Search --- */
  ESP_LOGI(s_tag, "\n--- Device Search ---");

  star_onewire_rom_t devices[8];
  size_t device_count = 8;

  ret = star_bus_onewire_search(&manager, BUS_NAME, devices, &device_count);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Found %zu device(s):", device_count);
    for (size_t i = 0; i < device_count; i++) {
      uint8_t family = star_bus_onewire_get_family(devices[i]);
      ESP_LOGI(s_tag, "  [%zu] ROM: 0x%016llX (family: 0x%02X)",
               i, (unsigned long long)devices[i], family);
    }
  } else {
    ESP_LOGW(s_tag, "Search failed or no devices: %s", esp_err_to_name(ret));
  }

  /* --- Search for Alarming Devices --- */
  ESP_LOGI(s_tag, "\n--- Alarm Search ---");

  star_onewire_rom_t alarm_devices[4];
  size_t alarm_count = 4;

  ret = star_bus_onewire_search_alarms(&manager, BUS_NAME, alarm_devices, &alarm_count);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Found %zu alarming device(s)", alarm_count);
    for (size_t i = 0; i < alarm_count; i++) {
      ESP_LOGI(s_tag, "  [%zu] ROM: 0x%016llX", i, (unsigned long long)alarm_devices[i]);
    }
  }

  /* --- ROM Conversion Utilities --- */
  ESP_LOGI(s_tag, "\n--- ROM Utilities ---");

  if (device_count > 0) {
    uint64_t rom = devices[0];

    /* Convert ROM to byte array */
    uint8_t rom_bytes[8];
    star_bus_onewire_rom_to_bytes(rom, rom_bytes);
    ESP_LOGI(s_tag, "ROM as bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
             rom_bytes[0], rom_bytes[1], rom_bytes[2], rom_bytes[3],
             rom_bytes[4], rom_bytes[5], rom_bytes[6], rom_bytes[7]);

    /* Convert back to ROM */
    uint64_t reconstructed = star_bus_onewire_bytes_to_rom(rom_bytes);
    ESP_LOGI(s_tag, "Reconstructed ROM: 0x%016llX", (unsigned long long)reconstructed);
    ESP_LOGI(s_tag, "Match: %s", (rom == reconstructed) ? "Yes" : "No");
  }

  /* --- CRC Calculations --- */
  ESP_LOGI(s_tag, "\n--- CRC Calculations ---");

  uint8_t data[] = {0x28, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x9A};
  uint8_t crc8 = star_bus_onewire_crc8(data, sizeof(data));
  ESP_LOGI(s_tag, "CRC-8 of data: 0x%02X", crc8);

  uint8_t data16[] = {0x01, 0x02, 0x03, 0x04};
  uint16_t crc16 = star_bus_onewire_crc16(data16, sizeof(data16));
  ESP_LOGI(s_tag, "CRC-16 of data: 0x%04X", crc16);

  /* --- Statistics --- */
  ESP_LOGI(s_tag, "\n--- Statistics ---");

  star_onewire_stats_t stats;
  ret = star_bus_onewire_get_stats(&manager, BUS_NAME, &stats);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Total resets: %llu", (unsigned long long)stats.total_resets);
    ESP_LOGI(s_tag, "Successful resets: %llu", (unsigned long long)stats.successful_resets);
    ESP_LOGI(s_tag, "Failed resets: %llu", (unsigned long long)stats.failed_resets);
    ESP_LOGI(s_tag, "Bytes written: %llu", (unsigned long long)stats.bytes_written);
    ESP_LOGI(s_tag, "Bytes read: %llu", (unsigned long long)stats.bytes_read);
    ESP_LOGI(s_tag, "CRC errors: %llu", (unsigned long long)stats.crc_errors);
    ESP_LOGI(s_tag, "Search operations: %llu", (unsigned long long)stats.search_operations);
    ESP_LOGI(s_tag, "Devices found: %u", stats.devices_found);
  }

  /* Print formatted stats */
  star_bus_onewire_print_stats(BUS_NAME, &stats);

  /* Reset stats */
  star_bus_onewire_reset_stats(&manager, BUS_NAME);

  /* Cleanup */
  star_bus_onewire_deinit(&manager, BUS_NAME);
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nBasic One-Wire example complete");
}
