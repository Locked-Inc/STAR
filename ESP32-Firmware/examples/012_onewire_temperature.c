/**
 * @file 012_onewire_temperature.c
 * @brief One-Wire DS18B20 temperature sensor example
 *
 * Demonstrates:
 * - DS18B20 temperature conversion
 * - Reading temperature from single device
 * - Reading from multiple devices
 * - Parasitic power mode
 * - Resolution configuration
 */

#include "star_bus_manager.h"
#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ONEWIRE_TEMP_EXAMPLE";

#define ONEWIRE_PIN   (GPIO_NUM_4)
#define BUS_NAME      "onewire0"

/* DS18B20 commands */
#define DS18B20_CMD_CONVERT_T (0x44)
#define DS18B20_CMD_READ_SCRATCH (0xBE)
#define DS18B20_CMD_WRITE_SCRATCH (0x4E)
#define DS18B20_CMD_COPY_SCRATCH (0x48)
#define DS18B20_CMD_RECALL_E2 (0xB8)
#define DS18B20_CMD_READ_POWER (0xB4)

/* ROM commands */
#define ROM_CMD_SEARCH (0xF0)
#define ROM_CMD_READ (0x33)
#define ROM_CMD_MATCH (0x55)
#define ROM_CMD_SKIP (0xCC)
#define ROM_CMD_ALARM (0xEC)

static star_bus_manager_t s_manager;

/* Read temperature from a specific device */
static esp_err_t read_temperature(uint64_t rom, float* temp_c)
{
  esp_err_t ret;
  bool presence;
  uint8_t scratchpad[9];

  /* Reset bus */
  ret = star_bus_onewire_reset(&s_manager, BUS_NAME, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Match ROM - select specific device */
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, ROM_CMD_MATCH);

  /* Send ROM bytes (LSB first) */
  uint8_t rom_bytes[8];
  star_bus_onewire_rom_to_bytes(rom, rom_bytes);
  for (int i = 0; i < 8; i++) {
    star_bus_onewire_write_byte(&s_manager, BUS_NAME, rom_bytes[i]);
  }

  /* Start temperature conversion */
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, DS18B20_CMD_CONVERT_T);

  /* Wait for conversion (750ms max for 12-bit resolution) */
  vTaskDelay(pdMS_TO_TICKS(750));

  /* Reset and read scratchpad */
  ret = star_bus_onewire_reset(&s_manager, BUS_NAME, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Match ROM again */
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, ROM_CMD_MATCH);
  for (int i = 0; i < 8; i++) {
    star_bus_onewire_write_byte(&s_manager, BUS_NAME, rom_bytes[i]);
  }

  /* Read scratchpad command */
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, DS18B20_CMD_READ_SCRATCH);

  /* Read 9 bytes of scratchpad */
  for (int i = 0; i < 9; i++) {
    star_bus_onewire_read_byte(&s_manager, BUS_NAME, &scratchpad[i]);
  }

  /* Verify CRC */
  uint8_t crc = star_bus_onewire_crc8(scratchpad, 8);
  if (crc != scratchpad[8]) {
    ESP_LOGE(s_tag, "CRC mismatch: calculated 0x%02X, read 0x%02X", crc, scratchpad[8]);
    return ESP_ERR_INVALID_CRC;
  }

  /* Convert temperature */
  int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
  *temp_c = raw / 16.0f;

  return ESP_OK;
}

/* Read temperature from all devices on bus (skip ROM) */
static esp_err_t read_temperature_single(float* temp_c)
{
  esp_err_t ret;
  bool presence;
  uint8_t scratchpad[9];

  /* Reset bus */
  ret = star_bus_onewire_reset(&s_manager, BUS_NAME, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Skip ROM - address all devices */
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, ROM_CMD_SKIP);

  /* Start temperature conversion */
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, DS18B20_CMD_CONVERT_T);

  /* Wait for conversion */
  vTaskDelay(pdMS_TO_TICKS(750));

  /* Reset and read */
  ret = star_bus_onewire_reset(&s_manager, BUS_NAME, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_INVALID_STATE;
  }

  star_bus_onewire_write_byte(&s_manager, BUS_NAME, ROM_CMD_SKIP);
  star_bus_onewire_write_byte(&s_manager, BUS_NAME, DS18B20_CMD_READ_SCRATCH);

  /* Read scratchpad */
  for (int i = 0; i < 9; i++) {
    star_bus_onewire_read_byte(&s_manager, BUS_NAME, &scratchpad[i]);
  }

  /* Verify CRC */
  uint8_t crc = star_bus_onewire_crc8(scratchpad, 8);
  if (crc != scratchpad[8]) {
    return ESP_ERR_INVALID_CRC;
  }

  /* Convert temperature */
  int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
  *temp_c = raw / 16.0f;

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== One-Wire Temperature Example ===\n");

  /* --- Initialize Bus Manager --- */
  ret = star_bus_manager_init(&s_manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Initialize One-Wire */
  star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
  config.gpio_pin = ONEWIRE_PIN;

  ret = star_bus_onewire_init(&s_manager, BUS_NAME, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init One-Wire: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&s_manager);
    return;
  }

  ESP_LOGI(s_tag, "One-Wire initialized on GPIO %d", ONEWIRE_PIN);

  /* --- Search for Temperature Sensors --- */
  ESP_LOGI(s_tag, "\n--- Searching for DS18B20 Sensors ---");

  star_onewire_rom_t devices[8];
  size_t device_count = 8;

  ret = star_bus_onewire_search(&s_manager, BUS_NAME, devices, &device_count);
  if (ret != ESP_OK || device_count == 0) {
    ESP_LOGW(s_tag, "No devices found");

    /* Try reading single device anyway */
    float temp;
    ret = read_temperature_single(&temp);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Temperature: %.2f C", temp);
    }

    star_bus_onewire_deinit(&s_manager, BUS_NAME);
    star_bus_manager_deinit(&s_manager);
    return;
  }

  ESP_LOGI(s_tag, "Found %zu device(s)", device_count);

  /* Filter for DS18B20 devices */
  star_onewire_rom_t temp_sensors[8];
  size_t temp_count = 0;

  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(devices[i]);
    if (family == STAR_ONEWIRE_FAMILY_DS18B20 ||
        family == STAR_ONEWIRE_FAMILY_DS18S20 ||
        family == STAR_ONEWIRE_FAMILY_DS1822) {
      temp_sensors[temp_count++] = devices[i];
      ESP_LOGI(s_tag, "Temperature sensor [%zu]: 0x%016llX",
               temp_count - 1, (unsigned long long)devices[i]);
    }
  }

  if (temp_count == 0) {
    ESP_LOGW(s_tag, "No temperature sensors found");
    star_bus_onewire_deinit(&s_manager, BUS_NAME);
    star_bus_manager_deinit(&s_manager);
    return;
  }

  /* --- Read Temperature from Each Sensor --- */
  ESP_LOGI(s_tag, "\n--- Reading Temperatures ---");

  for (size_t i = 0; i < temp_count; i++) {
    float temp;
    ret = read_temperature(temp_sensors[i], &temp);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Sensor %zu: %.2f C (%.2f F)",
               i, temp, temp * 9.0f / 5.0f + 32.0f);
    } else {
      ESP_LOGE(s_tag, "Sensor %zu: read failed (%s)", i, esp_err_to_name(ret));
    }
  }

  /* --- Continuous Reading --- */
  ESP_LOGI(s_tag, "\n--- Continuous Reading (5 samples) ---");

  for (int sample = 0; sample < 5; sample++) {
    float temp;
    ret = read_temperature_single(&temp);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Sample %d: %.2f C", sample + 1, temp);
    } else {
      ESP_LOGE(s_tag, "Sample %d: failed", sample + 1);
    }

    if (sample < 4) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  /* --- Parasitic Power Mode --- */
  ESP_LOGI(s_tag, "\n--- Parasitic Power Mode ---");

  /* Deinit and reinit with parasitic power */
  star_bus_onewire_deinit(&s_manager, BUS_NAME);

  config.use_parasitic_power = true;
  config.use_strong_pullup = true;

  ret = star_bus_onewire_init(&s_manager, BUS_NAME, &config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Parasitic power mode enabled");
    ESP_LOGI(s_tag, "Note: Strong pullup applied during temperature conversion");

    float temp;
    ret = read_temperature_single(&temp);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Temperature (parasitic): %.2f C", temp);
    }
  }

  /* --- Temperature Resolution Info --- */
  ESP_LOGI(s_tag, "\n--- Resolution Information ---");
  ESP_LOGI(s_tag, "DS18B20 resolution settings:");
  ESP_LOGI(s_tag, "  9-bit:  0.5 C,    93.75 ms conversion");
  ESP_LOGI(s_tag, "  10-bit: 0.25 C,   187.5 ms conversion");
  ESP_LOGI(s_tag, "  11-bit: 0.125 C,  375 ms conversion");
  ESP_LOGI(s_tag, "  12-bit: 0.0625 C, 750 ms conversion (default)");

  /* Cleanup */
  star_bus_onewire_deinit(&s_manager, BUS_NAME);
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nOne-Wire temperature example complete");
}
