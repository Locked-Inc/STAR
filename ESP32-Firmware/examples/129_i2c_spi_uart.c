/**
 * @file 129_i2c_spi_uart.c
 * @brief Complete system with all protocols together
 *
 * Demonstrates:
 * - Sensor on I2C, display on SPI, debug on UART
 * - Coordinated operations
 * - Complete system example
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

static const char* TAG = "I2C_SPI_UART";

/* Pin definitions */
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS_PIN       (GPIO_NUM_5)
#define SPI_DC_PIN       (GPIO_NUM_4)
#define UART_TX_PIN      (GPIO_NUM_17)
#define UART_RX_PIN      (GPIO_NUM_16)

/* Device addresses */
#define TEMP_SENSOR_ADDR  (0x48)  /* TMP102 temperature sensor */
#define HUMIDITY_ADDR     (0x40)  /* SI7021 humidity sensor */

static star_bus_manager_t g_manager;

/* Simulated sensor data structure */
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp;
} sensor_data_t;

/* Read temperature from I2C sensor */
static esp_err_t read_temperature(float* temp)
{
    uint8_t data[2];
    size_t bytes_read;

    esp_err_t ret = star_bus_i2c_read(&g_manager, "temp_sensor", data, 2, 0x00, &bytes_read);
    if (ret == ESP_OK && bytes_read == 2) {
        /* Convert TMP102 format to temperature */
        int16_t raw = (data[0] << 4) | (data[1] >> 4);
        if (raw & 0x800) raw |= 0xF000;  /* Sign extend */
        *temp = raw * 0.0625f;
        return ESP_OK;
    }
    /* Return simulated value on error */
    *temp = 25.0f + (rand() % 100) / 100.0f;
    return ESP_OK;
}

/* Read humidity from I2C sensor */
static esp_err_t read_humidity(float* humidity)
{
    uint8_t cmd = 0xE5;  /* Measure humidity command */
    uint8_t data[2];
    size_t bytes_written, bytes_read;

    /* Write command */
    esp_err_t ret = star_bus_i2c_write(&g_manager, "humidity_sensor", &cmd, 1, 0x00, &bytes_written);
    if (ret != ESP_OK) {
        *humidity = 50.0f + (rand() % 100) / 100.0f;
        return ESP_OK;
    }

    vTaskDelay(pdMS_TO_TICKS(20));  /* Wait for measurement */

    /* Read result */
    ret = star_bus_i2c_read(&g_manager, "humidity_sensor", data, 2, 0x00, &bytes_read);
    if (ret == ESP_OK && bytes_read == 2) {
        uint16_t raw = (data[0] << 8) | data[1];
        *humidity = (125.0f * raw / 65536.0f) - 6.0f;
        return ESP_OK;
    }

    *humidity = 50.0f + (rand() % 100) / 100.0f;
    return ESP_OK;
}

/* Send display command via SPI */
static esp_err_t display_send_command(uint8_t cmd)
{
    /* For displays with D/C pin, command mode is D/C=0 */
    uint8_t tx_buf[1] = {cmd};
    return star_bus_spi_transmit(&g_manager, "display", tx_buf, 1, 0);
}

/* Send display data via SPI */
static esp_err_t display_send_data(const uint8_t* data, size_t len)
{
    return star_bus_spi_transmit(&g_manager, "display", data, len, 0);
}

/* Initialize display */
static esp_err_t display_init(void)
{
    /* Send initialization sequence for typical SPI display */
    display_send_command(0xAE);  /* Display off */
    display_send_command(0xD5);  /* Set display clock */
    display_send_command(0x80);
    display_send_command(0xA8);  /* Set multiplex */
    display_send_command(0x3F);
    display_send_command(0xD3);  /* Set display offset */
    display_send_command(0x00);
    display_send_command(0x40);  /* Set start line */
    display_send_command(0xAF);  /* Display on */

    ESP_LOGI(TAG, "Display initialized");
    return ESP_OK;
}

/* Update display with sensor data */
static esp_err_t display_update(const sensor_data_t* data)
{
    /* In real application, this would render text/graphics */
    /* Here we just send some data bytes */
    uint8_t display_buf[16];

    /* Format temperature for display */
    display_buf[0] = 'T';
    display_buf[1] = ':';
    display_buf[2] = (uint8_t)data->temperature;
    display_buf[3] = '.';
    display_buf[4] = (uint8_t)((data->temperature - (int)data->temperature) * 10);

    display_send_data(display_buf, 8);

    ESP_LOGI(TAG, "Display updated");
    return ESP_OK;
}

/* Send debug message via UART */
static esp_err_t debug_log(const char* message)
{
    esp_err_t ret = star_bus_uart_write(&g_manager, "debug_uart",
                                         (const uint8_t*)message, strlen(message));
    if (ret == ESP_OK) {
        /* Send newline */
        star_bus_uart_write(&g_manager, "debug_uart", (const uint8_t*)"\r\n", 2);
    }
    return ret;
}

/* Format and send sensor data via UART */
static esp_err_t debug_send_data(const sensor_data_t* data)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "[%lu] T=%.2fC H=%.1f%%",
             (unsigned long)data->timestamp, data->temperature, data->humidity);
    return debug_log(buf);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Complete I2C/SPI/UART System Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "full_system", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* --- Initialize I2C Sensors --- */
    ESP_LOGI(TAG, "Initializing I2C sensors...");

    star_bus_config_t* temp_config = star_bus_config_create_i2c(
        "temp_sensor", I2C_NUM_0, TEMP_SENSOR_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    star_bus_config_t* humidity_config = star_bus_config_create_i2c(
        "humidity_sensor", I2C_NUM_0, HUMIDITY_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    if (temp_config) {
        star_bus_manager_add_bus(&g_manager, temp_config);
        star_bus_config_init(temp_config, &g_manager);
        ESP_LOGI(TAG, "  Temperature sensor ready");
    }

    if (humidity_config) {
        star_bus_manager_add_bus(&g_manager, humidity_config);
        star_bus_config_init(humidity_config, &g_manager);
        ESP_LOGI(TAG, "  Humidity sensor ready");
    }

    /* --- Initialize SPI Display --- */
    ESP_LOGI(TAG, "Initializing SPI display...");

    spi_device_interface_config_t display_cfg = {
        .clock_speed_hz = 10000000,  /* 10 MHz */
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 7,
    };

    star_bus_config_t* display_config = star_bus_config_create_spi_device(
        "display", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &display_cfg);

    if (display_config) {
        star_bus_manager_add_bus(&g_manager, display_config);
        star_bus_config_init(display_config, &g_manager);
        display_init();
        ESP_LOGI(TAG, "  Display ready");
    }

    /* --- Initialize Debug UART --- */
    ESP_LOGI(TAG, "Initializing debug UART...");

    star_uart_config_t uart_cfg = STAR_UART_CONFIG_DEFAULT();
    uart_cfg.port = UART_NUM_1;
    uart_cfg.baud_rate = 115200;
    uart_cfg.tx_pin = UART_TX_PIN;
    uart_cfg.rx_pin = UART_RX_PIN;
    uart_cfg.rx_buffer_size = 256;
    uart_cfg.tx_buffer_size = 256;

    ret = star_bus_uart_init(&g_manager, "debug_uart", &uart_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  Debug UART ready");
    }

    /* --- Main Application Loop --- */
    ESP_LOGI(TAG, "\n--- Starting Main Loop ---");
    debug_log("System initialized");

    sensor_data_t sensor_data;
    uint32_t sample_count = 0;

    for (int cycle = 0; cycle < 10; cycle++) {
        sensor_data.timestamp = sample_count++;

        /* 1. Read sensors (I2C) */
        read_temperature(&sensor_data.temperature);
        read_humidity(&sensor_data.humidity);

        ESP_LOGI(TAG, "Cycle %d: T=%.2fC, H=%.1f%%",
                 cycle, sensor_data.temperature, sensor_data.humidity);

        /* 2. Update display (SPI) */
        display_update(&sensor_data);

        /* 3. Send debug data (UART) */
        debug_send_data(&sensor_data);

        /* Wait before next cycle */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* --- Shutdown --- */
    ESP_LOGI(TAG, "\n--- Shutdown ---");
    debug_log("System shutting down");

    display_send_command(0xAE);  /* Display off */

    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nI2C/SPI/UART system example complete");
}
