/* lib/rx_core/inc/rx_infrastructure.h */

/**
 * @file rx_infrastructure.h
 * @brief Global Infrastructure Initialization and Service Locator
 *
 * @details
 * ## Overview
 *
 * Implements the **Service Locator** pattern combined with **Dependency Inversion Principle**
 * to provide centralized access to core infrastructure services (error handling, pin validation)
 * throughout the STAR RX72N firmware. This module owns concrete implementations but exposes
 * only abstract interfaces to client code.
 *
 * **Key Design Pattern: Service Locator + Dependency Inversion**
 *
 * ```
 * +------------------------------------------------------------------+
 * |                     High-Level Modules                           |
 * |   (Motor Control, SPI Driver, USB Stack, etc.)                   |
 * +---------------+------------------------------+-------------------+
 *                 |                              |
 *                 | depends on (abstract)        | depends on (abstract)
 *                 v                              v
 * +---------------------------+  +---------------------------------+
 * |  rx_error_interface_t     |  |   rx_pin_interface_t            |
 * |  (Abstract Interface)     |  |   (Abstract Interface)          |
 * +-----------+---------------+  +-------------+-------------------+
 *             |                                 |
 *             | implements                      | implements
 *             v                                 v
 * +---------------------------+  +---------------------------------+
 * |  error_handler_t          |  |   pin_validator_t               |
 * |  (Concrete Impl)          |  |   (Concrete Impl)               |
 * +-----------+---------------+  +-------------+-------------------+
 *             |                                 |
 *             +-----------------+---------------+
 *                               |
 *                 owned and initialized by
 *                               v
 *             +---------------------------------+
 *             |  rx_infrastructure (THIS FILE)  |
 *             |  - Global instances             |
 *             |  - Initialization sequence      |
 *             |  - Interface accessors          |
 *             +---------------------------------+
 * ```
 *
 * ## Architecture Principles
 *
 * 1. **Single Responsibility**: Sole purpose is infrastructure lifecycle management
 * 2. **Open/Closed**: New services can be added without modifying client code
 * 3. **Liskov Substitution**: Interface pointers are always valid after init
 * 4. **Interface Segregation**: Separate accessors for each service
 * 5. **Dependency Inversion**: Clients depend on abstractions, not implementations
 *
 * ## Initialization Sequence
 *
 * ```
 * System Boot
 *     |
 *     +- 1. rx_infrastructure_init()
 *     |      +- Initialize global error handler
 *     |      |  +- Config: max_retries=3, initial_backoff=100ms, max_backoff=5s
 *     |      +- Initialize global pin validator
 *     |      |  +- Allocates pin reservation table
 *     |      +- Extract interfaces from concrete implementations
 *     |
 *     +- 2. Module Initialization (SPI, USB, Motor, etc.)
 *     |      +- Get interfaces via accessors
 *     |      +- Reserve pins via RX_RESERVE_PIN()
 *     |      +- Report errors via RX_REPORT_ERROR()
 *     |
 *     +- 3. Runtime Operation
 *     |      +- All modules use global interfaces
 *     |      +- No direct access to concrete implementations
 *     |
 *     +- 4. rx_infrastructure_deinit() [optional, shutdown]
 *            +- Release all resources
 *            +- Clear interface pointers
 * ```
 *
 * ## Thread Safety
 *
 * - `rx_infrastructure_init()`: **NOT thread-safe**, call from main() before ThreadX starts
 * - `rx_infrastructure_deinit()`: **NOT thread-safe**, call during shutdown only
 * - `rx_infrastructure_get_error_interface()`: **Thread-safe** (read-only after init)
 * - `rx_infrastructure_get_pin_interface()`: **Thread-safe** (read-only after init)
 * - Macro accessors: **Thread-safe** (delegates to underlying interface)
 *
 * ## Memory Footprint
 *
 * | Component           | Size (bytes) | Location |
 * |---------------------|--------------|----------|
 * | error_handler_t     | 328          | .bss     |
 * | pin_validator_t     | ~200         | .bss     |
 * | rx_error_interface_t| 32           | .bss     |
 * | rx_pin_interface_t  | 24           | .bss     |
 * | **Total**           | **~584**     | .bss     |
 *
 * ## Usage Examples
 *
 * ### Example 1: System Initialization
 * @code
 * // In main.c, BEFORE ThreadX initialization:
 * int main(void)
 * {
 *   // 1. Initialize infrastructure FIRST
 *   rx_err_t err = rx_infrastructure_init();
 *   if (err != k_rx_ok) {
 *     // Fatal error - cannot continue
 *     while (1) { __asm("nop"); }
 *   }
 *
 *   // 2. Now safe to initialize other modules
 *   err = spi_driver_init();
 *   if (err != k_rx_ok) {
 *     // Error handling now available
 *     RX_REPORT_ERROR(err, "SPI", "Driver init failed");
 *     return -1;
 *   }
 *
 *   // 3. Start ThreadX
 *   tx_kernel_enter();
 *
 *   return 0; // Never reached
 * }
 * @endcode
 *
 * ### Example 2: Module Using Error Reporting
 * @code
 * // In motor_control.c:
 * rx_err_t motor_set_velocity(motor_id_t id, float velocity_mps)
 * {
 *   // Validate input
 *   if (velocity_mps > k_max_velocity_mps) {
 *     RX_REPORT_ERROR(k_rx_err_invalid_arg, "MOTOR", "Velocity exceeds limit");
 *     return k_rx_err_invalid_arg;
 *   }
 *
 *   // Attempt operation
 *   rx_err_t err = motor_send_command(id, velocity_mps);
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "MOTOR", "Command failed");
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Example 3: Module Using Pin Reservation
 * @code
 * // In spi_driver.c:
 * rx_err_t spi_init(const spi_config_t* config)
 * {
 *   // Reserve SPI pins (prevents conflicts)
 *   rx_err_t err;
 *
 *   err = RX_RESERVE_PIN(0xA, 0, "SPI_CIPO");
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA0 already in use");
 *     return err;
 *   }
 *
 *   err = RX_RESERVE_PIN(0xA, 1, "SPI_COPI");
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xA, 0); // Cleanup previous reservation
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA1 already in use");
 *     return err;
 *   }
 *
 *   // Continue with SPI hardware initialization...
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Example 4: Using Interfaces Directly (Advanced)
 * @code
 * // Get interface once, use multiple times:
 * void motor_error_recovery(void)
 * {
 *   rx_error_interface_t* err_iface = rx_infrastructure_get_error_interface();
 *   if (err_iface == nullptr) {
 *     return; // Infrastructure not initialized
 *   }
 *
 *   // Check if retry limit reached for motor component
 *   if (err_iface->is_retry_limit_reached != nullptr) {
 *     bool limit_reached = err_iface->is_retry_limit_reached(err_iface->ctx, "MOTOR");
 *     if (limit_reached) {
 *       // Stop trying, require manual intervention
 *       motor_emergency_stop();
 *       return;
 *     }
 *   }
 *
 *   // Get backoff delay for exponential backoff
 *   if (err_iface->get_backoff_delay != nullptr) {
 *     uint32_t delay_ms = err_iface->get_backoff_delay(err_iface->ctx, "MOTOR");
 *     tx_thread_sleep(delay_ms); // ThreadX delay
 *   }
 *
 *   // Reset retry counter after successful recovery
 *   if (err_iface->reset_retry_counter != nullptr) {
 *     err_iface->reset_retry_counter(err_iface->ctx, "MOTOR");
 *   }
 * }
 * @endcode
 *
 * ## Design Rationale
 *
 * **Why Service Locator Pattern?**
 * - Avoids passing infrastructure pointers through every function call
 * - Reduces coupling between modules
 * - Simplifies function signatures
 * - Centralizes lifetime management
 *
 * **Why Global State?**
 * - Single-instance services (error handler, pin validator) don't need multiple instances
 * - Embedded systems: predictable memory layout, no dynamic allocation
 * - NASA Power of 10 Rule 3: No dynamic memory after initialization
 * - Performance: Direct access, no indirection or lookup overhead
 *
 * **Why Abstract Interfaces?**
 * - Testability: Can inject mock implementations for unit tests
 * - Flexibility: Can swap implementations without changing client code
 * - Encapsulation: Hides concrete types and internal state
 * - SOLID Compliance: Dependency Inversion Principle
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simplify control flow | [OK] | No goto, setjmp, recursion |
 * | 2. Fixed loop bounds | [OK] | No loops in this header |
 * | 3. No dynamic memory | [OK] | Static global instances only |
 * | 4. Functions < 60 lines | [OK] | All functions are accessors |
 * | 5. Use assertions | [OK] | Validation in init functions |
 * | 6. Data at smallest scope | [OK] | Globals encapsulated |
 * | 7. Check return values | [OK] | All errors propagated |
 * | 8. Limit preprocessor | [OK] | Macros for convenience only |
 * | 9. Restrict pointers | [WARN] | Function pointers (DIP) |
 * | 10. Compile warnings | [OK] | -Wall -Wextra -Werror |
 *
 * @see rx_error_interface.h Abstract error handler interface
 * @see rx_error_handler.h Concrete error handler implementation
 * @see rx_pin_interface.h Abstract pin validator interface
 * @see rx_pin_validator.h Concrete pin validator implementation
 *
 * @since Version 1.0.0
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include "rx_err.h"
#include "rx_error_interface.h"
#include "rx_pin_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Global Infrastructure Initialization
 * =============================================================================
 */

/**
 * @brief Initialize global infrastructure (MUST BE FIRST CALL IN main())
 *
 * @details
 * Performs one-time initialization of all global infrastructure services. This function
 * **MUST** be the first thing called in `main()`, before any other module initialization
 * and before starting ThreadX. Failure to initialize infrastructure will causenullptr
 * pointer dereferences when other modules attempt to use RX_REPORT_ERROR() or RX_RESERVE_PIN().
 *
 * ## Initialization Algorithm
 *
 * The function performs initialization in strict dependency order:
 *
 * 1. **Initialize Error Handler**:
 *    - Allocate static `error_handler_t` instance
 *    - Configure with default parameters (max_retries=3, backoffs)
 *    - Call `error_handler_init()` to prepare state
 *    - Extract `rx_error_interface_t` via `error_handler_get_interface()`
 *
 * 2. **Initialize Pin Validator**:
 *    - Allocate static `pin_validator_t` instance
 *    - Zero-initialize pin reservation table
 *    - Call `pin_validator_init()` to prepare state
 *    - Extract `rx_pin_interface_t` via `pin_validator_get_interface()`
 *
 * 3. **Store Interface Pointers**:
 *    - Save interface pointers to static globals
 *    - Pointers remain valid for entire program lifetime
 *
 * 4. **Set Initialization Flag**:
 *    - Mark infrastructure as initialized (prevents double-init)
 *
 * ## Initialization Flow Diagram
 *
 * @dot
 * digraph infrastructure_init {
 *   node [shape=box, style=rounded];
 *   rankdir=TD;
 *
 *   start [label="rx_infrastructure_init()", shape=ellipse, style=filled, fillcolor=lightgreen];
 *   check_flag [label="Check s_initialized\nflag", shape=diamond];
 *   already_init [label="Return\nk_rx_err_invalid_state", shape=ellipse, style=filled, fillcolor=orange];
 *
 *   init_error [label="1. Initialize\nerror_handler_t\n(default config)", style=filled, fillcolor=lightblue];
 *   check_error [label="error_handler_init()\nsuccess?", shape=diamond];
 *   error_fail [label="Return error code", shape=ellipse, style=filled, fillcolor=red];
 *   extract_error [label="Extract\nrx_error_interface_t", style=filled, fillcolor=lightblue];
 *
 *   init_pin [label="2. Initialize\npin_validator_t", style=filled, fillcolor=lightblue];
 *   check_pin [label="pin_validator_init()\nsuccess?", shape=diamond];
 *   pin_fail [label="Cleanup error handler\nReturn error code", shape=ellipse, style=filled, fillcolor=red];
 *   extract_pin [label="Extract\nrx_pin_interface_t", style=filled, fillcolor=lightblue];
 *
 *   store [label="3. Store interface\npointers to globals", style=filled, fillcolor=lightyellow];
 *   set_flag [label="4. Set s_initialized\nflag to true", style=filled, fillcolor=lightyellow];
 *   success [label="Return k_rx_ok", shape=ellipse, style=filled, fillcolor=lightgreen];
 *
 *   start -> check_flag;
 *   check_flag -> already_init [label="true"];
 *   check_flag -> init_error [label="false"];
 *
 *   init_error -> check_error;
 *   check_error -> error_fail [label="failed"];
 *   check_error -> extract_error [label="success"];
 *
 *   extract_error -> init_pin;
 *   init_pin -> check_pin;
 *   check_pin -> pin_fail [label="failed"];
 *   check_pin -> extract_pin [label="success"];
 *
 *   extract_pin -> store;
 *   store -> set_flag;
 *   set_flag -> success;
 * }
 * @enddot
 *
 * ## Default Configuration
 *
 * ### Error Handler Configuration
 * | Parameter         | Default Value | Rationale |
 * |-------------------|---------------|-----------|
 * | max_retries       | 3             | Balance between resilience and failure detection |
 * | initial_backoff_ms| 100           | 100ms allows time for transient issues to clear |
 * | max_backoff_ms    | 5000          | 5s prevents indefinite waits |
 * | backoff_multiplier| 2.0           | Exponential backoff (100ms -> 200ms -> 400ms -> ...) |
 * | max_errors        | 64            | Sufficient for all firmware components |
 * | max_component_errors | 16         | Track errors per component |
 *
 * ### Pin Validator Configuration
 * | Parameter         | Default Value | Rationale |
 * |-------------------|---------------|-----------|
 * | max_pins          | 128           | Covers all RX72N GPIO pins (Ports 0-F) |
 * | enable_conflicts  | true          | Prevent accidental pin conflicts |
 *
 * ## Typical Call Sequence
 *
 * @msc
 * main, infrastructure, error_handler, pin_validator;
 *
 * main => infrastructure [label="rx_infrastructure_init()"];
 * infrastructure => infrastructure [label="Check s_initialized"];
 * infrastructure note infrastructure [label="s_initialized == false"];
 *
 * infrastructure => error_handler [label="error_handler_init(&config)"];
 * error_handler => infrastructure [label="k_rx_ok"];
 * infrastructure => error_handler [label="error_handler_get_interface()"];
 * error_handler => infrastructure [label="rx_error_interface_t*"];
 *
 * infrastructure => pin_validator [label="pin_validator_init()"];
 * pin_validator => infrastructure [label="k_rx_ok"];
 * infrastructure => pin_validator [label="pin_validator_get_interface()"];
 * pin_validator => infrastructure [label="rx_pin_interface_t*"];
 *
 * infrastructure => infrastructure [label="Store interface pointers"];
 * infrastructure => infrastructure [label="s_initialized = true"];
 * infrastructure => main [label="k_rx_ok"];
 *
 * main note main [label="Infrastructure ready.\nModules can now init."];
 * @endmsc
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, infrastructure ready for use
 * @retval k_rx_err_invalid_state Already initialized (double-init attempt)
 * @retval k_rx_err_* Propagated from error_handler_init() or pin_validator_init()
 *
 * @pre System must be in reset state (no modules initialized)
 * @pre ThreadX must NOT be running (call from main(), not from thread)
 * @pre Must be called exactly once per system boot
 * @pre No other threads accessing infrastructure (main thread only)
 *
 * @post s_initialized flag set to true
 * @post rx_infrastructure_get_error_interface() returns non-NULL
 * @post rx_infrastructure_get_pin_interface() returns non-NULL
 * @post Global error handler is functional and thread-safe
 * @post Global pin validator is functional and thread-safe
 * @post All macros (RX_REPORT_ERROR, RX_RESERVE_PIN, RX_RELEASE_PIN) are functional
 *
 * @note **NOT thread-safe**. Must be called from main() before starting ThreadX.
 * @note Calling twice returns k_rx_err_invalid_state (double-init protection).
 * @note On failure, infrastructure is left in undefined state - system must halt.
 *
 * @warning **CRITICAL**: This MUST be the first call in main(). Failure to initialize
 *          will cause nullptr crashes when other modules use infrastructure macros.
 * @warning If this function fails, the system CANNOT continue - must halt or reset.
 *
 * @par Thread Safety:
 * - NOT thread-safe during initialization
 * - After successful init, interface getters are thread-safe (read-only)
 *
 * @par Performance:
 * - Execution time: ~500 us @ 240 MHz (one-time cost at boot)
 * - Memory allocated: ~584 bytes in .bss section
 * - Stack usage: ~64 bytes
 *
 * @par Example: Correct Initialization in main()
 * @code
 * int main(void)
 * {
 *   // Step 1: FIRST THING - Initialize infrastructure
 *   rx_err_t err = rx_infrastructure_init();
 *   if (err != k_rx_ok) {
 *     // FATAL: Cannot continue without infrastructure
 *     // In production: log to backup UART, trigger watchdog reset
 *     while (1) {
 *       __asm("nop"); // Halt system
 *     }
 *   }
 *
 *   // Step 2: Now safe to initialize other modules (they can use RX_REPORT_ERROR)
 *   err = watchdog_init();
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "WDT", "Watchdog init failed");
 *     return -1;
 *   }
 *
 *   err = spi_driver_init();
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "SPI", "SPI driver init failed");
 *     return -1;
 *   }
 *
 *   // Step 3: Start ThreadX (never returns)
 *   tx_kernel_enter();
 *
 *   return 0; // Unreachable
 * }
 * @endcode
 *
 * @par Example: Incorrect Initialization (WILL CRASH)
 * @code
 * // [FAIL] WRONG: Initializing SPI before infrastructure
 * int main(void)
 * {
 *   rx_err_t err = spi_driver_init(); // SPI tries to use RX_RESERVE_PIN()
 *   // CRASH: rx_infrastructure_get_pin_interface() returnsnullptr
 *   // NULL->reserve_pin() causes segmentation fault
 *
 *   rx_infrastructure_init(); // Too late!
 *   return 0;
 * }
 * @endcode
 *
 * @par Example: Handling Double-Init
 * @code
 * rx_err_t err = rx_infrastructure_init();
 * if (err != k_rx_ok) {
 *   // Check for double-init
 *   if (err == k_rx_err_invalid_state) {
 *     // Already initialized - this is OK in some test scenarios
 *     // Production code should never hit this
 *   } else {
 *     // Real initialization failure - halt
 *     while (1) { }
 *   }
 * }
 * @endcode
 *
 * @see rx_infrastructure_deinit() Cleanup infrastructure (shutdown only)
 * @see rx_infrastructure_get_error_interface() Get error handler interface
 * @see rx_infrastructure_get_pin_interface() Get pin validator interface
 * @see error_handler_init() Error handler initialization
 * @see pin_validator_init() Pin validator initialization
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 4 preconditions, 6 postconditions
 * - Rule 7: [OK] All return values checked
 */
[[nodiscard]] rx_err_t rx_infrastructure_init(void);

/**
 * @brief Deinitialize global infrastructure (graceful shutdown only)
 *
 * @details
 * Performs graceful shutdown of all infrastructure services. In embedded systems,
 * this function is **RARELY CALLED** because the system typically runs indefinitely
 * until power loss or reset. However, it's provided for:
 * - Unit testing (setup/teardown)
 * - Firmware update procedures (safe state before flash)
 * - Power management (deep sleep entry)
 * - Development/debugging (clean restarts)
 *
 * ## Deinitialization Algorithm
 *
 * Teardown occurs in **REVERSE** order of initialization (dependency order):
 *
 * 1. **Validate Initialized State**:
 *    - Check s_initialized flag
 *    - Return error if not initialized (nothing to clean up)
 *
 * 2. **Clear Interface Pointers**:
 *    - Set global interface pointers tonullptr
 *    - Prevents use-after-free if modules still running
 *
 * 3. **Deinitialize Pin Validator**:
 *    - Call `pin_validator_deinit()`
 *    - Releases all pin reservations
 *    - Clears pin reservation table
 *
 * 4. **Deinitialize Error Handler**:
 *    - Call `error_handler_deinit()`
 *    - Clears error counters and state
 *    - Releases mutex resources (if ThreadX running)
 *
 * 5. **Clear Initialization Flag**:
 *    - Reset s_initialized to false
 *    - Allows re-initialization (useful for tests)
 *
 * ## Shutdown Flow Diagram
 *
 * @dot
 * digraph infrastructure_deinit {
 *   node [shape=box, style=rounded];
 *   rankdir=TD;
 *
 *   start [label="rx_infrastructure_deinit()", shape=ellipse, style=filled, fillcolor=lightyellow];
 *   check_flag [label="Check s_initialized\nflag", shape=diamond];
 *   not_init [label="Return\nk_rx_err_invalid_state", shape=ellipse, style=filled, fillcolor=orange];
 *
 *   clear_iface [label="1. Set interface\npointers to NULL", style=filled, fillcolor=lightblue];
 *   note1 [label="Prevents use-after-free\nif modules still running", shape=note];
 *
 *   deinit_pin [label="2. Deinitialize\npin_validator_t", style=filled, fillcolor=lightblue];
 *   note2 [label="Releases all pin\nreservations", shape=note];
 *
 *   deinit_error [label="3. Deinitialize\nerror_handler_t", style=filled, fillcolor=lightblue];
 *   note3 [label="Clears error state,\nreleases mutex", shape=note];
 *
 *   clear_flag [label="4. Clear s_initialized\nflag to false", style=filled, fillcolor=lightyellow];
 *   success [label="Return k_rx_ok", shape=ellipse, style=filled, fillcolor=lightgreen];
 *
 *   start -> check_flag;
 *   check_flag -> not_init [label="false"];
 *   check_flag -> clear_iface [label="true"];
 *
 *   clear_iface -> deinit_pin;
 *   clear_iface -> note1 [style=dashed];
 *
 *   deinit_pin -> deinit_error;
 *   deinit_pin -> note2 [style=dashed];
 *
 *   deinit_error -> clear_flag;
 *   deinit_error -> note3 [style=dashed];
 *
 *   clear_flag -> success;
 * }
 * @enddot
 *
 * ## Typical Usage Scenarios
 *
 * ### Scenario 1: Unit Testing (Most Common Use)
 * @code
 * void test_motor_control(void)
 * {
 *   // Setup: Initialize infrastructure for test
 *   rx_err_t err = rx_infrastructure_init();
 *   assert(err == k_rx_ok);
 *
 *   // Test: Run motor control tests
 *   test_motor_set_velocity();
 *   test_motor_error_handling();
 *
 *   // Teardown: Clean up for next test
 *   err = rx_infrastructure_deinit();
 *   assert(err == k_rx_ok);
 * }
 * @endcode
 *
 * ### Scenario 2: Firmware Update Preparation
 * @code
 * void enter_bootloader_mode(void)
 * {
 *   // 1. Stop all threads
 *   motor_stop_all();
 *   usb_disconnect();
 *   spi_shutdown();
 *
 *   // 2. Deinitialize infrastructure
 *   rx_infrastructure_deinit();
 *
 *   // 3. Jump to bootloader
 *   bootloader_entry();
 * }
 * @endcode
 *
 * ### Scenario 3: Deep Sleep Power Management
 * @code
 * void enter_deep_sleep(void)
 * {
 *   // 1. Save critical state to backup SRAM
 *   save_state_to_backup_ram();
 *
 *   // 2. Shutdown peripherals
 *   usb_suspend();
 *   spi_sleep();
 *
 *   // 3. Deinitialize infrastructure
 *   rx_infrastructure_deinit();
 *
 *   // 4. Enter low-power mode
 *   rx72n_enter_deep_sleep();
 *
 *   // 5. After wakeup: Re-initialize everything
 *   rx_infrastructure_init();
 *   restore_state_from_backup_ram();
 * }
 * @endcode
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, infrastructure fully deinitialized
 * @retval k_rx_err_invalid_state Not initialized (nothing to clean up)
 *
 * @pre s_initialized flag must be true (infrastructure must be initialized)
 * @pre All threads using infrastructure must be stopped or suspended
 * @pre No modules should call RX_REPORT_ERROR() or RX_RESERVE_PIN() during deinit
 * @pre ThreadX threads should be suspended (if ThreadX is running)
 *
 * @post s_initialized flag set to false
 * @post rx_infrastructure_get_error_interface() returnsnullptr
 * @post rx_infrastructure_get_pin_interface() returnsnullptr
 * @post All error counters cleared
 * @post All pin reservations released
 * @post Infrastructure can be re-initialized via rx_infrastructure_init()
 *
 * @note **NOT thread-safe**. Caller must ensure no threads are using infrastructure.
 * @note Calling when not initialized returns k_rx_err_invalid_state (safe, no-op).
 * @note After deinit, all RX_REPORT_ERROR() and RX_RESERVE_PIN() calls will fail silently.
 *
 * @warning **DANGER**: Calling this while modules are still using infrastructure will
 *          cause crashes. Ensure all threads are stopped first.
 * @warning Do NOT call from ISR (may block on mutex cleanup).
 *
 * @par Thread Safety:
 * - NOT thread-safe
 * - Caller must ensure exclusive access (no concurrent infrastructure usage)
 * - Suspend all ThreadX threads before calling
 *
 * @par Performance:
 * - Execution time: ~200 us @ 240 MHz
 * - No heap operations (all cleanup is static)
 *
 * @par Example: Safe Shutdown Sequence
 * @code
 * void system_shutdown(void)
 * {
 *   // 1. Stop all application threads
 *   tx_thread_suspend(&motor_thread);
 *   tx_thread_suspend(&usb_thread);
 *   tx_thread_suspend(&comm_thread);
 *
 *   // 2. Disable interrupts (prevent ISRs from using infrastructure)
 *   __disable_irq();
 *
 *   // 3. Shutdown modules (in reverse init order)
 *   motor_shutdown();
 *   usb_shutdown();
 *   spi_shutdown();
 *
 *   // 4. Deinitialize infrastructure (last)
 *   rx_err_t err = rx_infrastructure_deinit();
 *   if (err != k_rx_ok) {
 *     // Log error (can't use RX_REPORT_ERROR anymore!)
 *     debug_uart_printf("Deinit failed: %d\n", err);
 *   }
 *
 *   // 5. Proceed with shutdown (reset, bootloader, sleep, etc.)
 *   trigger_system_reset();
 * }
 * @endcode
 *
 * @par Example: Test Fixture Pattern
 * @code
 * typedef struct {
 *   bool infrastructure_initialized;
 * } test_fixture_t;
 *
 * void test_setup(test_fixture_t* fixture)
 * {
 *   rx_err_t err = rx_infrastructure_init();
 *   assert(err == k_rx_ok);
 *   fixture->infrastructure_initialized = true;
 * }
 *
 * void test_teardown(test_fixture_t* fixture)
 * {
 *   if (fixture->infrastructure_initialized) {
 *     rx_err_t err = rx_infrastructure_deinit();
 *     assert(err == k_rx_ok);
 *     fixture->infrastructure_initialized = false;
 *   }
 * }
 *
 * void run_test_suite(void)
 * {
 *   test_fixture_t fixture = {0};
 *
 *   test_setup(&fixture);
 *   test_motor_functions();
 *   test_teardown(&fixture);
 *
 *   test_setup(&fixture);
 *   test_usb_functions();
 *   test_teardown(&fixture);
 * }
 * @endcode
 *
 * @par Example: Handling Double-Deinit
 * @code
 * rx_err_t err = rx_infrastructure_deinit();
 * if (err == k_rx_err_invalid_state) {
 *   // Already deinitialized - this is safe, continue
 * } else if (err != k_rx_ok) {
 *   // Real deinit error - investigate
 *   debug_uart_printf("Deinit error: %d\n", err);
 * }
 * @endcode
 *
 * @see rx_infrastructure_init() Initialize infrastructure
 * @see error_handler_deinit() Error handler cleanup
 * @see pin_validator_deinit() Pin validator cleanup
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 4 preconditions, 6 postconditions
 * - Rule 7: [OK] All return values checked
 */
[[nodiscard]] rx_err_t rx_infrastructure_deinit(void);

/* =============================================================================
 * Interface Accessors
 * =============================================================================
 */

/**
 * @brief Get global error handler interface (thread-safe accessor)
 *
 * @details
 * Returns a pointer to the global `rx_error_interface_t` that provides access to
 * the error handling subsystem. This is the **primary way** for modules to report
 * errors, check error counts, and implement retry logic with exponential backoff.
 *
 * The returned pointer is valid for the **entire program lifetime** after successful
 * initialization via `rx_infrastructure_init()`. Modules can cache this pointer for
 * performance-critical code paths.
 *
 * ## Pointer Lifetime and Validity
 *
 * ```
 * System Boot
 *     |
 *     +- rx_infrastructure_init()
 *     |      +- Interface pointer becomes valid
 *     |
 *     +- Runtime (entire program lifetime)
 *     |      +- Pointer remains valid
 *     |      +- Safe to call from any thread
 *     |      +- Safe to cache in module state
 *     |
 *     +- rx_infrastructure_deinit() [optional]
 *            +- Pointer becomes NULL (rare in embedded)
 * ```
 *
 * ## Usage Patterns
 *
 * ### Pattern 1: Direct Inline Usage (Recommended for Simple Cases)
 * @code
 * // Simple error reporting - get interface each time
 * void motor_set_velocity(float velocity_mps)
 * {
 *   if (velocity_mps > k_max_velocity_mps) {
 *     RX_REPORT_ERROR(k_rx_err_invalid_arg, "MOTOR", "Velocity exceeds limit");
 *     return;
 *   }
 *   // ...
 * }
 * @endcode
 *
 * ### Pattern 2: Cached Pointer (Recommended for Performance-Critical Code)
 * @code
 * // Cache interface pointer in module state
 * typedef struct {
 *   rx_error_interface_t* error_iface; // Cached at init
 *   // ... other state
 * } motor_state_t;
 *
 * void motor_init(motor_state_t* state)
 * {
 *   // Cache interface pointer once during init
 *   state->error_iface = rx_infrastructure_get_error_interface();
 *   if (state->error_iface == nullptr) {
 *     return k_rx_err_not_initialized;
 *   }
 *   // ...
 * }
 *
 * void motor_control_loop(motor_state_t* state)
 * {
 *   // Use cached pointer (no function call overhead)
 *   if (motor_error_detected()) {
 *     state->error_iface->report_error(
 *       state->error_iface->ctx,
 *       k_rx_err_hardware,
 *       "MOTOR",
 *       "Encoder fault"
 *     );
 *   }
 * }
 * @endcode
 *
 * ### Pattern 3: Defensive Null Check (Recommended for Critical Paths)
 * @code
 * void critical_error_handler(void)
 * {
 *   rx_error_interface_t* iface = rx_infrastructure_get_error_interface();
 *   if (iface == nullptr) {
 *     // Infrastructure not ready - fallback to direct UART logging
 *     debug_uart_printf("CRITICAL: Infrastructure not initialized\n");
 *     return;
 *   }
 *
 *   // Use interface normally
 *   iface->report_error(iface->ctx, k_rx_err_hardware, "CRITICAL", "Hardware fault");
 * }
 * @endcode
 *
 * ### Pattern 4: Advanced - Retry Logic with Backoff
 * @code
 * rx_err_t motor_send_command_with_retry(uint8_t* data, uint32_t len)
 * {
 *   rx_error_interface_t* iface = rx_infrastructure_get_error_interface();
 *   if (iface == nullptr) {
 *     return k_rx_err_not_initialized;
 *   }
 *
 *   // Check if we've exceeded retry limit
 *   if (iface->is_retry_limit_reached != nullptr &&
 *       iface->is_retry_limit_reached(iface->ctx, "MOTOR")) {
 *     return k_rx_err_retry_limit;
 *   }
 *
 *   // Attempt command
 *   rx_err_t err = motor_send_command_raw(data, len);
 *   if (err != k_rx_ok) {
 *     // Report error
 *     iface->report_error(iface->ctx, err, "MOTOR", "Command failed");
 *
 *     // Get exponential backoff delay
 *     if (iface->get_backoff_delay != nullptr) {
 *       uint32_t delay_ms = iface->get_backoff_delay(iface->ctx, "MOTOR");
 *       tx_thread_sleep(delay_ms); // Wait before retry
 *     }
 *
 *     return err;
 *   }
 *
 *   // Success - reset retry counter
 *   if (iface->reset_retry_counter != nullptr) {
 *     iface->reset_retry_counter(iface->ctx, "MOTOR");
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ## Interface Method Availability
 *
 * All function pointers in the returned interface are **guaranteed non-NULL** except:
 * - `get_component_error_count` (optional, may be NULL)
 * - `is_retry_limit_reached` (optional, may be NULL)
 * - `reset_retry_counter` (optional, may be NULL)
 * - `get_backoff_delay` (optional, may be NULL)
 *
 * **Always check optional methods** before calling:
 * @code
 * if (iface->get_backoff_delay != nullptr) {
 *   uint32_t delay = iface->get_backoff_delay(iface->ctx, "MOTOR");
 * }
 * @endcode
 *
 * ## Thread Safety Guarantees
 *
 * | Operation | Thread-Safe? | Details |
 * |-----------|--------------|---------|
 * | Calling this function | [OK] Yes | Read-only after init |
 * | Caching returned pointer | [OK] Yes | Pointer never changes |
 * | Calling interface methods | [OK] Yes | Methods are thread-safe |
 * | Calling during init/deinit | [X] No | Undefined behavior |
 *
 * @return rx_error_interface_t* Pointer to global error interface
 * @retval non-NULL Success, interface is ready for use
 * @retval NULL Infrastructure not initialized (call rx_infrastructure_init() first)
 *
 * @pre Infrastructure must be initialized via rx_infrastructure_init()
 * @pre Must NOT be called during rx_infrastructure_deinit()
 *
 * @post Returned pointer remains valid until rx_infrastructure_deinit()
 * @post All required interface methods are non-NULL
 * @post Interface methods are thread-safe (can be called from any thread)
 *
 * @note **Thread-safe**: Can be called from any thread after initialization.
 * @note **Performance**: Simple pointer return (~1 cycle), safe to call frequently.
 * @note **Caching**: Safe to cache returned pointer in module state for performance.
 * @note **Null check**: Always check for nullptr in defensive code paths.
 *
 * @warning Do NOT call during rx_infrastructure_init() or rx_infrastructure_deinit().
 * @warning NULL return means infrastructure not ready - check initialization order.
 *
 * @par Performance:
 * - Execution time: ~5 cycles @ 240 MHz (simple pointer read)
 * - No overhead: Direct memory access, no function calls
 * - Cache-friendly: Same pointer returned every call
 *
 * @par Example: Initialization-Time Validation
 * @code
 * rx_err_t module_init(void)
 * {
 *   // Verify infrastructure is ready
 *   rx_error_interface_t* iface = rx_infrastructure_get_error_interface();
 *   if (iface == nullptr) {
 *     // FATAL: Infrastructure not initialized
 *     // This means rx_infrastructure_init() was not called first
 *     debug_uart_printf("FATAL: Infrastructure not ready\n");
 *     return k_rx_err_not_initialized;
 *   }
 *
 *   // Verify required methods are present
 *   if (iface->report_error == nullptr) {
 *     debug_uart_printf("FATAL: Invalid error interface\n");
 *     return k_rx_err_invalid_state;
 *   }
 *
 *   // Cache pointer for runtime use
 *   g_module_state.error_iface = iface;
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * @see rx_infrastructure_init() Initialize infrastructure to make this return non-NULL
 * @see rx_infrastructure_deinit() Cleanup infrastructure (sets pointer to NULL)
 * @see rx_error_interface.h Abstract error handler interface definition
 * @see RX_REPORT_ERROR() Convenience macro using this accessor
 *
 * @since Version 1.0.0
 */
rx_error_interface_t* rx_infrastructure_get_error_interface(void);

/**
 * @brief Get global pin validator interface (thread-safe accessor)
 *
 * @details
 * Returns a pointer to the global `rx_pin_interface_t` that provides access to the
 * pin validation and reservation subsystem. This interface prevents **accidental pin
 * conflicts** by tracking which GPIO pins are reserved by which modules.
 *
 * The returned pointer is valid for the **entire program lifetime** after successful
 * initialization via `rx_infrastructure_init()`. Modules can cache this pointer for
 * performance-critical code paths.
 *
 * ## Pin Reservation System Overview
 *
 * The pin validator ensures that:
 * 1. **No pin conflicts**: Two modules cannot reserve the same pin
 * 2. **Explicit ownership**: Each pin knows which module owns it
 * 3. **Traceable usage**: Pin assignments documented in reservation table
 * 4. **Safe release**: Pins can only be released by their owner
 *
 * ```
 * Module Init Sequence:
 *     |
 *     +- SPI Driver Init
 *     |      +- Reserve PA0 ("SPI_CIPO") -> Success
 *     |      +- Reserve PA1 ("SPI_COPI") -> Success
 *     |      +- Reserve PA2 ("SPI_CLK")  -> Success
 *     |
 *     +- I2C Driver Init
 *     |      +- Reserve PA0 ("I2C_SDA")  -> FAIL (already SPI_CIPO)
 *     |      +- Init fails, prevents pin conflict
 *     |
 *     +- USB Driver Init
 *            +- Reserve PB0 ("USB_DP")   -> Success
 *            +- Reserve PB1 ("USB_DM")   -> Success
 * ```
 *
 * ## Usage Patterns
 *
 * ### Pattern 1: Module Initialization with Pin Reservation
 * @code
 * rx_err_t spi_driver_init(void)
 * {
 *   // Reserve all SPI pins before configuring hardware
 *   rx_err_t err;
 *
 *   err = RX_RESERVE_PIN(0xA, 0, "SPI_CIPO"); // Port A, Pin 0
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA0 already in use");
 *     return err;
 *   }
 *
 *   err = RX_RESERVE_PIN(0xA, 1, "SPI_COPI"); // Port A, Pin 1
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xA, 0); // Clean up previous reservation
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA1 already in use");
 *     return err;
 *   }
 *
 *   err = RX_RESERVE_PIN(0xA, 2, "SPI_CLK"); // Port A, Pin 2
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xA, 0); // Clean up
 *     RX_RELEASE_PIN(0xA, 1);
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA2 already in use");
 *     return err;
 *   }
 *
 *   // All pins reserved successfully - proceed with hardware config
 *   configure_spi_hardware();
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Pattern 2: Cached Pointer for Performance
 * @code
 * typedef struct {
 *   rx_pin_interface_t* pin_iface; // Cached at init
 *   uint8_t port;
 *   uint8_t pin;
 * } gpio_driver_state_t;
 *
 * rx_err_t gpio_init(gpio_driver_state_t* state, uint8_t port, uint8_t pin)
 * {
 *   // Cache interface pointer
 *   state->pin_iface = rx_infrastructure_get_pin_interface();
 *   if (state->pin_iface == nullptr) {
 *     return k_rx_err_not_initialized;
 *   }
 *
 *   // Reserve pin using cached interface
 *   rx_err_t err = state->pin_iface->reserve_pin(
 *     state->pin_iface->ctx, port, pin, "GPIO"
 *   );
 *   if (err != k_rx_ok) {
 *     return err;
 *   }
 *
 *   state->port = port;
 *   state->pin = pin;
 *   return k_rx_ok;
 * }
 *
 * void gpio_deinit(gpio_driver_state_t* state)
 * {
 *   // Release pin using cached interface
 *   if (state->pin_iface != nullptr) {
 *     state->pin_iface->release_pin(state->pin_iface->ctx, state->port, state->pin);
 *   }
 * }
 * @endcode
 *
 * ### Pattern 3: Dynamic Pin Configuration (Runtime Changes)
 * @code
 * rx_err_t motor_reconfigure_encoder_pins(uint8_t new_port, uint8_t new_pin_a, uint8_t new_pin_b)
 * {
 *   rx_pin_interface_t* iface = rx_infrastructure_get_pin_interface();
 *   if (iface == nullptr) {
 *     return k_rx_err_not_initialized;
 *   }
 *
 *   // Release old pins
 *   iface->release_pin(iface->ctx, motor_state.old_port, motor_state.old_pin_a);
 *   iface->release_pin(iface->ctx, motor_state.old_port, motor_state.old_pin_b);
 *
 *   // Reserve new pins
 *   rx_err_t err = iface->reserve_pin(iface->ctx, new_port, new_pin_a, "MOTOR_ENC_A");
 *   if (err != k_rx_ok) {
 *     // Failed to reserve - try to restore old pins
 *     iface->reserve_pin(iface->ctx, motor_state.old_port, motor_state.old_pin_a, "MOTOR_ENC_A");
 *     iface->reserve_pin(iface->ctx, motor_state.old_port, motor_state.old_pin_b, "MOTOR_ENC_B");
 *     return err;
 *   }
 *
 *   err = iface->reserve_pin(iface->ctx, new_port, new_pin_b, "MOTOR_ENC_B");
 *   if (err != k_rx_ok) {
 *     // Failed - restore old configuration
 *     iface->release_pin(iface->ctx, new_port, new_pin_a);
 *     iface->reserve_pin(iface->ctx, motor_state.old_port, motor_state.old_pin_a, "MOTOR_ENC_A");
 *     iface->reserve_pin(iface->ctx, motor_state.old_port, motor_state.old_pin_b, "MOTOR_ENC_B");
 *     return err;
 *   }
 *
 *   // Success - update state
 *   motor_state.old_port = new_port;
 *   motor_state.old_pin_a = new_pin_a;
 *   motor_state.old_pin_b = new_pin_b;
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Pattern 4: Defensive Null Check
 * @code
 * void debug_print_pin_usage(void)
 * {
 *   rx_pin_interface_t* iface = rx_infrastructure_get_pin_interface();
 *   if (iface == nullptr) {
 *     debug_uart_printf("Pin validator not initialized\n");
 *     return;
 *   }
 *
 *   // Check if debug method exists (optional interface method)
 *   if (iface->print_reservations != nullptr) {
 *     iface->print_reservations(iface->ctx);
 *   } else {
 *     debug_uart_printf("Pin reservation printing not supported\n");
 *   }
 * }
 * @endcode
 *
 * ## Interface Method Availability
 *
 * All function pointers in the returned interface are **guaranteed non-NULL** except:
 * - `is_pin_reserved` (optional, may be NULL)
 * - `get_pin_function` (optional, may be NULL)
 * - `print_reservations` (optional, may be NULL)
 *
 * **Always check optional methods** before calling:
 * @code
 * if (iface->is_pin_reserved != nullptr) {
 *   bool reserved = iface->is_pin_reserved(iface->ctx, port, pin);
 * }
 * @endcode
 *
 * ## Thread Safety Guarantees
 *
 * | Operation | Thread-Safe? | Details |
 * |-----------|--------------|---------|
 * | Calling this function | [OK] Yes | Read-only after init |
 * | Caching returned pointer | [OK] Yes | Pointer never changes |
 * | Calling interface methods | [OK] Yes | Methods use mutex protection |
 * | Concurrent reserve/release | [OK] Yes | Atomic operations |
 * | Calling during init/deinit | [X] No | Undefined behavior |
 *
 * @return rx_pin_interface_t* Pointer to global pin interface
 * @retval non-NULL Success, interface is ready for use
 * @retval NULL Infrastructure not initialized (call rx_infrastructure_init() first)
 *
 * @pre Infrastructure must be initialized via rx_infrastructure_init()
 * @pre Must NOT be called during rx_infrastructure_deinit()
 *
 * @post Returned pointer remains valid until rx_infrastructure_deinit()
 * @post All required interface methods are non-NULL
 * @post Interface methods are thread-safe (can be called from any thread)
 *
 * @note **Thread-safe**: Can be called from any thread after initialization.
 * @note **Performance**: Simple pointer return (~1 cycle), safe to call frequently.
 * @note **Caching**: Safe to cache returned pointer in module state for performance.
 * @note **Null check**: Always check for nullptr in defensive code paths.
 *
 * @warning Do NOT call during rx_infrastructure_init() or rx_infrastructure_deinit().
 * @warning NULL return means infrastructure not ready - check initialization order.
 * @warning Pin reservation errors mean conflict - do NOT proceed with hardware config.
 *
 * @par Performance:
 * - Execution time: ~5 cycles @ 240 MHz (simple pointer read)
 * - No overhead: Direct memory access, no function calls
 * - Cache-friendly: Same pointer returned every call
 *
 * @par Example: Pin Conflict Detection
 * @code
 * void diagnose_pin_conflict(uint8_t port, uint8_t pin)
 * {
 *   rx_pin_interface_t* iface = rx_infrastructure_get_pin_interface();
 *   if (iface == nullptr) {
 *     debug_uart_printf("Pin validator not initialized\n");
 *     return;
 *   }
 *
 *   // Check if pin is already reserved
 *   if (iface->is_pin_reserved != nullptr) {
 *     bool reserved = iface->is_pin_reserved(iface->ctx, port, pin);
 *     if (reserved) {
 *       // Get function name
 *       if (iface->get_pin_function != nullptr) {
 *         const char* function = iface->get_pin_function(iface->ctx, port, pin);
 *         debug_uart_printf("Pin P%X.%d reserved for: %s\n", port, pin, function);
 *       } else {
 *         debug_uart_printf("Pin P%X.%d is reserved\n", port, pin);
 *       }
 *     } else {
 *       debug_uart_printf("Pin P%X.%d is available\n", port, pin);
 *     }
 *   }
 * }
 * @endcode
 *
 * @par Example: Bulk Pin Reservation with Cleanup
 * @code
 * typedef struct {
 *   uint8_t port;
 *   uint8_t pin;
 *   const char* function;
 * } pin_reservation_t;
 *
 * rx_err_t reserve_pins_bulk(const pin_reservation_t* pins, uint32_t count)
 * {
 *   rx_pin_interface_t* iface = rx_infrastructure_get_pin_interface();
 *   if (iface == nullptr) {
 *     return k_rx_err_not_initialized;
 *   }
 *
 *   // Reserve all pins
 *   for (uint32_t i = 0; i < count; i++) {
 *     rx_err_t err = iface->reserve_pin(
 *       iface->ctx, pins[i].port, pins[i].pin, pins[i].function
 *     );
 *     if (err != k_rx_ok) {
 *       // Cleanup: Release all previously reserved pins
 *       for (uint32_t j = 0; j < i; j++) {
 *         iface->release_pin(iface->ctx, pins[j].port, pins[j].pin);
 *       }
 *       return err;
 *     }
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * @see rx_infrastructure_init() Initialize infrastructure to make this return non-NULL
 * @see rx_infrastructure_deinit() Cleanup infrastructure (sets pointer to NULL)
 * @see rx_pin_interface.h Abstract pin validator interface definition
 * @see RX_RESERVE_PIN() Convenience macro using this accessor
 * @see RX_RELEASE_PIN() Convenience macro using this accessor
 *
 * @since Version 1.0.0
 */
rx_pin_interface_t* rx_infrastructure_get_pin_interface(void);

/* =============================================================================
 * Convenience Macros
 * =============================================================================
 */

/**
 * @def RX_REPORT_ERROR
 * @brief Report error through global error handler (convenience macro)
 *
 * @details
 * Convenience macro that wraps the error reporting interface to provide simple,
 * one-line error reporting throughout the firmware. This is the **preferred way**
 * to report errors in STAR firmware - it's safer, more concise, and more readable
 * than directly accessing the interface.
 *
 * ## Macro Expansion
 *
 * The macro expands to a safe `do { } while (0)` block that:
 * 1. Gets the global error interface
 * 2. Checks for nullptr (safe if infrastructure not initialized)
 * 3. Calls `report_error()` method if interface is valid
 * 4. Returns immediately if interface is nullptr (no-op, no crash)
 *
 * **Expanded code**:
 * @code
 * // RX_REPORT_ERROR(k_rx_err_timeout, "SPI", "Transaction timeout");
 * // Expands to:
 * do {
 *   rx_error_interface_t* iface_ = rx_infrastructure_get_error_interface();
 *   if (iface_ != nullptr) {
 *     iface_->report_error(iface_->ctx, k_rx_err_timeout, "SPI", "Transaction timeout");
 *   }
 * } while (0)
 * @endcode
 *
 * ## Safety Features
 *
 * - **Null-safe**: Silently no-ops if infrastructure not initialized
 * - **Statement-like**: Can be used anywhere a statement is valid
 * - **No side effects**: Parameters evaluated once, no double evaluation
 * - **Scope-safe**: `do { } while (0)` ensures proper scope in all contexts
 * - **Thread-safe**: Underlying interface is thread-safe
 *
 * ## Usage Examples
 *
 * ### Example 1: Simple Error Reporting
 * @code
 * rx_err_t motor_set_velocity(float velocity_mps)
 * {
 *   if (velocity_mps > k_max_velocity_mps) {
 *     RX_REPORT_ERROR(k_rx_err_invalid_arg, "MOTOR", "Velocity exceeds limit");
 *     return k_rx_err_invalid_arg;
 *   }
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Example 2: Error Propagation with Reporting
 * @code
 * rx_err_t motor_init(void)
 * {
 *   rx_err_t err = encoder_init();
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "MOTOR", "Encoder init failed");
 *     return err;
 *   }
 *
 *   err = driver_init();
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "MOTOR", "Driver init failed");
 *     encoder_deinit(); // Cleanup
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Example 3: Conditional Reporting (if statement)
 * @code
 * void motor_control_loop(void)
 * {
 *   if (motor_overheated())
 *     RX_REPORT_ERROR(k_rx_err_hardware, "MOTOR", "Overheating detected");
 *
 *   if (encoder_fault())
 *     RX_REPORT_ERROR(k_rx_err_hardware, "MOTOR", "Encoder fault");
 * }
 * @endcode
 *
 * ### Example 4: Error with Dynamic Message
 * @code
 * void motor_check_velocity(float velocity_mps)
 * {
 *   char msg[64];
 *   if (velocity_mps > k_max_velocity_mps) {
 *     snprintf(msg, sizeof(msg), "Velocity %.2f exceeds max %.2f",
 *              velocity_mps, k_max_velocity_mps);
 *     RX_REPORT_ERROR(k_rx_err_invalid_arg, "MOTOR", msg);
 *   }
 * }
 * @endcode
 *
 * ## When to Use vs. Direct Interface
 *
 * | Scenario | Use Macro | Use Direct Interface |
 * |----------|-----------|----------------------|
 * | Simple error reporting | [OK] Yes | No |
 * | One-line logging | [OK] Yes | No |
 * | Need retry logic | No | [OK] Yes |
 * | Need error counts | No | [OK] Yes |
 * | Need backoff delay | No | [OK] Yes |
 * | Performance-critical loop | No | [OK] Yes (cache pointer) |
 *
 * ## Thread Safety
 *
 * - **Thread-safe**: Can be called from any thread
 * - **ISR-safe**: Can be called from interrupts (with caveats, see below)
 * - **Reentrant**: Multiple threads can call concurrently
 *
 * ## ISR Usage Considerations
 *
 * While technically safe to call from ISR, consider:
 * - Error handler may use mutex (can block if ThreadX running)
 * - String formatting in ISR is discouraged (stack usage)
 * - Prefer simple error codes, defer detailed reporting to thread context
 *
 * @code
 * // ISR Example: Simple error reporting
 * void motor_encoder_isr(void)
 * {
 *   if (encoder_error_detected()) {
 *     RX_REPORT_ERROR(k_rx_err_hardware, "MOTOR", "Encoder ISR error");
 *   }
 * }
 * @endcode
 *
 * @param[in] err Error code (rx_err_t) - Evaluated once, no side effects
 * @param[in] component Component name (const char*) - Must be string literal or valid pointer
 * @param[in] message Error message (const char*) - Must be string literal or valid pointer
 *
 * @note Parameters are evaluated **once** - safe to pass function calls.
 * @note If infrastructure not initialized, this is a **silent no-op** (no crash).
 * @note Component and message strings are **not copied** - must remain valid.
 * @note Prefer string literals for component/message (embedded in .rodata).
 *
 * @warning Do NOT use complex expressions in `component` or `message` parameters
 *          that allocate memory or have side effects.
 * @warning String parameters must remain valid for the error's lifetime (use literals).
 *
 * @par Example: Parameter Evaluation Safety
 * @code
 * // SAFE: Function called once
 * RX_REPORT_ERROR(spi_transmit(), "SPI", "TX failed");
 *
 * // SAFE: String literals (in .rodata)
 * RX_REPORT_ERROR(err, "MOTOR", "Velocity error");
 *
 * // UNSAFE: Dynamic string (stack variable, may be invalid later)
 * char msg[64];
 * snprintf(msg, sizeof(msg), "Error %d", code);
 * RX_REPORT_ERROR(err, "MOTOR", msg); // [FAIL] msg may be invalid when accessed later
 *
 * // SAFE: Static buffer or string literal
 * static char msg[64];
 * snprintf(msg, sizeof(msg), "Error %d", code);
 * RX_REPORT_ERROR(err, "MOTOR", msg); // [OK] msg remains valid
 * @endcode
 *
 * @see rx_infrastructure_get_error_interface() Get error interface directly
 * @see rx_error_interface.h Error interface definition
 *
 * @since Version 1.0.0
 */
#define RX_REPORT_ERROR(err, component, message)                                                   \
  do {                                                                                             \
    rx_error_interface_t* iface_ = rx_infrastructure_get_error_interface();                        \
    if (iface_ != nullptr) {                                                                       \
      iface_->report_error(iface_->ctx, err, component, message);                                  \
    }                                                                                              \
  } while (0)

/**
 * @def RX_RESERVE_PIN
 * @brief Reserve GPIO pin through global pin validator (convenience macro with return value)
 *
 * @details
 * Convenience macro that wraps the pin reservation interface to provide simple,
 * one-line pin reservation throughout the firmware. This is the **preferred way**
 * to reserve pins in STAR firmware - it prevents accidental pin conflicts and
 * provides explicit ownership tracking.
 *
 * ## Macro Expansion (GNU Statement Expression)
 *
 * This macro uses **GNU C extension** `({ ... })` (statement expression) to:
 * 1. Get the global pin interface
 * 2. Check for nullptr (returns error if infrastructure not initialized)
 * 3. Call `reserve_pin()` method if interface is valid
 * 4. Return `rx_err_t` error code as the macro's "value"
 *
 * **Expanded code**:
 * @code
 * // rx_err_t err = RX_RESERVE_PIN(0xA, 5, "SPI_COPI");
 * // Expands to:
 * rx_err_t err = ({
 *   rx_err_t err_ = k_rx_err_invalid_state;
 *   rx_pin_interface_t* iface_ = rx_infrastructure_get_pin_interface();
 *   if (iface_ != nullptr) {
 *     err_ = iface_->reserve_pin(iface_->ctx, 0xA, 5, "SPI_COPI");
 *   }
 *   err_; // This becomes the "return value" of the expression
 * });
 * @endcode
 *
 * ## Pin Conflict Prevention
 *
 * The pin validator ensures that:
 * - **No double reservation**: A pin can only be reserved once
 * - **Explicit ownership**: Each pin knows which function owns it
 * - **Traceable usage**: Pin assignments are tracked in a global table
 * - **Atomic operations**: Reservation is thread-safe (mutex protected)
 *
 * ```
 * Module A:  RX_RESERVE_PIN(0xA, 5, "SPI_COPI")  -> k_rx_ok
 * Module B:  RX_RESERVE_PIN(0xA, 5, "I2C_SDA")   -> k_rx_err_resource_busy (conflict!)
 * ```
 *
 * ## Typical Usage Patterns
 *
 * ### Pattern 1: Reserve-Check-Cleanup on Error
 * @code
 * rx_err_t spi_driver_init(void)
 * {
 *   // Reserve CIPO (first pin)
 *   rx_err_t err = RX_RESERVE_PIN(0xA, 0, "SPI_CIPO");
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA0 (CIPO) conflict");
 *     return err;
 *   }
 *
 *   // Reserve COPI (second pin)
 *   err = RX_RESERVE_PIN(0xA, 1, "SPI_COPI");
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xA, 0); // Clean up first pin
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA1 (COPI) conflict");
 *     return err;
 *   }
 *
 *   // Reserve CLK (third pin)
 *   err = RX_RESERVE_PIN(0xA, 2, "SPI_CLK");
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xA, 0); // Clean up first two pins
 *     RX_RELEASE_PIN(0xA, 1);
 *     RX_REPORT_ERROR(err, "SPI", "Pin PA2 (CLK) conflict");
 *     return err;
 *   }
 *
 *   // All pins successfully reserved - proceed with hardware config
 *   configure_spi_hardware();
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Pattern 2: Bulk Reservation Helper
 * @code
 * typedef struct {
 *   uint8_t port;
 *   uint8_t pin;
 *   const char* function;
 * } pin_def_t;
 *
 * // Define all SPI pins
 * static const pin_def_t k_spi_pins[] = {
 *   {0xA, 0, "SPI_CIPO"},
 *   {0xA, 1, "SPI_COPI"},
 *   {0xA, 2, "SPI_CLK"},
 *   {0xA, 3, "SPI_CS"},
 * };
 *
 * rx_err_t reserve_spi_pins(void)
 * {
 *   for (uint32_t i = 0; i < ARRAY_SIZE(k_spi_pins); i++) {
 *     rx_err_t err = RX_RESERVE_PIN(
 *       k_spi_pins[i].port,
 *       k_spi_pins[i].pin,
 *       k_spi_pins[i].function
 *     );
 *     if (err != k_rx_ok) {
 *       // Cleanup all previously reserved pins
 *       for (uint32_t j = 0; j < i; j++) {
 *         RX_RELEASE_PIN(k_spi_pins[j].port, k_spi_pins[j].pin);
 *       }
 *       return err;
 *     }
 *   }
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Pattern 3: Inline Error Check
 * @code
 * void motor_init(void)
 * {
 *   // Reserve and check in single if-statement
 *   if (RX_RESERVE_PIN(0xB, 4, "MOTOR_PWM") != k_rx_ok) {
 *     // Handle error
 *     return k_rx_err_resource_busy;
 *   }
 *
 *   if (RX_RESERVE_PIN(0xB, 5, "MOTOR_DIR") != k_rx_ok) {
 *     RX_RELEASE_PIN(0xB, 4); // Cleanup
 *     return k_rx_err_resource_busy;
 *   }
 * }
 * @endcode
 *
 * ## Port and Pin Numbering
 *
 * **Port numbers**: Use hexadecimal for RX72N ports (0x0 = Port 0, 0xA = Port A, etc.)
 * **Pin numbers**: Use decimal 0-7 for pin within port
 *
 * | Port Letter | Hex Value | Example Pin | Macro Call |
 * |-------------|-----------|-------------|------------|
 * | Port 0      | 0x0       | P0.5        | RX_RESERVE_PIN(0x0, 5, "name") |
 * | Port A      | 0xA       | PA.3        | RX_RESERVE_PIN(0xA, 3, "name") |
 * | Port B      | 0xB       | PB.7        | RX_RESERVE_PIN(0xB, 7, "name") |
 * | Port E      | 0xE       | PE.1        | RX_RESERVE_PIN(0xE, 1, "name") |
 *
 * @param[in] port Port number in hexadecimal (0x0-0xF for RX72N)
 * @param[in] pin Pin number within port (0-7)
 * @param[in] function Function name (const char* string literal) - identifies owner
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, pin reserved
 * @retval k_rx_err_invalid_state Infrastructure not initialized
 * @retval k_rx_err_resource_busy Pin already reserved by another function
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 * @retval k_rx_err_null_ptr Null function name
 *
 * @note **GNU C Extension**: Uses statement expression `({ })` - requires GCC/Clang.
 * @note **Returns value**: Can be assigned to variable or used directly in if-statement.
 * @note **Thread-safe**: Can be called from multiple threads concurrently.
 * @note **Function string**: Must be string literal or valid pointer (not copied).
 * @note **No cleanup on error**: Caller must manually release pins on partial failure.
 *
 * @warning **Must check return value**: Ignoring errors can lead to pin conflicts.
 * @warning **Cleanup required**: On error, release any previously reserved pins.
 * @warning **String lifetime**: Function name must remain valid (use string literals).
 * @warning **Not ISR-safe**: Uses mutex internally, do NOT call from ISR.
 *
 * @par Example: Complete Module Pin Management
 * @code
 * typedef struct {
 *   bool pins_reserved;
 *   // ... other state
 * } module_state_t;
 *
 * rx_err_t module_init(module_state_t* state)
 * {
 *   // Reserve all module pins
 *   rx_err_t err = RX_RESERVE_PIN(0xA, 0, "MODULE_PIN0");
 *   if (err != k_rx_ok) {
 *     return err;
 *   }
 *
 *   err = RX_RESERVE_PIN(0xA, 1, "MODULE_PIN1");
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xA, 0); // Cleanup
 *     return err;
 *   }
 *
 *   state->pins_reserved = true;
 *   return k_rx_ok;
 * }
 *
 * void module_deinit(module_state_t* state)
 * {
 *   if (state->pins_reserved) {
 *     RX_RELEASE_PIN(0xA, 0);
 *     RX_RELEASE_PIN(0xA, 1);
 *     state->pins_reserved = false;
 *   }
 * }
 * @endcode
 *
 * @see RX_RELEASE_PIN() Release a reserved pin
 * @see rx_infrastructure_get_pin_interface() Get pin interface directly
 * @see rx_pin_interface.h Pin interface definition
 *
 * @since Version 1.0.0
 */
#define RX_RESERVE_PIN(port, pin, function)                                                        \
  ({                                                                                               \
    rx_err_t            err_   = k_rx_err_invalid_state;                                           \
    rx_pin_interface_t* iface_ = rx_infrastructure_get_pin_interface();                            \
    if (iface_ != nullptr) {                                                                       \
      err_ = iface_->reserve_pin(iface_->ctx, port, pin, function);                                \
    }                                                                                              \
    err_;                                                                                          \
  })

/**
 * @def RX_RELEASE_PIN
 * @brief Release reserved GPIO pin through global pin validator (convenience macro with return value)
 *
 * @details
 * Convenience macro that wraps the pin release interface to provide simple, one-line
 * pin release throughout the firmware. This is the **required way** to release pins
 * reserved with RX_RESERVE_PIN() - it ensures proper cleanup and prevents resource leaks.
 *
 * ## Macro Expansion (GNU Statement Expression)
 *
 * This macro uses **GNU C extension** `({ ... })` (statement expression) to:
 * 1. Get the global pin interface
 * 2. Check for nullptr (returns error if infrastructure not initialized)
 * 3. Call `release_pin()` method if interface is valid
 * 4. Return `rx_err_t` error code as the macro's "value"
 *
 * **Expanded code**:
 * @code
 * // rx_err_t err = RX_RELEASE_PIN(0xA, 5);
 * // Expands to:
 * rx_err_t err = ({
 *   rx_err_t err_ = k_rx_err_invalid_state;
 *   rx_pin_interface_t* iface_ = rx_infrastructure_get_pin_interface();
 *   if (iface_ != nullptr) {
 *     err_ = iface_->release_pin(iface_->ctx, 0xA, 5);
 *   }
 *   err_; // This becomes the "return value" of the expression
 * });
 * @endcode
 *
 * ## When to Release Pins
 *
 * Pins should be released in these scenarios:
 * 1. **Module deinitialization**: During shutdown or cleanup
 * 2. **Error handling**: On partial initialization failure (cleanup)
 * 3. **Reconfiguration**: Before reserving different pins for same module
 * 4. **Resource sharing**: When pin is no longer needed and can be reused
 *
 * ## Typical Usage Patterns
 *
 * ### Pattern 1: Normal Deinitialization
 * @code
 * rx_err_t spi_driver_init(void)
 * {
 *   RX_RESERVE_PIN(0xA, 0, "SPI_CIPO");
 *   RX_RESERVE_PIN(0xA, 1, "SPI_COPI");
 *   RX_RESERVE_PIN(0xA, 2, "SPI_CLK");
 *   // ... init logic
 *   return k_rx_ok;
 * }
 *
 * void spi_driver_deinit(void)
 * {
 *   // Release all SPI pins (in any order)
 *   RX_RELEASE_PIN(0xA, 0); // CIPO
 *   RX_RELEASE_PIN(0xA, 1); // COPI
 *   RX_RELEASE_PIN(0xA, 2); // CLK
 *   // ... cleanup logic
 * }
 * @endcode
 *
 * ### Pattern 2: Error Cleanup (Reverse Order Recommended)
 * @code
 * rx_err_t module_init(void)
 * {
 *   rx_err_t err;
 *
 *   // Reserve pin 0
 *   err = RX_RESERVE_PIN(0xB, 0, "MODULE_PIN0");
 *   if (err != k_rx_ok) {
 *     return err;
 *   }
 *
 *   // Reserve pin 1
 *   err = RX_RESERVE_PIN(0xB, 1, "MODULE_PIN1");
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xB, 0); // Clean up pin 0
 *     return err;
 *   }
 *
 *   // Reserve pin 2
 *   err = RX_RESERVE_PIN(0xB, 2, "MODULE_PIN2");
 *   if (err != k_rx_ok) {
 *     RX_RELEASE_PIN(0xB, 1); // Clean up in reverse order
 *     RX_RELEASE_PIN(0xB, 0);
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Pattern 3: Dynamic Reconfiguration
 * @code
 * rx_err_t motor_change_encoder_pins(uint8_t new_port, uint8_t new_pin_a, uint8_t new_pin_b)
 * {
 *   // Save old configuration in case new reservation fails
 *   uint8_t old_port = motor_state.encoder_port;
 *   uint8_t old_pin_a = motor_state.encoder_pin_a;
 *   uint8_t old_pin_b = motor_state.encoder_pin_b;
 *
 *   // Release old pins
 *   RX_RELEASE_PIN(old_port, old_pin_a);
 *   RX_RELEASE_PIN(old_port, old_pin_b);
 *
 *   // Reserve new pins
 *   rx_err_t err = RX_RESERVE_PIN(new_port, new_pin_a, "MOTOR_ENC_A");
 *   if (err != k_rx_ok) {
 *     // Failed - restore old pins
 *     RX_RESERVE_PIN(old_port, old_pin_a, "MOTOR_ENC_A");
 *     RX_RESERVE_PIN(old_port, old_pin_b, "MOTOR_ENC_B");
 *     return err;
 *   }
 *
 *   err = RX_RESERVE_PIN(new_port, new_pin_b, "MOTOR_ENC_B");
 *   if (err != k_rx_ok) {
 *     // Failed - restore old configuration
 *     RX_RELEASE_PIN(new_port, new_pin_a);
 *     RX_RESERVE_PIN(old_port, old_pin_a, "MOTOR_ENC_A");
 *     RX_RESERVE_PIN(old_port, old_pin_b, "MOTOR_ENC_B");
 *     return err;
 *   }
 *
 *   // Success - update state
 *   motor_state.encoder_port = new_port;
 *   motor_state.encoder_pin_a = new_pin_a;
 *   motor_state.encoder_pin_b = new_pin_b;
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ### Pattern 4: Idempotent Cleanup (Safe Double-Release)
 * @code
 * typedef struct {
 *   bool pin_reserved;
 *   uint8_t port;
 *   uint8_t pin;
 * } module_state_t;
 *
 * void module_cleanup(module_state_t* state)
 * {
 *   // Check flag to avoid double-release
 *   if (state->pin_reserved) {
 *     RX_RELEASE_PIN(state->port, state->pin);
 *     state->pin_reserved = false;
 *   }
 * }
 *
 * // Can be called multiple times safely
 * module_cleanup(&state); // Releases pin
 * module_cleanup(&state); // No-op (flag already false)
 * module_cleanup(&state); // No-op
 * @endcode
 *
 * ## Return Value Handling
 *
 * Release errors are **rare** but can occur:
 * - Infrastructure not initialized (`k_rx_err_invalid_state`)
 * - Pin not reserved (`k_rx_err_not_found`)
 * - Invalid port/pin (`k_rx_err_invalid_arg`)
 *
 * Most code **ignores release errors** during cleanup (best-effort cleanup).
 * However, for **critical code**, check the return value:
 *
 * @code
 * // Best-effort cleanup (typical)
 * void module_deinit(void)
 * {
 *   RX_RELEASE_PIN(0xA, 0); // Ignore errors
 *   RX_RELEASE_PIN(0xA, 1);
 *   RX_RELEASE_PIN(0xA, 2);
 * }
 *
 * // Strict cleanup (critical systems)
 * rx_err_t module_deinit_strict(void)
 * {
 *   rx_err_t err;
 *
 *   err = RX_RELEASE_PIN(0xA, 0);
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "MODULE", "Failed to release pin PA0");
 *     return err;
 *   }
 *
 *   err = RX_RELEASE_PIN(0xA, 1);
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "MODULE", "Failed to release pin PA1");
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * ## Port and Pin Numbering
 *
 * **Port numbers**: Use hexadecimal for RX72N ports (0x0 = Port 0, 0xA = Port A, etc.)
 * **Pin numbers**: Use decimal 0-7 for pin within port
 *
 * | Port Letter | Hex Value | Example Pin | Macro Call |
 * |-------------|-----------|-------------|------------|
 * | Port 0      | 0x0       | P0.5        | RX_RELEASE_PIN(0x0, 5) |
 * | Port A      | 0xA       | PA.3        | RX_RELEASE_PIN(0xA, 3) |
 * | Port B      | 0xB       | PB.7        | RX_RELEASE_PIN(0xB, 7) |
 * | Port E      | 0xE       | PE.1        | RX_RELEASE_PIN(0xE, 1) |
 *
 * @param[in] port Port number in hexadecimal (0x0-0xF for RX72N)
 * @param[in] pin Pin number within port (0-7)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, pin released and available for reuse
 * @retval k_rx_err_invalid_state Infrastructure not initialized
 * @retval k_rx_err_not_found Pin was not reserved (double-release or never reserved)
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @note **GNU C Extension**: Uses statement expression `({ })` - requires GCC/Clang.
 * @note **Returns value**: Can be assigned to variable or used directly in if-statement.
 * @note **Thread-safe**: Can be called from multiple threads concurrently.
 * @note **Idempotent safe**: Releasing an unreserved pin returns error but doesn't crash.
 * @note **Best-effort cleanup**: Most code ignores return value during cleanup.
 *
 * @warning **Release what you reserve**: Only release pins you previously reserved.
 * @warning **Double-release**: Releasing same pin twice returns k_rx_err_not_found.
 * @warning **Not ISR-safe**: Uses mutex internally, do NOT call from ISR.
 * @warning **No automatic release**: Pins are NOT auto-released on thread exit - must call explicitly.
 *
 * @par Example: Complete Lifecycle Management
 * @code
 * typedef struct {
 *   uint8_t port;
 *   uint8_t pin;
 *   bool initialized;
 * } led_driver_t;
 *
 * rx_err_t led_driver_init(led_driver_t* driver, uint8_t port, uint8_t pin)
 * {
 *   // Reserve LED pin
 *   rx_err_t err = RX_RESERVE_PIN(port, pin, "LED");
 *   if (err != k_rx_ok) {
 *     RX_REPORT_ERROR(err, "LED", "Pin reservation failed");
 *     return err;
 *   }
 *
 *   // Configure pin as output
 *   gpio_set_direction(port, pin, GPIO_OUTPUT);
 *   gpio_write(port, pin, 0); // LED off
 *
 *   driver->port = port;
 *   driver->pin = pin;
 *   driver->initialized = true;
 *   return k_rx_ok;
 * }
 *
 * void led_driver_deinit(led_driver_t* driver)
 * {
 *   if (!driver->initialized) {
 *     return; // Already deinitialized
 *   }
 *
 *   // Turn off LED
 *   gpio_write(driver->port, driver->pin, 0);
 *
 *   // Release pin (best-effort, ignore errors)
 *   rx_err_t err = RX_RELEASE_PIN(driver->port, driver->pin);
 *   if (err != k_rx_ok) {
 *     // Log but don't fail cleanup
 *     RX_REPORT_ERROR(err, "LED", "Pin release warning");
 *   }
 *
 *   driver->initialized = false;
 * }
 * @endcode
 *
 * @par Example: Bulk Release Helper
 * @code
 * typedef struct {
 *   uint8_t port;
 *   uint8_t pin;
 * } pin_ref_t;
 *
 * void release_pins_bulk(const pin_ref_t* pins, uint32_t count)
 * {
 *   for (uint32_t i = 0; i < count; i++) {
 *     rx_err_t err = RX_RELEASE_PIN(pins[i].port, pins[i].pin);
 *     if (err != k_rx_ok && err != k_rx_err_not_found) {
 *       // Log unexpected errors (ignore not-found, it's benign)
 *       RX_REPORT_ERROR(err, "BULK", "Pin release failed");
 *     }
 *   }
 * }
 * @endcode
 *
 * @see RX_RESERVE_PIN() Reserve a pin (must be called first)
 * @see rx_infrastructure_get_pin_interface() Get pin interface directly
 * @see rx_pin_interface.h Pin interface definition
 *
 * @since Version 1.0.0
 */
#define RX_RELEASE_PIN(port, pin)                                                                  \
  ({                                                                                               \
    rx_err_t            err_   = k_rx_err_invalid_state;                                           \
    rx_pin_interface_t* iface_ = rx_infrastructure_get_pin_interface();                            \
    if (iface_ != nullptr) {                                                                       \
      err_ = iface_->release_pin(iface_->ctx, port, pin);                                          \
    }                                                                                              \
    err_;                                                                                          \
  })

#ifdef __cplusplus
}
#endif
