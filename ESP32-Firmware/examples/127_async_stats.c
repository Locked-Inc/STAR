/**
 * @file 127_async_stats.c
 * @brief Async operation statistics and monitoring
 *
 * Demonstrates:
 * - Track pending/completed/failed ops
 * - Performance metrics
 * - Queue depth monitoring
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "ASYNC_STATS";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SENSOR_ADDR      (0x48)

static star_bus_manager_t g_manager;

/* Extended statistics structure */
typedef struct {
    uint32_t pending_ops;
    uint64_t completed_ops;
    uint64_t failed_ops;
    uint64_t cancelled_ops;
    int64_t total_time_us;
    int64_t min_time_us;
    int64_t max_time_us;
    uint32_t queue_high_water;
} extended_stats_t;

static extended_stats_t g_stats = {0};
static int64_t g_last_op_start = 0;

/* Callback for statistics tracking */
static void stats_callback(star_async_handle_t handle, star_async_status_t status,
                            esp_err_t result, void* context)
{
    int64_t elapsed = esp_timer_get_time() - g_last_op_start;

    if (status == STAR_ASYNC_STATUS_COMPLETE && result == ESP_OK) {
        g_stats.total_time_us += elapsed;
        if (g_stats.min_time_us == 0 || elapsed < (int64_t)g_stats.min_time_us) {
            g_stats.min_time_us = elapsed;
        }
        if (elapsed > (int64_t)g_stats.max_time_us) {
            g_stats.max_time_us = elapsed;
        }
    }
}

static void print_extended_stats(void)
{
    ESP_LOGI(TAG, "=== Extended Async Statistics ===");
    ESP_LOGI(TAG, "Operations:");
    ESP_LOGI(TAG, "  Pending: %lu", (unsigned long)g_stats.pending_ops);
    ESP_LOGI(TAG, "  Completed: %llu", (unsigned long long)g_stats.completed_ops);
    ESP_LOGI(TAG, "  Failed: %llu", (unsigned long long)g_stats.failed_ops);
    ESP_LOGI(TAG, "  Cancelled: %llu", (unsigned long long)g_stats.cancelled_ops);
    ESP_LOGI(TAG, "Timing:");
    ESP_LOGI(TAG, "  Total time: %lld us", g_stats.total_time_us);
    ESP_LOGI(TAG, "  Min time: %lld us", g_stats.min_time_us);
    ESP_LOGI(TAG, "  Max time: %lld us", g_stats.max_time_us);
    if (g_stats.completed_ops > 0) {
        ESP_LOGI(TAG, "  Avg time: %lld us",
                 g_stats.total_time_us / (int64_t)g_stats.completed_ops);
    }
    ESP_LOGI(TAG, "Queue:");
    ESP_LOGI(TAG, "  High water mark: %lu", (unsigned long)g_stats.queue_high_water);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Async Statistics Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "stats_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create I2C configuration */
    star_bus_config_t* sensor_config = star_bus_config_create_i2c(
        "sensor", I2C_NUM_0, SENSOR_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    if (sensor_config == NULL) {
        ESP_LOGE(TAG, "Failed to create config");
        return;
    }

    star_bus_manager_add_bus(&g_manager, sensor_config);
    star_bus_config_init(sensor_config, &g_manager);

    /* --- Basic Statistics --- */
    ESP_LOGI(TAG, "\n--- Initial Statistics ---");

    uint32_t pending;
    uint64_t completed, failed, cancelled;

    ret = star_async_get_stats(&g_manager, "sensor", &pending, &completed, &failed, &cancelled);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pending: %lu", (unsigned long)pending);
        ESP_LOGI(TAG, "Completed: %llu", (unsigned long long)completed);
        ESP_LOGI(TAG, "Failed: %llu", (unsigned long long)failed);
        ESP_LOGI(TAG, "Cancelled: %llu", (unsigned long long)cancelled);
    }

    /* --- Run Operations and Track Stats --- */
    ESP_LOGI(TAG, "\n--- Running Operations ---");

    #define NUM_OPERATIONS (20)
    star_async_handle_t handles[NUM_OPERATIONS];
    uint8_t buffers[NUM_OPERATIONS][4];

    /* Submit multiple operations */
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        star_async_config_t config = {
            .timeout_ms = 1000,
            .callback = stats_callback,
            .context = NULL,
            .priority = 5
        };

        g_last_op_start = esp_timer_get_time();
        ret = star_bus_i2c_read_async(&g_manager, "sensor", buffers[i], 4, 0x00,
                                       &config, &handles[i]);

        if (ret == ESP_OK) {
            /* Check queue depth */
            ret = star_async_get_stats(&g_manager, "sensor", &pending, NULL, NULL, NULL);
            if (ret == ESP_OK && pending > g_stats.queue_high_water) {
                g_stats.queue_high_water = pending;
            }
        }

        /* Stagger submissions */
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Submitted %d operations", NUM_OPERATIONS);

    /* Wait for completion */
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        star_async_wait(handles[i], 2000);
    }

    /* Get final stats from library */
    ret = star_async_get_stats(&g_manager, "sensor", &pending, &completed, &failed, &cancelled);
    if (ret == ESP_OK) {
        g_stats.pending_ops = pending;
        g_stats.completed_ops = completed;
        g_stats.failed_ops = failed;
        g_stats.cancelled_ops = cancelled;
    }

    ESP_LOGI(TAG, "\n--- After Operations ---");
    print_extended_stats();

    /* Free handles */
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        star_async_free_handle(handles[i]);
    }

    /* --- Queue Depth Monitoring --- */
    ESP_LOGI(TAG, "\n--- Queue Depth Monitoring ---");

    /* Flood the queue */
    #define FLOOD_COUNT (10)
    star_async_handle_t flood_handles[FLOOD_COUNT];
    uint8_t flood_bufs[FLOOD_COUNT][2];

    ESP_LOGI(TAG, "Flooding queue with %d operations...", FLOOD_COUNT);

    for (int i = 0; i < FLOOD_COUNT; i++) {
        star_async_config_t config = {
            .timeout_ms = 5000,
            .callback = NULL,
            .context = NULL,
            .priority = 5
        };
        star_bus_i2c_read_async(&g_manager, "sensor", flood_bufs[i], 2, 0x00,
                                 &config, &flood_handles[i]);
    }

    /* Monitor queue depth over time */
    for (int sample = 0; sample < 5; sample++) {
        ret = star_async_get_stats(&g_manager, "sensor", &pending, NULL, NULL, NULL);
        ESP_LOGI(TAG, "Sample %d: Queue depth = %lu", sample, (unsigned long)pending);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* Wait and cleanup */
    for (int i = 0; i < FLOOD_COUNT; i++) {
        star_async_wait(flood_handles[i], 2000);
        star_async_free_handle(flood_handles[i]);
    }

    /* --- Performance Metrics --- */
    ESP_LOGI(TAG, "\n--- Performance Metrics ---");

    /* Throughput test */
    int64_t throughput_start = esp_timer_get_time();
    int successful_ops = 0;

    for (int i = 0; i < 50; i++) {
        star_async_handle_t h;
        uint8_t buf[2];

        star_async_config_t config = {
            .timeout_ms = 500,
            .callback = NULL,
            .context = NULL,
            .priority = 5
        };

        ret = star_bus_i2c_read_async(&g_manager, "sensor", buf, 2, 0x00, &config, &h);
        if (ret == ESP_OK) {
            ret = star_async_wait(h, 1000);
            if (ret == ESP_OK) {
                successful_ops++;
            }
            star_async_free_handle(h);
        }
    }

    int64_t throughput_elapsed = esp_timer_get_time() - throughput_start;

    ESP_LOGI(TAG, "Throughput test:");
    ESP_LOGI(TAG, "  Operations: %d/%d successful", successful_ops, 50);
    ESP_LOGI(TAG, "  Total time: %lld ms", throughput_elapsed / 1000);
    if (throughput_elapsed > 0) {
        ESP_LOGI(TAG, "  Ops/second: %.1f",
                 (float)successful_ops * 1000000.0f / throughput_elapsed);
    }

    /* --- Final Summary --- */
    ESP_LOGI(TAG, "\n--- Final Summary ---");
    ESP_LOGI(TAG, "Example demonstrated:");
    ESP_LOGI(TAG, "  - Basic async statistics retrieval");
    ESP_LOGI(TAG, "  - Queue depth monitoring");
    ESP_LOGI(TAG, "  - High water mark tracking");
    ESP_LOGI(TAG, "  - Throughput measurement");

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nAsync statistics example complete");
}
