/**
 * @file 134_shared_pins.c
 * @brief Shared pin management for multiplexed buses
 *
 * Demonstrates:
 * - Multiplexed pins
 * - Mutex protection
 * - Conflict avoidance
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include "star_pin_validator.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char* TAG = "SHARED_PINS";

/* Shared pins - in this scenario, GPIO4 is used by multiple functions */
#define SHARED_PIN       (GPIO_NUM_4)
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)

#define SENSOR_ADDR      (0x48)

static star_bus_manager_t g_manager;
static SemaphoreHandle_t g_shared_pin_mutex;

/* Pin multiplexer state */
typedef enum {
    MUX_MODE_NONE,
    MUX_MODE_SPI_CS,
    MUX_MODE_I2C_INT,
    MUX_MODE_GPIO
} mux_mode_t;

static mux_mode_t g_current_mux_mode = MUX_MODE_NONE;

/* Configure shared pin for specific function */
static esp_err_t configure_shared_pin(mux_mode_t mode)
{
    if (xSemaphoreTake(g_shared_pin_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire pin mutex");
        return ESP_ERR_TIMEOUT;
    }

    if (g_current_mux_mode == mode) {
        xSemaphoreGive(g_shared_pin_mutex);
        return ESP_OK;  /* Already in correct mode */
    }

    ESP_LOGI(TAG, "Switching shared pin from mode %d to %d", g_current_mux_mode, mode);

    /* Reconfigure GPIO for new function */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SHARED_PIN),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    switch (mode) {
        case MUX_MODE_SPI_CS:
            io_conf.mode = GPIO_MODE_OUTPUT;
            gpio_set_level(SHARED_PIN, 1);  /* CS inactive high */
            break;

        case MUX_MODE_I2C_INT:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            break;

        case MUX_MODE_GPIO:
            io_conf.mode = GPIO_MODE_OUTPUT;
            break;

        default:
            io_conf.mode = GPIO_MODE_DISABLE;
            break;
    }

    gpio_config(&io_conf);
    g_current_mux_mode = mode;

    xSemaphoreGive(g_shared_pin_mutex);
    return ESP_OK;
}

/* Acquire shared pin for SPI operation */
static esp_err_t acquire_for_spi(void)
{
    return configure_shared_pin(MUX_MODE_SPI_CS);
}

/* Release shared pin from SPI */
static esp_err_t release_from_spi(void)
{
    return configure_shared_pin(MUX_MODE_NONE);
}

/* Protected SPI operation */
static esp_err_t protected_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    esp_err_t ret = acquire_for_spi();
    if (ret != ESP_OK) return ret;

    ret = star_bus_spi_transfer(&g_manager, "spi_device", tx, rx, len, 0);

    release_from_spi();
    return ret;
}

/* Task that uses SPI */
static void spi_task(void* arg)
{
    uint8_t tx_buf[4] = {0x9F, 0, 0, 0};
    uint8_t rx_buf[4];

    for (int i = 0; i < 5; i++) {
        esp_err_t ret = protected_spi_transfer(tx_buf, rx_buf, 4);
        ESP_LOGI(TAG, "SPI task: transfer %d - %s", i, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(NULL);
}

/* Task that uses I2C interrupt pin */
static void i2c_int_task(void* arg)
{
    for (int i = 0; i < 5; i++) {
        configure_shared_pin(MUX_MODE_I2C_INT);

        /* Check interrupt status */
        int pin_level = gpio_get_level(SHARED_PIN);
        ESP_LOGI(TAG, "I2C INT task: pin level = %d", pin_level);

        configure_shared_pin(MUX_MODE_NONE);
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Shared Pin Management Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "shared_pins", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create shared pin mutex */
    g_shared_pin_mutex = xSemaphoreCreateMutex();
    if (g_shared_pin_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    /* --- Pin Conflict Detection --- */
    ESP_LOGI(TAG, "--- Pin Conflict Detection ---");

    /* Register pins with validator */
    star_pin_interface_t pin_iface;
    pin_validator_get_interface(&pin_iface);

    if (pin_iface.register_pin) {
        pin_iface.register_pin(pin_iface.ctx, I2C_SDA_PIN, "I2C_SDA", false);
        pin_iface.register_pin(pin_iface.ctx, I2C_SCL_PIN, "I2C_SCL", false);
        pin_iface.register_pin(pin_iface.ctx, SPI_COPI_PIN, "SPI_COPI", false);
        pin_iface.register_pin(pin_iface.ctx, SPI_CIPO_PIN, "SPI_CIPO", false);
        pin_iface.register_pin(pin_iface.ctx, SPI_SCLK_PIN, "SPI_SCLK", false);
        pin_iface.register_pin(pin_iface.ctx, SHARED_PIN, "SHARED_MUX", true);
    }

    /* Validate - should pass since no conflicts */
    ret = star_validate_pins();
    ESP_LOGI(TAG, "Pin validation: %s", esp_err_to_name(ret));

    /* Try to register same pin again - should detect conflict */
    if (pin_iface.register_pin) {
        ret = pin_iface.register_pin(pin_iface.ctx, SHARED_PIN, "CONFLICT_PIN", false);
        ESP_LOGI(TAG, "Duplicate pin registration: %s (expected conflict)", esp_err_to_name(ret));
    }

    /* --- Initialize Buses --- */
    ESP_LOGI(TAG, "\n--- Initialize Buses ---");

    /* I2C bus */
    star_bus_config_t* i2c_config = star_bus_config_create_i2c(
        "i2c_sensor", I2C_NUM_0, SENSOR_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    if (i2c_config) {
        star_bus_manager_add_bus(&g_manager, i2c_config);
        star_bus_config_init(i2c_config, &g_manager);
        ESP_LOGI(TAG, "  I2C ready");
    }

    /* SPI bus with shared CS pin */
    spi_device_interface_config_t spi_cfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = -1,  /* Manual CS control */
        .queue_size = 7,
    };

    star_bus_config_t* spi_config = star_bus_config_create_spi_device(
        "spi_device", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_cfg);

    if (spi_config) {
        star_bus_manager_add_bus(&g_manager, spi_config);
        star_bus_config_init(spi_config, &g_manager);
        ESP_LOGI(TAG, "  SPI ready (manual CS)");
    }

    /* --- Mutex-Protected Pin Access --- */
    ESP_LOGI(TAG, "\n--- Mutex-Protected Pin Access ---");

    /* Configure as SPI CS */
    configure_shared_pin(MUX_MODE_SPI_CS);
    ESP_LOGI(TAG, "Pin configured for SPI CS");

    /* Perform SPI operation with manual CS */
    gpio_set_level(SHARED_PIN, 0);  /* CS active */
    uint8_t tx[4] = {0x9F}, rx[4];
    star_bus_spi_transfer(&g_manager, "spi_device", tx, rx, 4, 0);
    gpio_set_level(SHARED_PIN, 1);  /* CS inactive */
    ESP_LOGI(TAG, "SPI transfer complete");

    /* Switch to I2C interrupt mode */
    configure_shared_pin(MUX_MODE_I2C_INT);
    ESP_LOGI(TAG, "Pin configured for I2C interrupt");
    int level = gpio_get_level(SHARED_PIN);
    ESP_LOGI(TAG, "Interrupt pin level: %d", level);

    /* --- Concurrent Access with Arbitration --- */
    ESP_LOGI(TAG, "\n--- Concurrent Access Demonstration ---");

    /* Start tasks that compete for shared pin */
    xTaskCreate(spi_task, "spi_task", 4096, NULL, 5, NULL);
    xTaskCreate(i2c_int_task, "i2c_int_task", 4096, NULL, 5, NULL);

    /* Wait for tasks to complete */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* --- Pin Usage Summary --- */
    ESP_LOGI(TAG, "\n--- Pin Usage Summary ---");
    ESP_LOGI(TAG, "GPIO%d: I2C SDA (dedicated)", I2C_SDA_PIN);
    ESP_LOGI(TAG, "GPIO%d: I2C SCL (dedicated)", I2C_SCL_PIN);
    ESP_LOGI(TAG, "GPIO%d: SPI COPI (dedicated)", SPI_COPI_PIN);
    ESP_LOGI(TAG, "GPIO%d: SPI CIPO (dedicated)", SPI_CIPO_PIN);
    ESP_LOGI(TAG, "GPIO%d: SPI SCLK (dedicated)", SPI_SCLK_PIN);
    ESP_LOGI(TAG, "GPIO%d: SHARED (SPI_CS / I2C_INT / GPIO)", SHARED_PIN);

    /* --- Best Practices --- */
    ESP_LOGI(TAG, "\n--- Best Practices ---");
    ESP_LOGI(TAG, "1. Use pin validator to detect conflicts early");
    ESP_LOGI(TAG, "2. Protect shared pins with mutex");
    ESP_LOGI(TAG, "3. Always release pins after use");
    ESP_LOGI(TAG, "4. Use manual CS for shared chip select");
    ESP_LOGI(TAG, "5. Document pin multiplexing in design");

    /* Cleanup */
    vSemaphoreDelete(g_shared_pin_mutex);
    star_free_pin_validator();
    star_bus_manager_deinit(&g_manager);

    ESP_LOGI(TAG, "\nShared pins example complete");
}
