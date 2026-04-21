/**
 * @file test_rx_tmr.c
 * @brief Unit Tests for TMR HAL Driver (8-bit + 16-bit cascade)
 *
 * @details
 * Verifies the TMR HAL driver using mock registers:
 *  - Init for all 4 channels (8-bit independent)
 *  - Init for both cascaded pairs (TMR01, TMR23)
 *  - Start / stop / read counter (8-bit and 16-bit)
 *  - Period programming with 1 us / 1 ms / 1 s boundary cases
 *  - Compare-match ISR callback dispatch
 *  - Deinit cleanup (including paired odd channel for cascade)
 *  - Error paths: NULL config, invalid channel, odd-channel cascade
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mock_rx_onewire_hw.h"
#include "mock_rx_tmr.h"
#include "rx_tmr.h"
#include "unity.h"

/* Forward declaration of ISR dispatch hook (not in public header) */
void rx_tmr_isr_dispatch(rx_tmr_channel_t channel);

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Named constants for TMR test values */
typedef enum : uint8_t {
  k_tmr_test_mstpa5_bit      = 5, /**< Unit 0 module stop bit */
  k_tmr_test_mstpa4_bit      = 4, /**< Unit 1 module stop bit */
  k_tmr_test_invalid_channel = 4, /**< Out-of-range channel for error tests */
} tmr_test_bit_constants_t;

/** @brief Counter sample values used in read tests */
typedef enum : uint16_t {
  k_tmr_test_count_0x42   = 0x42U,
  k_tmr_test_count_0xABCD = 0xABCDU,
  k_tmr_test_count_0xFF   = 0xFFU,
} tmr_test_count_constants_t;

/** @brief Period constants for set_period_us tests (us) */
typedef enum : uint32_t {
  k_tmr_test_period_1us = 1U,       /**< 1 microsecond */
  k_tmr_test_period_1ms = 1000U,    /**< 1 millisecond */
  k_tmr_test_period_1s  = 1000000U, /**< 1 second      */
} tmr_test_period_constants_t;

/* =============================================================================
 * Mock Register Storage
 * =============================================================================
 */

/**
 * @brief Mock TMR unit buffers for tests (g_mock_tmr_unit[unit][byte])
 *
 * @details
 * Declared extern in mock_rx72n_tmr_regs.h; allocated here for each test
 * executable. Large enough (32 bytes per unit) to contain any access
 * made by tmr0()/tmr1() at offsets 0 and 1.
 */
uint8_t g_mock_tmr_unit[k_mock_tmr_unit_count][k_mock_tmr_unit_bytes];

/* =============================================================================
 * ISR Dispatch Test State
 * =============================================================================
 */

static volatile uint32_t         s_isr_calls_per_channel[k_tmr_channel_count];
static volatile rx_tmr_channel_t s_isr_last_channel;

/** @brief Test callback recording which channel invoked it. */
static void test_isr_callback(rx_tmr_channel_t channel)
{
  if ((uint8_t)channel < (uint8_t)k_tmr_channel_count) {
    s_isr_calls_per_channel[(uint8_t)channel]++;
  }
  s_isr_last_channel = channel;
}

/* =============================================================================
 * Fixtures
 * =============================================================================
 */

void setUp(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(g_mock_tmr_unit, 0, sizeof(g_mock_tmr_unit));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&g_mock_onewire_system_regs, 0, sizeof(g_mock_onewire_system_regs));

  /* Simulate both TMR units powered-down (reset default) */
  g_mock_onewire_system_regs.mstpcra =
    (1U << k_tmr_test_mstpa5_bit) | (1U << k_tmr_test_mstpa4_bit);

  /* Deinit all channels so repeated-init tests start fresh */
  (void)rx_tmr_deinit(k_tmr_channel_0);
  (void)rx_tmr_deinit(k_tmr_channel_1);
  (void)rx_tmr_deinit(k_tmr_channel_2);
  (void)rx_tmr_deinit(k_tmr_channel_3);

  /* Reset ISR bookkeeping and re-clear mock buffers after deinit writes */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset((void*)s_isr_calls_per_channel, 0, sizeof(s_isr_calls_per_channel));
  s_isr_last_channel = k_tmr_channel_0;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(g_mock_tmr_unit, 0, sizeof(g_mock_tmr_unit));
  g_mock_onewire_system_regs.mstpcra =
    (1U << k_tmr_test_mstpa5_bit) | (1U << k_tmr_test_mstpa4_bit);

  for (uint8_t ch = 0; ch < k_tmr_channel_count; ++ch) {
    (void)rx_tmr_register_compare_match_isr((rx_tmr_channel_t)ch, NULL);
  }
}

void tearDown(void)
{
  (void)rx_tmr_deinit(k_tmr_channel_0);
  (void)rx_tmr_deinit(k_tmr_channel_1);
  (void)rx_tmr_deinit(k_tmr_channel_2);
  (void)rx_tmr_deinit(k_tmr_channel_3);
}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

static rx_tmr_config_t make_default_config(rx_tmr_channel_t channel)
{
  rx_tmr_config_t cfg = {
    .channel       = channel,
    .mode          = k_tmr_mode_8bit_independent,
    .clock_source  = k_tmr_clock_pclk_div_1,
    .counter_clear = k_tmr_clear_cmp_match_a,
    .output_a      = k_tmr_output_no_change,
    .output_b      = k_tmr_output_no_change,
    .irq_mask      = k_tmr_irq_none,
  };
  return cfg;
}

void test_init_tmr0_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  /* MSTPA5 must be cleared (unit 0 clock enabled) */
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa5_bit));

  /* Counter halted after init (TCCR clock bits = 0) */
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);

  /* TCR CCLR = cmp match A */
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_tmr_tcr_cclr_cmp_match_a,
                          tmr0()->tcr & (uint8_t)k_tmr_tcr_cclr_mask);

  /* TCNT cleared */
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

void test_init_tmr1_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa5_bit));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr1()->tccr);
}

void test_init_tmr2_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  /* MSTPA4 must be cleared for unit 1 */
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa4_bit));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr2()->tccr);
}

void test_init_tmr3_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_3);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa4_bit));
}

void test_init_cascade_tmr01(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  /* Paired TMR1 must be programmed with CSS = cascade */
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_tmr_tccr_css_cascade,
                          tmr1()->tccr & (uint8_t)k_tmr_tccr_css_mask);
}

void test_init_cascade_tmr23(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_tmr_tccr_css_cascade,
                          tmr3()->tccr & (uint8_t)k_tmr_tccr_css_mask);
}

/* Error paths */

void test_init_null_config(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tmr_init(NULL));
}

void test_init_invalid_channel(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.channel = (rx_tmr_channel_t)k_tmr_test_invalid_channel;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
}

void test_init_cascade_on_odd_channel_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_1);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
}

void test_init_double_init_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tmr_init(&cfg));
}

/* =============================================================================
 * Start / Stop / Read Tests
 * =============================================================================
 */

void test_start_programs_tccr_clock_bits(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_32;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_0));

  const uint8_t expected_bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div32);
  TEST_ASSERT_EQUAL_UINT8(expected_bits,
                          tmr0()->tccr & (uint8_t)(k_tmr_tccr_css_mask | k_tmr_tccr_cks_mask));
}

void test_stop_clears_tccr_clock_bits(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_2));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_stop(k_tmr_channel_2));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr2()->tccr);
}

void test_read_8bit_counter(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  tmr0()->tcnt = (uint8_t)k_tmr_test_count_0x42;

  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_read(k_tmr_channel_0, &value));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_tmr_test_count_0x42, value);
}

void test_read_16bit_cascade_counter(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  /* Upper byte in even channel (TMR0), lower byte in odd channel (TMR1) */
  tmr0()->tcnt = 0xABU;
  tmr1()->tcnt = 0xCDU;

  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_read(k_tmr_channel_0, &value));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_tmr_test_count_0xABCD, value);
}

void test_read_null_ptr(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tmr_read(k_tmr_channel_0, NULL));
}

void test_read_uninitialised_channel(void)
{
  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tmr_read(k_tmr_channel_3, &value));
}

void test_start_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_start((rx_tmr_channel_t)k_tmr_test_invalid_channel));
}

void test_start_uninitialised_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tmr_start(k_tmr_channel_0));
}

/* =============================================================================
 * set_period_us Tests
 * =============================================================================
 */

void test_set_period_1us_at_pclk_div_1(void)
{
  /* PCLKB = 60 MHz, div 1 -> 60 ticks per us. TCORA = 60 - 1 = 59. */
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_1;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(59U, tmr0()->tcora);
}

void test_set_period_1ms_at_pclk_div_1024(void)
{
  /* PCLKB/1024 = 58594 Hz -> 58594 ticks/s -> 58.59 ticks/ms
   * tick = 1000 us * 60e6 / (1024 * 1e6) = 60000/1024 = 58.59375
   * TCORA = 58 (rounded down) - 1 = 57 */
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1ms));
  /* 60000000 * 1000 / (1024 * 1000000) = 58 (integer); TCORA = 58-1 = 57 */
  TEST_ASSERT_EQUAL_UINT8(57U, tmr0()->tcora);
}

void test_set_period_1s_in_cascade(void)
{
  /* At PCLK/8192 -> 7324.22 Hz. Ticks for 1s = 7324.
   * That fits in 16 bits (65535 max). TCORA = 7323 -> upper=0x1C, lower=0x9B */
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_8192;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_2, k_tmr_test_period_1s));

  /* ticks = 60e6 * 1e6 / (8192 * 1e6) = 7324 -> compare = 7323 = 0x1C9B */
  TEST_ASSERT_EQUAL_UINT8(0x1CU, tmr2()->tcora);
  TEST_ASSERT_EQUAL_UINT8(0x9BU, tmr3()->tcora);
}

void test_set_period_zero_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_set_period_us(k_tmr_channel_0, 0U));
}

void test_set_period_too_large_rejected(void)
{
  /* At PCLK/1: 1 ms needs 60000 ticks -> exceeds 8-bit max (255) */
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_1;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1ms));
}

void test_set_period_external_clock_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_external_rising;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
}

/* =============================================================================
 * ISR Dispatch Tests
 * =============================================================================
 */

void test_isr_dispatch_invokes_registered_callback(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask        = (uint8_t)k_tmr_irq_cmp_match_a;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_register_compare_match_isr(k_tmr_channel_0, test_isr_callback));

  rx_tmr_isr_dispatch(k_tmr_channel_0);
  TEST_ASSERT_EQUAL_UINT32(1U, s_isr_calls_per_channel[k_tmr_channel_0]);
  TEST_ASSERT_EQUAL(k_tmr_channel_0, s_isr_last_channel);
}

void test_isr_dispatch_unregistered_is_noop(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  /* No callback registered; dispatch must not invoke anything */
  rx_tmr_isr_dispatch(k_tmr_channel_1);
  TEST_ASSERT_EQUAL_UINT32(0U, s_isr_calls_per_channel[k_tmr_channel_1]);
}

void test_isr_dispatch_out_of_range_is_noop(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_tmr_isr_dispatch((rx_tmr_channel_t)k_tmr_test_invalid_channel);
  /* No assertion side-effect; just must not crash */
  TEST_PASS();
}

void test_isr_register_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_register_compare_match_isr((rx_tmr_channel_t)k_tmr_test_invalid_channel,
                                                      test_isr_callback));
}

/* =============================================================================
 * Deinit Tests
 * =============================================================================
 */

void test_deinit_clears_registers(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask        = (uint8_t)k_tmr_irq_cmp_match_a;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_0));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_deinit(k_tmr_channel_0));

  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

void test_deinit_cascade_clears_paired_odd_channel(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_tmr_tccr_css_cascade,
                          tmr1()->tccr & (uint8_t)k_tmr_tccr_css_mask);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_deinit(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr1()->tccr);
}

void test_deinit_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_deinit((rx_tmr_channel_t)k_tmr_test_invalid_channel));
}

/* =============================================================================
 * Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_init_tmr0_independent);
  RUN_TEST(test_init_tmr1_independent);
  RUN_TEST(test_init_tmr2_independent);
  RUN_TEST(test_init_tmr3_independent);
  RUN_TEST(test_init_cascade_tmr01);
  RUN_TEST(test_init_cascade_tmr23);
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_invalid_channel);
  RUN_TEST(test_init_cascade_on_odd_channel_rejected);
  RUN_TEST(test_init_double_init_rejected);

  RUN_TEST(test_start_programs_tccr_clock_bits);
  RUN_TEST(test_stop_clears_tccr_clock_bits);
  RUN_TEST(test_read_8bit_counter);
  RUN_TEST(test_read_16bit_cascade_counter);
  RUN_TEST(test_read_null_ptr);
  RUN_TEST(test_read_uninitialised_channel);
  RUN_TEST(test_start_invalid_channel);
  RUN_TEST(test_start_uninitialised_channel);

  RUN_TEST(test_set_period_1us_at_pclk_div_1);
  RUN_TEST(test_set_period_1ms_at_pclk_div_1024);
  RUN_TEST(test_set_period_1s_in_cascade);
  RUN_TEST(test_set_period_zero_rejected);
  RUN_TEST(test_set_period_too_large_rejected);
  RUN_TEST(test_set_period_external_clock_rejected);

  RUN_TEST(test_isr_dispatch_invokes_registered_callback);
  RUN_TEST(test_isr_dispatch_unregistered_is_noop);
  RUN_TEST(test_isr_dispatch_out_of_range_is_noop);
  RUN_TEST(test_isr_register_invalid_channel);

  RUN_TEST(test_deinit_clears_registers);
  RUN_TEST(test_deinit_cascade_clears_paired_odd_channel);
  RUN_TEST(test_deinit_invalid_channel);

  return UNITY_END();
}
