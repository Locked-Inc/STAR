/* src/main.c - STAR ESP32-S3 Quad Motor Control Application Entry Point */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "communication_task.h"
#include "motor_control_task.h"
#include "system_config.h"
#include "system_context.h"
#include "system_init.h"
#include "telemetry_task.h"

/* Global system context */
static system_context_t g_sys_ctx;

extern const char* const s_TAG;

void app_main(void)
{
    ESP_LOGI(s_TAG, "=== STAR ESP32-S3 Quad Motor Firmware Starting ===");
    ESP_LOGI(s_TAG, "Version: %s", STAR_FIRMWARE_VERSION);
    ESP_LOGI(s_TAG, "Build: %s %s", __DATE__, __TIME__);

    /* ===================================================================== */
    /* Initialize Entire System                                              */
    /* ===================================================================== */

    esp_err_t ret = system_init(&g_sys_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(s_TAG, "System initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(s_TAG, "System initialization complete");

    /* ===================================================================== */
    /* Create FreeRTOS Tasks                                                 */
    /* ===================================================================== */

    /* Create motor control task (highest priority) */
    BaseType_t task_ret = xTaskCreate(motor_control_task,
                                      "motor_ctrl",
                                      s_motor_task_stack_size,
                                      &g_sys_ctx,
                                      s_motor_task_priority,
                                      &g_sys_ctx.motor_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(s_TAG, "Failed to create motor control task");
        return;
    }

    /* Create telemetry task (medium priority) */
    task_ret = xTaskCreate(telemetry_task,
                           "telemetry",
                           s_telemetry_task_stack_size,
                           &g_sys_ctx,
                           s_telemetry_task_priority,
                           &g_sys_ctx.telemetry_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(s_TAG, "Failed to create telemetry task");
        return;
    }

    /* Create communication task (high priority for responsive commands) */
    task_ret = xTaskCreate(communication_task,
                           "comm",
                           s_comm_task_stack_size,
                           &g_sys_ctx,
                           s_comm_task_priority,
                           &g_sys_ctx.comm_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(s_TAG, "Failed to create communication task");
        return;
    }

    ESP_LOGI(s_TAG, "All tasks created successfully");
    ESP_LOGI(s_TAG, "=== System initialization complete ===");

    /* ===================================================================== */
    /* Main Loop - Heartbeat                                                 */
    /* ===================================================================== */

    ESP_LOGI(s_TAG, "Entering main loop...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(s_heartbeat_period_ms));
        ESP_LOGI(s_TAG, "System running - Heartbeat");
    }
}
