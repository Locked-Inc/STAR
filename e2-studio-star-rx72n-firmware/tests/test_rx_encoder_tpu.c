/* tests/test_rx_encoder_tpu.c */

/**
 * @file test_rx_encoder_tpu.c
 * @brief Unit Tests for TPU Quadrature Encoder Driver
 *
 * @details
 * Tests the rx_encoder_tpu module which provides 16-bit overflow detection,
 * velocity calculation, and position tracking using the TPU HAL driver.
 *
 * Tests exercise:
 * - Initialization/deinitialization
 * - Raw counter reads
 * - Overflow-handled count reads (forward, reverse, wrap)
 * - Velocity calculation
 * - Count reset and set
 * - Error handling (null pointers, invalid channels, uninit)
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project
 * @since Version 1.0.0
 */

#include <string.h>

#include "mock_rx_onewire_hw.h"
#include "mock_rx_tpu_regs.h"
#include "rx_encoder_tpu.h"
#include "unity.h"

/* =============================================================================
 * Mock TPU Register Storage (defined by test, used by shadow headers)
 * =============================================================================
 */

rx_tpu_control_regs_t g_mock_tpu_control;
rx_tpu_regs_t         g_mock_tpu_regs[k_mock_tpu_channel_count];

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum : uint16_t {
  k_test_counts_per_rev = 1364, /**< 341 PPR x 4 */
  k_test_half_range     = 32768,
  k_test_full_range     = 65535,
} test_constants_t;

typedef enum : uint16_t {
  k_test_count_100  = 100,
  k_test_count_200  = 200,
  k_test_count_1000 = 1000,
  k_test_count_1100 = 1100,
} test_count_values_t;

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

void setUp(void)
{
  /* Reset all mock TPU registers */
  memset(&g_mock_tpu_control, 0, sizeof(g_mock_tpu_control));
  memset(g_mock_tpu_regs, 0, sizeof(g_mock_tpu_regs));

  /* Reset system register mocks */
  memset(&g_mock_onewire_system_regs, 0, sizeof(g_mock_onewire_system_regs));

  /* Deinit all channels to clean state */
  (void)rx_tpu_encoder_deinit(k_tpu_channel_1);
  (void)rx_tpu_encoder_deinit(k_tpu_channel_2);
  (void)rx_tpu_encoder_deinit(k_tpu_channel_4);
  (void)rx_tpu_encoder_deinit(k_tpu_channel_5);

  /* Deinit TPU HAL channels */
  (void)rx_tpu_deinit(k_tpu_channel_1);
  (void)rx_tpu_deinit(k_tpu_channel_2);
  (void)rx_tpu_deinit(k_tpu_channel_4);
  (void)rx_tpu_deinit(k_tpu_channel_5);
}

void tearDown(void)
{
  (void)rx_tpu_encoder_deinit(k_tpu_channel_1);
  (void)rx_tpu_encoder_deinit(k_tpu_channel_2);
  (void)rx_tpu_encoder_deinit(k_tpu_channel_4);
  (void)rx_tpu_encoder_deinit(k_tpu_channel_5);

  (void)rx_tpu_deinit(k_tpu_channel_1);
  (void)rx_tpu_deinit(k_tpu_channel_2);
  (void)rx_tpu_deinit(k_tpu_channel_4);
  (void)rx_tpu_deinit(k_tpu_channel_5);
}

/* =============================================================================
 * Helper: Initialize a channel with standard STAR config
 * =============================================================================
 */

static rx_err_t helper_init_channel(const rx_tpu_channel_t channel)
{
  const rx_tpu_encoder_config_t config = {
    .channel          = channel,
    .counts_per_rev   = k_test_counts_per_rev,
    .invert_direction = false,
  };
  return rx_tpu_encoder_init(&config);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/** @brief Init TPU1 encoder succeeds */
void test_init_tpu1(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
}

/** @brief Init TPU2 encoder succeeds */
void test_init_tpu2(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_2));
}

/** @brief Init TPU4 encoder succeeds */
void test_init_tpu4(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_4));
}

/** @brief Init TPU5 encoder succeeds */
void test_init_tpu5(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_5));
}

/** @brief Null config returns null_ptr error */
void test_init_null_config(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tpu_encoder_init(NULL));
}

/** @brief Invalid channel (0) returns invalid_arg */
void test_init_invalid_channel(void)
{
  const rx_tpu_encoder_config_t config = {
    .channel          = 0,
    .counts_per_rev   = k_test_counts_per_rev,
    .invert_direction = false,
  };
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tpu_encoder_init(&config));
}

/** @brief Zero counts_per_rev returns invalid_arg */
void test_init_zero_counts_per_rev(void)
{
  const rx_tpu_encoder_config_t config = {
    .channel          = k_tpu_channel_1,
    .counts_per_rev   = 0,
    .invert_direction = false,
  };
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tpu_encoder_init(&config));
}

/** @brief Double init returns invalid_state */
void test_init_double_init(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, helper_init_channel(k_tpu_channel_1));
}

/* =============================================================================
 * Raw Read Tests
 * =============================================================================
 */

/** @brief Raw read returns current TCNT */
void test_read_raw_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  /* Set mock register value */
  g_mock_tpu_regs[0].tcnt = k_test_count_1000;

  uint16_t count = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_raw(k_tpu_channel_1, &count));
  TEST_ASSERT_EQUAL_UINT16(k_test_count_1000, count);
}

/** @brief Raw read with null pointer returns null_ptr */
void test_read_raw_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tpu_encoder_read_raw(k_tpu_channel_1, NULL));
}

/** @brief Raw read on uninit channel returns invalid_state */
void test_read_raw_uninit(void)
{
  uint16_t count = 0;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_read_raw(k_tpu_channel_1, &count));
}

/* =============================================================================
 * Count Read Tests (Overflow Handling)
 * =============================================================================
 */

/** @brief Forward motion: count 0 -> 100 = delta +100 */
void test_read_count_forward(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  g_mock_tpu_regs[0].tcnt = k_test_count_100;

  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_count_100, state.total_count);
}

/** @brief Forward motion across 65535->0 boundary */
void test_read_count_forward_wrap(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_2));

  /* Walk counter up to near overflow in safe steps (<32768 each) */
  g_mock_tpu_regs[1].tcnt = 30000;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_2, &state));
  TEST_ASSERT_EQUAL_INT32(30000, state.total_count);

  g_mock_tpu_regs[1].tcnt = 60000;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_2, &state));
  TEST_ASSERT_EQUAL_INT32(60000, state.total_count);

  g_mock_tpu_regs[1].tcnt = 65530;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_2, &state));
  TEST_ASSERT_EQUAL_INT32(65530, state.total_count);

  /* Forward wrap: 65530 -> 10 (16 counts forward across boundary) */
  g_mock_tpu_regs[1].tcnt = 10;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_2, &state));
  TEST_ASSERT_EQUAL_INT32(65546, state.total_count);
}

/** @brief Reverse motion: count 1100 -> 1000 = delta -100 */
void test_read_count_reverse(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  /* First read at 1100 */
  g_mock_tpu_regs[0].tcnt = k_test_count_1100;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_count_1100, state.total_count);

  /* Second read at 1000 (reverse 100 counts) */
  g_mock_tpu_regs[0].tcnt = k_test_count_1000;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_count_1000, state.total_count);
}

/** @brief Reverse motion across 0->65535 boundary */
void test_read_count_reverse_wrap(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  /* First read at 10 */
  g_mock_tpu_regs[0].tcnt = 10;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(10, state.total_count);

  /* Second read at 65530 (wrapped backward by 16 counts) */
  g_mock_tpu_regs[0].tcnt = 65530;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  /* 10 - 16 = -6 */
  TEST_ASSERT_EQUAL_INT32(-6, state.total_count);
}

/** @brief Stationary: count doesn't change, delta = 0 */
void test_read_count_stationary(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  g_mock_tpu_regs[0].tcnt = k_test_count_200;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_count_200, state.total_count);

  /* Read again without changing register */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_count_200, state.total_count);
}

/** @brief Null state pointer returns null_ptr */
void test_read_count_null_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tpu_encoder_read_count(k_tpu_channel_1, NULL));
}

/** @brief Read count on uninit channel returns invalid_state */
void test_read_count_uninit(void)
{
  rx_encoder_state_t state;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
}

/** @brief Direction inversion reverses delta sign */
void test_read_count_inverted(void)
{
  const rx_tpu_encoder_config_t config = {
    .channel          = k_tpu_channel_4,
    .counts_per_rev   = k_test_counts_per_rev,
    .invert_direction = true,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_init(&config));

  g_mock_tpu_regs[2].tcnt = k_test_count_100;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_4, &state));
  /* With inversion, forward 100 becomes -100 */
  TEST_ASSERT_EQUAL_INT32(-100, state.total_count);
}

/** @brief Revolution count correct after full revolution */
void test_read_count_revolutions(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  /* Set count to exactly 1 revolution (1364 counts) */
  g_mock_tpu_regs[0].tcnt = k_test_counts_per_rev;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(1, state.revolutions);
}

/* =============================================================================
 * Velocity Tests
 * =============================================================================
 */

/** @brief Velocity with known delta returns correct RPS */
void test_velocity_forward(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  /* First velocity call establishes baseline */
  g_mock_tpu_regs[0].tcnt = 0;
  float velocity          = 0.0f;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_velocity(&velocity, 0.01f, k_tpu_channel_1));

  /* Second call: 1364 counts in 1 second = 1.0 RPS */
  g_mock_tpu_regs[0].tcnt = k_test_counts_per_rev;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_velocity(&velocity, 1.0f, k_tpu_channel_1));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, velocity);
}

/** @brief Null velocity_rps returns null_ptr */
void test_velocity_null_ptr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_tpu_encoder_read_velocity(NULL, 0.01f, k_tpu_channel_1));
}

/** @brief Zero delta_time returns invalid_arg */
void test_velocity_zero_dt(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  float velocity = 0.0f;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tpu_encoder_read_velocity(&velocity, 0.0f, k_tpu_channel_1));
}

/** @brief Negative delta_time returns invalid_arg */
void test_velocity_negative_dt(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  float velocity = 0.0f;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_tpu_encoder_read_velocity(&velocity, -1.0f, k_tpu_channel_1));
}

/** @brief Velocity on uninit channel returns invalid_state */
void test_velocity_uninit(void)
{
  float velocity = 0.0f;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_tpu_encoder_read_velocity(&velocity, 0.01f, k_tpu_channel_1));
}

/* =============================================================================
 * Reset Tests
 * =============================================================================
 */

/** @brief Reset clears count to zero */
void test_reset_clears_count(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  /* Accumulate some counts */
  g_mock_tpu_regs[0].tcnt = k_test_count_1000;
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_count_1000, state.total_count);

  /* Reset */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_reset(k_tpu_channel_1));

  /* Read again - should be zero delta from reset point */
  g_mock_tpu_regs[0].tcnt = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(0, state.total_count);
  TEST_ASSERT_EQUAL_INT32(0, state.revolutions);
}

/** @brief Reset on uninit channel returns invalid_state */
void test_reset_uninit(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_reset(k_tpu_channel_1));
}

/** @brief Reset on invalid channel returns invalid_arg */
void test_reset_invalid_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tpu_encoder_reset(0));
}

/* =============================================================================
 * Set Count Tests
 * =============================================================================
 */

/** @brief Set count updates total_count */
void test_set_count(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_set_count(5000, k_tpu_channel_1));

  /* Read - delta from set point */
  g_mock_tpu_regs[0].tcnt = (uint16_t)(5000 & 0xFFFF);
  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
  TEST_ASSERT_EQUAL_INT32(5000, state.total_count);
}

/** @brief Set count on uninit returns invalid_state */
void test_set_count_uninit(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_set_count(100, k_tpu_channel_1));
}

/* =============================================================================
 * Deinit Tests
 * =============================================================================
 */

/** @brief Deinit marks channel as uninitialized */
void test_deinit_then_read_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_deinit(k_tpu_channel_1));

  rx_encoder_state_t state;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_read_count(k_tpu_channel_1, &state));
}

/** @brief Deinit then reinit succeeds */
void test_deinit_then_reinit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_deinit(k_tpu_channel_1));

  /* Need to deinit TPU HAL too before reinit */
  (void)rx_tpu_deinit(k_tpu_channel_1);

  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
}

/** @brief Deinit on uninit channel returns invalid_state */
void test_deinit_uninit(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_deinit(k_tpu_channel_1));
}

/** @brief Deinit on invalid channel returns invalid_arg */
void test_deinit_invalid_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_tpu_encoder_deinit(3));
}

/* =============================================================================
 * Multi-Channel Tests
 * =============================================================================
 */

/** @brief Two channels operate independently */
void test_multi_channel_independent(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_channel(k_tpu_channel_2));

  /* Set different counts */
  g_mock_tpu_regs[0].tcnt = k_test_count_100;
  g_mock_tpu_regs[1].tcnt = k_test_count_200;

  rx_encoder_state_t state1;
  rx_encoder_state_t state2;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_1, &state1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_2, &state2));

  TEST_ASSERT_EQUAL_INT32(k_test_count_100, state1.total_count);
  TEST_ASSERT_EQUAL_INT32(k_test_count_200, state2.total_count);

  /* Deinit channel 1, channel 2 still works */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_deinit(k_tpu_channel_1));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_tpu_encoder_read_count(k_tpu_channel_1, &state1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_tpu_encoder_read_count(k_tpu_channel_2, &state2));
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization */
  RUN_TEST(test_init_tpu1);
  RUN_TEST(test_init_tpu2);
  RUN_TEST(test_init_tpu4);
  RUN_TEST(test_init_tpu5);
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_invalid_channel);
  RUN_TEST(test_init_zero_counts_per_rev);
  RUN_TEST(test_init_double_init);

  /* Raw reads */
  RUN_TEST(test_read_raw_value);
  RUN_TEST(test_read_raw_null);
  RUN_TEST(test_read_raw_uninit);

  /* Count reads (overflow handling) */
  RUN_TEST(test_read_count_forward);
  RUN_TEST(test_read_count_forward_wrap);
  RUN_TEST(test_read_count_reverse);
  RUN_TEST(test_read_count_reverse_wrap);
  RUN_TEST(test_read_count_stationary);
  RUN_TEST(test_read_count_null_state);
  RUN_TEST(test_read_count_uninit);
  RUN_TEST(test_read_count_inverted);
  RUN_TEST(test_read_count_revolutions);

  /* Velocity */
  RUN_TEST(test_velocity_forward);
  RUN_TEST(test_velocity_null_ptr);
  RUN_TEST(test_velocity_zero_dt);
  RUN_TEST(test_velocity_negative_dt);
  RUN_TEST(test_velocity_uninit);

  /* Reset */
  RUN_TEST(test_reset_clears_count);
  RUN_TEST(test_reset_uninit);
  RUN_TEST(test_reset_invalid_channel);

  /* Set count */
  RUN_TEST(test_set_count);
  RUN_TEST(test_set_count_uninit);

  /* Deinit */
  RUN_TEST(test_deinit_then_read_fails);
  RUN_TEST(test_deinit_then_reinit);
  RUN_TEST(test_deinit_uninit);
  RUN_TEST(test_deinit_invalid_channel);

  /* Multi-channel */
  RUN_TEST(test_multi_channel_independent);

  return UNITY_END();
}
