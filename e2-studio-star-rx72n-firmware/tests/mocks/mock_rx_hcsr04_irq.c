/**
 * @file mock_rx_hcsr04_irq.c
 * @brief Mock implementations for HC-SR04 IRQ mode functions (for unit testing)
 *
 * @details
 * Stub implementations of MPC, ICU, and ISR functions used by the HC-SR04
 * driver in IRQ mode. These allow unit tests to exercise driver logic without
 * requiring real hardware or ICU configuration.
 *
 * All stubs accept calls silently and return success (or k_rx_err_timeout for
 * get_duration, which simulates the "not ready" state expected by the driver).
 *
 * @note Signatures must exactly match the real implementations in:
 *       - rx_mpc.h (rx_mpc_set_gpio, rx_mpc_set_irq)
 *       - rx_hcsr04_icu.h (rx_hcsr04_icu_configure, rx_hcsr04_icu_disable)
 *       - rx_hcsr04_isr.h (rx_hcsr04_isr_register, rx_hcsr04_isr_unregister,
 *                          rx_hcsr04_isr_start, rx_hcsr04_isr_get_duration)
 *
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.2.0 (Issue #296)
 */

#include <stdint.h>

#include "rx_err.h"
#include "rx_hcsr04_icu.h"
#include "rx_hcsr04_isr.h"
#include "rx_mpc.h"
#include "rx_port_constants.h"

/* =============================================================================
 * MPC Mock Functions
 * =============================================================================
 */

/**
 * @brief Mock: Configure pin for GPIO mode (no-op stub)
 *
 * @param[in] pin GPIO pin identifier (unused in mock)
 * @return k_rx_ok always
 */
rx_err_t rx_mpc_set_gpio(const rx_port_pin_t pin)
{
  (void)pin;
  return k_rx_ok;
}

/**
 * @brief Mock: Configure pin for IRQ function (no-op stub)
 *
 * @param[in] pin GPIO pin identifier (unused in mock)
 * @return k_rx_ok always
 */
rx_err_t rx_mpc_set_irq(const rx_port_pin_t pin)
{
  (void)pin;
  return k_rx_ok;
}

/* =============================================================================
 * ICU Mock Functions
 * =============================================================================
 */

/**
 * @brief Mock: Configure ICU for edge detection (no-op stub)
 *
 * @param[in] irq_num  IRQ number (unused in mock)
 * @param[in] priority Interrupt priority (unused in mock)
 * @return k_rx_ok always
 */
rx_err_t rx_hcsr04_icu_configure(const uint8_t irq_num, const uint8_t priority)
{
  (void)irq_num;
  (void)priority;
  return k_rx_ok;
}

/**
 * @brief Mock: Disable ICU interrupt (no-op stub)
 *
 * @param[in] irq_num IRQ number to disable (unused in mock)
 * @return k_rx_ok always
 */
rx_err_t rx_hcsr04_icu_disable(const uint8_t irq_num)
{
  (void)irq_num;
  return k_rx_ok;
}

/* =============================================================================
 * ISR Mock Functions
 * =============================================================================
 */

/**
 * @brief Mock: Register sensor for IRQ echo measurement (no-op stub)
 *
 * @param[in] irq_num      IRQ number (unused in mock)
 * @param[in] sensor_index Sensor array index (unused in mock)
 * @return k_rx_ok always
 */
rx_err_t rx_hcsr04_isr_register(const uint8_t irq_num, const uint8_t sensor_index)
{
  (void)irq_num;
  (void)sensor_index;
  return k_rx_ok;
}

/**
 * @brief Mock: Unregister sensor from IRQ echo measurement (no-op stub)
 *
 * @param[in] irq_num IRQ number (unused in mock)
 * @return k_rx_ok always
 */
rx_err_t rx_hcsr04_isr_unregister(const uint8_t irq_num)
{
  (void)irq_num;
  return k_rx_ok;
}

/**
 * @brief Mock: Arm ISR state machine before trigger pulse (no-op stub)
 *
 * @param[in] irq_num IRQ number (unused in mock)
 */
void rx_hcsr04_isr_start(const uint8_t irq_num)
{
  (void)irq_num;
}

/**
 * @brief Mock: Get echo pulse duration (always returns timeout)
 *
 * @details
 * Returns k_rx_err_timeout to simulate "not ready" state. The driver's
 * timeout handling is exercised this way during unit tests.
 *
 * @param[in]  irq_num     IRQ number (unused in mock)
 * @param[out] duration_us Output pointer (unused in mock)
 * @return k_rx_err_timeout always (ISR never fires in test environment)
 */
rx_err_t rx_hcsr04_isr_get_duration(const uint8_t irq_num, uint32_t* const duration_us)
{
  (void)irq_num;
  (void)duration_us;
  return k_rx_err_timeout; /* ISR never fires in unit test environment */
}
