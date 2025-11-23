/**
 * @file 161_pca9685_single.c
 * @brief Single PCA9685 PWM controller (16 channels)
 *
 * Demonstrates:
 * - Basic single PCA9685 initialization
 * - All 16 channel control
 * - LED dimming and servo control
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PCA9685_SINGLE";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define PCA9685_ADDR (0x40) /* Default address (A0-A5 all low) */

void pca9685_single_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "PWM_Single", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, PCA9685_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  pca9685_handle_t pwm;
  pca9685_config_t cfg = {
    .i2c_addr = PCA9685_ADDR,
    .pwm_freq = 1000,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };

  if (star_sensor_pca9685_init(&pwm, &manager, "pca9685", &cfg) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init PCA9685");
    return;
  }

  /* Set to 1kHz for LED dimming */
  star_sensor_pca9685_set_frequency(&pwm, 1000);
  ESP_LOGI(s_tag, "Single PCA9685 ready - 16 channels available");

  /* Cycle through all 16 channels */
  for (int cycle = 0; cycle < 3; cycle++) {
    for (uint8_t ch = 0; ch < 16; ch++) {
      /* Fade in */
      for (uint16_t duty = 0; duty <= 4095; duty += 256) {
        star_sensor_pca9685_set_pwm(&pwm, ch, 0, duty);
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      /* Fade out */
      for (uint16_t duty = 4095; duty > 0; duty -= 256) {
        star_sensor_pca9685_set_pwm(&pwm, ch, 0, duty);
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      star_sensor_pca9685_set_pwm(&pwm, ch, 0, 0);
      ESP_LOGI(s_tag, "Channel %d complete", ch);
    }
  }

  star_sensor_pca9685_deinit(&pwm);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { pca9685_single_example(); }
