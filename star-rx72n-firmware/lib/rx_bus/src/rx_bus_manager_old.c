/* src/rx_bus_manager.c */

/**
 * @file rx_bus_manager.c
 * @brief Bus manager implementation for RX72N
 * @details
 * Thread-safe bus management using ThreadX mutex and linked list.
 * Supports GPIO, ADC, I2C, SMBUS, SPI, UART, and 1-Wire buses.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include <string.h>

#include "rx_bus_manager.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_MGR";

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Internal linked list search (assumes mutex is held)
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name to find
 *
 * @return Pointer to bus config if found, NULL otherwise
 */
static rx_bus_config_t* internal_find_bus(rx_bus_manager_t* manager, const char* name)
{
  rx_bus_config_t* current = manager->buses;

  while (current != NULL) {
    if (strcmp(current->name, name) == 0) {
      return current;
    }
    current = current->next;
  }

  return NULL;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_manager_init(rx_bus_manager_t*     manager,
                             const char*           tag,
                             rx_error_interface_t* error_iface,
                             rx_pin_interface_t*   pin_iface)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(tag, s_tag, "tag pointer is NULL");

  /* Initialize structure */
  memset(manager, 0, sizeof(rx_bus_manager_t));
  manager->tag         = tag;
  manager->error_iface = error_iface;
  manager->pin_iface   = pin_iface;
  manager->buses       = NULL;
  manager->bus_count   = 0;

  /* Create ThreadX mutex */
  UINT status = tx_mutex_create(&manager->mutex, "bus_mgr_mutex", TX_INHERIT);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Failed to create ThreadX mutex");
    return RX_ERR_THREADX;
  }

  RX_LOG_INFO(s_tag, "Bus manager initialized");
  return RX_OK;
}

rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");

  /* Acquire mutex before cleanup */
  UINT status = tx_mutex_get(&manager->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Failed to acquire mutex for deinit");
    return RX_ERR_THREADX;
  }

  /* Free all buses in linked list */
  rx_bus_config_t* current = manager->buses;
  while (current != NULL) {
    rx_bus_config_t* next = current->next;
    /* Note: We don't free the bus_config here as user may have allocated it
     * statically. Caller should remove buses before deinit if dynamic. */
    current = next;
  }

  manager->buses     = NULL;
  manager->bus_count = 0;

  /* Release mutex before deleting it */
  tx_mutex_put(&manager->mutex);

  /* Delete mutex */
  status = tx_mutex_delete(&manager->mutex);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Failed to delete ThreadX mutex");
    return RX_ERR_THREADX;
  }

  RX_LOG_INFO(s_tag, "Bus manager deinitialized");
  return RX_OK;
}

rx_err_t rx_bus_manager_add_bus(rx_bus_manager_t* manager, rx_bus_config_t* bus_config)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_config, s_tag, "bus_config pointer is NULL");
  RX_CHECK_NULL_PTR(bus_config->name, s_tag, "bus_config->name pointer is NULL");

  /* Validate bus name is not empty */
  if (strlen(bus_config->name) == 0) {
    RX_LOG_ERROR(s_tag, "Bus name cannot be empty");
    return RX_ERR_INVALID_ARG;
  }

  /* Acquire mutex */
  UINT status = tx_mutex_get(&manager->mutex, k_bus_manager_mutex_timeout_ms);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Mutex timeout when adding bus");
    return RX_ERR_TIMEOUT;
  }

  /* Check if bus with same name already exists */
  if (internal_find_bus(manager, bus_config->name) != NULL) {
    tx_mutex_put(&manager->mutex);
    RX_LOG_ERROR(s_tag, "Bus already exists");
    return RX_ERR_EXISTS;
  }

  /* Check bus count limit */
  if (manager->bus_count >= k_max_buses) {
    tx_mutex_put(&manager->mutex);
    RX_LOG_ERROR(s_tag, "Maximum buses reached");
    return RX_ERR_NO_MEM;
  }

  /* Add bus to linked list (prepend for O(1) insertion) */
  bus_config->next        = manager->buses;
  bus_config->initialized = false;
  manager->buses          = bus_config;
  manager->bus_count++;

  /* Release mutex */
  tx_mutex_put(&manager->mutex);

  RX_LOG_INFO(s_tag, "Bus added successfully");
  return RX_OK;
}

rx_err_t rx_bus_manager_remove_bus(rx_bus_manager_t* manager, const char* name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Acquire mutex */
  UINT status = tx_mutex_get(&manager->mutex, k_bus_manager_mutex_timeout_ms);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Mutex timeout when removing bus");
    return RX_ERR_TIMEOUT;
  }

  /* Find bus in linked list */
  rx_bus_config_t* current  = manager->buses;
  rx_bus_config_t* previous = NULL;

  while (current != NULL) {
    if (strcmp(current->name, name) == 0) {
      /* Found bus, remove from list */
      if (previous == NULL) {
        /* Removing head */
        manager->buses = current->next;
      } else {
        /* Removing middle or tail */
        previous->next = current->next;
      }

      manager->bus_count--;

      /* Release mutex */
      tx_mutex_put(&manager->mutex);

      RX_LOG_INFO(s_tag, "Bus removed successfully");
      return RX_OK;
    }

    previous = current;
    current  = current->next;
  }

  /* Bus not found */
  tx_mutex_put(&manager->mutex);
  RX_LOG_ERROR(s_tag, "Bus not found");
  return RX_ERR_NOT_FOUND;
}

rx_err_t
rx_bus_manager_find_bus(rx_bus_manager_t* manager, const char* name, rx_bus_config_t** bus_config)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");
  RX_CHECK_NULL_PTR(bus_config, s_tag, "bus_config pointer is NULL");

  /* Acquire mutex */
  UINT status = tx_mutex_get(&manager->mutex, k_bus_manager_mutex_timeout_ms);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Mutex timeout when finding bus");
    return RX_ERR_TIMEOUT;
  }

  /* Search linked list */
  rx_bus_config_t* found = internal_find_bus(manager, name);

  /* Release mutex */
  tx_mutex_put(&manager->mutex);

  if (found == NULL) {
    *bus_config = NULL;
    return RX_ERR_NOT_FOUND;
  }

  *bus_config = found;
  return RX_OK;
}

rx_err_t rx_bus_manager_with_bus(rx_bus_manager_t* manager,
                                 const char*       name,
                                 rx_bus_callback_t callback,
                                 void*             user_ctx)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");
  RX_CHECK_NULL_PTR(callback, s_tag, "callback pointer is NULL");

  /* Acquire mutex */
  UINT status = tx_mutex_get(&manager->mutex, k_bus_manager_mutex_timeout_ms);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Mutex timeout when accessing bus");
    return RX_ERR_TIMEOUT;
  }

  /* Find bus (mutex held, so bus can't be removed during callback) */
  rx_bus_config_t* bus = internal_find_bus(manager, name);
  if (bus == NULL) {
    tx_mutex_put(&manager->mutex);
    return RX_ERR_NOT_FOUND;
  }

  /* Execute callback with mutex held */
  rx_err_t result = callback(bus, user_ctx);

  /* Release mutex */
  tx_mutex_put(&manager->mutex);

  return result;
}
