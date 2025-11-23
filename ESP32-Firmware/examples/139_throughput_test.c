/*
 * Example 139: Throughput Testing
 * Maximum data rates, sustained throughput, and buffer size effects
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include "star_bus_i2c.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char *TAG = "THROUGHPUT";

#define SPI_BUS         "tput_spi"
#define I2C_BUS         "tput_i2c"
#define TEST_DEVICE (0x50)
#define SUSTAINED_SECS  (5)

static star_bus_manager_t manager;

/* Throughput result */
typedef struct {
    const char *test_name;
    size_t buffer_size;
    uint64_t bytes_transferred;
    int64_t duration_us;
    float throughput_kbps;
} throughput_result_t;

/* Calculate and print throughput */
static void print_throughput(throughput_result_t *r) {
    r->throughput_kbps = (float)(r->bytes_transferred * 8000) / r->duration_us;
    ESP_LOGI(TAG, "%-16s | %4u B | %8.2f Kbps",
             r->test_name, (unsigned)r->buffer_size, r->throughput_kbps);
}

/* Measure maximum burst throughput */
static void measure_burst_throughput(uint8_t *tx_buf, uint8_t *rx_buf,
                                      size_t size, throughput_result_t *result) {
    uint32_t iterations = 1000;
    result->buffer_size = size;
    result->bytes_transferred = 0;

    int64_t start = esp_timer_get_time();

    for (uint32_t i = 0; i < iterations; i++) {
        if (star_bus_spi_transfer(&manager, SPI_BUS, tx_buf, rx_buf, size, 1000) == ESP_OK) {
            result->bytes_transferred += size * 2;  /* Full duplex */
        }
    }

    result->duration_us = esp_timer_get_time() - start;
}

/* Measure sustained throughput over time */
static void measure_sustained_throughput(void) {
    ESP_LOGI(TAG, "=== Sustained Throughput Test (%d seconds) ===", SUSTAINED_SECS);

    uint8_t *tx_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA);
    uint8_t *rx_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA);

    if (!tx_buf || !rx_buf) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        free(tx_buf);
        free(rx_buf);
        return;
    }

    memset(tx_buf, 0xAA, 1024);

    uint64_t total_bytes = 0;
    uint32_t transfer_count = 0;
    int64_t start_time = esp_timer_get_time();
    int64_t end_time = start_time + (SUSTAINED_SECS * 1000000LL);

    ESP_LOGI(TAG, "Second | Transfers | Rate (Kbps)");

    int last_second = 0;
    uint64_t second_bytes = 0;

    while (esp_timer_get_time() < end_time) {
        if (star_bus_spi_transfer(&manager, SPI_BUS, tx_buf, rx_buf, 1024, 1000) == ESP_OK) {
            total_bytes += 2048;
            second_bytes += 2048;
            transfer_count++;
        }

        int current_second = (esp_timer_get_time() - start_time) / 1000000;
        if (current_second > last_second) {
            float kbps = (float)(second_bytes * 8) / 1000;
            ESP_LOGI(TAG, "%6d | %9lu | %10.2f",
                     last_second + 1, (unsigned long)transfer_count, kbps);
            last_second = current_second;
            second_bytes = 0;
        }
    }

    int64_t total_time = esp_timer_get_time() - start_time;
    float avg_kbps = (float)(total_bytes * 8000) / total_time;

    ESP_LOGI(TAG, "Total: %lu transfers, %.2f Kbps avg",
             (unsigned long)transfer_count, avg_kbps);

    free(tx_buf);
    free(rx_buf);
}

/* Test buffer size effects on throughput */
static void test_buffer_size_effects(void) {
    ESP_LOGI(TAG, "=== Buffer Size Effect on Throughput ===");
    ESP_LOGI(TAG, "Test             | Size | Throughput");

    size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024};
    uint8_t *tx_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA);
    uint8_t *rx_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA);

    if (!tx_buf || !rx_buf) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        free(tx_buf);
        free(rx_buf);
        return;
    }

    for (int i = 0; i < 8; i++) {
        memset(tx_buf, i, sizes[i]);
        throughput_result_t result = {.test_name = "SPI Transfer"};

        measure_burst_throughput(tx_buf, rx_buf, sizes[i], &result);
        print_throughput(&result);
    }

    free(tx_buf);
    free(rx_buf);
}

/* I2C throughput test */
static void test_i2c_throughput(void) {
    ESP_LOGI(TAG, "=== I2C Throughput Test ===");

    uint8_t buffer[32];
    size_t bytes_written;
    throughput_result_t result = {.test_name = "I2C Write", .buffer_size = 32};
    uint32_t iterations = 500;

    int64_t start = esp_timer_get_time();
    for (uint32_t i = 0; i < iterations; i++) {
        if (star_bus_i2c_write(&manager, I2C_BUS, buffer, 32, 0x00, &bytes_written) == ESP_OK) {
            result.bytes_transferred += bytes_written;
        }
    }
    result.duration_us = esp_timer_get_time() - start;

    print_throughput(&result);
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 139: Throughput Testing");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "tput_mgr", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager init failed: %d", ret);
        return;
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

    /* Initialize I2C bus */
    star_bus_config_t *i2c_cfg = star_bus_config_create_i2c(
        I2C_BUS, I2C_NUM_0, TEST_DEVICE,
        GPIO_NUM_21, GPIO_NUM_22, 400000
    );
    if (i2c_cfg) {
        star_bus_manager_add_bus(&manager, i2c_cfg);
        star_bus_config_init(i2c_cfg, &manager);
    }

    /* Run tests */
    test_buffer_size_effects();
    test_i2c_throughput();
    measure_sustained_throughput();

    ESP_LOGI(TAG, "========== Throughput Tests Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
