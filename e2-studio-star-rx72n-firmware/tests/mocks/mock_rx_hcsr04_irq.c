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
 * @version 1.2.0
 *
 * @see docs/sections/06_nasa_power_of_10.tex NASA Power of 10 rules applied in this module
 * @see docs/sections/01_nanopb_protocol.tex System architecture and design document
 *
 * @par NASA Power of 10
 * - Rule 2: All stub functions have O(1) complexity (no loops)
 * - Rule 3: No dynamic memory allocation; all stubs return constants
 * - Rule 5: Stubs accept any argument values; validation is the real implementation's concern
 * - Rule 7: Return values are fixed constants; callers must always check them
 *
 * @par SOLID
 * - Single Responsibility: This file only provides no-op stubs for IRQ-mode hardware functions
 * - Dependency Injection: Tests link against this file instead of the real driver objects,
 *                         enabling driver logic testing without hardware
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
 * @details
 * No-op stub. Real implementation writes the PFS register via MPC to put
 * the pin back in GPIO mode. In the test environment no hardware is present,
 * so this function discards its argument and returns success.
 *
 * @param[in] pin GPIO pin identifier (any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre GPIO subsystem initialized (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real MPC hardware)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no GPIO state)
 *
 * @note No-op: does not write any hardware registers
 * @code
 * rx_err_t err = rx_mpc_set_gpio(k_rx_p0_3);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_mpc_set_gpio() Real implementation in rx_mpc.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_mpc_set_gpio(const rx_port_pin_t pin)
{
  (void)pin;
  return k_rx_ok;
}

/**
 * @brief Mock: Configure pin for IRQ function (no-op stub)
 *
 * @details
 * No-op stub. Real implementation sets the ISEL bit in the PFS register
 * via MPC to route the pin to the ICU. In the test environment no hardware
 * is present, so this function discards its argument and returns success.
 *
 * @param[in] pin GPIO pin identifier (any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre GPIO subsystem initialized (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real MPC hardware)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no MPC state)
 *
 * @note No-op: does not write any hardware registers
 * @code
 * rx_err_t err = rx_mpc_set_irq(k_rx_p0_3);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_mpc_set_irq() Real implementation in rx_mpc.h
 * @since Version 1.2.0 (Issue #296)
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
 * @details
 * No-op stub. Real implementation configures IRQCR, enables the digital
 * filter, sets the interrupt priority and enables the IER bit. In the test
 * environment no ICU hardware is present, so this function discards its
 * arguments and returns success.
 *
 * @param[in] irq_num  IRQ number (8-11; any value accepted in mock)
 * @param[in] priority Interrupt priority (1-15; any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre ICU subsystem initialized (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real ICU hardware)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ICU state)
 *
 * @note No-op: does not write any ICU registers
 * @code
 * rx_err_t err = rx_hcsr04_icu_configure((uint8_t)k_hcsr04_irq_11,
 *                                         k_hcsr04_irq_priority_default);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_icu_configure() Real implementation in rx_hcsr04_icu.h
 * @since Version 1.2.0 (Issue #296)
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
 * @details
 * No-op stub. Real implementation clears the IER bit, clears the IR flag
 * and disables the digital filter. In the test environment no ICU hardware
 * is present, so this function discards its argument and returns success.
 *
 * @param[in] irq_num IRQ number to disable (8-11; any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre ICU subsystem initialized (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real ICU hardware)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ICU state)
 *
 * @note No-op: does not write any ICU registers
 * @code
 * rx_err_t err = rx_hcsr04_icu_disable((uint8_t)k_hcsr04_irq_11);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_icu_disable() Real implementation in rx_hcsr04_icu.h
 * @since Version 1.2.0 (Issue #296)
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
 * @details
 * No-op stub. Real implementation stores the irq_num → sensor_index
 * mapping in a static lookup table so the ISR can identify which sensor
 * triggered. In the test environment no ISR fires, so this function
 * discards its arguments and returns success.
 *
 * @param[in] irq_num      IRQ number (8-11; any value accepted in mock)
 * @param[in] sensor_index Sensor array index (0-3; any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre IRQ subsystem initialized (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real ISR will fire)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no sensor map)
 *
 * @note No-op: does not update any lookup tables
 * @code
 * rx_err_t err = rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_11, 0);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_isr_register() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
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
 * @details
 * No-op stub. Real implementation restores the sensor map slot to the
 * k_sensor_unused sentinel. In the test environment no ISR fires, so
 * this function discards its argument and returns success.
 *
 * @param[in] irq_num IRQ number (8-11; any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre IRQ subsystem initialized (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real ISR will fire)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no sensor map)
 *
 * @note No-op: does not modify any lookup tables
 * @code
 * rx_err_t err = rx_hcsr04_isr_unregister((uint8_t)k_hcsr04_irq_11);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_isr_unregister() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_unregister(const uint8_t irq_num)
{
  (void)irq_num;
  return k_rx_ok;
}

/**
 * @brief Mock: Arm ISR state machine before trigger pulse (no-op stub)
 *
 * @details
 * No-op stub. Real implementation sets the active flag and clears the
 * complete flag in the per-IRQ state structure. In the test environment
 * no ISR fires, so this function discards its argument and returns success.
 *
 * @param[in] irq_num IRQ number (8-11; any value accepted in mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok always (no hardware to fail)
 *
 * @pre IRQ registered via rx_hcsr04_isr_register() (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real ISR will fire)
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ISR state)
 *
 * @note No-op: does not update any ISR state flags
 * @code
 * rx_err_t err = rx_hcsr04_isr_start((uint8_t)k_hcsr04_irq_11);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_isr_start() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_start(const uint8_t irq_num)
{
  (void)irq_num;
  return k_rx_ok;
}

/**
 * @brief Mock: Get echo pulse duration (always returns timeout)
 *
 * @details
 * Always returns k_rx_err_timeout to simulate the "not ready" state.
 * Because no real ISR fires in the unit-test environment, the complete
 * flag is never set and the driver's timeout handling path is exercised.
 *
 * @param[in]  irq_num     IRQ number (8-11; any value accepted in mock)
 * @param[out] duration_us Output pointer (not written in mock; may be NULL)
 *
 * @return rx_err_t
 * @retval k_rx_err_timeout always (ISR never fires in test environment)
 *
 * @pre rx_hcsr04_isr_start() called before trigger pulse (in real hardware; not required for mock)
 * @pre Running in unit-test mock mode (no real ISR will fire)
 *
 * @post No hardware side-effects
 * @post *duration_us unchanged (mock does not write the output)
 *
 * @note Always returns k_rx_err_timeout; tests must rely on driver timeout logic
 * @code
 * uint32_t dur = 0;
 * rx_err_t err = rx_hcsr04_isr_get_duration((uint8_t)k_hcsr04_irq_11, &dur);
 * assert(err == k_rx_err_timeout);  // Always times out in mock
 * @endcode
 *
 * @see rx_hcsr04_isr_get_duration() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_get_duration(const uint8_t irq_num, uint32_t* const duration_us)
{
  (void)irq_num;
  (void)duration_us;
  return k_rx_err_timeout; /* ISR never fires in unit test environment */
}
