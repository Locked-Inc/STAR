/*
 * Example 141: Error Injection Testing
 * Simulate bus errors, test error handlers, verify recovery
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
#include <esp_log.h>

static const char *TAG = "ERR_INJECT";

#define I2C_BUS         "err_i2c"
#define SPI_BUS         "err_spi"
#define TEST_DEVICE (0x50)
#define INVALID_DEVICE  (0x7F)

static star_bus_manager_t manager;

/* Error handler statistics */
static struct {
    uint32_t errors_detected;
    uint32_t recovery_attempts;
    uint32_t errors_recovered;
} error_stats = {0};

/* Test invalid device address error */
static void test_invalid_address(void) {
    ESP_LOGI(TAG, "=== Test: Invalid Device Address ===");

    /* Create config for non-existent device */
    star_bus_config_t *invalid_cfg = star_bus_config_create_i2c(
        "invalid_dev", I2C_NUM_0, INVALID_DEVICE,
        GPIO_NUM_21, GPIO_NUM_22, 400000
    );

    if (invalid_cfg) {
        star_bus_manager_add_bus(&manager, invalid_cfg);
        star_bus_config_init(invalid_cfg, &manager);

        uint8_t buffer[8];
        size_t bytes_read;
        esp_err_t ret = star_bus_i2c_read_raw(&manager, "invalid_dev", buffer, 8, &bytes_read);

        ESP_LOGI(TAG, "Result: %s (0x%x)",
                 (ret != ESP_OK) ? "EXPECTED ERROR" : "UNEXPECTED SUCCESS", ret);

        if (ret != ESP_OK) {
            error_stats.errors_detected++;
        }
    }
}

/* Test NACK handling */
static void test_nack_handling(void) {
    ESP_LOGI(TAG, "=== Test: NACK Handling ===");

    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    size_t bytes;

    /* Try to write to non-existent device address */
    star_bus_config_t *nack_cfg = star_bus_config_create_i2c(
        "nack_dev", I2C_NUM_0, 0x7E,
        GPIO_NUM_21, GPIO_NUM_22, 400000
    );

    if (nack_cfg) {
        star_bus_manager_add_bus(&manager, nack_cfg);
        star_bus_config_init(nack_cfg, &manager);

        esp_err_t ret = star_bus_i2c_write(&manager, "nack_dev", data, sizeof(data), 0x00, &bytes);
        ESP_LOGI(TAG, "Write to non-existent device: %s (0x%x)",
                 (ret != ESP_OK) ? "NACK DETECTED" : "UNEXPECTED SUCCESS", ret);

        if (ret != ESP_OK) {
            error_stats.errors_detected++;
        }

        /* Verify bus is still operational */
        ret = star_bus_i2c_read_raw(&manager, I2C_BUS, data, 4, &bytes);
        ESP_LOGI(TAG, "Bus operational after NACK: %s", (ret == ESP_OK) ? "YES" : "NO");
    }
}

/* Test error recovery procedure */
static void test_error_recovery(void) {
    ESP_LOGI(TAG, "=== Test: Error Recovery ===");

    uint32_t initial_errors = error_stats.errors_detected;

    /* Induce multiple errors */
    ESP_LOGI(TAG, "Inducing errors...");
    uint8_t buffer[8];
    size_t bytes;

    for (int i = 0; i < 10; i++) {
        star_bus_i2c_read_raw(&manager, "invalid_dev", buffer, 8, &bytes);
    }

    uint32_t errors_induced = error_stats.errors_detected - initial_errors;
    ESP_LOGI(TAG, "Errors induced: %lu", (unsigned long)errors_induced);

    /* Attempt recovery */
    ESP_LOGI(TAG, "Attempting recovery...");
    error_stats.recovery_attempts++;

    /* Allow bus to settle */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Verify operation with valid device */
    esp_err_t ret = star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 8, &bytes);

    if (ret == ESP_OK) {
        error_stats.errors_recovered++;
        ESP_LOGI(TAG, "Recovery: SUCCESSFUL");
    } else {
        ESP_LOGI(TAG, "Recovery: PARTIAL (device access failed)");
    }
}

/* Test SPI error conditions */
static void test_spi_errors(void) {
    ESP_LOGI(TAG, "=== Test: SPI Error Conditions ===");

    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    memset(tx_buf, 0xAA, sizeof(tx_buf));

    /* Test with invalid bus name */
    esp_err_t ret = star_bus_spi_transfer(&manager, "nonexistent", tx_buf, rx_buf, 64, 100);
    ESP_LOGI(TAG, "Invalid bus name: %s (0x%x)",
             (ret != ESP_OK) ? "ERROR DETECTED" : "UNEXPECTED SUCCESS", ret);

    if (ret != ESP_OK) {
        error_stats.errors_detected++;
    }

    /* Test with zero length - may or may not be an error depending on implementation */
    ret = star_bus_spi_transfer(&manager, SPI_BUS, tx_buf, rx_buf, 0, 100);
    ESP_LOGI(TAG, "Zero length: 0x%x", ret);
}

/* Print error statistics summary */
static void print_error_summary(void) {
    ESP_LOGI(TAG, "========== Error Injection Summary ==========");
    ESP_LOGI(TAG, "Total errors detected: %lu", (unsigned long)error_stats.errors_detected);
    ESP_LOGI(TAG, "Recovery attempts: %lu", (unsigned long)error_stats.recovery_attempts);
    ESP_LOGI(TAG, "Successful recoveries: %lu", (unsigned long)error_stats.errors_recovered);

    if (error_stats.recovery_attempts > 0) {
        float recovery_rate = (float)error_stats.errors_recovered * 100 /
                              error_stats.recovery_attempts;
        ESP_LOGI(TAG, "Recovery rate: %.1f%%", recovery_rate);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 141: Error Injection Testing");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "err_mgr", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager init failed: %d", ret);
        return;
    }

    /* Initialize valid I2C device */
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

    /* Run error injection tests */
    test_invalid_address();
    test_nack_handling();
    test_spi_errors();
    test_error_recovery();

    /* Summary */
    print_error_summary();

    ESP_LOGI(TAG, "========== Error Injection Tests Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
