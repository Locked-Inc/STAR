/**
 * @file test_rx_lpc.c
 * @brief Unit Tests for Low Power Consumption (LPC) HAL Driver
 *
 * @details
 * **Host-side Unity test suite** for the RX72N Low Power Consumption driver
 * (rx_lpc.c / rx_lpc.h). Because every hardware register write in the
 * driver is guarded by `#ifdef __RX__`, the host build is driven entirely
 * through the driver's own UNIT_TEST helpers:
 *
 * - rx_lpc_test_reset()                 - factory-reset the driver
 * - rx_lpc_test_get_last_mode()         - inspect which mode entry ran
 * - rx_lpc_test_set_deep_standby_wake() - inject "reset was a DSBY wake"
 * - rx_lpc_test_set_pending_wake_flags() - inject DPSIFR0..3 bitmask
 *
 * ## Coverage
 *
 * | Area                               | # Tests |
 * |------------------------------------|---------|
 * | Initialization + re-init           | 2       |
 * | API-before-init guard              | 6       |
 * | rx_lpc_set_operating_power         | 5       |
 * | rx_lpc_enter_sleep                 | 1       |
 * | rx_lpc_enter_software_standby      | 1       |
 * | rx_lpc_enter_deep_software_standby | 6       |
 * | rx_lpc_was_deep_standby_wake       | 2       |
 * | rx_lpc_get_wake_flags              | 3       |
 *
 * ## NASA Power of 10 Considerations
 *
 * - All tests are bounded, straight-line (Rules 1, 2)
 * - No dynamic allocation (Rule 3)
 * - Each test < 40 lines (Rule 4)
 * - Each test asserts explicit pre-conditions via setUp + explicit
 *   post-conditions via TEST_ASSERT_* (Rule 5)
 * - Variables at smallest scope (Rule 6)
 * - All rx_err_t results checked (Rule 7)
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdbool.h>
#include <stdint.h>

#include "mock_rx_lpc.h"
#include "rx_err.h"
#include "rx_lpc.h"
#include "unity.h"

/* =============================================================================
 * Test fixture helpers
 * =============================================================================
 */

/**
 * @brief Reset driver state before each test
 *
 * @post Driver is uninitialized; last-mode is k_lpc_mode_none; no injected
 *       wake state is carried over from the previous test.
 */
static void test_setup(void)
{
  mock_rx_lpc_reset();
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief rx_lpc_init() returns k_rx_ok on fresh state
 */
static void test_init_success(void)
{
  test_setup();

  rx_err_t err = rx_lpc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((int)k_lpc_mode_none, (int)rx_lpc_test_get_last_mode());
}

/**
 * @brief rx_lpc_init() clears injected wake state after latching it
 *
 * @details
 * Injecting a wake value, calling init, then re-initing must not re-surface
 * the value on the second init (injected state is one-shot per init).
 */
static void test_init_consumes_injected_state(void)
{
  test_setup();

  rx_lpc_test_set_deep_standby_wake(true);
  rx_lpc_test_set_pending_wake_flags(k_lpc_wake_rtc_alarm);

  rx_err_t err = rx_lpc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_lpc_was_deep_standby_wake());

  /* Second init must start clean */
  err = rx_lpc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(rx_lpc_was_deep_standby_wake());

  uint32_t flags = 0xDEADBEEFU;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_get_wake_flags(&flags));
  TEST_ASSERT_EQUAL_UINT32(0U, flags);
}

/* =============================================================================
 * API-before-init guards
 * =============================================================================
 */

static void test_set_operating_power_before_init_returns_not_initialized(void)
{
  test_setup();

  rx_err_t err = rx_lpc_set_operating_power(k_lpc_opcc_high_speed);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_enter_sleep_before_init_returns_not_initialized(void)
{
  test_setup();

  rx_err_t err = rx_lpc_enter_sleep();
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_enter_software_standby_before_init_returns_not_initialized(void)
{
  test_setup();

  rx_err_t err = rx_lpc_enter_software_standby();
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_enter_deep_standby_before_init_returns_not_initialized(void)
{
  test_setup();

  rx_err_t err = rx_lpc_enter_deep_software_standby(k_lpc_wake_irq0, k_lpc_deep_ram_usb_on, false);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_get_wake_flags_before_init_returns_not_initialized(void)
{
  test_setup();

  uint32_t flags = 0U;
  rx_err_t err   = rx_lpc_get_wake_flags(&flags);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_was_deep_standby_wake_before_init_is_false(void)
{
  test_setup();

  /* Querying before init is permitted; default is false */
  TEST_ASSERT_FALSE(rx_lpc_was_deep_standby_wake());
}

/* =============================================================================
 * rx_lpc_set_operating_power
 * =============================================================================
 */

static void test_set_operating_power_high_speed_succeeds(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_set_operating_power(k_lpc_opcc_high_speed);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Setting power mode does not count as a low-power mode entry */
  TEST_ASSERT_EQUAL((int)k_lpc_mode_none, (int)rx_lpc_test_get_last_mode());
}

static void test_set_operating_power_low_speed_1_succeeds(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_set_operating_power(k_lpc_opcc_low_speed_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_set_operating_power_low_speed_2_succeeds(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_set_operating_power(k_lpc_opcc_low_speed_2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_set_operating_power_invalid_enum_returns_invalid_arg(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  /* A value that is not any of the three defined enumerators */
  rx_err_t err = rx_lpc_set_operating_power((rx_lpc_opcc_mode_t)0xFFU);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_set_operating_power_does_not_pollute_last_mode(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_enter_sleep());
  TEST_ASSERT_EQUAL((int)k_lpc_mode_sleep, (int)rx_lpc_test_get_last_mode());

  /* OPCCR change must not clobber the last-entered-mode tracker */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_set_operating_power(k_lpc_opcc_high_speed));
  TEST_ASSERT_EQUAL((int)k_lpc_mode_sleep, (int)rx_lpc_test_get_last_mode());
}

/* =============================================================================
 * rx_lpc_enter_sleep
 * =============================================================================
 */

static void test_enter_sleep_records_last_mode(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_enter_sleep();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((int)k_lpc_mode_sleep, (int)rx_lpc_test_get_last_mode());
}

/* =============================================================================
 * rx_lpc_enter_software_standby
 * =============================================================================
 */

static void test_enter_software_standby_records_last_mode(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_enter_software_standby();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((int)k_lpc_mode_software_standby, (int)rx_lpc_test_get_last_mode());
}

/* =============================================================================
 * rx_lpc_enter_deep_software_standby
 * =============================================================================
 */

static void test_enter_deep_standby_zero_mask_returns_invalid_arg(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_enter_deep_software_standby(0U, k_lpc_deep_ram_usb_on, false);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  /* No state transition on error */
  TEST_ASSERT_EQUAL((int)k_lpc_mode_none, (int)rx_lpc_test_get_last_mode());
}

static void test_enter_deep_standby_unknown_bits_returns_invalid_arg(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  /* Bit 31 is not assigned in DPSIER3 (only bit 24 = CAN1 RX) */
  const uint32_t bad_mask = k_lpc_wake_irq0 | 0x80000000U;
  rx_err_t       err = rx_lpc_enter_deep_software_standby(bad_mask, k_lpc_deep_ram_usb_on, false);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_enter_deep_standby_invalid_power_returns_invalid_arg(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err =
    rx_lpc_enter_deep_software_standby(k_lpc_wake_irq0, (rx_lpc_deep_power_t)0xFFU, false);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_enter_deep_standby_success_records_last_mode(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  const uint32_t mask = k_lpc_wake_irq0 | k_lpc_wake_rtc_alarm;
  rx_err_t       err  = rx_lpc_enter_deep_software_standby(mask, k_lpc_deep_ram_usb_off, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((int)k_lpc_mode_deep_software_standby, (int)rx_lpc_test_get_last_mode());
}

static void test_enter_deep_standby_accepts_full_wake_mask(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_enter_deep_software_standby(k_lpc_wake_all_mask, k_lpc_deep_lvd_off, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_enter_deep_standby_accepts_each_deep_power_level(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_lpc_enter_deep_software_standby(k_lpc_wake_irq0, k_lpc_deep_ram_usb_on, false));
  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_lpc_enter_deep_software_standby(k_lpc_wake_irq0, k_lpc_deep_ram_usb_off, false));
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_lpc_enter_deep_software_standby(k_lpc_wake_irq0, k_lpc_deep_lvd_off, false));
}

/* =============================================================================
 * rx_lpc_was_deep_standby_wake
 * =============================================================================
 */

static void test_was_deep_standby_wake_default_is_false(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());
  TEST_ASSERT_FALSE(rx_lpc_was_deep_standby_wake());
}

static void test_was_deep_standby_wake_reports_injected_true(void)
{
  test_setup();
  rx_lpc_test_set_deep_standby_wake(true);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());
  TEST_ASSERT_TRUE(rx_lpc_was_deep_standby_wake());
}

/* =============================================================================
 * rx_lpc_get_wake_flags
 * =============================================================================
 */

static void test_get_wake_flags_null_returns_invalid_arg(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  rx_err_t err = rx_lpc_get_wake_flags(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_get_wake_flags_default_is_zero(void)
{
  test_setup();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  uint32_t flags = 0xFFFFFFFFU;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_get_wake_flags(&flags));
  TEST_ASSERT_EQUAL_UINT32(0U, flags);
}

static void test_get_wake_flags_returns_injected_value(void)
{
  test_setup();

  const uint32_t injected = k_lpc_wake_irq3 | k_lpc_wake_rtc_alarm | k_lpc_wake_can1_rx;
  rx_lpc_test_set_pending_wake_flags(injected);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_init());

  uint32_t flags = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_lpc_get_wake_flags(&flags));
  TEST_ASSERT_EQUAL_UINT32(injected, flags);
}

/* =============================================================================
 * Unity harness
 * =============================================================================
 */

void setUp(void)
{
  test_setup();
}

void tearDown(void) {}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_init_success);
  RUN_TEST(test_init_consumes_injected_state);

  RUN_TEST(test_set_operating_power_before_init_returns_not_initialized);
  RUN_TEST(test_enter_sleep_before_init_returns_not_initialized);
  RUN_TEST(test_enter_software_standby_before_init_returns_not_initialized);
  RUN_TEST(test_enter_deep_standby_before_init_returns_not_initialized);
  RUN_TEST(test_get_wake_flags_before_init_returns_not_initialized);
  RUN_TEST(test_was_deep_standby_wake_before_init_is_false);

  RUN_TEST(test_set_operating_power_high_speed_succeeds);
  RUN_TEST(test_set_operating_power_low_speed_1_succeeds);
  RUN_TEST(test_set_operating_power_low_speed_2_succeeds);
  RUN_TEST(test_set_operating_power_invalid_enum_returns_invalid_arg);
  RUN_TEST(test_set_operating_power_does_not_pollute_last_mode);

  RUN_TEST(test_enter_sleep_records_last_mode);
  RUN_TEST(test_enter_software_standby_records_last_mode);

  RUN_TEST(test_enter_deep_standby_zero_mask_returns_invalid_arg);
  RUN_TEST(test_enter_deep_standby_unknown_bits_returns_invalid_arg);
  RUN_TEST(test_enter_deep_standby_invalid_power_returns_invalid_arg);
  RUN_TEST(test_enter_deep_standby_success_records_last_mode);
  RUN_TEST(test_enter_deep_standby_accepts_full_wake_mask);
  RUN_TEST(test_enter_deep_standby_accepts_each_deep_power_level);

  RUN_TEST(test_was_deep_standby_wake_default_is_false);
  RUN_TEST(test_was_deep_standby_wake_reports_injected_true);

  RUN_TEST(test_get_wake_flags_null_returns_invalid_arg);
  RUN_TEST(test_get_wake_flags_default_is_zero);
  RUN_TEST(test_get_wake_flags_returns_injected_value);

  return UNITY_END();
}
