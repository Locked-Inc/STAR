/*
 * Example 144: Memory Optimization
 * Track heap usage, DMA buffer allocation, memory leak detection
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
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char *TAG = "MEM_USAGE";

#define I2C_BUS         "mem_i2c"
#define SPI_BUS         "mem_spi"
#define TEST_DEVICE (0x50)

static star_bus_manager_t manager;

/* Memory snapshot structure */
typedef struct {
    const char *label;
    size_t free_heap;
    size_t free_dma;
    size_t free_internal;
} mem_snapshot_t;

static mem_snapshot_t snapshots[16];
static int snapshot_count = 0;

/* Take memory snapshot */
static void take_snapshot(const char *label) {
    if (snapshot_count >= 16) return;

    mem_snapshot_t *s = &snapshots[snapshot_count++];
    s->label = label;
    s->free_heap = esp_get_free_heap_size();
    s->free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    s->free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    ESP_LOGI(TAG, "[MEM] %s: heap=%u, dma=%u, internal=%u",
             label, (unsigned)s->free_heap, (unsigned)s->free_dma,
             (unsigned)s->free_internal);
}

/* Print memory delta */
static void print_memory_delta(const char *from, const char *to) {
    mem_snapshot_t *s1 = NULL, *s2 = NULL;

    for (int i = 0; i < snapshot_count; i++) {
        if (strcmp(snapshots[i].label, from) == 0) s1 = &snapshots[i];
        if (strcmp(snapshots[i].label, to) == 0) s2 = &snapshots[i];
    }

    if (s1 && s2) {
        int heap_delta = (int)s2->free_heap - (int)s1->free_heap;
        int dma_delta = (int)s2->free_dma - (int)s1->free_dma;
        ESP_LOGI(TAG, "Delta %s -> %s: heap=%+d, dma=%+d",
                 from, to, heap_delta, dma_delta);
    }
}

/* Measure memory usage of initialization */
static void measure_init_memory(void) {
    ESP_LOGI(TAG, "=== Initialization Memory Usage ===");

    take_snapshot("before_manager");

    /* Initialize manager */
    star_bus_manager_init(&manager, "mem_mgr", NULL, NULL);
    take_snapshot("after_manager");

    /* Initialize I2C */
    star_bus_config_t *i2c_cfg = star_bus_config_create_i2c(
        I2C_BUS, I2C_NUM_0, TEST_DEVICE,
        GPIO_NUM_21, GPIO_NUM_22, 400000
    );
    if (i2c_cfg) {
        star_bus_manager_add_bus(&manager, i2c_cfg);
        star_bus_config_init(i2c_cfg, &manager);
    }
    take_snapshot("after_i2c");

    /* Initialize SPI with DMA */
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
    take_snapshot("after_spi");

    /* Print deltas */
    print_memory_delta("before_manager", "after_manager");
    print_memory_delta("after_manager", "after_i2c");
    print_memory_delta("after_i2c", "after_spi");
    print_memory_delta("before_manager", "after_spi");
}

/* Test DMA buffer allocation */
static void test_dma_allocation(void) {
    ESP_LOGI(TAG, "=== DMA Buffer Allocation Test ===");

    size_t sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
    void *buffers[7] = {NULL};

    ESP_LOGI(TAG, "Size (B) | Allocated | DMA Free After");

    take_snapshot("dma_before");

    for (int i = 0; i < 7; i++) {
        buffers[i] = heap_caps_malloc(sizes[i], MALLOC_CAP_DMA);
        size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);

        ESP_LOGI(TAG, "%8u | %9s | %u",
                 (unsigned)sizes[i],
                 buffers[i] ? "YES" : "NO",
                 (unsigned)dma_free);
    }

    /* Free all buffers */
    for (int i = 0; i < 7; i++) {
        if (buffers[i]) free(buffers[i]);
    }

    take_snapshot("dma_after");
    print_memory_delta("dma_before", "dma_after");
}

/* Memory leak detection test */
static void test_memory_leaks(void) {
    ESP_LOGI(TAG, "=== Memory Leak Detection Test ===");

    take_snapshot("leak_start");

    uint8_t buffer[64];
    size_t bytes;

    /* Perform many operations */
    ESP_LOGI(TAG, "Performing 1000 operations...");
    for (int i = 0; i < 1000; i++) {
        star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 64, &bytes);
        star_bus_i2c_write(&manager, I2C_BUS, buffer, 64, 0x00, &bytes);
    }

    take_snapshot("leak_end");

    /* Check for leaks */
    print_memory_delta("leak_start", "leak_end");

    mem_snapshot_t *start = NULL, *end = NULL;
    for (int i = 0; i < snapshot_count; i++) {
        if (strcmp(snapshots[i].label, "leak_start") == 0) start = &snapshots[i];
        if (strcmp(snapshots[i].label, "leak_end") == 0) end = &snapshots[i];
    }

    if (start && end) {
        int leak = (int)start->free_heap - (int)end->free_heap;
        if (leak > 100) {
            ESP_LOGW(TAG, "WARNING: Potential memory leak detected: %d bytes", leak);
        } else {
            ESP_LOGI(TAG, "No significant memory leak detected");
        }
    }
}

/* Print memory optimization recommendations */
static void print_recommendations(void) {
    ESP_LOGI(TAG, "========== Memory Optimization Recommendations ==========");

    size_t total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t free_mem = esp_get_free_heap_size();
    size_t used = total - free_mem;
    float usage_pct = (float)used * 100 / total;

    ESP_LOGI(TAG, "Current Memory Status:");
    ESP_LOGI(TAG, "  Total heap: %u bytes", (unsigned)total);
    ESP_LOGI(TAG, "  Used: %u bytes (%.1f%%)", (unsigned)used, usage_pct);
    ESP_LOGI(TAG, "  Free: %u bytes", (unsigned)free_mem);
    ESP_LOGI(TAG, "  Minimum free (watermark): %u bytes",
             (unsigned)esp_get_minimum_free_heap_size());

    ESP_LOGI(TAG, "Recommendations:");

    if (usage_pct > 80) {
        ESP_LOGW(TAG, "  [!] High memory usage - consider reducing buffer sizes");
    }

    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    if (dma_free < 4096) {
        ESP_LOGW(TAG, "  [!] Low DMA memory - reduce DMA buffer count or size");
    }

    ESP_LOGI(TAG, "  - Use static buffers for frequently used data");
    ESP_LOGI(TAG, "  - Share buffers between operations when possible");
    ESP_LOGI(TAG, "  - Release unused buses to free memory");
    ESP_LOGI(TAG, "  - Use minimum necessary buffer sizes");
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 144: Memory Optimization");

    /* Run memory tests */
    measure_init_memory();
    test_dma_allocation();
    test_memory_leaks();

    /* Print recommendations */
    print_recommendations();

    ESP_LOGI(TAG, "========== Memory Analysis Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);

    take_snapshot("final_cleanup");
    print_memory_delta("before_manager", "final_cleanup");
}
