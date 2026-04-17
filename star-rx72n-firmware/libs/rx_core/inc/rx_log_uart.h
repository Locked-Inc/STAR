/**
 * @file rx_log_uart.h
 * @brief UART-framed log backend for the RX72N logging system
 *
 * @details
 * Stores log bytes in a ring buffer in RAM and exposes a drain API that the
 * communication task calls on every poll tick. Drained bytes are wrapped in a
 * @ref k_frame_type_log_message frame (TYPE=0x20) and sent over the SCI9 UART
 * through the CY7C65213 USB-UART bridge.
 *
 * Framing logs (rather than writing raw ASCII to the UART) prevents log bytes
 * from colliding with the frame sync word 0x55AA used by the protobuf command
 * and telemetry frames on the same byte stream.
 *
 * ## Write path
 *   application thread -> rx_log_uart_putc / puts / ...
 *     -> append to s_ring (mutex-guarded, non-blocking)
 *     -> return immediately (no I/O from log site)
 *
 * ## Drain path (runs from comm_task at ~100 Hz)
 *   comm_task tick
 *     -> rx_log_uart_pending_len() > 0 ?
 *     -> rx_log_uart_drain(buf, max)
 *     -> rx_uart_comm_send(handle, k_frame_type_log_message, 0, buf, len)
 *
 * ## Thread safety
 * Producers (rx_log_uart_*) and the single consumer (rx_log_uart_drain) are
 * serialized by a lazy-initialized ThreadX mutex. Producers never block on
 * I/O; if the ring is full, bytes are dropped and counted in the drop stat.
 *
 * @author Locked, Inc.
 * @date 2026-04-17
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum rx_log_uart_constants_t
 * @brief Ring buffer sizing for the UART log backend
 */
typedef enum : uint16_t {
  /**
   * @brief Log ring buffer size in bytes (4096)
   *
   * At 921600 baud the UART can carry ~92 kB/s. 4096 bytes provides ~45 ms of
   * sustained burst capacity, which is well above the 10 ms comm_task poll
   * interval.
   */
  k_rx_log_uart_ring_size = 4096,
} rx_log_uart_constants_t;

/**
 * @struct rx_log_uart_stats_t
 * @brief Diagnostic counters for the UART log backend
 */
typedef struct {
  uint32_t total_bytes;   /**< Cumulative bytes accepted by producers (pre-drop) */
  uint32_t drained_bytes; /**< Cumulative bytes handed to the drain consumer */
  uint32_t dropped_bytes; /**< Cumulative bytes dropped because the ring was full */
  uint32_t high_water;    /**< Maximum observed ring occupancy (bytes) */
} rx_log_uart_stats_t;

/**
 * @brief Append a single character to the log ring buffer
 *
 * @param[in] c Character to append
 *
 * @note Thread-safe, non-blocking. Drops silently on ring overflow.
 */
void rx_log_uart_putc(char c);

/**
 * @brief Append a null-terminated string to the log ring buffer
 *
 * @param[in] str Null-terminated string (nullptr is silently ignored)
 *
 * @note Thread-safe, non-blocking. Drops silently on ring overflow.
 */
void rx_log_uart_puts(const char* str);

/**
 * @brief Append a signed integer as decimal ASCII to the log ring buffer
 *
 * @param[in] value Signed 32-bit integer value
 */
void rx_log_uart_putint(int32_t value);

/**
 * @brief Append an unsigned integer as decimal ASCII to the log ring buffer
 *
 * @param[in] value Unsigned 32-bit integer value
 */
void rx_log_uart_putuint(uint32_t value);

/**
 * @brief Append a value as zero-padded hexadecimal ASCII to the log ring buffer
 *
 * @param[in] value  Value to format
 * @param[in] digits Number of hex digits to emit (clamped to 1..8)
 */
void rx_log_uart_puthex(uint32_t value, uint8_t digits);

/**
 * @brief Query number of bytes currently queued in the ring buffer
 *
 * @return Pending byte count (0..@ref k_rx_log_uart_ring_size)
 */
uint32_t rx_log_uart_pending_len(void);

/**
 * @brief Pull up to @p max_len bytes out of the ring buffer for transmission
 *
 * @details
 * Copies a contiguous chunk of up to @p max_len bytes out of the ring into
 * @p out_buf. Returns the number of bytes actually copied in @p out_len.
 * A single call may return fewer bytes than requested when the ring has
 * wrapped; call again to drain the remainder.
 *
 * @param[out] out_buf Destination buffer
 * @param[in]  max_len Maximum bytes to copy into @p out_buf
 * @param[out] out_len Actual number of bytes written
 *
 * @retval k_rx_ok              Success (0 bytes also returns ok)
 * @retval k_rx_err_invalid_arg @p out_buf or @p out_len is nullptr, or max_len == 0
 */
[[nodiscard]] rx_err_t rx_log_uart_drain(uint8_t* out_buf, uint32_t max_len, uint32_t* out_len);

/**
 * @brief Snapshot diagnostic counters
 *
 * @param[out] stats Statistics destination (nullptr is silently ignored)
 */
void rx_log_uart_get_stats(rx_log_uart_stats_t* stats);

#ifdef UNIT_TEST
/**
 * @brief Reset all ring buffer and statistics state (unit tests only)
 */
void rx_log_uart_test_reset_state(void);
#endif

#ifdef __cplusplus
}
#endif
