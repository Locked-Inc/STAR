/**
 * @file rx_irq_filter.h
 * @brief IRQ Digital Filter Driver API for RX72N
 *
 * @details
 * Provides hardware digital noise filtering for external interrupt (IRQ) pins
 * on the RX72N microcontroller. The filter helps eliminate electrical noise,
 * switch bounce, and transient glitches that could cause spurious interrupts.
 *
 * The RX72N ICU provides hardware digital filters on all 16 IRQ pins (IRQ0-15).
 * Each filter samples the input at a configurable rate and requires a stable
 * level for 3 consecutive samples to register as a valid transition.
 *
 * @par System Architecture
 * @verbatim
 *                     IRQ Digital Filter Architecture
 *   +---------------------------------------------------------------------+
 *   |                        External Inputs                             |
 *   |   +---------+  +---------+  +---------+  +---------+               |
 *   |   | E-STOP  |  |  VBUS   |  |Expansion|  |  Hall   |  ... (16)    |
 *   |   | Button  |  | Detect  |  |  Input  |  | Encoder |               |
 *   |   +----+----+  +----+----+  +----+----+  +----+----+               |
 *   +--------+-----------+-----------+-----------+-----------------------+
 *            |           |           |           |
 *   +--------v-----------v-----------v-----------v-----------------------+
 *   |                    ICU Digital Filter Block                        |
 *   |   +-------------------------------------------------------------+  |
 *   |   |   IRQ0 --> [3-Sample Filter] --> IRQn Interrupt             |  |
 *   |   |   IRQ1 --> [3-Sample Filter] --> IRQn Interrupt             |  |
 *   |   |    ...              ...                  ...                 |  |
 *   |   |   IRQ15 -> [3-Sample Filter] --> IRQn Interrupt             |  |
 *   |   +-------------------------------------------------------------+  |
 *   |                                                                    |
 *   |   Control Registers:                                               |
 *   |   +------------+  +------------+                                   |
 *   |   | IRQFLTE[2] |  | IRQFLTC[2] |                                   |
 *   |   | (Enable)   |  | (Clock)    |                                   |
 *   |   +------------+  +------------+                                   |
 *   +--------------------------------------------------------------------+
 * @endverbatim
 *
 * @par Digital Filter Operation
 * @verbatim
 *   Input Signal with Noise:
 *        ___________      _   _______________
 *   ____|           |____| |_|
 *                        ^ ^
 *                        | | Noise spikes
 *
 *   After Digital Filter (3 samples @ PCLK/64):
 *        ___________      ___________________
 *   ____|           |____|
 *                              ^
 *                              | Noise rejected (< 3 sample duration)
 *
 *   Filter Timing:
 *   - Sample period = PCLKB / divisor
 *   - Transition delay = 3 x sample period
 *   - With PCLKB=60MHz, PCLK/64: 3.2us delay (rejects noise < 1us)
 * @endverbatim
 *
 * @par Filter Response Times (PCLKB = 60 MHz)
 * | Divisor | Sample Period | Response Time | Noise Rejection |
 * |---------|---------------|---------------|-----------------|
 * | PCLK/1 | 16.7 ns | 50 ns | Minimal |
 * | PCLK/8 | 133 ns | 400 ns | Fast signals |
 * | PCLK/32 | 533 ns | 1.6 us | Moderate |
 * | PCLK/64 | 1.07 us | 3.2 us | Buttons/switches |
 *
 * @par STAR Project Usage
 * | IRQ Pin | Function | Recommended Filter | Notes |
 * |---------|----------|-------------------|-------|
 * | IRQ0 | E-STOP Button | PCLK/64 | Max filtering for safety |
 * | IRQ5 | USB VBUS | PCLK/32 | Moderate noise rejection |
 * | IRQ8 | Expansion | PCLK/32 | General-purpose interrupt |
 *
 * @par Hardware Requirements
 * | Resource | Requirement | Notes |
 * |----------|-------------|-------|
 * | PCLKB | Active | Filter clock source |
 * | ICU | Enabled | Not in module stop |
 *
 * @par Memory Usage
 * | Category | Size | Notes |
 * |----------|------|-------|
 * | Code | ~200 bytes | All functions |
 * | Static data | 0 bytes | No static variables |
 * | Stack | 8 bytes | Per function call |
 *
 * @par Thread Safety
 * Not thread-safe. ICU register access is not atomic. Use mutex protection
 * if filter configuration changes at runtime. Recommended: configure filters
 * once during initialization before starting RTOS.
 *
 * @par Usage Example - Basic
 * @code{.c}
 * #include "rx_irq_filter.h"
 *
 * void configure_interrupts(void)
 * {
 *     // Enable maximum filtering on E-STOP button (safety-critical)
 *     rx_irq_filter_enable(0, k_irq_filter_pclk_64);
 *
 *     // Enable moderate filtering on VBUS detection
 *     rx_irq_filter_enable(5, k_irq_filter_pclk_32);
 *
 *     // Configure ICU for edge detection (separate from filtering)
 *     // ...
 * }
 * @endcode
 *
 * @par Usage Example - Dynamic Configuration
 * @code{.c}
 * void adjust_filter_for_mode(bool high_noise_environment)
 * {
 *     if (high_noise_environment) {
 *         // Increase filtering in noisy environments
 *         rx_irq_filter_enable(8, k_irq_filter_pclk_64);
 *     } else {
 *         // Reduce filtering for faster response
 *         rx_irq_filter_enable(8, k_irq_filter_pclk_8);
 *     }
 * }
 *
 * bool check_filter_config(uint8_t irq_num)
 * {
 *     bool enabled;
 *     rx_irq_filter_clk_t clock;
 *
 *     rx_irq_filter_is_enabled(irq_num, &enabled);
 *     rx_irq_filter_get_clock(irq_num, &clock);
 *
 *     return enabled && (clock == k_irq_filter_pclk_64);
 * }
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No goto, setjmp, longjmp, recursion
 * - **Rule 2**: [OK] No loops (simple register operations)
 * - **Rule 3**: [OK] No dynamic memory allocation
 * - **Rule 4**: [OK] All functions < 30 lines
 * - **Rule 5**: [OK] Input validation on all functions
 * - **Rule 7**: [OK] All return values are error codes
 * - **Rule 8**: [OK] C23 typed enums for filter clock options
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: Only IRQ filter configuration
 * - **Interface Segregation**: Minimal 4-function API
 *
 * @see rx72n_icu_regs.h ICU register definitions (IRQFLTE, IRQFLTC)
 * @see RX72N Hardware Manual, Chapter 13 - Interrupt Controller Unit
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum rx_irq_filter_limits_t
 * @brief IRQ filter limits for RX72N
 *
 * @details
 * The RX72N ICU supports external IRQ pins IRQ0-IRQ15 (16 total).
 */
typedef enum : uint8_t {
  k_rx_irq_max = 15,
} rx_irq_filter_limits_t;

/**
 * @enum rx_irq_filter_clk_t
 * @brief IRQ filter clock divisor options for sampling rate control
 *
 * @details
 * Selects the clock divisor for IRQ digital filter sampling. Higher divisors
 * result in slower sampling, providing more noise rejection but also longer
 * response times to valid transitions.
 *
 * The digital filter requires 3 consecutive samples at the same level to
 * recognize a valid transition. Total response time = 3 x sample period.
 *
 * @par Response Time Calculation
 * @f[
 *   T_{response} = 3 \times \frac{1}{f_{PCLKB} / divisor} = \frac{3 \times divisor}{f_{PCLKB}}
 * @f]
 *
 * @par Filter Characteristics (PCLKB = 60 MHz)
 * | Divisor | Sample Period | Response Time | Best For |
 * |---------|---------------|---------------|----------|
 * | /1 | 16.7 ns | 50 ns | High-speed signals |
 * | /8 | 133 ns | 400 ns | Encoder pulses |
 * | /32 | 533 ns | 1.6 us | Sensor interrupts |
 * | /64 | 1.07 us | 3.2 us | Buttons, switches |
 *
 * @par Noise Rejection Capability
 * Noise pulses shorter than 2 x sample period are rejected. For PCLK/64 with
 * PCLKB=60MHz, noise pulses < 2.1us are filtered out.
 *
 * @note Register encoding: 2 bits per IRQ in IRQFLTC register
 * @warning Lower divisors provide faster response but less noise immunity
 *
 * @see rx_irq_filter_enable() Set filter clock divisor
 * @see rx_irq_filter_get_clock() Read current divisor setting
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief PCLKB/1 - Fastest sampling, minimal filtering
   * @details
   * Sample period: 16.7 ns @ 60 MHz PCLKB
   * Response time: 50 ns (3 samples)
   * Noise rejection: < 33 ns pulses filtered
   * @par Use Case: High-frequency signals requiring minimal delay
   * @warning Provides almost no noise filtering
   */
  k_irq_filter_pclk_1 = 0,

  /**
   * @brief PCLKB/8 - Fast sampling, light filtering
   * @details
   * Sample period: 133 ns @ 60 MHz PCLKB
   * Response time: 400 ns (3 samples)
   * Noise rejection: < 267 ns pulses filtered
   * @par Use Case: Encoder signals, fast sensors
   */
  k_irq_filter_pclk_8 = 1,

  /**
   * @brief PCLKB/32 - Moderate sampling, good filtering
   * @details
   * Sample period: 533 ns @ 60 MHz PCLKB
   * Response time: 1.6 us (3 samples)
   * Noise rejection: < 1.07 us pulses filtered
   * @par Use Case: External interrupts, VBUS detection, expansion inputs
   */
  k_irq_filter_pclk_32 = 2,

  /**
   * @brief PCLKB/64 - Slowest sampling, maximum filtering
   * @details
   * Sample period: 1.07 us @ 60 MHz PCLKB
   * Response time: 3.2 us (3 samples)
   * Noise rejection: < 2.1 us pulses filtered
   * @par Use Case: Mechanical buttons, E-STOP, user input switches
   * @note Recommended for all human-activated inputs
   */
  k_irq_filter_pclk_64 = 3,
} rx_irq_filter_clk_t;

/**
 * @var s_irq_max
 * @brief Maximum valid IRQ number (0-15)
 *
 * @details
 * The RX72N ICU supports 16 external interrupt pins, numbered IRQ0 through
 * IRQ15. This constant defines the maximum valid IRQ number for validation.
 *
 * @par Value: 15
 * @par Valid IRQ Range: [0, 15]
 *
 * @note Static const rather than enum due to use in comparison operations
 * @since Version 1.0.0
 */
static const uint8_t s_irq_max = 15U;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Enable hardware digital filter on an IRQ pin
 *
 * @details
 * Enables the ICU digital filter for the specified IRQ pin with the given
 * clock divisor. The filter samples the input and requires 3 consecutive
 * samples at the same level to recognize a valid edge transition.
 *
 * @par Algorithm Steps
 * 1. Validate irq_num is in range [0, 15]
 * 2. Validate filter_clk is valid divisor
 * 3. Calculate register index (0 for IRQ0-7, 1 for IRQ8-15)
 * 4. Set clock divisor in IRQFLTC register
 * 5. Set enable bit in IRQFLTE register
 *
 * @param[in] irq_num IRQ number to configure
 *                    - Valid range: [0, 15]
 *                    - IRQ0-7 use IRQFLTE[0]/IRQFLTC[0]
 *                    - IRQ8-15 use IRQFLTE[1]/IRQFLTC[1]
 * @param[in] filter_clk Clock divisor for filter sampling
 *                       - k_irq_filter_pclk_1: Minimal filtering
 *                       - k_irq_filter_pclk_64: Maximum filtering
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Filter successfully enabled
 * @retval k_rx_err_invalid_arg irq_num > 15 or filter_clk > k_irq_filter_pclk_64
 *
 * @pre PCLKB clock must be running (filter clock source)
 * @pre ICU module not in module stop
 *
 * @post IRQFLTE bit set for this IRQ
 * @post IRQFLTC bits set to specified divisor
 * @post Filter immediately active
 *
 * @note Filter applies to all edge types (rising, falling, both)
 * @note Filter continues operating during software standby
 * @note Clock divisor is set before enable to avoid glitches
 *
 * @warning Enabling filter adds latency to interrupt response
 *
 * @par Thread Safety
 * Not thread-safe. ICU register modification is not atomic.
 *
 * @par Performance
 * - Execution time: ~200 ns @ 240 MHz
 * - No blocking operations
 *
 * @par Example - E-STOP Button
 * @code{.c}
 * // Maximum filtering for mechanical button debounce
 * rx_err_t err = rx_irq_filter_enable(0, k_irq_filter_pclk_64);
 * if (err != k_rx_ok) {
 *     // Handle error
 * }
 * @endcode
 *
 * @see rx_irq_filter_disable() Disable filter
 * @see rx_irq_filter_clk_t Clock divisor options
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_irq_filter_enable(uint8_t irq_num, rx_irq_filter_clk_t filter_clk);

/**
 * @brief Disable hardware digital filter on an IRQ pin
 *
 * @details
 * Disables the ICU digital filter for the specified IRQ pin. After disabling,
 * the raw unfiltered input signal is passed through to the interrupt detector.
 *
 * @par Algorithm Steps
 * 1. Validate irq_num is in range [0, 15]
 * 2. Calculate register index
 * 3. Clear enable bit in IRQFLTE register
 *
 * @param[in] irq_num IRQ number to configure
 *                    - Valid range: [0, 15]
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Filter successfully disabled
 * @retval k_rx_err_invalid_arg irq_num > 15
 *
 * @pre None (can be called regardless of current filter state)
 *
 * @post IRQFLTE bit cleared for this IRQ
 * @post Raw input passed to interrupt detector
 *
 * @note Clock divisor setting is preserved (unchanged)
 * @note Safe to call even if filter was already disabled
 *
 * @par Thread Safety
 * Not thread-safe. ICU register modification is not atomic.
 *
 * @par Example
 * @code{.c}
 * // Disable filter for faster response
 * rx_irq_filter_disable(5);
 * @endcode
 *
 * @see rx_irq_filter_enable() Enable filter
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_irq_filter_disable(uint8_t irq_num);

/**
 * @brief Check if digital filter is enabled on an IRQ pin
 *
 * @details
 * Reads the IRQFLTE register to determine if the digital filter is
 * currently enabled for the specified IRQ.
 *
 * @param[in] irq_num IRQ number to query
 *                    - Valid range: [0, 15]
 * @param[out] enabled Pointer to receive enable status
 *                     - true: Filter is enabled
 *                     - false: Filter is disabled
 *                     - Must not be nullptr
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Status successfully retrieved
 * @retval k_rx_err_invalid_arg irq_num > 15 or enabled pointer is nullptr
 *
 * @pre enabled pointer must be non-NULL
 *
 * @post *enabled contains current filter state
 * @post No registers modified (read-only operation)
 *
 * @par Thread Safety
 * Thread-safe for reading. Register read is atomic.
 *
 * @par Example
 * @code{.c}
 * bool filter_active;
 * rx_err_t err = rx_irq_filter_is_enabled(0, &filter_active);
 * if (err == k_rx_ok && filter_active) {
 *     // Filter is active on IRQ0
 * }
 * @endcode
 *
 * @see rx_irq_filter_enable() Enable filter
 * @see rx_irq_filter_get_clock() Get clock setting
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_irq_filter_is_enabled(uint8_t irq_num, bool* enabled);

/**
 * @brief Get current filter clock divisor setting for an IRQ
 *
 * @details
 * Reads the IRQFLTC register to retrieve the current clock divisor
 * setting for the specified IRQ's digital filter.
 *
 * @param[in] irq_num IRQ number to query
 *                    - Valid range: [0, 15]
 * @param[out] filter_clk Pointer to receive clock divisor setting
 *                        - Returns rx_irq_filter_clk_t value
 *                        - Must not be nullptr
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Clock setting successfully retrieved
 * @retval k_rx_err_invalid_arg irq_num > 15 or filter_clk pointer is nullptr
 *
 * @pre filter_clk pointer must be non-NULL
 *
 * @post *filter_clk contains current divisor setting
 * @post No registers modified (read-only operation)
 *
 * @note Returns current setting even if filter is disabled
 *
 * @par Thread Safety
 * Thread-safe for reading. Register read is atomic.
 *
 * @par Example
 * @code{.c}
 * rx_irq_filter_clk_t current_clock;
 * rx_err_t err = rx_irq_filter_get_clock(0, &current_clock);
 * if (err == k_rx_ok) {
 *     if (current_clock == k_irq_filter_pclk_64) {
 *         // Maximum filtering configured
 *     }
 * }
 * @endcode
 *
 * @see rx_irq_filter_enable() Set clock divisor
 * @see rx_irq_filter_clk_t Clock divisor values
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_irq_filter_get_clock(uint8_t irq_num, rx_irq_filter_clk_t* filter_clk);

#ifdef __cplusplus
}
#endif
