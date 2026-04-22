/**
 * @file test_rx_tmr.c
 * @brief Unit Tests for TMR HAL Driver (8-bit + 16-bit cascade)
 *
 * @details
 * Verifies the TMR HAL driver using mock registers:
 *  - Init for all 4 channels (8-bit independent) and both cascaded pairs
 *  - Every rx_tmr_clock_source_t value is exercised in init/start so the
 *    TCCR CSS|CKS composition is verified bit-for-bit
 *  - Every rx_tmr_counter_clear_t value is exercised in init so the TCR
 *    CCLR field composition is verified
 *  - Every rx_tmr_irq_source_t bit is exercised in init so the TCR
 *    CMIEA/CMIEB/OVIE fields are verified
 *  - Every PCLK divider in rx_tmr_set_period_us() is exercised end-to-end
 *    so the TCORA write arithmetic is pinned against the reference formula
 *    from RX72N Group user's manual chapter 32 (8-Bit Timer TMR)
 *  - Start / stop / read for 8-bit and 16-bit cascade, both TMR01 and TMR23
 *  - Compare-match ISR callback dispatch (registered, unregistered, out of
 *    range)
 *  - Deinit cleanup (including paired odd channel for cascade)
 *  - Every documented error-path return: NULL config, out-of-range channel,
 *    odd-channel cascade, already initialised, invalid enum fields,
 *    zero-period, too-large period, zero-ticks period, external clock
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware, chapter 32 "8-Bit Timer (TMR)",
 * TCR/TCSR/TCCR/TCORA/TCORB/TCNT register descriptions. Tests that pin
 * specific register-bit semantics cite the corresponding manual page in
 * their per-test documentation.
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

/** @brief Named bit and channel constants */
typedef enum : uint8_t {
  k_tmr_test_mstpa5_bit      = 5,  /**< Unit 0 module stop bit (TMR0/TMR1) */
  k_tmr_test_mstpa4_bit      = 4,  /**< Unit 1 module stop bit (TMR2/TMR3) */
  k_tmr_test_invalid_channel = 4,  /**< Out-of-range channel for error tests */
  k_tmr_test_invalid_mode    = 2,  /**< Out-of-range mode sentinel */
  k_tmr_test_invalid_clear   = 4,  /**< Out-of-range counter-clear sentinel */
  k_tmr_test_invalid_output  = 4,  /**< Out-of-range output sentinel */
  k_tmr_test_invalid_clock   = 10, /**< Out-of-range clock-source sentinel */
} tmr_test_bit_constants_t;

/** @brief 16-bit counter sample values used in cascade-read tests. Names
 * avoid '0x' hex-digit strings to satisfy readability-identifier-naming
 * (no embedded uppercase). */
typedef enum : uint16_t {
  k_tmr_test_count_0042      = 0x42U,
  k_tmr_test_count_abcd      = 0xABCDU,
  k_tmr_test_count_1234      = 0x1234U,
  k_tmr_test_count_16bit_cef = 0xCDEFU,
} tmr_test_count16_constants_t;

/** @brief 8-bit counter sample values used when writing TCNT directly. */
typedef enum : uint8_t {
  k_tmr_test_count_upper_cd = 0xCDU,
  k_tmr_test_count_lower_ef = 0xEFU,
  k_tmr_test_count_ab       = 0xABU,
  k_tmr_test_count_cd       = 0xCDU,
} tmr_test_count8_constants_t;

/** @brief Period constants for set_period_us tests (microseconds) */
typedef enum : uint32_t {
  k_tmr_test_period_1us  = 1U,       /**< 1 microsecond */
  k_tmr_test_period_8us  = 8U,       /**< 8 microseconds */
  k_tmr_test_period_32us = 32U,      /**< 32 microseconds */
  k_tmr_test_period_64us = 64U,      /**< 64 microseconds */
  k_tmr_test_period_1ms  = 1000U,    /**< 1 millisecond */
  k_tmr_test_period_1s   = 1000000U, /**< 1 second */
} tmr_test_period_constants_t;

/** @brief Expected TCORA values for set_period_us tests */
typedef enum : uint8_t {
  k_tmr_test_tcora_div1_1us    = 59U,   /**< 1us / (1/60MHz) - 1 = 60 - 1 = 59 */
  k_tmr_test_tcora_div2_1us    = 29U,   /**< 1us * 60 / 2 - 1 = 30 - 1 = 29 */
  k_tmr_test_tcora_div8_8us    = 59U,   /**< 8us * 60 / 8 - 1 = 60 - 1 = 59 */
  k_tmr_test_tcora_div32_32us  = 59U,   /**< 32us * 60 / 32 - 1 = 60 - 1 = 59 */
  k_tmr_test_tcora_div64_64us  = 59U,   /**< 64us * 60 / 64 - 1 = 60 - 1 = 59 */
  k_tmr_test_tcora_div1024_1ms = 57U,   /**< floor(60000/1024) - 1 = 58 - 1 = 57 */
  k_tmr_test_tcora_cascade_hi  = 0x1CU, /**< Upper byte of 7323 (0x1C9B) */
  k_tmr_test_tcora_cascade_lo  = 0x9BU, /**< Lower byte of 7323 (0x1C9B) */
} tmr_test_expected_tcora_t;

/** @brief TCR bit positions used for per-irq assertions */
typedef enum : uint8_t {
  k_tmr_test_tcr_cmieb_mask = (1U << 7),
  k_tmr_test_tcr_cmiea_mask = (1U << 6),
  k_tmr_test_tcr_ovie_mask  = (1U << 5),
} tmr_test_tcr_irq_mask_t;

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
  if ((uint8_t)channel < k_tmr_channel_count) {
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
 * Config Fixture
 * =============================================================================
 */

/**
 * @brief Build a safe default rx_tmr_config_t for @p channel.
 * @param[in] channel Channel identifier (caller picks even channel for cascade)
 * @return Initialised config suitable for rx_tmr_init()
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

/**
 * @brief Init @p channel with @p clock_source and start it so TCCR is written.
 * @param[in] channel      Channel to initialise
 * @param[in] clock_source Clock source to program
 *
 * @par Manual reference
 * RX72N UM Ch. 32 table "TCCR register" (clock select CKS and clock source
 * select CSS fields): tests using this helper verify that the HAL driver
 * composes the same CSS|CKS bit pattern as documented.
 */
static void init_and_start(rx_tmr_channel_t channel, rx_tmr_clock_source_t clock_source)
{
  rx_tmr_config_t cfg = make_default_config(channel);
  cfg.clock_source    = clock_source;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(channel));
}

/* =============================================================================
 * Init Tests -- Happy Paths
 * =============================================================================
 */

/**
 * @brief Init TMR0 in 8-bit independent mode clears MSTPA5 and TCNT.
 * @details Verifies module-stop clock release and counter halt after init.
 * @par Manual reference
 * RX72N UM Ch. 32, "Module Stop Function" (MSTPCRA.MSTPA5) and TCNT reset.
 * @since Version 1.0.0
 */
void test_init_tmr0_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa5_bit));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tcr_cclr_cmp_match_a, tmr0()->tcr & k_tmr_tcr_cclr_mask);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

/**
 * @brief Init TMR1 releases MSTPA5 and halts the counter.
 * @details TMR0 and TMR1 share unit 0; either channel's init releases MSTPA5.
 * @par Manual reference
 * RX72N UM Ch. 32, "Module Stop Function" MSTPCRA.MSTPA5.
 * @since Version 1.0.0
 */
void test_init_tmr1_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa5_bit));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr1()->tccr);
}

/**
 * @brief Init TMR2 releases the unit-1 MSTPA4 bit.
 * @par Manual reference
 * RX72N UM Ch. 32, "Module Stop Function" MSTPCRA.MSTPA4.
 * @since Version 1.0.0
 */
void test_init_tmr2_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa4_bit));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr2()->tccr);
}

/**
 * @brief Init TMR3 releases MSTPA4 (shared with TMR2).
 * @par Manual reference
 * RX72N UM Ch. 32, "Module Stop Function" MSTPCRA.MSTPA4.
 * @since Version 1.0.0
 */
void test_init_tmr3_independent(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_3);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_onewire_system_regs.mstpcra & (1U << k_tmr_test_mstpa4_bit));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr3()->tccr);
}

/**
 * @brief Init TMR01 cascade programs the odd channel with CSS=cascade.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" (TCCR.CSS = 11b on odd channel).
 * @since Version 1.0.0
 */
void test_init_cascade_tmr01(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tccr_css_cascade, tmr1()->tccr & k_tmr_tccr_css_mask);
}

/**
 * @brief Init TMR23 cascade programs TMR3 with CSS=cascade.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" (TCCR.CSS = 11b on odd channel).
 * @since Version 1.0.0
 */
void test_init_cascade_tmr23(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tccr_css_cascade, tmr3()->tccr & k_tmr_tccr_css_mask);
}

/* =============================================================================
 * Init Tests -- Counter-Clear Coverage
 * =============================================================================
 */

/**
 * @brief counter_clear = disabled leaves CCLR field = 00b in TCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCR register, CCLR[1:0].
 * @since Version 1.0.0
 */
void test_init_counter_clear_disabled(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.counter_clear   = k_tmr_clear_disabled;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tcr_cclr_disabled, tmr0()->tcr & k_tmr_tcr_cclr_mask);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

/**
 * @brief counter_clear = cmp_match_b programs CCLR=10b in TCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCR register, CCLR[1:0].
 * @since Version 1.0.0
 */
void test_init_counter_clear_cmp_match_b(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.counter_clear   = k_tmr_clear_cmp_match_b;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tcr_cclr_cmp_match_b, tmr0()->tcr & k_tmr_tcr_cclr_mask);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

/**
 * @brief counter_clear = external_reset programs CCLR=11b in TCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCR register, CCLR[1:0].
 * @since Version 1.0.0
 */
void test_init_counter_clear_external_reset(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.counter_clear   = k_tmr_clear_external_reset;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tcr_cclr_external_sig, tmr0()->tcr & k_tmr_tcr_cclr_mask);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

/* =============================================================================
 * Init Tests -- IRQ Mask Coverage
 * =============================================================================
 */

/**
 * @brief irq_mask with cmp_match_b sets TCR.CMIEB (bit 7).
 * @par Manual reference
 * RX72N UM Ch. 32, TCR register, CMIEB bit.
 * @since Version 1.0.0
 */
void test_init_irq_mask_cmp_match_b(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask        = k_tmr_irq_cmp_match_b;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcr_cmieb_mask, tmr0()->tcr & k_tmr_test_tcr_cmieb_mask);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr & k_tmr_test_tcr_ovie_mask);
}

/**
 * @brief irq_mask with overflow sets TCR.OVIE (bit 5).
 * @par Manual reference
 * RX72N UM Ch. 32, TCR register, OVIE bit.
 * @since Version 1.0.0
 */
void test_init_irq_mask_overflow(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask        = k_tmr_irq_overflow;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcr_ovie_mask, tmr0()->tcr & k_tmr_test_tcr_ovie_mask);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr & k_tmr_test_tcr_cmieb_mask);
}

/**
 * @brief irq_mask = cmp_match_a | cmp_match_b | overflow sets all 3 bits.
 * @par Manual reference
 * RX72N UM Ch. 32, TCR register, CMIEA/CMIEB/OVIE bits.
 * @since Version 1.0.0
 */
void test_init_irq_mask_all(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask = (uint8_t)(k_tmr_irq_cmp_match_a | k_tmr_irq_cmp_match_b | k_tmr_irq_overflow);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  const uint8_t expected =
    (uint8_t)(k_tmr_test_tcr_cmieb_mask | k_tmr_test_tcr_cmiea_mask | k_tmr_test_tcr_ovie_mask);
  TEST_ASSERT_EQUAL_UINT8(
    expected,
    tmr0()->tcr &
      (uint8_t)(k_tmr_test_tcr_cmieb_mask | k_tmr_test_tcr_cmiea_mask | k_tmr_test_tcr_ovie_mask));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tcr_cclr_cmp_match_a, tmr0()->tcr & k_tmr_tcr_cclr_mask);
}

/* =============================================================================
 * Init Tests -- Clock-Source Coverage (every enum value exercised)
 * =============================================================================
 */

/**
 * @brief Helper: init + start @p channel with @p clock_source, verify TCCR.
 * @param[in] channel       Channel to init/start
 * @param[in] clock_source  Clock-source enum value
 * @param[in] expected_css  Expected TCCR CSS field value
 * @param[in] expected_cks  Expected TCCR CKS field value
 */
static void assert_start_programs_tccr(rx_tmr_channel_t      channel,
                                       rx_tmr_clock_source_t clock_source,
                                       uint8_t               expected_css,
                                       uint8_t               expected_cks)
{
  init_and_start(channel, clock_source);

  volatile rx_tmr_channel_regs_t* regs = NULL;
  switch (channel) {
    case k_tmr_channel_0:
      regs = tmr0();
      break;
    case k_tmr_channel_1:
      regs = tmr1();
      break;
    case k_tmr_channel_2:
      regs = tmr2();
      break;
    case k_tmr_channel_3:
      regs = tmr3();
      break;
    default:
      TEST_FAIL_MESSAGE("Unreachable channel in test helper");
      break;
  }
  const uint8_t expected = (uint8_t)(expected_css | expected_cks);
  TEST_ASSERT_EQUAL_UINT8(expected,
                          regs->tccr & (uint8_t)(k_tmr_tccr_css_mask | k_tmr_tccr_cks_mask));
}

/**
 * @brief PCLK/1 clock source writes CSS=internal, CKS=001b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=001.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_1(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_1,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div1);
}

/**
 * @brief PCLK/2 clock source writes CSS=internal, CKS=010b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=010.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_2(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_2,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div2);
}

/**
 * @brief PCLK/8 clock source writes CSS=internal, CKS=011b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=011.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_8(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_8,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div8);
}

/**
 * @brief PCLK/32 clock source writes CSS=internal, CKS=100b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=100.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_32(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_32,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div32);
}

/**
 * @brief PCLK/64 clock source writes CSS=internal, CKS=101b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=101.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_64(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_64,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div64);
}

/**
 * @brief PCLK/1024 clock source writes CSS=internal, CKS=110b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=110.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_1024(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_1024,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div1024);
}

/**
 * @brief PCLK/8192 clock source writes CSS=internal, CKS=111b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=01 and CKS[2:0]=111.
 * @since Version 1.0.0
 */
void test_start_clock_pclk_div_8192(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_pclk_div_8192,
                             k_tmr_tccr_css_internal,
                             k_tmr_tccr_cks_pclk_div8192);
}

/**
 * @brief External-rising clock writes CSS=external, CKS=001b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=00 and CKS[2:0]=001.
 * @since Version 1.0.0
 */
void test_start_clock_external_rising(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_external_rising,
                             k_tmr_tccr_css_external,
                             k_tmr_tccr_cks_ext_rising);
}

/**
 * @brief External-falling clock writes CSS=external, CKS=010b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=00 and CKS[2:0]=010.
 * @since Version 1.0.0
 */
void test_start_clock_external_falling(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_external_falling,
                             k_tmr_tccr_css_external,
                             k_tmr_tccr_cks_ext_falling);
}

/**
 * @brief External-both-edges clock writes CSS=external, CKS=011b into TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register, CSS[1:0]=00 and CKS[2:0]=011.
 * @since Version 1.0.0
 */
void test_start_clock_external_both(void)
{
  assert_start_programs_tccr(k_tmr_channel_0,
                             k_tmr_clock_external_both,
                             k_tmr_tccr_css_external,
                             k_tmr_tccr_cks_ext_both);
}

/* =============================================================================
 * Init Tests -- Error Paths
 * =============================================================================
 */

/**
 * @brief NULL config is rejected with k_rx_err_null_ptr.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_null_ptr.
 * @since Version 1.0.0
 */
void test_init_null_config(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tmr_init(NULL));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Out-of-range channel is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_init_invalid_channel(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.channel = (rx_tmr_channel_t)k_tmr_test_invalid_channel;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Out-of-range mode is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_init_invalid_mode(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.mode = (rx_tmr_mode_t)k_tmr_test_invalid_mode;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Out-of-range counter_clear is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_init_invalid_counter_clear(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.counter_clear = (rx_tmr_counter_clear_t)k_tmr_test_invalid_clear;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Out-of-range output_a is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_init_invalid_output_a(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.output_a = (rx_tmr_output_t)k_tmr_test_invalid_output;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Out-of-range output_b is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_init_invalid_output_b(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.output_b = (rx_tmr_output_t)k_tmr_test_invalid_output;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Out-of-range clock_source is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_init_invalid_clock_source(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  cfg.clock_source = (rx_tmr_clock_source_t)k_tmr_test_invalid_clock;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
}

/**
 * @brief Cascade request on odd channel returns k_rx_err_invalid_arg.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode": only TMR01 and TMR23 pairs are
 * valid; TMR1 or TMR3 cannot be primaries.
 * @since Version 1.0.0
 */
void test_init_cascade_on_odd_channel_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_1);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr1()->tcr);
}

/**
 * @brief Double-init on the same channel returns k_rx_err_invalid_state.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_state.
 * @since Version 1.0.0
 */
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

/**
 * @brief rx_tmr_start() on a cascaded TMR01 re-asserts TMR1.TCCR=cascade.
 * @details
 * Ensures restart after a stop/start cycle preserves the odd-channel
 * cascade source select.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR.CSS=11b (cascade) required on odd channel.
 * @since Version 1.0.0
 */
void test_start_cascade_tmr01_reprograms_paired(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_stop(k_tmr_channel_0));

  tmr1()->tccr = 0U; /* Simulate post-stop register state */

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tccr_css_cascade, tmr1()->tccr & k_tmr_tccr_css_mask);
}

/**
 * @brief rx_tmr_start() on a cascaded TMR23 re-asserts TMR3.TCCR=cascade.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR.CSS=11b (cascade) required on odd channel.
 * @since Version 1.0.0
 */
void test_start_cascade_tmr23_reprograms_paired(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_stop(k_tmr_channel_2));

  tmr3()->tccr = 0U;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_2));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tccr_css_cascade, tmr3()->tccr & k_tmr_tccr_css_mask);
}

/**
 * @brief rx_tmr_start() writes CSS|CKS into TCCR after init.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR register CSS/CKS fields.
 * @since Version 1.0.0
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

/**
 * @brief rx_tmr_stop() clears TCCR clock bits on an independent channel.
 * @par Manual reference
 * RX72N UM Ch. 32, TCCR clock stop (CSS=00, CKS=000).
 * @since Version 1.0.0
 */
void test_stop_clears_tccr_clock_bits(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_2));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_stop(k_tmr_channel_2));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr2()->tccr);
}

/**
 * @brief rx_tmr_stop() on cascaded TMR01 also clears TMR1.TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" stop sequence: odd-channel TCCR
 * must be cleared in addition to even.
 * @since Version 1.0.0
 */
void test_stop_cascade_tmr01_clears_paired(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_0));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_stop(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr1()->tccr);
}

/**
 * @brief rx_tmr_stop() on cascaded TMR23 also clears TMR3.TCCR.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" stop sequence: odd-channel TCCR
 * must be cleared in addition to even.
 * @since Version 1.0.0
 */
void test_stop_cascade_tmr23_clears_paired(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_2));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_stop(k_tmr_channel_2));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr2()->tccr);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr3()->tccr);
}

/**
 * @brief rx_tmr_stop() on out-of-range channel returns k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_stop_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_stop((rx_tmr_channel_t)k_tmr_test_invalid_channel));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
}

/**
 * @brief rx_tmr_stop() on uninitialised channel returns k_rx_err_invalid_state.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_state.
 * @since Version 1.0.0
 */
void test_stop_uninitialised_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tmr_stop(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
}

/**
 * @brief rx_tmr_read() returns the 8-bit TCNT zero-extended to uint16_t.
 * @par Manual reference
 * RX72N UM Ch. 32, TCNT register (8-bit mode).
 * @since Version 1.0.0
 */
void test_read_8bit_counter(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  tmr0()->tcnt = k_tmr_test_count_0042;

  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_read(k_tmr_channel_0, &value));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_tmr_test_count_0042, value);
}

/**
 * @brief rx_tmr_read() assembles a 16-bit value on cascaded TMR01.
 * @details Upper byte = TMR0.TCNT, lower byte = TMR1.TCNT.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode": TMR0 holds upper, TMR1 lower.
 * @since Version 1.0.0
 */
void test_read_16bit_cascade_tmr01(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  tmr0()->tcnt = k_tmr_test_count_ab;
  tmr1()->tcnt = k_tmr_test_count_cd;

  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_read(k_tmr_channel_0, &value));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_tmr_test_count_abcd, value);
}

/**
 * @brief rx_tmr_read() assembles a 16-bit value on cascaded TMR23.
 * @details Exercises the alternate (channel == k_tmr_channel_3) branch in
 * the cascaded-read helper.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode": TMR2 holds upper, TMR3 lower.
 * @since Version 1.0.0
 */
void test_read_16bit_cascade_tmr23(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  tmr2()->tcnt = k_tmr_test_count_upper_cd;
  tmr3()->tcnt = k_tmr_test_count_lower_ef;

  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_read(k_tmr_channel_2, &value));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_tmr_test_count_16bit_cef, value);
}

/**
 * @brief rx_tmr_read() with NULL output pointer returns k_rx_err_null_ptr.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_null_ptr.
 * @since Version 1.0.0
 */
void test_read_null_ptr(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tmr_read(k_tmr_channel_0, NULL));
}

/**
 * @brief rx_tmr_read() on uninitialised channel returns k_rx_err_invalid_state.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_state.
 * @since Version 1.0.0
 */
void test_read_uninitialised_channel(void)
{
  uint16_t value = 0;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tmr_read(k_tmr_channel_3, &value));
  TEST_ASSERT_EQUAL_UINT16(0U, value);
}

/**
 * @brief rx_tmr_read() on out-of-range channel returns k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_read_invalid_channel(void)
{
  uint16_t value = 0;
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_read((rx_tmr_channel_t)k_tmr_test_invalid_channel, &value));
  TEST_ASSERT_EQUAL_UINT16(0U, value);
}

/**
 * @brief rx_tmr_start() on out-of-range channel returns k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_start_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_start((rx_tmr_channel_t)k_tmr_test_invalid_channel));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
}

/**
 * @brief rx_tmr_start() on uninitialised channel returns k_rx_err_invalid_state.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_state.
 * @since Version 1.0.0
 */
void test_start_uninitialised_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tmr_start(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
}

/* =============================================================================
 * set_period_us Tests -- every PCLK divider exercised
 * =============================================================================
 */

/**
 * @brief 1 us period at PCLK/1 sets TCORA = 59 (60 ticks - 1).
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA reload formula: match at TCNT==TCORA.
 * @since Version 1.0.0
 */
void test_set_period_1us_at_pclk_div_1(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_1;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_div1_1us, tmr0()->tcora);
}

/**
 * @brief 1 us period at PCLK/2 sets TCORA = 29.
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA reload formula.
 * @since Version 1.0.0
 */
void test_set_period_1us_at_pclk_div_2(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_2;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_div2_1us, tmr0()->tcora);
}

/**
 * @brief 8 us period at PCLK/8 sets TCORA = 59.
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA reload formula.
 * @since Version 1.0.0
 */
void test_set_period_8us_at_pclk_div_8(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_8;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_8us));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_div8_8us, tmr0()->tcora);
}

/**
 * @brief 32 us period at PCLK/32 sets TCORA = 59.
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA reload formula.
 * @since Version 1.0.0
 */
void test_set_period_32us_at_pclk_div_32(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_32;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_32us));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_div32_32us, tmr0()->tcora);
}

/**
 * @brief 64 us period at PCLK/64 sets TCORA = 59.
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA reload formula.
 * @since Version 1.0.0
 */
void test_set_period_64us_at_pclk_div_64(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_64;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_64us));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_div64_64us, tmr0()->tcora);
}

/**
 * @brief 1 ms period at PCLK/1024 sets TCORA = 57.
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA reload formula; floor(60000/1024)-1 = 57.
 * @since Version 1.0.0
 */
void test_set_period_1ms_at_pclk_div_1024(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1ms));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_div1024_1ms, tmr0()->tcora);
}

/**
 * @brief 1 s period on TMR23 cascade splits TCORA across TMR2/TMR3.
 * @details
 * PCLK/8192 gives 7324 ticks/sec; compare = 7323 = 0x1C9B. Upper byte
 * 0x1C is written to TMR2.TCORA and lower byte 0x9B to TMR3.TCORA.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" TCORA layout.
 * @since Version 1.0.0
 */
void test_set_period_1s_cascade_tmr23(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_2);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_8192;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_2, k_tmr_test_period_1s));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_cascade_hi, tmr2()->tcora);
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_cascade_lo, tmr3()->tcora);
}

/**
 * @brief 1 s period on TMR01 cascade splits TCORA across TMR0/TMR1.
 * @details
 * Verifies the other half of the cascade ternary (channel == TMR0 -> odd
 * channel == TMR1).
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" TCORA layout.
 * @since Version 1.0.0
 */
void test_set_period_1s_cascade_tmr01(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_8192;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1s));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_cascade_hi, tmr0()->tcora);
  TEST_ASSERT_EQUAL_UINT8(k_tmr_test_tcora_cascade_lo, tmr1()->tcora);
}

/**
 * @brief period_us == 0 is rejected with k_rx_err_invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_set_period_zero_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  const uint8_t tcora_before = tmr0()->tcora;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tmr_set_period_us(k_tmr_channel_0, 0U));
  TEST_ASSERT_EQUAL_UINT8(tcora_before, tmr0()->tcora);
}

/**
 * @brief Period too large for the selected divider is rejected.
 * @par Manual reference
 * RX72N UM Ch. 32: 8-bit TCORA saturates at 0xFF.
 * @since Version 1.0.0
 */
void test_set_period_too_large_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_1;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  const uint8_t tcora_before = tmr0()->tcora;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1ms));
  TEST_ASSERT_EQUAL_UINT8(tcora_before, tmr0()->tcora);
}

/**
 * @brief Period below 1-tick resolution (ticks==0) is rejected.
 * @details
 * At PCLK/8192, 1 us requests fewer than 1 tick (60/8192 = 0). The driver
 * must reject rather than program TCORA = -1 underflow.
 * @par Manual reference
 * RX72N UM Ch. 32, TCORA >= 1 required for a valid compare-match period.
 * @since Version 1.0.0
 */
void test_set_period_ticks_zero_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_pclk_div_8192;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  const uint8_t tcora_before = tmr0()->tcora;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(tcora_before, tmr0()->tcora);
}

/**
 * @brief External clock source cannot be used to compute a period.
 * @par Manual reference
 * RX72N UM Ch. 32: external TMCI has no fixed frequency; period in us is
 * meaningless.
 * @since Version 1.0.0
 */
void test_set_period_external_clock_rejected(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.clock_source    = k_tmr_clock_external_rising;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  const uint8_t tcora_before = tmr0()->tcora;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(tcora_before, tmr0()->tcora);
}

/**
 * @brief rx_tmr_set_period_us() out-of-range channel returns invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_set_period_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(
    k_rx_err_invalid_arg,
    rx_tmr_set_period_us((rx_tmr_channel_t)k_tmr_test_invalid_channel, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcora);
}

/**
 * @brief rx_tmr_set_period_us() on uninit channel returns invalid_state.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_state.
 * @since Version 1.0.0
 */
void test_set_period_uninitialised_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_tmr_set_period_us(k_tmr_channel_0, k_tmr_test_period_1us));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcora);
}

/* =============================================================================
 * ISR Dispatch Tests
 * =============================================================================
 */

/**
 * @brief rx_tmr_isr_dispatch() invokes a registered callback once.
 * @par Manual reference
 * RX72N UM Ch. 32, "Interrupt Sources" (CMIA/CMIB/OVI via ICU SELECTB).
 * @since Version 1.0.0
 */
void test_isr_dispatch_invokes_registered_callback(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask        = k_tmr_irq_cmp_match_a;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_register_compare_match_isr(k_tmr_channel_0, test_isr_callback));

  rx_tmr_isr_dispatch(k_tmr_channel_0);
  TEST_ASSERT_EQUAL_UINT32(1U, s_isr_calls_per_channel[k_tmr_channel_0]);
  TEST_ASSERT_EQUAL(k_tmr_channel_0, s_isr_last_channel);
}

/**
 * @brief rx_tmr_isr_dispatch() without a registered callback is a no-op.
 * @par Manual reference
 * HAL API contract: rx_tmr.h, NULL callback is permitted.
 * @since Version 1.0.0
 */
void test_isr_dispatch_unregistered_is_noop(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));

  rx_tmr_isr_dispatch(k_tmr_channel_1);
  TEST_ASSERT_EQUAL_UINT32(0U, s_isr_calls_per_channel[k_tmr_channel_1]);
  TEST_ASSERT_EQUAL(k_tmr_channel_0, s_isr_last_channel);
}

/**
 * @brief rx_tmr_isr_dispatch() with out-of-range channel is a no-op.
 * @par Manual reference
 * HAL defensive contract: out-of-range ISR invocation cannot invoke any
 * registered callback.
 * @since Version 1.0.0
 */
void test_isr_dispatch_out_of_range_is_noop(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_tmr_isr_dispatch((rx_tmr_channel_t)k_tmr_test_invalid_channel);
  TEST_ASSERT_EQUAL_UINT32(0U, s_isr_calls_per_channel[k_tmr_channel_0]);
  TEST_ASSERT_EQUAL(k_tmr_channel_0, s_isr_last_channel);
}

/**
 * @brief rx_tmr_register_compare_match_isr() rejects out-of-range channel.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_isr_register_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_register_compare_match_isr((rx_tmr_channel_t)k_tmr_test_invalid_channel,
                                                      test_isr_callback));
  TEST_ASSERT_EQUAL_UINT32(0U, s_isr_calls_per_channel[k_tmr_channel_0]);
}

/* =============================================================================
 * Deinit Tests
 * =============================================================================
 */

/**
 * @brief rx_tmr_deinit() clears TCR/TCCR/TCNT on the channel.
 * @par Manual reference
 * RX72N UM Ch. 32, TCR/TCCR/TCNT reset semantics.
 * @since Version 1.0.0
 */
void test_deinit_clears_registers(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.irq_mask        = k_tmr_irq_cmp_match_a;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_start(k_tmr_channel_0));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_deinit(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcr);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tcnt);
}

/**
 * @brief rx_tmr_deinit() on cascaded TMR01 also clears TMR1 registers.
 * @par Manual reference
 * RX72N UM Ch. 32, "16-Bit Counter Mode" teardown.
 * @since Version 1.0.0
 */
void test_deinit_cascade_clears_paired_odd_channel(void)
{
  rx_tmr_config_t cfg = make_default_config(k_tmr_channel_0);
  cfg.mode            = k_tmr_mode_16bit_cascade;
  cfg.clock_source    = k_tmr_clock_pclk_div_1024;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_init(&cfg));
  TEST_ASSERT_EQUAL_UINT8(k_tmr_tccr_css_cascade, tmr1()->tccr & k_tmr_tccr_css_mask);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tmr_deinit(k_tmr_channel_0));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr1()->tccr);
}

/**
 * @brief rx_tmr_deinit() with out-of-range channel returns invalid_arg.
 * @par Manual reference
 * HAL API contract: rx_tmr.h @retval k_rx_err_invalid_arg.
 * @since Version 1.0.0
 */
void test_deinit_invalid_channel(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tmr_deinit((rx_tmr_channel_t)k_tmr_test_invalid_channel));
  TEST_ASSERT_EQUAL_UINT8(0U, tmr0()->tccr);
}

/* =============================================================================
 * Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Init -- happy paths */
  RUN_TEST(test_init_tmr0_independent);
  RUN_TEST(test_init_tmr1_independent);
  RUN_TEST(test_init_tmr2_independent);
  RUN_TEST(test_init_tmr3_independent);
  RUN_TEST(test_init_cascade_tmr01);
  RUN_TEST(test_init_cascade_tmr23);

  /* Init -- counter_clear coverage */
  RUN_TEST(test_init_counter_clear_disabled);
  RUN_TEST(test_init_counter_clear_cmp_match_b);
  RUN_TEST(test_init_counter_clear_external_reset);

  /* Init -- irq_mask coverage */
  RUN_TEST(test_init_irq_mask_cmp_match_b);
  RUN_TEST(test_init_irq_mask_overflow);
  RUN_TEST(test_init_irq_mask_all);

  /* Init -- error paths */
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_invalid_channel);
  RUN_TEST(test_init_invalid_mode);
  RUN_TEST(test_init_invalid_counter_clear);
  RUN_TEST(test_init_invalid_output_a);
  RUN_TEST(test_init_invalid_output_b);
  RUN_TEST(test_init_invalid_clock_source);
  RUN_TEST(test_init_cascade_on_odd_channel_rejected);
  RUN_TEST(test_init_double_init_rejected);

  /* Start -- clock-source coverage */
  RUN_TEST(test_start_clock_pclk_div_1);
  RUN_TEST(test_start_clock_pclk_div_2);
  RUN_TEST(test_start_clock_pclk_div_8);
  RUN_TEST(test_start_clock_pclk_div_32);
  RUN_TEST(test_start_clock_pclk_div_64);
  RUN_TEST(test_start_clock_pclk_div_1024);
  RUN_TEST(test_start_clock_pclk_div_8192);
  RUN_TEST(test_start_clock_external_rising);
  RUN_TEST(test_start_clock_external_falling);
  RUN_TEST(test_start_clock_external_both);

  /* Start / stop -- cascade reprogram */
  RUN_TEST(test_start_cascade_tmr01_reprograms_paired);
  RUN_TEST(test_start_cascade_tmr23_reprograms_paired);
  RUN_TEST(test_start_programs_tccr_clock_bits);

  /* Stop -- independent, cascade, errors */
  RUN_TEST(test_stop_clears_tccr_clock_bits);
  RUN_TEST(test_stop_cascade_tmr01_clears_paired);
  RUN_TEST(test_stop_cascade_tmr23_clears_paired);
  RUN_TEST(test_stop_invalid_channel);
  RUN_TEST(test_stop_uninitialised_channel);

  /* Read */
  RUN_TEST(test_read_8bit_counter);
  RUN_TEST(test_read_16bit_cascade_tmr01);
  RUN_TEST(test_read_16bit_cascade_tmr23);
  RUN_TEST(test_read_null_ptr);
  RUN_TEST(test_read_uninitialised_channel);
  RUN_TEST(test_read_invalid_channel);

  /* Start errors */
  RUN_TEST(test_start_invalid_channel);
  RUN_TEST(test_start_uninitialised_channel);

  /* set_period_us -- every divider + cascade + errors */
  RUN_TEST(test_set_period_1us_at_pclk_div_1);
  RUN_TEST(test_set_period_1us_at_pclk_div_2);
  RUN_TEST(test_set_period_8us_at_pclk_div_8);
  RUN_TEST(test_set_period_32us_at_pclk_div_32);
  RUN_TEST(test_set_period_64us_at_pclk_div_64);
  RUN_TEST(test_set_period_1ms_at_pclk_div_1024);
  RUN_TEST(test_set_period_1s_cascade_tmr23);
  RUN_TEST(test_set_period_1s_cascade_tmr01);
  RUN_TEST(test_set_period_zero_rejected);
  RUN_TEST(test_set_period_too_large_rejected);
  RUN_TEST(test_set_period_ticks_zero_rejected);
  RUN_TEST(test_set_period_external_clock_rejected);
  RUN_TEST(test_set_period_invalid_channel);
  RUN_TEST(test_set_period_uninitialised_channel);

  /* ISR */
  RUN_TEST(test_isr_dispatch_invokes_registered_callback);
  RUN_TEST(test_isr_dispatch_unregistered_is_noop);
  RUN_TEST(test_isr_dispatch_out_of_range_is_noop);
  RUN_TEST(test_isr_register_invalid_channel);

  /* Deinit */
  RUN_TEST(test_deinit_clears_registers);
  RUN_TEST(test_deinit_cascade_clears_paired_odd_channel);
  RUN_TEST(test_deinit_invalid_channel);

  return UNITY_END();
}
