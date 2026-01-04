/* tests/mocks/mock_rx_bus_manager.c */

/**
 * @file mock_rx_bus_manager.c
 * @brief Mock bus manager for unit testing
 *
 * @details
 * Provides minimal mock implementation of bus manager for testing
 * device drivers without ThreadX or hardware dependencies.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_bus_manager.h"

/* =============================================================================
 * Mock Bus Manager (Stub Implementation)
 * =============================================================================
 */

rx_err_t rx_bus_manager_init(rx_bus_manager_t* manager,
                              const char*       tag,
                              void*             error_iface,
                              void*             pin_iface)
{
  (void)manager;
  (void)tag;
  (void)error_iface;
  (void)pin_iface;
  return k_rx_ok;
}

rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager)
{
  (void)manager;
  return k_rx_ok;
}
