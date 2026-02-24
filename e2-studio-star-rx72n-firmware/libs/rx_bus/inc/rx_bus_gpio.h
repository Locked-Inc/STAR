/* lib/rx_bus/inc/rx_bus_gpio.h */

/**
 * @file rx_bus_gpio.h
 * @brief GPIO (General Purpose Input/Output) Bus Abstraction for RX72N
 *
 * @details
 * Provides bus manager integration for digital GPIO operations on the Renesas
 * RX72N microcontroller. Wraps the low-level PORT HAL with the bus abstraction
 * pattern for consistent API across all bus types (I2C, SPI, UART, 1-Wire, GPIO)
 * with mutex-protected access and pin conflict detection.
 *
 * @par System Architecture
 * @dot
 * digraph gpio_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_api {
 *     label="GPIO Bus API (This Module)";
 *     style=filled;
 *     color=lightyellow;
 *     init [label="rx_bus_gpio_init()"];
 *     write [label="rx_bus_gpio_write()"];
 *     read [label="rx_bus_gpio_read()"];
 *     toggle [label="rx_bus_gpio_toggle()"];
 *   }
 *
 *   subgraph cluster_bus {
 *     label="Bus Manager";
 *     style=filled;
 *     color=lightblue;
 *     manager [label="rx_bus_manager_t\nMutex protection\nBus lookup"];
 *     config [label="rx_bus_config_t\nPort, Pin\nDirection config"];
 *   }
 *
 *   subgraph cluster_validator {
 *     label="Pin Validator";
 *     style=filled;
 *     color=lightcyan;
 *     validator [label="Pin Conflict Detection\nReservation Tracking"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="PORT HAL Layer";
 *     style=filled;
 *     color=lightgreen;
 *     port [label="port_init()\nport_write()\nport_read()"];
 *     regs [label="PORT0-J\nRegister Access"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="GPIO Hardware";
 *     style=filled;
 *     color=lightcoral;
 *     led [label="LEDs\nActive Low"];
 *     button [label="Buttons\nPull-up"];
 *     enable [label="Peripheral Enable\nChip Select"];
 *     detect [label="Sensor Detect\nInterrupt"];
 *   }
 *
 *   init -> manager;
 *   write -> manager;
 *   read -> manager;
 *   toggle -> manager;
 *   manager -> config;
 *   manager -> validator;
 *   config -> port;
 *   port -> regs;
 *   regs -> led;
 *   regs -> button;
 *   regs -> enable;
 *   regs -> detect;
 * }
 * @enddot
 *
 * @par GPIO Overview
 * General Purpose Input/Output (GPIO) provides direct digital control of MCU pins:
 * - **Digital I/O**: Each pin can be configured as input or output
 * - **Direct control**: Software directly reads/writes pin state
 * - **Fast response**: Single instruction latency (~1 clock cycle)
 * - **Pin multiplexing**: GPIO shares pins with peripherals (UART, I2C, etc.)
 * - **Conflict detection**: Pin validator prevents double allocation
 *
 * @par Pin Configuration
 * @verbatim
 * RX72N GPIO Configuration Registers (per port):
 *
 * PDR (Port Direction Register):  0 = Input, 1 = Output
 * PODR (Port Output Data):        Written value (output mode)
 * PIDR (Port Input Data):         Read value (input/output mode)
 * PMR (Port Mode Register):       0 = GPIO, 1 = Peripheral
 * PCR (Pull-up Control):          0 = Disabled, 1 = Enabled
 * ODR (Open-Drain Control):       0 = CMOS, 1 = Open-drain
 *
 * Example: Configure P05 as output:
 *   PORT0.PMR.BIT.B5 = 0;    // GPIO mode (not peripheral)
 *   PORT0.PDR.BIT.B5 = 1;    // Output direction
 *   PORT0.PODR.BIT.B5 = 1;   // Set high
 * @endverbatim
 *
 * @par GPIO Pin Usage
 * Application-level GPIO assignments are defined in hardware_config.h.
 * @see hardware_config.h Complete pin assignment table
 *
 * @par Electrical Characteristics
 * | Parameter | Min | Typ | Max | Unit | Notes |
 * |-----------|-----|-----|-----|------|-------|
 * | VDD | 2.7 | 3.3 | 3.6 | V | Supply voltage |
 * | V_OH (High) | VDD-0.4 | - | VDD | V | Output high @ 4mA |
 * | V_OL (Low) | 0 | - | 0.4 | V | Output low @ 4mA |
 * | V_IH (High) | 0.7*VDD | - | VDD | V | Input high threshold |
 * | V_IL (Low) | 0 | - | 0.3*VDD | V | Input low threshold |
 * | I_drive | - | - | 8 | mA | Maximum per pin |
 * | I_pullup | 30 | 50 | 100 | uA | Internal pull-up |
 * | C_in | - | 10 | - | pF | Input capacitance |
 *
 * @par Performance Characteristics
 * | Operation | Execution Time | Description |
 * |-----------|----------------|-------------|
 * | Init | ~15 us | Configure direction + pin validator |
 * | Write | ~2 us | Set output state (with mutex) |
 * | Read | ~2 us | Read input state (with mutex) |
 * | Toggle | ~3 us | Read-modify-write (with mutex) |
 * | Propagation | 10-20 ns | Output change to pin edge |
 * | Response | < 1 us | Input change to software read |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code (.text) | ~800 bytes | All API functions |
 * | Stack | 32 bytes | Maximum call depth |
 * | Pin validator | 16 bytes | Pin reservation bitmap |
 * | No heap | 0 | Static allocation only |
 *
 * @par Hardware Requirements
 * | Requirement | Specification |
 * |-------------|---------------|
 * | VDD | 3.3V +/- 10% |
 * | I/O levels | 3.3V CMOS |
 * | Drive strength | 8 mA maximum per pin |
 * | Total current | 80 mA maximum per port |
 * | Rise/fall time | < 10 ns @ 50 pF load |
 * | Input leakage | +/- 1 uA maximum |
 *
 * @par Pin Conflict Detection
 * The bus manager uses a pin validator to prevent accidental double allocation:
 * - Each GPIO pin can be reserved by only ONE bus/peripheral
 * - rx_bus_gpio_init() checks validator before configuring pin
 * - Returns k_rx_err_conflict if pin already in use
 * - Prevents bugs like LED sharing UART TX pin
 *
 * @par Pin Reservation Table
 * | Error | Cause | Example |
 * |-------|-------|---------|
 * | k_rx_err_conflict | Pin already reserved | P05 used by LED and SPI CS |
 * | k_rx_err_invalid_pin | Pin number out of range | Port 0 pin 8 (only 0-7 exist) |
 * | k_rx_err_peripheral_active | Pin used by peripheral | P26 is SCI1 TX |
 *
 * @par Thread Safety
 * All functions are thread-safe when used with the bus manager:
 * - Bus manager provides mutex protection per GPIO operation
 * - Timeout on mutex acquisition prevents deadlock
 * - Safe to call from multiple ThreadX threads
 * - Read-modify-write operations (toggle) are atomic
 *
 * @par Module Dependencies
 * - rx_bus_manager.h: Bus abstraction and mutex protection
 * - rx_err.h: Error code definitions
 * - rx_port_utils.h: Low-level PORT hardware access
 * - rx_pin_validator.h: Pin conflict detection and reservation
 * - rx_mpc.h: Pin function selection (GPIO vs peripheral)
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops have fixed bounds (none in this module)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Functions under 60 lines
 * - Rule 5: [OK] Input validation via RX_CHECK_NULL_PTR
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Limited preprocessor (typed enums preferred)
 * - Rule 9: [OK] Function pointers only for bus abstraction (DIP)
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single Responsibility - GPIO digital I/O operations only
 * - **O**: Open/Closed - Extensible via bus manager config
 * - **L**: Liskov Substitution - Implements rx_bus_interface_t
 * - **I**: Interface Segregation - Minimal 4-function API
 * - **D**: Dependency Inversion - Depends on abstractions (bus manager, pin validator)
 *
 * @par Usage Example - LED Control
 * @code
 * // Initialize bus manager
 * rx_bus_manager_t manager;
 * rx_bus_manager_init(&manager);
 *
 * // Register GPIO bus for status LED (P05 - red LED, active low)
 * rx_bus_config_t led_config = {
 *     .type = k_rx_bus_type_gpio,
 *     .gpio = {
 *         .port = 0,     // PORT0
 *         .pin = 5,      // Bit 5
 *     }
 * };
 * rx_bus_register(&manager, "led_red", &led_config);
 *
 * // Initialize as output
 * rx_err_t err = rx_bus_gpio_init(&manager, "led_red", true);  // true = output
 * if (err != k_rx_ok) {
 *     // Handle error
 *     return err;
 * }
 *
 * // Turn LED on (active low - write false)
 * rx_bus_gpio_write(&manager, "led_red", false);
 *
 * // Blink LED
 * while (1) {
 *     rx_bus_gpio_toggle(&manager, "led_red");
 *     tx_thread_sleep(50);  // 500 ms
 * }
 * @endcode
 *
 * @par Usage Example - Button Input
 * @code
 * // Register GPIO bus for user button (P20 - active low with pull-up)
 * rx_bus_config_t button_config = {
 *     .type = k_rx_bus_type_gpio,
 *     .gpio = {
 *         .port = 2,
 *         .pin = 0,
 *     }
 * };
 * rx_bus_register(&manager, "button_user", &button_config);
 *
 * // Initialize as input
 * err = rx_bus_gpio_init(&manager, "button_user", false);  // false = input
 *
 * // Read button state
 * bool button_pressed = false;
 * err = rx_bus_gpio_read(&manager, "button_user", &button_pressed);
 * if (err == k_rx_ok) {
 *     if (!button_pressed) {  // Active low
 *         // Button is pressed
 *         handle_button_press();
 *     }
 * }
 * @endcode
 *
 * @par Usage Example - Peripheral Enable Control
 * @code
 * // Register GPIO for motor driver nSLEEP (enable) pin
 * rx_bus_config_t motor_enable_config = {
 *     .type = k_rx_bus_type_gpio,
 *     .gpio = {
 *         .port = 3,
 *         .pin = 4,
 *     }
 * };
 * rx_bus_register(&manager, "motor_enable", &motor_enable_config);
 *
 * // Initialize as output
 * rx_bus_gpio_init(&manager, "motor_enable", true);
 *
 * // Enable motor driver
 * rx_bus_gpio_write(&manager, "motor_enable", true);  // Active high
 *
 * // ... motor operations ...
 *
 * // Disable motor driver (low power)
 * rx_bus_gpio_write(&manager, "motor_enable", false);
 * @endcode
 *
 * @see rx_bus_manager.h Bus manager API for registration and lookup
 * @see rx_bus_types.h Bus type definitions and configurations
 * @see rx_port_utils.h Low-level PORT hardware access
 * @see rx_pin_validator.h Pin conflict detection
 *
 * @version 1.0.0
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize GPIO pin through bus manager
 *
 * @details
 * Configures a GPIO pin for digital input or output operation. This function
 * reserves the pin through the pin validator (preventing conflicts with other
 * peripherals), configures the pin as GPIO mode (not peripheral), and sets the
 * data direction. This function must be called before any read/write operations.
 *
 * The initialization process:
 * 1. Acquires bus manager mutex (thread-safe)
 * 2. Looks up bus configuration by name
 * 3. Validates bus type is GPIO
 * 4. Checks pin validator for conflicts
 * 5. Reserves pin in validator
 * 6. Configures MPC for GPIO mode (PMR = 0)
 * 7. Sets PORT direction (PDR = output/input)
 * 8. Releases mutex
 *
 * @par Initialization Sequence
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="rx_bus_gpio_init()", shape=ellipse];
 *   mutex [label="Acquire mutex"];
 *   lookup [label="Lookup bus by name"];
 *   validate [label="Validate bus type"];
 *   check [label="Check pin validator", fillcolor=lightblue, style=filled];
 *   reserve [label="Reserve pin", fillcolor=lightblue, style=filled];
 *   mpc [label="Configure MPC\nPMR = 0 (GPIO)"];
 *   pdr [label="Set direction\nPDR = output/input"];
 *   release [label="Release mutex"];
 *   done [label="Return", shape=ellipse];
 *   error [label="Conflict Error", shape=ellipse, fillcolor=red, style=filled];
 *
 *   start -> mutex;
 *   mutex -> lookup;
 *   lookup -> validate;
 *   validate -> check;
 *   check -> reserve [label="Available"];
 *   check -> error [label="In Use"];
 *   reserve -> mpc;
 *   mpc -> pdr;
 *   pdr -> release;
 *   release -> done;
 * }
 * @enddot
 *
 * @par Pin Conflict Detection:
 * The pin validator maintains a reservation table for all MCU pins. This prevents
 * common bugs:
 * - **Same pin, multiple functions**: P05 as both LED and SPI CS
 * - **GPIO vs peripheral**: P26 as GPIO and SCI1 TX simultaneously
 * - **Bus manager collisions**: Two different bus entries for same pin
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must have GPIO bus registered via rx_bus_register()
 * @param[in] bus_name GPIO bus name
 *   - Null-terminated string
 *   - Must match name used in rx_bus_register()
 *   - Maximum 31 characters
 * @param[in] output Pin direction
 *   - true: Configure as output (PDR = 1)
 *   - false: Configure as input (PDR = 0)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, pin initialized and reserved
 * @retval k_rx_err_null_ptr nullptr in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_arg Bus is not GPIO type
 * @retval k_rx_err_timeout Mutex acquisition timeout (default 1000ms)
 * @retval k_rx_err_conflict Pin already reserved by another bus/peripheral
 * @retval k_rx_err_invalid_pin Port or pin number out of valid range
 *
 * @pre manager must be initialized via rx_bus_manager_init()
 * @pre Bus must be registered via rx_bus_register() with type k_rx_bus_type_gpio
 * @pre Pin must not be reserved by another peripheral
 *
 * @post Pin reserved in pin validator (cannot be reused)
 * @post Pin configured as GPIO mode (PMR = 0)
 * @post Pin direction set (PDR = output/input)
 * @post Bus state set to initialized
 *
 * @invariant Pin reservation is permanent until system reset
 *
 * @note Thread-safe via bus manager mutex
 * @note Call only once per pin; subsequent calls return k_rx_err_conflict
 * @note Pin reservation persists for lifetime of system
 *
 * @warning Pin conflict detection requires pin validator to be enabled
 * @warning Do not call from interrupt context (mutex wait)
 *
 * @par Performance:
 * - Execution time: ~15 us typical (includes pin validator check)
 * - Mutex timeout: 1000 ms (configurable via bus manager)
 *
 * @par Example - Output Initialization:
 * @code
 * // Configure P05 as output for LED
 * rx_bus_config_t led_config = {
 *     .type = k_rx_bus_type_gpio,
 *     .gpio = { .port = 0, .pin = 5 }
 * };
 * rx_bus_register(&manager, "led_status", &led_config);
 *
 * rx_err_t err = rx_bus_gpio_init(&manager, "led_status", true);  // Output
 * if (err == k_rx_err_conflict) {
 *     rx_log_error("GPIO", "P05 already in use!");
 * } else if (err != k_rx_ok) {
 *     rx_log_error("GPIO", "Init failed: %d", err);
 * }
 * @endcode
 *
 * @par Example - Input Initialization:
 * @code
 * // Configure P20 as input for button
 * rx_bus_config_t button_config = {
 *     .type = k_rx_bus_type_gpio,
 *     .gpio = { .port = 2, .pin = 0 }
 * };
 * rx_bus_register(&manager, "button_user", &button_config);
 *
 * err = rx_bus_gpio_init(&manager, "button_user", false);  // Input
 * if (err != k_rx_ok) {
 *     // Handle error
 *     return err;
 * }
 * @endcode
 *
 * @see rx_bus_manager_init() Initialize bus manager first
 * @see rx_bus_register() Register bus before initialization
 * @see rx_bus_gpio_write() Write output state
 * @see rx_bus_gpio_read() Read input state
 * @see rx_pin_validator.h Pin conflict detection
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_bus_gpio_init(rx_bus_manager_t* manager, const char* bus_name, bool output);

/**
 * @brief Write GPIO output through bus manager
 *
 * @details
 * Sets the output state of a GPIO pin configured as output. Writes directly to
 * the PORT output data register (PODR). The change propagates to the physical
 * pin within 10-20 ns.
 *
 * @par Algorithm Steps:
 * 1. Validate pointers (manager, bus_name)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration
 * 4. Validate bus initialized
 * 5. Validate pin is output mode
 * 6. Write value to PODR register
 * 7. Release mutex
 * 8. Return success
 *
 * @par Register Access:
 * For port P and pin n:
 * - Output high: `PORTP.PODR.BIT.Bn = 1`
 * - Output low:  `PORTP.PODR.BIT.Bn = 0`
 *
 * Propagation delay to pin: 10-20 ns typical @ 50 pF load
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name GPIO bus name
 *   - Must match registered bus
 * @param[in] value Output state
 *   - true: Set pin high (VDD)
 *   - false: Set pin low (GND)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, output state changed
 * @retval k_rx_err_null_ptr nullptr in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized or pin is input
 * @retval k_rx_err_timeout Mutex acquisition timeout
 *
 * @pre Bus must be initialized via rx_bus_gpio_init() with output = true
 * @pre Pin must be configured as output
 *
 * @post Pin output state changed to value
 * @post Physical pin voltage: high ~ VDD, low ~ 0V (within 10-20 ns)
 *
 * @invariant Bus state remains initialized
 *
 * @note Thread-safe via bus manager mutex
 * @note Fast operation (~2 us including mutex)
 * @note Output immediately reflects on physical pin
 *
 * @warning Attempting to write to input pin returns k_rx_err_invalid_state
 * @warning Exceeding 8 mA drive current may damage pin
 *
 * @par Performance:
 * - Execution time: ~2 us (with mutex)
 * - Propagation delay: 10-20 ns to pin edge
 *
 * @par Example - LED Control:
 * @code
 * // Turn LED on (assuming active low)
 * rx_err_t err = rx_bus_gpio_write(&manager, "led_red", false);
 * if (err != k_rx_ok) {
 *     rx_log_error("GPIO", "LED write failed");
 * }
 *
 * // Turn LED off
 * rx_bus_gpio_write(&manager, "led_red", true);
 * @endcode
 *
 * @par Example - Motor Driver Enable:
 * @code
 * // Wake DRV8263H motor driver (nSLEEP = HIGH to exit sleep, tWAKE ~1 ms)
 * err = rx_bus_gpio_write(&manager, "motor_enable", true);
 * tx_thread_sleep(k_motor_wake_ticks);  // 1 tick @ 100 Hz = 10 ms >= tWAKE
 *
 * // Disable motor driver (low power mode)
 * rx_bus_gpio_write(&manager, "motor_enable", false);
 * @endcode
 *
 * @see rx_bus_gpio_init() Initialize GPIO as output first
 * @see rx_bus_gpio_read() Read current output state (via PIDR)
 * @see rx_bus_gpio_toggle() Toggle output state
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_bus_gpio_write(rx_bus_manager_t* manager, const char* bus_name, bool value);

/**
 * @brief Read GPIO input through bus manager
 *
 * @details
 * Reads the current state of a GPIO pin. Can be used for both input and output
 * pins (reading output pins returns the actual pin state via PIDR, useful for
 * verifying output or detecting external conflicts).
 *
 * @par Algorithm Steps:
 * 1. Validate all pointers (manager, bus_name, value)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration
 * 4. Validate bus initialized
 * 5. Read PIDR register (port input data)
 * 6. Set *value = pin state
 * 7. Release mutex
 * 8. Return success
 *
 * @par Register Access:
 * For port P and pin n:
 * - Read: `value = PORTP.PIDR.BIT.Bn`
 *
 * PIDR always reflects actual pin state:
 * - **Input mode**: Reads external signal
 * - **Output mode**: Reads back written value (or external override)
 *
 * @par Input Voltage Thresholds:
 * | Voltage | Interpretation |
 * |---------|----------------|
 * | < 0.3*VDD | Logic low (false) |
 * | > 0.7*VDD | Logic high (true) |
 * | 0.3-0.7*VDD | Undefined (avoid) |
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name GPIO bus name
 *   - Must match registered bus
 * @param[out] value Pin state
 *   - true: Pin is high (> 0.7*VDD)
 *   - false: Pin is low (< 0.3*VDD)
 *   - Valid only if k_rx_ok returned
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, value contains pin state
 * @retval k_rx_err_null_ptr nullptr in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout Mutex acquisition timeout
 *
 * @pre Bus must be initialized via rx_bus_gpio_init()
 * @pre value pointer must be valid
 *
 * @post value contains current pin state
 * @post Pin state unchanged (non-destructive read)
 *
 * @invariant Bus state remains initialized
 *
 * @note Thread-safe via bus manager mutex
 * @note Works for both input and output pins
 * @note Fast operation (~2 us including mutex)
 * @note For inputs, enable internal pull-up if needed (via PORT PCR register)
 *
 * @warning value undefined if return != k_rx_ok
 * @warning Floating inputs may read unpredictably (use pull-up/down)
 *
 * @par Performance:
 * - Execution time: ~2 us (with mutex)
 * - Sampling delay: < 1 us from pin change to software read
 *
 * @par Example - Button Reading:
 * @code
 * bool button_state = false;
 * rx_err_t err = rx_bus_gpio_read(&manager, "button_user", &button_state);
 * if (err == k_rx_ok) {
 *     if (!button_state) {  // Active low (pulled to ground when pressed)
 *         // Button pressed
 *         handle_button_press();
 *     }
 * }
 * @endcode
 *
 * @par Example - Sensor Detect:
 * @code
 * // Read sensor presence detect pin
 * bool sensor_present = false;
 * err = rx_bus_gpio_read(&manager, "sensor_detect", &sensor_present);
 * if (err == k_rx_ok && sensor_present) {
 *     // Sensor detected, initialize it
 *     init_sensor();
 * }
 * @endcode
 *
 * @par Example - Output Verification:
 * @code
 * // Verify motor enable signal is actually high
 * rx_bus_gpio_write(&manager, "motor_enable", true);
 * bool actual_state = false;
 * rx_bus_gpio_read(&manager, "motor_enable", &actual_state);
 * if (!actual_state) {
 *     // Output not high - possible short to ground or hardware fault
 *     rx_log_error("GPIO", "Motor enable shorted!");
 * }
 * @endcode
 *
 * @see rx_bus_gpio_init() Initialize GPIO first
 * @see rx_bus_gpio_write() Write output state
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_bus_gpio_read(rx_bus_manager_t* manager, const char* bus_name, bool* value);

/**
 * @brief Toggle GPIO output through bus manager
 *
 * @details
 * Inverts the current state of a GPIO output pin (high -> low, low -> high).
 * Performs an atomic read-modify-write operation protected by mutex. Useful
 * for LED blinking and signal generation.
 *
 * @par Algorithm Steps:
 * 1. Validate pointers (manager, bus_name)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration
 * 4. Validate bus initialized and pin is output
 * 5. Read current state from PIDR
 * 6. Write inverted state to PODR
 * 7. Release mutex
 * 8. Return success
 *
 * @par Atomic Read-Modify-Write:
 * The toggle operation is atomic (no race conditions):
 * ```
 * current = PORTP.PIDR.BIT.Bn;    // Read actual pin state
 * PORTP.PODR.BIT.Bn = !current;   // Write inverted state
 * ```
 * Mutex ensures no other thread can modify pin between read and write.
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name GPIO bus name
 *   - Must match registered bus
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, pin state toggled
 * @retval k_rx_err_null_ptr nullptr in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized or pin is input
 * @retval k_rx_err_timeout Mutex acquisition timeout
 *
 * @pre Bus must be initialized via rx_bus_gpio_init() with output = true
 * @pre Pin must be configured as output
 *
 * @post Pin output state inverted (high <-> low)
 * @post Physical pin reflects new state within 10-20 ns
 *
 * @invariant Bus state remains initialized
 *
 * @note Thread-safe - atomic read-modify-write via mutex
 * @note Slightly slower than write (~3 us vs ~2 us) due to read
 * @note Useful for periodic signals (LED blink, clock generation)
 *
 * @warning Attempting to toggle input pin returns k_rx_err_invalid_state
 * @warning High-frequency toggling (>100 kHz) should use direct register access
 *
 * @par Performance:
 * - Execution time: ~3 us (read + write with mutex)
 * - Maximum toggle rate: ~330 kHz (not recommended - use PWM instead)
 * - Practical toggle rate: < 10 kHz for bus manager API
 *
 * @par Example - LED Blinking:
 * @code
 * // Simple LED blink in loop
 * while (1) {
 *     rx_bus_gpio_toggle(&manager, "led_status");
 *     tx_thread_sleep(50);  // 500 ms period = 1 Hz blink
 * }
 * @endcode
 *
 * @par Example - Heartbeat LED:
 * @code
 * // ThreadX task for heartbeat LED
 * void heartbeat_task_entry(ULONG arg) {
 *     rx_bus_manager_t* mgr = (rx_bus_manager_t*)arg;
 *     while (1) {
 *         rx_bus_gpio_toggle(mgr, "led_heartbeat");
 *         tx_thread_sleep(100);  // 1 second period
 *     }
 * }
 * @endcode
 *
 * @par Example - Test Signal Generation:
 * @code
 * // Generate 1 kHz square wave for testing (GPIO-based, not PWM)
 * for (uint32_t i = 0; i < 1000; i++) {
 *     rx_bus_gpio_toggle(&manager, "test_signal");
 *     tx_thread_sleep(1);  // 500 us delay = 1 kHz
 * }
 * // Note: Use hardware PWM for precise/high-frequency signals
 * @endcode
 *
 * @see rx_bus_gpio_init() Initialize GPIO as output first
 * @see rx_bus_gpio_write() Set explicit state (faster if known)
 * @see rx_bus_gpio_read() Read current state before deciding
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_gpio_toggle(rx_bus_manager_t* manager, const char* bus_name);

#ifdef __cplusplus
}
#endif
