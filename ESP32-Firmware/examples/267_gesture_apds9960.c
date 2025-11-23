/**
 * @file 267_gesture_apds9960.c
 * @brief APDS9960 gesture/proximity/color sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "GESTURE";

#define APDS9960_ADDR (0x39)

static star_bus_config_t *apds_bus;
static star_bus_manager_t *apds_manager;

static void priv_apds_write_reg(uint8_t reg, uint8_t val) {
  uint8_t buf[1] = {val};
  size_t bytes_written;
  star_bus_i2c_write(apds_manager, "apds9960", buf, 1, reg, &bytes_written);
}

static uint8_t priv_apds_read_reg(uint8_t reg) {
  uint8_t val;
  size_t bytes_read;
  star_bus_i2c_read(apds_manager, "apds9960", &val, 1, reg, &bytes_read);
  return val;
}

void gesture_apds9960_example(void)
{
  static star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Gesture_Manager", NULL, NULL);
  apds_manager = &manager;

  apds_bus = star_bus_config_create_i2c(
    "apds9960", I2C_NUM_0, APDS9960_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, apds_bus);
  star_bus_config_init(apds_bus, &manager);

  /* Initialize */
  priv_apds_write_reg(0x80, 0x00);  /* Power off */
  vTaskDelay(pdMS_TO_TICKS(10));
  priv_apds_write_reg(0x80, 0x45);  /* Power on, proximity, gesture enable */
  priv_apds_write_reg(0xA3, 0x40);  /* Gesture proximity threshold */

  ESP_LOGI(s_tag, "APDS9960 gesture sensor started");

  for (int i = 0; i < 200; i++) {
    uint8_t status = priv_apds_read_reg(0x93);
    uint8_t prox = priv_apds_read_reg(0x9C);

    if (status & 0x04) {  /* Gesture available */
      uint8_t gstatus = priv_apds_read_reg(0xAF);
      if (gstatus & 0x01) {
        uint8_t fifo[4];
        size_t bytes_read;
        star_bus_i2c_read(&manager, "apds9960", fifo, 4, 0xFC, &bytes_read);
        ESP_LOGI(s_tag, "Gesture: U=%d D=%d L=%d R=%d",
                 fifo[0], fifo[1], fifo[2], fifo[3]);
      }
    }

    if (prox > 20) {
      ESP_LOGI(s_tag, "Proximity: %d", prox);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { gesture_apds9960_example(); }
