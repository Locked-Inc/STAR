/* tests/mocks/mock_rx_bus_manager.h */

/**
 * @file mock_rx_bus_manager.h
 * @brief Mock bus manager for unit testing
 *
 * @details
 * Provides minimal mock implementation of bus manager for testing
 * device drivers without ThreadX or hardware dependencies.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_BUS_MANAGER_H
#define MOCK_RX_BUS_MANAGER_H

#include "rx_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Forward Declarations (avoid including real headers)
 * =============================================================================
 */

/**
 * @brief Mock bus manager (opaque structure for tests)
 */
typedef struct rx_bus_manager rx_bus_manager_t;

/* NOTE: rx_bus_config_t is forward-declared in rx_bus_command.h
 * Do not redefine it here to avoid conflicts
 */

/* =============================================================================
 * Mock Bus Manager Functions
 * =============================================================================
 */

rx_err_t rx_bus_manager_init(rx_bus_manager_t* manager,
                              const char*       tag,
                              void*             error_iface,
                              void*             pin_iface);

rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_BUS_MANAGER_H */
