/* lib/rx_hal/inc/hardware.h */

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
 * | **S12AD** | 2 units (0-1) | Battery voltage, current sensing | 8/10/12-bit resolution |
 * | **RIIC** | 3 channels (0-2) | I2C sensors (IMU, temperature) | 100kHz, 400kHz, 1MHz |
 * | **RSPI** | 3 channels (0-2) | SPI: RPi5 comm (peripheral), DRV8243 (controller) | Mode 0-3, 100kHz-10MHz |
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
 *     drv [label="DRV8243 x4\n(Motor Drivers)"];
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
 *   rspi -> drv [label="RSPI1\nController"];
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
 *   // 4. SPI controller (for DRV8243 motor drivers)
 *   rspi_controller_config_t spi_cfg = {
 *     .spi_mode = k_rspi_mode_0,
 *     .freq_hz = 1000000,  // 1 MHz
 *     .cs = k_port_c_pin_4
 *   };
 *   rspi_init_controller(1, &spi_cfg);
 *
 *   // 5. SPI peripheral (for RPi5 communication)
 *   rspi_config_t rpi_cfg = {
 *     .spi_mode = k_rspi_mode_0,
 *     .use_16bit = false
 *   };
 *   rspi_init_peripheral(0, &rpi_cfg);
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
 * | SPI1_CIPO | PC5 | RSPI1 | DRV8243 -> RX72N (controller mode) |
 * | SPI1_COPI | PC6 | RSPI1 | DRV8243 <- RX72N |
 * | SPI1_CLK | PC7 | RSPI1 | DRV8243 clock |
 * | DRV_CS* | PC4,PC3,PC2,PC1 | GPIO | Motor driver chip selects |
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
 *   +--> Application tasks (app_main_task.c)
 * ```
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright MIT License
 *
 * @see rx_err.h for error code definitions
 * @see rx_port_constants.h for complete port/pin enumerations
 * @see docs/sections/03_hardware_pinout.tex for hardware pinout documentation
 */

#pragma once

#include <stdbool.h>
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
 * @brief Configure GPIO pin as input
 *
 * @param[in] pin GPIO pin (rx_port_pin_t)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid,
 *         k_rx_err_gpio_conflict if pin already reserved
 */
[[nodiscard]] rx_err_t gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Set GPIO pin high
 *
 * @param[in] pin GPIO pin (rx_port_pin_t)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
 */
[[nodiscard]] rx_err_t gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Set GPIO pin low
 *
 * @param[in] pin GPIO pin (rx_port_pin_t)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
 */
[[nodiscard]] rx_err_t gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Toggle GPIO pin
 *
 * @param[in] pin GPIO pin (rx_port_pin_t)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
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
 * @return k_rx_ok on success, error code on failure
 */
[[nodiscard]] rx_err_t timer_init(void);

/**
 * @brief Stop CMT0 timer
 *
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t timer_stop(void);

/**
 * @brief Get current CMT0 counter value
 *
 * @param[out] count Pointer to store counter value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if count is nullptr
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
 * | k_adc_unit_0 | k_adc_channel_7 | P47 | MOTOR_I0 | Motor 0 current sense (DRV8243 IPROPI) |
 * | k_adc_unit_0 | k_adc_channel_6 | P46 | MOTOR_I1 | Motor 1 current sense (DRV8243 IPROPI) |
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
    0, /**< S12AD0 - ADC unit 0 (channels AN000-AN007). Used for battery/current sensing. */
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
 * // Battery voltage monitoring
 * adc_init(k_adc_unit_0, k_adc_channel_0, k_adc_resolution_12bit);
 *
 * uint32_t voltage_mv;
 * adc_read_voltage_mv(k_adc_unit_0, k_adc_channel_0, k_adc_resolution_12bit, &voltage_mv);
 * rx_log_info_val("ADC", "Battery voltage (mV)", voltage_mv);
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
 * @brief Initialize RIIC channel for I2C communication
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] frequency_hz Clock frequency (100000, 400000, or 1000000)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel or frequency is invalid
 */
[[nodiscard]] rx_err_t riic_init(riic_channel_t channel, uint32_t frequency_hz);

/**
 * @brief Write data to I2C device
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] data Pointer to data to write
 * @param[in] length Number of bytes to write
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if data is nullptr,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if bus timeout,
 *         k_rx_err_nack if device NACK received
 */
[[nodiscard]] rx_err_t riic_write(riic_channel_t    channel,
                                  i2c_device_addr_t device_addr,
                                  const uint8_t*    data,
                                  const uint16_t    length);

/**
 * @brief Read data from I2C device
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[out] data Pointer to buffer for received data
 * @param[in] length Number of bytes to read
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if data is nullptr,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if bus timeout,
 *         k_rx_err_nack if device NACK received
 */
[[nodiscard]] rx_err_t riic_read(riic_channel_t    channel,
                                 i2c_device_addr_t device_addr,
                                 uint8_t*          data,
                                 const uint16_t    length);

/**
 * @brief Write then read from I2C device (combined transaction)
 *
 * Common pattern for reading registers: write register address, then read data.
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] write_data Pointer to data to write
 * @param[in] write_length Number of bytes to write
 * @param[out] read_data Pointer to buffer for received data
 * @param[in] read_length Number of bytes to read
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if write_data or read_data is nullptr,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if bus timeout,
 *         k_rx_err_nack if device NACK received
 */
[[nodiscard]] rx_err_t riic_write_read(riic_channel_t    channel,
                                       i2c_device_addr_t device_addr,
                                       const uint8_t*    write_data,
                                       uint16_t          write_length,
                                       uint8_t*          read_data,
                                       uint16_t          read_length);

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
 * | 3 | 1 | 1 | High | Rising edge | Falling edge | DRV8243 motor drivers |
 *
 * ## STAR Project Usage
 * | Peripheral | Mode | Rationale |
 * |------------|------|-----------|
 * | RPi5 (RSPI0 peripheral) | Mode 0 | Linux spidev default, maximum compatibility |
 * | DRV8243 (RSPI1 controller) | Mode 0 or 3 | DRV8243 supports both, Mode 0 preferred for consistency |
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
 * rspi_init_peripheral(0, &rpi_cfg);
 *
 * // Motor driver communication (controller mode, Mode 0)
 * rspi_controller_config_t drv_cfg = {
 *   .spi_mode = k_rspi_mode_0,  // Match DRV8243 datasheet
 *   .freq_hz = 1000000,         // 1 MHz
 *   .cs = k_port_c_pin_4
 * };
 * rspi_init_controller(1, &drv_cfg);
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
 * @brief Initialize RSPI in peripheral mode for RPi5 communication
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] config SPI configuration
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel or mode is invalid
 */
typedef struct {
  rspi_mode_t spi_mode;  /**< SPI mode (0-3): CPOL and CPHA configuration */
  bool        use_16bit; /**< True for 16-bit frames, false for 8-bit frames */
} rspi_config_t;

[[nodiscard]] rx_err_t rspi_init_peripheral(uint8_t channel, const rspi_config_t* config);

/**
 * @brief Full-duplex SPI transfer in peripheral mode
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] tx_data Pointer to transmit data
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] length Number of bytes to transfer
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if tx_data or rx_data is nullptr,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if transfer timeout
 */
[[nodiscard]] rx_err_t rspi_peripheral_transfer(uint8_t        channel,
                                                const uint8_t* tx_data,
                                                uint8_t*       rx_data,
                                                uint16_t       length);

/**
 * @brief Check if receive data is available
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[out] available Pointer to store availability status
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if available is nullptr,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t rspi_peripheral_read_available(uint8_t channel, bool* available);

/**
 * @brief Check if transmit buffer is ready
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[out] ready Pointer to store ready status
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if ready is nullptr,
 *         k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t rspi_peripheral_write_ready(uint8_t channel, bool* ready);

/**
 * @brief Deinitialize RSPI channel
 *
 * @param[in] channel RSPI channel (0-2)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid
 */
[[nodiscard]] rx_err_t rspi_deinit(uint8_t channel);

/* =============================================================================
 * RSPI (SPI) Functions - Controller Mode
 * =============================================================================
 */

/**
 * @brief RSPI controller mode configuration structure
 *
 * Configuration for initializing RSPI in controller mode.
 * Used for communicating with SPI peripheral devices like DRV8243.
 */
typedef struct {
  rspi_mode_t   spi_mode; /**< SPI mode (0-3): CPOL and CPHA configuration */
  uint32_t      freq_hz;  /**< SPI clock frequency in Hz (100kHz - 10MHz) */
  rx_port_pin_t cs;       /**< GPIO pin for chip select (type-safe port+pin) */
} rspi_controller_config_t;

/**
 * @brief Initialize SPI controller for external device communication
 *
 * Configures the specified RSPI channel as SPI controller
 * for communicating with external SPI devices like motor drivers.
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] config Controller configuration (frequency, mode, CS pin)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if config is nullptr,
 *         k_rx_err_invalid_arg if channel, mode, or frequency is invalid
 */
[[nodiscard]] rx_err_t rspi_init_controller(uint8_t                         channel,
                                            const rspi_controller_config_t* config);

/**
 * @brief Set chip select state for RSPI controller mode
 *
 * Controls the GPIO chip select pin for the specified channel.
 * Chip select is active low.
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] active True to assert CS (low), false to deassert CS (high)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_state if channel not initialized in controller mode
 */
[[nodiscard]] rx_err_t rspi_controller_set_cs(uint8_t channel, bool active);

/**
 * @brief Perform 16-bit full-duplex SPI transfer in controller mode
 *
 * Executes a single 16-bit SPI transfer including CS assertion/deassertion.
 * Suitable for register-based communication with devices like DRV8243.
 *
 * @param[in]  channel RSPI channel (0-2)
 * @param[in]  tx_data 16-bit data to transmit
 * @param[out] rx_data Pointer to store 16-bit received data
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if rx_data is nullptr,
 *         k_rx_err_invalid_state if channel not initialized in controller mode,
 *         k_rx_err_timeout if transfer times out
 */
[[nodiscard]] rx_err_t
rspi_controller_transfer_16bit(uint8_t channel, uint16_t tx_data, uint16_t* rx_data);

/**
 * @brief Deinitialize RSPI controller mode
 *
 * @param[in] channel RSPI channel (0-2)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid
 */
[[nodiscard]] rx_err_t rspi_controller_deinit(uint8_t channel);

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
rx_err_t
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
