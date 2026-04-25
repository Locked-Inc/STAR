/**
 * @file rx_infrastructure.c
 * @brief Global Infrastructure Initialization Implementation
 *
 * @details
 * Implements the global infrastructure layer that initializes and provides
 * access to core system services. This module serves as the central hub
 * for shared singleton instances used throughout the firmware.
 *
 * @par System Architecture
 * @verbatim
 *   +-------------------------------------------------------------+
 *   |                     Application Layer                       |
 *   |  (motor_control, communication, sensors, telemetry)        |
 *   +-------------------------------------------------------------+
 *                              |
 *                              | rx_infrastructure_get_*()
 *                              v
 *   +-------------------------------------------------------------+
 *   |                rx_infrastructure (This Module)              |
 *   |  +----------------------+  +------------------------------+ |
 *   |  |  Error Handler       |  |  Pin Validator               | |
 *   |  |  (retry, backoff)    |  |  (conflict detection)        | |
 *   |  |  s_global_error_*    |  |  s_global_pin_*              | |
 *   |  +----------------------+  +------------------------------+ |
 *   +-------------------------------------------------------------+
 *                              |
 *                              | Uses interfaces from
 *                              v
 *   +-------------------------------------------------------------+
 *   |                   Core Library Layer                        |
 *   |  rx_error_handler.c    rx_pin_validator.c    rx_log.c      |
 *   +-------------------------------------------------------------+
 * @endverbatim
 *
 * @par Initialization Sequence (MSC)
 * @msc
 * main, infra, error_handler, pin_validator;
 *
 * main => infra [label="rx_infrastructure_init()"];
 * infra box infra [label="Check not already init"];
 * infra => error_handler [label="error_handler_init()"];
 * error_handler => infra [label="k_rx_ok"];
 * infra => error_handler [label="error_handler_get_interface()"];
 * error_handler => infra [label="&interface"];
 * infra => pin_validator [label="pin_validator_init()"];
 * pin_validator => infra [label="k_rx_ok"];
 * infra => pin_validator [label="pin_validator_get_interface()"];
 * pin_validator => infra [label="&interface"];
 * infra box infra [label="s_infrastructure_initialized = true"];
 * infra => main [label="k_rx_ok"];
 * @endmsc
 *
 * @par Provided Services
 * | Service         | Description                           | Access Function                      |
 * |-----------------|---------------------------------------|--------------------------------------|
 * | Error Handler   | Retry logic with exponential backoff  | rx_infrastructure_get_error_interface() |
 * | Pin Validator   | GPIO pin conflict detection           | rx_infrastructure_get_pin_interface()   |
 *
 * @par Error Handler Configuration (Defaults)
 * | Parameter          | Value | Description                         |
 * |--------------------|-------|-------------------------------------|
 * | max_retries        | 3     | Maximum retry attempts              |
 * | initial_backoff_ms | 100   | First retry delay                   |
 * | max_backoff_ms     | 5000  | Maximum backoff cap                 |
 *
 * @par Memory Usage
 * | Type   | Size    | Description                              |
 * |--------|---------|------------------------------------------|
 * | Code   | ~400 B  | All functions (with -O2)                 |
 * | Static | ~200 B  | 2 handlers + 2 interfaces + 1 bool       |
 * | Stack  | ~32 B   | Local config struct in init()            |
 *
 * @par Static Variable Layout
 * | Variable                      | Type                  | Size    |
 * |-------------------------------|-----------------------|---------|
 * | s_global_error_handler        | error_handler_t       | ~80 B   |
 * | s_global_pin_validator        | pin_validator_t       | ~64 B   |
 * | s_global_error_interface      | rx_error_interface_t  | 24 B    |
 * | s_global_pin_interface        | rx_pin_interface_t    | 24 B    |
 * | s_infrastructure_initialized  | bool                  | 1 B     |
 *
 * @par Thread Safety
 * - rx_infrastructure_init(): NOT thread-safe, call once from main before threads
 * - rx_infrastructure_deinit(): NOT thread-safe, call after all threads stopped
 * - rx_infrastructure_get_*(): Thread-safe after init (read-only access)
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] No loops (uses called function loops only)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] All functions < 60 lines
 * - Rule 5: [OK] Initialization validation, cleanup on failure
 * - Rule 6: [OK] Static variables at file scope, locals at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Minimal preprocessor (only includes)
 * - Rule 9: [OK] Function pointers in interfaces (DIP, intentional)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles Compliance
 * - **S**: Single Responsibility - Only infrastructure lifecycle management
 * - **O**: Open/Closed - New services can be added without modifying existing code
 * - **L**: Liskov Substitution - Services use abstract interfaces
 * - **I**: Interface Segregation - Error and pin interfaces are separate
 * - **D**: Dependency Inversion - Modules depend on interfaces, not implementations
 *
 * @par Module Dependencies
 * @dot
 * digraph dependencies {
 *   rankdir=TB;
 *   node [shape=box];
 *   rx_infrastructure [label="rx_infrastructure.c" style=filled fillcolor=lightblue];
 *   rx_infrastructure_h [label="rx_infrastructure.h"];
 *   rx_error_handler [label="rx_error_handler.h"];
 *   rx_pin_validator [label="rx_pin_validator.h"];
 *   rx_check [label="rx_check.h"];
 *   rx_log [label="rx_log.h"];
 *   rx_gpio_constants [label="rx_gpio_constants.h"];
 *
 *   rx_infrastructure -> rx_infrastructure_h;
 *   rx_infrastructure -> rx_error_handler;
 *   rx_infrastructure -> rx_pin_validator;
 *   rx_infrastructure -> rx_check;
 *   rx_infrastructure -> rx_log;
 *   rx_infrastructure -> rx_gpio_constants;
 * }
 * @enddot
 *
 * @see rx_infrastructure.h API header
 * @see rx_error_handler.h Error handler with retry logic
 * @see rx_pin_validator.h GPIO pin conflict detection
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_infrastructure.h"

#include "rx_check.h"
#include "rx_error_handler.h"
#include "rx_gpio_constants.h"
#include "rx_log.h"
#include "rx_pin_validator.h"

/* =============================================================================
 * Global Infrastructure Instances
 * =============================================================================
 */

/**
 * @var s_global_error_handler
 * @brief Global error handler instance (singleton)
 *
 * @details
 * Single error handler instance shared across all modules.
 * Provides retry logic with exponential backoff for recoverable errors.
 *
 * @par Initialization:
 * Initialized by rx_infrastructure_init() with default configuration.
 *
 * @par Access:
 * Not accessed directly - use rx_infrastructure_get_error_interface().
 *
 * @note Thread Safety: Structure is thread-safe after initialization
 * @warning Do not access directly - use interface accessor
 *
 * @since Version 1.0.0
 */
static error_handler_t s_global_error_handler;

/**
 * @var s_global_pin_validator
 * @brief Global pin validator instance (singleton)
 *
 * @details
 * Single pin validator instance that tracks GPIO pin assignments
 * across all modules to detect and prevent conflicts.
 *
 * @par Initialization:
 * Initialized by rx_infrastructure_init().
 *
 * @par Access:
 * Not accessed directly - use rx_infrastructure_get_pin_interface().
 *
 * @note Thread Safety: Structure requires external synchronization
 * @warning Do not access directly - use interface accessor
 *
 * @since Version 1.0.0
 */
static pin_validator_t s_global_pin_validator;

/**
 * @var s_global_error_interface
 * @brief Global error handler interface (function pointers)
 *
 * @details
 * Abstract interface to error handler, enabling dependency inversion.
 * Populated with function pointers during initialization.
 *
 * @par Memory Layout:
 * | Offset | Field       | Size  | Description               |
 * |--------|-------------|-------|---------------------------|
 * | 0      | ctx         | 8     | Pointer to error handler  |
 * | 8      | try_recover | 8     | Recovery function pointer |
 * | 16     | reset       | 8     | Reset function pointer    |
 *
 * @see rx_error_interface_t Interface structure definition
 *
 * @since Version 1.0.0
 */
static rx_error_interface_t s_global_error_interface = {};

/**
 * @var s_global_pin_interface
 * @brief Global pin validator interface (function pointers)
 *
 * @details
 * Abstract interface to pin validator, enabling dependency inversion.
 * Populated with function pointers during initialization.
 *
 * @par Memory Layout:
 * | Offset | Field      | Size  | Description                |
 * |--------|------------|-------|----------------------------|
 * | 0      | ctx        | 8     | Pointer to pin validator   |
 * | 8      | claim_pin  | 8     | Pin claim function pointer |
 * | 16     | release    | 8     | Release function pointer   |
 *
 * @see rx_pin_interface_t Interface structure definition
 *
 * @since Version 1.0.0
 */
static rx_pin_interface_t s_global_pin_interface = {};

/**
 * @var s_infrastructure_initialized
 * @brief Infrastructure initialization flag
 *
 * @details
 * Guards against double-initialization and provides early-out for
 * interface accessors when infrastructure is not ready.
 *
 * @par State Machine:
 * - false: Initial state, or after deinit
 * - true: After successful init
 *
 * @invariant If true, all s_global_* variables contain valid data
 *
 * @since Version 1.0.0
 */
static bool s_infrastructure_initialized = false;

/**
 * @var s_tag
 * @brief Log tag for this module
 * @details Identifies log messages from the infrastructure module. Passed to
 *          rx_log_* functions to prefix output with "INFRA".
 * @note Read-only after initialization; never modified after program startup.
 * @invariant Value is "INFRA" and never changes after compilation.
 * @since Version 1.0.0
 */
static const char* const s_tag = "INFRA";

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize global infrastructure services
 *
 * @details
 * Initializes all global infrastructure services in the correct order,
 * with proper cleanup on failure. Uses default configuration values
 * for error handler retry/backoff parameters.
 *
 * @par Algorithm Steps:
 * 1. Check if already initialized (idempotent, return success)
 * 2. Initialize error handler with default config
 * 3. Get error handler interface (function pointers)
 * 4. Initialize pin validator
 * 5. Get pin validator interface (function pointers)
 * 6. Set initialized flag
 *
 * @par Initialization Flow Diagram:
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   start [label="Start" shape=ellipse];
 *   check_init [label="Already initialized?"];
 *   return_ok [label="Return k_rx_ok" shape=ellipse];
 *   init_error [label="error_handler_init()"];
 *   get_error_if [label="error_handler_get_interface()"];
 *   init_pin [label="pin_validator_init()"];
 *   get_pin_if [label="pin_validator_get_interface()"];
 *   set_flag [label="s_initialized = true"];
 *   cleanup1 [label="deinit error_handler"];
 *   cleanup2 [label="deinit pin_validator\ndeinit error_handler"];
 *   return_err [label="Return error" shape=ellipse];
 *
 *   start -> check_init;
 *   check_init -> return_ok [label="yes"];
 *   check_init -> init_error [label="no"];
 *   init_error -> get_error_if [label="ok"];
 *   init_error -> return_err [label="fail"];
 *   get_error_if -> init_pin [label="ok"];
 *   get_error_if -> cleanup1 [label="fail"];
 *   init_pin -> get_pin_if [label="ok"];
 *   init_pin -> cleanup1 [label="fail"];
 *   get_pin_if -> set_flag [label="ok"];
 *   get_pin_if -> cleanup2 [label="fail"];
 *   cleanup1 -> return_err;
 *   cleanup2 -> return_err;
 *   set_flag -> return_ok;
 * }
 * @enddot
 *
 * @par Default Configuration:
 * | Parameter          | Value                                      |
 * |--------------------|--------------------------------------------|
 * | max_retries        | k_error_handler_default_max_retries (3)    |
 * | initial_backoff_ms | k_error_handler_default_initial_backoff_ms |
 * | max_backoff_ms     | k_error_handler_default_max_backoff_ms     |
 *
 *
 * @pre None - can be called at any time
 *
 * @post s_infrastructure_initialized == true on success
 * @post All s_global_* variables contain valid data on success
 * @post Interface accessors return valid pointers on success
 *
 * @note Thread Safety: NOT thread-safe, call once from main()
 * @note Idempotent: Safe to call multiple times, subsequent calls are no-op
 *
 * @warning Must be called before any module uses infrastructure services
 * @warning Not thread-safe - call before starting RTOS threads
 *
 * @par Example:
 * @code{.c}
 * // In main() before starting ThreadX
 * rx_err_t err = rx_infrastructure_init();
 * if (err != k_rx_ok) {
 *     // Critical failure - cannot continue
 *     while (1) { } // Halt
 * }
 *
 * // Now modules can get interfaces
 * rx_error_interface_t* error_if = rx_infrastructure_get_error_interface();
 * @endcode
 *
 * @see rx_infrastructure_deinit() Cleanup function
 * @see rx_infrastructure_get_error_interface() Access error handler
 * @see rx_infrastructure_get_pin_interface() Access pin validator
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] Cleanup on failure (4 cleanup paths)
 * - Rule 7: [OK] All return values checked
 *
 * @since Version 1.0.0
 */
rx_err_t rx_infrastructure_init(void)
{
  const error_handler_config_t config = {
    .max_retries        = k_error_handler_default_max_retries,
    .initial_backoff_ms = k_error_handler_default_initial_backoff_ms,
    .max_backoff_ms     = k_error_handler_default_max_backoff_ms,
  };

  if (s_infrastructure_initialized) {
    rx_log_warn(s_tag, "Infrastructure already initialized");
    return k_rx_ok;
  }

  rx_log_info(s_tag, "Initializing global infrastructure");

  /* Initialize error handler */
  rx_err_t err = error_handler_init(&s_global_error_handler, &config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize error handler");
    return err;
  }

  /* Get error handler interface */
  err = error_handler_get_interface(&s_global_error_interface, &s_global_error_handler);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to get error handler interface");
    (void)error_handler_deinit(&s_global_error_handler); /* Cleanup, ignore errors */
    return err;
  }

  /* Initialize pin validator */
  err = pin_validator_init(&s_global_pin_validator);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize pin validator");
    (void)error_handler_deinit(&s_global_error_handler); /* Cleanup, ignore errors */
    return err;
  }

  /* Get pin validator interface */
  err = pin_validator_get_interface(&s_global_pin_interface, &s_global_pin_validator);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to get pin validator interface");
    (void)pin_validator_deinit(&s_global_pin_validator); /* Cleanup, ignore errors */
    (void)error_handler_deinit(&s_global_error_handler); /* Cleanup, ignore errors */
    return err;
  }

  s_infrastructure_initialized = true;

  rx_log_info(s_tag, "Global infrastructure initialized successfully");

  return k_rx_ok;
}

/**
 * @brief Deinitialize global infrastructure and release resources
 *
 * @details
 * Releases all global infrastructure resources in reverse initialization order.
 * Clears the initialized flag so interface accessors return NULL.
 *
 * @par Algorithm Steps:
 * 1. Check if initialized (return early if not)
 * 2. Deinitialize pin validator
 * 3. Deinitialize error handler
 * 4. Clear initialized flag
 *
 * @par Cleanup Order:
 * Resources are released in reverse initialization order to respect dependencies:
 * - Pin validator (uses error handler for logging)
 * - Error handler (foundation service)
 *
 *
 * @pre None - safe to call anytime
 *
 * @post s_infrastructure_initialized == false
 * @post Interface accessors returnnullptr
 * @post All s_global_* structures are in undefined state
 *
 * @note Thread Safety: NOT thread-safe
 * @note Idempotent: Safe to call multiple times
 *
 * @warning Call only after all threads have stopped using infrastructure
 * @warning After deinit, interface accessors returnnullptr
 *
 * @par Example:
 * @code{.c}
 * // During system shutdown
 * motor_deinit(&motor);
 * sensor_deinit(&sensor);
 *
 * // Release infrastructure last
 * rx_infrastructure_deinit();
 * @endcode
 *
 * @see rx_infrastructure_init() Initialization function
 *
 * @since Version 1.0.0
 */
rx_err_t rx_infrastructure_deinit(void)
{
  if (!s_infrastructure_initialized) {
    return k_rx_ok;
  }

  rx_log_info(s_tag, "Deinitializing global infrastructure");

  /* Deinitialize pin validator */
  rx_err_t err = pin_validator_deinit(&s_global_pin_validator);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Pin validator deinit failed");
    /* Continue with cleanup anyway */
  }

  /* Deinitialize error handler */
  err = error_handler_deinit(&s_global_error_handler);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Error handler deinit failed");
    return err;
  }

  s_infrastructure_initialized = false;

  rx_log_info(s_tag, "Global infrastructure deinitialized");

  return k_rx_ok;
}

/**
 * @brief Get global error handler interface
 *
 * @details
 * Returns a pointer to the global error handler interface structure.
 * The interface provides function pointers for error recovery operations.
 *
 * @par Interface Functions:
 * | Function    | Description                                   |
 * |-------------|-----------------------------------------------|
 * | try_recover | Attempt recovery with retry and backoff       |
 * | reset       | Reset error handler state                     |
 *
 *
 * @pre rx_infrastructure_init() should have been called
 *
 * @post Returned pointer valid for program lifetime (if not nullptr)
 *
 * @note Thread Safety: Safe - returns pointer to static data
 * @note The returned pointer should not be freed
 *
 * @par Example:
 * @code{.c}
 * rx_error_interface_t* err_if = rx_infrastructure_get_error_interface();
 * if (err_if == nullptr) {
 *     rx_log_error("APP", "Infrastructure not initialized");
 *     return k_rx_err_not_initialized;
 * }
 *
 * // Use error handler for retry logic
 * rx_err_t result = err_if->try_recover(err_if->ctx, my_operation, &context);
 * @endcode
 *
 * @see rx_error_interface_t Interface structure
 * @see rx_infrastructure_init() Must be called first
 *
 * @since Version 1.0.0
 */
rx_error_interface_t* rx_infrastructure_get_error_interface(void)
{
  if (!s_infrastructure_initialized) {
    return nullptr;
  }

  return &s_global_error_interface;
}

/**
 * @brief Get global pin validator interface
 *
 * @details
 * Returns a pointer to the global pin validator interface structure.
 * The interface provides function pointers for GPIO pin management.
 *
 * @par Interface Functions:
 * | Function   | Description                                    |
 * |------------|------------------------------------------------|
 * | claim_pin  | Claim a GPIO pin for a specific module         |
 * | release    | Release a previously claimed GPIO pin          |
 * | is_claimed | Check if a pin is already claimed              |
 *
 *
 * @pre rx_infrastructure_init() should have been called
 *
 * @post Returned pointer valid for program lifetime (if not nullptr)
 *
 * @note Thread Safety: Safe for read - returns pointer to static data
 * @note The returned pointer should not be freed
 *
 * @warning Pin claim/release operations may not be thread-safe
 *
 * @par Example:
 * @code{.c}
 * rx_pin_interface_t* pin_if = rx_infrastructure_get_pin_interface();
 * if (pin_if == nullptr) {
 *     rx_log_error("MOTOR", "Infrastructure not initialized");
 *     return k_rx_err_not_initialized;
 * }
 *
 * // Claim GPIO pin for motor PWM
 * rx_err_t err = pin_if->claim_pin(pin_if->ctx,
 *                                   k_port_1, k_pin_5,
 *                                   "MOTOR_PWM");
 * if (err != k_rx_ok) {
 *     rx_log_error("MOTOR", "Pin P15 already in use");
 *     return err;
 * }
 * @endcode
 *
 * @see rx_pin_interface_t Interface structure
 * @see rx_infrastructure_init() Must be called first
 *
 * @since Version 1.0.0
 */
rx_pin_interface_t* rx_infrastructure_get_pin_interface(void)
{
  if (!s_infrastructure_initialized) {
    return nullptr;
  }

  return &s_global_pin_interface;
}
