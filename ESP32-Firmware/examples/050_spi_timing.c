/**
 * @file 050_spi_timing.c
 * @brief SPI timing analysis and measurement
 *
 * Demonstrates:
 * - Transfer timing measurement
 * - Throughput calculation
 * - Overhead analysis
 * - Performance optimization tips
 * - Timing verification
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "SPI_TIMING";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)

/**
 * @brief Timing measurement structure
 */
typedef struct {
  int64_t setup_time_us;
  int64_t transfer_time_us;
  int64_t cleanup_time_us;
  int64_t total_time_us;
  size_t bytes_transferred;
  double throughput_kbps;
  double efficiency_percent;
} spi_timing_t;

/**
 * @brief Measure single transfer timing
 */
static void priv_measure_transfer_timing(star_bus_manager_t *manager,
                                          const char *device_name,
                                     size_t transfer_size,
                                     int clock_speed_hz,
                                     spi_timing_t *timing)
{
  esp_err_t ret;
  int64_t t0, t1, t2, t3;

  /* Allocate buffers */
  uint8_t *tx_buffer = (uint8_t *)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);
  uint8_t *rx_buffer = (uint8_t *)heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);

  if (tx_buffer == NULL || rx_buffer == NULL) {
    ESP_LOGE(s_tag, "Buffer allocation failed");
    if (tx_buffer) heap_caps_free(tx_buffer);
    if (rx_buffer) heap_caps_free(rx_buffer);
    return;
  }

  /* Setup phase - prepare data */
  t0 = esp_timer_get_time();

  for (size_t i = 0; i < transfer_size; i++) {
    tx_buffer[i] = (uint8_t)(i & 0xFF);
  }

  t1 = esp_timer_get_time();

  /* Transfer phase */
  ret = star_bus_spi_transfer(manager, device_name,
                               tx_buffer, rx_buffer,
                               transfer_size, 5000);

  t2 = esp_timer_get_time();

  /* Cleanup phase - verify data */
  if (ret == ESP_OK) {
    /* Optional: verify received data */
    bool verify_ok = true;
    for (size_t i = 0; i < 16 && i < transfer_size; i++) {
      if (rx_buffer[i] != tx_buffer[i]) {
        verify_ok = false;
        break;
      }
    }
  }

  t3 = esp_timer_get_time();

  /* Calculate timing */
  timing->setup_time_us = t1 - t0;
  timing->transfer_time_us = t2 - t1;
  timing->cleanup_time_us = t3 - t2;
  timing->total_time_us = t3 - t0;
  timing->bytes_transferred = transfer_size;

  /* Calculate throughput */
  double transfer_time_sec = timing->transfer_time_us / 1000000.0;
  timing->throughput_kbps = (transfer_size * 8) / transfer_time_sec / 1000.0;

  /* Calculate efficiency vs theoretical max */
  double theoretical_kbps = clock_speed_hz / 1000.0;
  timing->efficiency_percent = (timing->throughput_kbps / theoretical_kbps) * 100.0;

  /* Cleanup */
  heap_caps_free(tx_buffer);
  heap_caps_free(rx_buffer);
}

/**
 * @brief Print timing results
 */
static void priv_print_timing_results(const spi_timing_t *timing)
{
  ESP_LOGI(s_tag, "Timing Results:");
  ESP_LOGI(s_tag, "  Setup time:    %6lld us", timing->setup_time_us);
  ESP_LOGI(s_tag, "  Transfer time: %6lld us", timing->transfer_time_us);
  ESP_LOGI(s_tag, "  Cleanup time:  %6lld us", timing->cleanup_time_us);
  ESP_LOGI(s_tag, "  Total time:    %6lld us", timing->total_time_us);
  ESP_LOGI(s_tag, "  Bytes:         %6zu", timing->bytes_transferred);
  ESP_LOGI(s_tag, "  Throughput:    %6.2f Kbps (%.3f Mbps)",
           timing->throughput_kbps, timing->throughput_kbps / 1000.0);
  ESP_LOGI(s_tag, "  Efficiency:    %6.2f%% of theoretical max",
           timing->efficiency_percent);
}

/**
 * @brief Analyze timing for different transfer sizes
 */
static void priv_analyze_transfer_sizes(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Transfer Size Analysis ===\n");

  const size_t test_sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
  const int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
  const int clock_speed = 10000000;  /* 10 MHz */

  ESP_LOGI(s_tag, "Clock speed: %d Hz (10 MHz)\n", clock_speed);

  for (int i = 0; i < num_tests; i++) {
    size_t size = test_sizes[i];
    spi_timing_t timing;

    ESP_LOGI(s_tag, "--- Transfer Size: %zu bytes ---", size);

    priv_measure_transfer_timing(manager, "timing_device", size, clock_speed, &timing);
    priv_print_timing_results(&timing);

    /* Calculate overhead */
    int64_t overhead_us = timing.total_time_us - timing.transfer_time_us;
    ESP_LOGI(s_tag, "  Overhead:      %6lld us (%.1f%%)\n",
             overhead_us, (overhead_us * 100.0) / timing.total_time_us);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * @brief Analyze timing for different clock speeds
 */
static void priv_analyze_clock_speeds(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Clock Speed Analysis ===\n");

  const int speeds[] = {1000000, 5000000, 10000000, 20000000, 40000000};
  const char* speed_names[] = {"1 MHz", "5 MHz", "10 MHz", "20 MHz", "40 MHz"};
  const int num_speeds = sizeof(speeds) / sizeof(speeds[0]);
  const size_t transfer_size = 512;

  ESP_LOGI(s_tag, "Transfer size: %zu bytes\n", transfer_size);

  for (int i = 0; i < num_speeds; i++) {
    ESP_LOGI(s_tag, "--- Speed: %s ---", speed_names[i]);

    /* Create device with specific speed */
    spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = speeds[i],
      .mode = 0,
      .spics_io_num = GPIO_NUM_15 + i,
      .queue_size = 7,
    };

    char device_name[32];
    snprintf(device_name, sizeof(device_name), "spi_%dmhz", speeds[i] / 1000000);

    star_bus_config_t *spi_config = star_bus_config_create_spi_device(
      device_name,
      SPI2_HOST,
      SPI_COPI_PIN,
      SPI_CIPO_PIN,
      SPI_SCLK_PIN,
      SPI_DMA_CH_AUTO,
      &dev_cfg
    );

    if (spi_config != NULL) {
      star_bus_manager_add_bus(manager, spi_config);
      if (star_bus_config_init(spi_config, manager) == ESP_OK) {
        spi_timing_t timing;
        priv_measure_transfer_timing(manager, device_name, transfer_size, speeds[i], &timing);
        priv_print_timing_results(&timing);
        ESP_LOGI(s_tag, "");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * @brief Calculate theoretical timing
 */
static void priv_calculate_theoretical_timing(void)
{
  ESP_LOGI(s_tag, "\n=== Theoretical Timing Calculations ===\n");

  const int clock_speeds[] = {1000000, 10000000, 40000000, 80000000};
  const size_t byte_counts[] = {1, 16, 256, 1024, 4096};

  ESP_LOGI(s_tag, "Transfer time = (bytes * 8 bits) / clock_speed\n");

  for (int s = 0; s < 4; s++) {
    int speed = clock_speeds[s];
    ESP_LOGI(s_tag, "Clock: %d Hz (%.0f MHz)", speed, speed / 1000000.0);

    for (int b = 0; b < 5; b++) {
      size_t bytes = byte_counts[b];
      double time_us = (bytes * 8.0 * 1000000.0) / speed;

      ESP_LOGI(s_tag, "  %4zu bytes: %.2f us (theoretical)", bytes, time_us);
    }
    ESP_LOGI(s_tag, "");
  }

  ESP_LOGI(s_tag, "Note: Actual times include:");
  ESP_LOGI(s_tag, "  - CS assertion/deassertion time");
  ESP_LOGI(s_tag, "  - Setup/hold time");
  ESP_LOGI(s_tag, "  - Driver overhead");
  ESP_LOGI(s_tag, "  - DMA setup time");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Timing Analysis Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_timing_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create base timing test device */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 10000000,  /* 10 MHz */
    .mode = 0,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "timing_device",
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

  ESP_LOGI(s_tag, "SPI initialized for timing analysis\n");

  /* Theoretical calculations */
  priv_calculate_theoretical_timing();

  /* Analyze different transfer sizes */
  priv_analyze_transfer_sizes(&manager);

  /* Analyze different clock speeds */
  priv_analyze_clock_speeds(&manager);

  /* --- Timing Optimization Tips --- */
  ESP_LOGI(s_tag, "\n--- Performance Optimization Tips ---");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "1. Use DMA for transfers > 32 bytes");
  ESP_LOGI(s_tag, "   - Reduces CPU overhead");
  ESP_LOGI(s_tag, "   - Enables true concurrent operation");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "2. Increase transfer size when possible");
  ESP_LOGI(s_tag, "   - Amortizes overhead across more data");
  ESP_LOGI(s_tag, "   - Better efficiency at larger sizes");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "3. Use transaction queuing");
  ESP_LOGI(s_tag, "   - Pipeline multiple transfers");
  ESP_LOGI(s_tag, "   - Reduce gaps between transactions");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "4. Optimize clock speed");
  ESP_LOGI(s_tag, "   - Test maximum stable speed for your setup");
  ESP_LOGI(s_tag, "   - Consider signal integrity vs speed");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "5. Use appropriate buffer alignment");
  ESP_LOGI(s_tag, "   - 4-byte aligned buffers for DMA");
  ESP_LOGI(s_tag, "   - Allocate from DMA-capable memory");

  /* --- Measurement Tools --- */
  ESP_LOGI(s_tag, "\n--- Timing Measurement Tools ---");
  ESP_LOGI(s_tag, "Software timing (esp_timer_get_time()):");
  ESP_LOGI(s_tag, "  - Microsecond resolution");
  ESP_LOGI(s_tag, "  - Good for overall performance");
  ESP_LOGI(s_tag, "  - Subject to task switching overhead");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Logic analyzer:");
  ESP_LOGI(s_tag, "  - Nanosecond resolution");
  ESP_LOGI(s_tag, "  - See actual bus signals");
  ESP_LOGI(s_tag, "  - Verify timing parameters");
  ESP_LOGI(s_tag, "  - Essential for debugging");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "GPIO toggle + oscilloscope:");
  ESP_LOGI(s_tag, "  - Toggle GPIO before/after transfer");
  ESP_LOGI(s_tag, "  - Measure with oscilloscope");
  ESP_LOGI(s_tag, "  - Good for callback timing");

  /* --- Common Timing Issues --- */
  ESP_LOGI(s_tag, "\n--- Common Timing Issues ---");
  ESP_LOGI(s_tag, "Issue: Lower throughput than expected");
  ESP_LOGI(s_tag, "  - Check for task switching overhead");
  ESP_LOGI(s_tag, "  - Verify DMA is enabled");
  ESP_LOGI(s_tag, "  - Look for buffer copy overhead");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Issue: Inconsistent timing");
  ESP_LOGI(s_tag, "  - FreeRTOS task scheduling");
  ESP_LOGI(s_tag, "  - Interrupt latency");
  ESP_LOGI(s_tag, "  - WiFi/BT coexistence");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Issue: Timing violations");
  ESP_LOGI(s_tag, "  - Clock too fast for cable length");
  ESP_LOGI(s_tag, "  - Poor signal integrity");
  ESP_LOGI(s_tag, "  - Incorrect SPI mode");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI timing analysis example complete");
}
