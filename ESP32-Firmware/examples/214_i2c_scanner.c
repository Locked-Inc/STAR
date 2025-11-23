/**
 * @file 214_i2c_scanner.c
 * @brief I2C bus scanner utility
 */

#include <esp_log.h>
#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_SCAN";

static const char *known_devices[] = {
  [0x08] = "BQ7850 BMS",
  [0x0D] = "QMC5883L",
  [0x10] = "IMX219",
  [0x1E] = "HMC5883L",
  [0x23] = "BH1750",
  [0x28] = "BNO055",
  [0x36] = "IMX219",
  [0x40] = "PCA9685",
  [0x48] = "ADS1115",
  [0x50] = "EEPROM",
  [0x68] = "MPU6050/DS1307",
  [0x76] = "BMP280",
  [0x77] = "BMP180/BME280",
};

void i2c_scanner_example(void)
{
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000
  };

  i2c_param_config(I2C_NUM_0, &conf);
  i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

  ESP_LOGI(s_tag, "Scanning I2C bus...\n");
  ESP_LOGI(s_tag, "     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

  int devices_found = 0;

  for (int row = 0; row < 8; row++) {
    printf("%02X: ", row * 16);
    for (int col = 0; col < 16; col++) {
      uint8_t addr = row * 16 + col;

      if (addr < 0x03 || addr > 0x77) {
        printf("   ");
        continue;
      }

      i2c_cmd_handle_t cmd = i2c_cmd_link_create();
      i2c_master_start(cmd);
      i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
      i2c_master_stop(cmd);

      esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
      i2c_cmd_link_delete(cmd);

      if (ret == ESP_OK) {
        printf("%02X ", addr);
        devices_found++;
      } else {
        printf("-- ");
      }
    }
    printf("\n");
  }

  ESP_LOGI(s_tag, "\nFound %d device(s)\n", devices_found);

  /* Identify known devices */
  for (int addr = 0x03; addr <= 0x77; addr++) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    if (i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50)) == ESP_OK) {
      const char *name = (addr < 128 && known_devices[addr]) ? known_devices[addr] : "Unknown";
      ESP_LOGI(s_tag, "0x%02X: %s", addr, name);
    }
    i2c_cmd_link_delete(cmd);
  }

  i2c_driver_delete(I2C_NUM_0);
}

void app_main(void) { i2c_scanner_example(); }
