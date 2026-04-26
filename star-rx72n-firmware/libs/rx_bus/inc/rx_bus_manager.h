/**
 * @file rx_bus_manager.h
 * @brief Unified Bus Manager API for RX72N Multi-Protocol Communication
 *
 * @details
 * # Overview
 *
 * The bus manager provides a **centralized registry** for managing multiple communication
 * protocols (GPIO, ADC, I2C, SPI, UART, 1-Wire) with thread-safe operations using
 * ThreadX mutex protection. It follows the **Dependency Inversion Principle** by accepting
 * injected interfaces for error handling and pin validation.
 *
 * ## Key Features
 *
 * - **Thread-safe**: All operations protected by ThreadX mutex
 * - **Multi-protocol**: Supports 6 different communication protocols
 * - **Testable**: Dependency injection for mocking in unit tests
 * - **Resource tracking**: Prevents double-initialization of shared hardware
 * - **Command pattern**: Extensible operation model via rx_bus_command_t
 *
 * ## Architecture Overview
 *
 * @dot
 * digraph bus_manager_api {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_api {
 *     label="Public API";
 *     style=filled;
 *     fillcolor=lightblue;
 *
 *     init [label="rx_bus_manager_init()"];
 *     deinit [label="rx_bus_manager_deinit()"];
 *     add [label="rx_bus_manager_add_bus()"];
 *     remove [label="rx_bus_manager_remove_bus()"];
 *     find [label="rx_bus_manager_find_bus()"];
 *     withbus [label="rx_bus_manager_with_bus()"];
 *     exec [label="rx_bus_manager_execute_command()"];
 *   }
 *
 *   subgraph cluster_internal {
 *     label="Internal State";
 *     style=filled;
 *     fillcolor=lightyellow;
 *
 *     manager [label="rx_bus_manager_t"];
 *     buses [label="Bus Linked List"];
 *     mutex [label="ThreadX Mutex"];
 *   }
 *
 *   init -> manager [label="Initialize"];
 *   add -> buses [label="Register"];
 *   remove -> buses [label="Unregister"];
 *   find -> buses [label="Lookup"];
 *   withbus -> mutex [label="Lock"];
 *   withbus -> buses [label="Access"];
 *   exec -> withbus [label="Uses"];
 *   deinit -> manager [label="Cleanup"];
 * }
 * @enddot
 *
 * ## Supported Bus Types
 *
 * | Bus Type | RX72N Peripheral | Speed | Use Case |
 * |----------|------------------|-------|----------|
 * | **GPIO** | I/O Ports | N/A | Digital I/O, LEDs, buttons |
 * | **ADC** | S12ADFa | ~1 us/sample | Current sensing, analog inputs |
 * | **I2C** | RIIC | 100-1000 kHz | Sensors, EEPROMs |
 * | **SPI** | RSPI | 1-15 MHz | RPi5 communication, sensors |
 * | **UART** | SCI | 9.6-921.6 kbps | Debug console, RS-485 |
 * | **1-Wire** | GPIO bit-bang | ~15 kbps | Temperature sensors |
 *
 * ## Thread Safety
 *
 * All public API functions are thread-safe. The mutex timeout is
 * k_bus_manager_mutex_timeout_ms (default 1000 ms).
 *
 * **Thread-safe operations:**
 * - rx_bus_manager_add_bus()
 * - rx_bus_manager_remove_bus()
 * - rx_bus_manager_find_bus()
 * - rx_bus_manager_with_bus() - Recommended for bus access
 * - rx_bus_manager_execute_command() - Recommended for operations
 *
 * @msc
 *   width=600;
 *   Thread1, Manager, Mutex, Thread2;
 *
 *   Thread1 => Manager [label="rx_bus_manager_with_bus()"];
 *   Manager => Mutex [label="tx_mutex_get()"];
 *   Mutex >> Manager [label="TX_SUCCESS"];
 *   Manager note Manager [label="Thread1 has lock"];
 *
 *   Thread2 => Manager [label="rx_bus_manager_add_bus()"];
 *   Manager => Mutex [label="tx_mutex_get()"];
 *   Mutex note Mutex [label="Thread2 blocks"];
 *
 *   Manager => Thread1 [label="callback()"];
 *   Thread1 >> Manager [label="k_rx_ok"];
 *   Manager => Mutex [label="tx_mutex_put()"];
 *
 *   Mutex >> Manager [label="TX_SUCCESS"];
 *   Manager note Manager [label="Thread2 gets lock"];
 * @endmsc
 *
 * ## Recommended Access Pattern
 *
 * **Use rx_bus_manager_with_bus() instead of rx_bus_manager_find_bus()**
 * to prevent race conditions where the bus could be removed while in use:
 *
 * @code{.c}
 * // RECOMMENDED: Thread-safe callback pattern
 * rx_err_t read_sensor_callback(rx_bus_config_t* bus, void* ctx) {
 *     uint8_t* data = (uint8_t*)ctx;
 *     return i2c_read(bus, data, 6);
 * }
 *
 * uint8_t sensor_data[6];
 * rx_err_t err = rx_bus_manager_with_bus(&manager, "imu", read_sensor_callback, sensor_data);
 *
 * // AVOID: Race condition possible
 * rx_bus_config_t* bus;
 * rx_bus_manager_find_bus(&manager, "imu", &bus);
 * // Bus could be removed here by another thread!
 * i2c_read(bus, data, 6);  // Potential use-after-free
 * @endcode
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | **Rule 1** | [PASS] | No goto, setjmp, recursion |
 * | **Rule 2** | [PASS] | Loop bounds via k_max_buses |
 * | **Rule 3** | [PASS] | Static allocation (bus pool) |
 * | **Rule 4** | [PASS] | Functions <=60 lines |
 * | **Rule 5** | [PASS] | Pre/post conditions documented |
 * | **Rule 6** | [PASS] | Smallest scope variables |
 * | **Rule 7** | [PASS] | All returns checked |
 * | **Rule 8** | [PASS] | C23 typed enums, no macros |
 * | **Rule 9** | [PASS] | Single-level pointers |
 * | **Rule 10** | [PASS] | -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S - Single Responsibility** | Manager only handles registration/lookup |
 * | **O - Open/Closed** | New protocols via bus config, not manager changes |
 * | **L - Liskov Substitution** | All bus configs interchangeable |
 * | **I - Interface Segregation** | Small focused API (7 functions) |
 * | **D - Dependency Inversion** | error_iface, pin_iface injected |
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=LR;
 *   node [shape=box];
 *
 *   bus_manager [label="rx_bus_manager.h", fillcolor=lightgreen, style=filled];
 *   bus_types [label="rx_bus_types.h"];
 *   bus_command [label="rx_bus_command.h"];
 *   rx_err [label="rx_err.h"];
 *
 *   bus_manager -> bus_types;
 *   bus_manager -> bus_command;
 *   bus_manager -> rx_err;
 * }
 * @enddot
 *
 * @see rx_bus_types.h Type definitions for bus manager
 * @see rx_bus_command.h Command pattern interface
 * @see rx_error_interface.h Error handler abstraction (DIP)
 * @see rx_pin_interface.h Pin validator abstraction (DIP)
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

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
 * @details
 * Creates and initializes a bus manager instance with ThreadX mutex for
 * thread-safe operations. Follows Dependency Inversion Principle by accepting
 * optional error handler and pin validator interfaces.
 *
 * ## Initialization Sequence
 *
 * 1. Validate input parameters (manager, tag non-NULL)
 * 2. Zero-initialize manager structure
 * 3. Create ThreadX mutex with name derived from tag
 * 4. Store injected interfaces (or nullptr for defaults)
 * 5. Initialize resource tracking arrays to false
 *
 * @param[in,out] manager Bus manager instance to initialize. Must point to
 *                        allocated rx_bus_manager_t structure.
 * @param[in] tag Logging tag for debug messages (e.g., "MOTOR", "SENSOR").
 *                Must be non-NULL, typically <=8 characters.
 * @param[in] error_iface Error handler interface for operation failures.
 *                        Can be NULL to use default error handling.
 * @param[in] pin_iface Pin validator interface for GPIO configuration.
 *                      Can be NULL to skip pin validation.
 *
 * @return rx_err_t Initialization status
 *
 * @retval k_rx_ok Manager initialized successfully, ready for use
 * @retval k_rx_err_null_ptr manager or tag parameter is nullptr
 * @retval k_rx_err_threadx ThreadX mutex creation failed (tx_mutex_create)
 *
 * @pre manager points to allocated rx_bus_manager_t structure
 * @pre tag is non-NULL string
 * @pre ThreadX kernel is running (tx_kernel_enter completed)
 *
 * @post Manager ready for rx_bus_manager_add_bus() calls
 * @post Mutex created with timeout k_bus_manager_mutex_timeout_ms
 * @post All resource tracking arrays initialized to false
 * @post bus_count initialized to 0
 *
 * @note Call rx_bus_manager_deinit() when done to release resources
 * @note Safe to call multiple times if deinit called between
 *
 * @warning Not safe to call before ThreadX kernel starts
 * @warning Not safe to call on already-initialized manager
 *
 * @par Thread Safety:
 * Not thread-safe for the same manager instance. Initialize from single thread.
 *
 * @par Example - Basic Initialization:
 * @code{.c}
 * rx_bus_manager_t manager;
 *
 * rx_err_t err = rx_bus_manager_init(&manager, "MOTOR", nullptr, nullptr);
 * if (err != k_rx_ok) {
 *     rx_log_error("MAIN", "Bus manager init failed: %d", err);
 *     return err;
 * }
 *
 * // Manager ready for use...
 * @endcode
 *
 * @par Example - With Injected Interfaces:
 * @code{.c}
 * // Production code with real interfaces
 * rx_bus_manager_t manager;
 * rx_err_t err = rx_bus_manager_init(&manager, "MAIN",
 *                                     &production_error_iface,
 *                                     &production_pin_iface);
 *
 * // Unit test with mock interfaces
 * rx_bus_manager_t test_manager;
 * err = rx_bus_manager_init(&test_manager, "TEST",
 *                           &mock_error_iface,
 *                           &mock_pin_iface);
 * @endcode
 *
 * @see rx_bus_manager_deinit() Cleanup and release resources
 * @see rx_error_interface_t Error handler abstraction
 * @see rx_pin_interface_t Pin validator abstraction
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_manager_init(rx_bus_manager_t*     manager,
                                           const char*           tag,
                                           rx_error_interface_t* error_iface,
                                           rx_pin_interface_t*   pin_iface);

/**
 * @brief Deinitialize bus manager and release all resources
 *
 * @details
 * Performs complete cleanup of bus manager:
 * 1. Removes and deinitializes all registered buses
 * 2. Releases references to all bus configurations
 * 3. Deletes ThreadX mutex
 * 4. Resets all tracking state to initial values
 *
 * @param[in,out] manager Bus manager instance to deinitialize
 *
 * @return rx_err_t Deinitialization status
 *
 * @retval k_rx_ok Manager deinitialized successfully
 * @retval k_rx_err_null_ptr manager parameter is nullptr
 *
 * @pre manager was initialized via rx_bus_manager_init()
 * @pre No other threads are using this manager
 *
 * @post All buses removed and hardware deinitialized
 * @post Mutex deleted
 * @post Manager not safe to use until reinitialized
 *
 * @note Safe to call on partially initialized manager (cleanup what exists)
 * @note Does not free the manager structure itself (caller's responsibility)
 *
 * @warning Call only when no other threads are using manager
 * @warning Manager not usable after this call without reinit
 *
 * @par Thread Safety:
 * Not thread-safe. Ensure no other threads access manager during deinit.
 *
 * @par Example:
 * @code{.c}
 * // Cleanup on shutdown
 * rx_err_t err = rx_bus_manager_deinit(&manager);
 * if (err != k_rx_ok) {
 *     rx_log_warn("MAIN", "Bus manager deinit issue: %d", err);
 * }
 * @endcode
 *
 * @see rx_bus_manager_init() Initialize manager
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager);

/* =============================================================================
 * Bus Registration
 * =============================================================================
 */

/**
 * @brief Register a bus configuration with the manager
 *
 * @details
 * Adds a bus to the manager's registry. The manager stores a reference to the
 * bus_config pointer; caller retains ownership and must ensure the configuration
 * remains valid until removal. The bus hardware is NOT initialized until first
 * access (lazy initialization).
 *
 * ## Registration Process
 *
 * 1. Validate parameters (manager, bus_config, bus_config->name)
 * 2. Acquire mutex
 * 3. Check for duplicate name
 * 4. Check capacity limit (k_max_buses)
 * 5. Add to linked list head
 * 6. Increment bus_count
 * 7. Release mutex
 *
 * @param[in,out] manager Bus manager instance (must be initialized)
 * @param[in] bus_config Bus configuration to add. Caller retains ownership;
 *                       must remain valid until removal. Must have valid name and type fields.
 *
 * @return rx_err_t Registration status
 *
 * @retval k_rx_ok Bus registered successfully
 * @retval k_rx_err_null_ptr manager or bus_config is nullptr
 * @retval k_rx_err_invalid_arg bus_config->name is nullptr or empty
 * @retval k_rx_err_exists Bus with same name already registered
 * @retval k_rx_err_no_mem Maximum buses reached (k_max_buses)
 * @retval k_rx_err_timeout Mutex timeout acquiring lock
 *
 * @pre manager initialized via rx_bus_manager_init()
 * @pre bus_config points to valid allocated configuration
 * @pre bus_config->name is unique non-empty string
 * @pre bus_config->type is valid (< k_bus_type_max)
 *
 * @post Manager holds reference to bus_config (caller retains ownership)
 * @post bus_count incremented
 * @post Bus accessible via rx_bus_manager_find_bus()
 *
 * @note Caller retains ownership - ensure bus_config lifetime exceeds registration
 * @note Hardware init deferred until first access (lazy init)
 *
 * @warning Do not modify bus_config after registration
 * @warning Ensure bus_config outlives registration (use static or module-scope allocation)
 *
 * @par Thread Safety:
 * Thread-safe. Acquires mutex internally.
 *
 * @par Example - Register SPI Bus:
 * @code{.c}
 * // Statically allocate and configure (zero dynamic allocation)
 * static rx_bus_config_t s_rpi5_spi = {
 *     .name = "rpi5_spi",
 *     .type = k_bus_type_spi,
 *     .proto.spi = {
 *         .channel       = k_rspi_channel_0,
 *         .frequency_hz  = k_rspi_freq_10mhz,
 *         .mode          = k_rspi_mode_0,
 *     },
 * };
 *
 * // Register (manager references static config - do not modify after registration)
 * rx_err_t err = rx_bus_manager_add_bus(&manager, &s_rpi5_spi);
 * if (err != k_rx_ok) {
 *     return err;
 * }
 * @endcode
 *
 * @see rx_bus_manager_remove_bus() Unregister bus
 * @see rx_bus_config_t Bus configuration structure
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_manager_add_bus(rx_bus_manager_t* manager,
                                              rx_bus_config_t*  bus_config);

/**
 * @brief Unregister and cleanup a bus by name
 *
 * @details
 * Removes a bus from the manager's registry and deinitializes the hardware
 * (if initialized). The configuration reference is released.
 *
 * ## Removal Process
 *
 * 1. Validate parameters
 * 2. Acquire mutex
 * 3. Find bus in linked list
 * 4. Deinitialize hardware if initialized
 * 5. Remove from linked list
 * 6. Release configuration reference
 * 7. Decrement bus_count
 * 8. Release mutex
 *
 * @param[in,out] manager Bus manager instance
 * @param[in] name Bus name to remove (case-sensitive match)
 *
 * @return rx_err_t Removal status
 *
 * @retval k_rx_ok Bus removed successfully
 * @retval k_rx_err_null_ptr manager or name is nullptr
 * @retval k_rx_err_not_found No bus with given name found
 * @retval k_rx_err_timeout Mutex timeout acquiring lock
 *
 * @pre manager initialized via rx_bus_manager_init()
 * @pre Bus with name exists in registry
 *
 * @post Bus removed from registry
 * @post Hardware deinitialized
 * @post Configuration reference removed
 * @post bus_count decremented
 *
 * @note Hardware is properly deinitialized before removal
 * @note Caller may clear config reference after removal
 *
 * @warning Do not hold references to bus_config after this call
 *
 * @par Thread Safety:
 * Thread-safe. Acquires mutex internally.
 *
 * @par Example:
 * @code{.c}
 * rx_err_t err = rx_bus_manager_remove_bus(&manager, "rpi5_spi");
 * if (err == k_rx_err_not_found) {
 *     rx_log_warn("MAIN", "Bus not found: rpi5_spi");
 * }
 * @endcode
 *
 * @see rx_bus_manager_add_bus() Register bus
 * @see rx_bus_manager_deinit() Remove all buses
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_manager_remove_bus(rx_bus_manager_t* manager, const char* name);

/* =============================================================================
 * Bus Access (Thread-Safe)
 * =============================================================================
 */

/**
 * @brief Find a bus by name (thread-safe lookup)
 *
 * @details
 * Searches the bus registry for a bus with the given name and returns
 * a pointer to its configuration. The lookup is thread-safe, but the
 * returned pointer is NOT protected after the function returns.
 *
 * @warning **Race Condition Risk:** Prefer rx_bus_manager_with_bus() instead.
 * The returned bus_config pointer could become invalid if another thread
 * calls rx_bus_manager_remove_bus() after this function returns.
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name to search for (case-sensitive)
 * @param[out] bus_config Pointer to receive bus configuration address
 *
 * @return rx_err_t Lookup status
 *
 * @retval k_rx_ok Bus found, bus_config points to configuration
 * @retval k_rx_err_null_ptr manager, name, or bus_config is nullptr
 * @retval k_rx_err_not_found No bus with given name exists
 * @retval k_rx_err_timeout Mutex timeout acquiring lock
 *
 * @pre manager initialized via rx_bus_manager_init()
 *
 * @post *bus_config points to found configuration (if k_rx_ok)
 * @post *bus_config unchanged (if error)
 *
 * @note Lookup is O(n) where n = bus_count
 * @warning Returned pointer not protected from concurrent removal
 *
 * @par Thread Safety:
 * Lookup is thread-safe, but returned pointer is NOT protected.
 * Use rx_bus_manager_with_bus() for protected access.
 *
 * @par Example (Simple, but has race condition risk):
 * @code{.c}
 * rx_bus_config_t* bus;
 * rx_err_t err = rx_bus_manager_find_bus(&manager, "imu", &bus);
 * if (err == k_rx_ok) {
 *     // DANGER: bus could be removed by another thread here!
 *     rx_log_info("MAIN", "Found bus: %s, type: %d", bus->name, bus->type);
 * }
 * @endcode
 *
 * @see rx_bus_manager_with_bus() Recommended thread-safe access pattern
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bus_manager_find_bus(rx_bus_manager_t* manager, const char* name, rx_bus_config_t** bus_config);

/**
 * @typedef rx_bus_callback_t
 * @brief Callback function type for rx_bus_manager_with_bus()
 *
 * @details
 * User-provided callback executed while bus is locked (mutex held).
 * Allows safe access to bus configuration and hardware operations.
 *
 * ## Callback Contract
 *
 * - **Called with mutex held**: Bus cannot be removed during execution
 * - **Must complete quickly**: Holding mutex blocks other threads
 * - **Must not call bus manager functions**: Causes deadlock
 * - **Return value propagated**: Error returned to with_bus caller
 *
 * @param[in] bus_config Pointer to bus configuration (guaranteed valid)
 * @param[in] user_ctx User context passed to rx_bus_manager_with_bus()
 *
 * @return rx_err_t Operation status (propagated to caller)
 *
 * @retval k_rx_ok Operation succeeded
 * @retval Other Error code propagated to with_bus caller
 *
 * @warning Do not call rx_bus_manager_* functions from callback (deadlock)
 * @warning Keep callback execution short to minimize mutex hold time
 *
 * @par Example Callback:
 * @code{.c}
 * rx_err_t read_sensor(rx_bus_config_t* bus, void* ctx) {
 *     uint8_t* data = (uint8_t*)ctx;
 *
 *     // Safe to use bus - mutex is held
 *     if (bus->type != k_bus_type_i2c) {
 *         return k_rx_err_invalid_arg;
 *     }
 *
 *     // Perform I2C read
 *     return i2c_read(bus, 0x00, data, 6);
 * }
 * @endcode
 *
 * @see rx_bus_manager_with_bus() Function that invokes this callback
 *
 * @since Version 1.0.0
 */
typedef rx_err_t (*rx_bus_callback_t)(rx_bus_config_t* bus_config, void* user_ctx);

/**
 * @brief Execute callback with bus locked (RECOMMENDED access pattern)
 *
 * @details
 * The **recommended** pattern for thread-safe bus access. Holds the manager
 * mutex while the callback executes, preventing bus removal race conditions.
 *
 * ## Why Use This Instead of find_bus()
 *
 * | Approach | Thread-Safe Lookup | Thread-Safe Use | Race Condition? |
 * |----------|-------------------|-----------------|-----------------|
 * | find_bus() | [PASS] | [FAIL] | Yes - bus can be removed |
 * | **with_bus()** | [PASS] | [PASS] | No - mutex held during callback |
 *
 * ## Execution Flow
 *
 * @msc
 *   width=500;
 *   Caller, WithBus, Mutex, Callback;
 *
 *   Caller => WithBus [label="with_bus(name, callback, ctx)"];
 *   WithBus => Mutex [label="tx_mutex_get()"];
 *   Mutex >> WithBus [label="TX_SUCCESS"];
 *   WithBus => WithBus [label="Find bus by name"];
 *   WithBus => Callback [label="callback(bus, ctx)"];
 *   Callback >> WithBus [label="k_rx_ok"];
 *   WithBus => Mutex [label="tx_mutex_put()"];
 *   WithBus >> Caller [label="k_rx_ok"];
 * @endmsc
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name to access
 * @param[in] callback Function to execute with bus locked
 * @param[in] user_ctx User context passed to callback (can be NULL)
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok Callback executed successfully
 * @retval k_rx_err_null_ptr manager, name, or callback is nullptr
 * @retval k_rx_err_not_found No bus with given name exists
 * @retval k_rx_err_timeout Mutex timeout (k_bus_manager_mutex_timeout_ms)
 * @retval Other Error code returned by callback
 *
 * @pre manager initialized via rx_bus_manager_init()
 * @pre callback is non-NULL function pointer
 *
 * @post Callback executed exactly once (if bus found)
 * @post Mutex released (even if callback fails)
 *
 * @note Callback executed with mutex held - keep it short
 * @note Callback errors propagated to caller
 *
 * @warning Do not call bus manager functions from callback (deadlock)
 * @warning Long-running callbacks block other threads
 *
 * @par Thread Safety:
 * Fully thread-safe. Mutex protects entire callback execution.
 *
 * @par Example - Read Sensor Data:
 * @code{.c}
 * // Callback function
 * rx_err_t read_imu(rx_bus_config_t* bus, void* ctx) {
 *     imu_data_t* data = (imu_data_t*)ctx;
 *     return imu_read_accel_gyro(bus, data);
 * }
 *
 * // Usage
 * imu_data_t sensor_data;
 * rx_err_t err = rx_bus_manager_with_bus(&manager, "imu", read_imu, &sensor_data);
 * if (err == k_rx_ok) {
 *     process_imu_data(&sensor_data);
 * }
 * @endcode
 *
 * @par Example - Write to Motor Driver:
 * @code{.c}
 * typedef struct {
 *     uint8_t duty_cycle;
 *     bool enable;
 * } motor_cmd_t;
 *
 * rx_err_t set_motor(rx_bus_config_t* bus, void* ctx) {
 *     motor_cmd_t* cmd = (motor_cmd_t*)ctx;
 *     return drv8263_set_pwm(bus, cmd->duty_cycle, cmd->enable);
 * }
 *
 * motor_cmd_t cmd = { .duty_cycle = 128, .enable = true };
 * rx_bus_manager_with_bus(&manager, "motor_drv0", set_motor, &cmd);
 * @endcode
 *
 * @see rx_bus_callback_t Callback function signature
 * @see rx_bus_manager_execute_command() Command pattern alternative
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_manager_with_bus(rx_bus_manager_t* manager,
                                               const char*       name,
                                               rx_bus_callback_t callback,
                                               void*             user_ctx);

/* =============================================================================
 * Command Pattern Interface (Recommended for new code)
 * =============================================================================
 */

/**
 * @brief Execute a command on a bus using Command Pattern (RECOMMENDED for extensibility)
 *
 * @details
 * The **recommended** interface for implementing new bus operations following the
 * Open/Closed Principle. New operations can be added as commands without modifying
 * the bus manager internals.
 *
 * ## Why Command Pattern?
 *
 * | Approach | Extensibility | Testability | SOLID Compliance |
 * |----------|---------------|-------------|------------------|
 * | Direct calls | Requires manager changes | Harder to mock | [FAIL] Violates OCP |
 * | Callbacks | Ad-hoc, inconsistent | Moderate | Partial |
 * | **Commands** | New ops = new commands | Easy to mock | [PASS] Full OCP |
 *
 * ## Command Pattern Benefits
 *
 * 1. **Encapsulation**: Operations wrapped as command objects
 * 2. **Extensibility**: New operations without manager changes
 * 3. **Consistency**: Uniform interface for all operations
 * 4. **Thread Safety**: Mutex held during command execution
 * 5. **Testability**: Commands can be mocked/stubbed
 *
 * ## Execution Flow
 *
 * @msc
 *   width=600;
 *   Caller, Manager, Mutex, Command, Hardware;
 *
 *   Caller => Manager [label="execute_command(name, cmd)"];
 *   Manager => Mutex [label="tx_mutex_get()"];
 *   Mutex >> Manager [label="TX_SUCCESS"];
 *   Manager => Manager [label="Find bus by name"];
 *   Manager => Command [label="cmd->execute(bus, cmd->data)"];
 *   Command => Hardware [label="Bus operation"];
 *   Hardware >> Command [label="Result"];
 *   Command >> Manager [label="k_rx_ok"];
 *   Manager => Mutex [label="tx_mutex_put()"];
 *   Manager >> Caller [label="k_rx_ok"];
 * @endmsc
 *
 * @param[in] manager Bus manager instance
 * @param[in] name Bus name to execute command on
 * @param[in,out] command Command to execute. Input: execute function and data.
 *                        Output: result stored in command->result.
 *
 * @return rx_err_t Execution status
 *
 * @retval k_rx_ok Command executed successfully, check command->result
 * @retval k_rx_err_null_ptr manager, name, or command is nullptr
 * @retval k_rx_err_not_found No bus with given name exists
 * @retval k_rx_err_timeout Mutex timeout (k_bus_manager_mutex_timeout_ms)
 * @retval k_rx_err_invalid_arg command->execute is nullptr
 *
 * @pre manager initialized via rx_bus_manager_init()
 * @pre command initialized via rx_bus_command_init()
 * @pre command->execute is non-NULL function pointer
 *
 * @post Command executed exactly once (if bus found)
 * @post command->result contains operation-specific result
 * @post Mutex released (even if command fails)
 *
 * @note Command result stored in command->result, not return value
 * @note Combine return value + command->result for full status
 *
 * @warning Do not call bus manager functions from command (deadlock)
 *
 * @par Thread Safety:
 * Fully thread-safe. Mutex protects command execution.
 *
 * @par Example - GPIO Write Command:
 * @code{.c}
 * // Command data structure
 * typedef struct {
 *     bool value;  // GPIO output value
 * } gpio_write_data_t;
 *
 * // Command execute function
 * rx_err_t gpio_write_execute(rx_bus_config_t* bus, void* data) {
 *     gpio_write_data_t* gpio_data = (gpio_write_data_t*)data;
 *     return gpio_set_output(bus->proto.gpio.pin, gpio_data->value);
 * }
 *
 * // Usage
 * gpio_write_data_t data = { .value = true };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_execute, &data);
 *
 * rx_err_t err = rx_bus_manager_execute_command(&manager, "led_status", &cmd);
 * if (err == k_rx_ok && cmd.result == k_rx_ok) {
 *     rx_log_info("LED", "Status LED turned on");
 * }
 * @endcode
 *
 * @par Example - I2C Read Command:
 * @code{.c}
 * // Command data structure
 * typedef struct {
 *     uint8_t  reg_addr;      // Register to read from
 *     uint8_t* buffer;        // Output buffer
 *     size_t   length;        // Bytes to read
 * } i2c_read_data_t;
 *
 * // Command execute function
 * rx_err_t i2c_read_execute(rx_bus_config_t* bus, void* data) {
 *     i2c_read_data_t* i2c_data = (i2c_read_data_t*)data;
 *     return riic_read_reg(bus, i2c_data->reg_addr,
 *                          i2c_data->buffer, i2c_data->length);
 * }
 *
 * // Usage
 * uint8_t accel_data[6];
 * i2c_read_data_t data = {
 *     .reg_addr = 0x3B,       // MPU6050 ACCEL_XOUT_H
 *     .buffer = accel_data,
 *     .length = 6
 * };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, i2c_read_execute, &data);
 *
 * rx_bus_manager_execute_command(&manager, "imu", &cmd);
 * @endcode
 *
 * @par Example - SPI Transfer Command:
 * @code{.c}
 * // Command data for full-duplex SPI transfer
 * typedef struct {
 *     const uint8_t* tx_data;
 *     uint8_t*       rx_data;
 *     size_t         length;
 * } spi_xfer_data_t;
 *
 * rx_err_t spi_xfer_execute(rx_bus_config_t* bus, void* data) {
 *     spi_xfer_data_t* xfer = (spi_xfer_data_t*)data;
 *     return rspi_transfer(bus, xfer->tx_data, xfer->rx_data, xfer->length);
 * }
 *
 * // Read motor driver status
 * uint8_t tx[] = { 0x80 };  // Read status command
 * uint8_t rx[2];
 * spi_xfer_data_t data = { .tx_data = tx, .rx_data = rx, .length = 2 };
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, spi_xfer_execute, &data);
 *
 * rx_bus_manager_execute_command(&manager, "motor_drv0", &cmd);
 * @endcode
 *
 * @see rx_bus_command_t Command structure
 * @see rx_bus_command_init() Initialize command
 * @see rx_bus_command.h Complete command pattern documentation
 * @see rx_bus_manager_with_bus() Callback-based alternative
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_manager_execute_command(rx_bus_manager_t* manager,
                                                      const char*       name,
                                                      rx_bus_command_t* command);

#ifdef __cplusplus
}
#endif
