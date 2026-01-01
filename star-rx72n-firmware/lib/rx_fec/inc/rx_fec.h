/**
 * @file rx_fec.h
 * @brief Forward Error Correction (FEC) Codec for RX72N
 *
 * Implements NASA-standard K=7 convolutional encoder and Viterbi decoder.
 * This is a C port of the Go FEC implementation in star-gateway for
 * bit-exact compatibility.
 *
 * Features:
 * - Rate 1/2 convolutional encoding
 * - Soft-decision Viterbi decoding
 * - Hard-decision fallback
 * - Static memory allocation only (no malloc)
 *
 * @note Generator polynomials: G1=0o171 (0xF9), G2=0o133 (0x5B)
 *
 * @see star-gateway/internal/fec/ for Go reference implementation
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 */

#ifndef STAR_RX_FEC_H
#define STAR_RX_FEC_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * FEC Parameters (NASA Standard K=7, Rate 1/2)
 * =============================================================================
 */

/**
 * @brief FEC codec parameters
 *
 * These parameters define the NASA-standard K=7 convolutional code:
 * - Constraint length K=7 (6-bit shift register)
 * - Rate 1/2 (2 output bits per input bit)
 * - Generator polynomials in octal: G1=171, G2=133
 */
typedef enum {
  k_fec_constraint_length = 7,    /**< K=7 constraint length */
  k_fec_num_states        = 64,   /**< 2^(K-1) = 64 states */
  k_fec_tail_bits         = 6,    /**< K-1 tail bits for termination */
  k_fec_g1_octal          = 0171, /**< Generator poly 1 (octal) */
  k_fec_g2_octal          = 0133, /**< Generator poly 2 (octal) */
  k_fec_traceback_depth   = 35,   /**< 5*K traceback depth */
  k_fec_num_input_values  = 2,    /**< Input bit values (0 or 1) */
  k_fec_num_outputs       = 2,    /**< Rate 1/2: 2 outputs per input (G1, G2) */
} rx_fec_params_t;

/* =============================================================================
 * Soft Bit Representation
 * =============================================================================
 */

/**
 * @brief Soft bit type for soft-decision decoding
 *
 * Range: [-127, +127]
 * - Positive values indicate confidence in bit=1
 * - Negative values indicate confidence in bit=0
 * - Zero indicates no confidence (erasure)
 */
typedef int8_t rx_soft_bit_t;

/**
 * @brief Soft bit value limits
 */
typedef enum {
  k_soft_bit_max  = 127,  /**< Maximum soft bit value (confident 1) */
  k_soft_bit_min  = -127, /**< Minimum soft bit value (confident 0) */
  k_soft_bit_zero = 0,    /**< No confidence (erasure) */
} rx_soft_bit_limits_t;

/* =============================================================================
 * Encoder
 * =============================================================================
 */

/**
 * @brief FEC encoder state
 *
 * The encoder is stateless between calls - each encode() resets the shift
 * register. The handle tracks initialization status only.
 */
typedef struct {
  bool initialized; /**< True if initialized */
} rx_fec_encoder_t;

/**
 * @brief Initialize FEC encoder
 *
 * @param[out] enc Pointer to encoder handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if enc is NULL
 */
rx_err_t rx_fec_encoder_init(rx_fec_encoder_t* enc);

/**
 * @brief Deinitialize FEC encoder
 *
 * @param[in,out] enc Pointer to encoder handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if enc is NULL
 */
rx_err_t rx_fec_encoder_deinit(rx_fec_encoder_t* enc);

/**
 * @brief Encode data using convolutional code
 *
 * Encodes input data using rate 1/2 convolutional code with tail termination.
 * Output length = (input_len * 8 + 6) * 2 bits, packed into bytes.
 *
 * @param[in]  enc        Initialized encoder handle
 * @param[in]  input      Input data bytes
 * @param[in]  input_len  Number of input bytes
 * @param[out] output     Output buffer (must be at least rx_fec_encoded_len())
 * @param[out] output_len Actual number of output bytes written
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if encoder not initialized
 */
rx_err_t rx_fec_encode(rx_fec_encoder_t* enc,
                       const uint8_t*    input,
                       uint32_t          input_len,
                       uint8_t*          output,
                       uint32_t*         output_len);

/**
 * @brief Calculate encoded output length
 *
 * @param[in] input_len Number of input bytes
 * @return Number of output bytes after encoding
 */
uint32_t rx_fec_encoded_len(uint32_t input_len);

/* =============================================================================
 * Decoder
 * =============================================================================
 */

/**
 * @brief Maximum symbols for decoder (for static allocation)
 *
 * With 1024 byte max payload, encoded = (1024*8+6)*2 = 16396 bits = 8198 symbols
 * Round up to 8200 for safe margin.
 */
typedef enum {
  k_fec_max_symbols = 8200, /**< Maximum symbols for static allocation */
} rx_fec_buffer_limits_t;

/**
 * @brief FEC decoder state
 *
 * Contains all working buffers for Viterbi decoding. The survivors buffer
 * must be provided by the caller for static allocation.
 */
typedef struct {
  int32_t   path_metrics[k_fec_num_states];     /**< Current path metrics */
  int32_t   new_path_metrics[k_fec_num_states]; /**< Next path metrics */
  uint64_t* survivors;                          /**< Survivor bits (caller-provided) */
  uint32_t  survivors_len;                      /**< Length of survivors buffer */
  uint8_t   branch_table[k_fec_num_states][k_fec_num_input_values]
                      [k_fec_num_outputs]; /**< Precomputed outputs */
  bool initialized;                        /**< True if initialized */
} rx_fec_decoder_t;

/**
 * @brief Initialize FEC decoder
 *
 * The caller must provide a survivors buffer for traceback. Size should be
 * at least (max_symbols / 64 + 1) * k_fec_num_states uint64_t elements.
 *
 * @param[out] dec            Pointer to decoder handle
 * @param[in]  survivors_buf  Caller-provided buffer for survivor bits
 * @param[in]  survivors_len  Number of uint64_t elements in survivors_buf
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL or buffer too small
 */
rx_err_t
rx_fec_decoder_init(rx_fec_decoder_t* dec, uint64_t* survivors_buf, uint32_t survivors_len);

/**
 * @brief Deinitialize FEC decoder
 *
 * @param[in,out] dec Pointer to decoder handle
 * @return k_rx_ok on success
 */
rx_err_t rx_fec_decoder_deinit(rx_fec_decoder_t* dec);

/**
 * @brief Decode soft bits using Viterbi algorithm
 *
 * Performs soft-decision Viterbi decoding on received soft bits.
 * Soft bits should be in pairs (G1, G2) for each encoded symbol.
 *
 * @param[in]  dec                 Initialized decoder handle
 * @param[in]  soft_bits           Received soft bits (pairs)
 * @param[in]  soft_len            Number of soft bits (must be even)
 * @param[in]  expected_output_len Expected decoded length in bytes
 * @param[out] output              Decoded output buffer
 * @param[out] output_len          Actual decoded length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL or soft_len is odd
 * @return k_rx_err_invalid_state if decoder not initialized
 * @return k_rx_err_invalid_size if output buffer too small
 */
rx_err_t rx_fec_decode_soft(rx_fec_decoder_t*    dec,
                            const rx_soft_bit_t* soft_bits,
                            uint32_t             soft_len,
                            uint32_t             expected_output_len,
                            uint8_t*             output,
                            uint32_t*            output_len);

/**
 * @brief Decode hard bits using Viterbi algorithm
 *
 * Converts hard bits to soft bits internally, then performs Viterbi decoding.
 * Less accurate than soft-decision decoding but useful for testing.
 *
 * @param[in]  dec                 Initialized decoder handle
 * @param[in]  data                Received hard bits (packed bytes)
 * @param[in]  data_len            Number of bytes
 * @param[in]  expected_output_len Expected decoded length in bytes
 * @param[out] output              Decoded output buffer
 * @param[out] output_len          Actual decoded length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if decoder not initialized
 */
rx_err_t rx_fec_decode_hard(rx_fec_decoder_t* dec,
                            const uint8_t*    data,
                            uint32_t          data_len,
                            uint32_t          expected_output_len,
                            uint8_t*          output,
                            uint32_t*         output_len);

/* =============================================================================
 * Utility Functions
 *
 * NOTE: These are intentionally static inline in the header because:
 * 1. They are trivial one-liner conversions (single ternary operation)
 * 2. They are called in tight loops during encoding/decoding
 * 3. They need to be available across multiple translation units (rx_fec.c,
 *    rx_harq.c, tests)
 * 4. Function call overhead would exceed the actual computation cost
 *
 * For such trivial operations, static inline is the standard C pattern and
 * results in better performance with no binary size increase (the function
 * body is smaller than the call setup/teardown).
 * =============================================================================
 */

/**
 * @brief Convert hard bit to soft bit
 *
 * @param[in] bit Hard bit value (0 or 1)
 * @return Soft bit: k_soft_bit_min for 0, k_soft_bit_max for 1
 */
static inline rx_soft_bit_t rx_fec_hard_to_soft(uint8_t bit)
{
  return (bit != 0) ? k_soft_bit_max : k_soft_bit_min;
}

/**
 * @brief Convert soft bit to hard bit
 *
 * @param[in] soft Soft bit value
 * @return Hard bit: 1 if soft >= 0, 0 otherwise
 */
static inline uint8_t rx_fec_soft_to_hard(rx_soft_bit_t soft)
{
  return (soft >= 0) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_FEC_H */
