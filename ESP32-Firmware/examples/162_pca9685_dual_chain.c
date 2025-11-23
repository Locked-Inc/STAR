/**
 * @file 162_pca9685_dual_chain.c
 * @brief Two PCA9685 controllers daisy-chained (32 channels)
 *
 * Demonstrates:
 * - Daisy chain with different I2C addresses
 * - Coordinated control across 32 channels
 * - Address configuration via A0-A5 pins
 *
 * Hardware setup:
 * - PCA9685 #1: A0-A5 all LOW  = 0x40
 * - PCA9685 #2: A0 HIGH        = 0x41
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PCA9685_DUAL";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define PCA9685_ADDR_1 (0x40) /* First controller */
#define PCA9685_ADDR_2 (0x41) /* Second controller (A0 high) */

#define TOTAL_CHANNELS (32)

typedef struct {
  pca9685_handle_t* pwm;
  uint8_t channel;
} channel_map_t;

void pca9685_dual_chain_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "PWM_Dual", NULL, NULL);

  /* Create I2C configs for both controllers */
  star_bus_config_t* i2c_1 = star_bus_config_create_i2c(
    "pca9685_1", I2C_NUM_0, PCA9685_ADDR_1, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  star_bus_config_t* i2c_2 = star_bus_config_create_i2c(
    "pca9685_2", I2C_NUM_0, PCA9685_ADDR_2, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  star_bus_manager_add_bus(&manager, i2c_1);
  star_bus_manager_add_bus(&manager, i2c_2);
  star_bus_config_init(i2c_1, &manager);
  star_bus_config_init(i2c_2, &manager);

  /* Initialize both PCA9685 controllers */
  pca9685_handle_t pwm1, pwm2;
  pca9685_config_t cfg1 = {
    .i2c_addr = PCA9685_ADDR_1,
    .pwm_freq = 1000,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };
  pca9685_config_t cfg2 = {
    .i2c_addr = PCA9685_ADDR_2,
    .pwm_freq = 1000,
    .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
    .ext_clock = false,
    .invert_output = false
  };

  if (star_sensor_pca9685_init(&pwm1, &manager, "pca9685_1", &cfg1) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init PCA9685 #1");
    return;
  }
  if (star_sensor_pca9685_init(&pwm2, &manager, "pca9685_2", &cfg2) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init PCA9685 #2");
    return;
  }

  /* Set same frequency on both */
  star_sensor_pca9685_set_frequency(&pwm1, 1000);
  star_sensor_pca9685_set_frequency(&pwm2, 1000);

  ESP_LOGI(s_tag, "Dual PCA9685 chain ready - 32 channels available");

  /* Helper to set unified channel (0-31) */
  pca9685_handle_t* controllers[] = {&pwm1, &pwm2};

  /* Wave pattern across all 32 channels */
  for (int wave = 0; wave < 10; wave++) {
    for (int ch = 0; ch < TOTAL_CHANNELS; ch++) {
      int ctrl_idx = ch / 16;
      int local_ch = ch % 16;

      /* Turn on this channel */
      star_sensor_pca9685_set_pwm(controllers[ctrl_idx], local_ch, 0, 2048);

      /* Turn off previous channel */
      if (ch > 0) {
        int prev_ctrl = (ch - 1) / 16;
        int prev_ch = (ch - 1) % 16;
        star_sensor_pca9685_set_pwm(controllers[prev_ctrl], prev_ch, 0, 0);
      }

      vTaskDelay(pdMS_TO_TICKS(50));
    }
    /* Turn off last channel */
    star_sensor_pca9685_set_pwm(&pwm2, 15, 0, 0);
  }

  /* All channels fade in unison */
  ESP_LOGI(s_tag, "Synchronized fade across all 32 channels");
  for (uint16_t duty = 0; duty <= 4095; duty += 64) {
    for (uint8_t ch = 0; ch < 16; ch++) {
      star_sensor_pca9685_set_pwm(&pwm1, ch, 0, duty);
      star_sensor_pca9685_set_pwm(&pwm2, ch, 0, duty);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  /* All off */
  for (uint8_t ch = 0; ch < 16; ch++) {
    star_sensor_pca9685_set_pwm(&pwm1, ch, 0, 0);
    star_sensor_pca9685_set_pwm(&pwm2, ch, 0, 0);
  }

  star_sensor_pca9685_deinit(&pwm1);
  star_sensor_pca9685_deinit(&pwm2);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { pca9685_dual_chain_example(); }
