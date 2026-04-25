/**
 * @file rx_hcsr04.h
 * @brief HC-SR04 Ultrasonic Distance Sensor Driver for RX72N
 *
 * @details
 * Production-ready driver for HC-SR04 ultrasonic distance sensors with blocking
 * and asynchronous measurement modes, temperature compensation, and comprehensive
 * error handling. Designed for obstacle detection and collision avoidance in
 * autonomous robotics applications.
 *
 * ## Sensor Specifications
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | **Measurement Range** | 2cm - 400cm | Effective operating range |
 * | **Accuracy** | +/-3mm | Under ideal conditions |
 * | **Resolution** | ~3mm | Based on timing resolution |
 * | **Beam Angle** | 15deg | Effective detection cone |
 * | **Frequency** | 40 kHz | Ultrasonic carrier |
 * | **Supply Voltage** | 5V +/-10% | Requires level shifter for 3.3V MCU |
 * | **Current Draw** | 15mA typical | 30mA peak during burst |
 * | **Trigger Input** | 10us HIGH pulse | Initiates measurement |
 * | **Echo Output** | Pulse width = distance | 58us per cm (roundtrip) |
 * | **Max Measurement Rate** | ~16 Hz | 60ms minimum gap between readings |
 *
 * ## Hardware Setup
 *
 * **Pin Connections:**
 * ```
 * HC-SR04          RX72N (via level shifter)
 * +----------+     +------------+
 * |   VCC    |-----| 5V supply  |
 * |   TRIG   |<----| GPIO output| (10us pulse)
 * |   ECHO   |---->| GPIO input | (pulse width)
 * |   GND    |-----| GND        |
 * +----------+     +------------+
 * ```
 *
 * **Level Shifting:**
 * - RX72N GPIO: 3.3V logic
 * - HC-SR04: 5V logic
 * - Use bidirectional level shifter (e.g., TXS0108E)
 *
 * **Mounting:**
 * - Keep sensor level and stable
 * - Avoid soft/angled surfaces (sound absorption)
 * - Clear 15deg cone in front of sensor
 *
 * ## Operating Principle
 *
 * **Measurement Sequence:**
 * ```
 * @startuml
 * participant MCU
 * participant HCSR04
 * participant Object
 *
 * MCU -> HCSR04: TRIG pulse (10us HIGH)
 * activate HCSR04
 * HCSR04 -> Object: Ultrasonic burst (8 pulses @ 40kHz)
 * Object --> HCSR04: Echo reflection
 * HCSR04 -> MCU: ECHO pulse (width = time-of-flight)
 * deactivate HCSR04
 * MCU -> MCU: Calculate distance = (pulse_width * speed_of_sound) / 2
 * @enduml
 * ```
 *
 * **Distance Calculation:**
 * ```
 * Speed of sound at 20degC: 343 m/s = 0.0343 cm/us
 * Roundtrip distance: echo_time_us * 0.0343 cm/us
 * One-way distance: (echo_time_us * 0.0343) / 2
 * Simplified: distance_cm = echo_time_us / 58
 * ```
 *
 * ## Features
 *
 * **Measurement Modes:**
 * - **Blocking** - Simple synchronous API, blocks until complete
 * - **Asynchronous** - Non-blocking with callback, requires worker thread
 *
 * **Temperature Compensation:**
 * - Adjusts speed of sound based on ambient temperature
 * - Improves accuracy by ~0.17% per degC
 * - Integrates with DS18B20 temperature sensor
 *
 * **Error Detection:**
 * - Timeout detection (no echo = object >400cm or absent)
 * - Range validation (rejects measurements <2cm or >400cm)
 * - Cancellation support for async measurements
 *
 * **Statistics Tracking:**
 * - Total measurement count
 * - Timeout occurrences
 * - Out-of-range errors
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto, no recursion, clear state machine |
 * | 2. Fixed loop bounds | [OK] | All loops bounded by typed enum constants |
 * | 3. No dynamic allocation | [OK] | Zero malloc/free, static thread stacks |
 * | 4. Short functions | [OK] | Max 60 lines per function |
 * | 5. Assertions | [OK] | Min 2 checks per function (nullptr, state) |
 * | 6. Data scope | [OK] | Variables declared at smallest scope |
 * | 7. Check returns | [OK] | All function returns validated |
 * | 8. Limit preprocessor | [OK] | Typed enums only, no magic numbers |
 * | 9. Pointer restrictions | [OK] | Function pointers for callbacks only |
 * | 10. Compile warnings | [OK] | -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * **Single Responsibility:**
 * - One purpose: HC-SR04 distance measurement
 * - GPIO operations delegated to HAL
 * - Timing delegated to platform-specific implementation
 *
 * **Open/Closed:**
 * - Extensible via HAL interface (rx_hcsr04_hal.h)
 * - New platforms supported without modifying driver
 *
 * **Liskov Substitution:**
 * - HAL implementations interchangeable (hardware/mock)
 * - Mock HAL enables host-side unit testing
 *
 * **Interface Segregation:**
 * - Clean separation: init, measure, temp compensation, utilities
 * - Optional features (async, temp) can be omitted
 *
 * **Dependency Inversion:**
 * - Depends on HAL abstraction (rx_hcsr04_hal.h)
 * - Hardware implementation injected at link time
 *
 * ## Performance
 *
 * **Typical Operation Times:**
 * - Initialization: <1ms (GPIO configuration)
 * - Trigger pulse: 10us
 * - Echo wait: 116us (2cm) to 23.2ms (400cm)
 * - Total measurement: 1-30ms depending on distance
 * - Timeout (no object): 30ms
 *
 * **Measurement Rate:**
 * - Minimum gap: 60ms (per HC-SR04 datasheet)
 * - Maximum rate: ~16 Hz
 * - Faster polling causes echo interference
 *
 * ## Thread Safety
 *
 * **Driver State:**
 * - NOT thread-safe - no internal locking
 * - Concurrent access to same handle causes race conditions
 * - Multiple handles (different sensors) safe if no shared resources
 *
 * **Async Worker Thread:**
 * - Thread-safe async operations via mutex-protected queue
 * - Single worker services all sensor instances
 * - FIFO processing order
 *
 * @par Example: Blocking Measurement
 * @code
 * // 1. Initialize sensor
 * rx_hcsr04_t sensor;
 * rx_hcsr04_config_t config = {
 *     .trigger_pin = k_gpio_pb2,  // Type-safe GPIO
 *     .echo_pin = k_gpio_pb3,
 *     .timeout_us = k_hcsr04_echo_timeout_us  // 30ms default
 * };
 *
 * rx_err_t err = rx_hcsr04_init(&sensor, &config);
 * if (err != k_rx_ok) handle_error(err);
 *
 * // 2. Perform measurement
 * float distance_cm = 0.0f;
 * err = rx_hcsr04_measure_blocking(&sensor, &distance_cm);
 *
 * if (err == k_rx_ok) {
 *     printf("Object at %.1f cm\n", distance_cm);
 * } else if (err == k_rx_err_timeout) {
 *     printf("No object detected (>400cm)\n");
 * } else if (err == k_rx_err_out_of_range) {
 *     printf("Object too close (<2cm)\n");
 * }
 *
 * // 3. Cleanup
 * rx_hcsr04_deinit(&sensor);
 * @endcode
 *
 * @par Example: Asynchronous Measurement with Callback
 * @code
 * // Callback invoked from worker thread
 * void measurement_callback(rx_hcsr04_t* sensor,
 *                          const rx_hcsr04_result_t* result,
 *                          void* user_data)
 * {
 *     if (result->status == k_rx_ok) {
 *         printf("Async: %.1f cm (%.2f in)\n",
 *                result->distance_cm, result->distance_in);
 *     } else {
 *         printf("Async error: %d\n", result->status);
 *     }
 * }
 *
 * // Initialize worker thread ONCE at startup
 * rx_hcsr04_worker_init();
 *
 * // Start async measurement
 * rx_err_t err = rx_hcsr04_measure_async(&sensor, measurement_callback, nullptr);
 * if (err == k_rx_ok) {
 *     // Returns immediately, callback invoked when complete
 *     // Do other work while measurement in progress...
 * }
 *
 * // Cancel if needed
 * if (rx_hcsr04_is_busy(&sensor)) {
 *     rx_hcsr04_cancel(&sensor);
 * }
 * @endcode
 *
 * @par Example: Temperature Compensation
 * @code
 * // Read temperature from DS18B20
 * float temp_c = 0.0f;
 * rx_ds18b20_read_temperature(&temp_sensor, &temp_c);
 *
 * // Enable compensation (applies to all subsequent measurements)
 * rx_hcsr04_set_temperature(&distance_sensor, temp_c);
 *
 * // Measurements now use temperature-adjusted speed of sound
 * float distance_cm = 0.0f;
 * rx_hcsr04_measure_blocking(&distance_sensor, &distance_cm);
 *
 * // Accuracy improvement at 10degC vs 20degC: ~1.75%
 * // For 100cm reading: +/-1.75cm error correction
 * @endcode
 *
 * @par Example: Statistics Monitoring
 * @code
 * uint32_t total = 0, timeouts = 0, range_errors = 0;
 *
 * rx_hcsr04_get_stats(&sensor, &total, &timeouts, &range_errors);
 *
 * float timeout_rate = (float)timeouts / total * 100.0f;
 * float error_rate = (float)range_errors / total * 100.0f;
 *
 * printf("Measurements: %lu\n", total);
 * printf("Timeout rate: %.1f%%\n", timeout_rate);
 * printf("Error rate: %.1f%%\n", error_rate);
 *
 * // Reset counters
 * rx_hcsr04_reset_stats(&sensor);
 * @endcode
 *
 * @see rx_hcsr04_hal.h Hardware abstraction layer
 * @see rx_port_constants.h GPIO pin definitions
 * @see rx_ds18b20.h Temperature sensor integration
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum rx_hcsr04_timing_t
 * @brief HC-SR04 timing constants based on ultrasonic physics
 *
 * @details
 * Timing parameters derived from HC-SR04 datasheet and ultrasonic propagation.
 * All values calibrated for speed of sound at 20degC (343 m/s).
 *
 * **Physics Background:**
 * - Speed of sound at 20degC: 343 m/s = 34,300 cm/s = 0.0343 cm/us
 * - Roundtrip distance: sound travels to object and back
 * - Time per cm (roundtrip): 1 / 0.0343 = 29.15 us/cm (one-way)
 * - Time per cm (total): 29.15 * 2 = 58.3 us/cm ~ 58 us/cm
 *
 * **Distance Calculation:**
 * ```
 * distance_cm = echo_time_us / k_hcsr04_us_per_cm_roundtrip
 * distance_cm = echo_time_us / 58
 * ```
 *
 * **Trigger Pulse Timing:**
 * ```
 * Time (us)  Signal
 * 0          TRIG -+
 * 2          TRIG  | (settle time)
 * 2          TRIG  |
 * 12         TRIG -+ (10us pulse)
 * 12+        Wait for echo...
 * ```
 *
 * **Echo Pulse Timing:**
 * - Minimum (2cm): 116 us = 2 cm * 58 us/cm
 * - Maximum (400cm): 23,200 us = 400 cm * 58 us/cm
 * - Timeout: 30,000 us (400cm + 30% margin)
 *
 * **Measurement Rate Limit:**
 * - HC-SR04 needs 60ms recovery time between measurements
 * - Faster polling causes echo interference/false readings
 * - Maximum rate: 1000ms / 60ms ~ 16 Hz
 *
 * @note Temperature affects speed of sound (~0.17%/degC) - use temp compensation
 * @see rx_hcsr04_set_temperature() For temperature compensation
 * @see rx_hcsr04_get_speed_of_sound() Speed of sound calculation
 */
typedef enum : uint16_t {
  k_hcsr04_trigger_settle_us   = 2,     /**< Pre-trigger settle time (ensure clean LOW) */
  k_hcsr04_trigger_pulse_us    = 10,    /**< Trigger pulse width (10us min per datasheet) */
  k_hcsr04_echo_timeout_us     = 30000, /**< Echo timeout (30ms = 400cm + margin) */
  k_hcsr04_min_echo_us         = 116,   /**< Minimum valid echo (2cm * 58 us/cm) */
  k_hcsr04_max_echo_us         = 23200, /**< Maximum valid echo (400cm * 58 us/cm) */
  k_hcsr04_us_per_cm_roundtrip = 58,    /**< Microseconds per cm roundtrip at 20degC */
  k_hcsr04_measurement_gap_ms  = 60,    /**< Minimum gap between measurements (per datasheet) */
} rx_hcsr04_timing_t;

/**
 * @enum rx_hcsr04_range_t
 * @brief HC-SR04 measurement range limits
 *
 * @details
 * Valid distance range for HC-SR04 sensor per datasheet specifications.
 * Measurements outside this range are rejected as k_rx_err_out_of_range.
 *
 * **Range Characteristics:**
 * - **Minimum (2cm):** Blind zone - echo overlaps trigger pulse
 * - **Maximum (400cm):** Signal too weak for reliable detection
 * - **Optimal:** 10cm - 200cm (best accuracy and reliability)
 *
 * **Factors Affecting Maximum Range:**
 * - Object size: Larger objects detectable at greater distance
 * - Object material: Hard surfaces reflect better than soft
 * - Surface angle: Perpendicular surfaces give strongest echo
 * - Ambient noise: Ultrasonic interference reduces range
 *
 * **Dead Zone (<2cm):**
 * - Sensor cannot distinguish echo from trigger pulse
 * - Use IR sensor for closer range
 *
 * @note Practical max range often 300cm due to real-world conditions
 * @see rx_hcsr04_measure() Returns k_rx_err_out_of_range if outside limits
 */
typedef enum : uint16_t {
  k_hcsr04_min_distance_cm = 2,   /**< Minimum measurable distance (blind zone limit) */
  k_hcsr04_max_distance_cm = 400, /**< Maximum measurable distance (detection limit) */
} rx_hcsr04_range_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum rx_hcsr04_echo_mode_t
 * @brief Echo pulse measurement mode selection
 *
 * @details
 * Determines how the HC-SR04 echo pulse is measured:
 * - **Polling**: Software reads GPIO pin state in a loop (simple, portable)
 * - **IRQ**: Hardware captures edges via ICU interrupt (precise, low CPU overhead)
 *
 * **Mode Comparison:**
 * | Feature | Polling | IRQ |
 * |---------|---------|-----|
 * | Precision | +/-5us | +/-1us |
 * | CPU Usage | High (busy-wait) | Low (interrupt-driven) |
 * | Complexity | Simple | Requires ICU config |
 * | Timing Jitter | Variable | Minimal |
 * | Concurrent Sensors | Limited | Supports all 4 |
 * | Max Range | ~400 cm (32-bit overflow tracking via hcsr04_hal_get_time_us) | ~150 cm (8.7 ms CMT2 wrap; hcsr04_hal_get_time_us_isr has no overflow tracking) |
 *
 * **Recommended Usage:**
 * - Use **polling** for: debugging, single sensor, simple applications
 * - Use **IRQ** for: production code, multiple sensors, CPU efficiency
 *
 * @invariant value is one of k_hcsr04_echo_polling or k_hcsr04_echo_irq
 *
 * @code
 * // Select IRQ mode and configure ICU
 * rx_hcsr04_config_t cfg = {
 *     .trigger_pin  = k_rx_pf_5,
 *     .echo_pin     = k_rx_p0_3,
 *     .timeout_us   = k_hcsr04_echo_timeout_us,
 *     .echo_mode    = k_hcsr04_echo_irq,          // Hardware edge capture
 *     .echo_irq     = k_hcsr04_irq_11,         // P03 -> IRQ11
 *     .irq_priority = k_hcsr04_irq_priority_default,
 * };
 * rx_hcsr04_t sensor;
 * rx_err_t err = rx_hcsr04_init(&sensor, &cfg);
 * // ICU is configured automatically inside rx_hcsr04_init() for IRQ mode
 * @endcode
 *
 * @see rx_hcsr04_config_t Configuration structure using this enum
 * @see rx_hcsr04_icu_configure() ICU setup required for IRQ mode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Software polling mode (default)
   * @details
   * Echo pin configured as GPIO input. Software polls pin state in a loop.
   * Simple and works everywhere, but consumes CPU during measurement.
   */
  k_hcsr04_echo_polling = 0,

  /**
   * @brief Hardware IRQ edge capture mode
   * @details
   * Echo pin configured for IRQ function via MPC. ICU captures rising and
   * falling edges automatically with interrupt-driven timing.
   * Requires ICU configuration via rx_hcsr04_icu_configure().
   */
  k_hcsr04_echo_irq = 1,
} rx_hcsr04_echo_mode_t;

/**
 * @enum rx_hcsr04_irq_t
 * @brief IRQ number selection for HC-SR04 echo pin (IRQ mode)
 *
 * @details
 * Type-safe enumeration of the valid IRQ numbers for HC-SR04 echo pulse
 * measurement. Use in the `echo_irq` field of `rx_hcsr04_config_t` when
 * `echo_mode == k_hcsr04_echo_irq`. The IRQ number must match the physical
 * echo pin: P00 -> IRQ8, P01 -> IRQ9, P02 -> IRQ10, P03 -> IRQ11.
 *
 * @invariant Only values k_hcsr04_irq_8 through k_hcsr04_irq_11 are
 *            currently used in the STAR project (one per sonar sensor)
 *
 * @code
 * rx_hcsr04_config_t config = {
 *     .echo_pin  = k_rx_p0_3,
 *     .echo_mode = k_hcsr04_echo_irq,
 *     .echo_irq  = k_hcsr04_irq_11,  // P03 -> IRQ11
 * };
 * @endcode
 *
 * @see rx_hcsr04_config_t Configuration structure using this type
 * @see RX72N Manual Chapter 15 - ICU, Table listing IRQ pin assignments
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_hcsr04_irq_none = 0, /**< Sentinel: no IRQ assigned (polling mode); zero so zero-init is safe */
  k_hcsr04_irq_8    = 8, /**< IRQ8  - maps to P00 (Sonar 3 Back-Right) */
  k_hcsr04_irq_9    = 9, /**< IRQ9  - maps to P01 (Sonar 2 Back-Left) */
  k_hcsr04_irq_10   = 10, /**< IRQ10 - maps to P02 (Sonar 1 Front-Right) */
  k_hcsr04_irq_11   = 11, /**< IRQ11 - maps to P03 (Sonar 0 Front-Left) */
  k_hcsr04_irq_12   = 12, /**< IRQ12 - reserved; not accepted by rx_hcsr04_init() */
  k_hcsr04_irq_13   = 13, /**< IRQ13 - reserved; not accepted by rx_hcsr04_init() */
  k_hcsr04_irq_14   = 14, /**< IRQ14 - reserved; not accepted by rx_hcsr04_init() */
  k_hcsr04_irq_15   = 15, /**< IRQ15 - reserved; not accepted by rx_hcsr04_init() */
} rx_hcsr04_irq_t;

/**
 * @enum rx_hcsr04_irq_priority_t
 * @brief Named constants for IRQ interrupt priority configuration
 *
 * @details
 * Provides named values for the `irq_priority` field in `rx_hcsr04_config_t`.
 * Use `k_hcsr04_irq_priority_unset` (0) to let `rx_hcsr04_init()` select
 * the default priority, or provide an explicit value 1-15.
 *
 * @invariant k_hcsr04_irq_priority_unset (0) means "not set" (rx_hcsr04_init() selects the default);
 *            valid explicit priorities are 1-15; any other value is rejected by rx_hcsr04_icu_configure()
 *
 * @code
 * // Use sentinel to let rx_hcsr04_init() pick the default priority
 * rx_hcsr04_config_t cfg = {
 *     .trigger_pin  = k_rx_pf_5,
 *     .echo_pin     = k_rx_p0_3,
 *     .timeout_us   = k_hcsr04_echo_timeout_us,
 *     .echo_mode    = k_hcsr04_echo_irq,
 *     .echo_irq     = k_hcsr04_irq_11,
 *     .irq_priority = k_hcsr04_irq_priority_unset,  // rx_hcsr04_init() applies k_hcsr04_irq_priority_default
 * };
 * rx_hcsr04_t sensor;
 * rx_err_t err = rx_hcsr04_init(&sensor, &cfg);
 * @endcode
 *
 * @see rx_hcsr04_config_t.irq_priority Configure sensor priority
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_hcsr04_irq_priority_unset = 0, /**< Sentinel: use default priority in rx_hcsr04_init() */
  k_hcsr04_irq_priority_min   = 1, /**< Minimum valid ICU interrupt priority */
  k_hcsr04_irq_priority_default =
    10, /**< Default ICU interrupt priority (balances latency and preemption) */
  k_hcsr04_irq_priority_max = 15, /**< Maximum valid ICU interrupt priority */
} rx_hcsr04_irq_priority_t;

/**
 * @enum rx_hcsr04_sensor_index_t
 * @brief Named sensor slot indices for ISR registration and dispatch
 *
 * @details
 * Maps each physical sensor position to a zero-based array index. Use in
 * the `sensor_index` field of `rx_hcsr04_config_t` when
 * `echo_mode == k_hcsr04_echo_irq`. The ISR dispatch table uses this index
 * to route echo timestamps to the correct sensor handle.
 *
 * **STAR Hardware Mapping:**
 * | Index | Position    | IRQ  | Pin |
 * |-------|-------------|------|-----|
 * | 0     | Front-Left  | IRQ11| P03 |
 * | 1     | Front-Right | IRQ10| P02 |
 * | 2     | Back-Left   | IRQ9 | P01 |
 * | 3     | Back-Right  | IRQ8 | P00 |
 *
 * `k_hcsr04_sensor_count` is the exclusive upper bound used for range
 * validation inside `rx_hcsr04_init()`: a `sensor_index` value must satisfy
 * `(uint8_t)sensor_index < (uint8_t)k_hcsr04_sensor_count`.
 *
 * @invariant Values are zero-based consecutive integers [0, 3];
 *            k_hcsr04_sensor_count (4) is the exclusive upper bound
 *
 * @code
 * // Configure front-left sensor in IRQ mode
 * rx_hcsr04_config_t cfg = {
 *     .trigger_pin  = k_rx_pf_5,
 *     .echo_pin     = k_rx_p0_3,              // P03 -> IRQ11
 *     .timeout_us   = k_hcsr04_echo_timeout_us,
 *     .echo_mode    = k_hcsr04_echo_irq,
 *     .echo_irq     = k_hcsr04_irq_11,
 *     .irq_priority = k_hcsr04_irq_priority_default,
 *     .sensor_index = k_hcsr04_sensor_front_left,  // Slot 0
 * };
 * @endcode
 *
 * @see rx_hcsr04_config_t.sensor_index Field using this enum
 * @see rx_hcsr04_isr_register() Receives this value during init
 *
 * @note k_hcsr04_sensor_count is used as the exclusive upper-bound for range
 *       validation; it does not represent a valid sensor slot
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_hcsr04_sensor_front_left  = 0, /**< Sonar 0 Front-Left  (IRQ11, P03) */
  k_hcsr04_sensor_front_right = 1, /**< Sonar 1 Front-Right (IRQ10, P02) */
  k_hcsr04_sensor_back_left   = 2, /**< Sonar 2 Back-Left   (IRQ9,  P01) */
  k_hcsr04_sensor_back_right  = 3, /**< Sonar 3 Back-Right  (IRQ8,  P00) */
  k_hcsr04_sensor_count =
    4, /**< Total number of sensors; used as exclusive upper-bound for range validation */
} rx_hcsr04_sensor_index_t;

/**
 * @struct rx_hcsr04_config_t
 * @brief HC-SR04 sensor initialization configuration
 *
 * @details
 * Configuration structure for initializing HC-SR04 sensor handle.
 * Specifies GPIO pin assignments, measurement timeout, echo measurement mode,
 * and (for IRQ mode) the sensor slot index.
 *
 * **Memory Layout (13 bytes on RX72N, padded to 16):**
 * ```
 * Offset | Field        | Type                     | Size | Alignment
 * -------|--------------|--------------------------|------|----------
 * 0      | trigger_pin  | rx_port_pin_t            | 2    | 2
 * 2      | echo_pin     | rx_port_pin_t            | 2    | 2
 * 4      | timeout_us   | uint32_t                 | 4    | 4
 * 8      | echo_mode    | rx_hcsr04_echo_mode_t    | 1    | 1
 * 9      | echo_irq     | rx_hcsr04_irq_t          | 1    | 1
 * 10     | irq_priority | rx_hcsr04_irq_priority_t | 1    | 1
 * 11     | sensor_index | rx_hcsr04_sensor_index_t | 1    | 1
 * Total: 12 bytes (no trailing pad required)
 * ```
 *
 * **Field Descriptions:**
 * - `trigger_pin`: GPIO output pin for 10us trigger pulse
 * - `echo_pin`: GPIO input pin (or IRQ pin) for measuring echo pulse width
 * - `timeout_us`: Maximum wait time for echo (default: k_hcsr04_echo_timeout_us = 30ms)
 * - `echo_mode`: Measurement mode (polling or IRQ, default: polling)
 * - `echo_irq`: IRQ number (k_hcsr04_irq_8..k_hcsr04_irq_11) if echo_mode == IRQ, otherwise ignored
 * - `sensor_index`: Sensor slot index (0-3) for ISR dispatch table; IRQ mode only
 *
 * **GPIO Pin Selection:**
 * - Use type-safe `rx_port_pin_t` enum from rx_port_constants.h
 * - Trigger and echo must be different pins
 * - Both pins must support digital I/O
 * - For IRQ mode: echo_pin must support IRQ function (P00-P03 for IRQ8-11)
 *
 * **Timeout Recommendations:**
 * - Default (30ms): Covers full 400cm range + margin
 * - Short range (10ms): Objects < 170cm only, faster response
 * - Long range (50ms): Extra margin for difficult environments
 *
 * @par Example: Polling Mode (Default)
 * @code
 * rx_hcsr04_config_t config = {
 *     .trigger_pin = k_rx_pf_5,                // Trigger on PF5
 *     .echo_pin    = k_rx_p0_3,                // Echo on P03
 *     .timeout_us  = k_hcsr04_echo_timeout_us, // 30ms timeout
 *     .echo_mode   = k_hcsr04_echo_polling,    // Software polling (default)
 *     // echo_irq and sensor_index not used in polling mode
 * };
 *
 * rx_hcsr04_t sensor;
 * rx_err_t err = rx_hcsr04_init(&sensor, &config);
 * @endcode
 *
 * @par Example: IRQ Mode (High Precision)
 * @code
 * rx_hcsr04_config_t config = {
 *     .trigger_pin  = k_rx_pf_5,                       // Trigger on PF5
 *     .echo_pin     = k_rx_p0_3,                       // Echo on P03 (IRQ11)
 *     .timeout_us   = k_hcsr04_echo_timeout_us,        // 30ms timeout
 *     .echo_mode    = k_hcsr04_echo_irq,               // Hardware edge capture
 *     .echo_irq     = k_hcsr04_irq_11,                 // IRQ11 for P03
 *     .irq_priority = k_hcsr04_irq_priority_default,   // Default ICU priority
 *     .sensor_index = k_hcsr04_sensor_front_left,      // Slot 0 (P03/IRQ11)
 * };
 *
 * rx_hcsr04_t sensor;
 * rx_err_t err = rx_hcsr04_init(&sensor, &config);
 * // Note: ICU configuration and ISR registration done automatically in init
 * @endcode
 *
 * @invariant trigger_pin != echo_pin (trigger and echo must be different physical pins)
 * @invariant trigger_pin and echo_pin must be valid digital I/O pins (rx_port_pin_t)
 * @invariant If echo_mode == k_hcsr04_echo_irq: echo_pin must support ICU/IRQ and
 *            echo_irq must match the echo_pin mapping (P00->k_hcsr04_irq_8,
 *            P01->k_hcsr04_irq_9, P02->k_hcsr04_irq_10, P03->k_hcsr04_irq_11)
 * @invariant If echo_mode == k_hcsr04_echo_irq: sensor_index < k_hcsr04_sensor_count
 * @invariant timeout_us > 0 and within supported range (use k_hcsr04_echo_timeout_us)
 * @invariant echo_mode defaults to k_hcsr04_echo_polling; irq_priority defaults to
 *            k_hcsr04_irq_priority_default when set to k_hcsr04_irq_priority_unset
 *
 * @note This structure is passed by pointer to rx_hcsr04_init()
 * @note All invariants are validated by rx_hcsr04_init()
 * @warning Do not modify fields after initialization - call deinit/init instead
 * @warning For IRQ mode: ensure echo_irq matches echo_pin (P00=IRQ8, P01=IRQ9, etc.)
 *
 * @see rx_hcsr04_init() Validates all invariants during initialization
 * @see rx_hcsr04_echo_mode_t Echo measurement mode enum
 * @see rx_hcsr04_sensor_index_t Sensor slot index enum
 * @see rx_port_constants.h Type-safe GPIO pin definitions
 * @see rx_hcsr04_timing_t Timeout constants
 *
 * @since Version 1.0.0
 */
typedef struct {
  rx_port_pin_t trigger_pin; /**< Trigger pin (type-safe GPIO, output) */
  rx_port_pin_t echo_pin;    /**< Echo pin (type-safe GPIO or IRQ, input) */
  uint32_t      timeout_us; /**< Echo timeout in microseconds (default: k_hcsr04_echo_timeout_us) */
  rx_hcsr04_echo_mode_t echo_mode; /**< Polling or IRQ mode (default: polling) */
  rx_hcsr04_irq_t
    echo_irq; /**< IRQ number (k_hcsr04_irq_8..k_hcsr04_irq_11), used only if echo_mode==IRQ */
  rx_hcsr04_irq_priority_t
    irq_priority; /**< ICU interrupt priority (k_hcsr04_irq_priority_unset = use default), IRQ mode only */
  rx_hcsr04_sensor_index_t
    sensor_index; /**< Sensor slot index (IRQ mode only; must be < k_hcsr04_sensor_count) */
} rx_hcsr04_config_t;

/**
 * @struct rx_hcsr04_t
 * @brief HC-SR04 sensor handle
 *
 * @details
 * Runtime state for one HC-SR04 sensor instance. The caller allocates this
 * structure (statically or on the stack) and passes it to rx_hcsr04_init().
 * The driver populates all fields during initialization and manages them
 * thereafter. Do not modify fields directly after initialization.
 *
 * **Lifecycle:**
 * 1. Declare uninitialized (zero-initialize with `= {}` is safe)
 * 2. Call rx_hcsr04_init() to configure GPIO/ICU and set all fields
 * 3. Call rx_hcsr04_measure_blocking() or rx_hcsr04_measure() as needed
 * 4. Call rx_hcsr04_deinit() to release GPIO reservations
 *
 * @invariant initialized == false implies all other fields are undefined
 * @invariant echo_irq == k_hcsr04_irq_none when echo_mode == k_hcsr04_echo_polling
 * @invariant measurement_count >= timeout_count + range_error_count
 *
 * @code
 * rx_hcsr04_t sensor = {};  // Zero-initialize before calling init
 * rx_hcsr04_config_t cfg = {
 *     .trigger_pin  = k_rx_pf_5,
 *     .echo_pin     = k_rx_p0_3,
 *     .timeout_us   = k_hcsr04_echo_timeout_us,     // 30ms default
 *     .echo_mode    = k_hcsr04_echo_polling,
 *     .echo_irq     = k_hcsr04_irq_none,
 *     .irq_priority = k_hcsr04_irq_priority_unset, // 0 = not applicable in polling mode
 * };
 * rx_err_t err = rx_hcsr04_init(&sensor, &cfg);
 * if (err != k_rx_ok) {
 *     // Handle initialization failure
 *     return err;
 * }
 * @endcode
 *
 * @see rx_hcsr04_init() Populates all fields
 * @see rx_hcsr04_deinit() Releases resources and clears initialized flag
 * @see rx_hcsr04_config_t Configuration input to rx_hcsr04_init()
 *
 * @since Version 1.0.0
 */
typedef struct {
  /* Configuration (set during init) */
  rx_port_pin_t         trigger_pin; /**< Trigger pin (type-safe GPIO enum) */
  rx_port_pin_t         echo_pin;    /**< Echo pin (type-safe GPIO or IRQ enum) */
  uint32_t              timeout_us;  /**< Measurement timeout in microseconds */
  rx_hcsr04_echo_mode_t echo_mode;   /**< Echo measurement mode (polling or IRQ) */
  rx_hcsr04_irq_t
    echo_irq; /**< IRQ number (k_hcsr04_irq_8..11) if IRQ mode, else k_hcsr04_irq_none */
  rx_hcsr04_irq_priority_t
    irq_priority; /**< ICU interrupt priority (1-15); k_hcsr04_irq_priority_unset = polling */

  /* State */
  bool  initialized;               /**< True if handle is initialized */
  bool  measurement_active;        /**< True if async measurement in progress */
  bool  cancel_requested;          /**< True if async measurement cancellation requested */
  float temperature_celsius;       /**< Ambient temperature for compensation (20.0 if not set) */
  bool  temp_compensation_enabled; /**< True if temperature compensation is enabled */

  /* Statistics */
  uint32_t measurement_count; /**< Total measurements attempted */
  uint32_t timeout_count;     /**< Measurements that timed out */
  uint32_t range_error_count; /**< Out-of-range readings (too close/far) */
} rx_hcsr04_t;

/**
 * @brief Measurement result structure
 *
 * Contains distance in multiple units and raw timing data.
 */
typedef struct {
  float    distance_cm;  /**< Distance in centimeters */
  float    distance_in;  /**< Distance in inches */
  uint32_t echo_time_us; /**< Raw echo pulse duration in microseconds */
  rx_err_t status;       /**< Measurement status (k_rx_ok or error code) */
} rx_hcsr04_result_t;

/**
 * @brief Async measurement callback function type
 *
 * Called when async measurement completes or times out.
 *
 * @param[in] handle    Sensor handle
 * @param[in] result    Measurement result
 * @param[in] user_data User context passed to rx_hcsr04_measure_async()
 */
typedef void (*rx_hcsr04_callback_t)(rx_hcsr04_t*              handle,
                                     const rx_hcsr04_result_t* result,
                                     void*                     user_data);

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

/**
 * @brief Initialize HC-SR04 sensor
 *
 * @details
 * Configures GPIO pins for trigger (output) and echo (input). In IRQ mode,
 * also configures the MPC pin function and ICU for edge detection. Validates
 * that the echo_pin matches the echo_irq mapping (P00->IRQ8, P01->IRQ9,
 * P02->IRQ10, P03->IRQ11) before calling rx_mpc_set_irq().
 *
 * @param[out] handle Handle to initialize (caller-allocated)
 * @param[in]  config Configuration with pin assignments and mode
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr handle or config is nullptr
 * @retval k_rx_err_invalid_arg port/pin values are invalid, IRQ number out of
 *                              range, or echo_pin/echo_irq mismatch
 * @retval k_rx_err_gpio_conflict pins already reserved
 * @retval k_rx_err_invalid_state handle already initialized
 *
 * @pre handle must not be NULL
 * @pre config must not be NULL with valid port/pin values
 * @pre If echo_mode == k_hcsr04_echo_irq: echo_pin and echo_irq must match hardware mapping
 *          (P00<->IRQ8, P01<->IRQ9, P02<->IRQ10, P03<->IRQ11)
 * @pre If echo_mode == k_hcsr04_echo_irq: echo_irq must be in range [k_hcsr04_irq_8..k_hcsr04_irq_11]
 *          (only IRQ8-11 have ISR handlers and ICU support in this firmware)
 *
 * @post handle->initialized == true on success
 * @post Trigger pin configured as GPIO output (low)
 * @post Echo pin configured as GPIO input (polling) or IRQ input (IRQ mode)
 * @post handle->irq_priority reflects the effective priority used (IRQ mode only)
 *
 * @note Not thread-safe; caller must synchronize if called from multiple threads
 * @note For IRQ mode, irq_priority of k_hcsr04_irq_priority_unset (0) selects default (k_hcsr04_irq_priority_default)
 * @warning echo_pin and echo_irq MUST match the hardware mapping
 *          (P00<->IRQ8, P01<->IRQ9, P02<->IRQ10, P03<->IRQ11)
 *
 * @par Example: Initialize Polling Mode Sensor
 * @code
 * rx_hcsr04_t sensor = {};
 * rx_hcsr04_config_t cfg = {
 *     .trigger_pin = k_rx_pf_5,
 *     .echo_pin    = k_rx_p0_3,
 *     .timeout_us  = k_hcsr04_echo_timeout_us,
 *     .echo_mode   = k_hcsr04_echo_polling,
 * };
 * rx_err_t err = rx_hcsr04_init(&sensor, &cfg);
 * if (err != k_rx_ok) {
 *     rx_log_error("SONAR", "Init failed");
 *     return err;
 * }
 * @endcode
 *
 * @par Example: Initialize IRQ Mode Sensor
 * @code
 * rx_hcsr04_t sensor = {};
 * rx_hcsr04_config_t cfg = {
 *     .trigger_pin  = k_rx_pf_5,
 *     .echo_pin     = k_rx_p0_3,
 *     .timeout_us   = k_hcsr04_echo_timeout_us,
 *     .echo_mode    = k_hcsr04_echo_irq,
 *     .echo_irq     = k_hcsr04_irq_11,
 *     .irq_priority = k_hcsr04_irq_priority_default,
 * };
 * rx_err_t err = rx_hcsr04_init(&sensor, &cfg);
 * if (err != k_rx_ok) {
 *     rx_log_error("SONAR", "IRQ init failed");
 *     return err;
 * }
 * @endcode
 *
 * @see rx_hcsr04_irq_t Type-safe IRQ number enum
 * @see rx_hcsr04_config_t.irq_priority Configurable interrupt priority
 * @see rx_hcsr04_irq_priority_t Named priority constants
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_init(rx_hcsr04_t* handle, const rx_hcsr04_config_t* config);

/**
 * @brief Deinitialize HC-SR04 sensor
 *
 * Releases GPIO pin reservations and resets handle state.
 *
 * @param[in,out] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_deinit(rx_hcsr04_t* handle);

/**
 * @brief Initialize HC-SR04 async worker thread
 *
 * Creates a ThreadX worker thread for non-blocking async measurements.
 * Must be called before using rx_hcsr04_measure_async() for true async operation.
 *
 * The worker thread:
 * - Runs at priority 10
 * - Uses 1KB stack
 * - Processes measurement requests from event flags
 * - Invokes callbacks in worker thread context
 *
 * @note This is optional. If not called, rx_hcsr04_measure_async() will
 *       fall back to synchronous operation (callback invoked before return).
 *
 * @return k_rx_ok on success
 * @return k_rx_err_rtos_error if thread creation fails
 * @return k_rx_err_invalid_state if worker already initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_worker_init(void);

/**
 * @brief Deinitialize HC-SR04 async worker thread
 *
 * Terminates the worker thread and releases resources.
 * Any pending async measurements will be cancelled.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if worker not initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_worker_deinit(void);

/* =============================================================================
 * Public API - Measurement
 * =============================================================================
 */

/**
 * @brief Measure distance (blocking)
 *
 * Performs a complete measurement cycle:
 * 1. Send 10us trigger pulse
 * 2. Wait for echo pulse start
 * 3. Measure echo pulse duration
 * 4. Convert to distance
 *
 * Blocks for up to timeout_us (default 30ms).
 *
 * @param[in]  handle      Sensor handle
 * @param[out] distance_cm Measured distance in centimeters
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or distance_cm is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_timeout if no echo received (object >400cm or absent)
 * @return k_rx_err_out_of_range if distance <2cm (too close)
 */
[[nodiscard]] rx_err_t rx_hcsr04_measure_blocking(rx_hcsr04_t* handle, float* distance_cm);

/**
 * @brief Measure distance with full result (blocking)
 *
 * Same as rx_hcsr04_measure_blocking() but returns complete result
 * including both distance units and raw timing.
 *
 * @param[in]  handle Sensor handle
 * @param[out] result Complete measurement result
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or result is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_timeout if no echo received
 * @return k_rx_err_out_of_range if distance out of bounds
 */
[[nodiscard]] rx_err_t rx_hcsr04_measure(rx_hcsr04_t* handle, rx_hcsr04_result_t* result);

/**
 * @brief Start asynchronous measurement
 *
 * Initiates measurement and invokes callback with result.
 *
 * **Operating Modes:**
 * - **Worker initialized** (rx_hcsr04_worker_init() called):
 *   Returns immediately, callback invoked from worker thread when complete.
 *   True non-blocking operation.
 *
 * - **Worker not initialized**:
 *   Performs synchronous measurement, callback invoked before return.
 *   Caller blocks for up to timeout_us (default 30ms).
 *
 * @note Use rx_hcsr04_cancel() to abort in-progress async measurements.
 *       Cancellation only works when worker thread is active.
 *
 * @param[in] handle    Sensor handle
 * @param[in] callback  Completion callback (required)
 * @param[in] user_data User context passed to callback (can be NULL)
 *
 * @return k_rx_ok if measurement queued (async) or completed (sync)
 * @return k_rx_err_null_ptr if handle or callback is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_busy if measurement already in progress
 */
rx_err_t
rx_hcsr04_measure_async(rx_hcsr04_t* handle, rx_hcsr04_callback_t callback, void* user_data);

/**
 * @brief Check if async measurement is in progress
 *
 * @param[in] handle Sensor handle
 *
 * @return true if measurement in progress
 * @return false if idle or handle is nullptr
 */
[[nodiscard]] bool rx_hcsr04_is_busy(const rx_hcsr04_t* handle);

/**
 * @brief Cancel async measurement
 *
 * Cancels in-progress async measurement. Callback will NOT be invoked.
 *
 * @param[in] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if no measurement in progress
 */
[[nodiscard]] rx_err_t rx_hcsr04_cancel(rx_hcsr04_t* handle);

/* =============================================================================
 * Public API - Temperature Compensation
 * =============================================================================
 */

/**
 * @brief Set ambient temperature for automatic distance compensation
 *
 * Enables temperature compensation and updates the temperature used for
 * all subsequent distance measurements. The speed of sound varies with
 * temperature (~0.17% per degC), affecting measurement accuracy.
 *
 * When enabled, all measurement functions (rx_hcsr04_measure_blocking,
 * rx_hcsr04_measure, rx_hcsr04_measure_async) will automatically use
 * temperature-compensated distance calculations.
 *
 * Temperature can be updated at any time, including between measurements.
 * Typical usage: read from DS18B20 sensor periodically and update.
 *
 * @param[in,out] handle         Sensor handle
 * @param[in]     temp_celsius   Ambient temperature in degrees Celsius
 *                                Valid range: -40degC to +85degC
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_invalid_arg if temperature out of valid range
 *
 * @note To disable temperature compensation, call rx_hcsr04_disable_temp_compensation()
 *
 * @see rx_hcsr04_disable_temp_compensation()
 * @see rx_hcsr04_echo_to_cm_with_temp()
 */
[[nodiscard]] rx_err_t rx_hcsr04_set_temperature(rx_hcsr04_t* handle, float temp_celsius);

/**
 * @brief Disable automatic temperature compensation
 *
 * Disables temperature compensation and reverts to assuming 20degC for all
 * distance calculations. This is the default behavior after initialization.
 *
 * @param[in,out] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_disable_temp_compensation(rx_hcsr04_t* handle);

/**
 * @brief Check if temperature compensation is enabled
 *
 * @param[in] handle Sensor handle
 *
 * @return true if temperature compensation is enabled
 * @return false if disabled or handle is nullptr
 */
[[nodiscard]] bool rx_hcsr04_is_temp_compensation_enabled(const rx_hcsr04_t* handle);

/**
 * @brief Get current temperature setting
 *
 * Returns the currently configured temperature value used for compensation.
 * If compensation is disabled, still returns the last set value (or 20.0degC default).
 *
 * @param[in]  handle       Sensor handle
 * @param[out] temp_celsius Current temperature setting
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or temp_celsius is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_get_temperature(const rx_hcsr04_t* handle, float* temp_celsius);

/* =============================================================================
 * Public API - Utilities
 * =============================================================================
 */

/**
 * @brief Convert distance from centimeters to inches
 *
 * @param[in] distance_cm Distance in centimeters
 *
 * @return Distance in inches
 */
float rx_hcsr04_cm_to_inches(float distance_cm);

/**
 * @brief Convert echo time to distance in centimeters
 *
 * Uses formula: distance = (echo_us * speed_of_sound) / 2
 * Speed of sound at 20C = 343 m/s = 0.0343 cm/us
 *
 * @note This function assumes 20degC. For temperature compensation, use
 *       rx_hcsr04_echo_to_cm_with_temp() instead.
 *
 * @param[in] echo_time_us Echo pulse duration in microseconds
 *
 * @return Distance in centimeters
 */
float rx_hcsr04_echo_to_cm(uint32_t echo_time_us);

/**
 * @brief Convert echo time to distance with temperature compensation
 *
 * Calculates distance using temperature-compensated speed of sound.
 * Speed of sound varies with temperature: v = 331.3 + (0.606 * temp_c) m/s
 *
 * Temperature compensation improves accuracy by ~0.17% per degC deviation from 20degC.
 *
 * Example: At 10degC, speed is ~337 m/s vs 343 m/s at 20degC (~1.75% difference)
 *
 * @param[in] echo_time_us  Echo pulse duration in microseconds
 * @param[in] temp_celsius  Ambient temperature in degrees Celsius
 *
 * @return Distance in centimeters (temperature-compensated)
 */
float rx_hcsr04_echo_to_cm_with_temp(uint32_t echo_time_us, float temp_celsius);

/**
 * @brief Calculate speed of sound at given temperature
 *
 * Uses formula: v = 331.3 + (0.606 * temp_c) m/s
 *
 * Valid temperature range: -40degC to +85degC (DS18B20 operating range)
 *
 * @param[in] temp_celsius Ambient temperature in degrees Celsius
 *
 * @return Speed of sound in m/s
 */
float rx_hcsr04_get_speed_of_sound(float temp_celsius);

/**
 * @brief Get sensor statistics
 *
 * @param[in]  handle           Sensor handle
 * @param[out] measurement_count Total measurements (can be NULL)
 * @param[out] timeout_count     Timeout count (can be NULL)
 * @param[out] range_error_count Range error count (can be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_get_stats(const rx_hcsr04_t* handle,
                                           uint32_t*          measurement_count,
                                           uint32_t*          timeout_count,
                                           uint32_t*          range_error_count);

/**
 * @brief Reset sensor statistics
 *
 * @param[in,out] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_hcsr04_reset_stats(rx_hcsr04_t* handle);

#ifdef UNIT_TEST
#include "tx_api.h" /* Required for TX_SEMAPHORE, ULONG used in internal declarations */

/**
 * @brief Worker event flag bits (internal, exposed for unit testing).
 */
typedef enum : unsigned long {
  k_event_measurement_request = 1U, /**< Bit 0: Measurement request */
  k_event_shutdown_request    = 2U, /**< Bit 1: Shutdown request */
} hcsr04_event_flags_t;

/**
 * @brief Async measurement pending context (internal struct exposed for unit testing).
 */
typedef struct {
  rx_hcsr04_t*         handle;    /**< Sensor handle (NULL = idle) */
  rx_hcsr04_callback_t callback;  /**< Completion callback function */
  void*                user_data; /**< Caller-provided context pointer */
} hcsr04_pending_measurement_t;

/* Internal variables exposed for direct manipulation in tests */
extern hcsr04_pending_measurement_t s_pending;             /**< Pending measurement context */
extern bool                         s_worker_initialized;  /**< Worker init flag */
extern TX_SEMAPHORE                 s_worker_shutdown_sem; /**< Shutdown semaphore */

/* Internal functions exposed for unit testing via RX_STATIC_TESTABLE */
rx_err_t internal_send_trigger_pulse(const rx_hcsr04_t* handle);
rx_err_t internal_wait_for_echo(rx_hcsr04_t* handle, bool target_state, uint32_t timeout_us);
rx_err_t internal_measure_echo_pulse(rx_hcsr04_t* handle, uint32_t* duration_us);
rx_err_t internal_measure_echo_pulse_irq(rx_hcsr04_t* handle, uint32_t* duration_us);
rx_err_t internal_init_irq_mode(const rx_hcsr04_config_t* config, uint8_t* out_priority);
rx_err_t internal_trigger_and_measure(rx_hcsr04_t* handle, uint32_t* out_echo_us);
bool     internal_handle_worker_event(unsigned long actual_flags);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
