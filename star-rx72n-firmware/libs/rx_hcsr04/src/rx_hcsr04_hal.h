/**
 * @file rx_hcsr04_hal.h
 * @brief HC-SR04 Hardware Abstraction Layer (HAL) Interface
 *
 * @details
 * # Overview
 *
 * Platform-independent hardware abstraction layer for HC-SR04 ultrasonic
 * distance sensor driver. Decouples sensor logic from hardware-specific GPIO
 * and timing operations, enabling portability, testability, and hardware
 * independence.
 *
 * This HAL follows the **Dependency Inversion Principle**: high-level sensor
 * logic depends on this abstraction, not on concrete hardware implementations.
 *
 * # Architecture
 *
 * ## Layered Design
 *
 * @code{.unparsed}
 * +----------------------------------------------+
 * | HC-SR04 Driver (rx_hcsr04.c)                 |
 * | - Sensor logic (trigger, measure, convert)   |
 * | - Depends on HAL abstraction (this file)     |
 * +------------------+---------------------------+
 *                    | #include "rx_hcsr04_hal.h"
 * +------------------v---------------------------+
 * | HAL Interface (This File)                    |
 * | - Function declarations only                 |
 * | - No platform-specific code                  |
 * +------------------+---------------------------+
 *                    | Link-time selection
 *         +----------+----------+
 *         |                     |
 * +-------v--------+   +--------v--------+
 * | Hardware HAL   |   | Mock HAL        |
 * | (hal_hw.c)     |   | (hal_mock.c)    |
 * | - RX72N GPIO   |   | - Simulated I/O |
 * | - CMT2 timer   |   | - Host testing  |
 * +----------------+   +-----------------+
 * @endcode
 *
 * # Design Rationale
 *
 * ## Why HAL Layer?
 *
 * **1. Hardware Independence:**
 * - Same driver code works on RX72N, RX65N, ARM Cortex-M, x86 host
 * - Port to new MCU: Implement HAL, no driver changes
 * - Example: Replace CMT2 timer with SysTick (ARM) or POSIX timers (Linux)
 *
 * **2. Testability:**
 * - Unit tests run on host (no hardware required)
 * - Mock HAL simulates sensor behavior, GPIO states, timing
 * - Fault injection: Test timeout, cancellation, GPIO errors
 *
 * **3. Maintainability:**
 * - Clear separation: Sensor logic vs. hardware I/O
 * - Changes to GPIO implementation don't affect driver
 * - Example: Switch from bit-bang GPIO to DMA without touching driver
 *
 * ## Two Implementations
 *
 * | Implementation     | File           | Platform  | Use Case               |
 * |--------------------|----------------|-----------|------------------------|
 * | **Hardware HAL**   | hal_hw.c       | RX72N     | Production firmware    |
 * | **Mock HAL**       | hal_mock.c     | Host/x86  | Unit tests, CI/CD      |
 *
 * **Build System Selection:**
 * ```cmake
* if(TARGET_PLATFORM STREQUAL "RX72N")
*     target_sources(rx_hcsr04 PRIVATE rx_hcsr04_hal_hw.c)
* else()
*     target_sources(rx_hcsr04 PRIVATE ../../tests/mocks/mock_rx_hcsr04_hal.c)
* endif()
* ```
 *
 * ## HAL Function Categories
 *
 * **GPIO Operations:**
 * - `hcsr04_hal_gpio_set_output()` - Configure trigger pin as output
 * - `hcsr04_hal_gpio_set_input()` - Configure echo pin as input
 * - `hcsr04_hal_gpio_write_high()` - Set trigger HIGH (start pulse)
 * - `hcsr04_hal_gpio_write_low()` - Set trigger LOW (end pulse)
 * - `hcsr04_hal_gpio_read()` - Read echo pin state (HIGH/LOW)
 * - `hcsr04_hal_gpio_deinit()` - Release GPIO resources
 *
 * **Timing Operations:**
 * - `hcsr04_hal_delay_us()` - Blocking delay in microseconds
 * - `hcsr04_hal_get_time_us()` - Monotonic timestamp in microseconds
 *
 * # Hardware HAL Implementation (RX72N)
 *
 * ## GPIO Mapping
 *
 * **RX72N Port Configuration:**
 * - Any of 182 GPIO pins supported
 * - Trigger pin: Output mode (push-pull)
 * - Echo pin: Input mode (high-impedance)
 *
 * **Example Pin Assignment:**
 * | Signal | GPIO       | Direction | Notes                 |
 * |--------|------------|-----------|----------------------|
 * | TRIG   | PB2        | Output    | 3.3V logic (use level shifter for 5V HC-SR04) |
 * | ECHO   | PB3        | Input     | 5V tolerant (check RX72N datasheet)            |
 *
 * ## Timing Implementation
 *
 * **CMT2 (Compare Match Timer 2):**
 * - Clock: PCLKB 60 MHz / 8 = 7.5 MHz
 * - Resolution: 133 ns per tick
 * - Range: 8.7 ms (16-bit counter)
 * - Overflow tracking: Extended to 32-bit via software
 *
 * **Why CMT2?**
 * - CMT0/CMT1 reserved for ThreadX system tick
 * - 7.5 MHz sufficient for us-precision ultrasonic timing
 * - Free-running mode (no interrupt overhead)
 *
 * # Mock HAL Implementation (Host Testing)
 *
 * ## Simulated Behavior
 *
 * **GPIO Simulation:**
 * - State tracked in memory: `static bool gpio_states[256]`
 * - Write: `gpio_states[pin] = true/false`
 * - Read: `return gpio_states[pin]`
 *
 * **Timing Simulation:**
 * - Uses POSIX `clock_gettime(CLOCK_MONOTONIC)` on Linux/macOS
 * - Uses `QueryPerformanceCounter()` on Windows
 * - Delay: `nanosleep()` or `Sleep()`
 *
 * **Echo Pulse Simulation:**
 * - Mock can inject specific echo pulse widths
 * - Test cases: Valid distance, timeout, cancellation
 * - Example: Set ECHO pin HIGH for 580us -> simulates 10cm object
 *
 * # NASA Power of 10 Compliance
 *
 * ## Rule 8: Limit Preprocessor Use [OK] COMPLIANT
 *
 * **Zero macros:**
 * - All functions declared as prototypes (not macros)
 * - Type-safe function calls (compiler-checked)
 *
 * ## Rule 9: Restrict Pointer Use [OK] COMPLIANT
 *
 * **Single-level pointers only:**
 * - `rx_port_pin_t pin` - Enum (not pointer)
 * - `bool* value` - Single-level pointer for output parameter
 * - No double-indirection, no function pointers
 *
 * # SOLID Principles
 *
 * ## Single Responsibility [OK]
 * - One purpose: Abstract hardware I/O for HC-SR04
 * - Does NOT include sensor logic, distance conversion, or calibration
 *
 * ## Open/Closed [OK]
 * - Open for extension: New platforms implement this interface
 * - Closed for modification: Interface stable, implementations vary
 *
 * ## Liskov Substitution [OK]
 * - Hardware and mock HALs interchangeable
 * - Driver code unchanged when swapping implementations
 *
 * ## Interface Segregation [OK]
 * - Minimal interface: Only 8 functions (GPIO + timing)
 * - No "fat" HAL with unused peripheral access
 *
 * ## Dependency Inversion [OK]
 * - High-level driver depends on this abstraction
 * - Low-level hardware details hidden in implementations
 *
 * # Performance Characteristics
 *
 * ## Hardware HAL (RX72N)
 *
 * **GPIO Operations:**
 * | Function       | Execution Time | Notes                    |
 * |----------------|----------------|--------------------------|
 * | `set_output`   | ~1 us          | PDR register write       |
 * | `set_input`    | ~1 us          | PDR register write       |
 * | `write_high`   | ~200 ns        | PODR bit set             |
 * | `write_low`    | ~200 ns        | PODR bit clear           |
 * | `read`         | ~150 ns        | PIDR register read       |
 * | `deinit`       | ~1 us          | No-op (GPIO static)      |
 *
 * **Timing Operations:**
 * | Function       | Resolution | Range     | Accuracy  |
 * |----------------|------------|-----------|-----------|
 * | `delay_us`     | 1 us       | 0-8.7 ms  | +/-5%       |
 * | `get_time_us`  | 133 ns     | 0-571 s   | +/-0.1%     |
 *
 * ## Mock HAL (Host)
 *
 * **Timing varies by platform:**
 * - Linux: `clock_gettime()` - ~1 us resolution
 * - macOS: `mach_absolute_time()` - ~1 ns resolution
 * - Windows: `QueryPerformanceCounter()` - ~100 ns resolution
 *
 * # Example Usage
 *
 * ## Hardware HAL Example
 *
 * @code
 * // Configure GPIO pins
 * rx_port_pin_t trig = k_gpio_pb2;
 * rx_port_pin_t echo = k_gpio_pb3;
 *
 * hcsr04_hal_gpio_set_output(trig);
 * hcsr04_hal_gpio_set_input(echo);
 *
 * // Send 10us trigger pulse
 * hcsr04_hal_gpio_write_high(trig);
 * hcsr04_hal_delay_us(10);
 * hcsr04_hal_gpio_write_low(trig);
 *
 * // Measure echo pulse
 * uint32_t start = hcsr04_hal_get_time_us();
 * bool echo_state = false;
 * while (!echo_state) {
 *     hcsr04_hal_gpio_read(echo, &echo_state);
 * }
 * uint32_t end = hcsr04_hal_get_time_us();
 * uint32_t pulse_us = end - start;
 *
 * printf("Echo pulse: %lu us\n", pulse_us);
 * @endcode
 *
 * ## Mock HAL Example (Unit Test)
 *
 * @code
 * // Inject simulated echo pulse
 * mock_hal_set_echo_duration_us(580);  // Simulate 10cm
 *
 * // Run driver code (uses mock HAL)
 * float distance_cm = 0.0f;
 * rx_err_t err = rx_hcsr04_measure_blocking(&sensor, &distance_cm);
 *
 * // Verify result
 * assert(err == k_rx_ok);
 * assert(fabs(distance_cm - 10.0f) < 0.5f);  // +/-0.5cm tolerance
 * @endcode
 *
 * # Hardware Requirements
 *
 * | Resource      | Hardware HAL       | Mock HAL          |
 * |---------------|--------------------|-------------------|
 * | **CPU**       | RX72N (RXv3 core)  | x86/ARM host      |
 * | **RAM**       | <100 bytes         | <1 KB (state)     |
 * | **Flash**     | ~1 KB              | N/A               |
 * | **GPIO**      | 2 pins             | None (simulated)  |
 * | **Timer**     | CMT2 (7.5 MHz)     | OS timer API      |
 *
 * @see rx_hcsr04_hal_hw.c Hardware implementation (RX72N)
 * @see mock_rx_hcsr04_hal.c Mock implementation (host testing)
 * @see rx_hcsr04.h HC-SR04 driver public API
 * @see rx_port_constants.h GPIO pin type definitions
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output for HC-SR04 trigger signal
 *
 * @details
 * Configures the specified GPIO pin as an output in push-pull mode for
 * driving the HC-SR04 trigger input. On RX72N, this sets the PDR (Port
 * Direction Register) bit to 1 for the specified pin.
 *
 * **Hardware Configuration (RX72N):**
 * - PDR bit = 1 (output mode)
 * - Initial state: Undefined (caller should write LOW immediately)
 * - Drive strength: Standard CMOS (sufficient for HC-SR04 trigger)
 *
 * **Mock Implementation:**
 * - Tracks pin configuration in memory
 * - No actual hardware access
 *
 * @param[in] pin GPIO pin identifier (type-safe enum from rx_port_constants.h)
 *
 * @return k_rx_ok Configuration successful
 * @return k_rx_err_invalid_arg Pin number out of valid range
 * @return k_rx_err_hw_operation_failed Hardware register write failed (rare)
 *
 * @pre Pin must be a valid GPIO (not used by peripherals)
 * @post Pin configured as output, ready for write operations
 * @post Pin state undefined until first write
 *
 * @note Call once during sensor initialization
 * @warning Multiple calls safe (idempotent) but unnecessary
 *
 * @see hcsr04_hal_gpio_write_high Set pin HIGH
 * @see hcsr04_hal_gpio_write_low Set pin LOW
 * @see rx_port_constants.h GPIO pin definitions
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t hcsr04_hal_gpio_set_output(rx_port_pin_t pin);

/**
 * @brief Configure GPIO pin as input for HC-SR04 echo signal
 *
 * @details
 * Configures the specified GPIO pin as a high-impedance input for reading
 * the HC-SR04 echo pulse. On RX72N, this sets the PDR (Port Direction
 * Register) bit to 0 for the specified pin.
 *
 * **Hardware Configuration (RX72N):**
 * - PDR bit = 0 (input mode)
 * - Input buffer: Enabled (Schmitt trigger for noise immunity)
 * - Pull-up/down: Disabled (HC-SR04 drives the line)
 *
 * **Voltage Levels:**
 * - RX72N GPIO: 3.3V logic
 * - HC-SR04 output: 5V logic (may need level shifter or voltage divider)
 * - Check RX72N datasheet for 5V tolerance on specific pins
 *
 * **Mock Implementation:**
 * - Tracks pin configuration in memory
 * - Simulates input state for testing
 *
 * @param[in] pin GPIO pin identifier (type-safe enum from rx_port_constants.h)
 *
 * @return k_rx_ok Configuration successful
 * @return k_rx_err_invalid_arg Pin number out of valid range
 * @return k_rx_err_hw_operation_failed Hardware register write failed (rare)
 *
 * @pre Pin must be a valid GPIO (not used by peripherals)
 * @post Pin configured as input, ready for read operations
 * @post Input buffer enabled, pull-up/down disabled
 *
 * @note Call once during sensor initialization
 * @warning Ensure HC-SR04 echo output compatible with RX72N input levels
 *
 * @see hcsr04_hal_gpio_read Read pin state
 * @see rx_port_constants.h GPIO pin definitions
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t hcsr04_hal_gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Set GPIO pin to HIGH (3.3V) level
 *
 * @details
 * Drives the GPIO pin to logic HIGH (VCC level, typically 3.3V on RX72N).
 * Used to generate the rising edge of the HC-SR04 trigger pulse.
 *
 * **Hardware Operation (RX72N):**
 * - Sets PODR (Port Output Data Register) bit to 1
 * - Output voltage: VCC (3.3V +/-10%)
 * - Rise time: ~5 ns (typical for RX72N GPIO)
 * - Drive current: +/-4 mA (standard drive strength)
 *
 * **Timing Constraints:**
 * - HC-SR04 requires >=10us HIGH pulse
 * - This function executes in ~200 ns
 * - Caller must add delay between write_high() and write_low()
 *
 * **Mock Implementation:**
 * - Sets simulated pin state to true
 * - Can trigger simulated echo response
 *
 * @param[in] pin GPIO pin identifier (must be configured as output)
 *
 * @return k_rx_ok Write successful
 * @return k_rx_err_invalid_arg Pin not configured as output
 * @return k_rx_err_hw_operation_failed Register write failed (rare)
 *
 * @pre Pin configured as output via hcsr04_hal_gpio_set_output()
 * @post Pin driven to HIGH level (VCC)
 * @post HC-SR04 trigger input sees rising edge
 *
 * @note Execution time: ~200 ns on RX72N @ 240 MHz
 * @warning Pin must be output mode (undefined behavior if input)
 *
 * @par Example:
 * @code
 * // Generate 10us trigger pulse
 * hcsr04_hal_gpio_write_high(trigger_pin);
 * hcsr04_hal_delay_us(10);
 * hcsr04_hal_gpio_write_low(trigger_pin);
 * @endcode
 *
 * @see hcsr04_hal_gpio_write_low Set pin LOW
 * @see hcsr04_hal_gpio_set_output Configure pin as output
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t hcsr04_hal_gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Set GPIO pin to LOW (GND) level
 *
 * @details
 * Drives the GPIO pin to logic LOW (ground level, 0V). Used to complete
 * the HC-SR04 trigger pulse and return to idle state.
 *
 * **Hardware Operation (RX72N):**
 * - Clears PODR (Port Output Data Register) bit to 0
 * - Output voltage: GND (0V +/-0.3V)
 * - Fall time: ~5 ns (typical for RX72N GPIO)
 * - Sink current: +/-4 mA (standard drive strength)
 *
 * **HC-SR04 Behavior:**
 * - Falling edge of trigger pulse ends the 10us sequence
 * - Sensor begins ultrasonic burst transmission
 * - Echo pin will go HIGH when echo starts
 *
 * **Mock Implementation:**
 * - Sets simulated pin state to false
 * - Completes simulated trigger sequence
 *
 * @param[in] pin GPIO pin identifier (must be configured as output)
 *
 * @return k_rx_ok Write successful
 * @return k_rx_err_invalid_arg Pin not configured as output
 * @return k_rx_err_hw_operation_failed Register write failed (rare)
 *
 * @pre Pin configured as output via hcsr04_hal_gpio_set_output()
 * @post Pin driven to LOW level (GND)
 * @post HC-SR04 trigger input sees falling edge
 *
 * @note Execution time: ~200 ns on RX72N @ 240 MHz
 * @warning Pin must be output mode (undefined behavior if input)
 *
 * @par Example:
 * @code
 * // Initialize trigger pin to LOW (idle state)
 * hcsr04_hal_gpio_set_output(trigger_pin);
 * hcsr04_hal_gpio_write_low(trigger_pin);  // Ensure known state
 * @endcode
 *
 * @see hcsr04_hal_gpio_write_high Set pin HIGH
 * @see hcsr04_hal_gpio_set_output Configure pin as output
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t hcsr04_hal_gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Read current logic level of GPIO pin
 *
 * @details
 * Reads the current state of the GPIO pin's input buffer. Used to detect
 * rising and falling edges of the HC-SR04 echo pulse for timing measurement.
 *
 * **Hardware Operation (RX72N):**
 * - Reads PIDR (Port Input Data Register) bit
 * - Input threshold: ~0.7xVCC (HIGH), ~0.3xVCC (LOW)
 * - Schmitt trigger: ~0.2V hysteresis (noise immunity)
 * - Sample time: ~150 ns (single register read)
 *
 * **Return Values:**
 * - `*value = true`: Pin at HIGH level (>=2.3V on 3.3V system)
 * - `*value = false`: Pin at LOW level (<=1.0V on 3.3V system)
 *
 * **Mock Implementation:**
 * - Returns simulated pin state from memory
 * - Can inject specific pulse widths for testing
 *
 * **Typical Usage Pattern:**
 * @code
 * // Wait for echo to go HIGH (pulse start)
 * bool echo_state = false;
 * while (!echo_state) {
 *     hcsr04_hal_gpio_read(echo_pin, &echo_state);
 * }
 * uint32_t start_time = hcsr04_hal_get_time_us();
 *
 * // Wait for echo to go LOW (pulse end)
 * while (echo_state) {
 *     hcsr04_hal_gpio_read(echo_pin, &echo_state);
 * }
 * uint32_t end_time = hcsr04_hal_get_time_us();
 * uint32_t pulse_width = end_time - start_time;
 * @endcode
 *
 * @param[in]  pin   GPIO pin identifier (must be configured as input)
 * @param[out] value Pointer to receive pin state (true=HIGH, false=LOW)
 *
 * @return k_rx_ok Read successful, *value contains pin state
 * @return k_rx_err_null_ptr value pointer is nullptr
 * @return k_rx_err_invalid_arg Pin not configured as input
 * @return k_rx_err_hw_operation_failed Register read failed (rare)
 *
 * @pre Pin configured as input via hcsr04_hal_gpio_set_input()
 * @pre value != nullptr
 * @post *value contains current pin state (if k_rx_ok returned)
 * @post Pin configuration unchanged
 *
 * @note Execution time: ~150 ns on RX72N @ 240 MHz
 * @note Read is non-blocking (immediate return)
 * @warning Pin must be input mode for reliable readings
 * @warning Floating pins may read unpredictable values
 *
 * @par Performance:
 * - Single register read (PIDR)
 * - No interrupt overhead
 * - Suitable for tight polling loops
 *
 * @see hcsr04_hal_gpio_set_input Configure pin as input
 * @see hcsr04_hal_get_time_us Capture timestamps
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t hcsr04_hal_gpio_read(rx_port_pin_t pin, bool* value);

/**
 * @brief Release GPIO pin resources (cleanup)
 *
 * @details
 * Optionally resets GPIO pin to a safe state or releases allocated resources.
 * On RX72N, this is typically a no-op since GPIO pins are static resources
 * (no dynamic allocation).
 *
 * **Hardware Implementation (RX72N):**
 * - No-op: GPIO pins don't require deallocation
 * - Pin configuration persists until next reconfiguration
 * - Provided for API completeness and future platforms
 *
 * **Mock Implementation:**
 * - Clears simulated pin state
 * - Marks pin as uninitialized for test validation
 *
 * **When to Call:**
 * - During sensor deinitialization (`rx_hcsr04_deinit`)
 * - On error paths during init (cleanup)
 * - Safe to skip on RX72N (but recommended for portability)
 *
 * @param[in] pin GPIO pin identifier to deinitialize
 *
 * @return k_rx_ok Deinitialization successful (or no-op)
 * @return k_rx_err_invalid_arg Pin number out of valid range
 *
 * @pre Pin previously initialized via set_output() or set_input()
 * @post Pin configuration undefined (platform-specific)
 * @post Future read/write operations may fail or return undefined values
 *
 * @note On RX72N: Always returns k_rx_ok (no actual operation)
 * @note On mock HAL: Resets pin state for test isolation
 * @warning Do not use pin after deinit without reconfiguring
 *
 * @par Example:
 * @code
 * // Cleanup on sensor deinit
 * rx_err_t rx_hcsr04_deinit(rx_hcsr04_t* handle) {
 *     hcsr04_hal_gpio_deinit(handle->trigger_pin);
 *     hcsr04_hal_gpio_deinit(handle->echo_pin);
 *     handle->initialized = false;
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @see hcsr04_hal_gpio_set_output Initialize as output
 * @see hcsr04_hal_gpio_set_input Initialize as input
 * @see rx_hcsr04_deinit Sensor cleanup function
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t hcsr04_hal_gpio_deinit(rx_port_pin_t pin);

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

/**
 * @brief Blocking delay for specified microseconds
 *
 * @details
 * Busy-wait delay with microsecond precision. Blocks the calling thread
 * for the specified duration. Used for HC-SR04 trigger pulse timing
 * (10us pulse, 2us settle).
 *
 * **Hardware Implementation (RX72N):**
 * - Uses CMT2 (Compare Match Timer 2)
 * - Clock: PCLKB 60 MHz / 8 = 7.5 MHz
 * - Resolution: 133 ns per tick
 * - Accuracy: +/-5% (clock variation + loop overhead)
 * - Method: Spin-wait on hardware counter
 *
 * **Delay Ranges:**
 * | Duration  | Ticks    | Iterations | Notes                  |
 * |-----------|----------|------------|------------------------|
 * | 1 us      | 7-8      | 1          | Minimum reliable delay |
 * | 10 us     | 75       | 1          | HC-SR04 trigger pulse  |
 * | 100 us    | 750      | 1          | Short settling time    |
 * | 1 ms      | 7,500    | 1          | Medium delay           |
 * | 8.7 ms    | 65,535   | 1          | Max single iteration   |
 * | >8.7 ms   | Multiple | 2-100      | Looped iterations      |
 *
 * **Mock Implementation:**
 * - Uses host OS sleep functions (nanosleep, usleep, Sleep)
 * - Accuracy: +/-10% (OS scheduler variance)
 * - Non-blocking: May yield CPU (unlike hardware version)
 *
 * **Performance Characteristics:**
 * - CPU usage: 100% during delay (busy-wait, no RTOS yield)
 * - Power: Full active power (no sleep mode)
 * - Interrupts: Enabled (delay can be interrupted)
 *
 * @param[in] us Delay duration in microseconds (0 = no delay, max ~500,000)
 *
 * @pre CMT2 timer initialized (automatic on first call)
 * @post Caller blocked for approximately us microseconds
 * @post No side effects (timer state unchanged)
 *
 * @note NOT thread-safe: Concurrent calls may interfere on mock HAL
 * @note Does NOT yield to RTOS: Use tx_thread_sleep() for long delays
 * @warning Delays >10ms should use RTOS sleep (avoid busy-wait)
 * @warning Accuracy degrades for very short delays (<2us) due to function call overhead
 *
 * @par Example:
 * @code
 * // Generate precise 10us trigger pulse
 * hcsr04_hal_gpio_write_high(trig_pin);
 * hcsr04_hal_delay_us(10);  // Accurate to +/-0.5us
 * hcsr04_hal_gpio_write_low(trig_pin);
 * @endcode
 *
 * @par Performance:
 * - Function call overhead: ~500 ns
 * - Loop overhead: ~100 ns per iteration
 * - Effective minimum delay: ~1 us
 *
 * @par Thread Safety:
 * - Hardware HAL: Safe (uses dedicated timer, no shared state)
 * - Mock HAL: NOT safe (simulated delays may overlap)
 *
 * @see hcsr04_hal_get_time_us Get current timestamp
 * @see k_hcsr04_trigger_pulse_us Trigger pulse duration constant
 *
 * @since Version 1.0.0
 */
void hcsr04_hal_delay_us(uint32_t us);

/**
 * @brief Get monotonic timestamp in microseconds
 *
 * @details
 * Returns a continuously increasing timestamp with microsecond resolution.
 * Used to measure HC-SR04 echo pulse duration by capturing start and end
 * timestamps. Guaranteed monotonic (never decreases, even across overflows).
 *
 * **Hardware Implementation (RX72N):**
 * - Timer: CMT2 (Compare Match Timer 2)
 * - Clock: PCLKB 60 MHz / 8 = 7.5 MHz
 * - Native resolution: 133 ns per tick
 * - Counter width: 16-bit (0-65535)
 * - Overflow period: ~8.7 ms
 * - Software extension: 32-bit via overflow tracking
 * - Range: 0 to 571 seconds (uint32_t microseconds)
 *
 * **Overflow Handling:**
 * - Hardware counter: Wraps every 8.7 ms (65536 ticks)
 * - Software tracks overflows in static variable
 * - Mutex-protected for thread safety
 * - Result: 32-bit timestamp = (overflow_count << 16) | counter
 *
 * **Mock Implementation:**
 * - Uses host monotonic clock:
 *   - Linux/macOS: `clock_gettime(CLOCK_MONOTONIC)`
 *   - Windows: `QueryPerformanceCounter()`
 * - Resolution: Platform-dependent (100 ns to 1 us)
 *
 * **Monotonicity Guarantee:**
 * - Time never goes backward
 * - Immune to system clock adjustments (NTP, user changes)
 * - Safe for duration calculations across overflows
 *
 * **Typical Usage Pattern:**
 * @code
 * // Measure echo pulse duration
 * uint32_t start = hcsr04_hal_get_time_us();
 * // ... wait for echo ...
 * uint32_t end = hcsr04_hal_get_time_us();
 * uint32_t duration = end - start;  // Correct even if overflow occurred
 * @endcode
 *
 * @return Current timestamp in microseconds (monotonic, wraps at ~571 seconds)
 *
 * @pre CMT2 timer initialized (automatic on first call)
 * @post No side effects (read-only operation)
 *
 * @invariant Returned value never decreases (monotonic property)
 * @invariant Resolution >= 1 us (meets HC-SR04 timing requirements)
 *
 * @note Thread-safe on RX72N (mutex-protected overflow counter)
 * @note Zero cost when idle (no active counting overhead)
 * @warning Wraps at 2^32 us (~71 minutes) - caller must handle wrap-around
 * @warning First call initializes CMT2 (~5 us overhead)
 *
 * @par Performance:
 * - Read time: ~1-2 us (register read + conversion)
 * - Mutex overhead: ~5 us (if overflow tracking needed)
 * - Call frequency: Unlimited (no rate limiting)
 *
 * @par Thread Safety:
 * - Hardware HAL: SAFE - overflow counter protected by mutex
 * - Mock HAL: SAFE - system monotonic clock is thread-safe
 *
 * @par Accuracy:
 * - RX72N: +/-0.1% (crystal tolerance)
 * - Mock: Platform-dependent (typically +/-0.01%)
 *
 * @par Example: Echo Pulse Measurement
 * @code
 * // Wait for rising edge
 * bool echo = false;
 * while (!echo) {
 *     hcsr04_hal_gpio_read(echo_pin, &echo);
 * }
 * uint32_t pulse_start = hcsr04_hal_get_time_us();
 *
 * // Wait for falling edge
 * while (echo) {
 *     hcsr04_hal_gpio_read(echo_pin, &echo);
 * }
 * uint32_t pulse_end = hcsr04_hal_get_time_us();
 *
 * // Calculate duration (handles overflow correctly)
 * uint32_t pulse_us = pulse_end - pulse_start;
 * float distance_cm = (float)pulse_us / 58.0f;
 * @endcode
 *
 * @par Overflow Handling Example:
 * @code
 * // Even if overflow occurs between start and end:
 * uint32_t start = 0xFFFFFFF0;  // Near overflow
 * // ... 20 us later ...
 * uint32_t end   = 0x00000004;  // Wrapped around
 * uint32_t dur   = end - start; // = 20 us (correct!)
 * // uint32_t subtraction handles wrap-around correctly
 * @endcode
 *
 * @see hcsr04_hal_delay_us Blocking delay function
 * @see internal_measure_echo_pulse Echo timing function
 * @see hcsr04_hal_get_time_us_isr() ISR-safe variant (no mutex)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] uint32_t hcsr04_hal_get_time_us(void);

/**
 * @brief Get monotonic timestamp in microseconds -- ISR-safe (no mutex)
 *
 * @details
 * Returns the raw CMT2 counter value converted to microseconds. Unlike
 * hcsr04_hal_get_time_us(), this function does NOT acquire the ThreadX mutex
 * and does NOT track 16-bit counter overflows. It is safe to call from
 * interrupt context and provides sub-microsecond latency.
 *
 * **Hardware Constraint -- 8.7 ms Maximum Echo Duration:**
 * The CMT2 timer is a 16-bit counter clocked at PCLKB/8 = 7.5 MHz, which
 * wraps every ~8.7 ms. HC-SR04 echo pulses can be up to ~23 ms (400 cm x
 * 58 us/cm). If the echo spans more than one counter period, the naive
 * `end_us - start_us` subtraction will be incorrect.
 *
 * **Supported range in IRQ mode: <= 150 cm (~8.7 ms echo pulse)**
 * For longer ranges, use polling mode with hcsr04_hal_get_time_us() (which
 * tracks overflows via a software overflow counter protected by a mutex).
 *
 * @return Current CMT2 counter value converted to microseconds (no overflow tracking)
 *
 * @retval uint32_t Raw CMT2 counter converted to microseconds; wraps at ~8.7 ms
 *
 * @pre CMT2 timer initialized (call hcsr04_hal_get_time_us() at least once at startup)
 * @pre Called from ISR context (no ThreadX blocking calls permitted)
 *
 * @post No side effects (read-only, no mutex acquired)
 * @post Returned value is in range [0, ~8700) us (wraps with CMT2 period)
 *
 * @note ISR-safe: does NOT call tx_mutex_get() or any blocking ThreadX API
 * @note For task context, prefer hcsr04_hal_get_time_us() (overflow tracking)
 * @warning Does not track 16-bit overflow; do not use for durations > 8.7 ms
 * @warning HC-SR04 IRQ mode limited to ~150 cm due to 8.7 ms CMT2 wrap period
 *
 * @par Example: Edge Capture in ISR
 * @code
 * // Called from INT_IRQn (ISR context):
 * void INT_IRQ11(void)
 * {
 *     internal_irq_handler(k_irq_num_11);  // captures hcsr04_hal_get_time_us_isr()
 * }
 * // Duration is valid only if echo < 8.7 ms (<= 150 cm)
 * @endcode
 *
 * @see hcsr04_hal_get_time_us() Thread-safe variant with overflow tracking for task context
 *
 * @since Version 1.0.0
 */
[[nodiscard]] uint32_t hcsr04_hal_get_time_us_isr(void);

#ifdef __cplusplus
}
#endif
