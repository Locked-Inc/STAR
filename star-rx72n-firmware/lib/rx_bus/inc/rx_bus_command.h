/* lib/rx_bus/inc/rx_bus_command.h */

/**
 * @file rx_bus_command.h
 * @brief Generic command pattern interface for bus operations
 * @details
 * Implements the Command pattern to encapsulate bus operations as objects.
 * This design improves Open/Closed Principle (OCP) compliance by allowing
 * new bus operations to be added without modifying the bus manager internals.
 *
 * Benefits:
 * - Extensibility: New operations can be added without changing bus manager
 * - Separation of Concerns: Command logic is decoupled from bus management
 * - Testability: Commands can be tested independently
 * - Flexibility: Commands can be queued, logged, or undone
 *
 * Example Usage:
 * @code
 * // Define command-specific data structure
 * typedef struct {
 *   bool value;
 * } gpio_write_data_t;
 *
 * // Implement command execution function
 * static rx_err_t gpio_write_execute(rx_bus_config_t* bus, void* data) {
 *   gpio_write_data_t* write_data = (gpio_write_data_t*)data;
 *
 *   if (bus->type != k_bus_type_gpio) {
 *     return k_rx_err_invalid_arg;
 *   }
 *
 *   if (write_data->value) {
 *     return gpio_write_high(bus->proto.gpio.port, bus->proto.gpio.pin);
 *   } else {
 *     return gpio_write_low(bus->proto.gpio.port, bus->proto.gpio.pin);
 *   }
 * }
 *
 * // Use the command
 * gpio_write_data_t write_data = { .value = true };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_execute, &write_data);
 * rx_err_t err = rx_bus_manager_execute_command(manager, "led", &cmd);
 * @endcode
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BUS_COMMAND_H
#define STAR_RX72N_BUS_COMMAND_H

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration to avoid circular dependency */
typedef struct rx_bus_config rx_bus_config_t;

/* =============================================================================
 * Command Pattern Interface
 * =============================================================================
 */

/**
 * @brief Command execution function pointer
 *
 * This function encapsulates a specific bus operation. It will be called
 * by the bus manager while holding the bus mutex, ensuring thread safety.
 *
 * @param[in,out] bus Bus configuration to operate on
 * @param[in,out] data Command-specific data (can be read/written)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if bus type doesn't match expected type
 * @return k_rx_err_invalid_state if bus is not initialized
 * @return k_rx_err_hw_error if hardware operation fails
 */
typedef rx_err_t (*rx_bus_command_execute_fn)(rx_bus_config_t* bus, void* data);

/**
 * @brief Generic bus command structure
 *
 * Encapsulates a bus operation as a command object following the
 * Command design pattern.
 */
typedef struct rx_bus_command {
  rx_bus_command_execute_fn execute; /**< Command execution function */
  void*                     data;    /**< Command-specific data */
  rx_err_t                  result;  /**< Execution result (output) */
} rx_bus_command_t;

/* =============================================================================
 * Command Initialization
 * =============================================================================
 */

/**
 * @brief Initialize a bus command
 *
 * Prepares a command for execution by setting the execution function
 * and associated data.
 *
 * @param[out] cmd Command structure to initialize
 * @param[in] execute Command execution function
 * @param[in,out] data Command-specific data (NULL allowed if not needed)
 *
 * Example:
 * @code
 * gpio_write_data_t data = { .value = true };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_execute, &data);
 * @endcode
 */
static inline void
rx_bus_command_init(rx_bus_command_t* cmd, rx_bus_command_execute_fn execute, void* data)
{
  cmd->execute = execute;
  cmd->data    = data;
  cmd->result  = k_rx_ok;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_COMMAND_H */
