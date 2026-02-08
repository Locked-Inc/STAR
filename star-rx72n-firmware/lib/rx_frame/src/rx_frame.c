/* lib/rx_frame/src/rx_frame.c */

/**
 * @file rx_frame.c
 * @brief Frame Layer Implementation
 *
 * Implements frame encoding/decoding with CRC-32 verification.
 * Bit-exact compatible with star-gateway/internal/frame/.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_frame.h"

#include <string.h>

#include "rx_check.h"
#include "rx_crc.h"

/* =============================================================================
 * Byte Serialization Constants
 *
 * Named constants for byte manipulation in serialization functions.
 * Eliminates magic numbers and documents the bit/byte operations.
 * =============================================================================
 */

/**
 * @brief Bit shift amounts for extracting bytes from multi-byte values
 *
 * Each byte position requires shifting by (position * 8) bits.
 */
typedef enum : uint8_t {
  k_shift_byte_0 = 0,  /**< No shift needed for byte 0 (LSB) */
  k_shift_byte_1 = 8,  /**< Shift 8 bits for byte 1 */
  k_shift_byte_2 = 16, /**< Shift 16 bits for byte 2 */
  k_shift_byte_3 = 24, /**< Shift 24 bits for byte 3 (MSB) */
} byte_shift_t;

/**
 * @brief Initialization state values
 */
typedef enum : uint8_t {
  k_state_uninitialized = 0, /**< Object not initialized */
  k_state_initialized   = 1, /**< Object initialized and ready */
} init_state_t;

typedef enum : uint8_t {
  k_frame_offset_start = 0, /**< Start offset for frame parsing */
} frame_offset_t;
/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Write uint32 in little-endian format (for CRC-32)
 *
 * @param[out] buf     Output buffer (at least 4 bytes)
 * @param[in]  buf_len Size of output buffer in bytes (must be >= 4)
 * @param[in]  val     Value to write
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_invalid_arg if buf is NULL
 * @retval k_rx_err_invalid_size if buf_len < 4
 */
static rx_err_t internal_write_le32(uint8_t* buf, const uint32_t buf_len, const uint32_t val)
{
  RX_CHECK_NULL_PTR(buf, "FRAME", "LE32 write buffer is NULL");
  if (buf_len < k_frame_crc_size) {
    return k_rx_err_invalid_size;
  }

  buf[k_le32_byte_0] = (uint8_t)(val & k_rx_byte_mask);
  buf[k_le32_byte_1] = (uint8_t)((val >> k_shift_byte_1) & k_rx_byte_mask);
  buf[k_le32_byte_2] = (uint8_t)((val >> k_shift_byte_2) & k_rx_byte_mask);
  buf[k_le32_byte_3] = (uint8_t)((val >> k_shift_byte_3) & k_rx_byte_mask);
  return k_rx_ok;
}

/**
 * @brief Read uint32 in little-endian format (for CRC-32)
 *
 * @param[in]  buf     Input buffer (at least 4 bytes)
 * @param[in]  buf_len Size of input buffer in bytes (must be >= 4)
 * @param[out] out_val Pointer to store decoded value
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_invalid_arg if buf or out_val is NULL
 * @retval k_rx_err_invalid_size if buf_len < 4
 */
static rx_err_t internal_read_le32(const uint8_t* buf, const uint32_t buf_len, uint32_t* out_val)
{
  RX_CHECK_NULL_PTR(buf, "FRAME", "LE32 read buffer is NULL");
  RX_CHECK_NULL_PTR(out_val, "FRAME", "LE32 output pointer is NULL");
  if (buf_len < k_frame_crc_size) {
    return k_rx_err_invalid_size;
  }

  *out_val = (uint32_t)buf[k_le32_byte_0] | ((uint32_t)buf[k_le32_byte_1] << k_shift_byte_1) |
             ((uint32_t)buf[k_le32_byte_2] << k_shift_byte_2) |
             ((uint32_t)buf[k_le32_byte_3] << k_shift_byte_3);
  return k_rx_ok;
}

/**
 * @brief Decode frame header from raw data buffer
 *
 * Extracts and validates frame header fields (sync word, sequence, length, type, flags)
 * from the beginning of a raw data buffer. Verifies the sync word matches expected value
 * and validates payload length is within bounds. Outputs the offset where payload data
 * begins for subsequent processing.
 *
 * @param[in]  data       Input data buffer containing encoded frame
 * @param[in]  data_len   Length of input buffer in bytes
 * @param[out] frame      Frame structure to populate with decoded header fields
 * @param[out] offset_out Offset in buffer where payload begins (after header and sync)
 *
 * @return k_rx_ok on successful header decode
 * @retval k_rx_err_invalid_arg if data, frame, or offset_out is NULL
 * @retval k_rx_err_invalid_size if data_len is too small or decoded payload length is out of bounds
 * @retval k_rx_err_protocol_error if sync word does not match expected value (k_frame_sync_word)
 */
static rx_err_t internal_decode_header(const uint8_t* data,
                                       const uint32_t data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out)
{
  uint32_t offset;
  uint16_t sync_word;
  uint32_t expected_size;

  if (data == NULL || frame == NULL || offset_out == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (data_len < k_frame_min_size) {
    return k_rx_err_invalid_size;
  }

  offset = k_frame_offset_start;

  sync_word = rx_frame_read_be16(&data[offset]);
  if (sync_word != k_frame_sync_word) {
    return k_rx_err_protocol_error;
  }
  offset += k_frame_sync_size;

  frame->header.sequence = rx_frame_read_be16(&data[offset]);
  offset += k_frame_seq_size;

  frame->header.length = rx_frame_read_be16(&data[offset]);
  offset += k_frame_len_size;

  if (frame->header.length > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  expected_size = rx_frame_encoded_size(frame->header.length);
  if (data_len < expected_size) {
    return k_rx_err_invalid_size;
  }

  frame->header.type = data[offset];
  offset += k_frame_type_size;

  frame->header.flags = data[offset];
  offset += k_frame_flags_size;

  if (offset_out != NULL) {
    *offset_out = offset;
  }

  return k_rx_ok;
}

/**
 * @brief Verify CRC-32 checksum at specified offset in data buffer
 *
 * Reads the CRC-32 value stored at the specified offset in the buffer, calculates
 * the expected CRC over the data preceding it, and compares for match. Uses IEEE 802.3
 * CRC-32 polynomial (0x04C11DB7), compatible with Go's crc32.ChecksumIEEE(). On success,
 * outputs the received CRC value for caller inspection.
 *
 * @param[in]  data    Input data buffer containing payload followed by CRC
 * @param[in]  data_len Total length of input buffer in bytes
 * @param[in]  offset  Byte offset in buffer where CRC value is stored
 * @param[out] crc_out Pointer to store received CRC value from buffer
 *
 * @return k_rx_ok on successful CRC verification (received matches calculated)
 * @retval k_rx_err_invalid_arg if data or crc_out is NULL
 * @retval k_rx_err_invalid_size if offset + CRC size exceeds buffer length or data_len too small
 * @retval k_rx_err_crc_mismatch if calculated CRC does not match received CRC value
 */
static rx_err_t
internal_verify_crc(const uint8_t* data, uint32_t data_len, uint32_t offset, uint32_t* crc_out)
{
  uint32_t received_crc   = 0;
  uint32_t calculated_crc = 0;
  rx_err_t err;

  if (data == NULL || crc_out == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (data_len < k_frame_min_size || (offset + k_frame_crc_size) > data_len) {
    return k_rx_err_invalid_size;
  }

  err = internal_read_le32(&data[offset], data_len - offset, &received_crc);
  if (err != k_rx_ok) {
    return err;
  }
  calculated_crc = rx_crc32_ieee(data, offset);

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  *crc_out = received_crc;

  return k_rx_ok;
}

/* =============================================================================
 * Encoder API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize a frame encoder instance
 *
 * Prepares the encoder for use by setting the initialized flag and verifying
 * the initialization completed successfully (redundant validation per Rule 5).
 *
 * @param[out] enc Encoder instance to initialize
 *
 * @return k_rx_ok on successful initialization
 * @retval k_rx_err_invalid_arg if enc is NULL
 * @retval k_rx_err_validation_failed if initialization verification fails
 */
rx_err_t rx_frame_encoder_init(rx_frame_encoder_t* enc)
{
  if (enc == NULL) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = k_state_initialized;
  if (enc->initialized != k_state_initialized) {
    return k_rx_err_validation_failed;
  }
  return k_rx_ok;
}

/**
 * @brief Deinitialize a frame encoder instance
 *
 * Marks the encoder as uninitialized, preventing further use.
 * Verifies deinitialization completed successfully (redundant validation per Rule 5).
 *
 * @param[out] enc Encoder instance to deinitialize
 *
 * @return k_rx_ok on successful deinitialization
 * @retval k_rx_err_invalid_arg if enc is NULL
 * @retval k_rx_err_validation_failed if deinitialization verification fails
 */
rx_err_t rx_frame_encoder_deinit(rx_frame_encoder_t* enc)
{
  if (enc == NULL) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = k_state_uninitialized;
  if (enc->initialized != k_state_uninitialized) {
    return k_rx_err_validation_failed;
  }
  return k_rx_ok;
}

rx_err_t rx_frame_encode(const rx_frame_encoder_t* enc,
                         const rx_frame_t*         frame,
                         uint8_t*                  output,
                         uint32_t*                 output_len)
{
  uint32_t frame_size;
  uint32_t offset;
  uint32_t crc;
  rx_err_t err;

  if (enc == NULL || frame == NULL || output == NULL || output_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (enc->initialized != k_state_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Validate payload size */
  if (frame->header.length > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  /* Calculate total frame size */
  frame_size = rx_frame_encoded_size(frame->header.length);
  offset     = k_frame_offset_start;

  /* Write SYNC word (big-endian) */
  rx_frame_write_be16(&output[offset], k_frame_sync_word);
  offset += k_frame_sync_size;

  /* Write SEQ (big-endian, network byte order per RFC 1700) */
  rx_frame_write_be16(&output[offset], frame->header.sequence);
  offset += k_frame_seq_size;

  /* Write LEN (big-endian, network byte order per RFC 1700) */
  rx_frame_write_be16(&output[offset], frame->header.length);
  offset += k_frame_len_size;

  /* Write TYPE (1 byte) */
  output[offset] = frame->header.type;
  offset += k_frame_type_size;

  /* Write FLAGS (1 byte) */
  output[offset] = frame->header.flags;
  offset += k_frame_flags_size;

  /* Write PAYLOAD */
  if (frame->header.length > 0) {
    memcpy(&output[offset], frame->payload, frame->header.length);
    offset += frame->header.length;
  }

  /* Calculate CRC-32 over SYNC + Header + Payload (IEEE 802.3 polynomial) */
  crc = rx_crc32_ieee(output, offset);

  /* Write CRC-32 (little-endian to match IEEE 802.3 LSB-first order) */
  err = internal_write_le32(&output[offset], frame_size - offset, crc);
  if (err != k_rx_ok) {
    return err;
  }
  offset += k_frame_crc_size;

  *output_len = frame_size;
  return k_rx_ok;
}

/* =============================================================================
 * Decoder API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize a frame decoder instance
 *
 * Prepares the decoder for use by setting the initialized flag and verifying
 * the initialization completed successfully (redundant validation per Rule 5).
 *
 * @param[out] dec Decoder instance to initialize
 *
 * @return k_rx_ok on successful initialization
 * @retval k_rx_err_invalid_arg if dec is NULL
 * @retval k_rx_err_validation_failed if initialization verification fails
 */
rx_err_t rx_frame_decoder_init(rx_frame_decoder_t* dec)
{
  if (dec == NULL) {
    return k_rx_err_invalid_arg;
  }

  dec->initialized = k_state_initialized;
  if (dec->initialized != k_state_initialized) {
    return k_rx_err_validation_failed;
  }
  return k_rx_ok;
}

/**
 * @brief Deinitialize a frame decoder instance
 *
 * Marks the decoder as uninitialized, preventing further use.
 * Verifies deinitialization completed successfully (redundant validation per Rule 5).
 *
 * @param[out] dec Decoder instance to deinitialize
 *
 * @return k_rx_ok on successful deinitialization
 * @retval k_rx_err_invalid_arg if dec is NULL
 * @retval k_rx_err_validation_failed if deinitialization verification fails
 */
rx_err_t rx_frame_decoder_deinit(rx_frame_decoder_t* dec)
{
  if (dec == NULL) {
    return k_rx_err_invalid_arg;
  }

  dec->initialized = k_state_uninitialized;
  if (dec->initialized != k_state_uninitialized) {
    return k_rx_err_validation_failed;
  }
  return k_rx_ok;
}

/**
 * @brief Decode a frame from raw data
 *
 * Validates the decoder state, extracts the frame header, copies the payload,
 * and verifies the CRC-32 checksum.
 *
 * @param[in]  dec       Initialized frame decoder instance
 * @param[in]  data      Raw frame data buffer
 * @param[in]  data_len  Length of data buffer in bytes
 * @param[out] frame     Decoded frame (header, payload, CRC)
 *
 * @retval k_rx_ok Success - frame decoded and CRC verified
 * @retval k_rx_err_invalid_arg Any pointer parameter is NULL or other invalid arguments
 * @retval k_rx_err_invalid_state Decoder not initialized
 * @retval k_rx_err_crc CRC-32 verification failed
 * @retval k_rx_err_invalid_size Payload exceeds maximum frame size
 *
 * @note This function performs validation at multiple points:
 *       - Null pointer checks on all parameters
 *       - Decoder initialization check
 *       - Header parsing via internal_decode_header()
 *       - CRC verification via internal_verify_crc()
 */
rx_err_t rx_frame_decode(const rx_frame_decoder_t* dec,
                         const uint8_t*            data,
                         const uint32_t            data_len,
                         rx_frame_t*               frame)
{
  uint32_t offset;
  rx_err_t err;

  if (dec == NULL || data == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (dec->initialized != k_state_initialized) {
    return k_rx_err_invalid_state;
  }

  err = internal_decode_header(data, data_len, frame, &offset);
  if (err != k_rx_ok) {
    return err;
  }

  /* Read PAYLOAD */
  if (frame->header.length > 0) {
    memcpy(frame->payload, &data[offset], frame->header.length);
    offset += frame->header.length;
  }

  err = internal_verify_crc(data, data_len, offset, &frame->crc);
  if (err != k_rx_ok) {
    return err;
  }
  return k_rx_ok;
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

/**
 * @brief Create an ACK (acknowledgment) frame
 *
 * Constructs a frame with type=ACK, empty payload, and the specified sequence number.
 * The ACK response frames are used by the receiver to confirm successful reception
 * of a command or data frame from the sender.
 *
 * @param[out] frame    Frame structure to populate with ACK
 * @param[in]  sequence Sequence number of the frame being acknowledged
 *
 * @return k_rx_ok on successful frame creation
 * @retval k_rx_err_invalid_arg if frame is NULL
 * @retval k_rx_err_validation_failed if ACK type verification fails
 */
rx_err_t rx_frame_create_ack(rx_frame_t* frame, const uint16_t sequence)
{
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_ack;
  frame->header.flags    = k_frame_flag_none;

  if (frame->header.type != k_frame_type_ack) {
    return k_rx_err_validation_failed;
  }

  return k_rx_ok;
}

/**
 * @brief Create a NACK (negative acknowledgment) frame
 *
 * Constructs a frame with type=NACK, empty payload, the specified sequence number,
 * and error/status flags. NACK frames are sent by the receiver to indicate that a
 * frame was not processed successfully due to an error condition (specified in flags).
 *
 * @param[out] frame    Frame structure to populate with NACK
 * @param[in]  sequence Sequence number of the frame being negative-acknowledged
 * @param[in]  flags    Status/error flags indicating reason for NACK
 *
 * @return k_rx_ok on successful frame creation
 * @retval k_rx_err_invalid_arg if frame is NULL
 * @retval k_rx_err_validation_failed if NACK type verification fails
 */
rx_err_t rx_frame_create_nack(rx_frame_t* frame, const uint16_t sequence, uint8_t flags)
{
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_nack;
  frame->header.flags    = flags;

  if (frame->header.type != k_frame_type_nack) {
    return k_rx_err_validation_failed;
  }

  return k_rx_ok;
}

rx_err_t rx_frame_create_ping(rx_frame_t*    frame,
                               const uint16_t sequence,
                               const uint8_t* payload,
                               const uint32_t payload_len)
{
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > 0 && payload == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = k_frame_type_ping;
  frame->header.flags    = k_frame_flag_none;

  if (payload_len > 0) {
    memcpy(frame->payload, payload, payload_len);
  }

  if (frame->header.type != k_frame_type_ping) {
    return k_rx_err_validation_failed;
  }

  return k_rx_ok;
}

rx_err_t rx_frame_create_pong(rx_frame_t*    frame,
                               const uint16_t sequence,
                               const uint8_t* payload,
                               const uint32_t payload_len)
{
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > 0 && payload == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = k_frame_type_pong;
  frame->header.flags    = k_frame_flag_none;

  if (payload_len > 0) {
    memcpy(frame->payload, payload, payload_len);
  }

  if (frame->header.type != k_frame_type_pong) {
    return k_rx_err_validation_failed;
  }

  return k_rx_ok;
}

rx_err_t rx_frame_create_reset(rx_frame_t* frame, const uint16_t sequence)
{
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_reset;
  frame->header.flags    = k_frame_flag_none;

  if (frame->header.type != k_frame_type_reset) {
    return k_rx_err_validation_failed;
  }

  return k_rx_ok;
}

rx_err_t rx_frame_create_reset_ack(rx_frame_t* frame, const uint16_t sequence)
{
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  memset(frame, 0, sizeof(rx_frame_t));
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_reset_ack;
  frame->header.flags    = k_frame_flag_none;

  if (frame->header.type != k_frame_type_reset_ack) {
    return k_rx_err_validation_failed;
  }

  return k_rx_ok;
}
