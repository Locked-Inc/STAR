/**
 * @file 126_async_cancel.c
 * @brief Async operation cancellation scenarios
 *
 * Demonstrates:
 * - Cancel pending operations
 * - Timeout cancellation
 * - Graceful shutdown
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "ASYNC_CANCEL";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SENSOR_ADDR      (0x48)

static star_bus_manager_t g_manager;

/* Callback for cancelled operations */
static void cancel_callback(star_async_handle_t handle, star_async_status_t status,
                             esp_err_t result, void* context)
{
    const char* op_name = (const char*)context;

    switch (status) {
        case STAR_ASYNC_STATUS_COMPLETE:
            ESP_LOGI(TAG, "%s: completed successfully", op_name);
            break;
        case STAR_ASYNC_STATUS_CANCELLED:
            ESP_LOGW(TAG, "%s: was cancelled", op_name);
            break;
        case STAR_ASYNC_STATUS_TIMEOUT:
            ESP_LOGW(TAG, "%s: timed out", op_name);
            break;
        case STAR_ASYNC_STATUS_ERROR:
            ESP_LOGE(TAG, "%s: error - %s", op_name, esp_err_to_name(result));
            break;
        default:
            ESP_LOGI(TAG, "%s: status %d", op_name, status);
            break;
    }
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Async Cancellation Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "cancel_demo", NULL, NULL);
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

    /* --- Basic Cancellation --- */
    ESP_LOGI(TAG, "\n--- Basic Cancellation ---");

    star_async_handle_t handle;
    uint8_t buffer[16];

    star_async_config_t config = {
        .timeout_ms = 5000,
        .callback = cancel_callback,
        .context = "basic_op",
        .priority = 5
    };

    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffer, 16, 0x00, &config, &handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Operation submitted");

        /* Cancel immediately */
        ret = star_async_cancel(handle);
        ESP_LOGI(TAG, "Cancel request: %s", esp_err_to_name(ret));

        /* Check status */
        star_async_status_t status = star_async_get_status(handle);
        ESP_LOGI(TAG, "Status after cancel: %d (CANCELLED=%d)",
                 status, STAR_ASYNC_STATUS_CANCELLED);

        star_async_free_handle(handle);
    }

    /* --- Cancel After Delay --- */
    ESP_LOGI(TAG, "\n--- Cancel After Delay ---");

    star_async_config_t delay_config = {
        .timeout_ms = 5000,
        .callback = cancel_callback,
        .context = "delayed_cancel",
        .priority = 5
    };

    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffer, 16, 0x00, &delay_config, &handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Operation submitted, waiting 100ms before cancel...");
        vTaskDelay(pdMS_TO_TICKS(100));

        ret = star_async_cancel(handle);
        ESP_LOGI(TAG, "Cancel after delay: %s", esp_err_to_name(ret));

        /* Wait for callback */
        vTaskDelay(pdMS_TO_TICKS(100));
        star_async_free_handle(handle);
    }

    /* --- Timeout Cancellation --- */
    ESP_LOGI(TAG, "\n--- Timeout Cancellation ---");

    star_async_config_t timeout_config = {
        .timeout_ms = 50,  /* Very short timeout */
        .callback = cancel_callback,
        .context = "timeout_op",
        .priority = 5
    };

    ret = star_bus_i2c_read_async(&g_manager, "sensor", buffer, 16, 0x00, &timeout_config, &handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Operation submitted with 50ms timeout");

        /* Wait longer than timeout */
        vTaskDelay(pdMS_TO_TICKS(200));

        star_async_status_t status = star_async_get_status(handle);
        ESP_LOGI(TAG, "Status after timeout: %d (TIMEOUT=%d)",
                 status, STAR_ASYNC_STATUS_TIMEOUT);

        star_async_free_handle(handle);
    }

    /* --- Cancel Multiple Operations --- */
    ESP_LOGI(TAG, "\n--- Cancel Multiple Operations ---");

    #define NUM_OPS (5)
    star_async_handle_t handles[NUM_OPS];
    uint8_t buffers[NUM_OPS][4];
    static const char* op_names[NUM_OPS] = {"op1", "op2", "op3", "op4", "op5"};

    /* Submit multiple operations */
    for (int i = 0; i < NUM_OPS; i++) {
        star_async_config_t cfg = {
            .timeout_ms = 5000,
            .callback = cancel_callback,
            .context = (void*)op_names[i],
            .priority = 5
        };
        ret = star_bus_i2c_read_async(&g_manager, "sensor", buffers[i], 4, 0x00,
                                       &cfg, &handles[i]);
        ESP_LOGI(TAG, "Submitted %s: %s", op_names[i], esp_err_to_name(ret));
    }

    /* Cancel all pending */
    ESP_LOGI(TAG, "Cancelling all operations...");
    for (int i = 0; i < NUM_OPS; i++) {
        ret = star_async_cancel(handles[i]);
        ESP_LOGI(TAG, "Cancelled %s: %s", op_names[i], esp_err_to_name(ret));
    }

    /* Wait for callbacks */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Free handles */
    for (int i = 0; i < NUM_OPS; i++) {
        star_async_free_handle(handles[i]);
    }

    /* --- Graceful Shutdown Pattern --- */
    ESP_LOGI(TAG, "\n--- Graceful Shutdown Pattern ---");

    /* Submit long-running operations */
    star_async_handle_t shutdown_handles[3];
    uint8_t shutdown_bufs[3][8];

    for (int i = 0; i < 3; i++) {
        star_async_config_t cfg = {
            .timeout_ms = 10000,
            .callback = NULL,
            .context = NULL,
            .priority = 5
        };
        star_bus_i2c_read_async(&g_manager, "sensor", shutdown_bufs[i], 8, 0x00,
                                 &cfg, &shutdown_handles[i]);
    }

    ESP_LOGI(TAG, "Starting graceful shutdown...");

    int64_t shutdown_start = esp_timer_get_time();

    /* Cancel all */
    for (int i = 0; i < 3; i++) {
        star_async_cancel(shutdown_handles[i]);
    }

    /* Wait for cancellations to complete */
    for (int i = 0; i < 3; i++) {
        star_async_wait(shutdown_handles[i], 1000);
    }

    int64_t shutdown_time = esp_timer_get_time() - shutdown_start;
    ESP_LOGI(TAG, "Shutdown completed in %lld us", shutdown_time);

    /* Free resources */
    for (int i = 0; i < 3; i++) {
        star_async_free_handle(shutdown_handles[i]);
    }

    /* --- Cancellation Statistics --- */
    ESP_LOGI(TAG, "\n--- Cancellation Statistics ---");

    uint32_t pending;
    uint64_t completed, failed, cancelled;

    ret = star_async_get_stats(&g_manager, "sensor", &pending, &completed, &failed, &cancelled);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pending: %lu", (unsigned long)pending);
        ESP_LOGI(TAG, "Completed: %llu", (unsigned long long)completed);
        ESP_LOGI(TAG, "Failed: %llu", (unsigned long long)failed);
        ESP_LOGI(TAG, "Cancelled: %llu", (unsigned long long)cancelled);
    }

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nAsync cancellation example complete");
}
