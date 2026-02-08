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
typedef enum : uint16_t {
  k_frame_sync_word   = (0x55AA), /**< Frame sync marker */
  k_frame_sync_size   = (2),      /**< SYNC field size */
  k_frame_seq_size    = (2),      /**< SEQ field size */
  k_frame_len_size    = (2),      /**< LEN field size */
  k_frame_type_size   = (1),      /**< TYPE field size */
  k_frame_flags_size  = (1),      /**< FLAGS field size */
  k_frame_crc_size    = (4),      /**< CRC-32 field size */
  k_frame_header_size = (6),      /**< SEQ+LEN+TYPE+FLAGS */
  k_frame_min_payload = (0),      /**< Minimum payload bytes */
  k_frame_max_payload = (1024),   /**< Maximum payload bytes */
  k_frame_min_size    = (12),     /**< SYNC+Header+CRC (no payload) */
  k_frame_max_size    = (1036),   /**< Min + MaxPayload */
} rx_frame_constants_t;

/**
 * @brief Frame types (matches Go FrameType in star-gateway/internal/frame/)
 *
 * Ranges:
 *   0x00-0x0F: Control/heartbeat frames
 *   0x10-0x1F: Data/command frames
 *   0xFx:      Session management frames
 */
typedef enum : uint8_t {
  k_frame_type_ping      = (0x00), /**< Heartbeat request */
  k_frame_type_pong      = (0x01), /**< Heartbeat response */
  k_frame_type_command   = (0x10), /**< Command from controller */
  k_frame_type_response  = (0x11), /**< Response from peripheral */
  k_frame_type_ack       = (0x12), /**< Acknowledgment */
  k_frame_type_nack      = (0x13), /**< Negative acknowledgment */
  k_frame_type_unknown   = (0x14), /**< Unknown/invalid frame type */
  k_frame_type_reset_ack = (0xFE), /**< Session reset acknowledgment */
  k_frame_type_reset     = (0xFF), /**< Session reset */
} rx_frame_type_t;

/**
 * @brief Frame flags (matches Go FrameFlags)
 */
typedef enum : uint8_t {
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
rx_err_t rx_frame_encode(const rx_frame_encoder_t* enc,
                         const rx_frame_t*         frame,
                         uint8_t*                  output,
                         uint32_t*                 output_len);

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
 *
 * Big-endian format stores the most significant byte (MSB) at the lowest
 * memory address. For a 16-bit value:
 *   - buf[0] = high byte (MSB)
 *   - buf[1] = low byte (LSB)
 *
 * Example: 0x55AA stored in big-endian:
 *   buf[0] = 0x55 (high byte)
 *   buf[1] = 0xAA (low byte)
 * =============================================================================
 */

/**
 * @brief Byte indices for 16-bit big-endian serialization
 *
 * In big-endian format, the high byte (MSB) is stored at index 0 and the
 * low byte (LSB) is stored at index 1.
 */
typedef enum : uint8_t {
  k_be16_byte_high = 0, /**< High byte (MSB) at index 0 */
  k_be16_byte_low  = 1, /**< Low byte (LSB) at index 1 */
} rx_be16_byte_idx_t;

/**
 * @brief Byte manipulation constants for endianness conversions
 */
typedef enum : uint8_t {
  k_rx_be16_high_shift = (8),     /**< Bit shift for high byte in 16-bit value */
  k_rx_byte_mask       = (0xFFU), /**< Mask to extract one byte */
} rx_byte_order_constants_t;

/**
 * @brief Byte indices for 32-bit little-endian serialization
 */
typedef enum : uint8_t {
  k_le32_byte_0 = 0, /**< Byte 0 (LSB) */
  k_le32_byte_1 = 1, /**< Byte 1 */
  k_le32_byte_2 = 2, /**< Byte 2 */
  k_le32_byte_3 = 3, /**< Byte 3 (MSB) */
} rx_le32_byte_idx_t;

/**
 * @brief Bit shift amounts for extracting bytes from 32-bit values
 */
typedef enum : uint8_t {
  k_rx_le32_shift_0 = 0,  /**< Shift 0 bits for byte 0 */
  k_rx_le32_shift_1 = 8,  /**< Shift 8 bits for byte 1 */
  k_rx_le32_shift_2 = 16, /**< Shift 16 bits for byte 2 */
  k_rx_le32_shift_3 = 24, /**< Shift 24 bits for byte 3 */
} rx_le32_shift_t;

/**
 * @brief Read uint16 from big-endian buffer
 *
 * Reads a 16-bit unsigned integer from a buffer in big-endian (network byte
 * order) format. The high byte (MSB) is at index 0, the low byte (LSB) is at
 * index 1.
 *
 * @param[in] buf Input buffer (at least 2 bytes)
 * @return Decoded 16-bit value in host byte order
 *
 * @note This is the preferred function for parsing multi-byte protocol fields.
 */
static inline uint16_t rx_frame_read_be16(const uint8_t* buf)
{
  return ((uint16_t)buf[k_be16_byte_high] << k_rx_be16_high_shift) | (uint16_t)buf[k_be16_byte_low];
}

/**
 * @brief Write uint16 to big-endian buffer
 *
 * Writes a 16-bit unsigned integer to a buffer in big-endian (network byte
 * order) format. The high byte (MSB) is written at index 0, the low byte (LSB)
 * is written at index 1.
 *
 * @param[out] buf Output buffer (at least 2 bytes)
 * @param[in]  val Value to write in host byte order
 *
 * @note This is the preferred function for serializing multi-byte protocol
 * fields.
 */
static inline void rx_frame_write_be16(uint8_t* buf, uint16_t val)
{
  buf[k_be16_byte_high] = (uint8_t)(val >> k_rx_be16_high_shift);
  buf[k_be16_byte_low]  = (uint8_t)(val & k_rx_byte_mask);
}

/**
 * @brief Read uint32 from little-endian buffer
 *
 * Reads a 32-bit unsigned integer from a buffer in little-endian format.
 *
 * @param[in] buf Input buffer (at least 4 bytes)
 * @return Decoded 32-bit value in host byte order
 */
static inline uint32_t rx_frame_read_le32(const uint8_t* buf)
{
  return (uint32_t)buf[k_le32_byte_0] | ((uint32_t)buf[k_le32_byte_1] << k_rx_le32_shift_1) |
         ((uint32_t)buf[k_le32_byte_2] << k_rx_le32_shift_2) |
         ((uint32_t)buf[k_le32_byte_3] << k_rx_le32_shift_3);
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
rx_err_t rx_frame_decode(const rx_frame_decoder_t* dec,
                         const uint8_t*            data,
                         const uint32_t            data_len,
                         rx_frame_t*               frame);

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
 * @brief Create PING frame with optional payload
 *
 * @param[out] frame       Frame to initialize
 * @param[in]  sequence    Sequence number
 * @param[in]  payload     Payload data (e.g., 4-byte counter), may be NULL if payload_len is 0
 * @param[in]  payload_len Payload length in bytes
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_create_ping(rx_frame_t*    frame,
                               uint16_t       sequence,
                               const uint8_t* payload,
                               uint32_t       payload_len);

/**
 * @brief Create PONG frame echoing payload
 *
 * @param[out] frame       Frame to initialize
 * @param[in]  sequence    Sequence number
 * @param[in]  payload     Payload to echo back, may be NULL if payload_len is 0
 * @param[in]  payload_len Payload length in bytes
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_create_pong(rx_frame_t*    frame,
                               uint16_t       sequence,
                               const uint8_t* payload,
                               uint32_t       payload_len);

/**
 * @brief Create session RESET frame
 *
 * @param[out] frame    Frame to initialize
 * @param[in]  sequence Sequence number
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_create_reset(rx_frame_t* frame, uint16_t sequence);

/**
 * @brief Create session RESET_ACK frame
 *
 * @param[out] frame    Frame to initialize
 * @param[in]  sequence Sequence number
 * @return k_rx_ok on success
 */
rx_err_t rx_frame_create_reset_ack(rx_frame_t* frame, uint16_t sequence);

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
  return (type == k_frame_type_ping) || (type == k_frame_type_pong) ||
         (type >= k_frame_type_command && type <= k_frame_type_nack) ||
         (type == k_frame_type_reset_ack) || (type == k_frame_type_reset);
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_FRAME_H */
