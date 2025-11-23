/**
 * @file 133_mixed_speeds.c
 * @brief Different speed buses working together
 *
 * Demonstrates:
 * - Fast SPI + slow I2C
 * - Optimize for each device
 * - Throughput balancing
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "MIXED_SPEEDS";

/* Pin definitions */
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS_PIN       (GPIO_NUM_5)

#define I2C_SENSOR_ADDR  (0x48)

/* Speed configurations */
#define I2C_SLOW_SPEED   (100000)    /* 100 kHz - Standard mode */
#define I2C_FAST_SPEED   (400000)    /* 400 kHz - Fast mode */
#define SPI_SLOW_SPEED   (1000000)   /* 1 MHz */
#define SPI_FAST_SPEED   (10000000)  /* 10 MHz */
#define SPI_HIGH_SPEED   (40000000)  /* 40 MHz */

static star_bus_manager_t g_manager;

/* Throughput measurement */
typedef struct {
    const char* bus_name;
    uint32_t bytes_transferred;
    int64_t total_time_us;
    float throughput_kbps;
} throughput_stats_t;

static void measure_i2c_throughput(const char* bus_name, throughput_stats_t* stats)
{
    uint8_t buffer[32];
    size_t bytes_read;
    int iterations = 100;

    stats->bus_name = bus_name;
    stats->bytes_transferred = 0;

    int64_t start = esp_timer_get_time();

    for (int i = 0; i < iterations; i++) {
        esp_err_t ret = star_bus_i2c_read(&g_manager, bus_name, buffer, sizeof(buffer), 0x00, &bytes_read);
        if (ret == ESP_OK) {
            stats->bytes_transferred += bytes_read;
        }
    }

    stats->total_time_us = esp_timer_get_time() - start;
    stats->throughput_kbps = (stats->bytes_transferred * 8000.0f) / stats->total_time_us;
}

static void measure_spi_throughput(const char* bus_name, throughput_stats_t* stats)
{
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    memset(tx_buf, 0xAA, sizeof(tx_buf));
    int iterations = 100;

    stats->bus_name = bus_name;
    stats->bytes_transferred = 0;

    int64_t start = esp_timer_get_time();

    for (int i = 0; i < iterations; i++) {
        esp_err_t ret = star_bus_spi_transfer(&g_manager, bus_name, tx_buf, rx_buf, sizeof(tx_buf), 0);
        if (ret == ESP_OK) {
            stats->bytes_transferred += sizeof(tx_buf);
        }
    }

    stats->total_time_us = esp_timer_get_time() - start;
    stats->throughput_kbps = (stats->bytes_transferred * 8000.0f) / stats->total_time_us;
}

static void print_throughput_stats(const throughput_stats_t* stats)
{
    ESP_LOGI(TAG, "%s:", stats->bus_name);
    ESP_LOGI(TAG, "  Bytes: %lu", (unsigned long)stats->bytes_transferred);
    ESP_LOGI(TAG, "  Time: %lld ms", stats->total_time_us / 1000);
    ESP_LOGI(TAG, "  Throughput: %.2f kbps", stats->throughput_kbps);
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Mixed Speed Buses Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "speed_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* --- Create I2C Buses at Different Speeds --- */
    ESP_LOGI(TAG, "Creating I2C buses at different speeds...");

    star_bus_config_t* i2c_slow = star_bus_config_create_i2c(
        "i2c_slow", I2C_NUM_0, I2C_SENSOR_ADDR,
        I2C_SDA_PIN, I2C_SCL_PIN, I2C_SLOW_SPEED);

    if (i2c_slow) {
        star_bus_manager_add_bus(&g_manager, i2c_slow);
        star_bus_config_init(i2c_slow, &g_manager);
        ESP_LOGI(TAG, "  I2C slow (100 kHz) ready");
    }

    /* Note: Can't have two I2C configs on same port simultaneously */
    /* In real application, you'd reconfigure or use I2C_NUM_1 */

    /* --- Create SPI Buses at Different Speeds --- */
    ESP_LOGI(TAG, "Creating SPI devices at different speeds...");

    spi_device_interface_config_t spi_slow_cfg = {
        .clock_speed_hz = SPI_SLOW_SPEED,
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 7,
    };

    star_bus_config_t* spi_slow = star_bus_config_create_spi_device(
        "spi_slow", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_slow_cfg);

    if (spi_slow) {
        star_bus_manager_add_bus(&g_manager, spi_slow);
        star_bus_config_init(spi_slow, &g_manager);
        ESP_LOGI(TAG, "  SPI slow (1 MHz) ready");
    }

    spi_device_interface_config_t spi_fast_cfg = {
        .clock_speed_hz = SPI_FAST_SPEED,
        .mode = 0,
        .spics_io_num = GPIO_NUM_4,  /* Different CS for second device */
        .queue_size = 7,
    };

    star_bus_config_t* spi_fast = star_bus_config_create_spi_device(
        "spi_fast", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_fast_cfg);

    if (spi_fast) {
        star_bus_manager_add_bus(&g_manager, spi_fast);
        star_bus_config_init(spi_fast, &g_manager);
        ESP_LOGI(TAG, "  SPI fast (10 MHz) ready");
    }

    /* --- Throughput Comparison --- */
    ESP_LOGI(TAG, "\n--- Throughput Comparison ---");

    throughput_stats_t stats;

    /* I2C throughput */
    measure_i2c_throughput("i2c_slow", &stats);
    print_throughput_stats(&stats);

    /* SPI slow throughput */
    measure_spi_throughput("spi_slow", &stats);
    print_throughput_stats(&stats);

    /* SPI fast throughput */
    measure_spi_throughput("spi_fast", &stats);
    print_throughput_stats(&stats);

    /* --- Optimal Bus Selection --- */
    ESP_LOGI(TAG, "\n--- Optimal Bus Selection ---");

    /* For small control data, use I2C */
    ESP_LOGI(TAG, "Small control data (< 10 bytes): Use I2C");
    uint8_t ctrl_data[4];
    size_t bytes;
    int64_t start = esp_timer_get_time();
    star_bus_i2c_read(&g_manager, "i2c_slow", ctrl_data, 4, 0x00, &bytes);
    int64_t i2c_small_time = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "  I2C 4-byte read: %lld us", i2c_small_time);

    /* For bulk data, use fast SPI */
    ESP_LOGI(TAG, "Bulk data (> 100 bytes): Use fast SPI");
    uint8_t bulk_tx[256], bulk_rx[256];
    start = esp_timer_get_time();
    star_bus_spi_transfer(&g_manager, "spi_fast", bulk_tx, bulk_rx, 256, 0);
    int64_t spi_bulk_time = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "  SPI 256-byte transfer: %lld us", spi_bulk_time);

    /* --- Mixed Operation Pattern --- */
    ESP_LOGI(TAG, "\n--- Mixed Operation Pattern ---");

    /* Typical sensor application pattern */
    for (int cycle = 0; cycle < 5; cycle++) {
        int64_t cycle_start = esp_timer_get_time();

        /* 1. Read sensor config via slow I2C */
        uint8_t config[2];
        star_bus_i2c_read(&g_manager, "i2c_slow", config, 2, 0x00, &bytes);

        /* 2. Read bulk sensor data via fast SPI */
        uint8_t sensor_data[128];
        uint8_t tx_cmd[128] = {0x80};  /* Read command */
        star_bus_spi_transfer(&g_manager, "spi_fast", tx_cmd, sensor_data, 128, 0);

        /* 3. Write processed result via slow SPI (to EEPROM-like device) */
        uint8_t result[8] = {0x02, 0x00};  /* Write command + address */
        memcpy(result + 2, sensor_data, 4);
        star_bus_spi_transmit(&g_manager, "spi_slow", result, 6, 0);

        int64_t cycle_time = esp_timer_get_time() - cycle_start;
        ESP_LOGI(TAG, "Cycle %d: %lld us", cycle, cycle_time);
    }

    /* --- Throughput Balancing --- */
    ESP_LOGI(TAG, "\n--- Throughput Balancing ---");

    /* Calculate optimal batch sizes for balanced throughput */
    /* Assume we need to transfer 1KB total */
    uint32_t total_bytes = 1024;

    /* Option 1: All via slow I2C */
    float i2c_only_time = (total_bytes * 8.0f) / (I2C_SLOW_SPEED);
    ESP_LOGI(TAG, "All via I2C (100kHz): ~%.2f ms", i2c_only_time * 1000);

    /* Option 2: All via fast SPI */
    float spi_only_time = (total_bytes * 8.0f) / (SPI_FAST_SPEED);
    ESP_LOGI(TAG, "All via SPI (10MHz): ~%.2f ms", spi_only_time * 1000);

    /* Option 3: Split - config via I2C, data via SPI */
    uint32_t config_bytes = 32;
    uint32_t data_bytes = total_bytes - config_bytes;
    float mixed_time = (config_bytes * 8.0f) / I2C_SLOW_SPEED + (data_bytes * 8.0f) / SPI_FAST_SPEED;
    ESP_LOGI(TAG, "Mixed (32B I2C + 992B SPI): ~%.2f ms", mixed_time * 1000);

    /* --- Device-Specific Speed Selection --- */
    ESP_LOGI(TAG, "\n--- Device Speed Recommendations ---");
    ESP_LOGI(TAG, "EEPROM (24Cxx): I2C 100-400 kHz");
    ESP_LOGI(TAG, "Temperature sensor: I2C 100-400 kHz");
    ESP_LOGI(TAG, "Accelerometer: SPI 1-10 MHz");
    ESP_LOGI(TAG, "Display (SSD1306): SPI 10-20 MHz");
    ESP_LOGI(TAG, "Flash memory: SPI 20-80 MHz");
    ESP_LOGI(TAG, "SD Card: SPI 25-50 MHz");

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nMixed speeds example complete");
}
