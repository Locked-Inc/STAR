/**
 * @file stubs.c
 * @brief Log/UART no-op stubs for the bare-metal IMU smoke test.
 *
 * @details
 * The bench test does not pull in the rx_log family or the framed UART
 * ring buffer. Anything in the dependency chain that calls
 * rx_log_uart_* / uart_debug_* must have something to link against.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

void rx_log_uart_putc(char c)              { (void)c; }
void rx_log_uart_puts(const char *s)       { (void)s; }
void rx_log_uart_putint(int32_t v)         { (void)v; }
void rx_log_uart_putuint(uint32_t v)       { (void)v; }
void rx_log_uart_puthex(uint32_t v, uint8_t d) { (void)v; (void)d; }

void uart_debug_putc(char c)               { (void)c; }
void uart_debug_puts(const char *s)        { (void)s; }
void uart_debug_putint(int32_t v)          { (void)v; }
void uart_debug_putuint(uint32_t v)        { (void)v; }
void uart_debug_puthex(uint32_t v, uint8_t d) { (void)v; (void)d; }
