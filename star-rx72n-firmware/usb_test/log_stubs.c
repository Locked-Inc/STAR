/**
 * @file log_stubs.c
 * @brief Stub backend for rx_log so usb_test can link without uart/rx_log_usb.
 *
 * @details
 * The production rx_log macros call into UART (uart_debug_*) and USB CDC
 * (rx_log_usb_*) backends through a runtime backend bitmask.  When the
 * backend is k_log_backend_none the runtime branches are skipped, but the
 * symbols still need to resolve at link time.
 *
 * This file provides:
 *   - rx_log_get_backend()   -- always returns k_log_backend_none
 *   - rx_log_set_backend()   -- accepts and ignores
 *   - uart_debug_*           -- empty stubs
 *   - rx_log_usb_*           -- empty stubs (the heavy USB log backend lives
 *                                in libs/rx_core/src/rx_log_usb.c which we
 *                                deliberately do NOT compile here)
 *
 * Result: rx_usb's diagnostic logs go to /dev/null, but the bring-up still
 * runs.  We get USB enumeration first; richer logging can come later.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "rx_err.h"
#include "rx_log.h"

/* ==========================================================================
 * Backend state
 * ==========================================================================
 */

rx_log_backend_t rx_log_get_backend(void)
{
    return k_log_backend_none;
}

rx_err_t rx_log_set_backend(rx_log_backend_t backend)
{
    (void)backend;
    return k_rx_ok;
}

/* ==========================================================================
 * UART debug stubs (would normally write to SCI9 via the CY7C65213 bridge)
 * ==========================================================================
 */

void uart_debug_putc(char c)
{
    (void)c;
}

void uart_debug_puts(const char *s)
{
    (void)s;
}

void uart_debug_putint(int32_t v)
{
    (void)v;
}

void uart_debug_putuint(uint32_t v)
{
    (void)v;
}

void uart_debug_puthex(uint32_t v, uint8_t digits)
{
    (void)v;
    (void)digits;
}

/* ==========================================================================
 * USB log mirror stubs (would route to k_usb_port_log)
 * ==========================================================================
 */

void rx_log_usb_putc(char c)
{
    (void)c;
}

void rx_log_usb_puts(const char *s)
{
    (void)s;
}

void rx_log_usb_putint(int32_t v)
{
    (void)v;
}

void rx_log_usb_putuint(uint32_t v)
{
    (void)v;
}

void rx_log_usb_puthex(uint32_t v, uint8_t digits)
{
    (void)v;
    (void)digits;
}

void rx_log_usb_get_stats(usb_log_stats_t *stats)
{
    if (stats != (usb_log_stats_t *)0) {
        stats->total_bytes   = 0U;
        stats->dropped_bytes = 0U;
        stats->boot_buffered = 0U;
        stats->usb_errors    = 0U;
    }
}

void rx_log_usb_notify_ready(void)
{
}
