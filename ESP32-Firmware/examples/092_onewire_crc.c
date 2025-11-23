/**
 * @file 092_onewire_crc.c
 * @brief One-Wire CRC Calculations and Verification Example
 *
 * Demonstrates:
 * - CRC-8 calculation and verification (Dallas/Maxim polynomial)
 * - CRC-16 calculation for extended operations
 * - ROM code CRC verification
 * - Scratchpad data CRC checking
 * - EEPROM data integrity verification
 * - Custom CRC implementations
 * - Error detection and handling
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_CRC";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS18B20 commands */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBE)

/* DS2431 EEPROM commands */
#define DS2431_CMD_READ_MEMORY (0xF0)

/* Manual CRC-8 calculation for demonstration */
static uint8_t priv_calculate_crc8_manual(const uint8_t *data, size_t length)
{
  uint8_t crc = 0;

  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];

    for (int j = 0; j < 8; j++) {
      uint8_t mix = (crc ^ byte) & 0x01;
      crc >>= 1;

      if (mix) {
        crc ^= 0x8C; /* Dallas/Maxim polynomial: x^8 + x^5 + x^4 + 1 */
      }

      byte >>= 1;
    }
  }

  return crc;
}

/* Manual CRC-16 calculation for demonstration */
static uint16_t priv_calculate_crc16_manual(const uint8_t *data, size_t length)
{
  uint16_t crc = 0;

  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];

    for (int j = 0; j < 8; j++) {
      uint8_t mix = ((crc & 0x0001) ^ (byte & 0x01));
      crc >>= 1;

      if (mix) {
        crc ^= 0xA001; /* CRC-16 polynomial */
      }

      byte >>= 1;
    }
  }

  return crc;
}

/* Verify ROM code and display details */
static void priv_verify_rom_code(star_onewire_rom_t rom)
{
  uint8_t rom_bytes[8];
  star_bus_onewire_rom_to_bytes(rom, rom_bytes);

  ESP_LOGI(s_tag, "ROM: 0x%016llX", (unsigned long long)rom);
  ESP_LOGI(s_tag, "Bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
           rom_bytes[0], rom_bytes[1], rom_bytes[2], rom_bytes[3],
           rom_bytes[4], rom_bytes[5], rom_bytes[6], rom_bytes[7]);

  /* Calculate CRC using library function */
  uint8_t crc_lib = star_bus_onewire_crc8(rom_bytes, 7);
  ESP_LOGI(s_tag, "CRC-8 (library): 0x%02X", crc_lib);

  /* Calculate CRC manually */
  uint8_t crc_manual = priv_calculate_crc8_manual(rom_bytes, 7);
  ESP_LOGI(s_tag, "CRC-8 (manual):  0x%02X", crc_manual);

  /* Get stored CRC */
  uint8_t crc_stored = star_bus_onewire_get_crc(rom);
  ESP_LOGI(s_tag, "CRC-8 (stored):  0x%02X", crc_stored);

  /* Verify */
  bool valid_lib    = star_bus_onewire_verify_rom(rom);
  bool valid_manual = (crc_manual == crc_stored);
  bool match        = (crc_lib == crc_manual) && (crc_lib == crc_stored);

  ESP_LOGI(s_tag, "Library match: %s", valid_lib ? "PASS" : "FAIL");
  ESP_LOGI(s_tag, "Manual match:  %s", valid_manual ? "PASS" : "FAIL");
  ESP_LOGI(s_tag, "All match:     %s", match ? "PASS" : "FAIL");
}

/* Read and verify DS18B20 scratchpad with detailed CRC */
static esp_err_t priv_read_verify_scratchpad(star_bus_manager_t *manager,
                                             const char         *bus_name,
                                        star_onewire_rom_t  rom)
{
  esp_err_t ret;

  /* Start temperature conversion */
  uint8_t convert_cmd = DS18B20_CMD_CONVERT_T;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &convert_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

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

  /* Display scratchpad data */
  ESP_LOGI(s_tag, "Scratchpad data:");
  ESP_LOG_BUFFER_HEX_LEVEL(s_tag, scratchpad, 9, ESP_LOG_INFO);

  /* Calculate CRC using library */
  uint8_t crc_lib = star_bus_onewire_crc8(scratchpad, 8);
  ESP_LOGI(s_tag, "CRC-8 (library): 0x%02X", crc_lib);

  /* Calculate CRC manually */
  uint8_t crc_manual = priv_calculate_crc8_manual(scratchpad, 8);
  ESP_LOGI(s_tag, "CRC-8 (manual):  0x%02X", crc_manual);

  /* Get received CRC */
  uint8_t crc_received = scratchpad[8];
  ESP_LOGI(s_tag, "CRC-8 (received): 0x%02X", crc_received);

  /* Verify CRC */
  bool valid = (crc_lib == crc_received) && (crc_manual == crc_received);
  ESP_LOGI(s_tag, "CRC verification: %s", valid ? "PASS" : "FAIL");

  if (!valid) {
    ESP_LOGE(s_tag, "CRC mismatch detected!");
    return ESP_ERR_INVALID_CRC;
  }

  /* Parse temperature */
  int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
  float   temp_c   = (float)raw_temp / 16.0f;
  ESP_LOGI(s_tag, "Temperature: %.2f°C (%.2f°F)", temp_c, temp_c * 9.0f / 5.0f + 32.0f);

  return ESP_OK;
}

/* Demonstrate CRC-16 with EEPROM data */
static void priv_demonstrate_crc16(star_bus_manager_t *manager,
                                   const char         *bus_name,
                              star_onewire_rom_t  rom)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "Reading EEPROM data for CRC-16 demonstration...");

  /* Read 32 bytes from EEPROM */
  uint8_t cmd[3] = {DS2431_CMD_READ_MEMORY, 0x00, 0x00};
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, cmd, 3);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to send read command");
    return;
  }

  uint8_t data[32];
  ret = star_bus_onewire_read_bytes(manager, bus_name, data, 32);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read data");
    return;
  }

  /* Display data */
  ESP_LOGI(s_tag, "EEPROM data (32 bytes):");
  ESP_LOG_BUFFER_HEX_LEVEL(s_tag, data, 32, ESP_LOG_INFO);

  /* Calculate CRC-16 using library */
  uint16_t crc16_lib = star_bus_onewire_crc16(data, 32);
  ESP_LOGI(s_tag, "CRC-16 (library): 0x%04X", crc16_lib);

  /* Calculate CRC-16 manually */
  uint16_t crc16_manual = priv_calculate_crc16_manual(data, 32);
  ESP_LOGI(s_tag, "CRC-16 (manual):  0x%04X", crc16_manual);

  /* Verify match */
  bool match = (crc16_lib == crc16_manual);
  ESP_LOGI(s_tag, "CRC-16 match: %s", match ? "PASS" : "FAIL");
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire CRC Calculations Example ===\n");

  /* --- CRC Theory --- */
  ESP_LOGI(s_tag, "--- CRC (Cyclic Redundancy Check) ---");
  ESP_LOGI(s_tag, "Purpose: Detect transmission and storage errors");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "CRC-8 (Dallas/Maxim):");
  ESP_LOGI(s_tag, "  Polynomial: x^8 + x^5 + x^4 + 1");
  ESP_LOGI(s_tag, "  Binary: 0x8C (0b10001100)");
  ESP_LOGI(s_tag, "  Used for: ROM codes, scratchpad data");
  ESP_LOGI(s_tag, "  Error detection: Single-bit and some multi-bit errors");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "CRC-16:");
  ESP_LOGI(s_tag, "  Polynomial: x^16 + x^15 + x^2 + 1");
  ESP_LOGI(s_tag, "  Binary: 0xA001 (reversed)");
  ESP_LOGI(s_tag, "  Used for: EEPROM operations, extended data");
  ESP_LOGI(s_tag, "  Error detection: Better for larger data blocks");
  ESP_LOGI(s_tag, "");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Configure One-Wire bus */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire bus initialized on GPIO %d\n", ONEWIRE_GPIO);

  /* --- CRC-8 Test Vectors --- */
  ESP_LOGI(s_tag, "--- CRC-8 Test Vectors ---");

  struct {
    uint8_t data[8];
    size_t  len;
    uint8_t expected_crc;
    const char *description;
  } test_vectors[] = {
    {{0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 7, 0x00, "DS18B20 family + zeros"},
    {{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD}, 7, 0x00, "Sequential pattern"},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 7, 0x00, "All ones"},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 7, 0x00, "All zeros"},
  };

  for (size_t i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); i++) {
    uint8_t crc_lib = star_bus_onewire_crc8(test_vectors[i].data, test_vectors[i].len);
    uint8_t crc_manual = priv_calculate_crc8_manual(test_vectors[i].data, test_vectors[i].len);

    ESP_LOGI(s_tag, "Test %zu: %s", i + 1, test_vectors[i].description);
    ESP_LOGI(s_tag, "  Data: %02X %02X %02X %02X %02X %02X %02X",
             test_vectors[i].data[0], test_vectors[i].data[1], test_vectors[i].data[2],
             test_vectors[i].data[3], test_vectors[i].data[4], test_vectors[i].data[5],
             test_vectors[i].data[6]);
    ESP_LOGI(s_tag, "  CRC (library): 0x%02X", crc_lib);
    ESP_LOGI(s_tag, "  CRC (manual):  0x%02X", crc_manual);
    ESP_LOGI(s_tag, "  Match: %s", (crc_lib == crc_manual) ? "PASS" : "FAIL");
    ESP_LOGI(s_tag, "");
  }

  /* Search for devices */
  ESP_LOGI(s_tag, "--- Device ROM CRC Verification ---");

  star_onewire_rom_t rom_codes[10];
  size_t             device_count = 10;

  ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &device_count);
  if (ret != ESP_OK || device_count == 0) {
    ESP_LOGW(s_tag, "No devices found");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu device(s)\n", device_count);

  /* Verify ROM CRC for each device */
  for (size_t i = 0; i < device_count; i++) {
    ESP_LOGI(s_tag, "Device %zu:", i);
    priv_verify_rom_code(rom_codes[i]);
    ESP_LOGI(s_tag, "");
  }

  /* --- Scratchpad CRC Verification --- */
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);

    if (family == STAR_ONEWIRE_FAMILY_DS18B20 || family == STAR_ONEWIRE_FAMILY_DS18S20) {
      ESP_LOGI(s_tag, "--- DS18B20 Scratchpad CRC (Device %zu) ---", i);
      ret = priv_read_verify_scratchpad(&manager, "onewire0", rom_codes[i]);
      if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Scratchpad verification failed: %s", esp_err_to_name(ret));
      }
      ESP_LOGI(s_tag, "");
      break; /* Only demo first sensor */
    }
  }

  /* --- CRC-16 Demonstration --- */
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);

    if (family == STAR_ONEWIRE_FAMILY_DS2431 || family == STAR_ONEWIRE_FAMILY_DS2433) {
      ESP_LOGI(s_tag, "--- EEPROM CRC-16 (Device %zu) ---", i);
      priv_demonstrate_crc16(&manager, "onewire0", rom_codes[i]);
      ESP_LOGI(s_tag, "");
      break; /* Only demo first EEPROM */
    }
  }

  /* --- Error Detection Capabilities --- */
  ESP_LOGI(s_tag, "--- CRC Error Detection Capabilities ---");
  ESP_LOGI(s_tag, "CRC-8:");
  ESP_LOGI(s_tag, "  - Detects all single-bit errors");
  ESP_LOGI(s_tag, "  - Detects all 2-bit errors");
  ESP_LOGI(s_tag, "  - Detects ~99.6%% of all errors");
  ESP_LOGI(s_tag, "  - Good for short messages (8-16 bytes)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "CRC-16:");
  ESP_LOGI(s_tag, "  - Detects all single-bit errors");
  ESP_LOGI(s_tag, "  - Detects all 2-bit errors");
  ESP_LOGI(s_tag, "  - Detects ~99.998%% of all errors");
  ESP_LOGI(s_tag, "  - Good for longer messages (32+ bytes)");
  ESP_LOGI(s_tag, "");

  /* --- Common CRC Errors --- */
  ESP_LOGI(s_tag, "--- Common CRC Errors and Causes ---");
  ESP_LOGI(s_tag, "1. Noise on bus:");
  ESP_LOGI(s_tag, "   - Poor cabling or connections");
  ESP_LOGI(s_tag, "   - Electromagnetic interference");
  ESP_LOGI(s_tag, "   - Solution: Better shielding, shorter cables");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. Timing violations:");
  ESP_LOGI(s_tag, "   - Incorrect bit timings");
  ESP_LOGI(s_tag, "   - CPU interrupts during critical sections");
  ESP_LOGI(s_tag, "   - Solution: Disable interrupts, use hardware timers");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Power issues:");
  ESP_LOGI(s_tag, "   - Insufficient pull-up current");
  ESP_LOGI(s_tag, "   - Voltage drops on long cables");
  ESP_LOGI(s_tag, "   - Solution: Strong pull-up, better power supply");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. Device problems:");
  ESP_LOGI(s_tag, "   - Defective device");
  ESP_LOGI(s_tag, "   - Poor solder connections");
  ESP_LOGI(s_tag, "   - Solution: Test individual devices, check connections");
  ESP_LOGI(s_tag, "");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "--- CRC Verification Best Practices ---");
  ESP_LOGI(s_tag, "1. ALWAYS verify CRC before using data");
  ESP_LOGI(s_tag, "2. Retry operations on CRC failure (2-3 times)");
  ESP_LOGI(s_tag, "3. Log CRC errors for diagnostics");
  ESP_LOGI(s_tag, "4. Use CRC-16 for critical data");
  ESP_LOGI(s_tag, "5. Implement error counters");
  ESP_LOGI(s_tag, "6. Consider device replacement if high error rate");
  ESP_LOGI(s_tag, "7. Validate ROM codes at startup");
  ESP_LOGI(s_tag, "");

  /* --- Implementation Notes --- */
  ESP_LOGI(s_tag, "--- Implementation Notes ---");
  ESP_LOGI(s_tag, "Library vs Manual CRC:");
  ESP_LOGI(s_tag, "  - Library: Optimized, table-based, fast");
  ESP_LOGI(s_tag, "  - Manual: Slower, but demonstrates algorithm");
  ESP_LOGI(s_tag, "  - Both produce identical results");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "When to use each:");
  ESP_LOGI(s_tag, "  - Production: Use library functions");
  ESP_LOGI(s_tag, "  - Learning: Study manual implementation");
  ESP_LOGI(s_tag, "  - Debugging: Compare both implementations");
  ESP_LOGI(s_tag, "");

  /* Display statistics */
  ESP_LOGI(s_tag, "--- Bus Statistics ---");
  star_onewire_stats_t stats;
  ret = star_bus_onewire_get_stats(&manager, "onewire0", &stats);
  if (ret == ESP_OK) {
    star_bus_onewire_print_stats("onewire0", &stats);
    ESP_LOGI(s_tag, "CRC errors detected: %llu", (unsigned long long)stats.crc_errors);

    if (stats.bytes_read > 0) {
      float error_rate = (float)stats.crc_errors / (float)stats.bytes_read * 100.0f;
      ESP_LOGI(s_tag, "CRC error rate: %.4f%%", error_rate);
    }
  }

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nOne-Wire CRC example complete");
}
