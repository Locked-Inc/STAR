/**
 * @file 019_bus_manager.c
 * @brief Bus manager operations example
 *
 * Demonstrates:
 * - Bus manager initialization
 * - Adding and removing buses
 * - Finding buses by name
 * - Safe bus access with callbacks
 * - Multiple bus management
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"

#include <esp_log.h>

static const char *s_tag = "BUS_MANAGER_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

/* Pin registration callback wrapper */
static esp_err_t pin_registration_callback(gpio_num_t pin_num,
                                           const char* bus_name,
                                           const char* pin_usage,
                                           void* user_ctx)
{
  (void)user_ctx; /* Unused */

  /* Create combined description */
  char desc[64];
  snprintf(desc, sizeof(desc), "%s - %s", bus_name, pin_usage);

  /* Register with pin validator - I2C pins can be shared */
  return star_register_pin(pin_num, desc, true);
}

/* Callback for safe bus access */
static esp_err_t read_sensor_callback(star_bus_config_t* config, void* user_data)
{
  uint8_t* buffer = (uint8_t*)user_data;

  ESP_LOGI(s_tag, "Inside callback - bus is locked");
  ESP_LOGI(s_tag, "Bus name: %s", config->name);

  /* Do bus operations here - bus is thread-safe */
  /* In real code, you would read from the sensor */
  buffer[0] = 0x55;
  buffer[1] = 0xAA;

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Bus Manager Example ===\n");

  /* --- Initialize with Interfaces --- */
  ESP_LOGI(s_tag, "--- Initialize with Interfaces ---");

  /* Create error handler */
  error_handler_t error_handler;
  error_handler_init(&error_handler, 3, 100, 1000, NULL, NULL);

  /* Create interface adapters */
  star_error_interface_t error_iface;
  star_pin_interface_t pin_iface;

  error_handler_get_interface(&error_iface, &error_handler);
  pin_validator_get_interface(&pin_iface);

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "demo_manager", &error_iface, &pin_iface);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "Bus manager initialized with:");
  ESP_LOGI(s_tag, "  Tag: %s", manager.tag);
  ESP_LOGI(s_tag, "  Error interface: %s", error_iface.record_error ? "Yes" : "No");
  ESP_LOGI(s_tag, "  Pin interface: %s", pin_iface.register_pin ? "Yes" : "No");

  /* --- Add Buses --- */
  ESP_LOGI(s_tag, "\n--- Adding Buses ---");

  /* Create I2C configurations */
  star_bus_config_t* sensor1 = star_bus_config_create_i2c(
    "temp_sensor",
    I2C_NUM_0,
    0x48,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  star_bus_config_t* sensor2 = star_bus_config_create_i2c(
    "humidity_sensor",
    I2C_NUM_0,
    0x40,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  star_bus_config_t* eeprom = star_bus_config_create_i2c(
    "eeprom",
    I2C_NUM_0,
    0x50,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (sensor1 == NULL || sensor2 == NULL || eeprom == NULL) {
    ESP_LOGE(s_tag, "Failed to create configs");
    return;
  }

  /* Add buses to manager */
  ret = star_bus_manager_add_bus(&manager, sensor1);
  ESP_LOGI(s_tag, "Added 'temp_sensor': %s", esp_err_to_name(ret));

  ret = star_bus_manager_add_bus(&manager, sensor2);
  ESP_LOGI(s_tag, "Added 'humidity_sensor': %s", esp_err_to_name(ret));

  ret = star_bus_manager_add_bus(&manager, eeprom);
  ESP_LOGI(s_tag, "Added 'eeprom': %s", esp_err_to_name(ret));

  /* Initialize buses */
  star_bus_config_init(sensor1, &manager);
  star_bus_config_init(sensor2, &manager);
  star_bus_config_init(eeprom, &manager);

  /* --- Find Buses --- */
  ESP_LOGI(s_tag, "\n--- Finding Buses ---");

  star_bus_config_t* found = star_bus_manager_find_bus(&manager, "temp_sensor");
  if (found != NULL) {
    ESP_LOGI(s_tag, "Found 'temp_sensor' at address 0x%02X", found->proto.i2c.address);
  } else {
    ESP_LOGW(s_tag, "'temp_sensor' not found");
  }

  found = star_bus_manager_find_bus(&manager, "humidity_sensor");
  if (found != NULL) {
    ESP_LOGI(s_tag, "Found 'humidity_sensor' at address 0x%02X", found->proto.i2c.address);
  }

  /* Try to find non-existent bus */
  found = star_bus_manager_find_bus(&manager, "nonexistent");
  if (found == NULL) {
    ESP_LOGI(s_tag, "'nonexistent' correctly not found");
  }

  /* --- Safe Bus Access --- */
  ESP_LOGI(s_tag, "\n--- Safe Bus Access ---");

  uint8_t read_buffer[2] = {0};

  ret = star_bus_manager_with_bus(&manager, "temp_sensor", read_sensor_callback, read_buffer);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Callback executed successfully");
    ESP_LOGI(s_tag, "Read data: 0x%02X 0x%02X", read_buffer[0], read_buffer[1]);
  }

  ESP_LOGI(s_tag, "\nBenefits of star_bus_manager_with_bus():");
  ESP_LOGI(s_tag, "  - Automatic mutex locking");
  ESP_LOGI(s_tag, "  - Bus lookup by name");
  ESP_LOGI(s_tag, "  - Error handling integration");
  ESP_LOGI(s_tag, "  - Thread-safe access");

  /* --- Register All Pins --- */
  ESP_LOGI(s_tag, "\n--- Register All Pins ---");

  ret = star_bus_manager_register_all_pins(&manager, pin_registration_callback, NULL);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "All bus pins registered with validator");
  }

  /* Validate pins */
  ret = star_validate_pins();
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Pin validation passed");
  } else {
    ESP_LOGW(s_tag, "Pin conflicts detected");
  }

  /* --- Remove Bus --- */
  ESP_LOGI(s_tag, "\n--- Removing Bus ---");

  ret = star_bus_manager_remove_bus(&manager, "eeprom");
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Removed 'eeprom' from manager");
  }

  /* Verify removal */
  found = star_bus_manager_find_bus(&manager, "eeprom");
  if (found == NULL) {
    ESP_LOGI(s_tag, "'eeprom' correctly removed");
  }

  /* --- Manager without Interfaces --- */
  ESP_LOGI(s_tag, "\n--- Manager without Interfaces ---");

  star_bus_manager_t simple_manager;
  ret = star_bus_manager_init(&simple_manager, "simple", NULL, NULL);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Simple manager initialized (no error/pin interfaces)");

    /* Add a bus */
    star_bus_config_t* simple_bus = star_bus_config_create_i2c(
      "simple_device",
      I2C_NUM_1,
      0x68,
      GPIO_NUM_25,
      GPIO_NUM_26,
      100000
    );

    if (simple_bus != NULL) {
      star_bus_manager_add_bus(&simple_manager, simple_bus);
      ESP_LOGI(s_tag, "Added bus to simple manager");
    }

    star_bus_manager_deinit(&simple_manager);
  }

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Use descriptive bus names");
  ESP_LOGI(s_tag, "2. Initialize interfaces before manager");
  ESP_LOGI(s_tag, "3. Use star_bus_manager_with_bus() for thread safety");
  ESP_LOGI(s_tag, "4. Register pins to detect conflicts");
  ESP_LOGI(s_tag, "5. Remove buses before deinit if needed");
  ESP_LOGI(s_tag, "6. Check return values for all operations");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
  error_handler_deinit(&error_handler);

  ESP_LOGI(s_tag, "\nBus manager example complete");
}
