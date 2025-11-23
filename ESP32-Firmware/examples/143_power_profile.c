/*
 * Example 143: Power Consumption Profiling
 * Measure power per operation, sleep mode transitions, low-power patterns
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
#include <esp_sleep.h>
#include <esp_log.h>

static const char *TAG = "PWR_PROFILE";

#define I2C_BUS         "pwr_i2c"
#define SPI_BUS         "pwr_spi"
#define TEST_DEVICE (0x50)

static star_bus_manager_t manager;

/* Power profile entry */
typedef struct {
    const char *operation;
    uint32_t count;
    int64_t total_active_us;
    float estimated_current_ma;
} power_entry_t;

static power_entry_t power_log[16];
static int power_log_count = 0;

/* Log power usage for operation */
static void log_power_usage(const char *op, int64_t active_us, float current_ma) {
    for (int i = 0; i < power_log_count; i++) {
        if (strcmp(power_log[i].operation, op) == 0) {
            power_log[i].count++;
            power_log[i].total_active_us += active_us;
            return;
        }
    }

    if (power_log_count < 16) {
        power_log[power_log_count].operation = op;
        power_log[power_log_count].count = 1;
        power_log[power_log_count].total_active_us = active_us;
        power_log[power_log_count].estimated_current_ma = current_ma;
        power_log_count++;
    }
}

/* Profile I2C operations */
static void profile_i2c_power(void) {
    ESP_LOGI(TAG, "=== I2C Power Profile ===");

    uint8_t buffer[32];
    size_t bytes;
    int64_t start, elapsed;

    /* Profile read operation */
    start = esp_timer_get_time();
    for (int i = 0; i < 100; i++) {
        star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 32, &bytes);
    }
    elapsed = esp_timer_get_time() - start;
    log_power_usage("I2C Read (32B)", elapsed, 5.0);
    ESP_LOGI(TAG, "I2C Read: %lld us/op, ~5.0 mA estimated", elapsed / 100);

    /* Profile write operation */
    start = esp_timer_get_time();
    for (int i = 0; i < 100; i++) {
        star_bus_i2c_write(&manager, I2C_BUS, buffer, 32, 0x00, &bytes);
    }
    elapsed = esp_timer_get_time() - start;
    log_power_usage("I2C Write (32B)", elapsed, 5.5);
    ESP_LOGI(TAG, "I2C Write: %lld us/op, ~5.5 mA estimated", elapsed / 100);
}

/* Profile SPI operations */
static void profile_spi_power(void) {
    ESP_LOGI(TAG, "=== SPI Power Profile ===");

    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    int64_t start, elapsed;

    /* Profile transfer operation */
    start = esp_timer_get_time();
    for (int i = 0; i < 100; i++) {
        star_bus_spi_transfer(&manager, SPI_BUS, tx_buf, rx_buf, 64, 100);
    }
    elapsed = esp_timer_get_time() - start;
    log_power_usage("SPI Transfer (64B)", elapsed, 8.0);
    ESP_LOGI(TAG, "SPI Transfer: %lld us/op, ~8.0 mA estimated", elapsed / 100);
}

/* Test sleep mode integration */
static void test_sleep_transitions(void) {
    ESP_LOGI(TAG, "=== Sleep Mode Transitions ===");

    int64_t start, prep_time;

    ESP_LOGI(TAG, "Testing light sleep integration...");

    /* Measure sleep preparation time */
    start = esp_timer_get_time();

    /* Configure wake source (timer) */
    esp_sleep_enable_timer_wakeup(10000);  /* 10ms */

    prep_time = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "Sleep preparation time: %lld us", prep_time);

    /* Enter light sleep */
    ESP_LOGI(TAG, "Entering light sleep for 10ms...");
    esp_light_sleep_start();

    /* Measure wake time */
    start = esp_timer_get_time();

    /* Small operation to verify wake */
    uint8_t buffer[4];
    size_t bytes;
    star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 1, &bytes);

    int64_t wake_time = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "Post-sleep first operation time: %lld us", wake_time);
}

/* Low-power communication patterns */
static void demonstrate_low_power_patterns(void) {
    ESP_LOGI(TAG, "=== Low-Power Communication Patterns ===");

    uint8_t buffer[8];
    size_t bytes;
    int64_t start, elapsed;

    /* Pattern 1: Batch operations */
    ESP_LOGI(TAG, "Pattern 1: Batched Operations");
    start = esp_timer_get_time();

    /* Single burst of operations instead of spread out */
    for (int i = 0; i < 10; i++) {
        star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 8, &bytes);
    }

    elapsed = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "  10 batched reads: %lld us (allows sleep between batches)", elapsed);

    /* Pattern 2: Reduced frequency polling */
    ESP_LOGI(TAG, "Pattern 2: Reduced Polling Frequency");
    ESP_LOGI(TAG, "  High power: Poll every 10ms -> ~100 ops/sec");
    ESP_LOGI(TAG, "  Low power:  Poll every 100ms -> ~10 ops/sec");
    ESP_LOGI(TAG, "  Power savings: ~90%% bus active time reduction");

    /* Pattern 3: Event-driven vs polling */
    ESP_LOGI(TAG, "Pattern 3: Event-Driven Communication");
    ESP_LOGI(TAG, "  Polling: Continuous bus activity");
    ESP_LOGI(TAG, "  Interrupt: Bus active only on events");
    ESP_LOGI(TAG, "  Recommendation: Use GPIO interrupt for device ready signals");
}

/* Print power profile summary */
static void print_power_summary(void) {
    ESP_LOGI(TAG, "========== Power Profile Summary ==========");
    ESP_LOGI(TAG, "Operation            | Count | Active (us) | Est. mA");

    float total_energy = 0;

    for (int i = 0; i < power_log_count; i++) {
        ESP_LOGI(TAG, "%-20s | %5lu | %11lld | %.1f",
                 power_log[i].operation,
                 (unsigned long)power_log[i].count,
                 power_log[i].total_active_us,
                 power_log[i].estimated_current_ma);

        /* Rough energy estimate */
        total_energy += power_log[i].estimated_current_ma *
                        (power_log[i].total_active_us / 1000.0);
    }

    ESP_LOGI(TAG, "Estimated energy usage: %.2f uAh", total_energy / 1000);

    ESP_LOGI(TAG, "Power Optimization Recommendations:");
    ESP_LOGI(TAG, "1. Use lowest acceptable clock speed");
    ESP_LOGI(TAG, "2. Batch operations to maximize sleep time");
    ESP_LOGI(TAG, "3. Use interrupts instead of polling");
    ESP_LOGI(TAG, "4. Disable unused peripherals");
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 143: Power Consumption Profiling");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "pwr_mgr", NULL, NULL);
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

    /* Run profiling */
    profile_i2c_power();
    profile_spi_power();
    test_sleep_transitions();
    demonstrate_low_power_patterns();

    /* Summary */
    print_power_summary();

    ESP_LOGI(TAG, "========== Power Profiling Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
