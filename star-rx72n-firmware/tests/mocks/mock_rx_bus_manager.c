/* tests/mocks/mock_rx_bus_manager.c */

/**
 * @file mock_rx_bus_manager.c
 * @brief Mock Bus Manager Implementation for Host-Side Testing
 *
 * Provides simplified bus manager functionality for testing bus abstraction
 * layers (rx_bus_uart, rx_bus_i2c, etc.) without full ThreadX integration.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "rx_bus_manager.h"

static const char* s_tag = "MOCK_BUS_MGR";

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_manager_init(rx_bus_manager_t*     manager,
                             const char*           tag,
                             rx_error_interface_t* error_iface,
                             rx_pin_interface_t*   pin_iface)
{
  if (manager == NULL || tag == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Clear manager state */
  memset(manager, 0, sizeof(rx_bus_manager_t));

  /* Store configuration */
  manager->tag         = tag;
  manager->error_iface = error_iface;
  manager->pin_iface   = pin_iface;
  manager->buses       = NULL;
  manager->bus_count   = 0;

  /* Create mutex (mock version - just sets ID) */
  manager->mutex.tx_mutex_id   = k_tx_mutex_magic;
  manager->mutex.tx_mutex_name = (char*)"BusMutex";
  manager->mutex.locked        = false;

  return k_rx_ok;
}

rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager)
{
  if (manager == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Clear all state */
  manager->buses             = NULL;
  manager->bus_count         = 0;
  manager->mutex.tx_mutex_id = 0;

  return k_rx_ok;
}

rx_err_t rx_bus_manager_add_bus(rx_bus_manager_t* manager, rx_bus_config_t* bus_config)
{
  if (manager == NULL || bus_config == NULL) {
    return k_rx_err_null_pointer;
  }

  if (bus_config->name == NULL || bus_config->name[0] == '\0') {
    return k_rx_err_invalid_arg;
  }

  /* Check for duplicate name */
  rx_bus_config_t* current = manager->buses;
  while (current != NULL) {
    if (strcmp(current->name, bus_config->name) == 0) {
      return k_rx_err_exists;
    }
    current = current->next;
  }

  /* Check max buses */
  if (manager->bus_count >= k_max_buses) {
    return k_rx_err_no_mem;
  }

  /* Add to linked list head */
  bus_config->next = manager->buses;
  manager->buses   = bus_config;
  manager->bus_count++;

  return k_rx_ok;
}

rx_err_t rx_bus_manager_remove_bus(rx_bus_manager_t* manager, const char* name)
{
  if (manager == NULL || name == NULL) {
    return k_rx_err_null_pointer;
  }

  rx_bus_config_t** pp = &manager->buses;
  while (*pp != NULL) {
    if (strcmp((*pp)->name, name) == 0) {
      rx_bus_config_t* to_remove = *pp;
      *pp                        = to_remove->next;
      to_remove->next            = NULL;
      manager->bus_count--;
      return k_rx_ok;
    }
    pp = &(*pp)->next;
  }

  return k_rx_err_not_found;
}

rx_err_t
rx_bus_manager_find_bus(rx_bus_manager_t* manager, const char* name, rx_bus_config_t** bus_config)
{
  if (manager == NULL || name == NULL || bus_config == NULL) {
    return k_rx_err_null_pointer;
  }

  rx_bus_config_t* current = manager->buses;
  while (current != NULL) {
    if (strcmp(current->name, name) == 0) {
      *bus_config = current;
      return k_rx_ok;
    }
    current = current->next;
  }

  return k_rx_err_not_found;
}

rx_err_t rx_bus_manager_with_bus(rx_bus_manager_t* manager,
                                 const char*       name,
                                 rx_bus_callback_t callback,
                                 void*             user_ctx)
{
  if (manager == NULL || name == NULL || callback == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Find the bus */
  rx_bus_config_t* bus = NULL;
  rx_err_t         err = rx_bus_manager_find_bus(manager, name, &bus);
  if (err != k_rx_ok) {
    return err;
  }

  /* Mock mutex acquire */
  manager->mutex.locked = true;

  /* Execute callback */
  err = callback(bus, user_ctx);

  /* Mock mutex release */
  manager->mutex.locked = false;

  return err;
}

rx_err_t rx_bus_manager_execute_command(rx_bus_manager_t* manager,
                                        const char*       name,
                                        rx_bus_command_t* command)
{
  if (manager == NULL || name == NULL || command == NULL) {
    return k_rx_err_null_pointer;
  }

  if (command->execute == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Find the bus */
  rx_bus_config_t* bus = NULL;
  rx_err_t         err = rx_bus_manager_find_bus(manager, name, &bus);
  if (err != k_rx_ok) {
    return err;
  }

  /* Execute command */
  command->result = command->execute(bus, command->data);

  return command->result;
}
