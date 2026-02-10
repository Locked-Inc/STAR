/* lib/rx_hal/inc/rx_bms_alert.h */

/**
 * @file rx_bms_alert.h
 * @brief BMS Alert Interrupt Driver for BQ4050 ALERT Pin (IRQ13)
 *
 * @details
 * Configures the RX72N ICU for external interrupt IRQ13, connected to the
 * BQ4050 fuel gauge ALERT output pin (P05, active-low, open-drain). When
 * the BQ4050 detects a battery fault condition, it asserts ALERT low,
 * triggering an immediate interrupt that sets the battery fault event flag
 * in shared_data and triggers an emergency stop.
 *
 * ## Hardware Configuration
 *
 * | Parameter     | Value                          |
 * |---------------|--------------------------------|
 * | Pin           | P05 (PORT0, pin 5, package pin 2) |
 * | IRQ           | IRQ13 (ICU vector 77)          |
 * | Edge          | Falling edge (active-low ALERT)|
 * | Digital Filter| PCLK/32 (1.6 us noise rejection)|
 * | Priority      | 13 (high, below motor fault)   |
 *
 * ## Response Time
 *
 * | Metric        | Value    | Notes                      |
 * |---------------|----------|----------------------------|
 * | Filter delay  | 1.6 us   | 3 samples at PCLK/32       |
 * | ISR entry     | ~0.5 us  | Interrupt latency @ 240 MHz|
 * | ISR execution | ~2 us    | Clear IR + set flag + e-stop|
 * | Total         | ~4 us    | Pin assertion to e-stop     |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 3: No dynamic memory (static state only)
 * - Rule 5: 2+ validation checks per function
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums, no magic numbers
 * - Rule 10: -Wall -Wextra -Werror clean
 *
 * @see rx_bq4050.h BQ4050 fuel gauge driver
 * @see rx_irq_filter.h IRQ digital filter configuration
 * @see bms_monitor_task.c BMS polling task (complementary to ISR)
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
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
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BMS alert interrupt (IRQ13, falling edge)
 *
 * @details
 * Configures ICU IRQ13 for falling-edge detection on pin P05 (BQ4050 ALERT).
 * Enables digital filtering at PCLK/32 for noise rejection. Sets interrupt
 * priority to 13 (high priority, just below POEG motor fault at 14).
 *
 * Initialization steps:
 * 1. Clear pending IR flag
 * 2. Set IRQCR[13] to falling edge
 * 3. Enable digital filter (PCLK/32)
 * 4. Set IPR[77] priority to 13
 * 5. Enable interrupt in IER[9] bit 5
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, IRQ13 enabled and ready
 * @retval k_rx_err_invalid_state Already initialized
 *
 * @pre P05 configured as input with pull-up
 * @pre BQ4050 ALERT pin physically connected to P05
 *
 * @post IRQ13 enabled for falling-edge detection
 * @post Digital filter active at PCLK/32
 *
 * @note Not thread-safe. Call during system init only.
 *
 * @see rx_bms_alert_deinit() Disable the interrupt
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bms_alert_init(void);

/**
 * @brief Disable BMS alert interrupt
 *
 * @details
 * Disables IRQ13 in the IER register and clears any pending interrupt.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_state Not initialized
 *
 * @pre rx_bms_alert_init() called successfully
 * @post IRQ13 disabled, no further interrupts generated
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bms_alert_deinit(void);

/**
 * @brief Check if BMS alert has been triggered since last clear
 *
 * @details
 * Returns the alert flag state. The flag is set by the ISR and must be
 * cleared by calling rx_bms_alert_clear().
 *
 * @param[out] triggered Pointer to store alert state (true = alert active)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr triggered is nullptr
 *
 * @pre rx_bms_alert_init() called
 * @post triggered set to current alert state
 *
 * @note Thread-safe (reads volatile flag)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bms_alert_is_triggered(bool* triggered);

/**
 * @brief Clear the BMS alert flag
 *
 * @details
 * Clears the software alert flag set by the ISR. Does not affect
 * the hardware interrupt configuration.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 *
 * @post Alert flag cleared
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bms_alert_clear(void);

#ifdef __cplusplus
}
#endif
