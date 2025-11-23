/**
 * @file 088_onewire_parasite.c
 * @brief One-Wire Parasite Power Mode Example
 *
 * Demonstrates:
 * - Parasite power vs normal power modes
 * - Strong pull-up timing for power-hungry operations
 * - Detecting parasite-powered devices
 * - Temperature conversion with parasite power
 * - EEPROM operations with parasite power
 * - Power delivery timing and constraints
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_PARASITE";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS18B20 commands */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBE)
#define DS18B20_CMD_READ_POWER_SUPPLY (0xB4)

/* Check if device is parasite-powered */
static esp_err_t priv_check_parasite_power(star_bus_manager_t *manager,
                                           const char         *bus_name,
                                           star_onewire_rom_t  rom,
                                           bool               *is_parasite)
{
  esp_err_t ret;

  /* Send read power supply command */
  uint8_t cmd = DS18B20_CMD_READ_POWER_SUPPLY;
  ret         = star_bus_onewire_write_bytes(manager, bus_name, rom, &cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read response bit */
  uint8_t bit;
  ret = star_bus_onewire_read_bit(manager, bus_name, &bit);
  if (ret != ESP_OK) {
    return ret;
  }

  /* 0 = parasite power, 1 = external power */
  *is_parasite = (bit == 0);

  return ESP_OK;
}

/* Read temperature with parasite power handling */
static esp_err_t priv_read_temperature_parasite(star_bus_manager_t *manager,
                                                const char         *bus_name,
                                                star_onewire_rom_t  rom,
                                                bool                use_strong_pullup,
                                                float              *temperature)
{
  esp_err_t ret;

  /* Start temperature conversion */
  uint8_t convert_cmd = DS18B20_CMD_CONVERT_T;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &convert_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  /* If using parasite power, enable strong pull-up during conversion */
  if (use_strong_pullup) {
    ESP_LOGI(s_tag, "Strong pull-up enabled during conversion");
    /* The strong pull-up is automatically managed by the driver */
  }

  /* Wait for conversion (750ms for 12-bit) */
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
    ESP_LOGE(s_tag, "CRC mismatch: expected 0x%02X, got 0x%02X", scratchpad[8], crc);
    return ESP_ERR_INVALID_CRC;
  }

  /* Calculate temperature */
  int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
  *temperature     = (float)raw_temp / 16.0f;

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Parasite Power Mode Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "onewire_parasite", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Parasite Power Explained --- */
  ESP_LOGI(s_tag, "--- Parasite Power Mode ---");
  ESP_LOGI(s_tag, "Parasite power allows devices to operate without VDD connection");
  ESP_LOGI(s_tag, "Power is 'stolen' from the data line when it's high");
  ESP_LOGI(s_tag, "Requires strong pull-up for high-current operations");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Power Sources:");
  ESP_LOGI(s_tag, "  1. Normal: VDD pin + data line");
  ESP_LOGI(s_tag, "  2. Parasite: Data line only (VDD to GND)");
  ESP_LOGI(s_tag, "");

  /* Configure with parasite power support */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin              = ONEWIRE_GPIO;
  config.use_parasitic_power   = true;  /* Enable parasite power support */
  config.use_strong_pullup     = true;  /* Enable strong pull-up */

  ret = star_bus_onewire_init(&manager, "onewire0", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire bus initialized on GPIO %d", ONEWIRE_GPIO);
  ESP_LOGI(s_tag, "Parasite power: ENABLED");
  ESP_LOGI(s_tag, "Strong pull-up: ENABLED\n");

  /* Search for devices */
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

  /* Check each device's power mode */
  ESP_LOGI(s_tag, "--- Device Power Detection ---");

  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);

    ESP_LOGI(s_tag, "Device %zu:", i);
    ESP_LOGI(s_tag, "  ROM: 0x%016llX", (unsigned long long)rom_codes[i]);
    ESP_LOGI(s_tag, "  Family: 0x%02X", family);

    /* Check if device supports power detection */
    if (family == STAR_ONEWIRE_FAMILY_DS18B20 || family == STAR_ONEWIRE_FAMILY_DS18S20) {
      bool is_parasite;
      ret = priv_check_parasite_power(&manager, "onewire0", rom_codes[i], &is_parasite);

      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "  Power mode: %s", is_parasite ? "PARASITE" : "EXTERNAL");

        if (is_parasite) {
          ESP_LOGI(s_tag, "  Note: Strong pull-up required for operations");
        }
      } else {
        ESP_LOGW(s_tag, "  Power detection failed");
      }
    } else {
      ESP_LOGI(s_tag, "  Power detection not supported for this family");
    }

    ESP_LOGI(s_tag, "");
  }

  /* --- Strong Pull-Up Timing --- */
  ESP_LOGI(s_tag, "--- Strong Pull-Up Requirements ---");
  ESP_LOGI(s_tag, "┌────────────────────┬──────────────┬─────────────┐");
  ESP_LOGI(s_tag, "│ Operation          │ Current Draw │ Pull-up Time│");
  ESP_LOGI(s_tag, "├────────────────────┼──────────────┼─────────────┤");
  ESP_LOGI(s_tag, "│ Idle               │    ~1 uA     │      -      │");
  ESP_LOGI(s_tag, "│ Temperature Conv.  │  ~1.5 mA     │   750 ms    │");
  ESP_LOGI(s_tag, "│ EEPROM Write       │  ~1.5 mA     │    10 ms    │");
  ESP_LOGI(s_tag, "│ EEPROM Copy        │  ~1.5 mA     │    10 ms    │");
  ESP_LOGI(s_tag, "└────────────────────┴──────────────┴─────────────┘");
  ESP_LOGI(s_tag, "");

  /* Demonstrate temperature reading with parasite power */
  if (device_count > 0) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[0]);

    if (family == STAR_ONEWIRE_FAMILY_DS18B20) {
      ESP_LOGI(s_tag, "--- Temperature Reading Test ---");

      /* Check if parasite-powered */
      bool is_parasite;
      priv_check_parasite_power(&manager, "onewire0", rom_codes[0], &is_parasite);

      ESP_LOGI(s_tag, "Reading temperature from %s device",
               is_parasite ? "parasite-powered" : "externally-powered");

      float temperature;
      ret = priv_read_temperature_parasite(&manager, "onewire0", rom_codes[0], is_parasite, &temperature);

      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Temperature: %.2f C (%.2f F)", temperature, temperature * 9.0f / 5.0f + 32.0f);
        ESP_LOGI(s_tag, "Read successful with %s", is_parasite ? "parasite power" : "external power");
      } else {
        ESP_LOGE(s_tag, "Temperature read failed: %s", esp_err_to_name(ret));
      }

      ESP_LOGI(s_tag, "");
    }
  }

  /* --- Hardware Configuration --- */
  ESP_LOGI(s_tag, "--- Hardware Setup ---");
  ESP_LOGI(s_tag, "Normal Power Configuration:");
  ESP_LOGI(s_tag, "  DS18B20 Pin 1 (GND)  -> Ground");
  ESP_LOGI(s_tag, "  DS18B20 Pin 2 (DQ)   -> GPIO %d + 4.7k pull-up to 3.3V", ONEWIRE_GPIO);
  ESP_LOGI(s_tag, "  DS18B20 Pin 3 (VDD)  -> 3.3V");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Parasite Power Configuration:");
  ESP_LOGI(s_tag, "  DS18B20 Pin 1 (GND)  -> Ground");
  ESP_LOGI(s_tag, "  DS18B20 Pin 2 (DQ)   -> GPIO %d + 4.7k pull-up to 3.3V", ONEWIRE_GPIO);
  ESP_LOGI(s_tag, "  DS18B20 Pin 3 (VDD)  -> Ground (!)");
  ESP_LOGI(s_tag, "  Strong pull-up       -> Via MOSFET or dedicated circuit");
  ESP_LOGI(s_tag, "");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "--- Parasite Power Best Practices ---");
  ESP_LOGI(s_tag, "1. Use external power when possible (more reliable)");
  ESP_LOGI(s_tag, "2. Limit cable length (<30m) for parasite power");
  ESP_LOGI(s_tag, "3. Reduce number of parasite devices on same bus");
  ESP_LOGI(s_tag, "4. Implement strong pull-up with MOSFET");
  ESP_LOGI(s_tag, "5. Add decoupling capacitor near device (optional)");
  ESP_LOGI(s_tag, "6. Test thoroughly in target environment");
  ESP_LOGI(s_tag, "7. Consider temperature limits of operation");
  ESP_LOGI(s_tag, "");

  /* --- Advantages and Disadvantages --- */
  ESP_LOGI(s_tag, "--- Parasite Power Trade-offs ---");
  ESP_LOGI(s_tag, "Advantages:");
  ESP_LOGI(s_tag, "  + Only 2 wires needed (GND + Data)");
  ESP_LOGI(s_tag, "  + Simpler wiring");
  ESP_LOGI(s_tag, "  + Lower component count");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Disadvantages:");
  ESP_LOGI(s_tag, "  - Requires strong pull-up circuit");
  ESP_LOGI(s_tag, "  - Limited cable length");
  ESP_LOGI(s_tag, "  - Fewer devices per bus");
  ESP_LOGI(s_tag, "  - Higher power consumption on data line");
  ESP_LOGI(s_tag, "  - More susceptible to noise");
  ESP_LOGI(s_tag, "");

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

  ESP_LOGI(s_tag, "\nOne-Wire parasite power example complete");
}
