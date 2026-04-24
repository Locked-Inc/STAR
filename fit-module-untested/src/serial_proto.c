/**
 * @file serial_proto.c
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* serial_proto.c -- STAR sandbox wire-protocol encoder + RX state machine. */

#include "serial_proto.h"
#include "crc8.h"
#include <string.h>

/* ---- Encoder ------------------------------------------------------------ */
size_t proto_encode(uint8_t type, const void *payload, uint8_t payload_len,
                    uint8_t *out, size_t out_cap)
{
    if (out == NULL || payload_len > k_proto_max_payload) {
        return 0;
    }
    if (payload_len > 0U && payload == NULL) {
        return 0;
    }
    const size_t total = (size_t)5U + payload_len;
    if (out_cap < total) {
        return 0;
    }

    out[0] = k_proto_sync_0;
    out[1] = k_proto_sync_1;
    out[2] = type;
    out[3] = payload_len;
    if (payload_len > 0U) {
        memcpy(&out[4], payload, payload_len);
    }
    /* CRC covers TYPE..last payload byte, i.e. out[2..4+payload_len). */
    out[4 + payload_len] = crc8(&out[2], (size_t)2U + payload_len);
    return total;
}

/* ---- RX state machine --------------------------------------------------- */
typedef enum : uint8_t {
    k_st_sync0 = 0,
    k_st_sync1,
    k_st_type,
    k_st_len,
    k_st_payload,
    k_st_crc,
} rx_state_t;

void proto_rx_init(proto_rx_state_t *s)
{
    s->state        = (uint8_t)k_st_sync0;
    s->type         = 0U;
    s->expected_len = 0U;
    s->payload_idx  = 0U;
}

proto_rx_result_t proto_rx_feed(proto_rx_state_t *s, uint8_t b,
                                motor_cmd_payload_t *out_motor)
{
    switch ((rx_state_t)s->state) {
    case k_st_sync0:
        if (b == k_proto_sync_0) {
            s->state = (uint8_t)k_st_sync1;
        }
        return k_proto_rx_need_more;

    case k_st_sync1:
        if (b == k_proto_sync_1) {
            s->state = (uint8_t)k_st_type;
        } else if (b == k_proto_sync_0) {
            /* stay -- next byte might be the real sync1 */
        } else {
            s->state = (uint8_t)k_st_sync0;
        }
        return k_proto_rx_need_more;

    case k_st_type:
        s->type  = b;
        s->state = (uint8_t)k_st_len;
        return k_proto_rx_need_more;

    case k_st_len:
        if (b > k_proto_max_payload) {
            s->state = (uint8_t)k_st_sync0;
            return k_proto_rx_drop;
        }
        s->expected_len = b;
        s->payload_idx  = 0U;
        s->state = (b == 0U) ? (uint8_t)k_st_crc : (uint8_t)k_st_payload;
        return k_proto_rx_need_more;

    case k_st_payload:
        s->payload[s->payload_idx++] = b;
        if (s->payload_idx >= s->expected_len) {
            s->state = (uint8_t)k_st_crc;
        }
        return k_proto_rx_need_more;

    case k_st_crc: {
        /* Recompute CRC over TYPE + LEN + payload. */
        uint8_t buf[2 + k_proto_max_payload];
        buf[0] = s->type;
        buf[1] = s->expected_len;
        if (s->expected_len > 0U) {
            memcpy(&buf[2], s->payload, s->expected_len);
        }
        const uint8_t want = crc8(buf, (size_t)2U + s->expected_len);
        proto_rx_result_t result = k_proto_rx_drop;
        if (b == want) {
            switch ((proto_type_t)s->type) {
            case k_proto_type_motor_cmd:
                if (s->expected_len == sizeof(motor_cmd_payload_t)
                    && out_motor != NULL) {
                    memcpy(out_motor, s->payload, sizeof(motor_cmd_payload_t));
                    result = k_proto_rx_motor_cmd;
                }
                break;
            case k_proto_type_estop:
                if (s->expected_len == 0U) {
                    result = k_proto_rx_estop;
                }
                break;
            default:
                /* IMU_SAMPLE / STATUS go RX72N -> Pi only; firmware drops them. */
                break;
            }
        }
        s->state = (uint8_t)k_st_sync0;
        return result;
    }

    default:
        s->state = (uint8_t)k_st_sync0;
        return k_proto_rx_drop;
    }
}
