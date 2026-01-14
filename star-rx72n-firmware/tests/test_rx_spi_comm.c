/* tests/test_rx_spi_comm.c */

/**
 * @file test_rx_spi_comm.c
 * @brief Unit Tests for SPI Communication Layer
 *
 * Tests the high-level SPI communication layer that integrates
 * frame encoding/decoding with the SPI HAL for RPi5 to RX72N communication.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "hardware.h"
#include "mock_rspi.h"
#include "rx_frame.h"
#include "rx_spi_comm.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Test constant values */
typedef enum {
  k_test_channel_default = 0,
  k_test_channel_alt     = 1,
  k_test_channel_invalid = 5,
  k_test_sequence_a      = 42,
  k_test_sequence_b      = 123,
  k_test_sequence_max    = 0xFFFF,
  k_test_timeout_zero    = 0,
  k_test_timeout_short   = 100,
  k_test_payload_small   = 4,
  k_test_payload_medium  = 64,
  k_test_payload_large   = 256,
} test_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_spi_comm_handle_t s_handle;

/**
 * @brief Initialize RSPI channel via mock so channel is ready
 */
static void helper_init_rspi_channel(uint8_t channel)
{
  rspi_init_peripheral(channel, k_spi_comm_default_mode, false);
}

/**
 * @brief Create and encode a valid frame for injection
 *
 * @param type Frame type
 * @param sequence Sequence number
 * @param payload Payload data (can be NULL)
 * @param payload_len Payload length
 * @param out_buffer Output buffer
 * @param out_len Output length
 */
static void helper_create_encoded_frame(rx_frame_type_t type,
                                        uint16_t        sequence,
                                        const uint8_t*  payload,
                                        uint32_t        payload_len,
                                        uint8_t*        out_buffer,
                                        uint32_t*       out_len)
{
  rx_frame_encoder_t encoder;
  rx_frame_encoder_init(&encoder);

  rx_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.header.sequence = sequence;
  frame.header.length   = (uint16_t)payload_len;
  frame.header.type     = (uint8_t)type;
  frame.header.flags    = k_frame_flag_none;

  if (payload != NULL && payload_len > 0) {
    memcpy(frame.payload, payload, payload_len);
  }

  rx_frame_encode(&encoder, &frame, out_buffer, out_len);
  rx_frame_encoder_deinit(&encoder);
}

void setUp(void)
{
  /* Initialize mock hardware */
  mock_rspi_init(NULL);

  /* Clear handle */
  memset(&s_handle, 0, sizeof(s_handle));
}

void tearDown(void)
{
  /* Deinitialize comm layer if initialized */
  rx_spi_comm_deinit(&s_handle);

  /* Clear mock state */
  mock_rspi_deinit(NULL);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_spi_comm_init_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_init(NULL, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_init_success_default_config(void)
{
  rx_err_t err = rx_spi_comm_init(&s_handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
  TEST_ASSERT_EQUAL_UINT8(k_spi_comm_default_channel, s_handle.channel);
  TEST_ASSERT_FALSE(s_handle.fec_enabled);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.tx_sequence);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.rx_sequence);
}

void test_spi_comm_init_with_custom_channel(void)
{
  rx_spi_comm_config_t config = {
    .channel     = k_test_channel_alt,
    .spi_mode    = 0,
    .fec_enabled = false,
  };

  rx_err_t err = rx_spi_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_channel_alt, s_handle.channel);
}

void test_spi_comm_init_with_fec_enabled(void)
{
  rx_spi_comm_config_t config = {
    .channel     = 0,
    .spi_mode    = 0,
    .fec_enabled = true,
  };

  rx_err_t err = rx_spi_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.fec_enabled);
}

void test_spi_comm_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_deinit(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_deinit_success(void)
{
  rx_spi_comm_init(&s_handle, NULL);

  rx_err_t err = rx_spi_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

void test_spi_comm_deinit_not_initialized_succeeds(void)
{
  /* Deinit on uninitialized handle should succeed gracefully */
  rx_err_t err = rx_spi_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Send Tests
 * =============================================================================
 */

void test_spi_comm_send_null_handle_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(NULL, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_null_payload_with_len_fails(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, NULL, 10);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_payload_too_large_fails(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  uint8_t data[k_frame_max_payload + 1];

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_spi_comm_send_empty_payload_succeeds(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, NULL, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, s_handle.tx_sequence);
}

void test_spi_comm_send_with_payload_succeeds(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  uint8_t data[] = "Hello SPI!";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 10);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, s_handle.tx_sequence);
}

void test_spi_comm_send_increments_sequence(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  uint8_t data[] = "test";

  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);
  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);
  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL_UINT16(3, s_handle.tx_sequence);
}

void test_spi_comm_send_sequence_wraps(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  s_handle.tx_sequence = k_test_sequence_max;
  uint8_t data[]       = "test";

  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  /* 0xFFFF + 1 wraps to 0x0000 */
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.tx_sequence);
}

void test_spi_comm_send_with_fec_flag(void)
{
  rx_spi_comm_config_t config = {.channel = 0, .spi_mode = 0, .fec_enabled = true};
  rx_spi_comm_init(&s_handle, &config);
  helper_init_rspi_channel(k_test_channel_default);
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify FEC flag is set in the transmitted frame */
  uint8_t  tx_data[64];
  uint32_t tx_len = 0;
  mock_rspi_get_tx_data(NULL, k_test_channel_default, tx_data, sizeof(tx_data), &tx_len);

  /* Frame: [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)]... */
  /* Flags are at offset 7 (0-based) */
  TEST_ASSERT_GREATER_THAN(8, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_fec_enabled, tx_data[7] & k_frame_flag_fec_enabled);
}

void test_spi_comm_send_transfer_error_propagates(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_set_transfer_return(NULL, k_rx_err_timeout);
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_spi_comm_send_large_payload(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  uint8_t data[k_test_payload_large];
  for (uint32_t i = 0; i < k_test_payload_large; i++) {
    data[i] = (uint8_t)(i & 0xFF);
  }

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_payload_large);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_spi_comm_send_missing_host_ack_times_out(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_clear_calls(NULL);
  mock_rspi_set_write_ready(NULL, k_test_channel_default, false);

  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  uint32_t transfer_calls = mock_rspi_get_call_count(NULL, "rspi_peripheral_transfer");
  TEST_ASSERT_EQUAL_UINT32(0, transfer_calls);

  uint32_t ready_calls = mock_rspi_get_call_count(NULL, "rspi_peripheral_write_ready");
  TEST_ASSERT_TRUE(ready_calls > 0);
}

/* =============================================================================
 * Send ACK/NACK Tests
 * =============================================================================
 */

void test_spi_comm_send_ack_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_send_ack(NULL, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_ack_not_initialized_fails(void)
{
  rx_err_t err = rx_spi_comm_send_ack(&s_handle, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_ack_success(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  rx_err_t err = rx_spi_comm_send_ack(&s_handle, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify ACK frame was transmitted */
  uint8_t  tx_data[32];
  uint32_t tx_len = 0;
  mock_rspi_get_tx_data(NULL, k_test_channel_default, tx_data, sizeof(tx_data), &tx_len);

  TEST_ASSERT_EQUAL(k_frame_min_size, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_ack, tx_data[6]);
}

void test_spi_comm_send_ack_transfer_error_propagates(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_set_transfer_return(NULL, k_rx_err_timeout);

  rx_err_t err = rx_spi_comm_send_ack(&s_handle, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_spi_comm_send_nack_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_send_nack(NULL, k_test_sequence_a, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_nack_not_initialized_fails(void)
{
  rx_err_t err = rx_spi_comm_send_nack(&s_handle, k_test_sequence_a, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_nack_success(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  rx_err_t err = rx_spi_comm_send_nack(&s_handle, k_test_sequence_b, k_frame_flag_soft_nack);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify NACK frame was transmitted */
  uint8_t  tx_data[32];
  uint32_t tx_len = 0;
  mock_rspi_get_tx_data(NULL, k_test_channel_default, tx_data, sizeof(tx_data), &tx_len);

  TEST_ASSERT_EQUAL(k_frame_min_size, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_nack, tx_data[6]);
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_soft_nack, tx_data[7] & k_frame_flag_soft_nack);
}

void test_spi_comm_send_nack_transfer_error_propagates(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_set_transfer_return(NULL, k_rx_err_timeout);

  rx_err_t err = rx_spi_comm_send_nack(&s_handle, k_test_sequence_a, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Receive Tests
 * =============================================================================
 */

void test_spi_comm_receive_null_handle_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_spi_comm_receive(NULL, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_receive_null_frame_fails(void)
{
  rx_spi_comm_init(&s_handle, NULL);

  rx_err_t err = rx_spi_comm_receive(&s_handle, NULL, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_receive_not_initialized_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_receive_no_data_timeout(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  rx_frame_t frame;

  /* No data injected, should timeout immediately */
  rx_err_t err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_spi_comm_receive_available_check_error_propagates(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_set_available_return(NULL, k_rx_err_spi_error);
  rx_frame_t frame;

  rx_err_t err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_spi_error, err);
}

void test_spi_comm_receive_valid_frame_success(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  /* Create and inject a valid frame */
  uint8_t  payload[] = "TEST";
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command,
                              k_test_sequence_a,
                              payload,
                              4,
                              encoded_frame,
                              &encoded_len);

  mock_rspi_inject_rx_data(NULL, k_test_channel_default, encoded_frame, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_sequence_a, frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(4, frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY(payload, frame.payload, 4);
}

void test_spi_comm_receive_updates_rx_sequence(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command, 100, NULL, 0, encoded_frame, &encoded_len);

  mock_rspi_inject_rx_data(NULL, k_test_channel_default, encoded_frame, encoded_len);

  rx_frame_t frame;
  rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* RX sequence should be updated to received sequence + 1 */
  TEST_ASSERT_EQUAL_UINT16(101, s_handle.rx_sequence);
}

void test_spi_comm_receive_invalid_sync_word(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  /* Create a frame with invalid sync word */
  uint8_t bad_frame[k_frame_min_size] = {
    0x00,
    0x00, /* Invalid SYNC */
    0x00,
    0x01, /* SEQ = 1 */
    0x00,
    0x00, /* LEN = 0 */
    0x01, /* TYPE = command */
    0x00, /* FLAGS = none */
    0x00,
    0x00,
    0x00,
    0x00, /* CRC (invalid) */
  };

  mock_rspi_inject_rx_data(NULL, k_test_channel_default, bad_frame, sizeof(bad_frame));

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

void test_spi_comm_receive_transfer_error_propagates(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  /* Inject valid frame to pass availability check */
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command, 0, NULL, 0, encoded_frame, &encoded_len);
  mock_rspi_inject_rx_data(NULL, k_test_channel_default, encoded_frame, encoded_len);

  /* But make transfer fail */
  mock_rspi_set_transfer_return(NULL, k_rx_err_spi_error);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_spi_error, err);
}

/* =============================================================================
 * Data Available Tests
 * =============================================================================
 */

void test_spi_comm_data_available_null_handle_fails(void)
{
  bool available;

  rx_err_t err = rx_spi_comm_data_available(NULL, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_data_available_null_available_fails(void)
{
  rx_spi_comm_init(&s_handle, NULL);

  rx_err_t err = rx_spi_comm_data_available(&s_handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_data_available_not_initialized_fails(void)
{
  bool available;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_data_available_empty(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  bool available = true;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(available);
}

void test_spi_comm_data_available_with_data(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_set_data_available(NULL, k_test_channel_default, true);
  bool available = false;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(available);
}

void test_spi_comm_data_available_hal_error_propagates(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_set_available_return(NULL, k_rx_err_spi_error);
  bool available;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_err_spi_error, err);
}

/* =============================================================================
 * Utility Function Tests
 * =============================================================================
 */

void test_spi_comm_reset_sequence(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  s_handle.tx_sequence = 100;
  s_handle.rx_sequence = 200;

  rx_spi_comm_reset_sequence(&s_handle);

  TEST_ASSERT_EQUAL_UINT16(0, s_handle.tx_sequence);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.rx_sequence);
}

void test_spi_comm_reset_sequence_null_handle(void)
{
  /* Should not crash on NULL */
  rx_spi_comm_reset_sequence(NULL);
}

void test_spi_comm_get_tx_sequence(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  s_handle.tx_sequence = 0xBEEF;

  uint16_t seq    = 0;
  rx_err_t result = rx_spi_comm_get_tx_sequence(&s_handle, &seq);

  TEST_ASSERT_EQUAL(k_rx_ok, result);
  TEST_ASSERT_EQUAL_UINT16(0xBEEF, seq);
}

void test_spi_comm_get_tx_sequence_null_handle(void)
{
  uint16_t seq    = 0;
  rx_err_t result = rx_spi_comm_get_tx_sequence(NULL, &seq);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, result);
}

void test_spi_comm_get_rx_sequence(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  s_handle.rx_sequence = 0xCAFE;

  uint16_t seq    = 0;
  rx_err_t result = rx_spi_comm_get_rx_sequence(&s_handle, &seq);

  TEST_ASSERT_EQUAL(k_rx_ok, result);
  TEST_ASSERT_EQUAL_UINT16(0xCAFE, seq);
}

void test_spi_comm_get_rx_sequence_null_handle(void)
{
  uint16_t seq    = 0;
  rx_err_t result = rx_spi_comm_get_rx_sequence(NULL, &seq);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, result);
}

/* =============================================================================
 * Sequence Number Tests
 * =============================================================================
 */

void test_spi_comm_sequence_starts_at_zero(void)
{
  rx_spi_comm_init(&s_handle, NULL);

  uint16_t tx_seq = 0xFFFF;
  uint16_t rx_seq = 0xFFFF;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_get_tx_sequence(&s_handle, &tx_seq));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_get_rx_sequence(&s_handle, &rx_seq));
  TEST_ASSERT_EQUAL_UINT16(0, tx_seq);
  TEST_ASSERT_EQUAL_UINT16(0, rx_seq);
}

void test_spi_comm_sequence_max_value(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  s_handle.tx_sequence = k_test_sequence_max;
  s_handle.rx_sequence = k_test_sequence_max;

  uint16_t tx_seq = 0;
  uint16_t rx_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_get_tx_sequence(&s_handle, &tx_seq));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_get_rx_sequence(&s_handle, &rx_seq));
  TEST_ASSERT_EQUAL_UINT16(k_test_sequence_max, tx_seq);
  TEST_ASSERT_EQUAL_UINT16(k_test_sequence_max, rx_seq);
}

void test_spi_comm_rx_sequence_wraparound(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  /* Receive a frame with sequence 0xFFFF */
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command,
                              k_test_sequence_max,
                              NULL,
                              0,
                              encoded_frame,
                              &encoded_len);
  mock_rspi_inject_rx_data(NULL, k_test_channel_default, encoded_frame, encoded_len);

  rx_frame_t frame;
  rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* RX sequence should wrap to 0 (0xFFFF + 1 = 0x0000) */
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.rx_sequence);
}

/* =============================================================================
 * Channel Configuration Tests
 * =============================================================================
 */

void test_spi_comm_uses_configured_channel(void)
{
  rx_spi_comm_config_t config = {.channel     = k_test_channel_alt,
                                 .spi_mode    = 0,
                                 .fec_enabled = false};
  rx_spi_comm_init(&s_handle, &config);
  helper_init_rspi_channel(k_test_channel_alt);
  uint8_t data[] = "test";

  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  /* Verify transfer was called on channel 1 */
  mock_rspi_call_t call;
  rx_err_t         err = mock_rspi_get_last_call(NULL, "rspi_peripheral_transfer", &call);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_channel_alt, call.channel);
}

/* =============================================================================
 * Buffer Size Tests
 * =============================================================================
 */

void test_spi_comm_buffer_size_constants(void)
{
  /* Verify buffer size constants are reasonable */
  TEST_ASSERT_EQUAL(2048, k_spi_comm_rx_buffer_size);
  TEST_ASSERT_EQUAL(2048, k_spi_comm_tx_buffer_size);
  TEST_ASSERT_GREATER_OR_EQUAL(k_frame_max_size, k_spi_comm_tx_buffer_size);
}

void test_spi_comm_max_payload_fits_in_buffer(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);

  /* Create maximum size payload */
  uint8_t data[k_frame_max_payload];
  memset(data, 0xAA, k_frame_max_payload);

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, k_frame_max_payload);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Mock Verification Tests
 * =============================================================================
 */

void test_spi_comm_transfer_is_called_on_send(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_clear_calls(NULL);
  uint8_t data[] = "test";

  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_TRUE(mock_rspi_was_called(NULL, "rspi_peripheral_transfer"));
}

void test_spi_comm_available_is_called_on_receive(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  mock_rspi_clear_calls(NULL);
  rx_frame_t frame;

  rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_TRUE(mock_rspi_was_called(NULL, "rspi_peripheral_read_available"));
}

void test_spi_comm_transfer_count(void)
{
  rx_spi_comm_init(&s_handle, NULL);
  helper_init_rspi_channel(k_test_channel_default);
  uint8_t data[] = "test";

  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);
  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);
  rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  /* Each send calls transfer once */
  uint32_t count = mock_rspi_get_call_count(NULL, "rspi_peripheral_transfer");
  TEST_ASSERT_EQUAL_UINT32(3, count);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_spi_comm_init_null_handle_fails);
  RUN_TEST(test_spi_comm_init_success_default_config);
  RUN_TEST(test_spi_comm_init_with_custom_channel);
  RUN_TEST(test_spi_comm_init_with_fec_enabled);
  RUN_TEST(test_spi_comm_deinit_null_handle_fails);
  RUN_TEST(test_spi_comm_deinit_success);
  RUN_TEST(test_spi_comm_deinit_not_initialized_succeeds);

  /* Send tests */
  RUN_TEST(test_spi_comm_send_null_handle_fails);
  RUN_TEST(test_spi_comm_send_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_null_payload_with_len_fails);
  RUN_TEST(test_spi_comm_send_payload_too_large_fails);
  RUN_TEST(test_spi_comm_send_empty_payload_succeeds);
  RUN_TEST(test_spi_comm_send_with_payload_succeeds);
  RUN_TEST(test_spi_comm_send_increments_sequence);
  RUN_TEST(test_spi_comm_send_sequence_wraps);
  RUN_TEST(test_spi_comm_send_with_fec_flag);
  RUN_TEST(test_spi_comm_send_transfer_error_propagates);
  RUN_TEST(test_spi_comm_send_large_payload);
  RUN_TEST(test_spi_comm_send_missing_host_ack_times_out);

  /* Send ACK/NACK tests */
  RUN_TEST(test_spi_comm_send_ack_null_handle_fails);
  RUN_TEST(test_spi_comm_send_ack_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_ack_success);
  RUN_TEST(test_spi_comm_send_ack_transfer_error_propagates);
  RUN_TEST(test_spi_comm_send_nack_null_handle_fails);
  RUN_TEST(test_spi_comm_send_nack_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_nack_success);
  RUN_TEST(test_spi_comm_send_nack_transfer_error_propagates);

  /* Receive tests */
  RUN_TEST(test_spi_comm_receive_null_handle_fails);
  RUN_TEST(test_spi_comm_receive_null_frame_fails);
  RUN_TEST(test_spi_comm_receive_not_initialized_fails);
  RUN_TEST(test_spi_comm_receive_no_data_timeout);
  RUN_TEST(test_spi_comm_receive_available_check_error_propagates);
  RUN_TEST(test_spi_comm_receive_valid_frame_success);
  RUN_TEST(test_spi_comm_receive_updates_rx_sequence);
  RUN_TEST(test_spi_comm_receive_invalid_sync_word);
  RUN_TEST(test_spi_comm_receive_transfer_error_propagates);

  /* Data available tests */
  RUN_TEST(test_spi_comm_data_available_null_handle_fails);
  RUN_TEST(test_spi_comm_data_available_null_available_fails);
  RUN_TEST(test_spi_comm_data_available_not_initialized_fails);
  RUN_TEST(test_spi_comm_data_available_empty);
  RUN_TEST(test_spi_comm_data_available_with_data);
  RUN_TEST(test_spi_comm_data_available_hal_error_propagates);

  /* Utility function tests */
  RUN_TEST(test_spi_comm_reset_sequence);
  RUN_TEST(test_spi_comm_reset_sequence_null_handle);
  RUN_TEST(test_spi_comm_get_tx_sequence);
  RUN_TEST(test_spi_comm_get_tx_sequence_null_handle);
  RUN_TEST(test_spi_comm_get_rx_sequence);
  RUN_TEST(test_spi_comm_get_rx_sequence_null_handle);

  /* Sequence number tests */
  RUN_TEST(test_spi_comm_sequence_starts_at_zero);
  RUN_TEST(test_spi_comm_sequence_max_value);
  RUN_TEST(test_spi_comm_rx_sequence_wraparound);

  /* Channel configuration tests */
  RUN_TEST(test_spi_comm_uses_configured_channel);

  /* Buffer size tests */
  RUN_TEST(test_spi_comm_buffer_size_constants);
  RUN_TEST(test_spi_comm_max_payload_fits_in_buffer);

  /* Mock verification tests */
  RUN_TEST(test_spi_comm_transfer_is_called_on_send);
  RUN_TEST(test_spi_comm_available_is_called_on_receive);
  RUN_TEST(test_spi_comm_transfer_count);

  return UNITY_END();
}
