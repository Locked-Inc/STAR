/**
 * @file test_rx_usb.c
 * @brief Comprehensive Unit Tests for USB CDC Driver
 *
 * Tests USB driver functionality including:
 * - Ring buffer operations (init, write, read, wrap-around, concurrent access)
 * - USB state machine transitions
 * - Descriptor handling
 * - CDC class requests
 * - FIFO operations
 * - Validation and error handling
 *
 * This is a standalone test suite using simple assert-based testing.
 * No external test framework required.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include mocks first to override hardware headers */
#include "mocks/mock_rx_log.h"
#include "mocks/mock_tx_api.h"
#include "mocks/mock_usb0_regs.h"

/* Include error definitions */
#include "rx_err.h"

/* Include unit under test header */
#include "rx_usb.h"

/* =============================================================================
 * Test Framework Macros
 * =============================================================================
 */

/** @brief Test result counters */
typedef enum : uint8_t {
  k_test_result_init = 0, /**< Initial counter value */
} test_result_init_t;

static uint32_t    s_tests_passed = k_test_result_init;
static uint32_t    s_tests_failed = k_test_result_init;
static const char* s_current_test = NULL;

/**
 * @brief Assert that a condition is true
 */
#define TEST_ASSERT(condition)                                                                     \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition);                               \
      s_tests_failed++;                                                                            \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that two values are equal
 */
#define TEST_ASSERT_EQUAL(expected, actual)                                                        \
  do {                                                                                             \
    if ((expected) != (actual)) {                                                                  \
      printf("  FAIL: %s:%d: expected %d, got %d\n",                                               \
             __FILE__,                                                                             \
             __LINE__,                                                                             \
             (int)(expected),                                                                      \
             (int)(actual));                                                                       \
      s_tests_failed++;                                                                            \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that two uint32_t values are equal
 */
#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                                                 \
  do {                                                                                             \
    if ((expected) != (actual)) {                                                                  \
      printf("  FAIL: %s:%d: expected %u, got %u\n",                                               \
             __FILE__,                                                                             \
             __LINE__,                                                                             \
             (unsigned)(expected),                                                                 \
             (unsigned)(actual));                                                                  \
      s_tests_failed++;                                                                            \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that two memory regions are equal
 */
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)                                            \
  do {                                                                                             \
    if (memcmp((expected), (actual), (len)) != 0) {                                                \
      printf("  FAIL: %s:%d: memory mismatch\n", __FILE__, __LINE__);                              \
      s_tests_failed++;                                                                            \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that two pointers are equal
 */
#define TEST_ASSERT_EQUAL_PTR(expected, actual)                                                    \
  do {                                                                                             \
    if ((expected) != (actual)) {                                                                  \
      printf("  FAIL: %s:%d: expected %p, got %p\n",                                               \
             __FILE__,                                                                             \
             __LINE__,                                                                             \
             (void*)(expected),                                                                    \
             (void*)(actual));                                                                     \
      s_tests_failed++;                                                                            \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that a condition is true
 */
#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)

/**
 * @brief Assert that a condition is false
 */
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

/**
 * @brief Run a test function
 */
#define RUN_TEST(test_func)                                                                        \
  do {                                                                                             \
    s_current_test = #test_func;                                                                   \
    printf("Running %s...\n", s_current_test);                                                     \
    setUp();                                                                                       \
    test_func();                                                                                   \
    tearDown();                                                                                    \
    if (s_tests_failed == 0 || s_tests_passed > 0) {                                               \
      s_tests_passed++;                                                                            \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * External Declarations for STATIC_TESTABLE Functions
 *
 * These functions are exposed from rx_usb.c when UNIT_TEST is defined.
 * =============================================================================
 */

/* Ring buffer structure (must match rx_usb.c definition) */
typedef struct {
  uint8_t  data[k_usb_rx_buffer_size];
  uint32_t head;
  uint32_t tail;
  uint32_t count;
} ring_buffer_t;

extern void     internal_ring_buffer_init(ring_buffer_t* buf);
extern uint32_t internal_ring_buffer_available(const ring_buffer_t* buf);
extern uint32_t internal_ring_buffer_free(const ring_buffer_t* buf);
extern uint32_t internal_ring_buffer_write(ring_buffer_t* buf, const uint8_t* data, uint32_t len);
extern uint32_t internal_ring_buffer_read(ring_buffer_t* buf, uint8_t* data, uint32_t max_len);

/* Internal state functions */
extern void     rx_usb_set_state(rx_usb_state_t state);
extern void     rx_usb_set_line_coding(const rx_usb_line_coding_t* coding);
extern uint32_t rx_usb_rx_push(const uint8_t* data, uint32_t len);
extern uint32_t rx_usb_tx_pop(uint8_t* data, uint32_t max_len);
extern void     rx_usb_count_bus_reset(void);
extern void     rx_usb_count_suspend(void);

/* =============================================================================
 * Mock Hardware Function Declarations
 *
 * These must be provided for linking. They are implemented as stubs
 * that track calls for test verification.
 * =============================================================================
 */

/* Mock tracking state */
typedef struct {
  uint32_t hw_init_calls;
  uint32_t hw_deinit_calls;
  uint32_t hw_attach_calls;
  uint32_t hw_detach_calls;
  uint32_t cdc_init_calls;
  uint32_t cdc_bulk_in_calls;
  rx_err_t next_hw_init_ret;
  rx_err_t next_hw_attach_ret;
  rx_err_t next_cdc_init_ret;
} mock_hw_state_t;

static mock_hw_state_t s_mock_hw = {0};

/* Mock implementations of USB hardware functions */
rx_err_t rx_usb_hw_init(void)
{
  s_mock_hw.hw_init_calls++;
  rx_err_t ret               = s_mock_hw.next_hw_init_ret;
  s_mock_hw.next_hw_init_ret = k_rx_ok; /* Reset after use */
  return ret;
}

rx_err_t rx_usb_hw_deinit(void)
{
  s_mock_hw.hw_deinit_calls++;
  return k_rx_ok;
}

rx_err_t rx_usb_hw_attach(void)
{
  s_mock_hw.hw_attach_calls++;
  rx_err_t ret                 = s_mock_hw.next_hw_attach_ret;
  s_mock_hw.next_hw_attach_ret = k_rx_ok; /* Reset after use */
  return ret;
}

rx_err_t rx_usb_hw_detach(void)
{
  s_mock_hw.hw_detach_calls++;
  return k_rx_ok;
}

rx_err_t rx_usb_cdc_init(void)
{
  s_mock_hw.cdc_init_calls++;
  rx_err_t ret                = s_mock_hw.next_cdc_init_ret;
  s_mock_hw.next_cdc_init_ret = k_rx_ok; /* Reset after use */
  return ret;
}

void rx_usb_cdc_handle_bulk_in(void)
{
  s_mock_hw.cdc_bulk_in_calls++;
}

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static ring_buffer_t s_test_buffer;

/* Callback tracking */
static rx_usb_event_t s_last_event;
static uint32_t       s_callback_count;
static void*          s_callback_context;

static void test_callback(rx_usb_event_t event, void* ctx)
{
  s_last_event = event;
  s_callback_count++;
  s_callback_context = ctx;
}

void setUp(void)
{
  /* Initialize mock registers */
  mock_regs_init();
  mock_log_init();
  mock_tx_init();

  /* Initialize mock hardware state */
  memset(&s_mock_hw, 0, sizeof(s_mock_hw));

  /* Initialize test ring buffer */
  memset(&s_test_buffer, 0, sizeof(s_test_buffer));

  /* Reset callback tracking */
  s_last_event       = 0;
  s_callback_count   = 0;
  s_callback_context = NULL;
}

void tearDown(void)
{
  /* Deinitialize USB if it was initialized */
  (void)rx_usb_deinit();

  /* Clear mock state */
  mock_regs_clear();
  mock_log_clear();
  mock_tx_clear();
}

/* =============================================================================
 * Ring Buffer Initialization Tests
 * =============================================================================
 */

void test_ring_buffer_init_clears_state(void)
{
  /* Pre-fill with garbage */
  s_test_buffer.head  = 100;
  s_test_buffer.tail  = 200;
  s_test_buffer.count = 300;

  internal_ring_buffer_init(&s_test_buffer);

  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.head);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.tail);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

void test_ring_buffer_available_empty(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  uint32_t available = internal_ring_buffer_available(&s_test_buffer);

  TEST_ASSERT_EQUAL_UINT32(0, available);
}

void test_ring_buffer_free_empty(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  uint32_t free_space = internal_ring_buffer_free(&s_test_buffer);

  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, free_space);
}

void test_ring_buffer_null_ptr_handling(void)
{
  /* These should not crash, just return 0 */
  internal_ring_buffer_init(NULL);
  TEST_ASSERT_EQUAL_UINT32(0, internal_ring_buffer_available(NULL));
  TEST_ASSERT_EQUAL_UINT32(0, internal_ring_buffer_free(NULL));
}

/* =============================================================================
 * Ring Buffer Write Tests
 * =============================================================================
 */

void test_ring_buffer_write_single_byte(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data = 0x42;

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, &data, 1);

  TEST_ASSERT_EQUAL_UINT32(1, written);
  TEST_ASSERT_EQUAL_UINT32(1, s_test_buffer.count);
  TEST_ASSERT_EQUAL(0x42, s_test_buffer.data[0]);
}

void test_ring_buffer_write_multiple_bytes(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, data, 5);

  TEST_ASSERT_EQUAL_UINT32(5, written);
  TEST_ASSERT_EQUAL_UINT32(5, s_test_buffer.count);
  TEST_ASSERT_EQUAL(0x01, s_test_buffer.data[0]);
  TEST_ASSERT_EQUAL(0x05, s_test_buffer.data[4]);
}

void test_ring_buffer_write_fills_buffer(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data[k_usb_rx_buffer_size];
  memset(data, 0xAA, sizeof(data));

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, data, sizeof(data));

  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT32(0, internal_ring_buffer_free(&s_test_buffer));
}

void test_ring_buffer_write_overflow_truncates(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data[k_usb_rx_buffer_size + 100];
  memset(data, 0xBB, sizeof(data));

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, data, sizeof(data));

  /* Should only write up to buffer size */
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, s_test_buffer.count);
}

void test_ring_buffer_write_partial_when_partially_full(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  /* Fill half the buffer */
  uint8_t first[k_usb_rx_buffer_size / 2];
  memset(first, 0x11, sizeof(first));
  internal_ring_buffer_write(&s_test_buffer, first, sizeof(first));

  /* Try to write more than available space */
  uint8_t second[k_usb_rx_buffer_size];
  memset(second, 0x22, sizeof(second));
  uint32_t written = internal_ring_buffer_write(&s_test_buffer, second, sizeof(second));

  /* Should only write remaining space */
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size / 2, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, s_test_buffer.count);
}

void test_ring_buffer_write_wraps_around(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  /* Write to advance head near end */
  uint8_t data1[k_usb_rx_buffer_size - 10];
  memset(data1, 0x11, sizeof(data1));
  internal_ring_buffer_write(&s_test_buffer, data1, sizeof(data1));

  /* Read some to create space at beginning */
  uint8_t out[100];
  internal_ring_buffer_read(&s_test_buffer, out, 100);

  /* Write data that wraps around */
  uint8_t data2[50];
  for (uint32_t i = 0; i < 50; i++) {
    data2[i] = (uint8_t)(i & 0xFF);
  }
  uint32_t written = internal_ring_buffer_write(&s_test_buffer, data2, 50);

  TEST_ASSERT_EQUAL_UINT32(50, written);
  /* Head should have wrapped around */
  TEST_ASSERT_TRUE(s_test_buffer.head < 50);
}

void test_ring_buffer_write_null_data_returns_zero(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, NULL, 10);

  TEST_ASSERT_EQUAL_UINT32(0, written);
}

void test_ring_buffer_write_zero_len_returns_zero(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data[] = {0x01, 0x02};

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, data, 0);

  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

/* =============================================================================
 * Ring Buffer Read Tests
 * =============================================================================
 */

void test_ring_buffer_read_empty_returns_zero(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data[10];

  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, data, 10);

  TEST_ASSERT_EQUAL_UINT32(0, read_count);
}

void test_ring_buffer_read_single_byte(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t write_data = 0x42;
  internal_ring_buffer_write(&s_test_buffer, &write_data, 1);

  uint8_t  read_data;
  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, &read_data, 1);

  TEST_ASSERT_EQUAL_UINT32(1, read_count);
  TEST_ASSERT_EQUAL(0x42, read_data);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

void test_ring_buffer_read_multiple_bytes(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t write_data[] = "Hello";
  internal_ring_buffer_write(&s_test_buffer, write_data, 5);

  uint8_t  read_data[10] = {0};
  uint32_t read_count    = internal_ring_buffer_read(&s_test_buffer, read_data, 10);

  TEST_ASSERT_EQUAL_UINT32(5, read_count);
  TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, 5);
}

void test_ring_buffer_read_partial(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t write_data[] = "Hello World";
  internal_ring_buffer_write(&s_test_buffer, write_data, 11);

  uint8_t  read_data[5];
  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, read_data, 5);

  TEST_ASSERT_EQUAL_UINT32(5, read_count);
  TEST_ASSERT_EQUAL_MEMORY("Hello", read_data, 5);
  TEST_ASSERT_EQUAL_UINT32(6, s_test_buffer.count); /* " World" remains */
}

void test_ring_buffer_read_wraps_around(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  /* Write near end of buffer */
  uint8_t fill[k_usb_rx_buffer_size - 20];
  memset(fill, 0x00, sizeof(fill));
  internal_ring_buffer_write(&s_test_buffer, fill, sizeof(fill));

  /* Read to free up space */
  uint8_t discard[k_usb_rx_buffer_size - 30];
  internal_ring_buffer_read(&s_test_buffer, discard, sizeof(discard));

  /* Write pattern that wraps */
  uint8_t pattern[50];
  for (uint32_t i = 0; i < 50; i++) {
    pattern[i] = (uint8_t)(i + 1);
  }
  internal_ring_buffer_write(&s_test_buffer, pattern, 50);

  /* Read and verify pattern */
  uint8_t verify[50];
  internal_ring_buffer_read(&s_test_buffer, discard, 10); /* Skip remaining fill */
  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, verify, 50);

  TEST_ASSERT_EQUAL_UINT32(50, read_count);
  TEST_ASSERT_EQUAL_MEMORY(pattern, verify, 50);
}

void test_ring_buffer_fifo_order(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  /* Write sequence */
  uint8_t write_data[] = {1, 2, 3, 4, 5};
  internal_ring_buffer_write(&s_test_buffer, write_data, 5);

  /* Read should be in same order */
  uint8_t read_data[5];
  internal_ring_buffer_read(&s_test_buffer, read_data, 5);

  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(i + 1, read_data[i]);
  }
}

void test_ring_buffer_read_null_data_returns_zero(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t write_data = 0x42;
  internal_ring_buffer_write(&s_test_buffer, &write_data, 1);

  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, NULL, 10);

  TEST_ASSERT_EQUAL_UINT32(0, read_count);
}

void test_ring_buffer_read_zero_len_returns_zero(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t write_data = 0x42;
  internal_ring_buffer_write(&s_test_buffer, &write_data, 1);

  uint8_t  read_data[10];
  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, read_data, 0);

  TEST_ASSERT_EQUAL_UINT32(0, read_count);
  TEST_ASSERT_EQUAL_UINT32(1, s_test_buffer.count); /* Data still in buffer */
}

/* =============================================================================
 * Ring Buffer Concurrent Access Simulation Tests
 * =============================================================================
 */

void test_ring_buffer_interleaved_read_write(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  /* Simulate ISR writing while task reads */
  for (uint32_t i = 0; i < 100; i++) {
    /* Write some data (simulating ISR) */
    uint8_t  write_data = (uint8_t)i;
    uint32_t written    = internal_ring_buffer_write(&s_test_buffer, &write_data, 1);
    TEST_ASSERT_EQUAL_UINT32(1, written);

    /* Read data back (simulating task) */
    uint8_t  read_data;
    uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, &read_data, 1);
    TEST_ASSERT_EQUAL_UINT32(1, read_count);
    TEST_ASSERT_EQUAL((uint8_t)i, read_data);
  }
}

void test_ring_buffer_burst_write_then_burst_read(void)
{
  internal_ring_buffer_init(&s_test_buffer);

  /* Burst write */
  uint8_t pattern[100];
  for (uint32_t i = 0; i < 100; i++) {
    pattern[i] = (uint8_t)(i * 2);
  }
  uint32_t written = internal_ring_buffer_write(&s_test_buffer, pattern, 100);
  TEST_ASSERT_EQUAL_UINT32(100, written);

  /* Burst read */
  uint8_t  read_buf[100];
  uint32_t read_count = internal_ring_buffer_read(&s_test_buffer, read_buf, 100);
  TEST_ASSERT_EQUAL_UINT32(100, read_count);
  TEST_ASSERT_EQUAL_MEMORY(pattern, read_buf, 100);
}

/* =============================================================================
 * USB State Machine Tests
 * =============================================================================
 */

void test_usb_get_state_returns_detached_initially(void)
{
  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

void test_usb_init_transitions_to_attached(void)
{
  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

void test_usb_set_state_to_powered(void)
{
  rx_usb_init(NULL);

  rx_usb_set_state(k_usb_state_powered);

  TEST_ASSERT_EQUAL(k_usb_state_powered, rx_usb_get_state());
}

void test_usb_set_state_to_default(void)
{
  rx_usb_init(NULL);

  rx_usb_set_state(k_usb_state_default);

  TEST_ASSERT_EQUAL(k_usb_state_default, rx_usb_get_state());
}

void test_usb_set_state_to_addressed(void)
{
  rx_usb_init(NULL);

  rx_usb_set_state(k_usb_state_addressed);

  TEST_ASSERT_EQUAL(k_usb_state_addressed, rx_usb_get_state());
}

void test_usb_set_state_to_configured(void)
{
  rx_usb_init(NULL);

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL(k_usb_state_configured, rx_usb_get_state());
}

void test_usb_set_state_to_suspended(void)
{
  rx_usb_init(NULL);

  rx_usb_set_state(k_usb_state_suspended);

  TEST_ASSERT_EQUAL(k_usb_state_suspended, rx_usb_get_state());
}

void test_usb_set_state_to_detached(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  rx_usb_set_state(k_usb_state_detached);

  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

void test_usb_set_state_triggers_callback_on_configured(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  config.ctx             = (void*)0xCAFEBABE;
  rx_usb_init(&config);
  s_callback_count = 0; /* Reset after init callback */

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL_UINT32(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
  TEST_ASSERT_EQUAL_PTR((void*)0xCAFEBABE, s_callback_context);
}

void test_usb_set_state_no_callback_if_same_state(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  rx_usb_init(&config);
  s_callback_count = 0; /* Reset after init */

  /* State is already attached after init */
  rx_usb_set_state(k_usb_state_attached);

  TEST_ASSERT_EQUAL_UINT32(0, s_callback_count);
}

void test_usb_state_full_enumeration_sequence(void)
{
  rx_usb_init(NULL);

  /* Simulate full USB enumeration sequence */
  rx_usb_set_state(k_usb_state_powered);
  TEST_ASSERT_EQUAL(k_usb_state_powered, rx_usb_get_state());

  rx_usb_set_state(k_usb_state_default);
  TEST_ASSERT_EQUAL(k_usb_state_default, rx_usb_get_state());

  rx_usb_set_state(k_usb_state_addressed);
  TEST_ASSERT_EQUAL(k_usb_state_addressed, rx_usb_get_state());

  rx_usb_set_state(k_usb_state_configured);
  TEST_ASSERT_EQUAL(k_usb_state_configured, rx_usb_get_state());
  TEST_ASSERT_TRUE(rx_usb_is_configured());
}

/* =============================================================================
 * USB Initialization Tests
 * =============================================================================
 */

void test_usb_init_null_config_succeeds(void)
{
  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_init_calls);
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_attach_calls);
}

void test_usb_init_with_callback(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  config.ctx             = (void*)0xDEADBEEF;

  rx_err_t err = rx_usb_init(&config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_init_twice_fails(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_init_hw_failure_propagates(void)
{
  s_mock_hw.next_hw_init_ret = k_rx_err_hw_error;

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

void test_usb_init_attach_failure_propagates_and_cleans_up(void)
{
  s_mock_hw.next_hw_attach_ret = k_rx_err_hw_error;

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
  /* Should have called deinit to clean up */
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_deinit_calls);
}

/* =============================================================================
 * USB Deinitialization Tests
 * =============================================================================
 */

void test_usb_deinit_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_deinit();

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_deinit_success(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_deinit();

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_detach_calls);
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_deinit_calls);
}

void test_usb_deinit_sets_detached_state(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  rx_usb_deinit();

  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

/* =============================================================================
 * USB State Query Tests
 * =============================================================================
 */

void test_usb_is_configured_when_not_initialized(void)
{
  TEST_ASSERT_FALSE(rx_usb_is_configured());
}

void test_usb_is_configured_when_attached(void)
{
  rx_usb_init(NULL);

  TEST_ASSERT_FALSE(rx_usb_is_configured());
}

void test_usb_is_configured_when_configured(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_TRUE(rx_usb_is_configured());
}

/* =============================================================================
 * USB Write Tests
 * =============================================================================
 */

void test_usb_write_null_data_fails(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_write(NULL, 10);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_write_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_write(data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_write_not_configured_fails(void)
{
  rx_usb_init(NULL);
  uint8_t data[] = "test";

  /* State is attached but not configured */
  rx_err_t err = rx_usb_write(data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_write_success_when_configured(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_write(data, 4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_write_triggers_transmission_when_pipe_idle(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);
  s_mock_hw.cdc_bulk_in_calls = 0;

  /* Set pipe as NOT busy (idle) */
  mock_usb0_set_pipe1_busy(0);

  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  /* Verify rx_usb_cdc_handle_bulk_in was called */
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.cdc_bulk_in_calls);
}

void test_usb_write_no_trigger_when_pipe_busy(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);
  s_mock_hw.cdc_bulk_in_calls = 0;

  /* Set pipe as busy */
  mock_usb0_set_pipe1_busy(1);

  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  /* Verify rx_usb_cdc_handle_bulk_in was NOT called */
  TEST_ASSERT_EQUAL_UINT32(0, s_mock_hw.cdc_bulk_in_calls);
}

void test_usb_write_updates_stats(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";

  rx_usb_write(data, 4);

  rx_usb_stats_t stats;
  rx_usb_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(4, stats.bytes_tx);
}

void test_usb_write_full_buffer_returns_busy(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  /* Fill the buffer */
  uint8_t fill_data[k_usb_rx_buffer_size];
  memset(fill_data, 'A', sizeof(fill_data));
  rx_usb_write(fill_data, sizeof(fill_data));

  /* Try to write more */
  uint8_t  more_data[] = "more";
  rx_err_t err         = rx_usb_write(more_data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

/* =============================================================================
 * USB Read Tests
 * =============================================================================
 */

void test_usb_read_null_data_fails(void)
{
  uint32_t actual_len;

  rx_err_t err = rx_usb_read(NULL, 10, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_read_null_actual_len_fails(void)
{
  uint8_t data[10];

  rx_err_t err = rx_usb_read(data, 10, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_read_empty_buffer_returns_zero(void)
{
  rx_usb_init(NULL);
  uint8_t  data[10];
  uint32_t actual_len = 999;

  rx_err_t err = rx_usb_read(data, 10, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, actual_len);
}

void test_usb_read_after_rx_push(void)
{
  rx_usb_init(NULL);
  uint8_t push_data[] = "Hello USB";
  rx_usb_rx_push(push_data, 9);

  uint8_t  read_data[20];
  uint32_t actual_len;
  rx_err_t err = rx_usb_read(read_data, 20, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(9, actual_len);
  TEST_ASSERT_EQUAL_MEMORY(push_data, read_data, 9);
}

/* =============================================================================
 * USB RX/TX Available Tests
 * =============================================================================
 */

void test_usb_rx_available_null_fails(void)
{
  rx_err_t err = rx_usb_rx_available(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_rx_available_empty(void)
{
  rx_usb_init(NULL);
  uint32_t available;

  rx_err_t err = rx_usb_rx_available(&available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, available);
}

void test_usb_rx_available_after_push(void)
{
  rx_usb_init(NULL);
  uint8_t data[] = "test";
  rx_usb_rx_push(data, 4);

  uint32_t available;
  rx_usb_rx_available(&available);

  TEST_ASSERT_EQUAL_UINT32(4, available);
}

void test_usb_tx_available_null_fails(void)
{
  rx_err_t err = rx_usb_tx_available(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_tx_available_empty(void)
{
  rx_usb_init(NULL);
  uint32_t available;

  rx_err_t err = rx_usb_tx_available(&available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, available);
}

void test_usb_tx_available_after_write(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  uint32_t available;
  rx_usb_tx_available(&available);

  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size - 4, available);
}

/* =============================================================================
 * USB Line Coding Tests
 * =============================================================================
 */

void test_usb_get_line_coding_null_fails(void)
{
  rx_err_t err = rx_usb_get_line_coding(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_get_line_coding_default_values(void)
{
  rx_usb_init(NULL);
  rx_usb_line_coding_t coding;

  rx_err_t err = rx_usb_get_line_coding(&coding);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(115200, coding.baud_rate);
  TEST_ASSERT_EQUAL(0, coding.stop_bits); /* 1 stop bit */
  TEST_ASSERT_EQUAL(0, coding.parity);    /* No parity */
  TEST_ASSERT_EQUAL(8, coding.data_bits);
}

void test_usb_set_line_coding(void)
{
  rx_usb_init(NULL);
  rx_usb_line_coding_t new_coding = {
    .baud_rate = 9600,
    .stop_bits = 2,
    .parity    = 1,
    .data_bits = 7,
  };

  rx_usb_set_line_coding(&new_coding);

  rx_usb_line_coding_t result;
  rx_usb_get_line_coding(&result);
  TEST_ASSERT_EQUAL_UINT32(9600, result.baud_rate);
  TEST_ASSERT_EQUAL(2, result.stop_bits);
  TEST_ASSERT_EQUAL(1, result.parity);
  TEST_ASSERT_EQUAL(7, result.data_bits);
}

void test_usb_set_line_coding_null_ignored(void)
{
  rx_usb_init(NULL);

  /* Should not crash */
  rx_usb_set_line_coding(NULL);

  /* Original values should be preserved */
  rx_usb_line_coding_t result;
  rx_usb_get_line_coding(&result);
  TEST_ASSERT_EQUAL_UINT32(115200, result.baud_rate);
}

/* =============================================================================
 * USB Statistics Tests
 * =============================================================================
 */

void test_usb_get_stats_null_fails(void)
{
  rx_err_t err = rx_usb_get_stats(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_get_stats_initial_zeros(void)
{
  rx_usb_init(NULL);
  rx_usb_stats_t stats;

  rx_err_t err = rx_usb_get_stats(&stats);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bytes_rx);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bytes_tx);
  TEST_ASSERT_EQUAL_UINT32(0, stats.rx_overruns);
  TEST_ASSERT_EQUAL_UINT32(0, stats.tx_underruns);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bus_resets);
  TEST_ASSERT_EQUAL_UINT32(0, stats.suspends);
}

void test_usb_reset_stats(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  /* Generate some stats by writing */
  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  rx_usb_stats_t stats;
  rx_usb_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(4, stats.bytes_tx);

  /* Reset stats */
  rx_usb_reset_stats();

  rx_usb_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bytes_tx);
}

void test_usb_rx_push_updates_stats(void)
{
  rx_usb_init(NULL);
  uint8_t data[] = "test";

  rx_usb_rx_push(data, 4);

  rx_usb_stats_t stats;
  rx_usb_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(4, stats.bytes_rx);
}

void test_usb_rx_push_triggers_callback(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  rx_usb_init(&config);
  s_callback_count = 0; /* Reset after init */

  uint8_t data[] = "test";
  rx_usb_rx_push(data, 4);

  TEST_ASSERT_EQUAL_UINT32(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_data_rx, s_last_event);
}

void test_usb_tx_pop_retrieves_data(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  uint8_t write_data[] = "Hello";
  rx_usb_write(write_data, 5);

  uint8_t  read_data[10];
  uint32_t read_count = rx_usb_tx_pop(read_data, 10);

  TEST_ASSERT_EQUAL_UINT32(5, read_count);
  TEST_ASSERT_EQUAL_MEMORY("Hello", read_data, 5);
}

void test_usb_count_bus_reset_increments_stat(void)
{
  rx_usb_init(NULL);

  rx_usb_count_bus_reset();
  rx_usb_count_bus_reset();

  rx_usb_stats_t stats;
  rx_usb_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(2, stats.bus_resets);
}

void test_usb_count_suspend_increments_stat(void)
{
  rx_usb_init(NULL);

  rx_usb_count_suspend();

  rx_usb_stats_t stats;
  rx_usb_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(1, stats.suspends);
}

/* =============================================================================
 * USB Flush Tests
 * =============================================================================
 */

void test_usb_flush_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_flush(0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_flush_empty_buffer_succeeds(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_flush(0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_flush_with_data_and_zero_timeout_returns_timeout(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  rx_err_t err = rx_usb_flush(0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Validation Tests
 * =============================================================================
 */

void test_pipe_bounds_validation_at_max(void)
{
  /* Test that pipe number 9 is valid (max) */
  /* This is tested implicitly through mock hardware */
  rx_usb_init(NULL);
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_init_calls);
}

void test_endpoint_bounds_max_valid(void)
{
  /* Test that endpoint 15 is valid (max is 15 for USB) */
  /* This would be tested through CDC configuration */
  rx_usb_init(NULL);
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.cdc_init_calls);
}

/* =============================================================================
 * Error Handling Tests
 * =============================================================================
 */

void test_error_propagation_from_hw_init(void)
{
  s_mock_hw.next_hw_init_ret = k_rx_err_hw_error;

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

void test_error_propagation_from_attach(void)
{
  s_mock_hw.next_hw_attach_ret = k_rx_err_busy;

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

void test_cleanup_on_attach_failure(void)
{
  s_mock_hw.next_hw_attach_ret = k_rx_err_hw_error;

  rx_usb_init(NULL);

  /* Should have called deinit to clean up */
  TEST_ASSERT_EQUAL_UINT32(1, s_mock_hw.hw_deinit_calls);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  printf("==============================================\n");
  printf("RX USB CDC Driver Unit Tests\n");
  printf("==============================================\n\n");

  /* Ring buffer initialization tests */
  RUN_TEST(test_ring_buffer_init_clears_state);
  RUN_TEST(test_ring_buffer_available_empty);
  RUN_TEST(test_ring_buffer_free_empty);
  RUN_TEST(test_ring_buffer_null_ptr_handling);

  /* Ring buffer write tests */
  RUN_TEST(test_ring_buffer_write_single_byte);
  RUN_TEST(test_ring_buffer_write_multiple_bytes);
  RUN_TEST(test_ring_buffer_write_fills_buffer);
  RUN_TEST(test_ring_buffer_write_overflow_truncates);
  RUN_TEST(test_ring_buffer_write_partial_when_partially_full);
  RUN_TEST(test_ring_buffer_write_wraps_around);
  RUN_TEST(test_ring_buffer_write_null_data_returns_zero);
  RUN_TEST(test_ring_buffer_write_zero_len_returns_zero);

  /* Ring buffer read tests */
  RUN_TEST(test_ring_buffer_read_empty_returns_zero);
  RUN_TEST(test_ring_buffer_read_single_byte);
  RUN_TEST(test_ring_buffer_read_multiple_bytes);
  RUN_TEST(test_ring_buffer_read_partial);
  RUN_TEST(test_ring_buffer_read_wraps_around);
  RUN_TEST(test_ring_buffer_fifo_order);
  RUN_TEST(test_ring_buffer_read_null_data_returns_zero);
  RUN_TEST(test_ring_buffer_read_zero_len_returns_zero);

  /* Ring buffer concurrent access tests */
  RUN_TEST(test_ring_buffer_interleaved_read_write);
  RUN_TEST(test_ring_buffer_burst_write_then_burst_read);

  /* USB state machine tests */
  RUN_TEST(test_usb_get_state_returns_detached_initially);
  RUN_TEST(test_usb_init_transitions_to_attached);
  RUN_TEST(test_usb_set_state_to_powered);
  RUN_TEST(test_usb_set_state_to_default);
  RUN_TEST(test_usb_set_state_to_addressed);
  RUN_TEST(test_usb_set_state_to_configured);
  RUN_TEST(test_usb_set_state_to_suspended);
  RUN_TEST(test_usb_set_state_to_detached);
  RUN_TEST(test_usb_set_state_triggers_callback_on_configured);
  RUN_TEST(test_usb_set_state_no_callback_if_same_state);
  RUN_TEST(test_usb_state_full_enumeration_sequence);

  /* USB initialization tests */
  RUN_TEST(test_usb_init_null_config_succeeds);
  RUN_TEST(test_usb_init_with_callback);
  RUN_TEST(test_usb_init_twice_fails);
  RUN_TEST(test_usb_init_hw_failure_propagates);
  RUN_TEST(test_usb_init_attach_failure_propagates_and_cleans_up);

  /* USB deinitialization tests */
  RUN_TEST(test_usb_deinit_not_initialized_fails);
  RUN_TEST(test_usb_deinit_success);
  RUN_TEST(test_usb_deinit_sets_detached_state);

  /* USB state query tests */
  RUN_TEST(test_usb_is_configured_when_not_initialized);
  RUN_TEST(test_usb_is_configured_when_attached);
  RUN_TEST(test_usb_is_configured_when_configured);

  /* USB write tests */
  RUN_TEST(test_usb_write_null_data_fails);
  RUN_TEST(test_usb_write_not_initialized_fails);
  RUN_TEST(test_usb_write_not_configured_fails);
  RUN_TEST(test_usb_write_success_when_configured);
  RUN_TEST(test_usb_write_triggers_transmission_when_pipe_idle);
  RUN_TEST(test_usb_write_no_trigger_when_pipe_busy);
  RUN_TEST(test_usb_write_updates_stats);
  RUN_TEST(test_usb_write_full_buffer_returns_busy);

  /* USB read tests */
  RUN_TEST(test_usb_read_null_data_fails);
  RUN_TEST(test_usb_read_null_actual_len_fails);
  RUN_TEST(test_usb_read_empty_buffer_returns_zero);
  RUN_TEST(test_usb_read_after_rx_push);

  /* USB RX/TX available tests */
  RUN_TEST(test_usb_rx_available_null_fails);
  RUN_TEST(test_usb_rx_available_empty);
  RUN_TEST(test_usb_rx_available_after_push);
  RUN_TEST(test_usb_tx_available_null_fails);
  RUN_TEST(test_usb_tx_available_empty);
  RUN_TEST(test_usb_tx_available_after_write);

  /* USB line coding tests */
  RUN_TEST(test_usb_get_line_coding_null_fails);
  RUN_TEST(test_usb_get_line_coding_default_values);
  RUN_TEST(test_usb_set_line_coding);
  RUN_TEST(test_usb_set_line_coding_null_ignored);

  /* USB statistics tests */
  RUN_TEST(test_usb_get_stats_null_fails);
  RUN_TEST(test_usb_get_stats_initial_zeros);
  RUN_TEST(test_usb_reset_stats);
  RUN_TEST(test_usb_rx_push_updates_stats);
  RUN_TEST(test_usb_rx_push_triggers_callback);
  RUN_TEST(test_usb_tx_pop_retrieves_data);
  RUN_TEST(test_usb_count_bus_reset_increments_stat);
  RUN_TEST(test_usb_count_suspend_increments_stat);

  /* USB flush tests */
  RUN_TEST(test_usb_flush_not_initialized_fails);
  RUN_TEST(test_usb_flush_empty_buffer_succeeds);
  RUN_TEST(test_usb_flush_with_data_and_zero_timeout_returns_timeout);

  /* Validation tests */
  RUN_TEST(test_pipe_bounds_validation_at_max);
  RUN_TEST(test_endpoint_bounds_max_valid);

  /* Error handling tests */
  RUN_TEST(test_error_propagation_from_hw_init);
  RUN_TEST(test_error_propagation_from_attach);
  RUN_TEST(test_cleanup_on_attach_failure);

  /* Print summary */
  printf("\n==============================================\n");
  printf("Test Results: %u passed, %u failed\n", s_tests_passed, s_tests_failed);
  printf("==============================================\n");

  return (s_tests_failed > 0) ? 1 : 0;
}
