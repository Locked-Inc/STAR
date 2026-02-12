/* lib/rx_bus/inc/rx_bus_command.h */

/**
 * @file rx_bus_command.h
 * @brief Command Pattern interface for polymorphic bus operations
 *
 * @details
 * This file implements the **Command design pattern** to encapsulate bus
 * operations (I2C, SPI, GPIO, UART, 1-Wire) as first-class objects. Commands
 * decouple the request for an operation from the execution, enabling flexible
 * operation queueing, logging, and thread-safe execution within the bus manager.
 *
 * ## Purpose and Design Rationale
 *
 * The bus manager handles multiple heterogeneous bus types (I2C, SPI, GPIO,
 * etc.) with different operation semantics. Traditional approach would require
 * switch statements and tight coupling. Command pattern solves this by:
 *
 * **Key benefits**:
 * - **Open/Closed Principle**: Add new bus operations without modifying bus manager
 * - **Dependency Inversion**: High-level bus manager depends on command abstraction
 * - **Testability**: Commands can be unit tested in isolation from hardware
 * - **Thread safety**: Bus manager executes commands under mutex protection
 * - **Flexibility**: Commands can be queued, deferred, logged, or canceled
 *
 * **Design pattern**: Gang of Four Command Pattern
 * ```
 * Client           Invoker (Bus Manager)       Command              Receiver (Bus)
 *   |                     |                        |                      |
 *   |--create command---->|                        |                      |
 *   |                     |                        |                      |
 *   |--execute()--------->|                        |                      |
 *   |                     |---execute()----------->|                      |
 *   |                     |                        |---operate on-------->|
 *   |                     |                        |<--result-------------|
 *   |                     |<--result---------------|                      |
 *   |<--result------------|                        |                      |
 * ```
 *
 * ## System Architecture Context
 *
 * Commands integrate into the STAR bus abstraction hierarchy:
 * ```
 * Application (Motor, Sensor, LED drivers)
 *         ↓ creates command with operation-specific data
 * Command Object (rx_bus_command_t)
 *         ↓ submitted to
 * Bus Manager (rx_bus_manager.c)
 *         ↓ acquires mutex, calls command->execute()
 * Command Execute Function (user-defined)
 *         ↓ validates bus type, performs operation
 * Bus Adapter (rx_bus_i2c, rx_bus_gpio, etc.)
 *         ↓ hardware-specific implementation
 * Hardware (RIIC, PORT, SCI, SPI peripherals)
 * ```
 *
 * Example flow for I2C temperature read:
 * 1. Application creates `i2c_read_command` with device address and buffer
 * 2. Submits command to bus manager via `rx_bus_manager_execute_command()`
 * 3. Bus manager acquires bus mutex (thread safety)
 * 4. Calls `command->execute(bus, data)`
 * 5. Command function validates bus type is I2C
 * 6. Command calls `rx_bus_i2c_read()` adapter
 * 7. Adapter performs hardware I2C transaction
 * 8. Result propagates back through call chain
 * 9. Bus manager releases mutex
 *
 * ## Hardware Requirements
 *
 * N/A - This is pure software abstraction. Commands operate on bus configurations
 * which reference hardware, but this header has no direct hardware dependencies.
 *
 * ## Performance Characteristics
 *
 * - **Memory overhead**: 12 bytes per command structure (function pointer + data pointer + result)
 * - **Execution overhead**: ~50-100 ns @ 240 MHz (function pointer call)
 * - **Thread safety cost**: Mutex acquisition time (~1-2 µs with ThreadX)
 * - **Scalability**: O(1) execution time regardless of number of bus types
 *
 * ## Usage Patterns
 *
 * ### Pattern 1: GPIO Write Command
 * @code
 * // Define operation-specific data
 * typedef struct {
 *     bool value;  // High or low
 * } gpio_write_data_t;
 *
 * // Implement command execution function
 * static rx_err_t gpio_write_execute(rx_bus_config_t* bus, void* data)
 * {
 *     gpio_write_data_t* write_data = (gpio_write_data_t*)data;
 *
 *     // Validate bus type
 *     if (bus->type != k_bus_type_gpio) {
 *         return k_rx_err_invalid_arg;
 *     }
 *
 *     // Perform operation
 *     if (write_data->value) {
 *         return gpio_write_high(bus->proto.gpio.port, bus->proto.gpio.pin);
 *     } else {
 *         return gpio_write_low(bus->proto.gpio.port, bus->proto.gpio.pin);
 *     }
 * }
 *
 * // Use command in application
 * gpio_write_data_t write_data = { .value = true };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_execute, &write_data);
 *
 * rx_err_t err = rx_bus_manager_execute_command(manager, "led", &cmd);
 * if (err != k_rx_ok) {
 *     rx_log_error("LED", "Failed to turn on LED");
 * }
 * @endcode
 *
 * ### Pattern 2: I2C Read Command
 * @code
 * // I2C read operation data
 * typedef struct {
 *     uint8_t  device_addr;
 *     uint8_t  reg_addr;
 *     uint8_t* buffer;
 *     uint32_t length;
 * } i2c_read_data_t;
 *
 * // I2C read execution function
 * static rx_err_t i2c_read_execute(rx_bus_config_t* bus, void* data)
 * {
 *     i2c_read_data_t* read_data = (i2c_read_data_t*)data;
 *
 *     if (bus->type != k_bus_type_i2c) {
 *         return k_rx_err_invalid_arg;
 *     }
 *
 *     return rx_bus_i2c_read_register(bus, read_data->device_addr,
 *                                     read_data->reg_addr,
 *                                     read_data->buffer,
 *                                     read_data->length);
 * }
 *
 * // Use for temperature sensor read
 * uint8_t temp_data[2];
 * i2c_read_data_t read_data = {
 *     .device_addr = 0x48,
 *     .reg_addr = 0x00,
 *     .buffer = temp_data,
 *     .length = 2
 * };
 *
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, i2c_read_execute, &read_data);
 * rx_bus_manager_execute_command(manager, "temp_sensor", &cmd);
 * @endcode
 *
 * ### Pattern 3: Composite Command (Multiple Operations)
 * @code
 * // Execute multiple commands atomically under single mutex acquisition
 * rx_bus_command_t commands[3];
 *
 * // Command 1: Configure motor driver
 * config_data_t config = { .mode = MODE_PWM };
 * rx_bus_command_init(&commands[0], motor_config_execute, &config);
 *
 * // Command 2: Set velocity
 * velocity_data_t vel = { .velocity_mps = 1.0f };
 * rx_bus_command_init(&commands[1], motor_set_velocity_execute, &vel);
 *
 * // Command 3: Enable motor
 * enable_data_t enable = { .enabled = true };
 * rx_bus_command_init(&commands[2], motor_enable_execute, &enable);
 *
 * // Execute atomically
 * for (int i = 0; i < 3; i++) {
 *     rx_err_t err = rx_bus_manager_execute_command(manager, "motor0", &commands[i]);
 *     if (err != k_rx_ok) {
 *         // Rollback on error
 *         break;
 *     }
 * }
 * @endcode
 *
 * @par Module Dependencies
 * - rx_err.h: Error code definitions (k_rx_ok, k_rx_err_*)
 * - rx_bus_config.h: Bus configuration structure (forward declared)
 *
 * @par NASA Power of 10 Compliance
 *
 * | Rule | Compliance | Details |
 * |------|------------|---------|
 * | 1    | [OK]          | No goto, setjmp, recursion |
 * | 2    | [OK]          | No loops (inline init function is O(1)) |
 * | 3    | [OK]          | No dynamic allocation (commands stack-allocated) |
 * | 4    | [OK]          | Inline function is 5 lines (under 60 line limit) |
 * | 5    | [OK]          | Validation in execute functions (user responsibility) |
 * | 6    | [OK]          | Minimal scope (command data encapsulated) |
 * | 7    | [OK]          | Execute function returns must be checked |
 * | 8    | [OK]          | Uses error code enums (k_rx_ok, k_rx_err_*) |
 * | 9    | [WARN]          | **Intentional deviation**: Function pointers for DIP (Rule 9 compliance note) |
 * | 10   | [OK]          | Compiles with -Wall -Wextra -Werror |
 *
 * **Rule 9 Deviation Rationale**: Function pointers enable Dependency Inversion
 * Principle (SOLID), allowing mock implementations for unit testing and decoupling
 * high-level bus manager from low-level hardware operations. This is essential
 * for testability in safety-critical embedded systems.
 *
 * @par SOLID Principles
 *
 * **Single Responsibility**: Command structure has one job - encapsulate a
 * single bus operation with its data and execution logic. Does not handle
 * threading, bus selection, or error recovery (those are bus manager duties).
 *
 * **Open/Closed**: Bus system is open for extension (add new command types)
 * without modification (bus manager code unchanged). New sensors/actuators
 * add new execute functions without touching core infrastructure.
 *
 * **Liskov Substitution**: All commands implement same interface
 * (rx_bus_command_execute_fn). Any command can be substituted for another
 * without breaking bus manager. Mock commands substitute real commands in tests.
 *
 * **Interface Segregation**: Minimal interface - just execute function pointer
 * and data. Callers depend only on what they need. No "fat" interface with
 * unused methods.
 *
 * **Dependency Inversion**: High-level bus manager depends on command
 * abstraction (interface), not on concrete GPIO/I2C/SPI implementations.
 * Low-level adapters implement command interface. Enables mock injection
 * for testing.
 *
 * @see rx_bus_manager.h Bus manager that executes commands
 * @see rx_bus_config.h Bus configuration structures
 * @see rx_bus_i2c.h I2C-specific command implementations
 * @see rx_bus_gpio.h GPIO-specific command implementations
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright MIT License
 */

#pragma once

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
 * @typedef rx_bus_command_execute_fn
 * @brief Function pointer type for bus command execution callbacks
 *
 * @details
 * Defines the signature for command execution functions that encapsulate
 * specific bus operations. This callback is invoked by the bus manager while
 * holding the bus mutex, ensuring thread-safe access to shared bus resources.
 *
 * **Execution context**: Called by bus manager under mutex protection. Thread-safe
 * access to bus hardware guaranteed. Must not block for extended periods to
 * avoid starving other bus operations.
 *
 * **Implementation responsibilities**:
 * 1. Validate bus type matches expected type (I2C, SPI, GPIO, etc.)
 * 2. Cast data pointer to command-specific structure
 * 3. Validate command data parameters
 * 4. Perform bus operation via appropriate adapter (rx_bus_i2c, rx_bus_gpio, etc.)
 * 5. Return error code indicating success/failure
 *
 * **Thread safety contract**:
 * - Bus manager holds mutex during entire execution
 * - Multiple threads can submit commands, bus manager serializes execution
 * - Execute function must not acquire additional bus mutexes (deadlock risk)
 * - Execute function can safely access bus->proto fields without synchronization
 *
 * **Performance expectations**:
 * - **Short-duration operations**: GPIO writes (<1 µs), I2C single byte (<50 µs)
 * - **Medium-duration operations**: I2C multi-byte (<500 µs), SPI transfers (<1 ms)
 * - **Never block indefinitely**: No polling loops, use timeout-based waits
 * - **Mutex hold time**: Target <1 ms to maintain system responsiveness
 *
 * @param[in,out] bus Pointer to bus configuration structure
 *                    - **Type**: rx_bus_config_t*
 *                    - **Valid range**: Must be non-NULL, initialized via rx_bus_config_init()
 *                    - **Constraints**: bus->type field determines available proto union fields
 *                    - **Null handling**: Execute function should validate non-NULL
 *                    - **Lifetime**: Valid for duration of execute call (bus manager manages)
 *                    - **Modification**: Can modify bus state (e.g., error flags) if needed
 *
 * @param[in,out] data Pointer to command-specific data structure
 *                     - **Type**: void* (type-erased, cast to specific type in execute function)
 *                     - **Valid range**: Can be NULL if command needs no data
 *                     - **Constraints**: Execute function defines structure layout and validation
 *                     - **Null handling**: NULL allowed for parameterless commands
 *                     - **Lifetime**: Caller must ensure valid until execute returns
 *                     - **Modification**: Execute function can write output data to this structure
 *
 * @return rx_err_t Error code indicating operation result
 * @retval k_rx_ok Success - operation completed successfully
 * @retval k_rx_err_invalid_arg Bus type mismatch, invalid data pointer, or parameter out of range
 * @retval k_rx_err_invalid_state Bus not initialized, hardware not ready
 * @retval k_rx_err_hw_error Hardware operation failed (NACK, timeout, fault)
 * @retval k_rx_err_null_ptr Required pointer parameter is nullptr
 * @retval k_rx_err_timeout Operation timed out waiting for hardware
 *
 * @par Return Value Usage:
 *
 * | Return Code              | Meaning                        | Caller Action              |
 * |--------------------------|--------------------------------|----------------------------|
 * | k_rx_ok                  | Success                        | Use output data            |
 * | k_rx_err_invalid_arg     | Wrong bus type or bad params   | Check command data         |
 * | k_rx_err_invalid_state   | Bus not ready                  | Initialize bus first       |
 * | k_rx_err_hw_error        | Hardware fault                 | Retry or report error      |
 * | k_rx_err_null_ptr        | NULL pointer                   | Fix code bug               |
 * | k_rx_err_timeout         | Operation timeout              | Retry or check hardware    |
 *
 * @pre bus pointer must be valid and non-NULL
 * @pre bus must be initialized via rx_bus_config_init()
 * @pre Bus manager must hold bus mutex (caller responsibility)
 *
 * @post If k_rx_ok: bus operation completed, output written to data (if applicable)
 * @post If error: bus state unchanged (atomic operation), data may be partially written
 * @post Bus mutex remains held (bus manager releases after execute returns)
 *
 * @note **Thread Safety**: Execute function runs under bus manager mutex protection.
 *       Multiple threads can submit commands concurrently - bus manager serializes.
 *       Execute function must not acquire bus mutex (already held).
 *
 * @note **Re-entrancy**: Not reentrant. Bus manager ensures only one command
 *       executes at a time per bus. Execute function can call other functions
 *       but must not recursively submit commands to same bus (deadlock).
 *
 * @note **Performance**: Keep execution time minimal (<1 ms target). Long
 *       operations block other threads waiting for bus access. Use interrupts
 *       or DMA for long transfers, not polling.
 *
 * @warning **No blocking calls**: Do not call tx_thread_sleep(), tx_mutex_get(),
 *          or other blocking ThreadX APIs from execute function. Bus mutex
 *          held during execution - blocking causes system-wide delays.
 *
 * @warning **Validate bus type**: Always check bus->type matches expected type
 *          before accessing bus->proto union. Accessing wrong union member is
 *          undefined behavior (memory corruption risk).
 *
 * @attention **Data lifetime**: Caller must ensure data pointer valid until
 *            execute returns. Execute function should not store data pointer
 *            for later use (dangling pointer risk).
 *
 * @par Example - GPIO Write Execute Function:
 * @code
 * typedef struct {
 *     bool value;
 * } gpio_write_data_t;
 *
 * static rx_err_t gpio_write_execute(rx_bus_config_t* bus, void* data)
 * {
 *     // Validate parameters
 *     if (bus == nullptr) {
 *         return k_rx_err_null_ptr;
 *     }
 *
 *     if (bus->type != k_bus_type_gpio) {
 *         return k_rx_err_invalid_arg;  // Wrong bus type
 *     }
 *
 *     // Cast to specific data type
 *     gpio_write_data_t* write_data = (gpio_write_data_t*)data;
 *     if (write_data == nullptr) {
 *         return k_rx_err_null_ptr;
 *     }
 *
 *     // Perform operation
 *     if (write_data->value) {
 *         return gpio_write_high(bus->proto.gpio.port, bus->proto.gpio.pin);
 *     } else {
 *         return gpio_write_low(bus->proto.gpio.port, bus->proto.gpio.pin);
 *     }
 * }
 * @endcode
 *
 * @par Example - I2C Read Execute Function:
 * @code
 * typedef struct {
 *     uint8_t device_addr;
 *     uint8_t reg_addr;
 *     uint8_t* buffer;
 *     uint32_t length;
 * } i2c_read_data_t;
 *
 * static rx_err_t i2c_read_execute(rx_bus_config_t* bus, void* data)
 * {
 *     // Validate bus type
 *     if (bus == nullptr || bus->type != k_bus_type_i2c) {
 *         return k_rx_err_invalid_arg;
 *     }
 *
 *     // Cast and validate data
 *     i2c_read_data_t* read_data = (i2c_read_data_t*)data;
 *     if (read_data == nullptr || read_data->buffer == nullptr) {
 *         return k_rx_err_null_ptr;
 *     }
 *
 *     // Perform I2C operation
 *     return rx_bus_i2c_read_register(bus, read_data->device_addr,
 *                                     read_data->reg_addr,
 *                                     read_data->buffer,
 *                                     read_data->length);
 * }
 * @endcode
 *
 * @see rx_bus_command_t Command structure using this function pointer
 * @see rx_bus_config_t Bus configuration structure passed to execute
 * @see rx_bus_manager_execute_command() Bus manager function that calls execute
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 9** [WARN] **Intentional deviation**: Uses function pointers for
 *   Dependency Inversion Principle (SOLID). Enables mock implementations for
 *   unit testing and decouples bus manager from hardware-specific adapters.
 *   Essential for testability in safety-critical systems.
 */
typedef rx_err_t (*rx_bus_command_execute_fn)(rx_bus_config_t* bus, void* data);

/**
 * @struct rx_bus_command
 * @brief Generic bus command encapsulating an operation using Command pattern
 *
 * @details
 * Represents a single bus operation (read, write, configure) as a first-class
 * object following the Gang of Four Command design pattern. Commands decouple
 * the request for an operation from its execution, enabling flexible operation
 * queueing, thread-safe execution, and testability.
 *
 * **Design pattern role**: Command object in Command pattern
 * - **Invoker**: Bus manager (rx_bus_manager.c)
 * - **Command**: This structure (rx_bus_command_t)
 * - **Receiver**: Bus adapter (rx_bus_i2c, rx_bus_gpio, etc.)
 * - **Client**: Application code (motor driver, sensor driver, etc.)
 *
 * **Lifetime and ownership**:
 * - Commands are typically stack-allocated by caller
 * - Execute function pointer must remain valid for command lifetime
 * - Data pointer must remain valid until command completes
 * - Bus manager does not copy or take ownership of command
 *
 * **Memory layout**:
 *
 * | Offset | Size | Field   | Type                        | Alignment |
 * |--------|------|---------|-----------------------------|-----------|
 * | 0      | 4    | execute | rx_bus_command_execute_fn   | 4         |
 * | 4      | 4    | data    | void*                       | 4         |
 * | 8      | 4    | result  | rx_err_t                    | 4         |
 * | **Total** | **12** | | | |
 *
 * **Usage workflow**:
 * 1. Define command-specific data structure for parameters/results
 * 2. Implement execute function matching rx_bus_command_execute_fn signature
 * 3. Stack-allocate rx_bus_command_t
 * 4. Initialize via rx_bus_command_init()
 * 5. Submit to bus manager via rx_bus_manager_execute_command()
 * 6. Bus manager calls execute() under mutex protection
 * 7. Check result field for execution outcome
 *
 * @par Field Constraints:
 *
 * | Field   | Constraint                                  |
 * |---------|---------------------------------------------|
 * | execute | Must be non-NULL function pointer           |
 * | data    | Can be NULL if command needs no parameters  |
 * | result  | Written by bus manager after execution      |
 *
 * @invariant execute pointer must be valid function when command is executed
 * @invariant If data is non-NULL, must point to valid memory until execute returns
 * @invariant result field only valid after rx_bus_manager_execute_command() returns
 *
 * @par Usage Example - GPIO Write Command:
 * @code
 * // 1. Define command data structure
 * typedef struct {
 *     bool value;  // GPIO high or low
 * } gpio_write_data_t;
 *
 * // 2. Implement execute function
 * static rx_err_t gpio_write_execute(rx_bus_config_t* bus, void* data)
 * {
 *     if (bus->type != k_bus_type_gpio) {
 *         return k_rx_err_invalid_arg;
 *     }
 *
 *     gpio_write_data_t* write_data = (gpio_write_data_t*)data;
 *     return write_data->value ?
 *         gpio_write_high(bus->proto.gpio.port, bus->proto.gpio.pin) :
 *         gpio_write_low(bus->proto.gpio.port, bus->proto.gpio.pin);
 * }
 *
 * // 3. Use command in application
 * gpio_write_data_t write_data = { .value = true };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_execute, &write_data);
 *
 * // 4. Execute via bus manager
 * rx_err_t err = rx_bus_manager_execute_command(manager, "led", &cmd);
 * if (err != k_rx_ok) {
 *     rx_log_error("LED", "Failed to control LED: %d", err);
 * }
 *
 * // 5. Check result (same as return value for immediate execution)
 * if (cmd.result != k_rx_ok) {
 *     rx_log_error("LED", "Command execution failed: %d", cmd.result);
 * }
 * @endcode
 *
 * @par Usage Example - I2C Read with Output:
 * @code
 * // Command data with input parameters and output buffer
 * typedef struct {
 *     uint8_t  device_addr;  // Input: I2C device address
 *     uint8_t  reg_addr;     // Input: Register to read
 *     uint8_t* buffer;       // Output: Read data written here
 *     uint32_t length;       // Input: Bytes to read
 * } i2c_read_data_t;
 *
 * static rx_err_t i2c_read_execute(rx_bus_config_t* bus, void* data)
 * {
 *     i2c_read_data_t* read_data = (i2c_read_data_t*)data;
 *     return rx_bus_i2c_read_register(bus, read_data->device_addr,
 *                                     read_data->reg_addr,
 *                                     read_data->buffer,
 *                                     read_data->length);
 * }
 *
 * // Read temperature sensor
 * uint8_t temp_buffer[2];
 * i2c_read_data_t read_data = {
 *     .device_addr = 0x48,
 *     .reg_addr = 0x00,
 *     .buffer = temp_buffer,
 *     .length = 2
 * };
 *
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, i2c_read_execute, &read_data);
 * rx_bus_manager_execute_command(manager, "temp_sensor", &cmd);
 *
 * // temp_buffer now contains sensor data
 * int16_t temperature = (temp_buffer[0] << 8) | temp_buffer[1];
 * @endcode
 *
 * @par Usage Example - Parameterless Command:
 * @code
 * // Reset command needs no parameters
 * static rx_err_t device_reset_execute(rx_bus_config_t* bus, void* data)
 * {
 *     (void)data;  // Unused
 *     return device_perform_reset(bus);
 * }
 *
 * // Execute with NULL data
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, device_reset_execute, nullptr);
 * rx_bus_manager_execute_command(manager, "device", &cmd);
 * @endcode
 *
 * @see rx_bus_command_init() Initialize command structure
 * @see rx_bus_command_execute_fn Function pointer type for execute callback
 * @see rx_bus_manager_execute_command() Execute command via bus manager
 * @see rx_bus_config_t Bus configuration passed to execute function
 *
 * @since Version 1.0.0
 */
typedef struct rx_bus_command {
  /**
   * @brief Pointer to command execution function
   * @details
   * Function implementing the bus operation. Called by bus manager while
   * holding bus mutex. Must validate bus type, cast data pointer, and
   * perform operation via appropriate bus adapter.
   * @par Constraints: Must be non-NULL when command executed
   * @par Thread Safety: Called under bus manager mutex protection
   * @see rx_bus_command_execute_fn Function pointer type documentation
   */
  rx_bus_command_execute_fn execute;

  /**
   * @brief Pointer to command-specific data structure
   * @details
   * Type-erased pointer to structure containing command parameters and
   * output buffers. Execute function casts to appropriate type. Can be
   * NULL for parameterless commands.
   * @par Valid range: NULL allowed, otherwise must point to valid memory
   * @par Lifetime: Must remain valid until command execution completes
   * @par Ownership: Caller retains ownership, bus manager does not copy
   * @par Usage: Input parameters, output buffers, or both
   */
  void* data;

  /**
   * @brief Command execution result code (output)
   * @details
   * Written by bus manager after executing command. Contains same value
   * as execute function return. Initialized to k_rx_ok by rx_bus_command_init(),
   * updated after execution.
   * @par Initial value: k_rx_ok (set by rx_bus_command_init)
   * @par Final value: Result of execute() function call
   * @par Usage: Check after rx_bus_manager_execute_command() returns
   * @par Synchronization: Only valid after command execution completes
   */
  rx_err_t result;
} rx_bus_command_t;

/* =============================================================================
 * Command Initialization
 * =============================================================================
 */

/**
 * @brief Initialize bus command structure for execution
 *
 * @details
 * Prepares a command object for execution by setting the execution function
 * pointer, command-specific data pointer, and initializing the result field.
 * This must be called before submitting command to bus manager.
 *
 * **Algorithm steps**:
 * 1. Set execute function pointer (defines what operation to perform)
 * 2. Set data pointer (parameters and output buffers for operation)
 * 3. Initialize result to k_rx_ok (will be overwritten after execution)
 *
 * **Why inline**: This is a trivial 3-field initialization. Inlining eliminates
 * function call overhead (~20-30 CPU cycles @ 240 MHz) for performance-critical
 * initialization path. Function compiles to 3 store instructions (~3 cycles).
 *
 * **Control flow**:
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="Start"];
 *   set_execute [label="cmd->execute = execute"];
 *   set_data [label="cmd->data = data"];
 *   set_result [label="cmd->result = k_rx_ok"];
 *   done [label="Done"];
 *
 *   start -> set_execute;
 *   set_execute -> set_data;
 *   set_data -> set_result;
 *   set_result -> done;
 * }
 * @enddot
 *
 * @param[out] cmd Pointer to command structure to initialize
 *                 - **Type**: rx_bus_command_t*
 *                 - **Valid range**: Must be non-NULL
 *                 - **Constraints**: Should be uninitialized or reusable command
 *                 - **Null handling**: No validation (caller responsibility, inline function)
 *                 - **Lifetime**: Caller must ensure valid until command execution completes
 *                 - **Output**: All three fields written (execute, data, result)
 *
 * @param[in] execute Function pointer for command execution
 *                    - **Type**: rx_bus_command_execute_fn
 *                    - **Valid range**: Must be non-NULL, valid function pointer
 *                    - **Constraints**: Must match rx_bus_command_execute_fn signature
 *                    - **Null handling**: No validation (caller must ensure non-NULL)
 *                    - **Lifetime**: Function must remain valid for command lifetime
 *
 * @param[in,out] data Pointer to command-specific data structure
 *                     - **Type**: void* (type-erased, cast in execute function)
 *                     - **Valid range**: Can be NULL for parameterless commands
 *                     - **Constraints**: Execute function defines structure layout
 *                     - **Null handling**: NULL allowed, execute function must handle
 *                     - **Lifetime**: Must remain valid until command execution completes
 *                     - **Ownership**: Caller retains ownership (not copied)
 *
 * @pre cmd pointer must be valid (not validated - caller responsibility)
 * @pre execute must be non-NULL valid function pointer
 * @pre If data is non-NULL, must point to valid memory
 *
 * @post cmd->execute set to execute parameter
 * @post cmd->data set to data parameter (can be NULL)
 * @post cmd->result initialized to k_rx_ok
 * @post Command ready for submission to rx_bus_manager_execute_command()
 *
 * @invariant cmd structure fully initialized after return
 * @invariant result field always k_rx_ok after init (overwritten after execution)
 *
 * @note **Thread Safety**: Not thread-safe. Command should be initialized
 *       in single thread before submission to bus manager. Once submitted,
 *       do not modify command until execution completes.
 *
 * @note **Re-entrancy**: Reentrant. Multiple threads can call with different
 *       cmd pointers. No shared state accessed.
 *
 * @note **Performance**: Inlined to 3 store instructions. Execution time:
 *       ~12 ns @ 240 MHz (3 cycles). Zero stack usage (inline).
 *
 * @note **Memory**: No heap allocation. All initialization is stack/register
 *       operations. Command structure is 12 bytes.
 *
 * @warning **No NULL validation**: This inline function does not validate
 *          cmd or execute pointers. Passing NULL causes undefined behavior
 *          (segmentation fault). Caller must ensure valid pointers.
 *
 * @warning **Data lifetime**: Data pointer is stored, not copied. Caller
 *          must ensure data remains valid until command execution completes.
 *          Stack-allocated data must not go out of scope before command finishes.
 *
 * @attention **Reusable commands**: Commands can be reinitialized for reuse.
 *            Call rx_bus_command_init() again with new execute/data pointers
 *            after previous execution completes.
 *
 * @par Example - Basic GPIO Write:
 * @code
 * // Stack-allocate command data
 * gpio_write_data_t write_data = { .value = true };
 *
 * // Stack-allocate command
 * rx_bus_command_t cmd;
 *
 * // Initialize command
 * rx_bus_command_init(&cmd, gpio_write_execute, &write_data);
 *
 * // Execute via bus manager
 * rx_err_t err = rx_bus_manager_execute_command(manager, "led", &cmd);
 * @endcode
 *
 * @par Example - I2C Read with Output Buffer:
 * @code
 * // Prepare command data with output buffer
 * uint8_t temp_buffer[2];
 * i2c_read_data_t read_data = {
 *     .device_addr = 0x48,
 *     .reg_addr = 0x00,
 *     .buffer = temp_buffer,  // Output buffer
 *     .length = 2
 * };
 *
 * // Initialize command
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, i2c_read_execute, &read_data);
 *
 * // Execute - temp_buffer will be filled
 * rx_bus_manager_execute_command(manager, "temp_sensor", &cmd);
 * @endcode
 *
 * @par Example - Parameterless Command:
 * @code
 * // Command needs no parameters
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, device_reset_execute, nullptr);
 *
 * rx_bus_manager_execute_command(manager, "device", &cmd);
 * @endcode
 *
 * @par Example - Command Reuse:
 * @code
 * rx_bus_command_t cmd;
 *
 * // First use: Turn LED on
 * gpio_write_data_t write_on = { .value = true };
 * rx_bus_command_init(&cmd, gpio_write_execute, &write_on);
 * rx_bus_manager_execute_command(manager, "led", &cmd);
 *
 * // Reuse same command: Turn LED off
 * gpio_write_data_t write_off = { .value = false };
 * rx_bus_command_init(&cmd, gpio_write_execute, &write_off);
 * rx_bus_manager_execute_command(manager, "led", &cmd);
 * @endcode
 *
 * @par Example - Array of Commands (Batch Execution):
 * @code
 * // Initialize multiple commands for sequential execution
 * rx_bus_command_t commands[3];
 *
 * config_data_t config = { .mode = MODE_PWM };
 * rx_bus_command_init(&commands[0], motor_config_execute, &config);
 *
 * velocity_data_t vel = { .velocity_mps = 1.0f };
 * rx_bus_command_init(&commands[1], motor_set_velocity_execute, &vel);
 *
 * enable_data_t enable = { .enabled = true };
 * rx_bus_command_init(&commands[2], motor_enable_execute, &enable);
 *
 * // Execute batch
 * for (int i = 0; i < 3; i++) {
 *     rx_bus_manager_execute_command(manager, "motor0", &commands[i]);
 * }
 * @endcode
 *
 * @see rx_bus_command_t Command structure documentation
 * @see rx_bus_command_execute_fn Execute function pointer type
 * @see rx_bus_manager_execute_command() Submit command for execution
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1** [OK] No goto, setjmp, recursion (simple assignment statements)
 * - **Rule 2** [OK] No loops (straight-line code)
 * - **Rule 3** [OK] No dynamic allocation (stack-based initialization)
 * - **Rule 4** [OK] Function is 5 lines (under 60 line limit)
 * - **Rule 5** [WARN] No validation (inline performance optimization, caller validates)
 * - **Rule 6** [OK] Minimal scope (function parameters only)
 * - **Rule 7** [OK] No return value to check (void function)
 * - **Rule 8** [OK] Uses enum constant (k_rx_ok)
 * - **Rule 9** [OK] Single level of dereferencing (cmd->field)
 * - **Rule 10** [OK] Compiles with -Wall -Wextra -Werror
 *
 * @callgraph
 * @callergraph
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
