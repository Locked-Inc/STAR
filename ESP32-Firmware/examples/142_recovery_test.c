/*
 * Example 142: Recovery Procedures
 * Bus reset recovery, device reconnection, state restoration
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
#include <driver/gpio.h>

static const char *TAG = "RECOVERY";

#define I2C_BUS         "recov_i2c"
#define SPI_BUS         "recov_spi"
#define TEST_DEVICE (0x50)

static star_bus_manager_t manager;

/* Device state structure for restoration */
typedef struct {
    uint8_t config_reg;
    uint8_t mode_reg;
    bool initialized;
} device_state_t;

static device_state_t saved_state = {0};

/* Save device state before recovery */
static esp_err_t save_device_state(void) {
    ESP_LOGI(TAG, "Saving device state...");

    uint8_t buffer[2];
    size_t bytes;

    /* Read config register */
    esp_err_t ret = star_bus_i2c_read(&manager, I2C_BUS, buffer, 1, 0x00, &bytes);
    if (ret == ESP_OK) {
        saved_state.config_reg = buffer[0];
    }

    /* Read mode register */
    ret = star_bus_i2c_read(&manager, I2C_BUS, buffer, 1, 0x01, &bytes);
    if (ret == ESP_OK) {
        saved_state.mode_reg = buffer[0];
    }

    saved_state.initialized = true;
    ESP_LOGI(TAG, "  State saved: config=0x%02X, mode=0x%02X",
             saved_state.config_reg, saved_state.mode_reg);

    return ESP_OK;
}

/* Restore device state after recovery */
static esp_err_t restore_device_state(void) {
    if (!saved_state.initialized) {
        ESP_LOGI(TAG, "No saved state to restore");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Restoring device state...");

    uint8_t data[1];
    size_t bytes;

    /* Restore config register */
    data[0] = saved_state.config_reg;
    star_bus_i2c_write(&manager, I2C_BUS, data, 1, 0x00, &bytes);

    /* Restore mode register */
    data[0] = saved_state.mode_reg;
    star_bus_i2c_write(&manager, I2C_BUS, data, 1, 0x01, &bytes);

    ESP_LOGI(TAG, "  State restored");
    return ESP_OK;
}

/* I2C bus reset procedure */
static esp_err_t i2c_bus_reset(void) {
    ESP_LOGI(TAG, "Performing I2C bus reset...");

    /* Toggle SCL to release stuck devices */
    gpio_set_direction(GPIO_NUM_22, GPIO_MODE_OUTPUT);
    for (int i = 0; i < 9; i++) {
        gpio_set_level(GPIO_NUM_22, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(GPIO_NUM_22, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Small delay for bus stabilization */
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "  Bus reset complete");
    return ESP_OK;
}

/* Test device reconnection */
static void test_device_reconnection(void) {
    ESP_LOGI(TAG, "=== Device Reconnection Test ===");

    ESP_LOGI(TAG, "Simulating device disconnect...");
    /* In real scenario, device would be physically disconnected */

    int reconnect_attempts = 0;
    int max_attempts = 10;
    bool reconnected = false;

    ESP_LOGI(TAG, "Attempting reconnection...");
    uint8_t probe[1];
    size_t bytes;

    while (reconnect_attempts < max_attempts && !reconnected) {
        reconnect_attempts++;
        esp_err_t ret = star_bus_i2c_read_raw(&manager, I2C_BUS, probe, 1, &bytes);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Reconnected after %d attempts", reconnect_attempts);
            reconnected = true;
        } else {
            ESP_LOGI(TAG, "  Attempt %d: no response", reconnect_attempts);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    if (!reconnected) {
        ESP_LOGI(TAG, "  Device did not reconnect within %d attempts", max_attempts);
    }
}

/* Test graceful degradation */
static void test_graceful_degradation(void) {
    ESP_LOGI(TAG, "=== Graceful Degradation Test ===");

    uint8_t buffer[32];
    size_t bytes;
    int success_count = 0;
    int fail_count = 0;

    ESP_LOGI(TAG, "Testing operation continuity during failures...");

    for (int i = 0; i < 20; i++) {
        esp_err_t ret = star_bus_i2c_read_raw(&manager, I2C_BUS, buffer, 32, &bytes);

        if (ret == ESP_OK) {
            success_count++;
        } else {
            fail_count++;
            /* Attempt micro-recovery */
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    ESP_LOGI(TAG, "Results: %d success, %d failures", success_count, fail_count);

    float success_rate = (float)success_count * 100 / (success_count + fail_count);
    ESP_LOGI(TAG, "Success rate: %.1f%%", success_rate);

    if (success_rate >= 90) {
        ESP_LOGI(TAG, "Degradation handling: EXCELLENT");
    } else if (success_rate >= 70) {
        ESP_LOGI(TAG, "Degradation handling: GOOD");
    } else {
        ESP_LOGI(TAG, "Degradation handling: NEEDS IMPROVEMENT");
    }
}

/* Recovery timing test */
static void test_recovery_timing(void) {
    ESP_LOGI(TAG, "=== Recovery Timing Test ===");

    int64_t start, elapsed;

    /* Time I2C bus reset */
    start = esp_timer_get_time();
    i2c_bus_reset();
    elapsed = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "I2C reset time: %lld ms", elapsed / 1000);

    /* Time state save */
    start = esp_timer_get_time();
    save_device_state();
    elapsed = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "State save time: %lld ms", elapsed / 1000);

    /* Time state restore */
    start = esp_timer_get_time();
    restore_device_state();
    elapsed = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "State restore time: %lld ms", elapsed / 1000);
}

void app_main(void) {
    ESP_LOGI(TAG, "Example 142: Recovery Procedures");

    /* Initialize manager */
    esp_err_t ret = star_bus_manager_init(&manager, "recov_mgr", NULL, NULL);
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

    /* Run recovery tests */
    test_device_reconnection();
    test_graceful_degradation();
    test_recovery_timing();

    ESP_LOGI(TAG, "========== Recovery Tests Complete ==========");

    /* Cleanup */
    star_bus_manager_deinit(&manager);
}
