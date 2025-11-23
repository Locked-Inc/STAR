/**
 * @file 155_pca9685_servo.c
 * @brief PCA9685 16-channel PWM servo control example
 *
 * Demonstrates:
 * - PWM frequency configuration for servos
 * - Multi-channel servo control
 * - Smooth servo movements
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PCA9685_SERVO";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define PCA9685_ADDR (0x40)

/* Servo PWM pulse widths (in microseconds) */
#define SERVO_MIN_US (500) /* 0 degrees */
#define SERVO_MAX_US (2500) /* 180 degrees */
#define SERVO_FREQ_HZ (50) /* 50Hz for servos */

static uint16_t angle_to_pwm(uint8_t angle)
{
  /* Convert angle (0-180) to PWM value (0-4095) */
  uint32_t pulse_us = SERVO_MIN_US + (angle * (SERVO_MAX_US - SERVO_MIN_US) / 180);
  return (uint16_t)((pulse_us * 4096) / (1000000 / SERVO_FREQ_HZ));
}

void pca9685_servo_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Servo_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, PCA9685_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize PCA9685 */
  pca9685_handle_t pwm;
  pca9685_config_t cfg = {
    .i2c_addr = PCA9685_ADDR,
    .pwm_freq = SERVO_FREQ_HZ,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };

  ret = star_sensor_pca9685_init(&pwm, &manager, "pca9685", &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init PCA9685");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Set servo frequency */
  star_sensor_pca9685_set_frequency(&pwm, SERVO_FREQ_HZ);
  ESP_LOGI(s_tag, "PCA9685 initialized at %d Hz", SERVO_FREQ_HZ);

  /* Sweep servos on channels 0-3 */
  for (int sweep = 0; sweep < 5; sweep++) {
    /* Sweep from 0 to 180 degrees */
    for (int angle = 0; angle <= 180; angle += 5) {
      for (int ch = 0; ch < 4; ch++) {
        uint16_t pwm_val = angle_to_pwm(angle);
        star_sensor_pca9685_set_pwm(&pwm, ch, 0, pwm_val);
      }
      ESP_LOGI(s_tag, "Angle: %d deg", angle);
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* Sweep back from 180 to 0 degrees */
    for (int angle = 180; angle >= 0; angle -= 5) {
      for (int ch = 0; ch < 4; ch++) {
        uint16_t pwm_val = angle_to_pwm(angle);
        star_sensor_pca9685_set_pwm(&pwm, ch, 0, pwm_val);
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }

  star_sensor_pca9685_deinit(&pwm);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { pca9685_servo_example(); }
