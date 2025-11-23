/**
 * @file 165_pca9685_dynamic_chain.c
 * @brief Dynamic PCA9685 chain detection and management
 *
 * Demonstrates:
 * - Runtime detection of chained controllers
 * - Dynamic address scanning
 * - Hot-plug support
 * - Unified channel abstraction for any chain size
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

static const char *s_tag = "PCA9685_DYNAMIC";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define PCA9685_BASE_ADDR (0x40) /* First possible address */
#define PCA9685_MAX_ADDR (0x7F) /* Last possible address */
#define MAX_CHAIN_SIZE (62) /* Maximum possible with A0-A5 address bits */

typedef struct {
  pca9685_handle_t* handles;
  uint8_t *addresses;
  char** bus_names;
  int count;
  int total_channels;
  star_bus_manager_t* manager;
} dynamic_pwm_chain_t;

static bool probe_i2c_address(star_bus_manager_t* manager, uint8_t addr)
{
  /* Create temporary config for probing */
  char probe_name[16];
  snprintf(probe_name, 16, "probe_%02x", addr);

  star_bus_config_t* probe = star_bus_config_create_i2c(
    probe_name, I2C_NUM_0, addr, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

  if (probe == NULL) return false;

  star_bus_manager_add_bus(manager, probe);
  esp_err_t ret = star_bus_config_init(probe, manager);

  if (ret == ESP_OK) {
    /* Try to read a register to confirm it's a PCA9685 */
    uint8_t mode1;
    ret = star_bus_i2c_read(manager, probe_name, &mode1, 1, 0x00, NULL);
    star_bus_config_deinit(probe);
    star_bus_manager_remove_bus(manager, probe_name);

    /* PCA9685 MODE1 register should have reasonable value */
    return (ret == ESP_OK && (mode1 & 0x80) == 0);  /* Check restart bit */
  }

  star_bus_manager_remove_bus(manager, probe_name);
  return false;
}

static dynamic_pwm_chain_t* create_dynamic_chain(star_bus_manager_t* manager)
{
  dynamic_pwm_chain_t* chain = calloc(1, sizeof(dynamic_pwm_chain_t));
  if (chain == NULL) return NULL;

  chain->manager = manager;
  chain->handles = calloc(MAX_CHAIN_SIZE, sizeof(pca9685_handle_t));
  chain->addresses = calloc(MAX_CHAIN_SIZE, sizeof(uint8_t));
  chain->bus_names = calloc(MAX_CHAIN_SIZE, sizeof(char*));

  if (!chain->handles || !chain->addresses || !chain->bus_names) {
    free(chain->handles);
    free(chain->addresses);
    free(chain->bus_names);
    free(chain);
    return NULL;
  }

  /* Scan for PCA9685 devices */
  ESP_LOGI(s_tag, "Scanning I2C bus for PCA9685 controllers...");

  for (uint8_t addr = PCA9685_BASE_ADDR; addr <= 0x7F && chain->count < MAX_CHAIN_SIZE; addr++) {
    if (probe_i2c_address(manager, addr)) {
      ESP_LOGI(s_tag, "Found PCA9685 at address 0x%02X", addr);

      chain->addresses[chain->count] = addr;
      chain->bus_names[chain->count] = malloc(16);
      snprintf(chain->bus_names[chain->count], 16, "pca_%02x", addr);

      /* Create permanent config */
      star_bus_config_t* i2c = star_bus_config_create_i2c(
        chain->bus_names[chain->count], I2C_NUM_0, addr,
        I2C_SDA_PIN, I2C_SCL_PIN, 400000);
      star_bus_manager_add_bus(manager, i2c);
      star_bus_config_init(i2c, manager);

      /* Initialize PCA9685 */
      pca9685_config_t cfg = {
        .i2c_addr = addr,
        .pwm_freq = 1000,
        .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
        .ext_clock = false,
        .invert_output = false
      };
      star_sensor_pca9685_init(&chain->handles[chain->count], manager, chain->bus_names[chain->count], &cfg);
      star_sensor_pca9685_set_frequency(&chain->handles[chain->count], 1000);

      chain->count++;
    }
  }

  chain->total_channels = chain->count * 16;
  ESP_LOGI(s_tag, "Dynamic chain created: %d controllers, %d total channels",
           chain->count, chain->total_channels);

  return chain;
}

static void set_unified_pwm(dynamic_pwm_chain_t* chain, int channel, uint16_t value)
{
  if (channel < 0 || channel >= chain->total_channels) return;
  int ctrl_idx = channel / 16;
  int local_ch = channel % 16;
  star_sensor_pca9685_set_pwm(&chain->handles[ctrl_idx], local_ch, 0, value);
}

static void destroy_dynamic_chain(dynamic_pwm_chain_t* chain)
{
  for (int i = 0; i < chain->count; i++) {
    star_sensor_pca9685_deinit(&chain->handles[i]);
    star_bus_manager_remove_bus(chain->manager, chain->bus_names[i]);
    free(chain->bus_names[i]);
  }
  free(chain->handles);
  free(chain->addresses);
  free(chain->bus_names);
  free(chain);
}

void pca9685_dynamic_chain_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Dynamic_PWM", NULL, NULL);

  /* Dynamically detect and initialize all PCA9685 controllers */
  dynamic_pwm_chain_t* chain = create_dynamic_chain(&manager);

  if (chain == NULL || chain->count == 0) {
    ESP_LOGE(s_tag, "No PCA9685 controllers found!");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Detected %d PCA9685 controllers", chain->count);
  for (int i = 0; i < chain->count; i++) {
    ESP_LOGI(s_tag, "  Controller %d: 0x%02X (%s)",
             i + 1, chain->addresses[i], chain->bus_names[i]);
  }

  /* Demo: Sequential sweep across all detected channels */
  ESP_LOGI(s_tag, "Running sweep across %d channels", chain->total_channels);

  for (int sweep = 0; sweep < 3; sweep++) {
    for (int ch = 0; ch < chain->total_channels; ch++) {
      /* Turn on current channel */
      set_unified_pwm(chain, ch, 2048);

      /* Turn off previous channel */
      if (ch > 0) {
        set_unified_pwm(chain, ch - 1, 0);
      }

      vTaskDelay(pdMS_TO_TICKS(30));
    }
    /* Turn off last channel */
    set_unified_pwm(chain, chain->total_channels - 1, 0);
  }

  /* Demo: Synchronized fade across all channels */
  ESP_LOGI(s_tag, "Synchronized fade");
  for (uint16_t brightness = 0; brightness <= 4095; brightness += 128) {
    for (int ch = 0; ch < chain->total_channels; ch++) {
      set_unified_pwm(chain, ch, brightness);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  for (uint16_t brightness = 4095; brightness > 0; brightness -= 128) {
    for (int ch = 0; ch < chain->total_channels; ch++) {
      set_unified_pwm(chain, ch, brightness);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  /* All off */
  for (int ch = 0; ch < chain->total_channels; ch++) {
    set_unified_pwm(chain, ch, 0);
  }

  destroy_dynamic_chain(chain);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { pca9685_dynamic_chain_example(); }
