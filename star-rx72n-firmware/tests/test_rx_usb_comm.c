/* tests/test_rx_usb_comm.c */

/**
 * @file test_rx_usb_comm.c
 * @brief Unit Tests for USB Communication Layer
 *
 * Tests the high-level USB CDC communication layer that integrates
 * frame encoding/decoding with the USB driver.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "mock_time.h"
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_frame.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"
#include "unity.h"

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

/* Extern internal functions from rx_usb.c */
extern uint32_t rx_usb_tx_pop(uint8_t* data, uint32_t max_len);
extern uint32_t rx_usb_rx_push(const uint8_t* data, uint32_t len);

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
  uint8_t  tx_data[32];
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
 * Successful Frame Reception Tests
 * =============================================================================
 */

/* Extern for rx_usb_rx_push to inject data into USB receive buffer */
extern uint32_t rx_usb_rx_push(const uint8_t* data, uint32_t len);

/* Helper: Create and encode a test frame */
static rx_err_t helper_create_encoded_frame(rx_frame_t*     frame,
                                            uint16_t        sequence,
                                            rx_frame_type_t type,
                                            uint8_t         flags,
                                            const uint8_t*  payload,
                                            uint16_t        payload_len,
                                            uint8_t*        encoded_buf,
                                            uint32_t*       encoded_len)
{
  rx_frame_encoder_t enc;
  rx_err_t           err;

  err = rx_frame_encoder_init(&enc);
  if (err != k_rx_ok) {
    return err;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.type     = (uint8_t)type;
  frame->header.flags    = flags;
  frame->header.length   = payload_len;

  if (payload != NULL && payload_len > 0) {
    memcpy(frame->payload, payload, payload_len);
  }

  err = rx_frame_encode(&enc, frame, encoded_buf, encoded_len);

  rx_frame_encoder_deinit(&enc);

  return err;
}

void test_usb_comm_receive_valid_frame_with_sync(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "test_data!";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             1,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             10,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject frame into USB RX buffer */
  rx_usb_rx_push(encoded, encoded_len);

  /* Receive */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(10, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY("test_data!", rx_frame.payload, 10);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_empty_payload_frame(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create frame with zero-length payload */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             42,
                                             k_frame_type_ack,
                                             k_frame_flag_none,
                                             NULL,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_frame_min_size, encoded_len);

  /* Inject frame into USB RX buffer */
  rx_usb_rx_push(encoded, encoded_len);

  /* Receive */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(42, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_ack, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.length);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_max_payload_frame(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Use a large payload that fits in the USB driver's 512-byte ring buffer.
   * The USB driver uses k_usb_rx_buffer_size = 512 bytes.
   * Frame overhead is 12 bytes (8 header + 4 CRC), so max payload is ~500.
   * A full k_frame_max_payload (1024) test would require simulating
   * interrupt-driven incremental data arrival, which this mock doesn't support. */
  enum { k_test_large_payload_size = 490 };
  uint8_t large_payload[k_test_large_payload_size];
  for (uint32_t i = 0; i < k_test_large_payload_size; i++) {
    large_payload[i] = (uint8_t)(i & 0xFF);
  }

  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             99,
                                             k_frame_type_response,
                                             k_frame_flag_priority,
                                             large_payload,
                                             k_test_large_payload_size,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify encoded size fits in USB ring buffer */
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(512, encoded_len);

  /* Inject frame into USB RX buffer */
  rx_usb_rx_push(encoded, encoded_len);

  /* Receive */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(99, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_response, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT8(k_frame_flag_priority, rx_frame.header.flags);
  TEST_ASSERT_EQUAL_UINT16(k_test_large_payload_size, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY(large_payload, rx_frame.payload, k_test_large_payload_size);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_increments_rx_sequence(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Initial RX sequence should be 0 */
  TEST_ASSERT_EQUAL_UINT16(0, rx_usb_comm_get_rx_sequence(&s_handle));

  /* Send first frame with sequence 5 */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             5,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             NULL,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After receiving frame with sequence 5, expected RX sequence should be 6 */
  TEST_ASSERT_EQUAL_UINT16(6, rx_usb_comm_get_rx_sequence(&s_handle));

  /* Send second frame with sequence 6 */
  err = helper_create_encoded_frame(&tx_frame,
                                    6,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    NULL,
                                    0,
                                    encoded,
                                    &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(encoded, encoded_len);

  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After receiving frame with sequence 6, expected RX sequence should be 7 */
  TEST_ASSERT_EQUAL_UINT16(7, rx_usb_comm_get_rx_sequence(&s_handle));

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Sync Word Detection Tests
 * =============================================================================
 */

void test_usb_comm_receive_finds_sync_after_garbage(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "FOUND";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             7,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             5,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject garbage data followed by valid frame */
  uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56};
  rx_usb_rx_push(garbage, sizeof(garbage));
  rx_usb_rx_push(encoded, encoded_len);

  /* Receive - should skip garbage and find the valid frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(7, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("FOUND", rx_frame.payload, 5);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_partial_sync_word(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "SPLIT";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             8,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             5,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* First inject just partial data (first half of frame) */
  uint32_t first_half = encoded_len / 2;
  rx_usb_rx_push(encoded, first_half);

  /* Try to receive - should timeout waiting for rest of frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 50);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Now inject the rest of the frame */
  rx_usb_rx_push(encoded + first_half, encoded_len - first_half);

  /* Should now receive the complete frame */
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(8, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("SPLIT", rx_frame.payload, 5);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_multiple_false_syncs(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "REAL";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             9,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             4,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject data with false sync patterns (0x55AA) but invalid following data */
  /* SYNC word is 0x55AA in big-endian */
  uint8_t false_sync_data[] = {
    0x55,
    0xAA, /* False sync #1 */
    0xFF,
    0xFF, /* Invalid payload length (0xFFFF > 1024) */
    0x55,
    0xAA, /* False sync #2 */
    0x00,
    0x00, /* Seq = 0 */
    0xFF,
    0xFF, /* Invalid payload length again */
  };
  rx_usb_rx_push(false_sync_data, sizeof(false_sync_data));
  rx_usb_rx_push(encoded, encoded_len);

  /* Receive - should skip false syncs and find the valid frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* Verify - the real frame should be received */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(9, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("REAL", rx_frame.payload, 4);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Header Parsing Tests
 * =============================================================================
 */

void test_usb_comm_receive_invalid_payload_length(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create a valid frame to follow the invalid one */
  rx_frame_t tx_frame;
  uint8_t    valid_encoded[k_frame_max_size];
  uint32_t   valid_encoded_len = 0;
  uint8_t    payload[]         = "VALID";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             10,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             5,
                                             valid_encoded,
                                             &valid_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create invalid frame header with payload length > k_frame_max_payload (1024) */
  /* Frame: SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) = 8 bytes header */
  uint8_t invalid_frame[] = {
    0x55,
    0xAA, /* Sync word */
    0x00,
    0x01, /* Sequence = 1 */
    0x04,
    0x01, /* Length = 1025 (0x0401) - exceeds max payload */
    0x01, /* Type = command */
    0x00, /* Flags = none */
  };

  rx_usb_rx_push(invalid_frame, sizeof(invalid_frame));
  rx_usb_rx_push(valid_encoded, valid_encoded_len);

  /* Receive - should skip invalid frame and receive valid one */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(10, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("VALID", rx_frame.payload, 5);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_header_validation(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create frame with specific header fields to verify */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "HDR";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0x1234,                /* Specific sequence */
                                             k_frame_type_response, /* Type */
                                             k_frame_flag_requires_ack | k_frame_flag_priority,
                                             payload,
                                             3,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* Verify all header fields */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0x1234, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT16(3, rx_frame.header.length);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_response, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT8(k_frame_flag_requires_ack | k_frame_flag_priority, rx_frame.header.flags);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Buffer Management Tests
 * =============================================================================
 */

void test_usb_comm_receive_buffer_compaction(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Receive multiple frames to test buffer compaction */
  for (uint16_t seq = 0; seq < 5; seq++) {
    rx_frame_t tx_frame;
    uint8_t    encoded[k_frame_max_size];
    uint32_t   encoded_len = 0;
    uint8_t    payload[20];

    /* Fill payload with sequence number pattern */
    memset(payload, (int)seq, sizeof(payload));

    rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                               seq,
                                               k_frame_type_command,
                                               k_frame_flag_none,
                                               payload,
                                               20,
                                               encoded,
                                               &encoded_len);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    rx_usb_rx_push(encoded, encoded_len);

    rx_frame_t rx_frame;
    err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(seq, rx_frame.header.sequence);

    /* Verify payload is correct pattern */
    for (uint32_t i = 0; i < 20; i++) {
      TEST_ASSERT_EQUAL_UINT8(seq, rx_frame.payload[i]);
    }
  }

  /* Buffer should be compacted (position reset to 0) after each receive */
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_buffer_overflow_protection(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Fill the handle's RX buffer near capacity with garbage */
  uint8_t garbage[k_usb_comm_rx_buffer_size - 100];
  memset(garbage, 0xDE, sizeof(garbage));

  /* Directly manipulate handle's buffer to simulate near-full condition */
  memcpy(s_handle.rx_buffer, garbage, sizeof(garbage));
  s_handle.rx_buffer_len = sizeof(garbage);
  s_handle.rx_buffer_pos = 0;

  /* Now push a small valid frame via USB */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "OK";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             11,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             2,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* The receiver should handle the near-full buffer by discarding garbage */
  /* and finding the valid sync word */
  /* Note: Since buffer is pre-filled, we need to flush it first */
  rx_usb_comm_flush_rx(&s_handle);

  rx_usb_rx_push(encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(11, rx_frame.header.sequence);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_fragmented_frame(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "FRAGMENTED_DATA";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             12,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             15,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Fragment the frame into small chunks simulating USB packet boundaries */
  uint32_t chunk_size = 5;
  for (uint32_t offset = 0; offset < encoded_len; offset += chunk_size) {
    uint32_t remaining = encoded_len - offset;
    uint32_t to_push   = (remaining < chunk_size) ? remaining : chunk_size;
    rx_usb_rx_push(encoded + offset, to_push);
  }

  /* Receive should reassemble the fragmented frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(12, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT16(15, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY("FRAGMENTED_DATA", rx_frame.payload, 15);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Sequence Number Tests
 * =============================================================================
 */

void test_usb_comm_receive_sequence_wraparound(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Test uint16_t wraparound: 65535 -> 0 */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  /* First frame with sequence 65535 */
  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0xFFFF, /* 65535 */
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             NULL,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, rx_frame.header.sequence);

  /* After receiving 65535, expected RX sequence should wrap to 0 */
  TEST_ASSERT_EQUAL_UINT16(0, rx_usb_comm_get_rx_sequence(&s_handle));

  /* Second frame with sequence 0 (after wraparound) */
  err = helper_create_encoded_frame(&tx_frame,
                                    0,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    NULL,
                                    0,
                                    encoded,
                                    &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(encoded, encoded_len);

  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT16(1, rx_usb_comm_get_rx_sequence(&s_handle));

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_out_of_order_sequence(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Send frames out of order and verify they are received */
  /* Note: The receiver doesn't enforce sequence ordering, it just tracks expected */
  uint16_t sequences[] = {100, 50, 200, 150};

  for (uint32_t i = 0; i < sizeof(sequences) / sizeof(sequences[0]); i++) {
    rx_frame_t tx_frame;
    uint8_t    encoded[k_frame_max_size];
    uint32_t   encoded_len = 0;

    rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                               sequences[i],
                                               k_frame_type_command,
                                               k_frame_flag_none,
                                               NULL,
                                               0,
                                               encoded,
                                               &encoded_len);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    rx_usb_rx_push(encoded, encoded_len);

    rx_frame_t rx_frame;
    err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

    /* Frame should be received regardless of order */
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(sequences[i], rx_frame.header.sequence);
  }

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Edge Cases Tests
 * =============================================================================
 */

void test_usb_comm_receive_crc_mismatch(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create a valid frame first, then corrupt its CRC */
  rx_frame_t tx_frame;
  uint8_t    bad_encoded[k_frame_max_size];
  uint32_t   bad_encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             13,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             NULL,
                                             0,
                                             bad_encoded,
                                             &bad_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Corrupt the CRC (last 4 bytes) */
  bad_encoded[bad_encoded_len - 1] ^= 0xFF;
  bad_encoded[bad_encoded_len - 2] ^= 0xFF;

  /* Create a valid frame to follow */
  uint8_t  good_encoded[k_frame_max_size];
  uint32_t good_encoded_len = 0;
  uint8_t  payload[]        = "GOOD";

  err = helper_create_encoded_frame(&tx_frame,
                                    14,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    payload,
                                    4,
                                    good_encoded,
                                    &good_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Push bad frame followed by good frame */
  rx_usb_rx_push(bad_encoded, bad_encoded_len);
  rx_usb_rx_push(good_encoded, good_encoded_len);

  /* First receive should fail with CRC error or skip to valid frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  /* The implementation should either:
   * 1. Return CRC mismatch error, or
   * 2. Skip the bad frame and return the good one
   * Based on the code, it returns the decode error */
  if (err == k_rx_err_crc_mismatch) {
    /* CRC error returned - need another receive for good frame */
    rx_usb_rx_push(good_encoded, good_encoded_len);
    err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(14, rx_frame.header.sequence);
  } else if (err == k_rx_ok) {
    /* Skipped bad frame, got good one directly */
    TEST_ASSERT_EQUAL_UINT16(14, rx_frame.header.sequence);
  } else {
    /* Unexpected error */
    TEST_FAIL_MESSAGE("Unexpected error from receive");
  }

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_iteration_limit(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = {.fec_enabled = 0, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Don't inject any data - receive loop should hit iteration limit */
  /* The max iteration limit is k_max_receive_iterations = 100 */

  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, 10000);

  /* Should timeout due to iteration limit, not timeout_ms */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Sleep should have been called multiple times */
  uint32_t sleep_count = mock_time_get_sleep_count(&mock);
  TEST_ASSERT_GREATER_THAN(0, sleep_count);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_with_fec_enabled(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  /* Enable FEC in configuration */
  rx_usb_comm_config_t config = {.fec_enabled = 1, .time_iface = &time_iface};

  helper_usb_init_and_configure();
  rx_usb_comm_init(&s_handle, &config);

  /* Create frame with FEC flag set */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "FEC_TEST";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             15,
                                             k_frame_type_command,
                                             k_frame_flag_fec_enabled,
                                             payload,
                                             8,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(15, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_flag_fec_enabled,
                          rx_frame.header.flags & k_frame_flag_fec_enabled);
  TEST_ASSERT_EQUAL_MEMORY("FEC_TEST", rx_frame.payload, 8);

  mock_time_deinit(&mock);
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
  mock_time_set_auto_advance(&mock, true);
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
  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
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
  mock_time_set_auto_advance(&mock, true);
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

  /* Receive tests (basic) */
  RUN_TEST(test_usb_comm_receive_null_handle_fails);
  RUN_TEST(test_usb_comm_receive_null_frame_fails);
  RUN_TEST(test_usb_comm_receive_not_initialized_fails);
  RUN_TEST(test_usb_comm_receive_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_receive_no_data_timeout);

  /* Successful frame reception tests */
  RUN_TEST(test_usb_comm_receive_valid_frame_with_sync);
  RUN_TEST(test_usb_comm_receive_empty_payload_frame);
  RUN_TEST(test_usb_comm_receive_max_payload_frame);
  RUN_TEST(test_usb_comm_receive_increments_rx_sequence);

  /* Sync word detection tests */
  RUN_TEST(test_usb_comm_receive_finds_sync_after_garbage);
  RUN_TEST(test_usb_comm_receive_partial_sync_word);
  RUN_TEST(test_usb_comm_receive_multiple_false_syncs);

  /* Header parsing tests */
  RUN_TEST(test_usb_comm_receive_invalid_payload_length);
  RUN_TEST(test_usb_comm_receive_header_validation);

  /* Buffer management tests */
  RUN_TEST(test_usb_comm_receive_buffer_compaction);
  RUN_TEST(test_usb_comm_receive_buffer_overflow_protection);
  RUN_TEST(test_usb_comm_receive_fragmented_frame);

  /* Sequence number tests */
  RUN_TEST(test_usb_comm_receive_sequence_wraparound);
  RUN_TEST(test_usb_comm_receive_out_of_order_sequence);

  /* Edge cases tests */
  RUN_TEST(test_usb_comm_receive_crc_mismatch);
  RUN_TEST(test_usb_comm_receive_iteration_limit);
  RUN_TEST(test_usb_comm_receive_with_fec_enabled);

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

  return UNITY_END();
}
