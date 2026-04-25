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
 * - ISR functions for IRQ8-11 (INT_IRQ8 through INT_IRQ11, for 4 HC-SR04 sensors)
 * - Per-IRQ state management (start/end timestamps, completion flag)
 * - Edge type detection (rising vs falling)
 * - Timestamp capture via hardware abstraction layer
 *
 * **Operation:**
 * 1. Application calls `rx_hcsr04_isr_start()` before trigger pulse
 * 2. ISR fires on rising edge -> captures start timestamp
 * 3. ISR fires on falling edge -> captures end timestamp, sets complete flag
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
 * @note Constants used below: k_hcsr04_irq_11, k_hcsr04_sensor_front_left,
 *       k_hcsr04_trigger_pulse_us from rx_hcsr04.h; s_hcsr04_us_per_cm from this header
 * @code
 * // Step 1: Register sensor with ISR (during init)
 * rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_11, k_hcsr04_sensor_front_left);  // IRQ11 -> Sensor 0
 *
 * // Step 2: Arm ISR BEFORE trigger (prevents missing rising edge)
 * rx_hcsr04_isr_start((uint8_t)k_hcsr04_irq_11);
 *
 * // Step 3: Send trigger pulse
 * gpio_set_high(trigger_pin);
 * delay_us(k_hcsr04_trigger_pulse_us);
 * gpio_set_low(trigger_pin);
 *
 * // Step 4: Wait for completion (with timeout)
 * uint32_t duration_us;
 * rx_err_t err = rx_hcsr04_isr_get_duration((uint8_t)k_hcsr04_irq_11, &duration_us);
 * if (err == k_rx_ok) {
 *     float distance_cm = (float)duration_us / s_hcsr04_us_per_cm;
 * }
 * @endcode
 *
 * @note This is an internal header (src/ not inc/)
 * @note ISR functions must match vector table naming (INT_IRQn)
 * @note Keep ISR execution time minimal (< 5us)
 *
 * @see rx_hcsr04_icu.h Configure ICU before enabling ISRs
 * @see rx_hcsr04.c Main driver using this ISR layer
 * @see RX72N Manual Chapter 15 - ICU (Interrupt Controller Unit)
 *
 * @author Locked, Inc.
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @see docs/sections/06_nasa_power_of_10.tex NASA Power of 10 rules applied in this module
 * @see docs/sections/01_nanopb_protocol.tex System architecture and design document
 *
 * @par NASA Power of 10 Compliance
 * - Rule 2: ISR is O(1); all loops in caller code have statically bounded iterations
 * - Rule 3: No dynamic memory; all state held in static arrays (s_irq_state, s_sensor_map)
 * - Rule 5: Minimum 2 preconditions and 2 postconditions per public function
 * - Rule 7: All return values checked; ISR clears IR flag unconditionally on entry
 * - Rule 8: All constants are C23 typed enums (isr_constants_t); no magic numbers
 *
 * @par SOLID Principles Adherence
 * - Single Responsibility: ISR edge capture only; ICU configuration in rx_hcsr04_icu.h,
 *                          driver orchestration in rx_hcsr04.c
 * - Dependency Inversion: Uses hcsr04_hal_get_time_us_isr() abstraction for timestamp capture
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "rx_hcsr04.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @var s_hcsr04_us_per_cm
 * @brief Microseconds per centimeter (roundtrip) for HC-SR04 distance conversion
 *
 * @details
 * The HC-SR04 echo pulse width encodes round-trip time-of-flight. At 20degC,
 * sound travels at 343 m/s. The round-trip conversion factor is:
 *
 *   2 cm / (343 m/s * 100 cm/m) = 58.31 us/cm ~ 58 us/cm
 *
 * This variable is replicated from `rx_hcsr04_timing_t::k_hcsr04_us_per_cm_roundtrip`
 * as a `static const float` for direct use in floating-point distance calculations
 * in code examples within this header. It is a translation-unit-local constant and
 * carries the `s_` prefix per the STAR naming convention for static variables.
 *
 * @note Each translation unit that includes this header gets its own copy (static linkage)
 * @warning Do not use for production distance calculations in rx_hcsr04.c; use the
 *          `k_hcsr04_us_per_cm_roundtrip` enum constant from rx_hcsr04.h instead
 *
 * @since Version 1.0.0
 */
[[maybe_unused]] static const float s_hcsr04_us_per_cm = 58.0F;

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
 * @startuml
 * [*] --> Idle
 * Idle --> Active : rx_hcsr04_isr_start() / active=true, complete=false
 * Active --> Active : Rising edge ISR / capture start_us
 * Active --> Complete : Falling edge ISR / capture end_us, complete=true, active=false
 * Complete --> Idle : rx_hcsr04_isr_get_duration() / returns duration, clears complete
 * @enduml
 *
 * @invariant active==true implies measurement is in progress (edges not yet fully captured)
 * @invariant complete==true implies both edges captured and duration is valid
 *
 * @code
 * // Typical usage sequence:
 * rx_hcsr04_isr_start(irq_num);             // Transition: Idle -> Active
 * // ISR fires on rising edge               // Transition: Active -> Active (start_us captured)
 * // ISR fires on falling edge              // Transition: Active -> Complete (end_us captured)
 * uint32_t duration_us;
 * rx_err_t err = rx_hcsr04_isr_get_duration(irq_num, &duration_us);
 * // On k_rx_ok: Transition Complete -> Idle
 * @endcode
 *
 * @see rx_hcsr04_isr_start() Transitions Idle -> Active
 * @see rx_hcsr04_isr_get_duration() Reads duration and transitions Complete -> Idle
 *
 * @note All fields declared volatile: written in ISR, read in task context
 * @note get_duration() clears complete after successful read
 *
 * @since Version 1.0.0
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
 * @param[in] irq_num      IRQ number (8-11 for P00-P03)
 * @param[in] sensor_index Sensor position index (rx_hcsr04_sensor_index_t)
 *                         - k_hcsr04_sensor_front_left  (0): IRQ11 / P03
 *                         - k_hcsr04_sensor_front_right (1): IRQ10 / P02
 *                         - k_hcsr04_sensor_back_left   (2): IRQ9  / P01
 *                         - k_hcsr04_sensor_back_right  (3): IRQ8  / P00
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Registration successful
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]; sensor_index >= k_hcsr04_sensor_count (4)
 *
 * @pre IRQ configured via rx_hcsr04_icu_configure()
 * @pre sensor_index must be a valid sensor array index (< k_hcsr04_sensor_count)
 *
 * @post s_sensor_map[irq_num] = sensor_index
 * @post ISR can identify sensor on interrupt
 *
 * @note Call once during sensor initialization
 * @note Not thread-safe; call during single-threaded initialization only
 *
 * @par Example
 * @code
 * // Register all 4 sensors using named sensor index constants
 * rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_11, k_hcsr04_sensor_front_left);
 * rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_10, k_hcsr04_sensor_front_right);
 * rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_9,  k_hcsr04_sensor_back_left);
 * rx_hcsr04_isr_register((uint8_t)k_hcsr04_irq_8,  k_hcsr04_sensor_back_right);
 * @endcode
 *
 * @see rx_hcsr04_isr_unregister() Cleanup counterpart for deinitialization
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_register(uint8_t                  irq_num,
                                              rx_hcsr04_sensor_index_t sensor_index);

/**
 * @brief Unregister HC-SR04 sensor from IRQ echo measurement
 *
 * @details
 * Clears the sensor mapping for the given IRQ number, restoring the slot
 * to the k_sensor_unused sentinel. Call during sensor deinitialization
 * after rx_hcsr04_icu_disable() to prevent stale ISR callbacks.
 *
 * @param[in] irq_num IRQ number (8-11) to unregister
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Unregistration successful
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]
 * @retval k_rx_err_invalid_state IRQ slot was not registered (already unset)
 *
 * @pre IRQ was previously registered via rx_hcsr04_isr_register()
 * @pre No measurement currently active on this IRQ
 *
 * @post s_sensor_map[irq_num] reset to k_sensor_unused
 * @post ISR will ignore subsequent interrupts on this IRQ
 *
 * @note Call during sensor deinitialization, after rx_hcsr04_icu_disable()
 *
 * @par Example: Sensor Deinitialization
 * @code
 * // Step 1: Disable ICU interrupt
 * rx_err_t err = rx_hcsr04_icu_disable((uint8_t)k_hcsr04_irq_11);
 * // Step 2: Unregister sensor from ISR handler
 * err = rx_hcsr04_isr_unregister((uint8_t)k_hcsr04_irq_11);
 * if (err != k_rx_ok) {
 *     rx_log_warn("SONAR", "Failed to unregister IRQ11");
 * }
 * @endcode
 *
 * @see rx_hcsr04_isr_register() Register sensor mapping
 * @see rx_hcsr04_deinit() Calls this during IRQ-mode cleanup
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_unregister(uint8_t irq_num);

/**
 * @brief Get echo pulse duration from ISR state
 *
 * @details
 * Reads the captured timestamps and calculates pulse duration.
 * Returns error if measurement is not complete yet.
 *
 * @param[in] irq_num IRQ number (8-11)
 * @param[out] duration_us Pointer to store pulse duration (microseconds)
 *                         - Valid range: 150us - 8700us (2cm - 150cm in IRQ mode)
 *                         - CMT2 16-bit wrap handled automatically
 *                         - Must not be NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Duration available, written to *duration_us
 * @retval k_rx_err_timeout Measurement not complete yet
 * @retval k_rx_err_null_ptr duration_us pointer is NULL
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]
 * @retval k_rx_err_range_check_failed Computed duration exceeds CMT2 wrap period (invalid)
 *
 * @pre rx_hcsr04_isr_start() called before trigger pulse
 * @pre Both rising and falling edges captured by ISR
 *
 * @post On k_rx_ok: *duration_us contains echo pulse width in microseconds
 * @post On k_rx_ok: complete flag cleared (ready for next measurement via start())
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
 * // Bounded loop - max k_hcsr04_echo_timeout_us iterations (NASA Rule 2)
 * for (uint32_t i = 0; i < k_hcsr04_echo_timeout_us; i++) {
 *     err = rx_hcsr04_isr_get_duration((uint8_t)k_hcsr04_irq_11, &duration_us);
 *     if (err == k_rx_ok) {
 *         break;  // Got result
 *     }
 *     if ((get_time_us() - start_time) > k_hcsr04_echo_timeout_us) {
 *         break;  // 30ms timeout
 *     }
 * }
 * if (err == k_rx_ok) {
 *     float distance_cm = (float)duration_us / s_hcsr04_us_per_cm;
 * }
 * @endcode
 *
 * @see rx_hcsr04_isr_start() Must be called before trigger pulse to arm ISR
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_get_duration(uint8_t irq_num, uint32_t* duration_us);

/**
 * @brief Start new echo measurement (call before trigger pulse)
 *
 * @details
 * Prepares ISR state for a new measurement. Validates that the IRQ number
 * is registered in the sensor map, then clears the completion flag and sets
 * the active flag. Must be called before sending trigger pulse.
 *
 * @param[in] irq_num IRQ number (k_hcsr04_irq_8 through k_hcsr04_irq_11)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok ISR armed successfully
 * @retval k_rx_err_invalid_arg irq_num not in valid range
 * @retval k_rx_err_invalid_state irq_num not registered via rx_hcsr04_isr_register()
 *
 * @pre ISR registered via rx_hcsr04_isr_register()
 * @pre No measurement currently active
 *
 * @post State.active = true (on k_rx_ok)
 * @post State.complete = false (on k_rx_ok)
 * @post Ready to capture rising edge (on k_rx_ok)
 *
 * @note Always call before sending trigger pulse
 * @note Do not call while previous measurement active
 *
 * @par Example: Typical Measurement Sequence
 * @code
 * // Step 1: Start measurement
 * rx_err_t err = rx_hcsr04_isr_start((uint8_t)k_hcsr04_irq_11);
 * if (err != k_rx_ok) {
 *     return err;
 * }
 *
 * // Step 2: Send trigger pulse
 * gpio_set_high(trigger_pin);
 * delay_us(k_hcsr04_trigger_pulse_us);  // 10us pulse
 * gpio_set_low(trigger_pin);
 *
 * // Step 3: Wait for completion (bounded loop - NASA Rule 2)
 * uint32_t duration_us;
 * err = k_rx_err_timeout;
 * for (uint32_t i = 0; i < k_hcsr04_echo_timeout_us; i++) {  // k_hcsr04_echo_timeout_us iter max (~30ms at 1us/iter)
 *     err = rx_hcsr04_isr_get_duration((uint8_t)k_hcsr04_irq_11, &duration_us);
 *     if (err == k_rx_ok) {
 *         break;  // Measurement complete
 *     }
 * }
 * if (err != k_rx_ok) {
 *     // Handle timeout
 * }
 * @endcode
 *
 * @see rx_hcsr04_isr_disarm() Disarm ISR on trigger failure after successful start
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_start(uint8_t irq_num);

/**
 * @brief Disarm ISR state machine (call on trigger failure after arm)
 *
 * @details
 * Clears the active flag in the per-IRQ state structure to cancel a
 * previously armed measurement. Must be called when rx_hcsr04_isr_start()
 * has succeeded but the subsequent trigger pulse fails, to prevent the ISR
 * state machine from remaining armed indefinitely.
 *
 * **Use Case:**
 * @code
 * err = rx_hcsr04_isr_start((uint8_t)handle->echo_irq);  // Arm ISR
 * if (err != k_rx_ok) { return err; }
 *
 * err = internal_send_trigger_pulse(handle);              // Send trigger
 * if (err != k_rx_ok) {
 *     // Trigger failed -- disarm ISR to restore consistent state
 *     (void)rx_hcsr04_isr_disarm((uint8_t)handle->echo_irq);
 *     return err;
 * }
 * @endcode
 *
 * @param[in] irq_num IRQ number (8-11) to disarm
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok ISR disarmed successfully
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]
 * @retval k_rx_err_invalid_state irq_num not registered via rx_hcsr04_isr_register()
 *
 * @pre rx_hcsr04_isr_start() called successfully for this IRQ
 * @pre irq_num registered via rx_hcsr04_isr_register() (sensor map entry != k_sensor_unused)
 *
 * @post s_irq_state[idx].active = false
 * @post s_irq_state[idx].complete = false
 * @post ISR will ignore subsequent interrupts on this IRQ
 *
 * @note Call only in the error path, after a successful rx_hcsr04_isr_start()
 * @note Ignores return value of disarm in error path (best-effort cleanup)
 *
 * @see rx_hcsr04_isr_start() Arm ISR state before trigger pulse
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_hcsr04_isr_disarm(uint8_t irq_num);

/* =============================================================================
 * ISR Functions (must match vector table naming)
 * =============================================================================
 */

/**
 * @brief ISR for IRQ8 (P00 - Sonar 3 Back-Right)
 *
 * @details
 * Hardware-triggered ISR on both edges of the HC-SR04 echo pulse from P00.
 * Delegates to internal_irq_handler(8). Clears IR[72], detects rising/falling
 * edge from PORT0 PIDR bit 0, and captures hcsr04_hal_get_time_us_isr() timestamp.
 *
 * @pre ICU configured for IRQ8 via rx_hcsr04_icu_configure()
 * @pre PIN P00 configured for IRQ function via rx_mpc_set_irq()
 *
 * @post IR[72] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[0] if measurement active
 *
 * @note Called by hardware on both rising and falling edges; execution time < 5us
 *
 * @see rx_hcsr04_isr_start() Arms ISR state before trigger pulse
 * @see rx_hcsr04_isr_get_duration() Reads captured timestamps
 *
 * @since Version 1.0.0
 */
void INT_IRQ8(void);

/**
 * @brief ISR for IRQ9 (P01 - Sonar 2 Back-Left)
 *
 * @details
 * Hardware-triggered ISR on both edges of the HC-SR04 echo pulse from P01.
 * Delegates to internal_irq_handler(9). Clears IR[73], detects rising/falling
 * edge from PORT0 PIDR bit 1, and captures hcsr04_hal_get_time_us_isr() timestamp.
 *
 * @pre ICU configured for IRQ9 via rx_hcsr04_icu_configure()
 * @pre PIN P01 configured for IRQ function via rx_mpc_set_irq()
 *
 * @post IR[73] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[1] if measurement active
 *
 * @note Called by hardware on both rising and falling edges; execution time < 5us
 *
 * @see rx_hcsr04_isr_start() Arms ISR state before trigger pulse
 * @see rx_hcsr04_isr_get_duration() Reads captured timestamps
 *
 * @since Version 1.0.0
 */
void INT_IRQ9(void);

/**
 * @brief ISR for IRQ10 (P02 - Sonar 1 Front-Right)
 *
 * @details
 * Hardware-triggered ISR on both edges of the HC-SR04 echo pulse from P02.
 * Delegates to internal_irq_handler(10). Clears IR[74], detects rising/falling
 * edge from PORT0 PIDR bit 2, and captures hcsr04_hal_get_time_us_isr() timestamp.
 *
 * @pre ICU configured for IRQ10 via rx_hcsr04_icu_configure()
 * @pre PIN P02 configured for IRQ function via rx_mpc_set_irq()
 *
 * @post IR[74] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[2] if measurement active
 *
 * @note Called by hardware on both rising and falling edges; execution time < 5us
 *
 * @see rx_hcsr04_isr_start() Arms ISR state before trigger pulse
 * @see rx_hcsr04_isr_get_duration() Reads captured timestamps
 *
 * @since Version 1.0.0
 */
void INT_IRQ10(void);

/**
 * @brief ISR for IRQ11 (P03 - Sonar 0 Front-Left)
 *
 * @details
 * Hardware-triggered ISR on both edges of the HC-SR04 echo pulse from P03.
 * Delegates to internal_irq_handler(11). Clears IR[75], detects rising/falling
 * edge from PORT0 PIDR bit 3, and captures hcsr04_hal_get_time_us_isr() timestamp.
 *
 * @pre ICU configured for IRQ11 via rx_hcsr04_icu_configure()
 * @pre PIN P03 configured for IRQ function via rx_mpc_set_irq()
 *
 * @post IR[75] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[3] if measurement active
 *
 * @note Called by hardware on both rising and falling edges; execution time < 5us
 *
 * @see rx_hcsr04_isr_start() Arms ISR state before trigger pulse
 * @see rx_hcsr04_isr_get_duration() Reads captured timestamps
 *
 * @since Version 1.0.0
 */
void INT_IRQ11(void);

#ifdef __cplusplus
}
#endif
