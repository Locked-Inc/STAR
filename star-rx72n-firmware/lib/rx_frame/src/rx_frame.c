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

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Write uint32 in little-endian format (for CRC-32)
 *
 * @param[out] buf Output buffer (at least 4 bytes)
 * @param[in]  val Value to write
 */
static void internal_write_le32(uint8_t* buf, uint32_t val)
{
  buf[k_le32_byte_0] = (uint8_t)(val & k_rx_byte_mask);
  buf[k_le32_byte_1] = (uint8_t)((val >> k_shift_byte_1) & k_rx_byte_mask);
  buf[k_le32_byte_2] = (uint8_t)((val >> k_shift_byte_2) & k_rx_byte_mask);
  buf[k_le32_byte_3] = (uint8_t)((val >> k_shift_byte_3) & k_rx_byte_mask);
}

/**
 * @brief Read uint32 in little-endian format (for CRC-32)
 *
 * @param[in] buf Input buffer (at least 4 bytes)
 * @return Decoded value
 */
static uint32_t internal_read_le32(const uint8_t* buf)
{
  return (uint32_t)buf[k_le32_byte_0] | ((uint32_t)buf[k_le32_byte_1] << k_shift_byte_1) |
         ((uint32_t)buf[k_le32_byte_2] << k_shift_byte_2) |
         ((uint32_t)buf[k_le32_byte_3] << k_shift_byte_3);
}

static rx_err_t internal_decode_header(const uint8_t* data,
                                       uint32_t       data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out)
{
  uint32_t offset;
  uint16_t sync_word;
  uint32_t expected_size;

  if (data_len < k_frame_min_size) {
    return k_rx_err_invalid_size;
  }

  offset = 0;

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

static rx_err_t internal_verify_crc(const uint8_t* data, uint32_t offset, uint32_t* crc_out)
{
  const uint32_t received_crc   = internal_read_le32(&data[offset]);
  const uint32_t calculated_crc = rx_crc32_ieee(data, offset);

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  if (crc_out != NULL) {
    *crc_out = received_crc;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Encoder API Implementation
 * =============================================================================
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

rx_err_t rx_frame_encode(rx_frame_encoder_t* enc,
                         const rx_frame_t*   frame,
                         uint8_t*            output,
                         uint32_t*           output_len)
{
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
  const uint32_t frame_size = rx_frame_encoded_size(frame->header.length);
  uint32_t       offset     = 0;

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
  const uint32_t crc = rx_crc32_ieee(output, offset);

  /* Write CRC-32 (little-endian to match IEEE 802.3 LSB-first order) */
  internal_write_le32(&output[offset], crc);
  offset += k_frame_crc_size;

  *output_len = frame_size;
  return k_rx_ok;
}

/* =============================================================================
 * Decoder API Implementation
 * =============================================================================
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

rx_err_t
rx_frame_decode(rx_frame_decoder_t* dec, const uint8_t* data, uint32_t data_len, rx_frame_t* frame)
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

  err = internal_verify_crc(data, offset, &frame->crc);
  if (err != k_rx_ok) {
    return err;
  }
  return k_rx_ok;
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

rx_err_t rx_frame_create_ack(rx_frame_t* frame, uint16_t sequence)
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

rx_err_t rx_frame_create_nack(rx_frame_t* frame, uint16_t sequence, uint8_t flags)
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
