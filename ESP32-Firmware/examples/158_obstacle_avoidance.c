/**
 * @file 158_obstacle_avoidance.c
 * @brief Obstacle detection and avoidance example
 *
 * Demonstrates:
 * - Multiple HC-SR04 sensors for 180-degree coverage
 * - Distance-based movement decisions
 * - Servo-based steering with PCA9685
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_hcsr04.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "OBSTACLE_AVOID";

/* Ultrasonic sensor pins */
#define LEFT_TRIGGER    (GPIO_NUM_25)
#define LEFT_ECHO       (GPIO_NUM_26)
#define CENTER_TRIGGER  (GPIO_NUM_27)
#define CENTER_ECHO     (GPIO_NUM_14)
#define RIGHT_TRIGGER   (GPIO_NUM_12)
#define RIGHT_ECHO      (GPIO_NUM_13)

/* I2C for PCA9685 */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Steering servo channel */
#define STEERING_CH (0)
#define MOTOR_CH (1)

typedef enum {
  DIR_FORWARD,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_STOP,
  DIR_REVERSE
} direction_t;

static direction_t decide_direction(float left, float center, float right)
{
  const float DANGER_DIST = 30.0f;   /* cm */
  const float CAUTION_DIST = 60.0f;

  /* Emergency stop if center is too close */
  if (center < DANGER_DIST) {
    if (left > right && left > CAUTION_DIST) return DIR_LEFT;
    if (right > left && right > CAUTION_DIST) return DIR_RIGHT;
    return DIR_REVERSE;
  }

  /* All clear */
  if (left > CAUTION_DIST && center > CAUTION_DIST && right > CAUTION_DIST) {
    return DIR_FORWARD;
  }

  /* Turn away from obstacles */
  if (left < CAUTION_DIST && right > CAUTION_DIST) return DIR_RIGHT;
  if (right < CAUTION_DIST && left > CAUTION_DIST) return DIR_LEFT;

  /* Default to following the clearest path */
  if (left > right) return DIR_LEFT;
  return DIR_RIGHT;
}

void obstacle_avoidance_example(void)
{
  /* Initialize sensors */
  hcsr04_handle_t left_us, center_us, right_us;
  hcsr04_config_t left_cfg = { .trigger_pin = LEFT_TRIGGER, .echo_pin = LEFT_ECHO, .temperature_c = 20.0f };
  hcsr04_config_t center_cfg = { .trigger_pin = CENTER_TRIGGER, .echo_pin = CENTER_ECHO, .temperature_c = 20.0f };
  hcsr04_config_t right_cfg = { .trigger_pin = RIGHT_TRIGGER, .echo_pin = RIGHT_ECHO, .temperature_c = 20.0f };

  star_sensor_hcsr04_init(&left_us, &left_cfg);
  star_sensor_hcsr04_init(&center_us, &center_cfg);
  star_sensor_hcsr04_init(&right_us, &right_cfg);

  /* Initialize PCA9685 for servo control */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Motor_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, 0x40, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  pca9685_handle_t pwm;
  pca9685_config_t pwm_cfg = {
    .i2c_addr = 0x40,
    .pwm_freq = 50,  /* 50Hz for servos */
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };
  star_sensor_pca9685_init(&pwm, &manager, "pca9685", &pwm_cfg);

  ESP_LOGI(s_tag, "Obstacle avoidance system initialized");

  /* Main control loop */
  for (int i = 0; i < 200; i++) {
    float left_dist, center_dist, right_dist;

    /* Read all sensors */
    star_sensor_hcsr04_read_distance(&left_us, &left_dist);
    vTaskDelay(pdMS_TO_TICKS(30));
    star_sensor_hcsr04_read_distance(&center_us, &center_dist);
    vTaskDelay(pdMS_TO_TICKS(30));
    star_sensor_hcsr04_read_distance(&right_us, &right_dist);

    direction_t dir = decide_direction(left_dist, center_dist, right_dist);

    const char *dir_str;
    uint16_t steering_pwm = 307;  /* Center (1.5ms at 50Hz) */

    switch (dir) {
      case DIR_FORWARD:
        dir_str = "FORWARD";
        steering_pwm = 307;
        break;
      case DIR_LEFT:
        dir_str = "LEFT";
        steering_pwm = 205;  /* 1.0ms */
        break;
      case DIR_RIGHT:
        dir_str = "RIGHT";
        steering_pwm = 410;  /* 2.0ms */
        break;
      case DIR_REVERSE:
        dir_str = "REVERSE";
        steering_pwm = 307;
        break;
      case DIR_STOP:
      default:
        dir_str = "STOP";
        break;
    }

    star_sensor_pca9685_set_pwm(&pwm, STEERING_CH, 0, steering_pwm);

    ESP_LOGI(s_tag, "L=%.0f C=%.0f R=%.0f -> %s",
             left_dist, center_dist, right_dist, dir_str);

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_sensor_hcsr04_deinit(&left_us);
  star_sensor_hcsr04_deinit(&center_us);
  star_sensor_hcsr04_deinit(&right_us);
  star_sensor_pca9685_deinit(&pwm);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { obstacle_avoidance_example(); }
