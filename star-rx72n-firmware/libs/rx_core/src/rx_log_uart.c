/**
 * @file rx_log_uart.c
 * @brief UART-framed log backend: ring buffer + drain API
 *
 * @details
 * Producers call rx_log_uart_putc / puts / putint / puthex from any ThreadX
 * task or ISR-adjacent context. Bytes are appended to a static ring buffer
 * under a ThreadX mutex and the call returns without touching any hardware.
 *
 * The single consumer (the communication task) calls rx_log_uart_drain() on
 * every poll tick. Drained bytes are wrapped in a k_frame_type_log_message
 * (0x20) frame and transmitted via rx_uart_comm over SCI9 -> CY7C65213 ->
 * /dev/ttyACM0 on the Pi5 (CDC-ACM class device), where the gateway routes
 * them to its own log sink.
 *
 * No I/O happens on the producer path: even a burst of 1000 log lines can only
 * cost a mutex, a memcpy, and a ring index update. Bytes that overflow the
 * ring are counted and dropped -- preferable to blocking real-time motor
 * control code on serial line drain.
 *
 * @author Locked, Inc.
 * @date 2026-04-17
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_log_uart.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rx_err.h"
#include "tx_api.h"

#ifdef UNIT_TEST
/**
 * @brief Test-only fault injection flag for RX_ASSERT_POST coverage
 *
 * When set to @c true, every RX_ASSERT_POST fires regardless of its condition,
 * making the failure branch reachable for branch coverage testing. Declared
 * extern in @ref rx_check.h. The definition previously lived in mock_rx_log.c,
 * which was deleted along with the USB0 log backend; it now lives here so
 * UNIT_TEST builds continue to link.
 */
bool g_rx_assert_post_force_fail = false;
#endif

/* =============================================================================
 * Static state
 * ============================================================================= */

/**
 * @enum rx_log_uart_format_t
 * @brief Numeric formatting constants
 */
typedef enum : uint8_t {
  k_int32_buf_len  = 12U, /**< '-' + 10 digits + NUL */
  k_uint32_buf_len = 11U, /**< 10 digits + NUL */
  k_decimal_base   = 10U,
  k_hex_max_digits = 8U,
  k_hex_buf_len    = 9U,
  k_nibble_shift   = 4U,
  k_nibble_mask    = 0x0FU,
} rx_log_uart_format_t;

/** @brief Ring buffer storage (contiguous bytes, head wraps) */
static uint8_t s_ring[k_rx_log_uart_ring_size];

/** @brief Write index: next byte goes to s_ring[s_head] */
static uint32_t s_head;

/** @brief Read index: next byte to drain is s_ring[s_tail] */
static uint32_t s_tail;

/** @brief Number of occupied bytes in the ring (0..k_rx_log_uart_ring_size) */
static uint32_t s_count;

/** @brief Diagnostic counters */
static rx_log_uart_stats_t s_stats;

/** @brief ThreadX mutex (lazy init on first use) */
static TX_MUTEX s_mutex;
static bool     s_mutex_initialized;

/* =============================================================================
 * Internal helpers (mutex must be held by caller)
 * ============================================================================= */

/**
 * @brief Append one byte to the ring under lock; drop on overflow
 */
static void internal_append_byte(uint8_t b)
{
  if (s_count >= k_rx_log_uart_ring_size) {
    s_stats.dropped_bytes += 1U;
    return;
  }
  s_ring[s_head] = b;
  s_head         = (s_head + 1U) % k_rx_log_uart_ring_size;
  s_count += 1U;
  s_stats.total_bytes += 1U;
  if (s_count > s_stats.high_water) {
    s_stats.high_water = s_count;
  }
}

/**
 * @brief Lazy mutex init (safe to call from any task context after kernel entry)
 */
static void internal_init_mutex_once(void)
{
  if (!s_mutex_initialized) {
    (void)tx_mutex_create(&s_mutex, (CHAR*)"rx_log_uart", TX_NO_INHERIT);
    s_mutex_initialized = true;
  }
}

/* =============================================================================
 * Public API -- producers
 * ============================================================================= */

void rx_log_uart_putc(char c)
{
  internal_init_mutex_once();
  (void)tx_mutex_get(&s_mutex, TX_WAIT_FOREVER);
  internal_append_byte((uint8_t)c);
  (void)tx_mutex_put(&s_mutex);
}

void rx_log_uart_puts(const char* str)
{
  if (str == NULL) {
    return;
  }
  internal_init_mutex_once();
  (void)tx_mutex_get(&s_mutex, TX_WAIT_FOREVER);
  for (const char* p = str; *p != '\0'; ++p) {
    internal_append_byte((uint8_t)*p);
  }
  (void)tx_mutex_put(&s_mutex);
}

void rx_log_uart_putint(int32_t value)
{
  char buf[k_int32_buf_len];

  char* p = &buf[sizeof(buf) - 1U];
  *p      = '\0';

  const bool negative  = (bool)(value < 0);
  uint32_t   abs_value = (int)negative ? (uint32_t)(-value) : (uint32_t)value;

  do {
    --p;
    *p = (char)('0' + (abs_value % k_decimal_base));
    abs_value /= k_decimal_base;
  } while (abs_value > 0U);

  if (negative) {
    --p;
    *p = '-';
  }

  rx_log_uart_puts(p);
}

void rx_log_uart_putuint(uint32_t value)
{
  char buf[k_uint32_buf_len];

  char* p = &buf[sizeof(buf) - 1U];
  *p      = '\0';

  do {
    --p;
    *p = (char)('0' + (value % k_decimal_base));
    value /= k_decimal_base;
  } while (value > 0U);

  rx_log_uart_puts(p);
}

void rx_log_uart_puthex(uint32_t value, uint8_t digits)
{
  if (digits == 0U) {
    digits = 1U;
  } else if (digits > k_hex_max_digits) {
    digits = k_hex_max_digits;
  }

  char  buf[k_hex_buf_len];
  char* p = &buf[digits];
  *p      = '\0';

  for (uint8_t i = 0U; i < digits; ++i) {
    --p;
    const uint8_t nibble = (uint8_t)(value & (uint32_t)k_nibble_mask);
    *p = (char)((nibble < k_decimal_base) ? ('0' + nibble) : ('A' + (nibble - k_decimal_base)));
    value >>= (uint32_t)k_nibble_shift;
  }

  rx_log_uart_puts(buf);
}

/* =============================================================================
 * Public API -- consumer (comm_task)
 * ============================================================================= */

uint32_t rx_log_uart_pending_len(void)
{
  internal_init_mutex_once();
  (void)tx_mutex_get(&s_mutex, TX_WAIT_FOREVER);
  const uint32_t n = s_count;
  (void)tx_mutex_put(&s_mutex);
  return n;
}

rx_err_t rx_log_uart_drain(uint8_t* out_buf, uint32_t max_len, uint32_t* out_len)
{
  if ((out_buf == NULL) || (out_len == NULL) || (max_len == 0U)) {
    return k_rx_err_invalid_arg;
  }

  internal_init_mutex_once();
  (void)tx_mutex_get(&s_mutex, TX_WAIT_FOREVER);

  uint32_t to_copy = (s_count < max_len) ? s_count : max_len;

  /* Prefer a single contiguous copy -- if the unread region wraps the ring
   * end, return only the pre-wrap portion this call. Caller drains again to
   * pick up the remainder. This keeps the consumer loop simple and bounds
   * execution time per drain call (NASA Rule 2). */
  const uint32_t contiguous = k_rx_log_uart_ring_size - s_tail;
  if (to_copy > contiguous) {
    to_copy = contiguous;
  }

  if (to_copy > 0U) {
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memcpy(out_buf, &s_ring[s_tail], to_copy);
    s_tail = (s_tail + to_copy) % k_rx_log_uart_ring_size;
    s_count -= to_copy;
    s_stats.drained_bytes += to_copy;
  }

  *out_len = to_copy;

  (void)tx_mutex_put(&s_mutex);
  return k_rx_ok;
}

void rx_log_uart_get_stats(rx_log_uart_stats_t* stats)
{
  if (stats == NULL) {
    return;
  }
  internal_init_mutex_once();
  (void)tx_mutex_get(&s_mutex, TX_WAIT_FOREVER);
  *stats = s_stats;
  (void)tx_mutex_put(&s_mutex);
}

#ifdef UNIT_TEST
void rx_log_uart_test_reset_state(void)
{
  s_head              = 0U;
  s_tail              = 0U;
  s_count             = 0U;
  s_stats             = (rx_log_uart_stats_t){0U, 0U, 0U, 0U};
  s_mutex_initialized = false;
}
#endif
