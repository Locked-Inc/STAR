/*
 * Example 136: Comprehensive Performance Benchmarks
 * Benchmark I2C, SPI operations with latency and throughput measurements
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_i2c.h"
#include "star_bus_spi.h"
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "PERF_BENCH";

#define I2C_BUS_NAME    "bench_i2c"
#define SPI_BUS_NAME    "bench_spi"
#define TEST_DEVICE (0x50)
#define NUM_ITERATIONS (1000)
#define MAX_BUFFER_SIZE (256)

static star_bus_manager_t manager;

/* Benchmark result structure */
typedef struct {
    const char *test_name;
    uint32_t iterations;
    int64_t total_time_us;
    int64_t min_time_us;
    int64_t max_time_us;
    uint32_t bytes_transferred;
    uint32_t errors;
} bench_result_t;

/* Calculate and print statistics */
static void print_bench_result(bench_result_t *result) {
    int64_t avg_us = result->total_time_us / result->iterations;
    float throughput_kbps = 0;

    if (result->bytes_transferred > 0 && result->total_time_us > 0) {
        throughput_kbps = (float)(result->bytes_transferred * 8000) / result->total_time_us;
    }

    ESP_LOGI(TAG, "=== %s ===", result->test_name);
    ESP_LOGI(TAG, "Iterations: %lu", (unsigned long)result->iterations);
    ESP_LOGI(TAG, "Total time: %lld us", result->total_time_us);
    ESP_LOGI(TAG, "Average: %lld us", avg_us);
    ESP_LOGI(TAG, "Min: %lld us, Max: %lld us", result->min_time_us, result->max_time_us);
    ESP_LOGI(TAG, "Bytes transferred: %lu", (unsigned long)result->bytes_transferred);
    ESP_LOGI(TAG, "Throughput: %.2f Kbps", throughput_kbps);
    ESP_LOGI(TAG, "Errors: %lu (%.2f%%)", (unsigned long)result->errors,
             (float)result->errors * 100 / result->iterations);
}

/* Benchmark I2C read operations */
static void bench_i2c_read(uint8_t *buffer, size_t size, bench_result_t *result) {
    result->test_name = "I2C Read";
    result->iterations = NUM_ITERATIONS;
    result->total_time_us = 0;
    result->min_time_us = INT64_MAX;
    result->max_time_us = 0;
    result->bytes_transferred = 0;
    result->errors = 0;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        size_t bytes_read;
        int64_t start = esp_timer_get_time();
        esp_err_t ret = star_bus_i2c_read_raw(&manager, I2C_BUS_NAME, buffer, size, &bytes_read);
        int64_t elapsed = esp_timer_get_time() - start;

        result->total_time_us += elapsed;
        if (elapsed < result->min_time_us) result->min_time_us = elapsed;
        if (elapsed > result->max_time_us) result->max_time_us = elapsed;

        if (ret == ESP_OK) {
            result->bytes_transferred += bytes_read;
        } else {
            result->errors++;
        }
    }
}

/* Benchmark I2C write operations */
static void bench_i2c_write(uint8_t *buffer, size_t size, bench_result_t *result) {
    result->test_name = "I2C Write";
    result->iterations = NUM_ITERATIONS;
    result->total_time_us = 0;
    result->min_time_us = INT64_MAX;
    result->max_time_us = 0;
    result->bytes_transferred = 0;
    result->errors = 0;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        size_t bytes_written;
        int64_t start = esp_timer_get_time();
        esp_err_t ret = star_bus_i2c_write(&manager, I2C_BUS_NAME, buffer, size, 0x00, &bytes_written);
        int64_t elapsed = esp_timer_get_time() - start;

        result->total_time_us += elapsed;
        if (elapsed < result->min_time_us) result->min_time_us = elapsed;
        if (elapsed > result->max_time_us) result->max_time_us = elapsed;

        if (ret == ESP_OK) {
            result->bytes_transferred += bytes_written;
        } else {
            result->errors++;
        }
    }
}

/* Benchmark SPI transfer operations */
static void bench_spi_transfer(uint8_t *tx_buf, uint8_t *rx_buf, size_t size,
                                bench_result_t *result) {
    result->test_name = "SPI Transfer";
    result->iterations = NUM_ITERATIONS;
    result->total_time_us = 0;
    result->min_time_us = INT64_MAX;
    result->max_time_us = 0;
    result->bytes_transferred = 0;
    result->errors = 0;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        int64_t start = esp_timer_get_time();
        esp_err_t ret = star_bus_spi_transfer(&manager, SPI_BUS_NAME, tx_buf, rx_buf, size, 1000);
        int64_t elapsed = esp_timer_get_time() - start;

        result->total_time_us += elapsed;
        if (elapsed < result->min_time_us) result->min_time_us = elapsed;
        if (elapsed > result->max_time_us) result->max_time_us = elapsed;

        if (ret == ESP_OK) {
            result->bytes_transferred += size * 2;  /* TX + RX */
        } else {
            result->errors++;
        }
    }
}

/* Compare different buffer sizes */
static void benchmark_buffer_sizes(void) {
    static uint8_t tx_buffer[MAX_BUFFER_SIZE];
    static uint8_t rx_buffer[MAX_BUFFER_SIZE];
    bench_result_t result;
    size_t sizes[] = {1, 4, 16, 32, 64, 128};

    ESP_LOGI(TAG, "========== Buffer Size Comparison ==========");

    /* Initialize test data */
    for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
        tx_buffer[i] = i & 0xFF;
    }

    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t size = sizes[s];

        ESP_LOGI(TAG, "--- Testing %u byte transfers ---", (unsigned)size);

        /* I2C benchmarks */
        bench_i2c_write(tx_buffer, size, &result);
        print_bench_result(&result);

        bench_i2c_read(rx_buffer, size, &result);
        print_bench_result(&result);

        /* SPI benchmarks */
        bench_spi_transfer(tx_buffer, rx_buffer, size, &result);
        print_bench_result(&result);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 136: Comprehensive Performance Benchmarks");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "bench_mgr", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager init failed: %d", ret);
        return;
    }

    /* Create and initialize I2C bus */
    star_bus_config_t *i2c_cfg = star_bus_config_create_i2c(
        I2C_BUS_NAME,
        I2C_NUM_0,
        TEST_DEVICE,
        GPIO_NUM_21,
        GPIO_NUM_22,
        400000
    );

    if (i2c_cfg) {
        star_bus_manager_add_bus(&manager, i2c_cfg);
        star_bus_config_init(i2c_cfg, &manager);
    }

    /* Create and initialize SPI device */
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10000000,
        .mode = 0,
        .spics_io_num = GPIO_NUM_5,
        .queue_size = 7,
    };

    star_bus_config_t *spi_cfg = star_bus_config_create_spi_device(
        SPI_BUS_NAME,
        SPI2_HOST,
        GPIO_NUM_23,
        GPIO_NUM_19,
        GPIO_NUM_18,
        SPI_DMA_CH_AUTO,
        &dev_cfg
    );

    if (spi_cfg) {
        star_bus_manager_add_bus(&manager, spi_cfg);
        star_bus_config_init(spi_cfg, &manager);
    }

    /* Run benchmarks */
    benchmark_buffer_sizes();

    ESP_LOGI(TAG, "========== Benchmark Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
