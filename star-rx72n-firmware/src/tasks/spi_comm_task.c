/**
 * @file spi_comm_task.c
 * @brief ThreadX task for SPI Protocol Buffer communication
 *
 * Implements the 100Hz SPI communication task that handles command reception
 * from RPi5 and response transmission. Integrates frame layer, nanopb, and
 * motor control shared state.
 *
 * Protocol Stack:
 * [SPI HAL] → [Frame Layer + CRC] → [nanopb] → [Application Handlers]
 *
 * Supported Commands:
 * - SetVelocityRequest: Set wheel velocities (dispatched to motor control)
 * - EmergencyStopRequest: Engage emergency stop
 * - GetTelemetryRequest: Request telemetry snapshot
 *
 * @date 2025-12-27
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "tasks/spi_comm_task.h"

#include <string.h>

#include "hardware.h"
#include "rx_check.h"
#include "rx_frame.h"
#include "rx_log.h"
#include "rx_nanopb.h"
#include "rx_spi_comm.h"

static const char *s_tag = "SPI_COMM";

/* =============================================================================
 * Frame Types for Protocol Mapping
 * =============================================================================
 */

/**
 * @brief Message type identifiers for dispatching
 *
 * These match the frame type field for identifying command types.
 * In practice, you may encode the message type in the payload or use
 * separate frame types for each message.
 */
typedef enum {
    k_msg_type_velocity = 0x01,   /**< SetVelocityRequest */
    k_msg_type_estop    = 0x02,   /**< EmergencyStopRequest */
    k_msg_type_telemetry = 0x03,  /**< GetTelemetryRequest */
} msg_type_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static TX_THREAD s_spi_comm_thread;
static uint8_t   s_spi_comm_stack[k_spi_comm_task_stack_size];

static motor_shared_state_t *s_shared_state = NULL;
static rx_spi_comm_handle_t  s_spi_handle;

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
 * @brief Handle SetVelocityRequest command
 *
 * Decodes velocity command and updates shared state setpoints.
 *
 * @param[in] payload Command payload
 * @param[in] len Payload length
 *
 * @return RX_OK on success
 */
static rx_err_t internal_handle_velocity_command(const uint8_t *payload,
                                                  size_t len)
{
    star_v1_SetVelocityRequest request;
    rx_err_t err = rx_nanopb_decode_velocity_request(payload, len, &request);

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

    RX_LOG_DEBUG(s_tag, "Velocity cmd: L/R in m/s");

    /* Update shared state for skid-steer (4 motors) */
    /* Left motors: indices 0 and 2 */
    /* Right motors: indices 1 and 3 */
    motor_shared_state_set_velocity(s_shared_state, 0, left_mps);
    motor_shared_state_set_velocity(s_shared_state, 1, right_mps);
    motor_shared_state_set_velocity(s_shared_state, 2, left_mps);
    motor_shared_state_set_velocity(s_shared_state, 3, right_mps);

    return RX_OK;
}

/**
 * @brief Handle EmergencyStopRequest command
 *
 * Engages emergency stop via shared state.
 *
 * @param[in] payload Command payload
 * @param[in] len Payload length
 *
 * @return RX_OK on success
 */
static rx_err_t internal_handle_estop_command(const uint8_t *payload,
                                              size_t len)
{
    star_v1_EmergencyStopRequest request;
    rx_err_t err = rx_nanopb_decode_estop_request(payload, len, &request);

    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Failed to decode estop request");
        s_stats.decode_errors++;
        return err;
    }

    RX_LOG_WARN(s_tag, "Emergency stop command received");

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
    size_t len = 0;

    rx_err_t err = rx_nanopb_encode_velocity_response(&response, buffer, &len);
    if (err != RX_OK) {
        return err;
    }

    err = rx_spi_comm_send(&s_spi_handle, k_frame_type_response, 0, buffer, len);
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
    star_v1_EmergencyStopResponse response =
        star_v1_EmergencyStopResponse_init_zero;

    response.has_header = true;
    rx_nanopb_create_response_header(&response.header, star_v1_Status_STATUS_OK,
                                     NULL);
    response.estop_engaged = engaged;

    uint8_t buffer[RX_NANOPB_BUFFER_SIZE];
    size_t len = 0;

    rx_err_t err = rx_nanopb_encode_estop_response(&response, buffer, &len);
    if (err != RX_OK) {
        return err;
    }

    err = rx_spi_comm_send(&s_spi_handle, k_frame_type_response, 0, buffer, len);
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
static rx_err_t internal_process_frame(const rx_frame_t *frame)
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

    /* Determine message type from first byte of payload (simple approach) */
    if (frame->header.length < 1) {
        RX_LOG_WARN(s_tag, "Empty command payload");
        return RX_ERR_INVALID_SIZE;
    }

    uint8_t msg_type = frame->payload[0];
    const uint8_t *payload_data = &frame->payload[1];
    size_t payload_len = frame->header.length - 1;

    rx_err_t err = RX_OK;

    switch (msg_type) {
        case k_msg_type_velocity:
            err = internal_handle_velocity_command(payload_data, payload_len);
            if (err == RX_OK) {
                internal_send_velocity_response(star_v1_Status_STATUS_OK);
            } else {
                internal_send_velocity_response(
                    star_v1_Status_STATUS_INVALID_REQUEST);
            }
            break;

        case k_msg_type_estop:
            err = internal_handle_estop_command(payload_data, payload_len);
            internal_send_estop_response(err == RX_OK);
            break;

        case k_msg_type_telemetry:
            /* Telemetry request - respond with current status */
            RX_LOG_DEBUG(s_tag, "Telemetry request received");
            /* TODO: Implement telemetry response */
            break;

        default:
            RX_LOG_WARN(s_tag, "Unknown message type");
            err = RX_ERR_PROTOCOL_ERROR;
            break;
    }

    /* Send ACK if required */
    if (frame->header.flags & k_frame_flag_requires_ack) {
        rx_spi_comm_send_ack(&s_spi_handle, frame->header.sequence);
    }

    return err;
}

/**
 * @brief Main SPI communication loop
 */
static void internal_spi_comm_loop(void)
{
    rx_frame_t frame;

    RX_LOG_INFO(s_tag, "SPI comm loop started (100Hz)");

    while (true) {
        /* Sleep for polling period */
        tx_thread_sleep(k_spi_comm_task_period_ms);

        /* Check for incoming frame */
        bool available = false;
        rx_err_t err = rx_spi_comm_data_available(&s_spi_handle, &available);

        if (err != RX_OK) {
            RX_LOG_ERROR(s_tag, "Failed to check SPI data availability");
            continue;
        }

        if (!available) {
            continue;
        }

        /* Receive and decode frame */
        err = rx_spi_comm_receive(&s_spi_handle, &frame, 0);

        if (err == RX_ERR_CRC_MISMATCH) {
            RX_LOG_WARN(s_tag, "CRC mismatch - sending NACK");
            s_stats.crc_errors++;
            rx_spi_comm_send_nack(&s_spi_handle, frame.header.sequence,
                                  k_frame_flag_none);
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
 * @brief SPI communication task entry point
 *
 * @param[in] input ThreadX task input (unused)
 */
static void internal_spi_comm_task(ULONG input)
{
    (void)input;

    RX_LOG_INFO(s_tag, "SPI communication task started");

    internal_spi_comm_loop();

    /* Should never reach here */
    RX_LOG_ERROR(s_tag, "SPI comm task exited unexpectedly");
    tx_thread_delete(&s_spi_comm_thread);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t spi_comm_task_create(motor_shared_state_t *shared_state)
{
    RX_CHECK_NULL_PTR(shared_state, s_tag, "shared_state is NULL");

    s_shared_state = shared_state;

    /* Clear statistics */
    memset(&s_stats, 0, sizeof(s_stats));

    /* Initialize SPI hardware */
    rx_err_t err = rspi_init_peripheral(k_spi_comm_rspi_channel,
                                        k_spi_comm_spi_mode, false);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Failed to initialize SPI peripheral");
        return err;
    }

    /* Initialize nanopb wrapper */
    err = rx_nanopb_init();
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Failed to initialize nanopb");
        rspi_deinit(k_spi_comm_rspi_channel);
        return err;
    }

    /* Initialize SPI communication handle */
    rx_spi_comm_config_t spi_config = {
        .channel     = k_spi_comm_rspi_channel,
        .spi_mode    = k_spi_comm_spi_mode,
        .fec_enabled = 0, /* FEC disabled for now */
    };

    err = rx_spi_comm_init(&s_spi_handle, &spi_config);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Failed to initialize SPI comm handle");
        rspi_deinit(k_spi_comm_rspi_channel);
        return err;
    }

    /* Create SPI communication thread */
    UINT status = tx_thread_create(&s_spi_comm_thread,
                                   "spi_comm",
                                   internal_spi_comm_task,
                                   0,
                                   s_spi_comm_stack,
                                   k_spi_comm_task_stack_size,
                                   k_spi_comm_task_priority,
                                   k_spi_comm_task_priority,
                                   TX_NO_TIME_SLICE,
                                   TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error_int(s_tag, "Failed to create SPI comm thread, status",
                         (int32_t)status);
        rx_spi_comm_deinit(&s_spi_handle);
        rspi_deinit(k_spi_comm_rspi_channel);
        return RX_ERR_INVALID_STATE;
    }

    RX_LOG_INFO(s_tag, "SPI communication task created");

    return RX_OK;
}

rx_err_t spi_comm_task_get_stats(uint32_t *frames_rx, uint32_t *frames_tx,
                                 uint32_t *crc_errors, uint32_t *decode_errors)
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

void spi_comm_task_reset_stats(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}
