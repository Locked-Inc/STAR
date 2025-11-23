/**
 * @file 132_protocol_bridge.c
 * @brief Protocol bridging between different bus types
 *
 * Demonstrates:
 * - I2C to SPI bridge
 * - UART gateway
 * - Format conversion
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>

static const char* TAG = "PROTOCOL_BRIDGE";

/* Pin definitions */
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS_PIN       (GPIO_NUM_5)
#define UART_TX_PIN      (GPIO_NUM_17)
#define UART_RX_PIN      (GPIO_NUM_16)

#define I2C_SENSOR_ADDR  (0x48)

static star_bus_manager_t g_manager;

/* Bridge message format */
typedef struct {
    uint8_t msg_type;
    uint8_t device_addr;
    uint8_t reg_addr;
    uint8_t data_len;
    uint8_t data[32];
    uint8_t checksum;
} bridge_message_t;

/* Message types */
#define MSG_TYPE_I2C_READ (0x01)
#define MSG_TYPE_I2C_WRITE (0x02)
#define MSG_TYPE_SPI_XFER (0x03)
#define MSG_TYPE_RESPONSE (0x80)

/* Calculate checksum */
static uint8_t calc_checksum(const uint8_t* data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/* I2C to SPI bridge - forward I2C sensor data to SPI device */
static esp_err_t i2c_to_spi_bridge(uint8_t i2c_reg, uint8_t spi_cmd)
{
    uint8_t i2c_data[4];
    size_t bytes_read;

    /* Read from I2C sensor */
    esp_err_t ret = star_bus_i2c_read(&g_manager, "i2c_sensor", i2c_data, 4, i2c_reg, &bytes_read);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C read: 0x%02X%02X%02X%02X", i2c_data[0], i2c_data[1], i2c_data[2], i2c_data[3]);

    /* Format for SPI device */
    uint8_t spi_tx[5];
    spi_tx[0] = spi_cmd;
    memcpy(spi_tx + 1, i2c_data, 4);

    /* Send to SPI device */
    ret = star_bus_spi_transmit(&g_manager, "spi_device", spi_tx, 5, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Bridged I2C data to SPI with command 0x%02X", spi_cmd);
    return ESP_OK;
}

/* SPI to I2C bridge - read SPI data and write to I2C device */
static esp_err_t spi_to_i2c_bridge(uint8_t spi_cmd, uint8_t i2c_reg)
{
    uint8_t spi_tx[5] = {spi_cmd | 0x80, 0, 0, 0, 0};  /* Read flag */
    uint8_t spi_rx[5];

    /* Read from SPI device */
    esp_err_t ret = star_bus_spi_transfer(&g_manager, "spi_device", spi_tx, spi_rx, 5, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI read: 0x%02X%02X%02X%02X", spi_rx[1], spi_rx[2], spi_rx[3], spi_rx[4]);

    /* Write to I2C device */
    size_t bytes_written;
    ret = star_bus_i2c_write(&g_manager, "i2c_sensor", spi_rx + 1, 4, i2c_reg, &bytes_written);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Bridged SPI data to I2C register 0x%02X", i2c_reg);
    return ESP_OK;
}

/* UART gateway - receive commands via UART, execute on bus, send response */
static esp_err_t uart_gateway_process(void)
{
    uint8_t rx_buf[64];
    size_t bytes_read;

    /* Read UART command */
    esp_err_t ret = star_bus_uart_read(&g_manager, "uart_gateway", rx_buf, sizeof(rx_buf), &bytes_read, 100);

    if (ret != ESP_OK || bytes_read < 5) {
        return ESP_ERR_TIMEOUT;  /* No complete message */
    }

    /* Parse bridge message */
    bridge_message_t* msg = (bridge_message_t*)rx_buf;

    /* Verify checksum */
    uint8_t expected_cs = calc_checksum(rx_buf, 4 + msg->data_len);
    if (msg->checksum != expected_cs) {
        ESP_LOGW(TAG, "Checksum mismatch");
        return ESP_ERR_INVALID_CRC;
    }

    /* Process based on message type */
    bridge_message_t response = {
        .msg_type = MSG_TYPE_RESPONSE | msg->msg_type,
        .device_addr = msg->device_addr,
        .reg_addr = msg->reg_addr,
        .data_len = 0
    };

    switch (msg->msg_type) {
        case MSG_TYPE_I2C_READ: {
            size_t read_len;
            ret = star_bus_i2c_read(&g_manager, "i2c_sensor", response.data, msg->data_len,
                                     msg->reg_addr, &read_len);
            response.data_len = read_len;
            ESP_LOGI(TAG, "Gateway: I2C read from 0x%02X", msg->reg_addr);
            break;
        }

        case MSG_TYPE_I2C_WRITE: {
            size_t written;
            ret = star_bus_i2c_write(&g_manager, "i2c_sensor", msg->data, msg->data_len,
                                      msg->reg_addr, &written);
            response.data[0] = (ret == ESP_OK) ? 0x00 : 0xFF;
            response.data_len = 1;
            ESP_LOGI(TAG, "Gateway: I2C write to 0x%02X", msg->reg_addr);
            break;
        }

        case MSG_TYPE_SPI_XFER: {
            uint8_t spi_rx[32];
            ret = star_bus_spi_transfer(&g_manager, "spi_device", msg->data, spi_rx, msg->data_len, 0);
            memcpy(response.data, spi_rx, msg->data_len);
            response.data_len = msg->data_len;
            ESP_LOGI(TAG, "Gateway: SPI transfer");
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown message type: 0x%02X", msg->msg_type);
            return ESP_ERR_INVALID_ARG;
    }

    /* Calculate response checksum */
    response.checksum = calc_checksum((uint8_t*)&response, 4 + response.data_len);

    /* Send response */
    star_bus_uart_write(&g_manager, "uart_gateway", (uint8_t*)&response, 5 + response.data_len);

    return ret;
}

/* Format converter - convert between different data formats */
static void convert_sensor_format(const uint8_t* raw, size_t raw_len, char* ascii, size_t ascii_size)
{
    /* Convert binary sensor data to ASCII hex format */
    size_t pos = 0;
    for (size_t i = 0; i < raw_len && pos < ascii_size - 3; i++) {
        pos += snprintf(ascii + pos, ascii_size - pos, "%02X ", raw[i]);
    }
    ascii[pos > 0 ? pos - 1 : 0] = '\0';  /* Remove trailing space */
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Protocol Bridge Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "bridge_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* --- Initialize All Buses --- */
    ESP_LOGI(TAG, "Initializing buses...");

    /* I2C */
    star_bus_config_t* i2c_config = star_bus_config_create_i2c(
        "i2c_sensor", I2C_NUM_0, I2C_SENSOR_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    if (i2c_config) {
        star_bus_manager_add_bus(&g_manager, i2c_config);
        star_bus_config_init(i2c_config, &g_manager);
        ESP_LOGI(TAG, "  I2C ready");
    }

    /* SPI */
    spi_device_interface_config_t spi_cfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 7,
    };

    star_bus_config_t* spi_config = star_bus_config_create_spi_device(
        "spi_device", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_cfg);

    if (spi_config) {
        star_bus_manager_add_bus(&g_manager, spi_config);
        star_bus_config_init(spi_config, &g_manager);
        ESP_LOGI(TAG, "  SPI ready");
    }

    /* UART */
    star_uart_config_t uart_cfg = STAR_UART_CONFIG_DEFAULT();
    uart_cfg.port = UART_NUM_1;
    uart_cfg.baud_rate = 115200;
    uart_cfg.tx_pin = UART_TX_PIN;
    uart_cfg.rx_pin = UART_RX_PIN;
    uart_cfg.rx_buffer_size = 256;
    uart_cfg.tx_buffer_size = 256;

    ret = star_bus_uart_init(&g_manager, "uart_gateway", &uart_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  UART ready");
    }

    /* --- I2C to SPI Bridge --- */
    ESP_LOGI(TAG, "\n--- I2C to SPI Bridge ---");

    /* Bridge sensor data from I2C to SPI display/storage */
    ret = i2c_to_spi_bridge(0x00, 0x40);
    ESP_LOGI(TAG, "Bridge result: %s", esp_err_to_name(ret));

    ret = i2c_to_spi_bridge(0x04, 0x44);
    ESP_LOGI(TAG, "Bridge result: %s", esp_err_to_name(ret));

    /* --- SPI to I2C Bridge --- */
    ESP_LOGI(TAG, "\n--- SPI to I2C Bridge ---");

    /* Bridge config data from SPI flash to I2C EEPROM */
    ret = spi_to_i2c_bridge(0x00, 0x10);
    ESP_LOGI(TAG, "Bridge result: %s", esp_err_to_name(ret));

    /* --- UART Gateway --- */
    ESP_LOGI(TAG, "\n--- UART Gateway ---");

    /* Process a few gateway cycles */
    for (int i = 0; i < 3; i++) {
        ret = uart_gateway_process();
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGI(TAG, "Gateway cycle %d: no command", i);
        } else {
            ESP_LOGI(TAG, "Gateway cycle %d: %s", i, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* --- Format Conversion --- */
    ESP_LOGI(TAG, "\n--- Format Conversion ---");

    /* Read raw sensor data */
    uint8_t raw_data[8];
    size_t bytes_read;
    star_bus_i2c_read(&g_manager, "i2c_sensor", raw_data, 8, 0x00, &bytes_read);

    /* Convert to ASCII */
    char ascii_data[64];
    convert_sensor_format(raw_data, 8, ascii_data, sizeof(ascii_data));
    ESP_LOGI(TAG, "Raw data as ASCII: %s", ascii_data);

    /* Send ASCII via UART */
    star_bus_uart_write(&g_manager, "uart_gateway", (uint8_t*)ascii_data, strlen(ascii_data));
    star_bus_uart_write(&g_manager, "uart_gateway", (uint8_t*)"\r\n", 2);
    ESP_LOGI(TAG, "Sent ASCII data via UART");

    /* --- Bidirectional Bridge --- */
    ESP_LOGI(TAG, "\n--- Bidirectional Bridge ---");

    /* Continuous bridge operation */
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Read sensor via I2C */
        uint8_t sensor_val[2];
        star_bus_i2c_read(&g_manager, "i2c_sensor", sensor_val, 2, 0x00, &bytes_read);

        /* Forward to SPI storage */
        uint8_t spi_cmd[3] = {0x02, sensor_val[0], sensor_val[1]};  /* Write command */
        star_bus_spi_transmit(&g_manager, "spi_device", spi_cmd, 3, 0);

        /* Log via UART */
        char log_msg[32];
        snprintf(log_msg, sizeof(log_msg), "Cycle %d: %02X%02X\r\n", cycle, sensor_val[0], sensor_val[1]);
        star_bus_uart_write(&g_manager, "uart_gateway", (uint8_t*)log_msg, strlen(log_msg));

        ESP_LOGI(TAG, "Cycle %d: bridged 0x%02X%02X", cycle, sensor_val[0], sensor_val[1]);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nProtocol bridge example complete");
}
