/**
 * @file 201_robot_controller.c
 * @brief 4WD robot with sensors and motor control
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_hcsr04.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "ROBOT_CTRL";

#define MOTOR_FL (0)
#define MOTOR_FR (1)
#define MOTOR_RL (2)
#define MOTOR_RR (3)

typedef struct {
  pca9685_handle_t* pwm;
  float speed_scale;
} motor_controller_t;

static void priv_set_motor_speed(motor_controller_t *mc, int motor, float speed)
{
  int pwm_ch = motor * 2;
  if (speed >= 0) {
    star_sensor_pca9685_set_pwm(mc->pwm, pwm_ch, 0, (int)(speed * mc->speed_scale * 4095));
    star_sensor_pca9685_set_pwm(mc->pwm, pwm_ch + 1, 0, 0);
  } else {
    star_sensor_pca9685_set_pwm(mc->pwm, pwm_ch, 0, 0);
    star_sensor_pca9685_set_pwm(mc->pwm, pwm_ch + 1, 0, (int)(-speed * mc->speed_scale * 4095));
  }
}

static void priv_drive(motor_controller_t *mc, float linear, float angular)
{
  float left = linear - angular;
  float right = linear + angular;
  if (left > 1.0f) left = 1.0f;
  if (left < -1.0f) left = -1.0f;
  if (right > 1.0f) right = 1.0f;
  if (right < -1.0f) right = -1.0f;

  priv_set_motor_speed(mc, MOTOR_FL, left);
  priv_set_motor_speed(mc, MOTOR_RL, left);
  priv_set_motor_speed(mc, MOTOR_FR, right);
  priv_set_motor_speed(mc, MOTOR_RR, right);
}

void robot_controller_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Robot_Manager", NULL, NULL);

  star_bus_config_t* i2c_pwm = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, 0x40, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_config_t* i2c_imu = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c_pwm);
  star_bus_manager_add_bus(&manager, i2c_imu);
  star_bus_config_init(i2c_pwm, &manager);
  star_bus_config_init(i2c_imu, &manager);

  pca9685_handle_t pwm;
  mpu6050_handle_t imu;
  hcsr04_handle_t us;

  pca9685_config_t pwm_cfg = {
    .i2c_addr = 0x40,
    .pwm_freq = 1000,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };
  mpu6050_config_t imu_cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  hcsr04_config_t us_cfg = {
    .trigger_pin = GPIO_NUM_5,
    .echo_pin = GPIO_NUM_18,
    .temperature_c = 20.0f
  };

  star_sensor_pca9685_init(&pwm, &manager, "pca9685", &pwm_cfg);
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);
  star_sensor_hcsr04_init(&us, &us_cfg);

  motor_controller_t mc = { .pwm = &pwm, .speed_scale = 0.8f };

  ESP_LOGI(s_tag, "Robot initialized - starting autonomous mode");

  for (int i = 0; i < 500; i++) {
    float distance_cm;
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    float temp;

    star_sensor_hcsr04_read_distance(&us, &distance_cm);
    star_sensor_mpu6050_read_all(&imu, &accel, &gyro, &temp);

    float heading_rate = gyro.z_dps;

    if (distance_cm < 30) {
      /* Obstacle - turn */
      ESP_LOGI(s_tag, "Obstacle at %.1f cm - turning", distance_cm);
      priv_drive(&mc, 0, 0.5f);
    } else if (distance_cm < 60) {
      /* Slow down */
      priv_drive(&mc, 0.3f, 0);
    } else {
      /* Full speed ahead */
      priv_drive(&mc, 0.7f, -heading_rate * 0.01f);  /* Heading correction */
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  priv_drive(&mc, 0, 0);
  star_sensor_pca9685_deinit(&pwm);
  star_sensor_mpu6050_deinit(&imu);
  star_sensor_hcsr04_deinit(&us);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { robot_controller_example(); }
