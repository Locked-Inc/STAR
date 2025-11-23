/*
 * Example 145: Real-Time Constraints
 * Deterministic timing, deadline monitoring, priority inversion avoidance
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

static const char *TAG = "REALTIME";

#define I2C_BUS             "rt_i2c"
#define SPI_BUS             "rt_spi"
#define TEST_DEVICE (0x50)
#define DEADLINE_US         1000    /* 1ms deadline */
#define RT_TASK_PRIORITY    (configMAX_PRIORITIES - 1)

static star_bus_manager_t manager;
static SemaphoreHandle_t bus_mutex;

/* Real-time statistics */
typedef struct {
    uint32_t total_ops;
    uint32_t deadline_met;
    uint32_t deadline_missed;
    int64_t max_latency_us;
    int64_t min_latency_us;
    int64_t sum_latency_us;
} rt_stats_t;

static rt_stats_t rt_stats = {0};
static volatile bool rt_test_running = false;

/* Reset RT statistics */
static void reset_rt_stats(void) {
    rt_stats.total_ops = 0;
    rt_stats.deadline_met = 0;
    rt_stats.deadline_missed = 0;
    rt_stats.max_latency_us = 0;
    rt_stats.min_latency_us = INT64_MAX;
    rt_stats.sum_latency_us = 0;
}

/* Record RT operation */
static void record_rt_op(int64_t latency_us, int64_t deadline_us) {
    rt_stats.total_ops++;
    rt_stats.sum_latency_us += latency_us;

    if (latency_us > rt_stats.max_latency_us) {
        rt_stats.max_latency_us = latency_us;
    }
    if (latency_us < rt_stats.min_latency_us) {
        rt_stats.min_latency_us = latency_us;
    }

    if (latency_us <= deadline_us) {
        rt_stats.deadline_met++;
    } else {
        rt_stats.deadline_missed++;
    }
}

/* Print RT statistics */
static void print_rt_stats(const char *test_name) {
    ESP_LOGI(TAG, "=== %s Results ===", test_name);
    ESP_LOGI(TAG, "Total operations: %lu", (unsigned long)rt_stats.total_ops);

    if (rt_stats.total_ops > 0) {
        float met_pct = (float)rt_stats.deadline_met * 100 / rt_stats.total_ops;
        ESP_LOGI(TAG, "Deadline met: %lu (%.2f%%)",
                 (unsigned long)rt_stats.deadline_met, met_pct);
        ESP_LOGI(TAG, "Deadline missed: %lu", (unsigned long)rt_stats.deadline_missed);
        ESP_LOGI(TAG, "Min latency: %lld us", rt_stats.min_latency_us);
        ESP_LOGI(TAG, "Max latency: %lld us", rt_stats.max_latency_us);
        ESP_LOGI(TAG, "Avg latency: %lld us", rt_stats.sum_latency_us / rt_stats.total_ops);
        ESP_LOGI(TAG, "Jitter: %lld us", rt_stats.max_latency_us - rt_stats.min_latency_us);
    }
}

/* High-priority real-time task */
static void rt_high_priority_task(void *arg) {
    uint8_t buffer[16];
    size_t bytes;
    int64_t deadline = DEADLINE_US;

    ESP_LOGI(TAG, "RT High Priority task started (priority %d)", RT_TASK_PRIORITY);

    while (rt_test_running) {
        int64_t start = esp_timer_get_time();

        /* Acquire mutex with priority inheritance */
        if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 16, &bytes);
            xSemaphoreGive(bus_mutex);

            int64_t latency = esp_timer_get_time() - start;
            record_rt_op(latency, deadline);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

/* Low-priority background task (causes priority inversion without mutex) */
static void background_task(void *arg) {
    uint8_t buffer[64];
    size_t bytes;

    ESP_LOGI(TAG, "Background task started (priority %d)", tskIDLE_PRIORITY + 1);

    while (rt_test_running) {
        if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Simulate long operation */
            star_bus_i2c_write(&manager, I2C_BUS, buffer, 64, 0x00, &bytes);
            vTaskDelay(pdMS_TO_TICKS(5));  /* Hold mutex longer */
            xSemaphoreGive(bus_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelete(NULL);
}

/* Test deterministic timing */
static void test_deterministic_timing(void) {
    ESP_LOGI(TAG, "=== Deterministic Timing Test ===");

    reset_rt_stats();
    uint8_t buffer[8];
    size_t bytes;
    int64_t deadline = 500;  /* 500us deadline for small transfer */

    ESP_LOGI(TAG, "Testing with %lld us deadline...", deadline);

    for (int i = 0; i < 1000; i++) {
        int64_t start = esp_timer_get_time();
        star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 8, &bytes);
        int64_t latency = esp_timer_get_time() - start;
        record_rt_op(latency, deadline);
    }

    print_rt_stats("Deterministic Timing");
}

/* Test priority inversion avoidance */
static void test_priority_inversion(void) {
    ESP_LOGI(TAG, "=== Priority Inversion Test ===");

    /* Create mutex with priority inheritance */
    bus_mutex = xSemaphoreCreateMutex();
    if (!bus_mutex) {
        ESP_LOGE(TAG, "Mutex creation failed");
        return;
    }

    reset_rt_stats();
    rt_test_running = true;

    /* Create tasks */
    TaskHandle_t rt_task, bg_task;
    xTaskCreate(rt_high_priority_task, "rt_high", 4096, NULL,
                RT_TASK_PRIORITY, &rt_task);
    xTaskCreate(background_task, "bg_low", 4096, NULL,
                tskIDLE_PRIORITY + 1, &bg_task);

    /* Run test for 5 seconds */
    ESP_LOGI(TAG, "Running priority inversion test for 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    rt_test_running = false;
    vTaskDelay(pdMS_TO_TICKS(500));

    print_rt_stats("Priority Inversion");

    vSemaphoreDelete(bus_mutex);
}

/* Test deadline monitoring */
static void test_deadline_monitoring(void) {
    ESP_LOGI(TAG, "=== Deadline Monitoring Test ===");

    uint8_t buffer[32];
    size_t bytes;
    int deadlines_strict = 0;
    int deadlines_soft = 0;

    int64_t strict_deadline = 200;   /* 200us */
    int64_t soft_deadline = 1000;    /* 1ms */

    ESP_LOGI(TAG, "Strict deadline: %lld us", strict_deadline);
    ESP_LOGI(TAG, "Soft deadline: %lld us", soft_deadline);

    for (int i = 0; i < 500; i++) {
        int64_t start = esp_timer_get_time();
        star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 32, &bytes);
        int64_t latency = esp_timer_get_time() - start;

        if (latency <= strict_deadline) {
            deadlines_strict++;
            deadlines_soft++;
        } else if (latency <= soft_deadline) {
            deadlines_soft++;
        }
    }

    ESP_LOGI(TAG, "Results (500 operations):");
    ESP_LOGI(TAG, "  Strict deadline met: %d (%.1f%%)",
             deadlines_strict, deadlines_strict * 100.0 / 500);
    ESP_LOGI(TAG, "  Soft deadline met: %d (%.1f%%)",
             deadlines_soft, deadlines_soft * 100.0 / 500);

    if (deadlines_strict > 450) {
        ESP_LOGI(TAG, "  Rating: HARD REAL-TIME capable");
    } else if (deadlines_soft > 450) {
        ESP_LOGI(TAG, "  Rating: SOFT REAL-TIME capable");
    } else {
        ESP_LOGI(TAG, "  Rating: BEST-EFFORT only");
    }
}

/* Test worst-case execution time */
static void test_wcet(void) {
    ESP_LOGI(TAG, "=== Worst-Case Execution Time (WCET) Analysis ===");

    uint8_t buffer[64];
    size_t bytes;
    size_t sizes[] = {1, 8, 16, 32, 64};

    ESP_LOGI(TAG, "Size (B) | Min (us) | Max (us) | WCET Ratio");

    for (int s = 0; s < 5; s++) {
        int64_t min_time = INT64_MAX;
        int64_t max_time = 0;

        for (int i = 0; i < 200; i++) {
            int64_t start = esp_timer_get_time();
            star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, sizes[s], &bytes);
            int64_t elapsed = esp_timer_get_time() - start;

            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;
        }

        float wcet_ratio = (float)max_time / min_time;
        ESP_LOGI(TAG, "%8u | %8lld | %8lld | %10.2fx",
                 (unsigned)sizes[s], min_time, max_time, wcet_ratio);
    }

    ESP_LOGI(TAG, "Note: WCET ratio > 2x indicates timing variability");
}

/* Real-time scheduling recommendations */
static void print_rt_recommendations(void) {
    ESP_LOGI(TAG, "========== Real-Time Recommendations ==========");
    ESP_LOGI(TAG, "1. Use priority inheritance mutexes for shared bus access");
    ESP_LOGI(TAG, "2. Keep critical section duration minimal");
    ESP_LOGI(TAG, "3. Use separate buses for time-critical operations");
    ESP_LOGI(TAG, "4. Consider DMA for predictable transfer times");
    ESP_LOGI(TAG, "5. Profile and establish WCET bounds for all operations");
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 145: Real-Time Constraints");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "rt_mgr", NULL, NULL);
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

    /* Run RT tests */
    test_deterministic_timing();
    test_deadline_monitoring();
    test_wcet();
    test_priority_inversion();

    /* Recommendations */
    print_rt_recommendations();

    ESP_LOGI(TAG, "========== Real-Time Tests Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
