/**
 * @file 093_onewire_eeprom_2431.c
 * @brief DS2431 1KB EEPROM operations example
 *
 * Demonstrates:
 * - DS2431 1K EEPROM (128 bytes x 8 pages)
 * - Memory organization (4 rows of 8 bytes per page)
 * - Scratchpad operations
 * - Page write and copy
 * - Memory read
 * - CRC verification
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_DS2431";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS2431 Commands */
#define DS2431_WRITE_SCRATCHPAD (0x0F)
#define DS2431_READ_SCRATCHPAD (0xAA)
#define DS2431_COPY_SCRATCHPAD (0x55)
#define DS2431_READ_MEMORY (0xF0)

/* ROM commands */
#define ROM_CMD_SKIP (0xCC)

/* Memory organization */
#define DS2431_MEMORY_SIZE (128)
#define DS2431_PAGE_SIZE (32)
#define DS2431_ROW_SIZE (8)
#define DS2431_NUM_PAGES (4)

/**
 * @brief Read DS2431 memory
 */
static esp_err_t priv_ds2431_read_memory(star_bus_manager_t *manager,
                                         const char         *bus_name,
                                         uint8_t             address,
                                         uint8_t            *data,
                                         size_t              length)
{
  esp_err_t ret;
  bool      presence;

  /* Reset and presence */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    ESP_LOGE(s_tag, "No device presence");
    return ESP_ERR_NOT_FOUND;
  }

  /* Skip ROM (single device) */
  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);

  /* Read Memory command */
  star_bus_onewire_write_byte(manager, bus_name, DS2431_READ_MEMORY);

  /* Target address (2 bytes: TA1, TA2) */
  star_bus_onewire_write_byte(manager, bus_name, address);       /* TA1 */
  star_bus_onewire_write_byte(manager, bus_name, 0x00);          /* TA2 (always 0 for DS2431) */

  /* Read data bytes */
  for (size_t i = 0; i < length; i++) {
    star_bus_onewire_read_byte(manager, bus_name, &data[i]);
  }

  return ESP_OK;
}

/**
 * @brief Write DS2431 memory (one 8-byte row)
 */
static esp_err_t priv_ds2431_write_row(star_bus_manager_t *manager,
                                       const char         *bus_name,
                                       uint8_t             address,
                                       const uint8_t      *data)
{
  esp_err_t ret;
  bool      presence;
  uint8_t   scratchpad[13];

  /* Validate address (must be row-aligned) */
  if (address % DS2431_ROW_SIZE != 0) {
    ESP_LOGE(s_tag, "Address must be row-aligned (multiple of 8)");
    return ESP_ERR_INVALID_ARG;
  }

  /* Reset and presence */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Skip ROM */
  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);

  /* Write Scratchpad command */
  star_bus_onewire_write_byte(manager, bus_name, DS2431_WRITE_SCRATCHPAD);

  /* Target address */
  star_bus_onewire_write_byte(manager, bus_name, address);
  star_bus_onewire_write_byte(manager, bus_name, 0x00);

  /* Write 8 bytes to scratchpad */
  for (int i = 0; i < DS2431_ROW_SIZE; i++) {
    star_bus_onewire_write_byte(manager, bus_name, data[i]);
  }

  /* Read scratchpad for verification */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_INVALID_STATE;
  }

  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);
  star_bus_onewire_write_byte(manager, bus_name, DS2431_READ_SCRATCHPAD);

  /* Read: TA1, TA2, E/S, Data[8], CRC16 = 13 bytes */
  for (int i = 0; i < 13; i++) {
    star_bus_onewire_read_byte(manager, bus_name, &scratchpad[i]);
  }

  /* Verify CRC16 */
  uint16_t recv_crc = scratchpad[11] | (scratchpad[12] << 8);
  uint16_t calc_crc = star_bus_onewire_crc16(scratchpad, 11);

  if (recv_crc != calc_crc) {
    ESP_LOGE(s_tag, "Scratchpad CRC error: expected 0x%04X, got 0x%04X", calc_crc, recv_crc);
    return ESP_ERR_INVALID_CRC;
  }

  /* Copy scratchpad to memory */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_INVALID_STATE;
  }

  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);
  star_bus_onewire_write_byte(manager, bus_name, DS2431_COPY_SCRATCHPAD);

  /* Authorization code: TA1, TA2, E/S */
  star_bus_onewire_write_byte(manager, bus_name, scratchpad[0]);
  star_bus_onewire_write_byte(manager, bus_name, scratchpad[1]);
  star_bus_onewire_write_byte(manager, bus_name, scratchpad[2]);

  /* Wait for copy to complete (10ms typical) */
  vTaskDelay(pdMS_TO_TICKS(15));

  /* Read copy status (0xAA = success, 0xFF = fail) */
  uint8_t status;
  star_bus_onewire_read_byte(manager, bus_name, &status);

  if (status != 0xAA) {
    ESP_LOGE(s_tag, "Copy failed, status: 0x%02X", status);
    return ESP_FAIL;
  }

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== DS2431 1KB EEPROM Example ===\n");

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

  ESP_LOGI(s_tag, "One-Wire initialized on GPIO %d", ONEWIRE_GPIO);

  /* --- Search for DS2431 --- */
  ESP_LOGI(s_tag, "\n--- Searching for DS2431 ---");

  star_onewire_rom_t rom_codes[8];
  size_t             device_count = 8;

  ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &device_count);
  if (ret != ESP_OK || device_count == 0) {
    ESP_LOGE(s_tag, "No devices found");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Find DS2431 device */
  bool     found = false;
  star_onewire_rom_t ds2431_rom;

  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);
    if (family == STAR_ONEWIRE_FAMILY_DS2431) {
      ds2431_rom = rom_codes[i];
      found      = true;
      ESP_LOGI(s_tag, "Found DS2431: 0x%016llX", (unsigned long long)ds2431_rom);
      break;
    }
  }

  if (!found) {
    ESP_LOGW(s_tag, "DS2431 not found on bus");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* --- Memory Organization --- */
  ESP_LOGI(s_tag, "\n--- DS2431 Memory Organization ---");
  ESP_LOGI(s_tag, "Total memory: %d bytes", DS2431_MEMORY_SIZE);
  ESP_LOGI(s_tag, "Pages: %d x %d bytes", DS2431_NUM_PAGES, DS2431_PAGE_SIZE);
  ESP_LOGI(s_tag, "Rows: 16 x %d bytes (write unit)", DS2431_ROW_SIZE);
  ESP_LOGI(s_tag, "Address range: 0x00 - 0x7F");

  /* --- Read Initial Memory --- */
  ESP_LOGI(s_tag, "\n--- Read Initial Memory (first 32 bytes) ---");

  uint8_t read_buf[32];
  ret = priv_ds2431_read_memory(&manager, "onewire0", 0x00, read_buf, 32);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Memory content:");
    ESP_LOG_BUFFER_HEX_LEVEL(s_tag, read_buf, 32, ESP_LOG_INFO);
  } else {
    ESP_LOGE(s_tag, "Read failed: %s", esp_err_to_name(ret));
  }

  /* --- Write Test Data --- */
  ESP_LOGI(s_tag, "\n--- Write Test Data ---");

  uint8_t write_data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  ESP_LOGI(s_tag, "Writing to address 0x00:");
  ESP_LOG_BUFFER_HEX_LEVEL(s_tag, write_data, 8, ESP_LOG_INFO);

  ret = priv_ds2431_write_row(&manager, "onewire0", 0x00, write_data);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Write successful");
  } else {
    ESP_LOGE(s_tag, "Write failed: %s", esp_err_to_name(ret));
  }

  /* --- Verify Write --- */
  ESP_LOGI(s_tag, "\n--- Verify Write ---");

  uint8_t verify_buf[8];
  ret = priv_ds2431_read_memory(&manager, "onewire0", 0x00, verify_buf, 8);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read back:");
    ESP_LOG_BUFFER_HEX_LEVEL(s_tag, verify_buf, 8, ESP_LOG_INFO);

    if (memcmp(write_data, verify_buf, 8) == 0) {
      ESP_LOGI(s_tag, "Verification: PASS");
    } else {
      ESP_LOGE(s_tag, "Verification: FAIL - Data mismatch");
    }
  }

  /* --- Write Multiple Rows --- */
  ESP_LOGI(s_tag, "\n--- Write Multiple Rows ---");

  for (int row = 0; row < 4; row++) {
    uint8_t row_data[8];
    for (int i = 0; i < 8; i++) {
      row_data[i] = (row * 8) + i;
    }

    uint8_t address = row * DS2431_ROW_SIZE;
    ESP_LOGI(s_tag, "Writing row %d at address 0x%02X", row, address);

    ret = priv_ds2431_write_row(&manager, "onewire0", address, row_data);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "  Row %d: OK", row);
    } else {
      ESP_LOGE(s_tag, "  Row %d: FAILED", row);
    }
  }

  /* --- Read Back Multiple Rows --- */
  ESP_LOGI(s_tag, "\n--- Read Back Multiple Rows ---");

  ret = priv_ds2431_read_memory(&manager, "onewire0", 0x00, read_buf, 32);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "32 bytes from address 0x00:");
    for (int row = 0; row < 4; row++) {
      ESP_LOGI(s_tag, "  Row %d:", row);
      ESP_LOG_BUFFER_HEX_LEVEL(s_tag, &read_buf[row * 8], 8, ESP_LOG_INFO);
    }
  }

  /* --- Write ASCII String --- */
  ESP_LOGI(s_tag, "\n--- Write ASCII String ---");

  const char *message = "ESP32OW!"; /* Exactly 8 chars */
  ESP_LOGI(s_tag, "Writing: '%s'", message);

  ret = priv_ds2431_write_row(&manager, "onewire0", 0x20, (const uint8_t*)message);
  if (ret == ESP_OK) {
    /* Read back */
    char read_str[9] = {0};
    priv_ds2431_read_memory(&manager, "onewire0", 0x20, (uint8_t*)read_str, 8);
    ESP_LOGI(s_tag, "Read back: '%s'", read_str);
  }

  /* --- Application Notes --- */
  ESP_LOGI(s_tag, "\n--- DS2431 Application Notes ---");
  ESP_LOGI(s_tag, "1. Write granularity: 8 bytes (row)");
  ESP_LOGI(s_tag, "2. Address must be row-aligned (0x00, 0x08, 0x10, etc.)");
  ESP_LOGI(s_tag, "3. Write endurance: 50,000 cycles typical");
  ESP_LOGI(s_tag, "4. Data retention: 10 years");
  ESP_LOGI(s_tag, "5. Write time: ~10ms per row");
  ESP_LOGI(s_tag, "6. CRC16 verification on scratchpad operations");
  ESP_LOGI(s_tag, "7. Copy verification byte (0xAA = success)");

  ESP_LOGI(s_tag, "\n--- Use Cases ---");
  ESP_LOGI(s_tag, "- Configuration storage");
  ESP_LOGI(s_tag, "- Calibration data");
  ESP_LOGI(s_tag, "- Serial numbers and identifiers");
  ESP_LOGI(s_tag, "- Small data logging");
  ESP_LOGI(s_tag, "- Product information");

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nDS2431 example complete");
}
