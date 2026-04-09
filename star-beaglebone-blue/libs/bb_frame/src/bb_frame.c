/**
 * @file bb_frame.c
 * @brief Wire frame codec and streaming decoder implementation.
 *
 * @details
 * Encodes/decodes STAR binary frames with CRC-32 validation. The streaming
 * decoder processes USB CDC input one byte at a time with automatic SYNC
 * hunting and CRC-failure re-synchronization.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "bb_frame.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "bb_crc.h"

/* ---------------------------------------------------------------------------
 * Wire byte offsets
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_frame_offset_t
 * @brief Byte offsets into the wire frame.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_offset_sync  = 0U, /**< SYNC word offset */
    k_offset_seq   = 2U, /**< Sequence number offset */
    k_offset_len   = 4U, /**< Payload length offset */
    k_offset_type  = 6U, /**< Frame type offset */
    k_offset_flags = 7U, /**< Flags offset */
    k_offset_data  = 8U, /**< Payload start offset */
} bb_frame_offset_t;

/**
 * @enum bb_frame_sync_bytes_t
 * @brief SYNC word individual bytes (little-endian on wire).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_sync_lo = 0xAAU, /**< SYNC low byte (first on wire) */
    k_sync_hi = 0x55U, /**< SYNC high byte (second on wire) */
} bb_frame_sync_bytes_t;

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * @brief Write a uint16_t as little-endian to a buffer.
 *
 * @param[out] buf Destination buffer (must have at least 2 bytes available)
 * @param[in]  val Value to write in little-endian byte order
 *
 * @pre buf must point to a writable buffer of at least 2 bytes
 * @post buf[0] contains the low byte and buf[1] the high byte of val
 *
 * @since Version 1.0.0
 */
static inline void internal_write_le16(uint8_t* buf, const uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFFU);
    buf[1] = (uint8_t)((val >> 8U) & 0xFFU);
}

/**
 * @brief Read a uint16_t from a little-endian buffer.
 *
 * @param[in] buf Source buffer containing at least 2 bytes in little-endian order
 *
 * @return uint16_t Reconstructed 16-bit value
 *
 * @pre buf must point to a readable buffer of at least 2 bytes
 * @post No side effects; buf is not modified
 *
 * @since Version 1.0.0
 */
static inline uint16_t internal_read_le16(const uint8_t* buf)
{
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8U));
}

/**
 * @brief Write a uint32_t as little-endian to a buffer.
 *
 * @param[out] buf Destination buffer (must have at least 4 bytes available)
 * @param[in]  val Value to write in little-endian byte order
 *
 * @pre buf must point to a writable buffer of at least 4 bytes
 * @post buf[0..3] contain val in little-endian order
 *
 * @since Version 1.0.0
 */
static inline void internal_write_le32(uint8_t* buf, const uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFFU);
    buf[1] = (uint8_t)((val >> 8U) & 0xFFU);
    buf[2] = (uint8_t)((val >> 16U) & 0xFFU);
    buf[3] = (uint8_t)((val >> 24U) & 0xFFU);
}

/**
 * @brief Read a uint32_t from a little-endian buffer.
 *
 * @param[in] buf Source buffer containing at least 4 bytes in little-endian order
 *
 * @return uint32_t Reconstructed 32-bit value
 *
 * @pre buf must point to a readable buffer of at least 4 bytes
 * @post No side effects; buf is not modified
 *
 * @since Version 1.0.0
 */
static inline uint32_t internal_read_le32(const uint8_t* buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8U)
         | ((uint32_t)buf[2] << 16U)
         | ((uint32_t)buf[3] << 24U);
}

/* ---------------------------------------------------------------------------
 * One-shot encode
 * ---------------------------------------------------------------------------*/

bb_err_t bb_frame_encode(const bb_frame_t* frame, uint8_t* out_buf,
                         const uint32_t out_size, uint32_t* out_len)
{
    if (frame == NULL || out_buf == NULL || out_len == NULL) {
        return k_bb_err_null_ptr;
    }

    if (frame->header.length > k_bb_frame_max_payload) {
        return k_bb_err_invalid_arg;
    }

    const uint32_t total = (uint32_t)k_bb_frame_overhead + frame->header.length;

    if (out_size < total) {
        return k_bb_err_invalid_arg;
    }

    /* SYNC (LE) */
    internal_write_le16(&out_buf[k_offset_sync], k_bb_frame_sync_word);

    /* Header fields */
    internal_write_le16(&out_buf[k_offset_seq], frame->header.sequence);
    internal_write_le16(&out_buf[k_offset_len], frame->header.length);
    out_buf[k_offset_type]  = frame->header.type;
    out_buf[k_offset_flags] = frame->header.flags;

    /* Payload */
    if (frame->header.length > 0U) {
        (void)memcpy(&out_buf[k_offset_data], frame->payload,
                     frame->header.length);
    }

    /* CRC-32 over SYNC + header + payload */
    const uint32_t crc_len = (uint32_t)k_offset_data + frame->header.length;
    uint32_t crc = 0U;
    bb_err_t err = bb_crc32_compute(out_buf, crc_len, &crc);
    if (err != k_bb_err_ok) {
        return err;
    }

    /* Write CRC (LE) */
    internal_write_le32(&out_buf[crc_len], crc);

    *out_len = total;
    return k_bb_err_ok;
}

/* ---------------------------------------------------------------------------
 * One-shot decode
 * ---------------------------------------------------------------------------*/

bb_err_t bb_frame_decode(const uint8_t* wire_data, const uint32_t wire_len,
                         bb_frame_t* out_frame)
{
    if (wire_data == NULL || out_frame == NULL) {
        return k_bb_err_null_ptr;
    }

    if (wire_len < (uint32_t)k_bb_frame_overhead) {
        return k_bb_err_invalid_arg;
    }

    /* Verify SYNC */
    const uint16_t sync = internal_read_le16(&wire_data[k_offset_sync]);
    if (sync != k_bb_frame_sync_word) {
        return k_bb_err_invalid_arg;
    }

    /* Parse header */
    out_frame->header.sequence = internal_read_le16(&wire_data[k_offset_seq]);
    out_frame->header.length   = internal_read_le16(&wire_data[k_offset_len]);
    out_frame->header.type     = wire_data[k_offset_type];
    out_frame->header.flags    = wire_data[k_offset_flags];

    /* Validate length */
    if (out_frame->header.length > k_bb_frame_max_payload) {
        return k_bb_err_invalid_arg;
    }

    const uint32_t expected_total = (uint32_t)k_bb_frame_overhead
                                  + out_frame->header.length;
    if (wire_len < expected_total) {
        return k_bb_err_invalid_arg;
    }

    /* Copy payload */
    if (out_frame->header.length > 0U) {
        (void)memcpy(out_frame->payload, &wire_data[k_offset_data],
                     out_frame->header.length);
    }

    /* Verify CRC */
    const uint32_t crc_offset = (uint32_t)k_offset_data + out_frame->header.length;
    const uint32_t received_crc = internal_read_le32(&wire_data[crc_offset]);

    uint32_t computed_crc = 0U;
    bb_err_t err = bb_crc32_compute(wire_data, crc_offset, &computed_crc);
    if (err != k_bb_err_ok) {
        return err;
    }

    if (received_crc != computed_crc) {
        return k_bb_err_crc_mismatch;
    }

    out_frame->crc = received_crc;
    return k_bb_err_ok;
}

/* ---------------------------------------------------------------------------
 * Streaming decoder
 * ---------------------------------------------------------------------------*/

bb_err_t bb_frame_decoder_init(bb_frame_decoder_t* dec)
{
    if (dec == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)memset(dec, 0, sizeof(*dec));
    dec->state       = k_bb_frame_state_hunt_sync_lo;
    dec->initialized = true;

    return k_bb_err_ok;
}

bb_err_t bb_frame_decoder_reset(bb_frame_decoder_t* dec)
{
    if (dec == NULL) {
        return k_bb_err_null_ptr;
    }

    dec->state       = k_bb_frame_state_hunt_sync_lo;
    dec->header_pos  = 0U;
    dec->payload_pos = 0U;
    dec->crc_pos     = 0U;
    (void)memset(&dec->frame, 0, sizeof(dec->frame));
    (void)memset(dec->header_buf, 0, sizeof(dec->header_buf));
    (void)memset(dec->crc_buf, 0, sizeof(dec->crc_buf));

    return k_bb_err_ok;
}

/**
 * @brief Parse header bytes into frame header struct.
 *
 * @param[in,out] dec Decoder whose header_buf is read and frame.header is populated
 *
 * @pre dec->header_buf must contain k_bb_frame_header_size valid bytes
 * @post dec->frame.header fields (sequence, length, type, flags) are populated
 *
 * @since Version 1.0.0
 */
static void internal_parse_header(bb_frame_decoder_t* dec)
{
    dec->frame.header.sequence = internal_read_le16(&dec->header_buf[0]);
    dec->frame.header.length   = internal_read_le16(&dec->header_buf[2]);
    dec->frame.header.type     = dec->header_buf[4];
    dec->frame.header.flags    = dec->header_buf[5];
}

/**
 * @brief Validate CRC of the assembled frame.
 *
 * @param[in,out] dec Decoder with header_buf, payload, and crc_buf populated
 *
 * @return true if computed CRC matches the received CRC, false otherwise
 *
 * @pre dec->header_buf, dec->frame.payload, and dec->crc_buf must be fully populated
 * @post dec->frame.crc is set to the received CRC value
 *
 * @since Version 1.0.0
 */
static bool internal_validate_crc(bb_frame_decoder_t* dec)
{
    /* Build wire representation for CRC computation */
    uint8_t sync_buf[k_bb_frame_sync_size];
    internal_write_le16(sync_buf, k_bb_frame_sync_word);

    /* Incremental CRC: SYNC + header + payload */
    uint32_t crc = k_bb_crc32_init_value;

    (void)bb_crc32_update(crc, sync_buf, k_bb_frame_sync_size, &crc);
    (void)bb_crc32_update(crc, dec->header_buf, k_bb_frame_header_size, &crc);

    if (dec->frame.header.length > 0U) {
        (void)bb_crc32_update(crc, dec->frame.payload,
                              dec->frame.header.length, &crc);
    }

    crc ^= k_bb_crc32_final_xor; /* finalize */

    const uint32_t received_crc = internal_read_le32(dec->crc_buf);
    dec->frame.crc = received_crc;

    return (crc == received_crc);
}

bb_err_t bb_frame_decoder_feed(bb_frame_decoder_t* dec, const uint8_t byte,
                               bool* frame_ready)
{
    if (dec == NULL || frame_ready == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!dec->initialized) {
        return k_bb_err_not_initialized;
    }

    *frame_ready = false;

    switch (dec->state) {

    case k_bb_frame_state_hunt_sync_lo:
        if (byte == k_sync_lo) {
            dec->state = k_bb_frame_state_hunt_sync_hi;
        }
        break;

    case k_bb_frame_state_hunt_sync_hi:
        if (byte == k_sync_hi) {
            dec->header_pos  = 0U;
            dec->payload_pos = 0U;
            dec->crc_pos     = 0U;
            dec->state       = k_bb_frame_state_read_header;
        } else if (byte == k_sync_lo) {
            /* Stay in hunt_sync_hi -- this byte might be start of new SYNC */
        } else {
            dec->state = k_bb_frame_state_hunt_sync_lo;
        }
        break;

    case k_bb_frame_state_read_header:
        dec->header_buf[dec->header_pos] = byte;
        dec->header_pos++;

        if (dec->header_pos >= k_bb_frame_header_size) {
            internal_parse_header(dec);

            /* Validate payload length */
            if (dec->frame.header.length > k_bb_frame_max_payload) {
                dec->state = k_bb_frame_state_hunt_sync_lo;
                break;
            }

            if (dec->frame.header.length == 0U) {
                dec->state = k_bb_frame_state_read_crc;
            } else {
                dec->state = k_bb_frame_state_read_payload;
            }
        }
        break;

    case k_bb_frame_state_read_payload:
        dec->frame.payload[dec->payload_pos] = byte;
        dec->payload_pos++;

        if (dec->payload_pos >= dec->frame.header.length) {
            dec->state = k_bb_frame_state_read_crc;
        }
        break;

    case k_bb_frame_state_read_crc:
        dec->crc_buf[dec->crc_pos] = byte;
        dec->crc_pos++;

        if (dec->crc_pos >= k_bb_frame_crc_size) {
            if (internal_validate_crc(dec)) {
                dec->state   = k_bb_frame_state_complete;
                *frame_ready = true;
            } else {
                /* CRC failure -- re-sync */
                dec->state = k_bb_frame_state_hunt_sync_lo;
            }
        }
        break;

    case k_bb_frame_state_complete:
        /* Frame waiting to be retrieved -- ignore bytes until reset */
        break;
    }

    return k_bb_err_ok;
}

bb_err_t bb_frame_decoder_get_frame(const bb_frame_decoder_t* dec,
                                    bb_frame_t* out_frame)
{
    if (dec == NULL || out_frame == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!dec->initialized) {
        return k_bb_err_not_initialized;
    }

    if (dec->state != k_bb_frame_state_complete) {
        return k_bb_err_invalid_arg;
    }

    *out_frame = dec->frame;
    return k_bb_err_ok;
}

/* ---------------------------------------------------------------------------
 * Convenience builders
 * ---------------------------------------------------------------------------*/

bb_err_t bb_frame_build_ack(const uint16_t seq, bb_frame_t* out_frame)
{
    if (out_frame == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)memset(out_frame, 0, sizeof(*out_frame));
    out_frame->header.sequence = seq;
    out_frame->header.length   = 0U;
    out_frame->header.type     = k_bb_frame_type_ack;
    out_frame->header.flags    = 0U;

    return k_bb_err_ok;
}

bb_err_t bb_frame_build_nack(const uint16_t seq, bb_frame_t* out_frame)
{
    if (out_frame == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)memset(out_frame, 0, sizeof(*out_frame));
    out_frame->header.sequence = seq;
    out_frame->header.length   = 0U;
    out_frame->header.type     = k_bb_frame_type_nack;
    out_frame->header.flags    = 0U;

    return k_bb_err_ok;
}
