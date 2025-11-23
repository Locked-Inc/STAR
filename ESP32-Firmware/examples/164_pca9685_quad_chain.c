/**
 * @file 164_pca9685_quad_chain.c
 * @brief Four PCA9685 controllers daisy-chained (64 channels)
 *
 * Demonstrates:
 * - Maximum practical daisy chain (64 channels)
 * - Servo army control
 * - Coordinated multi-limb robotics
 *
 * Hardware setup:
 * - PCA9685 #1: Address 0x40 (default)
 * - PCA9685 #2: Address 0x41 (A0 high)
 * - PCA9685 #3: Address 0x42 (A1 high)
 * - PCA9685 #4: Address 0x43 (A0 + A1 high)
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "PCA9685_QUAD";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

#define NUM_CONTROLLERS (4)
#define TOTAL_CHANNELS (64)
#define SERVO_FREQ_HZ (50)
#define SERVO_MIN_US (500)
#define SERVO_MAX_US (2500)

typedef struct {
  pca9685_handle_t handles[NUM_CONTROLLERS];
  star_bus_manager_t manager;
} servo_system_t;

static uint16_t angle_to_pwm(uint8_t angle)
{
  uint32_t pulse_us = SERVO_MIN_US + (angle * (SERVO_MAX_US - SERVO_MIN_US) / 180);
  return (uint16_t)((pulse_us * 4096) / (1000000 / SERVO_FREQ_HZ));
}

static void set_servo(servo_system_t* sys, uint8_t channel, uint8_t angle)
{
  if (channel >= TOTAL_CHANNELS) return;
  int ctrl_idx = channel / 16;
  int local_ch = channel % 16;
  star_sensor_pca9685_set_pwm(&sys->handles[ctrl_idx], local_ch, 0, angle_to_pwm(angle));
}

static void set_all_servos(servo_system_t* sys, uint8_t angle)
{
  uint16_t pwm = angle_to_pwm(angle);
  for (int c = 0; c < NUM_CONTROLLERS; c++) {
    for (uint8_t ch = 0; ch < 16; ch++) {
      star_sensor_pca9685_set_pwm(&sys->handles[c], ch, 0, pwm);
    }
  }
}

void pca9685_quad_chain_example(void)
{
  servo_system_t sys = {0};
  star_bus_manager_init(&sys.manager, "Servo_Quad", NULL, NULL);

  uint8_t addresses[] = {0x40, 0x41, 0x42, 0x43};
  char names[4][16];

  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    snprintf(names[i], 16, "pca_%d", i);
    star_bus_config_t* i2c = star_bus_config_create_i2c(
      names[i], I2C_NUM_0, addresses[i], I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    star_bus_manager_add_bus(&sys.manager, i2c);
    star_bus_config_init(i2c, &sys.manager);

    pca9685_config_t cfg = {
      .i2c_addr = addresses[i],
      .pwm_freq = SERVO_FREQ_HZ,
      .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
      .ext_clock = false,
      .invert_output = false
    };
    if (star_sensor_pca9685_init(&sys.handles[i], &sys.manager, names[i], &cfg) != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to init PCA9685 #%d (0x%02X)", i + 1, addresses[i]);
      return;
    }
    star_sensor_pca9685_set_frequency(&sys.handles[i], SERVO_FREQ_HZ);
    ESP_LOGI(s_tag, "PCA9685 #%d initialized at 0x%02X", i + 1, addresses[i]);
  }

  ESP_LOGI(s_tag, "Quad PCA9685 chain ready - 64 servo channels");

  /* Center all servos */
  ESP_LOGI(s_tag, "Centering all 64 servos");
  set_all_servos(&sys, 90);
  vTaskDelay(pdMS_TO_TICKS(1000));

  /* Wave motion across all servos */
  ESP_LOGI(s_tag, "Wave motion");
  for (int frame = 0; frame < 100; frame++) {
    for (int ch = 0; ch < TOTAL_CHANNELS; ch++) {
      float phase = (float)frame / 20.0f + (float)ch / 8.0f;
      uint8_t angle = 90 + (int)(45.0f * sinf(phase * M_PI));
      set_servo(&sys, ch, angle);
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  /* Quadruped walking simulation (16 servos per leg) */
  ESP_LOGI(s_tag, "Quadruped walking simulation");
  for (int step = 0; step < 20; step++) {
    /* Leg 1 (channels 0-15) - lift */
    for (int ch = 0; ch < 16; ch++) {
      set_servo(&sys, ch, (step % 2 == 0) ? 120 : 60);
    }
    /* Leg 2 (channels 16-31) - opposite phase */
    for (int ch = 16; ch < 32; ch++) {
      set_servo(&sys, ch, (step % 2 == 0) ? 60 : 120);
    }
    /* Leg 3 (channels 32-47) - same as leg 2 */
    for (int ch = 32; ch < 48; ch++) {
      set_servo(&sys, ch, (step % 2 == 0) ? 60 : 120);
    }
    /* Leg 4 (channels 48-63) - same as leg 1 */
    for (int ch = 48; ch < 64; ch++) {
      set_servo(&sys, ch, (step % 2 == 0) ? 120 : 60);
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  /* Return to center */
  set_all_servos(&sys, 90);
  vTaskDelay(pdMS_TO_TICKS(500));

  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    star_sensor_pca9685_deinit(&sys.handles[i]);
  }
  star_bus_manager_deinit(&sys.manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { pca9685_quad_chain_example(); }
