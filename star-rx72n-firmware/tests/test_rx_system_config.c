/**
 * @file test_rx_system_config.c
 * @brief Unit tests for comm_task_apply_system_config() and k_system_config_default
 *
 * @details
 * Validates the unified system configuration API that coordinates comm channel
 * selection with log backend selection and enforces the SCI9 conflict rule:
 * UART comm (binary frames on SCI9) and UART log (ASCII text on SCI9) cannot
 * both be active simultaneously.
 *
 * **Test categories:**
 * 1. NULL guard -- NULL config pointer returns k_rx_err_null_ptr
 * 2. SCI9 conflict -- UART comm + UART log returns k_rx_err_invalid_arg
 * 3. Valid UART comm -- UART comm + USB log returns k_rx_ok
 * 4. Valid UART log -- USB comm + UART log returns k_rx_ok
 * 5. All channels -- all 4 comm channels + USB log returns k_rx_ok
 * 6. All disabled -- no channels, no log returns k_rx_ok
 * 7. Unknown comm bits -- rejected with k_rx_err_invalid_arg
 * 8. Unknown log bits -- rejected with k_rx_err_invalid_arg
 * 9. Default config -- k_system_config_default passes without error
 * 10. Backend apply -- successful apply updates rx_log_get_backend()
 * 11. Rejection atomicity -- backend unchanged after rejected apply
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "comm_task.h"
#include "rx_log.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_invalid_mask_t
 * @brief Named constants for invalid bitmask values used in negative tests
 *
 * @details
 * Replaces raw hex literals in test_unknown_comm_bits_rejected and
 * test_unknown_log_bits_rejected so the intent of each pattern is clear.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_invalid_comm_bits = 0xF0u, /**< Bits above bit 3 -- all unknown comm bits set */
  k_test_invalid_log_bits  = 0xFCu, /**< Bits above bit 1 -- all unknown log bits set */
} test_invalid_mask_t;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

/**
 * @brief Reset log backend to UART default before each test
 *
 * @details
 * comm_task_apply_system_config() stores state in two static variables:
 * s_log_backend (rx_log.c) and s_enabled_channels (comm_task.c).
 * Resetting s_log_backend to the safe default ensures test-order independence
 * for the backend assertions.  s_enabled_channels is an output that each test
 * sets explicitly via apply_system_config(), so no reset is needed.
 */
void setUp(void)
{
  (void)rx_log_set_backend(k_log_backend_uart);
}

/** @brief No teardown required */
void tearDown(void) {}

/* =============================================================================
 * 1. NULL Guard
 * =============================================================================
 */

/**
 * @brief NULL config pointer returns k_rx_err_null_ptr
 *
 * @details
 * Verifies RX_CHECK_NULL_PTR guard at the top of the function.  No state
 * must change when this error is returned.
 */
void test_null_config_returns_null_ptr_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, comm_task_apply_system_config(nullptr));
}

/* =============================================================================
 * 2. SCI9 Conflict Detection
 * =============================================================================
 */

/**
 * @brief UART comm + UART log combination is rejected (SCI9 conflict)
 *
 * @details
 * Both rx_uart_comm (binary frames) and uart_debug_* (ASCII logs) write to
 * SCI9.  The function must detect this conflict and return
 * k_rx_err_invalid_arg without modifying any state.
 */
void test_uart_comm_plus_uart_log_is_rejected(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_uart,
    .log_backends  = k_log_backend_uart,
  };
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 3. Valid UART Comm (no UART log)
 * =============================================================================
 */

/**
 * @brief UART comm + USB log is accepted (no SCI9 conflict)
 *
 * @details
 * Binary frames on SCI9 with ASCII logs only on USB CDC Port 2 is a valid
 * configuration -- SCI9 is used exclusively by the comm channel.
 */
void test_uart_comm_plus_usb_log_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_uart,
    .log_backends  = k_log_backend_usb,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 4. Valid UART Log (no UART comm)
 * =============================================================================
 */

/**
 * @brief USB comm + UART log is accepted (no SCI9 conflict)
 *
 * @details
 * USB-only comm with ASCII logs on SCI9 is valid: SCI9 is used exclusively
 * by the log backend, no binary framing conflicts.
 */
void test_usb_comm_plus_uart_log_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_usb,
    .log_backends  = k_log_backend_uart,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 5. All Comm Channels + USB Log
 * =============================================================================
 */

/**
 * @brief All four comm channels with USB-only log is accepted
 *
 * @details
 * USB, SPI, I2C, and UART comm channels are all enabled.  Log output is
 * routed exclusively to USB CDC Port 2, so SCI9 is consumed only by the
 * UART comm channel (no conflict with the UART log backend).
 */
void test_all_comm_channels_usb_log_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_all,
    .log_backends  = k_log_backend_usb,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 6. All Disabled
 * =============================================================================
 */

/**
 * @brief No comm channels and no log backend (silent mode) is accepted
 *
 * @details
 * Zero bits in both fields is a valid silent configuration used in
 * production when logging is not needed and no channel is yet assigned.
 */
void test_no_channels_no_log_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_none,
    .log_backends  = k_log_backend_none,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 7. Unknown Comm Bits
 * =============================================================================
 */

/**
 * @brief Unknown bits in comm_channels are rejected
 *
 * @details
 * Any bit set above bit 3 (the UART channel bit) is undefined and must be
 * rejected early to catch caller mistakes.
 */
void test_unknown_comm_bits_rejected(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = (rx_comm_channel_mask_t)k_test_invalid_comm_bits,
    .log_backends  = k_log_backend_usb,
  };
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 8. Unknown Log Bits
 * =============================================================================
 */

/**
 * @brief Unknown bits in log_backends are rejected
 *
 * @details
 * Any bit set above bit 1 (the USB log bit) is undefined and must be
 * rejected early before any state is modified.
 */
void test_unknown_log_bits_rejected(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_usb,
    .log_backends  = (rx_log_backend_t)k_test_invalid_log_bits,
  };
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, comm_task_apply_system_config(&cfg));
}

/* =============================================================================
 * 9. Default Config
 * =============================================================================
 */

/**
 * @brief k_system_config_default passes validation without error
 *
 * @details
 * The compile-time default (USB+SPI+I2C comm, UART+USB log) must be
 * immediately usable without any manual configuration.  This is the
 * safe production configuration that excludes UART comm to keep SCI9
 * free for ASCII debug output.
 */
void test_default_config_accepted(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&k_system_config_default));
}

/* =============================================================================
 * 10. Backend Apply
 * =============================================================================
 */

/**
 * @brief Successful apply updates rx_log_get_backend() to the new value
 *
 * @details
 * Verifies that comm_task_apply_system_config() calls rx_log_set_backend()
 * internally: after a successful apply the active backend must equal
 * config->log_backends.
 */
void test_successful_apply_updates_log_backend(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_usb,
    .log_backends  = k_log_backend_usb,
  };
  (void)comm_task_apply_system_config(&cfg);
  TEST_ASSERT_EQUAL(k_log_backend_usb, rx_log_get_backend());
}

/* =============================================================================
 * 11. Rejection Atomicity
 * =============================================================================
 */

/**
 * @brief Backend is unchanged when comm_task_apply_system_config() is rejected
 *
 * @details
 * Sets the backend to USB-only via a valid apply, then attempts an invalid
 * apply (SCI9 conflict).  Verifies the backend is still USB-only after the
 * rejected call (no partial state update).
 */
void test_backend_unchanged_after_rejected_apply(void)
{
  const rx_system_config_t good_cfg = {
    .comm_channels = k_comm_channel_mask_usb,
    .log_backends  = k_log_backend_usb,
  };
  (void)comm_task_apply_system_config(&good_cfg);

  const rx_system_config_t bad_cfg = {
    .comm_channels = k_comm_channel_mask_uart,
    .log_backends  = k_log_backend_uart,
  };
  (void)comm_task_apply_system_config(&bad_cfg);

  TEST_ASSERT_EQUAL(k_log_backend_usb, rx_log_get_backend());
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner for comm_task_apply_system_config() tests
 *
 * @return int 0 on success, non-zero if any test fails
 */
int main(void)
{
  UNITY_BEGIN();

  /* 1. NULL guard */
  RUN_TEST(test_null_config_returns_null_ptr_error);

  /* 2. SCI9 conflict */
  RUN_TEST(test_uart_comm_plus_uart_log_is_rejected);

  /* 3. Valid UART comm */
  RUN_TEST(test_uart_comm_plus_usb_log_accepted);

  /* 4. Valid UART log */
  RUN_TEST(test_usb_comm_plus_uart_log_accepted);

  /* 5. All channels */
  RUN_TEST(test_all_comm_channels_usb_log_accepted);

  /* 6. All disabled */
  RUN_TEST(test_no_channels_no_log_accepted);

  /* 7. Unknown comm bits */
  RUN_TEST(test_unknown_comm_bits_rejected);

  /* 8. Unknown log bits */
  RUN_TEST(test_unknown_log_bits_rejected);

  /* 9. Default config */
  RUN_TEST(test_default_config_accepted);

  /* 10. Backend apply */
  RUN_TEST(test_successful_apply_updates_log_backend);

  /* 11. Rejection atomicity */
  RUN_TEST(test_backend_unchanged_after_rejected_apply);

  return UNITY_END();
}
