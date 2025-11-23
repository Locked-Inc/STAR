/**
 * @file 037_i2c_pullup_config.c
 * @brief Internal pull-up resistor configuration
 *
 * This example demonstrates:
 * - Enabling/disabling internal pull-ups
 * - When to use internal vs external pull-ups
 * - Pull-up resistor value considerations
 * - Testing with and without pull-ups
 * - Troubleshooting pull-up issues
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_PULLUP";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address */
#define DEVICE_ADDR     (0x48)

/* Clock speeds for testing */
#define CLK_STANDARD    (100000)   /* 100 kHz */
#define CLK_FAST        (400000)   /* 400 kHz */

void explain_pullup_resistors(void)
{
  ESP_LOGI(s_tag, "\n=== I2C Pull-up Resistors Explained ===");
  ESP_LOGI(s_tag, "\nWhy pull-ups are needed:");
  ESP_LOGI(s_tag, "  - I2C uses open-drain outputs");
  ESP_LOGI(s_tag, "  - Devices can only pull lines LOW");
  ESP_LOGI(s_tag, "  - Pull-ups are needed to bring lines HIGH");
  ESP_LOGI(s_tag, "  - Without pull-ups, lines stay at 0V (undefined state)");

  ESP_LOGI(s_tag, "\nInternal vs External pull-ups:");
  ESP_LOGI(s_tag, "  ESP32 internal: ~45 kOhm (weak)");
  ESP_LOGI(s_tag, "  External typical: 2.2-4.7 kOhm (strong)");

  ESP_LOGI(s_tag, "\nWhen to use internal:");
  ESP_LOGI(s_tag, "  - Short bus (<10cm)");
  ESP_LOGI(s_tag, "  - Few devices (1-2)");
  ESP_LOGI(s_tag, "  - Low speed (100 kHz)");
  ESP_LOGI(s_tag, "  - Testing/prototyping");

  ESP_LOGI(s_tag, "\nWhen to use external:");
  ESP_LOGI(s_tag, "  - Long bus (>10cm)");
  ESP_LOGI(s_tag, "  - Multiple devices (>2)");
  ESP_LOGI(s_tag, "  - High speed (400 kHz+)");
  ESP_LOGI(s_tag, "  - Production systems");
  ESP_LOGI(s_tag, "  - High capacitance on bus");
}

void configure_internal_pullups(bool enable)
{
  ESP_LOGI(s_tag, "\n=== %s Internal Pull-ups ===",
           enable ? "Enabling" : "Disabling");

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << I2C_SDA_PIN) | (1ULL << I2C_SCL_PIN),
    .mode         = GPIO_MODE_INPUT_OUTPUT_OD,  /* Open-drain */
    .pull_up_en   = enable ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };

  esp_err_t ret = gpio_config(&io_conf);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pull-ups %s on SDA (GPIO%d) and SCL (GPIO%d)",
             enable ? "ENABLED" : "DISABLED", I2C_SDA_PIN, I2C_SCL_PIN);
  } else {
    ESP_LOGE(s_tag, "Failed to configure pull-ups: %s", esp_err_to_name(ret));
  }
}

void test_with_pullup_config(star_bus_manager_t *manager, bool use_internal_pullup)
{
  ESP_LOGI(s_tag, "\n=== Test with %s Pull-ups ===",
           use_internal_pullup ? "Internal" : "External (or None)");

  /* Configure pull-ups */
  configure_internal_pullups(use_internal_pullup);

  /* Small delay for settling */
  vTaskDelay(pdMS_TO_TICKS(50));

  /* Try communication */
  uint8_t data[4];
  size_t  bytes_read;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "device", data, 4,
                                     0x00, &bytes_read);
  int64_t elapsed_us = esp_timer_get_time() - start;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SUCCESS: Read %u bytes in %lld us",
             (unsigned)bytes_read, elapsed_us);
    ESP_LOGI(s_tag, "Data: 0x%02X 0x%02X 0x%02X 0x%02X",
             data[0], data[1], data[2], data[3]);
  } else {
    ESP_LOGW(s_tag, "FAILED: %s (time: %lld us)",
             esp_err_to_name(ret), elapsed_us);

    if (ret == ESP_ERR_TIMEOUT) {
      ESP_LOGW(s_tag, "Possible causes:");
      ESP_LOGW(s_tag, "  - Pull-ups too weak for bus length/capacitance");
      ESP_LOGW(s_tag, "  - No pull-ups present");
      ESP_LOGW(s_tag, "  - Device not responding");
    }
  }
}

void test_different_speeds(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Testing Different Clock Speeds ===");

  uint32_t    speeds[]      = {CLK_STANDARD, CLK_FAST};
  const char *speed_names[] = {"100 kHz (Standard)", "400 kHz (Fast)"};

  for (int i = 0; i < 2; i++) {
    ESP_LOGI(s_tag, "\n--- Testing at %s ---", speed_names[i]);

    /* Create config with specific speed */
    char bus_name[32];
    snprintf(bus_name, sizeof(bus_name), "speed_test_%d", i);

    star_bus_config_t *config = star_bus_config_create_i2c(
      bus_name,
      I2C_NUM_0,
      DEVICE_ADDR,
      I2C_SDA_PIN,
      I2C_SCL_PIN,
      speeds[i]
    );

    if (config) {
      star_bus_manager_add_bus(manager, config);
      esp_err_t init_ret = star_bus_config_init(config, manager);

      if (init_ret == ESP_OK) {
        uint8_t data;
        size_t  bytes_read;

        int64_t start = esp_timer_get_time();
        esp_err_t ret = star_bus_i2c_read(manager, bus_name, &data, 1,
                                          0x00, &bytes_read);
        int64_t elapsed_us = esp_timer_get_time() - start;

        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "SUCCESS at %s: %lld us", speed_names[i], elapsed_us);
        } else {
          ESP_LOGW(s_tag, "FAILED at %s: %s", speed_names[i],
                   esp_err_to_name(ret));

          if (i == 1) {
            ESP_LOGI(s_tag, "Fast mode failure often indicates:");
            ESP_LOGI(s_tag, "  - Pull-ups too weak (use stronger external)");
            ESP_LOGI(s_tag, "  - Bus capacitance too high");
            ESP_LOGI(s_tag, "  - Cable too long");
          }
        }
      }

      star_bus_manager_remove_bus(manager, bus_name);
      star_bus_config_destroy(config);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void calculate_pullup_value(void)
{
  ESP_LOGI(s_tag, "\n=== Pull-up Resistor Value Calculation ===");

  /* Formula: R = (Vdd - Vol) / Iol
   * Typical values:
   * Vdd = 3.3V (ESP32 supply)
   * Vol = 0.4V (max LOW level)
   * Iol = 3mA (minimum sink current)
   */

  float vdd     = 3.3f;
  float vol     = 0.4f;
  float iol_min = 0.003f;  /* 3mA */

  float r_max = (vdd - vol) / iol_min;

  ESP_LOGI(s_tag, "Maximum pull-up resistance:");
  ESP_LOGI(s_tag, "  R_max = (Vdd - Vol) / Iol");
  ESP_LOGI(s_tag, "  R_max = (%.1f - %.1f) / %.3f", vdd, vol, iol_min);
  ESP_LOGI(s_tag, "  R_max = %.0f Ohms", r_max);

  ESP_LOGI(s_tag, "\nCapacitance consideration:");
  ESP_LOGI(s_tag, "  Rise time = 0.693 * R * C");
  ESP_LOGI(s_tag, "  For 100 kHz: max rise time = 1000 ns");
  ESP_LOGI(s_tag, "  For 400 kHz: max rise time = 300 ns");

  /* Example with 100 pF bus capacitance */
  float c_bus           = 100e-12f;  /* 100 pF */
  float t_rise_max_100k = 1000e-9f;  /* 1000 ns */
  float r_for_100k      = t_rise_max_100k / (0.693f * c_bus);

  ESP_LOGI(s_tag, "\nFor 100 pF bus capacitance at 100 kHz:");
  ESP_LOGI(s_tag, "  R_max = %.1f kOhm", r_for_100k / 1000.0f);

  ESP_LOGI(s_tag, "\nRecommended values:");
  ESP_LOGI(s_tag, "  100 kHz: 4.7 kOhm - 10 kOhm");
  ESP_LOGI(s_tag, "  400 kHz: 2.2 kOhm - 4.7 kOhm");
  ESP_LOGI(s_tag, "  ESP32 internal: ~45 kOhm (suitable for 100 kHz only)");
}

void diagnose_pullup_issues(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Pull-up Issue Diagnosis ===");

  ESP_LOGI(s_tag, "\nStep 1: Check if device responds at all");
  uint8_t   dummy;
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, "device", &dummy, 1,
                                    0x00, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "  Device responds - pull-ups are adequate");
  } else {
    ESP_LOGI(s_tag, "  Device doesn't respond - checking pull-ups...");

    ESP_LOGI(s_tag, "\nStep 2: Try enabling internal pull-ups");
    configure_internal_pullups(true);
    vTaskDelay(pdMS_TO_TICKS(50));

    ret = star_bus_i2c_read(manager, "device", &dummy, 1, 0x00, &bytes_read);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "  Device responds with internal pull-ups");
      ESP_LOGI(s_tag, "  RECOMMENDATION: Add external pull-ups for reliability");
    } else {
      ESP_LOGI(s_tag, "  Still no response");
      ESP_LOGI(s_tag, "  Possible issues:");
      ESP_LOGI(s_tag, "    - Device not connected");
      ESP_LOGI(s_tag, "    - Wrong device address");
      ESP_LOGI(s_tag, "    - Device not powered");
      ESP_LOGI(s_tag, "    - SDA/SCL swapped");
      ESP_LOGI(s_tag, "    - Wiring issue");
    }
  }
}

void i2c_pullup_config_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Pullup", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    CLK_STANDARD
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c_config);
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "I2C driver initialized");

  /* Educational content */
  explain_pullup_resistors();
  vTaskDelay(pdMS_TO_TICKS(500));

  calculate_pullup_value();
  vTaskDelay(pdMS_TO_TICKS(500));

  /* Test with different configurations */
  test_with_pullup_config(&manager, false);  /* External/none */
  vTaskDelay(pdMS_TO_TICKS(500));

  test_with_pullup_config(&manager, true);   /* Internal */
  vTaskDelay(pdMS_TO_TICKS(500));

  test_different_speeds(&manager);
  vTaskDelay(pdMS_TO_TICKS(500));

  diagnose_pullup_issues(&manager);

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Pull-up Configuration Summary ===");
  ESP_LOGI(s_tag, "Key Points:");
  ESP_LOGI(s_tag, "  1. I2C requires pull-up resistors");
  ESP_LOGI(s_tag, "  2. Internal pull-ups (~45k) OK for short, slow buses");
  ESP_LOGI(s_tag, "  3. External pull-ups (2.2-4.7k) for production");
  ESP_LOGI(s_tag, "  4. Stronger pull-ups needed for:");
  ESP_LOGI(s_tag, "     - Longer cables");
  ESP_LOGI(s_tag, "     - Higher speeds");
  ESP_LOGI(s_tag, "     - More devices");
  ESP_LOGI(s_tag, "  5. Calculate based on bus capacitance and speed");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nPull-up configuration example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Pull-up Configuration Example ===\n");
  i2c_pullup_config_example();
}
