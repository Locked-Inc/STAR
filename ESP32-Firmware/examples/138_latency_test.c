/*
 * Example 138: Latency Measurements
 * Measure round-trip latency at different bus speeds
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_i2c.h"
#include "star_bus_spi.h"
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "LATENCY";

#define I2C_BUS         "lat_i2c"
#define SPI_BUS         "lat_spi"
#define TEST_DEVICE (0x50)
#define LATENCY_SAMPLES (1000)

static star_bus_manager_t manager;

/* Latency statistics */
typedef struct {
    int64_t min_us;
    int64_t max_us;
    int64_t sum_us;
    int64_t samples[LATENCY_SAMPLES];
    uint32_t count;
} latency_stats_t;

/* Reset statistics */
static void reset_stats(latency_stats_t *stats) {
    stats->min_us = INT64_MAX;
    stats->max_us = 0;
    stats->sum_us = 0;
    stats->count = 0;
}

/* Record latency sample */
static void record_sample(latency_stats_t *stats, int64_t latency_us) {
    if (stats->count < LATENCY_SAMPLES) {
        stats->samples[stats->count] = latency_us;
    }
    stats->sum_us += latency_us;
    if (latency_us < stats->min_us) stats->min_us = latency_us;
    if (latency_us > stats->max_us) stats->max_us = latency_us;
    stats->count++;
}

/* Calculate percentile */
static int compare_int64(const void *a, const void *b) {
    int64_t va = *(const int64_t *)a;
    int64_t vb = *(const int64_t *)b;
    return (va > vb) - (va < vb);
}

static int64_t get_percentile(latency_stats_t *stats, int percentile) {
    uint32_t count = (stats->count < LATENCY_SAMPLES) ? stats->count : LATENCY_SAMPLES;
    qsort(stats->samples, count, sizeof(int64_t), compare_int64);
    uint32_t idx = (count * percentile) / 100;
    return stats->samples[idx];
}

/* Print latency report */
static void print_latency_report(const char *test_name, latency_stats_t *stats) {
    int64_t avg = stats->sum_us / stats->count;
    int64_t p50 = get_percentile(stats, 50);
    int64_t p95 = get_percentile(stats, 95);
    int64_t p99 = get_percentile(stats, 99);

    ESP_LOGI(TAG, "=== %s Latency Report ===", test_name);
    ESP_LOGI(TAG, "Samples: %lu", (unsigned long)stats->count);
    ESP_LOGI(TAG, "Min: %lld us", stats->min_us);
    ESP_LOGI(TAG, "Max: %lld us (worst-case)", stats->max_us);
    ESP_LOGI(TAG, "Avg: %lld us", avg);
    ESP_LOGI(TAG, "P50: %lld us, P95: %lld us, P99: %lld us", p50, p95, p99);
    ESP_LOGI(TAG, "Jitter: %lld us", stats->max_us - stats->min_us);
}

/* Measure I2C latency */
static void measure_i2c_latency(latency_stats_t *stats) {
    uint8_t buffer[8];
    size_t bytes_read;

    reset_stats(stats);

    for (uint32_t i = 0; i < LATENCY_SAMPLES; i++) {
        int64_t start = esp_timer_get_time();
        star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 1, &bytes_read);
        int64_t latency = esp_timer_get_time() - start;
        record_sample(stats, latency);
    }
}

/* Measure SPI latency */
static void measure_spi_latency(latency_stats_t *stats) {
    uint8_t tx_data = 0xAA;
    uint8_t rx_data;

    reset_stats(stats);

    for (uint32_t i = 0; i < LATENCY_SAMPLES; i++) {
        int64_t start = esp_timer_get_time();
        star_bus_spi_transfer(&manager, SPI_BUS, &tx_data, &rx_data, 1, 100);
        int64_t latency = esp_timer_get_time() - start;
        record_sample(stats, latency);
    }
}

/* Test different payload sizes */
static void test_payload_latency(void) {
    ESP_LOGI(TAG, "========== Payload Size vs Latency ==========");

    static uint8_t tx_buf[256];
    static uint8_t rx_buf[256];
    size_t sizes[] = {1, 4, 16, 32, 64, 128};
    latency_stats_t stats;

    ESP_LOGI(TAG, "Size | Min (us) | Max (us) | Avg (us)");

    for (int s = 0; s < 6; s++) {
        size_t size = sizes[s];
        reset_stats(&stats);

        for (uint32_t i = 0; i < 500; i++) {
            int64_t start = esp_timer_get_time();
            star_bus_spi_transfer(&manager, SPI_BUS, tx_buf, rx_buf, size, 100);
            int64_t latency = esp_timer_get_time() - start;
            record_sample(&stats, latency);
        }

        int64_t avg = stats.sum_us / stats.count;
        ESP_LOGI(TAG, "%4u | %8lld | %8lld | %8lld",
                 (unsigned)size, stats.min_us, stats.max_us, avg);
    }
}

/* Worst-case latency analysis */
static void analyze_worst_case(void) {
    ESP_LOGI(TAG, "========== Worst-Case Latency Analysis ==========");

    latency_stats_t stats;
    uint8_t data[8];
    size_t bytes_read;

    /* Measure under normal conditions */
    ESP_LOGI(TAG, "Normal operation:");
    reset_stats(&stats);
    for (int i = 0; i < 1000; i++) {
        int64_t start = esp_timer_get_time();
        star_bus_i2c_read_raw(&manager, I2C_BUS, data, 1, &bytes_read);
        record_sample(&stats, esp_timer_get_time() - start);
    }
    ESP_LOGI(TAG, "  Max latency: %lld us", stats.max_us);

    /* With task yield (simulating load) */
    ESP_LOGI(TAG, "With task contention:");
    reset_stats(&stats);
    for (int i = 0; i < 1000; i++) {
        if (i % 10 == 0) taskYIELD();
        int64_t start = esp_timer_get_time();
        star_bus_i2c_read_raw(&manager, I2C_BUS, data, 1, &bytes_read);
        record_sample(&stats, esp_timer_get_time() - start);
    }
    ESP_LOGI(TAG, "  Max latency: %lld us", stats.max_us);
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 138: Latency Measurements");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "lat_mgr", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager init failed: %d", ret);
        return;
    }

    /* Initialize I2C bus */
    star_bus_config_t *i2c_cfg = star_bus_config_create_i2c(
        I2C_BUS, I2C_NUM_0, TEST_DEVICE,
        GPIO_NUM_21, GPIO_NUM_22, 400000
    );
    if (i2c_cfg) {
        star_bus_manager_add_bus(&manager, i2c_cfg);
        star_bus_config_init(i2c_cfg, &manager);
    }

    /* Initialize SPI bus */
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10000000,
        .mode = 0,
        .spics_io_num = GPIO_NUM_5,
        .queue_size = 7,
    };

    star_bus_config_t *spi_cfg = star_bus_config_create_spi_device(
        SPI_BUS, SPI2_HOST,
        GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
        SPI_DMA_CH_AUTO, &dev_cfg
    );
    if (spi_cfg) {
        star_bus_manager_add_bus(&manager, spi_cfg);
        star_bus_config_init(spi_cfg, &manager);
    }

    latency_stats_t stats;

    /* I2C latency */
    ESP_LOGI(TAG, "========== I2C Latency Tests ==========");
    measure_i2c_latency(&stats);
    print_latency_report("I2C @ 400 KHz", &stats);

    /* SPI latency */
    ESP_LOGI(TAG, "========== SPI Latency Tests ==========");
    measure_spi_latency(&stats);
    print_latency_report("SPI @ 10 MHz", &stats);

    /* Payload size effect */
    test_payload_latency();

    /* Worst-case analysis */
    analyze_worst_case();

    ESP_LOGI(TAG, "========== Latency Tests Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
