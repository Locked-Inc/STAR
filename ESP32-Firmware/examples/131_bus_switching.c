/**
 * @file 131_bus_switching.c
 * @brief Dynamic bus switching at runtime
 *
 * Demonstrates:
 * - Switch between buses at runtime
 * - Hot-swap patterns
 * - Device migration
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "BUS_SWITCHING";

/* Pin definitions for two I2C buses */
#define I2C0_SDA_PIN     (GPIO_NUM_21)
#define I2C0_SCL_PIN     (GPIO_NUM_22)
#define I2C1_SDA_PIN     (GPIO_NUM_25)
#define I2C1_SCL_PIN     (GPIO_NUM_26)

/* SPI pins */
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS1_PIN      (GPIO_NUM_5)
#define SPI_CS2_PIN      (GPIO_NUM_4)

#define SENSOR_ADDR      (0x48)

static star_bus_manager_t g_manager;

/* Device abstraction for bus-independent operations */
typedef struct {
    const char* bus_name;
    uint8_t address;
    bool is_spi;
} device_handle_t;

static esp_err_t device_read(device_handle_t* dev, uint8_t* data, size_t len, uint8_t reg)
{
    if (dev->is_spi) {
        /* SPI read */
        uint8_t tx_buf[17] = {reg | 0x80};  /* Read flag */
        uint8_t rx_buf[17];
        esp_err_t ret = star_bus_spi_transfer(&g_manager, dev->bus_name, tx_buf, rx_buf, len + 1, 0);
        if (ret == ESP_OK) {
            memcpy(data, rx_buf + 1, len);
        }
        return ret;
    } else {
        /* I2C read */
        size_t bytes_read;
        return star_bus_i2c_read(&g_manager, dev->bus_name, data, len, reg, &bytes_read);
    }
}

static esp_err_t device_write(device_handle_t* dev, const uint8_t* data, size_t len, uint8_t reg)
{
    if (dev->is_spi) {
        /* SPI write */
        uint8_t tx_buf[17];
        tx_buf[0] = reg & 0x7F;  /* Write flag */
        memcpy(tx_buf + 1, data, len);
        return star_bus_spi_transmit(&g_manager, dev->bus_name, tx_buf, len + 1, 0);
    } else {
        /* I2C write */
        size_t bytes_written;
        return star_bus_i2c_write(&g_manager, dev->bus_name, data, len, reg, &bytes_written);
    }
}

/* Switch device from one bus to another */
static esp_err_t switch_device_bus(device_handle_t* dev, const char* new_bus, bool new_is_spi)
{
    ESP_LOGI(TAG, "Switching device from %s to %s", dev->bus_name, new_bus);

    /* Verify new bus exists */
    star_bus_config_t* new_config = star_bus_manager_find_bus(&g_manager, new_bus);
    if (new_config == NULL) {
        ESP_LOGE(TAG, "Target bus '%s' not found", new_bus);
        return ESP_ERR_NOT_FOUND;
    }

    /* Update device handle */
    dev->bus_name = new_bus;
    dev->is_spi = new_is_spi;

    ESP_LOGI(TAG, "Device now using bus: %s (SPI=%d)", dev->bus_name, dev->is_spi);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Bus Switching Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "switch_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* --- Create Multiple I2C Buses --- */
    ESP_LOGI(TAG, "Creating I2C buses...");

    star_bus_config_t* i2c0_config = star_bus_config_create_i2c(
        "i2c_bus0", I2C_NUM_0, SENSOR_ADDR,
        I2C0_SDA_PIN, I2C0_SCL_PIN, 400000);

    star_bus_config_t* i2c1_config = star_bus_config_create_i2c(
        "i2c_bus1", I2C_NUM_1, SENSOR_ADDR,
        I2C1_SDA_PIN, I2C1_SCL_PIN, 400000);

    if (i2c0_config) {
        star_bus_manager_add_bus(&g_manager, i2c0_config);
        star_bus_config_init(i2c0_config, &g_manager);
        ESP_LOGI(TAG, "  I2C bus 0 ready");
    }

    if (i2c1_config) {
        star_bus_manager_add_bus(&g_manager, i2c1_config);
        star_bus_config_init(i2c1_config, &g_manager);
        ESP_LOGI(TAG, "  I2C bus 1 ready");
    }

    /* --- Create SPI Bus with Multiple CS --- */
    ESP_LOGI(TAG, "Creating SPI buses...");

    spi_device_interface_config_t spi_cfg1 = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS1_PIN,
        .queue_size = 7,
    };

    spi_device_interface_config_t spi_cfg2 = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS2_PIN,
        .queue_size = 7,
    };

    star_bus_config_t* spi1_config = star_bus_config_create_spi_device(
        "spi_dev1", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_cfg1);

    star_bus_config_t* spi2_config = star_bus_config_create_spi_device(
        "spi_dev2", SPI2_HOST,
        SPI_COPI_PIN, SPI_CIPO_PIN, SPI_SCLK_PIN,
        SPI_DMA_CH_AUTO, &spi_cfg2);

    if (spi1_config) {
        star_bus_manager_add_bus(&g_manager, spi1_config);
        star_bus_config_init(spi1_config, &g_manager);
        ESP_LOGI(TAG, "  SPI device 1 ready");
    }

    if (spi2_config) {
        star_bus_manager_add_bus(&g_manager, spi2_config);
        star_bus_config_init(spi2_config, &g_manager);
        ESP_LOGI(TAG, "  SPI device 2 ready");
    }

    /* --- Dynamic Bus Switching --- */
    ESP_LOGI(TAG, "\n--- Dynamic Bus Switching ---");

    /* Create device handle starting on I2C bus 0 */
    device_handle_t sensor = {
        .bus_name = "i2c_bus0",
        .address = SENSOR_ADDR,
        .is_spi = false
    };

    uint8_t data[4];

    /* Read from I2C bus 0 */
    ESP_LOGI(TAG, "Reading from %s...", sensor.bus_name);
    ret = device_read(&sensor, data, 4, 0x00);
    ESP_LOGI(TAG, "  Result: %s", esp_err_to_name(ret));

    /* Switch to I2C bus 1 */
    switch_device_bus(&sensor, "i2c_bus1", false);

    /* Read from I2C bus 1 */
    ESP_LOGI(TAG, "Reading from %s...", sensor.bus_name);
    ret = device_read(&sensor, data, 4, 0x00);
    ESP_LOGI(TAG, "  Result: %s", esp_err_to_name(ret));

    /* Switch to SPI */
    switch_device_bus(&sensor, "spi_dev1", true);

    /* Read via SPI */
    ESP_LOGI(TAG, "Reading from %s...", sensor.bus_name);
    ret = device_read(&sensor, data, 4, 0x00);
    ESP_LOGI(TAG, "  Result: %s", esp_err_to_name(ret));

    /* --- Hot-Swap Pattern --- */
    ESP_LOGI(TAG, "\n--- Hot-Swap Pattern ---");

    /* Simulate primary bus failure and fallback */
    const char* primary_bus = "i2c_bus0";
    const char* backup_bus = "i2c_bus1";

    device_handle_t critical_device = {
        .bus_name = primary_bus,
        .address = SENSOR_ADDR,
        .is_spi = false
    };

    /* Try primary */
    ret = device_read(&critical_device, data, 4, 0x00);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Primary bus failed, switching to backup...");
        switch_device_bus(&critical_device, backup_bus, false);
        ret = device_read(&critical_device, data, 4, 0x00);
        ESP_LOGI(TAG, "Backup bus result: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Primary bus OK");
    }

    /* --- Load Balancing --- */
    ESP_LOGI(TAG, "\n--- Load Balancing Pattern ---");

    const char* buses[] = {"i2c_bus0", "i2c_bus1"};
    int bus_index = 0;

    for (int i = 0; i < 6; i++) {
        /* Round-robin bus selection */
        const char* selected_bus = buses[bus_index];
        bus_index = (bus_index + 1) % 2;

        device_handle_t lb_device = {
            .bus_name = selected_bus,
            .address = SENSOR_ADDR,
            .is_spi = false
        };

        ret = device_read(&lb_device, data, 4, 0x00);
        ESP_LOGI(TAG, "Read %d using %s: %s", i, selected_bus, esp_err_to_name(ret));
    }

    /* --- Runtime Bus Addition --- */
    ESP_LOGI(TAG, "\n--- Runtime Bus Addition ---");

    /* Add a new bus at runtime */
    star_bus_config_t* runtime_config = star_bus_config_create_i2c(
        "runtime_bus", I2C_NUM_0, 0x50,
        I2C0_SDA_PIN, I2C0_SCL_PIN, 100000);

    if (runtime_config) {
        ret = star_bus_manager_add_bus(&g_manager, runtime_config);
        if (ret == ESP_OK) {
            ret = star_bus_config_init(runtime_config, &g_manager);
            ESP_LOGI(TAG, "Runtime bus added: %s", esp_err_to_name(ret));

            /* Use the new bus */
            device_handle_t runtime_dev = {
                .bus_name = "runtime_bus",
                .address = 0x50,
                .is_spi = false
            };

            ret = device_read(&runtime_dev, data, 4, 0x00);
            ESP_LOGI(TAG, "Runtime bus read: %s", esp_err_to_name(ret));
        }
    }

    /* --- Bus Removal --- */
    ESP_LOGI(TAG, "\n--- Bus Removal ---");

    ret = star_bus_manager_remove_bus(&g_manager, "runtime_bus");
    ESP_LOGI(TAG, "Runtime bus removed: %s", esp_err_to_name(ret));

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nBus switching example complete");
}
