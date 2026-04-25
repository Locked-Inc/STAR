/**
 * @file rx_fec.h
 * @brief Forward Error Correction (FEC) Codec for RX72N
 *
 * @details
 * Implements NASA-standard K=7 constraint length convolutional encoder and
 * soft-decision Viterbi decoder for reliable communication over noisy channels.
 * This is a C port of the Go FEC implementation in star-gateway, ensuring
 * bit-exact compatibility between RX72N firmware and RPi5 gateway.
 *
 * ## Features
 * - **Rate 1/2 Convolutional Encoding**: 2 output bits per input bit
 * - **Soft-Decision Viterbi Decoding**: Uses soft bit reliability information
 * - **Hard-Decision Fallback**: Converts hard bits to soft for testing
 * - **Static Memory Allocation**: Zero dynamic allocation (NASA Rule 3)
 * - **Tail-Biting Termination**: Ensures encoder returns to zero state
 *
 * ## Convolutional Code Theory
 *
 * A convolutional code encodes input bits using a shift register and generator
 * polynomials. The encoder maintains a state (shift register contents) and
 * produces multiple output bits for each input bit.
 *
 * ### Mathematical Model
 *
 * For this K=7, rate 1/2 code:
 * - **Constraint Length** @f$ K = 7 @f$: Shift register holds 6 bits (K-1)
 * - **Number of States**: @f$ 2^{K-1} = 64 @f$ states
 * - **Code Rate**: @f$ R = \frac{1}{2} @f$ (1 input bit -> 2 output bits)
 * - **Generator Polynomials**:
 *   - @f$ G_1(D) = 1 + D^2 + D^3 + D^5 + D^6 @f$ (octal 171, hex 0xF9)
 *   - @f$ G_2(D) = 1 + D + D^2 + D^3 + D^6 @f$ (octal 133, hex 0x5B)
 *
 * ### Encoder Operation
 *
 * Given shift register state @f$ s = [s_5, s_4, s_3, s_2, s_1, s_0] @f$ and
 * input bit @f$ u @f$, the encoder produces output bits:
 *
 * @f[
 *   c_0 = u \oplus s_0 \oplus s_1 \oplus s_3 \oplus s_4 \oplus s_5 \quad (G_1)
 * @f]
 * @f[
 *   c_1 = u \oplus s_0 \oplus s_1 \oplus s_2 \oplus s_5 \quad (G_2)
 * @f]
 *
 * After outputting @f$ (c_0, c_1) @f$, the state updates:
 * @f$ s \leftarrow [u, s_5, s_4, s_3, s_2, s_1] @f$ (right shift with u
 * inserted)
 *
 * ### Viterbi Decoder
 *
 * The Viterbi algorithm finds the maximum likelihood path through the trellis
 * of possible encoder states. It uses dynamic programming to compute path
 * metrics incrementally.
 *
 * #### Branch Metric
 *
 * For received soft bits @f$ (r_0, r_1) @f$ and expected bits @f$ (e_0, e_1)
 * @f$, the correlation metric is:
 *
 * @f[
 *   \text{correlation} = r_0 \cdot e_0' + r_1 \cdot e_1'
 * @f]
 *
 * where @f$ e_i' = +127 @f$ if @f$ e_i = 1 @f$, else @f$ e_i' = -127 @f$.
 *
 * Branch metric (lower is better):
 * @f[
 *   M_{\text{branch}} = 32768 - \text{correlation}
 * @f]
 *
 * #### Path Metric Update
 *
 * For each state @f$ s @f$ at time @f$ t @f$:
 * @f[
 *   M_s(t) = \min_{s'} \left[ M_{s'}(t-1) + M_{\text{branch}}(s' \to s) \right]
 * @f]
 *
 * #### Traceback
 *
 * After processing all symbols, traceback from the zero state (encoder was
 * flushed with tail bits) to recover the input sequence.
 *
 * ## Protocol Integration
 *
 * @msc
 * Transmitter, Channel, Receiver;
 *
 * --- [label="Encoding Phase"];
 * Transmitter box Transmitter [label="Input: 100 bytes"];
 * Transmitter => Transmitter [label="rx_fec_encode()"];
 * Transmitter box Transmitter [label="Output: 202 bytes\n(100*8+6)*2 bits"];
 *
 * --- [label="Transmission"];
 * Transmitter => Channel [label="Encoded bits"];
 * Channel box Channel [label="Add noise\nBit flips\nErasures"];
 * Channel => Receiver [label="Noisy soft bits"];
 *
 * --- [label="Decoding Phase"];
 * Receiver box Receiver [label="Soft bits: [-127, +127]"];
 * Receiver => Receiver [label="rx_fec_decode_soft()"];
 * Receiver box Receiver [label="Viterbi algorithm\nForward pass\nTraceback"];
 * Receiver box Receiver [label="Output: 100 bytes"];
 * @endmsc
 *
 * ## Performance Characteristics
 *
 * - **Encoding**: @f$ O(n) @f$ where @f$ n @f$ is input bits
 *   - Time: ~15 us per 100 bytes @ 240 MHz
 *   - Memory: Zero dynamic allocation
 *
 * - **Decoding**: @f$ O(n \cdot 2^{K-1} \cdot 2) = O(128n) @f$
 *   - Time: ~800 us per 100 bytes @ 240 MHz
 *   - Memory: 64 path metrics + survivors buffer
 *
 * ## Coding Gain
 *
 * At BER @f$ 10^{-5} @f$, this code provides approximately 5 dB coding gain
 * over uncoded transmission on AWGN channel with soft-decision decoding.
 *
 * @par Hardware Requirements
 * | Resource       | Usage                                       |
 * |----------------|---------------------------------------------|
 * | CPU            | RX72N @ 240 MHz                            |
 * | RAM (Encoder)  | ~4 bytes (state only)                      |
 * | RAM (Decoder)  | ~16 KB (survivors buffer for 1024B payload)|
 * | Flash          | ~6 KB (code size)                          |
 *
 * @par Module Dependencies
 * - [rx_err.h](rx_err_8h.html): Error codes
 * - [rx_bit_constants.h](rx_bit_constants_8h.html): Bit manipulation constants
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No recursion, no goto, no setjmp/longjmp
 * - **Rule 2**: [OK] All loops have fixed bounds (k_fec_max_symbols)
 * - **Rule 3**: [OK] Zero dynamic memory allocation (all static buffers)
 * - **Rule 4**: [OK] Functions < 60 lines (largest: internal_viterbi_process_symbol ~50 lines)
 * - **Rule 5**: [OK] All functions have >=2 preconditions and postconditions
 * - **Rule 8**: [OK] Uses C23 typed enums instead of \#define for all constants
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: Encoder and decoder are separate concerns
 * - **Open/Closed**: FEC parameters configurable via enums, no hardcoded magic numbers
 * - **Dependency Inversion**: Uses abstract rx_err_t for error handling
 *
 * @note This implementation is **thread-safe** if encoder/decoder handles
 *       are not shared between threads. Caller must provide synchronization
 *       when sharing handles.
 *
 * @warning Soft bits must be in range [-127, +127]. Values outside this range
 *          will be clamped during processing, which may reduce decoding accuracy.
 *
 * @see star-gateway/internal/fec/ for Go reference implementation
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 * @see "Error Control Coding" by Lin & Costello for theory
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test test_rx_fec.c contains unit tests for encoder and decoder
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_bit_constants.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * FEC Parameters (NASA Standard K=7, Rate 1/2)
 * =============================================================================
 */

/**
 * @enum rx_fec_params_t
 * @brief FEC convolutional code parameters
 *
 * @details
 * Defines the NASA-standard K=7, rate 1/2 convolutional code used throughout
 * the STAR project. These parameters were selected for optimal balance between
 * coding gain (~5 dB @ BER 10^-5) and computational complexity.
 *
 * ## Parameter Relationships
 *
 * The parameters satisfy these mathematical constraints:
 * - @f$ \text{num\_states} = 2^{\text{constraint\_length} - 1} = 2^6 = 64 @f$
 * - @f$ \text{tail\_bits} = \text{constraint\_length} - 1 = 6 @f$
 * - @f$ \text{code\_rate} = \frac{1}{\text{num\_outputs}} = \frac{1}{2} @f$
 *
 * ## Generator Polynomial Selection
 *
 * G1=171 (octal) and G2=133 (octal) provide maximum free distance @f$ d_{\text{free}} = 10 @f$
 * among all K=7 codes, yielding best error correction performance.
 *
 * Binary representations:
 * - @f$ G_1 = 1111001_2 = 171_8 = 0x79 @f$
 * - @f$ G_2 = 1011011_2 = 133_8 = 0x5B @f$
 *
 * @par Traceback Depth Rationale
 * Traceback depth of 5K is industry standard for K=7 codes, providing
 * 99.99% probability of correct path selection while minimizing memory
 * (5x7 = 35 symbols = 70 bits = ~9 bytes per state).
 *
 * @par Field Constraints
 * | Field              | Min | Max | Rationale                                |
 * |--------------------|-----|-----|------------------------------------------|
 * | constraint_length  | 7   | 7   | Fixed by NASA standard                   |
 * | num_states         | 64  | 64  | Derived: 2^(K-1)                        |
 * | tail_bits          | 6   | 6   | Derived: K-1 (flush encoder to zero)    |
 * | traceback_depth    | 35  | 35  | 5K empirical optimal                    |
 * | num_outputs        | 2   | 2   | Rate 1/2 code (fixed)                   |
 *
 * @par Usage Example
 * @code{.c}
 * // Calculate encoded length
 * uint32_t input_bits = 800;  // 100 bytes
 * uint32_t total_bits = input_bits + k_fec_tail_bits;  // 806 bits
 * uint32_t output_bits = total_bits * k_fec_num_outputs;  // 1612 bits
 * uint32_t output_bytes = (output_bits + 7) / 8;  // 202 bytes
 *
 * // Verify state count
 * assert(k_fec_num_states == (1 << (k_fec_constraint_length - 1)));
 * @endcode
 *
 * @see "Error Control Coding" by Lin & Costello, Section 11.3
 * @see NASA/JPL standard for deep space communication (K=7, r=1/2)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**< @brief Constraint length K=7 (shift register holds K-1 = 6 bits)
   * @details Determines number of states (2^6=64) and code complexity.
   * Larger K increases coding gain but exponentially increases decoder
   * complexity. K=7 is optimal for embedded systems. */
  k_fec_constraint_length = 7,

  /**< @brief Number of encoder states: 2^(K-1) = 64
   * @details Each state represents a unique 6-bit shift register configuration.
   * Viterbi decoder maintains path metrics for all 64 states simultaneously. */
  k_fec_num_states = 64,

  /**< @brief Tail bits for termination: K-1 = 6 zeros
   * @details Appended to input to flush encoder to zero state, enabling
   * traceback to start from known state. Without tail bits, decoder would
   * need to search all final states, reducing performance. */
  k_fec_tail_bits = 6,

  /**< @brief Generator polynomial G1 in octal: 171 (binary 1111001)
   * @details Tap connections: positions 0,3,4,5,6 (MSB is input bit).
   * Combined with G2 to achieve d_free=10 (maximum for K=7). */
  k_fec_g1_octal = 0171,

  /**< @brief Generator polynomial G2 in octal: 133 (binary 1011011)
   * @details Tap connections: positions 0,1,2,3,6 (MSB is input bit).
   * Complementary to G1 to maximize minimum Hamming distance. */
  k_fec_g2_octal = 0133,

  /**< @brief Traceback depth: 5xK = 35 symbols
   * @details Industry standard for K=7 codes. Deeper traceback increases
   * latency and memory but negligibly improves error rate. 5K provides
   * >99.99% probability of correct decision. Memory: 35 x 64 bits = 280 bytes. */
  k_fec_traceback_depth = 35,

  /**< @brief Number of possible input values: 2 (binary: 0 or 1)
   * @details Used in branch table generation to enumerate all transitions.
   * For each state, there are 2 possible next states (input=0 or input=1). */
  k_fec_num_input_values = 2,

  /**< @brief Number of output bits per input bit: 2 (rate 1/2)
   * @details Code rate R = 1/2 means bandwidth expansion by factor of 2.
   * Lower rates (1/3, 1/4) increase coding gain but reduce data throughput.
   * 1/2 is optimal for most communication systems. */
  k_fec_num_outputs = 2,
} rx_fec_params_t;

/* =============================================================================
 * Soft Bit Representation
 * =============================================================================
 */

/**
 * @typedef rx_soft_bit_t
 * @brief Soft bit type for soft-decision decoding
 *
 * @details
 * Soft bits carry both **demodulated bit value** and **reliability information**.
 * This enables the Viterbi decoder to make better decisions than hard-decision
 * decoding, providing approximately 2 dB coding gain improvement.
 *
 * ## Soft Bit Interpretation
 *
 * The soft bit value @f$ s \in [-127, +127] @f$ represents:
 * - **Sign**: Demodulated bit value
 *   - @f$ s \geq 0 @f$: Received bit is likely 1
 *   - @f$ s < 0 @f$: Received bit is likely 0
 *
 * - **Magnitude**: Confidence in decision
 *   - @f$ |s| = 127 @f$: High confidence (strong signal, no noise)
 *   - @f$ |s| \approx 0 @f$: Low confidence (weak signal, high noise)
 *   - @f$ s = 0 @f$: Erasure (completely uncertain)
 *
 * ## Mathematical Model
 *
 * Given received signal @f$ r @f$ and expected values @f$ +A @f$ (bit=1)
 * or @f$ -A @f$ (bit=0), the soft bit is computed as:
 *
 * @f[
 *   s = \text{clamp}\left( \frac{r}{A} \cdot 127, -127, +127 \right)
 * @f]
 *
 * ## Conversion from Hard Bits
 *
 * Hard bit @f$ b \in \{0, 1\} @f$ converts to soft bit:
 * @f[
 *   s = \begin{cases}
 *     +127 & \text{if } b = 1 \\
 *     -127 & \text{if } b = 0
 *   \end{cases}
 * @f]
 *
 * This represents maximum confidence in the hard decision.
 *
 * ## Why Signed 8-bit?
 *
 * - **Range**: [-127, +127] provides sufficient quantization (8 levels above/below threshold)
 * - **Memory**: 1 byte per soft bit vs 1 bit per hard bit (8x overhead acceptable for 2 dB gain)
 * - **Arithmetic**: Signed addition in Chase Combining uses int16 to prevent overflow
 * - **Performance**: Native CPU type, no bit-packing overhead
 *
 * @par Usage Example
 * @code{.c}
 * // Hard bit conversion
 * uint8_t hard_bit = 1;
 * rx_soft_bit_t soft = rx_fec_hard_to_soft(hard_bit);  // soft = +127
 *
 * // Reverse conversion
 * uint8_t recovered = rx_fec_soft_to_hard(soft);  // recovered = 1
 *
 * // Erasure detection
 * rx_soft_bit_t uncertain = 0;
 * if (uncertain == k_soft_bit_zero) {
 *     // Channel reported complete uncertainty - may want to request retransmission
 * }
 *
 * // Confidence thresholding
 * rx_soft_bit_t weak_signal = 10;  // Low confidence
 * if (abs(weak_signal) < 50) {
 *     // Signal too weak, may need combining with retransmissions
 * }
 * @endcode
 *
 * @note The int8_t type is signed, providing natural representation of
 *       positive/negative polarities without bit manipulation.
 *
 * @warning Values outside [-127, +127] may occur during Chase Combining
 *          accumulation (uses int16), but are clamped before decoding.
 *
 * @see rx_fec_hard_to_soft() Convert hard bit to soft bit
 * @see rx_fec_soft_to_hard() Convert soft bit to hard bit
 * @see rx_chase_combiner_t Accumulates soft bits across retransmissions
 *
 * @since Version 1.0.0
 */
typedef int8_t rx_soft_bit_t;

/**
 * @enum rx_soft_bit_limits_t
 * @brief Soft bit value range limits
 *
 * @details
 * Defines the valid range for soft bit values. These limits ensure that
 * soft bits fit in an int8_t and provide symmetric positive/negative ranges.
 *
 * ## Range Justification
 *
 * - **Maximum +127**: Highest positive value for int8_t. Represents absolute
 *   certainty that received bit is 1.
 *
 * - **Minimum -127**: Symmetric negative bound. Represents absolute certainty
 *   that received bit is 0. Note: -128 is avoided to maintain symmetry
 *   (|+127| = |-127| = 127).
 *
 * - **Zero**: No information. Used for erasures (corrupted symbols that
 *   provide no reliable information).
 *
 * ## Why Not -128?
 *
 * Using -128 would create asymmetry: @f$ |-128| = 128 \neq 127 @f$.
 * This would complicate branch metric calculations and potentially cause
 * subtle biases in the Viterbi decoder. Symmetric range ensures:
 *
 * @f[
 *   |k_{\text{soft\_bit\_max}}| = |k_{\text{soft\_bit\_min}}| = 127
 * @f]
 *
 * @par Usage Example
 * @code{.c}
 * // Validate soft bit range
 * rx_soft_bit_t s = get_demodulated_soft_bit();
 * if (s < k_soft_bit_min || s > k_soft_bit_max) {
 *     // Clamp to valid range
 *     if (s < k_soft_bit_min) s = k_soft_bit_min;
 *     if (s > k_soft_bit_max) s = k_soft_bit_max;
 * }
 *
 * // Initialize soft bit array to erasures
 * rx_soft_bit_t soft_bits[100];
 * for (int i = 0; i < 100; i++) {
 *     soft_bits[i] = k_soft_bit_zero;  // All erasures
 * }
 * @endcode
 *
 * @see rx_soft_bit_t Soft bit type definition
 *
 * @since Version 1.0.0
 */
typedef enum : int8_t {
  /**< @brief Maximum soft bit value: +127 (confident bit=1)
   * @details Represents strongest possible belief that received bit is 1.
   * Used when channel has no noise and signal is clearly above threshold. */
  k_soft_bit_max = 127,

  /**< @brief Minimum soft bit value: -127 (confident bit=0)
   * @details Represents strongest possible belief that received bit is 0.
   * Used when channel has no noise and signal is clearly below threshold.
   * Note: -128 is NOT used to maintain symmetric range. */
  k_soft_bit_min = -127,

  /**< @brief Zero confidence: 0 (erasure)
   * @details Represents complete uncertainty about bit value. Used when:
   * - Signal is at exact threshold (cannot decide 0 vs 1)
   * - Channel reported error/corruption
   * - Demodulator detected ambiguous symbol
   * Viterbi decoder treats erasures as equally likely 0 or 1. */
  k_soft_bit_zero = 0,
} rx_soft_bit_limits_t;

/* =============================================================================
 * Encoder
 * =============================================================================
 */

/**
 * @struct rx_fec_encoder_t
 * @brief FEC convolutional encoder state
 *
 * @details
 * The encoder is intentionally **stateless between encode() calls**. Each
 * invocation of rx_fec_encode() starts with a zeroed shift register and
 * terminates with tail bits to return to zero state. This design simplifies
 * frame-based encoding and eliminates inter-frame dependencies.
 *
 * ## Rationale for Stateless Design
 *
 * **Advantages:**
 * - **Thread Safety**: Multiple threads can share read-only encoder (after init)
 * - **Determinism**: Same input always produces same output (no hidden state)
 * - **Testability**: No need to track encoder history across test cases
 * - **Frame Independence**: Dropped frame doesn't corrupt future frames
 *
 * **Trade-off:**
 * - Tail bits add 6 bits overhead per frame (~0.75% for 100-byte frame)
 * - Alternative (stateful encoder) would save overhead but risk error propagation
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field       | Type | Alignment | Description              |
 * |--------|------|-------------|------|-----------|--------------------------|
 * | 0      | 1    | initialized | bool | 1         | Initialization flag      |
 * | 1-3    | 3    | (padding)   | -    | -         | Compiler padding to 4B   |
 *
 * Total size: 4 bytes (aligned to word boundary for optimal access)
 *
 * @par Usage Example
 * @code{.c}
 * // Initialize encoder
 * rx_fec_encoder_t encoder;
 * rx_err_t err = rx_fec_encoder_init(&encoder);
 * if (err != k_rx_ok) {
 *     return err;
 * }
 *
 * // Encode multiple frames (stateless - no crosstalk)
 * uint8_t input1[100], input2[100];
 * uint8_t output1[250], output2[250];
 * uint32_t len1, len2;
 *
 * rx_fec_encode(&encoder, input1, 100, output1, &len1);  // Independent
 * rx_fec_encode(&encoder, input2, 100, output2, &len2);  // Independent
 *
 * // Cleanup
 * rx_fec_encoder_deinit(&encoder);
 * @endcode
 *
 * @note The encoder does NOT store shift register state between calls.
 *       Internal state is local to rx_fec_encode() execution.
 *
 * @see rx_fec_encoder_init() Initialize encoder
 * @see rx_fec_encoder_deinit() Deinitialize encoder
 * @see rx_fec_encode() Encode data with tail termination
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**< @brief Initialization status flag
   * @details Set to true by rx_fec_encoder_init(), cleared by rx_fec_encoder_deinit().
   * Checked by rx_fec_encode() to prevent use-before-init errors (NASA Rule 5).
   * @par Valid Values: true (initialized), false (not initialized)
   * @par Default: false (uninitialized struct has zero bytes) */
  bool initialized;
} rx_fec_encoder_t;

/**
 * @brief Initialize FEC encoder
 *
 * @details
 * Prepares encoder for use by setting initialization flag. The encoder itself
 * is stateless (no shift register persists between calls), so initialization
 * only validates the handle.
 *
 * **Algorithm:**
 * 1. Validate enc pointer (NULL check)
 * 2. Set enc->initialized = true
 * 3. Return success
 *
 * @param[out] enc Pointer to encoder handle to initialize
 *                 - Must not be nullptr
 *                 - Contents will be overwritten
 *                 - Lifetime: Caller-managed, must persist until deinit
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Successfully initialized, encoder ready for use
 * @retval k_rx_err_invalid_arg enc is nullptr
 *
 * @pre enc must point to valid rx_fec_encoder_t storage
 * @pre Sufficient stack/heap space for encoder structure (4 bytes)
 *
 * @post enc->initialized == true
 * @post Encoder is ready for rx_fec_encode() calls
 *
 * @note **Thread Safety**: Safe to call concurrently on different encoder handles.
 *       NOT safe to call concurrently on the same handle.
 *
 * @note **Re-initialization**: Safe to call on already-initialized encoder
 *       (idempotent operation).
 *
 * @par Example
 * @code{.c}
 * // Stack allocation
 * rx_fec_encoder_t encoder;
 * rx_err_t err = rx_fec_encoder_init(&encoder);
 * if (err != k_rx_ok) {
 *     // Handle error
 *     return err;
 * }
 *
 * // Now ready for encoding
 * uint8_t input[100] = {...};
 * uint8_t output[250];
 * uint32_t output_len;
 * rx_fec_encode(&encoder, input, 100, output, &output_len);
 *
 * // Cleanup
 * rx_fec_encoder_deinit(&encoder);
 * @endcode
 *
 * @see rx_fec_encoder_deinit() Deinitialize encoder
 * @see rx_fec_encode() Encode data
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_fec_encoder_init(rx_fec_encoder_t* enc);

/**
 * @brief Deinitialize FEC encoder
 *
 * @details
 * Marks encoder as uninitialized. After this call, encoder must be
 * re-initialized before use. This function provides symmetric
 * init/deinit API for resource management patterns.
 *
 * **Algorithm:**
 * 1. Validate enc pointer (NULL check)
 * 2. Set enc->initialized = false
 * 3. Return success
 *
 * @param[in,out] enc Pointer to encoder handle to deinitialize
 *                    - Must not be nullptr
 *                    - May be uninitialized (idempotent)
 *                    - Contents will be cleared
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Successfully deinitialized
 * @retval k_rx_err_invalid_arg enc is nullptr
 *
 * @pre enc must point to valid memory (may be uninitialized)
 *
 * @post enc->initialized == false
 * @post Encoder cannot be used until rx_fec_encoder_init() called again
 *
 * @note **Thread Safety**: Safe to call concurrently on different encoder handles.
 *       NOT safe to call concurrently on the same handle.
 *
 * @note **Idempotent**: Safe to call multiple times on same encoder.
 *
 * @warning Do NOT call rx_fec_encode() after deinit - will return k_rx_err_invalid_state
 *
 * @par Example
 * @code{.c}
 * rx_fec_encoder_t encoder;
 * rx_fec_encoder_init(&encoder);
 *
 * // ... use encoder ...
 *
 * // Cleanup (always succeeds if enc is valid pointer)
 * rx_err_t err = rx_fec_encoder_deinit(&encoder);
 * assert(err == k_rx_ok);
 *
 * // Re-initialization allowed
 * rx_fec_encoder_init(&encoder);
 * @endcode
 *
 * @see rx_fec_encoder_init() Initialize encoder
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_fec_encoder_deinit(rx_fec_encoder_t* enc);

/**
 * @brief Encode data using K=7, rate 1/2 convolutional code
 *
 * @details
 * Encodes input bytes into FEC-protected output using NASA-standard convolutional
 * code. Each input bit produces 2 output bits (G1 and G2), followed by 6 tail
 * bits to flush encoder to zero state.
 *
 * ## Algorithm Steps
 *
 * 1. **Validation**: Check pointers, initialization, input length
 * 2. **Calculation**: Compute output size = (input_bits + 6) x 2 / 8 bytes
 * 3. **Initialization**: Clear output buffer, reset shift register to 0
 * 4. **Data Encoding**: For each input bit (MSB first):
 *    - Compute G1 = parity(input_bit || state) & 0x79
 *    - Compute G2 = parity(input_bit || state) & 0x5B
 *    - Output (G1, G2) bit pair
 *    - Update state: shift right, insert input_bit
 * 5. **Tail Termination**: Encode 6 zero bits to flush state to 0
 * 6. **Return**: Set output_len, return success
 *
 * ## Mathematical Model
 *
 * For input sequence @f$ \mathbf{u} = [u_0, u_1, \ldots, u_{n-1}] @f$,
 * the encoder produces output @f$ \mathbf{c} = [c_0^1, c_0^2, c_1^1, c_1^2, \ldots] @f$
 * where:
 *
 * @f[
 *   c_i^1 = u_i \oplus \sum_{j=0}^{5} g_1[j] \cdot s_j
 * @f]
 * @f[
 *   c_i^2 = u_i \oplus \sum_{j=0}^{5} g_2[j] \cdot s_j
 * @f]
 *
 * State update: @f$ s_{i+1} = [u_i, s_{i,5}, s_{i,4}, s_{i,3}, s_{i,2}, s_{i,1}] @f$
 *
 * ## Output Length Formula
 *
 * Given @f$ n @f$ input bytes:
 * @f[
 *   L_{\text{out}} = \left\lceil \frac{(8n + 6) \times 2}{8} \right\rceil = 2n + 2 \text{ bytes}
 * @f]
 *
 * Examples:
 * - 100 bytes in -> 202 bytes out (expansion factor ~2.02)
 * - 1 byte in -> 4 bytes out (overhead significant for small frames)
 *
 * ## Performance Analysis
 *
 * - **Time Complexity**: @f$ O(8n) @f$ where @f$ n @f$ = input_len
 * - **Space Complexity**: @f$ O(1) @f$ (constant stack usage)
 * - **Execution Time**: ~15 us for 100 bytes @ 240 MHz
 * - **Per-Bit Cost**: ~19 CPU cycles per input bit
 *
 * ## State Machine
 *
 * @dot
 * digraph encoding_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   init [label="Initialize\nstate = 0"];
 *   loop [label="For each\ninput bit"];
 *   encode [label="Compute G1, G2\nOutput pair"];
 *   update [label="Shift state\nInsert input"];
 *   tail [label="Append 6\ntail bits"];
 *   done [label="Return\nencoded data"];
 *
 *   init -> loop;
 *   loop -> encode;
 *   encode -> update;
 *   update -> loop [label="more bits"];
 *   loop -> tail [label="done"];
 *   tail -> done;
 * }
 * @enddot
 *
 * @param[in] enc Initialized encoder handle
 *                - Must be initialized via rx_fec_encoder_init()
 *                - Read-only (const): Can share among threads
 *
 * @param[in] input Input data bytes to encode
 *                  - Must not be nullptr
 *                  - Valid range: [1, k_fec_max_input_bytes] bytes
 *                  - Read-only: Original data unchanged
 *
 * @param[in] input_len Number of input bytes
 *                      - Valid range: [1, 1024] bytes
 *                      - Constraint: input_len <= k_fec_max_input_bytes
 *
 * @param[out] output Output buffer for encoded data
 *                    - Must not be nullptr
 *                    - Minimum size: rx_fec_encoded_len(input_len) bytes
 *                    - Cleared before writing
 *                    - MSB-first bit packing (network byte order)
 *
 * @param[out] output_len Actual number of output bytes written
 *                        - Must not be nullptr
 *                        - Set to (input_len x 8 + 6) x 2 / 8 bytes
 *                        - Always <= buffer size if input valid
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Successfully encoded, output contains FEC data
 * @retval k_rx_err_invalid_arg Any pointer is nullptr or input_len is 0
 * @retval k_rx_err_invalid_state Encoder not initialized
 * @retval k_rx_err_invalid_size input_len exceeds k_fec_max_input_bytes
 *
 * @pre enc->initialized == true (via rx_fec_encoder_init())
 * @pre input != nullptr && output != nullptr && output_len != nullptr
 * @pre 1 <= input_len <= k_fec_max_input_bytes
 * @pre output buffer size >= rx_fec_encoded_len(input_len)
 *
 * @post output contains FEC-encoded data (big-endian bit packing)
 * @post *output_len == (input_len x 8 + 6) x 2 / 8 (rounded up)
 * @post Last 6 tail bits ensure encoder state returns to zero
 *
 * @invariant Encoding is deterministic: same input -> same output
 * @invariant Output length formula always holds: 2n + 2 bytes for n input bytes
 *
 * @note **Thread Safety**: Encoder is read-only during encode, safe for
 *       concurrent encode() calls on same encoder handle.
 *
 * @note **Bit Ordering**: Bits packed MSB-first (network byte order).
 *       First output bit goes to byte[0] bit 7.
 *
 * @note **Performance**: For critical paths, consider encoding in batches
 *       to amortize function call overhead.
 *
 * @warning output buffer MUST be at least rx_fec_encoded_len(input_len) bytes.
 *          Buffer overflow protection is caller's responsibility.
 *
 * @par Example: Basic Encoding
 * @code{.c}
 * rx_fec_encoder_t enc;
 * rx_fec_encoder_init(&enc);
 *
 * uint8_t input[100] = {...};  // 100 bytes input data
 * uint32_t output_size = rx_fec_encoded_len(100);  // 202 bytes
 * uint8_t output[202];
 * uint32_t actual_len;
 *
 * rx_err_t err = rx_fec_encode(&enc, input, 100, output, &actual_len);
 * if (err == k_rx_ok) {
 *     // actual_len == 202
 *     // Transmit output[0..201] over noisy channel
 * }
 * @endcode
 *
 * @par Example: Error Handling
 * @code{.c}
 * rx_err_t err = rx_fec_encode(&enc, data, len, out, &out_len);
 * switch (err) {
 *     case k_rx_ok:
 *         break;  // Success
 *     case k_rx_err_invalid_arg:
 *         rx_log_error("FEC", "nullptr or zero length");
 *         return err;
 *     case k_rx_err_invalid_state:
 *         rx_log_error("FEC", "Encoder not initialized");
 *         return err;
 *     case k_rx_err_invalid_size:
 *         rx_log_error("FEC", "Input too large (max 1024 bytes)");
 *         return err;
 * }
 * @endcode
 *
 * @par Example: Batch Encoding
 * @code{.c}
 * // Encode multiple frames efficiently
 * for (int i = 0; i < num_frames; i++) {
 *     uint32_t len;
 *     rx_err_t err = rx_fec_encode(&enc, frames[i], frame_sizes[i],
 *                                  encoded[i], &len);
 *     if (err != k_rx_ok) {
 *         // Handle error for frame i
 *         continue;
 *     }
 *     // Transmit encoded[i][0..len-1]
 * }
 * @endcode
 *
 * @see rx_fec_encoder_init() Initialize encoder first
 * @see rx_fec_encoded_len() Calculate required output buffer size
 * @see rx_fec_decode_soft() Decode with soft decision
 * @see rx_fec_decode_hard() Decode with hard decision
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test test_rx_fec.c::test_encode_basic() Tests basic encoding
 * @test test_rx_fec.c::test_encode_tail_bits() Verifies tail bit termination
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 2**: [OK] Loop bounds statically provable (input_len <= k_fec_max_input_bytes)
 * - **Rule 5**: [OK] 7 precondition checks, 3 postconditions
 */
[[nodiscard]] rx_err_t rx_fec_encode(const rx_fec_encoder_t* enc,
                                     const uint8_t*          input,
                                     uint32_t                input_len,
                                     uint8_t*                output,
                                     uint32_t*               output_len);

/**
 * @brief Calculate FEC encoded output length
 *
 * @details
 * Computes the number of bytes required to store FEC-encoded output for
 * a given input length. This function should be called before rx_fec_encode()
 * to allocate appropriately-sized output buffers.
 *
 * ## Formula Derivation
 *
 * Given @f$ n @f$ input bytes:
 *
 * 1. **Input bits**: @f$ b_{\text{in}} = 8n @f$
 * 2. **Add tail bits**: @f$ b_{\text{total}} = 8n + 6 @f$
 * 3. **Apply rate 1/2**: @f$ b_{\text{out}} = 2(8n + 6) = 16n + 12 @f$
 * 4. **Convert to bytes (ceiling)**: @f$ L_{\text{out}} = \lceil \frac{16n + 12}{8} \rceil = 2n + 2 @f$
 *
 * Simplified formula:
 * @f[
 *   L_{\text{out}}(n) = 2n + 2 \text{ bytes}
 * @f]
 *
 * ## Example Calculations
 *
 * | Input (bytes) | Output (bytes) | Expansion | Overhead (%) |
 * |---------------|----------------|-----------|--------------|
 * | 1             | 4              | 4.00x     | 300%         |
 * | 10            | 22             | 2.20x     | 120%         |
 * | 100           | 202            | 2.02x     | 102%         |
 * | 1024          | 2050           | 2.00x     | 100.2%       |
 *
 * Note: Overhead decreases with larger frames (tail bits amortized).
 *
 * ## Performance
 *
 * - **Time Complexity**: @f$ O(1) @f$ (constant time calculation)
 * - **Execution Time**: < 1 us @ 240 MHz (integer arithmetic only)
 *
 * @param[in] input_len Number of input bytes to encode
 *                      - Valid range: [0, k_fec_max_input_bytes]
 *                      - Value 0 returns 0 (no output for no input)
 *                      - Values > k_fec_max_input_bytes return 0 (error)
 *
 * @return uint32_t Number of output bytes after encoding
 * @retval 2xinput_len+2 Normal case (1 <= input_len <= 1024)
 * @retval 0 Invalid input (input_len == 0 or > k_fec_max_input_bytes)
 *
 * @pre None (pure function, no side effects)
 *
 * @post Return value is deterministic function of input_len
 * @post For valid input: return == 2xinput_len + 2
 * @post For invalid input: return == 0
 *
 * @note **Thread Safety**: Pure function, safe to call concurrently.
 *
 * @note Return value of 0 indicates invalid input_len parameter.
 *
 * @warning Caller must check return value before allocating buffer.
 *          Zero return means invalid input, not "zero bytes needed".
 *
 * @par Example: Buffer Allocation
 * @code{.c}
 * uint32_t input_len = 100;
 * uint32_t output_size = rx_fec_encoded_len(input_len);
 * if (output_size == 0) {
 *     // Invalid input length
 *     return k_rx_err_invalid_arg;
 * }
 *
 * uint8_t* output = malloc(output_size);  // Allocate 202 bytes
 * // ... use buffer ...
 * free(output);
 * @endcode
 *
 * @par Example: Static Buffer Sizing
 * @code{.c}
 * \#define MAX_INPUT_LEN 100
 * \#define MAX_OUTPUT_LEN (2 * MAX_INPUT_LEN + 2)  // 202 bytes
 *
 * uint8_t output[MAX_OUTPUT_LEN];
 * uint32_t actual_len = rx_fec_encoded_len(input_len);
 * assert(actual_len <= MAX_OUTPUT_LEN);  // Verify buffer sufficiency
 * @endcode
 *
 * @par Example: Error Detection
 * @code{.c}
 * uint32_t size = rx_fec_encoded_len(5000);  // Too large
 * if (size == 0) {
 *     // Input exceeds maximum (1024 bytes)
 *     rx_log_error("FEC", "Input too large for FEC encoding");
 *     return k_rx_err_invalid_size;
 * }
 * @endcode
 *
 * @see rx_fec_encode() Encode data (uses this for buffer size)
 * @see k_fec_max_input_bytes Maximum allowable input length
 *
 * @since Version 1.0.0
 */
uint32_t rx_fec_encoded_len(uint32_t input_len);

/* =============================================================================
 * Decoder
 * =============================================================================
 */

/**
 * @enum rx_fec_buffer_limits_t
 * @brief Maximum buffer sizes for static FEC decoder allocation
 *
 * @details
 * Defines compile-time buffer limits to enable zero dynamic allocation (NASA Rule 3).
 * Values calculated from maximum payload size (1024 bytes) and FEC parameters.
 *
 * ## Size Calculations
 *
 * For maximum 1024-byte payload:
 * - Input bits: @f$ 1024 \times 8 = 8192 @f$
 * - With tail: @f$ 8192 + 6 = 8198 @f$ bits
 * - Encoded symbols (rate 1/2): @f$ 8198 @f$ symbol pairs
 * - Safety margin: Round to 8200 symbols
 *
 * @par Memory Usage
 * | Buffer               | Size                           | Bytes  |
 * |----------------------|--------------------------------|--------|
 * | Soft bits            | 8200 x 2 x 1 byte              | 16400  |
 * | Survivors            | 8200 x 8 bytes (uint64_t)      | 65600  |
 * | Path metrics         | 64 x 4 bytes (int32_t)         | 256    |
 * | Branch table         | 64 x 2 x 2 x 1 byte            | 256    |
 * | **Total per decoder**|                                | **~82 KB** |
 *
 * @note Large memory footprint limits number of concurrent decoders.
 *       RX72N has 512 KB SRAM, allowing ~6 decoders maximum.
 *
 * @see rx_fec_decoder_t Decoder structure using these limits
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**< @brief Maximum symbols for Viterbi trellis: 8200
   * @details Supports up to 1024-byte payloads (8192 bits + 6 tail bits + 2 margin).
   * Each symbol is a pair of soft bits (G1, G2). */
  k_fec_max_symbols = 8200,

  /**< @brief Maximum input bytes before FEC encoding: 1024
   * @details Derived from k_fec_max_symbols: (8200 - 6) / 8 = 1024.25 -> 1024 bytes.
   * Larger payloads must be fragmented. */
  k_fec_max_input_bytes = (uint16_t)((k_fec_max_symbols - k_fec_tail_bits) / k_rx_bits_per_byte),
} rx_fec_buffer_limits_t;

static_assert((bool)(k_fec_max_input_bytes ==
                     (uint16_t)((k_fec_max_symbols - k_fec_tail_bits) / k_rx_bits_per_byte)),
              "k_fec_max_input_bytes mismatch with k_fec_max_symbols");

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
 * @return k_rx_err_invalid_arg if any pointer is nullptr or buffer too small
 */
rx_err_t
rx_fec_decoder_init(rx_fec_decoder_t* dec, uint64_t* survivors_buf, uint32_t survivors_len);

/**
 * @brief Deinitialize FEC decoder
 *
 * @param[in,out] dec Pointer to decoder handle
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_fec_decoder_deinit(rx_fec_decoder_t* dec);

/**
 * @brief Soft-decision decode parameters
 *
 * Groups parameters for soft-decision Viterbi decoding to simplify API.
 */
typedef struct {
  const rx_soft_bit_t* soft_bits;           /**< Received soft bits (pairs) */
  uint32_t             soft_len;            /**< Number of soft bits (must be even) */
  uint32_t             expected_output_len; /**< Expected decoded length in bytes */
  uint8_t*             output;              /**< Decoded output buffer */
  uint32_t*            output_len;          /**< Actual decoded length */
} rx_fec_decode_soft_params_t;

/**
 * @brief Decode soft bits using Viterbi algorithm
 *
 * Performs soft-decision Viterbi decoding on received soft bits.
 * Soft bits should be in pairs (G1, G2) for each encoded symbol.
 *
 * @param[in]     dec    Initialized decoder handle
 * @param[in,out] params Decode parameters (input soft bits, output buffer)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr or soft_len is odd
 * @return k_rx_err_invalid_state if decoder not initialized
 * @return k_rx_err_invalid_size if output buffer too small
 */
[[nodiscard]] rx_err_t rx_fec_decode_soft(rx_fec_decoder_t*                  dec,
                                          const rx_fec_decode_soft_params_t* params);

/**
 * @brief Hard-decision decode parameters
 *
 * Groups parameters for hard-decision Viterbi decoding to simplify API.
 */
typedef struct {
  const uint8_t* data;                /**< Received hard bits (packed bytes) */
  uint32_t       data_len;            /**< Number of bytes */
  uint32_t       expected_output_len; /**< Expected decoded length in bytes */
  uint8_t*       output;              /**< Decoded output buffer */
  uint32_t*      output_len;          /**< Actual decoded length */
  rx_soft_bit_t* soft_bits_buffer;    /**< Working buffer for soft bit conversion */
  uint32_t       soft_buffer_len;     /**< Length of soft_bits_buffer */
} rx_fec_decode_hard_params_t;

/**
 * @brief Decode hard bits using Viterbi algorithm
 *
 * Converts hard bits to soft bits internally, then performs Viterbi decoding.
 * Less accurate than soft-decision decoding but useful for testing.
 *
 * The caller must provide a working buffer for soft bit conversion to ensure
 * thread safety. Size should be at least k_fec_max_symbols * k_fec_num_outputs.
 *
 * @param[in]     dec    Initialized decoder handle
 * @param[in,out] params Decode parameters (input data, buffers)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is nullptr
 * @return k_rx_err_invalid_state if decoder not initialized
 * @return k_rx_err_invalid_size if soft_bits_buffer is too small
 */
[[nodiscard]] rx_err_t rx_fec_decode_hard(rx_fec_decoder_t*                  dec,
                                          const rx_fec_decode_hard_params_t* params);

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
