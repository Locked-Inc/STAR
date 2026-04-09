/**
 * @file comm_task.c
 * @brief Communication task -- frame parsing and protobuf command dispatch.
 *
 * @details
 * 100 Hz loop that reads USB CDC bytes, feeds them to a streaming frame
 * decoder, and dispatches decoded WireMessage commands to shared_data.
 *
 * Supported commands:
 * - VelocityCommand: maps 4 motor velocities to duty_percent (scaled by max velocity)
 * - EmergencyStopCommand: asserts estop flag
 * - MotorPowerCommand: maps motor_id + duty_cycle_percent to duty_percent
 *
 * Sends ACK frames when REQUIRES_ACK flag is set.
 * Responds to PING with PONG.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "comm_task.h"
#include "bb_frame.h"
#include "bb_usb_cdc.h"
#include "hardware_config.h"

#include <pb_decode.h>
#include <star/v1/wire.pb.h>

#include <robotcontrol.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum comm_buf_t
 * @brief Buffer sizes for the comm task.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_rx_buf_size   = 512U,  /**< USB CDC receive buffer size */
    k_wire_buf_size = 1036U, /**< Maximum encoded frame size */
} comm_buf_t;

/* Motor indices from hardware_config.h: k_bb_motor_idx_fl/fr/rl/rr */

/**
 * @var s_max_velocity_mps
 * @brief Maximum velocity for duty cycle scaling (m/s to [-1, 1]).
 *
 * @since Version 1.0.0
 */
static const float s_max_velocity_mps = 2.0F;

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * @brief Send an ACK frame for a given sequence number.
 *
 * @param[in] seq Sequence number to acknowledge
 *
 * @pre bb_usb_cdc_init() must have been called
 * @post An ACK frame with the given sequence number is sent over USB CDC
 *
 * @since Version 1.0.0
 */
static void internal_send_ack(const uint16_t seq)
{
    bb_frame_t ack = {0};
    (void)bb_frame_build_ack(seq, &ack);

    static uint8_t s_wire_buf[k_wire_buf_size];
    uint32_t wire_len = 0U;

    if (bb_frame_encode(&ack, s_wire_buf, k_wire_buf_size, &wire_len) == k_bb_err_ok) {
        (void)bb_usb_cdc_send(s_wire_buf, wire_len, 0U);
    }
}

/**
 * @brief Send a PONG frame echoing the PING payload.
 *
 * @param[in] ping Received PING frame whose payload is echoed back
 *
 * @pre ping must point to a valid decoded frame with type PING
 * @post A PONG frame with the same sequence and payload is sent over USB CDC
 *
 * @since Version 1.0.0
 */
static void internal_send_pong(const bb_frame_t* ping)
{
    bb_frame_t pong = {0};
    pong.header.sequence = ping->header.sequence;
    pong.header.length   = ping->header.length;
    pong.header.type     = k_bb_frame_type_pong;
    pong.header.flags    = 0U;

    if (ping->header.length > 0U) {
        (void)memcpy(pong.payload, ping->payload, ping->header.length);
    }

    static uint8_t s_pong_buf[k_wire_buf_size];
    uint32_t wire_len = 0U;

    if (bb_frame_encode(&pong, s_pong_buf, k_wire_buf_size, &wire_len) == k_bb_err_ok) {
        (void)bb_usb_cdc_send(s_pong_buf, wire_len, 0U);
    }
}

/**
 * @brief Clamp a float to [-1.0, 1.0].
 *
 * @param[in] val Value to clamp to the duty cycle range
 *
 * @return float Clamped value in [-1.0, 1.0]
 *
 * @pre None (accepts any float value)
 * @post Return value satisfies -1.0 <= result <= 1.0
 *
 * @since Version 1.0.0
 */
static inline float internal_clamp_duty(const float val)
{
    if (val < -1.0F) {
        return -1.0F;
    }
    if (val > 1.0F) {
        return 1.0F;
    }
    return val;
}

/**
 * @brief Process a decoded COMMAND frame.
 *
 * @details
 * Decodes the WireMessage protobuf payload and dispatches to the appropriate
 * shared_data setter based on which_payload.
 *
 * @param[in,out] sd    Shared data store to update with the decoded command
 * @param[in]     frame Decoded frame containing a protobuf WireMessage payload
 *
 * @pre sd must point to an initialized bb_shared_data_t
 * @pre frame must be a valid decoded frame with type COMMAND
 * @post shared_data is updated according to the decoded WireMessage payload
 *
 * @since Version 1.0.0
 */
static void internal_process_command(bb_shared_data_t* sd,
                                     const bb_frame_t* frame)
{
    star_v1_WireMessage msg = star_v1_WireMessage_init_zero;

    pb_istream_t stream = pb_istream_from_buffer(frame->payload,
                                                  frame->header.length);

    if (!pb_decode(&stream, star_v1_WireMessage_fields, &msg)) {
        return; /* decode failed -- discard */
    }

    switch (msg.which_payload) {

    case star_v1_WireMessage_velocity_command_tag: {
        bb_motor_cmd_t cmd = {0};
        const star_v1_VelocityCommand* vc = &msg.payload.velocity_command;

        /* Scale m/s to [-1.0, 1.0] duty range */
        cmd.duty_percent[k_bb_motor_idx_fl] = internal_clamp_duty(
            (float)vc->front_left_velocity_mps / s_max_velocity_mps);
        cmd.duty_percent[k_bb_motor_idx_fr] = internal_clamp_duty(
            (float)vc->front_right_velocity_mps / s_max_velocity_mps);
        cmd.duty_percent[k_bb_motor_idx_rl] = internal_clamp_duty(
            (float)vc->back_left_velocity_mps / s_max_velocity_mps);
        cmd.duty_percent[k_bb_motor_idx_rr] = internal_clamp_duty(
            (float)vc->back_right_velocity_mps / s_max_velocity_mps);

        (void)bb_shared_data_set_motor_cmd(sd, &cmd);
        break;
    }

    case star_v1_WireMessage_emergency_stop_command_tag:
        (void)bb_shared_data_set_estop(sd, true);
        break;

    case star_v1_WireMessage_motor_power_command_tag: {
        const star_v1_MotorPowerCommand* mp = &msg.payload.motor_power_command;

        /* Read current command, update single motor */
        bb_motor_cmd_t cmd = {0};
        (void)bb_shared_data_get_motor_cmd(sd, &cmd);

        if (mp->motor_id >= 0 && mp->motor_id < (int32_t)k_bb_motor_count) {
            cmd.duty_percent[mp->motor_id] = internal_clamp_duty(
                (float)mp->duty_cycle_percent / s_bb_duty_percent_scale);
        }

        (void)bb_shared_data_set_motor_cmd(sd, &cmd);
        break;
    }

    default:
        /* Unknown payload type -- ignore */
        break;
    }
}

/* ---------------------------------------------------------------------------
 * Task entry point
 * ---------------------------------------------------------------------------*/

/**
 * @brief comm_task thread entry point.
 *
 * @details
 * 100 Hz loop that polls USB CDC for incoming bytes, feeds them to the
 * streaming frame decoder, and dispatches decoded commands to shared_data.
 *
 * @param[in] arg Pointer to bb_shared_data_t
 *
 * @return void* Always NULL
 *
 * @pre bb_usb_cdc_init() must have been called
 * @pre arg must point to initialized bb_shared_data_t
 *
 * @since Version 1.0.0
 */
void* bb_comm_task(void* arg)
{
    if (arg == NULL) {
        return NULL;
    }

    bb_shared_data_t* sd = (bb_shared_data_t*)arg;

    static uint8_t s_rx_buf[k_rx_buf_size];
    static bb_frame_decoder_t s_decoder;

    (void)bb_frame_decoder_init(&s_decoder);

    /* Period derived from k_bb_rate_comm_hz in hardware_config.h */

    struct timespec next = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &next);

    while (rc_get_state() != EXITING) {
        uint32_t received = 0U;
        (void)bb_usb_cdc_receive(s_rx_buf, k_rx_buf_size, &received);

        /* Feed each received byte to the streaming decoder */
        for (uint32_t i = 0U; i < received; i++) {
            bool frame_ready = false;
            (void)bb_frame_decoder_feed(&s_decoder, s_rx_buf[i], &frame_ready);

            if (frame_ready) {
                bb_frame_t frame = {0};
                (void)bb_frame_decoder_get_frame(&s_decoder, &frame);
                (void)bb_frame_decoder_reset(&s_decoder);

                switch (frame.header.type) {

                case k_bb_frame_type_command:
                    internal_process_command(sd, &frame);

                    if (frame.header.flags & k_bb_frame_flag_requires_ack) {
                        internal_send_ack(frame.header.sequence);
                    }
                    break;

                case k_bb_frame_type_ping:
                    internal_send_pong(&frame);
                    break;

                default:
                    /* ACK, NACK, RESPONSE, PONG -- ignore */
                    break;
                }
            }
        }

        /* Advance next wakeup by one period */
        next.tv_nsec += (long)k_bb_comm_period_ns;
        if (next.tv_nsec >= (long)k_bb_ns_per_sec) {
            next.tv_sec++;
            next.tv_nsec -= (long)k_bb_ns_per_sec;
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return NULL;
}
