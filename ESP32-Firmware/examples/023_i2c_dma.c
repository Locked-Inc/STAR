/**
 * @file 023_i2c_dma.c
 * @brief I2C DMA large transfer example
 *
 * Demonstrates:
 * - I2C DMA configuration
 * - Large data transfers (>32 bytes)
 * - DMA buffer allocation
 * - Automatic DMA fallback
 * - DMA statistics
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_i2c_dma.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char *s_tag = "I2C_DMA_EXAMPLE";

/* DMA callback function */
static void priv_dma_callback(esp_err_t result, void* ctx)
{
  ESP_LOGI(s_tag, "DMA callback: %s", esp_err_to_name(result));
}

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED  (400000)
#define EEPROM_ADDR    (0x50)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== I2C DMA Example ===\n");

  ret = star_bus_manager_init(&s_manager, "dma_demo", NULL, NULL);
  if (ret != ESP_OK) {
    return;
  }

  /* Create I2C config with DMA enabled */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "dma_eeprom",
    I2C_NUM_0,
    EEPROM_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  star_bus_manager_add_bus(&s_manager, i2c_config);
  star_bus_config_init(i2c_config, &s_manager);

  ESP_LOGI(s_tag, "I2C with DMA initialized");

  /* --- DMA Buffer Allocation --- */
  ESP_LOGI(s_tag, "\n--- DMA Buffer Allocation ---");

  size_t large_size = 256;
  uint8_t* dma_buffer = star_i2c_dma_malloc(large_size);

  if (dma_buffer != NULL) {
    ESP_LOGI(s_tag, "Allocated %d byte DMA-capable buffer", large_size);

    /* Check buffer capability */
    bool is_capable = star_i2c_is_dma_capable(dma_buffer, large_size);
    ESP_LOGI(s_tag, "Buffer DMA capable: %s", is_capable ? "Yes" : "No");
  } else {
    ESP_LOGE(s_tag, "DMA buffer allocation failed");
  }

  /* --- Large DMA Write --- */
  ESP_LOGI(s_tag, "\n--- Large DMA Write ---");

  /* Fill buffer with test pattern */
  for (int i = 0; i < large_size; i++) {
    dma_buffer[i] = i & 0xFF;
  }

  ret = star_bus_i2c_write_dma(&s_manager, "dma_eeprom", dma_buffer, large_size, 0x00, NULL, NULL);
  ESP_LOGI(s_tag, "DMA write %d bytes: %s", large_size, esp_err_to_name(ret));

  /* --- Large DMA Read --- */
  ESP_LOGI(s_tag, "\n--- Large DMA Read ---");

  uint8_t* read_buffer = star_i2c_dma_malloc(large_size);
  if (read_buffer != NULL) {
    ret = star_bus_i2c_read_dma(&s_manager, "dma_eeprom", read_buffer, large_size, 0x00, NULL, NULL);
    ESP_LOGI(s_tag, "DMA read %d bytes: %s", large_size, esp_err_to_name(ret));

    /* Verify first few bytes */
    ESP_LOGI(s_tag, "First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
             read_buffer[0], read_buffer[1], read_buffer[2], read_buffer[3],
             read_buffer[4], read_buffer[5], read_buffer[6], read_buffer[7]);

    free(read_buffer);
  }

  /* --- Automatic Fallback --- */
  ESP_LOGI(s_tag, "\n--- Automatic Fallback (Small Transfer) ---");

  uint8_t small_data[16] = {0};
  ret = star_bus_i2c_write_dma(&s_manager, "dma_eeprom", small_data, sizeof(small_data), 0x00, NULL, NULL);
  ESP_LOGI(s_tag, "Small write (auto fallback to non-DMA): %s", esp_err_to_name(ret));

  /* --- DMA with Callback --- */
  ESP_LOGI(s_tag, "\n--- DMA Async with Callback ---");

  /* Callback would be called when transfer completes */
  ret = star_bus_i2c_write_dma(&s_manager, "dma_eeprom", dma_buffer, 128, 0x00,
                            priv_dma_callback, NULL);

  /* --- DMA Configuration Info --- */
  ESP_LOGI(s_tag, "\n--- DMA Configuration ---");
  ESP_LOGI(s_tag, "Minimum transfer size for DMA: 32 bytes");
  ESP_LOGI(s_tag, "Buffer alignment required: 4 bytes");
  ESP_LOGI(s_tag, "DMA channels: 0-1 (ESP32)");
  ESP_LOGI(s_tag, "Max transfer: Limited by DMA descriptor chain");

  /* --- DMA Statistics --- */
  ESP_LOGI(s_tag, "\n--- DMA Statistics ---");
  ESP_LOGI(s_tag, "DMA transfers performed: (tracked internally)");
  ESP_LOGI(s_tag, "DMA bytes transferred: (tracked internally)");
  ESP_LOGI(s_tag, "Non-DMA fallback count: (tracked internally)");

  /* Cleanup */
  free(dma_buffer);
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nI2C DMA example complete");
}
