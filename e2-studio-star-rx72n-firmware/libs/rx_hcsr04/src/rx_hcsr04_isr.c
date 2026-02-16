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
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.2.0 (Issue #296)
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
 * @brief ISR configuration constants
 */
typedef enum : uint8_t {
  k_irq_min       = 8,                         /**< Minimum IRQ number (IRQ8 = P00) */
  k_irq_max       = 15,                        /**< Maximum IRQ number (IRQ15 = P07) */
  k_irq_count     = k_irq_max - k_irq_min + 1, /**< Number of IRQs (8) */
  k_vector_base   = 64,                        /**< IRQ vector base (IRQ0 = vector 64) */
  k_sensor_unused = 0xFF,                      /**< Unused sensor index marker */
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
 */
static rx_hcsr04_irq_state_t s_irq_state[k_irq_count];

/**
 * @brief Sensor index mapping (IRQ number → sensor index)
 * @details
 * Maps IRQ number to sensor array index for application callback.
 * Initialized to k_sensor_unused, set via rx_hcsr04_isr_register().
 */
static uint8_t s_sensor_map[16];

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
 * 2. Check if measurement is active (ignore spurious interrupts)
 * 3. Read GPIO pin state (HIGH = rising edge, LOW = falling edge)
 * 4. Rising edge: Capture start_us timestamp
 * 5. Falling edge: Capture end_us timestamp, set complete flag
 *
 * @param[in] irq_num IRQ number (8-15)
 *
 * @note Called from ISR context - keep execution time minimal
 * @note Assumes PORT0 pins (P00-P07) map to IRQ8-15
 */
static void internal_irq_handler(const uint8_t irq_num)
{
  /* Step 1: Clear interrupt flag */
  const uint8_t vector = k_vector_base + irq_num;
  icu()->ir[vector]    = 0;

  /* Step 2: Check if measurement is active */
  const uint8_t idx = irq_num - k_irq_min;
  if (!s_irq_state[idx].active) {
    return; /* Ignore spurious interrupt */
  }

  /* Step 3: Read GPIO state to determine edge type
   * P00-P07 map to IRQ8-15, so pin_num = irq_num - 8 */
  const uint8_t pin_num   = irq_num - k_irq_min;
  const bool    pin_state = (port0()->pidr >> pin_num) & 0x01;

  /* Step 4/5: Capture timestamp based on edge type */
  if (pin_state) {
    /* Rising edge - start of echo pulse */
    s_irq_state[idx].start_us = hcsr04_hal_get_time_us();
  } else {
    /* Falling edge - end of echo pulse */
    s_irq_state[idx].end_us   = hcsr04_hal_get_time_us();
    s_irq_state[idx].complete = true;
    s_irq_state[idx].active   = false;
  }
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

rx_err_t rx_hcsr04_isr_register(const uint8_t irq_num, const uint8_t sensor_index)
{
  /* Validate IRQ number */
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Store sensor mapping */
  s_sensor_map[irq_num] = sensor_index;

  return k_rx_ok;
}

void rx_hcsr04_isr_start(const uint8_t irq_num)
{
  /* Validate IRQ number (debug builds only) */
  if (irq_num < k_irq_min || irq_num > k_irq_max) {
    return; /* Silently ignore invalid IRQ in release builds */
  }

  const uint8_t idx         = irq_num - k_irq_min;
  s_irq_state[idx].active   = true;
  s_irq_state[idx].complete = false;
}

rx_err_t rx_hcsr04_isr_get_duration(const uint8_t irq_num, uint32_t* const duration_us)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(duration_us, "ISR", "duration_us is NULL");
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Check if measurement is complete */
  const uint8_t idx = irq_num - k_irq_min;
  if (!s_irq_state[idx].complete) {
    return k_rx_err_timeout; /* Not ready yet */
  }

  /* Calculate duration */
  *duration_us = s_irq_state[idx].end_us - s_irq_state[idx].start_us;

  return k_rx_ok;
}

/* =============================================================================
 * ISR Functions (must match vector table naming)
 * =============================================================================
 */

/**
 * @brief ISR for IRQ8 (P00 - Sonar 3 Back-Right)
 */
void INT_IRQ8(void)
{
  internal_irq_handler(8);
}

/**
 * @brief ISR for IRQ9 (P01 - Sonar 2 Back-Left)
 */
void INT_IRQ9(void)
{
  internal_irq_handler(9);
}

/**
 * @brief ISR for IRQ10 (P02 - Sonar 1 Front-Right)
 */
void INT_IRQ10(void)
{
  internal_irq_handler(10);
}

/**
 * @brief ISR for IRQ11 (P03 - Sonar 0 Front-Left)
 */
void INT_IRQ11(void)
{
  internal_irq_handler(11);
}
