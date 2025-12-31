/**
 * @file usb_comm_task.c
 * @brief ThreadX task for USB CDC Protocol Buffer communication
 *
 * Implements the USB communication task that handles telemetry and
 * configuration commands from RPi5 via USB CDC-ACM virtual serial port.
 *
 * Protocol Stack:
 * [USB CDC] → [Frame Layer + CRC] → [nanopb] → [Application Handlers]
 *
 * Supported Commands:
 * - GetTelemetryRequest: Request telemetry snapshot
 * - SetConfigRequest: Update configuration parameters
 * - GetStatusRequest: Get system status
 *
 * @note USB is lower priority than SPI - SPI handles real-time motor control
 *       while USB handles telemetry and configuration.
 *
 * @date 2025-12-30
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "tasks/usb_comm_task.h"

#include <string.h>

#include "hardware.h"
#include "rx_check.h"
#include "rx_frame.h"
#include "rx_log.h"
#include "rx_nanopb.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"

static const char* s_tag = "USB_COMM";

/* =============================================================================
 * Frame Types for Protocol Mapping
 * =============================================================================
 */

/**
 * @brief Message type identifiers for dispatching
 */
typedef enum {
  k_msg_type_velocity  = 0x01, /**< SetVelocityRequest */
  k_msg_type_estop     = 0x02, /**< EmergencyStopRequest */
  k_msg_type_telemetry = 0x03, /**< GetTelemetryRequest */
  k_msg_type_status    = 0x04, /**< GetStatusRequest */
  k_msg_type_config    = 0x05, /**< SetConfigRequest */
} msg_type_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static TX_THREAD s_usb_comm_thread;
static uint8_t   s_usb_comm_stack[k_usb_comm_task_stack_size];

static motor_shared_state_t* s_shared_state = NULL;
static rx_usb_comm_handle_t  s_usb_handle;

/** Communication statistics */
static struct {
  uint32_t frames_rx;
  uint32_t frames_tx;
  uint32_t crc_errors;
  uint32_t decode_errors;
} s_stats;

/* =============================================================================
 * Internal Command Handlers
 * =============================================================================
 */

/**
 * @brief Handle velocity command received over USB
 *
 * Note: Velocity commands are typically sent over SPI for real-time control,
 * but USB can also accept them for testing or fallback purposes.
 *
 * @param[in] payload Command payload
 * @param[in] len Payload length
 *
 * @return RX_OK on success
 */
static rx_err_t internal_handle_velocity_command(const uint8_t* payload, uint32_t len)
{
  star_v1_SetVelocityRequest request;
  rx_err_t                   err = rx_nanopb_decode_velocity_request(payload, len, &request);

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to decode velocity request");
    s_stats.decode_errors++;
    return err;
  }

  if (!request.has_command) {
    RX_LOG_WARN(s_tag, "Velocity request missing command");
    return RX_ERR_INVALID_ARG;
  }

  /* Extract velocities */
  float left_mps  = (float)request.command.left_velocity_mps;
  float right_mps = (float)request.command.right_velocity_mps;

  RX_LOG_DEBUG(s_tag, "USB Velocity cmd received");

  /* Update shared state for skid-steer (4 motors) */
  motor_shared_state_set_velocity(s_shared_state, k_motor_idx_front_left, left_mps);
  motor_shared_state_set_velocity(s_shared_state, k_motor_idx_front_right, right_mps);
  motor_shared_state_set_velocity(s_shared_state, k_motor_idx_rear_left, left_mps);
  motor_shared_state_set_velocity(s_shared_state, k_motor_idx_rear_right, right_mps);

  return RX_OK;
}

/**
 * @brief Handle emergency stop command
 *
 * @param[in] payload Command payload
 * @param[in] len Payload length
 *
 * @return RX_OK on success
 */
static rx_err_t internal_handle_estop_command(const uint8_t* payload, uint32_t len)
{
  star_v1_EmergencyStopRequest request;
  rx_err_t                     err = rx_nanopb_decode_estop_request(payload, len, &request);

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to decode estop request");
    s_stats.decode_errors++;
    return err;
  }

  RX_LOG_WARN(s_tag, "USB Emergency stop command received");

  /* Engage emergency stop */
  motor_shared_state_set_estop(s_shared_state, true);

  return RX_OK;
}

/**
 * @brief Send velocity response
 *
 * @param[in] status Response status
 *
 * @return RX_OK on success
 */
static rx_err_t internal_send_velocity_response(star_v1_Status status)
{
  star_v1_SetVelocityResponse response = star_v1_SetVelocityResponse_init_zero;

  response.has_header = true;
  rx_nanopb_create_response_header(&response.header, status, NULL);

  uint8_t buffer[RX_NANOPB_BUFFER_SIZE];
  size_t  len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_response(&response, buffer, &len);
  if (err != RX_OK) {
    return err;
  }

  err = rx_usb_comm_send(&s_usb_handle, k_frame_type_response, 0, buffer, (uint32_t)len);
  if (err == RX_OK) {
    s_stats.frames_tx++;
  }

  return err;
}

/**
 * @brief Send emergency stop response
 *
 * @param[in] engaged True if e-stop was engaged
 *
 * @return RX_OK on success
 */
static rx_err_t internal_send_estop_response(bool engaged)
{
  star_v1_EmergencyStopResponse response = star_v1_EmergencyStopResponse_init_zero;

  response.has_header = true;
  rx_nanopb_create_response_header(&response.header, star_v1_Status_STATUS_OK, NULL);
  response.estop_engaged = engaged;

  uint8_t buffer[RX_NANOPB_BUFFER_SIZE];
  size_t  len = 0;

  rx_err_t err = rx_nanopb_encode_estop_response(&response, buffer, &len);
  if (err != RX_OK) {
    return err;
  }

  err = rx_usb_comm_send(&s_usb_handle, k_frame_type_response, 0, buffer, (uint32_t)len);
  if (err == RX_OK) {
    s_stats.frames_tx++;
  }

  return err;
}

/* =============================================================================
 * Internal Task Functions
 * =============================================================================
 */

/**
 * @brief Process received frame and dispatch to handler
 *
 * @param[in] frame Decoded frame
 *
 * @return RX_OK on success
 */
static rx_err_t internal_process_frame(const rx_frame_t* frame)
{
  s_stats.frames_rx++;

  /* Check frame type */
  if (frame->header.type != k_frame_type_command) {
    /* Only process command frames */
    if (frame->header.type == k_frame_type_ack) {
      RX_LOG_DEBUG(s_tag, "Received ACK");
    }
    return RX_OK;
  }

  /* Determine message type from first byte of payload */
  if (frame->header.length < 1) {
    RX_LOG_WARN(s_tag, "Empty command payload");
    return RX_ERR_INVALID_SIZE;
  }

  uint8_t        msg_type     = frame->payload[0];
  const uint8_t* payload_data = &frame->payload[1];
  uint32_t       payload_len  = frame->header.length - 1;

  rx_err_t err = RX_OK;

  switch (msg_type) {
    case k_msg_type_velocity:
      err = internal_handle_velocity_command(payload_data, payload_len);
      if (err == RX_OK) {
        internal_send_velocity_response(star_v1_Status_STATUS_OK);
      } else {
        internal_send_velocity_response(star_v1_Status_STATUS_INVALID_REQUEST);
      }
      break;

    case k_msg_type_estop:
      err = internal_handle_estop_command(payload_data, payload_len);
      internal_send_estop_response(err == RX_OK);
      break;

    case k_msg_type_telemetry:
      /* Telemetry request */
      RX_LOG_DEBUG(s_tag, "Telemetry request received");
      /* TODO: Implement telemetry response */
      break;

    case k_msg_type_status:
      /* Status request */
      RX_LOG_DEBUG(s_tag, "Status request received");
      /* TODO: Implement status response */
      break;

    case k_msg_type_config:
      /* Configuration request */
      RX_LOG_DEBUG(s_tag, "Config request received");
      /* TODO: Implement config handling */
      break;

    default:
      RX_LOG_WARN(s_tag, "Unknown message type");
      err = RX_ERR_PROTOCOL_ERROR;
      break;
  }

  /* Send ACK if required */
  if (frame->header.flags & k_frame_flag_requires_ack) {
    rx_usb_comm_send_ack(&s_usb_handle, frame->header.sequence);
  }

  return err;
}

/**
 * @brief Main USB communication loop
 */
static void internal_usb_comm_loop(void)
{
  rx_frame_t frame;

  RX_LOG_INFO(s_tag, "USB comm loop started (50Hz)");

  while (true) {
    /* Sleep for polling period */
    tx_thread_sleep(k_usb_comm_task_period_ms / 10); /* Convert ms to ticks */

    /* Check if USB is configured */
    if (!rx_usb_is_configured()) {
      /* USB not connected - wait and retry */
      continue;
    }

    /* Check for incoming data */
    bool     available = false;
    rx_err_t err       = rx_usb_comm_data_available(&s_usb_handle, &available);

    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Failed to check USB data availability");
      continue;
    }

    if (!available) {
      continue;
    }

    /* Receive and decode frame */
    err = rx_usb_comm_receive(&s_usb_handle, &frame, 0);

    if (err == RX_ERR_CRC_MISMATCH) {
      RX_LOG_WARN(s_tag, "CRC mismatch - sending NACK");
      s_stats.crc_errors++;
      rx_usb_comm_send_nack(&s_usb_handle, frame.header.sequence, k_frame_flag_none);
      continue;
    }

    if (err != RX_OK) {
      if (err != RX_ERR_TIMEOUT) {
        RX_LOG_ERROR(s_tag, "Failed to receive frame");
      }
      continue;
    }

    /* Process frame */
    internal_process_frame(&frame);
  }
}

/**
 * @brief USB communication task entry point
 *
 * @param[in] input ThreadX task input (unused)
 */
static void internal_usb_comm_task(ULONG input)
{
  (void)input;

  RX_LOG_INFO(s_tag, "USB communication task started");

  /* Wait for USB enumeration */
  RX_LOG_INFO(s_tag, "Waiting for USB host connection...");
  while (!rx_usb_is_configured()) {
    tx_thread_sleep(10); /* 100ms at 100Hz tick */
  }
  RX_LOG_INFO(s_tag, "USB host connected");

  internal_usb_comm_loop();

  /* Should never reach here */
  RX_LOG_ERROR(s_tag, "USB comm task exited unexpectedly");
  tx_thread_delete(&s_usb_comm_thread);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t usb_comm_task_create(motor_shared_state_t* shared_state)
{
  RX_CHECK_NULL_PTR(shared_state, s_tag, "shared_state is NULL");

  s_shared_state = shared_state;

  /* Clear statistics */
  memset(&s_stats, 0, sizeof(s_stats));

  /* Initialize USB CDC driver */
  rx_usb_config_t usb_config = {0};
  rx_err_t        err        = rx_usb_init(&usb_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to initialize USB CDC");
    return err;
  }

  /* Initialize USB communication handle */
  rx_usb_comm_config_t comm_config = {
    .fec_enabled = 0, /* FEC disabled for USB */
  };

  err = rx_usb_comm_init(&s_usb_handle, &comm_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to initialize USB comm handle");
    rx_usb_deinit();
    return err;
  }

  /* Create USB communication thread */
  UINT status = tx_thread_create(&s_usb_comm_thread,
                                 "usb_comm",
                                 internal_usb_comm_task,
                                 0,
                                 s_usb_comm_stack,
                                 k_usb_comm_task_stack_size,
                                 k_usb_comm_task_priority,
                                 k_usb_comm_task_priority,
                                 TX_NO_TIME_SLICE,
                                 TX_AUTO_START);

  if (status != TX_SUCCESS) {
    rx_log_error_int(s_tag, "Failed to create USB comm thread, status", (int32_t)status);
    rx_usb_comm_deinit(&s_usb_handle);
    rx_usb_deinit();
    return RX_ERR_INVALID_STATE;
  }

  RX_LOG_INFO(s_tag, "USB communication task created");

  return RX_OK;
}

rx_err_t usb_comm_task_get_stats(uint32_t* frames_rx,
                                 uint32_t* frames_tx,
                                 uint32_t* crc_errors,
                                 uint32_t* decode_errors)
{
  if (frames_rx != NULL) {
    *frames_rx = s_stats.frames_rx;
  }
  if (frames_tx != NULL) {
    *frames_tx = s_stats.frames_tx;
  }
  if (crc_errors != NULL) {
    *crc_errors = s_stats.crc_errors;
  }
  if (decode_errors != NULL) {
    *decode_errors = s_stats.decode_errors;
  }

  return RX_OK;
}

void usb_comm_task_reset_stats(void)
{
  memset(&s_stats, 0, sizeof(s_stats));
}

bool usb_comm_task_is_connected(void)
{
  return rx_usb_is_configured();
}
