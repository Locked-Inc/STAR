/**
 * @file test_rx_dmaca.c
 * @brief Unit Tests for RX72N DMAC Polling-Mode Driver
 *
 * @details
 * Tests rx_dmaca_init(), rx_dmaca_transfer_poll(), rx_dmaca_abort(), and
 * rx_dmaca_deinit() against RAM-backed mock hardware registers defined in
 * tests/mocks/rx72n_dmac_regs.h.
 *
 * ## Test Coverage
 * - Init/deinit lifecycle and double-init guard
 * - NULL-pointer and out-of-range argument validation
 * - Uninitialized-state guard on all non-init functions
 * - Register configuration verification (DMSAR, DMDAR, DMCRA, DMTMD, DMAMD)
 * - 8-bit vs 32-bit transfer-size selection logic
 * - DTE clear on both success and timeout
 * - Timeout path (ACT bit held set in mock DMSTS)
 * - Abort with immediate success and with timeout
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

/* Shadow header included first so rx72n_regs.h resolves to mock */
#include "mock_rx_onewire_hw.h"
#include "rx72n_regs.h"
#include "rx_dmaca.h"

/* =============================================================================
 * Mock Storage Definitions (extern'd in rx72n_dmac_regs.h)
 * =============================================================================
 */

#ifdef UNIT_TEST
/** @brief Mock DMAC channel register arrays - one per channel */
rx_dmac_channel_regs_t g_mock_dmac_ch[k_dmac_channel_count];

/** @brief Mock DMAC Module Start Register (DMAST) */
uint8_t g_mock_dmast;
#endif /* UNIT_TEST */

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test-local constants
 */
typedef enum : uint32_t {
  k_test_dst_addr_aligned   = 0x00088284U, /**< CRCDIR address (4-byte aligned) */
  k_test_dst_addr_unaligned = 0x00088285U, /**< Unaligned destination */
  k_test_timeout_short      = 1U,          /**< 1-cycle timeout for timeout tests */
  k_test_timeout_normal     = 50000U,      /**< Normal timeout for success tests */
} test_dmaca_constants_t;

/** @brief Byte count that is a multiple of 4 (triggers 32-bit DMA mode) */
typedef enum : uint32_t {
  k_test_len_aligned   = 8U, /**< 8 bytes - multiple of 4, uses 32-bit mode */
  k_test_len_unaligned = 7U, /**< 7 bytes - not multiple of 4, uses 8-bit mode */
} test_dmaca_len_t;

/* =============================================================================
 * Helpers
 * =============================================================================
 */

/** @brief Source buffer for transfer tests */
static uint8_t s_src_buf[16];

/**
 * @brief Build a valid transfer config for channel 0
 *
 * @param[in] len    Transfer length
 * @param[in] dst    Destination address
 * @param[in] timeout Timeout cycles
 * @return           Populated config
 */
static rx_dmaca_config_t make_config(uint32_t len, uintptr_t dst, uint32_t timeout)
{
  rx_dmaca_config_t cfg;
  cfg.channel        = 0;
  cfg.src            = s_src_buf;
  cfg.len            = len;
  cfg.dst_addr       = dst;
  cfg.timeout_cycles = timeout;
  return cfg;
}

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

void setUp(void)
{
  /* Ensure driver is deinitialized before each test (ignores error if not init) */
  (void)rx_dmaca_deinit();

  /* Reset mock system registers (PRCR, MSTPCRA, etc.) */
  mock_onewire_hw_init();

  /* Zero DMAC mock registers */
  memset(g_mock_dmac_ch, 0, sizeof(g_mock_dmac_ch));
  g_mock_dmast = 0;

  /* Initialize test source buffer with known data */
  memset(s_src_buf, 0xAB, sizeof(s_src_buf));
}

void tearDown(void)
{
  mock_onewire_hw_deinit();
}

/* =============================================================================
 * Init / Deinit Tests
 * =============================================================================
 */

/**
 * @brief rx_dmaca_init() returns k_rx_ok on first call
 */
void test_init_ok(void)
{
  rx_err_t err = rx_dmaca_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief After init, DMAST register has the DMST bit set
 */
void test_init_sets_dmast(void)
{
  (void)rx_dmaca_init();
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_dmast_dmst, g_mock_dmast);
}

/**
 * @brief Second call to rx_dmaca_init() returns k_rx_err_invalid_state
 */
void test_init_double_returns_invalid_state(void)
{
  (void)rx_dmaca_init();
  rx_err_t err = rx_dmaca_init();
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief rx_dmaca_deinit() without prior init returns k_rx_err_not_initialized
 */
void test_deinit_not_initialized(void)
{
  /* setUp calls deinit, so s_dmaca_initialized is false here */
  rx_err_t err = rx_dmaca_deinit();
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief After successful deinit, DMAST register is cleared
 */
void test_deinit_clears_dmast(void)
{
  (void)rx_dmaca_init();
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_dmast_dmst, g_mock_dmast);

  (void)rx_dmaca_deinit();
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_dmast);
}

/**
 * @brief Full init + deinit cycle returns k_rx_ok
 */
void test_deinit_ok(void)
{
  (void)rx_dmaca_init();
  rx_err_t err = rx_dmaca_deinit();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * transfer_poll Validation Tests
 * =============================================================================
 */

/**
 * @brief NULL config pointer returns k_rx_err_null_ptr
 */
void test_transfer_null_config(void)
{
  (void)rx_dmaca_init();
  rx_err_t err = rx_dmaca_transfer_poll(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Transfer without init returns k_rx_err_not_initialized
 */
void test_transfer_not_initialized(void)
{
  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_normal);
  rx_err_t err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Channel >= 8 returns k_rx_err_invalid_arg
 */
void test_transfer_channel_out_of_range(void)
{
  (void)rx_dmaca_init();
  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_normal);
  cfg.channel  = 8U;
  rx_err_t err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief len == 0 returns k_rx_err_invalid_arg
 */
void test_transfer_zero_len(void)
{
  (void)rx_dmaca_init();
  rx_dmaca_config_t cfg = make_config(0U, k_test_dst_addr_aligned, k_test_timeout_normal);
  rx_err_t          err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief len > 65535 returns k_rx_err_invalid_arg
 */
void test_transfer_len_too_large(void)
{
  (void)rx_dmaca_init();
  rx_dmaca_config_t cfg = make_config(65536U, k_test_dst_addr_aligned, k_test_timeout_normal);
  rx_err_t          err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief timeout_cycles == 0 returns k_rx_err_invalid_arg
 */
void test_transfer_zero_timeout(void)
{
  (void)rx_dmaca_init();
  rx_dmaca_config_t cfg = make_config(k_test_len_aligned, k_test_dst_addr_aligned, 0U);
  rx_err_t          err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief NULL src pointer returns k_rx_err_null_ptr
 */
void test_transfer_null_src(void)
{
  (void)rx_dmaca_init();
  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_normal);
  cfg.src      = nullptr;
  rx_err_t err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * transfer_poll Register Verification Tests
 * =============================================================================
 */

/**
 * @brief 8-byte aligned transfer configures DMSAR, DMDAR, DMCRA in 32-bit mode
 *
 * @details When len % 4 == 0 and dst_addr % 4 == 0, the driver selects 32-bit
 * DMA mode: count = len/4, DMTMD.SZ = k_dmtmd_sz_32bit.
 */
void test_transfer_32bit_mode_register_values(void)
{
  (void)rx_dmaca_init();
  /* Leave g_mock_dmac_ch[0].dmsts = 0 so poll succeeds immediately */
  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_normal);
  rx_err_t err = rx_dmaca_transfer_poll(&cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Source address */
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(uintptr_t)s_src_buf, g_mock_dmac_ch[0].dmsar);

  /* Destination address */
  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_dst_addr_aligned, g_mock_dmac_ch[0].dmdar);

  /* Count: len/4 = 8/4 = 2 */
  TEST_ASSERT_EQUAL_UINT32(2U, g_mock_dmac_ch[0].dmcra);

  /* DMTMD: 32-bit mode, software trigger, normal mode */
  uint16_t expected_dmtmd =
    (uint16_t)k_dmtmd_sz_32bit | (uint16_t)k_dmtmd_md_normal | (uint16_t)k_dmtmd_dctg_software;
  TEST_ASSERT_EQUAL_UINT16(expected_dmtmd, g_mock_dmac_ch[0].dmtmd);

  /* DMAMD: source increment, destination fixed */
  uint16_t expected_dmamd = (uint16_t)k_dmamd_sm_increment | (uint16_t)k_dmamd_dm_fixed;
  TEST_ASSERT_EQUAL_UINT16(expected_dmamd, g_mock_dmac_ch[0].dmamd);

  /* DTE cleared as post-condition */
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_dmac_ch[0].dmcnt);
}

/**
 * @brief 7-byte transfer (unaligned) uses 8-bit mode
 *
 * @details When len % 4 != 0, the driver selects 8-bit mode: count = len,
 * DMTMD.SZ = k_dmtmd_sz_8bit.
 */
void test_transfer_8bit_mode_unaligned_len(void)
{
  (void)rx_dmaca_init();
  rx_dmaca_config_t cfg =
    make_config(k_test_len_unaligned, k_test_dst_addr_aligned, k_test_timeout_normal);
  (void)rx_dmaca_transfer_poll(&cfg);

  /* Count: 7 (8-bit mode, count = len) */
  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_len_unaligned, g_mock_dmac_ch[0].dmcra);

  /* DMTMD: 8-bit mode */
  uint16_t sz_bits = g_mock_dmac_ch[0].dmtmd & 0x0300U;
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_dmtmd_sz_8bit, sz_bits);
}

/**
 * @brief Unaligned destination forces 8-bit mode even if len is aligned
 */
void test_transfer_8bit_mode_unaligned_dst(void)
{
  (void)rx_dmaca_init();
  /* len=8 is aligned, but dst is unaligned -> 8-bit mode */
  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_unaligned, k_test_timeout_normal);
  (void)rx_dmaca_transfer_poll(&cfg);

  /* Count: 8 (8-bit mode, count = len) */
  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_len_aligned, g_mock_dmac_ch[0].dmcra);

  uint16_t sz_bits = g_mock_dmac_ch[0].dmtmd & 0x0300U;
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_dmtmd_sz_8bit, sz_bits);
}

/**
 * @brief Successful transfer: DMSTS.ACT clear, DTE cleared after success
 */
void test_transfer_ok_clears_dte(void)
{
  (void)rx_dmaca_init();
  /* dmsts = 0: ACT not set, poll succeeds on first iteration */
  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_normal);
  rx_err_t err = rx_dmaca_transfer_poll(&cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_dmac_ch[0].dmcnt);
}

/**
 * @brief Timeout: ACT bit held set, transfer_poll returns k_rx_err_hw_timeout
 */
void test_transfer_timeout(void)
{
  (void)rx_dmaca_init();
  /* Set ACT bit: poll will never see ACT=0 */
  g_mock_dmac_ch[0].dmsts = (uint8_t)k_dmsts_act;

  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_short);
  rx_err_t err = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_hw_timeout, err);
}

/**
 * @brief DTE is cleared even when transfer times out (post-condition enforced)
 */
void test_transfer_timeout_still_clears_dte(void)
{
  (void)rx_dmaca_init();
  g_mock_dmac_ch[0].dmsts = (uint8_t)k_dmsts_act;

  rx_dmaca_config_t cfg =
    make_config(k_test_len_aligned, k_test_dst_addr_aligned, k_test_timeout_short);
  (void)rx_dmaca_transfer_poll(&cfg);

  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_dmac_ch[0].dmcnt);
}

/* =============================================================================
 * abort Tests
 * =============================================================================
 */

/**
 * @brief rx_dmaca_abort() without init returns k_rx_err_not_initialized
 */
void test_abort_not_initialized(void)
{
  rx_err_t err = rx_dmaca_abort(0);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief rx_dmaca_abort() with channel >= 8 returns k_rx_err_invalid_arg
 */
void test_abort_channel_out_of_range(void)
{
  (void)rx_dmaca_init();
  rx_err_t err = rx_dmaca_abort(8U);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief rx_dmaca_abort() with ACT=0 returns k_rx_ok and clears DTE
 */
void test_abort_ok(void)
{
  (void)rx_dmaca_init();
  /* dmsts = 0: ACT not set, abort poll succeeds immediately */
  rx_err_t err = rx_dmaca_abort(0U);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_dmac_ch[0].dmcnt);
}

/**
 * @brief rx_dmaca_abort() with ACT permanently set returns k_rx_err_hw_timeout
 */
void test_abort_timeout(void)
{
  (void)rx_dmaca_init();
  /* Set ACT bit: abort poll will never see ACT=0 */
  g_mock_dmac_ch[0].dmsts = (uint8_t)k_dmsts_act;

  rx_err_t err = rx_dmaca_abort(0U);
  TEST_ASSERT_EQUAL(k_rx_err_hw_timeout, err);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Init / Deinit lifecycle */
  RUN_TEST(test_init_ok);
  RUN_TEST(test_init_sets_dmast);
  RUN_TEST(test_init_double_returns_invalid_state);
  RUN_TEST(test_deinit_not_initialized);
  RUN_TEST(test_deinit_clears_dmast);
  RUN_TEST(test_deinit_ok);

  /* transfer_poll validation */
  RUN_TEST(test_transfer_null_config);
  RUN_TEST(test_transfer_not_initialized);
  RUN_TEST(test_transfer_channel_out_of_range);
  RUN_TEST(test_transfer_zero_len);
  RUN_TEST(test_transfer_len_too_large);
  RUN_TEST(test_transfer_zero_timeout);
  RUN_TEST(test_transfer_null_src);

  /* transfer_poll register verification */
  RUN_TEST(test_transfer_32bit_mode_register_values);
  RUN_TEST(test_transfer_8bit_mode_unaligned_len);
  RUN_TEST(test_transfer_8bit_mode_unaligned_dst);
  RUN_TEST(test_transfer_ok_clears_dte);
  RUN_TEST(test_transfer_timeout);
  RUN_TEST(test_transfer_timeout_still_clears_dte);

  /* abort */
  RUN_TEST(test_abort_not_initialized);
  RUN_TEST(test_abort_channel_out_of_range);
  RUN_TEST(test_abort_ok);
  RUN_TEST(test_abort_timeout);

  return UNITY_END();
}
