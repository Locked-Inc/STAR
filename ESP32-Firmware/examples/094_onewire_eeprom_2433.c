/**
 * @file 094_onewire_eeprom_2433.c
 * @brief DS2433 4KB EEPROM operations example
 *
 * Demonstrates:
 * - DS2433 4K EEPROM (512 bytes)
 * - Page-based organization (16 pages x 32 bytes)
 * - Scratchpad operations with 32-byte capacity
 * - Page write with 8-byte row granularity
 * - Sequential reading
 * - Large data storage
 */

#include "star_bus_onewire.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ONEWIRE_DS2433";

#define ONEWIRE_GPIO (GPIO_NUM_4)

/* DS2433 Commands */
#define DS2433_WRITE_SCRATCHPAD (0x0F)
#define DS2433_READ_SCRATCHPAD (0xAA)
#define DS2433_COPY_SCRATCHPAD (0x55)
#define DS2433_READ_MEMORY (0xF0)

/* ROM commands */
#define ROM_CMD_SKIP (0xCC)

/* Memory organization */
#define DS2433_MEMORY_SIZE (512)
#define DS2433_PAGE_SIZE (32)
#define DS2433_ROW_SIZE (8)
#define DS2433_NUM_PAGES (16)
#define DS2433_SCRATCHPAD_SIZE (32)

/**
 * @brief Read DS2433 memory
 */
static esp_err_t priv_ds2433_read_memory(star_bus_manager_t *manager,
                                         const char         *bus_name,
                                         uint16_t            address,
                                         uint8_t            *data,
                                         size_t              length)
{
  esp_err_t ret;
  bool      presence;

  /* Validate address */
  if (address + length > DS2433_MEMORY_SIZE) {
    ESP_LOGE(s_tag, "Read exceeds memory size");
    return ESP_ERR_INVALID_ARG;
  }

  /* Reset and presence */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Skip ROM */
  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);

  /* Read Memory command */
  star_bus_onewire_write_byte(manager, bus_name, DS2433_READ_MEMORY);

  /* Target address (2 bytes: TA1, TA2) */
  star_bus_onewire_write_byte(manager, bus_name, address & 0xFF);         /* TA1 */
  star_bus_onewire_write_byte(manager, bus_name, (address >> 8) & 0xFF);  /* TA2 */

  /* Read data bytes */
  for (size_t i = 0; i < length; i++) {
    star_bus_onewire_read_byte(manager, bus_name, &data[i]);
  }

  return ESP_OK;
}

/**
 * @brief Write DS2433 page (up to 32 bytes, in 8-byte rows)
 */
static esp_err_t priv_ds2433_write_page(star_bus_manager_t *manager,
                                        const char         *bus_name,
                                        uint16_t            address,
                                        const uint8_t      *data,
                                        size_t              length)
{
  esp_err_t ret;
  bool      presence;
  uint8_t   scratchpad[35];

  /* Validate parameters */
  if (length == 0 || length > DS2433_SCRATCHPAD_SIZE) {
    ESP_LOGE(s_tag, "Invalid write length: %zu (max 32)", length);
    return ESP_ERR_INVALID_ARG;
  }

  /* Address must be row-aligned */
  if (address % DS2433_ROW_SIZE != 0) {
    ESP_LOGE(s_tag, "Address 0x%03X not row-aligned", address);
    return ESP_ERR_INVALID_ARG;
  }

  /* Length must be multiple of 8 bytes */
  if (length % DS2433_ROW_SIZE != 0) {
    ESP_LOGE(s_tag, "Length must be multiple of 8");
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
  star_bus_onewire_write_byte(manager, bus_name, DS2433_WRITE_SCRATCHPAD);

  /* Target address */
  star_bus_onewire_write_byte(manager, bus_name, address & 0xFF);
  star_bus_onewire_write_byte(manager, bus_name, (address >> 8) & 0xFF);

  /* Write data to scratchpad */
  for (size_t i = 0; i < length; i++) {
    star_bus_onewire_write_byte(manager, bus_name, data[i]);
  }

  /* Read scratchpad for verification */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_INVALID_STATE;
  }

  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);
  star_bus_onewire_write_byte(manager, bus_name, DS2433_READ_SCRATCHPAD);

  /* Read: TA1, TA2, E/S, Data[length], CRC16 = length + 5 bytes */
  size_t scratchpad_len = length + 5;
  for (size_t i = 0; i < scratchpad_len; i++) {
    star_bus_onewire_read_byte(manager, bus_name, &scratchpad[i]);
  }

  /* Verify CRC16 */
  uint16_t recv_crc =
    scratchpad[scratchpad_len - 2] | (scratchpad[scratchpad_len - 1] << 8);
  uint16_t calc_crc = star_bus_onewire_crc16(scratchpad, scratchpad_len - 2);

  if (recv_crc != calc_crc) {
    ESP_LOGE(s_tag, "Scratchpad CRC error");
    return ESP_ERR_INVALID_CRC;
  }

  /* Copy scratchpad to memory */
  ret = star_bus_onewire_reset(manager, bus_name, &presence);
  if (ret != ESP_OK || !presence) {
    return ESP_ERR_INVALID_STATE;
  }

  star_bus_onewire_write_byte(manager, bus_name, ROM_CMD_SKIP);
  star_bus_onewire_write_byte(manager, bus_name, DS2433_COPY_SCRATCHPAD);

  /* Authorization code: TA1, TA2, E/S */
  star_bus_onewire_write_byte(manager, bus_name, scratchpad[0]);
  star_bus_onewire_write_byte(manager, bus_name, scratchpad[1]);
  star_bus_onewire_write_byte(manager, bus_name, scratchpad[2]);

  /* Wait for copy to complete (10ms + 2ms per byte) */
  uint32_t copy_time_ms = 10 + (length * 2);
  vTaskDelay(pdMS_TO_TICKS(copy_time_ms));

  /* Read copy status */
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

  ESP_LOGI(s_tag, "=== DS2433 4KB EEPROM Example ===\n");

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

  /* --- Search for DS2433 --- */
  ESP_LOGI(s_tag, "\n--- Searching for DS2433 ---");

  star_onewire_rom_t rom_codes[8];
  size_t             device_count = 8;

  ret = star_bus_onewire_search(&manager, "onewire0", rom_codes, &device_count);
  if (ret != ESP_OK || device_count == 0) {
    ESP_LOGE(s_tag, "No devices found");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Find DS2433 device */
  bool found = false;
  for (size_t i = 0; i < device_count; i++) {
    uint8_t family = star_bus_onewire_get_family(rom_codes[i]);
    if (family == STAR_ONEWIRE_FAMILY_DS2433) {
      found = true;
      ESP_LOGI(s_tag, "Found DS2433: 0x%016llX", (unsigned long long)rom_codes[i]);
      break;
    }
  }

  if (!found) {
    ESP_LOGW(s_tag, "DS2433 not found on bus");
    star_bus_onewire_deinit(&manager, "onewire0");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* --- Memory Organization --- */
  ESP_LOGI(s_tag, "\n--- DS2433 Memory Organization ---");
  ESP_LOGI(s_tag, "Total memory: %d bytes", DS2433_MEMORY_SIZE);
  ESP_LOGI(s_tag, "Pages: %d x %d bytes", DS2433_NUM_PAGES, DS2433_PAGE_SIZE);
  ESP_LOGI(s_tag, "Scratchpad: %d bytes", DS2433_SCRATCHPAD_SIZE);
  ESP_LOGI(s_tag, "Write unit: %d bytes (row)", DS2433_ROW_SIZE);
  ESP_LOGI(s_tag, "Address range: 0x000 - 0x1FF");

  /* --- Read Initial Memory --- */
  ESP_LOGI(s_tag, "\n--- Read Page 0 (first 32 bytes) ---");

  uint8_t read_buf[64];
  ret = priv_ds2433_read_memory(&manager, "onewire0", 0x000, read_buf, 32);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Page 0 content:");
    ESP_LOG_BUFFER_HEX_LEVEL(s_tag, read_buf, 32, ESP_LOG_INFO);
  } else {
    ESP_LOGE(s_tag, "Read failed: %s", esp_err_to_name(ret));
  }

  /* --- Write 8-byte Row --- */
  ESP_LOGI(s_tag, "\n--- Write 8-byte Row ---");

  uint8_t row_data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  ESP_LOGI(s_tag, "Writing 8 bytes to address 0x000:");
  ESP_LOG_BUFFER_HEX_LEVEL(s_tag, row_data, 8, ESP_LOG_INFO);

  ret = priv_ds2433_write_page(&manager, "onewire0", 0x000, row_data, 8);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Write successful");

    /* Verify */
    priv_ds2433_read_memory(&manager, "onewire0", 0x000, read_buf, 8);
    if (memcmp(row_data, read_buf, 8) == 0) {
      ESP_LOGI(s_tag, "Verification: PASS");
    }
  } else {
    ESP_LOGE(s_tag, "Write failed: %s", esp_err_to_name(ret));
  }

  /* --- Write 32-byte Page --- */
  ESP_LOGI(s_tag, "\n--- Write 32-byte Page ---");

  uint8_t page_data[32];
  for (int i = 0; i < 32; i++) {
    page_data[i] = i;
  }

  ESP_LOGI(s_tag, "Writing full page (32 bytes) to address 0x020");

  ret = priv_ds2433_write_page(&manager, "onewire0", 0x020, page_data, 32);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Page write successful");

    /* Read back and verify */
    priv_ds2433_read_memory(&manager, "onewire0", 0x020, read_buf, 32);
    if (memcmp(page_data, read_buf, 32) == 0) {
      ESP_LOGI(s_tag, "Verification: PASS");
    }
  } else {
    ESP_LOGE(s_tag, "Page write failed: %s", esp_err_to_name(ret));
  }

  /* --- Write ASCII Text --- */
  ESP_LOGI(s_tag, "\n--- Write ASCII Text ---");

  const char *text = "ESP32 One-Wire DS2433 Test!"; /* 28 chars, pad to 32 */
  uint8_t     text_buf[32];
  memset(text_buf, 0x20, 32);                         /* Fill with spaces */
  memcpy(text_buf, text, strlen(text));

  ESP_LOGI(s_tag, "Writing text: '%s'", text);

  ret = priv_ds2433_write_page(&manager, "onewire0", 0x040, text_buf, 32);
  if (ret == ESP_OK) {
    /* Read back */
    char read_text[33] = {0};
    priv_ds2433_read_memory(&manager, "onewire0", 0x040, (uint8_t*)read_text, 32);
    read_text[32] = '\0';
    ESP_LOGI(s_tag, "Read back: '%s'", read_text);
  }

  /* --- Sequential Read --- */
  ESP_LOGI(s_tag, "\n--- Sequential Read (64 bytes) ---");

  ret = priv_ds2433_read_memory(&manager, "onewire0", 0x000, read_buf, 64);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "64 bytes from address 0x000:");
    for (int i = 0; i < 2; i++) {
      ESP_LOGI(s_tag, "  Page %d (offset 0x%03X):", i, i * 32);
      ESP_LOG_BUFFER_HEX_LEVEL(s_tag, &read_buf[i * 32], 32, ESP_LOG_INFO);
    }
  }

  /* --- Performance Test --- */
  ESP_LOGI(s_tag, "\n--- Performance Test ---");

  uint32_t start_time = xTaskGetTickCount();

  /* Write 5 pages */
  for (int page = 0; page < 5; page++) {
    uint8_t  test_data[32];
    uint16_t addr = page * 32;

    for (int i = 0; i < 32; i++) {
      test_data[i] = (page * 32 + i) & 0xFF;
    }

    priv_ds2433_write_page(&manager, "onewire0", addr, test_data, 32);
  }

  uint32_t write_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
  ESP_LOGI(s_tag, "Write time (160 bytes): %lu ms", (unsigned long)write_time);
  ESP_LOGI(s_tag, "Average: %.1f ms per page", write_time / 5.0f);

  /* --- Application Notes --- */
  ESP_LOGI(s_tag, "\n--- DS2433 vs DS2431 Comparison ---");
  ESP_LOGI(s_tag, "DS2433: 512 bytes, 32-byte scratchpad");
  ESP_LOGI(s_tag, "DS2431: 128 bytes, 8-byte scratchpad");
  ESP_LOGI(s_tag, "DS2433 allows faster page writes (4x capacity per write)");

  ESP_LOGI(s_tag, "\n--- Use Cases ---");
  ESP_LOGI(s_tag, "- Data logging (timestamps, sensor data)");
  ESP_LOGI(s_tag, "- Configuration storage");
  ESP_LOGI(s_tag, "- Firmware updates (small bootloader configs)");
  ESP_LOGI(s_tag, "- Manufacturing data");
  ESP_LOGI(s_tag, "- Event history");
  ESP_LOGI(s_tag, "- Multi-parameter calibration tables");

  /* Cleanup */
  star_bus_onewire_deinit(&manager, "onewire0");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nDS2433 example complete");
}
