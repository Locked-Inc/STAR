/*
 * Example 140: Stress Testing
 * Continuous operations, error rate under load, recovery from saturation
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_i2c.h"
#include "star_bus_spi.h"
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "STRESS";

#define I2C_BUS         "stress_i2c"
#define SPI_BUS         "stress_spi"
#define TEST_DEVICE (0x50)
#define STRESS_DURATION_SEC (10)
#define NUM_WORKER_TASKS    (2)

static star_bus_manager_t manager;
static SemaphoreHandle_t stats_mutex;
static volatile bool stress_running = false;

/* Stress test statistics */
typedef struct {
    uint32_t total_ops;
    uint32_t success_ops;
    uint32_t failed_ops;
    int64_t start_time;
} stress_stats_t;

static stress_stats_t i2c_stats;
static stress_stats_t spi_stats;

/* Reset statistics */
static void reset_stats(stress_stats_t *stats) {
    memset(stats, 0, sizeof(stress_stats_t));
    stats->start_time = esp_timer_get_time();
}

/* Record operation result */
static void record_result(stress_stats_t *stats, esp_err_t result) {
    xSemaphoreTake(stats_mutex, portMAX_DELAY);
    stats->total_ops++;
    if (result == ESP_OK) {
        stats->success_ops++;
    } else {
        stats->failed_ops++;
    }
    xSemaphoreGive(stats_mutex);
}

/* Print stress test report */
static void print_stress_report(const char *bus_name, stress_stats_t *stats) {
    int64_t duration = esp_timer_get_time() - stats->start_time;
    float duration_sec = duration / 1000000.0f;
    float ops_per_sec = stats->total_ops / duration_sec;
    float error_rate = 0;
    if (stats->total_ops > 0) {
        error_rate = (float)stats->failed_ops * 100 / stats->total_ops;
    }

    ESP_LOGI(TAG, "=== %s Stress Test Report ===", bus_name);
    ESP_LOGI(TAG, "Duration: %.2f seconds", duration_sec);
    ESP_LOGI(TAG, "Total operations: %lu", (unsigned long)stats->total_ops);
    ESP_LOGI(TAG, "Successful: %lu", (unsigned long)stats->success_ops);
    ESP_LOGI(TAG, "Failed: %lu (%.2f%%)", (unsigned long)stats->failed_ops, error_rate);
    ESP_LOGI(TAG, "Operations/sec: %.2f", ops_per_sec);
}

/* I2C stress worker task */
static void i2c_stress_task(void *arg) {
    int task_id = (int)(intptr_t)arg;
    uint8_t buffer[32];
    size_t bytes;

    ESP_LOGI(TAG, "I2C stress task %d started", task_id);

    while (stress_running) {
        esp_err_t ret;
        if (task_id % 2 == 0) {
            ret = star_bus_i2c_write(&manager, I2C_BUS, buffer, 32, 0x00, &bytes);
        } else {
            ret = star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 32, &bytes);
        }
        record_result(&i2c_stats, ret);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG, "I2C stress task %d finished", task_id);
    vTaskDelete(NULL);
}

/* SPI stress worker task */
static void spi_stress_task(void *arg) {
    int task_id = (int)(intptr_t)arg;
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];

    ESP_LOGI(TAG, "SPI stress task %d started", task_id);
    memset(tx_buf, task_id, sizeof(tx_buf));

    while (stress_running) {
        esp_err_t ret = star_bus_spi_transfer(&manager, SPI_BUS, tx_buf, rx_buf, 64, 100);
        record_result(&spi_stats, ret);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG, "SPI stress task %d finished", task_id);
    vTaskDelete(NULL);
}

/* Run stress test */
static void run_stress_test(int duration_sec) {
    ESP_LOGI(TAG, "========== Starting %d Second Stress Test ==========", duration_sec);

    stats_mutex = xSemaphoreCreateMutex();
    reset_stats(&i2c_stats);
    reset_stats(&spi_stats);

    stress_running = true;

    /* Create worker tasks */
    for (int i = 0; i < NUM_WORKER_TASKS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "i2c_str_%d", i);
        xTaskCreate(i2c_stress_task, name, 4096, (void *)(intptr_t)i, 5, NULL);

        snprintf(name, sizeof(name), "spi_str_%d", i);
        xTaskCreate(spi_stress_task, name, 4096, (void *)(intptr_t)i, 5, NULL);
    }

    /* Progress reporting */
    for (int s = 0; s < duration_sec; s++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Progress: %d/%d sec - I2C: %lu ops, SPI: %lu ops",
                 s + 1, duration_sec,
                 (unsigned long)i2c_stats.total_ops,
                 (unsigned long)spi_stats.total_ops);
    }

    /* Stop stress test */
    stress_running = false;
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Print reports */
    print_stress_report("I2C", &i2c_stats);
    print_stress_report("SPI", &spi_stats);

    vSemaphoreDelete(stats_mutex);
}

/* Test recovery from bus saturation */
static void test_saturation_recovery(void) {
    ESP_LOGI(TAG, "=== Saturation Recovery Test ===");

    uint8_t buffer[64];
    size_t bytes;
    memset(buffer, 0xAA, sizeof(buffer));

    /* Saturate the bus with rapid transfers */
    ESP_LOGI(TAG, "Saturating I2C bus...");
    int errors = 0;
    for (int i = 0; i < 500; i++) {
        if (star_bus_i2c_write(&manager, I2C_BUS, buffer, 64, 0x00, &bytes) != ESP_OK) {
            errors++;
        }
    }
    ESP_LOGI(TAG, "Saturation phase: %d errors in 500 ops", errors);

    /* Allow recovery */
    ESP_LOGI(TAG, "Allowing recovery (1 second)...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Test normal operation */
    ESP_LOGI(TAG, "Testing recovery...");
    errors = 0;
    for (int i = 0; i < 100; i++) {
        if (star_bus_i2c_write(&manager, I2C_BUS, buffer, 32, 0x00, &bytes) != ESP_OK) {
            errors++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "Recovery phase: %d errors in 100 ops", errors);
    ESP_LOGI(TAG, "Recovery: %s", (errors < 5) ? "SUCCESSFUL" : "DEGRADED");
}

/* Continuous error monitoring */
static void error_rate_monitor(void) {
    ESP_LOGI(TAG, "=== Error Rate Under Sustained Load ===");
    ESP_LOGI(TAG, "Time (s) | Ops    | Errors | Error Rate");

    uint8_t buffer[64];
    size_t bytes;

    for (int sec = 0; sec < 5; sec++) {
        int64_t end_time = esp_timer_get_time() + 1000000;
        int window_ops = 0;
        int window_errors = 0;

        while (esp_timer_get_time() < end_time) {
            if (star_bus_i2c_write(&manager, I2C_BUS, buffer, 64, 0x00, &bytes) != ESP_OK) {
                window_errors++;
            }
            window_ops++;
        }

        float error_rate = (float)window_errors * 100 / window_ops;
        ESP_LOGI(TAG, "%8d | %6d | %6d | %9.2f%%",
                 sec + 1, window_ops, window_errors, error_rate);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 140: Stress Testing");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "stress_mgr", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager init failed: %d", ret);
        return;
    }

    /* Initialize I2C */
    star_bus_config_t *i2c_cfg = star_bus_config_create_i2c(
        I2C_BUS, I2C_NUM_0, TEST_DEVICE,
        GPIO_NUM_21, GPIO_NUM_22, 400000
    );
    if (i2c_cfg) {
        star_bus_manager_add_bus(&manager, i2c_cfg);
        star_bus_config_init(i2c_cfg, &manager);
    }

    /* Initialize SPI */
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

    /* Run tests */
    run_stress_test(STRESS_DURATION_SEC);
    test_saturation_recovery();
    error_rate_monitor();

    ESP_LOGI(TAG, "========== Stress Tests Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
