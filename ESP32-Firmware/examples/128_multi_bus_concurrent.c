/**
 * @file 128_multi_bus_concurrent.c
 * @brief Multiple buses active simultaneously
 *
 * Demonstrates:
 * - I2C + SPI + UART simultaneously
 * - Independent operations
 * - Resource management
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "MULTI_BUS_CONCURRENT";

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

/* Task synchronization */
static volatile bool g_tasks_running = true;
static volatile int g_i2c_ops = 0;
static volatile int g_spi_ops = 0;
static volatile int g_uart_ops = 0;

/* I2C worker task */
static void i2c_task(void* arg)
{
    uint8_t buffer[4];
    size_t bytes_read;

    ESP_LOGI(TAG, "I2C task started");

    while (g_tasks_running) {
        esp_err_t ret = star_bus_i2c_read(&g_manager, "i2c_sensor", buffer, 4, 0x00, &bytes_read);
        if (ret == ESP_OK) {
            g_i2c_ops++;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "I2C task completed: %d operations", g_i2c_ops);
    vTaskDelete(NULL);
}

/* SPI worker task */
static void spi_task(void* arg)
{
    uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};
    uint8_t rx_buf[4];

    ESP_LOGI(TAG, "SPI task started");

    while (g_tasks_running) {
        esp_err_t ret = star_bus_spi_transfer(&g_manager, "spi_flash", tx_buf, rx_buf, 4, 0);
        if (ret == ESP_OK) {
            g_spi_ops++;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    ESP_LOGI(TAG, "SPI task completed: %d operations", g_spi_ops);
    vTaskDelete(NULL);
}

/* UART worker task */
static void uart_task(void* arg)
{
    const char* msg = "PING";
    uint8_t rx_buf[16];

    ESP_LOGI(TAG, "UART task started");

    while (g_tasks_running) {
        /* Send */
        esp_err_t ret = star_bus_uart_write(&g_manager, "uart_debug",
                                             (const uint8_t*)msg, strlen(msg));
        if (ret == ESP_OK) {
            g_uart_ops++;
        }

        /* Try to receive */
        size_t bytes_read;
        star_bus_uart_read(&g_manager, "uart_debug", rx_buf, sizeof(rx_buf),
                            &bytes_read, 100);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "UART task completed: %d operations", g_uart_ops);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Multi-Bus Concurrent Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "multi_bus", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* --- Create I2C Bus --- */
    ESP_LOGI(TAG, "Creating I2C bus...");
    star_bus_config_t* i2c_config = star_bus_config_create_i2c(
        "i2c_sensor", I2C_NUM_0, I2C_SENSOR_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    if (i2c_config != NULL) {
        star_bus_manager_add_bus(&g_manager, i2c_config);
        star_bus_config_init(i2c_config, &g_manager);
        ESP_LOGI(TAG, "  I2C bus initialized");
    }

    /* --- Create SPI Bus --- */
    ESP_LOGI(TAG, "Creating SPI bus...");
    spi_device_interface_config_t spi_dev_cfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 7,
    };

    star_bus_config_t* spi_config = star_bus_config_create_spi_device(
        "spi_flash", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_dev_cfg);

    if (spi_config != NULL) {
        star_bus_manager_add_bus(&g_manager, spi_config);
        star_bus_config_init(spi_config, &g_manager);
        ESP_LOGI(TAG, "  SPI bus initialized");
    }

    /* --- Create UART Bus --- */
    ESP_LOGI(TAG, "Creating UART bus...");
    star_uart_config_t uart_cfg = STAR_UART_CONFIG_DEFAULT();
    uart_cfg.port = UART_NUM_1;
    uart_cfg.baud_rate = 115200;
    uart_cfg.tx_pin = UART_TX_PIN;
    uart_cfg.rx_pin = UART_RX_PIN;
    uart_cfg.rx_buffer_size = 256;
    uart_cfg.tx_buffer_size = 256;

    ret = star_bus_uart_init(&g_manager, "uart_debug", &uart_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  UART bus initialized");
    }

    /* --- Run Concurrent Operations --- */
    ESP_LOGI(TAG, "\n--- Starting Concurrent Operations ---");

    int64_t start_time = esp_timer_get_time();
    g_tasks_running = true;
    g_i2c_ops = 0;
    g_spi_ops = 0;
    g_uart_ops = 0;

    /* Create worker tasks on different cores if available */
    TaskHandle_t i2c_handle, spi_handle, uart_handle;

    xTaskCreatePinnedToCore(i2c_task, "i2c_task", 4096, NULL, 5, &i2c_handle, 0);
    xTaskCreatePinnedToCore(spi_task, "spi_task", 4096, NULL, 5, &spi_handle, 1);
    xTaskCreatePinnedToCore(uart_task, "uart_task", 4096, NULL, 5, &uart_handle, 0);

    /* Let tasks run for 2 seconds */
    ESP_LOGI(TAG, "Running for 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Stop tasks */
    g_tasks_running = false;
    vTaskDelay(pdMS_TO_TICKS(200));  /* Wait for tasks to finish */

    int64_t elapsed = esp_timer_get_time() - start_time;

    /* --- Results --- */
    ESP_LOGI(TAG, "\n--- Concurrent Operation Results ---");
    ESP_LOGI(TAG, "Elapsed time: %lld ms", elapsed / 1000);
    ESP_LOGI(TAG, "I2C operations: %d (%.1f ops/sec)", g_i2c_ops,
             (float)g_i2c_ops * 1000000.0f / elapsed);
    ESP_LOGI(TAG, "SPI operations: %d (%.1f ops/sec)", g_spi_ops,
             (float)g_spi_ops * 1000000.0f / elapsed);
    ESP_LOGI(TAG, "UART operations: %d (%.1f ops/sec)", g_uart_ops,
             (float)g_uart_ops * 1000000.0f / elapsed);
    ESP_LOGI(TAG, "Total operations: %d", g_i2c_ops + g_spi_ops + g_uart_ops);

    /* --- Sequential Comparison --- */
    ESP_LOGI(TAG, "\n--- Sequential Comparison ---");

    start_time = esp_timer_get_time();
    int seq_count = 0;

    for (int i = 0; i < 20; i++) {
        uint8_t buf[4];
        size_t bytes;

        /* I2C */
        if (star_bus_i2c_read(&g_manager, "i2c_sensor", buf, 4, 0x00, &bytes) == ESP_OK) {
            seq_count++;
        }

        /* SPI */
        uint8_t tx[4] = {0x9F}, rx[4];
        if (star_bus_spi_transfer(&g_manager, "spi_flash", tx, rx, 4, 0) == ESP_OK) {
            seq_count++;
        }

        /* UART */
        if (star_bus_uart_write(&g_manager, "uart_debug", (uint8_t*)"X", 1) == ESP_OK) {
            seq_count++;
        }
    }

    elapsed = esp_timer_get_time() - start_time;
    ESP_LOGI(TAG, "Sequential: %d operations in %lld ms", seq_count, elapsed / 1000);

    /* --- Resource Usage --- */
    ESP_LOGI(TAG, "\n--- Resource Summary ---");
    ESP_LOGI(TAG, "Buses managed: 3 (I2C, SPI, UART)");
    ESP_LOGI(TAG, "I2C port: I2C_NUM_0");
    ESP_LOGI(TAG, "SPI host: SPI2_HOST");
    ESP_LOGI(TAG, "UART port: UART_NUM_1");

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nMulti-bus concurrent example complete");
}
