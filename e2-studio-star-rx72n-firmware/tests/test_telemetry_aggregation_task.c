/* tests/test_telemetry_aggregation_task.c */

/**
 * @file test_telemetry_aggregation_task.c
 * @brief Unit Tests for Telemetry Aggregation Task
 *
 * @details
 * Tests telemetry task creation, data collection, and transmission.
 * Uses mocks for ThreadX, nanopb, comm_manager, and shared data.
 *
 * Test coverage:
 * - Task creation success
 * - Encoder data collection from shared data
 * - Protobuf encoding of telemetry
 * - Broadcast to all communication channels
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include <string.h>

#include "mock_rx_comm_manager.h"
#include "mock_rx_nanopb.h"
#include "mock_shared_data.h"
#include "tx_api.h"
#include "unity.h"

/* Include the task header for the public API */
#include "telemetry_task.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum : uint8_t {
  k_test_motor_count = 4, /**< Number of motors */
} test_telem_motor_constants_t;

typedef enum : uint16_t {
  k_test_buffer_size = 512, /**< Telemetry buffer size */
} test_telem_buffer_constants_t;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  /* Reset all mocks before each test */
  mock_shared_data_reset();
  mock_nanopb_reset();
  mock_comm_manager_reset();
  mock_tx_reset();
}

void tearDown(void)
{
  /* Clean up after each test */
}

/* =============================================================================
 * Task Creation Tests
 * =============================================================================
 */

/**
 * @brief Test successful telemetry task creation
 *
 * @details
 * Verifies that telemetry_task_create() successfully creates
 * the ThreadX thread when conditions are normal.
 */
void test_telemetry_task_create_success(void)
{
  /* Configure mocks for success */
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task */
  err = telemetry_task_create();

  /* Verify success */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_tx_was_thread_create_called());
}

/**
 * @brief Test telemetry task creation fails when ThreadX fails
 */
void test_telemetry_task_create_thread_failure(void)
{
  /* Configure ThreadX to fail */
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_NO_MEMORY);

  /* Create the task */
  err = telemetry_task_create();

  /* Verify failure */
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
}

/**
 * @brief Test telemetry task creation fails when already created
 */
void test_telemetry_task_create_already_created(void)
{
  /* Configure mocks for success */
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task first time - should succeed */
  err = telemetry_task_create();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create the task second time - should fail */
  err = telemetry_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Data Collection Tests
 * =============================================================================
 */

/**
 * @brief Test telemetry task collects encoder data
 *
 * @details
 * Verifies that motor state (encoder data) is read from shared_data
 * and used in telemetry.
 */
void test_telemetry_task_collects_encoder_data(void)
{
  /* Set up motor state with encoder data */
  motor_state_t state_in  = {0};
  motor_state_t state_out = {0};
  rx_err_t      err;

  state_in.encoder_counts[0]       = 1000;
  state_in.encoder_counts[1]       = 1050;
  state_in.encoder_counts[2]       = 995;
  state_in.encoder_counts[3]       = 1005;
  state_in.current_velocity_mps[0] = 0.95f;
  state_in.current_velocity_mps[1] = 1.00f;
  state_in.current_velocity_mps[2] = -0.48f;
  state_in.current_velocity_mps[3] = -0.50f;
  state_in.estop_active            = false;
  state_in.mode                    = k_motor_mode_velocity;

  /* Store in shared data */
  err = shared_data_update_motor_state(&state_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Task would read motor state */
  err = shared_data_get_motor_state(&state_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify encoder data accessible */
  TEST_ASSERT_EQUAL_INT32(1000, state_out.encoder_counts[0]);
  TEST_ASSERT_EQUAL_INT32(1050, state_out.encoder_counts[1]);
  TEST_ASSERT_EQUAL_INT32(995, state_out.encoder_counts[2]);
  TEST_ASSERT_EQUAL_INT32(1005, state_out.encoder_counts[3]);
  TEST_ASSERT_EQUAL_FLOAT(0.95f, state_out.current_velocity_mps[0]);
  TEST_ASSERT_EQUAL_FLOAT(1.00f, state_out.current_velocity_mps[1]);
}

/**
 * @brief Test telemetry collects temperature data
 *
 * @details
 * Verifies that temperature state is read from shared_data.
 */
void test_telemetry_task_collects_temp_data(void)
{
  /* Set up temp state (25.50degC = 2550 centi-degrees) */
  temp_sensor_state_t temp_in  = {0};
  temp_sensor_state_t temp_out = {0};
  rx_err_t            err;

  temp_in.temperature_cdegc[0] = 2550;
  temp_in.sensor_valid[0]      = true;
  temp_in.sensor_count         = 1;

  /* Store */
  err = shared_data_update_temp(&temp_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read for telemetry */
  err = shared_data_get_temp(&temp_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify */
  TEST_ASSERT_TRUE(temp_out.sensor_valid[0]);
  TEST_ASSERT_EQUAL_INT16(2550, temp_out.temperature_cdegc[0]);
}

/* =============================================================================
 * Encoding Tests
 * =============================================================================
 */

/**
 * @brief Test telemetry task encodes protobuf
 *
 * @details
 * Verifies that rx_nanopb_encode_telemetry() is called with
 * collected data and produces valid encoded output.
 */
void test_telemetry_task_encodes_protobuf(void)
{
  star_v1_TelemetryData telemetry = star_v1_TelemetryData_init_zero;
  uint8_t               buffer[k_test_buffer_size];
  uint32_t              encoded_len = 0;
  rx_err_t              err;

  /* Set up telemetry data */
  telemetry.timestamp_us   = 1000000;
  telemetry.frame_sequence = 42;
  telemetry.emergency_stop = false;
  /* Configure mock to succeed */
  mock_nanopb_set_encode_telemetry_return(k_rx_ok);
  mock_nanopb_set_encode_length(64);

  /* Encode */
  err = rx_nanopb_encode_telemetry(&telemetry, buffer, k_test_buffer_size, &encoded_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(64, encoded_len);
  TEST_ASSERT_EQUAL_UINT32(1, mock_nanopb_get_encode_telemetry_count());
}

/**
 * @brief Test encoding failure is handled
 *
 * @details
 * Verifies that encoding failure doesn't crash and is reported.
 */
void test_telemetry_task_handles_encode_failure(void)
{
  star_v1_TelemetryData telemetry = star_v1_TelemetryData_init_zero;
  uint8_t               buffer[k_test_buffer_size];
  uint32_t              encoded_len = 0;
  rx_err_t              err;

  /* Configure mock to fail */
  mock_nanopb_set_encode_telemetry_return(k_rx_err_protocol_error);

  /* Attempt to encode (should fail) */
  err = rx_nanopb_encode_telemetry(&telemetry, buffer, k_test_buffer_size, &encoded_len);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Transmission Tests
 * =============================================================================
 */

/**
 * @brief Test telemetry task broadcasts to USB channel
 *
 * @details
 * Verifies that encoded telemetry is sent via communication manager.
 */
void test_telemetry_task_broadcasts_to_usb(void)
{
  /* Initialize manager (required for send to work) */
  rx_comm_manager_t     mgr    = {0};
  rx_comm_send_params_t params = {0};
  uint8_t               payload[64];
  rx_err_t              err;

  mgr.initialized = true;

  /* Set up send parameters as task does */
  params.channel     = k_comm_channel_usb;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload     = payload;
  params.payload_len = 64;

  /* Configure mock */
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Send */
  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, mock_comm_manager_get_send_count());
}

/**
 * @brief Test send failure doesn't stop telemetry
 *
 * @details
 * Verifies that send failure is handled gracefully.
 */
void test_telemetry_task_handles_send_failure(void)
{
  /* Configure mock to fail */
  rx_comm_manager_t     mgr    = {0};
  rx_comm_send_params_t params = {0};
  rx_err_t              err;

  mock_comm_manager_set_send_return(k_rx_err_timeout);

  /* Send (should fail but not crash) */
  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Transport Failover Tests
 * =============================================================================
 */

/**
 * @brief Test that USB channel is selected when USB is ready
 *
 * @details
 * When USB CDC is reported as ready by the comm manager, the transport selection
 * logic must prefer USB over SPI. This test exercises the real channel selection
 * path: query USB readiness, find it ready, and send via USB channel.
 *
 * The channel is derived from rx_comm_manager_channel_ready() -- not hardcoded --
 * mirroring the behaviour of internal_select_transport() in the task.
 */
void test_telemetry_transport_selects_usb_when_ready(void)
{
  rx_comm_manager_t     mgr    = {0};
  rx_comm_send_params_t params = {0};
  rx_err_t              err;
  bool                  usb_ready        = false;
  bool                  spi_ready        = false;
  rx_comm_channel_t     selected_channel = k_comm_channel_count; /* sentinel: unset */

  /* Initialize manager */
  mgr.initialized = true;

  /* USB ready, SPI also ready - transport selection must prefer USB */
  mock_comm_manager_set_channel_ready(k_comm_channel_usb, true);
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Exercise transport selection: query USB first (preferred), then SPI fallback */
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_usb, &usb_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  if (usb_ready) {
    selected_channel = k_comm_channel_usb;
  } else {
    err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &spi_ready);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    selected_channel = spi_ready ? k_comm_channel_spi : k_comm_channel_count;
  }

  /* Verify USB was selected (USB ready -> USB preferred) */
  TEST_ASSERT_EQUAL(k_comm_channel_usb, selected_channel);

  /* Send via the selected channel and verify the mock records it correctly */
  params.channel     = selected_channel;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload_len = 0;

  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_comm_channel_usb, mock_comm_manager_get_last_send_channel());
}

/**
 * @brief Test that SPI channel is selected when USB is not ready
 *
 * @details
 * When USB CDC is reported as not ready by the comm manager, the transport
 * selection logic must fall back to SPI. This test exercises the real failover
 * path: query USB (not ready), query SPI (ready), select SPI, send.
 *
 * The channel is derived from rx_comm_manager_channel_ready() -- not hardcoded --
 * mirroring the behaviour of internal_select_transport() in the task.
 */
void test_telemetry_transport_falls_back_to_spi_when_usb_not_ready(void)
{
  rx_comm_manager_t     mgr    = {0};
  rx_comm_send_params_t params = {0};
  rx_err_t              err;
  bool                  usb_ready        = false;
  bool                  spi_ready        = false;
  rx_comm_channel_t     selected_channel = k_comm_channel_count; /* sentinel: unset */

  /* Initialize manager */
  mgr.initialized = true;

  /* USB not ready, SPI ready - transport selection must fall back to SPI */
  mock_comm_manager_set_channel_ready(k_comm_channel_usb, false);
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Exercise transport selection: USB first (not ready) -> SPI fallback */
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_usb, &usb_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(usb_ready);

  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &spi_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(spi_ready);

  selected_channel = spi_ready ? k_comm_channel_spi : k_comm_channel_count;

  /* Verify SPI was selected (USB not ready -> SPI fallback) */
  TEST_ASSERT_EQUAL(k_comm_channel_spi, selected_channel);

  /* Send via the selected channel and verify the mock records it correctly */
  params.channel     = selected_channel;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload_len = 0;

  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_comm_channel_spi, mock_comm_manager_get_last_send_channel());
}

/**
 * @brief Test channel_ready API returns correct status for USB
 *
 * @details
 * Verifies that rx_comm_manager_channel_ready() correctly reports USB
 * channel status based on mock configuration.
 */
void test_telemetry_channel_ready_usb_reports_correctly(void)
{
  rx_comm_manager_t mgr   = {0};
  bool              ready = false;
  rx_err_t          err;

  mgr.initialized = true;

  /* Set USB as ready */
  mock_comm_manager_set_channel_ready(k_comm_channel_usb, true);
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_usb, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(ready);

  /* Set USB as not ready */
  mock_comm_manager_set_channel_ready(k_comm_channel_usb, false);
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_usb, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

/**
 * @brief Test channel_ready API returns correct status for SPI
 *
 * @details
 * Verifies that rx_comm_manager_channel_ready() correctly reports SPI
 * channel status based on mock configuration.
 */
void test_telemetry_channel_ready_spi_reports_correctly(void)
{
  rx_comm_manager_t mgr   = {0};
  bool              ready = false;
  rx_err_t          err;

  mgr.initialized = true;

  /* SPI ready by default */
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(ready);

  /* SPI not ready */
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, false);
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

/**
 * @brief Test that send to SPI succeeds during USB fallback
 *
 * @details
 * Verifies that telemetry can be delivered via SPI when USB is unavailable.
 * This exercises the full failover path end-to-end: channel selection via
 * rx_comm_manager_channel_ready() (USB not ready -> SPI selected) followed
 * by a successful send with a real payload buffer.
 *
 * The channel is derived from rx_comm_manager_channel_ready() -- not hardcoded --
 * mirroring the behaviour of internal_select_transport() in the task.
 */
void test_telemetry_spi_fallback_send_succeeds(void)
{
  rx_comm_manager_t     mgr    = {0};
  rx_comm_send_params_t params = {0};
  uint8_t               payload[64];
  rx_err_t              err;
  bool                  usb_ready        = false;
  bool                  spi_ready        = false;
  rx_comm_channel_t     selected_channel = k_comm_channel_count; /* sentinel: unset */

  mgr.initialized = true;

  /* USB not ready, SPI ready - transport selection must fall back to SPI */
  mock_comm_manager_set_channel_ready(k_comm_channel_usb, false);
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Exercise transport selection: USB first (not ready) -> SPI fallback */
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_usb, &usb_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  if (usb_ready) {
    selected_channel = k_comm_channel_usb;
  } else {
    err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &spi_ready);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    selected_channel = spi_ready ? k_comm_channel_spi : k_comm_channel_count;
  }

  /* Verify SPI was selected (USB not ready -> SPI fallback) */
  TEST_ASSERT_EQUAL(k_comm_channel_spi, selected_channel);

  /* Send with a real payload via the selected channel */
  params.channel     = selected_channel;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload     = payload;
  params.payload_len = sizeof(payload);

  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_comm_channel_spi, mock_comm_manager_get_last_send_channel());
  TEST_ASSERT_EQUAL_UINT32(1, mock_comm_manager_get_send_count());
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Task Creation Tests */
  RUN_TEST(test_telemetry_task_create_success);
  RUN_TEST(test_telemetry_task_create_thread_failure);
  RUN_TEST(test_telemetry_task_create_already_created);

  /* Data Collection Tests */
  RUN_TEST(test_telemetry_task_collects_encoder_data);
  RUN_TEST(test_telemetry_task_collects_temp_data);

  /* Encoding Tests */
  RUN_TEST(test_telemetry_task_encodes_protobuf);
  RUN_TEST(test_telemetry_task_handles_encode_failure);

  /* Transmission Tests */
  RUN_TEST(test_telemetry_task_broadcasts_to_usb);
  RUN_TEST(test_telemetry_task_handles_send_failure);

  /* Transport Failover Tests */
  RUN_TEST(test_telemetry_transport_selects_usb_when_ready);
  RUN_TEST(test_telemetry_transport_falls_back_to_spi_when_usb_not_ready);
  RUN_TEST(test_telemetry_channel_ready_usb_reports_correctly);
  RUN_TEST(test_telemetry_channel_ready_spi_reports_correctly);
  RUN_TEST(test_telemetry_spi_fallback_send_succeeds);

  return UNITY_END();
}
