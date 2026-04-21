/**
 * @file stubs.c
 * @brief No-op log/UART stubs for the motor spin test.
 *
 * @details
 * The production rx_motor / rx_gptw / rx_mpc libraries call rx_log_error()
 * and RX_ASSERT() macros that ultimately reduce to a handful of UART log
 * primitives. This bench test does not bring up SCI9 or the framed UART
 * ring buffer, so we satisfy the linker with no-op stubs. RX_ASSERT
 * failures still halt (the static-inline internal_rx_fatal_error in
 * rx_check.h disables interrupts and spins).
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* rx_log_uart family (selected when RX_IS_SIMULATOR == 0). */
void rx_log_uart_putc(char c)              { (void)c; }
void rx_log_uart_puts(const char *s)       { (void)s; }
void rx_log_uart_putint(int32_t v)         { (void)v; }
void rx_log_uart_putuint(uint32_t v)       { (void)v; }
void rx_log_uart_puthex(uint32_t v, uint8_t d) { (void)v; (void)d; }

/* uart_debug family (called unconditionally by internal_rx_fatal_error). */
void uart_debug_putc(char c)               { (void)c; }
void uart_debug_puts(const char *s)        { (void)s; }
void uart_debug_putint(int32_t v)          { (void)v; }
void uart_debug_putuint(uint32_t v)        { (void)v; }
void uart_debug_puthex(uint32_t v, uint8_t d) { (void)v; (void)d; }
