/**
 * @file 125_async_events.c
 * @brief Event group integration with async operations
 *
 * Demonstrates:
 * - FreeRTOS event groups with async
 * - Multiple event bits
 * - Synchronization patterns
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

static const char* TAG = "ASYNC_EVENTS";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SENSOR1_ADDR     (0x48)
#define SENSOR2_ADDR     (0x49)
#define SENSOR3_ADDR     (0x4A)

/* Event bits */
#define SENSOR1_COMPLETE_BIT  (BIT0)
#define SENSOR2_COMPLETE_BIT  (BIT1)
#define SENSOR3_COMPLETE_BIT  (BIT2)
#define ALL_SENSORS_COMPLETE  (SENSOR1_COMPLETE_BIT | SENSOR2_COMPLETE_BIT | SENSOR3_COMPLETE_BIT)

#define SENSOR1_ERROR_BIT     (BIT4)
#define SENSOR2_ERROR_BIT     (BIT5)
#define SENSOR3_ERROR_BIT     (BIT6)
#define ANY_ERROR             (SENSOR1_ERROR_BIT | SENSOR2_ERROR_BIT | SENSOR3_ERROR_BIT)

#define DATA_READY_BIT        (BIT8)
#define PROCESSING_DONE_BIT   (BIT9)

static star_bus_manager_t g_manager;
static EventGroupHandle_t g_event_group;

/* Sensor data buffers */
static uint8_t g_sensor1_data[2];
static uint8_t g_sensor2_data[2];
static uint8_t g_sensor3_data[2];

/* Completion callback with event notification */
static void sensor_complete_callback(star_async_handle_t handle, star_async_status_t status,
                                      esp_err_t result, void* context)
{
    int sensor_num = (int)(intptr_t)context;

    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Sensor %d read complete", sensor_num);
    } else {
        ESP_LOGE(TAG, "Sensor %d read failed: %s", sensor_num, esp_err_to_name(result));
    }
}

/* Processing task that waits for all sensors */
static void processing_task(void* arg)
{
    ESP_LOGI(TAG, "Processing task: waiting for all sensor data...");

    EventBits_t bits = xEventGroupWaitBits(
        g_event_group,
        ALL_SENSORS_COMPLETE,
        pdTRUE,   /* Clear bits on exit */
        pdTRUE,   /* Wait for ALL bits */
        pdMS_TO_TICKS(5000)
    );

    if ((bits & ALL_SENSORS_COMPLETE) == ALL_SENSORS_COMPLETE) {
        ESP_LOGI(TAG, "Processing task: all data received!");
        ESP_LOGI(TAG, "  Sensor1: 0x%02X%02X", g_sensor1_data[0], g_sensor1_data[1]);
        ESP_LOGI(TAG, "  Sensor2: 0x%02X%02X", g_sensor2_data[0], g_sensor2_data[1]);
        ESP_LOGI(TAG, "  Sensor3: 0x%02X%02X", g_sensor3_data[0], g_sensor3_data[1]);

        /* Signal processing done */
        xEventGroupSetBits(g_event_group, PROCESSING_DONE_BIT);
    } else {
        ESP_LOGW(TAG, "Processing task: timeout waiting for sensors");
    }

    vTaskDelete(NULL);
}

/* Error monitoring task */
static void error_monitor_task(void* arg)
{
    ESP_LOGI(TAG, "Error monitor: watching for failures...");

    EventBits_t bits = xEventGroupWaitBits(
        g_event_group,
        ANY_ERROR | PROCESSING_DONE_BIT,
        pdFALSE,  /* Don't clear */
        pdFALSE,  /* Wait for ANY bit */
        pdMS_TO_TICKS(5000)
    );

    if (bits & ANY_ERROR) {
        ESP_LOGW(TAG, "Error monitor: detected error(s)!");
        if (bits & SENSOR1_ERROR_BIT) ESP_LOGW(TAG, "  Sensor 1 error");
        if (bits & SENSOR2_ERROR_BIT) ESP_LOGW(TAG, "  Sensor 2 error");
        if (bits & SENSOR3_ERROR_BIT) ESP_LOGW(TAG, "  Sensor 3 error");
    } else if (bits & PROCESSING_DONE_BIT) {
        ESP_LOGI(TAG, "Error monitor: no errors detected");
    } else {
        ESP_LOGW(TAG, "Error monitor: timeout");
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Async Events Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "events_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create event group */
    g_event_group = xEventGroupCreate();
    if (g_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    /* Create I2C configurations */
    star_bus_config_t* sensor1 = star_bus_config_create_i2c(
        "sensor1", I2C_NUM_0, SENSOR1_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    star_bus_config_t* sensor2 = star_bus_config_create_i2c(
        "sensor2", I2C_NUM_0, SENSOR2_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    star_bus_config_t* sensor3 = star_bus_config_create_i2c(
        "sensor3", I2C_NUM_0, SENSOR3_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

    star_bus_manager_add_bus(&g_manager, sensor1);
    star_bus_manager_add_bus(&g_manager, sensor2);
    star_bus_manager_add_bus(&g_manager, sensor3);

    star_bus_config_init(sensor1, &g_manager);
    star_bus_config_init(sensor2, &g_manager);
    star_bus_config_init(sensor3, &g_manager);

    /* --- Basic Event Group Integration --- */
    ESP_LOGI(TAG, "\n--- Basic Event Group Integration ---");

    star_async_handle_t handle1, handle2, handle3;

    /* Sensor 1 */
    star_async_config_t cfg1 = {
        .timeout_ms = 1000,
        .callback = sensor_complete_callback,
        .context = (void*)1,
        .priority = 5
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor1", g_sensor1_data, 2, 0x00, &cfg1, &handle1);
    if (ret == ESP_OK) {
        star_async_set_event_bits(handle1, g_event_group, SENSOR1_COMPLETE_BIT, SENSOR1_ERROR_BIT);
    }

    /* Sensor 2 */
    star_async_config_t cfg2 = {
        .timeout_ms = 1000,
        .callback = sensor_complete_callback,
        .context = (void*)2,
        .priority = 5
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor2", g_sensor2_data, 2, 0x00, &cfg2, &handle2);
    if (ret == ESP_OK) {
        star_async_set_event_bits(handle2, g_event_group, SENSOR2_COMPLETE_BIT, SENSOR2_ERROR_BIT);
    }

    /* Sensor 3 */
    star_async_config_t cfg3 = {
        .timeout_ms = 1000,
        .callback = sensor_complete_callback,
        .context = (void*)3,
        .priority = 5
    };
    ret = star_bus_i2c_read_async(&g_manager, "sensor3", g_sensor3_data, 2, 0x00, &cfg3, &handle3);
    if (ret == ESP_OK) {
        star_async_set_event_bits(handle3, g_event_group, SENSOR3_COMPLETE_BIT, SENSOR3_ERROR_BIT);
    }

    /* Create processing and error monitoring tasks */
    xTaskCreate(processing_task, "processing", 4096, NULL, 5, NULL);
    xTaskCreate(error_monitor_task, "error_mon", 4096, NULL, 5, NULL);

    /* Wait for processing to complete */
    EventBits_t bits = xEventGroupWaitBits(
        g_event_group,
        PROCESSING_DONE_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(6000)
    );

    if (bits & PROCESSING_DONE_BIT) {
        ESP_LOGI(TAG, "All processing completed successfully");
    }

    /* Cleanup handles */
    star_async_free_handle(handle1);
    star_async_free_handle(handle2);
    star_async_free_handle(handle3);

    /* --- Synchronization Barrier Pattern --- */
    ESP_LOGI(TAG, "\n--- Synchronization Barrier Pattern ---");

    xEventGroupClearBits(g_event_group, 0xFFFFFF);

    star_async_handle_t barrier_handles[3];
    uint8_t barrier_bufs[3][2];

    /* Submit all reads */
    const char* names[] = {"sensor1", "sensor2", "sensor3"};
    EventBits_t complete_bits[] = {SENSOR1_COMPLETE_BIT, SENSOR2_COMPLETE_BIT, SENSOR3_COMPLETE_BIT};

    for (int i = 0; i < 3; i++) {
        star_async_config_t cfg = {
            .timeout_ms = 1000,
            .callback = NULL,
            .context = NULL,
            .priority = 5
        };
        ret = star_bus_i2c_read_async(&g_manager, names[i], barrier_bufs[i], 2, 0x00,
                                       &cfg, &barrier_handles[i]);
        if (ret == ESP_OK) {
            star_async_set_event_bits(barrier_handles[i], g_event_group, complete_bits[i], 0);
        }
    }

    /* Wait at barrier for all */
    bits = xEventGroupWaitBits(
        g_event_group,
        ALL_SENSORS_COMPLETE,
        pdTRUE,
        pdTRUE,
        pdMS_TO_TICKS(3000)
    );

    if ((bits & ALL_SENSORS_COMPLETE) == ALL_SENSORS_COMPLETE) {
        ESP_LOGI(TAG, "Barrier: all operations synchronized");
    }

    for (int i = 0; i < 3; i++) {
        star_async_free_handle(barrier_handles[i]);
    }

    /* --- Timeout with Event Groups --- */
    ESP_LOGI(TAG, "\n--- Timeout Pattern ---");

    star_async_handle_t timeout_handle;
    uint8_t timeout_buf[2];

    star_async_config_t timeout_cfg = {
        .timeout_ms = 100,  /* Short timeout */
        .callback = NULL,
        .context = NULL,
        .priority = 5
    };

    xEventGroupClearBits(g_event_group, DATA_READY_BIT);

    ret = star_bus_i2c_read_async(&g_manager, "sensor1", timeout_buf, 2, 0x00,
                                   &timeout_cfg, &timeout_handle);
    if (ret == ESP_OK) {
        star_async_set_event_bits(timeout_handle, g_event_group, DATA_READY_BIT, 0);
    }

    bits = xEventGroupWaitBits(g_event_group, DATA_READY_BIT, pdTRUE, pdTRUE,
                                pdMS_TO_TICKS(200));

    if (bits & DATA_READY_BIT) {
        ESP_LOGI(TAG, "Data ready before timeout");
    } else {
        ESP_LOGW(TAG, "Timeout waiting for data");
    }

    star_async_free_handle(timeout_handle);

    /* Cleanup */
    vEventGroupDelete(g_event_group);
    star_bus_manager_deinit(&g_manager);

    ESP_LOGI(TAG, "\nAsync events example complete");
}
