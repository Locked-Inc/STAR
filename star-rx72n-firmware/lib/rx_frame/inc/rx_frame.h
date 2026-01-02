/* lib/rx_frame/inc/rx_frame.h */

/**
 * @file rx_frame.h
 * @brief Frame Layer for SPI Protocol
 *
 * Implements frame encoding/decoding with CRC-32 verification.
 * Bit-exact compatible with star-gateway/internal/frame/.
 *
 * Frame format (all multi-byte fields in network byte order / big-endian):
 * [SYNC(2B, BE)][SEQ(2B, BE)][LEN(2B, BE)][TYPE(1B)][FLAGS(1B)][PAYLOAD(0-1KB)][CRC-32(4B, LE)]
 *
 * CRC-32 is calculated over SYNC + Header + Payload (IEEE 802.3 polynomial).
 * CRC-32 is written in little-endian format to match IEEE 802.3 LSB-first order.
 *
 * @see star-gateway/internal/frame/ for Go reference implementation
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_FRAME_H
#define STAR_RX_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Frame Constants (must match Go implementation exactly)
 * =============================================================================
 */

/**
 * @brief Frame structure constants
 */
typedef enum {
  k_frame_sync_word   = (0x55AA), /**< Frame sync marker */
  k_frame_sync_size   = (2),      /**< SYNC field size */
  k_frame_seq_size    = (2),      /**< SEQ field size */
  k_frame_len_size    = (2),      /**< LEN field size */
  k_frame_type_size   = (1),      /**< TYPE field size */
  k_frame_flags_size  = (1),      /**< FLAGS field size */
  k_frame_crc_size    = (4),      /**< CRC-32 field size */
  k_frame_header_size = (6),      /**< SEQ+LEN+TYPE+FLAGS */
  k_frame_max_payload = (1024),   /**< Maximum payload bytes */
  k_frame_min_size    = (12),     /**< SYNC+Header+CRC (no payload) */
  k_frame_max_size    = (1036),   /**< Min + MaxPayload */
} rx_frame_constants_t;

/**
 * @brief Frame types (matches Go FrameType)
 */
typedef enum {
  k_frame_type_unknown  = (0), /**< Invalid frame type */
  k_frame_type_command  = (1), /**< Command from controller */
  k_frame_type_response = (2), /**< Response from peripheral */
  k_frame_type_ack      = (3), /**< Acknowledgment */
  k_frame_type_nack     = (4), /**< Negative acknowledgment */
} rx_frame_type_t;

/**
 * @brief Frame flags (matches Go FrameFlags)
 */
typedef enum {
  k_frame_flag_none         = (0x00), /**< No flags */
  k_frame_flag_requires_ack = (0x01), /**< Frame requires ACK */
  k_frame_flag_retransmit   = (0x02), /**< Retransmission */
  k_frame_flag_priority     = (0x04), /**< High priority */
  k_frame_flag_fec_enabled  = (0x08), /**< FEC encoded payload */
  k_frame_flag_soft_nack    = (0x10), /**< NACK with soft bits */
} rx_frame_flags_t;

/* =============================================================================
 * Frame Structures
 * =============================================================================
 */

/**
 * @brief Frame header structure
 */
typedef struct {
  uint16_t sequence; /**< Sequence number (big-endian on wire) */
  uint16_t length;   /**< Payload length (big-endian on wire) */
  uint8_t  type;     /**< Frame type */
  uint8_t  flags;    /**< Frame flags */
} rx_frame_header_t;

/**
 * @brief Complete frame structure
 */
typedef struct {
  rx_frame_header_t header;                       /**< Frame header */
  uint8_t           payload[k_frame_max_payload]; /**< Payload buffer */
  uint32_t          crc;                          /**< CRC-32 checksum */
} rx_frame_t;

/**
 * @brief Frame encoder handle
 */
typedef struct {
  uint8_t initialized; /**< Non-zero if initialized */
} rx_frame_encoder_t;

/**
 * @brief Frame decoder handle
 */
typedef struct {
  uint8_t initialized; /**< Non-zero if initialized */
} rx_frame_decoder_t;

/* =============================================================================
 * Encoder API
 * =============================================================================
 */

/**
 * @brief Initialize frame encoder
 *
 * @param[out] enc Pointer to encoder handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if enc is NULL
 */
rx_err_t rx_frame_encoder_init(rx_frame_encoder_t* enc);

/**
 * @brief Deinitialize frame encoder
 *
 * @param[in,out] enc Pointer to encoder handle
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_encoder_deinit(rx_frame_encoder_t* enc);

/**
 * @brief Encode frame to wire format
 *
 * Serializes frame to wire format with SYNC, header, payload, and CRC-32.
 * Multi-byte fields are big-endian except CRC-32 which is little-endian.
 *
 * @param[in]  enc        Encoder handle
 * @param[in]  frame      Frame to encode
 * @param[out] output     Output buffer (min k_frame_min_size + payload bytes)
 * @param[out] output_len Actual number of bytes written
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if encoder not initialized
 * @return k_rx_err_invalid_size if payload exceeds max
 */
rx_err_t rx_frame_encode(rx_frame_encoder_t* enc,
                         const rx_frame_t*   frame,
                         uint8_t*            output,
                         uint32_t*           output_len);

/* =============================================================================
 * Utility Functions (Static Inline)
 *
 * NOTE: These are intentionally static inline in the header because:
 * 1. They are trivial one-liner calculations (single arithmetic operation)
 * 2. They may be called in performance-sensitive code paths
 * 3. They need to be available across multiple translation units
 * 4. Function call overhead would exceed the actual computation cost
 *
 * For such trivial operations, static inline is the standard C pattern and
 * results in better performance with no binary size increase.
 * =============================================================================
 */

/**
 * @brief Calculate encoded frame size
 *
 * @param[in] payload_len Payload length
 * @return Total frame size in bytes
 */
static inline uint32_t rx_frame_encoded_size(uint32_t payload_len)
{
  return k_frame_sync_size + k_frame_header_size + payload_len + k_frame_crc_size;
}

/* =============================================================================
 * Byte Ordering Utilities (Static Inline)
 *
 * Big-endian read/write for network byte order serialization.
 * Used by both rx_frame and rx_usb_comm modules.
 * =============================================================================
 */

/**
 * @brief Byte manipulation constants for endianness conversions
 */
typedef enum {
  k_rx_be16_high_shift = (8),     /**< Bit shift for high byte in 16-bit value */
  k_rx_byte_mask       = (0xFFU), /**< Mask to extract one byte */
} rx_byte_order_constants_t;

/**
 * @brief Read uint16 from big-endian buffer
 *
 * @param[in] buf Input buffer (at least 2 bytes)
 * @return Decoded 16-bit value
 */
static inline uint16_t rx_read_be16(const uint8_t* buf)
{
  return ((uint16_t)buf[0] << k_rx_be16_high_shift) | (uint16_t)buf[1];
}

/**
 * @brief Write uint16 to big-endian buffer
 *
 * @param[out] buf Output buffer (at least 2 bytes)
 * @param[in]  val Value to write
 */
static inline void rx_write_be16(uint8_t* buf, uint16_t val)
{
  buf[0] = (uint8_t)(val >> k_rx_be16_high_shift);
  buf[1] = (uint8_t)(val & k_rx_byte_mask);
}

/* =============================================================================
 * Decoder API
 * =============================================================================
 */

/**
 * @brief Initialize frame decoder
 *
 * @param[out] dec Pointer to decoder handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if dec is NULL
 */
rx_err_t rx_frame_decoder_init(rx_frame_decoder_t* dec);

/**
 * @brief Deinitialize frame decoder
 *
 * @param[in,out] dec Pointer to decoder handle
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_decoder_deinit(rx_frame_decoder_t* dec);

/**
 * @brief Decode wire format to frame
 *
 * Parses wire format and validates SYNC word and CRC-32.
 *
 * @param[in]  dec      Decoder handle
 * @param[in]  data     Wire format data
 * @param[in]  data_len Data length
 * @param[out] frame    Decoded frame
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if decoder not initialized
 * @return k_rx_err_invalid_size if data too short
 * @return k_rx_err_protocol_error if SYNC word invalid
 * @return k_rx_err_crc_mismatch if CRC validation fails
 */
rx_err_t
rx_frame_decode(rx_frame_decoder_t* dec, const uint8_t* data, uint32_t data_len, rx_frame_t* frame);

/* =============================================================================
 * CRC-32 Functions
 * =============================================================================
 */

/**
 * @brief Calculate IEEE 802.3 CRC-32
 *
 * Uses polynomial 0x04C11DB7 (reflected: 0xEDB88320).
 * Compatible with Go's crc32.ChecksumIEEE().
 *
 * @param[in] data Input data
 * @param[in] len Data length
 * @return CRC-32 checksum
 */
uint32_t rx_crc32_ieee(const uint8_t* data, uint32_t len);

/**
 * @brief Update CRC-32 with additional data
 *
 * Allows incremental CRC calculation.
 *
 * @param[in] crc Previous CRC value (or 0 for first call)
 * @param[in] data Additional data
 * @param[in] len Data length
 * @return Updated CRC-32 checksum
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len);

/* =============================================================================
 * Frame Helper Functions
 * =============================================================================
 */

/**
 * @brief Create ACK frame for given sequence
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number to acknowledge
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_create_ack(rx_frame_t* frame, uint16_t sequence);

/**
 * @brief Create NACK frame for given sequence
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number
 * @param[in]  flags Additional flags (e.g., k_frame_flag_soft_nack)
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_create_nack(rx_frame_t* frame, uint16_t sequence, uint8_t flags);

/**
 * @brief Check if frame type is valid
 *
 * Static inline for same reasons as rx_frame_encoded_size() above.
 *
 * @param[in] type Frame type to check
 * @return true if valid, false otherwise
 */
static inline bool rx_frame_type_valid(uint8_t type)
{
  return (type >= k_frame_type_command && type <= k_frame_type_nack);
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_FRAME_H */
