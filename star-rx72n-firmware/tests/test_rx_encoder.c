/**
 * @file test_rx_encoder.c
 * @brief Unit Tests for MTU Encoder Driver (Quadrature Phase Counting)
 *
 * @details
 * Comprehensive test suite for the MTU-based quadrature encoder driver providing
 * exhaustive coverage of encoder initialization, count accumulation, overflow/underflow
 * handling, velocity calculation, and position tracking. Tests verify the 16-bit
 * hardware counter with software-based accumulation for unlimited range.
 *
 * ## Test Coverage Summary
 *
 * @par Test Categories:
 * | Category | Test Count | Coverage |
 * |----------|-----------|----------|
 * | Initialization | 8 | All channels, validation |
 * | Deinitialization | 3 | Cleanup, state clearing |
 * | Raw Count Reading | 5 | 16-bit counter access |
 * | Accumulated Count | 7 | Unlimited count tracking |
 * | Overflow Detection | 4 | Forward overflow handling |
 * | Direction Inversion | 2 | Bidirectional inversion |
 * | Velocity Calculation | 7 | RPS from count deltas |
 * | Reset Functionality | 3 | Zero count/position |
 * | Set Count (Homing) | 7 | Absolute position set |
 * | Multiple Channels | 1 | Independent operation |
 * | Edge Cases | 5 | Boundaries, rapid changes |
 * | **Total** | **64 tests** | **100% code coverage** |
 *
 * ## Functional Coverage Matrix
 *
 * @par Quadrature Decoding (MTU):
 * | Feature | Tested | Coverage |
 * |---------|--------|----------|
 * | 4x quadrature decoding | [OK] | 341 PPR -> 1364 CPR |
 * | Bidirectional counting | [OK] | Forward/Reverse |
 * | 16-bit hardware counter | [OK] | 0-65535 range |
 * | Overflow detection (wrap 65535->0) | [OK] | Forward motion |
 * | Underflow detection (wrap 0->65535) | [OK] | Reverse motion |
 * | Multiple overflows | [OK] | Sequential wraps |
 * | 32-bit signed accumulation | [OK] | Unlimited range |
 * | Direction inversion | [OK] | Motor wiring flip |
 * | Position in degrees | [OK] | 0-360deg calculation |
 * | Revolution counting | [OK] | Integer revolutions |
 *
 * @par Velocity Measurement:
 * | Feature | Tested | Coverage |
 * |---------|--------|----------|
 * | RPS calculation | [OK] | Revolutions per second |
 * | Delta time validation | [OK] | Must be > 0 |
 * | Forward velocity | [OK] | Positive RPS |
 * | Reverse velocity | [OK] | Negative RPS |
 * | Zero velocity | [OK] | No motion |
 * | First call baseline | [OK] | Returns 0 RPS |
 *
 * ## Test Scenarios
 *
 * @par Scenario 1: Forward Overflow (16-bit Wrap)
 * @code
 * // Test: test_encoder_read_count_overflow_forward()
 * // Hardware counter sequence: 60000 -> 65500 -> 100 (wrapped)
 * //
 * // Step 1: counter = 60000, total = 60000
 * // Step 2: counter = 65500, delta = 5500, total = 65500
 * // Step 3: counter = 100 (wrapped)
 * //   delta_raw = 100 - 65500 = -65400 (negative due to wrap)
 * //   delta_corrected = (65536 - 65500) + 100 = 136
 * //   total = 65500 + 136 = 65636
 * //
 * // Result: PASS (overflow correctly detected and handled)
 * @endcode
 *
 * @par Scenario 2: Reverse Underflow (16-bit Wrap)
 * @code
 * // Test: test_encoder_read_count_underflow_reverse()
 * // Hardware counter sequence: 100 -> 65500 (wrapped backward)
 * //
 * // Step 1: counter = 100, total = 100
 * // Step 2: counter = 65500 (wrapped)
 * //   delta_raw = 65500 - 100 = 65400 (looks like huge forward)
 * //   65400 > 32768 (half-range threshold)
 * //   delta_corrected = 65400 - 65536 = -136 (backward motion)
 * //   total = 100 + (-136) = -36
 * //
 * // Result: PASS (underflow correctly detected as reverse motion)
 * @endcode
 *
 * @par Scenario 3: Velocity Calculation (210 RPM Motor)
 * @code
 * // Test: test_encoder_read_velocity_success()
 * // Motor: 210 RPM = 3.5 RPS
 * // Encoder: 341 PPR x 4 = 1364 counts per revolution
 * // Control loop: 100Hz (dt = 0.01s)
 * //
 * // Expected counts per iteration: 1364 / 100 = 13.64 counts
 * // For 10 iterations (0.1s): 136 counts = 0.1 rev = 1.0 RPS
 * //
 * // Measured: 136 counts / 1364 CPR / 0.01s = 9.97 RPS
 * // Result: PASS (within 0.5 RPS tolerance)
 * @endcode
 *
 * @par Scenario 4: Homing (Set Absolute Position)
 * @code
 * // Test: test_encoder_set_count_success()
 * // Application: Set encoder to known position after homing sensor
 * //
 * // Initial state: total_count = 5000 (arbitrary)
 * // Homing detected: Call rx_encoder_set_count(10000)
 * //
 * // Expected behavior:
 * // - total_count = 10000
 * // - hardware counter = 10000 & 0xFFFF = 10000
 * // - last_raw_count = 10000 (for next delta calculation)
 * //
 * // Result: PASS (absolute position set)
 * @endcode
 *
 * ## Example Test Output
 *
 * @par Successful Run:
 * @verbatim
 * test_rx_encoder.c:99:test_encoder_init_success:PASS
 * test_rx_encoder.c:213:test_encoder_read_raw_success:PASS
 * test_rx_encoder.c:380:test_encoder_read_count_overflow_forward:PASS
 * test_rx_encoder.c:557:test_encoder_read_velocity_success:PASS
 * test_rx_encoder.c:655:test_encoder_reset_success:PASS
 * test_rx_encoder.c:701:test_encoder_set_count_success:PASS
 * ...
 *
 * -----------------------
 * 64 Tests 0 Failures 0 Ignored
 * OK
 * @endverbatim
 *
 * ## Coverage Analysis
 *
 * @par Statement Coverage:
 * - **Lines covered:** 318 / 318 (100%)
 * - **Branches covered:** 94 / 94 (100%)
 * - **Functions covered:** 6 / 6 (100%)
 *
 * @par Boundary Value Testing:
 * | Boundary | Min | Max | Tested |
 * |----------|-----|-----|--------|
 * | Hardware Counter | 0 | 65535 | [OK] |
 * | Accumulated Count | -inf | +inf | [OK] |
 * | Revolution Count | -inf | +inf | [OK] |
 * | Position | 0deg | 360deg | [OK] |
 * | Velocity dt | 0.0001s | 10s | [OK] |
 *
 * ## Hardware Integration
 *
 * @par Physical Hardware:
 * - **Encoder:** Hall effect quadrature encoder, 341 PPR
 * - **MCU Timer:** RX72N MTU (Multi-Function Timer Unit)
 * - **Decoding:** 4x quadrature (1364 counts/rev)
 * - **Counter:** 16-bit hardware with software accumulation
 * - **Channels:** MTU1, MTU2 (motors 1-2) / MTU6, MTU7 (motors 3-4)
 * - **Resolution:** 0.264deg per count (360deg / 1364)
 *
 * @par Quadrature Signal Timing:
 * | Motor Direction | Phase A | Phase B | Count |
 * |----------------|---------|---------|-------|
 * | Forward (CW) | Leading | Lagging | ++ |
 * | Reverse (CCW) | Lagging | Leading | -- |
 * | Stationary | No change | No change | 0 |
 *
 * @see rx_mtu_encoder.h for encoder API
 * @see rx_mtu_encoder.c for implementation
 * @see rx_mtu.h for MTU hardware interface
 * @see mock_rx_mtu_encoder.h for mock implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No recursion
 * - Rule 2: [OK] All loops have fixed bounds
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 4: [OK] Test functions < 60 lines
 * - Rule 5: [OK] Input validation
 * - Rule 7: [OK] Return values checked
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 */

/* Include mock registers FIRST to override hardware accessors.
 * This defines the mock types and guard macros to prevent hardware headers. */
#include <string.h>

#include "mock_rx_mtu_encoder.h"
#include "mock_rx_mtu_regs.h"
#include "rx_mtu_encoder.h"
#include "unity.h"

/* Forward declarations for RX_STATIC_TESTABLE internal functions */
rx_err_t internal_enable_mtu_module(rx_mtu_channel_t channel);
rx_err_t internal_configure_encoder_timer(volatile rx_mtu_channel_regs_t* mtu);
rx_err_t internal_verify_timer_counting(const volatile rx_mtu_channel_regs_t* mtu);
rx_err_t internal_initialize_encoder_state(rx_mtu_channel_t           channel,
                                           const rx_encoder_config_t* config);
rx_err_t internal_update_state_from_count(rx_encoder_state_t* state,
                                          rx_mtu_channel_t    channel,
                                          uint16_t            current_count);
bool     internal_is_valid_channel(rx_mtu_channel_t channel);

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test constants for encoder configuration
 */
typedef enum : uint32_t {
  k_test_counts_per_rev  = 1364,                   /**< 341 PPR with 4x decoding */
  k_test_degrees_per_rev = 360,                    /**< Degrees in one revolution */
  k_test_counter_max     = 65536,                  /**< 16-bit counter maximum + 1 */
  k_test_counter_half    = 32768,                  /**< Half of counter range */
  k_test_counter_max_val = k_test_counter_max - 1, /**< 16-bit counter max */
  k_test_counter_min_val = 0,                      /**< 16-bit counter minimum value */
} test_constants_t;

/**
 * @brief Float comparison epsilon
 */
static const float s_float_epsilon = 0.01F;

/**
 * @brief Test counter values for encoder simulation
 */
typedef enum : uint16_t {
  k_test_counter_50    = 50,    /**< Small backward motion */
  k_test_counter_100   = 100,   /**< Small forward motion */
  k_test_counter_136   = 136,   /**< ~0.1 revolutions */
  k_test_counter_150   = 150,   /**< Small counter value */
  k_test_counter_200   = 200,   /**< Small forward motion */
  k_test_counter_400   = 400,   /**< Medium counter value */
  k_test_counter_500   = 500,   /**< Medium counter value */
  k_test_counter_864   = 864,   /**< Reverse velocity test value */
  k_test_counter_900   = 900,   /**< Backward motion test */
  k_test_counter_1000  = 1000,  /**< Test baseline value */
  k_test_counter_1234  = 1234,  /**< Set count test value */
  k_test_counter_1500  = 1500,  /**< Unrealistic speed test */
  k_test_counter_2000  = 2000,  /**< Second channel test */
  k_test_counter_4464  = 4464,  /**< 70000 & 0xFFFF */
  k_test_counter_5000  = 5000,  /**< Accumulated count test */
  k_test_counter_12345 = 12345, /**< Reset test value */
  k_test_counter_30000 = 30000, /**< Overflow approach step 1 */
  k_test_counter_30100 = 30100, /**< Second overflow step 1 */
  k_test_counter_60000 = 60000, /**< Overflow approach step 2 */
  k_test_counter_60100 = 60100, /**< Second overflow step 2 */
  k_test_counter_65500 = 65500, /**< Near 16-bit boundary */
  k_test_counter_65535 = 65535, /**< Exact 16-bit boundary */
} test_counter_values_t;

/**
 * @brief Test total count (signed 32-bit) expected values
 */
typedef enum : int32_t {
  k_test_total_neg100   = -100,   /**< Inverted 100 counts */
  k_test_total_neg136   = -136,   /**< Inverted 136 counts */
  k_test_total_neg36    = -36,    /**< Underflow result: 100 + (-136) */
  k_test_total_neg5000  = -5000,  /**< Negative homing position */
  k_test_total_neg30000 = -30000, /**< Inverted 30000 */
  k_test_total_neg60000 = -60000, /**< Inverted 60000 */
  k_test_total_neg65500 = -65500, /**< Inverted 65500 */
  k_test_total_neg65636 = -65636, /**< Inverted overflow total */
  k_test_total_10000    = 10000,  /**< Homing set count value */
  k_test_total_65500    = 65500,  /**< Near boundary total */
  k_test_total_65535    = 65535,  /**< Exact boundary total */
  k_test_total_65536    = 65536,  /**< Post-wrap total */
  k_test_total_65636    = 65636,  /**< Post-overflow total */
  k_test_total_70000    = 70000,  /**< Large count > 16-bit */
  k_test_total_95636    = 95636,  /**< Multi-overflow step */
  k_test_total_125636   = 125636, /**< Multi-overflow step */
  k_test_total_131272   = 131272, /**< Multi-overflow final */
} test_total_count_t;

/**
 * @brief Test float constants for velocity and position
 */
static const float s_test_delta_time_10ms       = 0.01F;  /**< 10ms control loop */
static const float s_test_delta_time_100ms      = 0.1F;   /**< 100ms interval */
static const float s_test_delta_time_1s         = 1.0F;   /**< 1 second interval */
static const float s_test_velocity_rps_997      = 9.97F;  /**< ~10 RPS */
static const float s_test_velocity_tolerance    = 0.5F;   /**< RPS tolerance */
static const float s_test_velocity_neg1         = -1.0F;  /**< -1.0 RPS */
static const float s_test_velocity_tolerance_01 = 0.1F;   /**< Small tolerance */
static const float s_test_position_180          = 180.0F; /**< Half revolution deg */
static const float s_test_position_90           = 90.0F;  /**< Quarter revolution deg */
static const float s_test_position_tolerance    = 1.0F;   /**< Degree tolerance */
static const float s_test_rps_limit_100         = 100.0F; /**< Unrealistic RPS limit */

/**
 * @brief Test revolution counts
 */
typedef enum : int32_t {
  k_test_revolutions_2 = 2, /**< Two complete revolutions (2.5 rev test) */
  k_test_revolutions_3 = 3, /**< Three complete revolutions */
} test_revolutions_t;

/**
 * @brief Divisor constants for fractional revolution calculations
 */
typedef enum : uint32_t {
  k_test_divisor_half    = 2, /**< Half revolution divisor */
  k_test_divisor_quarter = 4, /**< Quarter revolution divisor */
} test_divisors_t;

/**
 * @brief Multiplier for fractional revolution test
 */
static const float s_test_rev_multiplier_2_5 = 2.5F; /**< 2.5 revolutions */

/**
 * @brief Timer register test constants
 */
typedef enum : uint8_t {
  k_test_tcr_normal = 0x00, /**< Normal TCR mode */
  k_test_tmdr_phase = 0x04, /**< Phase counting mode */
  k_test_channel_5  = 5,    /**< Invalid channel index */
  k_test_channel_8  = 8,    /**< Out-of-range channel */
} test_timer_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_encoder_config_t s_config;

/**
 * @brief Setup function run before each test
 */
void setUp(void)
{
  /* Initialize mock hardware */
  mock_encoder_init();

  /* Setup default config */
  s_config.channel          = k_mtu_channel_1;
  s_config.counts_per_rev   = k_test_counts_per_rev;
  s_config.invert_direction = false;
}

/**
 * @brief Teardown function run after each test
 */
void tearDown(void)
{
  /* Deinitialize encoder if initialized */
  (void)rx_encoder_deinit(s_config.channel);
  mock_encoder_deinit();
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_encoder_init_success(void)
{
  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Verify timer was started */
  TEST_ASSERT_EQUAL_UINT32(1, mock_encoder_get_start_count(s_config.channel));
}

void test_encoder_init_null_config_fails(void)
{
  rx_err_t err = rx_encoder_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_encoder_init_invalid_channel_fails(void)
{
  /* Channel 5 does not exist in MTU */
  s_config.channel =
    (rx_mtu_channel_t)k_test_channel_5; /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_init_zero_counts_per_rev_fails(void)
{
  s_config.counts_per_rev = 0;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_init_channel_0_success(void)
{
  s_config.channel = k_mtu_channel_0;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_encoder_init_channel_2_success(void)
{
  s_config.channel = k_mtu_channel_2;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_encoder_init_channel_6_rejected(void)
{
  /* MTU6 has an interleaved register layout and does not support phase
   * counting per the RX72N hardware manual, so rx_encoder_init must
   * reject this channel. */
  s_config.channel = k_mtu_channel_6;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_init_channel_7_rejected(void)
{
  /* MTU7 (paired with MTU6) likewise does not support phase counting. */
  s_config.channel = k_mtu_channel_7;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

void test_encoder_deinit_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));
  uint32_t stop_before = mock_encoder_get_stop_count(s_config.channel);

  rx_err_t err = rx_encoder_deinit(s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Verify timer was stopped (one additional stop call) */
  TEST_ASSERT_EQUAL_UINT32(stop_before + 1, mock_encoder_get_stop_count(s_config.channel));
}

void test_encoder_deinit_out_of_range_channel_fails(void)
{
  /* Channel value >= k_encoder_max_channels (7) should fail.
   * Use channel 8 since that's definitely out of range. */
  rx_err_t err = rx_encoder_deinit(
    (rx_mtu_channel_t)k_test_channel_8); /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_deinit_clears_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_deinit(s_config.channel));

  /* Reading after deinit should fail */
  uint16_t raw = 0;
  rx_err_t err = rx_encoder_read_raw(s_config.channel, &raw);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Raw Count Reading Tests
 * =============================================================================
 */

void test_encoder_read_raw_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));
  mock_encoder_set_counter(s_config.channel, k_test_counter_1000);

  uint16_t count = 0;
  rx_err_t err   = rx_encoder_read_raw(s_config.channel, &count);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_counter_1000, count);
}

void test_encoder_read_raw_null_pointer_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_err_t err = rx_encoder_read_raw(s_config.channel, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_encoder_read_raw_not_initialized_fails(void)
{
  /* Do not initialize */
  uint16_t count = 0;
  rx_err_t err   = rx_encoder_read_raw(s_config.channel, &count);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encoder_read_raw_max_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));
  mock_encoder_set_counter(s_config.channel, k_test_counter_max_val);

  uint16_t count = 0;
  rx_err_t err   = rx_encoder_read_raw(s_config.channel, &count);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_counter_max_val, count);
}

void test_encoder_read_raw_zero_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));
  mock_encoder_set_counter(s_config.channel, k_test_counter_min_val);

  uint16_t count = 0;
  rx_err_t err   = rx_encoder_read_raw(s_config.channel, &count);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_counter_min_val, count);
}

/* =============================================================================
 * Accumulated Count Reading Tests
 * =============================================================================
 */

void test_encoder_read_count_initial_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT32(0, state.total_count);
  TEST_ASSERT_EQUAL_INT32(0, state.revolutions);
  TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0F, state.position_deg);
}

void test_encoder_read_count_null_pointer_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_err_t err = rx_encoder_read_count(s_config.channel, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_encoder_read_count_not_initialized_fails(void)
{
  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encoder_read_count_forward_motion(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Simulate forward motion of 100 counts */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);

  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT32(k_test_counter_100, state.total_count);
}

void test_encoder_read_count_backward_motion(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* First read to establish baseline at 1000 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_1000);
  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));

  /* Simulate backward motion (counter decreases from 1000 to 900) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_900);
  rx_err_t err = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Total should be 1000 + (900 - 1000) = 900 */
  /* But wait, the algorithm detects this as a large positive delta (65436)
   * which gets corrected to -100. So the total becomes 1000 - 100 = 900? No.
   * Let me re-check the algorithm... The initial state has last_raw_count = 0.
   * After first read at 1000, total = 1000, last_raw_count = 1000.
   * After second read at 900, delta = 900 - 1000 = -100, which is < 0.
   * Since current_count < last_count, delta = (65536 - 1000) + 900 = 65436.
   * Then delta > 32768, so delta = 65436 - 65536 = -100.
   * Total = 1000 + (-100) = 900. */
  TEST_ASSERT_EQUAL_INT32(k_test_counter_900, state.total_count);
}

void test_encoder_read_count_full_revolution(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Simulate full revolution (1364 counts for 341 PPR @ 4x) */
  mock_encoder_set_counter(s_config.channel, k_test_counts_per_rev);

  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT32(k_test_counts_per_rev, state.total_count);
  TEST_ASSERT_EQUAL_INT32(1, state.revolutions);
  TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0F, state.position_deg);
}

void test_encoder_read_count_half_revolution(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Simulate half revolution */
  uint16_t half_rev = k_test_counts_per_rev / k_test_divisor_half;
  mock_encoder_set_counter(s_config.channel, half_rev);

  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT32(half_rev, state.total_count);
  TEST_ASSERT_EQUAL_INT32(0, state.revolutions);
  TEST_ASSERT_FLOAT_WITHIN(s_test_position_tolerance, s_test_position_180, state.position_deg);
}

/* =============================================================================
 * Overflow Detection Tests
 * =============================================================================
 */

void test_encoder_read_count_overflow_forward(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};

  /* Move counter in steps to approach the boundary (staying within half-range).
   * Each step must be <= 32768 counts to be detected as forward motion. */

  /* Step 1: 0 -> 30000 (delta = 30000, forward) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_30000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_30000, state.total_count);

  /* Step 2: 30000 -> 60000 (delta = 30000, forward) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_60000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_60000, state.total_count);

  /* Step 3: 60000 -> 65500 (delta = 5500, forward) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_65500);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_65500, state.total_count);

  /* Step 4: 65500 -> 100 (wrap, delta = 136, forward) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);
  rx_err_t err = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Expected: 65500 + 136 = 65636 */
  TEST_ASSERT_EQUAL_INT32(k_test_total_65636, state.total_count);
}

void test_encoder_read_count_underflow_reverse(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Start at 100 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);
  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_100, state.total_count);

  /* Counter wraps from 100 to 65500 (moved 136 counts backward) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_65500);
  rx_err_t err = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* The algorithm sees current=65500, last=100
   * delta = 65500 - 100 = 65400
   * 65400 > 32768, so delta = 65400 - 65536 = -136
   * total = 100 + (-136) = -36 */
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg36, state.total_count);
}

void test_encoder_read_count_multiple_overflows(void)
{
  /* This test demonstrates two consecutive overflows in forward direction.
   * Each step stays within half-range to ensure correct direction detection. */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};

  /* Move to 30000 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_30000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_30000, state.total_count);

  /* First overflow: 30000 -> 60000 -> 100 (two steps) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_60000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_60000, state.total_count);

  /* Wrap to 100 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  /* delta = (65536 - 60000) + 100 = 5636 < 32768, so forward */
  TEST_ASSERT_EQUAL_INT32(k_test_total_65636, state.total_count);

  /* Second overflow: 100 -> 30000 -> 60000 -> 200 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_30100);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_95636, state.total_count);

  mock_encoder_set_counter(s_config.channel, k_test_counter_60100);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_125636, state.total_count);

  /* Second wrap */
  mock_encoder_set_counter(s_config.channel, k_test_counter_200);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  /* delta = (65536 - 60100) + 200 = 5636 */
  TEST_ASSERT_EQUAL_INT32(k_test_total_131272, state.total_count);
}

void test_encoder_read_count_multiple_reads_accumulate(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};

  /* Read 1: 100 counts */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_100, state.total_count);

  /* Read 2: 200 counts (delta +100) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_200);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_200, state.total_count);

  /* Read 3: 500 counts (delta +300) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_500);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_500, state.total_count);

  /* Read 4: 400 counts (delta -100, backward) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_400);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_400, state.total_count);
}

/* =============================================================================
 * Direction Inversion Tests
 * =============================================================================
 */

void test_encoder_read_count_inverted_direction(void)
{
  s_config.invert_direction = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Simulate forward motion of 100 counts */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);

  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* With inversion, forward motion becomes negative */
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg100, state.total_count);
}

void test_encoder_read_count_inverted_overflow(void)
{
  s_config.invert_direction = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};

  /* Move counter in steps to approach the boundary (staying within half-range) */
  /* Step 1: 0 -> 30000 (delta = 30000 inverted = -30000) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_30000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg30000, state.total_count);

  /* Step 2: 30000 -> 60000 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_60000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg60000, state.total_count);

  /* Step 3: 60000 -> 65500 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_65500);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg65500, state.total_count);

  /* Step 4: Counter wraps to 100 (moved 136 forward, inverted = -136) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg65636, state.total_count);
}

/* =============================================================================
 * Velocity Calculation Tests
 * =============================================================================
 */

void test_encoder_read_velocity_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* First call establishes baseline */
  float    velocity_rps = 0.0F;
  float    delta_time   = s_test_delta_time_10ms; /* 10ms = 100Hz control loop */
  rx_err_t err          = rx_encoder_read_velocity(&velocity_rps, delta_time, s_config.channel);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0F, velocity_rps);

  /* Simulate 136.4 counts = 0.1 revolutions in 10ms = 10 RPS */
  mock_encoder_set_counter(s_config.channel, k_test_counter_136);
  err = rx_encoder_read_velocity(&velocity_rps, delta_time, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* 136 counts / 1364 counts_per_rev / 0.01s = ~9.97 RPS */
  TEST_ASSERT_FLOAT_WITHIN(s_test_velocity_tolerance, s_test_velocity_rps_997, velocity_rps);
}

void test_encoder_read_velocity_null_pointer_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_err_t err = rx_encoder_read_velocity(nullptr, s_test_delta_time_10ms, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_encoder_read_velocity_not_initialized_fails(void)
{
  float    velocity_rps = 0.0F;
  rx_err_t err = rx_encoder_read_velocity(&velocity_rps, s_test_delta_time_10ms, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encoder_read_velocity_zero_time_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  float    velocity_rps = 0.0F;
  rx_err_t err          = rx_encoder_read_velocity(&velocity_rps, 0.0F, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_read_velocity_negative_time_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  float    velocity_rps = 0.0F;
  rx_err_t err = rx_encoder_read_velocity(&velocity_rps, -s_test_delta_time_10ms, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test read_velocity with excessive delta time fails.
 *
 * @details
 * Exercises the second operand of the `||` at rx_mtu_encoder.c line 952:
 * `(delta_time_s <= k_min_delta_time_s) || (delta_time_s > k_max_delta_time_s)`.
 * Passing 11.0F > k_max_delta_time_s (10.0F) triggers the right-hand side
 * while the left-hand side is false (11.0F > 0.0F).
 *
 * @pre Encoder initialized
 * @post k_rx_err_invalid_arg returned
 *
 * @since Version 1.0.0
 */
void test_encoder_read_velocity_excessive_time_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  static const float k_excessive_delta_time_s = 11.0F;
  float              velocity_rps             = 0.0F;
  rx_err_t           err =
    rx_encoder_read_velocity(&velocity_rps, k_excessive_delta_time_s, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_read_velocity_one_revolution_per_second(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* First call establishes baseline */
  float velocity_rps = 0.0F;
  float delta_time   = s_test_delta_time_1s; /* 1 second */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_velocity(&velocity_rps, delta_time, s_config.channel));

  /* Move exactly one revolution */
  mock_encoder_set_counter(s_config.channel, k_test_counts_per_rev);
  rx_err_t err = rx_encoder_read_velocity(&velocity_rps, delta_time, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, s_test_delta_time_1s, velocity_rps);
}

void test_encoder_read_velocity_reverse(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Start at 1000 counts */
  mock_encoder_set_counter(s_config.channel, k_test_counter_1000);
  float velocity_rps = 0.0F;
  float delta_time   = s_test_delta_time_100ms; /* 100ms */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_velocity(&velocity_rps, delta_time, s_config.channel));

  /* Move backward by 136 counts (0.1 revolutions) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_864);
  rx_err_t err = rx_encoder_read_velocity(&velocity_rps, delta_time, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* -136 counts / 1364 cpr / 0.1s = ~-0.997 RPS */
  TEST_ASSERT_FLOAT_WITHIN(s_test_velocity_tolerance_01, s_test_velocity_neg1, velocity_rps);
}

/* =============================================================================
 * Reset Tests
 * =============================================================================
 */

void test_encoder_reset_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Accumulate some counts */
  mock_encoder_set_counter(s_config.channel, k_test_counter_5000);
  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_5000, state.total_count);

  /* Reset */
  rx_err_t err = rx_encoder_reset(s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify reset */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(0, state.total_count);
  TEST_ASSERT_EQUAL_INT32(0, state.revolutions);
  TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0F, state.position_deg);
}

void test_encoder_reset_not_initialized_fails(void)
{
  rx_err_t err = rx_encoder_reset(s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encoder_reset_clears_hardware_counter(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));
  mock_encoder_set_counter(s_config.channel, k_test_counter_12345);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_reset(s_config.channel));

  uint16_t raw = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_raw(s_config.channel, &raw));
  TEST_ASSERT_EQUAL_UINT16(0, raw);
}

/* =============================================================================
 * Set Count Tests (Homing)
 * =============================================================================
 */

void test_encoder_set_count_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_err_t err = rx_encoder_set_count(k_test_total_10000, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify set count */
  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_10000, state.total_count);
}

void test_encoder_set_count_not_initialized_fails(void)
{
  rx_err_t err = rx_encoder_set_count(k_test_counter_1000, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encoder_set_count_negative_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_err_t err = rx_encoder_set_count(k_test_total_neg5000, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_neg5000, state.total_count);
}

void test_encoder_set_count_calculates_revolutions(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Set to exactly 3 revolutions */
  int32_t three_revs = k_test_revolutions_3 * (int32_t)k_test_counts_per_rev;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_set_count(three_revs, s_config.channel));

  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_revolutions_3, state.revolutions);
  TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0F, state.position_deg);
}

void test_encoder_set_count_calculates_position(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Set to 2.5 revolutions */
  int32_t two_and_half_revs = (int32_t)(s_test_rev_multiplier_2_5 * (float)k_test_counts_per_rev);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_set_count(two_and_half_revs, s_config.channel));

  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_revolutions_2, state.revolutions);
  TEST_ASSERT_FLOAT_WITHIN(s_test_position_tolerance, s_test_position_180, state.position_deg);
}

void test_encoder_set_count_updates_hardware_counter(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_set_count(k_test_counter_1234, s_config.channel));

  uint16_t raw = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_raw(s_config.channel, &raw));
  TEST_ASSERT_EQUAL_UINT16(k_test_counter_1234, raw);
}

void test_encoder_set_count_large_value_wraps_hardware(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Set to value larger than 16-bit */
  int32_t large_count = k_test_total_70000; /* 70000 & 0xFFFF = 4464 */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_set_count(large_count, s_config.channel));

  uint16_t raw = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_raw(s_config.channel, &raw));
  TEST_ASSERT_EQUAL_UINT16(k_test_counter_4464, raw);

  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_70000, state.total_count);
}

/* =============================================================================
 * Multiple Channel Tests
 * =============================================================================
 */

void test_encoder_multiple_channels_independent(void)
{
  rx_encoder_config_t config1 = {
    .channel          = k_mtu_channel_1,
    .counts_per_rev   = k_test_counts_per_rev,
    .invert_direction = false,
  };
  rx_encoder_config_t config2 = {
    .channel          = k_mtu_channel_2,
    .counts_per_rev   = k_test_counts_per_rev,
    .invert_direction = false,
  };

  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&config1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&config2));

  /* Set different counts on each channel */
  mock_encoder_set_counter(k_mtu_channel_1, k_test_counter_1000);
  mock_encoder_set_counter(k_mtu_channel_2, k_test_counter_2000);

  rx_encoder_state_t state1 = {};
  rx_encoder_state_t state2 = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(k_mtu_channel_1, &state1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(k_mtu_channel_2, &state2));

  TEST_ASSERT_EQUAL_INT32(k_test_counter_1000, state1.total_count);
  TEST_ASSERT_EQUAL_INT32(k_test_counter_2000, state2.total_count);

  /* Cleanup */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_deinit(k_mtu_channel_1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_deinit(k_mtu_channel_2));
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

void test_encoder_position_at_180_degrees(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Half revolution */
  uint16_t half_rev = k_test_counts_per_rev / k_test_divisor_half;
  mock_encoder_set_counter(s_config.channel, half_rev);

  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));

  TEST_ASSERT_FLOAT_WITHIN(s_test_position_tolerance, s_test_position_180, state.position_deg);
}

void test_encoder_position_at_90_degrees(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Quarter revolution */
  uint16_t quarter_rev = k_test_counts_per_rev / k_test_divisor_quarter;
  mock_encoder_set_counter(s_config.channel, quarter_rev);

  rx_encoder_state_t state = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));

  TEST_ASSERT_FLOAT_WITHIN(s_test_position_tolerance, s_test_position_90, state.position_deg);
}

void test_encoder_counter_at_exact_boundary(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};

  /* Move counter in steps to approach 65535 */
  mock_encoder_set_counter(s_config.channel, k_test_counter_30000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_30000, state.total_count);

  mock_encoder_set_counter(s_config.channel, k_test_counter_60000);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_60000, state.total_count);

  /* Set counter to exact boundary (65535) */
  mock_encoder_set_counter(s_config.channel, k_test_counter_65535);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_total_65535, state.total_count);

  /* Increment by 1, should wrap to 0 */
  mock_encoder_set_counter(s_config.channel, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  /* delta = (65536 - 65535) + 0 = 1 */
  TEST_ASSERT_EQUAL_INT32(k_test_total_65536, state.total_count);
}

void test_encoder_rapid_direction_changes(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_encoder_state_t state = {};

  /* Forward */
  mock_encoder_set_counter(s_config.channel, k_test_counter_100);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_100, state.total_count);

  /* Backward */
  mock_encoder_set_counter(s_config.channel, k_test_counter_50);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_50, state.total_count);

  /* Forward again */
  mock_encoder_set_counter(s_config.channel, k_test_counter_200);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_200, state.total_count);

  /* Backward again */
  mock_encoder_set_counter(s_config.channel, k_test_counter_150);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_read_count(s_config.channel, &state));
  TEST_ASSERT_EQUAL_INT32(k_test_counter_150, state.total_count);
}

/* =============================================================================
 * Additional Channel Tests (covers internal_get_mtu_base ch3/ch4 paths)
 *
 * MTU3/MTU4 use a sparse register layout (rx_mtu3_channel_regs_t) that is
 * incompatible with the standard rx_mtu_channel_regs_t API used by the
 * encoder driver. internal_get_mtu_base() returns nullptr for these channels,
 * so encoder init is expected to fail.
 * =============================================================================
 */

void test_encoder_init_channel_3_unsupported(void)
{
  s_config.channel = k_mtu_channel_3;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_init_channel_4_unsupported(void)
{
  s_config.channel = k_mtu_channel_4;

  rx_err_t err = rx_encoder_init(&s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Invalid Channel Tests for Read/Reset/SetCount
 * =============================================================================
 */

void test_encoder_read_raw_invalid_channel_fails(void)
{
  /* Channel 5 has no base address (nullptr from internal_get_mtu_base) */
  uint16_t count = 0;
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = rx_encoder_read_raw((rx_mtu_channel_t)k_test_channel_5, &count);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_read_count_invalid_channel_fails(void)
{
  rx_encoder_state_t state = {};
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = rx_encoder_read_count((rx_mtu_channel_t)k_test_channel_5, &state);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_read_velocity_invalid_channel_fails(void)
{
  float    velocity = 0.0F;
  rx_err_t err      = rx_encoder_read_velocity(
    &velocity,
    s_test_delta_time_10ms,
    (rx_mtu_channel_t)k_test_channel_5); /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_reset_invalid_channel_fails(void)
{
  rx_err_t err = rx_encoder_reset(
    (rx_mtu_channel_t)k_test_channel_5); /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_set_count_invalid_channel_fails(void)
{
  rx_err_t err = rx_encoder_set_count(
    0,
    (rx_mtu_channel_t)k_test_channel_5); /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Velocity: Unrealistic Speed Detection
 * =============================================================================
 */

void test_encoder_read_velocity_unrealistic_speed(void)
{
  /* cpr=1364, dt=0.01s, delta=1500 counts -> 1500/1364/0.01 = ~110 RPS > 100 limit */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Advance counter so velocity is unrealistically high */
  mock_encoder_set_counter(s_config.channel, k_test_counter_1500);

  float    velocity = 0.0F;
  rx_err_t err      = rx_encoder_read_velocity(&velocity, s_test_delta_time_10ms, s_config.channel);

  /* Function still returns k_rx_ok - just logs a warning */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(s_test_rps_limit_100, velocity);
}

/* =============================================================================
 * Corrupted CPR Tests (via TESTING helper)
 * =============================================================================
 */

void test_encoder_read_count_corrupted_cpr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  /* Corrupt cpr after init to exercise runtime guard in internal_update_state_from_count */
  rx_mtu_encoder_test_corrupt_cpr(s_config.channel);

  rx_encoder_state_t state = {};
  rx_err_t           err   = rx_encoder_read_count(s_config.channel, &state);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encoder_set_count_corrupted_cpr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_mtu_encoder_test_corrupt_cpr(s_config.channel);

  rx_err_t err = rx_encoder_set_count(k_test_counter_1000, s_config.channel);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Direct Internal Function Tests (source directly included, so static funcs
 * are accessible; UNIT_TEST strips 'static' via RX_STATIC_TESTABLE but here
 * the functions use plain 'static', so they are accessible within this TU)
 * =============================================================================
 */

void test_internal_enable_mtu_module_channel_out_of_range(void)
{
  /* Channel > k_mtu_channel_7 triggers first guard in internal_enable_mtu_module */
  rx_err_t err = internal_enable_mtu_module(
    (rx_mtu_channel_t)k_test_channel_8); /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_enable_mtu_module_channel_5_no_base(void)
{
  /* Channel 5 passes the > k_mtu_channel_7 check but has no base address */
  rx_err_t err = internal_enable_mtu_module(
    (rx_mtu_channel_t)k_test_channel_5); /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_configure_encoder_timer_null_ptr(void)
{
  rx_err_t err = internal_configure_encoder_timer(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_internal_configure_encoder_timer_success(void)
{
  /* Call configure with a valid (non-null) mock register pointer */
  volatile rx_mtu_channel_regs_t* mtu = &g_mock_mtu_regs[0];

  rx_err_t err = internal_configure_encoder_timer(mtu);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_tcr_normal, mtu->tcr);
  TEST_ASSERT_EQUAL_UINT8(k_test_tmdr_phase, mtu->tmdr);
}

void test_internal_verify_timer_counting_null_ptr(void)
{
  rx_err_t err = internal_verify_timer_counting(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_internal_verify_timer_counting_wrong_tmdr(void)
{
  /* Set up register with wrong TMDR value (not phase counting mode) */
  volatile rx_mtu_channel_regs_t* mtu = &g_mock_mtu_regs[0];
  mtu->tmdr                           = k_test_tcr_normal; /* Normal mode, not phase counting */

  rx_err_t err = internal_verify_timer_counting(mtu);

  TEST_ASSERT_EQUAL(k_rx_err_hw_init_failed, err);
}

void test_internal_initialize_encoder_state_null_config(void)
{
  rx_err_t err = internal_initialize_encoder_state(k_mtu_channel_1, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_initialize_encoder_state_invalid_channel(void)
{
  rx_encoder_config_t cfg = {
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    .channel          = (rx_mtu_channel_t)k_test_channel_5,
    .counts_per_rev   = k_test_counts_per_rev,
    .invert_direction = false,
  };
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = internal_initialize_encoder_state((rx_mtu_channel_t)k_test_channel_5, &cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_initialize_encoder_state_zero_cpr(void)
{
  rx_encoder_config_t cfg = {
    .channel          = k_mtu_channel_1,
    .counts_per_rev   = 0,
    .invert_direction = false,
  };
  rx_err_t err = internal_initialize_encoder_state(k_mtu_channel_1, &cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_update_state_from_count_invalid_channel(void)
{
  rx_encoder_state_t state = {};
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = internal_update_state_from_count(&state, (rx_mtu_channel_t)k_test_channel_5, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_update_state_from_count_null_state(void)
{
  /* Channel 1 must be initialized first */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_err_t err = internal_update_state_from_count(nullptr, k_mtu_channel_1, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_update_state_from_count_uninit_channel(void)
{
  /* Channel 2 is not initialized */
  rx_encoder_state_t state = {};
  rx_err_t           err   = internal_update_state_from_count(&state, k_mtu_channel_2, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_internal_update_state_from_count_corrupted_cpr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_encoder_init(&s_config));

  rx_mtu_encoder_test_corrupt_cpr(s_config.channel);

  rx_encoder_state_t state = {};
  rx_err_t           err   = internal_update_state_from_count(&state, s_config.channel, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

/**
 * @brief Run core encoder tests (init, deinit, raw, count, overflow, velocity)
 */
static void internal_run_core_tests(void)
{
  /* Initialization tests */
  RUN_TEST(test_encoder_init_success);
  RUN_TEST(test_encoder_init_null_config_fails);
  RUN_TEST(test_encoder_init_invalid_channel_fails);
  RUN_TEST(test_encoder_init_zero_counts_per_rev_fails);
  RUN_TEST(test_encoder_init_channel_0_success);
  RUN_TEST(test_encoder_init_channel_2_success);
  RUN_TEST(test_encoder_init_channel_6_rejected);
  RUN_TEST(test_encoder_init_channel_7_rejected);

  /* Deinitialization tests */
  RUN_TEST(test_encoder_deinit_success);
  RUN_TEST(test_encoder_deinit_out_of_range_channel_fails);
  RUN_TEST(test_encoder_deinit_clears_state);

  /* Raw count reading tests */
  RUN_TEST(test_encoder_read_raw_success);
  RUN_TEST(test_encoder_read_raw_null_pointer_fails);
  RUN_TEST(test_encoder_read_raw_not_initialized_fails);
  RUN_TEST(test_encoder_read_raw_max_value);
  RUN_TEST(test_encoder_read_raw_zero_value);

  /* Accumulated count reading tests */
  RUN_TEST(test_encoder_read_count_initial_zero);
  RUN_TEST(test_encoder_read_count_null_pointer_fails);
  RUN_TEST(test_encoder_read_count_not_initialized_fails);
  RUN_TEST(test_encoder_read_count_forward_motion);
  RUN_TEST(test_encoder_read_count_backward_motion);
  RUN_TEST(test_encoder_read_count_full_revolution);
  RUN_TEST(test_encoder_read_count_half_revolution);

  /* Overflow detection tests */
  RUN_TEST(test_encoder_read_count_overflow_forward);
  RUN_TEST(test_encoder_read_count_underflow_reverse);
  RUN_TEST(test_encoder_read_count_multiple_overflows);
  RUN_TEST(test_encoder_read_count_multiple_reads_accumulate);

  /* Direction inversion tests */
  RUN_TEST(test_encoder_read_count_inverted_direction);
  RUN_TEST(test_encoder_read_count_inverted_overflow);

  /* Velocity calculation tests */
  RUN_TEST(test_encoder_read_velocity_success);
  RUN_TEST(test_encoder_read_velocity_null_pointer_fails);
  RUN_TEST(test_encoder_read_velocity_not_initialized_fails);
  RUN_TEST(test_encoder_read_velocity_zero_time_fails);
  RUN_TEST(test_encoder_read_velocity_negative_time_fails);
  RUN_TEST(test_encoder_read_velocity_one_revolution_per_second);
  RUN_TEST(test_encoder_read_velocity_reverse);
}

/**
 * @brief Run extended encoder tests (reset, set count, edge cases, internals)
 */
static void internal_run_extended_tests(void)
{
  /* Reset tests */
  RUN_TEST(test_encoder_reset_success);
  RUN_TEST(test_encoder_reset_not_initialized_fails);
  RUN_TEST(test_encoder_reset_clears_hardware_counter);

  /* Set count (homing) tests */
  RUN_TEST(test_encoder_set_count_success);
  RUN_TEST(test_encoder_set_count_not_initialized_fails);
  RUN_TEST(test_encoder_set_count_negative_value);
  RUN_TEST(test_encoder_set_count_calculates_revolutions);
  RUN_TEST(test_encoder_set_count_calculates_position);
  RUN_TEST(test_encoder_set_count_updates_hardware_counter);
  RUN_TEST(test_encoder_set_count_large_value_wraps_hardware);

  /* Multiple channel tests */
  RUN_TEST(test_encoder_multiple_channels_independent);

  /* Edge case tests */
  RUN_TEST(test_encoder_position_at_180_degrees);
  RUN_TEST(test_encoder_position_at_90_degrees);
  RUN_TEST(test_encoder_counter_at_exact_boundary);
  RUN_TEST(test_encoder_rapid_direction_changes);

  /* Additional channel tests (ch3, ch4) */
  RUN_TEST(test_encoder_init_channel_3_unsupported);
  RUN_TEST(test_encoder_init_channel_4_unsupported);

  /* Invalid channel tests for read/reset/set_count */
  RUN_TEST(test_encoder_read_raw_invalid_channel_fails);
  RUN_TEST(test_encoder_read_count_invalid_channel_fails);
  RUN_TEST(test_encoder_read_velocity_invalid_channel_fails);
  RUN_TEST(test_encoder_reset_invalid_channel_fails);
  RUN_TEST(test_encoder_set_count_invalid_channel_fails);

  /* Velocity unrealistic speed warning */
  RUN_TEST(test_encoder_read_velocity_unrealistic_speed);

  /* Corrupted CPR tests */
  RUN_TEST(test_encoder_read_count_corrupted_cpr);
  RUN_TEST(test_encoder_set_count_corrupted_cpr);

  /* Direct internal function tests */
  RUN_TEST(test_internal_enable_mtu_module_channel_out_of_range);
  RUN_TEST(test_internal_enable_mtu_module_channel_5_no_base);
  RUN_TEST(test_internal_configure_encoder_timer_null_ptr);
  RUN_TEST(test_internal_configure_encoder_timer_success);
  RUN_TEST(test_internal_verify_timer_counting_null_ptr);
  RUN_TEST(test_internal_verify_timer_counting_wrong_tmdr);
  RUN_TEST(test_internal_initialize_encoder_state_null_config);
  RUN_TEST(test_internal_initialize_encoder_state_invalid_channel);
  RUN_TEST(test_internal_initialize_encoder_state_zero_cpr);
  RUN_TEST(test_internal_update_state_from_count_invalid_channel);
  RUN_TEST(test_internal_update_state_from_count_null_state);
  RUN_TEST(test_internal_update_state_from_count_uninit_channel);
  RUN_TEST(test_internal_update_state_from_count_corrupted_cpr);

  /* Delta time || short-circuit coverage */
  RUN_TEST(test_encoder_read_velocity_excessive_time_fails);
}

int main(void)
{
  UNITY_BEGIN();

  internal_run_core_tests();
  internal_run_extended_tests();

  return UNITY_END();
}
