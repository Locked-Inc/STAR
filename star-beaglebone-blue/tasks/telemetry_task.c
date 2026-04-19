/**
 * @file telemetry_task.c
 * @brief Telemetry task -- encodes sensor data and sends to RPi5 via USB.
 *
 * @details
 * 10 Hz loop that:
 * 1. Reads encoder, IMU, and estop data from shared_data
 * 2. Builds a TelemetryData protobuf message
 * 3. Wraps in a WireMessage
 * 4. Encodes via nanopb into a RESPONSE frame
 * 5. Sends the frame over USB CDC
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "telemetry_task.h"
#include "bb_frame.h"
#include "bb_usb_cdc.h"
#include "hardware_config.h"

#include <pb_encode.h>
#include <star/v1/wire.pb.h>

#include <math.h>
#include <robotcontrol.h>
#include <string.h>
#include <time.h>

/* M_PI is a POSIX/GNU extension and not guaranteed by strict C23. Provide
 * a fallback so this file compiles even if the system math.h is built
 * without _GNU_SOURCE. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum telem_buf_t
 * @brief Buffer sizes for telemetry encoding.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_pb_buf_size   = 1024U, /**< Protobuf encode buffer */
    k_wire_buf_size = 1036U, /**< Maximum encoded frame size */
} telem_buf_t;

/** Multiplier for full revolution in radians (2 * pi). */
static const float s_bb_two_pi = 2.0F * (float)M_PI;

/* Motor/encoder indices and axis indices from hardware_config.h */

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * @brief Get current time in microseconds from CLOCK_MONOTONIC.
 *
 * @return int64_t Microseconds since boot
 *
 * @pre CLOCK_MONOTONIC must be available on the platform
 * @post Returned value increases monotonically across successive calls
 *
 * @since Version 1.0.0
 */
static int64_t internal_now_us(void)
{
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * (int64_t)k_bb_us_per_sec
         + (int64_t)ts.tv_nsec / (int64_t)k_bb_ns_per_us;
}

/* ---------------------------------------------------------------------------
 * Task entry point
 * ---------------------------------------------------------------------------*/

/**
 * @brief telemetry_task thread entry point.
 *
 * @details
 * 10 Hz loop that aggregates sensor data from shared_data, encodes it as
 * a TelemetryData protobuf wrapped in a WireMessage RESPONSE frame, and
 * sends it over USB CDC to the RPi5 gateway.
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
void* bb_telemetry_task(void* arg)
{
    if (arg == NULL) {
        return NULL;
    }

    bb_shared_data_t* sd = (bb_shared_data_t*)arg;

    static uint8_t s_pb_buf[k_pb_buf_size];
    static uint8_t s_wire_buf[k_wire_buf_size];
    static uint16_t s_tx_seq = 0U;

    /* Velocity is derived from tick deltas across the telemetry period.
     * First message seeds prev_ticks/prev_us and reports zero velocity. */
    static int32_t s_prev_ticks[k_bb_encoder_count] = {0};
    static int64_t s_prev_us = 0;
    static bool    s_have_prev = false;
    const float    m_per_tick = (s_bb_two_pi * s_bb_wheel_radius_m)
                              / (float)k_bb_ticks_per_rev;

    /* Period derived from k_bb_rate_telemetry_hz in hardware_config.h */

    struct timespec next = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &next);

    while (rc_get_state() != EXITING) {
        /* Read sensor data from shared_data */
        bb_encoder_data_t enc = {0};
        bb_imu_data_t     imu = {0};
        bool              estop = false;

        (void)bb_shared_data_get_encoder(sd, &enc);
        (void)bb_shared_data_get_imu(sd, &imu);
        (void)bb_shared_data_get_estop(sd, &estop);

        const int64_t now_us = internal_now_us();

        /* Compute per-wheel velocity from tick deltas. Zero on the first
         * iteration and on any non-positive dt. Uses CLOCK_MONOTONIC so it
         * is immune to wall-clock adjustments. */
        float vel_mps[k_bb_encoder_count] = {0};
        if (s_have_prev) {
            const int64_t dt_us = now_us - s_prev_us;
            if (dt_us > 0) {
                const float dt_s = (float)dt_us / (float)k_bb_us_per_sec;
                for (uint8_t i = 0U; i < (uint8_t)k_bb_encoder_count; i++) {
                    const int32_t d_ticks = enc.ticks[i] - s_prev_ticks[i];
                    vel_mps[i] = ((float)d_ticks * m_per_tick) / dt_s;
                }
            }
        }
        for (uint8_t i = 0U; i < (uint8_t)k_bb_encoder_count; i++) {
            s_prev_ticks[i] = enc.ticks[i];
        }
        s_prev_us = now_us;
        s_have_prev = true;

        /* Build TelemetryData */
        star_v1_TelemetryData telem = star_v1_TelemetryData_init_zero;

        /* IMU */
        telem.has_imu = true;
        telem.imu.accel_x_mps2     = (double)imu.accel_mps2[k_bb_axis_x];
        telem.imu.accel_y_mps2     = (double)imu.accel_mps2[k_bb_axis_y];
        telem.imu.accel_z_mps2     = (double)imu.accel_mps2[k_bb_axis_z];
        telem.imu.gyro_x_rad_per_s = (double)imu.gyro_rps[k_bb_axis_x];
        telem.imu.gyro_y_rad_per_s = (double)imu.gyro_rps[k_bb_axis_y];
        telem.imu.gyro_z_rad_per_s = (double)imu.gyro_rps[k_bb_axis_z];

        /* Encoders */
        telem.has_encoder_front_left  = true;
        telem.encoder_front_left.motor_id     = (int32_t)k_bb_motor_idx_fl;
        telem.encoder_front_left.ticks        = (int64_t)enc.ticks[k_bb_motor_idx_fl];
        telem.encoder_front_left.velocity_mps = (double)vel_mps[k_bb_motor_idx_fl];
        telem.encoder_front_left.timestamp_us = now_us;

        telem.has_encoder_front_right = true;
        telem.encoder_front_right.motor_id     = (int32_t)k_bb_motor_idx_fr;
        telem.encoder_front_right.ticks        = (int64_t)enc.ticks[k_bb_motor_idx_fr];
        telem.encoder_front_right.velocity_mps = (double)vel_mps[k_bb_motor_idx_fr];
        telem.encoder_front_right.timestamp_us = now_us;

        telem.has_encoder_back_left = true;
        telem.encoder_back_left.motor_id     = (int32_t)k_bb_motor_idx_rl;
        telem.encoder_back_left.ticks        = (int64_t)enc.ticks[k_bb_motor_idx_rl];
        telem.encoder_back_left.velocity_mps = (double)vel_mps[k_bb_motor_idx_rl];
        telem.encoder_back_left.timestamp_us = now_us;

        telem.has_encoder_back_right = true;
        telem.encoder_back_right.motor_id     = (int32_t)k_bb_motor_idx_rr;
        telem.encoder_back_right.ticks        = (int64_t)enc.ticks[k_bb_motor_idx_rr];
        telem.encoder_back_right.velocity_mps = (double)vel_mps[k_bb_motor_idx_rr];
        telem.encoder_back_right.timestamp_us = now_us;

        /* Status */
        telem.emergency_stop = estop;
        telem.timestamp_us   = now_us;
        telem.frame_sequence = s_tx_seq;

        /* Wrap in WireMessage */
        star_v1_WireMessage wire_msg = star_v1_WireMessage_init_zero;
        wire_msg.which_payload = star_v1_WireMessage_telemetry_data_tag;
        wire_msg.payload.telemetry_data = telem;

        /* Encode protobuf */
        pb_ostream_t stream = pb_ostream_from_buffer(s_pb_buf, k_pb_buf_size);

        if (pb_encode(&stream, star_v1_WireMessage_fields, &wire_msg)) {
            /* Build and send frame */
            bb_frame_t frame = {0};
            frame.header.sequence = s_tx_seq;
            frame.header.length   = (uint16_t)stream.bytes_written;
            frame.header.type     = k_bb_frame_type_response;
            frame.header.flags    = 0U;

            (void)memcpy(frame.payload, s_pb_buf, stream.bytes_written);

            uint32_t wire_len = 0U;

            if (bb_frame_encode(&frame, s_wire_buf, k_wire_buf_size,
                                &wire_len) == k_bb_err_ok) {
                (void)bb_usb_cdc_send(s_wire_buf, wire_len, 0U);
            }

            s_tx_seq++;
        }

        /* Advance next wakeup */
        next.tv_nsec += (long)k_bb_telemetry_period_ns;
        if (next.tv_nsec >= (long)k_bb_ns_per_sec) {
            next.tv_sec++;
            next.tv_nsec -= (long)k_bb_ns_per_sec;
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return NULL;
}
