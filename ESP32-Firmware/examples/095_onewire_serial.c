/**
 * @file 095_onewire_serial.c
 * @brief DS2401 Silicon Serial Number reading example
 *
 * Demonstrates:
 * - DS2401/DS2411/DS1990A serial number chips
 * - 64-bit unique serial number
 * - Read-only ROM identification
 * - CRC verification
 * - Serial number formatting
 * - Multiple device identification
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_SERIAL";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/**
 * @brief Format serial number as string
 */
static void priv_format_serial_number(uint64_t rom, char *buffer, size_t buffer_size)
{
  uint8_t bytes[8];
  star_bus_onewire_rom_to_bytes(rom, bytes);

  /* Format as XX:XX:XX:XX:XX:XX (skip family code and CRC) */
  snprintf(buffer,
           buffer_size,
           "%02X:%02X:%02X:%02X:%02X:%02X",
           bytes[1],
           bytes[2],
           bytes[3],
           bytes[4],
           bytes[5],
           bytes[6]);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== DS2401 Silicon Serial Number Example ===\n");

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

  ESP_LOGI(s_tag, "One-Wire initialized on GPIO %d\n", ONEWIRE_GPIO);

  /* --- Search for All Devices --- */
  ESP_LOGI(s_tag, "--- Searching for Devices ---");

  star_onewire_rom_t rom_codes[16];
  size_t             device_count = 16;

  ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &device_count);
  if (ret != ESP_OK || device_count == 0) {
    ESP_LOGW(s_tag, "No devices found");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu device(s)\n", device_count);

  /* --- Process Each Device --- */
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);
    uint8_t crc    = star_bus_onewire_get_crc(rom_codes[i]);
    bool    valid  = star_bus_onewire_verify_rom(rom_codes[i]);

    ESP_LOGI(s_tag, "--- Device %zu ---", i);
    ESP_LOGI(s_tag, "ROM: 0x%016llX", (unsigned long long)rom_codes[i]);
    ESP_LOGI(s_tag, "Family code: 0x%02X", family);
    ESP_LOGI(s_tag, "CRC: 0x%02X (%s)", crc, valid ? "VALID" : "INVALID");

    if (!valid) {
      ESP_LOGW(s_tag, "Skipping device with invalid CRC\n");
      continue;
    }

    /* Check if it's a serial number device */
    if (family == STAR_ONEWIRE_FAMILY_DS2401) {
      char serial_str[32];
      priv_format_serial_number(rom_codes[i], serial_str, sizeof(serial_str));

      ESP_LOGI(s_tag, "Device type: DS2401/DS2411 (Silicon Serial Number)");
      ESP_LOGI(s_tag, "Serial Number: %s", serial_str);

      /* Extract bytes for detailed analysis */
      uint8_t bytes[8];
      star_bus_onewire_rom_to_bytes(rom_codes[i], bytes);

      ESP_LOGI(s_tag, "Detailed breakdown:");
      ESP_LOGI(s_tag, "  Family code: 0x%02X", bytes[0]);
      ESP_LOGI(s_tag, "  Serial: %02X:%02X:%02X:%02X:%02X:%02X",
               bytes[1],
               bytes[2],
               bytes[3],
               bytes[4],
               bytes[5],
               bytes[6]);
      ESP_LOGI(s_tag, "  CRC: 0x%02X", bytes[7]);
    } else {
      ESP_LOGI(s_tag, "Device type: Other (family 0x%02X)", family);
    }

    ESP_LOGI(s_tag, "");
  }

  /* --- Single Device Read --- */
  ESP_LOGI(s_tag, "--- Single Device Read ROM ---");

  if (device_count == 1) {
    bool             presence;
    star_onewire_rom_t rom;

    ret = star_bus_onewire_reset(&manager, "onewire0", &presence);
    if (ret == ESP_OK && presence) {
      ret = star_bus_onewire_read_rom(&manager, "onewire0", &rom);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Read ROM: 0x%016llX", (unsigned long long)rom);

        if (rom == rom_codes[0]) {
          ESP_LOGI(s_tag, "Matches search result: YES");
        }
      }
    }
  } else {
    ESP_LOGI(s_tag, "Multiple devices present, skipping Read ROM");
    ESP_LOGI(s_tag, "(Read ROM only works with single device on bus)");
  }

  /* --- Application Examples --- */
  ESP_LOGI(s_tag, "\n--- Application Examples ---");

  ESP_LOGI(s_tag, "Example 1: Device Authentication");
  if (device_count > 0) {
    char serial[32];
    priv_format_serial_number(rom_codes[0], serial, sizeof(serial));
    ESP_LOGI(s_tag, "  Authenticating device: %s", serial);
    ESP_LOGI(s_tag, "  Device authenticated: %s",
             star_bus_onewire_verify_rom(rom_codes[0]) ? "YES" : "NO");
  }

  ESP_LOGI(s_tag, "\nExample 2: Unique Device ID");
  if (device_count > 0) {
    ESP_LOGI(s_tag, "  Using ROM as unique ID:");
    ESP_LOGI(s_tag, "  0x%016llX", (unsigned long long)rom_codes[0]);
  }

  ESP_LOGI(s_tag, "\nExample 3: Asset Tracking");
  for (size_t i = 0; i < device_count && i < 3; i++) {
    char serial[32];
    priv_format_serial_number(rom_codes[i], serial, sizeof(serial));
    ESP_LOGI(s_tag, "  Asset %zu: SN %s", i + 1, serial);
  }

  /* --- Use Cases --- */
  ESP_LOGI(s_tag, "\n--- DS2401/DS2411 Use Cases ---");
  ESP_LOGI(s_tag, "1. Unique device identification");
  ESP_LOGI(s_tag, "2. Product authentication");
  ESP_LOGI(s_tag, "3. Asset tracking and inventory");
  ESP_LOGI(s_tag, "4. License management (hardware dongles)");
  ESP_LOGI(s_tag, "5. MAC address generation");
  ESP_LOGI(s_tag, "6. Warranty tracking");
  ESP_LOGI(s_tag, "7. Accessory pairing");
  ESP_LOGI(s_tag, "8. Counterfeit prevention");

  ESP_LOGI(s_tag, "\n--- Device Characteristics ---");
  ESP_LOGI(s_tag, "Family code: 0x01 (DS2401/DS2411)");
  ESP_LOGI(s_tag, "Serial number: 48-bit unique");
  ESP_LOGI(s_tag, "Total ROM: 64-bit (family + serial + CRC)");
  ESP_LOGI(s_tag, "Read-only: Cannot be reprogrammed");
  ESP_LOGI(s_tag, "Voltage: 2.8V - 6.0V");
  ESP_LOGI(s_tag, "Operating temperature: -40C to +85C");
  ESP_LOGI(s_tag, "Package: TO-92, TSOC (DS2401) / SOT23 (DS2411)");

  ESP_LOGI(s_tag, "\n--- iButton (DS1990A) ---");
  ESP_LOGI(s_tag, "Same family code: 0x01");
  ESP_LOGI(s_tag, "Physical form: Stainless steel button");
  ESP_LOGI(s_tag, "Common uses: Access control, time attendance");
  ESP_LOGI(s_tag, "Touch memory: Contact-based reading");

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nDS2401 Serial Number example complete");
}
