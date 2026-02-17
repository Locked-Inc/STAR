/**
 * @file mock_rx_hcsr04_irq.c
 * @brief Mock implementations for HC-SR04 IRQ mode functions (for unit testing)
 *
 * @details
 * Stub implementations of MPC, ICU, and ISR functions used by the HC-SR04
 * driver in IRQ mode. These allow unit tests to exercise driver logic without
 * requiring real hardware or ICU configuration.
 *
 * Stubs perform the same minimal parameter validation as the real API
 * (IRQ range, priority range, NULL pointer checks) so that tests exercise
 * error paths. On valid input, stubs return success (or k_rx_err_timeout
 * for get_duration, simulating the "not ready" state expected by the driver).
 *
 * @note Signatures must exactly match the real implementations in:
 *       - rx_mpc.h (rx_mpc_set_gpio, rx_mpc_set_irq)
 *       - rx_hcsr04_icu.h (rx_hcsr04_icu_configure, rx_hcsr04_icu_disable)
 *       - rx_hcsr04_isr.h (rx_hcsr04_isr_register, rx_hcsr04_isr_unregister,
 *                          rx_hcsr04_isr_start, rx_hcsr04_isr_get_duration,
 *                          rx_hcsr04_isr_disarm)
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
 * - Rule 5: Stubs validate irq_num range, priority range, and NULL pointers
 *           (mirrors real API contract for Liskov substitutability)
 * - Rule 7: Return values are fixed constants; callers must always check them
 *
 * @par SOLID
 * - Single Responsibility: This file only provides no-op stubs for IRQ-mode hardware functions
 * - Liskov Substitution: Stubs enforce the same parameter constraints as the real API
 * - Dependency Injection: Tests link against this file instead of the real driver objects,
 *                         enabling driver logic testing without hardware
 */

#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_hcsr04_icu.h"
#include "rx_hcsr04_isr.h"
#include "rx_mpc.h"
#include "rx_port_constants.h"

/* =============================================================================
 * Mock Validation Constants
 * =============================================================================
 */

/**
 * @enum mock_irq_constants_t
 * @brief Validation constants for mock parameter checking
 *
 * @details
 * Mirrors the range constraints enforced by the real ICU and ISR implementations.
 * Ensures mocks reject out-of-range arguments with the same error codes as
 * the real API, enabling tests to exercise error paths correctly.
 *
 * @since Version 1.2.0 (Issue #296)
 */
typedef enum : uint8_t {
  k_mock_irq_min       = 8,  /**< Minimum valid IRQ number (IRQ8 = P00) */
  k_mock_irq_max       = 11, /**< Maximum valid IRQ number (IRQ11 = P03) */
  k_mock_priority_min  = 1,  /**< Minimum valid interrupt priority */
  k_mock_priority_max  = 15, /**< Maximum valid interrupt priority */
  k_mock_sensor_count  = 4,  /**< Number of valid sensor indices (0-3) */
} mock_irq_constants_t;

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
 * @note Thread-safe: no shared state; stub always returns constant k_rx_ok
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
 * @note Thread-safe: no shared state; stub always returns constant k_rx_ok
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
 * @brief Mock: Configure ICU for edge detection (validates irq_num and priority)
 *
 * @details
 * No-op stub with parameter validation matching the real API contract.
 * Returns k_rx_err_invalid_arg if irq_num is outside [8, 11] or priority
 * is outside [1, 15]. On valid input returns k_rx_ok without side-effects.
 *
 * @param[in] irq_num  IRQ number (must be in range [8, 11])
 * @param[in] priority Interrupt priority (must be in range [1, 15])
 *
 * @return rx_err_t
 * @retval k_rx_ok Valid arguments, no-op performed
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11] or priority not in [1, 15]
 *
 * @pre Running in unit-test mock mode (no real ICU hardware)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ICU state)
 *
 * @note Validates parameters to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
 * @code
 * rx_err_t err = rx_hcsr04_icu_configure((uint8_t)k_hcsr04_irq_11,
 *                                         k_hcsr04_irq_priority_default);
 * assert(err == k_rx_ok);
 *
 * err = rx_hcsr04_icu_configure(99, 10);  // Out-of-range IRQ
 * assert(err == k_rx_err_invalid_arg);
 * @endcode
 *
 * @see rx_hcsr04_icu_configure() Real implementation in rx_hcsr04_icu.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_icu_configure(const uint8_t irq_num, const uint8_t priority)
{
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  if (priority < k_mock_priority_min || priority > k_mock_priority_max) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}

/**
 * @brief Mock: Disable ICU interrupt (validates irq_num)
 *
 * @details
 * No-op stub with parameter validation matching the real API contract.
 * Returns k_rx_err_invalid_arg if irq_num is outside [8, 11].
 * On valid input returns k_rx_ok without side-effects.
 *
 * @param[in] irq_num IRQ number to disable (must be in range [8, 11])
 *
 * @return rx_err_t
 * @retval k_rx_ok Valid argument, no-op performed
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11]
 *
 * @pre Running in unit-test mock mode (no real ICU hardware)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ICU state)
 *
 * @note Validates irq_num to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
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
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}

/* =============================================================================
 * ISR Mock Functions
 * =============================================================================
 */

/**
 * @brief Mock: Register sensor for IRQ echo measurement (validates irq_num and sensor_index)
 *
 * @details
 * No-op stub with parameter validation matching the real API contract.
 * Returns k_rx_err_invalid_arg if irq_num is outside [8, 11] or
 * sensor_index >= k_mock_sensor_count (4).
 *
 * @param[in] irq_num      IRQ number (must be in range [8, 11])
 * @param[in] sensor_index Sensor array index (must be < k_mock_sensor_count)
 *
 * @return rx_err_t
 * @retval k_rx_ok Valid arguments, no-op performed
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11] or sensor_index >= 4
 *
 * @pre Running in unit-test mock mode (no real ISR will fire)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no sensor map)
 *
 * @note Validates parameters to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
 * @code
 * rx_err_t err = rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_11,
 *                                        k_hcsr04_sensor_front_left);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_isr_register() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_register(const uint8_t irq_num, const rx_hcsr04_sensor_index_t sensor_index)
{
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)sensor_index >= k_mock_sensor_count) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}

/**
 * @brief Mock: Unregister sensor from IRQ echo measurement (validates irq_num)
 *
 * @details
 * No-op stub with parameter validation matching the real API contract.
 * Returns k_rx_err_invalid_arg if irq_num is outside [8, 11].
 *
 * @param[in] irq_num IRQ number (must be in range [8, 11])
 *
 * @return rx_err_t
 * @retval k_rx_ok Valid argument, no-op performed
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11]
 *
 * @pre Running in unit-test mock mode (no real ISR will fire)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no sensor map)
 *
 * @note Validates irq_num to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
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
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}

/**
 * @brief Mock: Arm ISR state machine before trigger pulse (validates irq_num)
 *
 * @details
 * No-op stub with parameter validation matching the real API contract.
 * Returns k_rx_err_invalid_arg if irq_num is outside [8, 11].
 *
 * @param[in] irq_num IRQ number (must be in range [8, 11])
 *
 * @return rx_err_t
 * @retval k_rx_ok Valid argument, no-op performed
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11]
 *
 * @pre Running in unit-test mock mode (no real ISR will fire)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ISR state)
 *
 * @note Validates irq_num to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
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
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}

/**
 * @brief Mock: Get echo pulse duration (validates parameters, returns timeout)
 *
 * @details
 * Validates irq_num range and duration_us pointer before returning
 * k_rx_err_timeout to simulate the "not ready" state. Because no real
 * ISR fires in the unit-test environment, the complete flag is never set
 * and the driver's timeout handling path is exercised.
 *
 * @param[in]  irq_num     IRQ number (must be in range [8, 11])
 * @param[out] duration_us Output pointer (must not be NULL; not written in mock)
 *
 * @return rx_err_t
 * @retval k_rx_err_timeout Valid arguments, ISR never fires in test environment
 * @retval k_rx_err_null_ptr duration_us is NULL
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11]
 *
 * @pre Running in unit-test mock mode (no real ISR will fire)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post *duration_us unchanged (mock does not write the output)
 *
 * @note Validates parameters to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
 * @code
 * uint32_t dur = 0;
 * rx_err_t err = rx_hcsr04_isr_get_duration((uint8_t)k_hcsr04_irq_11, &dur);
 * assert(err == k_rx_err_timeout);  // Always times out in mock
 *
 * err = rx_hcsr04_isr_get_duration((uint8_t)k_hcsr04_irq_11, NULL);
 * assert(err == k_rx_err_null_ptr);  // NULL pointer rejected
 * @endcode
 *
 * @see rx_hcsr04_isr_get_duration() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_get_duration(const uint8_t irq_num, uint32_t* const duration_us)
{
  if (duration_us == NULL) {
    return k_rx_err_null_ptr;
  }
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_err_timeout; /* ISR never fires in unit test environment */
}

/**
 * @brief Mock: Disarm ISR state machine (validates irq_num)
 *
 * @details
 * No-op stub with parameter validation matching the real API contract.
 * Returns k_rx_err_invalid_arg if irq_num is outside [8, 11].
 *
 * @param[in] irq_num IRQ number (must be in range [8, 11])
 *
 * @return rx_err_t
 * @retval k_rx_ok Valid argument, no-op performed
 * @retval k_rx_err_invalid_arg irq_num not in [8, 11]
 *
 * @pre Running in unit-test mock mode (no real ISR will fire)
 * @pre irq_num in [k_mock_irq_min, k_mock_irq_max]
 *
 * @post No hardware side-effects
 * @post State unchanged (mock has no ISR state)
 *
 * @note Validates irq_num to match real API error paths (Liskov substitution)
 * @note Thread-safe: no shared state
 * @code
 * rx_err_t err = rx_hcsr04_isr_disarm((uint8_t)k_hcsr04_irq_11);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_hcsr04_isr_disarm() Real implementation in rx_hcsr04_isr.h
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_disarm(const uint8_t irq_num)
{
  if (irq_num < k_mock_irq_min || irq_num > k_mock_irq_max) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}
