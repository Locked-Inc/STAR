/**
 * @file rx_host_irq.c
 * @brief HOST_IRQ GPIO Output Driver Implementation
 *
 * @details
 * Configures P67 (PORT6 pin 7) as a GPIO output for signaling the RPi5
 * host controller that the RX72N has data ready for SPI transfer. The pin
 * is active-low: assert LOW = data ready, deassert HIGH = idle.
 *
 * ## GPIO Configuration
 *
 * ```
 * rx_host_irq_init()
 *   1. PMR6.B7 = 0    (GPIO mode)
 *   2. PODR6.B7 = 1   (initial HIGH = idle)
 *   3. PDR6.B7 = 1    (output direction)
 *
 * rx_host_irq_assert()
 *   PODR6.B7 = 0      (drive LOW = data ready)
 *
 * rx_host_irq_deassert()
 *   PODR6.B7 = 1      (drive HIGH = idle)
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
 * @see rx_host_irq.h Public API
 * @see hardware_config.h Pin assignments
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_host_irq.h"

#include "rx72n_port_regs.h"
#include "rx_log.h"

static const char* const s_tag = "HOST_IRQ";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief GPIO configuration constants for HOST_IRQ on P67 */
typedef enum : uint8_t {
  k_host_irq_pin_bit  = 7,    /**< Pin 7 within PORT6 */
  k_host_irq_pin_mask = 0x80, /**< Bit mask for pin 7 (1 << 7) */
  k_host_irq_active   = 0,    /**< Active level: LOW */
  k_host_irq_idle     = 1,    /**< Idle level: HIGH */
} host_irq_gpio_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief Initialization flag */
static bool s_initialized = false;

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_host_irq_init(void)
{
  if (s_initialized) {
    rx_log_error(s_tag, "Already initialized");
    return k_rx_err_invalid_state;
  }

  volatile rx_port_regs_t* p6 = port6();

  /* Step 1: GPIO mode (clear PMR bit 7) */
  p6->pmr &= (uint8_t)~k_host_irq_pin_mask;

  /* Step 2: Set output HIGH (deasserted / idle) before enabling output */
  p6->podr |= k_host_irq_pin_mask;

  /* Step 3: Set direction to output */
  p6->pdr |= k_host_irq_pin_mask;

  s_initialized = true;

  rx_log_info(s_tag, "HOST_IRQ P67 initialized (output, idle HIGH)");
  return k_rx_ok;
}

/**
 * @brief Deinitialize the HOST_IRQ output and revert P67 to input
 *
 * @details
 * Symmetric counterpart to rx_host_irq_init().  Drives P67 high one last
 * time so the line is left in its idle (de-asserted) state, then changes
 * the pin direction back to input, gating off any spurious downstream
 * effects.  Clears the s_initialized flag so subsequent assert/deassert
 * calls return k_rx_err_invalid_state.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                Deinitialized successfully.
 * @retval k_rx_err_invalid_state Driver was not initialized.
 *
 * @pre rx_host_irq_init() previously returned k_rx_ok.
 *
 * @post On k_rx_ok: P67 is configured as an input and the s_initialized
 *       flag is false.
 * @post No effect on other PORT6 pins.
 *
 * @note Not thread-safe.  Serialize all rx_host_irq_* operations at the
 *       application layer.
 *
 * @see rx_host_irq_init() Re-arm before next assertion cycle.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_host_irq_deinit(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  volatile rx_port_regs_t* p6 = port6();

  /* Deassert before reverting to input */
  p6->podr |= k_host_irq_pin_mask;

  /* Revert to input (safe default) */
  p6->pdr &= (uint8_t)~k_host_irq_pin_mask;

  s_initialized = false;

  rx_log_info(s_tag, "HOST_IRQ P67 deinitialized (input)");
  return k_rx_ok;
}

/**
 * @brief Assert the HOST_IRQ line (drive P67 LOW, active-low semantics)
 *
 * @details
 * Drives the P67 output LOW to signal the host SoC that the RX72N has
 * data ready or wants attention.  HOST_IRQ is an active-low,
 * level-triggered line; the host will continue to see "asserted" until
 * rx_host_irq_deassert() drives it back HIGH.  The function is a single
 * register read-modify-write on PORT6.PODR; no interrupts are touched.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                Line driven low.
 * @retval k_rx_err_invalid_state Driver was not initialized.
 *
 * @pre rx_host_irq_init() previously returned k_rx_ok.
 *
 * @post On k_rx_ok: PORT6.PODR bit 7 == 0 (line driven low).
 * @post No interrupt registers are touched.
 *
 * @note Safe to call from task context.  Not protected against concurrent
 *       calls; serialize at the application layer.
 *
 * @see rx_host_irq_deassert()    Drive line high (de-assert).
 * @see rx_host_irq_is_asserted() Read the current line state.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_host_irq_assert(void)
{
  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  volatile rx_port_regs_t* p6 = port6();
  p6->podr &= (uint8_t)~k_host_irq_pin_mask;

  return k_rx_ok;
}

/**
 * @brief De-assert the HOST_IRQ line (drive P67 HIGH, idle state)
 *
 * @details
 * Drives the P67 output HIGH to release the active-low HOST_IRQ line back
 * to its idle state.  Single register read-modify-write on PORT6.PODR;
 * no interrupts touched.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                Line driven high.
 * @retval k_rx_err_invalid_state Driver was not initialized.
 *
 * @pre rx_host_irq_init() previously returned k_rx_ok.
 *
 * @post On k_rx_ok: PORT6.PODR bit 7 == 1 (line driven high).
 * @post No interrupt registers are touched.
 *
 * @note Safe to call from task context.  Not protected against concurrent
 *       calls; serialize at the application layer.
 *
 * @see rx_host_irq_assert()      Drive line low (assert).
 * @see rx_host_irq_is_asserted() Read the current line state.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_host_irq_deassert(void)
{
  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  volatile rx_port_regs_t* p6 = port6();
  p6->podr |= k_host_irq_pin_mask;

  return k_rx_ok;
}

rx_err_t rx_host_irq_is_asserted(bool* asserted)
{
  if (asserted == nullptr) {
    return k_rx_err_null_ptr;
  }

  volatile rx_port_regs_t* p6 = port6();

  /* Active-low: pin LOW means asserted */
  *asserted = ((p6->podr & k_host_irq_pin_mask) == k_host_irq_active);

  return k_rx_ok;
}
