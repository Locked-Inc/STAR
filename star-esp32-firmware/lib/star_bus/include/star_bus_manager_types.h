/* lib/star_bus/include/star_bus_manager_types.h */

#ifndef STAR_COMPONENT_BUS_MANAGER_TYPES_H
#define STAR_COMPONENT_BUS_MANAGER_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "star_bus_common_types.h"
#include "star_bus_protocol_types.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"

/* --- Structs --- */

/**
 * @brief Bus configuration structure (common part).
 *
 * This structure contains all the configuration data for a specific bus device instance.
 * It serves as a node in a linked list managed by the star_bus_manager.
 */
typedef struct star_bus_config {
  const char*     name; /**< Unique name for this bus/device instance */
  star_bus_type_t type; /**< Type of bus (I2C, SPI, etc.) */
  bool  initialized; /**< Whether this bus/device has been initialized via star_bus_config_init */
  void* handle;      /**< Driver/device handle (e.g., spi_device_handle_t for SPI) */
  void* user_ctx;    /**< User context pointer, passed to callbacks */

  /* Union to hold protocol-specific configuration */
  union {
    star_i2c_bus_config_t   i2c;   /**< I2C-specific configuration */
    star_spi_bus_config_t   spi;   /**< SPI-specific configuration */
    /* Add other bus types here */
  } proto;

  struct star_bus_config* next; /**< Pointer to the next bus config in the manager's list */
} star_bus_config_t;

/**
 * @brief Bus manager structure that maintains a list of bus configurations.
 *
 * Provides thread-safe functions to add, find, remove, and manage the lifecycle
 * of bus/device configurations.
 */
typedef struct star_bus_manager {
  star_bus_config_t*      buses;       /**< Linked list head of bus configurations */
  SemaphoreHandle_t       mutex;       /**< Mutex for thread-safe access to the list */
  const char*             tag;         /**< Tag used for logging by this manager instance */
  star_error_interface_t* error_iface; /**< Error handler interface (injected dependency) */
  star_pin_interface_t*   pin_iface;   /**< Pin validator interface (injected dependency) */
  /* Track initialized SPI hosts and device count per host */
  bool
    spi_host_initialized[SPI_HOST_MAX]; /**< Track which SPI hosts have the bus driver installed */
  int spi_device_count[SPI_HOST_MAX];   /**< Count of active devices per SPI host */
} star_bus_manager_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_MANAGER_TYPES_H */
