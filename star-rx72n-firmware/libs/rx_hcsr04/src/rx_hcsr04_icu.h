/**
 * @file rx_hcsr04_icu.h
 * @brief HC-SR04 ICU (Interrupt Controller Unit) Configuration Layer
 *
 * @details
 * Provides ICU configuration functions for HC-SR04 ultrasonic sensors
 * operating in IRQ mode. Configures external interrupt pins (IRQ8-11)
 * for hardware edge detection with microsecond-precision timing.
 *
 * **Responsibilities:**
 * - Configure IRQCR for edge detection (both edges)
 * - Enable digital filtering to debounce mechanical noise
 * - Set interrupt priority (typical: 10)
 * - Enable/disable IRQ in ICU IER register
 *
 * **Usage Flow:**
 * 1. Configure pin for IRQ function via `rx_mpc_set_irq()`
 * 2. Call `rx_hcsr04_icu_configure()` to set up ICU
 * 3. ISR captures rising and falling edges automatically
 * 4. Call `rx_hcsr04_icu_disable()` to clean up
 *
 * @par STAR Project Usage (HC-SR04 Sensors)
 * | IRQ | Pin | Sensor   | Location     |
 * |-----|-----|----------|--------------|
 * | 11  | P03 | Sonar 0  | Front-Left   |
 * | 10  | P02 | Sonar 1  | Front-Right  |
 * | 9   | P01 | Sonar 2  | Back-Left    |
 * | 8   | P00 | Sonar 3  | Back-Right   |
 *
 * @par Example: Configure IRQ11 for HC-SR04 Echo
 * @code
 * // Step 1: Configure pin for IRQ function
 * rx_err_t err = rx_mpc_set_irq(k_rx_p0_3);  // P03 -> IRQ11
 * if (err != k_rx_ok) {
 *     return err;
 * }
 *
 * // Step 2: Configure ICU for edge detection
 * err = rx_hcsr04_icu_configure((uint8_t)k_hcsr04_irq_11, k_hcsr04_irq_priority_default);
 * if (err != k_rx_ok) {
 *     return err;
 * }
 *
 * // Now ISR will fire on both rising and falling edges
 * @endcode
 *
 * @note This is an internal header (src/ not inc/)
 * @note Only supports IRQ8-11 (P00-P03 pins; ISR handlers exist for IRQ8-11 only)
 *
 * @see rx_mpc_set_irq() Configure pin for IRQ function first
 * @see rx_hcsr04_isr.h ISR handler layer
 * @see RX72N Manual Chapter 15 - ICU (Interrupt Controller Unit)
 * @see docs/sections/06_nasa_power_of_10.tex NASA Power of 10 rules applied in this module
 * @see docs/sections/01_nanopb_protocol.tex System architecture and design document
 *
 * @author Locked, Inc.
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @version 1.0.0
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 2: All loops in rx_hcsr04_icu_configure() / rx_hcsr04_icu_disable() have fixed bounds
 * - Rule 5: Minimum 2 preconditions and 4 postconditions per public function
 * - Rule 7: All register write results validated; filter enable/disable errors propagated
 * - Rule 8: All constants are named C23 typed enums (icu_constants_t); no magic numbers
 *
 * @par SOLID Principles Adherence
 * - Single Responsibility: ICU configuration only; MPC pin setup is in rx_mpc.h,
 *                          ISR state management is in rx_hcsr04_isr.h
 * - Open/Closed: Behaviour extended via configuration parameters (irq_num, priority)
 *                without modifying this module
 * - Dependency Inversion: Depends on rx_err.h abstraction; no direct hardware coupling
 *                         beyond ICU register access
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure ICU for HC-SR04 echo IRQ measurement
 *
 * @details
 * Sets up IRQ edge detection, digital filter, priority, and enables interrupt.
 * Uses both-edge detection to capture rising (start) and falling (end) edges
 * of the echo pulse automatically.
 *
 * **Configuration Applied:**
 * - IRQCR[n] = k_irqcr_both_edges (both edges trigger interrupt)
 * - Digital filter enabled (PCLK/8 = 133ns @ 60MHz PCLKB)
 * - Interrupt priority set to specified value (recommend 10)
 * - IR flag cleared (any pending interrupt discarded)
 * - IER bit set (interrupt enabled)
 *
 * **Edge Detection:**
 * - Rising edge (echo HIGH): ISR captures start timestamp
 * - Falling edge (echo LOW): ISR captures end timestamp
 * - Duration = end - start (microseconds)
 *
 * @param[in] irq_num IRQ number (8-11 for P00-P03; ISR handlers exist for IRQ8-11 only)
 *                    - IRQ8  (P00) - Sonar 3 (Back-Right)
 *                    - IRQ9  (P01) - Sonar 2 (Back-Left)
 *                    - IRQ10 (P02) - Sonar 1 (Front-Right)
 *                    - IRQ11 (P03) - Sonar 0 (Front-Left)
 * @param[in] priority Interrupt priority (1-15)
 *                     - 1  = Lowest priority
 *                     - 10 = Recommended (balances latency and preemption)
 *                     - 15 = Highest priority (use sparingly)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok ICU configured successfully
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]; priority not in range [1, 15]; irq_num not valid for digital filter (propagated from rx_irq_filter_enable())
 *
 * @pre Pin already configured for IRQ function via rx_mpc_set_irq()
 * @pre ICU module not in module stop (MSTPCRA bit cleared)
 * @pre No other driver using this IRQ number
 *
 * @post IRQCR[irq_num] configured for both-edge detection
 * @post Digital filter enabled (PCLK/8)
 * @post Interrupt priority set
 * @post ISR will trigger on rising and falling edges
 *
 * @note Call this AFTER rx_mpc_set_irq() (pin must be in IRQ mode first)
 * @note Thread-safe: Can be called during initialization only
 * @note Digital filter provides ~133ns noise immunity @ 60MHz PCLKB
 *
 * @warning Do not call while measurement in progress
 * @warning Ensure ISR handler is registered before enabling interrupt
 *
 * @par Example: Configure All 4 HC-SR04 Sensors
 * @code
 * const rx_hcsr04_irq_t irq_nums[] = {
 *     k_hcsr04_irq_11,  // Sonar 0 Front-Left
 *     k_hcsr04_irq_10,  // Sonar 1 Front-Right
 *     k_hcsr04_irq_9,   // Sonar 2 Back-Left
 *     k_hcsr04_irq_8,   // Sonar 3 Back-Right
 * };
 * const uint8_t sensor_count = (uint8_t)(sizeof(irq_nums) / sizeof(irq_nums[0]));
 * const uint8_t priority = k_hcsr04_irq_priority_default;  // 10
 *
 * for (uint8_t i = 0; i < sensor_count; i++) {
 *     rx_err_t err = rx_hcsr04_icu_configure((uint8_t)irq_nums[i], priority);
 *     if (err != k_rx_ok) {
 *         rx_log_error("SONAR", "ICU config failed");
 *         return err;
 *     }
 * }
 * @endcode
 *
 * @see rx_mpc_set_irq() Must be called first to configure pin
 * @see rx_hcsr04_icu_disable() Disable IRQ when done
 * @see rx_hcsr04_isr.h ISR handler receiving edge events
 * @see RX72N Manual Chapter 15.2 - External Interrupts (IRQ)
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 5: [OK] 3 preconditions, 4 postconditions
 * - Rule 7: [OK] All register writes validated
 * - Rule 8: [OK] Typed enums for all constants
 */
[[nodiscard]] rx_err_t rx_hcsr04_icu_configure(uint8_t irq_num, uint8_t priority);

/**
 * @brief Disable HC-SR04 echo IRQ in ICU
 *
 * @details
 * Disables the specified IRQ interrupt and digital filter. Use this
 * during sensor deinitialization or when switching from IRQ mode back
 * to polling mode.
 *
 * **Operations Performed:**
 * - Clear IER bit (interrupt disabled)
 * - Clear IR flag (discard any pending interrupt)
 * - Disable digital filter
 * - IRQCR left unchanged (safe state)
 *
 * @param[in] irq_num IRQ number to disable (8-11)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok IRQ and digital filter both disabled successfully
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]; irq_num not valid for digital filter (propagated from rx_irq_filter_disable())
 * @retval k_rx_err_invalid_state IRQ was not enabled in IER (rx_hcsr04_icu_configure() not called first)
 *
 * @pre IRQ was previously enabled via rx_hcsr04_icu_configure() (IER bit must be set)
 * @pre No measurement in progress on this IRQ
 *
 * @post IER bit cleared (interrupt will not fire)
 * @post IR flag cleared (no stale pending interrupt)
 * @post Digital filter disabled
 * @post ISR will not trigger on pin edges
 *
 * @note Call only after rx_hcsr04_icu_configure() has enabled the IRQ
 * @note Does NOT reconfigure pin back to GPIO (use rx_mpc_set_gpio separately)
 *
 * @warning Do not call while measurement in progress
 * @warning Returns k_rx_err_invalid_state if the IRQ IER bit is not set; always call configure() first
 *
 * @par Example: Clean Up Sensor
 * @code
 * // Disable IRQ11 (Sonar 0)
 * rx_err_t err = rx_hcsr04_icu_disable(k_hcsr04_irq_11);
 * if (err != k_rx_ok) {
 *     rx_log_warn("SONAR", "Failed to disable IRQ11");
 * }
 *
 * // Optionally reconfigure pin back to GPIO
 * (void)rx_mpc_set_gpio(k_rx_p0_3);
 * @endcode
 *
 * @see rx_hcsr04_icu_configure() Enable IRQ
 * @see rx_mpc_set_gpio() Reconfigure pin back to GPIO mode
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 5: [OK] 2 preconditions (IER enabled + no measurement in progress), 4 postconditions
 * - Rule 7: [OK] IER state validated before clearing; rx_irq_filter_disable() return propagated
 * - Rule 8: [OK] Typed enums for all constants (icu_constants_t)
 */
[[nodiscard]] rx_err_t rx_hcsr04_icu_disable(uint8_t irq_num);

#ifdef __cplusplus
}
#endif
