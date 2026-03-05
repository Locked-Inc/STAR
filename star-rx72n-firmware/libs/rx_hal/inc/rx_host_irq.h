// SPDX-License-Identifier: MIT
/* lib/rx_hal/inc/rx_host_irq.h */

/**
 * @file rx_host_irq.h
 * @brief HOST_IRQ GPIO Output Driver for RPi5 Data-Ready Signaling
 *
 * @details
 * Manages the HOST_IRQ output pin (P67, pin 98) used to signal the Raspberry
 * Pi 5 host controller that the RX72N has data ready for SPI transfer. The
 * signal is active-low (assert LOW to request attention, deassert HIGH when
 * idle).
 *
 * ## Hardware Configuration
 *
 * | Parameter       | Value                              |
 * |-----------------|------------------------------------|
 * | Pin             | P67 (PORT6, pin 7, package pin 98) |
 * | Direction       | Output (RX72N -> RPi5)              |
 * | Active Level    | Active-low (asserted = LOW)        |
 * | Default State   | HIGH (deasserted / idle)           |
 * | Drive           | CMOS push-pull                     |
 *
 * ## Signaling Protocol
 *
 * ```
 * RX72N has telemetry data ready
 *         |
 * rx_host_irq_assert()     -> P67 driven LOW
 *         |
 * RPi5 detects falling edge on its GPIO
 *         |
 * RPi5 initiates SPI read
 *         |
 * rx_host_irq_deassert()   -> P67 driven HIGH
 * ```
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 3: No dynamic memory (static variables only)
 * - Rule 4: All functions < 60 lines
 * - Rule 5: 2+ validation checks per function
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums, no magic numbers
 * - Rule 10: -Wall -Wextra -Werror clean
 *
 * @see rx_spi_comm.h SPI communication driver
 * @see hardware_config.h Pin assignments (host_irq_pins group)
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
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
 * @brief Initialize HOST_IRQ pin as GPIO output (deasserted / HIGH)
 *
 * @details
 * Configures P67 as a GPIO output in CMOS push-pull mode. Sets the initial
 * output level to HIGH (deasserted / idle). The MPC function select is set
 * to GPIO (not IRQ input).
 *
 * Initialization steps:
 * 1. Clear PMR bit 7 (GPIO mode, not peripheral)
 * 2. Set PODR bit 7 HIGH (deasserted state)
 * 3. Set PDR bit 7 (output direction)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, P67 configured as output
 * @retval k_rx_err_invalid_state Already initialized
 *
 * @pre P67 not in use by another peripheral (RIIC2 etc.)
 * @post P67 configured as GPIO output, driven HIGH
 *
 * @note Not thread-safe. Call during system init only.
 *
 * @see rx_host_irq_deinit() Disable the output
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_host_irq_init(void);

/**
 * @brief Deinitialize HOST_IRQ pin (revert to input)
 *
 * @details
 * Reverts P67 to input mode (safe default) and deasserts the output.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_state Not initialized
 *
 * @pre rx_host_irq_init() called successfully
 * @post P67 reverted to input, no longer driving
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_host_irq_deinit(void);

/**
 * @brief Assert HOST_IRQ (drive LOW to signal data ready)
 *
 * @details
 * Drives P67 LOW to signal the RPi5 that the RX72N has data ready for
 * SPI transfer. The RPi5 should respond by initiating an SPI read.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin driven LOW
 * @retval k_rx_err_invalid_state Not initialized
 *
 * @pre rx_host_irq_init() called successfully
 * @post P67 driven LOW (active)
 *
 * @note Thread-safe: single register write is atomic on RX72N
 *
 * @see rx_host_irq_deassert() Release the signal
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_host_irq_assert(void);

/**
 * @brief Deassert HOST_IRQ (drive HIGH, idle state)
 *
 * @details
 * Drives P67 HIGH (idle). Call after the RPi5 has completed its SPI read.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin driven HIGH
 * @retval k_rx_err_invalid_state Not initialized
 *
 * @pre rx_host_irq_init() called successfully
 * @post P67 driven HIGH (idle)
 *
 * @note Thread-safe: single register write is atomic on RX72N
 *
 * @see rx_host_irq_assert() Assert the signal
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_host_irq_deassert(void);

/**
 * @brief Check if HOST_IRQ is currently asserted
 *
 * @details
 * Returns the current state of the HOST_IRQ output.
 *
 * @param[out] asserted Pointer to store state (true = asserted / LOW)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr asserted is nullptr
 *
 * @pre rx_host_irq_init() called
 * @post asserted set to current output state
 *
 * @note Thread-safe (reads volatile register)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_host_irq_is_asserted(bool* asserted);

#ifdef __cplusplus
}
#endif
