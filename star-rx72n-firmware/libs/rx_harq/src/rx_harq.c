/**
 * @file rx_harq.c
 * @brief Hybrid Automatic Repeat Request (HARQ) Protocol Implementation
 *
 * @details
 * Implements Chase Combining HARQ Type I combining FEC with soft bit accumulation
 * across retransmissions. This is a C port of star-gateway/internal/harq/ and
 * fec/combiner.go for bit-exact compatibility between RX72N and RPi5.
 *
 * ## Implementation Architecture
 *
 * ### Three-Layer Design
 *
 * 1. **Chase Combiner** (rx_chase_combiner_t)
 *    - Accumulates soft bits element-wise across transmissions
 *    - Uses int16 accumulators (prevents overflow with <=255 combines)
 *    - Clamps output to [-127, +127] range before decoding
 *
 * 2. **FEC Codec** (rx_fec_encoder_t, rx_fec_decoder_t)
 *    - Optional layer for error correction
 *    - K=7, rate 1/2 convolutional code
 *    - Integrated seamlessly with combiner
 *
 * 3. **HARQ State Machine** (rx_harq_handle_t)
 *    - Manages retransmission protocol
 *    - Tracks retry count and sequence numbers
 *    - Coordinates encoder, decoder, and combiner
 *
 * ## Chase Combining Algorithm
 *
 * For @f$ n @f$ transmissions, accumulate soft bits:
 *
 * @f[
 *   \text{acc}[i] = \sum_{k=1}^{n} s_k[i]
 * @f]
 *
 * Then clamp to valid range:
 *
 * @f[
 *   s_{\text{combined}}[i] = \begin{cases}
 *     +127 & \text{if } \text{acc}[i] > 127 \\
 *     -127 & \text{if } \text{acc}[i] < -127 \\
 *     \text{acc}[i] & \text{otherwise}
 *   \end{cases}
 * @f]
 *
 * ## Overflow Prevention
 *
 * Accumulators use int16 (range +/-32768):
 * - Max combines: 255
 * - Max soft bit: +/-127
 * - Max accumulator value: +/-(255 x 127) = +/-32385
 * - **Safe**: 32385 < 32768 (no overflow with default max_combines=3)
 *
 * ## Protocol Flow
 *
 * @dot
 * digraph harq_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   tx [label="Transmit\nFEC-encoded data"];
 *   rx [label="Receive\nnoisy soft bits"];
 *   add [label="Add to\nCombiner"];
 *   decode [label="Decode\ncombined bits"];
 *   success [label="Success\nSend ACK", fillcolor=green];
 *   retry [label="Failed\nretry_count++"];
 *   max [label="Max retries?\nSend NACK", shape=diamond];
 *   error [label="Error\nGive up", fillcolor=red];
 *
 *   tx -> rx -> add -> decode;
 *   decode -> success [label="OK"];
 *   decode -> max [label="FAIL"];
 *   max -> retry [label="No"];
 *   retry -> tx;
 *   max -> error [label="Yes"];
 * }
 * @enddot
 *
 * @par Module Dependencies
 * - [rx_harq.h](rx_harq_8h.html): Public API
 * - [rx_fec.h](rx_fec_8h.html): FEC codec
 * - [rx_bit_constants.h](rx_bit_constants_8h.html): Bit manipulation
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No recursion, goto, setjmp/longjmp
 * - **Rule 2**: [OK] Loops bounded by k_harq_soft_buffer_size
 * - **Rule 3**: [OK] Zero dynamic allocation
 * - **Rule 4**: [OK] Functions <= 60 lines
 * - **Rule 5**: [OK] Extensive validation checks
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 * @version 1.0.0
 */

#include "rx_harq.h"

#include "rx_bit_constants.h"
#include "rx_check.h"

/** @brief HARQ FEC size constants */
typedef enum : uint16_t {
  k_harq_fec_rate_multiplier = 2, /**< Rate 1/2: encoded length multiplier */
  k_harq_tail_bytes          = 2, /**< Tail overhead in bytes */
} harq_fec_constants_t;

/**
 * @enum harq_cfg_sentinel_t
 * @brief HARQ config validation sentinels -- zero values for "not configured, use default"
 * @details
 * Used to detect unconfigured (zero) values for max_combines and max_retries
 * fields in HARQ configuration, to fall back to project defaults.
 *
 * @invariant k_harq_zero_combines and k_harq_zero_retries equal zero, signifying
 * "use default config" -- these sentinels must never be changed to non-zero values.
 *
 * @code
 * // Checking sentinels when validating config fields:
 * const rx_harq_config_t config = { .max_combines = 0, .max_retries = 0 };
 * // max_combines == k_harq_zero_combines -> use k_harq_default_combines
 * // max_retries  == k_harq_zero_retries  -> use k_harq_default_retries
 * uint8_t combines = (config.max_combines > k_harq_zero_combines)
 *                        ? config.max_combines : k_harq_default_combines;
 * uint8_t retries  = (config.max_retries  > k_harq_zero_retries)
 *                        ? config.max_retries  : k_harq_default_retries;
 * @endcode
 *
 * @see rx_harq_config_t HARQ configuration structure containing max_combines/max_retries
 * @see rx_harq_init() Applies these sentinels during initialization
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_harq_zero_combines = 0, /**< Sentinel: no max_combines configured -> fall back to default */
  k_harq_zero_retries  = 0, /**< Sentinel: no max_retries configured -> fall back to default */
} harq_cfg_sentinel_t;

typedef enum : uint8_t {
  k_harq_false = 0U,
  k_harq_true  = 1U,
} harq_bool_t;

/* =============================================================================
 * Chase Combiner Implementation
 * =============================================================================
 */

rx_err_t rx_chase_combiner_init(rx_chase_combiner_t* combiner, const uint8_t max_combines)
{
  if (combiner == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Clear accumulator */
  for (uint32_t i = 0U; i < k_harq_soft_buffer_size; i++) {
    combiner->accumulated[i] = 0;
  }

  combiner->expected_len = 0;
  combiner->count        = 0;
  if (max_combines > k_harq_zero_combines) {
    combiner->max_combines = max_combines;
  } else {
    combiner->max_combines = k_harq_default_combines;
  }
  combiner->initialized = k_harq_true;

  return k_rx_ok;
}

rx_err_t rx_chase_combiner_deinit(rx_chase_combiner_t* combiner)
{
  if (combiner == nullptr) {
    return k_rx_err_invalid_arg;
  }

  combiner->initialized = k_harq_false;
  return k_rx_ok;
}

rx_err_t
rx_chase_combiner_add(rx_chase_combiner_t* combiner, const rx_soft_bit_t* soft_bits, uint32_t len)
{
  if (combiner == nullptr || soft_bits == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (combiner->initialized == k_harq_false) {
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
    for (uint32_t i = 0U; i < len; i++) {
      combiner->accumulated[i] = 0;
    }
  } else if (len != combiner->expected_len) {
    return k_rx_err_invalid_size;
  }

  /*
   * Element-wise addition (accumulate).
   *
   * Loop bound verification (NASA Rule 2):
   * - len is validated: len > 0 (line 62-64) AND len <= k_harq_soft_buffer_size (line 66-68)
   * - Maximum iterations: k_harq_soft_buffer_size (16400)
   * - Array bounds: accumulated[i] and soft_bits[i] are safe for i < len
   */
  for (uint32_t i = 0; i < len; i++) {
    combiner->accumulated[i] = (int16_t)(combiner->accumulated[i] + (int16_t)soft_bits[i]);
  }

  combiner->count++;
  return k_rx_ok;
}

rx_err_t rx_chase_combiner_combined(const rx_chase_combiner_t* combiner,
                                    rx_soft_bit_t*             output,
                                    uint32_t*                  len)
{
  if (combiner == nullptr || output == nullptr || len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (combiner->initialized == k_harq_false) {
    return k_rx_err_invalid_state;
  }

  const bool count_zero = (bool)(combiner->count == 0);
  const bool len_zero   = (bool)(combiner->expected_len == 0);
  if ((bool)((int)count_zero | (int)len_zero)) {
    return k_rx_err_invalid_state;
  }

  /*
   * Clamp accumulated values to SoftBit range [-127, +127].
   *
   * Loop bound verification (NASA Rule 2):
   * - expected_len is set only via rx_chase_combiner_add() with validated len
   * - expected_len > 0 (validated at line 103-105)
   * - expected_len <= k_harq_soft_buffer_size (validated when set in add)
   * - Maximum iterations: k_harq_soft_buffer_size (16400)
   * - Array bounds: accumulated[i] and output[i] are safe for i < expected_len
   */
  for (uint32_t i = 0; i < combiner->expected_len; i++) {
    const int16_t acc = combiner->accumulated[i];
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

rx_err_t rx_chase_combiner_reset(rx_chase_combiner_t* combiner)
{
  if (combiner == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (combiner->initialized == k_harq_false) {
    return k_rx_err_invalid_state;
  }

  /* Clear the entire accumulator to prevent stale data */
  for (uint32_t i = 0U; i < k_harq_soft_buffer_size; i++) {
    combiner->accumulated[i] = 0;
  }
  combiner->count        = 0;
  combiner->expected_len = 0;

  return k_rx_ok;
}

/**
 * @brief Check if combiner can accept more transmissions
 *
 *
 */
bool rx_chase_combiner_can_add(const rx_chase_combiner_t* combiner)
{
  if (combiner == nullptr) {
    return false;
  }
  if (combiner->initialized == k_harq_false) {
    return false;
  }
  return (bool)(combiner->count < combiner->max_combines);
}

/**
 * @brief Get number of transmissions combined so far
 *
 *
 */
uint8_t rx_chase_combiner_count(const rx_chase_combiner_t* combiner)
{
  if (combiner == nullptr) {
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
  if (harq == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize state */
  harq->state       = k_harq_state_idle;
  harq->tx_sequence = 0;
  harq->rx_sequence = 0;
  harq->retry_count = 0;

  /* Apply configuration */
  if (config != nullptr) {
    if (config->max_retries > k_harq_zero_retries) {
      harq->max_retries = config->max_retries;
    } else {
      harq->max_retries = k_harq_default_retries;
    }
    harq->fec_enabled = config->fec_enabled;
  } else {
    harq->max_retries = k_harq_default_retries;
    harq->fec_enabled = k_harq_true; /* FEC enabled by default */
  }

  /* Initialize Chase Combiner */
  const bool has_combines =
    (bool)(config != nullptr && (int)(config->max_combines > k_harq_zero_combines));
  const uint8_t max_combines = (int)has_combines ? config->max_combines : k_harq_default_combines;
  (void)(rx_chase_combiner_init(&harq->combiner, max_combines));
  /* Post-condition: combiner_init only fails on nullptr; &harq->combiner is never null */

  /* Initialize FEC encoder if enabled */
  if (harq->fec_enabled == k_harq_true) {
    (void)(rx_fec_encoder_init(&harq->encoder));
    /* Post-condition: encoder_init only fails on nullptr; &harq->encoder is never null */

    /* Initialize FEC decoder with survivors buffer */
    (void)(rx_fec_decoder_init(&harq->decoder,
                               harq->decoder_survivors,
                               k_harq_survivors_buffer_size));
    /* Post-condition: decoder_init only fails on nullptr or zero size; both are impossible here */
  }

  harq->initialized = k_harq_true;
  return k_rx_ok;
}

/**
 * @brief Deinitialize HARQ handle and release resources
 *
 *
 */
rx_err_t rx_harq_deinit(rx_harq_handle_t* harq)
{
  if (harq == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (harq->fec_enabled == k_harq_true) {
    (void)(rx_fec_encoder_deinit(&harq->encoder));
    /* Post-condition: deinit only fails on nullptr; &harq->encoder is never null */
    (void)(rx_fec_decoder_deinit(&harq->decoder));
    /* Post-condition: deinit only fails on nullptr; &harq->decoder is never null */
  }

  (void)(rx_chase_combiner_deinit(&harq->combiner));
  /* Post-condition: deinit only fails on nullptr; &harq->combiner is never null */

  harq->initialized = k_harq_false;
  return k_rx_ok;
}

/**
 * @brief Get current HARQ state
 *
 *
 */
rx_harq_state_t rx_harq_get_state(const rx_harq_handle_t* harq)
{
  if (harq == nullptr) {
    return k_harq_state_error;
  }
  return harq->state;
}

/**
 * @brief Reset HARQ state for new transmission
 *
 *
 */
rx_err_t rx_harq_reset(rx_harq_handle_t* harq)
{
  if (harq == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (harq->initialized == k_harq_false) {
    return k_rx_err_invalid_state;
  }

  harq->state       = k_harq_state_idle;
  harq->retry_count = 0;

  (void)(rx_chase_combiner_reset(&harq->combiner));
  /* Post-condition: combiner_reset only fails on nullptr or uninitialized;
   * &harq->combiner is always non-null and initialized after rx_harq_init() */

  return k_rx_ok;
}

rx_err_t rx_harq_encode(const rx_harq_handle_t* harq,
                        const uint8_t*          payload,
                        const uint32_t          payload_len,
                        uint8_t*                output,
                        const uint32_t          output_size,
                        uint32_t*               output_len)
{
  if (harq == nullptr || payload == nullptr || output == nullptr || output_len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (harq->initialized == k_harq_false) {
    return k_rx_err_invalid_state;
  }

  if (payload_len == 0 || payload_len > k_harq_max_payload) {
    return k_rx_err_invalid_size;
  }

  /* If FEC is disabled, just copy payload */
  if (harq->fec_enabled == k_harq_false) {
    /* Validate output buffer can hold payload */
    if (output_size < payload_len) {
      return k_rx_err_invalid_size;
    }
    for (uint32_t i = 0U; i < payload_len; i++) {
      output[i] = payload[i];
    }
    *output_len = payload_len;
    return k_rx_ok;
  }

  /*
   * FEC encode: Rate 1/2 convolutional code doubles output size.
   * Required buffer: (payload_len * 8 + 6) * 2 bits = (payload_len * 8 + 6) / 4 bytes
   * Simplified worst case: payload_len * 2 + 2 bytes
   */
  const uint32_t min_output_size = (payload_len * k_harq_fec_rate_multiplier) + k_harq_tail_bytes;
  if (output_size < min_output_size) {
    return k_rx_err_invalid_size;
  }

  /* FEC encode */
  return rx_fec_encode(&harq->encoder, payload, payload_len, output, output_len);
}

/* =============================================================================
 * Decode Helper Functions
 * =============================================================================
 */

/**
 * @brief Bit position constants for soft-to-hard conversion
 */
typedef enum : uint8_t {
  k_msb_position         = k_rx_bits_per_byte - 1U, /**< Most significant bit position */
  k_rounding_adjustment  = k_rx_bits_per_byte - 1U, /**< Ceiling division: (n + 7) / 8 */
  k_soft_bit_zero_thresh = 0, /**< Threshold for soft-to-hard: >= 0 is bit 1 */
} bit_constants_t;

/**
 * @brief Convert combined soft bits to hard bits (non-FEC path)
 *
 *
 */
static rx_err_t internal_soft_to_hard(const rx_soft_bit_t* soft_bits,
                                      const uint32_t       soft_bit_count,
                                      const uint32_t       max_output_bytes,
                                      uint8_t*             output,
                                      uint32_t*            output_len)
{
  /* Pre-conditions: pointers are always valid from rx_harq_decode (harq->decode_buffer,
   * caller-validated output, caller-validated output_len) */
  if (max_output_bytes == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate output bytes with ceiling division */
  uint32_t out_bytes = (soft_bit_count + k_rounding_adjustment) / k_rx_bits_per_byte;
  if (out_bytes > max_output_bytes) {
    out_bytes = max_output_bytes;
  }
  for (uint32_t i = 0U; i < out_bytes; i++) {
    output[i] = 0U;
  }

  /*
   * Loop bound: i < soft_len is validated by prior check that soft_bits != nullptr.
   * Loop terminates when i >= soft_len OR when byte index exceeds out_bytes.
   * Maximum iterations: min(soft_len, out_bytes * k_rx_bits_per_byte).
   */
  for (uint32_t i = 0; i < soft_bit_count && (i / k_rx_bits_per_byte) < out_bytes; i++) {
    if (soft_bits[i] >= k_soft_bit_zero_thresh) {
      const uint32_t byte_idx = i / k_rx_bits_per_byte;
      const uint32_t bit_pos  = k_msb_position - (i % k_rx_bits_per_byte);
      output[byte_idx] |= (uint8_t)(1U << bit_pos);
    }
  }

  *output_len = out_bytes;
  return k_rx_ok;
}

/**
 * @brief Handle FEC decode result and update HARQ state
 *
 *
 */
static rx_err_t internal_handle_fec_result(rx_harq_handle_t* harq, const rx_err_t result)
{
  /* Pre-condition: only called from rx_harq_decode where harq is already validated */

  harq->retry_count++;

  if (result == k_rx_ok) {
    /* Ignore return - combiner is already validated via harq->initialized */
    (void)rx_chase_combiner_reset(&harq->combiner);
    harq->state = k_harq_state_idle;
    return k_rx_ok;
  }

  /* Decode failed - check if we can retry */
  const bool can_add_more   = rx_chase_combiner_can_add(&harq->combiner);
  const bool retries_remain = (bool)(harq->retry_count < harq->max_retries);
  if ((bool)((int)can_add_more & (int)retries_remain)) {
    harq->state = k_harq_state_combining;
    return k_rx_err_protocol_error; /* Need more retransmissions */
  }

  /* Max retries reached */
  (void)rx_chase_combiner_reset(&harq->combiner);
  harq->state = k_harq_state_error;
  return result;
}

/* =============================================================================
 * Decode API
 * =============================================================================
 */

rx_err_t rx_harq_decode(rx_harq_handle_t*              harq,
                        const rx_harq_decode_params_t* params,
                        uint8_t*                       output,
                        uint32_t*                      output_len)
{
  const bool harq_null       = (bool)(harq == nullptr);
  const bool params_null     = (bool)(params == nullptr);
  const bool output_null     = (bool)(output == nullptr);
  const bool output_len_null = (bool)(output_len == nullptr);
  if ((bool)((int)harq_null | (int)params_null | (int)output_null | (int)output_len_null)) {
    return k_rx_err_invalid_arg;
  }
  if (harq->initialized == k_harq_false) {
    return k_rx_err_invalid_state;
  }
  if (params->soft_bits == nullptr || params->soft_len == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Add soft bits to combiner */
  rx_err_t   err      = rx_chase_combiner_add(&harq->combiner, params->soft_bits, params->soft_len);
  const bool not_ok   = (bool)(err != k_rx_ok);
  const bool not_busy = (bool)(err != k_rx_err_busy);
  if ((bool)((int)not_ok & (int)not_busy)) {
    return err;
  }

  /* Get combined soft bits into handle's buffer (thread-safe) */
  uint32_t combined_len = 0U;
  (void)rx_chase_combiner_combined(&harq->combiner, harq->decode_buffer, &combined_len);
  /* Post-condition: combiner_combined only fails if count==0 or expected_len==0,
   * but combiner_add just succeeded (count>=1, expected_len>0) */

  /* Non-FEC path: direct soft-to-hard conversion */
  if (harq->fec_enabled == k_harq_false) {
    err = internal_soft_to_hard(harq->decode_buffer,
                                combined_len,
                                params->expected_output_len,
                                output,
                                output_len);
    if (err != k_rx_ok) {
      return err;
    }
    harq->retry_count++;
    /* Ignore return - combiner is already validated via harq->initialized */
    (void)rx_chase_combiner_reset(&harq->combiner);
    harq->state = k_harq_state_idle;
    return k_rx_ok;
  }

  /* FEC path: decode combined soft bits */
  const rx_fec_decode_soft_params_t decode_params = {
    .soft_bits           = harq->decode_buffer,
    .soft_len            = combined_len,
    .expected_output_len = params->expected_output_len,
    .output              = output,
    .output_len          = output_len,
  };

  err = rx_fec_decode_soft(&harq->decoder, &decode_params);

  return internal_handle_fec_result(harq, err);
}

/**
 * @brief Get number of retries attempted
 *
 *
 */
uint8_t rx_harq_get_retry_count(const rx_harq_handle_t* harq)
{
  if (harq == nullptr) {
    return 0;
  }
  return harq->retry_count;
}

/**
 * @brief Check if more retries are allowed
 *
 *
 */
bool rx_harq_can_retry(const rx_harq_handle_t* harq)
{
  if (harq == nullptr) {
    return false;
  }
  if (harq->initialized == k_harq_false) {
    return false;
  }
  const bool retries_left = (bool)(harq->retry_count < harq->max_retries);
  const bool can_add      = rx_chase_combiner_can_add(&harq->combiner);
  return (bool)((int)retries_left & (int)can_add);
}
