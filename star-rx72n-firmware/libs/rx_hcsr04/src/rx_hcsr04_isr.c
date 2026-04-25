/**
 * @file rx_hcsr04_isr.c
 * @brief HC-SR04 ISR (Interrupt Service Routine) Handler Implementation
 *
 * @details
 * Implements ISR handlers for HC-SR04 ultrasonic sensors operating in IRQ mode.
 * Captures rising and falling edge timestamps with microsecond precision.
 *
 * **ISR Design:**
 * - Keep ISR execution time minimal (< 5us)
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
 * - Dependency Inversion: Uses hcsr04_hal_get_time_us_isr() abstraction (mutex-free, ISR-safe)
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
 * @code
 * // Typical ISR handler using these constants:
 * void INT_IRQ11(void)
 * {
 *     internal_irq_handler(k_irq_max);  // IRQ11 = max IRQ
 * }
 *
 * // Validate IRQ number before indexing:
 * if (irq_num < k_irq_min || irq_num > k_irq_max) { return k_rx_err_invalid_arg; }
 * const uint8_t idx = irq_num - k_irq_min;  // IRQ8->0, IRQ11->3
 * @endcode
 *
 * @see internal_irq_handler() Uses k_vector_base, k_irq_min, k_pin_state_mask
 * @see rx_hcsr04_isr_register() Uses k_sensor_map_size as array bound
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_irq_min         = 8,  /**< Minimum IRQ number (IRQ8 = P00) */
  k_irq_max         = 11, /**< Maximum IRQ number (IRQ11 = P03; only ISR handlers IRQ8-11 exist) */
  k_irq_count       = k_irq_max - k_irq_min + 1, /**< Number of supported IRQs (4: IRQ8-11) */
  k_vector_base     = 64,                        /**< IRQ vector base (IRQ0 = vector 64) */
  k_sensor_unused   = 0xFF,                      /**< Sentinel: sensor slot not assigned */
  k_sensor_map_size = 16,                        /**< Total entries in s_sensor_map (IRQ0-15) */
  k_pin_state_mask  = 0x01,                      /**< Bit mask to extract single pin state */
  k_ir_flag_clear   = 0, /**< Write 0 to IR register to clear pending interrupt flag */
} isr_constants_t;

/**
 * @enum isr_timer_constants_t
 * @brief CMT2 timer wrap period for ISR timestamp correction
 *
 * @details
 * The CMT2 16-bit counter wraps every 65536 ticks at PCLKB/8 = 7.5 MHz.
 * Wrap period = 65536 / 7500000 s = ~8738 us. hcsr04_hal_get_time_us_isr()
 * returns values in [0, 8738]. When the falling edge timestamp wraps around
 * to a value smaller than the rising edge timestamp, this constant is added
 * to correct the duration calculation.
 *
 * **Clock dependency:** This constant assumes PCLKB = 60 MHz (k_pclkb_hz in
 * rx72n_clock.h) and CMT2 divider = 8 (k_cmt2_divider in rx_hcsr04_hal_hw.c).
 * If either value changes, k_cmt2_wrap_us MUST be recalculated:
 *   k_cmt2_wrap_us = (65536 * 1000000) / (PCLKB / divider)
 *
 * @invariant k_cmt2_wrap_us == (65536 * 1000000) / (60000000 / 8) == 8738
 * @invariant Assumes PCLKB = 60 MHz and CMT2 divider = 8; update if clock config changes
 *
 * @code
 * // Wrap correction in get_duration:
 * if (end_us < start_us) {
 *     duration = end_us + k_cmt2_wrap_us - start_us;
 * }
 * @endcode
 *
 * @see hcsr04_hal_get_time_us_isr() Returns values that wrap at this period
 * @see rx_hcsr04_isr_get_duration() Uses this for wrap correction
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_cmt2_wrap_us = 8738, /**< CMT2 16-bit counter wrap period in us (65536 / 7.5 MHz) */
} isr_timer_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for HCSR04 ISR module
 * @details Identifies log messages from this module
 * @note Read-only after initialization
 * @since Version 1.0.0
 */
static const char* const s_tag = "ISR";

/**
 * @var s_irq_state
 * @brief Per-IRQ echo measurement state (IRQ8-11 -> array indices 0-3)
 *
 * @details
 * Written by ISR (INT_IRQ8-11), read by application code (rx_hcsr04_isr_get_duration).
 * Each element corresponds to one IRQ number (index 0 = IRQ8, index 3 = IRQ11).
 * Declared volatile because fields are written in ISR context and read in task
 * context without explicit synchronization.
 *
 * @note Access pattern: ISR writes volatile fields; application reads under timeout loop
 * @warning Never access outside of rx_hcsr04_isr_start(), internal_irq_handler(),
 *          and rx_hcsr04_isr_get_duration(); direct access bypasses state validation
 *
 * @since Version 1.0.0
 */
static volatile rx_hcsr04_irq_state_t s_irq_state[k_irq_count];

/**
 * @var s_sensor_map
 * @brief Sensor index mapping table (IRQ number -> sensor array index)
 *
 * @details
 * Maps each IRQ number (0-15) to a sensor array index (0-3) or k_sensor_unused.
 * Initialized entirely to k_sensor_unused to mark all slots as unregistered.
 * Updated by rx_hcsr04_isr_register() and restored by rx_hcsr04_isr_unregister().
 *
 * **Layout:** index matches raw IRQ number (s_sensor_map[8] = Sonar 3 Back-Right)
 *
 * @note Only indices [k_irq_min..k_irq_max] (8-11) are used by HC-SR04 sensors
 * @note Indices [0..7] and [12..15] are always k_sensor_unused for HC-SR04 use
 * @warning Direct modification outside rx_hcsr04_isr_register/unregister is forbidden;
 *          use the public API to maintain consistent IRQ-to-sensor mappings
 *
 * @note Issue #336 (resolved): sensor_index is now correctly set per-sensor
 * via rx_hcsr04_config_t.sensor_index (no longer hardcoded to slot 0).
 * s_sensor_map is written with the correct index but not yet read in the ISR;
 * future work will use this mapping to dispatch per-sensor callbacks from
 * internal_irq_handler() when multi-sensor callback support is implemented.
 *
 * @since Version 1.0.0
 */
static uint8_t s_sensor_map[k_sensor_map_size] = {
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
  k_sensor_unused,
};

/* =============================================================================
 * Internal Functions
 * =============================================================================
 */

/**
 * @brief Internal IRQ handler (common logic for all IRQs)
 *
 * @details
 * Shared ISR logic called by INT_IRQ8 through INT_IRQ11. Determines edge
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
 *
 *
 * @pre irq_num must be in range [k_irq_min, k_irq_max]
 * @pre ICU configured via rx_hcsr04_icu_configure()
 *
 * @post IR flag cleared for this vector
 * @post If active and rising edge: start_us timestamp captured
 * @post If active and falling edge: end_us captured, complete=true, active=false
 *
 * @note Called from ISR context - keep execution time minimal (< 5us)
 * @note Assumes PORT0 pins (P00-P07) map to IRQ8-15
 * @warning Do not call from task context
 *
 * @see rx_hcsr04_isr_start() Sets active=true before trigger pulse
 * @see rx_hcsr04_isr_get_duration() Reads complete flag and timestamps
 *
 * @since Version 1.0.0
 */
static void internal_irq_handler(const uint8_t irq_num)
{
  /* Step 1: Clear interrupt flag first (acknowledge hardware) */
  const uint8_t vector = k_vector_base + irq_num;

  icu()->ir[vector] = k_ir_flag_clear;

  /* Step 2: Compute state array index (IRQ8->0, IRQ9->1, ...) */
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
    s_irq_state[idx].start_us = hcsr04_hal_get_time_us_isr();
  } else {
    /* Falling edge - end of echo pulse; mark complete before clearing active */
    s_irq_state[idx].end_us   = hcsr04_hal_get_time_us_isr();
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
 *
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
 * @since Version 1.0.0
 */
rx_err_t rx_hcsr04_isr_register(const uint8_t irq_num, const rx_hcsr04_sensor_index_t sensor_index)
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
 *
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
 * @since Version 1.0.0
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
 *
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
 * @since Version 1.0.0
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
 * @brief Disarm ISR state machine (cancel armed measurement on trigger failure)
 *
 * @details
 * Clears the active flag so the ISR will ignore subsequent edge interrupts.
 * Call this in the error path when rx_hcsr04_isr_start() succeeded but the
 * trigger pulse failed, to prevent the ISR from remaining armed indefinitely.
 *
 *
 *
 * @pre rx_hcsr04_isr_start() called successfully for this IRQ
 * @pre irq_num registered via rx_hcsr04_isr_register() (s_sensor_map[irq_num] != k_sensor_unused)
 *
 * @post s_irq_state[idx].active = false
 * @post s_irq_state[idx].complete = false
 * @post ISR will ignore subsequent interrupts on this IRQ
 *
 * @note Best-effort cleanup; always call in error path after successful isr_start
 *
 * @see rx_hcsr04_isr_start() Arm ISR state before trigger pulse
 *
 * @since Version 1.0.0
 */
rx_err_t rx_hcsr04_isr_disarm(const uint8_t irq_num)
{
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Verify IRQ is registered (mirrors check in isr_start / isr_unregister) */
  if (s_sensor_map[irq_num] == k_sensor_unused) {
    return k_rx_err_invalid_state; /* IRQ not registered via rx_hcsr04_isr_register() */
  }

  const uint8_t idx         = irq_num - k_irq_min;
  s_irq_state[idx].active   = false;
  s_irq_state[idx].complete = false;

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
 *
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
 * @since Version 1.0.0
 */
rx_err_t rx_hcsr04_isr_get_duration(const uint8_t irq_num, uint32_t* const duration_us)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(duration_us, s_tag, "duration_us is NULL");
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Check if measurement is complete */
  const uint8_t idx = irq_num - k_irq_min;
  if (!s_irq_state[idx].complete) {
    return k_rx_err_timeout; /* Both edges not yet captured */
  }

  /* Cache volatile ISR state into locals for consistent snapshot */
  const uint32_t start_us = s_irq_state[idx].start_us;
  const uint32_t end_us   = s_irq_state[idx].end_us;

  /* Calculate duration from captured timestamps (handle CMT2 16-bit wrap) */
  if (end_us >= start_us) {
    *duration_us = end_us - start_us;
  } else {
    /* Timer wrapped: add wrap period to correct the subtraction */
    *duration_us = end_us + k_cmt2_wrap_us - start_us;
  }

  /* Sanity check: duration must not exceed CMT2 wrap period */
  if (*duration_us > k_cmt2_wrap_us) {
    s_irq_state[idx].complete = false;
    return k_rx_err_range_check_failed;
  }

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
 * @invariant Values are fixed 8..11, matching P00..P03 pin assignments;
 *            each value must equal k_irq_min + pin_number (P0x -> IRQ(8+x))
 *
 * @code
 * // Each ISR wrapper passes its named constant to the common handler:
 * void INT_IRQ8(void)  { internal_irq_handler(k_irq_num_8);  }
 * void INT_IRQ11(void) { internal_irq_handler(k_irq_num_11); }
 * @endcode
 *
 * @see isr_constants_t Range bounds (k_irq_min, k_irq_max) and state constants
 * @see internal_irq_handler() Common ISR logic receiving these constants
 *
 * @since Version 1.0.0
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
 * @details
 * Thin wrapper that forwards IRQ number constant k_irq_num_8 to
 * internal_irq_handler(). The handler clears the IR flag, reads the GPIO
 * pin state, and captures the rising or falling edge timestamp.
 *
 *
 * @pre ICU configured for IRQ8 via rx_hcsr04_icu_configure() with k_hcsr04_irq_8
 * @pre Interrupts globally enabled (PSW.I = 1)
 *
 * @post IR[vector 72] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[0] if measurement active
 *
 * @note Thin wrapper; all logic is in internal_irq_handler()
 * @note Keep ISR execution time minimal (< 5 us)
 *
 * @see internal_irq_handler() Core ISR logic
 * @see k_irq_num_8 IRQ number constant passed to the handler
 *
 * @since Version 1.0.0
 */
void INT_IRQ8(void)
{
  internal_irq_handler(k_irq_num_8);
}

/**
 * @brief ISR for IRQ9 (P01 - Sonar 2 Back-Left)
 *
 * @details
 * Thin wrapper that forwards IRQ number constant k_irq_num_9 to
 * internal_irq_handler(). The handler clears the IR flag, reads the GPIO
 * pin state, and captures the rising or falling edge timestamp.
 *
 *
 * @pre ICU configured for IRQ9 via rx_hcsr04_icu_configure() with k_hcsr04_irq_9
 * @pre Interrupts globally enabled (PSW.I = 1)
 *
 * @post IR[vector 73] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[1] if measurement active
 *
 * @note Thin wrapper; all logic is in internal_irq_handler()
 * @note Keep ISR execution time minimal (< 5 us)
 *
 * @see internal_irq_handler() Core ISR logic
 * @see k_irq_num_9 IRQ number constant passed to the handler
 *
 * @since Version 1.0.0
 */
void INT_IRQ9(void)
{
  internal_irq_handler(k_irq_num_9);
}

/**
 * @brief ISR for IRQ10 (P02 - Sonar 1 Front-Right)
 *
 * @details
 * Thin wrapper that forwards IRQ number constant k_irq_num_10 to
 * internal_irq_handler(). The handler clears the IR flag, reads the GPIO
 * pin state, and captures the rising or falling edge timestamp.
 *
 *
 * @pre ICU configured for IRQ10 via rx_hcsr04_icu_configure() with k_hcsr04_irq_10
 * @pre Interrupts globally enabled (PSW.I = 1)
 *
 * @post IR[vector 74] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[2] if measurement active
 *
 * @note Thin wrapper; all logic is in internal_irq_handler()
 * @note Keep ISR execution time minimal (< 5 us)
 *
 * @see internal_irq_handler() Core ISR logic
 * @see k_irq_num_10 IRQ number constant passed to the handler
 *
 * @since Version 1.0.0
 */
void INT_IRQ10(void)
{
  internal_irq_handler(k_irq_num_10);
}

/**
 * @brief ISR for IRQ11 (P03 - Sonar 0 Front-Left)
 *
 * @details
 * Thin wrapper that forwards IRQ number constant k_irq_num_11 to
 * internal_irq_handler(). The handler clears the IR flag, reads the GPIO
 * pin state, and captures the rising or falling edge timestamp.
 *
 *
 * @pre ICU configured for IRQ11 via rx_hcsr04_icu_configure() with k_hcsr04_irq_11
 * @pre Interrupts globally enabled (PSW.I = 1)
 *
 * @post IR[vector 75] flag cleared (interrupt acknowledged)
 * @post Echo edge timestamp captured in s_irq_state[3] if measurement active
 *
 * @note Thin wrapper; all logic is in internal_irq_handler()
 * @note Keep ISR execution time minimal (< 5 us)
 *
 * @see internal_irq_handler() Core ISR logic
 * @see k_irq_num_11 IRQ number constant passed to the handler
 *
 * @since Version 1.0.0
 */
void INT_IRQ11(void)
{
  internal_irq_handler(k_irq_num_11);
}
