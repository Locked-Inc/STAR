/**
 * @file 089_onewire_strong_pullup.c
 * @brief One-Wire Strong Pull-up Example
 *
 * Demonstrates:
 * - Strong pull-up for power-hungry operations
 * - When to enable/disable strong pull-up
 * - Hardware implementation options
 * - Timing requirements
 * - EEPROM write operations requiring strong pull-up
 * - Temperature conversion with strong pull-up
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_STRONG_PULLUP";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS18B20 commands */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBE)
#define DS18B20_CMD_WRITE_SCRATCHPAD (0x4E)
#define DS18B20_CMD_COPY_SCRATCHPAD (0x48)

/* DS2431 EEPROM commands */
#define DS2431_CMD_WRITE_SCRATCHPAD (0x0F)
#define DS2431_CMD_READ_SCRATCHPAD (0xAA)
#define DS2431_CMD_COPY_SCRATCHPAD (0x55)
#define DS2431_CMD_READ_MEMORY (0xF0)

/* Temperature conversion with strong pull-up demonstration */
static esp_err_t priv_temperature_conversion_demo(star_bus_manager_t *manager,
                                                  const char         *bus_name,
                                             star_onewire_rom_t  rom)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "Starting temperature conversion...");
  ESP_LOGI(s_tag, "This operation requires ~1.5mA during conversion");

  /* Send convert command */
  uint8_t convert_cmd = DS18B20_CMD_CONVERT_T;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &convert_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Strong pull-up is automatically enabled by driver when configured */
  ESP_LOGI(s_tag, "Strong pull-up active during 750ms conversion");

  /* Wait for conversion */
  vTaskDelay(pdMS_TO_TICKS(750));

  ESP_LOGI(s_tag, "Conversion complete, strong pull-up released");

  /* Read result */
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

  /* Calculate temperature */
  int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
  float   temp_c   = (float)raw_temp / 16.0f;

  ESP_LOGI(s_tag, "Temperature: %.2f C", temp_c);

  return ESP_OK;
}

/* EEPROM copy operation with strong pull-up */
static esp_err_t priv_eeprom_copy_demo(star_bus_manager_t *manager,
                                       const char         *bus_name,
                                  star_onewire_rom_t  rom)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "EEPROM copy operation demonstration");
  ESP_LOGI(s_tag, "This requires strong pull-up for ~10ms");

  /* Write data to scratchpad first */
  uint8_t address    = 0x00;
  uint8_t write_data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

  /* Write to scratchpad */
  uint8_t write_cmd[3] = {DS2431_CMD_WRITE_SCRATCHPAD, address, 0x00};
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, write_cmd, 3);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, write_data, 8);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read scratchpad for authorization */
  uint8_t read_sp_cmd = DS2431_CMD_READ_SCRATCHPAD;
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, &read_sp_cmd, 1);
  if (ret != ESP_OK) {
    return ret;
  }

  uint8_t scratchpad[13];
  ret = star_bus_onewire_read_bytes(manager, bus_name, scratchpad, 13);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Copy scratchpad to EEPROM (requires strong pull-up) */
  ESP_LOGI(s_tag, "Starting EEPROM copy with strong pull-up...");

  uint8_t copy_cmd[4] = {DS2431_CMD_COPY_SCRATCHPAD, scratchpad[0], scratchpad[1], scratchpad[2]};
  ret = star_bus_onewire_write_bytes(manager, bus_name, rom, copy_cmd, 4);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Strong pull-up active during copy */
  ESP_LOGI(s_tag, "Strong pull-up active for 10ms during EEPROM write");
  vTaskDelay(pdMS_TO_TICKS(15)); /* Wait for copy */

  ESP_LOGI(s_tag, "EEPROM copy complete");

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Strong Pull-up Example ===\n");

  /* --- Strong Pull-Up Explained --- */
  ESP_LOGI(s_tag, "--- Strong Pull-Up Purpose ---");
  ESP_LOGI(s_tag, "Standard 4.7k pull-up resistor:");
  ESP_LOGI(s_tag, "  - Provides ~0.5mA at 3.3V");
  ESP_LOGI(s_tag, "  - Sufficient for communication");
  ESP_LOGI(s_tag, "  - NOT sufficient for power-hungry operations");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Strong pull-up (typically ~100 ohm):");
  ESP_LOGI(s_tag, "  - Provides ~30mA at 3.3V");
  ESP_LOGI(s_tag, "  - Required for temperature conversion");
  ESP_LOGI(s_tag, "  - Required for EEPROM writes");
  ESP_LOGI(s_tag, "  - Required for parasite-powered devices");
  ESP_LOGI(s_tag, "");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Configure with strong pull-up enabled */
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
  ESP_LOGI(s_tag, "Strong pull-up: ENABLED\n");

  /* --- Hardware Implementation --- */
  ESP_LOGI(s_tag, "--- Strong Pull-Up Hardware Options ---");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Option 1: Simple MOSFET Circuit");
  ESP_LOGI(s_tag, "  GPIO_PULLUP ----+---- MOSFET Gate");
  ESP_LOGI(s_tag, "                  |");
  ESP_LOGI(s_tag, "  3.3V -----100R--+---- MOSFET Drain");
  ESP_LOGI(s_tag, "                  |");
  ESP_LOGI(s_tag, "  DQ_LINE --------+---- MOSFET Source");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Option 2: DS2482-100 I2C to 1-Wire Bridge");
  ESP_LOGI(s_tag, "  - Built-in strong pull-up control");
  ESP_LOGI(s_tag, "  - Automatic timing management");
  ESP_LOGI(s_tag, "  - Recommended for production");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Option 3: Direct GPIO Control");
  ESP_LOGI(s_tag, "  - Software-controlled timing");
  ESP_LOGI(s_tag, "  - Requires careful implementation");
  ESP_LOGI(s_tag, "  - Used in this example");
  ESP_LOGI(s_tag, "");

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

  /* --- Strong Pull-Up Timing --- */
  ESP_LOGI(s_tag, "--- Strong Pull-Up Timing Requirements ---");
  ESP_LOGI(s_tag, "┌────────────────────┬──────────────┬─────────────┐");
  ESP_LOGI(s_tag, "│ Operation          │ Current Req. │ Duration    │");
  ESP_LOGI(s_tag, "├────────────────────┼──────────────┼─────────────┤");
  ESP_LOGI(s_tag, "│ DS18B20 Convert    │   1.5 mA     │   750 ms    │");
  ESP_LOGI(s_tag, "│ DS2431 EEPROM Copy │   1.5 mA     │    10 ms    │");
  ESP_LOGI(s_tag, "│ DS2433 EEPROM Copy │   1.5 mA     │    10 ms    │");
  ESP_LOGI(s_tag, "│ DS1994 Write       │   1.5 mA     │    10 ms    │");
  ESP_LOGI(s_tag, "└────────────────────┴──────────────┴─────────────┘");
  ESP_LOGI(s_tag, "");

  /* Demonstrate operations requiring strong pull-up */
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);

    ESP_LOGI(s_tag, "--- Device %zu (Family: 0x%02X) ---", i, family);

    switch (family) {
      case STAR_ONEWIRE_FAMILY_DS18B20:
      case STAR_ONEWIRE_FAMILY_DS18S20: {
        ESP_LOGI(s_tag, "Temperature sensor detected");
        ret = priv_temperature_conversion_demo(&manager, "onewire0", rom_codes[i]);
        if (ret != ESP_OK) {
          ESP_LOGE(s_tag, "Temperature conversion failed: %s", esp_err_to_name(ret));
        }
        break;
      }

      case STAR_ONEWIRE_FAMILY_DS2431: {
        ESP_LOGI(s_tag, "DS2431 EEPROM detected");
        ret = priv_eeprom_copy_demo(&manager, "onewire0", rom_codes[i]);
        if (ret != ESP_OK) {
          ESP_LOGE(s_tag, "EEPROM operation failed: %s", esp_err_to_name(ret));
        }
        break;
      }

      default:
        ESP_LOGI(s_tag, "Device may not require strong pull-up");
        break;
    }

    ESP_LOGI(s_tag, "");
  }

  /* --- Pull-Up Resistor Selection --- */
  ESP_LOGI(s_tag, "--- Pull-Up Resistor Selection Guide ---");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Standard Pull-Up (always present):");
  ESP_LOGI(s_tag, "  Recommended: 4.7k ohm");
  ESP_LOGI(s_tag, "  Range: 2k - 6k ohm");
  ESP_LOGI(s_tag, "  Purpose: Bus idle state and communication");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Strong Pull-Up (switched):");
  ESP_LOGI(s_tag, "  Recommended: 100 ohm");
  ESP_LOGI(s_tag, "  Range: 50 - 200 ohm");
  ESP_LOGI(s_tag, "  Purpose: Power delivery during operations");
  ESP_LOGI(s_tag, "  Current: ~15-30mA at 3.3V");
  ESP_LOGI(s_tag, "");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "--- Strong Pull-Up Best Practices ---");
  ESP_LOGI(s_tag, "1. Only enable during power-hungry operations");
  ESP_LOGI(s_tag, "2. Disable immediately after operation completes");
  ESP_LOGI(s_tag, "3. Use MOSFET for clean switching");
  ESP_LOGI(s_tag, "4. Add series resistor to limit current");
  ESP_LOGI(s_tag, "5. Consider voltage drop on long cables");
  ESP_LOGI(s_tag, "6. Ensure timing matches device requirements");
  ESP_LOGI(s_tag, "7. Monitor bus current during development");
  ESP_LOGI(s_tag, "");

  /* --- Common Mistakes --- */
  ESP_LOGI(s_tag, "--- Common Mistakes to Avoid ---");
  ESP_LOGI(s_tag, "1. Leaving strong pull-up on continuously");
  ESP_LOGI(s_tag, "   - Wastes power");
  ESP_LOGI(s_tag, "   - Can cause bus contention");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. Using too weak strong pull-up");
  ESP_LOGI(s_tag, "   - Insufficient current for operation");
  ESP_LOGI(s_tag, "   - Parasite-powered devices fail");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Poor MOSFET selection");
  ESP_LOGI(s_tag, "   - Slow switching affects timing");
  ESP_LOGI(s_tag, "   - Use logic-level MOSFETs");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. Incorrect timing");
  ESP_LOGI(s_tag, "   - Enabling too late or disabling too early");
  ESP_LOGI(s_tag, "   - Follow datasheet specifications");
  ESP_LOGI(s_tag, "");

  /* --- Cable Length Impact --- */
  ESP_LOGI(s_tag, "--- Cable Length Considerations ---");
  ESP_LOGI(s_tag, "┌──────────────┬─────────────┬──────────────┐");
  ESP_LOGI(s_tag, "│ Cable Length │ Std Pull-Up │ Strong P-Up  │");
  ESP_LOGI(s_tag, "├──────────────┼─────────────┼──────────────┤");
  ESP_LOGI(s_tag, "│    < 10 m    │   4.7k      │    100 ohm   │");
  ESP_LOGI(s_tag, "│   10-30 m    │   3.3k      │     75 ohm   │");
  ESP_LOGI(s_tag, "│   30-100 m   │   2.2k      │     50 ohm   │");
  ESP_LOGI(s_tag, "│  > 100 m     │   Custom    │    Custom    │");
  ESP_LOGI(s_tag, "└──────────────┴─────────────┴──────────────┘");
  ESP_LOGI(s_tag, "Note: Longer cables require lower resistance");
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

  ESP_LOGI(s_tag, "\nOne-Wire strong pull-up example complete");
}
