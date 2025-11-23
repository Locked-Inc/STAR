/**
 * @file 294_pulse_oximeter.c
 * @brief MAX30102 pulse oximeter and heart rate
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PULSE";

#define MAX30102_ADDR (0x57)

static star_bus_config_t *pulse_bus;
static star_bus_manager_t *pulse_manager;

static void priv_max_write(uint8_t reg, uint8_t val) {
  uint8_t buf[1] = {val};
  size_t bytes_written;
  star_bus_i2c_write(pulse_manager, "max30102", buf, 1, reg, &bytes_written);
}

static uint8_t priv_max_read(uint8_t reg) {
  uint8_t val;
  size_t bytes_read;
  star_bus_i2c_read(pulse_manager, "max30102", &val, 1, reg, &bytes_read);
  return val;
}

void pulse_oximeter_example(void)
{
  static star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Pulse_Manager", NULL, NULL);
  pulse_manager = &manager;

  pulse_bus = star_bus_config_create_i2c(
    "max30102", I2C_NUM_0, MAX30102_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, pulse_bus);
  star_bus_config_init(pulse_bus, &manager);

  /* Reset */
  priv_max_write(0x09, 0x40);
  vTaskDelay(pdMS_TO_TICKS(100));

  /* Configure */
  priv_max_write(0x02, 0xC0);  /* Interrupts */
  priv_max_write(0x03, 0x00);
  priv_max_write(0x04, 0x00);  /* FIFO config */
  priv_max_write(0x09, 0x03);  /* SpO2 mode */
  priv_max_write(0x0A, 0x27);  /* SpO2 config */
  priv_max_write(0x0C, 0x24);  /* LED1 pulse */
  priv_max_write(0x0D, 0x24);  /* LED2 pulse */

  ESP_LOGI(s_tag, "MAX30102 pulse oximeter started");
  ESP_LOGI(s_tag, "Place finger on sensor...");

  for (int i = 0; i < 200; i++) {
    uint8_t fifo[6];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "max30102", fifo, 6, 0x07, &bytes_read);

    uint32_t red = ((fifo[0] << 16) | (fifo[1] << 8) | fifo[2]) & 0x3FFFF;
    uint32_t ir = ((fifo[3] << 16) | (fifo[4] << 8) | fifo[5]) & 0x3FFFF;

    if (ir > 50000) {
      float ratio = (float)red / (float)ir;
      int spo2 = (int)(110.0f - 25.0f * ratio);
      if (spo2 > 100) spo2 = 100;
      if (spo2 < 0) spo2 = 0;

      ESP_LOGI(s_tag, "Red:%lu IR:%lu SpO2:~%d%%", red, ir, spo2);
    } else {
      ESP_LOGI(s_tag, "No finger detected");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { pulse_oximeter_example(); }
