/**
 * @file test_rx_tpu.c
 * @brief Unit Tests for TPU HAL Driver (Phase Counting Mode)
 *
 * @details
 * Comprehensive unit tests for the TPU hardware abstraction layer covering
 * initialization, start/stop, counter read/write, direction detection, and
 * deinitialization. Tests use RAM-based mock registers to verify register
 * configuration sequences without real hardware.
 *
 * ## Test Categories
 *
 * | Category              | Tests | Description                              |
 * |-----------------------|-------|------------------------------------------|
 * | Init Success          | 4     | Valid init for each phase-counting channel|
 * | Init Error Paths      | 5     | NULL config, invalid channel/mode, double |
 * | Start/Stop            | 6     | Start, stop, uninit channel errors        |
 * | Read Count            | 4     | Valid reads, NULL ptr, uninit channel      |
 * | Read Direction        | 4     | Up/down detection, NULL ptr, uninit       |
 * | Reset Count           | 3     | Valid reset, invalid channel, uninit      |
 * | Deinit                | 4     | Valid deinit, register cleanup, re-init   |
 * | Module Clock          | 1     | MSTPCRA.MSTPA13 cleared on init          |
 * | Register Config       | 3     | TCR, TMDR, TIER verified after init       |
 *
 * @see rx_tpu.h TPU HAL driver API
 * @see rx_tpu.c TPU HAL driver implementation
 * @see rx72n_tpu_regs.h Mock TPU register structures (mocks/)
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <string.h>

#include "mock_rx_tpu_regs.h"
#include "rx_tpu.h"
#include "unity.h"

/*
 * System register access:
 * - system_regs() is provided by mock_rx_onewire_hw.h (via shadow rx72n_regs.h)
 *   and returns &g_mock_onewire_system_regs (type mock_system_regs_t*)
 * - prcr_reg() is provided by shadow rx72n_system_regs.h and returns &g_mock_prcr
 * - rx_tpu.c includes rx72n_regs.h which transitively pulls in both
 *
 * We declare the extern here so the test can manipulate system registers
 * directly for setUp/assertions without including all of rx72n_regs.h.
 */
#include "mock_rx_onewire_hw.h"

/* =============================================================================
 * Mock Register Storage
 * =============================================================================
 */

/** @brief Mock TPU control registers */
rx_tpu_control_regs_t g_mock_tpu_control;

/** @brief Mock TPU channel registers (0=TPU1, 1=TPU2, 2=TPU4, 3=TPU5) */
rx_tpu_regs_t g_mock_tpu_regs[k_mock_tpu_channel_count];

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

void setUp(void)
{
  /* Clear all mock registers to zero */
  memset(&g_mock_tpu_control, 0, sizeof(g_mock_tpu_control));
  memset(g_mock_tpu_regs, 0, sizeof(g_mock_tpu_regs));
  memset(&g_mock_onewire_system_regs, 0, sizeof(g_mock_onewire_system_regs));

  /* Set MSTPA13 bit to simulate TPU module stopped (reset default) */
  g_mock_onewire_system_regs.mstpcra = (1U << 13);

  /* Deinitialize all channels to start fresh */
  (void)rx_tpu_deinit(k_tpu_channel_1);
  (void)rx_tpu_deinit(k_tpu_channel_2);
  (void)rx_tpu_deinit(k_tpu_channel_4);
  (void)rx_tpu_deinit(k_tpu_channel_5);

  /* Re-clear mock registers after deinit (deinit writes to them) */
  memset(&g_mock_tpu_control, 0, sizeof(g_mock_tpu_control));
  memset(g_mock_tpu_regs, 0, sizeof(g_mock_tpu_regs));
  g_mock_onewire_system_regs.mstpcra = (1U << 13);
}

void tearDown(void)
{
  /* Deinitialize all channels */
  (void)rx_tpu_deinit(k_tpu_channel_1);
  (void)rx_tpu_deinit(k_tpu_channel_2);
  (void)rx_tpu_deinit(k_tpu_channel_4);
  (void)rx_tpu_deinit(k_tpu_channel_5);
}

/* =============================================================================
 * Init Success Tests
 * =============================================================================
 */

/**
 * @brief Test: Initialize TPU1 for phase counting mode 4 (rear-left encoder)
 */
void test_init_tpu1_phase_mode_4(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify MSTPCRA.MSTPA13 was cleared (module clock enabled) */
  TEST_ASSERT_EQUAL_UINT32(0, g_mock_onewire_system_regs.mstpcra & (1U << 13));

  /* Verify TMDR set to phase counting mode 4 */
  TEST_ASSERT_EQUAL_UINT8(k_tpu_tmdr_md_phase_count_4, tpu1()->tmdr);

  /* Verify TCR set to free-running */
  TEST_ASSERT_EQUAL_UINT8(0x00, tpu1()->tcr);

  /* Verify TIER set to all disabled */
  TEST_ASSERT_EQUAL_UINT8(0x00, tpu1()->tier);

  /* Verify TCNT cleared */
  TEST_ASSERT_EQUAL_UINT16(0, tpu1()->tcnt);
}

/**
 * @brief Test: Initialize TPU2 for phase counting mode 4 (rear-right encoder)
 */
void test_init_tpu2_phase_mode_4(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_2,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL_UINT8(k_tpu_tmdr_md_phase_count_4, tpu2()->tmdr);
}

/**
 * @brief Test: Initialize TPU4 for phase counting mode 1 (1x resolution)
 */
void test_init_tpu4_phase_mode_1(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_4,
    .phase_mode = k_tpu_phase_mode_1,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL_UINT8(k_tpu_tmdr_md_phase_count_1, tpu4()->tmdr);
}

/**
 * @brief Test: Initialize TPU5 for phase counting mode 3 (2x resolution)
 */
void test_init_tpu5_phase_mode_3(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_5,
    .phase_mode = k_tpu_phase_mode_3,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL_UINT8(k_tpu_tmdr_md_phase_count_3, tpu5()->tmdr);
}

/* =============================================================================
 * Init Error Path Tests
 * =============================================================================
 */

/**
 * @brief Test: Init with NULL config returns null pointer error
 */
void test_init_null_config(void)
{
  rx_err_t err = rx_tpu_init_phase_count(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test: Init with invalid channel (0 - not phase counting capable)
 */
void test_init_invalid_channel_0(void)
{
  rx_tpu_config_t config = {
    .channel    = (rx_tpu_channel_t)0,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test: Init with invalid channel (3 - TPU3 cannot do phase counting)
 */
void test_init_invalid_channel_3(void)
{
  rx_tpu_config_t config = {
    .channel    = (rx_tpu_channel_t)3,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test: Init with invalid phase mode (0 is not a valid phase mode)
 */
void test_init_invalid_phase_mode(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = (rx_tpu_phase_mode_t)0,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test: Double initialization returns invalid state error
 */
void test_init_double_init(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Second init of same channel should fail */
  err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Start/Stop Tests
 * =============================================================================
 */

/**
 * @brief Test: Start sets CSTn bit in TSTR for TPU1
 */
void test_start_tpu1(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);

  rx_err_t err = rx_tpu_start(k_tpu_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify CST1 bit set in TSTR */
  TEST_ASSERT_EQUAL_UINT8(k_tpu_tstr_cst1, tpu_control()->tstr & k_tpu_tstr_cst1);
}

/**
 * @brief Test: Stop clears CSTn bit in TSTR for TPU2
 */
void test_stop_tpu2(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_2,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_start(k_tpu_channel_2);

  /* Verify CST2 is set */
  TEST_ASSERT_EQUAL_UINT8(k_tpu_tstr_cst2, tpu_control()->tstr & k_tpu_tstr_cst2);

  rx_err_t err = rx_tpu_stop(k_tpu_channel_2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify CST2 is cleared */
  TEST_ASSERT_EQUAL_UINT8(0, tpu_control()->tstr & k_tpu_tstr_cst2);
}

/**
 * @brief Test: Start uninit channel returns invalid state
 */
void test_start_uninit_channel(void)
{
  rx_err_t err = rx_tpu_start(k_tpu_channel_1);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test: Stop uninit channel returns invalid state
 */
void test_stop_uninit_channel(void)
{
  rx_err_t err = rx_tpu_stop(k_tpu_channel_2);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test: Start with invalid channel returns invalid arg
 */
void test_start_invalid_channel(void)
{
  rx_err_t err = rx_tpu_start((rx_tpu_channel_t)3);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test: Stop with invalid channel returns invalid arg
 */
void test_stop_invalid_channel(void)
{
  rx_err_t err = rx_tpu_stop((rx_tpu_channel_t)0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Read Count Tests
 * =============================================================================
 */

/**
 * @brief Test: Read count returns TCNT value from channel register
 */
void test_read_count_tpu1(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_start(k_tpu_channel_1);

  /* Simulate encoder counting to 1234 */
  tpu1()->tcnt = 1234;

  uint16_t count = 0;
  rx_err_t err   = rx_tpu_read_count(k_tpu_channel_1, &count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1234, count);
}

/**
 * @brief Test: Read count at 16-bit max (overflow boundary)
 */
void test_read_count_max_value(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_2,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_start(k_tpu_channel_2);

  /* Simulate counter at max */
  tpu2()->tcnt = 0xFFFF;

  uint16_t count = 0;
  rx_err_t err   = rx_tpu_read_count(k_tpu_channel_2, &count);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, count);
}

/**
 * @brief Test: Read count with NULL pointer returns error
 */
void test_read_count_null_ptr(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);

  rx_err_t err = rx_tpu_read_count(k_tpu_channel_1, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test: Read count on uninit channel returns invalid state
 */
void test_read_count_uninit(void)
{
  uint16_t count = 0;
  rx_err_t err   = rx_tpu_read_count(k_tpu_channel_4, &count);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test: Read count on invalid channel (3) returns invalid_arg
 */
void test_read_count_invalid_channel(void)
{
  uint16_t count = 0;
  rx_err_t err   = rx_tpu_read_count((rx_tpu_channel_t)3, &count);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Read Direction Tests
 * =============================================================================
 */

/**
 * @brief Test: Read direction returns true when TSR.TCFD is set (counting up)
 */
void test_read_direction_counting_up(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_start(k_tpu_channel_1);

  /* Set TCFD bit (bit 7) = counting up */
  tpu1()->tsr = k_tpu_tsr_tcfd;

  bool     counting_up = false;
  rx_err_t err         = rx_tpu_read_direction(k_tpu_channel_1, &counting_up);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(counting_up);
}

/**
 * @brief Test: Read direction returns false when TSR.TCFD is clear (counting down)
 */
void test_read_direction_counting_down(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_2,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_start(k_tpu_channel_2);

  /* Clear TCFD bit = counting down */
  tpu2()->tsr = 0;

  bool     counting_up = true;
  rx_err_t err         = rx_tpu_read_direction(k_tpu_channel_2, &counting_up);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(counting_up);
}

/**
 * @brief Test: Read direction with NULL pointer returns error
 */
void test_read_direction_null_ptr(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);

  rx_err_t err = rx_tpu_read_direction(k_tpu_channel_1, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test: Read direction on uninit channel returns invalid state
 */
void test_read_direction_uninit(void)
{
  bool     counting_up = false;
  rx_err_t err         = rx_tpu_read_direction(k_tpu_channel_5, &counting_up);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test: Read direction on invalid channel (0) returns invalid_arg
 */
void test_read_direction_invalid_channel(void)
{
  bool     counting_up = false;
  rx_err_t err         = rx_tpu_read_direction((rx_tpu_channel_t)0, &counting_up);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Reset Count Tests
 * =============================================================================
 */

/**
 * @brief Test: Reset count writes zero to TCNT
 */
void test_reset_count(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);

  /* Set counter to nonzero value */
  tpu1()->tcnt = 5000;

  rx_err_t err = rx_tpu_reset_count(k_tpu_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, tpu1()->tcnt);
}

/**
 * @brief Test: Reset count on invalid channel returns error
 */
void test_reset_count_invalid_channel(void)
{
  rx_err_t err = rx_tpu_reset_count((rx_tpu_channel_t)3);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test: Reset count on uninit channel returns invalid state
 */
void test_reset_count_uninit(void)
{
  rx_err_t err = rx_tpu_reset_count(k_tpu_channel_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Deinit Tests
 * =============================================================================
 */

/**
 * @brief Test: Deinit resets registers and marks channel uninitialized
 */
void test_deinit_resets_registers(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_start(k_tpu_channel_1);

  rx_err_t err = rx_tpu_deinit(k_tpu_channel_1);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify TMDR reset to normal mode */
  TEST_ASSERT_EQUAL_UINT8(0x00, tpu1()->tmdr);

  /* Verify TIER cleared */
  TEST_ASSERT_EQUAL_UINT8(0x00, tpu1()->tier);

  /* Verify TCNT cleared */
  TEST_ASSERT_EQUAL_UINT16(0, tpu1()->tcnt);

  /* Verify CST1 cleared in TSTR */
  TEST_ASSERT_EQUAL_UINT8(0, tpu_control()->tstr & k_tpu_tstr_cst1);
}

/**
 * @brief Test: After deinit, start returns invalid state (channel no longer init)
 */
void test_deinit_then_start_fails(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_2,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_deinit(k_tpu_channel_2);

  rx_err_t err = rx_tpu_start(k_tpu_channel_2);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test: After deinit, re-init succeeds
 */
void test_deinit_then_reinit(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);
  (void)rx_tpu_deinit(k_tpu_channel_1);

  /* Re-init should succeed */
  rx_err_t err = rx_tpu_init_phase_count(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test: Deinit with invalid channel returns error
 */
void test_deinit_invalid_channel(void)
{
  rx_err_t err = rx_tpu_deinit((rx_tpu_channel_t)0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Module Clock Tests
 * =============================================================================
 */

/**
 * @brief Test: Init clears MSTPCRA.MSTPA13 to enable TPU module clock
 */
void test_init_enables_module_clock(void)
{
  /* Verify module is stopped before init */
  TEST_ASSERT_NOT_EQUAL(0, g_mock_onewire_system_regs.mstpcra & (1U << 13));

  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);

  /* Verify MSTPA13 cleared (module clock enabled) */
  TEST_ASSERT_EQUAL_UINT32(0, g_mock_onewire_system_regs.mstpcra & (1U << 13));
}

/* =============================================================================
 * Register Configuration Verification Tests
 * =============================================================================
 */

/**
 * @brief Test: Phase mode 2 sets correct TMDR value
 */
void test_phase_mode_2_register(void)
{
  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_4,
    .phase_mode = k_tpu_phase_mode_2,
  };

  (void)rx_tpu_init_phase_count(&config);

  TEST_ASSERT_EQUAL_UINT8(k_tpu_tmdr_md_phase_count_2, tpu4()->tmdr);
}

/**
 * @brief Test: Init stops counter (clears CST bit) before configuring
 */
void test_init_stops_counter_first(void)
{
  /* Pre-set CST5 as if counter was running */
  tpu_control()->tstr = k_tpu_tstr_cst5;

  rx_tpu_config_t config = {
    .channel    = k_tpu_channel_5,
    .phase_mode = k_tpu_phase_mode_4,
  };

  (void)rx_tpu_init_phase_count(&config);

  /* CST5 should be cleared after init (counter stopped during config) */
  TEST_ASSERT_EQUAL_UINT8(0, tpu_control()->tstr & k_tpu_tstr_cst5);
}

/**
 * @brief Test: Multiple channels can be independently initialized
 */
void test_multi_channel_independent(void)
{
  rx_tpu_config_t config1 = {
    .channel    = k_tpu_channel_1,
    .phase_mode = k_tpu_phase_mode_4,
  };
  rx_tpu_config_t config2 = {
    .channel    = k_tpu_channel_2,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err1 = rx_tpu_init_phase_count(&config1);
  rx_err_t err2 = rx_tpu_init_phase_count(&config2);
  TEST_ASSERT_EQUAL(k_rx_ok, err1);
  TEST_ASSERT_EQUAL(k_rx_ok, err2);

  /* Start both */
  (void)rx_tpu_start(k_tpu_channel_1);
  (void)rx_tpu_start(k_tpu_channel_2);

  /* Both CST bits should be set */
  TEST_ASSERT_EQUAL_UINT8(k_tpu_tstr_cst1, tpu_control()->tstr & k_tpu_tstr_cst1);
  TEST_ASSERT_EQUAL_UINT8(k_tpu_tstr_cst2, tpu_control()->tstr & k_tpu_tstr_cst2);

  /* Set different counter values */
  tpu1()->tcnt = 100;
  tpu2()->tcnt = 200;

  uint16_t count1 = 0;
  uint16_t count2 = 0;
  (void)rx_tpu_read_count(k_tpu_channel_1, &count1);
  (void)rx_tpu_read_count(k_tpu_channel_2, &count2);
  TEST_ASSERT_EQUAL_UINT16(100, count1);
  TEST_ASSERT_EQUAL_UINT16(200, count2);

  /* Deinit channel 1 should not affect channel 2 */
  (void)rx_tpu_deinit(k_tpu_channel_1);

  /* Channel 2 should still be readable */
  (void)rx_tpu_read_count(k_tpu_channel_2, &count2);
  TEST_ASSERT_EQUAL_UINT16(200, count2);

  /* Channel 1 should fail (uninit) */
  rx_err_t err = rx_tpu_read_count(k_tpu_channel_1, &count1);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Internal Helper Default-Branch Coverage Tests
 * =============================================================================
 */

#ifdef UNIT_TEST
/**
 * @brief Forward declaration of internal helpers exposed via RX_STATIC_TESTABLE
 */
extern volatile rx_tpu_regs_t* internal_get_regs(rx_tpu_channel_t channel);
extern uint8_t                 internal_get_cst_bit(rx_tpu_channel_t channel);
#endif

/**
 * @brief Test: internal_get_regs default branch with invalid channel
 *
 * @details
 * Calls internal_get_regs with an invalid channel value to exercise the
 * default case (RX_ASSERT + return nullptr). In UNIT_TEST builds
 * internal_rx_fatal_error returns instead of halting, so the return
 * statement is reached and nullptr is returned.
 */
void test_internal_get_regs_invalid_channel(void)
{
#ifdef UNIT_TEST
  volatile rx_tpu_regs_t* result = internal_get_regs((rx_tpu_channel_t)0);
  TEST_ASSERT_NULL((void*)result);
#else
  TEST_IGNORE_MESSAGE("Only testable in UNIT_TEST builds");
#endif
}

/**
 * @brief Test: internal_get_cst_bit default branch with invalid channel
 *
 * @details
 * Calls internal_get_cst_bit with an invalid channel value to exercise the
 * default case (RX_ASSERT + return 0). In UNIT_TEST builds
 * internal_rx_fatal_error returns instead of halting, so the return 0
 * statement is reached.
 */
void test_internal_get_cst_bit_invalid_channel(void)
{
#ifdef UNIT_TEST
  uint8_t result = internal_get_cst_bit((rx_tpu_channel_t)0);
  TEST_ASSERT_EQUAL_UINT8(0, result);
#else
  TEST_IGNORE_MESSAGE("Only testable in UNIT_TEST builds");
#endif
}

/* =============================================================================
 * Unity Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Init success tests */
  RUN_TEST(test_init_tpu1_phase_mode_4);
  RUN_TEST(test_init_tpu2_phase_mode_4);
  RUN_TEST(test_init_tpu4_phase_mode_1);
  RUN_TEST(test_init_tpu5_phase_mode_3);

  /* Init error path tests */
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_invalid_channel_0);
  RUN_TEST(test_init_invalid_channel_3);
  RUN_TEST(test_init_invalid_phase_mode);
  RUN_TEST(test_init_double_init);

  /* Start/stop tests */
  RUN_TEST(test_start_tpu1);
  RUN_TEST(test_stop_tpu2);
  RUN_TEST(test_start_uninit_channel);
  RUN_TEST(test_stop_uninit_channel);
  RUN_TEST(test_start_invalid_channel);
  RUN_TEST(test_stop_invalid_channel);

  /* Read count tests */
  RUN_TEST(test_read_count_tpu1);
  RUN_TEST(test_read_count_max_value);
  RUN_TEST(test_read_count_null_ptr);
  RUN_TEST(test_read_count_uninit);
  RUN_TEST(test_read_count_invalid_channel);

  /* Read direction tests */
  RUN_TEST(test_read_direction_counting_up);
  RUN_TEST(test_read_direction_counting_down);
  RUN_TEST(test_read_direction_null_ptr);
  RUN_TEST(test_read_direction_uninit);
  RUN_TEST(test_read_direction_invalid_channel);

  /* Reset count tests */
  RUN_TEST(test_reset_count);
  RUN_TEST(test_reset_count_invalid_channel);
  RUN_TEST(test_reset_count_uninit);

  /* Deinit tests */
  RUN_TEST(test_deinit_resets_registers);
  RUN_TEST(test_deinit_then_start_fails);
  RUN_TEST(test_deinit_then_reinit);
  RUN_TEST(test_deinit_invalid_channel);

  /* Module clock test */
  RUN_TEST(test_init_enables_module_clock);

  /* Register config tests */
  RUN_TEST(test_phase_mode_2_register);
  RUN_TEST(test_init_stops_counter_first);
  RUN_TEST(test_multi_channel_independent);

  /* Internal helper default-branch tests */
  RUN_TEST(test_internal_get_regs_invalid_channel);
  RUN_TEST(test_internal_get_cst_bit_invalid_channel);

  return UNITY_END();
}
