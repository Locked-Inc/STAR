/* lib/rx_fec/src/rx_fec.c */

/**
 * @file rx_fec.c
 * @brief Forward Error Correction (FEC) Codec Implementation
 *
 * @details
 * Implements NASA-standard K=7, rate 1/2 convolutional encoder and soft-decision
 * Viterbi decoder. This is a C port of star-gateway/internal/fec/ ensuring
 * bit-exact compatibility between RX72N firmware and RPi5 gateway.
 *
 * ## Implementation Architecture
 *
 * ### Encoder (Stateless)
 * - Each encode() call starts from zero state
 * - Processes input MSB-first (network byte order)
 * - Appends 6 tail bits to flush to zero
 * - Output packed MSB-first into bytes
 *
 * ### Decoder (Viterbi Algorithm)
 * - **Forward Pass**: Compute path metrics for all 64 states at each time step
 * - **Traceback**: Reconstruct most-likely input sequence from survivors
 * - **Soft Decision**: Uses branch metrics based on soft bit correlation
 * - **Memory**: Survivors buffer stores predecessor bits for traceback
 *
 * ## Algorithm Complexity
 *
 * | Operation         | Time Complexity | Space Complexity |
 * |-------------------|-----------------|------------------|
 * | Encoding          | O(n)            | O(1)             |
 * | Decoding (Viterbi)| O(n x 64 x 2)   | O(n x 64 bits)   |
 * | Traceback         | O(n)            | O(1)             |
 *
 * where n = number of input bits.
 *
 * ## Viterbi Algorithm Overview
 *
 * The Viterbi algorithm finds the maximum likelihood path through the trellis
 * of encoder states using dynamic programming:
 *
 * 1. **Initialization**: Set path metric for state 0 to 0, all others to inf
 * 2. **Forward Pass**: For each received symbol pair (r_0, r_1):
 *    - For each current state with finite metric:
 *      - Compute metrics for both possible transitions (input=0, input=1)
 *      - Update next state metrics (keep minimum)
 *      - Store survivor path (which previous state was best)
 * 3. **Traceback**: Starting from final state 0 (encoder flushed), work backwards
 *    through survivors to recover input sequence
 *
 * ## Branch Metric Calculation
 *
 * Uses correlation metric for soft-decision decoding:
 *
 * @f[
 *   M_{\text{branch}} = 32768 - (r_0 \cdot e_0' + r_1 \cdot e_1')
 * @f]
 *
 * where:
 * - @f$ r_i @f$ = received soft bit (-127 to +127)
 * - @f$ e_i' = +127 @f$ if expected bit is 1, @f$ -127 @f$ if 0
 * - Lower metric = better match
 *
 * ## Bit Ordering Convention
 *
 * **Throughout this implementation: MSB-first (network byte order)**
 * - Bit index 0 -> byte[0] bit 7 (MSB)
 * - Bit index 7 -> byte[0] bit 0 (LSB)
 * - Bit index 8 -> byte[1] bit 7 (MSB)
 *
 * This matches wire protocols and simplifies interoperability.
 *
 * @par Module Dependencies
 * - [rx_fec.h](rx_fec_8h.html): Public API declarations
 * - [rx_check.h](rx_check_8h.html): Assertion macros (RX_ASSERT)
 * - [rx_bit_constants.h](rx_bit_constants_8h.html): Bit manipulation constants
 * - string.h: memset for buffer initialization
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No recursion, goto, or setjmp/longjmp
 * - **Rule 2**: [OK] All loops bounded (k_fec_max_symbols, k_fec_num_states)
 * - **Rule 3**: [OK] Zero dynamic allocation
 * - **Rule 4**: [OK] All functions <= 60 lines
 * - **Rule 5**: [OK] All functions have >=2 assertions
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 STAR Project. Licensed under MIT License.
 *
 * @since Version 1.0.0
 * @version 1.0.0
 */

#include "rx_fec.h"

#include <string.h>

#include "rx_bit_constants.h"
#include "rx_check.h"

/* =============================================================================
 * Private Constants
 * =============================================================================
 */

/**
 * @brief FEC implementation constants
 */
typedef enum : uint16_t {
  k_fec_zero                = 0,     /**< Zero constant */
  k_fec_msb_bit_position    = 7,     /**< MSB position in byte (0-indexed) */
  k_fec_shift_register_bits = 6,     /**< K-1 shift register size */
  k_fec_correlation_offset  = 32768, /**< Correlation metric offset (2^15) */
  k_fec_state_shift_amount  = 5,     /**< k_fec_constraint_length - 2 */
  k_fec_bit_mask            = 1,     /**< Mask for extracting single bit */
} rx_fec_impl_constants_t;

/**
 * @brief FEC output indices for G1 and G2 generator polynomials
 */
typedef enum : uint8_t {
  k_fec_output_g1 = 0, /**< G1 generator output (first encoded bit) */
  k_fec_output_g2 = 1, /**< G2 generator output (second encoded bit) */
} rx_fec_output_index_t;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Calculate parity (XOR of all bits) of a byte
 *
 * Uses parallel XOR reduction to compute parity in O(log n) operations.
 * This is more efficient than looping through each bit.
 *
 * Algorithm:
 * - Step 1 (x ^= x >> 4): XOR upper nibble with lower nibble
 *   Original: [b7 b6 b5 b4][b3 b2 b1 b0]
 *   Result:   [x  x  x  x ][b7^b3 b6^b2 b5^b1 b4^b0]
 *
 * - Step 2 (x ^= x >> 2): XOR pairs of bits
 *   Result bits [1:0] now contain XOR of all 8 original bits
 *
 * - Step 3 (x ^= x >> 1): Final XOR to get single parity bit
 *   Result bit [0] = XOR of all original bits
 *
 * @param[in] x Input byte
 * @return 0 if even parity (even number of 1s), 1 if odd parity
 */
static uint8_t internal_parity(uint8_t x)
{
  /* Compute parity using parallel XOR reduction */
  x ^= x >> 4; /* XOR upper nibble with lower nibble */
  x ^= x >> 2; /* XOR bit pairs */
  x ^= x >> 1; /* XOR final pair */

  const uint8_t result = x & k_fec_bit_mask;

  /* Post-condition: Result must be 0 or 1 */
  RX_ASSERT((result == k_fec_zero) || (result == k_fec_bit_mask), "Parity result must be 0 or 1");
  RX_ASSERT(result <= k_fec_bit_mask, "Parity result must be within bit mask");

  return result;
}

/**
 * @brief Set a bit at specified index in output buffer (MSB first)
 *
 * Bit ordering (MSB first, network byte order):
 *   bit_idx=0 -> byte 0, bit 7 (MSB)
 *   bit_idx=7 -> byte 0, bit 0 (LSB)
 *   bit_idx=8 -> byte 1, bit 7 (MSB)
 *
 * @param[out] output Output buffer
 * @param[in]  bit_idx Bit index (0 = MSB of first byte)
 * @param[in]  value Bit value (0 or 1)
 */
static void internal_set_output_bit(uint8_t* output, const uint32_t bit_idx, uint8_t value)
{
  /* Pre-condition 1: output must be valid */
  RX_ASSERT(output != nullptr, "Output buffer must not be nullptr");

  /* Pre-condition 2: bit index must be within maximum symbol range */
  RX_ASSERT(bit_idx < (k_fec_max_symbols * k_rx_bits_per_byte), "Bit index out of range");
  value = (value != 0) ? k_fec_bit_mask : 0;

  const uint32_t byte_idx = bit_idx / k_rx_bits_per_byte;
  const uint32_t bit_pos  = k_fec_msb_bit_position - (bit_idx % k_rx_bits_per_byte); /* MSB first */

  if (value != 0) {
    output[byte_idx] |= (uint8_t)(1U << bit_pos);
  }
}

/**
 * @brief Get a bit at specified index from buffer (MSB first)
 *
 * Bit ordering (MSB first, network byte order):
 *   bit_idx=0 -> byte 0, bit 7 (MSB)
 *   bit_idx=7 -> byte 0, bit 0 (LSB)
 *   bit_idx=8 -> byte 1, bit 7 (MSB)
 *
 * @param[in] data Input buffer
 * @param[in] bit_idx Bit index (0 = MSB of first byte)
 * @return Bit value (0 or 1)
 */
static uint8_t internal_get_bit(const uint8_t* data, uint32_t bit_idx)
{
  /* Pre-condition: data must be valid */
  RX_ASSERT(data != nullptr, "Data buffer must not be nullptr");

  const uint32_t byte_idx = bit_idx / k_rx_bits_per_byte;
  const uint32_t bit_pos  = k_fec_msb_bit_position - (bit_idx % k_rx_bits_per_byte); /* MSB first */

  const uint8_t result = (data[byte_idx] >> bit_pos) & k_fec_bit_mask;

  /* Post-condition: result must be 0 or 1 */
  RX_ASSERT((result == k_fec_zero) || (result == k_fec_bit_mask), "Bit result must be 0 or 1");

  return result;
}

/**
 * @brief Encode a single bit and update encoder state
 *
 * @param[in,out] state Current encoder state (6-bit shift register).
 *                      Modified: shifts right and incorporates input_bit.
 * @param[in]     input_bit Input bit (0 or 1)
 * @param[out]    out0 First output bit (G1)
 * @param[out]    out1 Second output bit (G2)
 */
static void
internal_encode_bit(uint8_t* state, const uint8_t input_bit, uint8_t* out0, uint8_t* out1)
{
  /* Pre-condition 1: All pointers must be valid */
  RX_ASSERT(state != nullptr, "State pointer must not be nullptr");
  RX_ASSERT(out0 != nullptr, "Output 0 pointer must not be nullptr");
  RX_ASSERT(out1 != nullptr, "Output 1 pointer must not be nullptr");

  /* Pre-condition 2: input_bit must be 0 or 1 */
  RX_ASSERT((input_bit == k_fec_zero) || (input_bit == k_fec_bit_mask), "Input bit must be 0 or 1");

  /* Shift in the new bit (input is MSB of the combined state) */
  const uint8_t combined = (uint8_t)((input_bit << k_fec_shift_register_bits) | *state);

  /* Calculate output bits using generator polynomials */
  *out0 = internal_parity(combined & k_fec_g1_octal);
  *out1 = internal_parity(combined & k_fec_g2_octal);

  /* Update state (shift right, new bit enters from left) */
  *state = combined >> k_fec_bit_mask;
}

/**
 * @brief Initialize the branch table for Viterbi decoder
 *
 * Precomputes expected output bits for each state and input combination.
 *
 * @param[out] branch_table Table to populate [state][input][output]
 */
static void internal_init_branch_table(
  uint8_t branch_table[k_fec_num_states][k_fec_num_input_values][k_fec_num_outputs])
{
  /* Pre-condition: branch_table array parameter is always valid in C */
  /* Loop bounds guarantee state < k_fec_num_states and input < k_fec_num_input_values */

  for (uint8_t state = k_fec_zero; state < k_fec_num_states; state++) {
    for (uint8_t input = k_fec_zero; input < k_fec_num_input_values; input++) {
      const uint8_t combined =
        (uint8_t)(((uint8_t)input << k_fec_shift_register_bits) | (uint8_t)state);
      branch_table[state][input][k_fec_output_g1] = internal_parity(combined & k_fec_g1_octal);
      branch_table[state][input][k_fec_output_g2] = internal_parity(combined & k_fec_g2_octal);
    }
  }
}

/**
 * @brief Compute branch metric for soft decoding
 *
 * Uses correlation metric: higher correlation = lower metric (better).
 * Formula: 32768 - (soft0 * exp0_soft + soft1 * exp1_soft)
 *
 * @param[in] soft0 First received soft bit
 * @param[in] soft1 Second received soft bit
 * @param[in] exp0 Expected first bit (0 or 1)
 * @param[in] exp1 Expected second bit (0 or 1)
 * @return Branch metric (lower is better)
 */
static int32_t internal_branch_metric(const rx_soft_bit_t soft0,
                                      const rx_soft_bit_t soft1,
                                      const uint8_t       exp0,
                                      const uint8_t       exp1)
{
  /* Pre-conditions: expected bits must be 0 or 1 */
  RX_ASSERT((exp0 == k_fec_zero) || (exp0 == k_fec_bit_mask), "Expected bit 0 must be 0 or 1");
  RX_ASSERT((exp1 == k_fec_zero) || (exp1 == k_fec_bit_mask), "Expected bit 1 must be 0 or 1");

  /* Convert expected bits to soft values: 0 -> -127, 1 -> +127 */
  const int32_t exp0_soft = (exp0 != 0) ? k_soft_bit_max : k_soft_bit_min;
  const int32_t exp1_soft = (exp1 != 0) ? k_soft_bit_max : k_soft_bit_min;

  /* Correlation = soft0*exp0_soft + soft1*exp1_soft */
  const int32_t correlation = ((int32_t)soft0 * exp0_soft) + ((int32_t)soft1 * exp1_soft);

  /* Negate and offset to keep metrics positive (lower is better) */
  return k_fec_correlation_offset - correlation;
}

/**
 * @brief Process one symbol pair through the Viterbi trellis
 *
 * Updates path metrics and survivors for one time step.
 *
 * @param[in,out] dec Decoder handle. Modified: path_metrics and survivors updated.
 * @param[in]     soft0 First soft bit of symbol pair
 * @param[in]     soft1 Second soft bit of symbol pair
 * @param[in]     symbol_idx Time step index
 */
static void internal_viterbi_process_symbol(rx_fec_decoder_t*   dec,
                                            const rx_soft_bit_t soft0,
                                            const rx_soft_bit_t soft1,
                                            const uint32_t      symbol_idx)
{
  /* Pre-conditions */
  RX_ASSERT(dec != nullptr, "Decoder handle must not be nullptr");
  RX_ASSERT(symbol_idx < dec->survivors_len, "Symbol index exceeds survivors buffer length");

  /* Reset new path metrics */
  for (uint8_t i = k_fec_zero; i < k_fec_num_states; i++) {
    dec->new_path_metrics[i] = INT32_MAX;
  }

  /* Clear survivors for this time step */
  dec->survivors[symbol_idx] = k_fec_zero;

  /* For each current state, compute transitions */
  for (uint8_t state = k_fec_zero; state < k_fec_num_states; state++) {
    if (dec->path_metrics[state] == INT32_MAX) {
      continue;
    }

    /* Try both input bits (0 and 1) */
    for (uint8_t input = k_fec_zero; input < k_fec_num_input_values; input++) {
      /* Get expected output bits for this transition */
      const uint8_t exp0 = dec->branch_table[state][input][k_fec_output_g1];
      const uint8_t exp1 = dec->branch_table[state][input][k_fec_output_g2];

      /* Compute branch metric */
      const int32_t branch_metric = internal_branch_metric(soft0, soft1, exp0, exp1);

      /* Compute next state: shift right and insert input as MSB */
      const uint8_t next_state =
        (state >> k_fec_bit_mask) | ((uint8_t)input << k_fec_state_shift_amount);

      /* Compute new path metric */
      const int32_t new_metric = dec->path_metrics[state] + branch_metric;

      /* Compare and select */
      if (new_metric < dec->new_path_metrics[next_state]) {
        dec->new_path_metrics[next_state] = new_metric;

        /* Store predecessor's LSB for traceback */
        dec->survivors[symbol_idx] &= ~((uint64_t)k_fec_bit_mask << (uint8_t)next_state);
        if ((state & k_fec_bit_mask) == k_fec_bit_mask) {
          dec->survivors[symbol_idx] |= ((uint64_t)k_fec_bit_mask << (uint8_t)next_state);
        }
      }
    }
  }

  /* Swap path metrics */
  for (uint8_t i = k_fec_zero; i < k_fec_num_states; i++) {
    dec->path_metrics[i]     = dec->new_path_metrics[i];
    dec->new_path_metrics[i] = INT32_MAX;
  }
}

/**
 * @brief Run Viterbi forward pass through trellis
 *
 * Initializes path metrics and processes all symbols through the trellis.
 *
 * @param[in,out] dec Decoder handle. Modified: path_metrics updated.
 * @param[in]     soft_bits Received soft bits (pairs)
 * @param[in]     num_symbols Number of symbols to process
 */
static void internal_viterbi_forward_pass(rx_fec_decoder_t*    dec,
                                          const rx_soft_bit_t* soft_bits,
                                          const uint32_t       num_symbols)
{
  /* Pre-conditions */
  RX_ASSERT(dec != nullptr, "Decoder handle must not be nullptr");
  RX_ASSERT(soft_bits != nullptr, "Soft bits buffer must not be nullptr");

  /* Initialize path metrics: state 0 = 0, others = MAX */
  for (uint8_t i = k_fec_zero; i < k_fec_num_states; i++) {
    dec->path_metrics[i] = INT32_MAX;
  }
  dec->path_metrics[k_fec_zero] = k_fec_zero;

  /* Process each symbol pair through the trellis */
  const uint32_t limit = (num_symbols < k_fec_max_symbols) ? num_symbols : k_fec_max_symbols;
  for (uint32_t t = k_fec_zero; t < limit; t++) {
    const rx_soft_bit_t soft0 = soft_bits[t * k_fec_num_outputs + k_fec_output_g1];
    const rx_soft_bit_t soft1 = soft_bits[t * k_fec_num_outputs + k_fec_output_g2];
    internal_viterbi_process_symbol(dec, soft0, soft1, t);
  }
}

/**
 * @brief Perform Viterbi traceback to extract decoded bits
 *
 * Works backwards through the trellis from the terminal state to recover
 * the maximum likelihood input sequence.
 *
 * @param[in]  dec Decoder handle
 * @param[in]  num_symbols Total number of symbols processed
 * @param[in]  data_bits Number of data bits (excluding tail)
 * @param[out] output Decoded output buffer
 * @param[out] output_bytes Number of output bytes
 */
static void internal_viterbi_traceback(const rx_fec_decoder_t* dec,
                                       const uint32_t          num_symbols,
                                       const uint32_t          data_bits,
                                       uint8_t*                output,
                                       const uint32_t          output_bytes)
{
  RX_ASSERT(dec != nullptr, "Decoder handle must not be nullptr");
  RX_ASSERT(output != nullptr, "Output buffer must not be nullptr");
  RX_ASSERT(output_bytes > k_fec_zero, "Output bytes must be greater than zero");
  RX_ASSERT(num_symbols <= dec->survivors_len, "Number of symbols exceeds survivors buffer");

  /* Clear output buffer */
  memset(output, k_fec_zero, output_bytes);

  if (num_symbols == k_fec_zero) {
    return;
  }

  /* Start from state 0 (encoder is flushed to zero by tail bits) */
  uint8_t state = k_fec_zero;

  /* Traceback: work backwards through the trellis */
  const uint32_t limit = (num_symbols < k_fec_max_symbols) ? num_symbols : k_fec_max_symbols;
  for (uint32_t t = limit; t > k_fec_zero; t--) {
    const uint32_t t_idx = t - k_fec_bit_mask;

    /* The input bit is the MSB of the current state */
    const uint8_t input_bit = (uint8_t)((state >> k_fec_state_shift_amount) & k_fec_bit_mask);

    /* Store decoded bit if it's a data bit (not tail bit) */
    if (t_idx < data_bits) {
      if (input_bit != k_fec_zero) {
        const uint32_t byte_idx = t_idx / k_rx_bits_per_byte;
        const uint32_t bit_pos  = k_fec_msb_bit_position - (t_idx % k_rx_bits_per_byte);
        output[byte_idx] |= (uint8_t)(k_fec_bit_mask << bit_pos);
      }
    }

    /* Get predecessor's LSB from survivors */
    const uint8_t predecessor_lsb = (uint8_t)((dec->survivors[t_idx] >> state) & k_fec_bit_mask);

    /* Compute predecessor state: shift left and insert the LSB */
    state = ((state << k_fec_bit_mask) & (k_fec_num_states - k_fec_bit_mask)) | predecessor_lsb;
  }
}

/* =============================================================================
 * Encoder API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize FEC encoder
 *
 * @param[in] enc Pointer to encoder structure
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if enc is nullptr
 */
rx_err_t rx_fec_encoder_init(rx_fec_encoder_t* enc)
{
  if (enc == nullptr) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = true;
  return k_rx_ok;
}

/**
 * @brief Deinitialize FEC encoder
 *
 * @param[in] enc Pointer to encoder structure
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if enc is nullptr
 */
rx_err_t rx_fec_encoder_deinit(rx_fec_encoder_t* enc)
{
  if (enc == nullptr) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = false;
  return k_rx_ok;
}

/**
 * @brief Calculate encoded output length for given input
 *
 * Computes the number of bytes required for FEC-encoded output
 * based on input data length. Accounts for tail bits and rate-1/2 encoding.
 *
 * @param[in] input_len Length of input data in bytes
 *
 * @return Encoded output length in bytes, or 0 if input length invalid
 *
 * @pre input_len must be in range [1, k_fec_max_input_bytes]
 */
uint32_t rx_fec_encoded_len(const uint32_t input_len)
{
  if (input_len == k_fec_zero) {
    return k_fec_zero;
  }

  if (input_len > k_fec_max_input_bytes) {
    return k_fec_zero;
  }

  /* Output bits = (input bits + tail bits) * 2 */
  const uint32_t input_bits        = input_len * k_rx_bits_per_byte;
  const uint32_t total_input_bits  = input_bits + k_fec_tail_bits;
  const uint32_t total_output_bits = total_input_bits * k_fec_num_outputs;

  /* Round up to full bytes */
  return (total_output_bits + k_fec_msb_bit_position) / k_rx_bits_per_byte;
}

/**
 * @brief Encode data using convolutional FEC
 *
 * Applies rate-1/2, constraint-length-7 convolutional encoding
 * to input data. Output length is approximately 2x input length.
 *
 * @param[in]  enc Pointer to initialized encoder
 * @param[in]  input Pointer to input data buffer
 * @param[in]  input_len Length of input data (1 to k_fec_max_input_bytes)
 * @param[out] output Pointer to output buffer (must be >= rx_fec_encoded_len(input_len))
 * @param[out] output_len Pointer to store actual output length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr or input_len is 0
 * @return k_rx_err_invalid_state if encoder not initialized
 * @return k_rx_err_invalid_size if input_len exceeds maximum
 *
 * @pre enc must be initialized via rx_fec_encoder_init()
 * @pre All pointers must be non-nullptr
 * @pre input_len in range [1, k_fec_max_input_bytes]
 *
 * @post output buffer contains FEC-encoded data
 * @post *output_len set to actual encoded length
 */
rx_err_t rx_fec_encode(const rx_fec_encoder_t* enc,
                       const uint8_t*          input,
                       const uint32_t          input_len,
                       uint8_t*                output,
                       uint32_t*               output_len)
{
  if (enc == nullptr || input == nullptr || output == nullptr || output_len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!enc->initialized) {
    return k_rx_err_invalid_state;
  }

  if (input_len == k_fec_zero) {
    return k_rx_err_invalid_arg;
  }

  if (input_len > k_fec_max_input_bytes) {
    return k_rx_err_invalid_size;
  }

  /* Calculate output size */
  const uint32_t expected_output_len = rx_fec_encoded_len(input_len);

  /* Clear output buffer */
  memset(output, k_fec_zero, expected_output_len);

  /* Reset encoder state */
  uint8_t  state       = k_fec_zero;
  uint32_t out_bit_idx = k_fec_zero;

  /* Calculate byte limit before loop to avoid redundant checks inside */
  const uint32_t byte_limit =
    (input_len < k_fec_max_input_bytes) ? input_len : k_fec_max_input_bytes;

  /* Encode each input byte, MSB first */
  for (uint32_t byte_idx = k_fec_zero; byte_idx < byte_limit; byte_idx++) {
    const uint8_t b = input[byte_idx];
    for (int8_t i = (int8_t)k_fec_msb_bit_position; i >= (int8_t)k_fec_zero; i--) {
      const uint8_t input_bit = (b >> i) & k_fec_bit_mask;
      uint8_t       out0;
      uint8_t       out1;

      internal_encode_bit(&state, input_bit, &out0, &out1);

      /* Pack output bits */
      internal_set_output_bit(output, out_bit_idx++, out0);
      internal_set_output_bit(output, out_bit_idx++, out1);
    }
  }

  /* Append tail bits (zeros) to flush encoder to zero state */
  for (uint8_t i = k_fec_zero; i < k_fec_tail_bits; i++) {
    uint8_t out0;
    uint8_t out1;
    internal_encode_bit(&state, k_fec_zero, &out0, &out1);
    internal_set_output_bit(output, out_bit_idx++, out0);
    internal_set_output_bit(output, out_bit_idx++, out1);
  }

  *output_len = expected_output_len;
  return k_rx_ok;
}

/* =============================================================================
 * Decoder API Implementation
 * =============================================================================
 */

/**
 * @brief Validate decode parameters before Viterbi decoding
 *
 * Pre-conditions:
 * - dec, params, num_symbols_out must be non-nullptr
 * - dec must be initialized
 * - params->soft_bits, output, output_len must be non-nullptr
 * - params->soft_len must be non-zero and divisible by 2 (G1, G2 pairs)
 *
 * Validation checks:
 * - Calculated num_symbols from expected_output_len must be valid and within bounds
 * - Sufficient soft bits must be available for calculated num_symbols
 *   (NOT silently reduced - caller error if params are inconsistent)
 * - Survivors buffer must be large enough for num_symbols
 *
 * Post-condition:
 * - If successful, num_symbols_out contains the number of symbols to decode
 * - If parameters are invalid or inconsistent, returns appropriate error code
 *
 * @param[in]  dec Pointer to FEC decoder (must be initialized)
 * @param[in]  params Soft-bit decode parameters
 * @param[out] num_symbols_out Calculated number of symbols to decode
 *
 * @return k_rx_ok on successful validation
 * @return k_rx_err_invalid_arg if pointer checks or soft_len checks fail
 * @return k_rx_err_invalid_state if decoder not initialized
 * @return k_rx_err_invalid_size if calculated symbols out of range or insufficient soft bits
 */
static rx_err_t internal_validate_decode_params(rx_fec_decoder_t*                  dec,
                                                const rx_fec_decode_soft_params_t* params,
                                                uint32_t*                          num_symbols_out)
{
  if (dec == nullptr || params == nullptr || num_symbols_out == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (params->soft_bits == nullptr || params->output == nullptr || params->output_len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!dec->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Soft bits must come in pairs (G1, G2) */
  if (params->soft_len == k_fec_zero || (params->soft_len % k_fec_num_outputs) != k_fec_zero) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t num_symbols =
    (params->expected_output_len > k_fec_zero)
      ? (uint32_t)((params->expected_output_len * k_rx_bits_per_byte) + k_fec_tail_bits)
      : k_fec_zero;

  if (num_symbols < k_fec_tail_bits) {
    return k_rx_err_invalid_size;
  }

  if (num_symbols > k_fec_max_symbols) {
    return k_rx_err_invalid_size;
  }

  /* Verify sufficient soft bits are available for the calculated num_symbols.
   * This is NOT a fallback condition - if the caller provides inconsistent
   * expected_output_len and soft_len parameters (e.g., expects to decode 100
   * bytes but only provides soft bits for 50 bytes), that is a programming error
   * that must be caught and reported, not silently degraded. Graceful degradation
   * would hide bugs in parameter preparation. */
  if (num_symbols * k_fec_num_outputs > params->soft_len) {
    return k_rx_err_invalid_size;
  }

  /* Check survivors buffer is large enough */
  if (num_symbols > dec->survivors_len) {
    return k_rx_err_invalid_size;
  }

  *num_symbols_out = num_symbols;
  return k_rx_ok;
}

/**
 * @brief Initialize FEC decoder
 *
 * Initializes decoder state and associates it with provided survivors buffer.
 * The buffer is used for Viterbi algorithm path storage.
 *
 * @param[in,out] dec Pointer to decoder structure
 * @param[in]     survivors_buf Pointer to survivors buffer for Viterbi algorithm
 * @param[in]     survivors_len Length of survivors buffer in uint64_t entries
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if dec or survivors_buf is nullptr
 * @return k_rx_err_invalid_size if survivors_len is 0
 *
 * @pre dec and survivors_buf must be non-nullptr
 * @pre survivors_len must be > 0 (minimum 1 entry per symbol)
 *
 * @post dec->initialized set to true
 * @post Branch table initialized
 */
rx_err_t
rx_fec_decoder_init(rx_fec_decoder_t* dec, uint64_t* survivors_buf, const uint32_t survivors_len)
{
  if (dec == nullptr || survivors_buf == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Minimum survivors buffer size: at least 1 entry per symbol */
  if (survivors_len == k_fec_zero) {
    return k_rx_err_invalid_size;
  }

  dec->survivors     = survivors_buf;
  dec->survivors_len = survivors_len;

  /* Initialize branch table */
  internal_init_branch_table(dec->branch_table);

  dec->initialized = true;
  return k_rx_ok;
}

/**
 * @brief Deinitialize FEC decoder
 *
 * @param[in] dec Pointer to decoder structure
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if dec is nullptr
 */
rx_err_t rx_fec_decoder_deinit(rx_fec_decoder_t* dec)
{
  if (dec == nullptr) {
    return k_rx_err_invalid_arg;
  }

  dec->survivors     = nullptr;
  dec->survivors_len = k_fec_zero;
  dec->initialized   = false;
  return k_rx_ok;
}

/**
 * @brief Decode FEC-encoded data using soft-decision Viterbi algorithm
 *
 * @details
 * Implements Rate-1/2 Viterbi soft-decision decoding with constraint length 7.
 * Accepts soft-decision values (signed integers in range [-128, 127]) to
 * achieve approximately 3 dB coding gain over hard-decision decoding.
 *
 * Algorithm steps:
 * 1. Validate parameters via internal_validate_decode_params()
 * 2. Forward pass: compute branch and path metrics for all symbols
 * 3. Calculate data bit count (num_symbols - tail bits)
 * 4. Traceback: extract maximum-likelihood decoded bit sequence
 * 5. Pack decoded bits into output byte array
 *
 * @param[in] dec    Pointer to initialized FEC decoder (must be initialized via
 *                   rx_fec_decoder_init(); survivors buffer must be sized for input)
 * @param[in] params Decode parameters:
 *                   - soft_bits: signed soft values, G1/G2 interleaved pairs
 *                   - soft_len: must be non-zero and divisible by 2
 *                   - expected_output_len: expected decoded byte count (> 0)
 *                   - output: output buffer (must be >= expected_output_len bytes)
 *                   - output_len: written with actual decoded byte count on success
 *
 * @return rx_err_t Status code
 * @retval k_rx_ok             Decoding succeeded; output_len updated with byte count
 * @retval k_rx_err_invalid_arg dec or params (or required fields) are nullptr;
 *                              or soft_len is zero or not divisible by 2
 * @retval k_rx_err_invalid_state dec is not initialized
 * @retval k_rx_err_invalid_size  expected_output_len out of range; or derived
 *                                num_symbols falls outside [k_fec_tail_bits,
 *                                k_fec_max_symbols]; or data_bits == 0 after
 *                                subtracting tail bits
 *
 * @pre dec must be initialized via rx_fec_decoder_init()
 * @pre params->soft_len must be divisible by 2 (G1/G2 symbol pairs)
 * @pre params->expected_output_len must be > 0 and <= k_fec_max_input_bytes
 *
 * @post *params->output_len written with actual decoded byte count on k_rx_ok
 * @post params->output contains decoded data bytes on k_rx_ok
 * @post dec path metric arrays updated (internal state consumed by traceback)
 *
 * @note Not thread-safe; caller must ensure exclusive access to dec and params
 * @note Soft values: positive = bit-1 likely, negative = bit-0 likely (signed).
 *
 * @see rx_fec_decoder_init()  Initialize decoder before calling this function
 * @see rx_fec_decode_hard()   Hard-decision wrapper (lower coding gain, simpler input)
 * @see rx_fec_encode()        Encoder that produces the data this function decodes
 *
 * @since Version 1.0.0
 *
 * @code
 * // Example: soft-decision decode of a previously FEC-encoded buffer
 * rx_fec_decoder_t            dec;
 * uint64_t                    survivors[k_fec_max_symbols];
 * rx_soft_bit_t               soft_buf[k_fec_max_symbols * k_fec_num_outputs];
 * uint8_t                     out_buf[k_fec_max_input_bytes];
 * uint32_t                    out_len = k_fec_zero;
 *
 * (void)rx_fec_decoder_init(&dec, survivors, k_fec_max_symbols);
 *
 * // Populate soft_buf from channel (positive = bit-1 likely)
 * rx_fec_decode_soft_params_t p = {
 *     .soft_bits           = soft_buf,
 *     .soft_len            = encoded_len_bits,
 *     .expected_output_len = original_data_len,
 *     .output              = out_buf,
 *     .output_len          = &out_len,
 * };
 * rx_err_t err = rx_fec_decode_soft(&dec, &p);
 * @endcode
 */
rx_err_t rx_fec_decode_soft(rx_fec_decoder_t* dec, const rx_fec_decode_soft_params_t* params)
{
  uint32_t       num_symbols = k_fec_zero;
  const rx_err_t err         = internal_validate_decode_params(dec, params, &num_symbols);
  if (err != k_rx_ok) {
    return err;
  }

  /* Forward pass: initialize path metrics and process symbols */
  internal_viterbi_forward_pass(dec, params->soft_bits, num_symbols);

  /* Calculate data bits and output size */
  const uint32_t data_bits = num_symbols - k_fec_tail_bits;
  if (data_bits == k_fec_zero) {
    return k_rx_err_invalid_size;
  }

  const uint32_t output_bytes = (data_bits + k_fec_msb_bit_position) / k_rx_bits_per_byte;

  /* Traceback: extract decoded bits */
  internal_viterbi_traceback(dec, num_symbols, data_bits, params->output, output_bytes);

  *params->output_len = output_bytes;
  return k_rx_ok;
}

/**
 * @brief Decode FEC-encoded data using hard-decision bits (converts to soft internally)
 *
 * @details
 * Convenience wrapper that converts hard-decision bits (0/1) to soft-decision
 * values and delegates to rx_fec_decode_soft(). Accepts raw bit-packed data
 * (MSB-first) as produced by standard digital logic or RSPI transfers.
 *
 * Algorithm steps:
 * 1. Validate all input pointers and size constraints
 * 2. Convert each hard bit to a soft value via rx_fec_hard_to_soft()
 *    (maps 0 -> negative confidence, 1 -> positive confidence)
 * 3. Build rx_fec_decode_soft_params_t from the converted soft bits
 * 4. Delegate to rx_fec_decode_soft() for full Viterbi decode
 *
 * @param[in] dec    Pointer to initialized FEC decoder (must be initialized via
 *                   rx_fec_decoder_init(); survivors buffer sized for decoded output)
 * @param[in] params Decode parameters:
 *                   - data: bit-packed hard-decision input (MSB-first per byte)
 *                   - data_len: byte count of hard data; must be > 0 and
 *                               <= k_fec_max_input_bytes
 *                   - soft_bits_buffer: caller-supplied scratch buffer for soft
 *                                       conversion; must be >= data_len*8 entries
 *                   - soft_buffer_len: element count of soft_bits_buffer
 *                   - expected_output_len: expected decoded byte count
 *                   - output: output buffer (must be >= expected_output_len bytes)
 *                   - output_len: written with actual decoded byte count on success
 *
 * @return rx_err_t Status code
 * @retval k_rx_ok             Decoding succeeded; output_len updated
 * @retval k_rx_err_invalid_arg dec or required params fields are nullptr;
 *                              or data_len == 0
 * @retval k_rx_err_invalid_state dec is not initialized
 * @retval k_rx_err_invalid_size  data_len > k_fec_max_input_bytes; or
 *                                soft_bits_buffer too small for converted bits;
 *                                or errors propagated from rx_fec_decode_soft()
 *
 * @pre dec must be initialized via rx_fec_decoder_init()
 * @pre params->soft_bits_buffer must be large enough: soft_buffer_len >= data_len * 8
 * @pre params->data_len must be > 0 and <= k_fec_max_input_bytes
 *
 * @post *params->output_len written with actual decoded byte count on k_rx_ok
 * @post params->output contains decoded data bytes on k_rx_ok
 * @post params->soft_bits_buffer populated with converted soft values (side effect)
 *
 * @note Not thread-safe; caller must ensure exclusive access to dec and params
 * @note Hard-decision decoding has ~3 dB lower coding gain than soft-decision
 *
 * @see rx_fec_decode_soft()   Preferred: use when soft values are available
 * @see rx_fec_decoder_init()  Initialize decoder before calling this function
 * @see rx_fec_hard_to_soft()  Conversion function used internally
 * @see rx_fec_encode()        Encoder that produces data this function decodes
 *
 * @since Version 1.0.0
 *
 * @code
 * // Example: hard-decision decode of packed FEC-encoded bytes
 * rx_fec_decoder_t            dec;
 * uint64_t                    survivors[k_fec_max_symbols];
 * rx_soft_bit_t               soft_scratch[k_fec_max_symbols * k_fec_num_outputs];
 * uint8_t                     out_buf[k_fec_max_input_bytes];
 * uint32_t                    out_len = k_fec_zero;
 *
 * (void)rx_fec_decoder_init(&dec, survivors, k_fec_max_symbols);
 *
 * rx_fec_decode_hard_params_t p = {
 *     .data                = received_bytes,
 *     .data_len            = received_len,
 *     .soft_bits_buffer    = soft_scratch,
 *     .soft_buffer_len     = k_fec_max_symbols * k_fec_num_outputs,
 *     .expected_output_len = original_data_len,
 *     .output              = out_buf,
 *     .output_len          = &out_len,
 * };
 * rx_err_t err = rx_fec_decode_hard(&dec, &p);
 * @endcode
 */
rx_err_t rx_fec_decode_hard(rx_fec_decoder_t* dec, const rx_fec_decode_hard_params_t* params)
{
  if (dec == nullptr || params == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (params->data == nullptr || params->output == nullptr || params->output_len == nullptr ||
      params->soft_bits_buffer == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!dec->initialized) {
    return k_rx_err_invalid_state;
  }

  if (params->data_len == k_fec_zero) {
    return k_rx_err_invalid_arg;
  }

  if (params->data_len > k_fec_max_input_bytes) {
    return k_rx_err_invalid_size;
  }

  /* Convert hard bits to soft bits */
  const uint32_t num_bits = (uint32_t)(params->data_len * k_rx_bits_per_byte);

  /* Ensure soft bits buffer is large enough */
  if (num_bits > params->soft_buffer_len) {
    return k_rx_err_invalid_size;
  }

  for (uint32_t i = k_fec_zero; i < num_bits; i++) {
    const uint8_t bit           = internal_get_bit(params->data, i);
    params->soft_bits_buffer[i] = rx_fec_hard_to_soft(bit);
  }

  /* Prepare soft decode parameters */
  const rx_fec_decode_soft_params_t soft_params = {
    .soft_bits           = params->soft_bits_buffer,
    .soft_len            = num_bits,
    .expected_output_len = params->expected_output_len,
    .output              = params->output,
    .output_len          = params->output_len,
  };

  return rx_fec_decode_soft(dec, &soft_params);
}
