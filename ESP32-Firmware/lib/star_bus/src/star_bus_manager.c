/* esp32-firmware/components/star_bus/star_bus_manager.c */

#include "star_bus_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h" /* For Kconfig values like timeout */
#include "star_bus_config.h"

/* --- Constants --- */

static const char* s_TAG = "Bus Manager";

/* --- Pin Registration Helpers --- */

/**
 * @brief Helper to register a single pin via the interface
 */
static esp_err_t priv_register_bus_pin(star_pin_interface_t* pin_iface,
                                       gpio_num_t            pin,
                                       const char*           bus_name,
                                       const char*           usage,
                                       bool                  can_be_shared)
{
  if (pin < 0 || pin >= GPIO_NUM_MAX) {
    return ESP_OK; /* Not an error, pin is not used */
  }

  if (!pin_iface || !pin_iface->register_pin) {
    return ESP_OK; /* No interface, skip registration */
  }

  /* Create description: "BusName: Usage" (e.g., "I2C0: SDA") */
  char desc[64];
  snprintf(desc, sizeof(desc), "%s: %s", bus_name, usage);

  return pin_iface->register_pin(pin_iface->ctx, pin, desc, can_be_shared);
}

/**
 * @brief Helper to unregister a single pin via the interface
 */
static esp_err_t priv_unregister_bus_pin(star_pin_interface_t* pin_iface,
                                         gpio_num_t            pin,
                                         const char*           bus_name,
                                         const char*           usage)
{
  if (pin < 0 || pin >= GPIO_NUM_MAX) {
    return ESP_OK; /* Not an error, pin was not used */
  }

  if (!pin_iface || !pin_iface->unregister_pin) {
    return ESP_OK; /* No interface, skip unregistration */
  }

  /* Create same description format as registration */
  char desc[64];
  snprintf(desc, sizeof(desc), "%s: %s", bus_name, usage);

  return pin_iface->unregister_pin(pin_iface->ctx, pin, desc);
}

/**
 * @brief Register all pins for a single bus configuration
 */
static esp_err_t priv_register_config_pins(star_pin_interface_t*    pin_iface,
                                           const star_bus_config_t* config)
{
  if (!config || !config->name) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret         = ESP_OK;
  esp_err_t first_error = ESP_OK;

  switch (config->type) {
    case k_star_bus_type_i2c:
      ret = priv_register_bus_pin(pin_iface,
                                  config->proto.i2c.config.sda_io_num,
                                  config->name,
                                  "I2C SDA",
                                  true); /* I2C pins can be shared */
      if (ret != ESP_OK && first_error == ESP_OK) {
        first_error = ret;
      }

      ret = priv_register_bus_pin(pin_iface,
                                  config->proto.i2c.config.scl_io_num,
                                  config->name,
                                  "I2C SCL",
                                  true); /* I2C pins can be shared */
      if (ret != ESP_OK && first_error == ESP_OK) {
        first_error = ret;
      }
      break;

    case k_star_bus_type_spi:
      /* SPI bus pins (shared across devices on same host) */
      ret = priv_register_bus_pin(pin_iface,
                                  config->proto.spi.copi_io_num,
                                  config->name,
                                  "SPI COPI",
                                  true);
      if (ret != ESP_OK && first_error == ESP_OK) {
        first_error = ret;
      }

      ret = priv_register_bus_pin(pin_iface,
                                  config->proto.spi.cipo_io_num,
                                  config->name,
                                  "SPI CIPO",
                                  true);
      if (ret != ESP_OK && first_error == ESP_OK) {
        first_error = ret;
      }

      ret = priv_register_bus_pin(pin_iface,
                                  config->proto.spi.sclk_io_num,
                                  config->name,
                                  "SPI SCLK",
                                  true);
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;

      /* SPI CS pin is device-specific and NOT shared */
      ret = priv_register_bus_pin(pin_iface,
                                  config->proto.spi.dev_cfg.spics_io_num,
                                  config->name,
                                  "SPI CS",
                                  false); /* CS is not shareable */
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;
      break;

    default:
      ESP_LOGD(s_TAG, "No pin registration for bus type: %d", config->type);
      break;
  }

  return first_error;
}

/**
 * @brief Unregister all pins for a single bus configuration
 */
static esp_err_t priv_unregister_config_pins(star_pin_interface_t*    pin_iface,
                                             const star_bus_config_t* config)
{
  if (!config || !config->name) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret         = ESP_OK;
  esp_err_t first_error = ESP_OK;

  switch (config->type) {
    case k_star_bus_type_i2c:
      ret = priv_unregister_bus_pin(pin_iface,
                                    config->proto.i2c.config.sda_io_num,
                                    config->name,
                                    "I2C SDA");
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;

      ret = priv_unregister_bus_pin(pin_iface,
                                    config->proto.i2c.config.scl_io_num,
                                    config->name,
                                    "I2C SCL");
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;
      break;

    case k_star_bus_type_spi:
      ret =
        priv_unregister_bus_pin(pin_iface, config->proto.spi.copi_io_num, config->name, "SPI COPI");
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;

      ret =
        priv_unregister_bus_pin(pin_iface, config->proto.spi.cipo_io_num, config->name, "SPI CIPO");
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;

      ret =
        priv_unregister_bus_pin(pin_iface, config->proto.spi.sclk_io_num, config->name, "SPI SCLK");
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;

      ret = priv_unregister_bus_pin(pin_iface,
                                    config->proto.spi.dev_cfg.spics_io_num,
                                    config->name,
                                    "SPI CS");
      if (ret != ESP_OK && first_error == ESP_OK)
        first_error = ret;
      break;

    default:
      ESP_LOGD(s_TAG, "No pin unregistration for bus type: %d", config->type);
      break;
  }

  return first_error;
}

/* --- Public Functions --- */

esp_err_t star_bus_manager_init(star_bus_manager_t*     manager,
                                const char*             tag,
                                star_error_interface_t* error_iface,
                                star_pin_interface_t*   pin_iface)
{
  ESP_RETURN_ON_FALSE(manager, ESP_ERR_INVALID_ARG, s_TAG, "Manager pointer is NULL");

  manager->buses       = NULL;
  manager->error_iface = error_iface; /* Can be NULL */
  manager->pin_iface   = pin_iface;   /* Can be NULL */

  /* Set logging tag */
  if (tag && strlen(tag) > 0) {
    /* Use strdup for safety, requires free later */
    /* Cast needed because manager->tag is const char* but strdup returns char* */
    manager->tag = strdup(tag);
    ESP_RETURN_ON_FALSE(manager->tag,
                        ESP_ERR_NO_MEM,
                        s_TAG,
                        "Failed to allocate memory for manager tag");
  } else {
    manager->tag = s_TAG; /* Use the component's default static tag */
  }

  manager->mutex = xSemaphoreCreateMutex();
  if (!manager->mutex) {
    ESP_LOGE(manager->tag, "Failed to create mutex");
    /* Free tag only if it was dynamically allocated */
    if (manager->tag != s_TAG) {
      free((void*)manager->tag); /* Cast needed for free */
      manager->tag = NULL;
    }
    return ESP_ERR_NO_MEM;
  }

  /* Initialize SPI host tracking */
  for (uint32_t i = 0; i < SPI_HOST_MAX; ++i) {
    manager->spi_host_initialized[i] = false;
    manager->spi_device_count[i]     = 0; /* Initialize device count */
  }

  ESP_LOGI(manager->tag, "Initialized");
  return ESP_OK;
}

esp_err_t star_bus_manager_add_bus(star_bus_manager_t* manager, star_bus_config_t* config)
{
  ESP_RETURN_ON_FALSE(manager && config,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager or config pointer is NULL");
  ESP_RETURN_ON_FALSE(config->name && strlen(config->name) > 0,
                      ESP_ERR_INVALID_ARG,
                      manager->tag,
                      "Config name is NULL or empty");
  ESP_RETURN_ON_FALSE(config->type > k_star_bus_type_none && config->type < k_star_bus_type_count,
                      ESP_ERR_INVALID_ARG,
                      manager->tag,
                      "Invalid bus type in config");
  ESP_RETURN_ON_FALSE(manager->mutex,
                      ESP_ERR_INVALID_STATE,
                      manager->tag,
                      "Manager mutex not initialized");

  esp_err_t ret = ESP_OK;

  if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    ESP_LOGE(manager->tag, "Timeout acquiring mutex for adding bus '%s'", config->name);
    return ESP_ERR_TIMEOUT;
  }

  /* Check for duplicate name */
  for (star_bus_config_t* curr = manager->buses; curr; curr = curr->next) {
    if (curr->name && strcmp(curr->name, config->name) == 0) {
      ESP_LOGE(manager->tag, "Bus with name '%s' already exists", config->name);
      ret = ESP_ERR_INVALID_STATE;
      goto add_give_mutex; /* Use goto for single exit point with mutex give */
    }
  }

  /* Initialize the bus hardware *before* adding it to the list */
  /* This ensures the manager's SPI host tracking is updated correctly */
  if (!config->initialized) {
    /* Pass the manager to init for SPI host tracking */
    ret = star_bus_config_init(config, manager);
    if (ret != ESP_OK) {
      ESP_LOGE(manager->tag,
               "Failed to initialize bus hardware for '%s': %s",
               config->name,
               esp_err_to_name(ret));
      /* Record the error for tracking and potential recovery */
      if (manager->error_iface && manager->error_iface->record_error) {
        manager->error_iface->record_error(manager->error_iface->ctx,
                                           ret,
                                           "Bus hardware initialization failed during add_bus",
                                           __FILE__,
                                           __LINE__,
                                           __func__);
      }
      goto add_give_mutex; /* Don't add if init failed */
    }
  }

  /* Add to head of list */
  config->next   = manager->buses;
  manager->buses = config;

  ESP_LOGI(manager->tag,
           "Added bus config: %s (Type: %s)",
           config->name,
           star_bus_type_to_string(config->type));

  /* Register pins with pin validator */
  esp_err_t pin_reg_ret = priv_register_config_pins(manager->pin_iface, config);
  if (pin_reg_ret != ESP_OK) {
    ESP_LOGW(manager->tag,
             "Failed to register pins for bus '%s': %s (bus still added)",
             config->name,
             esp_err_to_name(pin_reg_ret));
    /* Don't fail the add operation, just warn - bus is still functional */
  } else {
    ESP_LOGD(manager->tag, "Pins registered for bus '%s'", config->name);
  }

add_give_mutex:
  xSemaphoreGive(manager->mutex);
  return ret;
}

star_bus_config_t* star_bus_manager_find_bus(const star_bus_manager_t* manager, const char* name)
{
  /* Allow NULL manager or name, just return NULL without logging error */
  if (!manager || !name || strlen(name) == 0 || !manager->mutex) {
    return NULL;
  }

  star_bus_config_t* found_bus = NULL;

  if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    ESP_LOGE(manager->tag, "Timeout acquiring mutex for finding bus '%s'", name);
    return NULL; /* Return NULL on timeout */
  }

  for (star_bus_config_t* curr = manager->buses; curr; curr = curr->next) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      found_bus = curr;
      break;
    }
  }

  xSemaphoreGive(manager->mutex);
  return found_bus;
}

esp_err_t star_bus_manager_with_bus(star_bus_manager_t* manager,
                                    const char*         name,
                                    star_bus_callback_t callback,
                                    void*               user_ctx)
{
  ESP_RETURN_ON_FALSE(manager && name && strlen(name) > 0 && callback,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Invalid arguments to with_bus");
  ESP_RETURN_ON_FALSE(manager->mutex,
                      ESP_ERR_INVALID_STATE,
                      manager->tag,
                      "Manager mutex not initialized");

  if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    ESP_LOGE(manager->tag, "Timeout acquiring mutex for with_bus '%s'", name);
    return ESP_ERR_TIMEOUT;
  }

  /* Find the bus while holding mutex */
  star_bus_config_t* found_bus = NULL;
  for (star_bus_config_t* curr = manager->buses; curr; curr = curr->next) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      found_bus = curr;
      break;
    }
  }

  if (found_bus == NULL) {
    xSemaphoreGive(manager->mutex);
    return ESP_ERR_NOT_FOUND;
  }

  /* Invoke callback while still holding mutex - this prevents use-after-free */
  esp_err_t callback_result = callback(found_bus, user_ctx);

  xSemaphoreGive(manager->mutex);
  return callback_result;
}

esp_err_t star_bus_manager_remove_bus(star_bus_manager_t* manager, const char* name)
{
  ESP_RETURN_ON_FALSE(manager && name && strlen(name) > 0,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager is NULL or name is invalid");
  ESP_RETURN_ON_FALSE(manager->mutex,
                      ESP_ERR_INVALID_STATE,
                      manager->tag,
                      "Manager mutex not initialized");

  star_bus_config_t* to_remove         = NULL;
  star_bus_config_t* prev              = NULL;
  esp_err_t          ret               = ESP_ERR_NOT_FOUND;
  spi_host_device_t  spi_host_to_check = -1;
  bool               free_spi_bus      = false;

  /* Take mutex for the entire critical section to prevent race conditions */
  if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS * 2)) !=
      pdTRUE) {
    ESP_LOGE(manager->tag, "Timeout acquiring mutex for removing bus '%s'", name);
    return ESP_ERR_TIMEOUT;
  }

  /* Find and unlink the bus config */
  for (star_bus_config_t* curr = manager->buses; curr; prev = curr, curr = curr->next) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      to_remove = curr;
      if (prev == NULL) {
        manager->buses = curr->next;
      } else {
        prev->next = curr->next;
      }
      to_remove->next = NULL;
      ret             = ESP_OK;

      /* Handle SPI device count and determine if bus should be freed */
      if (to_remove->type == k_star_bus_type_spi) {
        spi_host_to_check = to_remove->proto.spi.host;
        if (spi_host_to_check >= 0 && spi_host_to_check < SPI_HOST_MAX) {
          if (manager->spi_device_count[spi_host_to_check] > 0) {
            manager->spi_device_count[spi_host_to_check]--;
            ESP_LOGD(manager->tag,
                     "Decremented device count for SPI host %d to %d",
                     spi_host_to_check,
                     manager->spi_device_count[spi_host_to_check]);
          } else {
            ESP_LOGW(manager->tag,
                     "SPI device count for host %d was already 0!",
                     spi_host_to_check);
          }

          /* Determine if we should free the SPI bus - do this while holding mutex */
          if (manager->spi_device_count[spi_host_to_check] == 0 &&
              manager->spi_host_initialized[spi_host_to_check]) {
            free_spi_bus = true;
            /* Pre-emptively mark as uninitialized to prevent new devices from using it */
            manager->spi_host_initialized[spi_host_to_check] = false;
          }
        } else {
          ESP_LOGE(manager->tag,
                   "Invalid SPI host %d found in config '%s'",
                   spi_host_to_check,
                   name);
          spi_host_to_check = -1;
        }
      }
      break;
    }
  }

  if (ret == ESP_ERR_NOT_FOUND) {
    xSemaphoreGive(manager->mutex);
    ESP_LOGW(manager->tag, "Bus '%s' not found for removal", name);
    return ESP_ERR_NOT_FOUND;
  }

  /* Unregister pins while still holding mutex */
  esp_err_t pin_unreg_ret = priv_unregister_config_pins(manager->pin_iface, to_remove);
  if (pin_unreg_ret != ESP_OK) {
    ESP_LOGW(manager->tag,
             "Failed to unregister pins for bus '%s': %s",
             name,
             esp_err_to_name(pin_unreg_ret));
  }

  /* Destroy the config (calls deinit internally) while holding mutex */
  esp_err_t destroy_result = star_bus_config_destroy(to_remove);
  if (destroy_result != ESP_OK) {
    ESP_LOGE(manager->tag,
             "Error destroying bus config '%s': %s",
             name,
             esp_err_to_name(destroy_result));
    ret = destroy_result;
  } else {
    ESP_LOGI(manager->tag, "Removed and destroyed bus config: %s", name);
  }

  /* Free the underlying SPI bus if necessary - still holding mutex */
  if (free_spi_bus) {
    ESP_LOGI(manager->tag, "Freeing SPI bus driver for host %d...", spi_host_to_check);
    esp_err_t free_ret = spi_bus_free(spi_host_to_check);
    if (free_ret != ESP_OK) {
      ESP_LOGE(manager->tag,
               "Failed to free SPI bus for host %d: %s",
               spi_host_to_check,
               esp_err_to_name(free_ret));
      /* Restore initialized flag since free failed */
      manager->spi_host_initialized[spi_host_to_check] = true;
      if (ret == ESP_OK) {
        ret = free_ret;
      }
    } else {
      ESP_LOGI(manager->tag, "SPI bus driver for host %d freed.", spi_host_to_check);
    }
  }

  xSemaphoreGive(manager->mutex);
  return ret;
}

esp_err_t star_bus_manager_deinit(star_bus_manager_t* manager)
{
  ESP_RETURN_ON_FALSE(manager, ESP_ERR_INVALID_ARG, s_TAG, "Manager pointer is NULL");

  bool mutex_taken = false;
  if (manager->mutex != NULL) {
    /* Use a slightly longer timeout for full cleanup */
    if (xSemaphoreTake(manager->mutex,
                       pdMS_TO_TICKS(CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS * 2)) == pdTRUE) {
      mutex_taken = true;
    } else {
      ESP_LOGE(manager->tag, "Timeout acquiring mutex for deinit - cleanup may be incomplete!");
      /* Continue cautiously without mutex */
    }
  } else {
    ESP_LOGW(manager->tag, "Manager mutex was NULL during deinit");
  }

  /* Detach list head while holding mutex to prevent new operations */
  star_bus_config_t* curr = manager->buses;
  manager->buses          = NULL;

  /* Note: We keep the mutex held during cleanup to prevent race conditions.
   * This may block other operations, but deinit should not be called during
   * normal operation - only at shutdown. */

  esp_err_t first_error = ESP_OK;

  ESP_LOGI(manager->tag, "Deinitializing all managed buses...");
  while (curr) {
    star_bus_config_t* next  = curr->next; /* Store next pointer before destroying curr */
    const char* current_name = curr->name ? curr->name : "UNKNOWN"; /* Save name for logging */

    /* Unregister pins before destroying */
    esp_err_t pin_unreg_ret = priv_unregister_config_pins(manager->pin_iface, curr);
    if (pin_unreg_ret != ESP_OK) {
      ESP_LOGW(manager->tag,
               "Failed to unregister pins for bus '%s' during deinit: %s",
               current_name,
               esp_err_to_name(pin_unreg_ret));
    }

    /* Destroy calls deinit internally */
    esp_err_t destroy_result = star_bus_config_destroy(curr); /* curr is valid here */
    if (destroy_result != ESP_OK) {
      ESP_LOGE(manager->tag,
               "Error destroying bus '%s' during manager deinit: %s",
               current_name,
               esp_err_to_name(destroy_result));
      if (first_error == ESP_OK) {
        first_error = destroy_result; /* Record the first error encountered */
      }
    }
    curr = next; /* Move to the next node (original curr pointer is now invalid) */
  }

  /* Free underlying SPI buses that might still be marked as initialized */
  for (uint32_t host = 0; host < SPI_HOST_MAX; ++host) {
    if (manager->spi_host_initialized[host]) {
      ESP_LOGI(manager->tag, "Freeing potentially orphaned SPI bus driver for host %d...", host);
      esp_err_t free_ret = spi_bus_free((spi_host_device_t)host);
      if (free_ret != ESP_OK) {
        ESP_LOGE(manager->tag,
                 "Failed to free SPI bus for host %d during manager deinit: %s",
                 host,
                 esp_err_to_name(free_ret));
        if (first_error == ESP_OK) {
          first_error = free_ret;
        }
      }
      manager->spi_host_initialized[host] = false;
      manager->spi_device_count[host]     = 0;
    }
  }

  /* Free the tag string if it was dynamically allocated */
  if (manager->tag != NULL && manager->tag != s_TAG) {
    free((void*)manager->tag); /* Cast needed for free */
  }
  manager->tag = NULL;

  /* Clear interface pointers (we don't own them) */
  manager->error_iface = NULL;
  manager->pin_iface   = NULL;

  /* Release and delete mutex */
  if (manager->mutex != NULL) {
    if (mutex_taken) {
      xSemaphoreGive(manager->mutex);
    }
    vSemaphoreDelete(manager->mutex);
    manager->mutex = NULL;
  }

  ESP_LOGI(s_TAG,
           "Manager deinitialized %s",
           (first_error == ESP_OK) ? "successfully" : "with errors");
  return first_error;
}

/* --- Pin Registration --- */

/* Helper function to register a single pin if valid */
static inline esp_err_t register_pin_if_valid(gpio_num_t                  pin,
                                              const char*                 bus_name,
                                              const char*                 usage,
                                              pin_registration_function_t reg_func,
                                              void*                       user_ctx,
                                              bool                        can_be_shared)
{
  if (pin >= 0 && pin < GPIO_NUM_MAX) /* Check if pin number is valid */
  {
    return reg_func(pin, bus_name, usage, user_ctx);
  }
  return ESP_OK; /* Not an error if pin is not used (-1) or invalid */
}

esp_err_t star_bus_manager_register_all_pins(const star_bus_manager_t*   manager,
                                             pin_registration_function_t reg_func,
                                             void*                       user_ctx)
{
  ESP_RETURN_ON_FALSE(manager && reg_func,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager or registration function is NULL");
  ESP_RETURN_ON_FALSE(manager->mutex,
                      ESP_ERR_INVALID_STATE,
                      manager->tag,
                      "Manager mutex not initialized");

  esp_err_t ret        = ESP_OK;
  esp_err_t reg_result = ESP_OK; /* Track registration errors separately */

  if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    ESP_LOGE(manager->tag, "Timeout acquiring mutex for pin registration");
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(manager->tag, "Registering pins for all managed buses...");

  for (star_bus_config_t* curr = manager->buses; curr; curr = curr->next) {
    const char* bus_name = curr->name ? curr->name : "UNKNOWN";
    ESP_LOGD(manager->tag,
             "Processing pins for bus '%s' (Type: %s)",
             bus_name,
             star_bus_type_to_string(curr->type));

    switch (curr->type) {
      case k_star_bus_type_i2c:
        reg_result = register_pin_if_valid(curr->proto.i2c.config.sda_io_num,
                                           bus_name,
                                           "I2C SDA",
                                           reg_func,
                                           user_ctx,
                                           true); /* I2C pins are shareable */
        if (reg_result != ESP_OK && ret == ESP_OK)
          ret = reg_result;

        reg_result = register_pin_if_valid(curr->proto.i2c.config.scl_io_num,
                                           bus_name,
                                           "I2C SCL",
                                           reg_func,
                                           user_ctx,
                                           true); /* I2C pins are shareable */
        if (reg_result != ESP_OK && ret == ESP_OK)
          ret = reg_result;
        break;

      case k_star_bus_type_spi:
        /* SPI bus pins (COPI, CIPO, SCLK) are shared per host */
        /* Use the renamed internal fields for registration */
        reg_result = register_pin_if_valid(curr->proto.spi.copi_io_num,
                                           bus_name,
                                           "SPI COPI", /* Updated description */
                                           reg_func,
                                           user_ctx,
                                           true);
        if (reg_result != ESP_OK && ret == ESP_OK)
          ret = reg_result;

        reg_result = register_pin_if_valid(curr->proto.spi.cipo_io_num,
                                           bus_name,
                                           "SPI CIPO", /* Updated description */
                                           reg_func,
                                           user_ctx,
                                           true);
        if (reg_result != ESP_OK && ret == ESP_OK)
          ret = reg_result;

        reg_result = register_pin_if_valid(curr->proto.spi.sclk_io_num,
                                           bus_name,
                                           "SPI SCLK",
                                           reg_func,
                                           user_ctx,
                                           true);
        if (reg_result != ESP_OK && ret == ESP_OK)
          ret = reg_result;

        /* SPI CS pin is device-specific and NOT shared */
        reg_result = register_pin_if_valid(curr->proto.spi.dev_cfg.spics_io_num,
                                           bus_name,
                                           "SPI CS",
                                           reg_func,
                                           user_ctx,
                                           false);
        if (reg_result != ESP_OK && ret == ESP_OK)
          ret = reg_result;

        /* Register DC pin if used (often specific to device, not shared) */
        /* Assuming DC pin info might be stored in user context or flags if needed */
        /* For now, we don't have DC pin explicitly in config, HAL registers it */
        break;

      default:
        ESP_LOGW(manager->tag,
                 "Skipping pin registration for unsupported bus type: %d",
                 curr->type);
        break;
    }

    /* Log first error encountered during registration for this bus */
    if (ret != ESP_OK && reg_result != ESP_OK) {
      ESP_LOGE(manager->tag,
               "Pin registration failed for bus '%s' (first error: %s)",
               bus_name,
               esp_err_to_name(ret));
    }
  }

  xSemaphoreGive(manager->mutex);

  if (ret != ESP_OK) {
    ESP_LOGE(manager->tag,
             "Pin registration process completed with errors (first error: %s)",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(manager->tag, "Pin registration process completed.");
  }

  /* Return the first error encountered during registration calls */
  return ret;
}
