/**
 * @file serial_proto.h
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* serial_proto.h -- STAR sandbox wire protocol.
 *
 * Frame layout (little-endian throughout):
 *
 *   +------+------+------+------+----------------+------+
 *   | 0xAA | 0x55 | TYPE | LEN  |  payload[LEN]  | CRC8 |
 *   +------+------+------+------+----------------+------+
 *
 * CRC-8 (Dallas/Maxim, poly 0x07) covers TYPE..last payload byte. Sync 0xAA
 * 0x55 + CRC means a missed byte resyncs cleanly within ~one frame.
 *
 * Frame types:
 *   0x01 IMU_SAMPLE  (RX72N -> Pi)  6 floats + uint32 ts_ms = 28 bytes
 *   0x02 MOTOR_CMD   (Pi -> RX72N)  4 int16 = 8 bytes
 *   0x03 ESTOP       (Pi -> RX72N)  0 bytes
 *   0x04 STATUS      (RX72N -> Pi)  6 bytes
 *
 * Both the firmware (this header) and the Pi-side bridge
 * (pi_node/star_serial_bridge/star_serial_bridge.py) speak this format. If
 * you change the wire here, mirror the change in the Python bridge or the
 * link goes silent.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- Frame constants ---------------------------------------------------- */
typedef enum : uint8_t {
    k_proto_sync_0      = 0xAAU,
    k_proto_sync_1      = 0x55U,
    k_proto_max_payload = 32U,
} proto_consts_t;

typedef enum : uint8_t {
    k_proto_type_imu_sample = 0x01U,
    k_proto_type_motor_cmd  = 0x02U,
    k_proto_type_estop      = 0x03U,
    k_proto_type_status     = 0x04U,
} proto_type_t;

/* ---- Payload structs (little-endian, packed) ---------------------------- */
typedef struct __attribute__((packed)) {
    float    accel_mps2[3];   /* x, y, z (m/s^2) */
    float    gyro_rps[3];     /* x, y, z (rad/s) */
    uint32_t ts_ms;           /* monotonic since boot */
} imu_sample_payload_t;       /* 28 bytes */

typedef struct __attribute__((packed)) {
    int16_t duty[4];          /* signed -10000..+10000; sign = direction */
} motor_cmd_payload_t;        /* 8 bytes */

typedef struct __attribute__((packed)) {
    uint8_t  imu_ok;
    uint8_t  motor_armed;
    uint32_t uptime_ms;
} status_payload_t;           /* 6 bytes */

/* ---- Encoder API (firmware side) ---------------------------------------- */

/* Encode a frame into `out` (must be at least 5 + payload_len bytes).
 * Returns total bytes written, or 0 on error. */
size_t proto_encode(uint8_t type, const void *payload, uint8_t payload_len,
                    uint8_t *out, size_t out_cap);

/* ---- Decoder API (firmware side) ---------------------------------------- */

/* Result of feeding one byte into the parser. */
typedef enum : uint8_t {
    k_proto_rx_need_more = 0U,  /* still building a frame */
    k_proto_rx_motor_cmd = 1U,  /* MOTOR_CMD ready -- read out_motor */
    k_proto_rx_estop     = 2U,  /* ESTOP received */
    k_proto_rx_drop      = 3U,  /* CRC fail or unknown type; resync from next 0xAA */
} proto_rx_result_t;

/* Opaque-ish parser state. Caller owns one of these per RX channel. */
typedef struct {
    uint8_t  state;           /* internal SM */
    uint8_t  type;
    uint8_t  expected_len;
    uint8_t  payload_idx;
    uint8_t  payload[k_proto_max_payload];
} proto_rx_state_t;

void proto_rx_init(proto_rx_state_t *s);

/* Feed one byte; returns the result. If it returns k_proto_rx_motor_cmd, the
 * caller may copy from `out_motor` (always provided so we don't double-buffer). */
proto_rx_result_t proto_rx_feed(proto_rx_state_t *s, uint8_t b,
                                motor_cmd_payload_t *out_motor);
