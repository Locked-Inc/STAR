/**
 * @file 221_encoder_motor.c
 * @brief Rotary encoder with DC motor position control
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/pcnt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ENCODER_MOTOR";

#define ENCODER_A_PIN (GPIO_NUM_34)
#define ENCODER_B_PIN (GPIO_NUM_35)
#define PCNT_UNIT     (PCNT_UNIT_0)

static volatile int32_t encoder_count = 0;

static void priv_encoder_init(void)
{
  pcnt_config_t pcnt_cfg = {
    .pulse_gpio_num = ENCODER_A_PIN,
    .ctrl_gpio_num = ENCODER_B_PIN,
    .lctrl_mode = PCNT_MODE_REVERSE,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_INC,
    .neg_mode = PCNT_COUNT_DEC,
    .counter_h_lim = 32767,
    .counter_l_lim = -32768,
    .unit = PCNT_UNIT,
    .channel = PCNT_CHANNEL_0,
  };

  pcnt_unit_config(&pcnt_cfg);
  pcnt_set_filter_value(PCNT_UNIT, 100);
  pcnt_filter_enable(PCNT_UNIT);
  pcnt_counter_pause(PCNT_UNIT);
  pcnt_counter_clear(PCNT_UNIT);
  pcnt_counter_resume(PCNT_UNIT);
}

static int32_t priv_encoder_get_count(void)
{
  int16_t count;
  pcnt_get_counter_value(PCNT_UNIT, &count);
  return count;
}

void encoder_motor_example(void)
{
  priv_encoder_init();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Motor_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, 0x40, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  pca9685_handle_t pwm;
  pca9685_config_t cfg = {
    .i2c_addr = 0x40,
    .pwm_freq = 1000,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };
  star_sensor_pca9685_init(&pwm, &manager, "pca9685", &cfg);

  ESP_LOGI(s_tag, "Encoder motor control started");

  /* PID controller */
  float kp = 0.5f, ki = 0.01f, kd = 0.1f;
  float integral = 0, prev_error = 0;
  int32_t target_position = 0;

  for (int i = 0; i < 500; i++) {
    /* Change target every 2 seconds */
    if (i % 200 == 0) {
      target_position = (i / 200) % 2 ? 1000 : 0;
      ESP_LOGI(s_tag, "New target: %ld", target_position);
    }

    int32_t position = priv_encoder_get_count();
    float error = target_position - position;
    integral += error;
    float derivative = error - prev_error;
    prev_error = error;

    float output = kp * error + ki * integral + kd * derivative;

    /* Limit output */
    if (output > 4095) output = 4095;
    if (output < -4095) output = -4095;

    /* Drive motor */
    if (output > 0) {
      star_sensor_pca9685_set_pwm(&pwm, 0, 0, (int)output);
      star_sensor_pca9685_set_pwm(&pwm, 1, 0, 0);
    } else {
      star_sensor_pca9685_set_pwm(&pwm, 0, 0, 0);
      star_sensor_pca9685_set_pwm(&pwm, 1, 0, (int)(-output));
    }

    if (i % 10 == 0) {
      ESP_LOGI(s_tag, "Pos: %ld, Target: %ld, Error: %.0f", position, target_position, error);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  star_sensor_pca9685_set_pwm(&pwm, 0, 0, 0);
  star_sensor_pca9685_set_pwm(&pwm, 1, 0, 0);
  star_sensor_pca9685_deinit(&pwm);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { encoder_motor_example(); }
