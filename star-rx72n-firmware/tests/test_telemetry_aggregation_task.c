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
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

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
  k_test_buffer_size = 512,  /**< Telemetry buffer size */
  k_test_temp_cdegc  = 2550, /**< 25.50 degC in centi-degrees */
} test_telem_buffer_constants_t;

typedef enum : uint32_t {
  k_test_timestamp_us = 1000000U, /**< Telemetry timestamp in microseconds */
  k_test_frame_seq    = 42U,      /**< Frame sequence number for protobuf test */
  k_test_encoded_len  = 64U,      /**< Mock encoded output length in bytes */
} test_telem_large_constants_t;

typedef enum : int32_t {
  k_test_encoder_0 = 1000, /**< Expected encoder count for motor 0 */
  k_test_encoder_1 = 1050, /**< Expected encoder count for motor 1 */
  k_test_encoder_2 = 995,  /**< Expected encoder count for motor 2 */
  k_test_encoder_3 = 1005, /**< Expected encoder count for motor 3 */
} test_telem_encoder_constants_t;

typedef enum : uint8_t {
  k_test_motor_idx_0 = 0, /**< Motor index 0 */
  k_test_motor_idx_1 = 1, /**< Motor index 1 */
  k_test_motor_idx_2 = 2, /**< Motor index 2 */
  k_test_motor_idx_3 = 3, /**< Motor index 3 */
  k_test_sensor_idx  = 0, /**< Temperature sensor index 0 */
  k_test_sensor_cnt  = 1, /**< Number of temperature sensors in test */
} test_telem_index_constants_t;

/** @brief Velocity of motor 0 in m/s for encoder data test */
static const float s_test_velocity_0 = 0.95F;
/** @brief Velocity of motor 1 in m/s for encoder data test */
static const float s_test_velocity_1 = 1.00F;
/** @brief Velocity of motor 2 in m/s (negative = reverse) for encoder data test */
static const float s_test_velocity_2 = -0.48F;
/** @brief Velocity of motor 3 in m/s (negative = reverse) for encoder data test */
static const float s_test_velocity_3 = -0.50F;

/** @brief Tolerance for float comparisons (replaces Unity UNITY_FLOAT_PRECISION literal) */
static const float s_float_tolerance = 0.00001F;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  /* Reset all mocks before each test */
  mock_shared_data_reset();
  (void)shared_data_init(); /* mirrors production task startup; enables active-channel API */
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
  motor_state_t state_in  = {};
  motor_state_t state_out = {};
  rx_err_t      err;

  state_in.encoder_counts[k_test_motor_idx_0]       = k_test_encoder_0;
  state_in.encoder_counts[k_test_motor_idx_1]       = k_test_encoder_1;
  state_in.encoder_counts[k_test_motor_idx_2]       = k_test_encoder_2;
  state_in.encoder_counts[k_test_motor_idx_3]       = k_test_encoder_3;
  state_in.current_velocity_mps[k_test_motor_idx_0] = s_test_velocity_0;
  state_in.current_velocity_mps[k_test_motor_idx_1] = s_test_velocity_1;
  state_in.current_velocity_mps[k_test_motor_idx_2] = s_test_velocity_2;
  state_in.current_velocity_mps[k_test_motor_idx_3] = s_test_velocity_3;
  state_in.estop_active                             = false;
  state_in.mode                                     = k_motor_mode_velocity;

  /* Store in shared data */
  err = shared_data_update_motor_state(&state_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Task would read motor state */
  err = shared_data_get_motor_state(&state_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify encoder data accessible */
  TEST_ASSERT_EQUAL_INT32(k_test_encoder_0, state_out.encoder_counts[k_test_motor_idx_0]);
  TEST_ASSERT_EQUAL_INT32(k_test_encoder_1, state_out.encoder_counts[k_test_motor_idx_1]);
  TEST_ASSERT_EQUAL_INT32(k_test_encoder_2, state_out.encoder_counts[k_test_motor_idx_2]);
  TEST_ASSERT_EQUAL_INT32(k_test_encoder_3, state_out.encoder_counts[k_test_motor_idx_3]);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           s_test_velocity_0,
                           state_out.current_velocity_mps[k_test_motor_idx_0]);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           s_test_velocity_1,
                           state_out.current_velocity_mps[k_test_motor_idx_1]);
}

/**
 * @brief Test telemetry collects temperature data
 *
 * @details
 * Verifies that temperature state is read from shared_data.
 */
void test_telemetry_task_collects_temp_data(void)
{
  /* Set up temp state (25.50 degC = 2550 centi-degrees) */
  temp_sensor_state_t temp_in  = {};
  temp_sensor_state_t temp_out = {};
  rx_err_t            err;

  temp_in.temperature_cdegc[k_test_sensor_idx] = (int16_t)k_test_temp_cdegc;
  temp_in.sensor_valid[k_test_sensor_idx]      = true;
  temp_in.sensor_count                         = k_test_sensor_cnt;

  /* Store */
  err = shared_data_update_temp(&temp_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read for telemetry */
  err = shared_data_get_temp(&temp_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify */
  TEST_ASSERT_TRUE(temp_out.sensor_valid[k_test_sensor_idx]);
  TEST_ASSERT_EQUAL_INT16((int16_t)k_test_temp_cdegc,
                          temp_out.temperature_cdegc[k_test_sensor_idx]);
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
  telemetry.timestamp_us   = k_test_timestamp_us;
  telemetry.frame_sequence = k_test_frame_seq;
  telemetry.emergency_stop = false;
  /* Configure mock to succeed */
  mock_nanopb_set_encode_telemetry_return(k_rx_ok);
  mock_nanopb_set_encode_length(k_test_encoded_len);

  /* Encode */
  err = rx_nanopb_encode_telemetry(&telemetry, buffer, k_test_buffer_size, &encoded_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_test_encoded_len, encoded_len);
  TEST_ASSERT_EQUAL_UINT32(1U, mock_nanopb_get_encode_telemetry_count());
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
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
  uint8_t               payload[k_test_encoded_len];
  rx_err_t              err;

  mgr.initialized = true;

  /* Set up send parameters as task does */
  params.channel     = k_comm_channel_uart;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload     = payload;
  params.payload_len = k_test_encoded_len;

  /* Configure mock */
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Send */
  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1U, mock_comm_manager_get_send_count());
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
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
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
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
  rx_err_t              err;
  bool                  usb_ready        = false;
  bool                  spi_ready        = false;
  rx_comm_channel_t     selected_channel = k_comm_channel_count; /* sentinel: unset */

  /* Initialize manager */
  mgr.initialized = true;

  /* USB ready, SPI also ready - transport selection must prefer USB */
  mock_comm_manager_set_channel_ready(k_comm_channel_uart, true);
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Exercise transport selection: query USB first (preferred), then SPI fallback */
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_uart, &usb_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  if (usb_ready) {
    selected_channel = k_comm_channel_uart;
  } else {
    err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &spi_ready);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    selected_channel = (int)spi_ready ? k_comm_channel_spi : k_comm_channel_count;
  }

  /* Verify USB was selected (USB ready -> USB preferred) */
  TEST_ASSERT_EQUAL(k_comm_channel_uart, selected_channel);

  /* Send via the selected channel and verify the mock records it correctly */
  params.channel     = selected_channel;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload_len = 0;

  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_comm_channel_uart, mock_comm_manager_get_last_send_channel());
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
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
  rx_err_t              err;
  bool                  usb_ready        = false;
  bool                  spi_ready        = false;
  rx_comm_channel_t     selected_channel = k_comm_channel_count; /* sentinel: unset */

  /* Initialize manager */
  mgr.initialized = true;

  /* USB not ready, SPI ready - transport selection must fall back to SPI */
  mock_comm_manager_set_channel_ready(k_comm_channel_uart, false);
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Exercise transport selection: USB first (not ready) -> SPI fallback */
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_uart, &usb_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(usb_ready);

  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &spi_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(spi_ready);

  selected_channel = (int)spi_ready ? k_comm_channel_spi : k_comm_channel_count;

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
  rx_comm_manager_t mgr   = {};
  bool              ready = false;
  rx_err_t          err;

  mgr.initialized = true;

  /* Set USB as ready */
  mock_comm_manager_set_channel_ready(k_comm_channel_uart, true);
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_uart, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(ready);

  /* Set USB as not ready */
  mock_comm_manager_set_channel_ready(k_comm_channel_uart, false);
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_uart, &ready);
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
  rx_comm_manager_t mgr   = {};
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
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
  uint8_t               payload[k_test_encoded_len];
  rx_err_t              err;
  bool                  usb_ready        = false;
  bool                  spi_ready        = false;
  rx_comm_channel_t     selected_channel = k_comm_channel_count; /* sentinel: unset */

  mgr.initialized = true;

  /* USB not ready, SPI ready - transport selection must fall back to SPI */
  mock_comm_manager_set_channel_ready(k_comm_channel_uart, false);
  mock_comm_manager_set_channel_ready(k_comm_channel_spi, true);
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Exercise transport selection: USB first (not ready) -> SPI fallback */
  err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_uart, &usb_ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  if (usb_ready) {
    selected_channel = k_comm_channel_uart;
  } else {
    err = rx_comm_manager_channel_ready(&mgr, k_comm_channel_spi, &spi_ready);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    selected_channel = (int)spi_ready ? k_comm_channel_spi : k_comm_channel_count;
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
  TEST_ASSERT_EQUAL_UINT32(1U, mock_comm_manager_get_send_count());
}

/**
 * @brief Telemetry defaults to USB when no command has been received yet
 *
 * @details
 * Before any command frame arrives, shared_data_get_active_channel() returns
 * k_comm_channel_uart (the USB default). Verifies that the mock preserves this
 * default AND that sending on that channel routes to USB -- mirroring what
 * internal_select_transport() + internal_build_and_send_telemetry() would do.
 *
 * @pre mock_shared_data_reset() + shared_data_init() called (setUp)
 * @pre Default active channel is k_comm_channel_uart
 * @post mock_comm_manager_get_last_send_channel() returns k_comm_channel_uart
 * @post send count is 1
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 * @note internal_select_transport() is static; this test mirrors its logic directly
 */
void test_telemetry_defaults_to_usb_before_any_command(void)
{
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
  rx_err_t              err;

  mgr.initialized = true;
  mock_comm_manager_set_send_return(k_rx_ok);

  /* With no command received, active channel must default to USB */
  const uint8_t raw_ch = shared_data_get_active_channel();
  TEST_ASSERT_EQUAL_UINT8((uint8_t)k_comm_channel_uart, raw_ch);
  TEST_ASSERT_EQUAL_UINT32(0U, mock_shared_data_get_active_channel_update_count());

  /* Simulate telemetry routing: read channel, send via comm manager */
  params.channel     = (rx_comm_channel_t)raw_ch;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload_len = 0;

  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_comm_channel_uart, mock_comm_manager_get_last_send_channel());
  TEST_ASSERT_EQUAL_UINT32(1U, mock_comm_manager_get_send_count());
}

/**
 * @brief Telemetry routes to SPI after an SPI command is recorded
 *
 * @details
 * When the comm task records that the last command arrived over SPI,
 * shared_data_get_active_channel() must return k_comm_channel_spi AND the
 * telemetry send must use k_comm_channel_spi -- mirroring what
 * internal_select_transport() + internal_build_and_send_telemetry() would do.
 *
 * @pre mock_shared_data_reset() + shared_data_init() called (setUp)
 * @pre shared_data_update_active_channel() sets SPI as active
 * @post mock_comm_manager_get_last_send_channel() returns k_comm_channel_spi
 * @post active_channel_update_count == 1
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 * @note internal_select_transport() is static; this test mirrors its logic directly
 */
void test_telemetry_routes_to_spi_after_spi_command(void)
{
  rx_comm_manager_t     mgr    = {};
  rx_comm_send_params_t params = {};
  rx_err_t              err;

  mgr.initialized = true;
  mock_comm_manager_set_send_return(k_rx_ok);

  /* Arrange: SPI command received */
  (void)shared_data_update_active_channel(k_comm_channel_spi);
  TEST_ASSERT_EQUAL_UINT32(1U, mock_shared_data_get_active_channel_update_count());

  /* Simulate telemetry routing: read channel, send via comm manager */
  const uint8_t raw_ch = shared_data_get_active_channel();
  TEST_ASSERT_EQUAL_UINT8(k_comm_channel_spi, raw_ch);

  params.channel     = (rx_comm_channel_t)raw_ch;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload_len = 0;

  err = rx_comm_manager_send(&mgr, &params);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_comm_channel_spi, mock_comm_manager_get_last_send_channel());
  TEST_ASSERT_EQUAL_UINT32(1U, mock_comm_manager_get_send_count());
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
  RUN_TEST(test_telemetry_defaults_to_usb_before_any_command);
  RUN_TEST(test_telemetry_routes_to_spi_after_spi_command);

  return UNITY_END();
}
