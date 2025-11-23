/**
 * @file 203_robotic_arm.c
 * @brief 6-DOF robotic arm control with inverse kinematics
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "ROBOTIC_ARM";

#define SERVO_BASE (0)
#define SERVO_SHOULDER (1)
#define SERVO_ELBOW (2)
#define SERVO_WRIST_P (3)
#define SERVO_WRIST_R (4)
#define SERVO_GRIPPER (5)

#define ARM_L1  100.0f  /* Base to shoulder (mm) */
#define ARM_L2  100.0f  /* Shoulder to elbow */
#define ARM_L3  80.0f   /* Elbow to wrist */

typedef struct {
  float angles[6];  /* Joint angles in degrees */
} arm_state_t;

static int priv_angle_to_pwm(float angle)
{
  /* Convert angle (0-180) to PCA9685 PWM (150-600 for typical servo) */
  return 150 + (int)(angle * 450.0f / 180.0f);
}

static void priv_set_arm_position(pca9685_handle_t *pwm, arm_state_t *state)
{
  for (int i = 0; i < 6; i++) {
    star_sensor_pca9685_set_pwm(pwm, i, 0, priv_angle_to_pwm(state->angles[i]));
  }
}

static void priv_smooth_move(pca9685_handle_t *pwm, arm_state_t *current, arm_state_t *target, int steps)
{
  float deltas[6];
  for (int i = 0; i < 6; i++) {
    deltas[i] = (target->angles[i] - current->angles[i]) / steps;
  }

  for (int s = 0; s < steps; s++) {
    for (int i = 0; i < 6; i++) {
      current->angles[i] += deltas[i];
    }
    priv_set_arm_position(pwm, current);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/* Simple 2D inverse kinematics for planar arm */
static bool priv_inverse_kinematics_2d(float x, float z, float *theta1, float *theta2)
{
  float r = sqrtf(x * x + z * z);
  if (r > ARM_L2 + ARM_L3) return false;

  float cos_theta2 = (r * r - ARM_L2 * ARM_L2 - ARM_L3 * ARM_L3) / (2 * ARM_L2 * ARM_L3);
  if (cos_theta2 < -1 || cos_theta2 > 1) return false;

  *theta2 = acosf(cos_theta2);
  float k1 = ARM_L2 + ARM_L3 * cosf(*theta2);
  float k2 = ARM_L3 * sinf(*theta2);
  *theta1 = atan2f(z, x) - atan2f(k2, k1);

  *theta1 *= 180.0f / M_PI;
  *theta2 *= 180.0f / M_PI;

  return true;
}

void robotic_arm_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Arm_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, 0x40, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  pca9685_handle_t pwm;
  pca9685_config_t cfg = {
    .i2c_addr = 0x40,
    .pwm_freq = 50,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };
  star_sensor_pca9685_init(&pwm, &manager, "pca9685", &cfg);

  arm_state_t state = { .angles = {90, 90, 90, 90, 90, 90} };
  priv_set_arm_position(&pwm, &state);

  ESP_LOGI(s_tag, "Robotic arm initialized - home position");
  vTaskDelay(pdMS_TO_TICKS(1000));

  /* Demo: Pick and place sequence */
  arm_state_t pick_approach = { .angles = {45, 60, 120, 90, 90, 30} };
  arm_state_t pick_down = { .angles = {45, 45, 135, 90, 90, 30} };
  arm_state_t pick_grip = { .angles = {45, 45, 135, 90, 90, 90} };
  arm_state_t pick_up = { .angles = {45, 60, 120, 90, 90, 90} };
  arm_state_t place_approach = { .angles = {135, 60, 120, 90, 90, 90} };
  arm_state_t place_down = { .angles = {135, 45, 135, 90, 90, 90} };
  arm_state_t place_release = { .angles = {135, 45, 135, 90, 90, 30} };

  ESP_LOGI(s_tag, "Pick and place demo starting...");

  priv_smooth_move(&pwm, &state, &pick_approach, 50);
  ESP_LOGI(s_tag, "Approaching pick position");

  priv_smooth_move(&pwm, &state, &pick_down, 30);
  ESP_LOGI(s_tag, "Moving down to object");

  priv_smooth_move(&pwm, &state, &pick_grip, 20);
  ESP_LOGI(s_tag, "Gripping object");

  priv_smooth_move(&pwm, &state, &pick_up, 30);
  ESP_LOGI(s_tag, "Lifting object");

  priv_smooth_move(&pwm, &state, &place_approach, 50);
  ESP_LOGI(s_tag, "Moving to place position");

  priv_smooth_move(&pwm, &state, &place_down, 30);
  ESP_LOGI(s_tag, "Lowering object");

  priv_smooth_move(&pwm, &state, &place_release, 20);
  ESP_LOGI(s_tag, "Releasing object");

  /* Return home */
  arm_state_t home = { .angles = {90, 90, 90, 90, 90, 90} };
  priv_smooth_move(&pwm, &state, &home, 50);

  ESP_LOGI(s_tag, "Sequence complete");

  star_sensor_pca9685_deinit(&pwm);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { robotic_arm_example(); }
