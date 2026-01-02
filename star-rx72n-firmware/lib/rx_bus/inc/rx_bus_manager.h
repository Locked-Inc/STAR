/* lib/rx_bus/inc/rx_bus_manager.h */

/**
 * @file rx_bus_manager.h
 * @brief Unified bus manager API for RX72N
 * @details
 * The bus manager provides central registry for managing multiple bus
 * configurations with thread-safe operations using ThreadX mutex.
 *
 * Supports dependency injection of error handlers and pin validators
 * following the Dependency Inversion Principle.
 *
 * Supported bus types:
 * - GPIO (digital I/O)
 * - ADC (analog input via S12ADFa)
 * - I2C (via RIIC peripheral)
 * - SMBUS (I2C variant for fuel gauge)
 * - SPI (via RSPI peripheral)
 * - UART (via SCI peripheral)
 * - 1-Wire (GPIO bit-bang)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BUS_MANAGER_H
#define STAR_RX72N_BUS_MANAGER_H

#include "rx_bus_command.h"
#include "rx_bus_types.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Bus Manager Initialization
 * =============================================================================
 */

/**
 * @brief Initialize bus manager with injected dependencies
 *
 * Creates ThreadX mutex for thread-safe bus access.
 *
 * @param[in,out] manager Bus manager instance to initialize
 * @param[in] tag Logging tag (e.g., "main")
 * @param[in] error_iface Error handler interface (DIP, can be NULL)
 * @param[in] pin_iface Pin validator interface (DIP, can be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or tag is NULL
 * @return k_rx_err_threadx if ThreadX mutex creation fails
 */
rx_err_t rx_bus_manager_init(rx_bus_manager_t*     manager,
                             const char*           tag,
                             rx_error_interface_t* error_iface,
                             rx_pin_interface_t*   pin_iface);

/**
 * @brief Deinitialize bus manager and free all resources
 *
 * Removes all buses and deletes ThreadX mutex.
 *
 * @param[in,out] manager Bus manager instance
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager is NULL
 */
rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager);

/* =============================================================================
 * Bus Registration
 * =============================================================================
 */

/**
 * @brief Add a bus configuration to the manager
 *
 * Takes ownership of the bus_config pointer. The bus will be
 * initialized when first accessed.
 *
 * @param[in,out] manager Bus manager instance
 * @param[in] bus_config Bus configuration to add (manager takes ownership)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_config is NULL
 * @return k_rx_err_invalid_arg if bus name is NULL or empty
 * @return k_rx_err_exists if bus with same name already exists
 * @return k_rx_err_no_mem if maximum buses reached
 */
rx_err_t rx_bus_manager_add_bus(rx_bus_manager_t* manager, rx_bus_config_t* bus_config);

/**
 * @brief Remove a bus from the manager by name
 *
 * Deinitializes the bus and frees the configuration.
 *
 * @param[in,out] manager Bus manager instance
 * @param[in] name Bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or name is NULL
 * @return k_rx_err_not_found if bus not found
 */
rx_err_t rx_bus_manager_remove_bus(rx_bus_manager_t* manager, const char* name);

/* =============================================================================
 * Bus Access (Thread-Safe)
 * =============================================================================
 */

/**
 * @brief Find a bus by name (thread-safe lookup only)
 *
 * WARNING: Prefer rx_bus_manager_with_bus() to avoid race conditions
 * where bus could be removed while in use.
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name
 * @param[out] bus_config Pointer to bus configuration (if found)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any parameter is NULL
 * @return k_rx_err_not_found if bus not found
 */
rx_err_t
rx_bus_manager_find_bus(rx_bus_manager_t* manager, const char* name, rx_bus_config_t** bus_config);

/**
 * @brief Callback function type for rx_bus_manager_with_bus()
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context passed to rx_bus_manager_with_bus()
 *
 * @return k_rx_ok on success, error code on failure
 */
typedef rx_err_t (*rx_bus_callback_t)(rx_bus_config_t* bus_config, void* user_ctx);

/**
 * @brief Execute callback with bus locked (prevents removal during use)
 *
 * Recommended pattern for thread-safe bus access. Holds mutex while
 * callback executes, preventing bus removal race conditions.
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name
 * @param[in] callback Function to execute with bus locked
 * @param[in] user_ctx User context passed to callback
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, name, or callback is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_timeout if mutex timeout
 * @return Error code from callback if callback fails
 */
rx_err_t rx_bus_manager_with_bus(rx_bus_manager_t* manager,
                                 const char*       name,
                                 rx_bus_callback_t callback,
                                 void*             user_ctx);

/* =============================================================================
 * Command Pattern Interface (Recommended for new code)
 * =============================================================================
 */

/**
 * @brief Execute a command on a bus (Command Pattern)
 *
 * This is the RECOMMENDED interface for implementing new bus operations.
 * It provides better OCP compliance by allowing new operations to be added
 * without modifying the bus manager internals.
 *
 * The command pattern approach:
 * 1. Encapsulates operations as command objects
 * 2. Allows operations to be extended without modifying bus manager
 * 3. Provides consistent interface for all bus operations
 * 4. Maintains thread safety via mutex
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name to execute command on
 * @param[in,out] command Command to execute (result stored in command->result)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, name, or command is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_timeout if mutex timeout
 * @return Command execution result (stored in command->result)
 *
 * Example usage:
 * @code
 * // Define command data
 * gpio_write_data_t data = { .value = true };
 *
 * // Create and initialize command
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_execute, &data);
 *
 * // Execute command
 * rx_err_t err = rx_bus_manager_execute_command(manager, "led", &cmd);
 * if (err == k_rx_ok) {
 *   // Command executed, check cmd.result for operation-specific result
 * }
 * @endcode
 *
 * @see rx_bus_command.h for command pattern documentation
 */
rx_err_t rx_bus_manager_execute_command(rx_bus_manager_t* manager,
                                        const char*       name,
                                        rx_bus_command_t* command);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_MANAGER_H */
