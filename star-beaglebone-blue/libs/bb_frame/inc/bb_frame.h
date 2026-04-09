/**
 * @file bb_frame.h
 * @brief Wire frame codec and streaming decoder for BeagleBone Blue.
 *
 * @details
 * Implements the STAR binary frame protocol, identical to the RX72N target:
 *
 * | SYNC(2B,LE) | SEQ(2B,LE) | LEN(2B,LE) | TYPE(1B) | FLAGS(1B) | PAYLOAD(0-1024B) | CRC32(4B,LE) |
 *
 * CRC-32/IEEE 802.3 covers SYNC through end of PAYLOAD.
 * All multi-byte fields are little-endian on wire.
 *
 * The streaming decoder (bb_frame_decoder_t) processes one byte at a time from
 * USB CDC input, automatically hunting for SYNC and re-syncing on CRC failure.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bb_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_frame_size_t
 * @brief Frame dimension constants.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
    k_bb_frame_sync_size    = 2U,    /**< SYNC word size in bytes */
    k_bb_frame_seq_size     = 2U,    /**< Sequence number size */
    k_bb_frame_len_size     = 2U,    /**< Payload length field size */
    k_bb_frame_type_size    = 1U,    /**< Frame type field size */
    k_bb_frame_flags_size   = 1U,    /**< Flags field size */
    k_bb_frame_crc_size     = 4U,    /**< CRC-32 field size */
    k_bb_frame_header_size  = 6U,    /**< SEQ + LEN + TYPE + FLAGS */
    k_bb_frame_overhead     = 12U,   /**< SYNC + header + CRC (min frame) */
    k_bb_frame_max_payload  = 1024U, /**< Maximum payload length */
} bb_frame_size_t;

/**
 * @enum bb_frame_sync_t
 * @brief Sync word constant.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
    k_bb_frame_sync_word = 0x55AAU, /**< Frame sync marker (LE on wire: 0xAA 0x55) */
} bb_frame_sync_t;

/**
 * @enum bb_frame_max_t
 * @brief Maximum frame size.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_bb_frame_max_size = 1036U, /**< SYNC + header + max payload + CRC */
} bb_frame_max_t;

/**
 * @enum bb_frame_type_t
 * @brief Frame type identifiers.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_frame_type_ping      = 0x00U, /**< Keepalive request */
    k_bb_frame_type_pong      = 0x01U, /**< Keepalive response */
    k_bb_frame_type_command   = 0x10U, /**< Command (RPi5 -> BBBlue) */
    k_bb_frame_type_response  = 0x11U, /**< Response (BBBlue -> RPi5) */
    k_bb_frame_type_ack       = 0x12U, /**< Acknowledgment */
    k_bb_frame_type_nack      = 0x13U, /**< Negative acknowledgment */
    k_bb_frame_type_reset_ack = 0xFEU, /**< Reset acknowledgment */
    k_bb_frame_type_reset     = 0xFFU, /**< State reset request */
} bb_frame_type_t;

/**
 * @enum bb_frame_flags_t
 * @brief Frame flag bits.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_frame_flag_requires_ack = 0x01U, /**< Sender expects ACK/NACK */
    k_bb_frame_flag_retransmit   = 0x02U, /**< This is a retransmission */
    k_bb_frame_flag_priority     = 0x04U, /**< High-priority frame */
    k_bb_frame_flag_fec_enabled  = 0x08U, /**< Forward error correction enabled */
    k_bb_frame_flag_soft_nack    = 0x10U, /**< NACK with recoverable error info */
} bb_frame_flags_t;

/* ---------------------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------------------*/

/**
 * @struct bb_frame_header_t
 * @brief Parsed frame header fields.
 *
 * @since Version 1.0.0
 */
typedef struct {
    uint16_t sequence; /**< Sequence number */
    uint16_t length;   /**< Payload length in bytes */
    uint8_t  type;     /**< Frame type (bb_frame_type_t) */
    uint8_t  flags;    /**< Control flags (bb_frame_flags_t bitmask) */
} bb_frame_header_t;

/**
 * @struct bb_frame_t
 * @brief Complete parsed frame.
 *
 * @since Version 1.0.0
 */
typedef struct {
    bb_frame_header_t header;                        /**< Parsed header */
    uint8_t           payload[k_bb_frame_max_payload]; /**< Payload data */
    uint32_t          crc;                           /**< CRC-32 value */
} bb_frame_t;

/**
 * @enum bb_frame_decoder_state_t
 * @brief Streaming decoder state machine states.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_bb_frame_state_hunt_sync_lo = 0U, /**< Waiting for SYNC low byte (0xAA) */
    k_bb_frame_state_hunt_sync_hi = 1U, /**< Waiting for SYNC high byte (0x55) */
    k_bb_frame_state_read_header  = 2U, /**< Accumulating 6 header bytes */
    k_bb_frame_state_read_payload = 3U, /**< Accumulating payload bytes */
    k_bb_frame_state_read_crc     = 4U, /**< Accumulating 4 CRC bytes */
    k_bb_frame_state_complete     = 5U, /**< Frame ready for retrieval */
} bb_frame_decoder_state_t;

/**
 * @struct bb_frame_decoder_t
 * @brief Streaming frame decoder for byte-at-a-time USB CDC input.
 *
 * @details
 * State machine that processes one byte at a time. Automatically hunts for
 * SYNC word and re-syncs on CRC failure.
 *
 * @since Version 1.0.0
 */
typedef struct {
    bb_frame_decoder_state_t state;                     /**< Current state */
    uint8_t  header_buf[k_bb_frame_header_size];        /**< Header accumulator */
    uint32_t header_pos;                                /**< Bytes received in header */
    uint32_t payload_pos;                               /**< Bytes received in payload */
    uint32_t crc_pos;                                   /**< Bytes received in CRC */
    uint8_t  crc_buf[k_bb_frame_crc_size];              /**< CRC accumulator */
    bb_frame_t frame;                                   /**< Assembled frame */
    bool     initialized;                               /**< Init flag */
} bb_frame_decoder_t;

/* ---------------------------------------------------------------------------
 * One-shot encode / decode
 * ---------------------------------------------------------------------------*/

/**
 * @brief Encode a frame to wire format.
 *
 * @param[in]  frame    Frame to encode (header + payload must be populated)
 * @param[out] out_buf  Output buffer for wire bytes
 * @param[in]  out_size Size of output buffer (must be >= 12 + frame->header.length)
 * @param[out] out_len  Actual number of wire bytes written
 *
 * @return bb_err_t
 * @retval k_bb_err_ok          Success
 * @retval k_bb_err_null_ptr    NULL argument
 * @retval k_bb_err_invalid_arg Payload length > max or buffer too small
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_encode(const bb_frame_t* frame, uint8_t* out_buf,
                         uint32_t out_size, uint32_t* out_len);

/**
 * @brief Decode a frame from wire format.
 *
 * @param[in]  wire_data Wire bytes
 * @param[in]  wire_len  Number of wire bytes
 * @param[out] out_frame Parsed frame
 *
 * @return bb_err_t
 * @retval k_bb_err_ok            Success
 * @retval k_bb_err_null_ptr      NULL argument
 * @retval k_bb_err_invalid_arg   Too few bytes or invalid sync/length
 * @retval k_bb_err_crc_mismatch  CRC validation failed
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_decode(const uint8_t* wire_data, uint32_t wire_len,
                         bb_frame_t* out_frame);

/* ---------------------------------------------------------------------------
 * Streaming decoder
 * ---------------------------------------------------------------------------*/

/**
 * @brief Initialize the streaming frame decoder.
 *
 * @param[out] dec Decoder instance
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Success
 * @retval k_bb_err_null_ptr dec is NULL
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_decoder_init(bb_frame_decoder_t* dec);

/**
 * @brief Feed one byte to the streaming decoder.
 *
 * @param[in,out] dec         Decoder instance
 * @param[in]     byte        Next wire byte
 * @param[out]    frame_ready Set to true when a complete valid frame is available
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success (frame_ready indicates completion)
 * @retval k_bb_err_null_ptr        NULL argument
 * @retval k_bb_err_not_initialized Decoder not initialized
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_decoder_feed(bb_frame_decoder_t* dec, uint8_t byte,
                               bool* frame_ready);

/**
 * @brief Retrieve the completed frame from the decoder.
 *
 * @param[in]  dec       Decoder instance (must be in COMPLETE state)
 * @param[out] out_frame Copy of the completed frame
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success
 * @retval k_bb_err_null_ptr        NULL argument
 * @retval k_bb_err_not_initialized Decoder not initialized
 * @retval k_bb_err_invalid_arg     No complete frame available
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_decoder_get_frame(const bb_frame_decoder_t* dec,
                                    bb_frame_t* out_frame);

/**
 * @brief Reset the decoder to HUNT_SYNC state.
 *
 * @param[in,out] dec Decoder instance
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Success
 * @retval k_bb_err_null_ptr dec is NULL
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_decoder_reset(bb_frame_decoder_t* dec);

/* ---------------------------------------------------------------------------
 * Convenience builders
 * ---------------------------------------------------------------------------*/

/**
 * @brief Build an ACK frame for a given sequence number.
 *
 * @param[in]  seq       Sequence number to acknowledge
 * @param[out] out_frame ACK frame (type=ACK, length=0)
 *
 * @return bb_err_t
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_build_ack(uint16_t seq, bb_frame_t* out_frame);

/**
 * @brief Build a NACK frame for a given sequence number.
 *
 * @param[in]  seq       Sequence number to negative-acknowledge
 * @param[out] out_frame NACK frame (type=NACK, length=0)
 *
 * @return bb_err_t
 *
 * @since Version 1.0.0
 */
bb_err_t bb_frame_build_nack(uint16_t seq, bb_frame_t* out_frame);

#ifdef __cplusplus
}
#endif
