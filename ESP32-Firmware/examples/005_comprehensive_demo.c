/**
 * @file 05_comprehensive_demo.c
 * @brief Comprehensive demonstration of all star_bus library features
 *
 * This example combines all library features:
 * - Bus manager with error handling and pin validation
 * - I2C and SPI bus types
 * - SMBus protocol
 * - Proper error handling and cleanup
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"
#include "star_bus_spi.h"
#include "star_error_handler.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "star_pin_validator.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "COMPREHENSIVE_DEMO";

/* Pin definitions */
#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define SPI_COPI_PIN     (GPIO_NUM_23)
#define SPI_CIPO_PIN     (GPIO_NUM_19)
#define SPI_SCLK_PIN     (GPIO_NUM_18)
#define SPI_CS_PIN       (GPIO_NUM_5)

/* Device addresses */
#define TEMP_SENSOR_ADDR (0x48)
#define EEPROM_ADDR      (0x50)

/* Global instances */
static star_bus_manager_t s_manager;
static error_handler_t    s_error_handler;

/* Initialize all subsystems */
static esp_err_t priv_init_system(void)
{
  esp_err_t ret;

  /* Initialize error handler */
  ret = error_handler_init(&s_error_handler, 3, 100, 2000, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Error handler init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Create interfaces */
  star_error_interface_t error_iface;
  error_handler_get_interface(&error_iface, &s_error_handler);

  star_pin_interface_t pin_iface;
  pin_validator_get_interface(&pin_iface);

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "ComprehensiveDemo", &error_iface,
                              &pin_iface);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Bus manager init failed: %s", esp_err_to_name(ret));
    error_handler_deinit(&s_error_handler);
    return ret;
  }

  ESP_LOGI(s_tag, "System initialized successfully");
  return ESP_OK;
}

/* Initialize all buses */
static esp_err_t priv_init_buses(void)
{
  esp_err_t ret;

  /* Create I2C bus for temperature sensor */
  star_bus_config_t *i2c_temp = star_bus_config_create_i2c(
    "temp_sensor",
    I2C_NUM_0,
    TEMP_SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    400000  /* 400 kHz Fast Mode */
  );

  if (i2c_temp == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config for temp sensor");
    return ESP_FAIL;
  }

  ret = star_bus_manager_add_bus(&s_manager, i2c_temp);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add temp sensor: %s", esp_err_to_name(ret));
    star_bus_config_destroy(i2c_temp);
    return ret;
  }

  ret = star_bus_config_init(i2c_temp, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init temp sensor: %s", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(s_tag, "Temperature sensor (I2C) initialized");

  /* Create I2C bus for EEPROM (same I2C port, different address) */
  star_bus_config_t *i2c_eeprom = star_bus_config_create_i2c(
    "eeprom",
    I2C_NUM_0,
    EEPROM_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    100000  /* 100 kHz Standard Mode for EEPROM */
  );

  if (i2c_eeprom != NULL) {
    ret = star_bus_manager_add_bus(&s_manager, i2c_eeprom);
    if (ret == ESP_OK) {
      ret = star_bus_config_init(i2c_eeprom, &s_manager);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "EEPROM (I2C) initialized");
      }
    }
  }

  /* Create SPI bus for flash device */
  spi_device_interface_config_t spi_dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
  };

  star_bus_config_t *spi_flash = star_bus_config_create_spi_device(
    "spi_flash",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &spi_dev_cfg
  );

  if (spi_flash != NULL) {
    ret = star_bus_manager_add_bus(&s_manager, spi_flash);
    if (ret == ESP_OK) {
      ret = star_bus_config_init(spi_flash, &s_manager);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "SPI flash initialized");
      }
    }
  }

  return ESP_OK;
}

/* Demonstrate I2C operations */
static void priv_demo_i2c_operations(void)
{
  ESP_LOGI(s_tag, "\n=== I2C Operations ===");

  /* Read from temperature sensor register */
  uint8_t   data[2];
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(&s_manager, "temp_sensor", data, 2, 0x00,
                                    &bytes_read);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Temp sensor read: 0x%02X 0x%02X", data[0], data[1]);
  } else {
    ESP_LOGW(s_tag, "Temp sensor read failed: %s", esp_err_to_name(ret));
  }

  /* SMBus read byte */
  uint8_t byte;
  ret = star_smbus_read_byte(&s_manager, "temp_sensor", TEMP_SENSOR_ADDR, 0x00,
                             &byte);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMBus read byte: 0x%02X", byte);
  }

  /* SMBus write byte */
  ret = star_smbus_write_byte(&s_manager, "temp_sensor", TEMP_SENSOR_ADDR, 0x01,
                              0x55);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMBus write byte: 0x55 to reg 0x01");
  }

  /* EEPROM operations */
  uint8_t eeprom_data[4];
  ret = star_bus_i2c_read(&s_manager, "eeprom", eeprom_data, 4, 0x00,
                          &bytes_read);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "EEPROM read: 0x%02X 0x%02X 0x%02X 0x%02X",
             eeprom_data[0], eeprom_data[1], eeprom_data[2], eeprom_data[3]);
  }
}

/* Demonstrate SPI operations */
static void priv_demo_spi_operations(void)
{
  ESP_LOGI(s_tag, "\n=== SPI Operations ===");

  /* Send JEDEC ID command and receive response */
  uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};
  uint8_t rx_buf[4] = {0};

  esp_err_t ret = star_bus_spi_transfer(&s_manager, "spi_flash", tx_buf, rx_buf,
                                        4, 0);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SPI JEDEC ID: 0x%02X 0x%02X 0x%02X",
             rx_buf[1], rx_buf[2], rx_buf[3]);
  } else {
    ESP_LOGW(s_tag, "SPI transfer failed: %s", esp_err_to_name(ret));
  }

  /* Transmit only */
  uint8_t cmd[] = {0x06};  /* Write enable command */
  ret = star_bus_spi_transmit(&s_manager, "spi_flash", cmd, 1, 0);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SPI write enable sent");
  }
}

/* Print system status */
static void priv_print_status(void)
{
  ESP_LOGI(s_tag, "\n=== System Status ===");

  /* Validate all pins */
  esp_err_t ret = star_validate_pins();
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pin validation: PASSED");
  } else {
    ESP_LOGW(s_tag, "Pin validation: CONFLICTS DETECTED");
  }

  /* Error handler stats */
  ESP_LOGI(s_tag, "Errors recorded: %lu",
           (unsigned long)s_error_handler.error_count);
  ESP_LOGI(s_tag, "In error state: %s",
           s_error_handler.in_error_state ? "yes" : "no");
}

/* Cleanup all resources */
static void priv_cleanup_system(void)
{
  ESP_LOGI(s_tag, "\n=== Cleanup ===");

  star_bus_manager_deinit(&s_manager);
  error_handler_deinit(&s_error_handler);
  star_free_pin_validator();

  ESP_LOGI(s_tag, "All resources freed");
}

/* Main demonstration function */
void comprehensive_demo(void)
{
  ESP_LOGI(s_tag, "========================================");
  ESP_LOGI(s_tag, "   Star Bus Library Comprehensive Demo");
  ESP_LOGI(s_tag, "========================================\n");

  /* Initialize */
  if (priv_init_system() != ESP_OK) {
    ESP_LOGE(s_tag, "System initialization failed");
    return;
  }

  if (priv_init_buses() != ESP_OK) {
    ESP_LOGE(s_tag, "Bus initialization failed");
    priv_cleanup_system();
    return;
  }

  /* Run demonstrations */
  priv_demo_i2c_operations();
  priv_demo_spi_operations();

  /* Print status */
  priv_print_status();

  /* Cleanup */
  priv_cleanup_system();

  ESP_LOGI(s_tag, "\n========================================");
  ESP_LOGI(s_tag, "   Demo Complete");
  ESP_LOGI(s_tag, "========================================");
}

/* App main entry point */
void app_main(void)
{
  comprehensive_demo();
}
