/**
 * @file rx_frame.c
 * @brief Frame Layer Implementation
 *
 * Implements frame encoding/decoding with CRC-32 verification.
 * Bit-exact compatible with star-gateway/internal/frame/.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "rx_frame.h"

#include <string.h>

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Write uint16 in big-endian format
 *
 * @param[out] buf Output buffer (at least 2 bytes)
 * @param[in]  val Value to write
 */
static void internal_write_be16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

/**
 * @brief Read uint16 in big-endian format
 *
 * @param[in] buf Input buffer (at least 2 bytes)
 * @return Decoded value
 */
static uint16_t internal_read_be16(const uint8_t *buf)
{
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/**
 * @brief Write uint32 in little-endian format (for CRC-32)
 *
 * @param[out] buf Output buffer (at least 4 bytes)
 * @param[in]  val Value to write
 */
static void internal_write_le32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

/**
 * @brief Read uint32 in little-endian format (for CRC-32)
 *
 * @param[in] buf Input buffer (at least 4 bytes)
 * @return Decoded value
 */
static uint32_t internal_read_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* =============================================================================
 * Encoder API Implementation
 * =============================================================================
 */

rx_err_t rx_frame_encoder_init(rx_frame_encoder_t *enc)
{
    if (enc == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    enc->initialized = 1;
    return RX_OK;
}

rx_err_t rx_frame_encoder_deinit(rx_frame_encoder_t *enc)
{
    if (enc == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    enc->initialized = 0;
    return RX_OK;
}

rx_err_t rx_frame_encode(rx_frame_encoder_t *enc, const rx_frame_t *frame,
                         uint8_t *output, size_t *output_len)
{
    if (enc == NULL || frame == NULL || output == NULL || output_len == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (enc->initialized == 0) {
        return RX_ERR_INVALID_STATE;
    }

    /* Validate payload size */
    if (frame->header.length > k_frame_max_payload) {
        return RX_ERR_INVALID_SIZE;
    }

    /* Calculate total frame size */
    size_t frame_size = rx_frame_encoded_size(frame->header.length);
    size_t offset     = 0;

    /* Write SYNC word (big-endian) */
    internal_write_be16(&output[offset], k_frame_sync_word);
    offset += k_frame_sync_size;

    /* Write SEQ (big-endian, network byte order per RFC 1700) */
    internal_write_be16(&output[offset], frame->header.sequence);
    offset += k_frame_seq_size;

    /* Write LEN (big-endian, network byte order per RFC 1700) */
    internal_write_be16(&output[offset], frame->header.length);
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
    uint32_t crc = rx_crc32_ieee(output, offset);

    /* Write CRC-32 (little-endian to match IEEE 802.3 LSB-first order) */
    internal_write_le32(&output[offset], crc);
    offset += k_frame_crc_size;

    *output_len = frame_size;
    return RX_OK;
}

/* =============================================================================
 * Decoder API Implementation
 * =============================================================================
 */

rx_err_t rx_frame_decoder_init(rx_frame_decoder_t *dec)
{
    if (dec == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    dec->initialized = 1;
    return RX_OK;
}

rx_err_t rx_frame_decoder_deinit(rx_frame_decoder_t *dec)
{
    if (dec == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    dec->initialized = 0;
    return RX_OK;
}

rx_err_t rx_frame_decode(rx_frame_decoder_t *dec, const uint8_t *data,
                         size_t data_len, rx_frame_t *frame)
{
    if (dec == NULL || data == NULL || frame == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (dec->initialized == 0) {
        return RX_ERR_INVALID_STATE;
    }

    /* Check minimum frame size */
    if (data_len < k_frame_min_size) {
        return RX_ERR_INVALID_SIZE;
    }

    size_t offset = 0;

    /* Verify SYNC word */
    uint16_t sync_word = internal_read_be16(&data[offset]);
    if (sync_word != k_frame_sync_word) {
        return RX_ERR_PROTOCOL_ERROR;
    }
    offset += k_frame_sync_size;

    /* Read SEQ */
    frame->header.sequence = internal_read_be16(&data[offset]);
    offset += k_frame_seq_size;

    /* Read LEN */
    frame->header.length = internal_read_be16(&data[offset]);
    offset += k_frame_len_size;

    /* Validate payload length */
    if (frame->header.length > k_frame_max_payload) {
        return RX_ERR_INVALID_SIZE;
    }

    /* Verify we have enough data for the declared payload + CRC */
    size_t expected_size = rx_frame_encoded_size(frame->header.length);
    if (data_len < expected_size) {
        return RX_ERR_INVALID_SIZE;
    }

    /* Read TYPE */
    frame->header.type = data[offset];
    offset += k_frame_type_size;

    /* Read FLAGS */
    frame->header.flags = data[offset];
    offset += k_frame_flags_size;

    /* Read PAYLOAD */
    if (frame->header.length > 0) {
        memcpy(frame->payload, &data[offset], frame->header.length);
        offset += frame->header.length;
    }

    /* Read CRC-32 (little-endian) */
    uint32_t received_crc = internal_read_le32(&data[offset]);

    /* Calculate expected CRC over SYNC + Header + Payload */
    uint32_t calculated_crc = rx_crc32_ieee(data, offset);

    /* Verify CRC */
    if (received_crc != calculated_crc) {
        return RX_ERR_CRC_MISMATCH;
    }

    frame->crc = received_crc;
    return RX_OK;
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

rx_err_t rx_frame_create_ack(rx_frame_t *frame, uint16_t sequence)
{
    if (frame == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    memset(frame, 0, sizeof(rx_frame_t));
    frame->header.sequence = sequence;
    frame->header.length   = 0;
    frame->header.type     = k_frame_type_ack;
    frame->header.flags    = k_frame_flag_none;

    return RX_OK;
}

rx_err_t rx_frame_create_nack(rx_frame_t *frame, uint16_t sequence,
                              uint8_t flags)
{
    if (frame == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    memset(frame, 0, sizeof(rx_frame_t));
    frame->header.sequence = sequence;
    frame->header.length   = 0;
    frame->header.type     = k_frame_type_nack;
    frame->header.flags    = flags;

    return RX_OK;
}
