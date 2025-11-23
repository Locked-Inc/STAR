/**
 * @file 222_stepper_control.c
 * @brief Stepper motor with acceleration control
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "STEPPER";

#define STEP_PIN  (GPIO_NUM_25)
#define DIR_PIN   (GPIO_NUM_26)
#define EN_PIN    (GPIO_NUM_27)

#define STEPS_PER_REV (200)
#define MICROSTEPS (16)

static void priv_step_pulse(void)
{
  gpio_set_level(STEP_PIN, 1);
  esp_rom_delay_us(2);
  gpio_set_level(STEP_PIN, 0);
}

static void priv_move_steps(int steps, float max_speed, float accel)
{
  if (steps == 0) return;

  gpio_set_level(DIR_PIN, steps > 0 ? 1 : 0);
  steps = abs(steps);

  float current_speed = 100;  /* Start speed */
  int accel_steps = (max_speed * max_speed - current_speed * current_speed) / (2 * accel);
  if (accel_steps > steps / 2) accel_steps = steps / 2;

  for (int i = 0; i < steps; i++) {
    priv_step_pulse();

    /* Acceleration phase */
    if (i < accel_steps) {
      current_speed = sqrtf(100 * 100 + 2 * accel * i);
    }
    /* Deceleration phase */
    else if (i >= steps - accel_steps) {
      current_speed = sqrtf(100 * 100 + 2 * accel * (steps - i - 1));
    }
    /* Constant speed */
    else {
      current_speed = max_speed;
    }

    int delay_us = (int)(1000000.0f / current_speed);
    esp_rom_delay_us(delay_us);
  }
}

void stepper_control_example(void)
{
  gpio_set_direction(STEP_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(DIR_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(EN_PIN, GPIO_MODE_OUTPUT);

  gpio_set_level(EN_PIN, 0);  /* Enable driver */

  ESP_LOGI(s_tag, "Stepper motor control started");

  /* Full rotation forward */
  ESP_LOGI(s_tag, "Rotating 360 degrees forward");
  priv_move_steps(STEPS_PER_REV * MICROSTEPS, 3000, 5000);
  vTaskDelay(pdMS_TO_TICKS(500));

  /* Full rotation backward */
  ESP_LOGI(s_tag, "Rotating 360 degrees backward");
  priv_move_steps(-STEPS_PER_REV * MICROSTEPS, 3000, 5000);
  vTaskDelay(pdMS_TO_TICKS(500));

  /* Multiple small moves */
  for (int i = 0; i < 8; i++) {
    ESP_LOGI(s_tag, "Moving 45 degrees");
    priv_move_steps(STEPS_PER_REV * MICROSTEPS / 8, 2000, 4000);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  gpio_set_level(EN_PIN, 1);  /* Disable driver */
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { stepper_control_example(); }
