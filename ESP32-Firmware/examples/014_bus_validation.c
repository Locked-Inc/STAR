/**
 * @file 014_bus_validation.c
 * @brief Bus parameter validation example
 *
 * Demonstrates:
 * - Bus configuration creation
 * - Error handling for invalid parameters
 * - Valid I2C address ranges
 * - Valid frequency ranges
 * - GPIO pin recommendations
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char *s_tag = "BUS_VALIDATION_EXAMPLE";

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Bus Validation Example ===\n");

  /* --- I2C Address Information --- */
  ESP_LOGI(s_tag, "--- I2C Address Information ---");

  ESP_LOGI(s_tag, "Valid 7-bit I2C addresses:");
  ESP_LOGI(s_tag, "  0x08-0x77: Standard device addresses");
  ESP_LOGI(s_tag, "\nReserved I2C addresses:");
  ESP_LOGI(s_tag, "  0x00-0x07: Reserved for special purposes");
  ESP_LOGI(s_tag, "  0x78-0x7F: Reserved for 10-bit addressing");

  /* Examples of valid addresses */
  uint8_t valid_addrs[] = {0x08, 0x48, 0x50, 0x68, 0x77};
  ESP_LOGI(s_tag, "\nCommon valid addresses:");
  for (int i = 0; i < sizeof(valid_addrs); i++) {
    ESP_LOGI(s_tag, "  0x%02X", valid_addrs[i]);
  }

  /* --- I2C Frequency Information --- */
  ESP_LOGI(s_tag, "\n--- I2C Frequency Information ---");

  ESP_LOGI(s_tag, "Standard I2C modes:");
  ESP_LOGI(s_tag, "  Standard Mode:  100 kHz (100000)");
  ESP_LOGI(s_tag, "  Fast Mode:      400 kHz (400000)");
  ESP_LOGI(s_tag, "  Fast Mode Plus: 1 MHz (1000000)");

  /* --- SPI Frequency Information --- */
  ESP_LOGI(s_tag, "\n--- SPI Frequency Information ---");

  ESP_LOGI(s_tag, "Common SPI frequencies:");
  ESP_LOGI(s_tag, "  1 MHz:   1000000");
  ESP_LOGI(s_tag, "  10 MHz:  10000000");
  ESP_LOGI(s_tag, "  26 MHz:  26000000");
  ESP_LOGI(s_tag, "  40 MHz:  40000000");
  ESP_LOGI(s_tag, "  80 MHz:  80000000 (maximum for ESP32)");

  /* --- UART Baud Rate Information --- */
  ESP_LOGI(s_tag, "\n--- UART Baud Rate Information ---");

  ESP_LOGI(s_tag, "Common UART baud rates:");
  ESP_LOGI(s_tag, "  9600, 19200, 38400, 57600");
  ESP_LOGI(s_tag, "  115200, 230400, 460800, 921600");

  /* --- GPIO Information --- */
  ESP_LOGI(s_tag, "\n--- GPIO Information ---");

  ESP_LOGI(s_tag, "Recommended GPIOs for I2C:");
  ESP_LOGI(s_tag, "  GPIO 21, 22 (default SDA, SCL)");
  ESP_LOGI(s_tag, "  GPIO 4, 5, 18, 19, 23 (also usable)");

  ESP_LOGI(s_tag, "\nGPIO restrictions:");
  ESP_LOGI(s_tag, "  GPIO 6-11: Connected to flash, do not use");
  ESP_LOGI(s_tag, "  GPIO 34-39: Input only, no internal pull-up");

  ESP_LOGI(s_tag, "\nRecommended GPIOs for SPI:");
  ESP_LOGI(s_tag, "  MOSI (COPI): GPIO 23");
  ESP_LOGI(s_tag, "  MISO (CIPO): GPIO 19");
  ESP_LOGI(s_tag, "  SCLK:        GPIO 18");
  ESP_LOGI(s_tag, "  CS:          GPIO 5 (or any output GPIO)");

  /* --- Valid Configuration Examples --- */
  ESP_LOGI(s_tag, "\n--- Valid Configuration Examples ---");

  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "validation_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create valid I2C configuration */
  star_bus_config_t* valid_config = star_bus_config_create_i2c(
    "valid_sensor",
    I2C_NUM_0,
    0x48,         /* Valid address */
    GPIO_NUM_21,  /* Valid SDA */
    GPIO_NUM_22,  /* Valid SCL */
    100000        /* Standard mode */
  );

  if (valid_config != NULL) {
    ESP_LOGI(s_tag, "Valid I2C config created successfully");
    ret = star_bus_manager_add_bus(&manager, valid_config);
    ret = star_bus_config_init(valid_config, &manager);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Valid I2C bus initialized successfully");
    }
  } else {
    ESP_LOGE(s_tag, "Failed to create valid config");
  }

  /* --- Testing Multiple Configurations --- */
  ESP_LOGI(s_tag, "\n--- Testing Multiple Configurations ---");

  /* Fast mode I2C */
  star_bus_config_t* fast_i2c = star_bus_config_create_i2c(
    "fast_sensor",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    400000  /* Fast mode */
  );

  if (fast_i2c != NULL) {
    ESP_LOGI(s_tag, "Fast mode I2C config created (400kHz)");
    star_bus_config_destroy(fast_i2c);
  }

  /* Alternative GPIO pins */
  star_bus_config_t* alt_gpio = star_bus_config_create_i2c(
    "alt_sensor",
    I2C_NUM_0,
    0x68,
    GPIO_NUM_4,   /* Alternative SDA */
    GPIO_NUM_5,   /* Alternative SCL */
    100000
  );

  if (alt_gpio != NULL) {
    ESP_LOGI(s_tag, "I2C config with alternative GPIOs created (4, 5)");
    star_bus_config_destroy(alt_gpio);
  }

  /* --- Summary --- */
  ESP_LOGI(s_tag, "\n--- Summary ---");
  ESP_LOGI(s_tag, "Parameter validation tips:");
  ESP_LOGI(s_tag, "1. Use standard I2C addresses (0x08-0x77)");
  ESP_LOGI(s_tag, "2. Use standard frequencies (100kHz, 400kHz, 1MHz)");
  ESP_LOGI(s_tag, "3. Avoid GPIOs 6-11 (flash), 34-39 (input-only)");
  ESP_LOGI(s_tag, "4. Check your board's datasheet for pin restrictions");
  ESP_LOGI(s_tag, "5. SPI max frequency: 80MHz for ESP32");
  ESP_LOGI(s_tag, "6. Use common UART baud rates for compatibility");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nBus validation example complete");
}
