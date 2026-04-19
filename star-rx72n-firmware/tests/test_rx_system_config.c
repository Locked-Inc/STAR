/**
 * @file test_rx_system_config.c
 * @brief Unit tests for comm_task_apply_system_config() and k_system_config_default
 *
 * @details
 * The unified system configuration API carries a single @ref comm_channels
 * bitmask (UART / SPI / I2C). There is no longer a log-backend selector:
 * logs always flow through the rx_log_uart ring buffer and are transmitted as
 * @ref k_frame_type_log_message frames on the UART comm channel.
 *
 * Tests:
 * 1. NULL guard
 * 2. UART-only channel accepted
 * 3. SPI-only channel accepted
 * 4. I2C-only channel accepted
 * 5. All three channels accepted
 * 6. No channels (silent) accepted
 * 7. Unknown bits rejected
 * 8. k_system_config_default accepted
 *
 * @author Locked, Inc.
 * @date 2026-04-17
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "comm_task.h"
#include "unity.h"

/**
 * @enum test_invalid_mask_t
 * @brief Named constant for the out-of-range channel mask used in negative tests
 */
typedef enum : uint8_t {
  k_test_invalid_comm_bits = 0xF0U, /**< Bits above the highest defined channel bit */
} test_invalid_mask_t;

void setUp(void) {}

void tearDown(void) {}

/* 1. NULL guard */
void test_null_config_returns_null_ptr_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, comm_task_apply_system_config(nullptr));
}

/* 2. UART-only channel accepted */
void test_uart_only_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_uart,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* 3. SPI-only channel accepted */
void test_spi_only_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_spi,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* 4. I2C-only channel accepted */
void test_i2c_only_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_i2c,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* 5. All three channels accepted */
void test_all_channels_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_all,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* 6. No channels (silent) accepted */
void test_no_channels_accepted(void)
{
  const rx_system_config_t cfg = {
    .comm_channels = k_comm_channel_mask_none,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&cfg));
}

/* 7. Unknown bits rejected */
void test_unknown_comm_bits_rejected(void)
{
  const rx_system_config_t cfg = {
    .comm_channels =
      (rx_comm_channel_mask_t) /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */
    k_test_invalid_comm_bits,  /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */
  };
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, comm_task_apply_system_config(&cfg));
}

/* 8. Default config accepted */
void test_default_config_accepted(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, comm_task_apply_system_config(&k_system_config_default));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_null_config_returns_null_ptr_error);
  RUN_TEST(test_uart_only_accepted);
  RUN_TEST(test_spi_only_accepted);
  RUN_TEST(test_i2c_only_accepted);
  RUN_TEST(test_all_channels_accepted);
  RUN_TEST(test_no_channels_accepted);
  RUN_TEST(test_unknown_comm_bits_rejected);
  RUN_TEST(test_default_config_accepted);
  return UNITY_END();
}
