/**
 * @file 056_spi_buffer_align.c
 * @brief Buffer alignment requirements for DMA transfers
 *
 * Demonstrates:
 * - DMA memory requirements
 * - Buffer alignment (4-byte boundary)
 * - heap_caps_malloc for DMA
 * - Aligned vs unaligned performance
 * - Common alignment pitfalls
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_BUFFER_ALIGN";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/**
 * @brief Check if pointer is aligned
 */
static bool priv_is_aligned(const void *ptr, size_t alignment)
{
  return ((uintptr_t)ptr % alignment) == 0;
}

/**
 * @brief Demonstrate DMA-capable memory allocation
 */
static void priv_demonstrate_dma_allocation(void)
{
  ESP_LOGI(s_tag, "\n=== DMA-Capable Memory Allocation ===\n");

  const size_t buffer_size = 512;

  /* Method 1: DMA-capable memory (CORRECT) */
  ESP_LOGI(s_tag, "Method 1: heap_caps_malloc(MALLOC_CAP_DMA)");

  uint8_t *dma_buf = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);

  if (dma_buf != NULL) {
    ESP_LOGI(s_tag, "  Address: %p", dma_buf);
    ESP_LOGI(s_tag, "  4-byte aligned: %s", priv_is_aligned(dma_buf, 4) ? "YES" : "NO");
    ESP_LOGI(s_tag, "  8-byte aligned: %s", priv_is_aligned(dma_buf, 8) ? "YES" : "NO");
    ESP_LOGI(s_tag, "  DMA-capable: YES (internal RAM)");

    heap_caps_free(dma_buf);
  } else {
    ESP_LOGE(s_tag, "  Allocation failed!");
  }

  /* Method 2: Regular malloc (MAY FAIL with DMA) */
  ESP_LOGI(s_tag, "\nMethod 2: malloc() - NOT recommended for DMA");

  uint8_t *regular_buf = (uint8_t*)malloc(buffer_size);

  if (regular_buf != NULL) {
    ESP_LOGI(s_tag, "  Address: %p", regular_buf);
    ESP_LOGI(s_tag, "  4-byte aligned: %s", priv_is_aligned(regular_buf, 4) ? "YES" : "NO");
    ESP_LOGI(s_tag, "  DMA-capable: UNKNOWN (could be PSRAM/Flash)");
    ESP_LOGI(s_tag, "  WARNING: May not work with DMA!");

    free(regular_buf);
  }

  /* Method 3: Stack allocation (DANGEROUS for DMA) */
  ESP_LOGI(s_tag, "\nMethod 3: Stack arrays - NEVER use with DMA");

  uint8_t stack_buf[64];

  ESP_LOGI(s_tag, "  Address: %p", stack_buf);
  ESP_LOGI(s_tag, "  4-byte aligned: %s", priv_is_aligned(stack_buf, 4) ? "YES" : "NO");
  ESP_LOGI(s_tag, "  DMA-capable: NO (stack memory)");
  ESP_LOGI(s_tag, "  CRITICAL: Will cause crashes with DMA!");

  /* Method 4: Static/global arrays (USUALLY OK) */
  ESP_LOGI(s_tag, "\nMethod 4: Static arrays - Usually safe");
  static uint8_t static_buf[64];

  ESP_LOGI(s_tag, "  Address: %p", static_buf);
  ESP_LOGI(s_tag, "  4-byte aligned: %s", priv_is_aligned(static_buf, 4) ? "YES" : "NO");
  ESP_LOGI(s_tag, "  DMA-capable: Usually (depends on linker)");
}

/**
 * @brief Test aligned vs unaligned buffers
 */
static void priv_test_alignment_performance(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Alignment Performance Test ===\n");

  const size_t transfer_size = 1024;

  /* Allocate aligned buffer */
  uint8_t *aligned_tx = (uint8_t*)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);
  uint8_t *aligned_rx = (uint8_t*)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);

  if (aligned_tx == NULL || aligned_rx == NULL) {
    ESP_LOGE(s_tag, "Aligned buffer allocation failed");
    if (aligned_tx) heap_caps_free(aligned_tx);
    if (aligned_rx) heap_caps_free(aligned_rx);
    return;
  }

  /* Initialize data */
  for (size_t i = 0; i < transfer_size; i++) {
    aligned_tx[i] = (uint8_t)(i & 0xFF);
  }

  /* Test aligned transfer */
  ESP_LOGI(s_tag, "Testing aligned buffers (4-byte boundary)...");

  int64_t start = esp_timer_get_time();

  esp_err_t ret = star_bus_spi_transfer(manager, "align_device",
                                        aligned_tx, aligned_rx,
                                        transfer_size, 5000);

  int64_t aligned_time = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Aligned transfer: %lld us", aligned_time);
    ESP_LOGI(s_tag, "  Throughput: %.2f KB/s",
             (transfer_size / 1024.0) / (aligned_time / 1000000.0));
  } else {
    ESP_LOGE(s_tag, "  Aligned transfer failed: %s", esp_err_to_name(ret));
  }

  /* Note: Deliberately testing misaligned is dangerous and may crash
   * We'll just demonstrate the concept */

  ESP_LOGI(s_tag, "\nNote: Misaligned buffers may cause:");
  ESP_LOGI(s_tag, "  - DMA errors");
  ESP_LOGI(s_tag, "  - ESP_ERR_INVALID_ARG returns");
  ESP_LOGI(s_tag, "  - System crashes");
  ESP_LOGI(s_tag, "  - Data corruption");

  heap_caps_free(aligned_tx);
  heap_caps_free(aligned_rx);
}

/**
 * @brief Demonstrate proper buffer allocation patterns
 */
static void priv_demonstrate_allocation_patterns(void)
{
  ESP_LOGI(s_tag, "\n=== Buffer Allocation Patterns ===\n");

  /* Pattern 1: Single large buffer */
  ESP_LOGI(s_tag, "Pattern 1: Single Large Buffer");
  ESP_LOGI(s_tag, "  uint8_t *buf = heap_caps_malloc(4096, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  Use case: Large transfers, streaming");
  ESP_LOGI(s_tag, "");

  /* Pattern 2: Double buffering */
  ESP_LOGI(s_tag, "Pattern 2: Double Buffering");
  ESP_LOGI(s_tag, "  uint8_t *buf1 = heap_caps_malloc(1024, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  uint8_t *buf2 = heap_caps_malloc(1024, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  Use case: Pipeline transfers, reduce latency");
  ESP_LOGI(s_tag, "");

  /* Pattern 3: Separate TX/RX */
  ESP_LOGI(s_tag, "Pattern 3: Separate TX/RX Buffers");
  ESP_LOGI(s_tag, "  uint8_t *tx = heap_caps_malloc(512, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  uint8_t *rx = heap_caps_malloc(512, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  Use case: Full-duplex transfers");
  ESP_LOGI(s_tag, "");

  /* Pattern 4: Ring buffer */
  ESP_LOGI(s_tag, "Pattern 4: Ring Buffer");
  ESP_LOGI(s_tag, "  uint8_t *ring = heap_caps_malloc(8192, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  Use case: Continuous data streaming");
}

/**
 * @brief Check memory capabilities
 */
static void priv_check_memory_capabilities(void)
{
  ESP_LOGI(s_tag, "\n=== Memory Capabilities ===\n");

  /* Get heap info for DMA memory */
  multi_heap_info_t dma_info;
  heap_caps_get_info(&dma_info, MALLOC_CAP_DMA);

  ESP_LOGI(s_tag, "DMA-capable memory:");
  ESP_LOGI(s_tag, "  Total free:      %u bytes", dma_info.total_free_bytes);
  ESP_LOGI(s_tag, "  Largest block:   %u bytes", dma_info.largest_free_block);
  ESP_LOGI(s_tag, "  Minimum free:    %u bytes", dma_info.minimum_free_bytes);
  ESP_LOGI(s_tag, "");

  /* Get general heap info */
  multi_heap_info_t general_info;
  heap_caps_get_info(&general_info, MALLOC_CAP_8BIT);

  ESP_LOGI(s_tag, "General memory:");
  ESP_LOGI(s_tag, "  Total free:      %u bytes", general_info.total_free_bytes);
  ESP_LOGI(s_tag, "  Largest block:   %u bytes", general_info.largest_free_block);
  ESP_LOGI(s_tag, "");

  /* Check if buffer would fit */
  const size_t test_size = 4096;
  void *test_buf = heap_caps_malloc(test_size, MALLOC_CAP_DMA);

  if (test_buf != NULL) {
    ESP_LOGI(s_tag, "Can allocate %zu byte DMA buffer: YES", test_size);
    heap_caps_free(test_buf);
  } else {
    ESP_LOGW(s_tag, "Can allocate %zu byte DMA buffer: NO", test_size);
  }
}

/**
 * @brief Demonstrate alignment requirements
 */
static void priv_demonstrate_alignment_requirements(void)
{
  ESP_LOGI(s_tag, "\n=== Alignment Requirements ===\n");

  ESP_LOGI(s_tag, "ESP32 DMA Requirements:");
  ESP_LOGI(s_tag, "  1. Buffer must be in internal RAM");
  ESP_LOGI(s_tag, "  2. Buffer must be 4-byte aligned");
  ESP_LOGI(s_tag, "  3. Buffer length should be multiple of 4");
  ESP_LOGI(s_tag, "  4. Use heap_caps_malloc(size, MALLOC_CAP_DMA)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ESP32-S2/S3/C3 Differences:");
  ESP_LOGI(s_tag, "  - May have different alignment requirements");
  ESP_LOGI(s_tag, "  - Check chip-specific documentation");
  ESP_LOGI(s_tag, "  - MALLOC_CAP_DMA handles it automatically");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "What happens with wrong alignment:");
  ESP_LOGI(s_tag, "  - ESP_ERR_INVALID_ARG from SPI driver");
  ESP_LOGI(s_tag, "  - DMA controller errors");
  ESP_LOGI(s_tag, "  - Possible system crash");
  ESP_LOGI(s_tag, "  - Silent data corruption");
}

/**
 * @brief Common alignment mistakes
 */
static void priv_demonstrate_common_mistakes(void)
{
  ESP_LOGI(s_tag, "\n=== Common Alignment Mistakes ===\n");

  ESP_LOGI(s_tag, "MISTAKE 1: Using stack arrays");
  ESP_LOGI(s_tag, "  uint8_t buf[64];");
  ESP_LOGI(s_tag, "  star_bus_spi_transfer(..., buf, ...); // CRASH!");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "MISTAKE 2: Using regular malloc");
  ESP_LOGI(s_tag, "  uint8_t *buf = malloc(512);");
  ESP_LOGI(s_tag, "  star_bus_spi_transfer(..., buf, ...); // May fail!");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "MISTAKE 3: String literals");
  ESP_LOGI(s_tag, "  char *data = \"Hello\";");
  ESP_LOGI(s_tag, "  star_bus_spi_transfer(..., data, ...); // CRASH!");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "MISTAKE 4: Global arrays without alignment");
  ESP_LOGI(s_tag, "  uint8_t buf[3] = {1,2,3}; // Odd size!");
  ESP_LOGI(s_tag, "  star_bus_spi_transfer(..., buf, 3, ...); // May fail!");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "CORRECT: Use heap_caps_malloc");
  ESP_LOGI(s_tag, "  uint8_t *buf = heap_caps_malloc(64, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  star_bus_spi_transfer(..., buf, ...); // OK!");
  ESP_LOGI(s_tag, "  heap_caps_free(buf);");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Buffer Alignment Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_align_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create SPI device with DMA */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "align_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,  /* DMA requires proper alignment */
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

  /* Demonstrate alignment concepts */
  priv_demonstrate_dma_allocation();
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_alignment_performance(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_allocation_patterns();
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_check_memory_capabilities();
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_alignment_requirements();
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_demonstrate_common_mistakes();

  /* Best practices summary */
  ESP_LOGI(s_tag, "\n=== Best Practices Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "1. ALWAYS use heap_caps_malloc(size, MALLOC_CAP_DMA)");
  ESP_LOGI(s_tag, "2. NEVER use stack arrays for DMA transfers");
  ESP_LOGI(s_tag, "3. Check allocation success before using buffer");
  ESP_LOGI(s_tag, "4. Free buffers with heap_caps_free() when done");
  ESP_LOGI(s_tag, "5. Prefer sizes that are multiples of 4");
  ESP_LOGI(s_tag, "6. Check available DMA memory before large allocations");
  ESP_LOGI(s_tag, "7. Consider double-buffering for continuous transfers");
  ESP_LOGI(s_tag, "8. Test on actual hardware - emulators may not catch errors");
  ESP_LOGI(s_tag, "");

  /* Quick reference */
  ESP_LOGI(s_tag, "=== Quick Reference ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Allocate DMA buffer:");
  ESP_LOGI(s_tag, "  uint8_t *buf = heap_caps_malloc(size, MALLOC_CAP_DMA);");
  ESP_LOGI(s_tag, "  if (buf == NULL) { /* handle error */ }");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Use buffer:");
  ESP_LOGI(s_tag, "  star_bus_spi_transfer(&mgr, \"dev\", tx_buf, rx_buf, len, timeout);");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Free buffer:");
  ESP_LOGI(s_tag, "  heap_caps_free(buf);");
  ESP_LOGI(s_tag, "  buf = NULL; // Good practice");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI buffer alignment example complete");
}
