/* lib/star_bus/src/star_bus_gpio.c */

#include "star_bus_gpio.h"

#include "driver/gpio.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"

static const char* s_TAG = "STAR_BUS_GPIO";

/* --- Default GPIO Operation Implementations --- */

/**
 * @brief Default implementation for configuring a GPIO pin.
 */
static esp_err_t gpio_configure_default(const star_bus_config_t* config,
                                        gpio_num_t               pin,
                                        gpio_mode_t              mode,
                                        gpio_pull_mode_t         pull)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Config is not GPIO type");
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO bus '%s' not initialized",
                      config->name);

  /* Verify pin is managed by this bus */
  const star_gpio_bus_config_t* gpio_cfg  = &config->proto.gpio;
  bool                          pin_found = false;
  for (uint8_t i = 0; i < gpio_cfg->pin_count; ++i) {
    if (gpio_cfg->pins[i] == pin) {
      pin_found = true;
      break;
    }
  }

  ESP_RETURN_ON_FALSE(pin_found,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Pin %d not managed by GPIO bus '%s'",
                      pin,
                      config->name);

  /* Configure GPIO */
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << pin),
    .mode         = mode,
    .pull_up_en   = (pull == GPIO_PULLUP_ONLY || pull == GPIO_PULLUP_PULLDOWN) ? GPIO_PULLUP_ENABLE
                                                                               : GPIO_PULLUP_DISABLE,
    .pull_down_en = (pull == GPIO_PULLDOWN_ONLY || pull == GPIO_PULLUP_PULLDOWN)
                      ? GPIO_PULLDOWN_ENABLE
                      : GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE, /* Set via set_interrupt function */
  };

  esp_err_t ret = gpio_config(&io_conf);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to configure GPIO pin %d on bus '%s'", pin, config->name);

  ESP_LOGD(s_TAG,
           "Configured GPIO pin %d on bus '%s' (mode=%d, pull=%d)",
           pin,
           config->name,
           mode,
           pull);
  return ESP_OK;
}

/**
 * @brief Default implementation for writing to a GPIO pin.
 */
static esp_err_t gpio_write_default(const star_bus_config_t* config, gpio_num_t pin, uint32_t level)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Config is not GPIO type");
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO bus '%s' not initialized",
                      config->name);

  /* Verify pin is managed by this bus */
  const star_gpio_bus_config_t* gpio_cfg  = &config->proto.gpio;
  bool                          pin_found = false;
  for (uint8_t i = 0; i < gpio_cfg->pin_count; ++i) {
    if (gpio_cfg->pins[i] == pin) {
      pin_found = true;
      break;
    }
  }

  ESP_RETURN_ON_FALSE(pin_found,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Pin %d not managed by GPIO bus '%s'",
                      pin,
                      config->name);

  esp_err_t ret = gpio_set_level(pin, level);
  ESP_RETURN_ON_ERROR(ret,
                      s_TAG,
                      "Failed to write level %lu to GPIO pin %d on bus '%s'",
                      (unsigned long)level,
                      pin,
                      config->name);

  ESP_LOGV(s_TAG,
           "Wrote level %lu to GPIO pin %d on bus '%s'",
           (unsigned long)level,
           pin,
           config->name);
  return ESP_OK;
}

/**
 * @brief Default implementation for reading from a GPIO pin.
 */
static esp_err_t gpio_read_default(const star_bus_config_t* config, gpio_num_t pin, uint32_t* level)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(level, ESP_ERR_INVALID_ARG, s_TAG, "Level pointer is NULL");
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Config is not GPIO type");
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO bus '%s' not initialized",
                      config->name);

  /* Verify pin is managed by this bus */
  const star_gpio_bus_config_t* gpio_cfg  = &config->proto.gpio;
  bool                          pin_found = false;
  for (uint8_t i = 0; i < gpio_cfg->pin_count; ++i) {
    if (gpio_cfg->pins[i] == pin) {
      pin_found = true;
      break;
    }
  }

  ESP_RETURN_ON_FALSE(pin_found,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Pin %d not managed by GPIO bus '%s'",
                      pin,
                      config->name);

  int read_level = gpio_get_level(pin);
  *level         = (uint32_t)read_level;

  ESP_LOGV(s_TAG,
           "Read level %lu from GPIO pin %d on bus '%s'",
           (unsigned long)*level,
           pin,
           config->name);
  return ESP_OK;
}

/**
 * @brief Default implementation for setting GPIO interrupt.
 */
static esp_err_t gpio_set_interrupt_default(const star_bus_config_t* config,
                                            gpio_num_t               pin,
                                            gpio_int_type_t          intr_type,
                                            gpio_isr_t               isr_handler,
                                            void*                    args)
{
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Config is not GPIO type");
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO bus '%s' not initialized",
                      config->name);

  /* Verify pin is managed by this bus */
  star_gpio_bus_config_t* gpio_cfg  = (star_gpio_bus_config_t*)&config->proto.gpio;
  bool                    pin_found = false;
  for (uint8_t i = 0; i < gpio_cfg->pin_count; ++i) {
    if (gpio_cfg->pins[i] == pin) {
      pin_found = true;
      break;
    }
  }

  ESP_RETURN_ON_FALSE(pin_found,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Pin %d not managed by GPIO bus '%s'",
                      pin,
                      config->name);

  esp_err_t ret = ESP_OK;

  /* Install ISR service if not already installed */
  if (!gpio_cfg->isr_installed) {
    ret = gpio_install_isr_service(0);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
      /* ESP_ERR_INVALID_STATE means service already installed (by another bus or component) */
      gpio_cfg->isr_installed = true;
      ret                     = ESP_OK;
      ESP_LOGD(s_TAG, "GPIO ISR service installed for bus '%s'", config->name);
    } else {
      ESP_RETURN_ON_ERROR(ret,
                          s_TAG,
                          "Failed to install GPIO ISR service for bus '%s'",
                          config->name);
    }
  }

  /* Set interrupt type */
  ret = gpio_set_intr_type(pin, intr_type);
  ESP_RETURN_ON_ERROR(ret,
                      s_TAG,
                      "Failed to set interrupt type for GPIO pin %d on bus '%s'",
                      pin,
                      config->name);

  /* Remove existing ISR handler if setting to disable */
  if (intr_type == GPIO_INTR_DISABLE || isr_handler == NULL) {
    ret = gpio_isr_handler_remove(pin);
    /* Ignore error if handler doesn't exist */
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(s_TAG, "Failed to remove ISR handler for pin %d: %s", pin, esp_err_to_name(ret));
    }
    ESP_LOGD(s_TAG, "Disabled interrupt for GPIO pin %d on bus '%s'", pin, config->name);
    return ESP_OK;
  }

  /* Add ISR handler */
  ret = gpio_isr_handler_add(pin, isr_handler, args);
  ESP_RETURN_ON_ERROR(ret,
                      s_TAG,
                      "Failed to add ISR handler for GPIO pin %d on bus '%s'",
                      pin,
                      config->name);

  ESP_LOGD(s_TAG,
           "Set interrupt (type=%d) for GPIO pin %d on bus '%s'",
           intr_type,
           pin,
           config->name);
  return ESP_OK;
}

/* --- Public Functions --- */

void star_bus_gpio_init_default_ops(star_gpio_ops_t* ops)
{
  if (!ops) {
    ESP_LOGE(s_TAG, "Cannot init default GPIO ops: ops pointer is NULL");
    return;
  }

  ops->configure     = gpio_configure_default;
  ops->write         = gpio_write_default;
  ops->read          = gpio_read_default;
  ops->set_interrupt = gpio_set_interrupt_default;

  ESP_LOGD(s_TAG, "GPIO default operations initialized");
}

esp_err_t star_bus_gpio_configure(const star_bus_manager_t* manager,
                                  const char*               name,
                                  gpio_num_t                pin,
                                  gpio_mode_t               mode,
                                  gpio_pull_mode_t          pull)
{
  ESP_RETURN_ON_FALSE(manager, ESP_ERR_INVALID_ARG, s_TAG, "Manager is NULL");
  ESP_RETURN_ON_FALSE(name, ESP_ERR_INVALID_ARG, s_TAG, "Bus name is NULL");

  star_bus_config_t* config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(config, ESP_ERR_NOT_FOUND, s_TAG, "GPIO bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not GPIO type",
                      name);
  ESP_RETURN_ON_FALSE(config->proto.gpio.ops.configure,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO configure operation not set for bus '%s'",
                      name);

  return config->proto.gpio.ops.configure(config, pin, mode, pull);
}

esp_err_t star_bus_gpio_write(const star_bus_manager_t* manager,
                              const char*               name,
                              gpio_num_t                pin,
                              uint32_t                  level)
{
  ESP_RETURN_ON_FALSE(manager, ESP_ERR_INVALID_ARG, s_TAG, "Manager is NULL");
  ESP_RETURN_ON_FALSE(name, ESP_ERR_INVALID_ARG, s_TAG, "Bus name is NULL");

  star_bus_config_t* config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(config, ESP_ERR_NOT_FOUND, s_TAG, "GPIO bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not GPIO type",
                      name);
  ESP_RETURN_ON_FALSE(config->proto.gpio.ops.write,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO write operation not set for bus '%s'",
                      name);

  return config->proto.gpio.ops.write(config, pin, level);
}

esp_err_t star_bus_gpio_read(const star_bus_manager_t* manager,
                             const char*               name,
                             gpio_num_t                pin,
                             uint32_t*                 level)
{
  ESP_RETURN_ON_FALSE(manager, ESP_ERR_INVALID_ARG, s_TAG, "Manager is NULL");
  ESP_RETURN_ON_FALSE(name, ESP_ERR_INVALID_ARG, s_TAG, "Bus name is NULL");
  ESP_RETURN_ON_FALSE(level, ESP_ERR_INVALID_ARG, s_TAG, "Level pointer is NULL");

  star_bus_config_t* config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(config, ESP_ERR_NOT_FOUND, s_TAG, "GPIO bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not GPIO type",
                      name);
  ESP_RETURN_ON_FALSE(config->proto.gpio.ops.read,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO read operation not set for bus '%s'",
                      name);

  return config->proto.gpio.ops.read(config, pin, level);
}

esp_err_t star_bus_gpio_set_interrupt(const star_bus_manager_t* manager,
                                      const char*               name,
                                      gpio_num_t                pin,
                                      gpio_int_type_t           intr_type,
                                      gpio_isr_t                isr_handler,
                                      void*                     args)
{
  ESP_RETURN_ON_FALSE(manager, ESP_ERR_INVALID_ARG, s_TAG, "Manager is NULL");
  ESP_RETURN_ON_FALSE(name, ESP_ERR_INVALID_ARG, s_TAG, "Bus name is NULL");

  star_bus_config_t* config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(config, ESP_ERR_NOT_FOUND, s_TAG, "GPIO bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(config->type == k_star_bus_type_gpio,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not GPIO type",
                      name);
  ESP_RETURN_ON_FALSE(config->proto.gpio.ops.set_interrupt,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "GPIO set_interrupt operation not set for bus '%s'",
                      name);

  return config->proto.gpio.ops.set_interrupt(config, pin, intr_type, isr_handler, args);
}
