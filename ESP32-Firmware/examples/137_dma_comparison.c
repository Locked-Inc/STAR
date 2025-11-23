/*
 * Example 137: DMA vs Non-DMA Comparison
 * Compare DMA and polled transfers with throughput and memory analysis
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char *TAG = "DMA_CMP";

#define SPI_DMA_BUS     "spi_dma"
#define SPI_POLLED_BUS  "spi_polled"
#define TEST_ITERATIONS (500)
#define MAX_TRANSFER    (4096)

static star_bus_manager_t manager;

/* Test result structure */
typedef struct {
    const char *mode;
    size_t transfer_size;
    int64_t total_time_us;
    float throughput_mbps;
} dma_test_result_t;

/* Run SPI transfer test */
static void run_transfer_test(const char *bus_name, uint8_t *tx_buf,
                               uint8_t *rx_buf, size_t size,
                               dma_test_result_t *result) {
    int64_t total = 0;

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int64_t start = esp_timer_get_time();
        star_bus_spi_transfer(&manager, bus_name, tx_buf, rx_buf, size, 1000);
        total += esp_timer_get_time() - start;
    }

    result->total_time_us = total;
    result->throughput_mbps = (float)(size * TEST_ITERATIONS * 8) / total;
}

/* Compare DMA and polled modes */
static void compare_modes(size_t transfer_size) {
    dma_test_result_t dma_result = {.mode = "DMA", .transfer_size = transfer_size};
    dma_test_result_t polled_result = {.mode = "Polled", .transfer_size = transfer_size};

    /* Allocate DMA-capable buffers */
    uint8_t *tx_buf = heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);
    uint8_t *rx_buf = heap_caps_malloc(transfer_size, MALLOC_CAP_DMA);

    if (!tx_buf || !rx_buf) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        free(tx_buf);
        free(rx_buf);
        return;
    }

    /* Initialize test pattern */
    for (size_t i = 0; i < transfer_size; i++) {
        tx_buf[i] = i & 0xFF;
    }

    /* Run DMA test */
    run_transfer_test(SPI_DMA_BUS, tx_buf, rx_buf, transfer_size, &dma_result);

    /* Run polled test */
    run_transfer_test(SPI_POLLED_BUS, tx_buf, rx_buf, transfer_size, &polled_result);

    /* Print comparison */
    ESP_LOGI(TAG, "=== Transfer Size: %u bytes ===", (unsigned)transfer_size);
    ESP_LOGI(TAG, "%-10s | Time: %lld us | Throughput: %.2f Mbps",
             dma_result.mode, dma_result.total_time_us, dma_result.throughput_mbps);
    ESP_LOGI(TAG, "%-10s | Time: %lld us | Throughput: %.2f Mbps",
             polled_result.mode, polled_result.total_time_us, polled_result.throughput_mbps);

    /* Speedup calculation */
    float speedup = (float)polled_result.total_time_us / dma_result.total_time_us;
    ESP_LOGI(TAG, "DMA Speedup: %.2fx", speedup);

    free(tx_buf);
    free(rx_buf);
}

/* Memory usage analysis */
static void analyze_memory_usage(void) {
    ESP_LOGI(TAG, "=== Memory Usage Analysis ===");

    size_t total_heap = esp_get_free_heap_size();
    size_t dma_heap = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    ESP_LOGI(TAG, "Total free heap: %u bytes", (unsigned)total_heap);
    ESP_LOGI(TAG, "DMA-capable heap: %u bytes", (unsigned)dma_heap);
    ESP_LOGI(TAG, "Internal heap: %u bytes", (unsigned)internal);

    /* Test DMA buffer allocation limits */
    ESP_LOGI(TAG, "DMA buffer allocation test:");
    size_t test_sizes[] = {256, 512, 1024, 2048, 4096};

    for (int i = 0; i < 5; i++) {
        uint8_t *buf = heap_caps_malloc(test_sizes[i], MALLOC_CAP_DMA);
        if (buf) {
            ESP_LOGI(TAG, "  %5u bytes: OK", (unsigned)test_sizes[i]);
            free(buf);
        } else {
            ESP_LOGI(TAG, "  %5u bytes: FAILED", (unsigned)test_sizes[i]);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 137: DMA vs Non-DMA Comparison");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "dma_cmp", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager init failed: %d", ret);
        return;
    }

    /* Create SPI with DMA */
    spi_device_interface_config_t dma_dev_cfg = {
        .clock_speed_hz = 20000000,
        .mode = 0,
        .spics_io_num = GPIO_NUM_5,
        .queue_size = 7,
    };

    star_bus_config_t *dma_cfg = star_bus_config_create_spi_device(
        SPI_DMA_BUS, SPI2_HOST,
        GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
        SPI_DMA_CH_AUTO, &dma_dev_cfg
    );

    if (dma_cfg) {
        star_bus_manager_add_bus(&manager, dma_cfg);
        star_bus_config_init(dma_cfg, &manager);
    }

    /* Create SPI without DMA (polled mode) */
    spi_device_interface_config_t polled_dev_cfg = {
        .clock_speed_hz = 20000000,
        .mode = 0,
        .spics_io_num = GPIO_NUM_15,
        .queue_size = 1,
    };

    star_bus_config_t *polled_cfg = star_bus_config_create_spi_device(
        SPI_POLLED_BUS, SPI3_HOST,
        GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14,
        0, &polled_dev_cfg  /* DMA channel 0 = no DMA */
    );

    if (polled_cfg) {
        star_bus_manager_add_bus(&manager, polled_cfg);
        star_bus_config_init(polled_cfg, &manager);
    }

    /* Run comparisons */
    ESP_LOGI(TAG, "========== DMA vs Polled Comparison ==========");

    compare_modes(64);
    compare_modes(256);
    compare_modes(1024);

    /* Memory analysis */
    analyze_memory_usage();

    ESP_LOGI(TAG, "========== Comparison Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
