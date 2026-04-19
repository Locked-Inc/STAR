/**
 * @file stubs.c
 * @brief Stubs for ThreadX, log, and ISR symbols pulled in by the STAR HAL.
 *
 * @details
 * The HAL pulls in a set of symbols we don't need for a minimal build:
 *
 *   - rx_log_uart_puts/putc/putint/putuint/puthex: hardware log-ring API used
 *     by rx_log.h in non-simulator builds. rx_log_uart.c depends on ThreadX
 *     (tx_api.h), so we stub them out to void-return.
 *
 *   - uart_debug_*: low-level raw-UART debug writers referenced by
 *     rx_check.h's internal_rx_fatal_error. Stub to void-return.
 *
 *   - _tx_thread_context_save/restore: referenced by vectors.S for the
 *     SWINT handler. Not exercised in this bare-metal test.
 *
 *   - cmt0_isr: referenced by vectors.S for the 100 Hz ThreadX tick vector.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* ===== rx_log_uart_* stubs ============================================== */

void rx_log_uart_putc(char c)                  { (void)c; }
void rx_log_uart_puts(const char* s)           { (void)s; }
void rx_log_uart_putint(int32_t v)             { (void)v; }
void rx_log_uart_putuint(uint32_t v)           { (void)v; }
void rx_log_uart_puthex(uint32_t v, uint8_t d) { (void)v; (void)d; }

/* ===== uart_debug_* stubs (used by rx_check.h fatal-error helper) ======== */

void uart_debug_putc(char c)                  { (void)c; }
void uart_debug_puts(const char* s)           { (void)s; }
void uart_debug_putint(int32_t v)             { (void)v; }
void uart_debug_putuint(uint32_t v)           { (void)v; }
void uart_debug_puthex(uint32_t v, uint8_t d) { (void)v; (void)d; }

/* ===== ThreadX / ISR stubs referenced by vectors.S ======================= */

void _tx_thread_context_save(void)    {}
void _tx_thread_context_restore(void) {}
void cmt0_isr(void)                   {}
