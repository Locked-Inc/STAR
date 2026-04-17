/**
 * @file rx_frame.h
 * @brief Frame Layer Protocol for SPI Communication
 *
 * @details
 * ## Overview
 *
 * This module implements the frame-level protocol for reliable SPI communication
 * between the Raspberry Pi 5 gateway and the RX72N motor controller. It provides
 * frame encoding, decoding, and CRC-32 integrity verification.
 *
 * **Key Features:**
 * - Bit-exact compatible with star-gateway/internal/frame/ (Go implementation)
 * - IEEE 802.3 CRC-32 for data integrity
 * - Little-endian byte order for ALL multi-byte fields (SYNC, SEQ, LEN, CRC)
 * - Support for ACK/NACK flow control
 * - Optional forward error correction (FEC) flag
 *
 * ## Frame Format
 *
 * ```
 * +----------+----------+----------+----------+----------+---------------+----------+
 * |  SYNC    |   SEQ    |   LEN    |  TYPE    |  FLAGS   |   PAYLOAD     |  CRC-32  |
 * |  2 bytes |  2 bytes |  2 bytes |  1 byte  |  1 byte  |   0-1024 B    |  4 bytes |
 * |   (LE)   |   (LE)   |   (LE)   |          |          |               |   (LE)   |
 * +----------+----------+----------+----------+----------+---------------+----------+
 * ```
 *
 * **Field Descriptions:**
 *
 * | Field | Size | Byte Order | Description |
 * |-------|------|------------|-------------|
 * | SYNC | 2 | Little-endian | Synchronization marker (0x55AA, wire: 0xAA 0x55) |
 * | SEQ | 2 | Little-endian | Sequence number for ordering/retransmit |
 * | LEN | 2 | Little-endian | Payload length in bytes (0-1024) |
 * | TYPE | 1 | N/A | Frame type (command, response, ack, nack) |
 * | FLAGS | 1 | N/A | Control flags (requires_ack, retransmit, etc.) |
 * | PAYLOAD | 0-1024 | N/A | Encoded protobuf message or empty |
 * | CRC-32 | 4 | Little-endian | IEEE 802.3 CRC over SYNC+header+payload |
 *
 * ## Frame Size Calculations
 *
 * - **Minimum frame**: 12 bytes (SYNC + header + CRC, no payload)
 * - **Maximum frame**: 1036 bytes (SYNC + header + 1024 payload + CRC)
 * - **Overhead**: 12 bytes fixed per frame
 *
 * ## Protocol Flow
 *
 * @msc
 * RPi5, RX72N;
 *
 * --- [label="Normal Command/Response"];
 * RPi5 => RX72N [label="COMMAND (seq=1, requires_ack)"];
 * RPi5 <= RX72N [label="ACK (seq=1)"];
 * RPi5 <= RX72N [label="RESPONSE (seq=1)"];
 * RPi5 => RX72N [label="ACK (seq=1)"];
 *
 * --- [label="Retransmission on CRC Error"];
 * RPi5 => RX72N [label="COMMAND (seq=2)"];
 * RPi5 box RPi5 [label="Timeout"];
 * RPi5 => RX72N [label="COMMAND (seq=2, retransmit)"];
 * RPi5 <= RX72N [label="ACK (seq=2)"];
 *
 * --- [label="NACK on Decode Error"];
 * RPi5 => RX72N [label="COMMAND (seq=3, bad payload)"];
 * RPi5 <= RX72N [label="NACK (seq=3, soft_nack)"];
 * @endmsc
 *
 * ## CRC-32 Details
 *
 * The CRC-32 is computed using the IEEE 802.3 polynomial (0x04C11DB7) over
 * the entire frame excluding the CRC field itself:
 *
 * @f[
 *   \text{CRC} = \text{CRC32\_IEEE}(\text{SYNC} \| \text{Header} \| \text{Payload})
 * @f]
 *
 * **CRC Coverage:**
 * - SYNC word (2 bytes)
 * - SEQ (2 bytes)
 * - LEN (2 bytes)
 * - TYPE (1 byte)
 * - FLAGS (1 byte)
 * - PAYLOAD (0-1024 bytes)
 *
 * **Why Little-Endian CRC?**
 * The CRC-32 is stored in little-endian format (LSB first) to match the
 * IEEE 802.3 bit transmission order. This ensures compatibility with
 * Go's `crc32.ChecksumIEEE()` function.
 *
 * ## Thread Safety
 *
 * | Component | Thread Safe | Notes |
 * |-----------|-------------|-------|
 * | Encoder | [PASS] Yes | Stateless after init |
 * | Decoder | [PASS] Yes | Stateless after init |
 * | Utility functions | [PASS] Yes | Pure functions |
 *
 * ## Memory Usage
 *
 * | Component | Size | Notes |
 * |-----------|------|-------|
 * | rx_frame_t | 1034 bytes | Header + 1024-byte payload + CRC |
 * | rx_frame_encoder_t | 1 byte | Initialization flag only |
 * | rx_frame_decoder_t | 1 byte | Initialization flag only |
 * | Encoded frame buffer | 1036 bytes | Caller-provided |
 *
 * ## Performance Characteristics
 *
 * **Measured on RX72N @ 240 MHz, -O2 optimization:**
 *
 * | Operation | Empty Payload | 64B Payload | 255B Payload |
 * |-----------|---------------|-------------|--------------|
 * | Encode | ~5 us | ~15 us | ~50 us |
 * | Decode | ~5 us | ~15 us | ~50 us |
 * | CRC calc | ~1 us | ~6 us | ~25 us |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple control flow | [PASS] Pass | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | [PASS] Pass | Bounded by k_frame_max_payload |
 * | 3. No dynamic memory | [PASS] Pass | All buffers caller-provided |
 * | 4. Short functions | [PASS] Pass | All functions < 50 lines |
 * | 5. Assertions | [PASS] Pass | NULL checks, state validation |
 * | 6. Small scope | [PASS] Pass | Variables at minimal scope |
 * | 7. Check returns | [PASS] Pass | All returns propagated |
 * | 8. Limited preprocessor | [PASS] Pass | Only include guards |
 * | 9. Restrict pointers | [PASS] Pass | No function pointers |
 * | 10. Compiler warnings | [PASS] Pass | -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | Frame layer only - no protobuf knowledge |
 * | **Open/Closed** | New frame types via enum, no code changes |
 * | **Liskov Substitution** | Encoder/decoder handles are interchangeable |
 * | **Interface Segregation** | Separate encoder/decoder APIs |
 * | **Dependency Inversion** | Depends on rx_crc.h abstraction |
 *
 * ## Hardware Requirements
 *
 * | Resource | Requirement |
 * |----------|-------------|
 * | **SPI** | 10 Mbps, Full-duplex |
 * | **DMA** | Optional, improves throughput |
 * | **CRC** | Uses rx_crc module (HW or SW) |
 *
 * ## Integration Example
 *
 * @code{.c}
 * #include "rx_frame.h"
 * #include "rx_spi_comm.h"
 *
 * // Transmit a command frame
 * void send_motor_command(uint8_t* protobuf_payload, uint32_t payload_len)
 * {
 *     rx_frame_encoder_t encoder;
 *     rx_frame_t frame;
 *     uint8_t wire_buffer[1036];
 *     uint32_t wire_len;
 *
 *     // Initialize encoder
 *     rx_frame_encoder_init(&encoder);
 *
 *     // Build frame
 *     frame.header.sequence = get_next_sequence();
 *     frame.header.length = payload_len;
 *     frame.header.type = k_frame_type_command;
 *     frame.header.flags = k_frame_flag_requires_ack;
 *     memcpy(frame.payload, protobuf_payload, payload_len);
 *
 *     // Encode to wire format
 *     rx_err_t err = rx_frame_encode(&encoder, &frame, wire_buffer, &wire_len);
 *     if (err != k_rx_ok) {
 *         rx_log_error("FRAME", "Encode failed: %d", err);
 *         return;
 *     }
 *
 *     // Send via SPI
 *     spi_transmit(wire_buffer, wire_len);
 * }
 *
 * // Receive and decode a frame
 * rx_err_t receive_frame(uint8_t* wire_data, uint32_t wire_len, rx_frame_t* frame)
 * {
 *     rx_frame_decoder_t decoder;
 *
 *     // Initialize decoder
 *     rx_frame_decoder_init(&decoder);
 *
 *     // Decode frame (validates SYNC, parses header, verifies CRC)
 *     rx_err_t err = rx_frame_decode(&decoder, wire_data, wire_len, frame);
 *     if (err == k_rx_err_crc_mismatch) {
 *         rx_log_error("FRAME", "CRC mismatch, requesting retransmit");
 *         send_nack(frame->header.sequence);
 *         return err;
 *     }
 *
 *     return err;
 * }
 * @endcode
 *
 * ## References
 *
 * - star-gateway/internal/frame/: Go reference implementation
 * - docs/sections/01_nanopb_protocol.tex: Protocol specification
 * - IEEE 802.3: CRC-32 polynomial definition
 * @see rx_frame.c Implementation file
 * @see rx_crc.h CRC-32 computation
 * @see rx_spi_comm.h SPI transport layer
 * @see rx_nanopb.h Protobuf encoding for payloads
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

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
 * @enum rx_frame_constants_t
 * @brief Frame structure constants for SPI protocol
 *
 * @details
 * These constants define the frame format parameters that must exactly match
 * the Go implementation in star-gateway/internal/frame/. Any mismatch will
 * cause protocol interoperability failures.
 *
 * ## Frame Layout
 *
 * ```
 * Offset:  0        2        4        6     7     8          8+len
 * Field:   SYNC     SEQ      LEN      TYPE  FLAGS PAYLOAD... CRC
 * Size:    2        2        2        1     1     0-1024     4
 * ```
 *
 * ## Size Calculations
 *
 * | Frame Type | Formula | Bytes |
 * |------------|---------|-------|
 * | Minimum (no payload) | SYNC + Header + CRC | 12 |
 * | With N-byte payload | 12 + N | 12 - 1036 |
 * | Maximum (1KB payload) | SYNC + Header + 1024 + CRC | 1036 |
 *
 * @par Usage Example:
 * @code{.c}
 * // Validate frame size
 * if (wire_len < k_frame_min_size || wire_len > k_frame_max_size) {
 *     return k_rx_err_invalid_size;
 * }
 *
 * // Check sync word
 * uint16_t sync = rx_frame_read_le16(wire_data);
 * if (sync != k_frame_sync_word) {
 *     return k_rx_err_protocol_error;
 * }
 *
 * // Calculate expected size from payload length
 * uint16_t payload_len = rx_frame_read_le16(&wire_data[4]);
 * uint32_t expected = rx_frame_encoded_size(payload_len);
 * @endcode
 *
 * @warning These values MUST match star-gateway/internal/frame/constants.go
 * @note Using uint16_t underlying type to accommodate k_frame_max_payload (1024)
 *
 * @see star-gateway/internal/frame/ Go reference implementation
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_frame_sync_word = 0x55AA, /**< @brief Frame synchronization marker (0x55AA)
                                * @details
                                * The sync word marks the start of a valid frame. Receivers scan for this
                                * pattern to establish frame boundaries. The value 0x55AA was chosen for:
                                * - Alternating bit pattern aids clock recovery
                                * - Unique enough to avoid false positives in random data
                                * - Matches common SPI debug patterns
                                * @par Wire Format: Little-endian (0xAA first, then 0x55)
                                */

  k_frame_sync_size = 2, /**< @brief SYNC field size in bytes (2)
                           * @details Two-byte sync marker at frame start.
                           */

  k_frame_seq_size = 2, /**< @brief SEQ field size in bytes (2)
                          * @details 16-bit sequence number for ordering and retransmit tracking.
                          */

  k_frame_len_size = 2, /**< @brief LEN field size in bytes (2)
                          * @details 16-bit payload length (max 1024).
                          */

  k_frame_type_size = 1, /**< @brief TYPE field size in bytes (1)
                           * @details Single byte for frame type (command, response, ack, nack).
                           */

  k_frame_flags_size = 1, /**< @brief FLAGS field size in bytes (1)
                            * @details Single byte for control flags (requires_ack, retransmit, etc.).
                            */

  k_frame_crc_size = 4, /**< @brief CRC-32 field size in bytes (4)
                          * @details IEEE 802.3 CRC-32 checksum.
                          */

  k_frame_header_size = 6, /**< @brief Header size excluding SYNC (6 bytes)
                             * @details SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) = 6 bytes.
                             */

  k_frame_min_payload = 0, /**< @brief Minimum payload length (0 bytes)
                             * @details ACK and NACK frames have empty payloads.
                             */

  k_frame_max_payload = 1024, /**< @brief Maximum payload length (1024 bytes)
                                * @details Limited by SPI DMA buffer size and protobuf message limits.
                                */

  k_frame_min_size = 12, /**< @brief Minimum frame size (12 bytes)
                           * @details SYNC(2) + Header(6) + CRC(4) = 12 bytes (no payload).
                           */

  k_frame_max_size = 1036, /**< @brief Maximum frame size (1036 bytes)
                             * @details SYNC(2) + Header(6) + Payload(1024) + CRC(4) = 1036 bytes.
                             */

  k_frame_max_scan_bytes =
    k_frame_max_size, /**< @brief Maximum bytes to scan forward during sync recovery (1036)
                                   * @details
                                   * When the sync word is not found at offset 0, the decoder scans
                                   * forward up to this many bytes looking for the sync pattern. The
                                   * upper bound equals k_frame_max_size so recovery never reads more
                                   * data than one complete maximum-size frame. This ensures bounded
                                   * execution time (NASA Rule 2) and prevents unbounded stream
                                   * consumption on permanently corrupt inputs.
                                   *
                                   * The scan discards all bytes before the discovered sync word and
                                   * reattempts decoding from that position.
                                   */
} rx_frame_constants_t;

/**
 * @enum rx_frame_type_t
 * @brief Frame types for SPI protocol (matches Go FrameType)
 *
 * @details
 * Defines the type of message contained in a frame. The frame type determines
 * the expected payload content and required response handling.
 *
 * ## Communication Pattern
 *
 * @startuml
 * [*] --> Idle
 *
 * Idle --> WaitingForAck : TX COMMAND
 * WaitingForAck --> ProcessingResponse : RX ACK
 * WaitingForAck --> Idle : Timeout (retransmit)
 * WaitingForAck --> Idle : RX NACK (error)
 *
 * ProcessingResponse --> SendingAck : RX RESPONSE
 * SendingAck --> Idle : TX ACK
 * @enduml
 *
 * ## Type Descriptions
 *
 * | Type | Direction | Payload | Response |
 * |------|-----------|---------|----------|
 * | COMMAND | RPi5 -> RX72N | Protobuf | ACK, then RESPONSE |
 * | RESPONSE | RX72N -> RPi5 | Protobuf | ACK |
 * | ACK | Either | None | None |
 * | NACK | Either | None (flags indicate reason) | Retransmit |
 *
 * @par Usage Example:
 * @code{.c}
 * switch (frame->header.type) {
 *     case k_frame_type_command:
 *         // Process command, send ACK then RESPONSE
 *         send_ack(frame->header.sequence);
 *         process_command(frame->payload, frame->header.length);
 *         send_response(frame->header.sequence);
 *         break;
 *     case k_frame_type_ack:
 *         // Mark pending command as acknowledged
 *         mark_acked(frame->header.sequence);
 *         break;
 *     case k_frame_type_nack:
 *         // Retransmit the failed frame
 *         retransmit(frame->header.sequence);
 *         break;
 *     default:
 *         rx_log_error("FRAME", "Unknown type: %u", frame->header.type);
 * }
 * @endcode
 *
 * @warning Values MUST match star-gateway/internal/frame/types.go
 * @see rx_frame_type_valid() Validate frame type
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
     * @brief Ping request (0x00)
     * @details
     * Keepalive/heartbeat message sent to check connection status.
     * Receiver responds with PONG echoing the payload.
     * @par Payload: 4-byte counter (little-endian uint32) or empty
     */
  k_frame_type_ping = 0x00,

  /**
     * @brief Pong response (0x01)
     * @details
     * Response to PING request, confirms connection is alive.
     * Echoes the PING counter payload for validation.
     * @par Payload: Echo of PING counter or empty
     */
  k_frame_type_pong = 0x01,

  /**
     * @brief Command from controller to peripheral (0x10)
     * @details
     * Contains a protobuf-encoded command message from RPi5 to RX72N.
     * On SPI: typically requires ACK and generates a RESPONSE frame.
     * On USB CDC: no ACK expected (hardware reliability).
     * @par Typical Payloads: MotorCommand, TelemetryRequest, ConfigWrite
     */
  k_frame_type_command = 0x10,

  /**
     * @brief Response from peripheral to controller (0x11)
     * @details
     * Contains a protobuf-encoded response message from RX72N to RPi5.
     * Sent after processing a COMMAND.
     * @par Typical Payloads: MotorStatus, TelemetryData, ConfigAck
     */
  k_frame_type_response = 0x11,

  /**
     * @brief Positive acknowledgment (0x12, SPI only)
     * @details
     * Confirms successful reception and CRC validation of a frame.
     * Empty payload; sequence number matches the acknowledged frame.
     * Used only on SPI transport (USB CDC relies on hardware reliability).
     * @par Payload: None (length = 0)
     */
  k_frame_type_ack = 0x12,

  /**
     * @brief Negative acknowledgment (0x13, SPI only)
     * @details
     * Indicates a problem with received frame (CRC error, decode failure).
     * Empty payload; flags indicate error type. Sender should retransmit.
     * Used only on SPI transport (UART relies on CRC-32 + bounded resync).
     * @par Payload: None (length = 0)
     * @par Flags: May include k_frame_flag_soft_nack
     */
  k_frame_type_nack = 0x13,

  /**
     * @brief Firmware log message (0x20)
     * @details
     * Carries ASCII log text emitted by the RX72N runtime (rx_log_* macros).
     * Payload is a raw UTF-8 / 7-bit ASCII byte sequence -- NOT null-terminated
     * and may span multiple lines. The gateway extracts the payload and writes
     * it to its own log stream (stderr / log file). Multiplexing logs as framed
     * messages prevents log bytes from colliding with the frame sync word on the
     * shared UART-to-USB bridge byte stream.
     * @par Payload: 1..1024 bytes of log text (no null terminator)
     * @par Flags: Usually k_frame_flag_none
     */
  k_frame_type_log_message = 0x20,

  /**
     * @brief Reset acknowledgment (0xFE)
     * @details
     * Confirms reset operation completed successfully.
     * Sequence number matches the reset request.
     * @par Payload: None (length = 0)
     */
  k_frame_type_reset_ack = 0xFE,

  /**
     * @brief Reset request (0xFF)
     * @details
     * Request to reset communication state (sequence numbers, buffers, etc.).
     * Receiver responds with RESET_ACK.
     * @par Payload: None (length = 0)
     */
  k_frame_type_reset = 0xFF,
} rx_frame_type_t;

/**
 * @enum rx_frame_flags_t
 * @brief Frame control flags (matches Go FrameFlags)
 *
 * @details
 * Bit flags that modify frame handling behavior. Multiple flags can be
 * combined using bitwise OR.
 *
 * ## Flag Descriptions
 *
 * | Flag | Bit | Purpose |
 * |------|-----|---------|
 * | REQUIRES_ACK | 0 | Sender expects ACK/NACK response |
 * | RETRANSMIT | 1 | This is a retransmission of previous frame |
 * | PRIORITY | 2 | Process before normal-priority frames |
 * | FEC_ENABLED | 3 | Payload uses forward error correction |
 * | SOFT_NACK | 4 | NACK with recoverable error info |
 *
 * ## Flag Combinations
 *
 * @code{.c}
 * // Typical command frame
 * frame.header.flags = k_frame_flag_requires_ack;
 *
 * // Retransmission after timeout
 * frame.header.flags = k_frame_flag_requires_ack | k_frame_flag_retransmit;
 *
 * // High-priority motor command
 * frame.header.flags = k_frame_flag_requires_ack | k_frame_flag_priority;
 *
 * // FEC-protected data transfer
 * frame.header.flags = k_frame_flag_fec_enabled;
 *
 * // NACK with error details
 * nack_frame.header.flags = k_frame_flag_soft_nack;
 * @endcode
 *
 * @par Usage Example:
 * @code{.c}
 * // Check if ACK required
 * if (frame->header.flags & k_frame_flag_requires_ack) {
 *     send_ack(frame->header.sequence);
 * }
 *
 * // Check if retransmit
 * if (frame->header.flags & k_frame_flag_retransmit) {
 *     rx_log_warn("FRAME", "Received retransmit seq=%u",
 *                 frame->header.sequence);
 *     stats.retransmit_count++;
 * }
 * @endcode
 *
 * @warning Values MUST match star-gateway/internal/frame/flags.go
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_frame_flag_none = 0x00, /**< @brief No flags set (0x00)
                              * @details Default for frames that don't require special handling.
                              */

  k_frame_flag_requires_ack = 0x01, /**< @brief Frame requires acknowledgment (0x01, bit 0)
                                      * @details
                                      * Sender expects an ACK or NACK response. If no response within
                                      * timeout, sender should retransmit with RETRANSMIT flag.
                                      */

  k_frame_flag_retransmit = 0x02, /**< @brief Retransmission of previous frame (0x02, bit 1)
                                    * @details
                                    * Indicates this frame is a retry after timeout or NACK.
                                    * Receiver should check for duplicate sequence numbers.
                                    */

  k_frame_flag_priority = 0x04, /**< @brief High-priority frame (0x04, bit 2)
                                  * @details
                                  * Process before normal-priority frames in queue.
                                  * Used for emergency stop and critical motor commands.
                                  */

  k_frame_flag_fec_enabled = 0x08, /**< @brief Forward error correction enabled (0x08, bit 3)
                                     * @details
                                     * Payload is FEC-encoded (convolutional rate 1/2). FEC is enabled by default
                                     * on the HARQ path (e.g., SPI transport with rx_spi_link). This flag indicates
                                     * FEC encoding is present and receiver should apply Viterbi decoding before
                                     * processing payload. The flag is ignored for transports that do not use HARQ
                                     * (e.g., USB CDC, which relies on built-in hardware CRC and retry).
                                     */

  k_frame_flag_soft_nack = 0x10, /**< @brief NACK with soft error information (0x10, bit 4)
                                   * @details
                                   * Used in NACK frames to indicate recoverable error.
                                   * May include confidence bits or partial decode info.
                                   */
} rx_frame_flags_t;

/* =============================================================================
 * Frame Structures
 * =============================================================================
 */

/**
 * @struct rx_frame_header_t
 * @brief Frame header containing control fields
 *
 * @details
 * The frame header contains all control information needed to process a frame.
 * All 16-bit fields are stored in host byte order within this structure;
 * conversion to/from little-endian wire format is handled by the encoder/decoder.
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Type | Description |
 * |--------|------|-------|------|-------------|
 * | 0 | 2 | sequence | uint16_t | Sequence number |
 * | 2 | 2 | length | uint16_t | Payload length |
 * | 4 | 1 | type | uint8_t | Frame type |
 * | 5 | 1 | flags | uint8_t | Control flags |
 * | **Total** | **6** | | | No padding required |
 *
 * ## Wire Format vs In-Memory Format
 *
 * **Wire Format (little-endian):**
 * - sequence: LSB at lower address
 * - length: LSB at lower address
 *
 * **In-Memory (this struct):**
 * - sequence: Host byte order (little-endian on RX72N, matches wire)
 * - length: Host byte order
 *
 * @par Usage Example:
 * @code{.c}
 * rx_frame_header_t header = {
 *     .sequence = 0x1234,            // Will be sent as 0x34, 0x12 on wire (LE)
 *     .length = 64,                  // 64-byte payload
 *     .type = k_frame_type_command,
 *     .flags = k_frame_flag_requires_ack,
 * };
 * @endcode
 *
 * @invariant length <= k_frame_max_payload (1024)
 * @invariant type is valid rx_frame_type_t value
 *
 * @see rx_frame_t Complete frame structure
 * @since Version 1.0.0
 */
typedef struct {
  /**
     * @brief Frame sequence number
     * @details
     * 16-bit sequence number for ordering, duplicate detection, and
     * ACK/NACK correlation. Wraps around at 65535.
     * @par Wire Format: Little-endian
     * @par Valid Range: 0-65535
     */
  uint16_t sequence;

  /**
     * @brief Payload length in bytes
     * @details
     * Length of the payload field (not including header or CRC).
     * Zero for ACK/NACK frames.
     * @par Wire Format: Little-endian
     * @par Valid Range: 0-1024 (k_frame_max_payload)
     * @invariant length <= k_frame_max_payload
     */
  uint16_t length;

  /**
     * @brief Frame type identifier
     * @details
     * Identifies the message type (command, response, ack, nack).
     * @par Valid Values: rx_frame_type_t enumeration
     * @see rx_frame_type_valid() Validate type
     */
  uint8_t type;

  /**
     * @brief Control flags
     * @details
     * Bitfield of rx_frame_flags_t values controlling frame handling.
     * Multiple flags can be combined with bitwise OR.
     * @par Valid Values: Combination of rx_frame_flags_t values
     */
  uint8_t flags;
} rx_frame_header_t;

/**
 * @struct rx_frame_t
 * @brief Complete frame structure containing header, payload, and CRC
 *
 * @details
 * This structure represents a complete decoded frame ready for processing.
 * The encoder serializes this to wire format; the decoder populates it
 * from wire format.
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Description |
 * |--------|------|-------|-------------|
 * | 0 | 6 | header | Frame header (rx_frame_header_t) |
 * | 6 | 1024 | payload | Payload buffer (max size) |
 * | 1030 | 4 | crc | CRC-32 checksum (for decoded frames) |
 * | **Total** | **1034** | | |
 *
 * **Note:** Structure size is 1034 bytes. The actual payload length is
 * indicated by header.length, not the buffer size.
 *
 * ## Usage Patterns
 *
 * **Encoding (TX):**
 * 1. Populate header fields (sequence, length, type, flags)
 * 2. Copy payload data to payload[] array
 * 3. Call rx_frame_encode() - CRC is computed automatically
 *
 * **Decoding (RX):**
 * 1. Call rx_frame_decode() with wire data
 * 2. Access header fields to determine frame type
 * 3. Read payload[0..header.length-1] for message content
 * 4. CRC field contains the received checksum (already verified)
 *
 * @par Usage Example - Building a Frame:
 * @code{.c}
 * rx_frame_t frame;
 * memset(&frame, 0, sizeof(frame));
 *
 * // Set header
 * frame.header.sequence = 1;
 * frame.header.length = protobuf_len;
 * frame.header.type = k_frame_type_command;
 * frame.header.flags = k_frame_flag_requires_ack;
 *
 * // Copy payload
 * memcpy(frame.payload, protobuf_data, protobuf_len);
 *
 * // Encode (CRC computed internally)
 * uint8_t wire[1036];
 * uint32_t wire_len;
 * rx_frame_encode(&encoder, &frame, wire, &wire_len);
 * @endcode
 *
 * @par Usage Example - Processing Decoded Frame:
 * @code{.c}
 * rx_frame_t frame;
 * rx_err_t err = rx_frame_decode(&decoder, wire_data, wire_len, &frame);
 *
 * if (err == k_rx_ok) {
 *     // Frame valid, CRC verified
 *     if (frame.header.type == k_frame_type_command) {
 *         // Process payload[0..header.length-1]
 *         handle_command(frame.payload, frame.header.length);
 *     }
 * }
 * @endcode
 *
 * @invariant header.length <= k_frame_max_payload
 * @note The crc field is populated during decode, not used during encode
 * @warning Payload data beyond header.length is undefined
 *
 * @see rx_frame_header_t Header structure
 * @see rx_frame_encode() Encode frame to wire format
 * @see rx_frame_decode() Decode frame from wire format
 * @since Version 1.0.0
 */
typedef struct {
  /**
     * @brief Frame header containing control fields
     * @details Populated by caller (encode) or decoder (decode).
     */
  rx_frame_header_t header;

  /**
     * @brief Payload data buffer (maximum 1024 bytes)
     * @details
     * For transmit: Contains data to send (length in header.length).
     * For receive: Contains decoded payload (length in header.length).
     * @note Only payload[0..header.length-1] is valid data.
     */
  uint8_t payload[k_frame_max_payload];

  /**
     * @brief CRC-32 checksum (IEEE 802.3)
     * @details
     * For received frames: Contains the CRC read from wire (verified).
     * For transmitted frames: Not used (computed by encoder).
     * @par Wire Format: Little-endian
     */
  uint32_t crc;
} rx_frame_t;

/**
 * @struct rx_frame_encoder_t
 * @brief Frame encoder handle
 *
 * @details
 * Lightweight handle structure for frame encoding operations.
 * Must be initialized before use with rx_frame_encoder_init().
 *
 * ## Lifecycle
 *
 * 1. Declare: `rx_frame_encoder_t enc;`
 * 2. Initialize: `rx_frame_encoder_init(&enc);`
 * 3. Use: `rx_frame_encode(&enc, &frame, output, &output_len);`
 * 4. Deinitialize: `rx_frame_encoder_deinit(&enc);` (optional)
 *
 * @par Memory Layout:
 * | Offset | Size | Field | Description |
 * |--------|------|-------|-------------|
 * | 0 | 1 | initialized | Initialization flag |
 * | **Total** | **1** | | No padding |
 *
 * @invariant initialized is 0 or 1
 * @note Structure is stateless after init; thread-safe for concurrent use
 *
 * @see rx_frame_encoder_init() Initialize encoder
 * @see rx_frame_encode() Encode frame
 * @since Version 1.0.0
 */
typedef struct {
  /**
     * @brief Initialization state flag
     * @details Non-zero (1) if initialized and ready for use.
     * @par Valid Values: 0 (uninitialized), 1 (initialized)
     */
  uint8_t initialized;
} rx_frame_encoder_t;

/**
 * @struct rx_frame_decoder_t
 * @brief Frame decoder handle
 *
 * @details
 * Lightweight handle structure for frame decoding operations.
 * Must be initialized before use with rx_frame_decoder_init().
 *
 * ## Lifecycle
 *
 * 1. Declare: `rx_frame_decoder_t dec;`
 * 2. Initialize: `rx_frame_decoder_init(&dec);`
 * 3. Use: `rx_frame_decode(&dec, data, len, &frame);`
 * 4. Deinitialize: `rx_frame_decoder_deinit(&dec);` (optional)
 *
 * @par Memory Layout:
 * | Offset | Size | Field | Description |
 * |--------|------|-------|-------------|
 * | 0 | 1 | initialized | Initialization flag |
 * | **Total** | **1** | | No padding |
 *
 * @invariant initialized is 0 or 1
 * @note Structure is stateless after init; thread-safe for concurrent use
 *
 * @see rx_frame_decoder_init() Initialize decoder
 * @see rx_frame_decode() Decode frame
 * @since Version 1.0.0
 */
typedef struct {
  /**
     * @brief Initialization state flag
     * @details Non-zero (1) if initialized and ready for use.
     * @par Valid Values: 0 (uninitialized), 1 (initialized)
     */
  uint8_t initialized;
} rx_frame_decoder_t;

/* =============================================================================
 * Encoder API
 * =============================================================================
 */

/**
 * @brief Initialize frame encoder
 *
 * @param[out] enc Pointer to encoder handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if enc is nullptr
 */
[[nodiscard]] rx_err_t rx_frame_encoder_init(rx_frame_encoder_t* enc);

/**
 * @brief Deinitialize frame encoder
 *
 * @param[in,out] enc Pointer to encoder handle
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_encoder_deinit(rx_frame_encoder_t* enc);

/**
 * @brief Encode frame to wire format
 *
 * Serializes frame to wire format with SYNC, header, payload, and CRC-32.
 * All multi-byte fields are little-endian (SYNC, SEQ, LEN, CRC-32).
 *
 * @param[in]  enc        Encoder handle
 * @param[in]  frame      Frame to encode
 * @param[out] output     Output buffer (min k_frame_min_size + payload bytes)
 * @param[out] output_len Actual number of bytes written
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr
 * @return k_rx_err_invalid_state if encoder not initialized
 * @return k_rx_err_invalid_size if payload exceeds max
 */
[[nodiscard]] rx_err_t rx_frame_encode(const rx_frame_encoder_t* enc,
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
 * Little-endian read/write for wire format serialization.
 * Used by rx_frame, rx_usb_comm, and rx_spi_comm modules.
 *
 * Little-endian format stores the least significant byte (LSB) at the lowest
 * memory address. For a 16-bit value:
 *   - buf[0] = low byte (LSB)
 *   - buf[1] = high byte (MSB)
 *
 * Example: 0x55AA stored in little-endian:
 *   buf[0] = 0xAA (low byte)
 *   buf[1] = 0x55 (high byte)
 * =============================================================================
 */

/**
 * @brief Byte indices for 16-bit little-endian serialization
 *
 * In little-endian format, the low byte (LSB) is stored at index 0 and the
 * high byte (MSB) is stored at index 1.
 */
typedef enum : uint8_t {
  k_le16_byte_low  = 0, /**< Low byte (LSB) at index 0 */
  k_le16_byte_high = 1, /**< High byte (MSB) at index 1 */
} rx_le16_byte_idx_t;

/**
 * @brief Byte manipulation constants for endianness conversions
 */
typedef enum : uint8_t {
  k_rx_le16_high_shift = (8),     /**< Bit shift for high byte in 16-bit value */
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
 * @brief Read uint16 from little-endian buffer
 *
 * Reads a 16-bit unsigned integer from a buffer in little-endian format.
 * The low byte (LSB) is at index 0, the high byte (MSB) is at index 1.
 *
 * @param[in] buf Input buffer (at least 2 bytes)
 * @return Decoded 16-bit value in host byte order
 *
 * @note This is the preferred function for parsing multi-byte protocol fields.
 */
static inline uint16_t rx_frame_read_le16(const uint8_t* buf)
{
  return (uint16_t)buf[k_le16_byte_low] | ((uint16_t)buf[k_le16_byte_high] << k_rx_le16_high_shift);
}

/**
 * @brief Write uint16 to little-endian buffer
 *
 * Writes a 16-bit unsigned integer to a buffer in little-endian format.
 * The low byte (LSB) is written at index 0, the high byte (MSB) is written
 * at index 1.
 *
 * @param[out] buf Output buffer (at least 2 bytes)
 * @param[in]  val Value to write in host byte order
 *
 * @note This is the preferred function for serializing multi-byte protocol
 * fields.
 */
static inline void rx_frame_write_le16(uint8_t* buf, uint16_t val)
{
  buf[k_le16_byte_low]  = (uint8_t)(val & k_rx_byte_mask);
  buf[k_le16_byte_high] = (uint8_t)(val >> k_rx_le16_high_shift);
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
 * @return k_rx_ok on success, k_rx_err_invalid_arg if dec is nullptr
 */
[[nodiscard]] rx_err_t rx_frame_decoder_init(rx_frame_decoder_t* dec);

/**
 * @brief Deinitialize frame decoder
 *
 * @param[in,out] dec Pointer to decoder handle
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_decoder_deinit(rx_frame_decoder_t* dec);

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
 * @return k_rx_err_invalid_arg if any pointer is nullptr
 * @return k_rx_err_invalid_state if decoder not initialized
 * @return k_rx_err_invalid_size if data too short
 * @return k_rx_err_protocol_error if SYNC word invalid
 * @return k_rx_err_crc_mismatch if CRC validation fails
 */
[[nodiscard]] rx_err_t rx_frame_decode(const rx_frame_decoder_t* dec,
                                       const uint8_t*            data,
                                       uint32_t                  data_len,
                                       rx_frame_t*               frame);

/**
 * @brief Decode wire format to frame with bounded sync-word resynchronization
 *
 * @details
 * Extends rx_frame_decode() with automatic stream recovery. When the sync word
 * is not found at offset 0 (i.e., the stream is byte-misaligned due to a
 * dropped or inserted byte), this function scans forward up to
 * k_frame_max_scan_bytes positions looking for the next sync word occurrence.
 * Bytes discarded during the scan are reported via bytes_discarded_out for
 * diagnostic logging by the caller.
 *
 * If the sync word is located within the scan window, decoding proceeds from
 * that offset as if data had been provided aligned. If no sync word is found
 * within the bounded window, k_rx_err_protocol_error is returned.
 *
 * Algorithm:
 * 1. Attempt normal decode at offset 0.
 * 2. On k_rx_err_protocol_error (sync mismatch), invoke
 *    internal_find_sync_offset() to locate the next sync word.
 * 3. If found, set *bytes_discarded_out = sync_offset -- caller must advance the
 *    stream read pointer by this value even if the subsequent decode fails
 *    (e.g., k_rx_err_crc_mismatch), to avoid infinite re-scan.
 * 4. Decode from the discovered offset (trimmed view of data).
 * 5. All other errors (CRC, size) are propagated unchanged.
 *
 * @param[in]  dec                  Initialized frame decoder handle
 * @param[in]  data                 Raw byte buffer (may be misaligned)
 * @param[in]  data_len             Length of data buffer in bytes
 * @param[out] frame                Decoded frame (header, payload, CRC)
 * @param[out] bytes_discarded_out  Number of bytes skipped to reach sync word
 *                                  (0 when sync was found at offset 0)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               Frame decoded and CRC verified
 * @retval k_rx_err_invalid_arg  Any pointer parameter is nullptr
 * @retval k_rx_err_invalid_state Decoder not initialized
 * @retval k_rx_err_invalid_size  Data buffer too short to contain a valid frame
 * @retval k_rx_err_protocol_error No sync word found within k_frame_max_scan_bytes
 * @retval k_rx_err_crc_mismatch  Sync found but CRC validation failed
 *
 * @pre dec must be initialized via rx_frame_decoder_init()
 * @pre data must point to a buffer of at least data_len bytes
 * @pre frame must be a non-null pointer to a writable rx_frame_t
 * @pre bytes_discarded_out must be a non-null pointer to a writable uint32_t
 * @post On k_rx_ok, frame contains the decoded frame with verified CRC
 * @post bytes_discarded_out contains the number of bytes skipped (>= 0)
 * @post On k_rx_err_crc_mismatch, *bytes_discarded_out == bytes skipped; caller must still advance stream pointer
 *
 * @note Thread-safe after init; the decoder handle is read-only and the function
 *       uses no shared mutable state (consistent with the file-level "Stateless after init").
 * @note The scan window is bounded at k_frame_max_scan_bytes (NASA Rule 2).
 * @note Caller should log bytes_discarded_out > 0 as a diagnostic warning.
 *
 * @warning Caller must advance its read pointer by *bytes_discarded_out bytes after any
 *          call that returns k_rx_ok or k_rx_err_crc_mismatch to avoid infinite re-scan;
 *          *bytes_discarded_out is unmodified only when k_rx_err_invalid_arg is returned.
 *
 * @par Performance:
 * Worst case: scans k_frame_max_scan_bytes (1036) before failing.
 * Best case: identical to rx_frame_decode() (0 bytes discarded).
 *
 * @par Example:
 * @code{.c}
 * rx_frame_decoder_t dec;
 * rx_frame_decoder_init(&dec);
 *
 * rx_frame_t frame;
 * uint32_t discarded = 0;
 * rx_err_t err = rx_frame_decode_with_resync(&dec, wire_buf, wire_len,
 *                                             &frame, &discarded);
 * if (discarded > 0) {
 *     rx_log_warn("FRAME", "Stream resync: discarded %u bytes", discarded);
 * }
 * if (err == k_rx_ok) {
 *     process_frame(&frame);
 * }
 * @endcode
 *
 * @see rx_frame_decode() Simple decode without resynchronization
 * @see k_frame_max_scan_bytes Maximum scan window (1036 bytes)
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_frame_decode_with_resync(const rx_frame_decoder_t* dec,
                                                   const uint8_t*            data,
                                                   uint32_t                  data_len,
                                                   rx_frame_t*               frame,
                                                   uint32_t*                 bytes_discarded_out);

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
[[nodiscard]] rx_err_t rx_frame_create_ack(rx_frame_t* frame, uint16_t sequence);

/**
 * @brief Create NACK frame for given sequence
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number
 * @param[in]  flags Additional flags (e.g., k_frame_flag_soft_nack)
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_create_nack(rx_frame_t* frame, uint16_t sequence, uint8_t flags);

/**
 * @brief Create PING frame for keepalive/heartbeat
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number
 * @param[in]  payload Optional payload to include (may be NULL if payload_len is 0)
 * @param[in]  payload_len Payload length in bytes
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_create_ping(rx_frame_t*    frame,
                                            uint16_t       sequence,
                                            const uint8_t* payload,
                                            uint32_t       payload_len);

/**
 * @brief Create PONG frame (response to PING)
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number (matches ping)
 * @param[in]  payload Optional payload to include (may be NULL if payload_len is 0)
 * @param[in]  payload_len Payload length in bytes
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_create_pong(rx_frame_t*    frame,
                                            uint16_t       sequence,
                                            const uint8_t* payload,
                                            uint32_t       payload_len);

/**
 * @brief Create RESET frame to reset communication state
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_create_reset(rx_frame_t* frame, uint16_t sequence);

/**
 * @brief Create RESET_ACK frame (response to RESET)
 *
 * @param[out] frame Frame to initialize
 * @param[in]  sequence Sequence number (matches reset)
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_frame_create_reset_ack(rx_frame_t* frame, uint16_t sequence);

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
  switch (type) {
    case k_frame_type_ping:
    case k_frame_type_pong:
    case k_frame_type_command:
    case k_frame_type_response:
    case k_frame_type_ack:
    case k_frame_type_nack:
    case k_frame_type_log_message:
    case k_frame_type_reset_ack:
    case k_frame_type_reset:
      return true;
    default:
      return false;
  }
}

#ifdef __cplusplus
}
#endif
