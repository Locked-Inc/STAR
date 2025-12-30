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

/** Maximum path metric value for initialization */
#define MAX_PATH_METRIC (0x7FFFFFFF)

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Calculate parity (XOR of all bits) of a byte
 *
 * @param[in] x Input byte
 * @return 0 if even parity, 1 if odd parity
 */
static uint8_t internal_parity(uint8_t x)
{
  x ^= x >> 4;
  x ^= x >> 2;
  x ^= x >> 1;
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
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_pos  = 7 - (bit_idx % 8); /* MSB first */
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
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_pos  = 7 - (bit_idx % 8); /* MSB first */
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
  uint8_t combined = (uint8_t)((input_bit << 6) | *state);

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
  for (int state = 0; state < k_fec_num_states; state++) {
    for (int input = 0; input < k_fec_num_input_values; input++) {
      uint8_t combined              = (uint8_t)((input << 6) | state);
      branch_table[state][input][0] = internal_parity(combined & k_fec_g1_octal);
      branch_table[state][input][1] = internal_parity(combined & k_fec_g2_octal);
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
  return 32768 - correlation;
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
  for (int i = 0; i < k_fec_num_states; i++) {
    dec->new_path_metrics[i] = MAX_PATH_METRIC;
  }

  /* Clear survivors for this time step */
  dec->survivors[t] = 0;

  /* For each current state, compute transitions */
  for (int state = 0; state < k_fec_num_states; state++) {
    if (dec->path_metrics[state] == MAX_PATH_METRIC) {
      continue;
    }

    /* Try both input bits (0 and 1) */
    for (int input = 0; input < k_fec_num_input_values; input++) {
      /* Get expected output bits for this transition */
      uint8_t exp0 = dec->branch_table[state][input][0];
      uint8_t exp1 = dec->branch_table[state][input][1];

      /* Compute branch metric */
      int32_t branch_metric = internal_branch_metric(soft0, soft1, exp0, exp1);

      /* Compute next state: shift right and insert input as MSB */
      int next_state = (state >> 1) | (input << (k_fec_constraint_length - 2));

      /* Compute new path metric */
      int32_t new_metric = dec->path_metrics[state] + branch_metric;

      /* Compare and select */
      if (new_metric < dec->new_path_metrics[next_state]) {
        dec->new_path_metrics[next_state] = new_metric;

        /* Store predecessor's LSB for traceback */
        dec->survivors[t] &= ~(1ULL << (unsigned)next_state);
        if ((state & 1) == 1) {
          dec->survivors[t] |= (1ULL << (unsigned)next_state);
        }
      }
    }
  }

  /* Swap path metrics */
  for (int i = 0; i < k_fec_num_states; i++) {
    dec->path_metrics[i]     = dec->new_path_metrics[i];
    dec->new_path_metrics[i] = MAX_PATH_METRIC;
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
  int state = 0;

  /* Traceback: work backwards through the trellis */
  for (uint32_t t = num_symbols; t > 0; t--) {
    uint32_t t_idx = t - 1;

    /* The input bit is the MSB of the current state */
    uint8_t input_bit = (uint8_t)((state >> (k_fec_constraint_length - 2)) & 1);

    /* Store decoded bit if it's a data bit (not tail bit) */
    if (t_idx < data_bits) {
      if (input_bit != 0) {
        uint32_t byte_idx = t_idx / 8;
        uint32_t bit_pos  = 7 - (t_idx % 8);
        output[byte_idx] |= (uint8_t)(1U << bit_pos);
      }
    }

    /* Get predecessor's LSB from survivors */
    int predecessor_lsb = (int)((dec->survivors[t_idx] >> state) & 1);

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
    return RX_ERR_INVALID_ARG;
  }

  enc->initialized = 1;
  return RX_OK;
}

rx_err_t rx_fec_encoder_deinit(rx_fec_encoder_t* enc)
{
  if (enc == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  enc->initialized = 0;
  return RX_OK;
}

uint32_t rx_fec_encoded_len(uint32_t input_len)
{
  if (input_len == 0) {
    return 0;
  }

  /* Output bits = (input bits + tail bits) * 2 */
  uint32_t input_bits        = input_len * 8;
  uint32_t total_input_bits  = input_bits + k_fec_tail_bits;
  uint32_t total_output_bits = total_input_bits * 2;

  /* Round up to full bytes */
  return (total_output_bits + 7) / 8;
}

rx_err_t rx_fec_encode(rx_fec_encoder_t* enc,
                       const uint8_t*    input,
                       uint32_t          input_len,
                       uint8_t*          output,
                       uint32_t*         output_len)
{
  if (enc == NULL || input == NULL || output == NULL || output_len == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  if (enc->initialized == 0) {
    return RX_ERR_INVALID_STATE;
  }

  if (input_len == 0) {
    return RX_ERR_INVALID_ARG;
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
    for (int i = 7; i >= 0; i--) {
      uint8_t input_bit = (b >> i) & 1;
      uint8_t out0, out1;

      internal_encode_bit(&state, input_bit, &out0, &out1);

      /* Pack output bits */
      internal_set_output_bit(output, out_bit_idx++, out0);
      internal_set_output_bit(output, out_bit_idx++, out1);
    }
  }

  /* Append tail bits (zeros) to flush encoder to zero state */
  for (int i = 0; i < k_fec_tail_bits; i++) {
    uint8_t out0, out1;
    internal_encode_bit(&state, 0, &out0, &out1);
    internal_set_output_bit(output, out_bit_idx++, out0);
    internal_set_output_bit(output, out_bit_idx++, out1);
  }

  *output_len = expected_output_len;
  return RX_OK;
}

/* =============================================================================
 * Decoder API Implementation
 * =============================================================================
 */

rx_err_t rx_fec_decoder_init(rx_fec_decoder_t* dec, uint64_t* survivors_buf, uint32_t survivors_len)
{
  if (dec == NULL || survivors_buf == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Minimum survivors buffer size: at least 1 entry per symbol */
  if (survivors_len == 0) {
    return RX_ERR_INVALID_SIZE;
  }

  dec->survivors     = survivors_buf;
  dec->survivors_len = survivors_len;

  /* Initialize branch table */
  internal_init_branch_table(dec->branch_table);

  dec->initialized = 1;
  return RX_OK;
}

rx_err_t rx_fec_decoder_deinit(rx_fec_decoder_t* dec)
{
  if (dec == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  dec->survivors     = NULL;
  dec->survivors_len = 0;
  dec->initialized   = 0;
  return RX_OK;
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
    return RX_ERR_INVALID_ARG;
  }

  if (dec->initialized == 0) {
    return RX_ERR_INVALID_STATE;
  }

  /* Soft bits must come in pairs (G1, G2) */
  if (soft_len == 0 || (soft_len % k_fec_num_outputs) != 0) {
    return RX_ERR_INVALID_ARG;
  }

  /* Calculate number of symbols */
  uint32_t num_symbols;
  if (expected_output_len > 0) {
    num_symbols = (uint32_t)((expected_output_len * 8) + k_fec_tail_bits);
  } else {
    num_symbols = (uint32_t)(soft_len / k_fec_num_outputs);
  }

  if (num_symbols < k_fec_tail_bits) {
    return RX_ERR_INVALID_SIZE;
  }

  /* Ensure we have enough soft bits */
  if (num_symbols * k_fec_num_outputs > soft_len) {
    num_symbols = (uint32_t)(soft_len / k_fec_num_outputs);
  }

  /* Check survivors buffer is large enough */
  if (num_symbols > dec->survivors_len) {
    return RX_ERR_INVALID_SIZE;
  }

  /* Initialize path metrics: state 0 = 0, others = MAX */
  for (int i = 0; i < k_fec_num_states; i++) {
    dec->path_metrics[i] = MAX_PATH_METRIC;
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
    return RX_ERR_INVALID_SIZE;
  }

  uint32_t output_bytes = (data_bits + 7) / 8;

  /* Traceback: extract decoded bits */
  internal_viterbi_traceback(dec, num_symbols, data_bits, output, output_bytes);

  *output_len = output_bytes;
  return RX_OK;
}

rx_err_t rx_fec_decode_hard(rx_fec_decoder_t* dec,
                            const uint8_t*    data,
                            uint32_t          data_len,
                            uint32_t          expected_output_len,
                            uint8_t*          output,
                            uint32_t*         output_len)
{
  if (dec == NULL || data == NULL || output == NULL || output_len == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  if (dec->initialized == 0) {
    return RX_ERR_INVALID_STATE;
  }

  if (data_len == 0) {
    return RX_ERR_INVALID_ARG;
  }

  /* Convert hard bits to soft bits */
  uint32_t num_bits = (uint32_t)(data_len * 8);

  /*
   * Static buffer to avoid stack overflow on embedded systems.
   * RX_FEC_MAX_SYMBOLS * k_fec_num_outputs = 8200 * 2 = 16400 soft bits.
   * This buffer is ~16KB which would overflow typical embedded stacks.
   * Static allocation places it in BSS instead.
   *
   * Thread safety: This function is NOT reentrant due to static buffer.
   * For multi-threaded use, caller should provide their own buffer.
   */
  static rx_soft_bit_t soft_bits_buffer[RX_FEC_MAX_SYMBOLS * k_fec_num_outputs];

  if (num_bits > RX_FEC_MAX_SYMBOLS * k_fec_num_outputs) {
    return RX_ERR_INVALID_SIZE;
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
