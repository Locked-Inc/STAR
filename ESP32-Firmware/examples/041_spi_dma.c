/**
 * @file 041_spi_dma.c
 * @brief SPI DMA transfers with large data buffers
 *
 * Demonstrates:
 * - DMA channel configuration (AUTO, CH1, CH2)
 * - Large buffer allocation and transfers
 * - DMA vs non-DMA performance comparison
 * - Proper buffer alignment for DMA
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_DMA_EXAMPLE";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/* DMA requires 4-byte aligned buffers */
#define DMA_BUFFER_SIZE (4096)

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI DMA Transfer Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_dma_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* --- Configuration 1: DMA AUTO (recommended) --- */
  ESP_LOGI(s_tag, "--- DMA AUTO Configuration ---");

  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
    .flags = SPI_DEVICE_HALFDUPLEX,  /* Enable half-duplex mode for large transfers */
  };

  star_bus_config_t *spi_dma_auto = star_bus_config_create_spi_device(
    "dma_auto_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,  /* Let ESP-IDF choose DMA channel */
    &dev_cfg
  );

  if (spi_dma_auto == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI DMA AUTO config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, spi_dma_auto);
  ret = star_bus_config_init(spi_dma_auto, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init DMA AUTO: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "DMA AUTO initialized successfully");

  /* --- Allocate DMA-capable buffers --- */
  /* For ESP32, DMA buffers must be in DMA-capable memory (internal RAM) */
  uint8_t *tx_buffer = (uint8_t *)heap_caps_malloc(DMA_BUFFER_SIZE, MALLOC_CAP_DMA);
  uint8_t *rx_buffer = (uint8_t *)heap_caps_malloc(DMA_BUFFER_SIZE, MALLOC_CAP_DMA);

  if (tx_buffer == NULL || rx_buffer == NULL) {
    ESP_LOGE(s_tag, "Failed to allocate DMA buffers");
    if (tx_buffer) heap_caps_free(tx_buffer);
    if (rx_buffer) heap_caps_free(rx_buffer);
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Allocated %d bytes for TX/RX buffers (DMA-capable)", DMA_BUFFER_SIZE);

  /* Fill TX buffer with test pattern */
  for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
    tx_buffer[i] = (uint8_t)(i & 0xFF);
  }

  /* --- Large DMA Transfer --- */
  ESP_LOGI(s_tag, "\n--- Performing Large DMA Transfer (%d bytes) ---", DMA_BUFFER_SIZE);

  int64_t start_time = esp_timer_get_time();

  ret = star_bus_spi_transfer(&manager, "dma_auto_device",
                               tx_buffer, rx_buffer, DMA_BUFFER_SIZE, 1000);

  int64_t end_time = esp_timer_get_time();
  int64_t elapsed_us = end_time - start_time;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "DMA transfer complete:");
    ESP_LOGI(s_tag, "  Size: %d bytes", DMA_BUFFER_SIZE);
    ESP_LOGI(s_tag, "  Time: %lld us", elapsed_us);
    ESP_LOGI(s_tag, "  Speed: %.2f KB/s", (DMA_BUFFER_SIZE / 1024.0) / (elapsed_us / 1000000.0));

    /* Verify first few bytes */
    ESP_LOGI(s_tag, "  RX data (first 8 bytes): %02X %02X %02X %02X %02X %02X %02X %02X",
             rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3],
             rx_buffer[4], rx_buffer[5], rx_buffer[6], rx_buffer[7]);
  } else {
    ESP_LOGE(s_tag, "DMA transfer failed: %s", esp_err_to_name(ret));
  }

  /* --- Multiple smaller transfers --- */
  ESP_LOGI(s_tag, "\n--- Multiple Small DMA Transfers ---");

  #define SMALL_TRANSFER_SIZE (256)
  #define NUM_TRANSFERS (10)

  start_time = esp_timer_get_time();

  for (int i = 0; i < NUM_TRANSFERS; i++) {
    ret = star_bus_spi_transmit(&manager, "dma_auto_device",
                                 tx_buffer, SMALL_TRANSFER_SIZE, 100);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Transfer %d failed: %s", i, esp_err_to_name(ret));
      break;
    }
  }

  end_time = esp_timer_get_time();
  elapsed_us = end_time - start_time;

  int total_bytes = SMALL_TRANSFER_SIZE * NUM_TRANSFERS;
  ESP_LOGI(s_tag, "Completed %d transfers (%d bytes total)", NUM_TRANSFERS, total_bytes);
  ESP_LOGI(s_tag, "  Total time: %lld us", elapsed_us);
  ESP_LOGI(s_tag, "  Avg per transfer: %lld us", elapsed_us / NUM_TRANSFERS);

  /* --- Configuration 2: Explicit DMA Channel --- */
  ESP_LOGI(s_tag, "\n--- Explicit DMA Channel (CH1) ---");

  spi_device_interface_config_t dev_cfg_ch1 = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = GPIO_NUM_15,  /* Different CS */
    .queue_size = 7,
  };

  star_bus_config_t *spi_dma_ch1 = star_bus_config_create_spi_device(
    "dma_ch1_device",
    SPI3_HOST,  /* Use different SPI host */
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH1,  /* Explicit channel 1 */
    &dev_cfg_ch1
  );

  if (spi_dma_ch1 != NULL) {
    star_bus_manager_add_bus(&manager, spi_dma_ch1);
    ret = star_bus_config_init(spi_dma_ch1, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "DMA CH1 device initialized on SPI3_HOST");

      /* Test transfer */
      ret = star_bus_spi_transmit(&manager, "dma_ch1_device",
                                   tx_buffer, 512, 100);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "DMA CH1 transfer successful");
      }
    } else {
      ESP_LOGW(s_tag, "DMA CH1 init failed (may be in use): %s", esp_err_to_name(ret));
    }
  }

  /* --- DMA Best Practices --- */
  ESP_LOGI(s_tag, "\n--- DMA Best Practices ---");
  ESP_LOGI(s_tag, "1. Use heap_caps_malloc(size, MALLOC_CAP_DMA) for buffers");
  ESP_LOGI(s_tag, "2. Ensure 4-byte alignment for optimal performance");
  ESP_LOGI(s_tag, "3. Use SPI_DMA_CH_AUTO for automatic channel selection");
  ESP_LOGI(s_tag, "4. DMA is beneficial for transfers > 32 bytes");
  ESP_LOGI(s_tag, "5. Maximum DMA transfer size: 4092 bytes per transaction");
  ESP_LOGI(s_tag, "6. Use half-duplex mode for very large transfers");

  /* Cleanup */
  heap_caps_free(tx_buffer);
  heap_caps_free(rx_buffer);
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI DMA example complete");
}
