/**
 * @file rx_fec.c
 * @brief Forward Error Correction (FEC) Codec Implementation
 *
 * Implements NASA-standard K=7 convolutional encoder and Viterbi decoder.
 * This is a C port of star-gateway/internal/fec/ for bit-exact compatibility.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "rx_fec.h"

#include <string.h>

/* =============================================================================
 * Private Constants
 * =============================================================================
 */

/**
 * @brief FEC implementation constants
 */
typedef enum {
  k_fec_bits_per_byte        = 8,          /**< Bits in a byte */
  k_fec_msb_bit_position     = 7,          /**< MSB position in byte (0-indexed) */
  k_fec_shift_register_bits  = 6,          /**< K-1 shift register size */
  k_fec_correlation_offset   = 32768,      /**< Correlation metric offset */
  k_fec_max_path_metric      = 0x7FFFFFFF, /**< Maximum path metric (INT32_MAX) */
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
  x ^= x >> 4; /* XOR upper nibble with lower nibble */
  x ^= x >> 2; /* XOR bit pairs */
  x ^= x >> 1; /* XOR final pair */
  return x & 1;
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
static void internal_set_output_bit(uint8_t* output, uint32_t bit_idx, uint8_t value)
{
  uint32_t byte_idx = bit_idx / k_fec_bits_per_byte;
  uint32_t bit_pos  = k_fec_msb_bit_position - (bit_idx % k_fec_bits_per_byte); /* MSB first */
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
  uint32_t byte_idx = bit_idx / k_fec_bits_per_byte;
  uint32_t bit_pos  = k_fec_msb_bit_position - (bit_idx % k_fec_bits_per_byte); /* MSB first */
  return (data[byte_idx] >> bit_pos) & 1U;
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
static void internal_encode_bit(uint8_t* state, uint8_t input_bit, uint8_t* out0, uint8_t* out1)
{
  /* Shift in the new bit (input is MSB of the combined state) */
  uint8_t combined = (uint8_t)((input_bit << k_fec_shift_register_bits) | *state);

  /* Calculate output bits using generator polynomials */
  *out0 = internal_parity(combined & k_fec_g1_octal);
  *out1 = internal_parity(combined & k_fec_g2_octal);

  /* Update state (shift right, new bit enters from left) */
  *state = combined >> 1;
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
  for (uint8_t state = 0; state < k_fec_num_states; state++) {
    for (uint8_t input = 0; input < k_fec_num_input_values; input++) {
      uint8_t combined = (uint8_t)(((uint8_t)input << k_fec_shift_register_bits) | (uint8_t)state);
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
static int32_t
internal_branch_metric(rx_soft_bit_t soft0, rx_soft_bit_t soft1, uint8_t exp0, uint8_t exp1)
{
  /* Convert expected bits to soft values: 0 -> -127, 1 -> +127 */
  int32_t exp0_soft = (exp0 != 0) ? k_soft_bit_max : k_soft_bit_min;
  int32_t exp1_soft = (exp1 != 0) ? k_soft_bit_max : k_soft_bit_min;

  /* Correlation = soft0*exp0_soft + soft1*exp1_soft */
  int32_t correlation = ((int32_t)soft0 * exp0_soft) + ((int32_t)soft1 * exp1_soft);

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
 * @param[in]     t Time step index
 */
static void internal_viterbi_process_symbol(rx_fec_decoder_t* dec,
                                            rx_soft_bit_t     soft0,
                                            rx_soft_bit_t     soft1,
                                            uint32_t          t)
{
  /* Reset new path metrics */
  for (uint8_t i = 0; i < k_fec_num_states; i++) {
    dec->new_path_metrics[i] = k_fec_max_path_metric;
  }

  /* Clear survivors for this time step */
  dec->survivors[t] = 0;

  /* For each current state, compute transitions */
  for (uint8_t state = 0; state < k_fec_num_states; state++) {
    if (dec->path_metrics[state] == k_fec_max_path_metric) {
      continue;
    }

    /* Try both input bits (0 and 1) */
    for (uint8_t input = 0; input < k_fec_num_input_values; input++) {
      /* Get expected output bits for this transition */
      uint8_t exp0 = dec->branch_table[state][input][k_fec_output_g1];
      uint8_t exp1 = dec->branch_table[state][input][k_fec_output_g2];

      /* Compute branch metric */
      int32_t branch_metric = internal_branch_metric(soft0, soft1, exp0, exp1);

      /* Compute next state: shift right and insert input as MSB */
      uint8_t next_state = (state >> 1) | ((uint8_t)input << (k_fec_constraint_length - 2));

      /* Compute new path metric */
      int32_t new_metric = dec->path_metrics[state] + branch_metric;

      /* Compare and select */
      if (new_metric < dec->new_path_metrics[next_state]) {
        dec->new_path_metrics[next_state] = new_metric;

        /* Store predecessor's LSB for traceback */
        dec->survivors[t] &= ~(1ULL << (uint8_t)next_state);
        if ((state & 1) == 1) {
          dec->survivors[t] |= (1ULL << (uint8_t)next_state);
        }
      }
    }
  }

  /* Swap path metrics */
  for (uint8_t i = 0; i < k_fec_num_states; i++) {
    dec->path_metrics[i]     = dec->new_path_metrics[i];
    dec->new_path_metrics[i] = k_fec_max_path_metric;
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
                                       uint32_t                num_symbols,
                                       uint32_t                data_bits,
                                       uint8_t*                output,
                                       uint32_t                output_bytes)
{
  /* Clear output buffer */
  memset(output, 0, output_bytes);

  /* Start from state 0 (encoder is flushed to zero by tail bits) */
  uint8_t state = 0;

  /* Traceback: work backwards through the trellis */
  for (uint32_t t = num_symbols; t > 0; t--) {
    uint32_t t_idx = t - 1;

    /* The input bit is the MSB of the current state */
    uint8_t input_bit = (uint8_t)((state >> (k_fec_constraint_length - 2)) & 1);

    /* Store decoded bit if it's a data bit (not tail bit) */
    if (t_idx < data_bits) {
      if (input_bit != 0) {
        uint32_t byte_idx = t_idx / k_fec_bits_per_byte;
        uint32_t bit_pos  = k_fec_msb_bit_position - (t_idx % k_fec_bits_per_byte);
        output[byte_idx] |= (uint8_t)(1U << bit_pos);
      }
    }

    /* Get predecessor's LSB from survivors */
    uint8_t predecessor_lsb = (uint8_t)((dec->survivors[t_idx] >> state) & 1);

    /* Compute predecessor state: shift left and insert the LSB */
    state = ((state << 1) & (k_fec_num_states - 1)) | predecessor_lsb;
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
  return k_rx_ok;
}

rx_err_t rx_fec_encoder_deinit(rx_fec_encoder_t* enc)
{
  if (enc == NULL) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = false;
  return k_rx_ok;
}

uint32_t rx_fec_encoded_len(uint32_t input_len)
{
  if (input_len == 0) {
    return 0;
  }

  /* Output bits = (input bits + tail bits) * 2 */
  uint32_t input_bits        = input_len * k_fec_bits_per_byte;
  uint32_t total_input_bits  = input_bits + k_fec_tail_bits;
  uint32_t total_output_bits = total_input_bits * k_fec_num_outputs;

  /* Round up to full bytes */
  return (total_output_bits + k_fec_msb_bit_position) / k_fec_bits_per_byte;
}

rx_err_t rx_fec_encode(rx_fec_encoder_t* enc,
                       const uint8_t*    input,
                       uint32_t          input_len,
                       uint8_t*          output,
                       uint32_t*         output_len)
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

  /* Calculate output size */
  uint32_t expected_output_len = rx_fec_encoded_len(input_len);

  /* Clear output buffer */
  memset(output, 0, expected_output_len);

  /* Reset encoder state */
  uint8_t  state       = 0;
  uint32_t out_bit_idx = 0;

  /* Encode each input byte, MSB first */
  for (uint32_t byte_idx = 0; byte_idx < input_len; byte_idx++) {
    uint8_t b = input[byte_idx];
    for (int8_t i = k_fec_msb_bit_position; i >= 0; i--) {
      uint8_t input_bit = (b >> i) & 1;
      uint8_t out0, out1;

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

rx_err_t rx_fec_decoder_init(rx_fec_decoder_t* dec, uint64_t* survivors_buf, uint32_t survivors_len)
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

rx_err_t rx_fec_decode_soft(rx_fec_decoder_t*    dec,
                            const rx_soft_bit_t* soft_bits,
                            uint32_t             soft_len,
                            uint32_t             expected_output_len,
                            uint8_t*             output,
                            uint32_t*            output_len)
{
  /* Validate arguments */
  if (dec == NULL || soft_bits == NULL || output == NULL || output_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!dec->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Soft bits must come in pairs (G1, G2) */
  if (soft_len == 0 || (soft_len % k_fec_num_outputs) != 0) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate number of symbols */
  uint32_t num_symbols;
  if (expected_output_len > 0) {
    num_symbols = (uint32_t)((expected_output_len * 8) + k_fec_tail_bits);
  } else {
    num_symbols = (uint32_t)(soft_len / k_fec_num_outputs);
  }

  if (num_symbols < k_fec_tail_bits) {
    return k_rx_err_invalid_size;
  }

  /* Ensure we have enough soft bits */
  if (num_symbols * k_fec_num_outputs > soft_len) {
    num_symbols = (uint32_t)(soft_len / k_fec_num_outputs);
  }

  /* Check survivors buffer is large enough */
  if (num_symbols > dec->survivors_len) {
    return k_rx_err_invalid_size;
  }

  /* Initialize path metrics: state 0 = 0, others = MAX */
  for (uint8_t i = 0; i < k_fec_num_states; i++) {
    dec->path_metrics[i] = k_fec_max_path_metric;
  }
  dec->path_metrics[0] = 0;

  /* Forward pass: process each symbol pair through the trellis */
  for (uint32_t t = 0; t < num_symbols; t++) {
    rx_soft_bit_t soft0 = soft_bits[t * k_fec_num_outputs];
    rx_soft_bit_t soft1 = soft_bits[t * k_fec_num_outputs + 1];
    internal_viterbi_process_symbol(dec, soft0, soft1, t);
  }

  /* Calculate data bits and output size */
  uint32_t data_bits = num_symbols - k_fec_tail_bits;
  if (data_bits == 0) {
    return k_rx_err_invalid_size;
  }

  uint32_t output_bytes = (data_bits + k_fec_msb_bit_position) / k_fec_bits_per_byte;

  /* Traceback: extract decoded bits */
  internal_viterbi_traceback(dec, num_symbols, data_bits, output, output_bytes);

  *output_len = output_bytes;
  return k_rx_ok;
}

rx_err_t rx_fec_decode_hard(rx_fec_decoder_t* dec,
                            const uint8_t*    data,
                            uint32_t          data_len,
                            uint32_t          expected_output_len,
                            uint8_t*          output,
                            uint32_t*         output_len)
{
  if (dec == NULL || data == NULL || output == NULL || output_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!dec->initialized) {
    return k_rx_err_invalid_state;
  }

  if (data_len == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Convert hard bits to soft bits */
  uint32_t num_bits = (uint32_t)(data_len * k_fec_bits_per_byte);

  /*
   * Static buffer to avoid stack overflow on embedded systems.
   * k_fec_max_symbols * k_fec_num_outputs = 8200 * 2 = 16400 soft bits.
   * This buffer is ~16KB which would overflow typical embedded stacks.
   * Static allocation places it in BSS instead.
   *
   * Thread safety: This function is NOT reentrant due to static buffer.
   * For multi-threaded use, caller should provide their own buffer.
   */
  static rx_soft_bit_t soft_bits_buffer[k_fec_max_symbols * k_fec_num_outputs];

  if (num_bits > k_fec_max_symbols * k_fec_num_outputs) {
    return k_rx_err_invalid_size;
  }

  for (uint32_t i = 0; i < num_bits; i++) {
    uint8_t bit         = internal_get_bit(data, i);
    soft_bits_buffer[i] = rx_fec_hard_to_soft(bit);
  }

  return rx_fec_decode_soft(dec,
                            soft_bits_buffer,
                            num_bits,
                            expected_output_len,
                            output,
                            output_len);
}
