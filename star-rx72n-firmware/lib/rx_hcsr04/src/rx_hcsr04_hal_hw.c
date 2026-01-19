/* lib/rx_hcsr04/src/rx_hcsr04_hal_hw.c */

/**
 * @file rx_hcsr04_hal_hw.c
 * @brief HC-SR04 HAL Implementation for RX72N Hardware
 *
 * @details
 * Real hardware implementation of the HC-SR04 HAL interface for RX72N.
 * Uses actual GPIO and timer peripherals for production firmware.
 * Uses CMT2 for microsecond timing operations.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdbool.h>

#include "hardware.h"
#include "rx72n_clock.h"
#include "rx72n_cmt_regs.h"
#include "rx72n_system_regs.h"
#include "rx_check.h"
#include "rx_hcsr04_hal.h"
#include "rx_log.h"
#include "tx_api.h"

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

rx_err_t hcsr04_hal_gpio_set_output(rx_port_pin_t pin)
{
  uint8_t port    = 0;
  uint8_t pin_num = 0;

  if ((pin < k_rx_p0_0) || (pin > k_rx_pj_7)) {
    return k_rx_err_invalid_arg;
  }

  port    = rx_port_from_pin(pin);
  pin_num = rx_pin_from_pin(pin);

  /* Validate port range (0x00 - k_rx_port_j=0x13) */
  if ((port < k_rx_port_0) || (port > k_rx_port_j)) {
    return k_rx_err_invalid_arg;
  }

  /* Validate pin range (0 - k_rx_pin_max=7) */
  if ((pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  return gpio_set_output(pin);
}

rx_err_t hcsr04_hal_gpio_set_input(rx_port_pin_t pin)
{
  uint8_t port    = 0;
  uint8_t pin_num = 0;

  if ((pin < k_rx_p0_0) || (pin > k_rx_pj_7)) {
    return k_rx_err_invalid_arg;
  }

  port    = rx_port_from_pin(pin);
  pin_num = rx_pin_from_pin(pin);

  if ((port < k_rx_port_0) || (port > k_rx_port_j)) {
    return k_rx_err_invalid_arg;
  }

  if ((pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  return gpio_set_input(pin);
}

rx_err_t hcsr04_hal_gpio_write_high(rx_port_pin_t pin)
{
  uint8_t port    = 0;
  uint8_t pin_num = 0;

  if ((pin < k_rx_p0_0) || (pin > k_rx_pj_7)) {
    return k_rx_err_invalid_arg;
  }

  port    = rx_port_from_pin(pin);
  pin_num = rx_pin_from_pin(pin);

  if ((port < k_rx_port_0) || (port > k_rx_port_j)) {
    return k_rx_err_invalid_arg;
  }

  if ((pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  return gpio_write_high(pin);
}

rx_err_t hcsr04_hal_gpio_write_low(rx_port_pin_t pin)
{
  uint8_t port    = 0;
  uint8_t pin_num = 0;

  if ((pin < k_rx_p0_0) || (pin > k_rx_pj_7)) {
    return k_rx_err_invalid_arg;
  }

  port    = rx_port_from_pin(pin);
  pin_num = rx_pin_from_pin(pin);

  if ((port < k_rx_port_0) || (port > k_rx_port_j)) {
    return k_rx_err_invalid_arg;
  }

  if ((pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  return gpio_write_low(pin);
}

rx_err_t hcsr04_hal_gpio_read(rx_port_pin_t pin, bool* value)
{
  uint8_t port    = 0;
  uint8_t pin_num = 0;

  if (value == NULL) {
    return k_rx_err_null_ptr;
  }

  if ((pin < k_rx_p0_0) || (pin > k_rx_pj_7)) {
    return k_rx_err_invalid_arg;
  }

  port    = rx_port_from_pin(pin);
  pin_num = rx_pin_from_pin(pin);

  if ((port < k_rx_port_0) || (port > k_rx_port_j)) {
    return k_rx_err_invalid_arg;
  }

  if ((pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  return gpio_read(pin, value);
}

rx_err_t hcsr04_hal_gpio_deinit(rx_port_pin_t pin)
{
  uint8_t port    = 0;
  uint8_t pin_num = 0;

  if ((pin < k_rx_p0_0) || (pin > k_rx_pj_7)) {
    return k_rx_err_invalid_arg;
  }

  port    = rx_port_from_pin(pin);
  pin_num = rx_pin_from_pin(pin);

  if ((port < k_rx_port_0) || (port > k_rx_port_j)) {
    return k_rx_err_invalid_arg;
  }

  if ((pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  /*
   * GPIO_DEINIT is intentionally a no-op: the RX72N GPIO HAL does not provide
   * pin deallocation. GPIO pins are static resources allocated at init time.
   */
  return k_rx_ok;
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

/**
 * @brief CMT2 timer constants for microsecond timing
 *
 * CMT2 Configuration:
 * - PCLKB = 60 MHz (from rx72n_clock.h)
 * - Divider = 8 (fastest divider for high resolution)
 * - Timer frequency = 60 MHz / 8 = 7.5 MHz
 * - Timer period = 1 / 7.5 MHz = 133.33 ns per tick
 * - Ticks per microsecond = 7.5 ticks/us
 */
typedef enum : uint32_t {
  k_cmt2_divider           = 8,       /**< CMT2 clock divider */
  k_cmt2_divider_bits      = 0x0000,  /**< CKS[1:0] = 00 for /8 divider */
  k_timer_counter_max      = 0xFFFF,  /**< 16-bit counter maximum */
  k_timer_counter_bits     = 16,      /**< CMT2 counter width in bits */
  k_us_per_second          = 1000000, /**< Microseconds per second */
  k_timer_rounding         = 500000,  /**< Rounding factor for integer division */
  k_max_delay_iterations   = 100,     /**< Safety guard: max loop iterations */
  k_max_delay_ticks        = k_timer_counter_max * k_max_delay_iterations, /**< Max ticks */
  k_no_delay               = 0,                          /**< No delay requested */
  k_min_ticks              = 1,                          /**< Minimum ticks to wait */
  k_cmstr1_cmt2_enable_bit = k_rx72n_cmstr1_cmt2_enable, /**< CMSTR1 bit 0 enables CMT2 */
  k_counter_reset          = 0,                          /**< Counter reset value */
} cmt2_timing_constants_t;

static bool     s_cmt2_initialized = false;
static TX_MUTEX s_time_mutex;
static bool     s_time_mutex_initialized = false;

/**
 * @brief Initialize timing mutex safely
 */
static rx_err_t internal_time_mutex_init(void)
{
  UINT status = TX_SUCCESS;

  if (s_time_mutex_initialized) {
    return k_rx_ok;
  }

  status = tx_mutex_create(&s_time_mutex, "TimeMutex", TX_NO_INHERIT);
  if (status == TX_SUCCESS) {
    s_time_mutex_initialized = true;
    return k_rx_ok;
  }

  return k_rx_err_hw_init_failed;
}

/**
 * @brief Initialize CMT2 timer for microsecond timing
 */
static void internal_cmt2_init(void)
{
  volatile uint16_t* cmstr1 = NULL;

  if (s_cmt2_initialized) {
    return;
  }

  /* Enable CMT2 module (MSTPCRA bit 14 = CMT2/3) */
  system_regs()->mstpcra &= ~(1U << k_mstpa_cmt23);

  /* Get CMSTR1 register (controls CMT2/CMT3) */
  cmstr1 = &(cmt_ctrl()->cmstr1);

  /* Stop CMT2 (bit 0 of CMSTR1) */
  *cmstr1 &= ~k_cmstr1_cmt2_enable_bit;

  /* Configure CMT2: PCLK/8, no interrupt */
  cmt2()->cmcr = k_cmt2_divider_bits;

  /* Reset counter */
  cmt2()->cmcnt = k_counter_reset;

  /* Set compare match to maximum (free-running) */
  cmt2()->cmcor = k_timer_counter_max;

  /* Start CMT2 (bit 0 of CMSTR1) */
  *cmstr1 |= k_cmstr1_cmt2_enable_bit;

  s_cmt2_initialized = true;
}

void hcsr04_hal_delay_us(uint32_t us)
{
  uint32_t timer_hz        = 0;
  uint64_t ticks           = 0;
  uint16_t start           = 0;
  uint32_t wait_ticks      = 0;
  uint32_t iteration_count = 0;

  if (us == k_no_delay) {
    return;
  }

  internal_cmt2_init();

  timer_hz = k_pclkb_hz / k_cmt2_divider;
  ticks    = ((uint64_t)us * (uint64_t)timer_hz + k_timer_rounding) / k_us_per_second;
  if (ticks < k_min_ticks) {
    ticks = k_min_ticks;
  }

#ifdef UNIT_TEST
  RX_ASSERT(ticks <= k_max_delay_ticks, "HCSR04 delay exceeds max ticks");
#endif
  if (ticks > k_max_delay_ticks) {
    /* Exceeding k_max_delay_ticks is a no-op in release builds to avoid unbounded delays. */
    return;
  }

  while (ticks > 0 && iteration_count < k_max_delay_iterations) {
    wait_ticks = (ticks > k_timer_counter_max) ? k_timer_counter_max : (uint32_t)ticks;
    start      = cmt2()->cmcnt;
    while ((uint16_t)(cmt2()->cmcnt - start) < wait_ticks) {
      __asm__ volatile("nop");
    }
    ticks -= wait_ticks;
    iteration_count++;
  }
}

uint32_t hcsr04_hal_get_time_us(void)
{
  static uint32_t overflow_count  = 0;
  static uint16_t last_counter    = 0;
  uint16_t        current_counter = 0;
  uint32_t        timer_hz        = 0;
  uint64_t        total_ticks     = 0;
  uint32_t        result          = 0;
  UINT            mutex_status    = TX_SUCCESS;

  internal_cmt2_init();

  if (internal_time_mutex_init() != k_rx_ok) {
    timer_hz = k_pclkb_hz / k_cmt2_divider;
    return (uint32_t)((cmt2()->cmcnt * k_us_per_second) / timer_hz);
  }

  /* Protect static variables from concurrent access */
  mutex_status = tx_mutex_get(&s_time_mutex, TX_WAIT_FOREVER);
  if (mutex_status != TX_SUCCESS) {
    /* Fallback: return current counter value without overflow tracking */
    timer_hz = k_pclkb_hz / k_cmt2_divider;
    return (uint32_t)((cmt2()->cmcnt * k_us_per_second) / timer_hz);
  }

  current_counter = cmt2()->cmcnt;

  /* Detect overflow (counter wrapped around) */
  if (current_counter < last_counter) {
    overflow_count++;
  }
  last_counter = current_counter;

  /* Calculate total ticks including overflows */
  total_ticks = ((uint64_t)overflow_count << k_timer_counter_bits) | current_counter;

  /* Convert ticks to microseconds */
  timer_hz = k_pclkb_hz / k_cmt2_divider;
  result   = (uint32_t)((total_ticks * k_us_per_second) / timer_hz);

  mutex_status = tx_mutex_put(&s_time_mutex);
  if (mutex_status != TX_SUCCESS) {
    rx_log_error_val("HCSR04", "Failed to release time mutex", (uint32_t)mutex_status);
  }

  return result;
}
