/**
 * @file 04_pin_validator.c
 * @brief Pin validator example
 *
 * This example demonstrates:
 * - Registering GPIO pins with descriptions
 * - Detecting pin conflicts
 * - Sharing pins between components
 * - Validating pin configurations
 * - Integrating with bus manager
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_pin_interface.h"
#include "star_pin_validator.h"

#include <esp_log.h>

static const char *s_tag = "PIN_VALIDATOR_EXAMPLE";

void pin_validator_example(void)
{
  esp_err_t ret;

  /* Step 1: Register pins with descriptions */
  ESP_LOGI(s_tag, "--- Registering Pins ---");

  /* I2C pins - not sharable */
  ret = star_register_pin(GPIO_NUM_21, "I2C SDA", false);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Registered GPIO 21 as I2C SDA");
  } else {
    ESP_LOGE(s_tag, "Failed to register GPIO 21: %s", esp_err_to_name(ret));
  }

  ret = star_register_pin(GPIO_NUM_22, "I2C SCL", false);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Registered GPIO 22 as I2C SCL");
  }

  /* SPI pins - not sharable */
  star_register_pin(GPIO_NUM_23, "SPI COPI", false);
  star_register_pin(GPIO_NUM_19, "SPI CIPO", false);
  star_register_pin(GPIO_NUM_18, "SPI SCLK", false);
  star_register_pin(GPIO_NUM_5, "SPI CS1", false);
  ESP_LOGI(s_tag, "Registered SPI pins");

  /* UART pins - not sharable */
  star_register_pin(GPIO_NUM_17, "UART TX", false);
  star_register_pin(GPIO_NUM_16, "UART RX", false);
  ESP_LOGI(s_tag, "Registered UART pins");

  /* Step 2: Try to register a pin that's already in use */
  ESP_LOGI(s_tag, "\n--- Testing Pin Conflicts ---");

  ret = star_register_pin(GPIO_NUM_21, "ADC Input", false);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "GPIO 21 conflict detected: %s", esp_err_to_name(ret));
    /* Expected: ESP_ERR_INVALID_STATE because GPIO 21 is already used */
  }

  /* Step 3: Register sharable pins */
  ESP_LOGI(s_tag, "\n--- Sharable Pins ---");

  /* Some pins can be shared (e.g., for interrupt notification) */
  ret = star_register_pin(GPIO_NUM_4, "1-Wire Data", true);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Registered GPIO 4 as sharable 1-Wire");
  }

  /* Register same pin with another user (allowed because can_be_shared=true) */
  ret = star_register_pin(GPIO_NUM_4, "Temperature Sensor", true);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "GPIO 4 shared with Temperature Sensor");
  }

  /* Step 4: Validate all pins for conflicts */
  ESP_LOGI(s_tag, "\n--- Validating All Pins ---");

  ret = star_validate_pins();
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pin validation passed - no conflicts");
  } else {
    ESP_LOGE(s_tag, "Pin validation failed - conflicts detected!");
  }

  /* Step 5: Unregister a pin */
  ESP_LOGI(s_tag, "\n--- Unregistering Pins ---");

  ret = star_unregister_pin(GPIO_NUM_4, "Temperature Sensor");
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Unregistered 'Temperature Sensor' from GPIO 4");
  }

  /* The 1-Wire Data user is still registered */
  ret = star_unregister_pin(GPIO_NUM_4, "1-Wire Data");
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Unregistered '1-Wire Data' from GPIO 4");
  }

  /* Step 6: Show reserved/strapping pins note */
  ESP_LOGI(s_tag, "\n--- Reserved Pin Information ---");
  ESP_LOGI(s_tag, "Some GPIO pins have special functions:");
  ESP_LOGI(s_tag, "  GPIO 0  - Boot mode (strapping)");
  ESP_LOGI(s_tag, "  GPIO 2  - Boot mode (strapping)");
  ESP_LOGI(s_tag, "  GPIO 12 - Boot mode (strapping)");
  ESP_LOGI(s_tag, "  GPIO 15 - Boot mode (strapping)");
  ESP_LOGI(s_tag, "  GPIO 6-11 - Connected to flash (do not use)");
  ESP_LOGI(s_tag, "  GPIO 34-39 - Input only (no pull-up/down)");

  /* Step 7: Cleanup */
  star_free_pin_validator();
  ESP_LOGI(s_tag, "\nPin validator example complete");
}

/* Example: Using pin validator with bus manager */
void pin_validator_with_bus_example(void)
{
  esp_err_t ret;

  /* Create pin validator interface */
  star_pin_interface_t pin_iface;
  pin_validator_get_interface(&pin_iface);

  /* Initialize bus manager with pin validator */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "PinDemo", NULL, &pin_iface);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Bus manager initialized with pin validation");

    /* When you add and initialize buses, their pins will be */
    /* automatically registered with the pin validator */

    /* Example: Create I2C config */
    star_bus_config_t *i2c_config = star_bus_config_create_i2c(
      "i2c_sensor",
      I2C_NUM_0,
      0x50,
      GPIO_NUM_21,
      GPIO_NUM_22,
      100000
    );

    if (i2c_config != NULL) {
      ret = star_bus_manager_add_bus(&manager, i2c_config);
      if (ret == ESP_OK) {
        ret = star_bus_config_init(i2c_config, &manager);
        if (ret == ESP_OK) {
          ESP_LOGI(s_tag, "I2C initialized - pins automatically registered");
        }
      }
    }

    /* Try to create another device using same pins - should detect conflict */
    star_bus_config_t *conflict_config = star_bus_config_create_i2c(
      "conflict_device",
      I2C_NUM_1,
      0x60,
      GPIO_NUM_21,  /* Conflict with I2C above! */
      GPIO_NUM_22,  /* Conflict with I2C above! */
      100000
    );

    if (conflict_config != NULL) {
      ret = star_bus_manager_add_bus(&manager, conflict_config);
      if (ret == ESP_OK) {
        ret = star_bus_config_init(conflict_config, &manager);
        if (ret != ESP_OK) {
          ESP_LOGW(s_tag, "Second I2C init failed (expected - pin conflict)");
        }
      }
    }

    /* Validate all pins */
    ret = star_validate_pins();
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "All pins validated successfully");
    } else {
      ESP_LOGW(s_tag, "Pin conflicts detected!");
    }
  }

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== Pin Validator Example ===\n");
  pin_validator_example();

  ESP_LOGI(s_tag, "\n=== Pin Validator with Bus Manager ===\n");
  pin_validator_with_bus_example();
}
