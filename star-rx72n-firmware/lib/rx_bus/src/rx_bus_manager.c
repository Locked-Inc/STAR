/* lib/rx_bus/src/rx_bus_manager.c */

/**
 * @file rx_bus_manager.c
 * @brief Bus Manager Skeleton Implementation
 *
 * Skeleton implementation demonstrating Dependency Inversion Principle (DIP).
 * Full bus management functionality will be added in future commits.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_manager.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

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
  rx_bus_command_t* command = (rx_bus_command_t*)user_ctx;

  /* Validate command has execution function */
  if (command->execute == NULL) {
    rx_log_error(s_tag, "Command execute function is NULL");
    command->result = k_rx_err_null_pointer;
    return k_rx_err_null_pointer;
  }

  /* Execute the command */
  rx_err_t err = command->execute(bus_config, command->data);

  /* Store result in command for caller inspection */
  command->result = err;

  return err;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t bus_manager_init(rx_bus_manager_t*     manager,
                          rx_error_interface_t* error_iface,
                          rx_pin_interface_t*   pin_iface)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(error_iface, s_tag, "Error interface is NULL");
  RX_CHECK_NULL_PTR(pin_iface, s_tag, "Pin interface is NULL");

  /* Validate interfaces */
  rx_err_t err = rx_error_interface_validate(error_iface);
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

  /* Store injected interfaces */
  manager->error_iface = error_iface;
  manager->pin_iface   = pin_iface;

  rx_log_info(s_tag, "Bus manager initialized (skeleton)");

  return k_rx_ok;
}

rx_err_t bus_manager_deinit(rx_bus_manager_t* manager)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");

  /* Future: Cleanup bus instances here */

  rx_log_info(s_tag, "Bus manager deinitialized");

  return k_rx_ok;
}

/* =============================================================================
 * Command Pattern Implementation
 * =============================================================================
 */

rx_err_t rx_bus_manager_execute_command(rx_bus_manager_t* manager,
                                        const char*       name,
                                        rx_bus_command_t* command)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");
  RX_CHECK_NULL_PTR(command, s_tag, "command pointer is NULL");

  /* Validate command has execution function */
  if (command->execute == NULL) {
    rx_log_error(s_tag, "Command execute function is NULL");
    return k_rx_err_null_pointer;
  }

  /* Execute command using existing with_bus infrastructure */
  rx_err_t err = rx_bus_manager_with_bus(manager, name, internal_execute_command_callback, command);

  /* Return the error from with_bus (mutex/lookup errors) */
  /* The command execution result is stored in command->result */
  return err;
}
