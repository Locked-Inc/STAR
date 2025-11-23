/**
 * @file 054_spi_max_transfer.c
 * @brief Maximum SPI transfer size limits and handling
 *
 * Demonstrates:
 * - ESP32 SPI maximum transfer limits
 * - DMA transfer size constraints
 * - Chunking large transfers
 * - Non-DMA vs DMA limits
 * - Buffer size optimization
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_MAX_TRANSFER";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/* ESP32 SPI transfer limits */
#define SPI_MAX_DMA_LEN         (4092)   /* Maximum DMA transfer (bytes) */
#define SPI_MAX_NON_DMA_LEN     (64)     /* Recommended max for non-DMA (bytes) */

/**
 * @brief Test maximum single transfer size
 */
static void priv_test_max_single_transfer(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Maximum Single Transfer Size ===\n");

  ESP_LOGI(s_tag, "ESP32 SPI Transfer Limits:");
  ESP_LOGI(s_tag, "  DMA mode:     %d bytes (hardware limit)", SPI_MAX_DMA_LEN);
  ESP_LOGI(s_tag, "  Non-DMA mode: %d bytes (recommended)", SPI_MAX_NON_DMA_LEN);
  ESP_LOGI(s_tag, "");

  /* Test maximum DMA transfer */
  ESP_LOGI(s_tag, "Testing maximum DMA transfer (%d bytes)...", SPI_MAX_DMA_LEN);

  uint8_t *tx_buf = (uint8_t*)heap_caps_malloc(SPI_MAX_DMA_LEN, MALLOC_CAP_DMA);
  uint8_t *rx_buf = (uint8_t*)heap_caps_malloc(SPI_MAX_DMA_LEN, MALLOC_CAP_DMA);

  if (tx_buf == NULL || rx_buf == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    if (tx_buf) heap_caps_free(tx_buf);
    if (rx_buf) heap_caps_free(rx_buf);
    return;
  }

  /* Fill with test pattern */
  for (int i = 0; i < SPI_MAX_DMA_LEN; i++) {
    tx_buf[i] = (uint8_t)(i & 0xFF);
  }

  int64_t start_time = esp_timer_get_time();

  esp_err_t ret = star_bus_spi_transfer(manager, "dma_device",
                                        tx_buf, rx_buf,
                                        SPI_MAX_DMA_LEN, 5000);

  int64_t elapsed_us = esp_timer_get_time() - start_time;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SUCCESS: Transferred %d bytes in %lld us",
             SPI_MAX_DMA_LEN, elapsed_us);
    ESP_LOGI(s_tag, "First 4 RX bytes: %02X %02X %02X %02X",
             rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
    ESP_LOGI(s_tag, "Last 4 RX bytes:  %02X %02X %02X %02X",
             rx_buf[SPI_MAX_DMA_LEN-4], rx_buf[SPI_MAX_DMA_LEN-3],
             rx_buf[SPI_MAX_DMA_LEN-2], rx_buf[SPI_MAX_DMA_LEN-1]);
  } else {
    ESP_LOGE(s_tag, "FAILED: %s", esp_err_to_name(ret));
  }

  heap_caps_free(tx_buf);
  heap_caps_free(rx_buf);
}

/**
 * @brief Test transfer size that exceeds limit
 */
static void priv_test_oversized_transfer(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Oversized Transfer Test ===\n");

  const size_t oversized = SPI_MAX_DMA_LEN + 1000;

  ESP_LOGI(s_tag, "Attempting transfer of %zu bytes (exceeds limit)...", oversized);

  uint8_t *tx_buf = (uint8_t*)heap_caps_malloc(oversized, MALLOC_CAP_DMA);
  uint8_t *rx_buf = (uint8_t*)heap_caps_malloc(oversized, MALLOC_CAP_DMA);

  if (tx_buf == NULL || rx_buf == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    if (tx_buf) heap_caps_free(tx_buf);
    if (rx_buf) heap_caps_free(rx_buf);
    return;
  }

  memset(tx_buf, 0xAA, oversized);

  esp_err_t ret = star_bus_spi_transfer(manager, "dma_device",
                                        tx_buf, rx_buf,
                                        oversized, 5000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transfer succeeded (driver may have chunked it)");
  } else {
    ESP_LOGI(s_tag, "Transfer failed as expected: %s", esp_err_to_name(ret));
    ESP_LOGI(s_tag, "This is normal - size exceeds hardware limit");
  }

  heap_caps_free(tx_buf);
  heap_caps_free(rx_buf);
}

/**
 * @brief Demonstrate chunking for large transfers
 */
static void priv_demonstrate_chunking(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Large Transfer Chunking ===\n");

  const size_t total_size = 16384;  /* 16 KB total */
  const size_t chunk_size = SPI_MAX_DMA_LEN;
  const int num_chunks = (total_size + chunk_size - 1) / chunk_size;

  ESP_LOGI(s_tag, "Transferring %zu bytes in %d chunks of %zu bytes...",
           total_size, num_chunks, chunk_size);

  uint8_t *tx_buf = (uint8_t*)heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
  uint8_t *rx_buf = (uint8_t*)heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);

  if (tx_buf == NULL || rx_buf == NULL) {
    ESP_LOGE(s_tag, "Chunk buffer allocation failed");
    if (tx_buf) heap_caps_free(tx_buf);
    if (rx_buf) heap_caps_free(rx_buf);
    return;
  }

  int64_t start_time = esp_timer_get_time();
  size_t total_transferred = 0;
  int success_count = 0;

  for (int chunk = 0; chunk < num_chunks; chunk++) {
    /* Calculate this chunk's size */
    size_t remaining = total_size - total_transferred;
    size_t this_chunk = (remaining > chunk_size) ? chunk_size : remaining;

    /* Fill chunk with pattern */
    for (size_t i = 0; i < this_chunk; i++) {
      tx_buf[i] = (uint8_t)((total_transferred + i) & 0xFF);
    }

    /* Transfer chunk */
    esp_err_t ret = star_bus_spi_transfer(manager, "dma_device",
                                          tx_buf, rx_buf,
                                          this_chunk, 5000);

    if (ret == ESP_OK) {
      total_transferred += this_chunk;
      success_count++;
    } else {
      ESP_LOGE(s_tag, "Chunk %d failed: %s", chunk, esp_err_to_name(ret));
      break;
    }
  }

  int64_t elapsed_us = esp_timer_get_time() - start_time;

  ESP_LOGI(s_tag, "\nChunking Results:");
  ESP_LOGI(s_tag, "  Total size:    %zu bytes", total_size);
  ESP_LOGI(s_tag, "  Chunks sent:   %d/%d", success_count, num_chunks);
  ESP_LOGI(s_tag, "  Transferred:   %zu bytes", total_transferred);
  ESP_LOGI(s_tag, "  Total time:    %lld us", elapsed_us);
  ESP_LOGI(s_tag, "  Throughput:    %.2f KB/s",
           (total_transferred / 1024.0) / (elapsed_us / 1000000.0));
  ESP_LOGI(s_tag, "  Avg per chunk: %lld us", elapsed_us / success_count);

  heap_caps_free(tx_buf);
  heap_caps_free(rx_buf);
}

/**
 * @brief Compare different transfer sizes
 */
static void priv_compare_transfer_sizes(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Transfer Size Comparison ===\n");

  const size_t test_sizes[] = {
    64,      /* Small */
    256,     /* Medium */
    1024,    /* 1 KB */
    2048,    /* 2 KB */
    4092,    /* Max DMA */
  };

  const int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);

  ESP_LOGI(s_tag, "Testing different transfer sizes...\n");

  for (int i = 0; i < num_tests; i++) {
    size_t size = test_sizes[i];

    uint8_t *tx_buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_DMA);
    uint8_t *rx_buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_DMA);

    if (tx_buf == NULL || rx_buf == NULL) {
      ESP_LOGW(s_tag, "Skip size %zu: allocation failed", size);
      if (tx_buf) heap_caps_free(tx_buf);
      if (rx_buf) heap_caps_free(rx_buf);
      continue;
    }

    /* Fill buffer */
    for (size_t j = 0; j < size; j++) {
      tx_buf[j] = (uint8_t)(j & 0xFF);
    }

    /* Time the transfer */
    int64_t start = esp_timer_get_time();

    esp_err_t ret = star_bus_spi_transfer(manager, "dma_device",
                                          tx_buf, rx_buf, size, 5000);

    int64_t elapsed = esp_timer_get_time() - start;

    if (ret == ESP_OK) {
      double throughput_kbps = (size * 8) / (elapsed / 1000.0);
      ESP_LOGI(s_tag, "%4zu bytes: %5lld us  (%.1f Kbps)",
               size, elapsed, throughput_kbps);
    } else {
      ESP_LOGE(s_tag, "%4zu bytes: FAILED - %s", size, esp_err_to_name(ret));
    }

    heap_caps_free(tx_buf);
    heap_caps_free(rx_buf);

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/**
 * @brief Demonstrate optimal buffer sizing
 */
static void priv_demonstrate_optimal_sizing(void)
{
  ESP_LOGI(s_tag, "\n=== Optimal Buffer Sizing ===\n");

  ESP_LOGI(s_tag, "Size Selection Guidelines:\n");

  ESP_LOGI(s_tag, "Small transfers (< 64 bytes):");
  ESP_LOGI(s_tag, "  - Can use non-DMA mode");
  ESP_LOGI(s_tag, "  - Lower overhead");
  ESP_LOGI(s_tag, "  - Faster for tiny packets");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Medium transfers (64-1024 bytes):");
  ESP_LOGI(s_tag, "  - Use DMA for efficiency");
  ESP_LOGI(s_tag, "  - Good balance of overhead vs throughput");
  ESP_LOGI(s_tag, "  - Typical application sweet spot");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Large transfers (1024-4092 bytes):");
  ESP_LOGI(s_tag, "  - Always use DMA");
  ESP_LOGI(s_tag, "  - Maximum efficiency");
  ESP_LOGI(s_tag, "  - Best throughput per transaction");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Very large (> 4092 bytes):");
  ESP_LOGI(s_tag, "  - Must chunk into %d byte pieces", SPI_MAX_DMA_LEN);
  ESP_LOGI(s_tag, "  - Use loop to send multiple chunks");
  ESP_LOGI(s_tag, "  - Consider memory usage vs latency");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Memory Alignment:");
  ESP_LOGI(s_tag, "  - Use heap_caps_malloc(size, MALLOC_CAP_DMA)");
  ESP_LOGI(s_tag, "  - 4-byte alignment recommended");
  ESP_LOGI(s_tag, "  - Better performance with aligned buffers");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Power of 2 Sizing:");
  ESP_LOGI(s_tag, "  - 256, 512, 1024, 2048, 4096 bytes");
  ESP_LOGI(s_tag, "  - Often optimal for cache and DMA");
  ESP_LOGI(s_tag, "  - Simplifies buffer management");
}

/**
 * @brief Test different DMA channel configurations
 */
static void priv_test_dma_limits(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== DMA Configuration Limits ===\n");

  ESP_LOGI(s_tag, "DMA Channel Info:");
  ESP_LOGI(s_tag, "  SPI_DMA_CH_AUTO:    Auto-select (recommended)");
  ESP_LOGI(s_tag, "  SPI_DMA_CH1:        Channel 1 (explicit)");
  ESP_LOGI(s_tag, "  SPI_DMA_CH2:        Channel 2 (explicit)");
  ESP_LOGI(s_tag, "  SPI_DMA_DISABLED:   No DMA (max ~64 bytes)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "DMA Descriptor Limits:");
  ESP_LOGI(s_tag, "  Max per descriptor: 4092 bytes");
  ESP_LOGI(s_tag, "  Descriptor overhead: ~8 bytes");
  ESP_LOGI(s_tag, "  Chain multiple descriptors for > 4092 bytes");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Note: star_bus handles DMA setup automatically");
  ESP_LOGI(s_tag, "      You just specify the total transfer size");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Maximum Transfer Size Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_max_xfer_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create DMA-enabled SPI device */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "dma_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,  /* Enable DMA */
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, spi_config);
  ret = star_bus_config_init(spi_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "SPI initialized with DMA\n");

  /* Run tests */
  priv_test_max_single_transfer(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_oversized_transfer(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_chunking(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_compare_transfer_sizes(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_optimal_sizing();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_dma_limits(&manager);

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Transfer Size Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Key Limits:");
  ESP_LOGI(s_tag, "  DMA max single:     %d bytes", SPI_MAX_DMA_LEN);
  ESP_LOGI(s_tag, "  Non-DMA recommend:  %d bytes", SPI_MAX_NON_DMA_LEN);
  ESP_LOGI(s_tag, "  Theoretical max:    Unlimited (via chunking)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Best Practices:");
  ESP_LOGI(s_tag, "  1. Use DMA for transfers > 64 bytes");
  ESP_LOGI(s_tag, "  2. Keep single transfers <= 4092 bytes");
  ESP_LOGI(s_tag, "  3. Chunk larger transfers in application");
  ESP_LOGI(s_tag, "  4. Use power-of-2 sizes when possible");
  ESP_LOGI(s_tag, "  5. Allocate from DMA-capable memory");
  ESP_LOGI(s_tag, "  6. Consider queue depth for pipelining");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Common Sizes:");
  ESP_LOGI(s_tag, "  Commands:     4-16 bytes (no DMA needed)");
  ESP_LOGI(s_tag, "  Registers:    16-64 bytes (can use DMA)");
  ESP_LOGI(s_tag, "  Data blocks:  512-4092 bytes (always DMA)");
  ESP_LOGI(s_tag, "  Large files:  Chunk into 4092 byte pieces");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI maximum transfer size example complete");
}
