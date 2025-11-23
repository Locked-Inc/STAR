/**
 * @file 135_bus_isolation.c
 * @brief Bus isolation techniques for fault containment
 *
 * Demonstrates:
 * - Isolate failing devices
 * - Bus recovery
 * - Fault containment
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include "star_error_handler.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "BUS_ISOLATION";

/* Pin definitions */
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS1_PIN      (GPIO_NUM_5)
#define SPI_CS2_PIN      (GPIO_NUM_4)

#define SENSOR1_ADDR     (0x48)
#define SENSOR2_ADDR     (0x49)
#define SENSOR3_ADDR     (0x4A)

static star_bus_manager_t g_manager;
static error_handler_t g_error_handler;

/* Device health status */
typedef enum {
    DEVICE_HEALTHY,
    DEVICE_DEGRADED,
    DEVICE_ISOLATED,
    DEVICE_RECOVERING
} device_health_t;

typedef struct {
    const char* name;
    device_health_t health;
    uint32_t error_count;
    uint32_t success_count;
    int64_t last_error_time;
    int64_t isolation_time;
    uint32_t recovery_attempts;
} device_state_t;

#define MAX_DEVICES (5)
static device_state_t g_devices[MAX_DEVICES];
static int g_device_count = 0;

/* Error threshold for isolation */
#define ERROR_THRESHOLD (3)
#define RECOVERY_DELAY_MS (5000)
#define MAX_RECOVERY_ATTEMPTS (3)

/* Register a device for monitoring */
static device_state_t* register_device(const char* name)
{
    if (g_device_count >= MAX_DEVICES) return NULL;

    device_state_t* dev = &g_devices[g_device_count++];
    dev->name = name;
    dev->health = DEVICE_HEALTHY;
    dev->error_count = 0;
    dev->success_count = 0;
    dev->last_error_time = 0;
    dev->isolation_time = 0;
    dev->recovery_attempts = 0;

    return dev;
}

/* Find device by name */
static device_state_t* find_device(const char* name)
{
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].name, name) == 0) {
            return &g_devices[i];
        }
    }
    return NULL;
}

/* Record operation result */
static void record_result(device_state_t* dev, bool success)
{
    if (success) {
        dev->success_count++;
        /* Reset error count on success streak */
        if (dev->success_count >= 3) {
            dev->error_count = 0;
            if (dev->health == DEVICE_DEGRADED) {
                dev->health = DEVICE_HEALTHY;
                ESP_LOGI(TAG, "Device %s recovered to HEALTHY", dev->name);
            }
        }
    } else {
        dev->error_count++;
        dev->success_count = 0;
        dev->last_error_time = esp_timer_get_time();

        if (dev->error_count >= ERROR_THRESHOLD && dev->health != DEVICE_ISOLATED) {
            dev->health = DEVICE_ISOLATED;
            dev->isolation_time = esp_timer_get_time();
            ESP_LOGW(TAG, "Device %s ISOLATED after %lu errors",
                     dev->name, (unsigned long)dev->error_count);
        } else if (dev->error_count > 0 && dev->health == DEVICE_HEALTHY) {
            dev->health = DEVICE_DEGRADED;
            ESP_LOGW(TAG, "Device %s DEGRADED", dev->name);
        }
    }
}

/* Check if device can be accessed */
static bool can_access_device(device_state_t* dev)
{
    if (dev->health == DEVICE_ISOLATED) {
        /* Check if recovery period has passed */
        int64_t now = esp_timer_get_time();
        int64_t isolation_duration = (now - dev->isolation_time) / 1000;

        if (isolation_duration >= RECOVERY_DELAY_MS) {
            if (dev->recovery_attempts < MAX_RECOVERY_ATTEMPTS) {
                dev->health = DEVICE_RECOVERING;
                dev->recovery_attempts++;
                ESP_LOGI(TAG, "Device %s attempting recovery (%lu/%d)",
                         dev->name, (unsigned long)dev->recovery_attempts, MAX_RECOVERY_ATTEMPTS);
                return true;
            }
        }
        return false;
    }
    return true;
}

/* Protected I2C read with isolation */
static esp_err_t isolated_i2c_read(const char* bus_name, uint8_t* data, size_t len, uint8_t reg)
{
    device_state_t* dev = find_device(bus_name);
    if (dev == NULL) {
        /* Unknown device, proceed anyway */
        size_t bytes_read;
        return star_bus_i2c_read(&g_manager, bus_name, data, len, reg, &bytes_read);
    }

    if (!can_access_device(dev)) {
        ESP_LOGW(TAG, "Access to %s denied (isolated)", bus_name);
        return ESP_ERR_NOT_ALLOWED;
    }

    size_t bytes_read;
    esp_err_t ret = star_bus_i2c_read(&g_manager, bus_name, data, len, reg, &bytes_read);

    bool success = (ret == ESP_OK);
    record_result(dev, success);

    if (dev->health == DEVICE_RECOVERING) {
        if (success) {
            dev->health = DEVICE_HEALTHY;
            dev->error_count = 0;
            dev->recovery_attempts = 0;
            ESP_LOGI(TAG, "Device %s recovery SUCCESSFUL", bus_name);
        } else {
            dev->health = DEVICE_ISOLATED;
            dev->isolation_time = esp_timer_get_time();
            ESP_LOGW(TAG, "Device %s recovery FAILED", bus_name);
        }
    }

    return ret;
}

/* Attempt bus recovery */
static esp_err_t recover_i2c_bus(int port)
{
    ESP_LOGI(TAG, "Attempting I2C bus %d recovery...", port);

    /* Toggle SCL to release stuck devices */
    gpio_config_t scl_conf = {
        .pin_bit_mask = (1ULL << I2C_SCL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&scl_conf);

    /* Generate clock pulses */
    for (int i = 0; i < 9; i++) {
        gpio_set_level(I2C_SCL_PIN, 0);
        vTaskDelay(1);
        gpio_set_level(I2C_SCL_PIN, 1);
        vTaskDelay(1);
    }

    /* Generate STOP condition */
    gpio_config_t sda_conf = {
        .pin_bit_mask = (1ULL << I2C_SDA_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&sda_conf);

    gpio_set_level(I2C_SDA_PIN, 0);
    vTaskDelay(1);
    gpio_set_level(I2C_SCL_PIN, 1);
    vTaskDelay(1);
    gpio_set_level(I2C_SDA_PIN, 1);
    vTaskDelay(1);

    ESP_LOGI(TAG, "I2C bus recovery sequence complete");
    return ESP_OK;
}

/* Print device health report */
static void print_health_report(void)
{
    ESP_LOGI(TAG, "\n=== Device Health Report ===");
    for (int i = 0; i < g_device_count; i++) {
        device_state_t* dev = &g_devices[i];
        const char* health_str;
        switch (dev->health) {
            case DEVICE_HEALTHY: health_str = "HEALTHY"; break;
            case DEVICE_DEGRADED: health_str = "DEGRADED"; break;
            case DEVICE_ISOLATED: health_str = "ISOLATED"; break;
            case DEVICE_RECOVERING: health_str = "RECOVERING"; break;
            default: health_str = "UNKNOWN"; break;
        }
        ESP_LOGI(TAG, "  %s: %s (errors=%lu, successes=%lu)",
                 dev->name, health_str,
                 (unsigned long)dev->error_count, (unsigned long)dev->success_count);
    }
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Bus Isolation Example ===\n");

    /* Initialize error handler */
    error_handler_init(&g_error_handler, 3, 100, 1000, NULL, NULL);

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "isolation_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* --- Create Multiple I2C Devices --- */
    ESP_LOGI(TAG, "Creating I2C devices...");

    star_bus_config_t* sensor1 = star_bus_config_create_i2c(
        "sensor1", I2C_NUM_0, SENSOR1_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    star_bus_config_t* sensor2 = star_bus_config_create_i2c(
        "sensor2", I2C_NUM_0, SENSOR2_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    star_bus_config_t* sensor3 = star_bus_config_create_i2c(
        "sensor3", I2C_NUM_0, SENSOR3_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

    if (sensor1) {
        star_bus_manager_add_bus(&g_manager, sensor1);
        star_bus_config_init(sensor1, &g_manager);
        register_device("sensor1");
    }
    if (sensor2) {
        star_bus_manager_add_bus(&g_manager, sensor2);
        star_bus_config_init(sensor2, &g_manager);
        register_device("sensor2");
    }
    if (sensor3) {
        star_bus_manager_add_bus(&g_manager, sensor3);
        star_bus_config_init(sensor3, &g_manager);
        register_device("sensor3");
    }

    ESP_LOGI(TAG, "Registered %d devices", g_device_count);

    /* --- Normal Operation --- */
    ESP_LOGI(TAG, "\n--- Normal Operation ---");

    uint8_t data[4];
    for (int i = 0; i < 5; i++) {
        isolated_i2c_read("sensor1", data, 4, 0x00);
        isolated_i2c_read("sensor2", data, 4, 0x00);
        isolated_i2c_read("sensor3", data, 4, 0x00);
    }

    print_health_report();

    /* --- Simulated Device Failure --- */
    ESP_LOGI(TAG, "\n--- Simulating Device Failure ---");

    /* Manually inject errors for sensor2 */
    device_state_t* sensor2_dev = find_device("sensor2");
    if (sensor2_dev) {
        for (int i = 0; i < 5; i++) {
            record_result(sensor2_dev, false);
        }
    }

    print_health_report();

    /* --- Access Attempts to Isolated Device --- */
    ESP_LOGI(TAG, "\n--- Access to Isolated Device ---");

    ret = isolated_i2c_read("sensor2", data, 4, 0x00);
    ESP_LOGI(TAG, "Read sensor2: %s", esp_err_to_name(ret));

    /* Other devices should still work */
    ret = isolated_i2c_read("sensor1", data, 4, 0x00);
    ESP_LOGI(TAG, "Read sensor1: %s", esp_err_to_name(ret));

    ret = isolated_i2c_read("sensor3", data, 4, 0x00);
    ESP_LOGI(TAG, "Read sensor3: %s", esp_err_to_name(ret));

    /* --- Bus Recovery --- */
    ESP_LOGI(TAG, "\n--- Bus Recovery ---");

    recover_i2c_bus(I2C_NUM_0);

    /* Reinitialize bus after recovery */
    star_bus_config_init(sensor2, &g_manager);

    /* --- Wait for Recovery Period --- */
    ESP_LOGI(TAG, "\n--- Waiting for Recovery Period ---");
    ESP_LOGI(TAG, "Device will attempt recovery after %d ms", RECOVERY_DELAY_MS);

    /* Simulate time passing (in real application, use actual delay) */
    if (sensor2_dev) {
        sensor2_dev->isolation_time -= (RECOVERY_DELAY_MS * 1000);  /* Adjust timestamp */
    }

    /* Attempt access - should trigger recovery */
    ret = isolated_i2c_read("sensor2", data, 4, 0x00);
    ESP_LOGI(TAG, "Recovery attempt: %s", esp_err_to_name(ret));

    print_health_report();

    /* --- Fault Containment Summary --- */
    ESP_LOGI(TAG, "\n--- Fault Containment Strategy ---");
    ESP_LOGI(TAG, "1. Monitor each device independently");
    ESP_LOGI(TAG, "2. Track error counts per device");
    ESP_LOGI(TAG, "3. Isolate failing devices at threshold");
    ESP_LOGI(TAG, "4. Continue operating healthy devices");
    ESP_LOGI(TAG, "5. Periodically attempt recovery");
    ESP_LOGI(TAG, "6. Use bus recovery procedures");
    ESP_LOGI(TAG, "7. Limit recovery attempts to prevent loops");

    /* Cleanup */
    error_handler_deinit(&g_error_handler);
    star_bus_manager_deinit(&g_manager);

    ESP_LOGI(TAG, "\nBus isolation example complete");
}
