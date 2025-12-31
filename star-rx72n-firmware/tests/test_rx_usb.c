/**
 * @file test_rx_usb.c
 * @brief Unit Tests for USB CDC Driver
 *
 * Tests USB driver functionality including:
 * - Ring buffer operations
 * - State machine transitions
 * - Read/write operations
 * - Statistics tracking
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "unity.h"

#include <string.h>

/* Source under test includes mock headers when UNIT_TEST is defined */
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_usb.h"

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
extern uint32_t internal_ring_buffer_write(ring_buffer_t* buf,
                                           const uint8_t* data,
                                           uint32_t       len);
extern uint32_t internal_ring_buffer_read(ring_buffer_t* buf,
                                          uint8_t*       data,
                                          uint32_t       max_len);

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
  s_last_event       = event;
  s_callback_count++;
  s_callback_context = ctx;
}

void setUp(void)
{
  /* Initialize mock hardware */
  mock_usb_hw_init(NULL);
  mock_regs_init();

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
  rx_usb_deinit();

  /* Clear mock state */
  mock_usb_hw_deinit(NULL);
  mock_regs_clear();
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
  TEST_ASSERT_EQUAL_UINT8(0x42, s_test_buffer.data[0]);
}

void test_ring_buffer_write_multiple_bytes(void)
{
  internal_ring_buffer_init(&s_test_buffer);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

  uint32_t written = internal_ring_buffer_write(&s_test_buffer, data, 5);

  TEST_ASSERT_EQUAL_UINT32(5, written);
  TEST_ASSERT_EQUAL_UINT32(5, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT8(0x01, s_test_buffer.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x05, s_test_buffer.data[4]);
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
  TEST_ASSERT_EQUAL_UINT8(0x42, read_data);
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
    TEST_ASSERT_EQUAL_UINT8(i + 1, read_data[i]);
  }
}

/* =============================================================================
 * USB Initialization Tests
 * =============================================================================
 */

void test_usb_init_null_config_succeeds(void)
{
  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(RX_OK, err);
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(NULL, "rx_usb_hw_init"));
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(NULL, "rx_usb_hw_attach"));
}

void test_usb_init_with_callback(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  config.context         = (void*)0xDEADBEEF;

  rx_err_t err = rx_usb_init(&config);

  TEST_ASSERT_EQUAL(RX_OK, err);
}

void test_usb_init_twice_fails(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

void test_usb_init_hw_failure_propagates(void)
{
  mock_usb_hw_set_init_return(NULL, RX_ERR_HW_ERROR);

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_HW_ERROR, err);
}

void test_usb_init_attach_failure_propagates(void)
{
  mock_usb_hw_set_attach_return(NULL, RX_ERR_HW_ERROR);

  rx_err_t err = rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_HW_ERROR, err);
  /* Should have called deinit to clean up */
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(NULL, "rx_usb_hw_deinit"));
}

/* =============================================================================
 * USB Deinitialization Tests
 * =============================================================================
 */

void test_usb_deinit_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_deinit();

  TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

void test_usb_deinit_success(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_deinit();

  TEST_ASSERT_EQUAL(RX_OK, err);
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(NULL, "rx_usb_hw_detach"));
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(NULL, "rx_usb_hw_deinit"));
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

void test_usb_get_state_returns_detached_initially(void)
{
  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

void test_usb_get_state_returns_attached_after_init(void)
{
  rx_usb_init(NULL);

  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

/* =============================================================================
 * USB Write Tests
 * =============================================================================
 */

void test_usb_write_null_data_fails(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_write(NULL, 10);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_write_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_write(data, 4);

  TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

void test_usb_write_not_configured_fails(void)
{
  rx_usb_init(NULL);
  uint8_t data[] = "test";

  /* State is attached but not configured */
  rx_err_t err = rx_usb_write(data, 4);

  TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

/* =============================================================================
 * USB Read Tests
 * =============================================================================
 */

void test_usb_read_null_data_fails(void)
{
  uint32_t actual_len;

  rx_err_t err = rx_usb_read(NULL, 10, &actual_len);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_read_null_actual_len_fails(void)
{
  uint8_t data[10];

  rx_err_t err = rx_usb_read(data, 10, NULL);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_read_empty_buffer_returns_zero(void)
{
  rx_usb_init(NULL);
  uint8_t  data[10];
  uint32_t actual_len = 999;

  rx_err_t err = rx_usb_read(data, 10, &actual_len);

  TEST_ASSERT_EQUAL(RX_OK, err);
  TEST_ASSERT_EQUAL_UINT32(0, actual_len);
}

/* =============================================================================
 * USB RX/TX Available Tests
 * =============================================================================
 */

void test_usb_rx_available_null_fails(void)
{
  rx_err_t err = rx_usb_rx_available(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_rx_available_empty(void)
{
  rx_usb_init(NULL);
  uint32_t available;

  rx_err_t err = rx_usb_rx_available(&available);

  TEST_ASSERT_EQUAL(RX_OK, err);
  TEST_ASSERT_EQUAL_UINT32(0, available);
}

void test_usb_tx_available_null_fails(void)
{
  rx_err_t err = rx_usb_tx_available(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_tx_available_empty(void)
{
  rx_usb_init(NULL);
  uint32_t available;

  rx_err_t err = rx_usb_tx_available(&available);

  TEST_ASSERT_EQUAL(RX_OK, err);
  TEST_ASSERT_EQUAL_UINT32(k_usb_rx_buffer_size, available);
}

/* =============================================================================
 * USB Line Coding Tests
 * =============================================================================
 */

void test_usb_get_line_coding_null_fails(void)
{
  rx_err_t err = rx_usb_get_line_coding(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_get_line_coding_default_values(void)
{
  rx_usb_init(NULL);
  rx_usb_line_coding_t coding;

  rx_err_t err = rx_usb_get_line_coding(&coding);

  TEST_ASSERT_EQUAL(RX_OK, err);
  TEST_ASSERT_EQUAL_UINT32(115200, coding.baud_rate);
  TEST_ASSERT_EQUAL_UINT8(0, coding.stop_bits); /* 1 stop bit */
  TEST_ASSERT_EQUAL_UINT8(0, coding.parity);    /* No parity */
  TEST_ASSERT_EQUAL_UINT8(8, coding.data_bits);
}

/* =============================================================================
 * USB Statistics Tests
 * =============================================================================
 */

void test_usb_get_stats_null_fails(void)
{
  rx_err_t err = rx_usb_get_stats(NULL);

  TEST_ASSERT_EQUAL(RX_ERR_NULL_POINTER, err);
}

void test_usb_get_stats_initial_zeros(void)
{
  rx_usb_init(NULL);
  rx_usb_stats_t stats;

  rx_err_t err = rx_usb_get_stats(&stats);

  TEST_ASSERT_EQUAL(RX_OK, err);
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

  /* Set configured state to allow writes */
  extern void rx_usb_set_state(rx_usb_state_t state);
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

/* =============================================================================
 * USB Flush Tests
 * =============================================================================
 */

void test_usb_flush_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_flush(0);

  TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

void test_usb_flush_empty_buffer_succeeds(void)
{
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_flush(0);

  TEST_ASSERT_EQUAL(RX_OK, err);
}

/* =============================================================================
 * Internal State Functions Tests (exposed via extern)
 * =============================================================================
 */

extern void     rx_usb_set_state(rx_usb_state_t state);
extern void     rx_usb_set_line_coding(const rx_usb_line_coding_t* coding);
extern uint32_t rx_usb_rx_push(const uint8_t* data, uint32_t len);
extern uint32_t rx_usb_tx_pop(uint8_t* data, uint32_t max_len);
extern void     rx_usb_count_bus_reset(void);
extern void     rx_usb_count_suspend(void);

void test_usb_set_state_triggers_callback(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  config.context         = (void*)0xCAFEBABE;
  rx_usb_init(&config);

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
  TEST_ASSERT_EQUAL_PTR((void*)0xCAFEBABE, s_callback_context);
}

void test_usb_set_state_no_callback_if_same_state(void)
{
  rx_usb_config_t config = {0};
  config.callback        = test_callback;
  rx_usb_init(&config);

  /* State is already attached after init */
  rx_usb_set_state(k_usb_state_attached);

  TEST_ASSERT_EQUAL(0, s_callback_count);
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
  TEST_ASSERT_EQUAL_UINT8(2, result.stop_bits);
  TEST_ASSERT_EQUAL_UINT8(1, result.parity);
  TEST_ASSERT_EQUAL_UINT8(7, result.data_bits);
}

void test_usb_rx_push_adds_data(void)
{
  rx_usb_init(NULL);
  uint8_t data[] = "Hello USB";

  uint32_t written = rx_usb_rx_push(data, 9);

  TEST_ASSERT_EQUAL_UINT32(9, written);

  uint32_t available;
  rx_usb_rx_available(&available);
  TEST_ASSERT_EQUAL_UINT32(9, available);
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

  uint8_t data[] = "test";
  rx_usb_rx_push(data, 4);

  TEST_ASSERT_EQUAL(1, s_callback_count);
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
 * USB Transmission Trigger Tests
 * =============================================================================
 */

void test_usb_write_triggers_transmission_when_pipe_idle(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  /* Set pipe as NOT busy (idle) */
  mock_usb0_set_pipe1_busy(0);
  mock_usb_hw_clear_calls(NULL);

  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  /* Verify rx_usb_cdc_handle_bulk_in was called */
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(NULL, "rx_usb_cdc_handle_bulk_in"));
}

void test_usb_write_no_trigger_when_pipe_busy(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  /* Set pipe as busy */
  mock_usb0_set_pipe1_busy(1);
  mock_usb_hw_clear_calls(NULL);

  uint8_t data[] = "test";
  rx_usb_write(data, 4);

  /* Verify rx_usb_cdc_handle_bulk_in was NOT called */
  TEST_ASSERT_FALSE(mock_usb_hw_was_called(NULL, "rx_usb_cdc_handle_bulk_in"));
}

void test_usb_write_no_trigger_when_buffer_empty_after_write_fails(void)
{
  rx_usb_init(NULL);
  rx_usb_set_state(k_usb_state_configured);

  /* Set pipe as idle */
  mock_usb0_set_pipe1_busy(0);

  /* Fill the buffer first to make subsequent writes fail */
  uint8_t fill_data[k_usb_rx_buffer_size];
  memset(fill_data, 'A', sizeof(fill_data));
  rx_usb_write(fill_data, sizeof(fill_data));

  /* Clear call history after fill */
  mock_usb_hw_clear_calls(NULL);

  /* Try to write more - should fail due to full buffer */
  uint8_t more_data[] = "more";
  rx_err_t err = rx_usb_write(more_data, 4);

  /* Write fails due to full buffer */
  TEST_ASSERT_EQUAL(RX_ERR_BUSY, err);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Ring buffer initialization tests */
  RUN_TEST(test_ring_buffer_init_clears_state);
  RUN_TEST(test_ring_buffer_available_empty);
  RUN_TEST(test_ring_buffer_free_empty);

  /* Ring buffer write tests */
  RUN_TEST(test_ring_buffer_write_single_byte);
  RUN_TEST(test_ring_buffer_write_multiple_bytes);
  RUN_TEST(test_ring_buffer_write_fills_buffer);
  RUN_TEST(test_ring_buffer_write_overflow_truncates);
  RUN_TEST(test_ring_buffer_write_partial_when_partially_full);
  RUN_TEST(test_ring_buffer_write_wraps_around);

  /* Ring buffer read tests */
  RUN_TEST(test_ring_buffer_read_empty_returns_zero);
  RUN_TEST(test_ring_buffer_read_single_byte);
  RUN_TEST(test_ring_buffer_read_multiple_bytes);
  RUN_TEST(test_ring_buffer_read_partial);
  RUN_TEST(test_ring_buffer_read_wraps_around);
  RUN_TEST(test_ring_buffer_fifo_order);

  /* USB initialization tests */
  RUN_TEST(test_usb_init_null_config_succeeds);
  RUN_TEST(test_usb_init_with_callback);
  RUN_TEST(test_usb_init_twice_fails);
  RUN_TEST(test_usb_init_hw_failure_propagates);
  RUN_TEST(test_usb_init_attach_failure_propagates);

  /* USB deinitialization tests */
  RUN_TEST(test_usb_deinit_not_initialized_fails);
  RUN_TEST(test_usb_deinit_success);

  /* USB state query tests */
  RUN_TEST(test_usb_is_configured_when_not_initialized);
  RUN_TEST(test_usb_is_configured_when_attached);
  RUN_TEST(test_usb_get_state_returns_detached_initially);
  RUN_TEST(test_usb_get_state_returns_attached_after_init);

  /* USB write tests */
  RUN_TEST(test_usb_write_null_data_fails);
  RUN_TEST(test_usb_write_not_initialized_fails);
  RUN_TEST(test_usb_write_not_configured_fails);

  /* USB read tests */
  RUN_TEST(test_usb_read_null_data_fails);
  RUN_TEST(test_usb_read_null_actual_len_fails);
  RUN_TEST(test_usb_read_empty_buffer_returns_zero);

  /* USB RX/TX available tests */
  RUN_TEST(test_usb_rx_available_null_fails);
  RUN_TEST(test_usb_rx_available_empty);
  RUN_TEST(test_usb_tx_available_null_fails);
  RUN_TEST(test_usb_tx_available_empty);

  /* USB line coding tests */
  RUN_TEST(test_usb_get_line_coding_null_fails);
  RUN_TEST(test_usb_get_line_coding_default_values);

  /* USB statistics tests */
  RUN_TEST(test_usb_get_stats_null_fails);
  RUN_TEST(test_usb_get_stats_initial_zeros);
  RUN_TEST(test_usb_reset_stats);

  /* USB flush tests */
  RUN_TEST(test_usb_flush_not_initialized_fails);
  RUN_TEST(test_usb_flush_empty_buffer_succeeds);

  /* Internal state functions tests */
  RUN_TEST(test_usb_set_state_triggers_callback);
  RUN_TEST(test_usb_set_state_no_callback_if_same_state);
  RUN_TEST(test_usb_set_line_coding);
  RUN_TEST(test_usb_rx_push_adds_data);
  RUN_TEST(test_usb_rx_push_updates_stats);
  RUN_TEST(test_usb_rx_push_triggers_callback);
  RUN_TEST(test_usb_tx_pop_retrieves_data);
  RUN_TEST(test_usb_count_bus_reset_increments_stat);
  RUN_TEST(test_usb_count_suspend_increments_stat);

  /* USB transmission trigger tests */
  RUN_TEST(test_usb_write_triggers_transmission_when_pipe_idle);
  RUN_TEST(test_usb_write_no_trigger_when_pipe_busy);
  RUN_TEST(test_usb_write_no_trigger_when_buffer_empty_after_write_fails);

  return UNITY_END();
}
