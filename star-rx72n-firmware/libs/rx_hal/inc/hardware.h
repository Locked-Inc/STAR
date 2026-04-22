/**
 * @file hardware.h
 * @brief Hardware Abstraction Layer (HAL) - Unified Interface for RX72N Peripherals
 *
 * @details
 * ## Overview
 * Comprehensive HAL providing type-safe, unified API for all RX72N microcontroller peripherals
 * used in the STAR motor controller. Abstracts hardware register manipulation and provides
 * safe, validated access to GPIO, timers, ADC, I2C, SPI, and UART peripherals.
 *
 * ## Supported Peripherals
 *
 * | Peripheral | Channels | Purpose | Configuration |
 * |------------|----------|---------|---------------|
 * | **GPIO** | 14 ports (A-N) | Digital I/O, motor control signals | Output/Input, Read/Write/Toggle |
 * | **CMT** | 4 channels | ThreadX system tick (100 Hz) | CMT0 configured for 10ms intervals |
 * | **S12AD** | 2 units (0-1) | Motor current sensing, analog inputs | 8/10/12-bit resolution |
 * | **RIIC** | 3 channels (0-2) | I2C sensors (IMU); temperature sensor uses 1-Wire | 100kHz, 400kHz, 1MHz |
 * | **RSPI** | 3 channels (0-2) | SPI: RPi5 comm (peripheral), sensors (controller) | Mode 0-3, 100kHz-10MHz |
 * | **SCI** | 13 channels (0-12) | UART: Debug (SCI9 @ 115200), expansion | Configurable baud rates |
 *
 * ## Hardware Architecture
 *
 * @dot
 * digraph hal_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   app [label="Application Code\n(Motor Control, USB, Protocol)", shape=component];
 *   hal [label="hardware.h\n(HAL API)", style="filled", fillcolor="lightblue"];
 *
 *   subgraph cluster_peripherals {
 *     label="RX72N Peripherals";
 *     gpio [label="GPIO\n(Ports A-N)"];
 *     cmt [label="CMT0\n(Timer)"];
 *     adc [label="S12AD0/1\n(ADC)"];
 *     riic [label="RIIC0-2\n(I2C)"];
 *     rspi [label="RSPI0-2\n(SPI)"];
 *     sci [label="SCI0-12\n(UART)"];
 *   }
 *
 *   subgraph cluster_external {
 *     label="External Hardware";
 *     rpi [label="Raspberry Pi 5\n(SPI Peripheral)"];
 *     drv [label="DRV8263H x4\n(Motor Drivers)"];
 *     imu [label="IMU\n(I2C Sensor)"];
 *     uart_bridge [label="CY7C65213\n(USB-UART Bridge)"];
 *   }
 *
 *   app -> hal;
 *   hal -> gpio;
 *   hal -> cmt;
 *   hal -> adc;
 *   hal -> riic;
 *   hal -> rspi;
 *   hal -> sci;
 *
 *   rspi -> rpi [label="RSPI0\nPeripheral"];
 *   gpio -> drv [label="PWM+GPIO"];
 *   riic -> imu [label="RIIC0\n400kHz"];
 *   sci -> uart_bridge [label="SCI9\n115200"];
 * }
 * @enddot
 *
 * ## Key Design Principles
 *
 * ### 1. Type Safety (C23 Typed Enums + Struct Wrappers)
 * - **Typed enums**: All constants use C23 `: uint8_t` syntax for guaranteed size
 * - **Struct wrappers**: I2C channel/address use struct wrappers to prevent argument swaps
 * - **Compile-time validation**: Invalid types caught at compile time, not runtime
 *
 * **Example - Type Safety Prevents Bugs**:
 * @code{.c}
 * // WRONG - Swapped arguments with plain uint8_t
 * riic_write(0x48, 0, data, len);  // Accidentally swapped channel and address
 *
 * // CORRECT - Struct wrappers catch this at compile time
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t addr = {.value = 0x48};
 * riic_write(ch, addr, data, len);  // Type-checked, no accidental swaps possible
 * @endcode
 *
 * ### 2. Unified Error Handling
 * - **Consistent returns**: All functions return `rx_err_t` (except debug helpers)
 * - **Comprehensive error codes**: Covers NULL pointers, invalid arguments, timeouts, conflicts
 * - **Validation**: All functions validate inputs before hardware access
 *
 * ### 3. Configuration Structs Over Positional Parameters
 * - **Maintainability**: Adding new config options doesn't break API
 * - **Clarity**: Named fields > positional parameters
 * - **Safety**: Reduces parameter order mistakes
 *
 * **Example**:
 * @code{.c}
 * // OLD STYLE (error-prone):
 * uart_init(9, 115200, k_port_b_pin_7, k_port_b_pin_6);  // Which pin is TX vs RX?
 *
 * // NEW STYLE (clear and safe):
 * uart_channel_config_t config = {
 *   .channel = k_uart_channel_9,
 *   .baudrate = 115200,
 *   .tx_gpio = k_port_b_pin_7,
 *   .rx_gpio = k_port_b_pin_6
 * };
 * uart_init_channel(&config);  // Self-documenting
 * @endcode
 *
 * ### 4. Zero Dynamic Allocation (NASA Rule 3)
 * - All functions use stack-based parameters
 * - No malloc/free anywhere in HAL
 * - Predictable memory behavior for safety-critical systems
 *
 * ## Performance Characteristics
 *
 * ### Function Call Overhead (RX72N @ 240 MHz)
 * | Function Category | Cycles | Time | Notes |
 * |-------------------|--------|------|-------|
 * | GPIO write | ~20 | 83 ns | Direct register access |
 * | GPIO read | ~25 | 104 ns | Includes pin validation |
 * | UART putc | ~150 | 625 ns | Polling TDRE flag |
 * | UART puts (10 chars) | ~1500 | 6.25 us | 150 cycles/char |
 * | I2C write (1 byte) | ~24000 | 100 us | 400kHz I2C clock |
 * | SPI transfer (1 byte) | ~480 | 2 us | 10 MHz SPI clock |
 * | ADC read | ~7200 | 30 us | 12-bit conversion + validation |
 *
 * ### Memory Footprint
 * | Component | Flash | RAM | Stack (max) |
 * |-----------|-------|-----|-------------|
 * | GPIO drivers | ~2 KB | 16 bytes (state) | 32 bytes |
 * | UART drivers | ~3 KB | 128 bytes (buffers) | 48 bytes |
 * | I2C drivers | ~4 KB | 64 bytes (state) | 64 bytes |
 * | SPI drivers | ~5 KB | 96 bytes (state) | 64 bytes |
 * | ADC drivers | ~2 KB | 32 bytes (state) | 32 bytes |
 * | **Total HAL** | ~16 KB | ~336 bytes | ~240 bytes |
 *
 * ## Thread Safety
 * - **NOT thread-safe**: Hardware registers are not protected by mutexes
 * - **Single-threaded access**: Each peripheral should be accessed from one thread only
 * - **RTOS integration**: Use ThreadX mutexes if multi-threaded access needed
 * - **Interrupt safety**: All HAL functions disable interrupts during critical sections
 *
 * ## Initialization Sequence
 *
 * Recommended initialization order for STAR motor controller:
 * @code{.c}
 * void hardware_init(void) {
 *   // 1. Debug UART (for early logging)
 *   uart_debug_init();
 *   rx_log_info("INIT", "Starting hardware initialization");
 *
 *   // 2. System timer (for ThreadX tick)
 *   timer_init();
 *
 *   // 3. GPIO (for motor control signals)
 *   gpio_set_output(k_port_c_pin_0);  // Motor enable
 *   gpio_set_output(k_port_c_pin_1);  // Direction
 *
 *   // 4. SPI controller (for sensors; DRV8263H uses PWM+GPIO, not SPI)
 *   rspi_controller_config_t spi_cfg = {
 *     .spi_mode = k_rspi_mode_0,
 *     .freq_hz = k_rspi_freq_1mhz,
 *     .cs = k_port_c_pin_4
 *   };
 *   rspi_init_controller(k_rspi_channel_1, &spi_cfg);
 *
 *   // 5. SPI peripheral (for RPi5 communication)
 *   rspi_config_t rpi_cfg = {
 *     .spi_mode = k_rspi_mode_0,
 *     .use_16bit = false
 *   };
 *   rspi_init_peripheral(k_rspi_channel_0, &rpi_cfg);
 *
 *   // 6. ADC (for motor current sensing on AN007/P47)
 *   adc_init(k_adc_unit_0, k_adc_channel_7, k_adc_resolution_12bit);
 *
 *   // 7. I2C (for IMU sensor)
 *   riic_init((riic_channel_t){.value = 0}, 400000);
 *
 *   rx_log_info("INIT", "Hardware initialization complete");
 * }
 * @endcode
 *
 * ## Pin Assignments (From Project Schematic)
 *
 * See `docs/sections/03_hardware_pinout.tex` for complete pinout documentation.
 *
 * **Critical Signals**:
 * | Signal | Pin | Peripheral | Purpose |
 * |--------|-----|------------|---------|
 * | SPI0_CIPO | P30 | RSPI0 | RPi5 <- RX72N (peripheral mode) |
 * | SPI0_COPI | P31 | RSPI0 | RPi5 -> RX72N |
 * | SPI0_CLK | P32 | RSPI0 | RPi5 clock |
 * | SPI0_CS | P33 | GPIO | RPi5 chip select |
 * | SPI1_CIPO | PC5 | RSPI1 | Not used (RSPI1 controller mode) |
 * | SPI1_COPI | PC6 | RSPI1 | Not used (RSPI1 controller mode) |
 * | SPI1_CLK | PC7 | RSPI1 | Not used (RSPI1 clock) |
 * | UART_TX | PB7 | SCI9 | Debug output (CY7C65213) |
 * | UART_RX | PB6 | SCI9 | Debug input |
 *
 * ## Dependencies
 * - `rx_err.h` - Error code definitions
 * - `rx_check.h` - Validation macros
 * - `rx_log.h` - Logging infrastructure
 * - `rx_port_constants.h` - Port/pin definitions
 * - `rx_bus_types.h` - Bus enumeration types
 * - `rx72n_regs.h` - Hardware register definitions (internal)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp, recursion (pure validation + register access)
 * - **Rule 2 [YES]:** All loops bounded (timeout loops use compile-time max iterations)
 * - **Rule 3 [YES]:** No dynamic allocation (all stack-based or static configuration)
 * - **Rule 4 [YES]:** All functions <60 lines (HAL functions are typically 20-40 lines)
 * - **Rule 5 [YES]:** Minimum 2 validations per function (NULL checks + range validation)
 * - **Rule 6 [YES]:** Data at smallest scope (loop counters, temporary variables)
 * - **Rule 7 [YES]:** All returns validated (rx_err_t checked at every call site)
 * - **Rule 8 [YES]:** C23 typed enums for constants, minimal preprocessor use
 * - **Rule 9 [YES]:** Single-level dereferencing (pointer parameters for output values)
 * - **Rule 10 [YES]:** Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Each peripheral has dedicated functions (GPIO, UART, SPI, I2C, ADC)
 * - **O (Open/Closed):** Config structs allow extension without modifying function signatures
 * - **L (Liskov Substitution):** All HAL functions return rx_err_t with consistent semantics
 * - **I (Interface Segregation):** Separate controller/peripheral SPI APIs, debug UART wrappers
 * - **D (Dependency Inversion):** Depends on abstract rx_err_t, rx_log, not concrete implementations
 *
 * @par Module Dependencies:
 * ```
 * hardware.h
 *   +--> rx_err.h (error codes)
 *   +--> rx_check.h (validation macros)
 *   +--> rx_log.h (logging)
 *   +--> rx_port_constants.h (port/pin enums)
 *   +--> rx_bus_types.h (ADC resolution enums)
 *   +--> [internal] rx72n_regs.h (register definitions)
 *
 * Used by:
 *   +--> Motor control (rx_motor_control.c)
 *   +--> USB communication (rx_usb.c)
 *   +--> Protocol handling (rx_comm_manager.c)
 *   +--> Application tasks (comm, motor, sensors)
 * ```
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see rx_err.h for error code definitions
 * @see rx_port_constants.h for complete port/pin enumerations
 * @see docs/sections/03_hardware_pinout.tex for hardware pinout documentation
 */

#pragma once

#include <stdint.h>

/* Core infrastructure */
#include "rx_check.h"
#include "rx_err.h"
#include "rx_infrastructure.h"
#include "rx_log.h"

/* Port/pin constants and types */
#include "rx_port_constants.h"

/* Bus types (includes ADC enums) */
#include "rx_bus_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as digital output
 *
 * @details
 * Configures the specified GPIO pin as a digital output with push-pull drive.
 * Performs full hardware initialization including port direction register (PDR),
 * port mode register (PMR), and open-drain control (ODR). Validates pin assignment
 * and checks for peripheral conflicts.
 *
 * ## Algorithm
 * 1. Validate pin parameter (port exists, pin number valid)
 * 2. Check pin is not already reserved by another peripheral
 * 3. Unlock port write protection (PWPR register)
 * 4. Set PMR bit to 0 (GPIO mode, not peripheral function)
 * 5. Set PDR bit to 1 (output direction)
 * 6. Ensure ODR is 0 (push-pull, not open-drain)
 * 7. Lock port write protection
 * 8. Mark pin as reserved in allocation tracker
 *
 * ## Performance
 * - **Cycles**: ~120 @ 240 MHz
 * - **Time**: ~500 ns
 * - **Stack**: 8 bytes (pin validation, temporary registers)
 *
 * @param[in] pin GPIO pin identifier from rx_port_constants.h
 *                - Type: rx_port_pin_t (C23 typed enum)
 *                - Format: k_port_X_pin_Y (X = A-N, Y = 0-7)
 *                - Example: k_port_c_pin_0 (Port C, Pin 0)
 *                - Range: All valid combinations defined in rx_port_constants.h
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured as output
 * @retval k_rx_err_gpio_invalid_port Port letter out of range (not A-N)
 * @retval k_rx_err_gpio_invalid_pin Pin number out of range (not 0-7)
 * @retval k_rx_err_gpio_conflict Pin already reserved by UART, SPI, I2C, or other peripheral
 *
 * @pre Pin must not be currently in use by another peripheral
 * @post Pin configured as push-pull output, initial state undefined (use gpio_write_* to set level)
 * @post Pin marked as reserved in internal allocation tracker
 *
 * @note **Thread Safety**: NOT thread-safe. Do not call concurrently from multiple threads.
 * @note **Initial State**: Output level is undefined after configuration. Call gpio_write_high() or gpio_write_low() to set initial state.
 * @note **Peripheral Conflicts**: Automatically detects conflicts with UART, SPI, I2C pin assignments.
 *
 * @warning Changing pin direction while peripheral is active may cause undefined behavior.
 *
 * @par Example: Motor Enable Pin
 * @code{.c}
 * #include "hardware.h"
 *
 * void motor_init(void) {
 *   // Configure motor enable pin as output
 *   rx_err_t err = gpio_set_output(k_port_c_pin_0);
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("MOTOR", "Failed to configure enable pin", err);
 *     return;
 *   }
 *
 *   // Set initial state (motors disabled)
 *   gpio_write_low(k_port_c_pin_0);
 *   rx_log_info("MOTOR", "Enable pin configured");
 * }
 * @endcode
 *
 * @par Example: Multiple GPIO Outputs
 * @code{.c}
 * void configure_leds(void) {
 *   const rx_port_pin_t led_pins[] = {
 *     k_port_e_pin_5,  // LED1
 *     k_port_e_pin_6,  // LED2
 *     k_port_e_pin_7   // LED3
 *   };
 *
 *   for (uint8_t i = 0; i < 3; i++) {
 *     rx_err_t err = gpio_set_output(led_pins[i]);
 *     if (err != k_rx_ok) {
 *       rx_log_error_hex("GPIO", "LED init failed", i, 2);
 *     } else {
 *       gpio_write_low(led_pins[i]);  // LEDs off initially
 *     }
 *   }
 * }
 * @endcode
 *
 * @par Example: Error Handling
 * @code{.c}
 * rx_err_t setup_motor_control_pins(void) {
 *   // Try to configure direction pin
 *   rx_err_t err = gpio_set_output(k_port_c_pin_1);
 *   if (err == k_rx_err_gpio_conflict) {
 *     rx_log_error("MOTOR", "Pin conflict - already used by SPI?");
 *     return err;
 *   } else if (err != k_rx_ok) {
 *     rx_log_error_val("MOTOR", "Unexpected error", err);
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * @see gpio_set_input() Configure pin as digital input
 * @see gpio_write_high() Set output pin to logic high
 * @see gpio_write_low() Set output pin to logic low
 * @see gpio_toggle() Toggle output pin state
 * @see rx_port_constants.h for complete pin definitions
 */
[[nodiscard]] rx_err_t gpio_set_output(rx_port_pin_t pin);

/**
 * @brief Configure GPIO pin as digital input
 *
 * @details
 * Configures the specified GPIO pin as a digital input with high-impedance state.
 * Performs full hardware initialization including port direction register (PDR),
 * port mode register (PMR), and input buffer control. Validates pin assignment
 * and checks for peripheral conflicts.
 *
 * ## Algorithm
 * 1. Validate pin parameter (port exists, pin number valid)
 * 2. Check pin is not already reserved by another peripheral
 * 3. Unlock port write protection (PWPR register)
 * 4. Set PMR bit to 0 (GPIO mode, not peripheral function)
 * 5. Set PDR bit to 0 (input direction)
 * 6. Enable input buffer (ICR register)
 * 7. Lock port write protection
 * 8. Mark pin as reserved in allocation tracker
 *
 * @param[in] pin GPIO pin identifier from rx_port_constants.h
 *                - Type: rx_port_pin_t (C23 typed enum)
 *                - Format: k_port_X_pin_Y (X = A-N, Y = 0-7)
 *                - Example: k_port_e_pin_5 (Port E, Pin 5)
 *                - Range: All valid combinations defined in rx_port_constants.h
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured as input
 * @retval k_rx_err_gpio_invalid_port Port letter out of range (not A-N)
 * @retval k_rx_err_gpio_invalid_pin Pin number out of range (not 0-7)
 * @retval k_rx_err_gpio_conflict Pin already reserved by UART, SPI, I2C, or other peripheral
 *
 * @pre Pin must not be currently in use by another peripheral
 * @pre Pin must correspond to a valid physical pin on the RX72N package
 * @post Pin configured as high-impedance input with input buffer enabled
 * @post Pin marked as reserved in internal allocation tracker
 *
 * @note **Thread Safety**: NOT thread-safe. Do not call concurrently from multiple threads.
 *
 * @par Example: Button Input
 * @code{.c}
 * #include "hardware.h"
 *
 * void button_init(void) {
 *   rx_err_t err = gpio_set_input(k_port_e_pin_5);
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("BTN", "Failed to configure button pin", err);
 *     return;
 *   }
 *   rx_log_info("BTN", "Button input configured");
 * }
 * @endcode
 *
 * @see gpio_set_output() Configure pin as digital output
 * @see gpio_read() Read pin logic level
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Set GPIO output pin to logic high (VDD)
 *
 * @details
 * Drives the specified GPIO pin to logic high by setting the corresponding bit
 * in the Port Output Data Register (PODR). The pin must have been previously
 * configured as output via gpio_set_output(). Writing to an input pin has no
 * effect on the physical pin state.
 *
 * ## Algorithm
 * 1. Validate pin parameter (port exists, pin number valid)
 * 2. Extract port letter and pin number from rx_port_pin_t
 * 3. Set corresponding bit in PODR register (atomic read-modify-write)
 *
 * @param[in] pin GPIO pin identifier from rx_port_constants.h
 *                - Type: rx_port_pin_t (C23 typed enum)
 *                - Format: k_port_X_pin_Y (X = A-N, Y = 0-7)
 *                - Must be configured as output via gpio_set_output()
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Pin successfully driven high
 * @retval k_rx_err_gpio_invalid_port Port letter out of range (not A-N)
 * @retval k_rx_err_gpio_invalid_pin Pin number out of range (not 0-7)
 *
 * @pre Pin must be configured as output via gpio_set_output()
 * @pre Pin port and number must be within valid hardware range
 * @post Pin output level is logic high (VDD, typically 3.3V)
 * @post PODR register bit for the pin is set to 1
 *
 * @note **Thread Safety**: NOT thread-safe. Concurrent writes to pins on the same port may
 *       cause read-modify-write races on the PODR register.
 *
 * @par Example: Enable Motor
 * @code{.c}
 * // Enable motor driver (active high)
 * rx_err_t err = gpio_write_high(k_port_c_pin_0);
 * if (err != k_rx_ok) {
 *   rx_log_error_val("MOTOR", "Failed to enable motor", err);
 * }
 * @endcode
 *
 * @see gpio_write_low() Set output pin to logic low
 * @see gpio_toggle() Toggle output pin state
 * @see gpio_set_output() Configure pin as output first
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Set GPIO output pin to logic low (GND)
 *
 * @details
 * Drives the specified GPIO pin to logic low by clearing the corresponding bit
 * in the Port Output Data Register (PODR). The pin must have been previously
 * configured as output via gpio_set_output(). Writing to an input pin has no
 * effect on the physical pin state.
 *
 * ## Algorithm
 * 1. Validate pin parameter (port exists, pin number valid)
 * 2. Extract port letter and pin number from rx_port_pin_t
 * 3. Clear corresponding bit in PODR register (atomic read-modify-write)
 *
 * @param[in] pin GPIO pin identifier from rx_port_constants.h
 *                - Type: rx_port_pin_t (C23 typed enum)
 *                - Format: k_port_X_pin_Y (X = A-N, Y = 0-7)
 *                - Must be configured as output via gpio_set_output()
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Pin successfully driven low
 * @retval k_rx_err_gpio_invalid_port Port letter out of range (not A-N)
 * @retval k_rx_err_gpio_invalid_pin Pin number out of range (not 0-7)
 *
 * @pre Pin must be configured as output via gpio_set_output()
 * @pre Pin port and number must be within valid hardware range
 * @post Pin output level is logic low (GND, 0V)
 * @post PODR register bit for the pin is cleared to 0
 *
 * @note **Thread Safety**: NOT thread-safe. Concurrent writes to pins on the same port may
 *       cause read-modify-write races on the PODR register.
 *
 * @par Example: Disable Motor
 * @code{.c}
 * // Disable motor driver (active high enable)
 * rx_err_t err = gpio_write_low(k_port_c_pin_0);
 * if (err != k_rx_ok) {
 *   rx_log_error_val("MOTOR", "Failed to disable motor", err);
 * }
 * @endcode
 *
 * @see gpio_write_high() Set output pin to logic high
 * @see gpio_toggle() Toggle output pin state
 * @see gpio_set_output() Configure pin as output first
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Toggle GPIO output pin state (high to low, or low to high)
 *
 * @details
 * Inverts the current output level of the specified GPIO pin by XOR-ing the
 * corresponding bit in the Port Output Data Register (PODR). If the pin is
 * currently high it becomes low, and vice versa. The pin must have been
 * previously configured as output via gpio_set_output().
 *
 * ## Algorithm
 * 1. Validate pin parameter (port exists, pin number valid)
 * 2. Extract port letter and pin number from rx_port_pin_t
 * 3. XOR corresponding bit in PODR register (read-modify-write)
 *
 * @param[in] pin GPIO pin identifier from rx_port_constants.h
 *                - Type: rx_port_pin_t (C23 typed enum)
 *                - Format: k_port_X_pin_Y (X = A-N, Y = 0-7)
 *                - Must be configured as output via gpio_set_output()
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Pin successfully toggled
 * @retval k_rx_err_gpio_invalid_port Port letter out of range (not A-N)
 * @retval k_rx_err_gpio_invalid_pin Pin number out of range (not 0-7)
 *
 * @pre Pin must be configured as output via gpio_set_output()
 * @pre Pin port and number must be within valid hardware range
 * @post Pin output level is inverted from its previous state
 * @post PODR register bit for the pin is toggled
 *
 * @note **Thread Safety**: NOT thread-safe. Concurrent toggles or writes to pins on the same
 *       port may cause read-modify-write races on the PODR register.
 *
 * @par Example: LED Heartbeat
 * @code{.c}
 * // Toggle heartbeat LED every 500 ms
 * void heartbeat_task(void) {
 *   gpio_set_output(k_port_e_pin_5);
 *   while (1) {
 *     gpio_toggle(k_port_e_pin_5);
 *     tx_thread_sleep(50);  // 500 ms at 100 Hz tick
 *   }
 * }
 * @endcode
 *
 * @see gpio_write_high() Set output pin to logic high
 * @see gpio_write_low() Set output pin to logic low
 * @see gpio_set_output() Configure pin as output first
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t gpio_toggle(rx_port_pin_t pin);

/**
 * @brief Read current logic level of GPIO pin
 *
 * @details
 * Reads the current logic level from the specified GPIO pin's Port Input Data Register (PIDR).
 * Works for both input and output pins. For output pins, reads back the actual pin state
 * (not the output register), which may differ if external circuitry pulls the pin.
 *
 * ## Algorithm
 * 1. Validate pin parameter (port exists, pin number valid)
 * 2. Validate value pointer (NULL check)
 * 3. Extract port letter and pin number from rx_port_pin_t
 * 4. Read PIDR register for the specified port
 * 5. Extract bit corresponding to pin number
 * 6. Store result (true if bit=1, false if bit=0) in value pointer
 *
 * ## Performance
 * - **Cycles**: ~25 @ 240 MHz
 * - **Time**: ~104 ns
 * - **Stack**: 8 bytes (pin extraction, register read)
 *
 * @param[in] pin GPIO pin identifier from rx_port_constants.h
 *                - Type: rx_port_pin_t (C23 typed enum)
 *                - Format: k_port_X_pin_Y (X = A-N, Y = 0-7)
 *                - Can be input or output pin
 * @param[out] value Pointer to boolean for storing pin state
 *                   - Must point to valid bool variable
 *                   - Set to true if pin reads logic high (VDD)
 *                   - Set to false if pin reads logic low (GND)
 *                   - Unchanged if function returns error
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Pin level successfully read and stored in *value
 * @retval k_rx_err_gpio_invalid_port Port letter out of range (not A-N)
 * @retval k_rx_err_gpio_invalid_pin Pin number out of range (not 0-7)
 * @retval k_rx_err_null_ptr value pointer is nullptr
 *
 * @pre value pointer must be valid (not nullptr)
 * @post *value contains current pin state if function succeeds
 * @post *value unchanged if function returns error
 *
 * @note **Thread Safety**: NOT thread-safe. Reading PIDR register is atomic but value pointer access is not protected.
 * @note **Output Pins**: For output pins, this reads the actual pin level (PIDR), not the output register (PODR). This detects external pull-up/pull-down or short circuits.
 * @note **Input Pins**: Ensure pin is configured as input via gpio_set_input() before reading for sensor/button inputs.
 *
 * @par Example: Reading Button State
 * @code{.c}
 * #include "hardware.h"
 *
 * void check_button(void) {
 *   // Button connected to Port E Pin 5 (active low with pull-up)
 *   bool button_state;
 *   rx_err_t err = gpio_read(k_port_e_pin_5, &button_state);
 *
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("BUTTON", "Failed to read button", err);
 *     return;
 *   }
 *
 *   if (!button_state) {  // Active low (button pressed)
 *     rx_log_info("BUTTON", "Button pressed");
 *     handle_button_press();
 *   }
 * }
 * @endcode
 *
 * @par Example: Verifying Output State
 * @code{.c}
 * // Set motor enable high
 * gpio_write_high(k_port_c_pin_0);
 *
 * // Verify pin actually went high (detects short circuit)
 * bool actual_state;
 * gpio_read(k_port_c_pin_0, &actual_state);
 * if (!actual_state) {
 *   rx_log_error("MOTOR", "Enable pin stuck low - possible short circuit");
 *   emergency_shutdown();
 * }
 * @endcode
 *
 * @par Example: Polling Multiple Inputs
 * @code{.c}
 * typedef struct {
 *   rx_port_pin_t pin;
 *   const char* name;
 * } sensor_input_t;
 *
 * void poll_sensors(void) {
 *   const sensor_input_t sensors[] = {
 *     {k_port_a_pin_0, "LIMIT_SWITCH_1"},
 *     {k_port_a_pin_1, "LIMIT_SWITCH_2"},
 *     {k_port_a_pin_2, "ESTOP"}
 *   };
 *
 *   for (uint8_t i = 0; i < 3; i++) {
 *     bool state;
 *     if (gpio_read(sensors[i].pin, &state) == k_rx_ok) {
 *       rx_log_debug_val(sensors[i].name, "State", state ? 1 : 0);
 *     }
 *   }
 * }
 * @endcode
 *
 * @see gpio_set_input() Configure pin as digital input
 * @see gpio_set_output() Configure pin as digital output
 * @see gpio_write_high() Set output pin to logic high
 * @see gpio_write_low() Set output pin to logic low
 */
[[nodiscard]] rx_err_t gpio_read(rx_port_pin_t pin, bool* value);

/* =============================================================================
 * Timer Functions
 * =============================================================================
 */

/**
 * @brief Initialize CMT0 for ThreadX system tick (100 Hz)
 *
 * @details
 * Configures Compare Match Timer channel 0 (CMT0) to generate periodic interrupts
 * at 100 Hz (10 ms interval) for the ThreadX RTOS system tick. Performs full
 * hardware initialization including module stop release, clock source selection
 * (PCLKB/8), compare match constant calculation, and interrupt priority setup.
 *
 * ## Algorithm
 * 1. Release CMT0 from module stop (MSTPCRA.MSTPA15 = 0)
 * 2. Stop counter (CMSTR0.STR0 = 0)
 * 3. Configure CMCR: PCLKB/8 clock source, compare match interrupt enable
 * 4. Calculate CMCOR from PCLKB frequency and desired 100 Hz rate
 * 5. Clear counter (CMCNT = 0)
 * 6. Set compare match value (CMCOR)
 * 7. Configure interrupt priority (IPR = 5)
 * 8. Start counter (CMSTR0.STR0 = 1)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok CMT0 successfully initialized and running at 100 Hz
 * @retval k_rx_err_invalid_state CMT0 already initialized
 *
 * @pre System clocks must be configured (PCLKB running at expected frequency)
 * @pre CMT0 must not be already initialized (call timer_stop() first to reinitialize)
 * @post CMT0 generating 100 Hz interrupts for ThreadX system tick
 * @post CMT0 interrupt enabled at priority 5
 *
 * @note **Thread Safety**: NOT thread-safe. Must be called once during system initialization
 *       before ThreadX is started.
 *
 * @par Example: System Initialization
 * @code{.c}
 * void hardware_init(void) {
 *   rx_err_t err = timer_init();
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("TIMER", "CMT0 init failed", err);
 *     return;
 *   }
 *   rx_log_info("TIMER", "System tick running at 100 Hz");
 * }
 * @endcode
 *
 * @see timer_stop() Stop the CMT0 timer
 * @see timer_get_count() Read current counter value
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t timer_init(void);

/**
 * @brief Stop CMT0 timer and disable compare match interrupts
 *
 * @details
 * Halts the CMT0 counter by clearing the start bit (CMSTR0.STR0 = 0) and
 * disables the compare match interrupt. The counter value is preserved and can
 * be read via timer_get_count(). After stopping, timer_init() may be called
 * to restart the timer.
 *
 * ## Algorithm
 * 1. Disable compare match interrupt (CMCR.CMIE = 0)
 * 2. Stop counter (CMSTR0.STR0 = 0)
 * 3. Mark timer as stopped in internal state
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok CMT0 successfully stopped
 * @retval k_rx_err_invalid_state CMT0 was not initialized
 *
 * @pre CMT0 must have been initialized via timer_init()
 * @pre Caller must ensure stopping the system tick will not break RTOS scheduling
 * @post CMT0 counter halted, counter value preserved
 * @post Compare match interrupts disabled
 *
 * @note **Thread Safety**: NOT thread-safe. Stopping the system tick from a ThreadX thread
 *       may cause scheduling to freeze.
 * @warning Stopping CMT0 halts the ThreadX system tick. Only stop during shutdown or
 *          low-power entry.
 *
 * @par Example: Entering Low Power Mode
 * @code{.c}
 * void enter_low_power(void) {
 *   rx_err_t err = timer_stop();
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("TIMER", "Failed to stop timer", err);
 *     return;
 *   }
 *   // Enter low-power mode
 * }
 * @endcode
 *
 * @see timer_init() Reinitialize CMT0 after stopping
 * @see timer_get_count() Read counter value (works while stopped)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t timer_stop(void);

/**
 * @brief Get current CMT0 counter value
 *
 * @details
 * Reads the 16-bit free-running counter register (CMCNT) from CMT0. The counter
 * increments at PCLKB/8 rate and resets to 0 on compare match with CMCOR. This
 * can be used for sub-tick timing measurements or to check elapsed time within
 * a 10 ms system tick period. Works whether the timer is running or stopped.
 *
 * @param[out] count Pointer to store 16-bit counter value
 *                   - Must point to valid uint16_t variable
 *                   - Value range: 0 to CMCOR (compare match constant)
 *                   - Unchanged if function returns error
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Counter value successfully read and stored in *count
 * @retval k_rx_err_null_ptr count pointer is nullptr
 *
 * @pre count pointer must be valid (not nullptr)
 * @pre CMT0 must have been initialized via timer_init() for meaningful values
 * @post *count contains current CMCNT register value if function succeeds
 * @post *count unchanged if function returns error
 *
 * @note **Thread Safety**: NOT thread-safe. The 16-bit register read is atomic on RX72N
 *       but the pointer write is not protected.
 *
 * @par Example: Sub-Tick Timing
 * @code{.c}
 * uint16_t start, end;
 * timer_get_count(&start);
 * // ... perform operation ...
 * timer_get_count(&end);
 * uint16_t elapsed_ticks = (end >= start) ? (end - start) : (0xFFFF - start + end);
 * @endcode
 *
 * @see timer_init() Initialize CMT0 timer
 * @see timer_stop() Stop CMT0 timer
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t timer_get_count(uint16_t* count);

/* =============================================================================
 * ADC Functions
 * =============================================================================
 */

/**
 * @enum adc_unit_t
 * @brief ADC unit enumeration with type safety for 12-bit SAR ADC modules
 *
 * @details
 * Identifies which of the two independent 12-bit successive approximation ADC units (S12AD)
 * on the RX72N to use. Each unit has separate channels, registers, and conversion control.
 * Using a typed enum prevents accidental swapping of unit and channel parameters.
 *
 * ## Hardware Specifications
 * | Unit | Base Address | Channels Available | Max Sample Rate |
 * |------|--------------|-------------------|-----------------|
 * | S12AD0 | 0x00089000 | 8 (AN000-AN007) | 1 Msps |
 * | S12AD1 | 0x00089100 | 21 (AN100-AN120) | 1 Msps |
 *
 * ## STAR Project Usage (144-pin LFQFP)
 * | Unit | Channel | Pin | Signal | Purpose |
 * |------|---------|-----|--------|---------|
 * | k_adc_unit_0 | k_adc_channel_7 | P47 | MOTOR_I0 | Motor 0 current sense (DRV8263H IPROPI) |
 * | k_adc_unit_0 | k_adc_channel_6 | P46 | MOTOR_I1 | Motor 1 current sense (DRV8263H IPROPI) |
 * | k_adc_unit_0 | k_adc_channel_5 | P45 | MOTOR_I2 | Motor 2 current sense |
 * | k_adc_unit_0 | k_adc_channel_4 | P44 | MOTOR_I3 | Motor 3 current sense |
 *
 * ## Type Safety Benefit
 * @code{.c}
 * // WRONG - Easy to swap unit and channel with plain uint8_t
 * adc_read(0, 1, &value);  // Is 0 the unit or channel?
 *
 * // CORRECT - Typed enums make intent clear
 * adc_read(k_adc_unit_0, k_adc_channel_1, &value);  // Unambiguous
 * @endcode
 *
 * @par Underlying Type: uint8_t (C23 typed enum)
 * - **Range**: 0-1 (only 2 units on RX72N)
 * - **Size**: 1 byte
 * - **Rationale**: Smallest type sufficient, explicit size guarantees ABI stability
 *
 * @see adc_channel_t Channel enumeration
 * @see adc_resolution_t Resolution configuration
 * @see adc_init() ADC initialization function
 */
typedef enum : uint8_t {
  k_adc_unit_0 =
    0, /**< S12AD0 - ADC unit 0 (channels AN000-AN007). Used for motor current sensing. */
  k_adc_unit_1 =
    1 /**< S12AD1 - ADC unit 1 (channels AN100-AN120). Reserved for expansion sensors. */
} adc_unit_t;

/**
 * @enum adc_channel_t
 * @brief ADC channel enumeration with type safety for analog input selection
 *
 * @details
 * Identifies which analog input channel (0-7) to use within an ADC unit. Each unit
 * (S12AD0, S12AD1) has 8 independent channels with dedicated analog input pins.
 * Using a typed enum prevents accidental parameter swaps with unit or resolution.
 *
 * ## Pin Mapping (RX72N 144-pin LFQFP, R5F572NNHxFB)
 * ### S12AD0 Channels (use with k_adc_unit_0)
 * | Channel | Pin | Signal Name | STAR Usage |
 * |---------|-----|-------------|------------|
 * | k_adc_channel_0 | P40 | AN000 | Available (expansion) |
 * | k_adc_channel_1 | P41 | AN001 | Available (expansion) |
 * | k_adc_channel_2 | P42 | AN002 | Available (expansion) |
 * | k_adc_channel_3 | P43 | AN003 | Available (expansion) |
 * | k_adc_channel_4 | P44 | AN004 | Motor 3 current sense |
 * | k_adc_channel_5 | P45 | AN005 | Motor 2 current sense |
 * | k_adc_channel_6 | P46 | AN006 | Motor 1 current sense |
 * | k_adc_channel_7 | P47 | AN007 | Motor 0 current sense |
 *
 * ## Input Specifications
 * - **Voltage Range**: 0V to AVCC (3.3V)
 * - **Input Impedance**: 1 MOhm typical
 * - **Max Input Current**: +/-10 uA
 * - **Protection**: Internal clamping diodes to AVCC/AVSS
 *
 * ## Usage Example
 * @code{.c}
 * // Motor current sensing
 * adc_init(k_adc_unit_0, k_adc_channel_0, k_adc_resolution_12bit);
 *
 * uint32_t voltage_mv;
 * adc_read_voltage_mv(k_adc_unit_0, k_adc_channel_0, k_adc_resolution_12bit, &voltage_mv);
 * rx_log_info_val("ADC", "Current sense (mV)", voltage_mv);
 * @endcode
 *
 * @par Underlying Type: uint8_t (C23 typed enum)
 * - **Range**: 0-7 (8 channels per unit)
 * - **Size**: 1 byte
 * - **Rationale**: Matches hardware channel select bits (ADANSA register)
 *
 * @see adc_unit_t ADC unit selection
 * @see adc_resolution_t Resolution configuration
 * @see adc_read() Read raw ADC value
 * @see adc_read_voltage_mv() Read voltage in millivolts
 */
typedef enum : uint8_t {
  k_adc_channel_0 = 0, /**< Channel 0 - AN000 (unit 0) or AN100 (unit 1). Available. */
  k_adc_channel_1 = 1, /**< Channel 1 - AN001 (unit 0) or AN101 (unit 1). Available. */
  k_adc_channel_2 = 2, /**< Channel 2 - AN002 (unit 0) or AN102 (unit 1). Available. */
  k_adc_channel_3 = 3, /**< Channel 3 - AN003 (unit 0) or AN103 (unit 1). Available. */
  k_adc_channel_4 = 4, /**< Channel 4 - AN004/P44 (unit 0, Motor 3 current) or AN104 (unit 1) */
  k_adc_channel_5 = 5, /**< Channel 5 - AN005/P45 (unit 0, Motor 2 current) or AN105 (unit 1) */
  k_adc_channel_6 = 6, /**< Channel 6 - AN006/P46 (unit 0, Motor 1 current) or AN106 (unit 1) */
  k_adc_channel_7 = 7  /**< Channel 7 - AN007/P47 (unit 0, Motor 0 current) or AN107 (unit 1) */
} adc_channel_t;

/* ADC resolution type alias (enum defined in rx_bus_types.h) */
typedef rx_adc_resolution_t adc_resolution_t;

/**
 * @brief Initialize ADC unit and channel
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[in] bits Resolution (8, 10, or 12 bits)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if unit, channel, or bits is invalid,
 *         k_rx_err_gpio_conflict if pin already reserved
 */
[[nodiscard]] rx_err_t adc_init(adc_unit_t unit, adc_channel_t channel, adc_resolution_t bits);

/**
 * @brief Read ADC value
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[out] value Pointer to store ADC value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if value is nullptr,
 *         k_rx_err_invalid_arg if unit or channel is invalid,
 *         k_rx_err_invalid_state if ADC unit not initialized,
 *         k_rx_err_timeout if conversion times out
 */
[[nodiscard]] rx_err_t adc_read(adc_unit_t unit, adc_channel_t channel, uint16_t* value);

/**
 * @brief Read ADC value and convert to millivolts
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[in] bits Resolution used during init (8, 10, or 12)
 * @param[out] voltage_mv Pointer to store voltage in millivolts
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if voltage_mv is nullptr,
 *         k_rx_err_invalid_arg if unit or channel is invalid,
 *         k_rx_err_invalid_state if ADC unit not initialized,
 *         k_rx_err_timeout if conversion times out
 */
[[nodiscard]] rx_err_t adc_read_voltage_mv(adc_unit_t       unit,
                                           adc_channel_t    channel,
                                           adc_resolution_t bits,
                                           uint32_t*        voltage_mv);

/* =============================================================================
 * RIIC (I2C) Functions
 * =============================================================================
 */

/**
 * @struct riic_channel_t
 * @brief RIIC (I2C) channel wrapper providing strong type safety
 *
 * @details
 * Wraps RIIC channel number in a struct to prevent accidental argument swaps with
 * I2C device addresses. This is a **stronger** type safety mechanism than simple
 * typedefs because structs cannot be implicitly converted.
 *
 * ## Design Rationale: Struct Wrapper vs. Typedef
 *
 * **Why struct wrapper instead of simple typedef?**
 *
 * I2C functions have multiple `uint8_t` parameters that are easily confused:
 * - Channel number (0-2)
 * - Device address (0x00-0x7F)
 * - Register address (0x00-0xFF)
 * - Data bytes (0x00-0xFF)
 *
 * ### Problem with Simple Typedefs
 * @code{.c}
 * typedef uint8_t riic_channel_t;        // Simple typedef
 * typedef uint8_t i2c_device_addr_t;     // Simple typedef
 *
 * riic_channel_t ch = 0;
 * i2c_device_addr_t addr = 0x48;
 *
 * // PROBLEM: Arguments can be swapped without compiler error!
 * riic_write(addr, ch, data, len);  // Compiles! Wrong order, but types are compatible
 * @endcode
 *
 * ### Solution with Struct Wrappers
 * @code{.c}
 * typedef struct { uint8_t value; } riic_channel_t;       // Struct wrapper
 * typedef struct { uint8_t value; } i2c_device_addr_t;    // Struct wrapper
 *
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t addr = {.value = 0x48};
 *
 * // CORRECT: Compiler catches swapped arguments!
 * riic_write(addr, ch, data, len);  // Compile error: type mismatch!
 * riic_write(ch, addr, data, len);  // Correct order, compiles successfully
 * @endcode
 *
 * ## Comparison: ADC vs. I2C Type Safety
 *
 * | Peripheral | Type Safety Mechanism | Rationale |
 * |------------|----------------------|-----------|
 * | **ADC** | Simple typed enums | Parameters are distinct (unit vs channel vs resolution bits) |
 * | **I2C** | Struct wrappers | Multiple uint8_t parameters easily confused |
 * | **SPI** | Simple typed enums | Mode, channel, frequency are distinct types |
 * | **UART** | Simple typed enums | Channel and baudrate are distinct types |
 *
 * ADC: `adc_read(k_adc_unit_0, k_adc_channel_1, &value)` -> Different types prevent swaps
 * I2C: `riic_write({.value = 0}, {.value = 0x48}, data, len)` -> Struct wrappers prevent swaps
 *
 * ## RX72N RIIC Hardware
 * - **Channels Available**: 3 (RIIC0, RIIC1, RIIC2)
 * - **Speeds**: 100 kHz (standard), 400 kHz (fast), 1 MHz (fast-plus)
 * - **Addressing**: 7-bit (0x00-0x7F) or 10-bit (not used in STAR project)
 *
 * ## STAR Project Usage
 * | Channel | SCL Pin | SDA Pin | Device | Address | Purpose |
 * |---------|---------|---------|--------|---------|---------|
 * | 0 | P16 | P17 | IMU (MPU6050) | 0x68 | Orientation sensing |
 * | 1 | P54 | P55 | Reserved | - | Future expansion |
 * | 2 | P52 | P53 | Reserved | - | Future expansion |
 *
 * ## Usage Example
 * @code{.c}
 * #include "hardware.h"
 *
 * void imu_init(void) {
 *   // Initialize I2C channel 0 at 400 kHz (fast mode)
 *   riic_channel_t ch = {.value = 0};
 *   riic_init(ch, 400000);
 *
 *   // MPU6050 IMU device address
 *   i2c_device_addr_t imu_addr = {.value = 0x68};
 *
 *   // Write to WHO_AM_I register (0x75) to verify device
 *   uint8_t reg_addr = 0x75;
 *   uint8_t who_am_i;
 *   riic_write_read(ch, imu_addr, &reg_addr, 1, &who_am_i, 1);
 *
 *   if (who_am_i == 0x68) {
 *     rx_log_info("IMU", "MPU6050 detected");
 *   }
 * }
 * @endcode
 *
 * @par Example: Compiler Catches Mistakes
 * @code{.c}
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t addr = {.value = 0x48};
 * uint8_t data[] = {0x01, 0x02};
 *
 * // WRONG - Arguments swapped (compiler error!)
 * riic_write(addr, ch, data, 2);
 * // Error: cannot convert 'i2c_device_addr_t' to 'riic_channel_t'
 *
 * // CORRECT - Proper order
 * riic_write(ch, addr, data, 2);  // Compiles successfully
 * @endcode
 *
 * @see i2c_device_addr_t I2C device address wrapper
 * @see riic_init() Initialize I2C channel
 * @see riic_write() Write data to I2C device
 * @see riic_read() Read data from I2C device
 * @see riic_write_read() Combined write-then-read transaction
 */
typedef struct {
  uint8_t value; /**< RIIC channel number (0-2). Use {.value = N} initialization. */
} riic_channel_t;

/**
 * @struct i2c_device_addr_t
 * @brief I2C device address wrapper providing strong type safety
 *
 * @details
 * Wraps 7-bit I2C device address in a struct to prevent accidental swaps with
 * channel numbers or register addresses. Part of the type safety system that
 * uses struct wrappers to catch parameter ordering mistakes at compile time.
 *
 * ## I2C Addressing
 * - **7-bit addressing**: Standard mode used in STAR project
 * - **Valid range**: 0x08-0x77 (0x00-0x07 and 0x78-0x7F reserved)
 * - **Reserved addresses**:
 *   - 0x00: General call
 *   - 0x01-0x07: CBUS, different bus formats
 *   - 0x78-0x7F: 10-bit addressing, reserved
 *
 * ## Common Device Addresses
 * | Device | Address | Alt Address | Configurable |
 * |--------|---------|-------------|--------------|
 * | MPU6050 (IMU) | 0x68 | 0x69 | Via AD0 pin |
 * | BMP280 (Pressure) | 0x76 | 0x77 | Via SDO pin |
 * | ADS1115 (ADC) | 0x48-0x4B | - | Via ADDR pin |
 * | PCA9685 (PWM) | 0x40-0x7F | - | Via A0-A5 pins |
 * | EEPROM (24C series) | 0x50-0x57 | - | Via A0-A2 pins |
 *
 * ## Address Format on Bus
 * The 7-bit address is shifted left by 1 bit and combined with R/W bit:
 * ```
 * [A6 A5 A4 A3 A2 A1 A0 R/W]
 *  \___7-bit address___/ \_ 0=Write, 1=Read
 * ```
 * **Note**: HAL automatically handles this shifting. Always provide 7-bit address.
 *
 * ## STAR Project Devices
 * | Device | Address | Channel | Purpose |
 * |--------|---------|---------|---------|
 * | MPU6050 IMU | 0x68 | RIIC0 | 6-axis motion tracking |
 * | Future sensor 1 | TBD | RIIC1 | Expansion |
 * | Future sensor 2 | TBD | RIIC2 | Expansion |
 *
 * ## Usage Example
 * @code{.c}
 * // MPU6050 IMU on RIIC0
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t imu_addr = {.value = 0x68};
 *
 * // Read power management register (0x6B)
 * uint8_t pwr_mgmt_reg = 0x6B;
 * uint8_t pwr_mgmt_val;
 *
 * riic_write_read(ch, imu_addr, &pwr_mgmt_reg, 1, &pwr_mgmt_val, 1);
 * rx_log_info_hex("IMU", "Power management", pwr_mgmt_val, 2);
 * @endcode
 *
 * @par Example: Multi-Device Bus
 * @code{.c}
 * // Multiple devices on same I2C bus
 * riic_channel_t ch = {.value = 0};
 *
 * i2c_device_addr_t imu_addr = {.value = 0x68};      // MPU6050
 * i2c_device_addr_t baro_addr = {.value = 0x76};     // BMP280
 * i2c_device_addr_t adc_addr = {.value = 0x48};      // ADS1115
 *
 * // Read from different devices on same bus
 * uint8_t imu_data[6];
 * riic_read(ch, imu_addr, imu_data, 6);
 *
 * uint8_t baro_data[3];
 * riic_read(ch, baro_addr, baro_data, 3);
 *
 * uint8_t adc_data[2];
 * riic_read(ch, adc_addr, adc_data, 2);
 * @endcode
 *
 * @see riic_channel_t RIIC channel wrapper
 * @see riic_write() Write data to I2C device
 * @see riic_read() Read data from I2C device
 * @see riic_write_read() Register read pattern (write reg addr, then read)
 */
typedef struct {
  uint8_t value; /**< 7-bit I2C device address (0x08-0x77). Use {.value = 0xNN} initialization. */
} i2c_device_addr_t;

/**
 * @brief Initialize RIIC channel for I2C controller communication
 *
 * @details
 * Configures the specified RIIC peripheral channel for I2C controller mode at the
 * requested clock frequency. Performs full hardware initialization including module
 * stop release, pin function selection (SCL/SDA via MPC), internal reset sequence,
 * bit rate register (BRH/BRL) calculation from PCLKB, and interrupt configuration.
 *
 * ## Algorithm
 * 1. Validate channel number (0-2) and frequency
 * 2. Release RIIC module from module stop (MSTPCRB)
 * 3. Assert internal reset (ICCR1.IICRST = 1)
 * 4. Configure SCL/SDA pins via MPC register
 * 5. Set bit rate registers (BRH/BRL) from PCLKB and frequency_hz
 * 6. Configure noise filter, timeout detection
 * 7. Release internal reset (ICCR1.IICRST = 0)
 * 8. Enable I2C bus (ICCR1.ICE = 1)
 *
 * ## Supported Frequencies
 * | Frequency | Mode | BRL/BRH | Max Bus Length |
 * |-----------|------|---------|----------------|
 * | 100000 Hz | Standard | Auto-calculated | 1 m |
 * | 400000 Hz | Fast | Auto-calculated | 0.3 m |
 * | 1000000 Hz | Fast-plus | Auto-calculated | 0.1 m |
 *
 * @param[in] channel RIIC channel wrapper
 *                    - channel.value range: 0-2
 *                    - Use {.value = N} designated initializer
 * @param[in] frequency_hz I2C clock frequency in Hz
 *                         - Valid values: 100000, 400000, 1000000
 *                         - Must match connected device capabilities
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Channel successfully initialized
 * @retval k_rx_err_invalid_arg Channel > 2 or unsupported frequency
 * @retval k_rx_err_invalid_state Channel already initialized
 *
 * @pre Channel must not be already initialized (call riic deinit first to reinitialize)
 * @pre System clocks must be configured (PCLKB running at expected frequency)
 * @post RIIC channel configured and ready for I2C transactions
 * @post SCL/SDA pins configured with open-drain output and internal pull-ups
 *
 * @note **Thread Safety**: NOT thread-safe. Initialize each channel from a single thread
 *       during system startup.
 *
 * @par Example: Initialize IMU I2C Bus
 * @code{.c}
 * riic_channel_t ch = {.value = 0};
 * rx_err_t err = riic_init(ch, 400000);  // 400 kHz fast mode
 * if (err != k_rx_ok) {
 *   rx_log_error_val("I2C", "RIIC0 init failed", err);
 *   return;
 * }
 * rx_log_info("I2C", "RIIC0 initialized at 400 kHz");
 * @endcode
 *
 * @see riic_write() Write data to I2C device
 * @see riic_read() Read data from I2C device
 * @see riic_write_read() Combined write-then-read transaction
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t riic_init(riic_channel_t channel, uint32_t frequency_hz);

/**
 * @brief Write data to I2C device on specified RIIC channel
 *
 * @details
 * Performs a complete I2C write transaction: generates START condition, transmits
 * device address with write bit (R/W=0), sends data bytes, and generates STOP
 * condition. Each byte is acknowledged by the peripheral device. The function
 * blocks until all bytes are transmitted or an error occurs.
 *
 * ## Transaction Sequence
 * 1. Generate START condition
 * 2. Send device address byte (7-bit address << 1 | 0)
 * 3. Wait for ACK from peripheral
 * 4. Send data bytes one at a time, waiting for ACK after each
 * 5. Generate STOP condition
 * 6. Wait for bus idle (BBSY = 0)
 *
 * @param[in] channel RIIC channel wrapper
 *                    - channel.value range: 0-2
 * @param[in] device_addr 7-bit I2C device address wrapper
 *                        - device_addr.value range: 0x08-0x77
 * @param[in] data Pointer to data buffer to write
 *                 - Must not be nullptr
 *                 - Must contain at least length bytes
 * @param[in] length Number of bytes to write
 *                   - Range: 1-65535
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok All bytes successfully written and ACKed
 * @retval k_rx_err_null_ptr data pointer is nullptr
 * @retval k_rx_err_invalid_state Channel not initialized via riic_init()
 * @retval k_rx_err_timeout Bus timeout waiting for ACK or bus idle
 * @retval k_rx_err_nack Device sent NACK (wrong address or device busy)
 *
 * @pre Channel must be initialized via riic_init()
 * @pre data pointer must be valid and contain at least length bytes
 * @post All data bytes transmitted to the I2C device if successful
 * @post I2C bus returned to idle state (STOP condition generated)
 *
 * @note **Thread Safety**: NOT thread-safe. Do not perform concurrent transactions on the
 *       same RIIC channel. Different channels may be used from different threads.
 *
 * @par Example: Write IMU Configuration Register
 * @code{.c}
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t imu_addr = {.value = 0x68};
 *
 * // Write power management register (0x6B) to wake up MPU6050
 * uint8_t wake_cmd[] = {0x6B, 0x00};
 * rx_err_t err = riic_write(ch, imu_addr, wake_cmd, 2);
 * if (err != k_rx_ok) {
 *   rx_log_error_val("IMU", "Failed to wake IMU", err);
 * }
 * @endcode
 *
 * @see riic_read() Read data from I2C device
 * @see riic_write_read() Combined write-then-read (register read pattern)
 * @see riic_init() Initialize RIIC channel first
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t riic_write(riic_channel_t    channel,
                                  i2c_device_addr_t device_addr,
                                  const uint8_t*    data,
                                  uint16_t          length);

/**
 * @brief Read data from I2C device on specified RIIC channel
 *
 * @details
 * Performs a complete I2C read transaction: generates START condition, transmits
 * device address with read bit (R/W=1), receives data bytes with ACK (NACK on
 * last byte), and generates STOP condition. The function blocks until all bytes
 * are received or an error occurs.
 *
 * ## Transaction Sequence
 * 1. Generate START condition
 * 2. Send device address byte (7-bit address << 1 | 1)
 * 3. Wait for ACK from peripheral
 * 4. Receive data bytes, sending ACK after each (NACK after last byte)
 * 5. Generate STOP condition
 * 6. Wait for bus idle (BBSY = 0)
 *
 * @param[in] channel RIIC channel wrapper
 *                    - channel.value range: 0-2
 * @param[in] device_addr 7-bit I2C device address wrapper
 *                        - device_addr.value range: 0x08-0x77
 * @param[out] data Pointer to buffer for received data
 *                  - Must not be nullptr
 *                  - Must have space for at least length bytes
 *                  - Unchanged if function returns error
 * @param[in] length Number of bytes to read
 *                   - Range: 1-65535
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok All bytes successfully received
 * @retval k_rx_err_null_ptr data pointer is nullptr
 * @retval k_rx_err_invalid_state Channel not initialized via riic_init()
 * @retval k_rx_err_timeout Bus timeout waiting for data or bus idle
 * @retval k_rx_err_nack Device sent NACK on address (wrong address or device busy)
 *
 * @pre Channel must be initialized via riic_init()
 * @pre data pointer must be valid with space for at least length bytes
 * @post data buffer contains received bytes if function succeeds
 * @post I2C bus returned to idle state (STOP condition generated)
 *
 * @note **Thread Safety**: NOT thread-safe. Do not perform concurrent transactions on the
 *       same RIIC channel. Different channels may be used from different threads.
 *
 * @par Example: Read IMU Accelerometer Data
 * @code{.c}
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t imu_addr = {.value = 0x68};
 *
 * uint8_t accel_data[6];
 * rx_err_t err = riic_read(ch, imu_addr, accel_data, 6);
 * if (err != k_rx_ok) {
 *   rx_log_error_val("IMU", "Failed to read accel data", err);
 * }
 * @endcode
 *
 * @see riic_write() Write data to I2C device
 * @see riic_write_read() Combined write-then-read (register read pattern)
 * @see riic_init() Initialize RIIC channel first
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
riic_read(riic_channel_t channel, i2c_device_addr_t device_addr, uint8_t* data, uint16_t length);

/**
 * @brief Write then read from I2C device using repeated START (combined transaction)
 *
 * @details
 * Performs an I2C combined write-then-read transaction using a repeated START
 * condition. This is the standard pattern for reading device registers: write the
 * register address, then issue a repeated START and read back the register data
 * without releasing the bus between operations.
 *
 * ## Transaction Sequence
 * 1. Generate START condition
 * 2. Send device address byte (7-bit address << 1 | 0) -- write mode
 * 3. Wait for ACK from peripheral
 * 4. Send write_data bytes (typically register address), ACK after each
 * 5. Generate repeated START condition (no STOP)
 * 6. Send device address byte (7-bit address << 1 | 1) -- read mode
 * 7. Wait for ACK from peripheral
 * 8. Receive read_data bytes, ACK each (NACK on last byte)
 * 9. Generate STOP condition
 * 10. Wait for bus idle (BBSY = 0)
 *
 * @param[in] channel RIIC channel wrapper
 *                    - channel.value range: 0-2
 * @param[in] device_addr 7-bit I2C device address wrapper
 *                        - device_addr.value range: 0x08-0x77
 * @param[in] write_data Pointer to data to write (typically register address)
 *                       - Must not be nullptr
 *                       - Must contain at least write_length bytes
 * @param[in] write_length Number of bytes to write
 *                         - Typically 1 (single register address byte)
 * @param[out] read_data Pointer to buffer for received data
 *                       - Must not be nullptr
 *                       - Must have space for at least read_length bytes
 *                       - Unchanged if function returns error
 * @param[in] read_length Number of bytes to read
 *                        - Range: 1-65535
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Write and read completed successfully
 * @retval k_rx_err_null_ptr write_data or read_data pointer is nullptr
 * @retval k_rx_err_invalid_state Channel not initialized via riic_init()
 * @retval k_rx_err_timeout Bus timeout during write or read phase
 * @retval k_rx_err_nack Device sent NACK (wrong address or device busy)
 *
 * @pre Channel must be initialized via riic_init()
 * @pre write_data and read_data pointers must be valid with sufficient size
 * @post read_data buffer contains received register data if function succeeds
 * @post I2C bus returned to idle state (STOP condition generated)
 *
 * @note **Thread Safety**: NOT thread-safe. Do not perform concurrent transactions on the
 *       same RIIC channel. Different channels may be used from different threads.
 *
 * @par Example: Read IMU WHO_AM_I Register
 * @code{.c}
 * riic_channel_t ch = {.value = 0};
 * i2c_device_addr_t imu_addr = {.value = 0x68};
 *
 * uint8_t reg_addr = 0x75;  // WHO_AM_I register
 * uint8_t who_am_i;
 * rx_err_t err = riic_write_read(ch, imu_addr, &reg_addr, 1, &who_am_i, 1);
 * if (err == k_rx_ok && who_am_i == 0x68) {
 *   rx_log_info("IMU", "MPU6050 detected");
 * }
 * @endcode
 *
 * @see riic_write() Write-only transaction
 * @see riic_read() Read-only transaction
 * @see riic_init() Initialize RIIC channel first
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t riic_write_read(riic_channel_t    channel,
                                       i2c_device_addr_t device_addr,
                                       const uint8_t*    write_data,
                                       uint16_t          write_length,
                                       uint8_t*          read_data,
                                       uint16_t          read_length);

/* =============================================================================
 * RIIC (I2C) Functions - Peripheral Mode
 * =============================================================================
 */

/**
 * @enum riic_peripheral_limits_t
 * @brief Public size limits for RIIC peripheral mode callers
 *
 * @details
 * Exposes the I/O size ceiling enforced by riic_peripheral_read() and
 * riic_peripheral_write() so that callers can statically size their buffers
 * and validate arguments without needing to inspect the driver implementation.
 *
 * @see riic_peripheral_read() Enforces this limit on max_length
 * @see riic_peripheral_write() Enforces this limit on length
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_riic_peripheral_transfer_limit = 256, /**< Maximum bytes per peripheral read or write call */
} riic_peripheral_limits_t;

/**
 * @brief Initialize RIIC channel in peripheral (device) mode
 *
 * @details
 * Configures the RIIC peripheral to act as an I2C peripheral device, responding
 * to a fixed 7-bit I2C address issued by a controller (RPi5). Sets up the
 * peripheral address register (SARL0/SARU0), enables address match detection
 * (ICSER.SAR0E), and leaves the module in peripheral mode (MS=0 in ICMR1).
 *
 * @par Peripheral Mode Architecture:
 * - RPi5 acts as I2C controller (generates SCL, START, STOP)
 * - RX72N acts as I2C peripheral (responds to address, provides/accepts data)
 * - Address matching handled in hardware (ICSER.SAR0E)
 * - Data transfer polled via ICSR2.RDRF and ICSR2.TDRE
 *
 * @param[in] channel RIIC channel wrapper (channel.value range: 0-2)
 * @param[in] device_addr 7-bit peripheral address for this device
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel initialized in peripheral mode
 * @retval k_rx_err_invalid_arg Channel value out of range
 * @retval k_rx_err_invalid_state Channel already initialized; deinitialize before reinitializing
 *
 * @pre channel.value must be in range [0, 2] (i.e., < k_riic_max_channels)
 * @pre device_addr.value must be in range [0x08, 0x77] (valid non-reserved 7-bit I2C address)
 * @post RIIC peripheral responds to device_addr on the I2C bus
 * @post s_riic_channel_initialized[channel.value] == true on success
 *
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe during initialization
 *
 * @see riic_peripheral_read() Read data written by I2C controller
 * @see riic_peripheral_write() Provide data to I2C controller read
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t riic_init_peripheral(riic_channel_t channel, i2c_device_addr_t device_addr);

/**
 * @brief Deinitialize an RIIC channel previously initialized in peripheral mode
 *
 * @details
 * Disables peripheral address match (ICSER.SAR0E), clears SARL0/SARU0
 * address registers, and resets the RIIC module. After this call, the channel
 * may be reinitialized via riic_init() or riic_init_peripheral().
 *
 * @param[in] channel RIIC channel to deinitialize (channel.value range: 0-2)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok              Channel deinitialized successfully
 * @retval k_rx_err_invalid_arg channel.value >= k_riic_max_channels
 * @retval k_rx_err_invalid_state Channel was not initialized in peripheral mode
 *
 * @pre channel.value must be in range [0, k_riic_max_channels - 1]
 * @pre Channel must have been initialized via riic_init_peripheral()
 * @post Channel mode reset to uninitialized; safe to call riic_init_peripheral() again
 * @post Peripheral address match disabled on the hardware
 *
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe; provide external synchronization if needed
 *
 * @par Example:
 * @code{.c}
 * riic_channel_t ch = { .value = k_i2c_comm_default_channel };
 * rx_err_t err = riic_deinit_peripheral(ch);
 * if (err != k_rx_ok) {
 *   // Channel was not in peripheral mode or index out of range
 *   return err;
 * }
 * // Channel is now free; may be reinitialized for controller or peripheral use
 * @endcode
 *
 * @see riic_init_peripheral() Complementary initialization function
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t riic_deinit_peripheral(riic_channel_t channel);

/**
 * @brief Read bytes written by the I2C controller to this peripheral
 *
 * @details
 * Polls ICSR2.RDRF (Receive Data Register Full) to detect incoming bytes from
 * the I2C controller (RPi5). Reads up to max_length bytes. Returns immediately
 * with bytes_read=0 if no data is currently available.
 *
 * @param[in] channel RIIC channel wrapper (channel.value range: 0-2)
 * @param[out] data Buffer to store received bytes
 * @param[in] max_length Maximum number of bytes to read
 * @param[out] bytes_read Actual number of bytes read (may be 0)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Read completed (check bytes_read for count)
 * @retval k_rx_err_null_ptr data or bytes_read is nullptr
 * @retval k_rx_err_invalid_arg Channel value out of range or max_length == 0
 *                               or max_length > k_riic_peripheral_transfer_limit
 * @retval k_rx_err_invalid_state Channel not initialized via riic_init_peripheral()
 *
 * @pre channel must be initialized via riic_init_peripheral()
 * @pre data != nullptr with capacity for at least max_length bytes
 * @pre max_length >= 1 && max_length <= k_riic_peripheral_transfer_limit
 * @post *bytes_read contains number of bytes placed in data (0 to max_length)
 * @post data[0..*bytes_read-1] contain valid received bytes on k_rx_ok
 *
 * @note Non-blocking: returns immediately with available data
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 *
 * @see riic_init_peripheral() Initialize channel first
 * @see riic_peripheral_write() Provide read data to controller
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t riic_peripheral_read(riic_channel_t channel,
                                            uint8_t*       data,
                                            uint16_t       max_length,
                                            uint16_t*      bytes_read);

/**
 * @brief Write bytes for the I2C controller to read from this peripheral
 *
 * @details
 * Polls ICSR2.TDRE (Transmit Data Register Empty) to detect when the I2C
 * controller is reading from this peripheral. Writes up to length bytes into
 * the ICDRT transmit data register. Returns immediately if the transmit
 * register is not yet empty (controller not currently reading).
 *
 * @param[in] channel RIIC channel wrapper (channel.value range: 0-2)
 * @param[in] data Pointer to data buffer to write
 * @param[in] length Number of bytes to write (valid range: 1..k_riic_peripheral_transfer_limit)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Write completed
 * @retval k_rx_err_null_ptr data is nullptr
 * @retval k_rx_err_invalid_arg Channel value out of range, length == 0,
 *         or length > k_riic_peripheral_transfer_limit
 * @retval k_rx_err_invalid_state Channel not initialized via riic_init_peripheral()
 * @retval k_rx_err_timeout Transmit data register not empty within timeout
 *
 * @pre channel must be initialized via riic_init_peripheral()
 * @pre data != nullptr with at least length valid bytes
 * @pre length >= 1 && length <= k_riic_peripheral_transfer_limit
 * @post On k_rx_ok: all length bytes have been written to ICDRT
 * @post On k_rx_err_timeout: partial write may have occurred
 *
 * @note Only available on RX72N target (guarded by __RX__ preprocessor)
 * @note Not thread-safe, caller must provide external synchronization
 *
 * @see riic_init_peripheral() Initialize channel first
 * @see riic_peripheral_read() Receive data from controller
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
riic_peripheral_write(riic_channel_t channel, const uint8_t* data, uint16_t length);

/* =============================================================================
 * RSPI (SPI) Functions - Peripheral Mode
 * =============================================================================
 */

/**
 * @enum rspi_mode_t
 * @brief SPI mode enumeration defining clock polarity (CPOL) and phase (CPHA) combinations
 *
 * @details
 * Defines the four standard SPI modes based on clock polarity and phase settings.
 * These modes determine when data is sampled relative to the clock edge and what
 * the idle state of the clock line is.
 *
 * ## SPI Mode Definitions
 * | Mode | CPOL | CPHA | Clock Idle | Data Sampled | Data Shifted | Common Devices |
 * |------|------|------|------------|--------------|--------------|----------------|
 * | 0 | 0 | 0 | Low | Rising edge | Falling edge | RPi5, most sensors |
 * | 1 | 0 | 1 | Low | Falling edge | Rising edge | SD cards |
 * | 2 | 1 | 0 | High | Falling edge | Rising edge | Rare |
 * | 3 | 1 | 1 | High | Rising edge | Falling edge | Rare |
 *
 * ## STAR Project Usage
 * | Peripheral | Mode | Rationale |
 * |------------|------|-----------|
 * | RPi5 (RSPI0 peripheral) | Mode 0 | Linux spidev default, maximum compatibility |
 * | RSPI1 controller (not used) | TBD | RSPI1 not used in current design |
 *
 * ## Clock Polarity (CPOL)
 * - **CPOL=0**: Clock idle state is low (GND). Clock starts low, pulses high.
 * - **CPOL=1**: Clock idle state is high (VDD). Clock starts high, pulses low.
 *
 * ## Clock Phase (CPHA)
 * - **CPHA=0**: Data sampled on leading clock edge, shifted on trailing edge.
 * - **CPHA=1**: Data shifted on leading clock edge, sampled on trailing edge.
 *
 * ## Timing Diagrams
 *
 * ### Mode 0 (CPOL=0, CPHA=0) - Most Common
 * ```
 * CLK:  ___/---\___/---\___
 * COPI: ===[D0]===[D1]===
 * CIPO: ===[D0]===[D1]===
 *        ^ Sample  ^ Sample
 * ```
 *
 * ### Mode 3 (CPOL=1, CPHA=1)
 * ```
 * CLK:  ---\___/---\___/---
 * COPI: ===[D0]===[D1]===
 * CIPO: ===[D0]===[D1]===
 *        ^ Sample  ^ Sample
 * ```
 *
 * ## Selection Guidelines
 * 1. **Check device datasheet**: Most devices specify preferred mode
 * 2. **Mode 0 default**: Use Mode 0 if device supports multiple modes
 * 3. **Controller/Peripheral match**: Both sides must use same mode
 * 4. **Avoid Modes 1 and 2**: Less common, higher chance of incompatibility
 *
 * ## Hardware Configuration
 * - **SPCR.CPOL bit**: Set for CPOL=1, clear for CPOL=0
 * - **SPCR.CPHA bit**: Set for CPHA=1, clear for CPHA=0
 * - **Both configurable**: HAL automatically sets registers based on mode
 *
 * ## Usage Example
 * @code{.c}
 * // RPi5 communication (peripheral mode, Mode 0)
 * rspi_config_t rpi_cfg = {
 *   .spi_mode = k_rspi_mode_0,  // CPOL=0, CPHA=0
 *   .use_16bit = false
 * };
 * rspi_init_peripheral(k_rspi_channel_0, &rpi_cfg);
 *
 * // External sensor communication (controller mode, Mode 0)
 * rspi_controller_config_t sensor_cfg = {
 *   .spi_mode = k_rspi_mode_0,  // Standard SPI mode 0
 *   .freq_hz = k_rspi_freq_1mhz, // 1 MHz
 *   .cs = k_port_c_pin_4
 * };
 * rspi_init_controller(k_rspi_channel_1, &sensor_cfg);  // RSPI1
 * @endcode
 *
 * @par Underlying Type: uint8_t (C23 typed enum)
 * - **Range**: 0-3 (4 SPI modes)
 * - **Size**: 1 byte
 * - **Rationale**: Matches SPCR register bit pattern (CPOL | (CPHA << 1))
 *
 * @see rspi_init_peripheral() Initialize SPI in peripheral mode
 * @see rspi_init_controller() Initialize SPI in controller mode
 * @see rspi_config_t Configuration structure for peripheral mode
 * @see rspi_controller_config_t Configuration structure for controller mode
 */
typedef enum : uint8_t {
  k_rspi_mode_0 =
    0, /**< CPOL=0, CPHA=0 - Data sampled on rising edge, idle low. Most common mode. */
  k_rspi_mode_1 =
    1, /**< CPOL=0, CPHA=1 - Data sampled on falling edge, idle low. Used by SD cards. */
  k_rspi_mode_2 = 2, /**< CPOL=1, CPHA=0 - Data sampled on falling edge, idle high. Rarely used. */
  k_rspi_mode_3 =
    3 /**< CPOL=1, CPHA=1 - Data sampled on rising edge, idle high. Alternative for some devices. */
} rspi_mode_t;

/**
 * @enum rspi_channel_t
 * @brief RSPI channel enumeration with type safety for SPI peripheral selection
 *
 * @details
 * Identifies which of the three RSPI channels (RSPI0, RSPI1, RSPI2) on the RX72N
 * to use. Using a typed enum instead of plain uint8_t prevents accidental argument
 * swaps and provides compile-time validation of channel numbers.
 *
 * ## RX72N RSPI Channel Assignments (STAR Project)
 * | Channel | Role | Connected Device | Pins |
 * |---------|------|-----------------|------|
 * | RSPI0 | Peripheral | Raspberry Pi 5 | P30 (CIPO), P31 (COPI), P32 (CLK), P33 (CS) |
 * | RSPI1 | Controller | Reserved (sensors) | PC5 (CIPO), PC6 (COPI), PC7 (CLK) |
 * | RSPI2 | Unused | - | - |
 *
 * @par Underlying Type: uint8_t (C23 typed enum)
 * - **Range**: 0-2 (3 channels on RX72N)
 * - **Size**: 1 byte
 *
 * @invariant Values must be in range [0, 2] corresponding to RSPI0, RSPI1, RSPI2
 *
 * @code{.c}
 * // Initialize RSPI channel as peripheral
 * rspi_config_t cfg = { .spi_mode = k_rspi_mode_0, .use_16bit = false };
 * rx_err_t err = rspi_init_peripheral(k_rspi_channel_0, &cfg);
 * @endcode
 *
 * @see rspi_init_peripheral() Initialize channel in peripheral mode
 * @see rspi_init_controller() Initialize channel in controller mode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rspi_channel_0 = 0, /**< RSPI0 - RPi5 communication (peripheral mode in STAR project) */
  k_rspi_channel_1 = 1, /**< RSPI1 - External sensor SPI (controller mode, reserved) */
  k_rspi_channel_2 = 2  /**< RSPI2 - Unused (available for expansion) */
} rspi_channel_t;

/**
 * @struct rspi_config_t
 * @brief RSPI peripheral mode configuration structure
 *
 * @details
 * Configuration parameters for initializing an RSPI channel in peripheral mode.
 * Used when the RX72N acts as an SPI peripheral device, receiving clock from an external
 * controller (e.g., Raspberry Pi 5). The struct encapsulates SPI mode and frame size
 * settings to avoid positional parameter errors.
 *
 * @code{.c}
 * rspi_config_t rpi_cfg = {
 *   .spi_mode = k_rspi_mode_0,  // CPOL=0, CPHA=0
 *   .use_16bit = false           // 8-bit frame size
 * };
 * @endcode
 *
 * @invariant spi_mode must be a valid rspi_mode_t value (0-3, CPOL/CPHA combinations)
 * @invariant use_16bit is boolean: false for 8-bit frames, true for 16-bit frames
 *
 * @see rspi_mode_t SPI mode enumeration (CPOL/CPHA combinations)
 * @see rspi_init_peripheral() Initialize channel with this configuration
 * @see rspi_controller_config_t Controller mode configuration (separate struct)
 *
 * @since Version 1.0.0
 */
typedef struct {
  rspi_mode_t spi_mode;  /**< SPI mode (0-3): CPOL and CPHA configuration */
  bool        use_16bit; /**< True for 16-bit frames, false for 8-bit frames */
} rspi_config_t;

/**
 * @brief Initialize RSPI channel in peripheral mode for RPi5 communication
 *
 * @details
 * Configures the specified RSPI channel as an SPI peripheral device.
 * In peripheral mode the RX72N receives the clock signal from an external SPI
 * controller (Raspberry Pi 5). Performs full hardware initialization including
 * module stop release, pin function selection (COPI/CIPO/CLK/CS via MPC),
 * SPI mode register configuration, and frame size setup.
 *
 * ## Algorithm
 * 1. Validate channel (0-2) and config pointer
 * 2. Release RSPI module from module stop (MSTPCRB)
 * 3. Disable RSPI (SPCR.SPE = 0)
 * 4. Configure pin functions via MPC (COPI, CIPO, CLK, CS)
 * 5. Set SPI mode (CPOL/CPHA in SPCMD0)
 * 6. Set frame length (8-bit or 16-bit in SPCMD0)
 * 7. Configure as peripheral (SPCR.MSTR = 0)
 * 8. Enable RSPI (SPCR.SPE = 1)
 *
 * @param[in] channel RSPI channel number (0-2)
 * @param[in] config Pointer to peripheral mode configuration
 *                   - Must not be nullptr
 *                   - config->spi_mode must be valid (0-3)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Channel successfully initialized in peripheral mode
 * @retval k_rx_err_invalid_arg Channel > 2 or invalid SPI mode
 * @retval k_rx_err_null_ptr config pointer is nullptr
 * @retval k_rx_err_invalid_state Channel already initialized (call rspi_deinit() to reinitialize)
 *
 * @pre Channel must not be already initialized (call rspi_deinit() first to reinitialize)
 * @pre System clocks must be configured (PCLKB running at expected frequency)
 * @post RSPI channel configured as SPI peripheral, ready for transfers
 * @post COPI/CIPO/CLK/CS pins configured for SPI peripheral function
 *
 * @note **Thread Safety**: NOT thread-safe. Initialize each channel from a single thread
 *       during system startup.
 *
 * @par Example: RPi5 Communication Setup
 * @code{.c}
 * rspi_config_t rpi_cfg = {
 *   .spi_mode = k_rspi_mode_0,
 *   .use_16bit = false
 * };
 * rx_err_t err = rspi_init_peripheral(k_rspi_channel_0, &rpi_cfg);
 * if (err != k_rx_ok) {
 *   rx_log_error_val("SPI", "RSPI0 peripheral init failed", err);
 * }
 * @endcode
 *
 * @see rspi_peripheral_transfer() Full-duplex SPI transfer
 * @see rspi_deinit() Deinitialize RSPI channel
 * @see rspi_config_t Configuration structure
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_init_peripheral(rspi_channel_t channel, const rspi_config_t* config);

/**
 * @brief Full-duplex SPI transfer in peripheral mode
 *
 * @details
 * Executes a byte-by-byte full-duplex SPI transfer on the specified RSPI
 * channel operating in peripheral mode. For each byte, the function waits
 * for the transmit buffer to empty, writes the TX byte, waits for the
 * receive buffer to fill, then reads the RX byte. Both TX and RX wait
 * operations are bounded by a 10 ms timeout to prevent infinite loops.
 *
 * The transfer loop is bounded by k_rspi_transfer_len_max (65535) per
 * NASA Rule 2 to ensure statically provable loop termination.
 *
 * @param[in]  channel RSPI channel number (valid: k_rspi_channel_0,
 *                     k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 * @param[in]  tx_data Pointer to transmit data buffer (must not be nullptr)
 * @param[out] rx_data Pointer to receive data buffer (must not be nullptr,
 *                     must have capacity for at least length bytes)
 * @param[in]  length  Number of bytes to transfer (valid: 1 to 65535)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Transfer completed successfully, rx_data filled
 * @retval k_rx_err_null_ptr tx_data or rx_data pointer is nullptr
 * @retval k_rx_err_invalid_arg length is zero, or RSPI base address
 *         lookup failed for the channel
 * @retval k_rx_err_invalid_state channel >= 3 or channel not initialized
 *         via rspi_init_peripheral()
 * @retval k_rx_err_timeout Transmit buffer did not empty or receive buffer
 *         did not fill within the 10 ms timeout
 *
 * @pre channel must be initialized via rspi_init_peripheral()
 * @pre tx_data and rx_data must be valid non-null pointers
 * @pre length must be in range [1, 65535]
 * @post rx_data buffer contains received bytes corresponding to each
 *       transmitted byte offset on success
 * @post SPSR status flags (SPRF, OVRF) cleared after each byte transfer
 *
 * @note Not thread-safe. Caller must provide external synchronization when
 *       accessing the same channel from multiple threads.
 *
 * @see rspi_init_peripheral() Must be called before this function
 * @see rspi_peripheral_read_available() Non-blocking receive check
 * @see rspi_peripheral_write_ready() Non-blocking transmit check
 * @see rspi_deinit() Deinitialize RSPI channel
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_peripheral_transfer(rspi_channel_t channel,
                                                const uint8_t* tx_data,
                                                uint8_t*       rx_data,
                                                uint16_t       length);

/**
 * @brief Check if receive data is available in the RSPI receive buffer
 *
 * @details
 * Polls the RSPI Status Register (SPSR) SPRF flag to determine whether the
 * receive buffer contains data ready for reading. This is a non-blocking
 * status check that returns immediately without waiting for data. Use this
 * to implement polled receive patterns without incurring the overhead of a
 * full transfer call.
 *
 * @param[in]  channel   RSPI channel number (valid: k_rspi_channel_0,
 *                       k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 * @param[out] available Pointer to bool set to true if SPRF flag indicates
 *                       data is available, false otherwise (must not be nullptr)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Status successfully read into *available
 * @retval k_rx_err_null_ptr available pointer is nullptr
 * @retval k_rx_err_invalid_state channel >= 3 or channel not initialized
 *         via rspi_init_peripheral()
 * @retval k_rx_err_invalid_arg RSPI base address lookup failed for the channel
 *
 * @pre channel must be initialized via rspi_init_peripheral()
 * @pre available must be a valid non-null pointer
 * @post *available reflects current SPRF flag state on success
 * @post *available set to false on RSPI base address lookup failure
 *
 * @note Not thread-safe. Caller must provide external synchronization when
 *       accessing the same channel from multiple threads.
 *
 * @see rspi_peripheral_transfer() Full-duplex transfer in peripheral mode
 * @see rspi_peripheral_write_ready() Non-blocking transmit buffer check
 * @see rspi_init_peripheral() Must be called before this function
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_peripheral_read_available(rspi_channel_t channel, bool* available);

/**
 * @brief Check if the RSPI transmit buffer is ready for new data
 *
 * @details
 * Polls the RSPI Status Register (SPSR) SPTEF flag to determine whether the
 * transmit buffer is empty and ready to accept new data. This is a non-blocking
 * status check that returns immediately without waiting for the buffer to drain.
 * Use this to implement polled transmit patterns or to verify readiness before
 * writing to the SPI data register.
 *
 * @param[in]  channel RSPI channel number (valid: k_rspi_channel_0,
 *                     k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 * @param[out] ready   Pointer to bool set to true if SPTEF flag indicates the
 *                     transmit buffer is empty, false otherwise (must not be nullptr)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Status successfully read into *ready
 * @retval k_rx_err_null_ptr ready pointer is nullptr
 * @retval k_rx_err_invalid_state channel >= 3 or channel not initialized
 *         via rspi_init_peripheral()
 * @retval k_rx_err_invalid_arg RSPI base address lookup failed for the channel
 *
 * @pre channel must be initialized via rspi_init_peripheral()
 * @pre ready must be a valid non-null pointer
 * @post *ready reflects current SPTEF flag state on success
 * @post *ready set to false on RSPI base address lookup failure
 *
 * @note Not thread-safe. Caller must provide external synchronization when
 *       accessing the same channel from multiple threads.
 *
 * @see rspi_peripheral_transfer() Full-duplex transfer in peripheral mode
 * @see rspi_peripheral_read_available() Non-blocking receive buffer check
 * @see rspi_init_peripheral() Must be called before this function
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_peripheral_write_ready(rspi_channel_t channel, bool* ready);

/**
 * @brief Deinitialize and disable an RSPI peripheral-mode channel
 *
 * @details
 * Performs a safe shutdown of the specified RSPI channel previously initialized
 * in peripheral mode. The deinitialization sequence disables SPI bus activity
 * (SPCR.SPE = 0), gates the module clock via MSTPCRB to reduce power
 * consumption, and clears the internal initialization flag. Register
 * protection (PRCR) is temporarily unlocked for module stop control.
 *
 * This function is safe to call on channels that were never initialized; it
 * will still disable the hardware and clear the init flag. To reinitialize
 * a channel after deinit, call rspi_init_peripheral() again.
 *
 * @param[in] channel RSPI channel number (valid: k_rspi_channel_0,
 *                    k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Channel successfully deinitialized and module stopped
 * @retval k_rx_err_invalid_arg channel >= 3 or RSPI base address lookup
 *         failed for the channel
 *
 * @pre channel must be in range [0, 2] (k_rspi_channel_0 through k_rspi_channel_2)
 * @pre No active SPI transfer should be in progress on this channel
 * @post RSPI peripheral disabled (SPCR.SPE = 0)
 * @post Module clock gated (MSTPCRB module stop bit set)
 * @post s_rspi_channel_initialized[channel] set to false
 *
 * @note Not thread-safe. Caller must ensure no concurrent transfers or
 *       accesses to the same channel during deinitialization.
 *
 * @see rspi_init_peripheral() Reinitialize channel after deinit
 * @see rspi_controller_deinit() Deinit for controller-mode channels
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_deinit(rspi_channel_t channel);

/* =============================================================================
 * RSPI (SPI) Functions - Controller Mode
 * =============================================================================
 */

/**
 * @enum rspi_freq_constants_t
 * @brief Common SPI clock frequency constants for controller mode
 *
 * @details
 * Named constants for commonly used SPI clock frequencies. Use these
 * instead of magic literals when configuring controller-mode SPI clock rate.
 * The RSPI bit rate register (SPBR) is calculated from these values and
 * the peripheral clock (PCLKB).
 *
 * @invariant All values must be positive and expressible by the SPBR register
 * @see rspi_init_controller() Uses freq_hz to calculate SPBR
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rspi_freq_1mhz  = 1000000,  /**< 1 MHz SPI clock frequency */
  k_rspi_freq_10mhz = 10000000, /**< 10 MHz SPI clock frequency (RPi5 link) */
} rspi_freq_constants_t;

/**
 * @brief RSPI controller mode configuration structure
 *
 * Configuration for initializing RSPI in controller mode.
 * Used for communicating with SPI peripheral devices (sensors, etc.).
 */
typedef struct {
  rspi_mode_t   spi_mode; /**< SPI mode (0-3): CPOL and CPHA configuration */
  uint32_t      freq_hz;  /**< SPI clock frequency in Hz (100kHz - 10MHz) */
  rx_port_pin_t cs;       /**< GPIO pin for chip select (type-safe port+pin) */
} rspi_controller_config_t;

/**
 * @brief Initialize RSPI channel in controller mode for external device communication
 *
 * @details
 * Configures the specified RSPI channel as an SPI controller for
 * communicating with external SPI peripheral devices such as sensors. Performs
 * argument validation, SPBR bit rate calculation, CS GPIO pin configuration,
 * and full hardware register setup following the RX72N Hardware Manual Section
 * 38.3.6 initialization sequence. Always configures 16-bit data frames with
 * word access mode.
 *
 * The initialization is split into three internal phases: argument validation,
 * hardware resource preparation (SPBR calculation, base address lookup, GPIO
 * setup), and register configuration (module clock enable, SPI mode, bit rate,
 * SPI enable). On any failure, no partial hardware state is left behind.
 *
 * @param[in] channel RSPI channel number (valid: k_rspi_channel_0,
 *                    k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 * @param[in] config  Pointer to rspi_controller_config_t containing:
 *                    - spi_mode: SPI mode (0-3) for CPOL/CPHA configuration
 *                    - freq_hz: SPI clock frequency in Hz (100 kHz to 10 MHz)
 *                    - cs: GPIO pin for chip select (rx_port_pin_t encoding)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Channel successfully initialized in controller mode
 * @retval k_rx_err_null_ptr config pointer is nullptr
 * @retval k_rx_err_invalid_arg channel >= 3, spi_mode > 3, freq_hz outside
 *         100 kHz - 10 MHz range, RSPI base address lookup failed, or CS
 *         GPIO port/pin is invalid
 * @retval k_rx_err_invalid_state channel already initialized in controller mode
 *         (call rspi_controller_deinit() first)
 *
 * @pre config must be a valid non-null pointer
 * @pre channel must not already be initialized in controller mode
 * @pre System clocks must be configured (PCLKB running at 60 MHz)
 * @post RSPI module clock enabled, registers configured, SPI enabled in
 *       controller mode (SPCR.MSTR = 1, SPCR.SPE = 1)
 * @post CS GPIO configured as output, driven high (inactive / deasserted)
 * @post s_rspi_controller_initialized[channel] set to true
 *
 * @note Not thread-safe. Initialize each channel from a single thread during
 *       system startup.
 *
 * @see rspi_controller_transfer_16bit() Perform 16-bit transfers after init
 * @see rspi_controller_set_cs() Manual chip select control
 * @see rspi_controller_deinit() Deinitialize controller-mode channel
 * @see rspi_controller_config_t Configuration structure
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_init_controller(rspi_channel_t                  channel,
                                            const rspi_controller_config_t* config);

/**
 * @brief Set chip select line state for an RSPI controller-mode channel
 *
 * @details
 * Controls the GPIO chip select pin associated with the specified RSPI
 * channel operating in controller mode. The CS signal is active-low: when
 * active is true, the GPIO pin is driven low (CS asserted, peripheral
 * selected); when active is false, the pin is driven high (CS deasserted,
 * peripheral released). The port and pin are read from the internal
 * s_rspi_cs_config[] array populated during rspi_init_controller().
 *
 * This function provides direct CS control for protocols that require
 * multi-transfer CS framing. For single 16-bit transfers with automatic
 * CS handling, use rspi_controller_transfer_16bit() instead.
 *
 * @param[in] channel RSPI channel number (valid: k_rspi_channel_0,
 *                    k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 * @param[in] active  true to assert CS (drive GPIO low),
 *                    false to deassert CS (drive GPIO high)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok CS line successfully driven to requested state
 * @retval k_rx_err_invalid_state channel >= 3 or channel not initialized
 *         in controller mode via rspi_init_controller()
 * @retval k_rx_err_invalid_arg GPIO port base address lookup failed for
 *         the stored CS port configuration
 *
 * @pre channel must be initialized via rspi_init_controller()
 * @pre s_rspi_cs_config[channel] must contain valid port/pin from init
 * @post GPIO PODR bit for the CS pin reflects the requested state
 * @post No other GPIO pins on the same port are modified (read-modify-write)
 *
 * @note Not thread-safe. Caller must provide external synchronization when
 *       accessing the same channel or GPIO port from multiple threads.
 *
 * @see rspi_init_controller() Configures CS pin during initialization
 * @see rspi_controller_transfer_16bit() Automatic CS handling for transfers
 * @see rspi_controller_deinit() Deasserts CS during cleanup
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_controller_set_cs(rspi_channel_t channel, bool active);

/**
 * @brief Perform a 16-bit full-duplex SPI transfer in controller mode with automatic CS
 *
 * @details
 * Executes a single 16-bit SPI transfer with full chip select lifecycle
 * management. The transfer sequence is:
 * 1. Assert CS (drive low) with ~300 ns setup delay
 * 2. Wait for TX buffer empty, write 16-bit tx_data to SPDR
 * 3. Wait for RX buffer full, read 16-bit response from SPDR
 * 4. Clear SPSR status flags (SPRF, OVRF)
 * 5. Apply ~300 ns hold delay, then deassert CS (drive high)
 *
 * On transfer error (timeout), CS is forcibly deasserted before returning
 * to avoid leaving the peripheral in a selected state. The CS setup and
 * hold delays use cycle-accurate NOP loops calibrated for 240 MHz core clock.
 *
 * @param[in]  channel RSPI channel number (valid: k_rspi_channel_0,
 *                     k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 * @param[in]  tx_data 16-bit data word to transmit
 * @param[out] rx_data Pointer to store the received 16-bit response
 *                     (must not be nullptr)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Transfer completed, *rx_data contains response, CS deasserted
 * @retval k_rx_err_null_ptr rx_data pointer is nullptr
 * @retval k_rx_err_invalid_state channel >= 3 or channel not initialized
 *         via rspi_init_controller()
 * @retval k_rx_err_invalid_arg RSPI base address lookup failed, or GPIO port
 *         lookup failed during CS assertion/deassertion
 * @retval k_rx_err_timeout TX buffer did not empty or RX buffer did not fill
 *         within the 10 ms timeout (CS is deasserted on this error)
 *
 * @pre channel must be initialized via rspi_init_controller()
 * @pre rx_data must be a valid non-null pointer
 * @post *rx_data contains received 16-bit value on success
 * @post CS is deasserted (GPIO driven high) on both success and error
 * @post SPSR status flags SPRF and OVRF are cleared on success
 *
 * @note Not thread-safe. Caller must provide external synchronization when
 *       accessing the same channel from multiple threads.
 *
 * @see rspi_init_controller() Must be called before this function
 * @see rspi_controller_set_cs() Manual CS control for multi-transfer framing
 * @see rspi_controller_deinit() Deinitialize controller-mode channel
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rspi_controller_transfer_16bit(rspi_channel_t channel, uint16_t tx_data, uint16_t* rx_data);

/**
 * @brief Deinitialize and disable an RSPI controller-mode channel
 *
 * @details
 * Performs a safe shutdown of the specified RSPI channel previously initialized
 * in controller mode. The deinitialization sequence is ordered to prevent bus
 * glitches: first deasserts CS to release any connected peripheral, then
 * disables SPI (SPCR.SPE = 0) to stop clock generation, then gates the
 * module clock via MSTPCRB for power savings, and finally clears internal
 * state (CS pin config and initialization flag). Register protection (PRCR)
 * is temporarily unlocked for module stop control.
 *
 * This function is safe to call on channels that were never initialized in
 * controller mode; it will still disable the hardware and clear the state.
 *
 * @param[in] channel RSPI channel number (valid: k_rspi_channel_0,
 *                    k_rspi_channel_1, k_rspi_channel_2; range 0-2)
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Channel successfully deinitialized and module stopped
 * @retval k_rx_err_invalid_arg channel >= 3 or RSPI base address lookup
 *         failed for the channel
 *
 * @pre channel must be in range [0, 2] (k_rspi_channel_0 through k_rspi_channel_2)
 * @pre No active SPI transfer should be in progress on this channel
 * @post CS GPIO deasserted (driven high) if channel was initialized
 * @post RSPI peripheral disabled (SPCR.SPE = 0)
 * @post Module clock gated (MSTPCRB module stop bit set)
 * @post s_rspi_cs_config[channel] cleared to defaults (port=0, pin=0)
 * @post s_rspi_controller_initialized[channel] set to false
 *
 * @note Not thread-safe. Caller must ensure no concurrent transfers or
 *       accesses to the same channel during deinitialization.
 *
 * @see rspi_init_controller() Reinitialize channel after deinit
 * @see rspi_deinit() Deinit for peripheral-mode channels
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rspi_controller_deinit(rspi_channel_t channel);

/* =============================================================================
 * UART Configuration Types
 * =============================================================================
 */

/**
 * @brief Default debug UART channel (SCI9 - CY7C65213 USB bridge)
 */
typedef enum : uint8_t {
  k_uart_debug_channel = 9 /**< Debug UART on SCI9 (PB7/TXD9, PB6/RXD9) */
} uart_defaults_t;

/**
 * @brief UART channel enumeration with type safety
 *
 * Prevents accidental argument swaps and provides compile-time type checking.
 * RX72N has SCI channels 0-12 (13 channels total).
 */
typedef enum : uint8_t {
  k_uart_channel_0  = 0,
  k_uart_channel_1  = 1,
  k_uart_channel_2  = 2,
  k_uart_channel_3  = 3,
  k_uart_channel_4  = 4,
  k_uart_channel_5  = 5,
  k_uart_channel_6  = 6,
  k_uart_channel_7  = 7,
  k_uart_channel_8  = 8,
  k_uart_channel_9  = 9,
  k_uart_channel_10 = 10,
  k_uart_channel_11 = 11,
  k_uart_channel_12 = 12
} uart_channel_t;

/**
 * @brief UART channel configuration structure
 *
 * Encapsulates all parameters needed to initialize a UART channel.
 * Using a struct instead of positional parameters prevents accidental
 * parameter swapping and makes the API more maintainable.
 */
typedef struct {
  uart_channel_t channel;  /**< SCI channel (0-12) */
  uint32_t       baudrate; /**< Baud rate (e.g., 9600, 115200) */
  rx_port_pin_t  tx_gpio;  /**< TX pin (rx_port_pin_t) */
  rx_port_pin_t  rx_gpio;  /**< RX pin (rx_port_pin_t) */
} uart_channel_config_t;

/* =============================================================================
 * UART HAL Functions
 * =============================================================================
 */

/**
 * @brief Initialize SCI channel for UART communication
 *
 * Performs full hardware initialization including:
 * - Module stop control (MSTPCRB)
 * - Pin function configuration (MPC)
 * - GPIO direction setup (PDR/PMR)
 * - SCI register configuration
 *
 * @param[in] config UART channel configuration (must not be NULL)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if config is nullptr or contains invalid values,
 *         k_rx_err_invalid_state if channel already initialized
 */
[[nodiscard]] rx_err_t uart_init_channel(const uart_channel_config_t* config);

/**
 * @brief Deinitialize SCI channel
 *
 * @param[in] channel SCI channel (0-12)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid
 */
[[nodiscard]] rx_err_t uart_deinit_channel(uart_channel_t channel);

/**
 * @brief Transmit a single character on specified channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] data Character to transmit
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t uart_putc_channel(uart_channel_t channel, char data);

/**
 * @brief Transmit a null-terminated string on specified channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] str Pointer to string
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if str is nullptr,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t uart_puts_channel(uart_channel_t channel, const char* str);

/**
 * @brief Write buffer to specified channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to write
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if data is nullptr,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t
uart_write_channel(uart_channel_t channel, const uint8_t* data, uint16_t length);

/**
 * @brief Receive a single character from specified channel (non-blocking)
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] data Pointer to store received character
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if data is nullptr,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_empty if no data available
 */
[[nodiscard]] rx_err_t uart_getc_channel(uart_channel_t channel, char* data);

/**
 * @brief Read available data from specified channel (non-blocking)
 *
 * Reads up to length bytes that are currently available.
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] data Pointer to buffer for received data
 * @param[in] length Maximum number of bytes to read
 * @param[out] bytes_read Pointer to store actual bytes read
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if data or bytes_read is nullptr,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t
uart_read_channel(uart_channel_t channel, uint8_t* data, uint16_t length, uint16_t* bytes_read);

/**
 * @brief Check if receive data is available on channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] available Pointer to store availability status
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if available is nullptr,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t uart_rx_available(uart_channel_t channel, bool* available);

/**
 * @brief SCI9 RXI ring-buffer diagnostic snapshot
 *
 * @details
 * Returns the current head/tail/dropped-byte counters for the SCI9
 * interrupt-driven RX ring. Useful for confirming that INT_SCI9_RXI9
 * is actually firing and that the byte stream is flowing end-to-end
 * from the gateway into comm_task.
 *
 * @param[out] head    Current write index (ISR-advanced).
 * @param[out] tail    Current read index (task-advanced).
 * @param[out] drops   Total bytes dropped due to ring-full since boot.
 * @return Always k_rx_ok.
 */
rx_err_t uart_sci9_rx_stats(uint16_t* head, uint16_t* tail, uint32_t* drops);
uint32_t uart_sci9_rxi_fires(void);

/* =============================================================================
 * UART Debug Functions - Convenience Wrappers for SCI9
 * =============================================================================
 */

/**
 * @brief Initialize debug UART (SCI9, 115200 bps, 8N1)
 *
 * Convenience wrapper for uart_init_channel(k_uart_debug_channel, 115200).
 * Used by rx_log for early boot logging before ThreadX is initialized.
 *
 * @return k_rx_ok on success, error code on failure
 */
[[nodiscard]] rx_err_t uart_debug_init(void);

/**
 * @brief Transmit a single character on debug UART (SCI9)
 *
 * @param[in] data Character to transmit
 */
void uart_debug_putc(char data);

/**
 * @brief Transmit a null-terminated string on debug UART (SCI9)
 *
 * @param[in] str Pointer to string
 */
void uart_debug_puts(const char* str);

/**
 * @brief Print a signed integer on debug UART (SCI9)
 *
 * @param[in] value Integer value to print
 */
void uart_debug_putint(int32_t value);

/**
 * @brief Print a hexadecimal value on debug UART (SCI9)
 *
 * @param[in] value Value to print
 * @param[in] digits Number of hex digits (1-8)
 */
void uart_debug_puthex(uint32_t value, uint8_t digits);

#ifdef __cplusplus
}
#endif
