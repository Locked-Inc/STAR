/**
 * @file rx_time_constants.h
 * @brief Time Unit Conversion Constants for RX72N Firmware
 *
 * @details
 * This header provides typed enumeration constants for time unit conversions
 * used throughout the RX72N firmware. All time conversions follow SI (International
 * System of Units) standards with milliseconds, microseconds, and nanoseconds.
 *
 * ## System Architecture Context
 *
 * Time conversions are fundamental to real-time embedded systems. The STAR
 * firmware uses these constants for:
 * - **Timeout calculations**: I2C/SPI/USB communication timeouts
 * - **ThreadX scheduling**: Converting milliseconds to RTOS ticks
 * - **Performance measurements**: Microsecond-precision timing
 * - **Delay functions**: Hardware timer configuration
 * - **Watchdog configuration**: IWDT timeout settings
 *
 * ## Design Rationale
 *
 * **C23 Typed Enums for Time Constants:**
 * - Type safety prevents mixing time units (compile error if mismatched)
 * - Compile-time resolution (zero runtime overhead)
 * - Debugger shows symbolic names ("k_rx_ms_per_second" vs "1000")
 * - Self-documenting code
 *
 * **uint32_t Underlying Type:**
 * - Accommodates large timeout values (up to 4,294,967 seconds ~= 49 days)
 * - Prevents overflow in microsecond/nanosecond conversions
 * - Matches ThreadX tick counter width (32-bit)
 *
 * **Zero Magic Numbers Policy:**
 * All time conversions use named constants from this header. Raw literals
 * like `1000` or `1000000` are forbidden in production code.
 *
 * ## Implementation Approach
 *
 * Constants are derived from SI unit definitions:
 * - 1 second = 1000 milliseconds (ms)
 * - 1 millisecond = 1000 microseconds (us)
 * - 1 microsecond = 1000 nanoseconds (ns)
 *
 * ThreadX configuration:
 * - System tick frequency: 100 Hz (hardware timer CMT0)
 * - Tick period: 10 milliseconds
 *
 * ## Performance Characteristics
 *
 * - All constants resolved at compile time (zero runtime cost)
 * - Multiplication/division optimized by compiler to bit shifts where possible
 * - No function call overhead
 *
 * ## Memory Usage
 *
 * - Constants occupy zero RAM (compile-time only)
 * - Enum type definitions occupy zero storage
 * - Total memory footprint: 0 bytes
 *
 * @par Module Dependencies:
 *
 * **Depends On:**
 * - None (foundational header)
 *
 * **Used By:**
 * - [rx_iwdt.h](rx_iwdt.h) - Watchdog timeout configuration
 * - [rx_usb.h](../rx_usb/inc/rx_usb.h) - USB timeout calculations
 * - [rx_comm_manager.h](../rx_comm_manager/inc/rx_comm_manager.h) - Communication timeouts
 * - ThreadX applications - Converting ms to ticks
 * - All HAL drivers - Hardware timer configuration
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple control flow | [OK] | No code (constants only) |
 * | 2. Fixed loop bounds | [OK] | No loops |
 * | 3. No dynamic memory | [OK] | No allocations |
 * | 4. Function length <60 lines | [OK] | No functions |
 * | 5. Assertions | [OK] | N/A (header only) |
 * | 6. Smallest scope | [OK] | File-scope enums |
 * | 7. Check return values | [OK] | N/A |
 * | 8. Limited preprocessor | [OK] | Include guards only, C23 enums used |
 * | 9. Pointer restrictions | [OK] | No pointers |
 * | 10. Compiler warnings | [OK] | `-Wall -Wextra -Werror` |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S):**
 * - This header has one responsibility: define time conversion constants
 * - All constants are time-related, cohesive grouping
 *
 * **Open/Closed (O):**
 * - New time units can be added without modifying existing constants
 * - Derived conversions (e.g., hours, days) can be added as new enum types
 *
 * **Interface Segregation (I):**
 * - Single focused enum type for SI time unit conversions
 * - No "fat" interface with unrelated constants
 *
 * @see rx_iwdt.h Independent watchdog timer with timeout configuration
 * @see ThreadX API tx_thread_sleep() uses these conversions
 * @see docs/sections/06_nasa_power_of_10.tex NASA Rule 8 compliance details
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Time Unit Conversion Constants
 * =============================================================================
 */

/**
 * @enum rx_time_conversion_t
 * @brief Time unit conversion factors following SI standards
 *
 * @details
 * Standard International System of Units (SI) time unit conversions used for
 * timeout calculations, RTOS tick conversions, and hardware timer configuration
 * throughout the RX72N firmware.
 *
 * ## Time Unit Hierarchy (SI Standard)
 *
 * ```
 * 1 second (s)
 *   +- 1,000 milliseconds (ms)
 *        +- 1,000 microseconds (us)
 *             +- 1,000 nanoseconds (ns)
 * ```
 *
 * ## ThreadX RTOS Tick Configuration
 *
 * The STAR firmware configures ThreadX with a 100 Hz tick rate:
 * - **Tick frequency**: 100 Hz (configurable in tx_initialize_low_level.c)
 * - **Tick period**: 10 milliseconds
 * - **Hardware timer**: CMT0 (Compare Match Timer)
 * - **CMT0 input clock**: PCLKB/32 @ 60 MHz = 1.875 MHz
 * - **CMT0 compare value**: 18,750 counts = 10ms period
 *
 * ## Common Conversion Patterns
 *
 * **Seconds -> Milliseconds:**
 * ```c
 * uint32_t timeout_ms = timeout_sec * k_rx_ms_per_second;
 * ```
 *
 * **Milliseconds -> ThreadX Ticks:**
 * ```c
 * uint32_t ticks = timeout_ms / k_threadx_ms_per_tick;
 * tx_thread_sleep(ticks);
 * ```
 *
 * **Microseconds -> Nanoseconds:**
 * ```c
 * uint32_t delay_ns = delay_us * k_rx_ns_per_us;
 * ```
 *
 * **Milliseconds -> Microseconds:**
 * ```c
 * uint32_t delay_us = delay_ms * k_rx_us_per_ms;
 * ```
 *
 * ## Overflow Prevention
 *
 * All constants are uint32_t (32-bit unsigned) to prevent overflow:
 * - **Max seconds**: 4,294,967 (~49.7 days)
 * - **Max milliseconds**: 4,294,967,295 (~49.7 days)
 * - **Max microseconds**: 4,294 seconds (~71.6 minutes)
 * - **Max nanoseconds**: 4.29 seconds
 *
 * When working with large values, use 64-bit arithmetic:
 * ```c
 * uint64_t large_ns = (uint64_t)seconds * k_rx_us_per_second * k_rx_ns_per_us;
 * ```
 *
 * @par Time Conversion Reference Table:
 *
 * | From | To | Multiply By | Constant |
 * |------|----|-----------|---------|
 * | seconds | milliseconds | x 1,000 | k_rx_ms_per_second |
 * | seconds | microseconds | x 1,000,000 | k_rx_us_per_second |
 * | milliseconds | microseconds | x 1,000 | k_rx_us_per_ms |
 * | milliseconds | nanoseconds | x 1,000,000 | k_rx_ns_per_ms |
 * | microseconds | nanoseconds | x 1,000 | k_rx_ns_per_us |
 * | milliseconds | ThreadX ticks | / 10 | k_threadx_ms_per_tick |
 *
 * @invariant k_rx_us_per_second == k_rx_ms_per_second * k_rx_us_per_ms
 * @invariant k_rx_ns_per_ms == k_rx_us_per_ms * k_rx_ns_per_us
 * @invariant k_threadx_ms_per_tick == 1000 / tick_frequency_hz (100 Hz)
 *
 * @par Basic Timeout Example:
 * @code{.c}
 * #include "rx_time_constants.h"
 * #include "tx_api.h"
 *
 * // Wait 500ms using ThreadX sleep
 * void delay_500ms(void)
 * {
 *     uint32_t timeout_ms = 500;
 *     uint32_t ticks = timeout_ms / k_threadx_ms_per_tick;  // 500 / 10 = 50 ticks
 *     tx_thread_sleep(ticks);
 * }
 *
 * // Convert timeout from seconds to milliseconds
 * uint32_t calculate_timeout_ms(uint32_t timeout_sec)
 * {
 *     return timeout_sec * k_rx_ms_per_second;
 * }
 *
 * // Example: 5 second timeout
 * uint32_t timeout_ms = calculate_timeout_ms(5);  // Returns 5000ms
 * @endcode
 *
 * @par USB Timeout Example:
 * @code{.c}
 * #include "rx_time_constants.h"
 * #include "rx_usb.h"
 *
 * // USB read with 100ms timeout
 * rx_err_t usb_read_with_timeout(uint8_t* buffer, uint32_t len)
 * {
 *     uint32_t timeout_ms = 100;
 *     uint32_t ticks = timeout_ms / k_threadx_ms_per_tick;  // 100 / 10 = 10 ticks
 *
 *     rx_err_t err = rx_usb_read(buffer, len, ticks);
 *     if (err == k_rx_err_timeout) {
 *         rx_log_warn("USB", "Read timeout after 100ms");
 *     }
 *     return err;
 * }
 * @endcode
 *
 * @par Hardware Timer Example:
 * @code{.c}
 * #include "rx_time_constants.h"
 * #include "rx72n_regs.h"
 *
 * // Configure CMT0 for 10ms periodic interrupt (100 Hz)
 * void configure_cmt0_for_threadx(void)
 * {
 *     // Target period: 10ms
 *     uint32_t target_period_us = k_threadx_ms_per_tick * k_rx_us_per_ms;  // 10,000 us
 *
 *     // CMT0 clock: PCLKB/32 = 60 MHz / 32 = 1.875 MHz
 *     uint32_t cmt_clock_hz = 1875000;
 *     uint32_t cmt_period_us = k_rx_us_per_second / cmt_clock_hz;  // 0.533 us per tick
 *
 *     // Calculate compare value
 *     uint16_t compare_value = target_period_us / cmt_period_us;  // 18,750
 *
 *     // Configure CMT0
 *     CMT0.CMCOR = compare_value;
 * }
 * @endcode
 *
 * @par Watchdog Timeout Example:
 * @code{.c}
 * #include "rx_time_constants.h"
 * #include "rx_iwdt.h"
 *
 * // Configure IWDT for 8 second timeout
 * void configure_watchdog(void)
 * {
 *     uint32_t timeout_sec = 8;
 *     uint32_t timeout_ms = timeout_sec * k_rx_ms_per_second;  // 8000ms
 *
 *     rx_iwdt_config_t config = {
 *         .timeout_ms = timeout_ms,
 *         .window_start_percent = 0,
 *         .window_end_percent = 100
 *     };
 *
 *     rx_err_t err = rx_iwdt_init(&config);
 *     if (err != k_rx_ok) {
 *         // Handle error
 *     }
 * }
 * @endcode
 *
 * @see rx_iwdt.h Independent watchdog timer configuration
 * @see tx_thread_sleep() ThreadX sleep function (takes ticks)
 * @see rx_usb.h USB communication with timeout parameters
 * @see ThreadX User Guide Section on time management
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 8 (Limited preprocessor): Uses C23 typed enum, not macros [OK]
 */
typedef enum : uint32_t {
  /**
   * @brief Milliseconds per second (SI standard)
   * @details
   * Fundamental SI time conversion. One second contains exactly 1000 milliseconds.
   * Used for converting timeout values from seconds to milliseconds.
   * @par Value: 1,000 ms/s
   * @par Rationale: SI definition - 1 second = 1000 milliseconds
   * @par Common Usage: Timeout configuration, delay functions
   * @par Example:
   * @code{.c}
   * uint32_t timeout_ms = 5 * k_rx_ms_per_second;  // 5000ms = 5 seconds
   * @endcode
   */
  k_rx_ms_per_second = 1000,

  /**
   * @brief Microseconds per millisecond (SI standard)
   * @details
   * One millisecond contains exactly 1000 microseconds. Used for fine-grained
   * timing conversions and hardware timer calculations.
   * @par Value: 1,000 us/ms
   * @par Rationale: SI definition - 1 millisecond = 1000 microseconds
   * @par Common Usage: Hardware timer period calculations, precise delays
   * @par Example:
   * @code{.c}
   * uint32_t delay_us = 250 * k_rx_us_per_ms;  // 250,000 us = 250ms
   * @endcode
   */
  k_rx_us_per_ms = 1000,

  /**
   * @brief Nanoseconds per microsecond (SI standard)
   * @details
   * One microsecond contains exactly 1000 nanoseconds. Used for ultra-precise
   * timing calculations and CPU cycle counting.
   * @par Value: 1,000 ns/us
   * @par Rationale: SI definition - 1 microsecond = 1000 nanoseconds
   * @par Common Usage: CPU cycle time calculations (240 MHz = 4.17ns/cycle)
   * @par Example:
   * @code{.c}
   * // At 240 MHz, CPU cycle period is ~4.17ns
   * uint32_t cpu_freq_hz = 240000000;
   * uint32_t cycle_time_ns = k_rx_ns_per_us * k_rx_us_per_second / cpu_freq_hz;
   * // Result: 4.166ns per cycle
   * @endcode
   */
  k_rx_ns_per_us = 1000,

  /**
   * @brief Microseconds per second (derived SI constant)
   * @details
   * One second contains exactly 1,000,000 microseconds. Derived from:
   * k_rx_ms_per_second x k_rx_us_per_ms = 1000 x 1000 = 1,000,000
   * @par Value: 1,000,000 us/s
   * @par Rationale: Derived SI conversion for direct second-to-microsecond conversion
   * @par Common Usage: High-resolution timestamp conversions
   * @par Example:
   * @code{.c}
   * // Convert 2.5 seconds to microseconds
   * uint32_t duration_us = (uint32_t)(2.5f * k_rx_us_per_second);  // 2,500,000 us
   * @endcode
   * @note Max representable time: ~4294 seconds (71.6 minutes) before uint32_t overflow
   */
  k_rx_us_per_second = 1000000,

  /**
   * @brief Nanoseconds per millisecond (derived SI constant)
   * @details
   * One millisecond contains exactly 1,000,000 nanoseconds. Derived from:
   * k_rx_us_per_ms x k_rx_ns_per_us = 1000 x 1000 = 1,000,000
   * @par Value: 1,000,000 ns/ms
   * @par Rationale: Derived SI conversion for direct millisecond-to-nanosecond conversion
   * @par Common Usage: Precision timing, interrupt latency measurements
   * @par Example:
   * @code{.c}
   * // Calculate interrupt latency budget: 50us = 50,000ns
   * uint32_t latency_ns = 50 * k_rx_ns_per_us;
   * uint32_t latency_ms = latency_ns / k_rx_ns_per_ms;  // 0.05ms
   * @endcode
   * @note Max representable time: ~4.29 seconds before uint32_t overflow
   */
  k_rx_ns_per_ms = 1000000,

  /**
   * @brief Milliseconds per ThreadX tick (RTOS configuration)
   * @details
   * ThreadX tick period configured to 10ms (100 Hz tick rate). This value
   * is hardware-dependent and configured via CMT0 timer in ThreadX port.
   *
   * Configuration details:
   * - **Tick frequency**: 100 Hz
   * - **Tick period**: 10 milliseconds
   * - **Hardware timer**: CMT0 (Compare Match Timer 0)
   * - **CMT0 clock source**: PCLKB/32 = 60 MHz / 32 = 1.875 MHz
   * - **CMT0 compare value**: 18,750 counts -> 10ms period
   *
   * @par Value: 10 ms/tick
   * @par Rationale:
   * - 100 Hz provides good balance between responsiveness and overhead
   * - 10ms quantum matches typical control loop periods (motor control)
   * - Simplifies tick-to-millisecond conversions (divide by 10)
   * - Low enough overhead (<1% CPU time for context switching)
   *
   * @par Common Usage: Converting millisecond timeouts to RTOS ticks
   *
   * @par Example:
   * @code{.c}
   * // Sleep for 250ms
   * uint32_t sleep_ms = 250;
   * uint32_t sleep_ticks = sleep_ms / k_threadx_ms_per_tick;  // 25 ticks
   * tx_thread_sleep(sleep_ticks);
   * @endcode
   *
   * @warning If ThreadX tick configuration changes, update this constant!
   * @note Defined in tx_initialize_low_level.c (CMT0 configuration)
   * @see tx_initialize_low_level.c ThreadX tick timer initialization
   */
  k_threadx_ms_per_tick = 10,

} rx_time_conversion_t;

#ifdef __cplusplus
}
#endif
