/**
 * @file 202_drone_stabilizer.c
 * @brief Quadcopter PID stabilization
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "DRONE_STAB";

typedef struct {
  float kp, ki, kd;
  float integral;
  float prev_error;
  float output_min, output_max;
} pid_t;

static float priv_pid_update(pid_t *pid, float setpoint, float measured, float dt)
{
  float error = setpoint - measured;
  pid->integral += error * dt;
  float derivative = (error - pid->prev_error) / dt;
  pid->prev_error = error;

  float output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;

  if (output > pid->output_max) output = pid->output_max;
  if (output < pid->output_min) output = pid->output_min;

  return output;
}

void drone_stabilizer_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Drone_Manager", NULL, NULL);

  star_bus_config_t *i2c_pwm = star_bus_config_create_i2c(
    "pca9685", I2C_NUM_0, 0x40, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_config_t *i2c_imu = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c_pwm);
  star_bus_manager_add_bus(&manager, i2c_imu);
  star_bus_config_init(i2c_pwm, &manager);
  star_bus_config_init(i2c_imu, &manager);

  pca9685_handle_t pwm;
  mpu6050_handle_t imu;

  pca9685_config_t pwm_cfg = {
    .i2c_addr = 0x40,
    .pwm_freq = 400,
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

  star_sensor_pca9685_init(&pwm, &manager, "pca9685", &pwm_cfg);
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);

  /* PID controllers for roll, pitch, yaw */
  pid_t roll_pid = { .kp = 1.5f, .ki = 0.01f, .kd = 0.3f, .output_min = -500, .output_max = 500 };
  pid_t pitch_pid = { .kp = 1.5f, .ki = 0.01f, .kd = 0.3f, .output_min = -500, .output_max = 500 };
  pid_t yaw_pid = { .kp = 2.0f, .ki = 0.0f, .kd = 0.1f, .output_min = -300, .output_max = 300 };

  float roll = 0, pitch = 0, yaw = 0;
  float dt = 0.004f;  /* 250 Hz */
  float base_throttle = 1500;

  ESP_LOGI(s_tag, "Drone stabilizer running at 250Hz");

  for (int i = 0; i < 5000; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    float temp;
    star_sensor_mpu6050_read_all(&imu, &accel, &gyro, &temp);

    /* Complementary filter for angles */
    float accel_roll = atan2f(accel.y_g, accel.z_g) * 180.0f / M_PI;
    float accel_pitch = atan2f(-accel.x_g, sqrtf(accel.y_g * accel.y_g + accel.z_g * accel.z_g)) * 180.0f / M_PI;

    roll = 0.98f * (roll + gyro.x_dps * dt) + 0.02f * accel_roll;
    pitch = 0.98f * (pitch + gyro.y_dps * dt) + 0.02f * accel_pitch;
    yaw += gyro.z_dps * dt;

    /* PID control */
    float roll_out = priv_pid_update(&roll_pid, 0, roll, dt);
    float pitch_out = priv_pid_update(&pitch_pid, 0, pitch, dt);
    float yaw_out = priv_pid_update(&yaw_pid, 0, gyro.z_dps, dt);

    /* Motor mixing */
    float m1 = base_throttle + roll_out + pitch_out - yaw_out;  /* Front-left */
    float m2 = base_throttle - roll_out + pitch_out + yaw_out;  /* Front-right */
    float m3 = base_throttle + roll_out - pitch_out + yaw_out;  /* Rear-left */
    float m4 = base_throttle - roll_out - pitch_out - yaw_out;  /* Rear-right */

    /* Output to ESCs (1000-2000us pulse via PCA9685) */
    star_sensor_pca9685_set_pwm(&pwm, 0, 0, (int)(m1 * 4096 / 20000));
    star_sensor_pca9685_set_pwm(&pwm, 1, 0, (int)(m2 * 4096 / 20000));
    star_sensor_pca9685_set_pwm(&pwm, 2, 0, (int)(m3 * 4096 / 20000));
    star_sensor_pca9685_set_pwm(&pwm, 3, 0, (int)(m4 * 4096 / 20000));

    if (i % 250 == 0) {
      ESP_LOGI(s_tag, "Roll:%.1f Pitch:%.1f Yaw:%.1f M:[%.0f,%.0f,%.0f,%.0f]",
               roll, pitch, yaw, m1, m2, m3, m4);
    }

    vTaskDelay(pdMS_TO_TICKS(4));
  }

  /* Stop motors */
  for (int m = 0; m < 4; m++) {
    star_sensor_pca9685_set_pwm(&pwm, m, 0, 0);
  }

  star_sensor_pca9685_deinit(&pwm);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { drone_stabilizer_example(); }
