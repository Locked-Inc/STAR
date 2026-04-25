/**
 * @file rx_uart_comm.h
 * @brief High-Level UART (SCI9) Communication Layer for RX72N
 *
 * @details
 * Integrates the Frame Layer and SCI9 UART for reliable communication with the
 * RPi5 controller. Provides the same frame-based protocol as USB and SPI but over
 * a dedicated UART serial link. This module enables an alternative communication
 * channel when USB CDC is unavailable or when a direct UART connection is preferred.
 *
 * @par System Architecture
 * @dot
 * digraph uart_comm_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_rpi5 {
 *     label="Raspberry Pi 5";
 *     style=filled;
 *     color=lightblue;
 *     ttyS [label="/dev/ttyS0\n(or USB-UART bridge)"];
 *     ros2 [label="ROS2 Node\nProtobuf Messages"];
 *   }
 *
 *   subgraph cluster_uart {
 *     label="UART (SCI9 - TXD9/RXD9)";
 *     style=filled;
 *     color=lightyellow;
 *     uart_hw [label="SCI9 Hardware\n115200 baud"];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N Firmware";
 *     style=filled;
 *     color=lightgreen;
 *     uart_comm [label="rx_uart_comm\n(This Module)"];
 *     frame [label="rx_frame\nEncoder/Decoder"];
 *     nanopb [label="nanopb\nProtobuf"];
 *   }
 *
 *   ros2 -> ttyS [label="Protobuf"];
 *   ttyS -> uart_hw;
 *   uart_hw -> uart_comm [label="SCI9"];
 *   uart_comm -> frame;
 *   frame -> nanopb;
 * }
 * @enddot
 *
 * @par Features
 * - Frame encoding/decoding with CRC-32 error detection
 * - Automatic sequence number tracking for reliable delivery
 * - Non-blocking send/receive operations
 * - Same wire protocol as USB/SPI (interchangeable transport)
 * - Configurable UART channel (default: k_uart_channel_9 for SCI9)
 *
 * @par Frame Protocol
 * Uses the same frame format as USB and SPI communication:
 * ```
 * +------+------+------+------+--------+-----------+-------+
 * | SYNC | TYPE | FLAG | SEQ  | LENGTH | PAYLOAD   | CRC32 |
 * | 0xAA | 1B   | 1B   | 2B   | 2B     | N bytes   | 4B    |
 * +------+------+------+------+--------+-----------+-------+
 * ```
 *
 * @par UART Channel Assignment
 * | Channel | SCI Channel | Baud Rate | Purpose                   |
 * |---------|-------------|-----------|---------------------------|
 * | SCI9    | k_uart_channel_9 | 115200 | Binary protocol (nanopb) |
 *
 * @par Debug Output Conflict
 * @warning When UART comm is active on SCI9, debug log output to SCI9 is
 * suppressed. Callers should redirect debug output to USB CDC Port 1 for
 * development and debugging. This is a known conflict documented in issue #418.
 *
 * @par Performance Characteristics
 * | Metric                | Value          | Notes                           |
 * |-----------------------|----------------|---------------------------------|
 * | Baud rate             | 115200 bps     | Configurable via uart_init      |
 * | Frame latency         | ~1-10 ms       | Depends on UART baud rate       |
 * | Max frame size        | 2048 bytes     | Buffer size limit               |
 *
 * @par Memory Usage
 * | Component              | Size     | Description                    |
 * |------------------------|----------|--------------------------------|
 * | rx_uart_comm_handle_t  | ~4.2 KB  | Per-instance handle            |
 * | RX buffer              | 2048 B   | Staging buffer for receives    |
 * | TX buffer              | 2048 B   | Staging buffer for sends       |
 * | Frame encoder/decoder  | ~100 B   | Internal state machines        |
 *
 * @par Thread Safety
 * The handle is **NOT thread-safe**. External synchronization (ThreadX mutex)
 * is required for concurrent access from multiple threads.
 *
 * @par Module Dependencies
 * - rx_frame.h: Frame encoding/decoding layer
 * - hardware.h: UART HAL (uart_write_channel, uart_read_channel, uart_rx_available)
 * - rx_err.h: Error code definitions
 * - rx_session.h: Cross-transport sequence continuity
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] Loop bounds via k_uart_comm_max_receive_iterations
 * - Rule 3: [OK] No dynamic memory (static buffers in handle)
 * - Rule 4: [OK] Functions are concise
 * - Rule 5: [OK] Input validation on all public functions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Constants use C23 typed enums
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** UART communication only, frame details in rx_frame
 * - **O (OCP):** Configurable channel via rx_uart_comm_config_t
 * - **L (LSP):** Same protocol as rx_usb_comm / rx_spi_comm (interchangeable)
 * - **I (ISP):** Clean send/receive interface
 * - **D (DIP):** Depends on hardware.h HAL abstraction
 *
 * @see rx_frame.h Frame layer implementation
 * @see hardware.h UART HAL
 * @see rx_usb_comm.h USB CDC communication (same protocol)
 * @see rx_spi_comm.h SPI communication (same protocol)
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "hardware.h"
#include "rx_err.h"
#include "rx_frame.h"
#include "rx_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum rx_uart_comm_constants_t
 * @brief UART communication configuration constants
 *
 * @details
 * Compile-time constants for UART communication buffer sizes and loop bounds.
 * These values are tuned for the RX72N SCI9 UART implementation with typical
 * control message sizes.
 *
 * @par Buffer Sizing Rationale
 * - 2048 bytes accommodates largest expected protobuf message + frame overhead
 * - Provides headroom for frame header (8 bytes) and CRC (4 bytes)
 *
 * @par Memory Impact
 * Total per-handle: ~4.2 KB (2 x 2048 buffers + encoder/decoder state)
 *
 * @invariant Buffer sizes must be >= maximum frame size
 * @invariant k_uart_comm_max_receive_iterations bounds all receive loops
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief RX staging buffer size in bytes
   * @details
   * Buffer for accumulating incoming UART data before frame decoding.
   * Must be large enough for largest expected frame + partial data.
   * @par Value: 2048 bytes
   */
  k_uart_comm_rx_buffer_size = 2048,

  /**
   * @brief TX staging buffer size in bytes
   * @details
   * Buffer for assembled frames before UART transmission.
   * Must accommodate frame header + max payload + CRC.
   * @par Value: 2048 bytes
   */
  k_uart_comm_tx_buffer_size = 2048,

  /**
   * @brief Maximum iterations for receive loop (NASA Rule 2)
   * @details
   * Provides static upper bound for receive polling loops to ensure
   * bounded execution time even under pathological conditions.
   * @par Value: 1000 iterations
   * @warning Reaching this limit indicates UART stall or protocol error
   */
  k_uart_comm_max_receive_iterations = 1000,
} rx_uart_comm_constants_t;

/* =============================================================================
 * Handle and Configuration
 * =============================================================================
 */

/**
 * @struct rx_uart_comm_handle_t
 * @brief UART communication handle containing all operational state
 *
 * @details
 * Contains all state for UART communication including frame encoder/decoder,
 * sequence tracking, and staging buffers. One handle corresponds to one
 * logical UART communication channel (SCI9).
 *
 * @par Memory Layout
 * | Offset | Size     | Field          | Description                    |
 * |--------|----------|----------------|--------------------------------|
 * | 0      | ~50 B    | encoder        | Frame encoder state machine    |
 * | 50     | ~50 B    | decoder        | Frame decoder state machine    |
 * | 100    | 2048 B   | rx_buffer      | RX staging buffer              |
 * | 2148   | 2048 B   | tx_buffer      | TX staging buffer              |
 * | 4196   | 4 B      | rx_buffer_len  | Valid bytes in RX buffer       |
 * | 4200   | 4 B      | rx_buffer_pos  | Current read position          |
 * | 4204   | 4-8 B    | session        | Shared session state pointer   |
 * | 4212   | 1 B      | initialized    | Init flag                      |
 * | 4213   | 1 B      | channel        | UART channel (SCI channel num) |
 * | **Total** | **~4.2 KB** |          |                                |
 *
 * @par Thread Safety
 * This handle is **NOT thread-safe**. External synchronization via ThreadX
 * mutex is required for concurrent access from multiple threads.
 *
 * @par Usage Example
 * @code
 * static rx_uart_comm_handle_t s_uart_comm;
 *
 * void uart_comm_setup(void) {
 *     rx_uart_comm_config_t config = {
 *         .session = &g_session,
 *         .channel = k_uart_channel_9
 *     };
 *     rx_err_t err = rx_uart_comm_init(&s_uart_comm, &config);
 *     if (err != k_rx_ok) {
 *         rx_log_error("UART_COMM", "Init failed");
 *         return;
 *     }
 * }
 * @endcode
 *
 * @invariant initialized == 0 or initialized == 1
 * @invariant rx_buffer_pos <= rx_buffer_len
 * @invariant rx_buffer_len <= k_uart_comm_rx_buffer_size
 *
 * @warning Do not access fields directly; use API functions
 * @attention Handle must be initialized via rx_uart_comm_init() before use
 *
 * @see rx_uart_comm_init() Initialize handle
 * @see rx_uart_comm_deinit() Cleanup handle
 * @see rx_uart_comm_config_t Configuration options
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Frame encoder state machine
   * @details Encodes payloads into framed format with sync, header, and CRC.
   * @see rx_frame_encoder_t
   */
  rx_frame_encoder_t encoder;

  /**
   * @brief Frame decoder state machine
   * @details Decodes incoming bytes, validates CRC, extracts payload.
   * @see rx_frame_decoder_t
   */
  rx_frame_decoder_t decoder;

  /**
   * @brief RX staging buffer
   * @details
   * Accumulates incoming UART data before frame decoding.
   * @par Size: k_uart_comm_rx_buffer_size (2048 bytes)
   * @invariant Valid data in [0, rx_buffer_len)
   */
  uint8_t rx_buffer[k_uart_comm_rx_buffer_size];

  /**
   * @brief TX staging buffer
   * @details
   * Holds assembled frame for UART transmission.
   * @par Size: k_uart_comm_tx_buffer_size (2048 bytes)
   */
  uint8_t tx_buffer[k_uart_comm_tx_buffer_size];

  /**
   * @brief Number of valid bytes in RX buffer
   * @details
   * Bytes [0, rx_buffer_len) contain unprocessed data.
   * @invariant rx_buffer_len <= k_uart_comm_rx_buffer_size
   */
  uint32_t rx_buffer_len;

  /**
   * @brief Current read position in RX buffer
   * @details
   * Next byte to process is rx_buffer[rx_buffer_pos].
   * @invariant rx_buffer_pos <= rx_buffer_len
   */
  uint32_t rx_buffer_pos;

  /**
   * @brief Shared session state for cross-transport sequence continuity
   * @details
   * Points to a single rx_session_state_t instance owned by the comm_manager.
   * All transports share this state so that sequence numbers remain continuous
   * across transport switches.
   * @see rx_session_state_t
   */
  rx_session_state_t* session;

  /**
   * @brief Initialization status flag
   * @details
   * Set to 1 after successful rx_uart_comm_init(), 0 otherwise.
   * @par Values: 0 = not initialized, 1 = initialized
   */
  uint8_t initialized;

  /**
   * @brief UART SCI channel used by this handle
   * @details
   * Stores the uart_channel_t value configured during init.
   * Default is k_uart_channel_9 (SCI9).
   * @see uart_channel_t
   */
  uart_channel_t channel;
} rx_uart_comm_handle_t;

/**
 * @struct rx_uart_comm_config_t
 * @brief UART communication configuration options
 *
 * @details
 * Configuration passed to rx_uart_comm_init(). The session pointer is
 * required for sequence tracking; the channel selects the SCI peripheral.
 *
 * @par Default Values
 * | Field    | Default              | Description                         |
 * |----------|----------------------|-------------------------------------|
 * | session  | (required)           | Shared session state pointer        |
 * | channel  | k_uart_channel_9     | SCI9 is the default RPi5 UART port  |
 *
 * @par Usage Example
 * @code
 * static rx_session_state_t g_session;
 * rx_session_init(&g_session);
 *
 * rx_uart_comm_config_t config = {
 *     .session = &g_session,
 *     .channel = k_uart_channel_9
 * };
 * rx_uart_comm_init(&handle, &config);
 * @endcode
 *
 * @see rx_uart_comm_init() Uses this configuration
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Shared session state for cross-transport sequence tracking
   * @details
   * Pointer to the session state owned by rx_comm_manager. Must be
   * initialized via rx_session_init() before passing to rx_uart_comm_init().
   * Required - must not be NULL.
   * @see rx_session_state_t
   */
  rx_session_state_t* session;

  /**
   * @brief UART SCI channel to use
   * @details
   * Selects which SCI channel to use for UART communication.
   * Use k_uart_channel_9 for SCI9 (the RPi5 UART interface).
   * @par Default: k_uart_channel_9
   * @see uart_channel_t
   */
  uart_channel_t channel;
} rx_uart_comm_config_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize UART communication handle
 *
 * @details
 * Sets up frame encoder/decoder, clears internal buffers, and prepares the
 * handle for communication. The UART driver must be initialized separately
 * before this handle can successfully send/receive data.
 *
 * @par Algorithm
 * 1. Validate handle pointer (NULL check)
 * 2. Clear handle memory
 * 3. Initialize frame encoder and decoder
 * 4. Copy configuration (channel, session)
 * 5. Set initialized flag
 *
 * @param[out] handle Pointer to handle to initialize
 *   - Must be valid non-nullptr
 *   - All fields will be overwritten
 *   - Caller maintains ownership
 * @param[in] config Configuration options (must not be NULL)
 *   - session: Required shared session state pointer
 *   - channel: UART channel (k_uart_channel_9 for SCI9)
 *   - Structure is copied, need not persist after call
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Handle initialized successfully
 * @retval k_rx_err_invalid_arg handle or config is nullptr
 *
 * @pre handle points to allocated rx_uart_comm_handle_t
 * @pre config->session points to initialized rx_session_state_t
 * @post handle->initialized == 1
 * @post Buffers cleared to zero
 *
 * @note Safe to call multiple times (reinitializes handle)
 * @note Not thread-safe; call from single initialization context
 *
 * @see rx_uart_comm_deinit() Cleanup resources
 * @see uart_channel_t UART channel enumeration
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_uart_comm_init(rx_uart_comm_handle_t*       handle,
                                         const rx_uart_comm_config_t* config);

/**
 * @brief Deinitialize UART communication handle
 *
 * @details
 * Clears the initialized flag to prevent further use of the handle.
 * Does not deinitialize the underlying UART hardware.
 *
 * @param[in,out] handle Pointer to initialized handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if handle is nullptr
 *
 * @pre handle must not be nullptr
 * @pre handle must have been initialized via rx_uart_comm_init()
 * @post handle->initialized == 0
 * @post Further calls to rx_uart_comm_send/receive will return k_rx_err_invalid_state
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_uart_comm_deinit(rx_uart_comm_handle_t* handle);

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Send a frame over UART (SCI9)
 *
 * @details
 * Encodes the payload into a frame with automatic sequence number assignment
 * and CRC-32 calculation, then transmits it via uart_write_channel().
 * This operation is non-blocking once the data is queued in the UART FIFO.
 *
 * @par Algorithm
 * 1. Validate handle and payload pointers
 * 2. Check initialization
 * 3. Check payload fits in TX buffer
 * 4. Encode frame with header, payload, and CRC-32
 * 5. Write encoded frame to UART via uart_write_channel()
 * 6. Increment session TX sequence counter
 *
 * @param[in,out] handle Initialized handle
 *   - Must be initialized via rx_uart_comm_init()
 *   - Session TX sequence incremented on success
 * @param[in] type Frame type (typically k_frame_type_response)
 *   - See rx_frame_type_t for valid types
 * @param[in] flags Frame flags (ORed together)
 *   - See rx_frame_flags_t for valid flags; 0 for no flags
 * @param[in] payload Pointer to payload data
 *   - Must not be nullptr (even for zero-length payload)
 *   - Copied to internal buffer (no ownership transfer)
 * @param[in] payload_len Payload length in bytes
 *   - Max: k_uart_comm_tx_buffer_size - frame overhead
 *   - 0 is valid (empty payload)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Frame sent successfully
 * @retval k_rx_err_invalid_arg handle or payload is nullptr
 * @retval k_rx_err_invalid_state Not initialized
 * @retval k_rx_err_invalid_size Payload exceeds maximum frame size
 * @retval k_rx_err_hw_error UART hardware write failure (propagated from uart_write_channel())
 * @retval other Propagated from rx_session_next_tx() or rx_frame_encode()
 *
 * @pre handle initialized via rx_uart_comm_init()
 * @pre payload != nullptr
 * @post On success: session TX sequence incremented
 * @post On success: frame transmitted via UART
 * @post On failure: no state changes
 *
 * @note Thread safety: caller must provide external synchronization
 * @warning When SCI9 is used for comm, debug log output to SCI9 is suppressed
 *
 * @see rx_uart_comm_receive() Receive frames from peer
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_uart_comm_send(rx_uart_comm_handle_t* handle,
                                         rx_frame_type_t        type,
                                         uint8_t                flags,
                                         const uint8_t*         payload,
                                         uint32_t               payload_len);

/* =============================================================================
 * Receive API
 * =============================================================================
 */

/**
 * @brief Receive and decode a frame from UART (SCI9)
 *
 * @details
 * Reads available bytes from the UART RX buffer, feeds them through the
 * frame decoder, validates CRC-32, and extracts the payload. Operates in
 * polling mode (non-blocking); returns k_rx_err_no_data if no complete
 * frame is available yet.
 *
 * @par Algorithm
 * 1. Validate handle and frame pointers
 * 2. Check initialization
 * 3. Check uart_rx_available() for pending data
 * 4. If data available: read bytes via uart_read_channel()
 * 5. Feed bytes to frame decoder
 * 6. If frame complete: validate CRC, populate output frame
 * 7. Update session RX sequence
 *
 * @param[in,out] handle Initialized handle
 *   - Must be initialized via rx_uart_comm_init()
 *   - Internal buffers and decoder state modified on each call
 * @param[out] frame Pointer to frame structure for output
 *   - Must be valid non-nullptr
 *   - All fields populated on success
 * @param[in] timeout_ms Timeout in milliseconds
 *   - 0: Poll once, return immediately if no complete frame
 *   - (timeout not currently used; always polls once)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Complete frame received and decoded
 * @retval k_rx_err_invalid_arg handle or frame is nullptr
 * @retval k_rx_err_invalid_state Not initialized
 * @retval k_rx_err_no_data No complete frame available yet
 * @retval k_rx_err_crc_mismatch CRC-32 validation failed
 * @retval k_rx_err_protocol_error Invalid frame format
 *
 * @pre handle initialized via rx_uart_comm_init()
 * @pre frame points to valid rx_frame_t
 * @post On success: frame contains decoded data
 * @post On success: session RX sequence updated
 *
 * @note Loop iterations bounded by k_uart_comm_max_receive_iterations
 * @note Thread safety: caller must provide external synchronization
 *
 * @see rx_uart_comm_send() Send frames to peer
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_uart_comm_receive(rx_uart_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
