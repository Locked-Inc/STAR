/**
 * @file star_bus_manager_types.h
 * @brief Bus manager internal type definitions
 * @details
 * This file defines the core data structures used internally by the bus manager to
 * maintain and manage multiple bus configurations.
 *
 * Key types defined:
 * - star_bus_config_t - Bus configuration node with protocol-specific union
 * - star_bus_manager_t - Manager structure with thread-safe linked list and resource tracking
 *
 * The bus manager uses a linked list to store heterogeneous bus configurations
 * (I2C, SPI, GPIO, ADC, OneWire) with thread-safe access via mutex. It tracks
 * shared hardware resources (SPI hosts, ADC units) to prevent conflicts and
 * manage reference counting.
 *
 * This architecture follows Dependency Inversion Principle by injecting error
 * and pin validation interfaces, allowing the manager to be tested independently
 * of concrete error handlers.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_COMPONENT_BUS_MANAGER_TYPES_H
#define STAR_COMPONENT_BUS_MANAGER_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_adc/adc_oneshot.h"
#include "soc/soc_caps.h"
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
    star_i2c_bus_config_t     i2c;     /**< I2C-specific configuration */
    star_spi_bus_config_t     spi;     /**< SPI-specific configuration */
    star_gpio_bus_config_t    gpio;    /**< GPIO-specific configuration */
    star_adc_bus_config_t     adc;     /**< ADC-specific configuration */
    star_onewire_bus_config_t onewire; /**< OneWire-specific configuration */
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
  /* Track ADC unit handles (shared across channels on same unit) */
  adc_oneshot_unit_handle_t adc_unit_handles[SOC_ADC_PERIPH_NUM]; /**< ADC unit handles */
  int adc_channel_count[SOC_ADC_PERIPH_NUM]; /**< Count of active channels per ADC unit */
} star_bus_manager_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_MANAGER_TYPES_H */
