/* lib/rx_fec/src/rx_fec.c */

/**
 * @file rx_fec.c
 * @brief Forward Error Correction (FEC) Codec Implementation
 *
 * Implements NASA-standard K=7 convolutional encoder and Viterbi decoder.
 * This is a C port of star-gateway/internal/fec/ for bit-exact compatibility.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_fec.h"

#include <assert.h>
#include <string.h>

#include "rx_bit_constants.h"

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
typedef enum {
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
  /* Pre-condition: Input x is uint8_t (0-255), always valid */

  /* Compute parity using parallel XOR reduction */
  x ^= x >> 4; /* XOR upper nibble with lower nibble */
  x ^= x >> 2; /* XOR bit pairs */
  x ^= x >> 1; /* XOR final pair */

  const uint8_t result = x & k_fec_bit_mask;

  /* Post-condition: Result must be 0 or 1 */
  assert((result == 0) || (result == k_fec_bit_mask));

  return result;
}

/**
 * @brief Set a bit at specified index in output buffer (MSB first)
 *
 * Bit ordering (MSB first, network byte order):
 *   bit_idx=0 → byte 0, bit 7 (MSB)
 *   bit_idx=7 → byte 0, bit 0 (LSB)
 *   bit_idx=8 → byte 1, bit 7 (MSB)
 *
 * @param[out] output Output buffer
 * @param[in]  bit_idx Bit index (0 = MSB of first byte)
 * @param[in]  value Bit value (0 or 1)
 */
static void internal_set_output_bit(uint8_t* output, const uint32_t bit_idx, uint8_t value)
{
  /* Pre-condition 1: output must be valid */
  assert(output != NULL);

  /* Pre-condition 2: bit index must be within maximum symbol range */
  assert(bit_idx < (k_fec_max_symbols * k_rx_bits_per_byte));
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
 *   bit_idx=0 → byte 0, bit 7 (MSB)
 *   bit_idx=7 → byte 0, bit 0 (LSB)
 *   bit_idx=8 → byte 1, bit 7 (MSB)
 *
 * @param[in] data Input buffer
 * @param[in] bit_idx Bit index (0 = MSB of first byte)
 * @return Bit value (0 or 1)
 */
static uint8_t internal_get_bit(const uint8_t* data, uint32_t bit_idx)
{
  /* Pre-condition: data must be valid */
  assert(data != NULL);

  const uint32_t byte_idx = bit_idx / k_rx_bits_per_byte;
  const uint32_t bit_pos  = k_fec_msb_bit_position - (bit_idx % k_rx_bits_per_byte); /* MSB first */

  const uint8_t result = (data[byte_idx] >> bit_pos) & k_fec_bit_mask;

  /* Post-condition: result must be 0 or 1 */
  assert((result == 0) || (result == k_fec_bit_mask));

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
  assert(state != NULL);
  assert(out0 != NULL);
  assert(out1 != NULL);

  /* Pre-condition 2: input_bit must be 0 or 1 */
  assert((input_bit == 0) || (input_bit == k_fec_bit_mask));

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
  /* Loop invariant: state and input values are within valid bounds */

  for (uint8_t state = 0; state < k_fec_num_states; state++) {
    for (uint8_t input = 0; input < k_fec_num_input_values; input++) {
      /* Verify loop invariants */
      assert(state < k_fec_num_states);
      assert(input < k_fec_num_input_values);

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
  assert((exp0 == 0) || (exp0 == k_fec_bit_mask));
  assert((exp1 == 0) || (exp1 == k_fec_bit_mask));

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
  assert(dec != NULL);
  assert(symbol_idx < dec->survivors_len);

  /* Reset new path metrics */
  for (uint8_t i = 0; i < k_fec_num_states; i++) {
    dec->new_path_metrics[i] = INT32_MAX;
  }

  /* Clear survivors for this time step */
  dec->survivors[symbol_idx] = k_fec_zero;

  /* For each current state, compute transitions */
  for (uint8_t state = 0; state < k_fec_num_states; state++) {
    if (dec->path_metrics[state] == INT32_MAX) {
      continue;
    }

    /* Try both input bits (0 and 1) */
    for (uint8_t input = 0; input < k_fec_num_input_values; input++) {
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
  for (uint8_t i = 0; i < k_fec_num_states; i++) {
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
  uint32_t limit;

  /* Pre-conditions */
  assert(dec != NULL);
  assert(soft_bits != NULL);

  /* Initialize path metrics: state 0 = 0, others = MAX */
  for (uint8_t i = 0; i < k_fec_num_states; i++) {
    dec->path_metrics[i] = INT32_MAX;
  }
  dec->path_metrics[0] = 0;

  /* Process each symbol pair through the trellis */
  limit = (num_symbols < k_fec_max_symbols) ? num_symbols : k_fec_max_symbols;
  for (uint32_t t = 0; t < limit; t++) {
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
  uint32_t limit;
  uint8_t  state;

  assert(dec != NULL);
  assert(output != NULL);
  assert(output_bytes > k_fec_zero);
  assert(num_symbols <= dec->survivors_len);

  /* Clear output buffer */
  memset(output, 0, output_bytes);

  if (num_symbols == k_fec_zero) {
    return;
  }

  /* Start from state 0 (encoder is flushed to zero by tail bits) */
  state = 0;

  /* Traceback: work backwards through the trellis */
  limit = (num_symbols < k_fec_max_symbols) ? num_symbols : k_fec_max_symbols;
  for (uint32_t t = limit; t > 0; t--) {
    const uint32_t t_idx = t - 1;

    /* The input bit is the MSB of the current state */
    const uint8_t input_bit = (uint8_t)((state >> k_fec_state_shift_amount) & k_fec_bit_mask);

    /* Store decoded bit if it's a data bit (not tail bit) */
    if (t_idx < data_bits) {
      if (input_bit != 0) {
        const uint32_t byte_idx = t_idx / k_rx_bits_per_byte;
        const uint32_t bit_pos  = k_fec_msb_bit_position - (t_idx % k_rx_bits_per_byte);
        output[byte_idx] |= (uint8_t)(1U << bit_pos);
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

rx_err_t rx_fec_encoder_init(rx_fec_encoder_t* enc)
{
  if (enc == NULL) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = true;
  assert(enc->initialized == true);
  return k_rx_ok;
}

rx_err_t rx_fec_encoder_deinit(rx_fec_encoder_t* enc)
{
  if (enc == NULL) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = false;
  assert(enc->initialized == false);
  return k_rx_ok;
}

uint32_t rx_fec_encoded_len(const uint32_t input_len)
{
  if (input_len == k_fec_zero) {
    return 0;
  }

  if (input_len > k_fec_max_input_bytes) {
    return 0;
  }

  /* Output bits = (input bits + tail bits) * 2 */
  const uint32_t input_bits        = input_len * k_rx_bits_per_byte;
  const uint32_t total_input_bits  = input_bits + k_fec_tail_bits;
  const uint32_t total_output_bits = total_input_bits * k_fec_num_outputs;

  /* Round up to full bytes */
  return (total_output_bits + k_fec_msb_bit_position) / k_rx_bits_per_byte;
}

rx_err_t rx_fec_encode(const rx_fec_encoder_t* enc,
                       const uint8_t*          input,
                       const uint32_t          input_len,
                       uint8_t*                output,
                       uint32_t*               output_len)
{
  if (enc == NULL || input == NULL || output == NULL || output_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!enc->initialized) {
    return k_rx_err_invalid_state;
  }

  if (input_len == 0) {
    return k_rx_err_invalid_arg;
  }

  if (input_len > k_fec_max_input_bytes) {
    return k_rx_err_invalid_size;
  }

  /* Calculate output size */
  const uint32_t expected_output_len = rx_fec_encoded_len(input_len);

  /* Clear output buffer */
  memset(output, 0, expected_output_len);

  /* Reset encoder state */
  uint8_t  state       = 0;
  uint32_t out_bit_idx = 0;

  /* Encode each input byte, MSB first */
  for (uint32_t byte_idx = 0; byte_idx < k_fec_max_input_bytes; byte_idx++) {
    if (byte_idx >= input_len) {
      break;
    }
    const uint8_t b = input[byte_idx];
    for (int8_t i = k_fec_msb_bit_position; i >= 0; i--) {
      const uint8_t input_bit = (b >> i) & k_fec_bit_mask;
      uint8_t       out0, out1;

      internal_encode_bit(&state, input_bit, &out0, &out1);

      /* Pack output bits */
      internal_set_output_bit(output, out_bit_idx++, out0);
      internal_set_output_bit(output, out_bit_idx++, out1);
    }
  }

  /* Append tail bits (zeros) to flush encoder to zero state */
  for (uint8_t i = 0; i < k_fec_tail_bits; i++) {
    uint8_t out0, out1;
    internal_encode_bit(&state, 0, &out0, &out1);
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
 * - dec, params, num_symbols_out must be non-NULL
 * - dec must be initialized
 * - params->soft_bits, output, output_len must be non-NULL
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
  uint32_t num_symbols;

  if (dec == NULL || params == NULL || num_symbols_out == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (params->soft_bits == NULL || params->output == NULL || params->output_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!dec->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Soft bits must come in pairs (G1, G2) */
  if (params->soft_len == 0 || (params->soft_len % k_fec_num_outputs) != 0) {
    return k_rx_err_invalid_arg;
  }

  num_symbols = (params->expected_output_len > 0)
                  ? (uint32_t)((params->expected_output_len * k_rx_bits_per_byte) +
                               k_fec_tail_bits)
                  : 0U;

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

rx_err_t
rx_fec_decoder_init(rx_fec_decoder_t* dec, uint64_t* survivors_buf, const uint32_t survivors_len)
{
  if (dec == NULL || survivors_buf == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Minimum survivors buffer size: at least 1 entry per symbol */
  if (survivors_len == 0) {
    return k_rx_err_invalid_size;
  }

  dec->survivors     = survivors_buf;
  dec->survivors_len = survivors_len;

  /* Initialize branch table */
  internal_init_branch_table(dec->branch_table);

  dec->initialized = true;
  return k_rx_ok;
}

rx_err_t rx_fec_decoder_deinit(rx_fec_decoder_t* dec)
{
  if (dec == NULL) {
    return k_rx_err_invalid_arg;
  }

  dec->survivors     = NULL;
  dec->survivors_len = 0;
  dec->initialized   = false;
  return k_rx_ok;
}

rx_err_t rx_fec_decode_soft(rx_fec_decoder_t* dec, const rx_fec_decode_soft_params_t* params)
{
  uint32_t num_symbols;
  rx_err_t err;
  uint32_t data_bits;
  uint32_t output_bytes;

  err = internal_validate_decode_params(dec, params, &num_symbols);
  if (err != k_rx_ok) {
    return err;
  }

  /* Forward pass: initialize path metrics and process symbols */
  internal_viterbi_forward_pass(dec, params->soft_bits, num_symbols);

  /* Calculate data bits and output size */
  data_bits = num_symbols - k_fec_tail_bits;
  if (data_bits == 0) {
    return k_rx_err_invalid_size;
  }

  output_bytes = (data_bits + k_fec_msb_bit_position) / k_rx_bits_per_byte;

  /* Traceback: extract decoded bits */
  internal_viterbi_traceback(dec, num_symbols, data_bits, params->output, output_bytes);

  *params->output_len = output_bytes;
  return k_rx_ok;
}

rx_err_t rx_fec_decode_hard(rx_fec_decoder_t* dec, const rx_fec_decode_hard_params_t* params)
{
  uint32_t                    num_bits;
  rx_fec_decode_soft_params_t soft_params;

  if (dec == NULL || params == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (params->data == NULL || params->output == NULL || params->output_len == NULL ||
      params->soft_bits_buffer == NULL) {
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
  num_bits = (uint32_t)(params->data_len * k_rx_bits_per_byte);

  /* Ensure soft bits buffer is large enough */
  if (num_bits > params->soft_buffer_len) {
    return k_rx_err_invalid_size;
  }

  for (uint32_t i = 0; i < num_bits; i++) {
    const uint8_t bit           = internal_get_bit(params->data, i);
    params->soft_bits_buffer[i] = rx_fec_hard_to_soft(bit);
  }

  /* Prepare soft decode parameters */
  soft_params.soft_bits           = params->soft_bits_buffer;
  soft_params.soft_len            = num_bits;
  soft_params.expected_output_len = params->expected_output_len;
  soft_params.output              = params->output;
  soft_params.output_len          = params->output_len;

  return rx_fec_decode_soft(dec, &soft_params);
}
