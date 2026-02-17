/**
 * @file rx_hcsr04_isr.h
 * @brief HC-SR04 ISR (Interrupt Service Routine) Handler Layer
 *
 * @details
 * Provides ISR handlers and state management for HC-SR04 ultrasonic sensors
 * operating in IRQ mode. Captures rising and falling edge timestamps with
 * microsecond precision for accurate distance measurement.
 *
 * **Responsibilities:**
 * - ISR functions for IRQ8-15 (INT_IRQ8 through INT_IRQ15)
 * - Per-IRQ state management (start/end timestamps, completion flag)
 * - Edge type detection (rising vs falling)
 * - Timestamp capture via hardware abstraction layer
 *
 * **Operation:**
 * 1. Application calls `rx_hcsr04_isr_start()` before trigger pulse
 * 2. ISR fires on rising edge → captures start timestamp
 * 3. ISR fires on falling edge → captures end timestamp, sets complete flag
 * 4. Application calls `rx_hcsr04_isr_get_duration()` to retrieve pulse width
 *
 * @par STAR Project ISR Mapping
 * | ISR Function | IRQ | Pin | Sensor   | Location     |
 * |--------------|-----|-----|----------|--------------|
 * | INT_IRQ11    | 11  | P03 | Sonar 0  | Front-Left   |
 * | INT_IRQ10    | 10  | P02 | Sonar 1  | Front-Right  |
 * | INT_IRQ9     | 9   | P01 | Sonar 2  | Back-Left    |
 * | INT_IRQ8     | 8   | P00 | Sonar 3  | Back-Right   |
 *
 * @par Example: Typical Usage Flow
 * @code
 * // Step 1: Register sensor with ISR (during init)
 * rx_hcsr04_isr_register(11, 0);  // IRQ11 → Sensor 0
 *
 * // Step 2: Start measurement
 * rx_hcsr04_isr_start(11);
 *
 * // Step 3: Send trigger pulse
 * gpio_set_high(trigger_pin);
 * delay_us(10);
 * gpio_set_low(trigger_pin);
 *
 * // Step 4: Wait for completion (with timeout)
 * uint32_t duration_us;
 * rx_err_t err = rx_hcsr04_isr_get_duration(11, &duration_us);
 * if (err == k_rx_ok) {
 *     float distance_cm = duration_us / 58.0f;
 * }
 * @endcode
 *
 * @note This is an internal header (src/ not inc/)
 * @note ISR functions must match vector table naming (INT_IRQn)
 * @note Keep ISR execution time minimal (< 5µs)
 *
 * @see rx_hcsr04_icu.h Configure ICU before enabling ISRs
 * @see rx_hcsr04.c Main driver using this ISR layer
 * @see RX72N Manual Chapter 15 - ICU (Interrupt Controller Unit)
 *
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.2.0 (Issue #296)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Types
 * =============================================================================
 */

/**
 * @struct rx_hcsr04_irq_state_t
 * @brief Echo pulse state captured by ISR
 *
 * @details
 * Per-IRQ state structure holding timestamps and completion flags.
 * Written by ISR, read by application code. All fields must be volatile
 * because they are written in ISR context and read in task context.
 *
 * **State Machine:**
 * @verbatim
 *   [Idle] --(start())--> [Active, !complete]
 *                              ↓
 *                        Rising edge ISR
 *                         (capture start_us)
 *                              ↓
 *                        Falling edge ISR
 *                         (capture end_us)
 *                              ↓
 *                        [Active, complete]
 *                              ↓
 *                     (get_duration() reads + clears complete)
 *                              ↓
 *                           [Idle]
 * @endverbatim
 *
 * @note All fields declared volatile: written in ISR, read in task context
 * @note get_duration() clears complete after successful read
 */
typedef struct {
  volatile uint32_t start_us; /**< Rising edge timestamp (microseconds) - written in ISR */
  volatile uint32_t end_us;   /**< Falling edge timestamp (microseconds) - written in ISR */
  volatile bool     complete; /**< True if both edges captured - written in ISR */
  volatile bool     active;   /**< True if measurement in progress - written in ISR */
} rx_hcsr04_irq_state_t;

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Register HC-SR04 sensor for IRQ echo measurement
 *
 * @details
 * Maps an IRQ number to a sensor index for future use. This allows
 * the ISR to identify which sensor triggered the interrupt.
 *
 * @param[in] irq_num IRQ number (8-15)
 * @param[in] sensor_index Sensor array index (0-3)
 *                         - 0: Front-Left (IRQ11)
 *                         - 1: Front-Right (IRQ10)
 *                         - 2: Back-Left (IRQ9)
 *                         - 3: Back-Right (IRQ8)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Registration successful
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 15]
 * @retval k_rx_err_invalid_arg sensor_index == k_sensor_unused (0xFF)
 *
 * @pre IRQ configured via rx_hcsr04_icu_configure()
 * @pre sensor_index must be a valid sensor array index (< 0xFF)
 *
 * @post s_sensor_map[irq_num] = sensor_index
 * @post ISR can identify sensor on interrupt
 *
 * @note Call once during sensor initialization
 * @note Not thread-safe; call during single-threaded initialization only
 *
 * @par Example
 * @code
 * // Register all 4 sensors using named constants
 * rx_hcsr04_isr_register(k_rx_hcsr04_irq_11, 0);  // Sonar 0 Front-Left
 * rx_hcsr04_isr_register(k_rx_hcsr04_irq_10, 1);  // Sonar 1 Front-Right
 * rx_hcsr04_isr_register(k_rx_hcsr04_irq_9,  2);  // Sonar 2 Back-Left
 * rx_hcsr04_isr_register(k_rx_hcsr04_irq_8,  3);  // Sonar 3 Back-Right
 * @endcode
 *
 * @since Version 1.2.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_register(uint8_t irq_num, uint8_t sensor_index);

/**
 * @brief Get echo pulse duration from ISR state
 *
 * @details
 * Reads the captured timestamps and calculates pulse duration.
 * Returns error if measurement is not complete yet.
 *
 * @param[in] irq_num IRQ number (8-15)
 * @param[out] duration_us Pointer to store pulse duration (microseconds)
 *                         - Valid range: 150µs - 25000µs (2cm - 400cm)
 *                         - Must not be NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Duration available, written to *duration_us
 * @retval k_rx_err_timeout Measurement not complete yet
 * @retval k_rx_err_null_ptr duration_us pointer is NULL
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 15]
 *
 * @pre rx_hcsr04_isr_start() called before trigger pulse
 * @pre Both rising and falling edges captured by ISR
 *
 * @post duration_us contains echo pulse width in microseconds
 * @post State remains complete until next start() call
 *
 * @note Does NOT block; returns immediately with k_rx_err_timeout if not ready
 * @note Call repeatedly in a bounded loop (caller must enforce timeout)
 *
 * @par Example: Bounded Polling for Completion
 * @code
 * uint32_t start_time = get_time_us();
 * uint32_t duration_us;
 * rx_err_t err = k_rx_err_timeout;
 *
 * // Bounded loop - max 30000 iterations (NASA Rule 2)
 * for (uint32_t i = 0; i < 30000; i++) {
 *     err = rx_hcsr04_isr_get_duration(k_rx_hcsr04_irq_11, &duration_us);
 *     if (err == k_rx_ok) {
 *         break;  // Got result
 *     }
 *     if ((get_time_us() - start_time) > 30000) {
 *         break;  // 30ms timeout
 *     }
 * }
 * if (err == k_rx_ok) {
 *     float distance_cm = duration_us / 58.0f;
 * }
 * @endcode
 *
 * @since Version 1.2.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_get_duration(uint8_t irq_num, uint32_t* duration_us);

/**
 * @brief Start new echo measurement (call before trigger pulse)
 *
 * @details
 * Prepares ISR state for a new measurement. Clears completion flag
 * and sets active flag. Must be called before sending trigger pulse.
 *
 * @param[in] irq_num IRQ number (8-15)
 *
 * @pre ISR registered via rx_hcsr04_isr_register()
 * @pre No measurement currently active
 *
 * @post State.active = true
 * @post State.complete = false
 * @post Ready to capture rising edge
 *
 * @note Always call before sending trigger pulse
 * @note Do not call while previous measurement active
 *
 * @par Example: Typical Measurement Sequence
 * @code
 * // Step 1: Start measurement
 * rx_hcsr04_isr_start(11);
 *
 * // Step 2: Send trigger pulse
 * gpio_set_high(trigger_pin);
 * delay_us(10);
 * gpio_set_low(trigger_pin);
 *
 * // Step 3: Wait for completion
 * uint32_t duration_us;
 * while (rx_hcsr04_isr_get_duration(11, &duration_us) != k_rx_ok) {
 *     // Poll or yield
 * }
 * @endcode
 *
 * @since Version 1.2.0
 */
void rx_hcsr04_isr_start(uint8_t irq_num);

/* =============================================================================
 * ISR Functions (must match vector table naming)
 * =============================================================================
 */

/**
 * @brief ISR for IRQ8 (P00 - Sonar 3 Back-Right)
 *
 * @pre ICU configured for IRQ8 via rx_hcsr04_icu_configure()
 * @pre PIN P00 configured for IRQ function via rx_mpc_set_irq()
 * @post IR[72] flag cleared; echo edge timestamp captured if measurement active
 * @note Called by hardware on both rising and falling edges
 */
void INT_IRQ8(void);

/**
 * @brief ISR for IRQ9 (P01 - Sonar 2 Back-Left)
 *
 * @pre ICU configured for IRQ9 via rx_hcsr04_icu_configure()
 * @pre PIN P01 configured for IRQ function via rx_mpc_set_irq()
 * @post IR[73] flag cleared; echo edge timestamp captured if measurement active
 * @note Called by hardware on both rising and falling edges
 */
void INT_IRQ9(void);

/**
 * @brief ISR for IRQ10 (P02 - Sonar 1 Front-Right)
 *
 * @pre ICU configured for IRQ10 via rx_hcsr04_icu_configure()
 * @pre PIN P02 configured for IRQ function via rx_mpc_set_irq()
 * @post IR[74] flag cleared; echo edge timestamp captured if measurement active
 * @note Called by hardware on both rising and falling edges
 */
void INT_IRQ10(void);

/**
 * @brief ISR for IRQ11 (P03 - Sonar 0 Front-Left)
 *
 * @pre ICU configured for IRQ11 via rx_hcsr04_icu_configure()
 * @pre PIN P03 configured for IRQ function via rx_mpc_set_irq()
 * @post IR[75] flag cleared; echo edge timestamp captured if measurement active
 * @note Called by hardware on both rising and falling edges
 */
void INT_IRQ11(void);

#ifdef __cplusplus
}
#endif
