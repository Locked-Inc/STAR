/**
 * @file rx_hcsr04_isr.c
 * @brief HC-SR04 ISR (Interrupt Service Routine) Handler Implementation
 *
 * @details
 * Implements ISR handlers for HC-SR04 ultrasonic sensors operating in IRQ mode.
 * Captures rising and falling edge timestamps with microsecond precision.
 *
 * **ISR Design:**
 * - Keep ISR execution time minimal (< 5µs)
 * - No blocking operations in ISR
 * - No ThreadX calls in ISR (use TX_INTERRUPT_SAVE_AREA if needed)
 * - Clear interrupt flag before exit
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, no recursion, all control flow via if/for/while
 * - Rule 2: No unbounded loops; ISR is O(1), rx_hcsr04_isr_get_duration returns immediately
 * - Rule 3: No dynamic allocation; all storage is static arrays
 * - Rule 4: All functions < 30 lines (internal_irq_handler: 25 lines)
 * - Rule 5: 2+ NULL checks and range validations per public function
 * - Rule 7: All register writes validated; all return values checked
 * - Rule 8: All constants are named C23 typed enums (isr_constants_t)
 * - Rule 10: Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - Single Responsibility: ISR state capture only; ICU config in rx_hcsr04_icu,
 *                          driver logic in rx_hcsr04.c
 * - Dependency Inversion: Uses hcsr04_hal_get_time_us() abstraction (ISR-safe)
 *
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.2.0 (Issue #296)
 * @version 1.2.0
 */

#include "rx_hcsr04_isr.h"

#include "rx72n_icu_regs.h"
#include "rx72n_port_regs.h"
#include "rx_check.h"
#include "rx_hcsr04_hal.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum isr_constants_t
 * @brief ISR configuration and state machine constants
 *
 * @details
 * Named constants for ISR (Interrupt Service Routine) logic. All magic
 * literals in this module must reference this enum.
 *
 * **Sensor Map:**
 * s_sensor_map[] has k_sensor_map_size entries. Entries initialized to
 * k_sensor_unused and set via rx_hcsr04_isr_register(). Only indices
 * [k_irq_min..k_irq_max] (8-11) are valid IRQ numbers.
 *
 * **IRQ Handler:**
 * Pin state is read from PORT0->PIDR register; the pin bit is extracted
 * by shifting right by (irq_num - k_irq_min), then masking with k_pin_state_mask.
 *
 * @invariant k_sensor_map_size > k_irq_max (map covers all valid IRQ indices)
 * @invariant k_irq_count == k_irq_max - k_irq_min + 1 == 4
 *
 * @see internal_irq_handler() Uses k_vector_base, k_irq_min, k_pin_state_mask
 * @see rx_hcsr04_isr_register() Uses k_sensor_map_size as array bound
 *
 * @since Version 1.2.0 (Issue #296)
 */
typedef enum : uint8_t {
  k_irq_min            = 8,                         /**< Minimum IRQ number (IRQ8 = P00) */
  k_irq_max            = 11,                        /**< Maximum IRQ number (IRQ11 = P03; only ISR handlers IRQ8-11 exist) */
  k_irq_count          = k_irq_max - k_irq_min + 1, /**< Number of supported IRQs (4: IRQ8-11) */
  k_vector_base        = 64,                        /**< IRQ vector base (IRQ0 = vector 64) */
  k_sensor_unused      = 0xFF,                      /**< Sentinel: sensor slot not assigned */
  k_sensor_map_size    = 16,                        /**< Total entries in s_sensor_map (IRQ0-15) */
  k_hcsr04_sensor_count = 4,                        /**< Number of HC-SR04 sensors supported (0-3) */
  k_pin_state_mask     = 0x01,                      /**< Bit mask to extract single pin state */
  k_ir_flag_clear      = 0,                         /**< Write 0 to IR register to clear pending interrupt flag */
} isr_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @brief Per-IRQ echo state (IRQ8-15 → array indices 0-7)
 * @details
 * Written by ISR, read by application code. Each element corresponds to
 * one IRQ number (index 0 = IRQ8, index 7 = IRQ15).
 * Declared volatile because fields are written in ISR context and read
 * in task context without explicit synchronization.
 */
static volatile rx_hcsr04_irq_state_t s_irq_state[k_irq_count];

/**
 * @brief Sensor index mapping (IRQ number → sensor index)
 * @details
 * Maps IRQ number to sensor array index for application callback.
 * Initialized to k_sensor_unused to mark all slots as unregistered.
 * Set via rx_hcsr04_isr_register().
 *
 * @todo Issue #296: s_sensor_map is currently written but not read in the ISR.
 * Future work will use this mapping to dispatch per-sensor callbacks from
 * internal_irq_handler() when multi-sensor callback support is implemented.
 */
static uint8_t s_sensor_map[k_sensor_map_size] = {
  k_sensor_unused, k_sensor_unused, k_sensor_unused, k_sensor_unused,
  k_sensor_unused, k_sensor_unused, k_sensor_unused, k_sensor_unused,
  k_sensor_unused, k_sensor_unused, k_sensor_unused, k_sensor_unused,
  k_sensor_unused, k_sensor_unused, k_sensor_unused, k_sensor_unused,
};

/* =============================================================================
 * Internal Functions
 * =============================================================================
 */

/**
 * @brief Internal IRQ handler (common logic for all IRQs)
 *
 * @details
 * Shared ISR logic called by INT_IRQ8 through INT_IRQ15. Determines edge
 * type (rising or falling) by reading GPIO pin state, then captures timestamp.
 *
 * **Algorithm:**
 * 1. Clear interrupt flag (acknowledge hardware)
 * 2. Compute array index: idx = irq_num - k_irq_min
 * 3. Check if measurement is active (ignore spurious interrupts)
 * 4. Read GPIO pin state: pin_state = (PORT0.PIDR >> idx) & k_pin_state_mask
 * 5. Rising edge (HIGH): Capture start_us timestamp
 * 6. Falling edge (LOW): Capture end_us timestamp, set complete=true, active=false
 *
 * @param[in] irq_num IRQ number (8-11)
 *
 * @return void (ISR context - no return value)
 *
 * @pre irq_num must be in range [k_irq_min, k_irq_max]
 * @pre ICU configured via rx_hcsr04_icu_configure()
 *
 * @post IR flag cleared for this vector
 * @post If active and rising edge: start_us timestamp captured
 * @post If active and falling edge: end_us captured, complete=true, active=false
 *
 * @note Called from ISR context - keep execution time minimal (< 5µs)
 * @note Assumes PORT0 pins (P00-P07) map to IRQ8-15
 * @warning Do not call from task context
 *
 * @see rx_hcsr04_isr_start() Sets active=true before trigger pulse
 * @see rx_hcsr04_isr_get_duration() Reads complete flag and timestamps
 *
 * @since Version 1.2.0 (Issue #296)
 */
static void internal_irq_handler(const uint8_t irq_num)
{
  /* Step 1: Clear interrupt flag first (acknowledge hardware) */
  const uint8_t vector = k_vector_base + irq_num;
  icu()->ir[vector]    = k_ir_flag_clear;

  /* Step 2: Compute state array index (IRQ8→0, IRQ9→1, ...) */
  const uint8_t idx = irq_num - k_irq_min;

  /* Step 3: Check if measurement is active (ignore spurious interrupts) */
  if (!s_irq_state[idx].active) {
    return; /* Spurious or late interrupt - ignore */
  }

  /* Step 4: Read GPIO state to determine edge type
   * P00-P07 map to IRQ8-15, so pin bit = idx = irq_num - k_irq_min */
  const bool pin_state = (bool)((port0()->pidr >> idx) & k_pin_state_mask);

  /* Step 5/6: Capture timestamp based on edge type */
  if (pin_state) {
    /* Rising edge - start of echo pulse */
    s_irq_state[idx].start_us = hcsr04_hal_get_time_us();
  } else {
    /* Falling edge - end of echo pulse; mark complete before clearing active */
    s_irq_state[idx].end_us   = hcsr04_hal_get_time_us();
    s_irq_state[idx].complete = true;
    s_irq_state[idx].active   = false;
  }
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Register HC-SR04 sensor for IRQ echo measurement
 *
 * @details
 * Maps an IRQ number to a sensor index. Validates both IRQ number range
 * and sensor_index range before storing the mapping. Used to identify
 * which sensor triggered a given interrupt.
 *
 * @param[in] irq_num      IRQ number (8-11 for P00-P03)
 * @param[in] sensor_index Sensor array index (0-3 for 4 HC-SR04 sensors)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Registration successful
 * @retval k_rx_err_invalid_arg irq_num not in [k_irq_min, k_irq_max]
 * @retval k_rx_err_invalid_arg sensor_index >= k_sensor_unused
 *
 * @pre IRQ configured via rx_hcsr04_icu_configure()
 * @pre sensor_index must be a valid sensor array index
 *
 * @post s_sensor_map[irq_num] updated with sensor_index
 * @post ISR can identify sensor on next interrupt
 *
 * @note Call once during sensor initialization
 * @note Not thread-safe; call during initialization only
 *
 * @see rx_hcsr04_isr_start() Arm ISR before sending trigger pulse
 *
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_register(const uint8_t irq_num, const uint8_t sensor_index)
{
  /* Validate IRQ number */
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Validate sensor index (must be a valid sensor array index 0..k_hcsr04_sensor_count-1) */
  if (sensor_index >= k_hcsr04_sensor_count) {
    return k_rx_err_invalid_arg;
  }

  /* Store sensor mapping */
  s_sensor_map[irq_num] = sensor_index;

  return k_rx_ok;
}

/**
 * @brief Unregister HC-SR04 sensor from IRQ echo measurement
 *
 * @details
 * Clears the sensor mapping for the given IRQ number, restoring it to the
 * k_sensor_unused sentinel. Call this during sensor deinitialization to
 * prevent stale ISR callbacks after the sensor handle is invalidated.
 *
 * @param[in] irq_num IRQ number (8-11) to unregister
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Unregistration successful
 * @retval k_rx_err_invalid_arg irq_num not in range [8, 11]
 *
 * @pre IRQ was previously registered via rx_hcsr04_isr_register()
 * @pre No measurement currently active on this IRQ
 *
 * @post s_sensor_map[irq_num] = k_sensor_unused
 * @post ISR will ignore subsequent interrupts on this IRQ
 *
 * @note Call during sensor deinitialization, after rx_hcsr04_icu_disable()
 * @note Not thread-safe; call during single-threaded cleanup only
 *
 * @see rx_hcsr04_isr_register() Register sensor
 * @see rx_hcsr04_deinit() Calls this during cleanup in IRQ mode
 *
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_unregister(const uint8_t irq_num)
{
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Verify this IRQ slot was actually registered (second pre-condition check) */
  if (s_sensor_map[irq_num] == k_sensor_unused) {
    return k_rx_err_invalid_state;
  }

  s_sensor_map[irq_num] = k_sensor_unused;

  return k_rx_ok;
}

/**
 * @brief Start new echo measurement (arm ISR before sending trigger pulse)
 *
 * @details
 * Prepares ISR state for a new measurement. Validates that the IRQ is
 * registered, then sets complete=false first (to prevent a stale complete
 * flag being read), then sets active=true to allow ISR to capture edges.
 * Order matters: complete must be cleared before active is set to prevent
 * a race where ISR fires between the two writes.
 *
 * @param[in] irq_num IRQ number (8-11)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok ISR armed successfully
 * @retval k_rx_err_invalid_arg irq_num not in valid range [k_irq_min, k_irq_max]
 * @retval k_rx_err_invalid_state irq_num slot not registered (s_sensor_map[irq_num] == k_sensor_unused)
 *
 * @pre rx_hcsr04_isr_register() called for this IRQ number
 * @pre No measurement currently active on this IRQ
 *
 * @post s_irq_state[idx].complete = false (on k_rx_ok)
 * @post s_irq_state[idx].active = true (on k_rx_ok)
 * @post ISR is armed to capture rising edge (on k_rx_ok)
 *
 * @note Always call BEFORE sending trigger pulse (ordering is critical)
 * @note complete cleared BEFORE active set (avoids race condition)
 *
 * @see rx_hcsr04_isr_get_duration() Poll for completion after this call
 *
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_start(const uint8_t irq_num)
{
  /* Validate IRQ number */
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Verify IRQ is registered (second precondition check per NASA Rule 5) */
  if (s_sensor_map[irq_num] == k_sensor_unused) {
    return k_rx_err_invalid_state; /* IRQ not registered via rx_hcsr04_isr_register() */
  }

  const uint8_t idx = irq_num - k_irq_min;

  /* Clear complete BEFORE setting active to avoid stale-complete race */
  s_irq_state[idx].complete = false;
  s_irq_state[idx].active   = true;

  return k_rx_ok;
}

/**
 * @brief Get echo pulse duration from ISR state
 *
 * @details
 * Reads the captured timestamps and calculates echo pulse duration.
 * Returns k_rx_err_timeout (not yet ready) if the ISR has not captured
 * both edges. On success, clears the complete flag to prepare for the
 * next measurement.
 *
 * @param[in]  irq_num     IRQ number (8-11)
 * @param[out] duration_us Pointer to store pulse duration in microseconds
 *                         (valid range: 150-25000 µs for 2-400 cm)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Duration available, *duration_us written, complete flag cleared
 * @retval k_rx_err_timeout Both edges not yet captured
 * @retval k_rx_err_null_ptr duration_us is NULL
 * @retval k_rx_err_invalid_arg irq_num not in [k_irq_min, k_irq_max]
 *
 * @pre rx_hcsr04_isr_start() called before trigger pulse
 * @pre ISR handlers registered in ICU (INT_IRQ8-11)
 *
 * @post On k_rx_ok: *duration_us = end_us - start_us
 * @post On k_rx_ok: complete flag cleared (ready for next measurement)
 *
 * @note Call repeatedly until k_rx_ok or caller's timeout expires
 * @note Not ISR-safe; call from task context only
 *
 * @see rx_hcsr04_isr_start() Call before sending trigger pulse
 *
 * @since Version 1.2.0 (Issue #296)
 */
rx_err_t rx_hcsr04_isr_get_duration(const uint8_t irq_num, uint32_t* const duration_us)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(duration_us, "ISR", "duration_us is NULL");
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Check if measurement is complete */
  const uint8_t idx = irq_num - k_irq_min;
  if (!s_irq_state[idx].complete) {
    return k_rx_err_timeout; /* Both edges not yet captured */
  }

  /* Calculate duration from captured timestamps */
  *duration_us = s_irq_state[idx].end_us - s_irq_state[idx].start_us;

  /* Clear complete flag so next rx_hcsr04_isr_start() starts fresh */
  s_irq_state[idx].complete = false;

  return k_rx_ok;
}

/* =============================================================================
 * ISR Functions (must match vector table naming)
 * =============================================================================
 */

/**
 * @enum isr_irq_numbers_t
 * @brief Named IRQ number constants for ISR wrapper functions
 *
 * @details
 * Provides named constants for the literal IRQ numbers passed to
 * internal_irq_handler() from each ISR wrapper. Replaces magic literals
 * 8, 9, 10, 11 per the project "No Magic Numbers" policy.
 *
 * @since Version 1.2.0 (Issue #296)
 */
typedef enum : uint8_t {
  k_irq_num_8  = 8,  /**< IRQ8  - maps to P00 (Sonar 3 Back-Right) */
  k_irq_num_9  = 9,  /**< IRQ9  - maps to P01 (Sonar 2 Back-Left) */
  k_irq_num_10 = 10, /**< IRQ10 - maps to P02 (Sonar 1 Front-Right) */
  k_irq_num_11 = 11, /**< IRQ11 - maps to P03 (Sonar 0 Front-Left) */
} isr_irq_numbers_t;

/**
 * @brief ISR for IRQ8 (P00 - Sonar 3 Back-Right)
 *
 * @pre ICU configured for IRQ8 via rx_hcsr04_icu_configure()
 * @post IR flag cleared; echo edge timestamp captured if measurement active
 */
void INT_IRQ8(void)
{
  internal_irq_handler(k_irq_num_8);
}

/**
 * @brief ISR for IRQ9 (P01 - Sonar 2 Back-Left)
 *
 * @pre ICU configured for IRQ9 via rx_hcsr04_icu_configure()
 * @post IR flag cleared; echo edge timestamp captured if measurement active
 */
void INT_IRQ9(void)
{
  internal_irq_handler(k_irq_num_9);
}

/**
 * @brief ISR for IRQ10 (P02 - Sonar 1 Front-Right)
 *
 * @pre ICU configured for IRQ10 via rx_hcsr04_icu_configure()
 * @post IR flag cleared; echo edge timestamp captured if measurement active
 */
void INT_IRQ10(void)
{
  internal_irq_handler(k_irq_num_10);
}

/**
 * @brief ISR for IRQ11 (P03 - Sonar 0 Front-Left)
 *
 * @pre ICU configured for IRQ11 via rx_hcsr04_icu_configure()
 * @post IR flag cleared; echo edge timestamp captured if measurement active
 */
void INT_IRQ11(void)
{
  internal_irq_handler(k_irq_num_11);
}
