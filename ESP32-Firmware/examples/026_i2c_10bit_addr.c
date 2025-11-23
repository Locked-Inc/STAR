/**
 * @file 026_i2c_10bit_addr.c
 * @brief I2C 10-bit addressing mode example
 *
 * This example demonstrates:
 * - Configuring I2C for 10-bit addressing mode
 * - Reading from a 10-bit addressed device
 * - Writing to a 10-bit addressed device
 * - Differences between 7-bit and 10-bit addressing
 * - Practical use with EEPROM or other 10-bit devices
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_10BIT";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* 10-bit device address - example: 0x2A4 (valid range 0x000-0x3FF) */
#define DEVICE_10BIT_ADDR (0x2A4)

/* Clock speed */
#define I2C_CLK_SPEED    (100000)  /* 100 kHz Standard Mode */

static void priv_i2c_10bit_addressing_example(void)
{
  esp_err_t ret;

  /* Step 1: Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_10bit", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Step 2: Create I2C bus configuration with 10-bit address */
  ESP_LOGI(s_tag, "Creating I2C config with 10-bit address: 0x%03X", DEVICE_10BIT_ADDR);

  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "eeprom_10bit",   /* Unique name for this device */
    I2C_NUM_0,        /* I2C port */
    DEVICE_10BIT_ADDR,/* 10-bit device address (0x000-0x3FF) */
    I2C_SDA_PIN,      /* SDA GPIO */
    I2C_SCL_PIN,      /* SCL GPIO */
    I2C_CLK_SPEED     /* Clock speed in Hz */
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "I2C config created for 10-bit address 0x%03X", DEVICE_10BIT_ADDR);

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
  ESP_LOGI(s_tag, "I2C driver initialized with 10-bit addressing mode");

  /* Step 5: Write to 10-bit addressed device */
  ESP_LOGI(s_tag, "\n--- Writing to 10-bit Device ---");
  uint8_t write_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  size_t bytes_written;

  ret = star_bus_i2c_write(&manager, "eeprom_10bit", write_data, sizeof(write_data),
                           0x00, &bytes_written);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Wrote %u bytes to 10-bit device at reg 0x00", (unsigned)bytes_written);
    ESP_LOGI(s_tag, "Data: 0x%02X 0x%02X 0x%02X 0x%02X",
             write_data[0], write_data[1], write_data[2], write_data[3]);
  } else {
    ESP_LOGW(s_tag, "Write failed: %s (device may not be present)", esp_err_to_name(ret));
  }

  /* Wait for EEPROM write cycle to complete */
  vTaskDelay(pdMS_TO_TICKS(10));

  /* Step 6: Read from 10-bit addressed device */
  ESP_LOGI(s_tag, "\n--- Reading from 10-bit Device ---");
  uint8_t read_data[4];
  size_t bytes_read;

  ret = star_bus_i2c_read(&manager, "eeprom_10bit", read_data, sizeof(read_data),
                          0x00, &bytes_read);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read %u bytes from 10-bit device at reg 0x00", (unsigned)bytes_read);
    ESP_LOGI(s_tag, "Data: 0x%02X 0x%02X 0x%02X 0x%02X",
             read_data[0], read_data[1], read_data[2], read_data[3]);

    /* Verify data */
    bool match = true;
    for (int i = 0; i < sizeof(read_data); i++) {
      if (read_data[i] != write_data[i]) {
        match = false;
        break;
      }
    }

    if (match) {
      ESP_LOGI(s_tag, "Read data matches written data - SUCCESS!");
    } else {
      ESP_LOGW(s_tag, "Read data does not match written data");
    }
  } else {
    ESP_LOGW(s_tag, "Read failed: %s", esp_err_to_name(ret));
  }

  /* Step 7: Demonstrate sequential writes to different registers */
  ESP_LOGI(s_tag, "\n--- Sequential Register Access ---");
  for (uint8_t reg = 0x00; reg < 0x04; reg++) {
    uint8_t value = 0xA0 + reg;
    ret = star_bus_i2c_write(&manager, "eeprom_10bit", &value, 1, reg, &bytes_written);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Wrote 0x%02X to register 0x%02X", value, reg);
    }
    vTaskDelay(pdMS_TO_TICKS(5));  /* EEPROM write cycle time */
  }

  /* Step 8: Read back sequential registers */
  ESP_LOGI(s_tag, "\n--- Reading Sequential Registers ---");
  for (uint8_t reg = 0x00; reg < 0x04; reg++) {
    uint8_t value;
    ret = star_bus_i2c_read(&manager, "eeprom_10bit", &value, 1, reg, &bytes_read);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Read 0x%02X from register 0x%02X", value, reg);
    }
  }

  /* Step 9: Information about 10-bit addressing */
  ESP_LOGI(s_tag, "\n--- 10-bit Addressing Information ---");
  ESP_LOGI(s_tag, "10-bit address format: 0x%03X (%d decimal)", DEVICE_10BIT_ADDR, DEVICE_10BIT_ADDR);
  ESP_LOGI(s_tag, "Valid 10-bit range: 0x000 to 0x3FF (0-1023)");
  ESP_LOGI(s_tag, "Address bits 9-8 encoded in: 11110XX0 (write) / 11110XX1 (read)");
  ESP_LOGI(s_tag, "Address bits 7-0 sent as second byte");
  ESP_LOGI(s_tag, "Uses 2-byte addressing vs 1-byte for 7-bit mode");

  /* Step 10: Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "I2C 10-bit addressing example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C 10-bit Addressing Example ===\n");
  priv_i2c_10bit_addressing_example();
}
