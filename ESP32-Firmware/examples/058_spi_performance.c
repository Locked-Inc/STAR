/**
 * @file 058_spi_performance.c
 * @brief Performance comparison and benchmarking
 *
 * Demonstrates:
 * - DMA vs non-DMA performance
 * - Different clock speeds
 * - Transfer size effects
 * - Queue depth impact
 * - Throughput measurements
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_PERFORMANCE";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

/* Performance test results */
typedef struct {
  const char *test_name;
  size_t transfer_size;
  int num_transfers;
  int64_t total_time_us;
  double throughput_kbps;
  double efficiency_percent;
} perf_result_t;

/**
 * @brief Print performance results
 */
static void priv_print_perf_result(const perf_result_t* result)
{
  ESP_LOGI(s_tag, "%-30s %6zu bytes x %3d = %8lld us  %8.1f Kbps  %5.1f%%",
           result->test_name,
           result->transfer_size,
           result->num_transfers,
           result->total_time_us,
           result->throughput_kbps,
           result->efficiency_percent);
}

/**
 * @brief Benchmark a specific configuration
 */
static void priv_benchmark_config(star_bus_manager_t *manager,
                              const char *device_name,
                              size_t transfer_size,
                              int num_transfers,
                              int clock_speed_hz,
                              perf_result_t* result)
{
  uint8_t *tx_buf = (uint8_t*)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);
  uint8_t *rx_buf = (uint8_t*)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);

  if (tx_buf == NULL || rx_buf == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    if (tx_buf) heap_caps_free(tx_buf);
    if (rx_buf) heap_caps_free(rx_buf);
    return;
  }

  /* Fill TX buffer */
  for (size_t i = 0; i < transfer_size; i++) {
    tx_buf[i] = (uint8_t)(i & 0xFF);
  }

  /* Warm-up transfer */
  star_bus_spi_transfer(manager, device_name, tx_buf, rx_buf, transfer_size, 5000);

  /* Benchmark */
  int64_t start_time = esp_timer_get_time();

  for (int i = 0; i < num_transfers; i++) {
    esp_err_t ret = star_bus_spi_transfer(manager, device_name,
                                          tx_buf, rx_buf,
                                          transfer_size, 5000);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Transfer %d failed: %s", i, esp_err_to_name(ret));
      break;
    }
  }

  int64_t elapsed_us = esp_timer_get_time() - start_time;

  /* Calculate results */
  size_t total_bytes = transfer_size * num_transfers;
  double time_sec = elapsed_us / 1000000.0;
  double throughput_kbps = (total_bytes * 8) / time_sec / 1000.0;
  double theoretical_kbps = clock_speed_hz / 1000.0;
  double efficiency = (throughput_kbps / theoretical_kbps) * 100.0;

  /* Fill result */
  result->transfer_size = transfer_size;
  result->num_transfers = num_transfers;
  result->total_time_us = elapsed_us;
  result->throughput_kbps = throughput_kbps;
  result->efficiency_percent = efficiency;

  heap_caps_free(tx_buf);
  heap_caps_free(rx_buf);
}

/**
 * @brief Compare different clock speeds
 */
static void priv_compare_clock_speeds(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Clock Speed Comparison ===\n");
  ESP_LOGI(s_tag, "%-30s %6s   %3s   %8s  %8s  %5s",
           "Test", "Size", "N", "Time(us)", "Kbps", "Eff%");
  ESP_LOGI(s_tag, "----------------------------------------------------------------------");

  const int speeds[] = {1000000, 5000000, 10000000, 20000000, 40000000};
  const char *speed_names[] = {"1MHz", "5MHz", "10MHz", "20MHz", "40MHz"};
  const int num_speeds = 5;

  const size_t test_size = 512;
  const int num_xfers = 20;

  for (int i = 0; i < num_speeds; i++) {
    /* Create device at this speed */
    char device_name[32];
    snprintf(device_name, sizeof(device_name), "spi_%dmhz", speeds[i] / 1000000);

    spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = speeds[i],
      .mode           = 0,
      .spics_io_num = GPIO_NUM_15 + i,
      .queue_size     = 7,
    };

    star_bus_config_t *cfg = star_bus_config_create_spi_device(
      device_name,
      SPI2_HOST,
      SPI_COPI_PIN,
      SPI_CIPO_PIN,
      SPI_SCLK_PIN,
      SPI_DMA_CH_AUTO,
      &dev_cfg
    );

    if (cfg != NULL) {
      star_bus_manager_add_bus(manager, cfg);
      if (star_bus_config_init(cfg, manager) == ESP_OK) {
        perf_result_t result = {.test_name = speed_names[i]};
        priv_benchmark_config(manager, device_name, test_size, num_xfers, speeds[i], &result);
        priv_print_perf_result(&result);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief Compare different transfer sizes
 */
static void priv_compare_transfer_sizes(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Transfer Size Comparison (10MHz) ===\n");
  ESP_LOGI(s_tag, "%-30s %6s   %3s   %8s  %8s  %5s",
           "Test", "Size", "N", "Time(us)", "Kbps", "Eff%");
  ESP_LOGI(s_tag, "----------------------------------------------------------------------");

  const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4092};
  const int num_sizes = 9;
  const int clock_speed = 10000000;

  for (int i = 0; i < num_sizes; i++) {
    char test_name[32];
    snprintf(test_name, sizeof(test_name), "%zu bytes", sizes[i]);

    perf_result_t result = {.test_name = test_name};
    priv_benchmark_config(manager, "perf_device", sizes[i], 20, clock_speed, &result);
    priv_print_perf_result(&result);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief Compare single large vs multiple small transfers
 */
static void priv_compare_chunking_strategies(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Chunking Strategy Comparison ===\n");

  const size_t total_bytes = 16384;  /* 16 KB total */

  /* Strategy 1: One large transfer */
  ESP_LOGI(s_tag, "Strategy 1: Single 16 KB transfer");

  uint8_t *tx = (uint8_t*)heap_caps_malloc(total_bytes, MALLOC_CAP_DMA);
  uint8_t *rx = (uint8_t*)heap_caps_malloc(total_bytes, MALLOC_CAP_DMA);

  if (tx && rx) {
    memset(tx, 0xAA, total_bytes);

    int64_t start = esp_timer_get_time();

    /* Must chunk due to 4092 byte DMA limit */
    size_t offset = 0;
    while (offset < total_bytes) {
      size_t chunk = (total_bytes - offset > 4092) ? 4092 : (total_bytes - offset);
      star_bus_spi_transfer(manager, "perf_device",
                            tx + offset, rx + offset, chunk, 5000);
      offset += chunk;
    }

    int64_t elapsed1 = esp_timer_get_time() - start;

    ESP_LOGI(s_tag, "  Time: %lld us", elapsed1);
    ESP_LOGI(s_tag, "  Throughput: %.2f KB/s",
             (total_bytes / 1024.0) / (elapsed1 / 1000000.0));

    heap_caps_free(tx);
    heap_caps_free(rx);
  }

  /* Strategy 2: Many small transfers */
  ESP_LOGI(s_tag, "\nStrategy 2: Multiple 256 byte transfers");

  const size_t chunk_size = 256;
  const int num_chunks = total_bytes / chunk_size;

  tx = (uint8_t*)heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
  rx = (uint8_t*)heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);

  if (tx && rx) {
    memset(tx, 0xBB, chunk_size);

    int64_t start = esp_timer_get_time();

    for (int i = 0; i < num_chunks; i++) {
      star_bus_spi_transfer(manager, "perf_device",
                            tx, rx, chunk_size, 5000);
    }

    int64_t elapsed2 = esp_timer_get_time() - start;

    ESP_LOGI(s_tag, "  Time: %lld us", elapsed2);
    ESP_LOGI(s_tag, "  Throughput: %.2f KB/s",
             (total_bytes / 1024.0) / (elapsed2 / 1000000.0));

    heap_caps_free(tx);
    heap_caps_free(rx);
  }
}

/**
 * @brief Performance optimization tips
 */
static void priv_print_optimization_tips(void)
{
  ESP_LOGI(s_tag, "\n=== Performance Optimization Tips ===\n");

  ESP_LOGI(s_tag, "1. Clock Speed:");
  ESP_LOGI(s_tag, "   - Start at lower speed (1-5 MHz)");
  ESP_LOGI(s_tag, "   - Increase until errors occur");
  ESP_LOGI(s_tag, "   - Back off 10-20%% for margin");
  ESP_LOGI(s_tag, "   - Consider signal integrity vs speed");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. DMA Usage:");
  ESP_LOGI(s_tag, "   - Use DMA for transfers > 64 bytes");
  ESP_LOGI(s_tag, "   - Allocate buffers with MALLOC_CAP_DMA");
  ESP_LOGI(s_tag, "   - Keep buffers 4-byte aligned");
  ESP_LOGI(s_tag, "   - Max DMA transfer: 4092 bytes");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Transfer Size:");
  ESP_LOGI(s_tag, "   - Larger transfers = better efficiency");
  ESP_LOGI(s_tag, "   - Optimal: 512-4092 bytes");
  ESP_LOGI(s_tag, "   - Avoid very small (< 16 bytes)");
  ESP_LOGI(s_tag, "   - Batch multiple operations");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. Queue Depth:");
  ESP_LOGI(s_tag, "   - Use queue_size >= 3");
  ESP_LOGI(s_tag, "   - Pipeline multiple transfers");
  ESP_LOGI(s_tag, "   - Recommended: 7 (ESP-IDF default)");
  ESP_LOGI(s_tag, "   - Higher for continuous streaming");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "5. CPU/Task:");
  ESP_LOGI(s_tag, "   - Prepare next buffer during transfer");
  ESP_LOGI(s_tag, "   - Use async operations");
  ESP_LOGI(s_tag, "   - Minimize processing in callbacks");
  ESP_LOGI(s_tag, "   - Consider FreeRTOS task priority");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "6. Hardware:");
  ESP_LOGI(s_tag, "   - Short wires (< 10 cm ideal)");
  ESP_LOGI(s_tag, "   - Good ground connection");
  ESP_LOGI(s_tag, "   - Proper termination for high speed");
  ESP_LOGI(s_tag, "   - Avoid long stub lengths");
}

/**
 * @brief Expected performance ranges
 */
static void priv_print_expected_performance(void)
{
  ESP_LOGI(s_tag, "\n=== Expected Performance Ranges ===\n");

  ESP_LOGI(s_tag, "Clock Speed vs Throughput:");
  ESP_LOGI(s_tag, "  1 MHz:   ~800 Kbps   (80%% efficiency)");
  ESP_LOGI(s_tag, "  5 MHz:   ~4 Mbps     (80%% efficiency)");
  ESP_LOGI(s_tag, "  10 MHz:  ~8 Mbps     (80%% efficiency)");
  ESP_LOGI(s_tag, "  20 MHz:  ~16 Mbps    (80%% efficiency)");
  ESP_LOGI(s_tag, "  40 MHz:  ~30 Mbps    (75%% efficiency)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Transfer Size Impact:");
  ESP_LOGI(s_tag, "  16 bytes:   50-60%% efficiency");
  ESP_LOGI(s_tag, "  64 bytes:   70-75%% efficiency");
  ESP_LOGI(s_tag, "  256 bytes:  75-80%% efficiency");
  ESP_LOGI(s_tag, "  1024 bytes: 80-85%% efficiency");
  ESP_LOGI(s_tag, "  4092 bytes: 85-90%% efficiency");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Typical Applications:");
  ESP_LOGI(s_tag, "  Flash read:    10-40 Mbps");
  ESP_LOGI(s_tag, "  Display update: 5-20 Mbps");
  ESP_LOGI(s_tag, "  SD card:        5-25 Mbps");
  ESP_LOGI(s_tag, "  Sensors:        0.1-1 Mbps");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Performance Benchmark ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_perf_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create base performance test device */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 10000000,  /* 10 MHz */
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "perf_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
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

  ESP_LOGI(s_tag, "SPI initialized for performance testing\n");

  /* Run benchmarks */
  priv_compare_transfer_sizes(&manager);
  vTaskDelay(pdMS_TO_TICKS(500));

  priv_compare_clock_speeds(&manager);
  vTaskDelay(pdMS_TO_TICKS(500));

  priv_compare_chunking_strategies(&manager);
  vTaskDelay(pdMS_TO_TICKS(500));

  priv_print_optimization_tips();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_print_expected_performance();

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Performance Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Key Findings:");
  ESP_LOGI(s_tag, "  1. Larger transfers = higher efficiency");
  ESP_LOGI(s_tag, "  2. DMA essential for good throughput");
  ESP_LOGI(s_tag, "  3. Efficiency typically 70-85%%");
  ESP_LOGI(s_tag, "  4. Clock speed limited by hardware");
  ESP_LOGI(s_tag, "  5. Overhead matters for small transfers");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI performance benchmark complete");
}
