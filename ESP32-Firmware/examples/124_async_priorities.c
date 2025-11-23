/**
 * @file 124_async_priorities.c
 * @brief Async priority handling demonstration
 *
 * Demonstrates:
 * - High/low priority async operations
 * - Priority queue demonstration
 * - Completion order tracking
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "ASYNC_PRIORITIES";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SENSOR_ADDR      (0x48)

/* Priority levels */
#define PRIORITY_LOW (0)
#define PRIORITY_NORMAL (5)
#define PRIORITY_HIGH (10)
#define PRIORITY_URGENT (15)

static star_bus_manager_t g_manager;

/* Track completion order */
#define MAX_COMPLETIONS (10)
static int g_completion_order[MAX_COMPLETIONS];
static int g_completion_count = 0;
static int64_t g_start_time;

typedef struct {
    const char* name;
    int priority;
    int order_submitted;
} op_context_t;

/* Callback to track completion order */
static void priority_callback(star_async_handle_t handle, star_async_status_t status,
                               esp_err_t result, void* context)
{
    op_context_t* ctx = (op_context_t*)context;
    int64_t elapsed = esp_timer_get_time() - g_start_time;

    if (g_completion_count < MAX_COMPLETIONS) {
        g_completion_order[g_completion_count++] = ctx->order_submitted;
    }

    ESP_LOGI(TAG, "[%lld us] %s (priority %d, submitted #%d) completed: %s",
             elapsed, ctx->name, ctx->priority, ctx->order_submitted,
             result == ESP_OK ? "OK" : esp_err_to_name(result));
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Async Priority Handling Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "priority_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create I2C configuration */
    star_bus_config_t* sensor_config = star_bus_config_create_i2c(
        "sensor", I2C_NUM_0, SENSOR_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

    if (sensor_config == NULL) {
        ESP_LOGE(TAG, "Failed to create config");
        return;
    }

    star_bus_manager_add_bus(&g_manager, sensor_config);
    star_bus_config_init(sensor_config, &g_manager);

    /* --- Basic Priority Demonstration --- */
    ESP_LOGI(TAG, "\n--- Basic Priority Demo ---");
    ESP_LOGI(TAG, "Submitting operations with different priorities");

    g_completion_count = 0;
    g_start_time = esp_timer_get_time();

    /* Create contexts for tracking */
    static op_context_t contexts[4] = {
        {"Low-1", PRIORITY_LOW, 1},
        {"Normal-2", PRIORITY_NORMAL, 2},
        {"High-3", PRIORITY_HIGH, 3},
        {"Urgent-4", PRIORITY_URGENT, 4}
    };

    star_async_handle_t handles[4];
    uint8_t buffers[4][2];

    /* Submit low priority first */
    star_async_config_t low_config = {
        .timeout_ms = 1000,
        .callback = priority_callback,
        .context = &contexts[0],
        .priority = PRIORITY_LOW
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffers[0], 2, 0x00,
                                   &low_config, &handles[0]);
    ESP_LOGI(TAG, "Submitted: %s (priority %d)", contexts[0].name, PRIORITY_LOW);

    /* Submit normal priority */
    star_async_config_t normal_config = {
        .timeout_ms = 1000,
        .callback = priority_callback,
        .context = &contexts[1],
        .priority = PRIORITY_NORMAL
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffers[1], 2, 0x00,
                                   &normal_config, &handles[1]);
    ESP_LOGI(TAG, "Submitted: %s (priority %d)", contexts[1].name, PRIORITY_NORMAL);

    /* Submit high priority */
    star_async_config_t high_config = {
        .timeout_ms = 1000,
        .callback = priority_callback,
        .context = &contexts[2],
        .priority = PRIORITY_HIGH
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffers[2], 2, 0x00,
                                   &high_config, &handles[2]);
    ESP_LOGI(TAG, "Submitted: %s (priority %d)", contexts[2].name, PRIORITY_HIGH);

    /* Submit urgent priority */
    star_async_config_t urgent_config = {
        .timeout_ms = 1000,
        .callback = priority_callback,
        .context = &contexts[3],
        .priority = PRIORITY_URGENT
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffers[3], 2, 0x00,
                                   &urgent_config, &handles[3]);
    ESP_LOGI(TAG, "Submitted: %s (priority %d)", contexts[3].name, PRIORITY_URGENT);

    /* Wait for all to complete */
    for (int i = 0; i < 4; i++) {
        star_async_wait(handles[i], 2000);
        star_async_free_handle(handles[i]);
    }

    ESP_LOGI(TAG, "\nCompletion order (by submission #): ");
    for (int i = 0; i < g_completion_count; i++) {
        ESP_LOGI(TAG, "  %d: Submission #%d", i + 1, g_completion_order[i]);
    }

    /* --- Multiple Rounds with Different Priorities --- */
    ESP_LOGI(TAG, "\n--- Multiple Priority Rounds ---");

    for (int round = 0; round < 3; round++) {
        ESP_LOGI(TAG, "Round %d:", round + 1);

        g_completion_count = 0;
        g_start_time = esp_timer_get_time();

        /* Vary priorities each round */
        uint8_t priorities[] = {
            (round * 3) % 16,
            (round * 5 + 2) % 16,
            (round * 7 + 4) % 16
        };

        static op_context_t round_contexts[3];
        star_async_handle_t round_handles[3];
        uint8_t round_buffers[3][2];

        for (int i = 0; i < 3; i++) {
            round_contexts[i].name = "op";
            round_contexts[i].priority = priorities[i];
            round_contexts[i].order_submitted = i + 1;

            star_async_config_t cfg = {
                .timeout_ms = 1000,
                .callback = priority_callback,
                .context = &round_contexts[i],
                .priority = priorities[i]
            };

            star_bus_i2c_read_async(&g_manager, "sensor", round_buffers[i], 2, 0x00,
                                     &cfg, &round_handles[i]);
        }

        /* Wait for completion */
        for (int i = 0; i < 3; i++) {
            star_async_wait(round_handles[i], 2000);
            star_async_free_handle(round_handles[i]);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* --- Priority Statistics --- */
    ESP_LOGI(TAG, "\n--- Async Statistics ---");

    uint32_t pending;
    uint64_t completed, failed, cancelled;

    ret = star_async_get_stats(&g_manager, "sensor", &pending, &completed, &failed, &cancelled);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pending: %lu", (unsigned long)pending);
        ESP_LOGI(TAG, "Completed: %llu", (unsigned long long)completed);
        ESP_LOGI(TAG, "Failed: %llu", (unsigned long long)failed);
        ESP_LOGI(TAG, "Cancelled: %llu", (unsigned long long)cancelled);
    }

    /* --- Best Practices --- */
    ESP_LOGI(TAG, "\n--- Priority Best Practices ---");
    ESP_LOGI(TAG, "1. Use priority 0-4 for background tasks");
    ESP_LOGI(TAG, "2. Use priority 5-9 for normal operations");
    ESP_LOGI(TAG, "3. Use priority 10-14 for time-sensitive ops");
    ESP_LOGI(TAG, "4. Use priority 15+ for critical/urgent ops");
    ESP_LOGI(TAG, "5. Avoid priority inversion by design");

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nAsync priorities example complete");
}
