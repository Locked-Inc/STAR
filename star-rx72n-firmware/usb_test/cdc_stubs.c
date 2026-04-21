/**
 * @file cdc_stubs.c
 * @brief Link-time stubs for rx_log / rx_check backends (cdc_test only)
 *
 * @details
 * The cdc_test target builds with -DLOG_LEVEL=0 so all rx_log_* macros
 * expand to ((void)0).  However:
 *   1. At -O0 the compiler does NOT strip the `static inline` helpers in
 *      rx_log.h (internal_log_putc, internal_log_puts), so they end up
 *      referencing rx_log_uart_putc / rx_log_uart_puts at link time.
 *   2. rx_check.h's internal_rx_fatal_error() unconditionally calls
 *      uart_debug_putc/puts/puthex regardless of LOG_LEVEL.
 *
 * This file provides the minimum set of symbols needed for the link to
 * succeed.  None of them are expected to be called at runtime because
 * LOG_LEVEL=0 means no log calls exist in any rx_usb_* translation unit.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* Hardware log backend (normally rx_log_uart.c) */
void rx_log_uart_putc(char c) {
  (void)c;
}
void rx_log_uart_puts(const char* s) {
  (void)s;
}
void rx_log_uart_putint(int32_t v) {
  (void)v;
}
void rx_log_uart_putuint(uint32_t v) {
  (void)v;
}
void rx_log_uart_puthex(uint32_t v, uint8_t d) {
  (void)v;
  (void)d;
}

/* Fatal-error UART backend (rx_check.h internal_rx_fatal_error) */
void uart_debug_putc(char c) {
  (void)c;
}
void uart_debug_puts(const char* s) {
  (void)s;
}
void uart_debug_putint(int32_t v) {
  (void)v;
}
void uart_debug_putuint(uint32_t v) {
  (void)v;
}
void uart_debug_puthex(uint32_t v, uint8_t d) {
  (void)v;
  (void)d;
}
