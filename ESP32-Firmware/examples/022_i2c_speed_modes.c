/**
 * @file 022_i2c_speed_modes.c
 * @brief I2C speed modes and presets example
 *
 * Demonstrates:
 * - Standard mode (100 kHz)
 * - Fast mode (400 kHz)
 * - Fast mode plus (1 MHz)
 * - Using speed presets
 */

#include "star_bus_config.h"
#include "star_bus_helpers.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char *s_tag = "I2C_SPEED_EXAMPLE";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define SENSOR_ADDR    (0x48)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== I2C Speed Modes Example ===\n");

  ret = star_bus_manager_init(&s_manager, "speed_demo", NULL, NULL);
  if (ret != ESP_OK) {
    return;
  }

  /* --- Standard Mode (100 kHz) --- */
  ESP_LOGI(s_tag, "--- Standard Mode (100 kHz) ---");

  /* Use helper function to create standard mode config */
  star_bus_config_t* std_config = star_bus_config_i2c_standard(
    "std_sensor",
    SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN
  );

  if (std_config != NULL) {
    star_bus_manager_add_bus(&s_manager, std_config);
    ret = star_bus_config_init(std_config, &s_manager);
    ESP_LOGI(s_tag, "Standard mode init: %s", esp_err_to_name(ret));
    ESP_LOGI(s_tag, "Use for: Long wires, noisy environments, legacy devices");
  }

  /* --- Fast Mode (400 kHz) --- */
  ESP_LOGI(s_tag, "\n--- Fast Mode (400 kHz) ---");

  /* Use helper function to create fast mode config */
  star_bus_config_t* fast_config = star_bus_config_i2c_fast(
    "fast_sensor",
    SENSOR_ADDR,
    GPIO_NUM_25,
    GPIO_NUM_26
  );

  if (fast_config != NULL) {
    star_bus_manager_add_bus(&s_manager, fast_config);
    ret = star_bus_config_init(fast_config, &s_manager);
    ESP_LOGI(s_tag, "Fast mode init: %s", esp_err_to_name(ret));
    ESP_LOGI(s_tag, "Use for: Most modern sensors and EEPROMs");
  }

  /* --- Fast Mode Plus (1 MHz) --- */
  ESP_LOGI(s_tag, "\n--- Fast Mode Plus (1 MHz) ---");

  /* Use helper function to create fast mode plus config */
  star_bus_config_t* fmp_config = star_bus_config_i2c_fast_plus(
    "fmp_sensor",
    0x50,
    I2C_SDA_PIN,
    I2C_SCL_PIN
  );

  if (fmp_config != NULL) {
    ESP_LOGI(s_tag, "Fast mode plus configured");
    ESP_LOGI(s_tag, "Use for: High-speed EEPROMs, displays, short wires");
    ESP_LOGI(s_tag, "Note: Requires compatible devices and proper pull-ups");
  }

  /* --- Speed Comparison --- */
  ESP_LOGI(s_tag, "\n--- Speed Comparison ---");
  ESP_LOGI(s_tag, "| Mode            | Speed    | Typical Use            |");
  ESP_LOGI(s_tag, "|-----------------|----------|------------------------|");
  ESP_LOGI(s_tag, "| Standard        | 100 kHz  | Legacy, long wires     |");
  ESP_LOGI(s_tag, "| Fast            | 400 kHz  | Most sensors, EEPROMs  |");
  ESP_LOGI(s_tag, "| Fast Mode Plus  | 1 MHz    | High-speed, short wire |");

  /* --- Pull-up Recommendations --- */
  ESP_LOGI(s_tag, "\n--- Pull-up Resistor Recommendations ---");
  ESP_LOGI(s_tag, "Standard mode: 10kΩ");
  ESP_LOGI(s_tag, "Fast mode: 4.7kΩ");
  ESP_LOGI(s_tag, "Fast mode plus: 2kΩ - 3kΩ");
  ESP_LOGI(s_tag, "Note: Lower resistance for higher speeds");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nI2C speed modes example complete");
}
