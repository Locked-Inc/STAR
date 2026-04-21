/**
 * @file rx_tmr.h
 * @brief TMR HAL Driver for RX72N 8-Bit Timer (TMRb)
 *
 * @details
 * Hardware abstraction layer for the RX72N TMR peripheral, providing
 * four 8-bit timers (TMR0..TMR3) organized as two units of two channels.
 * Each unit can be used either as two independent 8-bit timers or as a
 * single 16-bit cascaded timer (TMR01 or TMR23).
 *
 * ## Architecture
 *
 * ```
 * Application Layer:   periodic tasks, soft PWM, low-speed triggers
 *                           |
 * HAL Layer:          rx_tmr.h/c (THIS MODULE - register access)
 *                           |
 * Register Layer:     rx72n_tmr_regs.h (memory-mapped I/O)
 * ```
 *
 * ## Unit and Cascade Layout
 *
 * | Unit | Channels    | Module Stop    | Cascade Name |
 * |------|-------------|----------------|--------------|
 * | 0    | TMR0, TMR1  | MSTPCRA.MSTPA5 | TMR01        |
 * | 1    | TMR2, TMR3  | MSTPCRA.MSTPA4 | TMR23        |
 *
 * In a cascaded pair, the even channel (TMR0/TMR2) holds the upper 8 bits
 * and the odd channel (TMR1/TMR3) holds the lower 8 bits.
 *
 * ## Clock Sources
 *
 * The TMR counts on either an external signal at the TMCI pin or an
 * internal PCLK-divided signal:
 *
 * | Source      | Divider  | Tick @ PCLKB=60 MHz | 8-bit max period |
 * |-------------|----------|---------------------|------------------|
 * | Internal    | PCLK/1   | 16.67 ns            | 4.27 us          |
 * | Internal    | PCLK/2   | 33.33 ns            | 8.53 us          |
 * | Internal    | PCLK/8   | 133.3 ns            | 34.13 us         |
 * | Internal    | PCLK/32  | 533.3 ns            | 136.5 us         |
 * | Internal    | PCLK/64  | 1.067 us            | 273.1 us         |
 * | Internal    | PCLK/1024| 17.07 us            | 4.369 ms         |
 * | Internal    | PCLK/8192| 136.5 us            | 34.95 ms         |
 * | External    | rising   | external            | external         |
 * | External    | falling  | external            | external         |
 * | External    | both     | external            | external         |
 *
 * 16-bit cascaded mode extends the max period by 256x.
 *
 * ## Interrupts
 *
 * TMR compare-match A/B and overflow interrupts are routed via the ICU
 * SELECTB (Software Configurable Interrupt B) mechanism. The driver
 * provides a callback registration hook so higher layers can install
 * handlers without touching the interrupt controller directly.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: All loops bounded
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: All functions < 60 lines
 * - Rule 5: Minimum 2 validation checks per function
 * - Rule 6: Variables at smallest scope
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums, minimal macros
 * - Rule 9: Function pointers allowed (DIP) for ISR registration
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Only TMR hardware access (no scheduling, no signal logic)
 * - **O**: Extensible via rx_tmr_config_t
 * - **L**: Consistent rx_err_t return semantics
 * - **I**: Focused API: init, start/stop, read, set period, register ISR, deinit
 * - **D**: Depends on register abstractions, not raw addresses
 *
 * @see rx72n_tmr_regs.h TMR register definitions
 * @see rx_tpu.h TPU HAL (16-bit pulse unit)
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Enumerations
 * =============================================================================
 */

/**
 * @enum rx_tmr_channel_t
 * @brief TMR channel identifiers
 *
 * @details
 * Four 8-bit TMR channels are available. Channels within the same unit
 * (TMR0+TMR1 in unit 0, TMR2+TMR3 in unit 1) may be cascaded into a
 * 16-bit counter; see #rx_tmr_mode_t and rx_tmr_init().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_channel_0 = 0, /**< TMR0 - unit 0 even channel */
  k_tmr_channel_1 = 1, /**< TMR1 - unit 0 odd channel  */
  k_tmr_channel_2 = 2, /**< TMR2 - unit 1 even channel */
  k_tmr_channel_3 = 3, /**< TMR3 - unit 1 odd channel  */
} rx_tmr_channel_t;

/**
 * @enum rx_tmr_mode_t
 * @brief TMR operating mode (independent 8-bit vs cascaded 16-bit)
 *
 * @details
 * In 16-bit cascade mode, the even channel (TMR0/TMR2) provides the upper
 * byte and the odd channel (TMR1/TMR3) provides the lower byte. When
 * initialising cascade mode, pass the even channel number; the driver
 * configures both channels accordingly.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_mode_8bit_independent = 0, /**< Two independent 8-bit counters per unit */
  k_tmr_mode_16bit_cascade    = 1, /**< Cascaded 16-bit counter (TMR01 or TMR23) */
} rx_tmr_mode_t;

/**
 * @enum rx_tmr_clock_source_t
 * @brief TMR clock source selection
 *
 * @details
 * Combines the CSS (clock source) and CKS (clock select) fields of TCCR
 * into a single configuration value. Internal sources use PCLK dividers;
 * external sources use the TMCI input pin with selectable edge detection.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_clock_pclk_div_1    = 0, /**< Internal PCLK / 1    */
  k_tmr_clock_pclk_div_2    = 1, /**< Internal PCLK / 2    */
  k_tmr_clock_pclk_div_8    = 2, /**< Internal PCLK / 8    */
  k_tmr_clock_pclk_div_32   = 3, /**< Internal PCLK / 32   */
  k_tmr_clock_pclk_div_64   = 4, /**< Internal PCLK / 64   */
  k_tmr_clock_pclk_div_1024 = 5, /**< Internal PCLK / 1024 */
  k_tmr_clock_pclk_div_8192 = 6, /**< Internal PCLK / 8192 */
  k_tmr_clock_external_rising  = 7, /**< External TMCI rising-edge count  */
  k_tmr_clock_external_falling = 8, /**< External TMCI falling-edge count */
  k_tmr_clock_external_both    = 9, /**< External TMCI both-edge count    */
} rx_tmr_clock_source_t;

/**
 * @enum rx_tmr_counter_clear_t
 * @brief Counter clear source (CCLR field of TCR)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_clear_disabled       = 0, /**< Counter never cleared by TMR itself */
  k_tmr_clear_cmp_match_a    = 1, /**< Clear on TCORA compare match        */
  k_tmr_clear_cmp_match_b    = 2, /**< Clear on TCORB compare match        */
  k_tmr_clear_external_reset = 3, /**< Clear on external reset input       */
} rx_tmr_counter_clear_t;

/**
 * @enum rx_tmr_output_t
 * @brief TMO pin behaviour on compare-match (OSA/OSB encoding)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_output_no_change = 0, /**< Pin unchanged on match  */
  k_tmr_output_low       = 1, /**< Drive pin low on match  */
  k_tmr_output_high      = 2, /**< Drive pin high on match */
  k_tmr_output_toggle    = 3, /**< Toggle pin on match     */
} rx_tmr_output_t;

/**
 * @enum rx_tmr_irq_source_t
 * @brief TMR interrupt source bitmask (may be OR-combined)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_irq_none         = 0x00, /**< No interrupts enabled               */
  k_tmr_irq_cmp_match_a  = 0x01, /**< Enable CMIA (TCORA compare match)   */
  k_tmr_irq_cmp_match_b  = 0x02, /**< Enable CMIB (TCORB compare match)   */
  k_tmr_irq_overflow     = 0x04, /**< Enable OVI (TCNT overflow)          */
} rx_tmr_irq_source_t;

/* =============================================================================
 * Configuration Structure
 * =============================================================================
 */

/**
 * @struct rx_tmr_config_t
 * @brief TMR channel configuration
 *
 * @details
 * Passed to rx_tmr_init() to configure one channel (independent mode) or
 * one cascaded pair (16-bit mode). In cascade mode, #channel must be an
 * even channel (k_tmr_channel_0 or k_tmr_channel_2) and the paired odd
 * channel is configured automatically.
 *
 * @par Example - 1 ms periodic interrupt on TMR0
 * @code{.c}
 * rx_tmr_config_t cfg = {
 *     .channel        = k_tmr_channel_0,
 *     .mode           = k_tmr_mode_8bit_independent,
 *     .clock_source   = k_tmr_clock_pclk_div_1024,
 *     .counter_clear  = k_tmr_clear_cmp_match_a,
 *     .output_a       = k_tmr_output_no_change,
 *     .output_b       = k_tmr_output_no_change,
 *     .irq_mask       = k_tmr_irq_cmp_match_a,
 * };
 * rx_err_t err = rx_tmr_init(&cfg);
 * @endcode
 *
 * @invariant channel is one of k_tmr_channel_0..3
 * @invariant clock_source is a valid rx_tmr_clock_source_t value
 *
 * @since Version 1.0.0
 */
typedef struct {
  /** @brief TMR channel (cascade mode requires an even channel) */
  rx_tmr_channel_t channel;

  /** @brief Operating mode (8-bit independent or 16-bit cascade) */
  rx_tmr_mode_t mode;

  /** @brief Clock source (internal divider or external edge) */
  rx_tmr_clock_source_t clock_source;

  /** @brief Counter clear trigger */
  rx_tmr_counter_clear_t counter_clear;

  /** @brief TMO pin behaviour on TCORA compare match */
  rx_tmr_output_t output_a;

  /** @brief TMO pin behaviour on TCORB compare match */
  rx_tmr_output_t output_b;

  /**
   * @brief Interrupt enable bitmask (OR of rx_tmr_irq_source_t values)
   * @details Set to k_tmr_irq_none to leave all TMR interrupts disabled.
   */
  uint8_t irq_mask;
} rx_tmr_config_t;

/**
 * @typedef rx_tmr_isr_callback_t
 * @brief User callback for compare-match / overflow events
 *
 * @param[in] channel TMR channel that raised the event
 *
 * @note Called from interrupt context; keep implementations short.
 * @since Version 1.0.0
 */
typedef void (*rx_tmr_isr_callback_t)(rx_tmr_channel_t channel);

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialise a TMR channel (or cascaded pair)
 *
 * @details
 * Enables the module stop clock for the channel's unit (MSTPA5 for unit
 * 0, MSTPA4 for unit 1), stops the counter, writes TCR/TCSR/TCCR/TCORA/
 * TCORB according to the supplied configuration, and clears TCNT to zero.
 * In cascade mode, both channels of the unit are configured in one call.
 *
 * @param[in] config Pointer to configuration (non-NULL)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr config is NULL
 * @retval k_rx_err_invalid_arg Invalid channel, clock, clear, output, or
 *         a cascade request on an odd channel
 * @retval k_rx_err_invalid_state Channel already initialised
 *
 * @pre System clocks configured (PCLKB running)
 * @pre In cascade mode, config->channel is k_tmr_channel_0 or
 *      k_tmr_channel_2 (even channel)
 *
 * @post Channel counter stopped at 0
 * @post Module clock enabled (MSTPCRA bit cleared)
 * @post Internal init flag set
 *
 * @note Not thread-safe; call from main-task context at startup.
 *
 * @see rx_tmr_start() Begin counting
 * @see rx_tmr_deinit() Release the channel
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_init(const rx_tmr_config_t* config);

/**
 * @brief Start counting on a TMR channel
 *
 * @details
 * Writes the configured clock-source bits into TCCR so the counter begins
 * advancing on each selected clock edge. Channel must have been initialised
 * via rx_tmr_init().
 *
 * @param[in] channel TMR channel (cascade: pass the even channel)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Counter started
 * @retval k_rx_err_invalid_arg Channel out of range
 * @retval k_rx_err_invalid_state Channel not initialised
 *
 * @pre Channel initialised via rx_tmr_init()
 * @post TCCR programmed with clock source (counter running)
 *
 * @note Thread Safety: Safe - single register write.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_start(rx_tmr_channel_t channel);

/**
 * @brief Stop a TMR channel counter
 *
 * @details
 * Writes 0 into TCCR CKS/CSS to disable the clock input. The TCNT value
 * is preserved.
 *
 * @param[in] channel TMR channel (cascade: pass the even channel)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Counter stopped
 * @retval k_rx_err_invalid_arg Channel out of range
 * @retval k_rx_err_invalid_state Channel not initialised
 *
 * @pre Channel initialised
 * @post Counter no longer advances
 * @post TCNT preserved
 *
 * @note Thread Safety: Safe - single register write.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_stop(rx_tmr_channel_t channel);

/**
 * @brief Read the current TMR counter value
 *
 * @details
 * For 8-bit mode, *value holds the 8-bit TCNT zero-extended to uint16_t.
 * For 16-bit cascade mode, *value holds the full 16-bit value
 * (even channel = upper byte, odd channel = lower byte).
 *
 * @param[in]  channel TMR channel (cascade: pass the even channel)
 * @param[out] value   Pointer receiving counter value (non-NULL)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr value is NULL
 * @retval k_rx_err_invalid_arg Channel out of range
 * @retval k_rx_err_invalid_state Channel not initialised
 *
 * @pre Channel initialised
 * @post *value contains counter at time of read
 *
 * @note Thread Safety: Safe - single volatile read.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_read(rx_tmr_channel_t channel, uint16_t* value);

/**
 * @brief Set the compare-match period in microseconds
 *
 * @details
 * Recomputes and writes TCORA so that a TCORA compare match fires every
 * @p period_us microseconds using the channel's configured clock source.
 * TCORB is untouched. The caller must ensure the period fits into the
 * channel counter width (8-bit single channel max = ~35 ms at PCLK/8192;
 * 16-bit cascade max ~= 8.9 s at PCLK/8192).
 *
 * The underlying formula is
 * <tt>tcora = round(period_us * PCLK_Hz / (divider * 1e6)) - 1</tt>,
 * where divider is 1/2/8/32/64/1024/8192 according to the configured
 * clock_source. External clock sources are not supported.
 *
 * @param[in] channel   TMR channel (cascade: pass the even channel)
 * @param[in] period_us Desired period in microseconds (1..max for mode)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, TCORA updated
 * @retval k_rx_err_invalid_arg Channel out of range, period=0, period too
 *         large for the selected divider, or external clock source
 * @retval k_rx_err_invalid_state Channel not initialised
 *
 * @pre Channel initialised with an internal-clock clock_source
 * @post TCORA holds the rounded tick count - 1
 *
 * @note Thread Safety: Safe - single register write.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_set_period_us(rx_tmr_channel_t channel, uint32_t period_us);

/**
 * @brief Register an interrupt callback for compare-match / overflow events
 *
 * @details
 * Installs a user callback invoked from the ICU SELECTB dispatcher when
 * any of the channel's enabled TMR interrupts (CMIA, CMIB, or OVI) fire.
 * The driver's ISR shim clears the source flag before calling back.
 * Pass NULL to deregister.
 *
 * @param[in] channel  TMR channel (cascade: pass the even channel)
 * @param[in] callback User callback, or NULL to deregister
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Channel out of range
 *
 * @pre Channel initialised with desired irq_mask
 * @post callback is installed (or cleared) atomically
 *
 * @note Thread Safety: Safe - function-pointer write is atomic on RX.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_register_compare_match_isr(rx_tmr_channel_t      channel,
                                                         rx_tmr_isr_callback_t callback);

/**
 * @brief Deinitialise a TMR channel
 *
 * @details
 * Stops the counter, clears TCR/TCSR/TCCR, zeroes TCNT, removes any
 * installed ISR callback, and marks the channel as uninitialised. The
 * module stop bit is NOT asserted because the paired channel in the same
 * unit may still be active.
 *
 * @param[in] channel TMR channel (cascade: pass the even channel)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel released
 * @retval k_rx_err_invalid_arg Channel out of range
 *
 * @pre (none - safe to call on uninitialised channel)
 * @post Channel registers cleared
 * @post Internal init flag cleared
 *
 * @note Thread Safety: Not thread-safe.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tmr_deinit(rx_tmr_channel_t channel);

#ifdef __cplusplus
}
#endif
