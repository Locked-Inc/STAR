/**
 * @file rx_encoder_tpu.h
 * @brief TPU Quadrature Encoder Driver for RX72N (Hardware Phase Counting Mode)
 *
 * @details
 * Hardware-accelerated quadrature encoder interface using the RX72N Timer Pulse
 * Unit (TPU) peripheral in phase counting mode. Provides zero-CPU-overhead
 * encoder counting with automatic edge detection and direction sensing for the
 * STAR robot's rear wheel encoders.
 *
 * ## System Architecture Context
 *
 * The STAR robot has 4 motors with 341 PPR Hall effect encoders. Front wheels
 * use the MTU peripheral (see rx_mtu_encoder.h); rear wheels use the TPU
 * peripheral handled by this module.
 *
 * | Encoder | Peripheral | Channel | Phase A Pin         | Phase B Pin         |
 * |---------|------------|---------|---------------------|---------------------|
 * | 0 (FL)  | MTU1       | MTU1    | P24/MTCLKA (pin 33) | P25/MTCLKB (pin 32) |
 * | 1 (FR)  | MTU2       | MTU2    | PA1/MTCLKC (pin 96) | PC5/MTCLKD (pin 62) |
 * | 2 (RL)  | TPU1       | TPU1    | PC2/TCLKA (pin 70)  | PA3/TCLKB (pin 94)  |
 * | 3 (RR)  | TPU2       | TPU2    | PC0/TCLKC (pin 75)  | PB3/TCLKD (pin 82)  |
 *
 * ## Design Rationale
 *
 * This module mirrors the rx_mtu_encoder.h API but operates on TPU channels.
 * The same 16-bit overflow detection algorithm and velocity calculation logic
 * are used, providing consistent behavior across front and rear encoders.
 *
 * **16-Bit Counter Overflow Handling:**
 * ```
 * delta = current_count - last_count
 * if (delta > 32768):   # Large positive jump
 *     delta -= 65536    # Counter underflowed
 * elif (delta < -32768):
 *     delta += 65536    # Counter overflowed
 * accumulated_count += delta
 * ```
 *
 * ## Performance Characteristics
 *
 * | Metric                         | Value       | Notes                        |
 * |--------------------------------|-------------|------------------------------|
 * | Counter update                 | Real-time   | Hardware (zero CPU overhead)  |
 * | Read latency                   | ~0.5 us     | Single register read         |
 * | Overflow detection             | ~2 us       | @ 240 MHz                    |
 * | Max safe poll interval (210 RPM)| 6.8 s      | 32768 / (1364 * 3.5 RPS)     |
 * | Actual poll rate               | 250 Hz      | 1666x safety margin          |
 *
 * @par Module Dependencies:
 *
 * **Depends On:**
 * - [rx_err.h](../rx_core/inc/rx_err.h) - Error codes
 * - [rx_tpu.h](../rx_hal/inc/rx_tpu.h) - TPU hardware abstraction
 * - [rx_mtu_encoder.h](rx_mtu_encoder.h) - Shared rx_encoder_state_t type
 *
 * **Used By:**
 * - Motor control task - 250 Hz PID control loop (rear wheels)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 2: All loops bounded
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: All functions < 60 lines
 * - Rule 5: Minimum 2 validation checks per function
 * - Rule 6: Variables at smallest scope
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums, minimal macros
 * - Rule 9: Single-level pointers only
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @see rx_mtu_encoder.h MTU encoder driver (front wheels)
 * @see rx_tpu.h TPU HAL driver
 * @see rx72n_tpu_regs.h TPU register definitions
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "rx_err.h"
#include "rx_mtu_encoder.h"
#include "rx_tpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @struct rx_tpu_encoder_config_t
 * @brief TPU encoder configuration structure
 *
 * @details
 * Configuration parameters for initializing a quadrature encoder on a TPU
 * channel. Mirrors rx_encoder_config_t but uses TPU channel identifiers.
 *
 * @par STAR Project Configuration (back-wheel Hall harness is cross-wired, so
 *      motor 2 = BR reads TPU2 and motor 3 = BL reads TPU1; see
 *      src/tasks/motor_control_task.c:2214-2216):
 * | Motor   | Channel          | counts_per_rev | invert |
 * |---------|------------------|----------------|--------|
 * | 2 (BR)  | k_tpu_channel_2  | 1364           | false  |
 * | 3 (BL)  | k_tpu_channel_1  | 1364           | true   |
 *
 * @invariant counts_per_rev > 0
 * @invariant channel must be 1, 2, 4, or 5
 *
 * @see rx_tpu_encoder_init() Initialization function
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief TPU channel for encoder counting
   * @details Must be a phase-counting-capable channel (1, 2, 4, or 5).
   */
  rx_tpu_channel_t channel;

  /**
   * @brief Encoder counts per revolution (after 4x decoding)
   * @details For STAR's 341 PPR encoders: 341 x 4 = 1364 counts/rev.
   * @par Valid Range: 1 to 65535
   * @warning Zero causes division-by-zero in velocity calculation
   */
  uint16_t counts_per_rev;

  /**
   * @brief Invert encoder count direction
   * @details When true, reverses count direction to compensate for reversed
   * wiring or motor mounting orientation. Software-only inversion.
   */
  bool invert_direction;

} rx_tpu_encoder_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize TPU channel for hardware quadrature encoder counting
 *
 * @details
 * Configures a TPU channel in phase counting mode (4x decoding) and
 * initializes software overflow tracking state. After initialization,
 * the encoder is counting and ready for position/velocity reads.
 *
 * Initialization steps:
 * 1. Validate configuration parameters
 * 2. Initialize TPU HAL in phase counting mode 4
 * 3. Start the TPU counter
 * 4. Initialize software overflow detection state
 *
 * @param[in] config Pointer to encoder configuration
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, encoder initialized and counting
 * @retval k_rx_err_null_ptr config is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel or counts_per_rev == 0
 * @retval k_rx_err_invalid_state Channel already initialized
 *
 * @pre GPIO pins for TCLKA-D configured as peripheral I/O
 * @pre Encoder physically connected
 *
 * @post TPU channel in phase counting mode, counter running
 * @post Software state zeroed (count=0, position=0)
 *
 * @note Thread Safety: Not thread-safe, call during init only
 *
 * @see rx_tpu_encoder_deinit() Release resources
 * @see rx_tpu_encoder_read_count() Read position after init
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_encoder_init(const rx_tpu_encoder_config_t* config);

/**
 * @brief Read raw 16-bit counter value from TPU channel
 *
 * @details
 * Reads the current TCNT value without overflow handling. For position
 * tracking with overflow correction, use rx_tpu_encoder_read_count().
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 * @param[out] count Pointer to store raw 16-bit count (0-65535)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr count is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Encoder not initialized
 *
 * @pre Channel initialized via rx_tpu_encoder_init()
 * @post count contains current TCNT register value
 *
 * @note Thread Safety: Safe (single volatile register read)
 *
 * @see rx_tpu_encoder_read_count() Read with overflow handling
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_encoder_read_raw(rx_tpu_channel_t channel, uint16_t* count);

/**
 * @brief Read encoder count with automatic 16-bit overflow handling
 *
 * @details
 * Reads the current 16-bit hardware counter and updates the 32-bit
 * accumulated count with automatic overflow/underflow correction.
 * Also computes derived position (degrees) and revolution count.
 *
 * Must be called frequently enough to catch overflows: at 210 RPM max
 * motor speed with 1364 counts/rev, the safe interval is 6.8 seconds.
 * The 250 Hz control loop provides a 1666x safety margin.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 * @param[out] state Pointer to encoder state structure to update
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, state updated
 * @retval k_rx_err_null_ptr state is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Encoder not initialized
 *
 * @pre Channel initialized via rx_tpu_encoder_init()
 * @pre Called frequently enough to catch overflows
 *
 * @post state->total_count updated with overflow correction
 * @post state->last_raw_count updated
 * @post state->revolutions and position_deg computed
 *
 * @note Thread Safety: Not thread-safe for same channel
 *
 * @see rx_tpu_encoder_read_velocity() Calculate velocity
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_encoder_read_count(rx_tpu_channel_t    channel,
                                                 rx_encoder_state_t* state);

/**
 * @brief Calculate encoder velocity in revolutions per second
 *
 * @details
 * Derives angular velocity from the change in encoder position over a known
 * time interval. Tracks previous count internally for delta calculation.
 *
 * @param[out] velocity_rps Pointer to store velocity (rev/sec)
 * @param[in] delta_time_s Time since last call in seconds (must be > 0)
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr velocity_rps is nullptr
 * @retval k_rx_err_invalid_arg Invalid delta_time_s or channel
 * @retval k_rx_err_invalid_state Encoder not initialized
 *
 * @pre Channel initialized via rx_tpu_encoder_init()
 * @pre delta_time_s matches actual elapsed time
 *
 * @post velocity_rps contains calculated velocity
 * @post Internal last_count updated for next delta
 *
 * @note Parameter order: output, time, channel (prevents swaps)
 * @note Thread Safety: Not thread-safe for same channel
 *
 * @see rx_tpu_encoder_read_count() Read position
 * @since Version 1.0.0
 */
rx_err_t
rx_tpu_encoder_read_velocity(float* velocity_rps, float delta_time_s, rx_tpu_channel_t channel);

/**
 * @brief Reset encoder count to zero
 *
 * @details
 * Resets both the TPU hardware counter and software accumulated state.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Encoder not initialized
 *
 * @pre Channel initialized
 * @post TCNT=0, total_count=0, position=0
 *
 * @see rx_tpu_encoder_set_count() Set to specific value
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_encoder_reset(rx_tpu_channel_t channel);

/**
 * @brief Set encoder count to specific value
 *
 * @details
 * Sets both hardware counter (low 16 bits) and software accumulated count.
 *
 * @param[in] count Count value to set
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Encoder not initialized
 *
 * @pre Channel initialized
 * @post Software and hardware counts updated
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_encoder_set_count(int32_t count, rx_tpu_channel_t channel);

/**
 * @brief Deinitialize TPU encoder
 *
 * @details
 * Stops counter and releases resources. Can be reinitialized after.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Encoder not initialized
 *
 * @pre Channel previously initialized
 * @post Counter stopped, channel marked uninitialized
 *
 * @see rx_tpu_encoder_init() Reinitialize after deinit
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_encoder_deinit(rx_tpu_channel_t channel);

#ifdef UNIT_TEST
/**
 * @brief Corrupt the counts_per_rev field for a channel (test-only)
 *
 * @details
 * Sets s_counts_per_rev[channel] = 0 to exercise runtime cpr-corrupted guards
 * in internal_update_state(), rx_tpu_encoder_read_velocity(), and
 * rx_tpu_encoder_set_count(). Available only in UNIT_TEST builds.
 *
 * @param[in] channel TPU channel (must be valid: 1, 2, 4, or 5)
 * @post s_counts_per_rev[channel] == 0
 *
 * @note Test-only; not compiled into production firmware
 * @since Version 1.0.0
 */
void rx_tpu_encoder_test_corrupt_cpr(rx_tpu_channel_t channel);
#endif /* UNIT_TEST */

#ifdef UNIT_TEST
/**
 * @brief Direct access to internal_update_state for white-box testing (test-only)
 *
 * @details
 * Exposes the RX_STATIC_TESTABLE internal_update_state() function when compiled
 * with UNIT_TEST defined. Allows tests to exercise defensive guard paths that
 * are unreachable through the public API (null state, uninit channel, position
 * overflow) without modifying production code paths.
 *
 * @param[out] state Output state structure (may be NULL to test null guard)
 * @param[in] channel TPU channel (may be invalid to test channel guard)
 * @param[in] current_count Current 16-bit TCNT value
 *
 * @return rx_err_t Error code as returned by internal_update_state
 *
 * @since Version 1.0.0
 */
rx_err_t
internal_update_state(rx_encoder_state_t* state, rx_tpu_channel_t channel, uint16_t current_count);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
