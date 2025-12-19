/* lib/star_bus/src/star_bus_gpio.c */

#include "star_bus_gpio.h"

#include "driver/gpio.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager.h" /* Needed for star_bus_manager_find_bus */
#include "star_bus_types.h"   /* Needed for star_bus_config_t */

/* --- Constants --- */

static const char* s_tag = "STAR_BUS_GPIO";

/* --- Private Function Prototypes (Default Ops) --- */

static esp_err_t internal_star_bus_gpio_write_digital(const star_bus_config_t* config,
                                                      uint8_t                  pin_index,
                                                      uint8_t                  value);

static esp_err_t internal_star_bus_gpio_read_digital(const star_bus_config_t* config,
                                                     uint8_t                  pin_index,
                                                     uint8_t*                 value);

/* --- Public Functions --- */

star_gpio_ops_t star_bus_gpio_get_default_ops(void)
{
  star_gpio_ops_t ops = {
    .write_digital = internal_star_bus_gpio_write_digital,
    .read_digital  = internal_star_bus_gpio_read_digital,
  };
  ESP_LOGD(s_tag, "Default GPIO operations initialized");
  return ops;
}

esp_err_t star_bus_gpio_write_digital(star_bus_manager_t* manager,
                                      const char*         bus_name,
                                      uint8_t             pin_index,
                                      uint8_t             value)
{
  ESP_RETURN_ON_FALSE(manager && bus_name,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Manager or bus_name is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, bus_name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", bus_name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not a GPIO bus",
                      bus_name);
  ESP_RETURN_ON_FALSE(bus_config->proto.gpio.ops.write_digital,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No write_digital op for '%s'",
                      bus_name);

  return bus_config->proto.gpio.ops.write_digital(bus_config, pin_index, value);
}

esp_err_t star_bus_gpio_read_digital(star_bus_manager_t* manager,
                                     const char*         bus_name,
                                     uint8_t             pin_index,
                                     uint8_t*            value)
{
  ESP_RETURN_ON_FALSE(manager && bus_name && value,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Manager, bus_name, or value is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, bus_name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", bus_name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not a GPIO bus",
                      bus_name);
  ESP_RETURN_ON_FALSE(bus_config->proto.gpio.ops.read_digital,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No read_digital op for '%s'",
                      bus_name);

  return bus_config->proto.gpio.ops.read_digital(bus_config, pin_index, value);
}

/* --- Private Functions (Default Ops Implementation) --- */

static esp_err_t internal_star_bus_gpio_write_digital(const star_bus_config_t* config,
                                                      uint8_t                  pin_index,
                                                      uint8_t                  value)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
  ESP_RETURN_ON_FALSE(pin_index < config->proto.gpio.pin_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Pin index %d out of range (max %d)",
                      pin_index,
                      config->proto.gpio.pin_count - 1);

  gpio_num_t pin = config->proto.gpio.pins[pin_index];

  esp_err_t ret = gpio_set_level(pin, value ? 1 : 0);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set GPIO %d", pin);

  ESP_LOGD(s_tag, "GPIO %d set to %d", pin, value ? 1 : 0);
  return ESP_OK;
}

static esp_err_t internal_star_bus_gpio_read_digital(const star_bus_config_t* config,
                                                     uint8_t                  pin_index,
                                                     uint8_t*                 value)
{
  ESP_RETURN_ON_FALSE(config && value, ESP_ERR_INVALID_ARG, s_tag, "Config or value is NULL");
  ESP_RETURN_ON_FALSE(pin_index < config->proto.gpio.pin_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Pin index %d out of range (max %d)",
                      pin_index,
                      config->proto.gpio.pin_count - 1);

  gpio_num_t pin = config->proto.gpio.pins[pin_index];

  int level = gpio_get_level(pin);
  *value    = (level > 0) ? 1 : 0;

  ESP_LOGD(s_tag, "GPIO %d read as %d", pin, *value);
  return ESP_OK;
}
