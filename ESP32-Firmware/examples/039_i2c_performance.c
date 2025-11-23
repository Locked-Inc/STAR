/**
 * @file 039_i2c_performance.c
 * @brief Performance benchmarks (ops/sec, latency)
 *
 * This example demonstrates:
 * - Measuring I2C transaction throughput
 * - Latency measurements
 * - Operations per second (ops/sec)
 * - Performance comparison at different speeds
 * - Optimization techniques
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_stats.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_PERF";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address */
#define DEVICE_ADDR     (0x48)

/* Test parameters */
#define TEST_DURATION_MS    (5000)   /* 5 second test */
#define ITERATIONS          (1000)   /* For specific tests */

/**
 * @brief Performance results structure
 */
typedef struct {
  uint32_t total_operations;
  uint32_t successful_ops;
  uint32_t failed_ops;
  int64_t  total_time_us;
  int64_t  min_latency_us;
  int64_t  max_latency_us;
  uint64_t total_bytes;
} perf_results_t;

/**
 * @brief Benchmark single-byte reads
 */
void benchmark_single_byte_reads(star_bus_manager_t *manager,
                                 uint32_t clock_speed)
{
  ESP_LOGI(s_tag, "\n=== Single-Byte Read Benchmark @ %u Hz ===", clock_speed);

  perf_results_t results = {0};
  results.min_latency_us = INT64_MAX;

  uint8_t data;
  size_t  bytes_read;

  int64_t test_start = esp_timer_get_time();
  int64_t test_end   = test_start + (TEST_DURATION_MS * 1000);

  while (esp_timer_get_time() < test_end) {
    int64_t op_start = esp_timer_get_time();
    esp_err_t ret = star_bus_i2c_read(manager, "device", &data, 1,
                                      0x00, &bytes_read);
    int64_t op_time = esp_timer_get_time() - op_start;

    results.total_operations++;

    if (ret == ESP_OK) {
      results.successful_ops++;
      results.total_bytes += bytes_read;

      /* Track latency */
      if (op_time < results.min_latency_us) {
        results.min_latency_us = op_time;
      }
      if (op_time > results.max_latency_us) {
        results.max_latency_us = op_time;
      }
    } else {
      results.failed_ops++;
    }

    results.total_time_us += op_time;
  }

  /* Calculate statistics */
  float avg_latency  = (float)results.total_time_us / results.total_operations;
  float ops_per_sec  = (float)results.successful_ops /
                       (TEST_DURATION_MS / 1000.0f);
  float throughput   = (float)results.total_bytes / (TEST_DURATION_MS / 1000.0f);

  ESP_LOGI(s_tag, "Results:");
  ESP_LOGI(s_tag, "  Operations: %u (success: %u, failed: %u)",
           results.total_operations, results.successful_ops, results.failed_ops);
  ESP_LOGI(s_tag, "  Throughput: %u ops/sec", (uint32_t)ops_per_sec);
  ESP_LOGI(s_tag, "  Data rate: %.1f bytes/sec", throughput);
  ESP_LOGI(s_tag, "  Latency: min=%lld us, avg=%.1f us, max=%lld us",
           results.min_latency_us, avg_latency, results.max_latency_us);
}

/**
 * @brief Benchmark burst reads
 */
void benchmark_burst_reads(star_bus_manager_t *manager, size_t burst_size)
{
  ESP_LOGI(s_tag, "\n=== Burst Read Benchmark (%u bytes) ===", burst_size);

  perf_results_t results = {0};
  results.min_latency_us = INT64_MAX;

  uint8_t *buffer = (uint8_t *)malloc(burst_size);
  if (!buffer) {
    ESP_LOGE(s_tag, "Failed to allocate buffer");
    return;
  }

  size_t bytes_read;

  int64_t test_start = esp_timer_get_time();
  int64_t test_end   = test_start + (TEST_DURATION_MS * 1000);

  while (esp_timer_get_time() < test_end) {
    int64_t op_start = esp_timer_get_time();
    esp_err_t ret = star_bus_i2c_read(manager, "device", buffer, burst_size,
                                      0x00, &bytes_read);
    int64_t op_time = esp_timer_get_time() - op_start;

    results.total_operations++;

    if (ret == ESP_OK) {
      results.successful_ops++;
      results.total_bytes += bytes_read;

      if (op_time < results.min_latency_us) {
        results.min_latency_us = op_time;
      }
      if (op_time > results.max_latency_us) {
        results.max_latency_us = op_time;
      }
    } else {
      results.failed_ops++;
    }

    results.total_time_us += op_time;
  }

  free(buffer);

  /* Calculate statistics */
  float avg_latency = (float)results.total_time_us / results.total_operations;
  float ops_per_sec = (float)results.successful_ops /
                      (TEST_DURATION_MS / 1000.0f);
  float throughput  = (float)results.total_bytes / (TEST_DURATION_MS / 1000.0f);

  ESP_LOGI(s_tag, "Results:");
  ESP_LOGI(s_tag, "  Operations: %u (success: %u, failed: %u)",
           results.total_operations, results.successful_ops, results.failed_ops);
  ESP_LOGI(s_tag, "  Throughput: %u burst ops/sec", (uint32_t)ops_per_sec);
  ESP_LOGI(s_tag, "  Data rate: %.1f bytes/sec (%.1f KB/sec)",
           throughput, throughput / 1024.0f);
  ESP_LOGI(s_tag, "  Latency: min=%lld us, avg=%.1f us, max=%lld us",
           results.min_latency_us, avg_latency, results.max_latency_us);
  ESP_LOGI(s_tag, "  Efficiency: %.1f us/byte", avg_latency / burst_size);
}

/**
 * @brief Benchmark write operations
 */
void benchmark_writes(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Write Operation Benchmark ===");

  perf_results_t results = {0};
  results.min_latency_us = INT64_MAX;

  uint8_t write_data = 0xAA;
  size_t  bytes_written;

  int64_t test_start = esp_timer_get_time();
  int64_t test_end   = test_start + (TEST_DURATION_MS * 1000);

  while (esp_timer_get_time() < test_end) {
    int64_t op_start = esp_timer_get_time();
    esp_err_t ret = star_bus_i2c_write(manager, "device", &write_data, 1,
                                       0x00, &bytes_written);
    int64_t op_time = esp_timer_get_time() - op_start;

    results.total_operations++;

    if (ret == ESP_OK) {
      results.successful_ops++;
      results.total_bytes += bytes_written;

      if (op_time < results.min_latency_us) {
        results.min_latency_us = op_time;
      }
      if (op_time > results.max_latency_us) {
        results.max_latency_us = op_time;
      }
    } else {
      results.failed_ops++;
    }

    results.total_time_us += op_time;
    write_data++;  /* Change data each iteration */

    /* Small delay for EEPROM write cycle if applicable */
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  float avg_latency = (float)results.total_time_us / results.total_operations;
  float ops_per_sec = (float)results.successful_ops /
                      (TEST_DURATION_MS / 1000.0f);

  ESP_LOGI(s_tag, "Results:");
  ESP_LOGI(s_tag, "  Write ops: %u (success: %u, failed: %u)",
           results.total_operations, results.successful_ops, results.failed_ops);
  ESP_LOGI(s_tag, "  Throughput: %u writes/sec", (uint32_t)ops_per_sec);
  ESP_LOGI(s_tag, "  Latency: min=%lld us, avg=%.1f us, max=%lld us",
           results.min_latency_us, avg_latency, results.max_latency_us);
}

/**
 * @brief Compare performance at different clock speeds
 */
void compare_clock_speeds(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Clock Speed Comparison ===");

  uint32_t    speeds[] = {100000, 400000};  /* 100 kHz, 400 kHz */
  const char *names[]  = {"100 kHz", "400 kHz"};

  for (int i = 0; i < 2; i++) {
    ESP_LOGI(s_tag, "\n--- Testing at %s ---", names[i]);

    /* Create config with specific speed */
    char bus_name[32];
    snprintf(bus_name, sizeof(bus_name), "speed_%d", i);

    star_bus_config_t *config = star_bus_config_create_i2c(
      bus_name,
      I2C_NUM_0,
      DEVICE_ADDR,
      I2C_SDA_PIN,
      I2C_SCL_PIN,
      speeds[i]
    );

    if (config) {
      star_bus_manager_add_bus(manager, config);
      esp_err_t ret = star_bus_config_init(config, manager);

      if (ret == ESP_OK) {
        /* Quick performance test */
        uint8_t  data;
        size_t   bytes_read;
        int64_t  total_time = 0;
        uint32_t iterations = 100;

        for (uint32_t j = 0; j < iterations; j++) {
          int64_t start = esp_timer_get_time();
          star_bus_i2c_read(manager, bus_name, &data, 1, 0x00, &bytes_read);
          total_time += (esp_timer_get_time() - start);
        }

        float avg_us = (float)total_time / iterations;
        ESP_LOGI(s_tag, "Average latency: %.1f us", avg_us);
        ESP_LOGI(s_tag, "Theoretical max throughput: %.1f KB/s",
                 (speeds[i] / 8.0f) / 1024.0f);
      }

      star_bus_manager_remove_bus(manager, bus_name);
      star_bus_config_destroy(config);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * @brief Optimization tips demonstration
 */
void demonstrate_optimizations(void)
{
  ESP_LOGI(s_tag, "\n=== Performance Optimization Tips ===");

  ESP_LOGI(s_tag, "\n1. Use Burst Reads/Writes");
  ESP_LOGI(s_tag, "   - Read multiple bytes in one transaction");
  ESP_LOGI(s_tag, "   - Reduces overhead from addressing and start/stop");
  ESP_LOGI(s_tag, "   - Can be 3-5x faster than individual byte reads");

  ESP_LOGI(s_tag, "\n2. Increase Clock Speed");
  ESP_LOGI(s_tag, "   - 100 kHz -> 400 kHz = 4x theoretical speedup");
  ESP_LOGI(s_tag, "   - Requires adequate pull-up resistors");
  ESP_LOGI(s_tag, "   - Check device datasheet for max supported speed");

  ESP_LOGI(s_tag, "\n3. Use DMA for Large Transfers");
  ESP_LOGI(s_tag, "   - Offloads CPU during transfer");
  ESP_LOGI(s_tag, "   - Better for >128 byte transfers");
  ESP_LOGI(s_tag, "   - Requires DMA-capable memory buffers");

  ESP_LOGI(s_tag, "\n4. Minimize Transaction Overhead");
  ESP_LOGI(s_tag, "   - Cache register values when possible");
  ESP_LOGI(s_tag, "   - Batch multiple reads/writes");
  ESP_LOGI(s_tag, "   - Use repeated start for write-then-read");

  ESP_LOGI(s_tag, "\n5. Optimize Pull-up Resistors");
  ESP_LOGI(s_tag, "   - Lower resistance = faster rise times");
  ESP_LOGI(s_tag, "   - Balance with power consumption");
  ESP_LOGI(s_tag, "   - 2.2-3.3 kOhm for 400 kHz");

  ESP_LOGI(s_tag, "\n6. Use Async Operations");
  ESP_LOGI(s_tag, "   - Non-blocking for better CPU utilization");
  ESP_LOGI(s_tag, "   - Pipeline multiple operations");
  ESP_LOGI(s_tag, "   - Allows concurrent processing");
}

void i2c_performance_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Perf", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Performance test system initialized");

  /* Create I2C config - standard speed first */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    100000  /* 100 kHz */
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c_config);
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "I2C driver initialized");
  ESP_LOGI(s_tag, "\nStarting performance benchmarks...");
  ESP_LOGI(s_tag, "Test duration: %d ms per benchmark\n", TEST_DURATION_MS);

  /* Run benchmarks */
  benchmark_single_byte_reads(&manager, 100000);
  vTaskDelay(pdMS_TO_TICKS(500));

  benchmark_burst_reads(&manager, 16);
  vTaskDelay(pdMS_TO_TICKS(500));

  benchmark_burst_reads(&manager, 64);
  vTaskDelay(pdMS_TO_TICKS(500));

  benchmark_writes(&manager);
  vTaskDelay(pdMS_TO_TICKS(500));

  compare_clock_speeds(&manager);
  vTaskDelay(pdMS_TO_TICKS(500));

  /* Check bus statistics */
  ESP_LOGI(s_tag, "\n=== Bus Statistics ===");
  star_bus_stats_t stats;
  if (star_bus_stats_get(&manager, "device", &stats) == ESP_OK) {
    ESP_LOGI(s_tag, "Total operations: %llu", stats.total_operations);
    ESP_LOGI(s_tag, "Successful: %llu", stats.total_operations - stats.failed_operations);
    ESP_LOGI(s_tag, "Failed: %llu", stats.failed_operations);
    ESP_LOGI(s_tag, "Average time: %u us", stats.avg_operation_time_us);
    ESP_LOGI(s_tag, "Max time: %u us", stats.max_operation_time_us);
  }

  demonstrate_optimizations();

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nPerformance benchmark complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Performance Benchmark ===\n");
  i2c_performance_example();
}
