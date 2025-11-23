/**
 * @file 212_state_machine.c
 * @brief Finite state machine for sensor-based system
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_hcsr04.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "STATE_MACHINE";

typedef enum {
  STATE_IDLE,
  STATE_DETECTING,
  STATE_TRACKING,
  STATE_ALERT,
  STATE_COUNT
} system_state_t;

typedef enum {
  EVENT_NONE,
  EVENT_MOTION_START,
  EVENT_MOTION_STOP,
  EVENT_OBJECT_NEAR,
  EVENT_OBJECT_FAR,
  EVENT_TIMEOUT
} system_event_t;

static const char *state_names[] = {"IDLE", "DETECTING", "TRACKING", "ALERT"};

static system_state_t current_state = STATE_IDLE;
static uint32_t state_enter_time = 0;

static system_state_t transition_table[STATE_COUNT][6] = {
  /* IDLE */      {STATE_IDLE, STATE_DETECTING, STATE_IDLE, STATE_IDLE, STATE_IDLE, STATE_IDLE},
  /* DETECTING */ {STATE_DETECTING, STATE_DETECTING, STATE_IDLE, STATE_TRACKING, STATE_DETECTING, STATE_IDLE},
  /* TRACKING */  {STATE_TRACKING, STATE_TRACKING, STATE_IDLE, STATE_ALERT, STATE_DETECTING, STATE_DETECTING},
  /* ALERT */     {STATE_ALERT, STATE_ALERT, STATE_TRACKING, STATE_ALERT, STATE_TRACKING, STATE_TRACKING}
};

static void priv_enter_state(system_state_t new_state)
{
  if (new_state != current_state) {
    ESP_LOGI(s_tag, "State transition: %s -> %s", state_names[current_state], state_names[new_state]);
    current_state = new_state;
    state_enter_time = xTaskGetTickCount();
  }
}

static system_event_t priv_get_event(float distance, float accel_mag)
{
  static float prev_mag = 1.0f;
  system_event_t event = EVENT_NONE;

  if (fabsf(accel_mag - prev_mag) > 0.3f && accel_mag > 1.2f) {
    event = EVENT_MOTION_START;
  } else if (fabsf(accel_mag - 1.0f) < 0.1f && prev_mag > 1.2f) {
    event = EVENT_MOTION_STOP;
  } else if (distance < 50 && distance > 0) {
    event = EVENT_OBJECT_NEAR;
  } else if (distance > 100) {
    event = EVENT_OBJECT_FAR;
  } else if (xTaskGetTickCount() - state_enter_time > pdMS_TO_TICKS(10000)) {
    event = EVENT_TIMEOUT;
  }

  prev_mag = accel_mag;
  return event;
}

void state_machine_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "FSM_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  hcsr04_handle_t us;

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

  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);
  star_sensor_hcsr04_init(&us, &us_cfg);

  priv_enter_state(STATE_IDLE);

  for (int i = 0; i < 500; i++) {
    mpu6050_accel_t accel;
    float distance;

    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_hcsr04_read_distance(&us, &distance);

    float accel_mag = sqrtf(accel.x_g * accel.x_g +
                           accel.y_g * accel.y_g +
                           accel.z_g * accel.z_g);

    system_event_t event = priv_get_event(distance, accel_mag);
    if (event != EVENT_NONE) {
      system_state_t next = transition_table[current_state][event];
      priv_enter_state(next);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_sensor_hcsr04_deinit(&us);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { state_machine_example(); }
