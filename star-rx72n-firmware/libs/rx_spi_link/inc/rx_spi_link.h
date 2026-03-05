/**
 * @file rx_spi_link.h
 * @brief SPI Link Layer with HARQ
 *
 * @details
 * Implements a reliability layer between the communication manager and the raw
 * SPI transport. This module mirrors the Go gateway's `internal/link/spi.go`,
 * providing:
 *
 * - **FEC encoding** on TX (convolutional K=7, rate 1/2)
 * - **FEC decoding** on RX (soft-decision Viterbi)
 * - **Chase Combining** across retransmissions (soft bit accumulation)
 * - **ACK/NACK handshake** with sequence tracking
 * - **Configurable retries** (default 3)
 *
 * ## Architecture
 *
 * ```
 * rx_comm_manager
 *       |
 *       v
 * +--------------+
 * | rx_spi_link  |  <-- THIS MODULE (HARQ)
 * |              |
 * | FEC encode   |  TX: payload -> FEC encode -> frame -> SPI
 * | FEC decode   |  RX: SPI -> frame -> soft bits -> Viterbi -> payload
 * | Chase Cmb    |  Retry: combine soft bits across attempts
 * | ACK/NACK     |  Handshake with gateway
 * +--------------+
 *       |
 *       v
 * rx_spi_comm (frame + CRC-32 + raw SPI)
 * ```
 *
 * ## Protocol Flow (TX)
 *
 * 1. Application calls rx_spi_link_send(payload)
 * 2. If FEC enabled: rx_harq_encode(payload) -> encoded_payload (2N+2 bytes)
 * 3. rx_spi_comm_send(encoded_payload, flags=FEC_ENABLED|REQUIRES_ACK)
 * 4. Wait for ACK/NACK from gateway (via rx_spi_comm_receive)
 * 5. On NACK or timeout: retransmit (up to max_retries)
 * 6. On ACK: return success
 *
 * ## Protocol Flow (RX)
 *
 * 1. rx_spi_comm_receive() returns a frame
 * 2. If ACK/NACK frame: dispatch to pending TX operation
 * 3. If data frame with FEC flag: convert payload bytes to soft bits
 * 4. Pass soft bits to rx_harq_decode() (Chase Combining + Viterbi)
 * 5. If decode succeeds: send ACK, return decoded payload
 * 6. If decode fails and retries remain: store in combiner, send NACK
 * 7. If max retries exceeded: return error
 *
 * ## Comparison: USB CDC vs SPI Link
 *
 * | Feature             | USB CDC (rx_usb_comm) | SPI Link (rx_spi_link)     |
 * |---------------------|-----------------------|----------------------------|
 * | FEC encoding        | No                    | Yes (K=7, rate 1/2)        |
 * | Viterbi decoding    | No                    | Yes (soft-decision)        |
 * | Chase Combining     | No                    | Yes (int16 accumulation)   |
 * | ACK/NACK            | No (USB HW handles)   | Yes (explicit handshake)   |
 * | Retransmission      | No (USB HW handles)   | Yes (max 3 attempts)       |
 * | CRC-32              | Yes                   | Yes (frame layer)          |
 * | Sequence tracking   | Yes (shared session)  | Yes (shared session)       |
 *
 * ## Memory Requirements
 *
 * | Resource           | Size     | Notes                              |
 * |--------------------|----------|------------------------------------|
 * | rx_spi_link_t      | ~86 KB   | Dominated by HARQ handle (82 KB)   |
 * | FEC encode buffer  | 2050 B   | Max encoded payload (1024*2+2)     |
 * | Soft bits buffer   | 16400 B  | Part of HARQ handle                |
 * | Stack (send)       | ~256 B   | Function locals + encode buffer    |
 * | Stack (receive)    | ~128 B   | Function locals                    |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp/longjmp, recursion
 * - Rule 2: All loops bounded (max_retries, k_fec_max_symbols)
 * - Rule 3: Zero dynamic allocation (all static buffers)
 * - Rule 4: Functions < 60 lines
 * - Rule 5: Minimum 2 pre/postconditions per function
 * - Rule 8: C23 typed enums for all constants
 * - Rule 9: Function pointers for dependency inversion (INTENTIONAL)
 * - Rule 10: Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Only handles HARQ reliability (not framing, not transport)
 * - **O**: Configurable via rx_spi_link_config_t without modifying code
 * - **L**: Substitutable for rx_spi_comm (same send/receive semantics)
 * - **I**: Small focused API (init, deinit, send, receive, reset)
 * - **D**: Depends on rx_spi_comm and rx_harq abstractions
 *
 * @see star-gateway/internal/link/spi.go   Go reference implementation
 * @see rx_harq.h                           HARQ protocol and Chase Combiner
 * @see rx_fec.h                            FEC encoder/decoder
 * @see rx_spi_comm.h                       Raw SPI transport with framing
 *
 * @author Locked, Inc.
 * @date 2026-02-14
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_frame.h"
#include "rx_harq.h"
#include "rx_spi_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * ============================================================================= */

/**
 * @enum rx_spi_link_constants_t
 * @brief SPI link layer compile-time constants
 *
 * @details
 * Protocol timing and retry parameters matching the Go gateway's
 * `internal/link/spi.go` constants.
 *
 * @invariant max_retries must be in range [0, 255], where 0 means use default value of 3
 * @invariant fec_enabled is boolean (0 or 1)
 *
 * @code
 * // Example: Using default constants
 * rx_spi_link_config_t config = {
 *     .spi_handle = &spi_handle,
 *     .fec_enabled = k_spi_link_default_fec_enabled,  // 1 (enabled)
 *     .max_retries = k_spi_link_default_max_retries,  // 3 attempts
 * };
 * @endcode
 *
 * @see star-gateway/internal/link/spi.go Go reference constants (maxRetries=3)
 * @see rx_spi_link_config_t Configuration structure using these defaults
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_spi_link_default_max_retries =
    3, /**< Default maximum transmission attempts (matches Go maxRetries=3). After 3 failed attempts (including Chase Combining), the link returns an error and the comm_manager may fail over to USB. */

  k_spi_link_default_fec_enabled =
    1, /**< Default FEC enabled state (enabled by default, aligns with Go HARQ). FEC/HARQ is enabled by default for reliability. Can be disabled at runtime via config or test modes for raw SPI operation. */
} rx_spi_link_constants_t;

/**
 * @enum rx_spi_link_timing_t
 * @brief SPI link layer timing constants
 *
 * @details
 * Timeout values for ACK waiting and buffer sizing. Matches Go gateway's
 * ackTimeout = 10ms and FEC encoding buffer requirements.
 *
 * @invariant ack_timeout_ms must be > 0 and < 65535 milliseconds
 * @invariant max_encoded_payload = 2 * k_harq_max_payload + k_fec_tail_bits (2050 bytes for 1024-byte max payload)
 *
 * @code
 * // Example: Using timing constants for ACK wait
 * rx_err_t err = rx_spi_comm_receive(spi_handle, &ack_frame, k_spi_link_ack_timeout_ms);
 * if (err == k_rx_err_timeout) {
 *     // No ACK received within 10ms, retry transmission
 * }
 * @endcode
 *
 * @see star-gateway/internal/link/spi.go Go ackTimeout constant (10ms)
 * @see rx_fec.h FEC encoding parameters (rate 1/2, tail bits)
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_spi_link_ack_timeout_ms =
    10, /**< ACK/NACK wait timeout in milliseconds (matches Go ackTimeout=10ms). How long to wait for ACK/NACK after sending a frame. If no response arrives within this window, the frame is retransmitted. */
} rx_spi_link_timing_t;

/**
 * @enum rx_spi_link_buffer_t
 * @brief Buffer size constants for SPI link layer
 *
 * @details
 * Defines buffer sizes for FEC-encoded payloads. Sized to accommodate the
 * maximum payload after FEC encoding (rate 1/2) plus tail bits.
 *
 * **Formula Derivation:**
 * - FEC rate 1/2: Each input byte produces 2 output bytes
 * - k_harq_max_payload = 1024 bytes
 * - FEC encoded size = 2 * k_harq_max_payload = 2 * 1024 = 2048 bytes
 * - k_fec_tail_bits = 6 bits (trellis termination, K=7 constraint length)
 * - Tail bytes = ceiling(k_fec_tail_bits / 8) = ceiling(6 / 8) = 1 byte
 * - Total = 2048 + 1 = 2049 bytes
 *
 * @see rx_harq.h HARQ protocol maximum payload size
 * @see rx_fec.h FEC encoding parameters (rate 1/2, tail bits)
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_spi_link_max_encoded_payload =
    2049, /**< Maximum FEC-encoded payload size in bytes. Formula: bytes = (2 * k_harq_max_payload) + ceiling(k_fec_tail_bits / 8) = (2 * 1024) + ceiling(6 / 8) = 2048 + 1 = 2049. FEC rate 1/2 doubles the payload size, and tail bits (6 bits = 1 byte after ceiling) are appended for trellis termination. */
} rx_spi_link_buffer_t;

/* =============================================================================
 * State Machine
 * ============================================================================= */

/**
 * @enum rx_spi_link_state_t
 * @brief SPI link layer state machine states
 *
 * @details
 * Mirrors the Go gateway's harq.State enum. Tracks whether the link is
 * idle, waiting for acknowledgment, retransmitting, or in error.
 *
 * @startuml
 * [*] --> Idle
 *
 * state Idle {
 *   Idle : entry / clear_retry_count()
 *   Idle : exit / none
 * }
 *
 * state WaitingAck {
 *   WaitingAck : entry / start_ack_timer()
 *   WaitingAck : exit / stop_ack_timer()
 * }
 *
 * state Retransmitting {
 *   Retransmitting : entry / increment_retry_count()
 *   Retransmitting : exit / log_retry_attempt()
 * }
 *
 * state Error {
 *   Error : entry / set_error_flag(), log_failure()
 *   Error : exit / clear_error_flag()
 * }
 *
 * Idle --> WaitingAck : send() / encode_payload()
 * WaitingAck --> Idle : ACK received / clear_retry_count()
 * WaitingAck --> Retransmitting : NACK or timeout / [retry_count < max]
 * Retransmitting --> WaitingAck : retransmit sent / update_sequence()
 * Retransmitting --> Error : [retry_count >= max_retries]
 * Error --> Idle : reset() / clear_state()
 * @enduml
 *
 * @par State Transition Table
 *
 * | From State      | To State        | Event/Condition          | Guard/Action                  | Entry Action              | Exit Action         |
 * |-----------------|-----------------|--------------------------|-------------------------------|---------------------------|---------------------|
 * | Idle            | WaitingAck      | send() called            | encode_payload()              | start_ack_timer()         | none                |
 * | WaitingAck      | Idle            | ACK received             | clear_retry_count()           | clear_retry_count()       | stop_ack_timer()    |
 * | WaitingAck      | Retransmitting  | NACK or timeout          | retry_count < max_retries     | increment_retry_count()   | stop_ack_timer()    |
 * | Retransmitting  | WaitingAck      | retransmit sent          | update_sequence()             | start_ack_timer()         | log_retry_attempt() |
 * | Retransmitting  | Error           | max retries exceeded     | retry_count >= max_retries    | set_error_flag(), log()   | log_retry_attempt() |
 * | Error           | Idle            | reset() called           | clear_state()                 | clear_retry_count()       | clear_error_flag()  |
 *
 * @see rx_spi_link_get_state() Query current state
 * @see rx_spi_link_reset() Reset to Idle state
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_spi_link_state_idle           = 0, /**< Ready for new send/receive */
  k_spi_link_state_waiting_ack    = 1, /**< Frame sent, waiting for ACK/NACK */
  k_spi_link_state_retransmitting = 2, /**< Retransmitting after NACK/timeout */
  k_spi_link_state_error          = 3, /**< Unrecoverable error (max retries) */
} rx_spi_link_state_t;

/* =============================================================================
 * Types
 * ============================================================================= */

/**
 * @struct rx_spi_link_config_t
 * @brief SPI link layer configuration
 *
 * @details
 * Passed to rx_spi_link_init() to configure the link behavior. All zero
 * values use defaults.
 *
 * @invariant spi_handle must be non-NULL and initialized via rx_spi_comm_init()
 * @invariant max_retries in range [0, 255], where 0 means use default (3)
 * @invariant fec_enabled is boolean (0 or 1)
 *
 * @par Example
 * @code{.c}
 * rx_spi_link_config_t cfg = {
 *     .spi_handle  = &g_spi_comm,
 *     .fec_enabled = true,
 *     .max_retries = 3,
 * };
 * @endcode
 *
 * @see rx_spi_link_init() Initialization function consuming this config
 * @see rx_spi_comm_handle_t Underlying SPI transport handle type
 * @since Version 1.0.0
 */
typedef struct {
  rx_spi_comm_handle_t*
    spi_handle; /**< Underlying SPI transport (REQUIRED, must be non-NULL and initialized via rx_spi_comm_init) */
  bool
    fec_enabled; /**< Enable FEC encoding/decoding (true=HARQ with Chase Combining, false=raw frames) */
  uint8_t max_retries; /**< Maximum transmission attempts (0=use default of 3, range [0-255]) */
} rx_spi_link_config_t;

/**
 * @struct rx_spi_link_receive_result_t
 * @brief Result of a successful receive operation
 *
 * @details
 * Contains the decoded payload and metadata about the decoding process.
 * Mirrors Go's harq.ReceiveResult.
 *
 * @warning This structure contains a large 1024-byte payload buffer (total struct
 * size ~1032 bytes). MUST be statically allocated (global or static variable) only.
 * DO NOT allocate on the stack (causes stack overflow when passing to rx_spi_link_receive()).
 * DO NOT use dynamic allocation (malloc/new) - zero heap usage required per NASA Rule 3 and
 * project coding guidelines. All buffers must use enum-defined sizes for compile-time allocation.
 *
 * @invariant payload_len <= k_harq_max_payload (1024 bytes)
 * @invariant sequence in range [0, 65535] (wraps around)
 * @invariant combining_count in range [0, max_retries], typically [1-4]
 *
 * @see rx_spi_link_receive() Function that populates this result structure
 * @see star-gateway/internal/harq/harq.go Go equivalent (ReceiveResult)
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t payload
    [k_harq_max_payload]; /**< Decoded payload data (buffer size: k_harq_max_payload = 1024 bytes) */
  uint32_t payload_len;   /**< Decoded payload length in bytes (range: [0, k_harq_max_payload]) */
  uint16_t sequence;      /**< Frame sequence number (range: [0, 65535], wraps around) */
  rx_frame_type_t
       frame_type;  /**< Frame type (e.g., k_frame_type_command, k_frame_type_telemetry) */
  bool fec_decoded; /**< True if FEC/Viterbi decoding was applied, false if raw passthrough */
  uint8_t
    combining_count; /**< Number of Chase Combining attempts used (1 = first attempt, >1 = retransmissions combined) */
  bool
    is_retransmit; /**< True if frame had k_frame_flag_retransmit set, false for first transmission */
} rx_spi_link_receive_result_t;

/**
 * @struct rx_spi_link_t
 * @brief SPI link layer handle
 *
 * @details
 * Contains the HARQ state machine, FEC encoder/decoder, Chase Combiner,
 * and a pointer to the underlying SPI transport. Approximately 86 KB
 * total (dominated by the HARQ handle's FEC decoder buffers).
 *
 * ## Memory Layout
 *
 * | Field       | Size    | Description                         |
 * |-------------|---------|-------------------------------------|
 * | spi_handle  | 8 B     | Pointer to SPI transport            |
 * | harq        | ~82 KB  | HARQ handle (FEC + Chase Combiner)  |
 * | state       | 1 B     | Current link state                  |
 * | fec_enabled | 1 B     | FEC enable flag                     |
 * | max_retries | 1 B     | Max transmission attempts           |
 * | initialized | 1 B     | Initialization flag                 |
 * | fec_buf     | 2050 B  | FEC encode output buffer            |
 *
 * @invariant When initialized == true, spi_handle must be non-NULL
 * @invariant state must be one of k_spi_link_state_* values
 * @invariant max_retries in range [1, 255] when initialized (0 converted to 3 at init)
 * @invariant fec_encode_buf size == k_spi_link_max_encoded_payload (2050 bytes)
 *
 * @warning This structure is ~86 KB. Allocate statically, not on the stack.
 *
 * @see rx_spi_link_init() Initialization function
 * @see rx_spi_link_config_t Configuration structure
 * @see rx_harq_handle_t HARQ handle containing FEC encoder/decoder
 * @since Version 1.0.0
 */
typedef struct {
  rx_spi_comm_handle_t*
    spi_handle; /**< Underlying SPI transport (must be non-NULL when initialized, set via config) */
  rx_harq_handle_t
    harq; /**< HARQ handle containing FEC encoder/decoder and Chase Combiner (~82 KB) */
  rx_spi_link_state_t state; /**< Current link state (idle, waiting_ack, retransmitting, error) */
  bool fec_enabled; /**< FEC enabled flag (true=HARQ, false=raw frames, immutable after init) */
  uint8_t
    max_retries; /**< Maximum transmission attempts (range [1-255], 0 at init converted to 3) */
  bool
    initialized; /**< Initialization flag (true=ready for use, false=must call rx_spi_link_init) */

  uint8_t fec_encode_buf
    [k_spi_link_max_encoded_payload]; /**< Staging buffer for FEC-encoded payloads (size: 2049 bytes = 2*1024+1). Used by rx_spi_link_send() to hold FEC-encoded data before passing to rx_spi_comm_send(). Formula: 2 * k_harq_max_payload + ceiling(k_fec_tail_bits / 8), where k_fec_tail_bits = 6. */
} rx_spi_link_t;

/* =============================================================================
 * Lifecycle API
 * ============================================================================= */

/**
 * @brief Initialize SPI link layer
 *
 * @details
 * Initializes the HARQ handle (including FEC encoder/decoder and Chase
 * Combiner) and stores configuration. The underlying SPI transport must
 * already be initialized.
 *
 * @param[out] link   Link handle to initialize
 * @param[in]  config Configuration (spi_handle is REQUIRED)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg link or config isnullptr
 * @retval k_rx_err_invalid_arg config->spi_handle isnullptr
 *
 * @pre link must point to valid, allocated memory (~86 KB)
 * @pre config->spi_handle must be initialized via rx_spi_comm_init()
 * @post link->initialized == true
 * @post link->harq initialized with FEC if fec_enabled
 *
 * @note Thread Safety: Not thread-safe. Call from single thread during init.
 *
 * @see rx_spi_link_deinit() Cleanup counterpart
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_link_init(rx_spi_link_t* link, const rx_spi_link_config_t* config);

/**
 * @brief Deinitialize SPI link layer
 *
 * @details
 * Deinitializes the HARQ handle and marks the link as uninitialized.
 * Does NOT deinitialize the underlying SPI transport.
 *
 * @param[in,out] link Link handle to deinitialize
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg link isnullptr
 * @retval k_rx_err_invalid_state link not initialized
 *
 * @pre link must be non-NULL
 * @pre link must be initialized (link->initialized == true)
 * @post link->initialized == false
 * @post link->harq internal state deinitialized (HARQ handle invalid)
 *
 * @note Does NOT deinitialize the underlying SPI transport handle
 *
 * @see rx_spi_link_init() Initialization counterpart
 * @see rx_harq_deinit() Internal HARQ deinitialization
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_link_deinit(rx_spi_link_t* link);

/* =============================================================================
 * Send API
 * ============================================================================= */

/**
 * @brief Send payload with HARQ reliability over SPI
 *
 * @details
 * Encodes payload with FEC (if enabled), sends via SPI transport, and
 * waits for ACK/NACK. On NACK or timeout, retransmits up to max_retries.
 *
 * ## Algorithm
 *
 * 1. Validate parameters
 * 2. If FEC enabled: rx_harq_encode(payload) -> encoded (2N+2 bytes)
 * 3. For attempt = 1 to max_retries:
 *    a. Build frame with REQUIRES_ACK | FEC_ENABLED flags
 *    b. If retransmit: add RETRANSMIT flag
 *    c. Send via rx_spi_comm_send()
 *    d. Wait for ACK/NACK (rx_spi_comm_receive with ack_timeout_ms)
 *    e. On ACK: return success
 *    f. On NACK/timeout: continue to next attempt
 * 4. All retries exhausted: return error
 *
 * @param[in,out] link        Initialized link handle
 * @param[in]     type        Frame type (k_frame_type_command, etc.)
 * @param[in]     payload     Data to send
 * @param[in]     payload_len Data length (0-1024 bytes)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Frame sent and ACKed
 * @retval k_rx_err_invalid_arg link, payload NULL or payload_len > 1024
 * @retval k_rx_err_invalid_state link not initialized
 * @retval k_rx_err_retry_limit All retries exhausted (max NACKs/timeouts)
 * @retval k_rx_err_timeout No ACK received on any attempt
 *
 * @pre link must be initialized
 * @pre payload_len <= k_harq_max_payload (1024)
 * @post On success: frame delivered and acknowledged
 * @post On failure: link state set to error (call rx_spi_link_reset)
 *
 * @note Blocking: waits up to max_retries * ack_timeout_ms
 * @note Thread Safety: Not thread-safe. Serialize with external mutex.
 *
 * @see rx_spi_link_receive() Receive path
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_link_send(rx_spi_link_t*  link,
                                        rx_frame_type_t type,
                                        const uint8_t*  payload,
                                        uint32_t        payload_len);

/* =============================================================================
 * Receive API
 * ============================================================================= */

/**
 * @brief Receive and decode a frame from SPI with HARQ
 *
 * @details
 * Receives a frame from the SPI transport, handles ACK/NACK dispatch,
 * and performs FEC decoding with Chase Combining if applicable.
 *
 * ## Algorithm
 *
 * 1. Call rx_spi_comm_receive() with timeout
 * 2. If frame is ACK/NACK: handle internally (dispatch), return no-data
 * 3. If frame is PING: auto-respond with PONG (handled by spi_comm)
 * 4. If data frame with FEC flag:
 *    a. Convert payload bytes to soft bits (byte -> 8 soft bits)
 *    b. If retransmit flag: add to Chase Combiner, decode combined
 *    c. If first attempt: decode directly, store in combiner on failure
 *    d. On decode success: send ACK, populate result
 *    e. On decode failure: send NACK, return error
 * 5. If data frame without FEC: pass payload through directly, send ACK
 *
 * @param[in,out] link       Initialized link handle
 * @param[out]    result     Decoded payload and metadata
 * @param[in]     timeout_ms Receive timeout (0 = non-blocking)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Frame received and decoded successfully
 * @retval k_rx_err_invalid_arg link or result isnullptr
 * @retval k_rx_err_invalid_state link not initialized
 * @retval k_rx_err_timeout No frame received within timeout
 * @retval k_rx_err_protocol_error FEC decode failed (NACK sent)
 * @retval k_rx_err_crc_mismatch CRC validation failed at frame layer
 *
 * @pre link must be initialized
 * @pre result must point to valid memory
 * @post On success: result populated with decoded payload and metadata
 * @post On ACK/NACK frame: returns k_rx_err_timeout (control frame, not data)
 *
 * @note Thread Safety: Not thread-safe. Dedicate one thread to receiving.
 *
 * @see rx_spi_link_send() Send path
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_link_receive(rx_spi_link_t* link, rx_spi_link_receive_result_t* result, uint32_t timeout_ms);

/* =============================================================================
 * State Management
 * ============================================================================= */

/**
 * @brief Reset SPI link layer for new session
 *
 * @details
 * Resets the HARQ state machine, clears the Chase Combiner, and returns
 * the link to idle state. Does NOT reset the shared session state
 * (sequence numbers are managed by rx_session).
 *
 * @param[in,out] link Initialized link handle
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg link isnullptr
 * @retval k_rx_err_invalid_state link not initialized
 *
 * @pre link must be non-NULL
 * @pre link must be initialized (link->initialized == true)
 * @post link->state == k_spi_link_state_idle
 * @post Chase Combiner buffers cleared (all soft-decision history discarded)
 *
 * @note Does NOT reset TX/RX sequence numbers (managed externally by rx_session)
 * @note Safe to call after error state to recover link operation
 *
 * @see rx_harq_reset() Internal HARQ reset
 * @see rx_spi_link_get_state() Query current state
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_link_reset(rx_spi_link_t* link);

/**
 * @brief Get current SPI link state
 *
 * @details
 * Returns the current state of the SPI link state machine. Safe to call
 * with nullptr (returns error state). Does not modify link state.
 *
 * @param[in] link Link handle (may be NULL)
 *
 * @return rx_spi_link_state_t Current link state
 * @retval k_spi_link_state_idle Link is ready for new send/receive
 * @retval k_spi_link_state_waiting_ack Waiting for ACK/NACK response
 * @retval k_spi_link_state_retransmitting Retransmitting after NACK/timeout
 * @retval k_spi_link_state_error Unrecoverable error (also returned if link is NULL)
 *
 * @pre link may be NULL (function handles gracefully)
 * @pre If link is non-NULL, may be in any state (initialized or not)
 * @post No state change (pure query function)
 * @post Return value reflects state at time of call (may change immediately after)
 *
 * @note Thread-safe: Read-only operation, no synchronization needed
 * @note NULL-safe: Returns k_spi_link_state_error when link isnullptr
 * @note Does not distinguish between "link is NULL" vs "link is in error state"
 *
 * @see rx_spi_link_t Link handle structure
 * @see rx_spi_link_state_t State enumeration with all possible values
 * @see rx_spi_link_reset() Reset link to idle state
 *
 * @since Version 1.0.0
 */
rx_spi_link_state_t rx_spi_link_get_state(const rx_spi_link_t* link);

/**
 * @brief Check if FEC is enabled on this link
 *
 * @details
 * Queries the FEC/HARQ enabled flag. Returns false if link is NULL.
 * Does not modify link state. FEC setting is configured at init time
 * via rx_spi_link_config_t.fec_enabled and cannot be changed at runtime.
 *
 * @param[in] link Link handle (may be NULL)
 *
 * @return bool FEC enabled status
 * @retval true FEC encoding/decoding is active (Chase Combining, Viterbi)
 * @retval false FEC disabled (raw payload transmission) or link isnullptr
 *
 * @pre link may be NULL (function handles gracefully by returning false)
 * @pre If link is non-NULL, may be in any state (initialized or not)
 * @post No state change (pure query function)
 * @post Return value reflects FEC setting at time of call (constant for link lifetime)
 *
 * @note Thread-safe: Read-only operation, no synchronization needed
 * @note NULL-safe: Returns false when link isnullptr
 * @note FEC setting is immutable after initialization (cannot be changed at runtime)
 * @note When FEC is enabled, rx_spi_link_send/receive use HARQ with Chase Combining
 *
 * @see rx_spi_link_config_t Configuration structure with fec_enabled field
 * @see rx_spi_link_init() Initialization function that sets FEC mode
 * @see k_spi_link_default_fec_enabled Default FEC setting (enabled = 1)
 *
 * @since Version 1.0.0
 */
bool rx_spi_link_fec_enabled(const rx_spi_link_t* link);

#ifdef __cplusplus
}
#endif
