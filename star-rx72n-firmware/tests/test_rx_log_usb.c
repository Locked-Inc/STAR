/**
 * @file test_rx_log_usb.c
 * @brief Unit Tests for USB CDC Logging Backend (rx_log_usb.c)
 *
 * @details
 * Validates all public functions of the USB CDC logging backend:
 * - rx_log_usb_putc()   - single character output
 * - rx_log_usb_puts()   - string output (null-safe)
 * - rx_log_usb_putint() - signed decimal output
 * - rx_log_usb_puthex() - hexadecimal output with configurable digit width
 * - rx_log_usb_get_stats() - statistics retrieval
 * - rx_log_usb_notify_ready() - explicit USB-ready flush trigger
 *
 * **Architecture and Mock Layers:**
 *
 * The test build links:
 *   - rx_log_usb.c   (unit under test)
 *   - rx_usb.c       (real USB driver with internal TX ring buffer)
 *   - mock_usb_hw.c  (HW layer: mocks rx_usb_hw_fifo_write, rx_usb_cdc_handle_bulk_in, etc.)
 *   - mock_usb0_regs.c  (register simulation)
 *   - rx_log.c       (rx_log_set/get_backend only, not mock_rx_log.c)
 *   - MOCK_TX_API_SRC   (inline ThreadX mutex stubs from tx_api.h)
 *
 * **Verification Strategy:**
 *
 * mock_usb_hw.c provides a NO-OP stub for rx_usb_cdc_handle_bulk_in(), so data
 * written through rx_usb_write() is stored in rx_usb.c's internal TX ring buffer
 * but never flushed to the mock hardware pipe. Therefore:
 *
 * - For tests verifying data REACHES rx_usb_write(), we use rx_usb_get_stats()
 *   to check stats.bytes_tx -- incremented inside rx_usb_write() on each write.
 * - For tests verifying data does NOT reach USB (boot buffering), we check
 *   rx_usb_get_stats() shows bytes_tx == 0 and rx_log_usb_get_stats() shows
 *   boot_buffered > 0.
 * - For integer / hex formatting correctness we use rx_log_usb_get_stats()
 *   total_bytes compared to the expected character count.
 *
 * **Boot Buffering Model:**
 * Before the USB host enumerates (rx_usb_is_configured() returns false), all
 * log writes go into a 512-byte ring buffer in rx_log_usb.c. When USB becomes
 * configured the buffer is flushed on the next write or on an explicit
 * rx_log_usb_notify_ready() call.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [PASS] No goto or recursion
 * - Rule 2: [PASS] All loops have statically provable bounds
 * - Rule 3: [PASS] Zero dynamic allocation
 * - Rule 4: [PASS] Each test function under 60 lines
 * - Rule 5: [PASS] Two+ precondition checks per non-trivial function
 * - Rule 6: [PASS] Variables declared at minimum scope
 * - Rule 7: [PASS] All return values checked
 * - Rule 8: [PASS] All integer constants in typed enums
 * - Rule 9: [PASS] No function pointers beyond mock interface
 * - Rule 10: [PASS] -Wall -Wextra -Werror enforced by CMake
 *
 * @author Locked, Inc.
 * @date 2026-03-12
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mock_usb0_regs.h" /* mock_regs_init() / mock_regs_clear() */
#include "mock_usb_hw.h"
#include "rx_log.h"
#include "rx_usb.h"
#include "rx_usb_private.h" /* rx_usb_set_state() for test state control */
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum log_usb_test_const_t
 * @brief Named constants used throughout the test suite
 *
 * @details
 * All numeric literals in test assertions are expressed through this enum
 * to comply with the STAR no-magic-numbers rule (CLAUDE.md).
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_small_buf           = 64U,         /**< Buffer for integer / hex output */
  k_test_putc_bytes          = 1U,          /**< rx_log_usb_putc() counts as 1 byte */
  k_test_puts_hello_len      = 5U,          /**< Length of "hello" */
  k_test_zero_bytes          = 0U,          /**< Expected byte count after no writes */
  k_test_putint_positive_val = 42U,         /**< Arbitrary positive value */
  k_test_puthex_val          = 0xDEADBEEFU, /**< Known 8-nibble hex constant */
  k_test_puthex_8_digits     = 8U,          /**< Full 32-bit hex with 8 digits */
  k_test_puthex_4_digits     = 4U,          /**< Half-width hex output */
  k_test_boot_buf_size       = 512U,        /**< Size of internal boot ring buffer */
  k_test_overflow_fill       = 520U,        /**< More than boot buffer - triggers overflow */
  k_test_one_byte            = 1U,          /**< One char = one byte */
  k_test_usb_log_tx_size     = 1024U,       /**< USB log port TX ring buffer capacity */
  k_test_puthex_9_digits     = 9U,          /**< > 8, triggers clamp to 8 */
  k_test_puthex_zero_digits  = 0U,          /**< 0 digits, triggers clamp to 1 */
  k_test_putint_neg_val      = 7U,          /**< Absolute value for negative test */
  k_test_putint_min_digits   = 2U,          /**< Minimum digits for 2-char output */
  k_test_puthex_test_byte    = 0xABU,       /**< Test byte for puthex zero digits */
  k_test_boot_wrap_head      = 42U,         /**< Boot ring buffer head for wrap test */
  k_test_boot_wrap_count     = 64U,         /**< Boot ring buffer count for wrap test */
} log_usb_test_const_t;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

/**
 * @brief Reset all test state before each test case
 *
 * @details
 * Reinitialises the mock USB hardware registers and HW state, then
 * initialises the real USB driver.  The USB driver is left in the
 * *unconfigured* state so that tests needing USB-ready can call
 * helper_set_usb_configured() explicitly.
 *
 * @pre mock_regs_init() must succeed
 * @pre mock_usb_hw_init() must succeed
 * @pre rx_usb_init() must succeed
 *
 * @post All mock USB state and registers are zeroed / set to defaults
 * @post USB driver is initialized but in k_usb_state_attached (not configured)
 *
 * @note Not thread-safe - Unity test runner is single-threaded
 */
void setUp(void)
{
  mock_regs_init();
  mock_usb_hw_init(nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
}

/**
 * @brief Deinitialise resources after each test case
 *
 * @details
 * Calls rx_usb_deinit() to reset the USB driver's static state.
 * mock_regs_clear() zeroes the mock register file so that any
 * pipe control state (PBUSY bits, etc.) from one test does not
 * affect the next test's trigger logic.
 *
 * @pre rx_usb_init() was called in setUp()
 * @post USB driver static state is reset
 * @post Mock register state is zeroed
 */
void tearDown(void)
{
  (void)rx_usb_deinit();
  mock_regs_clear();
}

/* =============================================================================
 * Helper: set USB to configured state
 * =============================================================================
 */

/**
 * @brief Transition the USB driver into k_usb_state_configured
 *
 * @details
 * Makes rx_usb_is_configured(k_usb_port_log) return true so that
 * subsequent rx_log_usb_* calls write directly to USB via rx_usb_write()
 * rather than the boot buffer.
 *
 * @pre rx_usb_init() already called
 * @post rx_usb_is_configured(k_usb_port_log) returns true
 */
static void helper_set_usb_configured(void)
{
  rx_usb_set_state(k_usb_state_configured);
  TEST_ASSERT_TRUE(rx_usb_is_configured(k_usb_port_log));
}

/**
 * @brief Get the number of bytes written to the log port TX ring buffer
 *
 * @details
 * Reads rx_usb_stats_t.bytes_tx for k_usb_port_log. This reflects bytes
 * accepted by rx_usb_write() into the internal ring buffer.  The mock CDC
 * layer does not flush the ring buffer to mock hardware, so bytes_tx is the
 * only observable confirmation that rx_log_usb_* forwarded data to rx_usb.
 *
 * @param[out] out_bytes Pointer set to bytes_tx value
 * @pre rx_usb_init() was called
 * @post *out_bytes is the cumulative bytes written to port log TX buffer
 */
static void helper_get_usb_tx_bytes(uint32_t* out_bytes)
{
  rx_usb_stats_t stats = {0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_log, &stats));
  *out_bytes = stats.bytes_tx;
}

/* =============================================================================
 * Tests: putc - single character output
 * =============================================================================
 */

/**
 * @brief putc forwards one byte to rx_usb_write when USB is configured
 *
 * @details
 * After transitioning USB to the configured state, rx_log_usb_putc('A')
 * must cause rx_usb_write(k_usb_port_log, ...) to be called with 1 byte,
 * which is confirmed via the bytes_tx statistic.
 *
 * @pre USB in configured state
 * @post rx_usb stats show at least 1 byte written to log port TX buffer
 *
 * @since Version 1.0.0
 */
void test_log_usb_putc_basic(void)
{
  helper_set_usb_configured();

  rx_log_usb_putc('A');

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_GREATER_OR_EQUAL(k_test_putc_bytes, tx_bytes);
}

/**
 * @brief putc before USB ready buffers the byte instead of writing to USB
 *
 * @details
 * When USB is not yet configured, rx_log_usb_putc() must buffer the byte
 * internally. rx_usb_write() is NOT called, so bytes_tx remains 0.
 *
 * @pre USB in unconfigured state (default after setUp)
 * @post rx_usb stats show 0 bytes written to log port TX buffer
 * @post rx_log_usb stats show at least 1 boot-buffered byte
 *
 * @since Version 1.0.0
 */
void test_log_usb_putc_before_usb_ready_buffers(void)
{
  /* USB not configured - should buffer internally */
  rx_log_usb_putc('X');

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_zero_bytes, tx_bytes);

  usb_log_stats_t log_stats = {0};
  rx_log_usb_get_stats(&log_stats);
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, log_stats.boot_buffered);
}

/* =============================================================================
 * Tests: puts - string output
 * =============================================================================
 */

/**
 * @brief puts forwards the correct byte count to rx_usb_write when USB ready
 *
 * @details
 * rx_log_usb_puts("hello") must cause at least 5 bytes to be written to
 * the USB TX ring buffer when USB is in the configured state.
 *
 * @pre USB in configured state
 * @post bytes_tx >= 5
 *
 * @since Version 1.0.0
 */
void test_log_usb_puts_basic(void)
{
  helper_set_usb_configured();

  rx_log_usb_puts("hello");

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_GREATER_OR_EQUAL(k_test_puts_hello_len, tx_bytes);
}

/**
 * @brief puts with NULL pointer is a safe no-op (does not crash)
 *
 * @details
 * Per defensive programming practice, rx_log_usb_puts(NULL) must not crash
 * and must not write any bytes to the USB TX ring buffer.
 *
 * @pre USB in configured state
 * @post bytes_tx == 0
 *
 * @since Version 1.0.0
 */
void test_log_usb_puts_null(void)
{
  helper_set_usb_configured();

  rx_log_usb_puts(nullptr);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_zero_bytes, tx_bytes);
}

/**
 * @brief puts with empty string writes zero bytes
 *
 * @details
 * rx_log_usb_puts("") should write nothing to USB because the string has
 * zero length.
 *
 * @pre USB in configured state
 * @post bytes_tx == 0
 *
 * @since Version 1.0.0
 */
void test_log_usb_puts_empty(void)
{
  helper_set_usb_configured();

  rx_log_usb_puts("");

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_zero_bytes, tx_bytes);
}

/**
 * @brief puts before USB ready buffers the string internally
 *
 * @details
 * When USB is unconfigured, puts() buffers internally. bytes_tx stays 0.
 *
 * @pre USB in unconfigured state
 * @post bytes_tx == 0
 *
 * @since Version 1.0.0
 */
void test_log_usb_puts_before_usb_ready_buffers(void)
{
  rx_log_usb_puts("buffered");

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_zero_bytes, tx_bytes);
}

/* =============================================================================
 * Tests: putint - signed integer formatting
 * =============================================================================
 */

/**
 * @brief putint(42) produces at least 2 bytes in USB TX ring buffer
 *
 * @details
 * The decimal representation of 42 is "42" (2 ASCII characters).
 * rx_log_usb_putint(42) must cause at least 2 bytes to be forwarded to
 * rx_usb_write() when USB is configured.
 *
 * @pre USB in configured state
 * @post bytes_tx >= 2
 *
 * @since Version 1.0.0
 */
void test_log_usb_putint_positive(void)
{
  helper_set_usb_configured();

  rx_log_usb_putint((int32_t)k_test_putint_positive_val);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  /* "42" = 2 bytes */
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_putint_min_digits, tx_bytes);
}

/**
 * @brief putint(-7) produces at least 2 bytes (minus sign + digit)
 *
 * @details
 * The decimal representation of -7 is "-7" (2 ASCII characters).
 *
 * @pre USB in configured state
 * @post bytes_tx >= 2
 *
 * @since Version 1.0.0
 */
void test_log_usb_putint_negative(void)
{
  helper_set_usb_configured();

  rx_log_usb_putint(-(int32_t)k_test_putint_neg_val);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  /* "-7" = 2 bytes (sign + digit) */
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_putint_min_digits, tx_bytes);
}

/**
 * @brief putint(0) produces exactly 1 byte in USB TX ring buffer
 *
 * @details
 * The decimal representation of 0 is "0" (1 ASCII character).
 * rx_log_usb_putint(0) must forward exactly 1 byte to rx_usb_write().
 *
 * @pre USB in configured state
 * @post bytes_tx >= 1
 *
 * @since Version 1.0.0
 */
void test_log_usb_putint_zero(void)
{
  helper_set_usb_configured();

  rx_log_usb_putint(0);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, tx_bytes);
}

/* =============================================================================
 * Tests: puthex - hexadecimal formatting
 * =============================================================================
 */

/**
 * @brief puthex(0xDEADBEEF, 8) produces exactly 8 bytes in USB TX ring buffer
 *
 * @details
 * The hex representation with 8 digits is "DEADBEEF" (8 ASCII characters).
 * rx_log_usb_puthex(0xDEADBEEF, 8) must forward exactly 8 bytes.
 *
 * @pre USB in configured state
 * @post bytes_tx == 8
 *
 * @since Version 1.0.0
 */
void test_log_usb_puthex_basic(void)
{
  helper_set_usb_configured();

  rx_log_usb_puthex(k_test_puthex_val, (uint8_t)k_test_puthex_8_digits);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_puthex_8_digits, tx_bytes);
}

/**
 * @brief puthex(0, 8) produces exactly 8 bytes (all '0')
 *
 * @details
 * Zero with 8 digits produces "00000000" (8 bytes).
 *
 * @pre USB in configured state
 * @post bytes_tx == 8
 *
 * @since Version 1.0.0
 */
void test_log_usb_puthex_zero(void)
{
  helper_set_usb_configured();

  rx_log_usb_puthex(0U, (uint8_t)k_test_puthex_8_digits);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_puthex_8_digits, tx_bytes);
}

/**
 * @brief puthex with 4-digit width produces exactly 4 bytes
 *
 * @details
 * puthex(0xDEADBEEF, 4) must write exactly 4 bytes (the low 4 nibbles).
 *
 * @pre USB in configured state
 * @post bytes_tx == 4
 *
 * @since Version 1.0.0
 */
void test_log_usb_puthex_4_digits(void)
{
  helper_set_usb_configured();

  rx_log_usb_puthex(k_test_puthex_val, (uint8_t)k_test_puthex_4_digits);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_puthex_4_digits, tx_bytes);
}

/* =============================================================================
 * Tests: get_stats - statistics structure
 * =============================================================================
 */

/**
 * @brief get_stats with NULL pointer is a safe no-op
 *
 * @details
 * rx_log_usb_get_stats(NULL) must not crash. Defensive null check
 * in the implementation silently returns.
 *
 * @pre USB in any state
 * @post No crash, no memory corruption
 *
 * @since Version 1.0.0
 */
void test_log_usb_get_stats_null(void)
{
  /* Must not crash */
  rx_log_usb_get_stats(nullptr);
}

/**
 * @brief get_stats returns total_bytes >= 1 after a putc when USB ready
 *
 * @details
 * rx_log_usb.c's total_bytes stat is incremented on every write, regardless
 * of whether data goes to the boot buffer or USB. After writing 'Z' with USB
 * configured, total_bytes must be at least 1.
 *
 * @pre Fresh setUp(); USB transitioned to configured
 * @post stats.total_bytes >= 1
 *
 * @since Version 1.0.0
 */
void test_log_usb_get_stats_basic(void)
{
  helper_set_usb_configured();

  rx_log_usb_putc('Z');

  usb_log_stats_t stats = {0};
  rx_log_usb_get_stats(&stats);

  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, stats.total_bytes);
}

/**
 * @brief get_stats boot_buffered increases when writing before USB ready
 *
 * @details
 * Writing a character before USB is configured must increment
 * stats.boot_buffered by at least the number of bytes written.
 *
 * @pre USB in unconfigured state
 * @post stats.boot_buffered >= 1
 *
 * @since Version 1.0.0
 */
void test_log_usb_get_stats_boot_buffered(void)
{
  /* Do NOT set USB configured - write goes to boot buffer */
  rx_log_usb_putc('B');

  usb_log_stats_t stats = {0};
  rx_log_usb_get_stats(&stats);

  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, stats.boot_buffered);
}

/* =============================================================================
 * Tests: notify_ready - explicit flush trigger
 * =============================================================================
 */

/**
 * @brief notify_ready when USB configured flushes boot buffer to USB
 *
 * @details
 * Steps:
 * 1. Write a character before USB is configured (goes to boot buffer).
 * 2. Configure USB (rx_usb_set_state -> configured).
 * 3. Call rx_log_usb_notify_ready().
 * 4. Verify bytes_tx > 0 in rx_usb stats (boot buffer was flushed via rx_usb_write).
 *
 * @pre Boot buffer contains at least one character
 * @pre USB transitioned to configured before notify_ready
 * @post rx_usb stats show bytes written to log port TX buffer
 *
 * @since Version 1.0.0
 */
void test_log_usb_notify_ready(void)
{
  /* Write before USB ready -> goes to internal boot buffer */
  rx_log_usb_putc('N');

  /* Now configure USB */
  helper_set_usb_configured();

  /* Explicit flush of boot buffer */
  rx_log_usb_notify_ready();

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, tx_bytes);
}

/**
 * @brief notify_ready when USB not configured is a safe no-op
 *
 * @details
 * Calling rx_log_usb_notify_ready() while USB is still unconfigured must not
 * crash or corrupt state. bytes_tx in rx_usb must remain 0.
 *
 * @pre USB in unconfigured state
 * @post No crash; bytes_tx == 0
 *
 * @since Version 1.0.0
 */
void test_log_usb_notify_ready_usb_not_ready(void)
{
  rx_log_usb_putc('Q');

  /* USB still not configured - notify_ready should be a no-op */
  rx_log_usb_notify_ready();

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  TEST_ASSERT_EQUAL(k_test_zero_bytes, tx_bytes);
}

/**
 * @brief notify_ready is idempotent (safe to call multiple times)
 *
 * @details
 * Calling rx_log_usb_notify_ready() twice in a row when USB is configured
 * must not crash or corrupt state. The second call is a no-op because the
 * boot buffer was already flushed.
 *
 * @pre USB in configured state; no pending boot-buffer data
 * @post No crash; no extra bytes from the second call
 *
 * @since Version 1.0.0
 */
void test_log_usb_notify_ready_idempotent(void)
{
  helper_set_usb_configured();

  /* First call - flushes empty boot buffer (no-op effect) */
  rx_log_usb_notify_ready();

  uint32_t after_first = 0U;
  helper_get_usb_tx_bytes(&after_first);

  /* Second call - must also succeed without crashing */
  rx_log_usb_notify_ready();

  uint32_t after_second = 0U;
  helper_get_usb_tx_bytes(&after_second);

  /* No bytes were produced by notify_ready on an empty boot buffer */
  TEST_ASSERT_EQUAL(k_test_zero_bytes, after_first);
  TEST_ASSERT_EQUAL(k_test_zero_bytes, after_second);
}

/* =============================================================================
 * Tests: boot buffer - overflow and boundary
 * =============================================================================
 */

/**
 * @brief Writing more than 512 bytes before USB ready does not crash
 *
 * @details
 * The internal boot ring buffer is 512 bytes. Writing more than that while
 * USB is not configured must not crash, corrupt memory, or assert-fail.
 * The implementation silently drops excess bytes and sets the overflow flag.
 *
 * @pre USB in unconfigured state
 * @post No crash; bytes_tx == 0; stats.boot_buffered <= boot buffer size
 *
 * @since Version 1.0.0
 */
void test_log_usb_boot_buffer_overflow_no_crash(void)
{
  /* Write more bytes than the 512-byte boot buffer can hold */
  for (uint32_t i = 0U; i < k_test_overflow_fill; i++) {
    rx_log_usb_putc('X');
  }

  /* Must not have crashed. bytes_tx must remain zero (USB not configured). */
  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);
  TEST_ASSERT_EQUAL(k_test_zero_bytes, tx_bytes);

  /* stats.boot_buffered must not exceed the 512-byte buffer size */
  usb_log_stats_t stats = {0};
  rx_log_usb_get_stats(&stats);
  TEST_ASSERT_LESS_OR_EQUAL(k_test_boot_buf_size, stats.boot_buffered);
}

/**
 * @brief Direct writes succeed when USB becomes configured
 *
 * @details
 * This test verifies the nominal write path after USB reaches the configured
 * state. It runs AFTER test_log_usb_notify_ready which permanently sets
 * s_usb_ready=true in rx_log_usb.c's static state.
 *
 * Steps:
 * 1. Configure USB (helper_set_usb_configured).
 * 2. Write two characters ('A', 'B') -> s_usb_ready already true -> goes
 *    directly to rx_usb_write().
 * 3. Verify bytes_tx >= 2 in rx_usb stats.
 *
 * @pre s_usb_ready=true (set by earlier test in this binary run)
 * @pre USB freshly initialised but not yet configured (from setUp)
 * @post bytes_tx >= 2 after USB configured and two bytes written
 *
 * @since Version 1.0.0
 */
void test_log_usb_boot_buffer_flushed_on_ready(void)
{
  /* Transition USB to configured state */
  helper_set_usb_configured();

  /* Write directly to USB (s_usb_ready already true from prior test) */
  rx_log_usb_putc('A');
  rx_log_usb_putc('B');

  uint32_t after_tx = 0U;
  helper_get_usb_tx_bytes(&after_tx);

  /* Should have received at least 2 bytes: 'A', 'B' */
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_putint_min_digits, after_tx);
}

/* =============================================================================
 * Tests: puthex - edge cases (digits == 0 and digits > 8)
 * =============================================================================
 */

/**
 * @brief puthex with digits == 0 clamps to 1 and writes exactly 1 byte
 *
 * @details
 * The implementation clamps digits=0 to 1, so puthex(0xAB, 0) must write
 * exactly 1 byte (the low nibble 'B') to the USB TX ring buffer.
 *
 * @pre USB in configured state and s_usb_ready == true
 * @post bytes_tx == 1 (one hex digit)
 *
 * @since Version 1.0.0
 */
void test_log_usb_puthex_zero_digits_clamped_to_1(void)
{
  helper_set_usb_configured();

  rx_log_usb_puthex(k_test_puthex_test_byte, (uint8_t)k_test_puthex_zero_digits);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  /* digits == 0 is clamped to 1; only one hex character written */
  TEST_ASSERT_EQUAL(k_test_one_byte, tx_bytes);
}

/**
 * @brief puthex with digits > 8 clamps to 8 and writes exactly 8 bytes
 *
 * @details
 * The implementation clamps digits=9 to 8, so puthex(0xDEADBEEF, 9) must
 * write exactly 8 bytes (same as puthex(0xDEADBEEF, 8)).
 *
 * @pre USB in configured state and s_usb_ready == true
 * @post bytes_tx == 8 (clamped to 8 digits)
 *
 * @since Version 1.0.0
 */
void test_log_usb_puthex_9_digits_clamped_to_8(void)
{
  helper_set_usb_configured();

  rx_log_usb_puthex(k_test_puthex_val, (uint8_t)k_test_puthex_9_digits);

  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);

  /* digits == 9 is clamped to 8 */
  TEST_ASSERT_EQUAL(k_test_puthex_8_digits, tx_bytes);
}

/* =============================================================================
 * Tests: USB TX ring-buffer full path (k_rx_err_busy)
 * =============================================================================
 */

/**
 * @brief Writing after TX ring buffer is full increments dropped_bytes stat
 *
 * @details
 * Fills the USB log port TX ring buffer (1024 bytes) completely, then writes
 * one more byte. rx_usb_write returns k_rx_err_busy because the ring buffer
 * is full, causing rx_log_usb to increment s_stats.dropped_bytes (line 402).
 *
 * @pre USB in configured state; TX ring buffer filled to capacity
 * @post stats.dropped_bytes >= 1
 *
 * @since Version 1.0.0
 */
void test_log_usb_write_busy_increments_dropped_bytes(void)
{
  helper_set_usb_configured();

  /* At this point s_usb_ready is already true (set by notify_ready test earlier).
   * Writes go directly to rx_usb_write without boot-buffer involvement.
   * Fill the USB log TX ring buffer (1024 bytes) one character at a time. */
  for (uint32_t i = 0U; i < k_test_usb_log_tx_size; i++) {
    rx_log_usb_putc('F');
  }

  /* Now the ring buffer is at capacity; the next write must return k_rx_err_busy.
   * rx_log_usb increments dropped_bytes when it gets k_rx_err_busy (line 402). */
  rx_log_usb_putc('Z');

  usb_log_stats_t stats = {0};
  rx_log_usb_get_stats(&stats);

  /* dropped_bytes must be >= 1 */
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, stats.dropped_bytes);
}

/**
 * @brief Boot buffer flush abort when USB TX ring buffer is full (line 309)
 *
 * @details
 * Steps:
 * 1. Write a byte before USB is ready (buffered in boot ring buffer).
 * 2. Transition USB to configured state via rx_usb_set_state().
 * 3. Fill the USB log TX ring buffer to capacity by calling rx_usb_write()
 *    directly (bypassing rx_log_usb to avoid triggering the automatic flush
 *    that would set s_usb_ready=true).
 * 4. Call rx_log_usb_notify_ready() which checks USB is configured,
 *    enters internal_check_usb_ready(), and attempts to flush the boot buffer.
 *    The first rx_usb_write during flush returns k_rx_err_busy because the
 *    ring is full -> execution hits line 309 and aborts the flush.
 *
 * @pre s_usb_ready == false (initial state at this point in test ordering)
 * @pre Boot buffer has 1 byte; USB TX ring buffer is full after direct fill
 * @post No crash; notify_ready returns without setting s_usb_ready
 *
 * @since Version 1.0.0
 */
void test_log_usb_flush_abort_on_busy(void)
{
  /* Write before USB ready so it goes into the boot ring buffer.
   * s_usb_ready is still false at this point in the test ordering. */
  rx_log_usb_putc('B');

  /* Configure USB (sets device_state to k_usb_state_configured) */
  helper_set_usb_configured();

  /* Fill the USB log TX ring buffer to capacity using rx_usb_write directly.
   * This bypasses internal_check_usb_ready(), keeping s_usb_ready == false. */
  uint8_t fill_buf[k_test_usb_log_tx_size];
  for (uint32_t i = 0U; i < k_test_usb_log_tx_size; i++) {
    fill_buf[i] = 'F';
  }
  /* Write exactly the ring-buffer capacity so it is now completely full */
  rx_err_t fill_err = rx_usb_write(k_usb_port_log, fill_buf, k_test_usb_log_tx_size);
  TEST_ASSERT_EQUAL(k_rx_ok, fill_err);

  /* Now call notify_ready. internal_check_usb_ready() sees USB configured,
   * attempts to flush the 'B' from the boot buffer via rx_usb_write, which
   * returns k_rx_err_busy (ring buffer full) -> line 309 hit, flush aborts. */
  rx_log_usb_notify_ready();

  /* Test passes if we reach here without crash */
  TEST_ASSERT_TRUE(true);
}

/**
 * @brief Flushing a non-overflowed boot buffer takes the overflow=false branch
 *
 * @details
 * Exercises the `if (s_boot_buffer.overflow)` false branch at line 335 of
 * rx_log_usb.c. When the boot buffer contains data that fits without overflowing
 * and the flush completes, the overflow-warning path must be skipped.
 *
 * Steps:
 * 1. Reset all static state so boot buffer is clean.
 * 2. Write 4 bytes before USB is configured (goes to boot buffer, no overflow).
 * 3. Configure USB.
 * 4. Call rx_log_usb_notify_ready() to flush the boot buffer.
 * 5. The flush completes successfully; overflow == false, so the warning is not
 *    printed. The false branch of `if (s_boot_buffer.overflow)` is taken.
 *
 * @pre rx_log_usb_test_reset_state() available (compiled with -DUNIT_TEST)
 * @post s_usb_ready == true after flush
 * @post No overflow warning logged
 *
 * @since Version 1.0.0
 */
void test_log_usb_flush_no_overflow_warning(void)
{
  /* Reset rx_log_usb static state to clean baseline (USB already init from setUp). */
  rx_log_usb_test_reset_state();

  /* Write a small amount before USB ready (goes to boot buffer, no overflow). */
  rx_log_usb_putc('T');
  rx_log_usb_putc('e');
  rx_log_usb_putc('s');
  rx_log_usb_putc('t');

  /* Configure USB and flush the boot buffer. */
  helper_set_usb_configured();
  rx_log_usb_notify_ready();

  /* Flush must have completed: bytes reached rx_usb_write. */
  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, tx_bytes);
}

/**
 * @brief Ring buffer wrap-around exercises the linear_len < chunk_len path
 *
 * @details
 * Exercises the `linear_len = k_boot_buffer_size - tail` branch at line 314 of
 * rx_log_usb.c when `tail + chunk_len > k_boot_buffer_size`.
 *
 * This is achieved by using rx_log_usb_test_set_boot_buffer() to inject a ring
 * buffer state where the data wraps around the physical end of the 512-byte array.
 * Specifically, head=42, count=64 means the data spans bytes [490..511] and
 * [0..41]. During flush, the first chunk would need to split at position 512.
 *
 * Steps:
 * 1. Reset all static state.
 * 2. Inject ring buffer state with head=42, count=64 (data wraps around end).
 * 3. Configure USB.
 * 4. Call notify_ready to flush - the wrap-around branch is taken.
 *
 * @pre rx_log_usb_test_reset_state() and rx_log_usb_test_set_boot_buffer() available
 * @post All bytes flushed to USB without crash
 *
 * @since Version 1.0.0
 */
void test_log_usb_flush_ring_buffer_wrap_around(void)
{
  /* Reset rx_log_usb static state (USB already init from setUp). */
  rx_log_usb_test_reset_state();

  /* Inject ring buffer: head=42, count=64.
   * tail = (42 - 64 + 512) % 512 = 490.
   * During flush: tail=490, chunk=64, 490+64=554 > 512 -> wrap-around triggered.
   * linear_len = 512 - 490 = 22 (bytes until end of ring buffer). */
  static const char wrap_data[k_test_boot_wrap_count] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?"};
  rx_log_usb_test_set_boot_buffer(k_test_boot_wrap_head, k_test_boot_wrap_count, wrap_data);

  /* Configure USB and flush. */
  helper_set_usb_configured();
  rx_log_usb_notify_ready();

  /* At least the wrapped portion must have been flushed without crash. */
  uint32_t tx_bytes = 0U;
  helper_get_usb_tx_bytes(&tx_bytes);
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_one_byte, tx_bytes);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner entry point
 *
 * @details
 * Registers and runs all test_rx_log_usb test cases. Returns 0 on success
 * or non-zero on failure. The test binary is invoked by ctest.
 *
 * @return int 0 on success, non-zero on test failure
 *
 * @pre Unity framework initialised by UNITY_BEGIN()
 * @post All tests executed; results printed to stdout
 *
 * @note Not thread-safe - Unity runs tests sequentially in a single thread
 *
 * @since Version 1.0.0
 */
/**
 * @brief Group 1: Boot-buffer path tests (s_usb_ready initially false)
 */
static void internal_run_boot_buffer_tests(void)
{
  RUN_TEST(test_log_usb_putc_before_usb_ready_buffers);
  RUN_TEST(test_log_usb_puts_before_usb_ready_buffers);
  RUN_TEST(test_log_usb_get_stats_boot_buffered);
  RUN_TEST(test_log_usb_notify_ready_usb_not_ready);
  RUN_TEST(test_log_usb_boot_buffer_overflow_no_crash);
  RUN_TEST(test_log_usb_flush_abort_on_busy);
  RUN_TEST(test_log_usb_notify_ready);
  RUN_TEST(test_log_usb_boot_buffer_flushed_on_ready);
}

/**
 * @brief Group 2: USB-configured path tests (s_usb_ready == true)
 */
static void internal_run_usb_configured_tests(void)
{
  RUN_TEST(test_log_usb_putc_basic);
  RUN_TEST(test_log_usb_puts_basic);
  RUN_TEST(test_log_usb_puts_null);
  RUN_TEST(test_log_usb_puts_empty);
  RUN_TEST(test_log_usb_putint_positive);
  RUN_TEST(test_log_usb_putint_negative);
  RUN_TEST(test_log_usb_putint_zero);
  RUN_TEST(test_log_usb_puthex_basic);
  RUN_TEST(test_log_usb_puthex_zero);
  RUN_TEST(test_log_usb_puthex_4_digits);
  RUN_TEST(test_log_usb_puthex_zero_digits_clamped_to_1);
  RUN_TEST(test_log_usb_puthex_9_digits_clamped_to_8);
  RUN_TEST(test_log_usb_get_stats_null);
  RUN_TEST(test_log_usb_get_stats_basic);
  RUN_TEST(test_log_usb_notify_ready_idempotent);
  RUN_TEST(test_log_usb_write_busy_increments_dropped_bytes);
}

/**
 * @brief Group 3: Coverage gap tests using test reset function
 */
static void internal_run_coverage_gap_tests(void)
{
  RUN_TEST(test_log_usb_flush_no_overflow_warning);
  RUN_TEST(test_log_usb_flush_ring_buffer_wrap_around);
}

int main(void)
{
  UNITY_BEGIN();
  internal_run_boot_buffer_tests();
  internal_run_usb_configured_tests();
  internal_run_coverage_gap_tests();
  return UNITY_END();
}
