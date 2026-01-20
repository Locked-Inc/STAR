/* lib/rx_bus/src/rx_bus_manager.c */

/**
 * @file rx_bus_manager.c
 * @brief Bus Manager Implementation
 *
 * Complete implementation of bus manager with thread-safe linked list
 * management using ThreadX mutex protection.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_manager.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"
#include "rx_threadx_config.h"
#include "rx_time_constants.h"

static const char* s_tag = "BUS_MANAGER";

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Adapter callback to execute command via rx_bus_manager_with_bus
 *
 * This internal callback adapts the command pattern interface to work with
 * the existing rx_bus_manager_with_bus callback mechanism.
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (rx_bus_command_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_execute_command_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  rx_bus_command_t* command = NULL;
  rx_err_t          err     = k_rx_err_invalid_state;

  /* Pre-conditions: validate inputs (NASA Rule 5) */
  if (bus_config == NULL) {
    rx_log_error(s_tag, "Bus config is NULL in command callback");
    return k_rx_err_null_ptr;
  }

  if (user_ctx == NULL) {
    rx_log_error(s_tag, "User context (command) is NULL");
    return k_rx_err_null_ptr;
  }

  command = (rx_bus_command_t*)user_ctx;

  /* Validate command has execution function */
  if (command->execute == NULL) {
    rx_log_error(s_tag, "Command execute function is NULL");
    command->result = k_rx_err_null_ptr;
    return k_rx_err_null_ptr;
  }

  /* Execute the command */
  err = command->execute(bus_config, command->data);

  /* Store result in command for caller inspection */
  command->result = err;

  return err;
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
  rx_err_t err;
  UINT     status;

  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(tag, s_tag, "Tag pointer is NULL");
  RX_CHECK_NULL_PTR(error_iface, s_tag, "Error interface is NULL");
  RX_CHECK_NULL_PTR(pin_iface, s_tag, "Pin interface is NULL");

  /* Validate interfaces */
  err = rx_error_interface_validate(error_iface);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Error interface validation failed");
    return err;
  }

  err = rx_pin_interface_validate(pin_iface);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Pin interface validation failed");
    return err;
  }

  /* Clear manager state */
  memset(manager, 0, sizeof(rx_bus_manager_t));

  /* Create ThreadX mutex for thread safety */
  status = tx_mutex_create(&manager->mutex, "BusMgr", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "ThreadX mutex creation failed");
    return k_rx_err_threadx;
  }

  /* Store injected interfaces */
  manager->tag         = tag;
  manager->error_iface = error_iface;
  manager->pin_iface   = pin_iface;
  manager->buses       = NULL;
  manager->bus_count   = 0;

  rx_log_info(s_tag, "Bus manager initialized");

  return k_rx_ok;
}

rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager)
{
  const rx_bus_config_t* bus    = NULL;
  UINT                   status = TX_SUCCESS;

  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");

  /* Remove all buses before destroying mutex */
  while (manager->buses != NULL) {
    bus            = manager->buses;
    manager->buses = bus->next;
    manager->bus_count--;

    /* Note: bus_config memory is owned by caller, we don't free it */
    rx_log_info(s_tag, "Removed bus during deinit");
  }

  /* Post-condition: verify all buses removed (NASA Rule 5) */
  RX_ASSERT(manager->bus_count == 0, "Bus count should be zero after deinit");

  /* Delete ThreadX mutex */
  status = tx_mutex_delete(&manager->mutex);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "ThreadX mutex deletion failed");
    return k_rx_err_threadx;
  }

  /* Clear manager state */
  memset(manager, 0, sizeof(rx_bus_manager_t));

  rx_log_info(s_tag, "Bus manager deinitialized");

  return k_rx_ok;
}

/* =============================================================================
 * Bus Registration
 * =============================================================================
 */

rx_err_t rx_bus_manager_add_bus(rx_bus_manager_t* manager, rx_bus_config_t* bus_config)
{
  ULONG                  timeout_ticks = 0;
  UINT                   status        = TX_SUCCESS;
  const rx_bus_config_t* current       = NULL;

  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_config, s_tag, "Bus config pointer is NULL");
  RX_CHECK_NULL_PTR(bus_config->name, s_tag, "Bus name is NULL");

  /* Check for empty bus name */
  if (bus_config->name[0] == '\0') {
    rx_log_error(s_tag, "Bus name is empty");
    return k_rx_err_invalid_arg;
  }

  /* Convert timeout from ms to ThreadX ticks */
  timeout_ticks = (k_bus_manager_mutex_timeout_ms * k_rx_threadx_tick_rate_hz) / k_rx_ms_per_second;

  /* Lock mutex for thread-safe access */
  status = tx_mutex_get(&manager->mutex, timeout_ticks);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex timeout in add_bus");
    return k_rx_err_timeout;
  }

  /* Check for duplicate name */
  current = manager->buses;
  while (current != NULL) {
    if (strncmp(current->name, bus_config->name, k_max_bus_name_len) == 0) {
      (void)tx_mutex_put(&manager->mutex);
      rx_log_error(s_tag, "Bus with same name already exists");
      return k_rx_err_exists;
    }
    current = current->next;
  }

  /* Check maximum buses limit */
  if (manager->bus_count >= k_max_buses) {
    (void)tx_mutex_put(&manager->mutex);
    rx_log_error(s_tag, "Maximum buses limit reached");
    return k_rx_err_no_mem;
  }

  /* Add to front of linked list */
  bus_config->next = manager->buses;
  manager->buses   = bus_config;
  manager->bus_count++;

  /* Unlock mutex */
  (void)tx_mutex_put(&manager->mutex);

  rx_log_info(s_tag, "Bus added successfully");

  return k_rx_ok;
}

rx_err_t rx_bus_manager_remove_bus(rx_bus_manager_t* manager, const char* name)
{
  ULONG                  timeout_ticks = 0;
  UINT                   status        = TX_SUCCESS;
  const rx_bus_config_t* to_remove     = NULL;

  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "Name pointer is NULL");

  /* Convert timeout from ms to ThreadX ticks */
  timeout_ticks = (k_bus_manager_mutex_timeout_ms * k_rx_threadx_tick_rate_hz) / k_rx_ms_per_second;

  /* Lock mutex for thread-safe access */
  status = tx_mutex_get(&manager->mutex, timeout_ticks);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex timeout in remove_bus");
    return k_rx_err_timeout;
  }

  /* Find bus in linked list */
  rx_bus_config_t** indirect = &manager->buses;
  while (*indirect != NULL) {
    if (strncmp((*indirect)->name, name, k_max_bus_name_len) == 0) {
      /* Found - remove from list */
      to_remove = *indirect;
      *indirect = to_remove->next;
      manager->bus_count--;

      (void)tx_mutex_put(&manager->mutex);

      /* Note: bus_config memory is owned by caller, we don't free it */
      rx_log_info(s_tag, "Bus removed successfully");
      return k_rx_ok;
    }
    indirect = &(*indirect)->next;
  }

  /* Not found */
  (void)tx_mutex_put(&manager->mutex);
  rx_log_error(s_tag, "Bus not found");
  return k_rx_err_not_found;
}

/* =============================================================================
 * Bus Access (Thread-Safe)
 * =============================================================================
 */

rx_err_t
rx_bus_manager_find_bus(rx_bus_manager_t* manager, const char* name, rx_bus_config_t** bus_config)
{
  ULONG            timeout_ticks = 0;
  UINT             status        = TX_SUCCESS;
  rx_bus_config_t* current       = NULL;

  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "Name pointer is NULL");
  RX_CHECK_NULL_PTR(bus_config, s_tag, "Bus config output pointer is NULL");

  /* Convert timeout from ms to ThreadX ticks */
  timeout_ticks = (k_bus_manager_mutex_timeout_ms * k_rx_threadx_tick_rate_hz) / k_rx_ms_per_second;

  /* Lock mutex for thread-safe access */
  status = tx_mutex_get(&manager->mutex, timeout_ticks);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex timeout in find_bus");
    return k_rx_err_timeout;
  }

  /* Search linked list */
  current = manager->buses;
  while (current != NULL) {
    if (strncmp(current->name, name, k_max_bus_name_len) == 0) {
      *bus_config = current;
      (void)tx_mutex_put(&manager->mutex);
      return k_rx_ok;
    }
    current = current->next;
  }

  /* Not found */
  (void)tx_mutex_put(&manager->mutex);
  rx_log_error(s_tag, "Bus not found");
  return k_rx_err_not_found;
}

rx_err_t rx_bus_manager_with_bus(rx_bus_manager_t*       manager,
                                 const char*             name,
                                 const rx_bus_callback_t callback,
                                 void*                   user_ctx)
{
  ULONG            timeout_ticks = 0;
  UINT             status        = TX_SUCCESS;
  rx_bus_config_t* current       = NULL;
  rx_err_t         err           = k_rx_err_invalid_state;

  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "Name pointer is NULL");
  RX_CHECK_NULL_PTR(callback, s_tag, "Callback pointer is NULL");

  /* Convert timeout from ms to ThreadX ticks */
  timeout_ticks = (k_bus_manager_mutex_timeout_ms * k_rx_threadx_tick_rate_hz) / k_rx_ms_per_second;

  /* Lock mutex for thread-safe access */
  status = tx_mutex_get(&manager->mutex, timeout_ticks);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex timeout in with_bus");
    return k_rx_err_timeout;
  }

  /* Find bus */
  current = manager->buses;
  while (current != NULL) {
    if (strncmp(current->name, name, k_max_bus_name_len) == 0) {
      /* Found - execute callback while holding mutex */
      err = callback(current, user_ctx);

      /* Unlock mutex */
      (void)tx_mutex_put(&manager->mutex);

      return err;
    }
    current = current->next;
  }

  /* Not found */
  (void)tx_mutex_put(&manager->mutex);
  rx_log_error(s_tag, "Bus not found");
  return k_rx_err_not_found;
}

/* =============================================================================
 * Command Pattern Implementation
 * =============================================================================
 */

rx_err_t rx_bus_manager_execute_command(rx_bus_manager_t* manager,
                                        const char*       name,
                                        rx_bus_command_t* command)
{
  rx_err_t err = k_rx_err_invalid_state;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");
  RX_CHECK_NULL_PTR(command, s_tag, "command pointer is NULL");

  /* Validate command has execution function */
  if (command->execute == NULL) {
    rx_log_error(s_tag, "Command execute function is NULL");
    return k_rx_err_null_ptr;
  }

  /* Execute command using existing with_bus infrastructure */
  err = rx_bus_manager_with_bus(manager, name, internal_execute_command_callback, command);

  /* Return the error from with_bus (mutex/lookup errors) */
  /* The command execution result is stored in command->result */
  return err;
}
