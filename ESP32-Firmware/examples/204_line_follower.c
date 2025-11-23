/**
 * @file 204_line_follower.c
 * @brief Line following robot with IR sensors
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <driver/adc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "LINE_FOLLOWER";

#define IR_LEFT   (ADC1_CHANNEL_0)
#define IR_CENTER (ADC1_CHANNEL_3)
#define IR_RIGHT  (ADC1_CHANNEL_6)
#define THRESHOLD (2000)

static void priv_set_motors(pca9685_handle_t *pwm, int left, int right)
{
  star_sensor_pca9685_set_pwm(pwm, 0, 0, left > 0 ? left : 0);
  star_sensor_pca9685_set_pwm(pwm, 1, 0, left < 0 ? -left : 0);
  star_sensor_pca9685_set_pwm(pwm, 2, 0, right > 0 ? right : 0);
  star_sensor_pca9685_set_pwm(pwm, 3, 0, right < 0 ? -right : 0);
}

void line_follower_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(IR_LEFT, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(IR_CENTER, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(IR_RIGHT, ADC_ATTEN_DB_11);

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Line_Manager", NULL, NULL);

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

  ESP_LOGI(s_tag, "Line follower started");

  for (int i = 0; i < 1000; i++) {
    int left_val = adc1_get_raw(IR_LEFT);
    int center_val = adc1_get_raw(IR_CENTER);
    int right_val = adc1_get_raw(IR_RIGHT);

    bool left_on = left_val > THRESHOLD;
    bool center_on = center_val > THRESHOLD;
    bool right_on = right_val > THRESHOLD;

    int left_speed = 0, right_speed = 0;

    if (center_on && !left_on && !right_on) {
      left_speed = 3000; right_speed = 3000;  /* Straight */
    } else if (left_on && !right_on) {
      left_speed = 1500; right_speed = 3000;  /* Turn left */
    } else if (right_on && !left_on) {
      left_speed = 3000; right_speed = 1500;  /* Turn right */
    } else if (left_on && center_on) {
      left_speed = 1000; right_speed = 3000;  /* Sharp left */
    } else if (right_on && center_on) {
      left_speed = 3000; right_speed = 1000;  /* Sharp right */
    } else if (!left_on && !center_on && !right_on) {
      left_speed = 2000; right_speed = 2000;  /* Lost line - slow search */
    }

    priv_set_motors(&pwm, left_speed, right_speed);
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  priv_set_motors(&pwm, 0, 0);
  star_sensor_pca9685_deinit(&pwm);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { line_follower_example(); }
