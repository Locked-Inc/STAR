/**
 * @file star_frame.h
 * @brief Frame encoding/decoding API with CRC-32 error detection
 * @details
 * Provides frame protocol interface for reliable data transmission with CRC-32 error detection.
 * Includes frame serialization/deserialization with header, payload, and CRC validation
 * for SPI communication between ESP32 and RPi5.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_FRAME_H
#define STAR_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file star_frame.h
 * @brief Frame layer with CRC-32 validation for SPI communication protocol
 *
 * This library provides packet framing with CRC-32 error detection for
 * reliable SPI communication between RPi5 and ESP32. Frames include
 * synchronization markers, sequence numbers, and IEEE 802.3 CRC-32 checksums.
 *
 * Frame Format (12 bytes overhead):
 * [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)][PAYLOAD(0-1024)][CRC-32(4)]
 *
 * Key Features:
 * - SYNC marker (0xAA55) for frame alignment
 * - Sequence numbering for ARQ layer support
 * - CRC-32 (IEEE 802.3) error detection
 * - Static allocation (no malloc)
 * - Maximum payload: 1024 bytes
 *
 * Example Usage:
 * @code
 * // === Build a Frame ===
 *
 * #include "star_frame.h"
 *
 * star_frame_handle_t tx_frame;
 * star_frame_init(&tx_frame);
 *
 * uint8_t payload[] = "Hello, RPi5!";
 * uint8_t* frame_data;
 * size_t frame_len;
 *
 * esp_err_t ret = star_frame_build(
 *     &tx_frame,
 *     42,                              // Sequence number
 *     k_star_frame_type_request,       // Frame type
 *     payload,
 *     sizeof(payload),
 *     &frame_data,
 *     &frame_len
 * );
 *
 * if (ret == ESP_OK) {
 *     // Send frame_data (frame_len bytes) via SPI
 * }
 *
 *
 * // === Parse a Received Frame ===
 *
 * uint8_t rx_data[256];  // Received from SPI
 * size_t rx_len = 256;
 *
 * star_frame_handle_t rx_frame;
 * star_frame_init(&rx_frame);
 *
 * uint16_t seq;
 * star_frame_type_t type;
 * uint8_t* payload_data;
 * uint16_t payload_len;
 *
 * ret = star_frame_parse(
 *     &rx_frame,
 *     rx_data,
 *     rx_len,
 *     &seq,
 *     &type,
 *     &payload_data,
 *     &payload_len
 * );
 *
 * if (ret == ESP_OK) {
 *     // Process payload_data (payload_len bytes)
 *     ESP_LOGI(TAG, "Received seq=%u, type=%d", seq, type);
 * } else if (ret == ESP_ERR_INVALID_CRC) {
 *     ESP_LOGE(TAG, "CRC mismatch - corrupted frame");
 * }
 *
 *
 * // === Calculate CRC-32 ===
 *
 * uint8_t data[] = {1, 2, 3, 4, 5};
 * uint32_t crc = star_frame_crc32(data, sizeof(data));
 * @endcode
 */

/* --- Type Definitions --- */

/**
 * @brief Frame type identifiers
 */
typedef enum {
  k_star_frame_type_request  = 0x01, /**< Request frame (RPi5 → ESP32) */
  k_star_frame_type_response = 0x02, /**< Response frame (ESP32 → RPi5) */
  k_star_frame_type_ack      = 0x03, /**< Acknowledgement (ARQ layer) */
  k_star_frame_type_nack     = 0x04, /**< Negative acknowledgement (ARQ layer) */
} star_frame_type_t;

/**
 * @brief Frame size constants
 */
typedef enum {
  k_star_frame_sync_magic =
    0x55AA, /**< SYNC magic bytes for frame alignment (per docs/sections/07_gateway_architecture.tex) */
  k_star_frame_header_size = 8,    /**< Frame header size (SYNC + SEQ + LEN + TYPE + FLAGS) */
  k_star_frame_crc_size    = 4,    /**< CRC-32 footer size */
  k_star_frame_max_payload = 1024, /**< Maximum payload size (1024 bytes) */
  k_star_frame_max_size    = 1036, /**< Total max frame size (header + max payload + CRC) */
} star_frame_size_t;

/**
 * @brief Frame handle for building and parsing frames
 *
 * Contains internal buffer for frame construction and parsing.
 * All memory is statically allocated (no malloc).
 */
typedef struct {
  uint8_t  buffer[k_star_frame_max_size]; /**< Static frame buffer */
  uint16_t seq;                           /**< Last processed sequence number */
  uint16_t payload_len;                   /**< Last processed payload length */
  uint8_t  type;                          /**< Last processed frame type */
  bool     valid;                         /**< Frame validity flag */
} star_frame_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize a frame handle
 *
 * Resets the frame handle to initial state. Must be called before using
 * star_frame_build() or star_frame_parse().
 *
 * @param[in,out] handle  Pointer to frame handle to initialize
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 */
esp_err_t star_frame_init(star_frame_handle_t* handle);

/**
 * @brief Build a frame with CRC-32 validation
 *
 * Constructs a complete frame with header, payload, and CRC-32 checksum.
 * The frame is built in the handle's internal buffer.
 *
 * Frame Structure:
 * - SYNC (2 bytes): 0xAA55
 * - SEQ (2 bytes): Sequence number (big-endian)
 * - LEN (2 bytes): Payload length (big-endian)
 * - TYPE (1 byte): Frame type
 * - FLAGS (1 byte): Reserved (set to 0)
 * - PAYLOAD (0-1024 bytes): Data to transmit
 * - CRC-32 (4 bytes): IEEE 802.3 checksum (little-endian)
 *
 * @param[in,out] handle        Pointer to frame handle
 * @param[in]     seq           Sequence number (0-65535)
 * @param[in]     type          Frame type (request, response, ack, nack)
 * @param[in]     payload       Pointer to payload data (NULL for zero-length)
 * @param[in]     payload_len   Payload length in bytes (0-1024)
 * @param[out]    out_frame     Pointer to frame data (points into handle buffer)
 * @param[out]    out_frame_len Total frame length in bytes
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle, out_frame, or out_frame_len is NULL
 *     - ESP_ERR_INVALID_SIZE if payload_len > k_star_frame_max_payload
 *     - ESP_ERR_INVALID_ARG if payload is NULL but payload_len > 0
 */
esp_err_t star_frame_build(star_frame_handle_t* handle,
                           uint16_t             seq,
                           star_frame_type_t    type,
                           const uint8_t*       payload,
                           uint16_t             payload_len,
                           uint8_t**            out_frame,
                           size_t*              out_frame_len);

/**
 * @brief Parse and validate a received frame
 *
 * Parses frame header, validates SYNC marker and CRC-32 checksum, and
 * extracts payload data. The payload pointer points into the input data buffer.
 *
 * @param[in,out] handle          Pointer to frame handle
 * @param[in]     data            Pointer to received frame data
 * @param[in]     data_len        Received frame length in bytes
 * @param[out]    out_seq         Pointer to store sequence number
 * @param[out]    out_type        Pointer to store frame type
 * @param[out]    out_payload     Pointer to payload data (points into data buffer)
 * @param[out]    out_payload_len Pointer to store payload length
 *
 * @return
 *     - ESP_OK on success (frame valid)
 *     - ESP_ERR_INVALID_ARG if any pointer is NULL
 *     - ESP_ERR_INVALID_SIZE if data_len < k_star_frame_header_size + k_star_frame_crc_size
 *     - ESP_ERR_INVALID_STATE if SYNC marker not found
 *     - ESP_ERR_INVALID_CRC if CRC-32 checksum mismatch
 *     - ESP_ERR_INVALID_SIZE if payload length exceeds maximum
 */
esp_err_t star_frame_parse(star_frame_handle_t* handle,
                           const uint8_t*       data,
                           size_t               data_len,
                           uint16_t*            out_seq,
                           star_frame_type_t*   out_type,
                           uint8_t**            out_payload,
                           uint16_t*            out_payload_len);

/**
 * @brief Calculate CRC-32 checksum (IEEE 802.3)
 *
 * Computes CRC-32 using the IEEE 802.3 polynomial (0x04C11DB7).
 * Uses ESP32 ROM-based implementation for zero SRAM overhead.
 *
 * Test vector:
 * - Input: "123456789" (9 bytes)
 * - Expected CRC-32: 0xCBF43926
 *
 * @param[in] data  Pointer to data buffer
 * @param[in] len   Data length in bytes
 *
 * @return CRC-32 checksum (32-bit unsigned integer)
 */
uint32_t star_frame_crc32(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STAR_FRAME_H */
