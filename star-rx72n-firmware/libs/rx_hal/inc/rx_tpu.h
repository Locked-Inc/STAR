/**
 * @file rx_tpu.h
 * @brief TPU HAL Driver for RX72N 16-Bit Timer Pulse Unit (Phase Counting)
 *
 * @details
 * Hardware abstraction layer for the RX72N TPU peripheral, focused on
 * phase counting mode for quadrature encoder input. This driver provides
 * low-level channel configuration, counter access, and direction reading.
 *
 * ## STAR Project Usage
 *
 * The TPU is used for rear wheel encoder decoding. Front wheels use the MTU
 * peripheral (see rx_mtu.h / rx_mtu_encoder.h).
 *
 * | Encoder | Peripheral | Channel | Phase A Pin         | Phase B Pin         |
 * |---------|------------|---------|---------------------|---------------------|
 * | 0 (FL)  | MTU1       | MTU1    | P24/MTCLKA (pin 33) | P25/MTCLKB (pin 32) |
 * | 1 (FR)  | MTU2       | MTU2    | PA1/MTCLKC (pin 96) | PC5/MTCLKD (pin 62) |
 * | 2 (BR)  | TPU1       | TPU1    | PC2/TCLKA (pin 70)  | PA3/TCLKB (pin 94)  |
 * | 3 (BL)  | TPU2       | TPU2    | PC0/TCLKC (pin 75)  | PB3/TCLKD (pin 82)  |
 *
 * ## Architecture
 *
 * ```
 * Application Layer:  motor_control_task.c (250 Hz PID loop)
 *                          |
 * Encoder Layer:      rx_encoder_tpu.c (overflow handling, velocity)
 *                          |
 * HAL Layer:          rx_tpu.h/c (THIS MODULE - register access)
 *                          |
 * Register Layer:     rx72n_tpu_regs.h (memory-mapped I/O)
 * ```
 *
 * ## Key Features
 *
 * - Phase counting mode 4 (4x resolution, all edges)
 * - 16-bit up/down counter with direction flag
 * - Hardware noise filter on input pins
 * - Module stop control for power management
 *
 * @par Performance
 * | Operation              | Time     | Notes                    |
 * |------------------------|----------|--------------------------|
 * | rx_tpu_init_phase()    | ~20 us   | One-time setup           |
 * | rx_tpu_read_count()    | ~0.5 us  | Single register read     |
 * | rx_tpu_read_direction()| ~0.5 us  | Single register read     |
 * | rx_tpu_start()         | ~0.5 us  | Single register write    |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
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
 * @par SOLID Principles
 * - **S**: Only TPU hardware access (no encoder logic)
 * - **O**: Extensible via rx_tpu_config_t
 * - **L**: Consistent rx_err_t return semantics
 * - **I**: Focused API: init, start, stop, read, reset, deinit
 * - **D**: Depends on register abstractions, not raw addresses
 *
 * @see rx72n_tpu_regs.h TPU register definitions
 * @see rx_mtu.h MTU HAL (front wheel encoders)
 * @see rx_mtu_encoder.h MTU encoder interface
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_channel_t
 * @brief TPU channel identifiers for phase counting
 *
 * @details
 * Identifies TPU channels that support phase counting mode. Only TPU1/2/4/5
 * can do phase counting; TPU0 and TPU3 cannot.
 *
 * @par STAR Channel Assignments
 * | Value | Channel | Use               | Clock Pins      |
 * |-------|---------|-------------------|-----------------|
 * | 1     | TPU1    | Back-right encoder| TCLKA + TCLKB   |
 * | 2     | TPU2    | Back-left encoder | TCLKC + TCLKD   |
 * | 4     | TPU4    | Available         | TCLKC + TCLKD   |
 * | 5     | TPU5    | Available         | TCLKA + TCLKB   |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_channel_1 = 1, /**< TPU1: back-right encoder (TCLKA/TCLKB) */
  k_tpu_channel_2 = 2, /**< TPU2: back-left encoder (TCLKC/TCLKD) */
  k_tpu_channel_4 = 4, /**< TPU4: available (TCLKC/TCLKD, paired with TPU2) */
  k_tpu_channel_5 = 5, /**< TPU5: available (TCLKA/TCLKB, paired with TPU1) */
} rx_tpu_channel_t;

/**
 * @enum rx_tpu_phase_mode_t
 * @brief TPU phase counting mode selection
 *
 * @details
 * Selects the phase counting resolution. Mode 4 (4x) provides the highest
 * resolution by counting all edges of both A and B phases.
 *
 * @par Mode Comparison
 * | Mode | Resolution | Edges Counted                     | Counts/PPR |
 * |------|------------|-----------------------------------|------------|
 * | 1    | 1x         | A-phase rising only               | 1          |
 * | 2    | 1x         | B-phase rising only               | 1          |
 * | 3    | 2x         | A-phase rising and falling         | 2          |
 * | 4    | 4x         | All edges of A and B phases        | 4          |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_phase_mode_1 = 1, /**< 1x resolution (A rising only) */
  k_tpu_phase_mode_2 = 2, /**< 1x resolution (B rising only) */
  k_tpu_phase_mode_3 = 3, /**< 2x resolution (A both edges) */
  k_tpu_phase_mode_4 = 4, /**< 4x resolution (all edges) - STAR default */
} rx_tpu_phase_mode_t;

/**
 * @struct rx_tpu_config_t
 * @brief TPU phase counting configuration
 *
 * @details
 * Configuration parameters for initializing a TPU channel in phase
 * counting mode. Passed to rx_tpu_init_phase_count().
 *
 * @par Example
 * @code{.c}
 * rx_tpu_config_t config = {
 *     .channel    = k_tpu_channel_1,
 *     .phase_mode = k_tpu_phase_mode_4,
 * };
 * rx_err_t err = rx_tpu_init_phase_count(&config);
 * @endcode
 *
 * @invariant channel must be 1, 2, 4, or 5 (phase counting capable)
 * @invariant phase_mode must be 1-4
 *
 * @see rx_tpu_init_phase_count() Initialization function
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief TPU channel to configure
   * @details Must be a phase-counting-capable channel (1, 2, 4, or 5).
   */
  rx_tpu_channel_t channel;

  /**
   * @brief Phase counting mode (1-4)
   * @details Selects the counting resolution. Mode 4 recommended for
   * maximum resolution (4 counts per encoder pulse).
   */
  rx_tpu_phase_mode_t phase_mode;
} rx_tpu_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize TPU channel for phase counting mode
 *
 * @details
 * Configures a TPU channel for hardware quadrature encoder phase counting.
 * This function:
 * 1. Enables the TPU module (clears MSTPA13)
 * 2. Stops the channel counter
 * 3. Sets phase counting mode in TMDR
 * 4. Clears the counter to zero
 * 5. Enables overflow/underflow interrupts (if needed later)
 *
 * After initialization, call rx_tpu_start() to begin counting.
 *
 * @param[in] config Phase counting configuration
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, channel configured
 * @retval k_rx_err_null_ptr config is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel (not 1/2/4/5)
 * @retval k_rx_err_invalid_arg Invalid phase mode (not 1-4)
 * @retval k_rx_err_invalid_state Channel already initialized
 *
 * @pre TPU module clock enabled (or function enables it)
 * @pre External clock pins (TCLKA-D) configured as peripheral I/O
 *
 * @post Channel configured in phase counting mode
 * @post Counter cleared to zero
 * @post Counter NOT started (call rx_tpu_start())
 *
 * @note Thread Safety: Not thread-safe, call during init only
 *
 * @see rx_tpu_start() Start counting after init
 * @see rx_tpu_deinit() Release channel resources
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_init_phase_count(const rx_tpu_config_t* config);

/**
 * @brief Start TPU channel counter
 *
 * @details
 * Sets the CSTn bit in TSTR to begin counter operation.
 * In phase counting mode, the counter increments/decrements based
 * on the external clock phase relationship.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, counter started
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized via rx_tpu_init_phase_count()
 * @post CSTn bit set, counter running
 *
 * @note Thread Safety: Safe from any context (atomic register write)
 *
 * @see rx_tpu_stop() Stop counter
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_start(rx_tpu_channel_t channel);

/**
 * @brief Stop TPU channel counter
 *
 * @details
 * Clears the CSTn bit in TSTR to stop counter operation.
 * The counter value is preserved.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, counter stopped
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized
 * @post CSTn bit cleared, counter stopped
 * @post Counter value preserved
 *
 * @note Thread Safety: Safe from any context (atomic register write)
 *
 * @see rx_tpu_start() Restart counter
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_stop(rx_tpu_channel_t channel);

/**
 * @brief Read TPU channel 16-bit counter value
 *
 * @details
 * Reads the current TCNT register value. In phase counting mode this
 * is an up/down counter that tracks encoder position modulo 65536.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 * @param[out] count Pointer to store 16-bit counter value (0-65535)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, count written
 * @retval k_rx_err_null_ptr count is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized and running
 * @post count contains current TCNT value
 *
 * @note Thread Safety: Safe - single volatile register read
 *
 * @see rx_tpu_read_direction() Read counting direction
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_read_count(rx_tpu_channel_t channel, uint16_t* count);

/**
 * @brief Read TPU channel counting direction
 *
 * @details
 * Reads the TCFD bit in the TSR register to determine whether the
 * counter is currently counting up (forward) or down (reverse).
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 * @param[out] counting_up Pointer to store direction flag
 *             (true = counting up/forward, false = counting down/reverse)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, direction written
 * @retval k_rx_err_null_ptr counting_up is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized and running in phase counting mode
 * @post counting_up reflects TSR.TCFD bit state
 *
 * @note Thread Safety: Safe - single volatile register read
 *
 * @see rx_tpu_read_count() Read counter value
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_read_direction(rx_tpu_channel_t channel, bool* counting_up);

/**
 * @brief Reset TPU channel counter to zero
 *
 * @details
 * Writes zero to the TCNT register. Should be called while the counter
 * is stopped for reliable operation, though the hardware permits writing
 * TCNT while running.
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, counter zeroed
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized
 * @post TCNT = 0
 *
 * @note Thread Safety: Not thread-safe (read-modify-write possible issues)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_reset_count(rx_tpu_channel_t channel);

/**
 * @brief Deinitialize TPU channel
 *
 * @details
 * Stops the channel counter, resets mode to normal, and marks the
 * channel as uninitialized. Does NOT disable the TPU module clock
 * (other channels may still be active).
 *
 * @param[in] channel TPU channel (1, 2, 4, or 5)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, channel released
 * @retval k_rx_err_invalid_arg Invalid channel
 *
 * @pre Channel was previously initialized
 * @post Counter stopped and zeroed
 * @post Mode register reset to normal operation
 * @post Channel marked as uninitialized
 *
 * @note Thread Safety: Not thread-safe, call during shutdown only
 *
 * @see rx_tpu_init_phase_count() Re-initialize after deinit
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_tpu_deinit(rx_tpu_channel_t channel);

#ifdef __cplusplus
}
#endif
