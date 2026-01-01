/**
 * @file test_rx_usb_comm.c
 * @brief Unit Tests for USB Communication Layer
 *
 * Tests the high-level USB CDC communication layer that integrates
 * frame encoding/decoding with the USB driver.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "unity.h"

#include <string.h>

#include "mock_time.h"
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_frame.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"

/* =============================================================================
 * Constants & Definitions
 * =============================================================================
 */

/* Endpoint number from rx72n_regs.h */
#ifndef k_usb_cdc_ep_bulk_in
#define k_usb_cdc_ep_bulk_in 1
#endif

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_usb_comm_handle_t s_handle;

/* Helper to initialize USB and set configured state */
static void helper_usb_init_and_configure(void)
{
  rx_usb_init(NULL);
  extern void rx_usb_set_state(rx_usb_state_t state);
  rx_usb_set_state(k_usb_state_configured);
}

/* Extern internal function from rx_usb.c to drain TX buffer */
extern uint32_t rx_usb_tx_pop(uint8_t* data, uint32_t max_len);

void setUp(void)
{
  /* Initialize mock hardware */
  mock_usb_hw_init(NULL);
  mock_regs_init();

  /* Clear handle */
  memset(&s_handle, 0, sizeof(s_handle));
}

void tearDown(void)
{
  /* Deinitialize comm layer if initialized */
  rx_usb_comm_deinit(&s_handle);

  /* Deinitialize USB */
  rx_usb_deinit();

  /* Clear mock state */
  mock_usb_hw_deinit(NULL);
  mock_regs_clear();
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_usb_comm_init_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_init(NULL, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_init_success(void)
{
  rx_err_t err = rx_usb_comm_init(&s_handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.tx_sequence);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.rx_sequence);
}

void test_usb_comm_init_with_fec_config(void)
{
  rx_usb_comm_config_t config = {.fec_enabled = 1};

  rx_err_t err = rx_usb_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.fec_enabled);
}

void test_usb_comm_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_deinit(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_deinit_success(void)
{
  rx_usb_comm_init(&s_handle, NULL);

  rx_err_t err = rx_usb_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

void test_usb_comm_deinit_not_initialized_succeeds(void)
{
  /* Deinit on uninitialized handle should succeed gracefully */
  rx_err_t err = rx_usb_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Send Tests
 * =============================================================================
 */

void test_usb_comm_send_null_handle_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(NULL, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_usb_not_configured_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  rx_usb_init(NULL); /* USB initialized but not configured */
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_null_payload_with_len_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, NULL, 10);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_payload_too_large_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  uint8_t data[k_frame_max_payload + 1];

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_usb_comm_send_empty_payload_succeeds(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, NULL, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, s_handle.tx_sequence); /* Sequence should increment */
}

void test_usb_comm_send_with_payload_succeeds(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  uint8_t data[] = "Hello USB!";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 10);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, s_handle.tx_sequence);
}

void test_usb_comm_send_increments_sequence(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  uint8_t data[] = "test";

  rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 4);
  rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 4);
  rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL_UINT16(3, s_handle.tx_sequence);
}

void test_usb_comm_send_with_fec_flag(void)
{
  rx_usb_comm_config_t config = {.fec_enabled = 1};
  rx_usb_comm_init(&s_handle, &config);
  helper_usb_init_and_configure();
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  
  /* Verify FEC flag is set in the transmitted frame header */
  /* Frame: [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)]... */
  /* Flags are at offset 7 (0-based) */
  uint8_t tx_data[32];
  uint32_t tx_len = 0;
  
  /* Read from driver's ring buffer (since driver buffers data) */
  tx_len = rx_usb_tx_pop(tx_data, sizeof(tx_data));
  
  TEST_ASSERT_GREATER_THAN(8, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_fec_enabled, tx_data[7] & k_frame_flag_fec_enabled);
}

/* =============================================================================
 * Send ACK/NACK Tests
 * =============================================================================
 */

void test_usb_comm_send_ack_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_send_ack(NULL, 42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_ack_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_comm_send_ack(&s_handle, 42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_ack_usb_not_configured_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  rx_usb_init(NULL);

  rx_err_t err = rx_usb_comm_send_ack(&s_handle, 42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_ack_success(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send_ack(&s_handle, 42);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_comm_send_nack_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_send_nack(NULL, 42, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_nack_success(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send_nack(&s_handle, 42, k_frame_flag_soft_nack);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Receive Tests
 * =============================================================================
 */

void test_usb_comm_receive_null_handle_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(NULL, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_receive_null_frame_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);

  rx_err_t err = rx_usb_comm_receive(&s_handle, NULL, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_receive_not_initialized_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_receive_usb_not_configured_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  rx_usb_init(NULL);
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_receive_no_data_timeout(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Data Available Tests
 * =============================================================================
 */

void test_usb_comm_data_available_null_handle_fails(void)
{
  bool available;

  rx_err_t err = rx_usb_comm_data_available(NULL, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_data_available_null_available_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);

  rx_err_t err = rx_usb_comm_data_available(&s_handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_data_available_not_initialized_fails(void)
{
  bool available;

  rx_err_t err = rx_usb_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_data_available_empty(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  bool available = true;

  rx_err_t err = rx_usb_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(available);
}

/* =============================================================================
 * Is Ready Tests
 * =============================================================================
 */

void test_usb_comm_is_ready_null_handle_fails(void)
{
  bool ready;

  rx_err_t err = rx_usb_comm_is_ready(NULL, &ready);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_is_ready_null_ready_fails(void)
{
  rx_usb_comm_init(&s_handle, NULL);

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_is_ready_not_initialized(void)
{
  bool ready = true;

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, &ready);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_FALSE(ready);
}

void test_usb_comm_is_ready_usb_not_configured(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  rx_usb_init(NULL);
  bool ready = true;

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, &ready);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

void test_usb_comm_is_ready_configured(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  bool ready = false;

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, &ready);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(ready);
}

/* =============================================================================
 * Utility Function Tests
 * =============================================================================
 */

void test_usb_comm_reset_sequence(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  s_handle.tx_sequence = 100;
  s_handle.rx_sequence = 200;

  rx_usb_comm_reset_sequence(&s_handle);

  TEST_ASSERT_EQUAL_UINT16(0, s_handle.tx_sequence);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.rx_sequence);
}

void test_usb_comm_reset_sequence_null_handle(void)
{
  /* Should not crash on NULL */
  rx_usb_comm_reset_sequence(NULL);
}

void test_usb_comm_get_tx_sequence(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  s_handle.tx_sequence = 0xBEEF;

  uint16_t seq = rx_usb_comm_get_tx_sequence(&s_handle);

  TEST_ASSERT_EQUAL_UINT16(0xBEEF, seq);
}

void test_usb_comm_get_tx_sequence_null_handle(void)
{
  uint16_t seq = rx_usb_comm_get_tx_sequence(NULL);

  TEST_ASSERT_EQUAL_UINT16(0, seq);
}

void test_usb_comm_get_rx_sequence(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  s_handle.rx_sequence = 0xCAFE;

  uint16_t seq = rx_usb_comm_get_rx_sequence(&s_handle);

  TEST_ASSERT_EQUAL_UINT16(0xCAFE, seq);
}

void test_usb_comm_get_rx_sequence_null_handle(void)
{
  uint16_t seq = rx_usb_comm_get_rx_sequence(NULL);

  TEST_ASSERT_EQUAL_UINT16(0, seq);
}

void test_usb_comm_flush_rx(void)
{
  rx_usb_comm_init(&s_handle, NULL);
  s_handle.rx_buffer_len = 100;
  s_handle.rx_buffer_pos = 50;

  rx_usb_comm_flush_rx(&s_handle);

  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
}

void test_usb_comm_flush_rx_null_handle(void)
{
  /* Should not crash on NULL */
  rx_usb_comm_flush_rx(NULL);
}

/* =============================================================================
 * Mock Time Timeout Tests
 * =============================================================================
 */

void test_usb_comm_init_with_time_interface(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  rx_err_t err = rx_usb_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(&time_iface, s_handle.time_iface);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_timeout_without_time_iface(void)
{
  /* Without time interface, receive should return immediately with timeout */
  rx_usb_comm_init(&s_handle, NULL);
  helper_usb_init_and_configure();
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 100);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_NULL(s_handle.time_iface);
}

void test_usb_comm_receive_timeout_with_mock_time(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true); /* Auto-advance on sleep */
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};
  rx_usb_comm_init(&s_handle, &config);
  helper_usb_init_and_configure();

  rx_frame_t frame;

  /* Try to receive with 50ms timeout - should loop and eventually timeout */
  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 50);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Verify sleep was called multiple times (time advanced) */
  uint32_t sleep_count = mock_time_get_sleep_count(&mock);
  TEST_ASSERT_GREATER_THAN(0, sleep_count);

  /* Verify total sleep time is at least the timeout */
  uint32_t total_sleep = mock_time_get_total_sleep(&mock);
  TEST_ASSERT_GREATER_OR_EQUAL(50, total_sleep);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_immediate_timeout_zero(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};
  rx_usb_comm_init(&s_handle, &config);
  helper_usb_init_and_configure();

  rx_frame_t frame;

  /* Zero timeout should return immediately without sleeping */
  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* No sleep should have been called */
  TEST_ASSERT_EQUAL_UINT32(0, mock_time_get_sleep_count(&mock));

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_usb_comm_init_null_handle_fails);
  RUN_TEST(test_usb_comm_init_success);
  RUN_TEST(test_usb_comm_init_with_fec_config);
  RUN_TEST(test_usb_comm_deinit_null_handle_fails);
  RUN_TEST(test_usb_comm_deinit_success);
  RUN_TEST(test_usb_comm_deinit_not_initialized_succeeds);

  /* Send tests */
  RUN_TEST(test_usb_comm_send_null_handle_fails);
  RUN_TEST(test_usb_comm_send_not_initialized_fails);
  RUN_TEST(test_usb_comm_send_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_send_null_payload_with_len_fails);
  RUN_TEST(test_usb_comm_send_payload_too_large_fails);
  RUN_TEST(test_usb_comm_send_empty_payload_succeeds);
  RUN_TEST(test_usb_comm_send_with_payload_succeeds);
  RUN_TEST(test_usb_comm_send_increments_sequence);
  RUN_TEST(test_usb_comm_send_with_fec_flag);

  /* Send ACK/NACK tests */
  RUN_TEST(test_usb_comm_send_ack_null_handle_fails);
  RUN_TEST(test_usb_comm_send_ack_not_initialized_fails);
  RUN_TEST(test_usb_comm_send_ack_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_send_ack_success);
  RUN_TEST(test_usb_comm_send_nack_null_handle_fails);
  RUN_TEST(test_usb_comm_send_nack_success);

  /* Receive tests */
  RUN_TEST(test_usb_comm_receive_null_handle_fails);
  RUN_TEST(test_usb_comm_receive_null_frame_fails);
  RUN_TEST(test_usb_comm_receive_not_initialized_fails);
  RUN_TEST(test_usb_comm_receive_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_receive_no_data_timeout);

  /* Data available tests */
  RUN_TEST(test_usb_comm_data_available_null_handle_fails);
  RUN_TEST(test_usb_comm_data_available_null_available_fails);
  RUN_TEST(test_usb_comm_data_available_not_initialized_fails);
  RUN_TEST(test_usb_comm_data_available_empty);

  /* Is ready tests */
  RUN_TEST(test_usb_comm_is_ready_null_handle_fails);
  RUN_TEST(test_usb_comm_is_ready_null_ready_fails);
  RUN_TEST(test_usb_comm_is_ready_not_initialized);
  RUN_TEST(test_usb_comm_is_ready_usb_not_configured);
  RUN_TEST(test_usb_comm_is_ready_configured);

  /* Utility function tests */
  RUN_TEST(test_usb_comm_reset_sequence);
  RUN_TEST(test_usb_comm_reset_sequence_null_handle);
  RUN_TEST(test_usb_comm_get_tx_sequence);
  RUN_TEST(test_usb_comm_get_tx_sequence_null_handle);
  RUN_TEST(test_usb_comm_get_rx_sequence);
  RUN_TEST(test_usb_comm_get_rx_sequence_null_handle);
  RUN_TEST(test_usb_comm_flush_rx);
  RUN_TEST(test_usb_comm_flush_rx_null_handle);

  /* Mock time timeout tests */
  RUN_TEST(test_usb_comm_init_with_time_interface);
  RUN_TEST(test_usb_comm_receive_timeout_without_time_iface);
  RUN_TEST(test_usb_comm_receive_timeout_with_mock_time);
  RUN_TEST(test_usb_comm_receive_immediate_timeout_zero);

  return UNITY_END();
}
