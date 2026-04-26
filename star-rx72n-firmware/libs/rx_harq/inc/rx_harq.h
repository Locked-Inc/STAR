/**
 * @file rx_harq.h
 * @brief Hybrid Automatic Repeat Request (HARQ) Protocol for RX72N
 *
 * @details
 * Implements **Chase Combining HARQ Type I** for reliable communication over
 * lossy channels. Combines FEC (Forward Error Correction) with ARQ (Automatic
 * Repeat Request) to achieve both error correction and detection.
 *
 * ## HARQ Protocol Overview
 *
 * HARQ merges FEC and ARQ:
 * - **FEC**: Corrects random bit errors via convolutional coding
 * - **ARQ**: Requests retransmission when FEC fails
 * - **Chase Combining**: Accumulates soft bits from multiple transmissions
 *
 * ## Chase Combining Theory
 *
 * When a transmission fails to decode, instead of discarding it, the receiver:
 * 1. **Stores** soft bits from failed attempt
 * 2. **Requests** retransmission (NACK)
 * 3. **Adds** new soft bits to accumulated values (element-wise)
 * 4. **Decodes** combined soft bits (higher SNR -> better chance)
 *
 * ### Mathematical Model
 *
 * For @f$ n @f$ transmissions of the same data, the combined soft bit is:
 * @f[
 *   s_{\text{combined}}[i] = \sum_{k=1}^{n} s_k[i]
 * @f]
 *
 * This provides **diversity gain**: independent noise samples are averaged out.
 *
 * ### SNR Improvement
 *
 * Assuming independent noise across transmissions, SNR improves as:
 * @f[
 *   \text{SNR}_{\text{combined}} \approx n \cdot \text{SNR}_{\text{single}}
 * @f]
 *
 * In dB: @f$ 10 \log_{10}(n) @f$ gain (e.g., 3 dB for 2 transmissions, 4.8 dB for 3).
 *
 * ## Protocol Sequence
 *
 * @msc
 * Transmitter, Channel, Receiver;
 *
 * ... [label="Transmission 1"];
 * Transmitter box Transmitter [label="Encode with FEC\n100 bytes -> 202 bytes"];
 * Transmitter => Channel [label="Soft bits (1st attempt)"];
 * Channel box Channel [label="Add noise\nBER = 10^-3"];
 * Channel => Receiver [label="Noisy soft bits"];
 * Receiver box Receiver [label="Decode attempt\nFAILED"];
 * Receiver box Receiver [label="Store soft bits\nin combiner"];
 * Receiver => Transmitter [label="NACK (request retry)"];
 *
 * ... [label="Transmission 2"];
 * Transmitter => Channel [label="Soft bits (2nd attempt)"];
 * Channel box Channel [label="Independent noise\nBER = 10^-3"];
 * Channel => Receiver [label="Noisy soft bits"];
 * Receiver box Receiver [label="Add to combiner\nCombine attempts 1+2"];
 * Receiver box Receiver [label="Decode combined\nSUCCESS (3 dB gain)"];
 * Receiver => Transmitter [label="ACK (success)"];
 * Transmitter box Transmitter [label="Move to next frame"];
 * @endmsc
 *
 * ## HARQ State Machine
 *
 * @startuml
 * [*] --> Idle
 * Idle --> WaitingAck : tx_encode()
 * WaitingAck --> Idle : rx_ACK()
 * WaitingAck --> Combining : rx_NACK()
 * Combining --> Idle : decode_success()
 * Combining --> Error : max_retries_reached()
 * Error --> Idle : reset()
 * @enduml
 *
 * **State Descriptions:**
 * - **Idle**: Ready for new transmission, no pending frames
 * - **WaitingAck**: Waiting for ACK/NACK from receiver
 * - **Combining**: Accumulating retransmissions, attempting decode
 * - **Error**: Unrecoverable error (exceeded retry limit)
 *
 * **Transition Guards:**
 * - WaitingAck -> Combining: Only if retry_count < max_retries
 * - Combining -> Error: When retry_count >= max_retries AND decode still fails
 *
 * ## Performance Characteristics
 *
 * | Metric                  | Value                                    |
 * |-------------------------|------------------------------------------|
 * | Throughput (1 tx)       | 100 / 202 ~ 49.5% (FEC overhead)        |
 * | Throughput (2 tx avg)   | 100 / 404 ~ 24.8% (2x retransmissions)  |
 * | Latency (1 tx)          | ~1 ms encode + transmit                  |
 * | Latency (2 tx)          | ~2-5 ms (includes RTT for NACK)         |
 * | Memory (per handle)     | ~82 KB (dominated by FEC decoder)        |
 *
 * ## Features
 *
 * - [OK] **Chase Combining**: Soft bit accumulation (int16 to prevent overflow)
 * - [OK] **FEC Integration**: Optional K=7 rate 1/2 convolutional code
 * - [OK] **Configurable Retries**: 1-255 attempts (default: 3)
 * - [OK] **Static Allocation**: Zero malloc (NASA Rule 3)
 * - [OK] **Thread Safety**: Safe if handles not shared
 *
 * @par Hardware Requirements
 * | Resource | Usage                              |
 * |----------|------------------------------------|
 * | CPU      | RX72N @ 240 MHz                   |
 * | RAM      | ~82 KB per HARQ handle            |
 * | Flash    | ~8 KB (code size with FEC)        |
 *
 * @par Module Dependencies
 * - [rx_fec.h](rx_fec_8h.html): FEC encoder/decoder
 * - [rx_err.h](rx_err_8h.html): Error codes
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No recursion, goto, setjmp/longjmp
 * - **Rule 2**: [OK] All loops bounded (k_harq_soft_buffer_size)
 * - **Rule 3**: [OK] Zero dynamic allocation
 * - **Rule 4**: [OK] Functions <= 60 lines
 * - **Rule 5**: [OK] All functions have >=2 pre/postconditions
 *
 * @note **Thread Safety**: HARQ handles are NOT thread-safe. Caller must
 *       provide synchronization if sharing handles between threads.
 *
 * @warning Soft bit accumulation uses int16 (range +/-32768). With max 3 combines
 *          and soft bits +/-127, accumulator saturates at +/-381 (no overflow).
 *
 * @see star-gateway/internal/harq/ Go reference implementation
 * @see star-gateway/internal/fec/combiner.go Chase Combiner algorithm
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 * @see "Hybrid ARQ Schemes" by S. Lin & D. Costello
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test test_rx_harq.c Unit tests for HARQ protocol
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "rx_fec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * HARQ Parameters
 * =============================================================================
 */

/**
 * @brief HARQ protocol parameters
 */
typedef enum : uint16_t {
  k_harq_max_payload           = 1024,  /**< Maximum payload size in bytes */
  k_harq_default_retries       = 3,     /**< Default maximum retries */
  k_harq_default_combines      = 3,     /**< Default max combining attempts */
  k_harq_soft_buffer_size      = 16400, /**< Max soft buffer size for Chase Combining
                                     * (1024*8+6)*2 = 16396 bits, rounded to 16400 */
  k_harq_survivors_buffer_size = k_harq_soft_buffer_size / 2U, /**< Survivors buffer size */
} rx_harq_params_t;

/* =============================================================================
 * HARQ State Machine
 * =============================================================================
 */

/**
 * @brief HARQ state machine states
 */
typedef enum : uint8_t {
  k_harq_state_idle        = 0, /**< Ready for new transmission */
  k_harq_state_waiting_ack = 1, /**< Waiting for ACK */
  k_harq_state_combining   = 2, /**< Combining retransmissions */
  k_harq_state_error       = 3, /**< Unrecoverable error */
} rx_harq_state_t;

/* =============================================================================
 * Chase Combiner
 * =============================================================================
 */

/**
 * @brief Chase Combiner for soft bit accumulation
 *
 * Stores soft bits from failed transmission attempts and combines them
 * element-wise (addition) before decoding. Uses int16 accumulators to
 * prevent overflow.
 */
typedef struct {
  int16_t  accumulated[k_harq_soft_buffer_size]; /**< Accumulated soft bits */
  uint32_t expected_len;                         /**< Expected soft bits len */
  uint8_t  count;                                /**< Transmissions combined */
  uint8_t  max_combines;                         /**< Max combining attempts */
  uint8_t  initialized;                          /**< Non-zero if initialized */
} rx_chase_combiner_t;

/**
 * @brief Initialize Chase Combiner
 *
 * @param[out] combiner Pointer to combiner handle
 * @param[in]  max_combines Maximum number of combining attempts (0 = default)
 * @return k_rx_ok on success, k_rx_err_invalid_arg if combiner is nullptr
 */
[[nodiscard]] rx_err_t rx_chase_combiner_init(rx_chase_combiner_t* combiner, uint8_t max_combines);

/**
 * @brief Deinitialize Chase Combiner
 *
 * @param[in,out] combiner Pointer to combiner handle
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_chase_combiner_deinit(rx_chase_combiner_t* combiner);

/**
 * @brief Add soft bits from a transmission attempt
 *
 * Accumulates soft bits element-wise. First call sets expected length.
 *
 * @param[in,out] combiner Initialized combiner handle
 * @param[in]     soft_bits Received soft bits
 * @param[in]     len Number of soft bits
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr or len is 0
 * @return k_rx_err_invalid_size if len doesn't match expected length
 * @return k_rx_err_busy if max combines reached
 */
rx_err_t
rx_chase_combiner_add(rx_chase_combiner_t* combiner, const rx_soft_bit_t* soft_bits, uint32_t len);

/**
 * @brief Get combined soft bits for decoding
 *
 * Returns accumulated values clamped to SoftBit range [-127, +127].
 *
 * @param[in]  combiner Initialized combiner handle
 * @param[out] output Output buffer for combined soft bits
 * @param[out] len Number of combined soft bits
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr
 * @return k_rx_err_invalid_state if no soft bits have been added
 */
[[nodiscard]] rx_err_t rx_chase_combiner_combined(const rx_chase_combiner_t* combiner,
                                                  rx_soft_bit_t*             output,
                                                  uint32_t*                  len);

/**
 * @brief Reset combiner for new frame
 *
 * Clears accumulated buffer and count.
 *
 * @param[in,out] combiner Pointer to combiner handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if combiner is nullptr
 * @return k_rx_err_invalid_state if combiner is not initialized
 */
[[nodiscard]] rx_err_t rx_chase_combiner_reset(rx_chase_combiner_t* combiner);

/**
 * @brief Check if more transmissions can be combined
 *
 * @param[in] combiner Pointer to combiner handle
 * @return true if count < max_combines
 */
[[nodiscard]] bool rx_chase_combiner_can_add(const rx_chase_combiner_t* combiner);

/**
 * @brief Get number of combined transmissions
 *
 * @param[in] combiner Pointer to combiner handle
 * @return Number of transmissions combined
 */
uint8_t rx_chase_combiner_count(const rx_chase_combiner_t* combiner);

/* =============================================================================
 * HARQ Handle
 * =============================================================================
 */

/**
 * @brief HARQ protocol handle
 *
 * Contains all state for HARQ operation including FEC encoder/decoder
 * and Chase Combiner.
 */
typedef struct {
  rx_harq_state_t     state;       /**< Current HARQ state */
  uint16_t            tx_sequence; /**< TX sequence number */
  uint16_t            rx_sequence; /**< RX sequence number */
  uint8_t             retry_count; /**< Current retry count */
  uint8_t             max_retries; /**< Maximum retries allowed */
  rx_chase_combiner_t combiner;    /**< Chase Combiner */
  rx_fec_encoder_t    encoder;     /**< FEC encoder */
  rx_fec_decoder_t    decoder;     /**< FEC decoder */
  uint64_t            decoder_survivors[k_harq_survivors_buffer_size]; /**< Decoder buf */
  rx_soft_bit_t       decode_buffer[k_harq_soft_buffer_size]; /**< Combined soft bits buf */
  uint8_t             fec_enabled;                            /**< Non-zero if FEC is enabled */
  uint8_t             initialized;                            /**< Non-zero if initialized */
} rx_harq_handle_t;

/**
 * @brief HARQ configuration
 */
typedef struct {
  uint8_t max_retries;  /**< Maximum transmission attempts (0 = default) */
  uint8_t fec_enabled;  /**< Non-zero to enable FEC encoding/decoding */
  uint8_t max_combines; /**< Maximum combining attempts (0 = default) */
} rx_harq_config_t;

/**
 * @brief HARQ decode parameters
 */
typedef struct {
  const rx_soft_bit_t* soft_bits;           /**< Received soft bits */
  uint32_t             soft_len;            /**< Number of soft bits */
  uint32_t             expected_output_len; /**< Expected decoded length */
} rx_harq_decode_params_t;

/**
 * @brief Initialize HARQ handle
 *
 * @param[out] harq Pointer to HARQ handle
 * @param[in]  config Configuration (NULL for defaults)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if harq is nullptr
 */
[[nodiscard]] rx_err_t rx_harq_init(rx_harq_handle_t* harq, const rx_harq_config_t* config);

/**
 * @brief Deinitialize HARQ handle
 *
 * @param[in,out] harq Pointer to HARQ handle
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_harq_deinit(rx_harq_handle_t* harq);

/**
 * @brief Get current HARQ state
 *
 * @param[in] harq Pointer to HARQ handle
 * @return Current HARQ state
 */
rx_harq_state_t rx_harq_get_state(const rx_harq_handle_t* harq);

/**
 * @brief Reset HARQ for new transaction
 *
 * Resets state to idle and clears combiner.
 *
 * @param[in,out] harq Pointer to HARQ handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if harq is nullptr
 * @return k_rx_err_invalid_state if harq is not initialized
 */
[[nodiscard]] rx_err_t rx_harq_reset(rx_harq_handle_t* harq);

/**
 * @brief Encode payload with FEC (if enabled)
 *
 * @param[in]  harq HARQ handle
 * @param[in]  payload Input payload
 * @param[in]  payload_len Payload length
 * @param[out] output Output buffer (must be large enough for encoded data)
 * @param[in]  output_size Size of output buffer in bytes
 * @param[out] output_len Actual output length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr
 * @return k_rx_err_invalid_size if output buffer too small
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_harq_encode(const rx_harq_handle_t* harq,
                                      const uint8_t*          payload,
                                      uint32_t                payload_len,
                                      uint8_t*                output,
                                      uint32_t                output_size,
                                      uint32_t*               output_len);

/**
 * @brief Decode soft bits with combining and FEC
 *
 * Adds soft bits to combiner, attempts decode. On failure, keeps accumulating.
 *
 * @param[in]  harq HARQ handle
 * @param[in]  params Decode parameters
 * @param[out] output Decoded output buffer
 * @param[out] output_len Actual decoded length
 *
 * @return k_rx_ok on successful decode
 * @return k_rx_err_protocol_error if decode failed (more retries possible)
 * @return k_rx_err_invalid_arg if any pointer is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_harq_decode(rx_harq_handle_t*              harq,
                                      const rx_harq_decode_params_t* params,
                                      uint8_t*                       output,
                                      uint32_t*                      output_len);

/**
 * @brief Get current retry count
 *
 * @param[in] harq Pointer to HARQ handle
 * @return Current retry count
 */
uint8_t rx_harq_get_retry_count(const rx_harq_handle_t* harq);

/**
 * @brief Check if more retries are available
 *
 * @param[in] harq Pointer to HARQ handle
 * @return true if retry_count < max_retries
 */
[[nodiscard]] bool rx_harq_can_retry(const rx_harq_handle_t* harq);

#ifdef __cplusplus
}
#endif
