/**
 * @file mock_rx_log.c
 * @brief Mock RX Log and UART Implementation for Unit Testing
 *
 * Provides stub implementations of UART functions used by rx_log.h
 * for host-side testing.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

/**
 * @var g_rx_assert_post_force_fail
 * @brief Test-only fault injection flag for RX_ASSERT_POST coverage
 *
 * @details
 * When set to @c true, every RX_ASSERT_POST fires regardless of its condition,
 * making the failure branch reachable for branch coverage testing. Reset to
 * @c false after each test case.
 *
 * @note Only exists in UNIT_TEST builds. Declared extern in rx_check.h.
 *
 * @since Version 1.1.0
 */
bool g_rx_assert_post_force_fail = false;

/* =============================================================================
 * UART Debug Stub Implementations
 *
 * These are called by the inline logging functions in rx_log.h.
 * =============================================================================
 */

/**
 * @brief Stub for uart_debug_putc() -- no-op
 *
 * @details
 * Discards the character without writing to SCI9. Satisfies the linker for
 * tests that pull in rx_log.h runtime LOG_PUTC dispatches but do not exercise
 * UART output.
 *
 * @param[in] data Character to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to SCI9
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void uart_debug_putc(char data)
{
  (void)data;
}

/**
 * @brief Stub for uart_debug_puts() -- no-op
 *
 * @details
 * Discards the string without writing to SCI9. Satisfies the linker for
 * tests that pull in rx_log.h runtime LOG_PUTS dispatches but do not exercise
 * UART output.
 *
 * @param[in] str Null-terminated string to print (ignored, may be NULL)
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to SCI9
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void uart_debug_puts(const char* str)
{
  (void)str;
}

/**
 * @brief Stub for uart_debug_putint() -- no-op
 *
 * @details
 * Discards the integer value without writing to SCI9. Satisfies the linker
 * for tests that pull in rx_log.h runtime LOG_PUTINT dispatches but do not
 * exercise UART output.
 *
 * @param[in] value Integer value to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to SCI9
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void uart_debug_putint(int32_t value)
{
  (void)value;
}

/**
 * @brief Stub for uart_debug_putuint() -- no-op
 *
 * @details
 * Discards the unsigned integer value without writing to SCI9. Satisfies the
 * linker for tests that pull in rx_log.h runtime LOG_PUTUINT dispatches but do
 * not exercise UART output.
 *
 * @param[in] value Unsigned value to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to SCI9
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void uart_debug_putuint(uint32_t value)
{
  (void)value;
}

/**
 * @brief Stub for uart_debug_puthex() -- no-op
 *
 * @details
 * Discards the hex value and digit count without writing to SCI9. Satisfies
 * the linker for tests that pull in rx_log.h runtime LOG_PUTHEX dispatches
 * but do not exercise UART output.
 *
 * @param[in] value  Unsigned value to print as hex (ignored)
 * @param[in] digits Number of hex digits to show (ignored)
 *
 * @pre  None -- stub accepts any value and digit count
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to SCI9
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void uart_debug_puthex(uint32_t value, uint8_t digits)
{
  (void)value;
  (void)digits;
}

/**
 * @brief Stub for rx_log_usb_putc() -- no-op
 *
 * @details
 * Discards the character without writing to USB CDC Port 2. Satisfies the
 * linker for tests that pull in rx_log.h runtime LOG_PUTC dispatches but do
 * not exercise USB output.
 *
 * @param[in] c Character to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_putc(char c)
{
  (void)c;
}

/**
 * @brief Stub for rx_log_usb_puts() -- no-op
 *
 * @details
 * Discards the string without writing to USB CDC Port 2. Satisfies the
 * linker for tests that pull in rx_log.h runtime LOG_PUTS dispatches but do
 * not exercise USB output.
 *
 * @param[in] str Null-terminated string to print (ignored, may be NULL)
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_puts(const char* str)
{
  (void)str;
}

/**
 * @brief Stub for rx_log_usb_putuint() -- no-op
 *
 * @details
 * Discards the unsigned integer value without writing to USB CDC Port 2.
 * Satisfies the linker for tests that pull in rx_log.h runtime LOG_PUTUINT
 * dispatches but do not exercise USB output.
 *
 * @param[in] value Unsigned value to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_putuint(uint32_t value)
{
  (void)value;
}

/**
 * @brief Stub for rx_log_usb_putint() -- no-op
 *
 * @details
 * Discards the integer value without writing to USB CDC Port 2.
 * This stub satisfies the linker for tests that pull in rx_log.h's
 * runtime LOG_PUTINT macro but do not exercise USB output.
 *
 * @param[in] value Integer value to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_putint(int32_t value)
{
  (void)value;
}

/**
 * @brief Stub for rx_log_usb_puthex() -- no-op
 *
 * @details
 * Discards the hex value and digit count without writing to USB CDC Port 2.
 * This stub satisfies the linker for tests that pull in rx_log.h's
 * runtime LOG_PUTHEX macro but do not exercise USB output.
 *
 * @param[in] value  Unsigned value to print as hex (ignored)
 * @param[in] digits Number of hex digits to show (ignored)
 *
 * @pre  None -- stub accepts any value and digit count
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_puthex(uint32_t value, uint8_t digits)
{
  (void)value;
  (void)digits;
}
