/* lib/rx_harq/inc/rx_harq.h */

/**
 * @file rx_harq.h
 * @brief Hybrid Automatic Repeat Request (HARQ) Protocol for RX72N
 * @details
 * Implements Chase Combining HARQ Type I for reliable communication.
 * This is a C port of the Go implementation in star-gateway for
 * bit-exact compatibility.
 *
 * Features:
 * - Chase Combining with soft bit accumulation
 * - Configurable maximum retries
 * - FEC integration (optional)
 * - Static memory allocation only (no malloc)
 *
 * @note Chase Combining: element-wise soft bit addition across retransmissions
 *
 * @see star-gateway/internal/harq/ for Go reference implementation
 * @see star-gateway/internal/fec/combiner.go for Chase Combiner
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_HARQ_H
#define STAR_RX_HARQ_H

#include <stdbool.h>
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
typedef enum {
  k_harq_max_payload      = 1024,  /**< Maximum payload size in bytes */
  k_harq_default_retries  = 3,     /**< Default maximum retries */
  k_harq_default_combines = 3,     /**< Default max combining attempts */
  k_harq_soft_buffer_size = 16400, /**< Max soft buffer size for Chase Combining
                                     * (1024*8+6)*2 = 16396 bits, rounded to 16400 */
} rx_harq_params_t;

/* =============================================================================
 * HARQ State Machine
 * =============================================================================
 */

/**
 * @brief HARQ state machine states
 */
typedef enum {
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
 * @return k_rx_ok on success, k_rx_err_invalid_arg if combiner is NULL
 */
rx_err_t rx_chase_combiner_init(rx_chase_combiner_t* combiner, uint8_t max_combines);

/**
 * @brief Deinitialize Chase Combiner
 *
 * @param[in,out] combiner Pointer to combiner handle
 * @return k_rx_ok on success
 */
rx_err_t rx_chase_combiner_deinit(rx_chase_combiner_t* combiner);

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
 * @return k_rx_err_invalid_arg if any pointer is NULL or len is 0
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
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if no soft bits have been added
 */
rx_err_t
rx_chase_combiner_combined(rx_chase_combiner_t* combiner, rx_soft_bit_t* output, uint32_t* len);

/**
 * @brief Reset combiner for new frame
 *
 * Clears accumulated buffer and count.
 *
 * @param[in,out] combiner Pointer to combiner handle
 */
void rx_chase_combiner_reset(rx_chase_combiner_t* combiner);

/**
 * @brief Check if more transmissions can be combined
 *
 * @param[in] combiner Pointer to combiner handle
 * @return true if count < max_combines
 */
bool rx_chase_combiner_can_add(const rx_chase_combiner_t* combiner);

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
  uint64_t            decoder_survivors[k_harq_soft_buffer_size / 2]; /**< Decoder buf */
  rx_soft_bit_t       decode_buffer[k_harq_soft_buffer_size];         /**< Combined soft bits buf */
  uint8_t             fec_enabled; /**< Non-zero if FEC is enabled */
  uint8_t             initialized; /**< Non-zero if initialized */
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
 * @brief Initialize HARQ handle
 *
 * @param[out] harq Pointer to HARQ handle
 * @param[in]  config Configuration (NULL for defaults)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if harq is NULL
 */
rx_err_t rx_harq_init(rx_harq_handle_t* harq, const rx_harq_config_t* config);

/**
 * @brief Deinitialize HARQ handle
 *
 * @param[in,out] harq Pointer to HARQ handle
 * @return k_rx_ok on success
 */
rx_err_t rx_harq_deinit(rx_harq_handle_t* harq);

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
 */
void rx_harq_reset(rx_harq_handle_t* harq);

/**
 * @brief Encode payload with FEC (if enabled)
 *
 * @param[in]  harq HARQ handle
 * @param[in]  payload Input payload
 * @param[in]  payload_len Payload length
 * @param[out] output Output buffer (must be large enough for encoded data)
 * @param[out] output_len Actual output length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_harq_encode(rx_harq_handle_t* harq,
                        const uint8_t*    payload,
                        uint32_t          payload_len,
                        uint8_t*          output,
                        uint32_t*         output_len);

/**
 * @brief Decode soft bits with combining and FEC
 *
 * Adds soft bits to combiner, attempts decode. On failure, keeps accumulating.
 *
 * @param[in]  harq HARQ handle
 * @param[in]  soft_bits Received soft bits
 * @param[in]  soft_len Number of soft bits
 * @param[in]  expected_output_len Expected decoded length
 * @param[out] output Decoded output buffer
 * @param[out] output_len Actual decoded length
 *
 * @return k_rx_ok on successful decode
 * @return k_rx_err_protocol_error if decode failed (more retries possible)
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_harq_decode(rx_harq_handle_t*    harq,
                        const rx_soft_bit_t* soft_bits,
                        uint32_t             soft_len,
                        uint32_t             expected_output_len,
                        uint8_t*             output,
                        uint32_t*            output_len);

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
bool rx_harq_can_retry(const rx_harq_handle_t* harq);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_HARQ_H */
