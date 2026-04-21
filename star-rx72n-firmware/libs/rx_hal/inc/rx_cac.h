/**
 * @file rx_cac.h
 * @brief Clock Frequency Accuracy Measurement Circuit (CAC) Driver API for RX72N
 *
 * @details
 * Public driver API for the RX72N Clock Frequency Accuracy Measurement
 * Circuit (CAC).  The CAC compares a measurement-target clock against a
 * reference clock by counting target-clock edges between reference-clock
 * edges, then comparing the count against upper/lower limits set in
 * CAULVR/CALLVR.  If the count is outside the window, the FERRF flag is set
 * and (when enabled) a group-interrupt is raised via GROUPBL0.
 *
 * @par STAR Usage
 * STAR uses CAC to watchdog the main-oscillator (MOSC, 24 MHz crystal)
 * against the internal HOCO (16 MHz). A drifted crystal - from aging,
 * mechanical damage, or PCB leakage - would silently corrupt motor-control
 * timing; the FERRF ISR instead triggers motor e-stop.
 *
 * @par Typical Configuration
 * @code{.c}
 * rx_cac_config_t cfg = {
 *     .measured_clock  = k_cac_clock_main,     // 24 MHz crystal (MOSC)
 *     .reference_clock = k_cac_clock_hoco,     // 16 MHz HOCO
 *     .reference_div   = k_cac_ref_div_1024,   // 16 MHz / 1024 = 15.625 kHz
 *     .caulvr          = 1560,                 // Upper count bound
 *     .callvr          = 1500,                 // Lower count bound
 *     .enable_ferrie   = true,                 // Raise IRQ on drift
 * };
 * (void)rx_cac_init(&cfg);
 * (void)rx_cac_start();
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto / setjmp / recursion
 * - Rule 2: [OK] All loops bounded
 * - Rule 3: [OK] No dynamic allocation (static state only)
 * - Rule 4: [OK] All functions < 60 lines
 * - Rule 5: [OK] >= 2 checks per function (@pre/@post)
 * - Rule 6: [OK] File-scope statics, minimal scope
 * - Rule 7: [OK] rx_err_t returns, checked via RX_RETURN_ON_ERROR
 * - Rule 8: [OK] C23 typed enums for all constants
 * - Rule 9: [OK] Single-level pointers
 * - Rule 10: [OK] -Wall -Wextra -Werror clean
 *
 * @see rx72n_cac_regs.h CAC register definitions
 * @see RX72N HW Manual R01UH0824EJ0111 Chapter 10
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Enums
 * =============================================================================
 */

/**
 * @enum rx_cac_clock_t
 * @brief Clock source selector for CAC measured/reference inputs
 *
 * @details
 * Maps directly onto the FMCS[2:0] / RSCS[2:0] field values defined in
 * CACR1 / CACR2 (RX72N HW Manual p.393 / p.394).  Not every source is legal
 * as a reference - RSCS supports values 000-101 only (Main, Sub, HOCO, LOCO,
 * PCLKB, IWDTCLK); the extra values (UCLK, CLKOUT25M) are measured-only.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_clock_main     = 0, /**< Main-clock oscillator (MOSC, external crystal) */
  k_cac_clock_sub      = 1, /**< Sub-clock oscillator (32.768 kHz) */
  k_cac_clock_hoco     = 2, /**< High-speed on-chip oscillator */
  k_cac_clock_loco     = 3, /**< Low-speed on-chip oscillator */
  k_cac_clock_pclkb    = 4, /**< Peripheral clock B */
  k_cac_clock_iwdtclk  = 5, /**< IWDT-dedicated oscillator */
  k_cac_clock_uclk     = 6, /**< USB clock (measured-only, not a valid RSCS) */
  k_cac_clock_clkout25 = 7, /**< 25 MHz CLKOUT (measured-only, not a valid RSCS) */
} rx_cac_clock_t;

/**
 * @enum rx_cac_ref_div_t
 * @brief Reference-clock divider (CACR2.RCDS[1:0]) per RX72N HW Manual p.394
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_ref_div_32   = 0, /**< Reference clock / 32 */
  k_cac_ref_div_128  = 1, /**< Reference clock / 128 */
  k_cac_ref_div_1024 = 2, /**< Reference clock / 1024 */
  k_cac_ref_div_8192 = 3, /**< Reference clock / 8192 */
} rx_cac_ref_div_t;

/**
 * @enum rx_cac_target_div_t
 * @brief Measured-clock divider (CACR1.TCSS[1:0]) per RX72N HW Manual p.393
 *
 * @details
 * TCSS is a target-clock divider (not a digital filter).  Use /1 for the
 * fastest allowed count rate and larger dividers only when the measurement
 * target clock exceeds the counter bandwidth.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_target_div_1  = 0, /**< Target clock / 1  */
  k_cac_target_div_4  = 1, /**< Target clock / 4  */
  k_cac_target_div_8  = 2, /**< Target clock / 8  */
  k_cac_target_div_32 = 3, /**< Target clock / 32 */
} rx_cac_target_div_t;

/* =============================================================================
 * Configuration Struct
 * =============================================================================
 */

/**
 * @struct rx_cac_config_t
 * @brief Configuration parameters for rx_cac_init()
 *
 * @details
 * Fully describes one CAC measurement campaign.  Callers fill in every field
 * before invoking rx_cac_init(); the driver does not look at partial or
 * uninitialised members.
 *
 * @par Example: MOSC watchdog against HOCO
 * @code{.c}
 * rx_cac_config_t cfg = {
 *     .measured_clock  = k_cac_clock_main,
 *     .reference_clock = k_cac_clock_hoco,
 *     .target_div      = k_cac_target_div_1,
 *     .reference_div   = k_cac_ref_div_1024,
 *     .caulvr          = 1560,
 *     .callvr          = 1500,
 *     .enable_ferrie   = true,
 *     .enable_mendie   = false,
 *     .enable_ovfie    = true,
 * };
 * @endcode
 *
 * @invariant callvr < caulvr (driver rejects otherwise)
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Measured (target) clock source - CACR1.FMCS[2:0]
   * @details The clock whose frequency is being evaluated.  All eight
   * sources defined by rx_cac_clock_t are legal here.
   */
  rx_cac_clock_t measured_clock;

  /**
   * @brief Reference clock source - CACR2.RSCS[2:0]
   * @details The clock the measurement is compared against.  Only the
   * first six entries of rx_cac_clock_t are legal (Main..IWDTCLK); passing
   * k_cac_clock_uclk or k_cac_clock_clkout25 causes rx_cac_init() to
   * return k_rx_err_invalid_arg.
   */
  rx_cac_clock_t reference_clock;

  /**
   * @brief Divider applied to the measured clock - CACR1.TCSS[1:0]
   * @details Defaults to k_cac_target_div_1 for highest-resolution counts.
   */
  rx_cac_target_div_t target_div;

  /**
   * @brief Divider applied to the reference clock - CACR2.RCDS[1:0]
   * @details Selects how many reference-clock edges accumulate into a
   * single measurement window (32 / 128 / 1024 / 8192).
   */
  rx_cac_ref_div_t reference_div;

  /**
   * @brief Upper-limit counter value - CAULVR
   * @details FERRF asserts when (counter > caulvr) at sampling time.
   */
  uint16_t caulvr;

  /**
   * @brief Lower-limit counter value - CALLVR
   * @details FERRF asserts when (counter < callvr) at sampling time.
   */
  uint16_t callvr;

  /**
   * @brief Enable frequency-error interrupt (CAICR.FERRIE)
   * @details When true, the CAC raises an IRQ on every transition into the
   * "counter outside [callvr, caulvr]" state.  Must be used together with
   * a GROUPBL0 IRQ handler wired to bit 26 (FERRF).
   */
  bool enable_ferrie;

  /**
   * @brief Enable measurement-end interrupt (CAICR.MENDIE)
   */
  bool enable_mendie;

  /**
   * @brief Enable counter-overflow interrupt (CAICR.OVFIE)
   */
  bool enable_ovfie;
} rx_cac_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the CAC module with the supplied configuration
 *
 * @details
 * Performs the full CAC bring-up sequence mandated by the RX72N HW Manual
 * Ch10.3 "Operation":
 * 1. Unlock PRCR.PRC1 and clear MSTPCRC.MSTPC19, then re-lock PRCR.
 * 2. With CACR0.CFME = 0, program CACR1 (FMCS, TCSS, EDGES, CAIE),
 *    CACR2 (RSCS, RCDS, RPS, DFS), CAULVR, CALLVR, and CAICR enables.
 * 3. Clear any stale CASTR flags via CAICR write-1-clear.
 * 4. Leave CFME = 0 (caller must invoke rx_cac_start() to begin measuring).
 *
 * @param[in] config Pointer to configuration struct (must not be nullptr)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                 Initialization succeeded, CAC ready
 * @retval k_rx_err_null_ptr       config is nullptr
 * @retval k_rx_err_invalid_arg    Invalid clock/divider, or callvr >= caulvr
 * @retval k_rx_err_invalid_state  CAC already initialized (call deinit first)
 *
 * @pre config != nullptr
 * @pre config->callvr < config->caulvr
 * @pre config->reference_clock is in [k_cac_clock_main .. k_cac_clock_iwdtclk]
 * @pre config->measured_clock is in [k_cac_clock_main .. k_cac_clock_clkout25]
 * @pre config->target_div is in [k_cac_target_div_1 .. k_cac_target_div_32]
 * @pre config->reference_div is in [k_cac_ref_div_32 .. k_cac_ref_div_8192]
 *
 * @post CAC module is powered on (MSTPCRC.MSTPC19 = 0)
 * @post CACR0.CFME = 0 (measurement not yet running)
 * @post All configured limits and interrupt enables are loaded
 *
 * @note Not thread-safe.  Call only during single-threaded initialization.
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cac_init(const rx_cac_config_t* config);

/**
 * @brief Begin CAC measurement (CACR0.CFME = 1)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                 Measurement started
 * @retval k_rx_err_not_initialized rx_cac_init() not called
 *
 * @pre rx_cac_init() returned k_rx_ok
 * @post CACR0.CFME = 1
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cac_start(void);

/**
 * @brief Stop CAC measurement (CACR0.CFME = 0)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                 Measurement stopped
 * @retval k_rx_err_not_initialized rx_cac_init() not called
 *
 * @pre rx_cac_init() returned k_rx_ok
 * @post CACR0.CFME = 0
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cac_stop(void);

/**
 * @brief Sample the CAC status and counter buffer, clearing acknowledged flags
 *
 * @details
 * Reads CASTR, optionally writes back FERRF / MENDF / OVFF acknowledgement
 * bits via CAICR, and returns the last-captured counter buffer via
 * @p out_count. If @p out_count is nullptr the counter is not returned; the
 * function still reads/acks CASTR.
 *
 * @param[out] out_count Optional pointer that receives CACNTBR on success.
 *
 * @return bool
 * @retval true  FERRF was set at read time (frequency outside limits)
 * @retval false FERRF was not set
 *
 * @pre rx_cac_init() returned k_rx_ok (driver returns false otherwise)
 * @post CASTR.FERRF / MENDF / OVFF are cleared (write-1-clear via CAICR)
 *
 * @note Safe to call from ISR context provided @p out_count is ISR-local.
 * @since Version 1.0.0
 */
bool rx_cac_check(uint32_t* out_count);

/**
 * @brief Tear down the CAC module and put it back in module-stop
 *
 * @details
 * Clears CACR0.CFME, disables all interrupt enables, clears status flags,
 * then asserts MSTPCRC.MSTPC19 to power the module down.  Init state is
 * reset so a subsequent rx_cac_init() call is permitted.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                 Module stopped and powered down
 * @retval k_rx_err_not_initialized rx_cac_init() not called
 *
 * @pre rx_cac_init() returned k_rx_ok
 * @post CACR0.CFME = 0 and MSTPCRC.MSTPC19 = 1
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_cac_deinit(void);

#ifdef __cplusplus
}
#endif
