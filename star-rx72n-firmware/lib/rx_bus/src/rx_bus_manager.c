/* src/core/rx_bus_manager.c */

/**
 * @file rx_bus_manager.c
 * @brief Bus Manager Skeleton Implementation
 *
 * Skeleton implementation demonstrating Dependency Inversion Principle (DIP).
 * Full bus management functionality will be added in future commits.
 */

#include "rx_bus_manager.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t bus_manager_init(rx_bus_manager_t*        manager,
                          rx_error_interface_t* error_iface,
                          rx_pin_interface_t*   pin_iface)
{
  RX_CHECK_NULL_PTR(manager, "BUS_MANAGER", "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(error_iface, "BUS_MANAGER", "Error interface is NULL");
  RX_CHECK_NULL_PTR(pin_iface, "BUS_MANAGER", "Pin interface is NULL");

  /* Validate interfaces */
  rx_err_t err = rx_error_interface_validate(error_iface);
  if (err != RX_OK) {
    RX_LOG_ERROR("BUS_MANAGER", "Error interface validation failed");
    return err;
  }

  err = rx_pin_interface_validate(pin_iface);
  if (err != RX_OK) {
    RX_LOG_ERROR("BUS_MANAGER", "Pin interface validation failed");
    return err;
  }

  /* Clear manager state */
  memset(manager, 0, sizeof(rx_bus_manager_t));

  /* Store injected interfaces */
  manager->error_iface = error_iface;
  manager->pin_iface   = pin_iface;

  RX_LOG_INFO("BUS_MANAGER", "Bus manager initialized (skeleton)");

  return RX_OK;
}

rx_err_t bus_manager_deinit(rx_bus_manager_t* manager)
{
  RX_CHECK_NULL_PTR(manager, "BUS_MANAGER", "Manager pointer is NULL");

  /* Future: Cleanup bus instances here */

  RX_LOG_INFO("BUS_MANAGER", "Bus manager deinitialized");

  return RX_OK;
}
