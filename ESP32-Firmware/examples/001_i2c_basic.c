/**
 * @file 01_i2c_basic.c
 * @brief Basic I2C operations example
 *
 * This example demonstrates:
 * - Creating an I2C bus configuration
 * - Adding it to the bus manager
 * - Initializing the bus
 * - Performing read/write operations
 * - Using SMBus protocol functions
 * - Proper cleanup
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>

static const char *s_tag = "I2C_EXAMPLE";

/* Pin definitions - adjust for your hardware */
#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)

/* I2C device address (7-bit) - example: temperature sensor */
#define SENSOR_ADDR   (0x48)

/* Clock speed */
#define I2C_CLK_SPEED (100000)  /* 100 kHz Standard Mode */

void i2c_basic_example(void)
{
  esp_err_t ret;

  /* Step 1: Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Example", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Step 2: Create I2C bus configuration */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "temp_sensor",    /* Unique name for this bus/device */
    I2C_NUM_0,        /* I2C port */
    SENSOR_ADDR,      /* 7-bit device address */
    I2C_SDA_PIN,      /* SDA GPIO */
    I2C_SCL_PIN,      /* SCL GPIO */
    I2C_CLK_SPEED     /* Clock speed in Hz */
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "I2C config created for address 0x%02X", SENSOR_ADDR);

  /* Step 3: Add config to manager */
  ret = star_bus_manager_add_bus(&manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add bus: %s", esp_err_to_name(ret));
    star_bus_config_destroy(i2c_config);
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "I2C bus added to manager");

  /* Step 4: Initialize the I2C driver */
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "I2C driver initialized");

  /* Step 5: Read from a register */
  uint8_t reg_data[2];
  size_t  bytes_read;
  ret = star_bus_i2c_read(&manager, "temp_sensor", reg_data, 2, 0x00, &bytes_read);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read %u bytes from reg 0x00: 0x%02X 0x%02X",
             (unsigned)bytes_read, reg_data[0], reg_data[1]);
  } else {
    ESP_LOGW(s_tag, "Read failed: %s (device may not be present)",
             esp_err_to_name(ret));
  }

  /* Step 6: Write to a register */
  uint8_t write_data[] = {0x55};
  size_t  bytes_written;
  ret = star_bus_i2c_write(&manager, "temp_sensor", write_data, 1, 0x01,
                           &bytes_written);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Wrote %u bytes to reg 0x01", (unsigned)bytes_written);
  } else {
    ESP_LOGW(s_tag, "Write failed: %s", esp_err_to_name(ret));
  }

  /* Step 7: Send a command byte (no register address) */
  ret = star_bus_i2c_write_command(&manager, "temp_sensor", 0xF3);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Command 0xF3 sent");
  } else {
    ESP_LOGW(s_tag, "Command failed: %s", esp_err_to_name(ret));
  }

  /* Step 8: Raw read (no register address) */
  uint8_t raw_data[4];
  ret = star_bus_i2c_read_raw(&manager, "temp_sensor", raw_data, 4, &bytes_read);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Raw read %u bytes: 0x%02X 0x%02X 0x%02X 0x%02X",
             (unsigned)bytes_read, raw_data[0], raw_data[1], raw_data[2],
             raw_data[3]);
  } else {
    ESP_LOGW(s_tag, "Raw read failed: %s", esp_err_to_name(ret));
  }

  /* Step 9: SMBus operations */
  /* Note: SMBus functions take bus_name and device address as separate params */
  ESP_LOGI(s_tag, "\n--- SMBus Operations ---");

  /* SMBus read byte from register */
  uint8_t smbus_byte;
  ret = star_smbus_read_byte(&manager, "temp_sensor", SENSOR_ADDR, 0x00,
                             &smbus_byte);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMBus read byte: 0x%02X", smbus_byte);
  }

  /* SMBus write byte to register */
  ret = star_smbus_write_byte(&manager, "temp_sensor", SENSOR_ADDR, 0x01, 0xAA);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMBus write byte: 0xAA to reg 0x01");
  }

  /* SMBus read word (16-bit) */
  uint16_t smbus_word;
  ret = star_smbus_read_word(&manager, "temp_sensor", SENSOR_ADDR, 0x00,
                             &smbus_word);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMBus read word: 0x%04X", smbus_word);
  }

  /* SMBus write word (16-bit) */
  ret = star_smbus_write_word(&manager, "temp_sensor", SENSOR_ADDR, 0x02,
                              0x1234);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMBus write word: 0x1234 to reg 0x02");
  }

  /* Step 10: Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "I2C example complete - all resources freed");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Basic Example ===\n");
  i2c_basic_example();
}
