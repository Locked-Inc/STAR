/* tests/test_rx_bms_alert.c */

/**
 * @file test_rx_bms_alert.c
 * @brief Unit Tests for BMS Alert Interrupt Driver (IRQ13)
 *
 * @details
 * Self-contained test that reimplements the BMS alert driver logic
 * against mock ICU registers. Follows the same pattern as test_rx_poeg.c.
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* =============================================================================
 * Mock Register Areas
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  ir[256];
  uint8_t           dtcer[256];
  volatile uint8_t  ier[32];
  uint8_t           reserved0[192];
  uint8_t           swintr;
  uint8_t           swint2r;
  uint8_t           reserved1[14];
  uint16_t          fir;
  uint8_t           reserved2[14];
  volatile uint8_t  ipr[256];
  uint8_t           reserved3[256];
  volatile uint8_t  irqcr[16];
  uint8_t           reserved4[16];
  volatile uint8_t  irqflte[2];
  uint8_t           reserved5[6];
  volatile uint16_t irqfltc[2];
} mock_icu_regs_t;

static mock_icu_regs_t s_mock_icu;

/* Mock shared_data state */
static bool     s_estop_active;
static uint8_t  s_estop_reason;
static uint32_t s_estop_trigger_count;

/* Mock log counts */
static uint32_t s_error_log_count;
static uint32_t s_info_log_count;

/* Mock IRQ filter state */
static bool    s_filter_enabled;
static uint8_t s_filter_irq_num;
static uint8_t s_filter_clk;
static bool    s_filter_inject_error;

/* =============================================================================
 * Constants (match rx_bms_alert.c)
 * =============================================================================
 */

typedef enum : uint8_t {
  k_bms_alert_irq_num   = 13,
  k_bms_alert_vector    = 77,
  k_bms_alert_priority  = 13,
  k_icu_ir_clear        = 0,
  k_ier_bit_enable_base = 1,
  k_ier_bits_per_reg    = 8,
  k_irqcr_falling_edge  = 0x04,
} test_bms_alert_constants_t;

enum {
  k_rx_ok                 = 0,
  k_rx_err_null_ptr       = 0x504,
  k_rx_err_invalid_state  = 0x104,
  k_rx_err_hw_init_failed = 0x201,
};

typedef int32_t rx_err_t;

typedef enum : uint8_t {
  k_estop_reason_battery_fault = 4,
} mock_estop_reason_t;

typedef enum : uint8_t {
  k_irq_filter_pclk_32 = 2,
} mock_filter_clk_t;

/* =============================================================================
 * Driver State (reimplemented for testing)
 * =============================================================================
 */

static bool          s_initialized;
static volatile bool s_alert_triggered;

/* =============================================================================
 * Mock Functions
 * =============================================================================
 */

static rx_err_t mock_irq_filter_enable(uint8_t irq_num, uint8_t filter_clk)
{
  if (s_filter_inject_error) {
    return k_rx_err_hw_init_failed;
  }
  s_filter_enabled = true;
  s_filter_irq_num = irq_num;
  s_filter_clk     = filter_clk;
  return k_rx_ok;
}

static rx_err_t mock_irq_filter_disable(uint8_t irq_num)
{
  (void)irq_num;
  s_filter_enabled = false;
  return k_rx_ok;
}

static rx_err_t mock_shared_data_trigger_estop(uint8_t reason)
{
  s_estop_active = true;
  s_estop_reason = reason;
  s_estop_trigger_count++;
  return k_rx_ok;
}

/* =============================================================================
 * Reimplemented Driver Functions (targeting mock registers)
 * =============================================================================
 */

static void test_irq13_bms_alert_isr(void)
{
  s_mock_icu.ir[k_bms_alert_vector] = k_icu_ir_clear;
  s_alert_triggered = true;
  s_error_log_count++;
  (void)mock_shared_data_trigger_estop(k_estop_reason_battery_fault);
}

static rx_err_t test_bms_alert_init(void)
{
  if (s_initialized) {
    s_error_log_count++;
    return k_rx_err_invalid_state;
  }

  s_mock_icu.ir[k_bms_alert_vector] = k_icu_ir_clear;
  s_mock_icu.irqcr[k_bms_alert_irq_num] = k_irqcr_falling_edge;

  rx_err_t err = mock_irq_filter_enable(k_bms_alert_irq_num, k_irq_filter_pclk_32);
  if (err != k_rx_ok) {
    s_error_log_count++;
    return err;
  }

  s_mock_icu.ipr[k_bms_alert_vector] = k_bms_alert_priority;

  const uint8_t ier_idx  = (uint8_t)(k_bms_alert_vector / k_ier_bits_per_reg);
  const uint8_t ier_bit  = (uint8_t)(k_bms_alert_vector % k_ier_bits_per_reg);
  const uint8_t ier_mask = (uint8_t)(k_ier_bit_enable_base << ier_bit);
  s_mock_icu.ier[ier_idx] |= ier_mask;

  s_alert_triggered = false;
  s_initialized     = true;

  s_info_log_count++;
  return k_rx_ok;
}

static rx_err_t test_bms_alert_deinit(void)
{
  if (!s_initialized) {
    s_error_log_count++;
    return k_rx_err_invalid_state;
  }

  const uint8_t ier_idx  = (uint8_t)(k_bms_alert_vector / k_ier_bits_per_reg);
  const uint8_t ier_bit  = (uint8_t)(k_bms_alert_vector % k_ier_bits_per_reg);
  const uint8_t ier_mask = (uint8_t)(k_ier_bit_enable_base << ier_bit);
  s_mock_icu.ier[ier_idx] &= (uint8_t)~ier_mask;

  s_mock_icu.ir[k_bms_alert_vector] = k_icu_ir_clear;
  (void)mock_irq_filter_disable(k_bms_alert_irq_num);

  s_alert_triggered = false;
  s_initialized     = false;

  s_info_log_count++;
  return k_rx_ok;
}

static rx_err_t test_bms_alert_is_triggered(bool* triggered)
{
  if (triggered == NULL) {
    return k_rx_err_null_ptr;
  }
  *triggered = s_alert_triggered;
  return k_rx_ok;
}

static rx_err_t test_bms_alert_clear(void)
{
  s_alert_triggered = false;
  return k_rx_ok;
}

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  memset(&s_mock_icu, 0, sizeof(s_mock_icu));
  s_estop_active        = false;
  s_estop_reason        = 0;
  s_estop_trigger_count = 0;
  s_error_log_count     = 0;
  s_info_log_count      = 0;
  s_filter_enabled      = false;
  s_filter_irq_num      = 0;
  s_filter_clk          = 0;
  s_filter_inject_error = false;
  s_initialized         = false;
  s_alert_triggered     = false;
}

void tearDown(void) {}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

void test_init_configures_irqcr_falling_edge(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_EQUAL(k_irqcr_falling_edge, s_mock_icu.irqcr[13]);
}

void test_init_enables_digital_filter(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_TRUE(s_filter_enabled);
  TEST_ASSERT_EQUAL(13, s_filter_irq_num);
  TEST_ASSERT_EQUAL(k_irq_filter_pclk_32, s_filter_clk);
}

void test_init_sets_priority(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_EQUAL(k_bms_alert_priority, s_mock_icu.ipr[77]);
}

void test_init_enables_ier_bit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  /* Vector 77: ier[9] bit 5 = 0x20 */
  TEST_ASSERT_BITS(0x20, 0x20, s_mock_icu.ier[9]);
}

void test_init_clears_ir_flag(void)
{
  s_mock_icu.ir[77] = 1;
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_EQUAL(0, s_mock_icu.ir[77]);
}

void test_init_double_init_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, test_bms_alert_init());
}

void test_init_filter_error_propagated(void)
{
  s_filter_inject_error = true;
  TEST_ASSERT_EQUAL(k_rx_err_hw_init_failed, test_bms_alert_init());
}

/* =============================================================================
 * ISR Tests
 * =============================================================================
 */

void test_isr_clears_ir_flag(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  s_mock_icu.ir[77] = 1;
  test_irq13_bms_alert_isr();
  TEST_ASSERT_EQUAL(0, s_mock_icu.ir[77]);
}

void test_isr_sets_alert_flag(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  test_irq13_bms_alert_isr();

  bool triggered = false;
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_is_triggered(&triggered));
  TEST_ASSERT_TRUE(triggered);
}

void test_isr_triggers_estop(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  test_irq13_bms_alert_isr();

  TEST_ASSERT_TRUE(s_estop_active);
  TEST_ASSERT_EQUAL(k_estop_reason_battery_fault, s_estop_reason);
  TEST_ASSERT_EQUAL(1, s_estop_trigger_count);
}

void test_isr_repeated_increments_estop_count(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  test_irq13_bms_alert_isr();
  test_irq13_bms_alert_isr();
  test_irq13_bms_alert_isr();
  TEST_ASSERT_EQUAL(3, s_estop_trigger_count);
}

/* =============================================================================
 * Flag Management Tests
 * =============================================================================
 */

void test_not_triggered_initially(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  bool triggered = true;
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_is_triggered(&triggered));
  TEST_ASSERT_FALSE(triggered);
}

void test_clear_resets_flag(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  test_irq13_bms_alert_isr();

  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_clear());

  bool triggered = true;
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_is_triggered(&triggered));
  TEST_ASSERT_FALSE(triggered);
}

void test_is_triggered_null_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, test_bms_alert_is_triggered(NULL));
}

/* =============================================================================
 * Deinit Tests
 * =============================================================================
 */

void test_deinit_clears_ier_bit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_BITS(0x20, 0x20, s_mock_icu.ier[9]);

  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_deinit());
  TEST_ASSERT_BITS(0x20, 0x00, s_mock_icu.ier[9]);
}

void test_deinit_disables_filter(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_TRUE(s_filter_enabled);

  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_deinit());
  TEST_ASSERT_FALSE(s_filter_enabled);
}

void test_deinit_without_init_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, test_bms_alert_deinit());
}

void test_reinit_after_deinit_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_deinit());
  TEST_ASSERT_EQUAL(k_rx_ok, test_bms_alert_init());
}

/* =============================================================================
 * IER Bit Calculation Verification
 * =============================================================================
 */

void test_ier_calculation_vector_77(void)
{
  /* Verify IER index and bit for vector 77 */
  const uint8_t ier_idx = 77 / 8; /* = 9 */
  const uint8_t ier_bit = 77 % 8; /* = 5 */
  TEST_ASSERT_EQUAL(9, ier_idx);
  TEST_ASSERT_EQUAL(5, ier_bit);

  /* Verify mask = 0x20 = 1 << 5 */
  const uint8_t mask = (uint8_t)(1 << ier_bit);
  TEST_ASSERT_EQUAL(0x20, mask);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Init tests */
  RUN_TEST(test_init_configures_irqcr_falling_edge);
  RUN_TEST(test_init_enables_digital_filter);
  RUN_TEST(test_init_sets_priority);
  RUN_TEST(test_init_enables_ier_bit);
  RUN_TEST(test_init_clears_ir_flag);
  RUN_TEST(test_init_double_init_returns_error);
  RUN_TEST(test_init_filter_error_propagated);

  /* ISR tests */
  RUN_TEST(test_isr_clears_ir_flag);
  RUN_TEST(test_isr_sets_alert_flag);
  RUN_TEST(test_isr_triggers_estop);
  RUN_TEST(test_isr_repeated_increments_estop_count);

  /* Flag management tests */
  RUN_TEST(test_not_triggered_initially);
  RUN_TEST(test_clear_resets_flag);
  RUN_TEST(test_is_triggered_null_returns_error);

  /* Deinit tests */
  RUN_TEST(test_deinit_clears_ier_bit);
  RUN_TEST(test_deinit_disables_filter);
  RUN_TEST(test_deinit_without_init_returns_error);
  RUN_TEST(test_reinit_after_deinit_succeeds);

  /* Verification tests */
  RUN_TEST(test_ier_calculation_vector_77);

  return UNITY_END();
}
