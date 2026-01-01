/* lib/rx_harq/src/rx_harq.c */

/**
 * @file rx_harq.c
 * @brief Hybrid Automatic Repeat Request (HARQ) Protocol Implementation
 * @details
 * Implements Chase Combining HARQ Type I for reliable communication.
 * This is a C port of star-gateway/internal/harq/ and fec/combiner.go
 * for bit-exact compatibility.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_harq.h"

#include <string.h>

/* =============================================================================
 * Chase Combiner Implementation
 * =============================================================================
 */

rx_err_t rx_chase_combiner_init(rx_chase_combiner_t* combiner, uint8_t max_combines)
{
  if (combiner == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Clear accumulator */
  memset(combiner->accumulated, 0, sizeof(combiner->accumulated));

  combiner->expected_len = 0;
  combiner->count        = 0;
  combiner->max_combines = (max_combines > 0) ? max_combines : k_harq_default_combines;
  combiner->initialized  = 1;

  return k_rx_ok;
}

rx_err_t rx_chase_combiner_deinit(rx_chase_combiner_t* combiner)
{
  if (combiner == NULL) {
    return k_rx_err_invalid_arg;
  }

  combiner->initialized = 0;
  return k_rx_ok;
}

rx_err_t
rx_chase_combiner_add(rx_chase_combiner_t* combiner, const rx_soft_bit_t* soft_bits, uint32_t len)
{
  if (combiner == NULL || soft_bits == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (combiner->initialized == 0) {
    return k_rx_err_invalid_state;
  }

  if (len == 0) {
    return k_rx_err_invalid_arg;
  }

  if (len > k_harq_soft_buffer_size) {
    return k_rx_err_invalid_size;
  }

  if (combiner->count >= combiner->max_combines) {
    return k_rx_err_busy;
  }

  /* First transmission sets the expected length */
  if (combiner->count == 0) {
    combiner->expected_len = len;
    /* Clear accumulator for new frame */
    memset(combiner->accumulated, 0, len * sizeof(int16_t));
  } else if (len != combiner->expected_len) {
    return k_rx_err_invalid_size;
  }

  /* Element-wise addition (accumulate) */
  for (uint32_t i = 0; i < len; i++) {
    combiner->accumulated[i] += (int16_t)soft_bits[i];
  }

  combiner->count++;
  return k_rx_ok;
}

rx_err_t
rx_chase_combiner_combined(rx_chase_combiner_t* combiner, rx_soft_bit_t* output, uint32_t* len)
{
  if (combiner == NULL || output == NULL || len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (combiner->initialized == 0) {
    return k_rx_err_invalid_state;
  }

  if (combiner->count == 0 || combiner->expected_len == 0) {
    return k_rx_err_invalid_state;
  }

  /* Clamp accumulated values to SoftBit range [-127, +127] */
  for (uint32_t i = 0; i < combiner->expected_len; i++) {
    int16_t acc = combiner->accumulated[i];
    if (acc > k_soft_bit_max) {
      output[i] = k_soft_bit_max;
    } else if (acc < k_soft_bit_min) {
      output[i] = k_soft_bit_min;
    } else {
      output[i] = (rx_soft_bit_t)acc;
    }
  }

  *len = combiner->expected_len;
  return k_rx_ok;
}

void rx_chase_combiner_reset(rx_chase_combiner_t* combiner)
{
  if (combiner == NULL) {
    return;
  }

  /* Clear the entire accumulator to prevent stale data */
  memset(combiner->accumulated, 0, sizeof(combiner->accumulated));
  combiner->count        = 0;
  combiner->expected_len = 0;
}

bool rx_chase_combiner_can_add(const rx_chase_combiner_t* combiner)
{
  if (combiner == NULL || combiner->initialized == 0) {
    return false;
  }
  return combiner->count < combiner->max_combines;
}

uint8_t rx_chase_combiner_count(const rx_chase_combiner_t* combiner)
{
  if (combiner == NULL) {
    return 0;
  }
  return combiner->count;
}

/* =============================================================================
 * HARQ Handle Implementation
 * =============================================================================
 */

rx_err_t rx_harq_init(rx_harq_handle_t* harq, const rx_harq_config_t* config)
{
  if (harq == NULL) {
    return k_rx_err_invalid_arg;
  }

  rx_err_t err;

  /* Initialize state */
  harq->state       = k_harq_state_idle;
  harq->tx_sequence = 0;
  harq->rx_sequence = 0;
  harq->retry_count = 0;

  /* Apply configuration */
  if (config != NULL) {
    harq->max_retries = (config->max_retries > 0) ? config->max_retries : k_harq_default_retries;
    harq->fec_enabled = config->fec_enabled;
  } else {
    harq->max_retries = k_harq_default_retries;
    harq->fec_enabled = 1; /* FEC enabled by default */
  }

  /* Initialize Chase Combiner */
  uint8_t max_combines =
    (config != NULL && config->max_combines > 0) ? config->max_combines : k_harq_default_combines;
  err = rx_chase_combiner_init(&harq->combiner, max_combines);
  if (err != k_rx_ok) {
    return err;
  }

  /* Initialize FEC encoder if enabled */
  if (harq->fec_enabled) {
    err = rx_fec_encoder_init(&harq->encoder);
    if (err != k_rx_ok) {
      return err;
    }

    /* Initialize FEC decoder with survivors buffer */
    err = rx_fec_decoder_init(&harq->decoder, harq->decoder_survivors, k_harq_soft_buffer_size / 2);
    if (err != k_rx_ok) {
      rx_fec_encoder_deinit(&harq->encoder);
      return err;
    }
  }

  harq->initialized = 1;
  return k_rx_ok;
}

rx_err_t rx_harq_deinit(rx_harq_handle_t* harq)
{
  if (harq == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (harq->fec_enabled) {
    rx_fec_encoder_deinit(&harq->encoder);
    rx_fec_decoder_deinit(&harq->decoder);
  }

  rx_chase_combiner_deinit(&harq->combiner);

  harq->initialized = 0;
  return k_rx_ok;
}

rx_harq_state_t rx_harq_get_state(const rx_harq_handle_t* harq)
{
  if (harq == NULL) {
    return k_harq_state_error;
  }
  return harq->state;
}

void rx_harq_reset(rx_harq_handle_t* harq)
{
  if (harq == NULL) {
    return;
  }

  harq->state       = k_harq_state_idle;
  harq->retry_count = 0;
  rx_chase_combiner_reset(&harq->combiner);
}

rx_err_t rx_harq_encode(rx_harq_handle_t* harq,
                        const uint8_t*    payload,
                        uint32_t          payload_len,
                        uint8_t*          output,
                        uint32_t*         output_len)
{
  if (harq == NULL || payload == NULL || output == NULL || output_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (harq->initialized == 0) {
    return k_rx_err_invalid_state;
  }

  if (payload_len == 0 || payload_len > k_harq_max_payload) {
    return k_rx_err_invalid_size;
  }

  /* If FEC is disabled, just copy payload */
  if (harq->fec_enabled == 0) {
    memcpy(output, payload, payload_len);
    *output_len = payload_len;
    return k_rx_ok;
  }

  /* FEC encode */
  return rx_fec_encode(&harq->encoder, payload, payload_len, output, output_len);
}

/* =============================================================================
 * Decode Helper Functions
 * =============================================================================
 */

/** @brief Bit position constants for soft-to-hard conversion */
typedef enum {
  k_bits_per_byte = 8,
  k_msb_position  = 7,
} bit_constants_t;

/**
 * @brief Convert combined soft bits to hard bits (non-FEC path)
 *
 * @param[in]  soft_bits   Combined soft bits
 * @param[in]  soft_len    Number of soft bits
 * @param[in]  max_out_len Maximum output length (0 = unlimited)
 * @param[out] output      Output hard bits (packed bytes)
 * @param[out] output_len  Actual output length
 */
static void internal_soft_to_hard(const rx_soft_bit_t* soft_bits,
                                  uint32_t             soft_len,
                                  uint32_t             max_out_len,
                                  uint8_t*             output,
                                  uint32_t*            output_len)
{
  uint32_t out_bytes = (soft_len + (k_bits_per_byte - 1)) / k_bits_per_byte;
  if (out_bytes > max_out_len && max_out_len > 0) {
    out_bytes = max_out_len;
  }
  memset(output, 0, out_bytes);

  for (uint32_t i = 0; i < soft_len && (i / k_bits_per_byte) < out_bytes; i++) {
    if (soft_bits[i] >= 0) {
      uint32_t byte_idx = i / k_bits_per_byte;
      uint32_t bit_pos  = k_msb_position - (i % k_bits_per_byte);
      output[byte_idx] |= (uint8_t)(1U << bit_pos);
    }
  }

  *output_len = out_bytes;
}

/**
 * @brief Handle FEC decode result and update HARQ state
 *
 * @param[in,out] harq   HARQ handle
 * @param[in]     result Decode result from FEC decoder
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t internal_handle_fec_result(rx_harq_handle_t* harq, rx_err_t result)
{
  harq->retry_count++;

  if (result == k_rx_ok) {
    rx_chase_combiner_reset(&harq->combiner);
    harq->state = k_harq_state_idle;
    return k_rx_ok;
  }

  /* Decode failed - check if we can retry */
  if (rx_chase_combiner_can_add(&harq->combiner) && harq->retry_count < harq->max_retries) {
    harq->state = k_harq_state_combining;
    return k_rx_err_protocol_error; /* Need more retransmissions */
  }

  /* Max retries reached */
  rx_chase_combiner_reset(&harq->combiner);
  harq->state = k_harq_state_error;
  return result;
}

/* =============================================================================
 * Decode API
 * =============================================================================
 */

rx_err_t rx_harq_decode(rx_harq_handle_t*    harq,
                        const rx_soft_bit_t* soft_bits,
                        uint32_t             soft_len,
                        uint32_t             expected_output_len,
                        uint8_t*             output,
                        uint32_t*            output_len)
{
  if (harq == NULL || soft_bits == NULL || output == NULL || output_len == NULL) {
    return k_rx_err_invalid_arg;
  }
  if (harq->initialized == 0) {
    return k_rx_err_invalid_state;
  }
  if (soft_len == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Add soft bits to combiner */
  rx_err_t err = rx_chase_combiner_add(&harq->combiner, soft_bits, soft_len);
  if (err != k_rx_ok && err != k_rx_err_busy) {
    return err;
  }

  /* Get combined soft bits into handle's buffer (thread-safe) */
  uint32_t combined_len;

  err = rx_chase_combiner_combined(&harq->combiner, harq->decode_buffer, &combined_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Non-FEC path: direct soft-to-hard conversion */
  if (harq->fec_enabled == 0) {
    internal_soft_to_hard(harq->decode_buffer,
                          combined_len,
                          expected_output_len,
                          output,
                          output_len);
    harq->retry_count++;
    rx_chase_combiner_reset(&harq->combiner);
    harq->state = k_harq_state_idle;
    return k_rx_ok;
  }

  /* FEC path: decode combined soft bits */
  err = rx_fec_decode_soft(&harq->decoder,
                           harq->decode_buffer,
                           combined_len,
                           expected_output_len,
                           output,
                           output_len);

  return internal_handle_fec_result(harq, err);
}

uint8_t rx_harq_get_retry_count(const rx_harq_handle_t* harq)
{
  if (harq == NULL) {
    return 0;
  }
  return harq->retry_count;
}

bool rx_harq_can_retry(const rx_harq_handle_t* harq)
{
  if (harq == NULL || harq->initialized == 0) {
    return false;
  }
  return (harq->retry_count < harq->max_retries) && rx_chase_combiner_can_add(&harq->combiner);
}
