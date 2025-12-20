/**
 * @file star_frame.c
 * @brief Frame encoding/decoding implementation with CRC-32 error detection
 * @details
 * Implements frame protocol for reliable data transmission with CRC-32 error detection.
 * Provides frame serialization/deserialization with header, payload, and CRC validation
 * for SPI communication between ESP32 and RPi5.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_frame.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_crc.h"

/* --- Constants --- */

static const char* s_tag = "STAR_FRAME";

/* --- Private Function Prototypes --- */

static inline void     internal_write_u16_be(uint8_t* dest, uint16_t value);
static inline uint16_t internal_read_u16_be(const uint8_t* src);
static inline void     internal_write_u32_le(uint8_t* dest, uint32_t value);
static inline uint32_t internal_read_u32_le(const uint8_t* src);

/* --- Public Functions --- */

esp_err_t star_frame_init(star_frame_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_frame_handle_t));
  handle->valid = false;

  return ESP_OK;
}

esp_err_t star_frame_build(star_frame_handle_t* handle,
                           uint16_t             seq,
                           star_frame_type_t    type,
                           const uint8_t*       payload,
                           uint16_t             payload_len,
                           uint8_t**            out_frame,
                           size_t*              out_frame_len)
{
  ESP_RETURN_ON_FALSE(handle && out_frame && out_frame_len,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "NULL pointer argument");
  ESP_RETURN_ON_FALSE(payload_len <= k_star_frame_max_payload,
                      ESP_ERR_INVALID_SIZE,
                      s_tag,
                      "Payload too large: %u > %d",
                      payload_len,
                      k_star_frame_max_payload);
  ESP_RETURN_ON_FALSE(!(payload == NULL && payload_len > 0),
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "NULL payload with non-zero length");

  /* Build frame header */
  uint8_t* buf    = handle->buffer;
  size_t   offset = 0;

  /* SYNC (2 bytes, big-endian network byte order) */
  internal_write_u16_be(&buf[offset], k_star_frame_sync_magic);
  offset += 2;

  /* SEQ (2 bytes, big-endian network byte order) */
  internal_write_u16_be(&buf[offset], seq);
  offset += 2;

  /* LEN (2 bytes, big-endian network byte order) */
  internal_write_u16_be(&buf[offset], payload_len);
  offset += 2;

  /* TYPE (1 byte) */
  buf[offset] = (uint8_t)type;
  offset += 1;

  /* FLAGS (1 byte, reserved for future use) */
  buf[offset] = 0;
  offset += 1;

  /* PAYLOAD (0-1024 bytes) */
  if (payload_len > 0) {
    memcpy(&buf[offset], payload, payload_len);
    offset += payload_len;
  }

  /* Calculate CRC-32 over header + payload */
  const size_t   crc_data_len = k_star_frame_header_size + payload_len;
  const uint32_t crc          = star_frame_crc32(buf, crc_data_len);

  /* CRC-32 (4 bytes, little-endian per IEEE 802.3) */
  internal_write_u32_le(&buf[offset], crc);
  offset += k_star_frame_crc_size;

  /* Update handle state */
  handle->seq         = seq;
  handle->payload_len = payload_len;
  handle->type        = (uint8_t)type;
  handle->valid       = true;

  /* Return frame data (points into handle buffer) */
  *out_frame     = buf;
  *out_frame_len = offset;

  ESP_LOGD(s_tag,
           "Built frame: seq=%u, type=%u, payload_len=%u, total_len=%zu",
           seq,
           (uint8_t)type,
           payload_len,
           offset);

  return ESP_OK;
}

esp_err_t star_frame_parse(star_frame_handle_t* handle,
                           const uint8_t*       data,
                           size_t               data_len,
                           uint16_t*            out_seq,
                           star_frame_type_t*   out_type,
                           uint8_t**            out_payload,
                           uint16_t*            out_payload_len)
{
  ESP_RETURN_ON_FALSE(handle && data && out_seq && out_type && out_payload && out_payload_len,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "NULL pointer argument");

  const size_t min_frame_size = k_star_frame_header_size + k_star_frame_crc_size;
  ESP_RETURN_ON_FALSE(data_len >= min_frame_size,
                      ESP_ERR_INVALID_SIZE,
                      s_tag,
                      "Frame too small: %zu < %zu",
                      data_len,
                      min_frame_size);

  size_t offset = 0;

  /* Parse SYNC (2 bytes, big-endian) */
  const uint16_t sync = internal_read_u16_be(&data[offset]);
  ESP_RETURN_ON_FALSE(sync == k_star_frame_sync_magic,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "SYNC mismatch: 0x%04X != 0x%04X",
                      sync,
                      k_star_frame_sync_magic);
  offset += 2;

  /* Parse SEQ (2 bytes, big-endian) */
  const uint16_t seq = internal_read_u16_be(&data[offset]);
  offset += 2;

  /* Parse LEN (2 bytes, big-endian) */
  const uint16_t payload_len = internal_read_u16_be(&data[offset]);
  offset += 2;

  ESP_RETURN_ON_FALSE(payload_len <= k_star_frame_max_payload,
                      ESP_ERR_INVALID_SIZE,
                      s_tag,
                      "Payload length too large: %u > %d",
                      payload_len,
                      k_star_frame_max_payload);

  /* Parse TYPE (1 byte) */
  const uint8_t type = data[offset];
  offset += 1;

  /* Parse FLAGS (1 byte, reserved) */
  offset += 1;

  /* Verify frame length matches expected */
  const size_t expected_len = k_star_frame_header_size + payload_len + k_star_frame_crc_size;
  ESP_RETURN_ON_FALSE(data_len >= expected_len,
                      ESP_ERR_INVALID_SIZE,
                      s_tag,
                      "Frame incomplete: %zu < %zu",
                      data_len,
                      expected_len);

  /* Verify CRC-32 (calculated over header + payload) */
  const size_t   crc_data_len   = k_star_frame_header_size + payload_len;
  const uint32_t calculated_crc = star_frame_crc32(data, crc_data_len);
  const uint32_t received_crc   = internal_read_u32_le(&data[crc_data_len]);

  ESP_RETURN_ON_FALSE(calculated_crc == received_crc,
                      ESP_ERR_INVALID_CRC,
                      s_tag,
                      "CRC mismatch: 0x%08lX != 0x%08lX",
                      calculated_crc,
                      received_crc);

  /* Update handle state */
  handle->seq         = seq;
  handle->payload_len = payload_len;
  handle->type        = type;
  handle->valid       = true;

  /* Return parsed data (payload points into input data buffer) */
  *out_seq         = seq;
  *out_type        = (star_frame_type_t)type;
  *out_payload     = (uint8_t*)&data[k_star_frame_header_size];
  *out_payload_len = payload_len;

  ESP_LOGD(s_tag, "Parsed frame: seq=%u, type=%u, payload_len=%u", seq, type, payload_len);

  return ESP_OK;
}

uint32_t star_frame_crc32(const uint8_t* data, size_t len)
{
  if (data == NULL || len == 0) {
    return 0;
  }

  /*
     * Use ESP32 ROM-based CRC-32 (IEEE 802.3 polynomial: 0x04C11DB7)
     * Initial value: 0xFFFFFFFF, final XOR: 0xFFFFFFFF
     * This implementation has zero SRAM overhead (uses ROM lookup table)
     */
  return esp_rom_crc32_le(0xFFFFFFFF, data, len) ^ 0xFFFFFFFF;
}

/* --- Private Functions --- */

/**
 * @brief Write 16-bit value in network byte order (big-endian)
 *
 * Protocol headers use big-endian format following RFC 1700 network byte order standard.
 * This ensures cross-platform compatibility when communicating between RPi5 (ARM, little-endian)
 * and ESP32 (Xtensa, little-endian) over SPI.
 *
 * @param[out] dest  Destination buffer (2 bytes)
 * @param[in]  value 16-bit value to write
 */
static inline void internal_write_u16_be(uint8_t* dest, uint16_t value)
{
  dest[0] = (value >> 8) & 0xFF;
  dest[1] = value & 0xFF;
}

/**
 * @brief Read 16-bit value from network byte order (big-endian)
 *
 * @param[in] src  Source buffer (2 bytes)
 * @return 16-bit value in host byte order
 */
static inline uint16_t internal_read_u16_be(const uint8_t* src)
{
  return ((uint16_t)src[0] << 8) | src[1];
}

/**
 * @brief Write 32-bit value in little-endian format
 *
 * CRC-32 uses little-endian format to match IEEE 802.3 standard and ESP32 ROM
 * CRC implementation. This is different from the big-endian header fields.
 *
 * @param[out] dest  Destination buffer (4 bytes)
 * @param[in]  value 32-bit value to write
 */
static inline void internal_write_u32_le(uint8_t* dest, uint32_t value)
{
  dest[0] = value & 0xFF;
  dest[1] = (value >> 8) & 0xFF;
  dest[2] = (value >> 16) & 0xFF;
  dest[3] = (value >> 24) & 0xFF;
}

/**
 * @brief Read 32-bit value from little-endian format
 *
 * @param[in] src  Source buffer (4 bytes)
 * @return 32-bit value in host byte order
 */
static inline uint32_t internal_read_u32_le(const uint8_t* src)
{
  return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) |
         ((uint32_t)src[3] << 24);
}
