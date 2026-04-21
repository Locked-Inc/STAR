/**
 * @file rx_lpc.h
 * @brief Low Power Consumption (LPC) HAL Driver for RX72N
 *
 * @details
 * **Hardware Abstraction Layer** for the RX72N Low Power Consumption subsystem.
 * Provides a small, auditable API for entering the three RX72N low-power modes
 * and for selecting the operating power control mode that governs active-run
 * current.
 *
 * ## Supported Modes (RX72N Manual Chapter 11)
 *
 * | Mode                      | CPU    | Peripherals | RAM      | Wake Latency | Exit Vector       |
 * |---------------------------|--------|-------------|----------|--------------|-------------------|
 * | Sleep                     | Stop   | Run         | Retained | ns           | Continue next PC  |
 * | Software Standby          | Stop   | Stop        | Retained | us           | Continue next PC  |
 * | Deep Software Standby     | Stop   | Stop        | Lost*    | ms           | Reset vector      |
 *
 * @verbatim
 *   * Deep Software Standby: On-chip RAM contents are lost, but the 32-byte
 *     Deep Standby Backup Register (DPSBKR) is retained. See rx72n_lpc_regs.h
 *     for the DPSBKR accessor.
 * @endverbatim
 *
 * ## Operating Power Control (OPCCR) Modes
 *
 * | Mode                | ICLK max | Notes                                   |
 * |---------------------|----------|-----------------------------------------|
 * | High-speed (default)| 240 MHz  | Full performance                        |
 * | Low-speed 1         | 1 MHz    | PLL/HOCO may be stopped                 |
 * | Low-speed 2         | 264 kHz  | HOCO must be stopped (LOCO/Main only)   |
 *
 * @note RX72N only implements three OPCCR modes (high-speed, low-speed 1,
 *       low-speed 2). The "middle-speed" and "sub-oscillator-speed" modes
 *       from the RX100/RX200 family (SOPCCR) are **not available** on RX72N
 *       and therefore have no corresponding API enumerators in this driver.
 *
 * ## Wake Source Selection (Deep Software Standby)
 *
 * Wake sources for deep software standby are specified as a 32-bit bitmask
 * passed to rx_lpc_enter_deep_software_standby(). The bitmask layout mirrors
 * DPSIER0..DPSIER3 as a single little-endian word:
 *
 * @verbatim
 *   bits [ 7: 0] - IRQ0-DS  .. IRQ7-DS   (DPSIER0)
 *   bits [15: 8] - IRQ8-DS  .. IRQ15-DS  (DPSIER1)
 *   bits [23:16] - LVD1/LVD2/RTC-P/RTC-A/NMI/IIC-SDA/IIC-SCL/USB (DPSIER2)
 *   bits [31:24] - CAN1-RX (bit 24 only) (DPSIER3)
 * @endverbatim
 *
 * The same encoding is returned by rx_lpc_get_wake_flags() when reading the
 * DPSIFRx flag registers after a deep-standby wake.
 *
 * ## Hardware Register References
 *
 * | Register | Address     | Manual Section | Purpose                       |
 * |----------|-------------|----------------|-------------------------------|
 * | SBYCR    | 0x0008000C  | 11.2.1 (p.405) | Standby control (SSBY, OPE)   |
 * | OPCCR    | 0x000800A0  | 11.2.6 (p.414) | Operating power control       |
 * | RSTCKCR  | 0x000800A1  | 11.2.7 (p.417) | Sleep-mode return clock       |
 * | DPSBYCR  | 0x0008C280  | 11.2.8 (p.418) | Deep standby control          |
 * | DPSIER0-3| 0x0008C282  | 11.2.9-12      | Deep standby IRQ enable       |
 * | DPSIFR0-3| 0x0008C286  | 11.2.13-16     | Deep standby wake flags       |
 * | DPSIEGR0-3| 0x0008C28A | 11.2.17-20     | Deep standby edge select      |
 * | DPSBKR   | 0x0008C2A0  | 11.2.21 (p.433)| Deep standby backup RAM (32B) |
 *
 * ## Power Protection (PRCR)
 *
 * All writes to SBYCR, OPCCR, RSTCKCR, and DPSBYCR require PRC1 to be
 * unlocked via PRCR. This driver handles PRCR lock/unlock internally.
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation                                        |
 * |------|--------|-------------------------------------------------------|
 * | 1    | [OK]   | No goto/setjmp/recursion                              |
 * | 2    | [OK]   | All loops bounded (OPCMTSF poll has bounded retry)    |
 * | 3    | [OK]   | Zero dynamic allocation                               |
 * | 4    | [OK]   | All functions < 60 lines                              |
 * | 5    | [OK]   | >= 2 pre/post conditions per function                 |
 * | 6    | [OK]   | Narrow scope, file-scope statics (s_*)                |
 * | 7    | [OK]   | All return values checked                             |
 * | 8    | [OK]   | C23 typed enums, minimal preprocessor                 |
 * | 9    | [OK]   | Single-level pointers, no function pointers           |
 * | 10   | [OK]   | Compiles with -Wall -Wextra -Werror                   |
 *
 * @see rx72n_lpc_regs.h   Register layout and bit definitions
 * @see rx72n_system_regs.h SBYCR / PRCR / MSTPCRx accessors
 * @see RX72N Hardware Manual Chapter 11 - Low Power Consumption
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
 * Configuration Types
 * =============================================================================
 */

/**
 * @enum rx_lpc_opcc_mode_t
 * @brief Operating Power Control (OPCCR) mode selector
 *
 * @details
 * Selects the active-run power mode used while the CPU is running. The
 * RX72N supports exactly three OPCCR modes (RX72N Manual Section 11.2.6,
 * page 414). The underlying OPCM[2:0] field values are encoded directly.
 *
 * @par Mode Transitions
 * Writing to OPCCR triggers an internal transition; the hardware sets
 * OPCCR.OPCMTSF=1 until the transition completes. This driver polls that
 * flag with a bounded retry count.
 *
 * @warning Low-speed mode 2 requires HOCO to be stopped. The caller is
 *          responsible for configuring the clock tree before invoking
 *          rx_lpc_set_operating_power().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lpc_opcc_high_speed  = 0, /**< High-speed mode (OPCM=000b, ICLK <= 240 MHz) */
  k_lpc_opcc_low_speed_1 = 1, /**< Low-speed mode 1 (OPCM=110b, ICLK <= 1 MHz) */
  k_lpc_opcc_low_speed_2 = 2, /**< Low-speed mode 2 (OPCM=111b, ICLK <= 264 kHz) */
} rx_lpc_opcc_mode_t;

/**
 * @enum rx_lpc_deep_power_t
 * @brief Deep-standby power supply configuration (DPSBYCR.DEEPCUT)
 *
 * @details
 * Selects how aggressively the deep-standby mode cuts power. Mirrors
 * DPSBYCR.DEEPCUT[1:0] (RX72N Manual Section 11.2.8, page 418).
 *
 * @note The 10b DEEPCUT combination is reserved and is not exposed here.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lpc_deep_ram_usb_on  = 0, /**< DEEPCUT=00b: standby RAM + USB powered */
  k_lpc_deep_ram_usb_off = 1, /**< DEEPCUT=01b: standby RAM + USB powered off */
  k_lpc_deep_lvd_off     = 2, /**< DEEPCUT=11b: LVD also off (lowest power) */
} rx_lpc_deep_power_t;

/**
 * @enum rx_lpc_last_mode_t
 * @brief Last low-power mode entered (test / diagnostic hook)
 *
 * @details
 * Recorded by rx_lpc_enter_*() prior to executing the WAIT instruction so
 * that host-side unit tests can verify the driver reached the correct code
 * path without executing a real WAIT.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lpc_mode_none                  = 0, /**< No low-power entry attempted yet */
  k_lpc_mode_sleep                 = 1, /**< Sleep mode (WAIT, SBYCR.SSBY=0) */
  k_lpc_mode_software_standby      = 2, /**< Software standby (SBYCR.SSBY=1, DPSBY=0) */
  k_lpc_mode_deep_software_standby = 3, /**< Deep software standby (SSBY=1, DPSBY=1) */
} rx_lpc_last_mode_t;

/* =============================================================================
 * Wake-source Bitmask (Deep Software Standby)
 * =============================================================================
 */

/**
 * @enum rx_lpc_wake_flags_t
 * @brief Wake-source bits for deep software standby
 *
 * @details
 * 32-bit bitmask layout used by rx_lpc_enter_deep_software_standby() and
 * returned by rx_lpc_get_wake_flags(). The layout mirrors DPSIER0..DPSIER3
 * (and the corresponding DPSIFR0..DPSIFR3 flag registers) as a single
 * little-endian word.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* DPSIER0 - IRQ0-DS .. IRQ7-DS */
  k_lpc_wake_irq0  = 0x00000001U, /**< IRQ0-DS  pin */
  k_lpc_wake_irq1  = 0x00000002U, /**< IRQ1-DS  pin */
  k_lpc_wake_irq2  = 0x00000004U, /**< IRQ2-DS  pin */
  k_lpc_wake_irq3  = 0x00000008U, /**< IRQ3-DS  pin */
  k_lpc_wake_irq4  = 0x00000010U, /**< IRQ4-DS  pin */
  k_lpc_wake_irq5  = 0x00000020U, /**< IRQ5-DS  pin */
  k_lpc_wake_irq6  = 0x00000040U, /**< IRQ6-DS  pin */
  k_lpc_wake_irq7  = 0x00000080U, /**< IRQ7-DS  pin */

  /* DPSIER1 - IRQ8-DS .. IRQ15-DS */
  k_lpc_wake_irq8  = 0x00000100U, /**< IRQ8-DS  pin */
  k_lpc_wake_irq9  = 0x00000200U, /**< IRQ9-DS  pin */
  k_lpc_wake_irq10 = 0x00000400U, /**< IRQ10-DS pin */
  k_lpc_wake_irq11 = 0x00000800U, /**< IRQ11-DS pin */
  k_lpc_wake_irq12 = 0x00001000U, /**< IRQ12-DS pin */
  k_lpc_wake_irq13 = 0x00002000U, /**< IRQ13-DS pin */
  k_lpc_wake_irq14 = 0x00004000U, /**< IRQ14-DS pin */
  k_lpc_wake_irq15 = 0x00008000U, /**< IRQ15-DS pin */

  /* DPSIER2 - LVD / RTC / NMI / IIC / USB */
  k_lpc_wake_lvd1         = 0x00010000U, /**< LVD1 (Voltage Monitor 1) */
  k_lpc_wake_lvd2         = 0x00020000U, /**< LVD2 (Voltage Monitor 2) */
  k_lpc_wake_rtc_periodic = 0x00040000U, /**< RTC periodic interrupt */
  k_lpc_wake_rtc_alarm    = 0x00080000U, /**< RTC alarm interrupt */
  k_lpc_wake_nmi          = 0x00100000U, /**< NMI pin (write-once bit) */
  k_lpc_wake_iic_sda      = 0x00200000U, /**< SDA2-DS pin */
  k_lpc_wake_iic_scl      = 0x00400000U, /**< SCL2-DS pin */
  k_lpc_wake_usb          = 0x00800000U, /**< USB suspend/resume */

  /* DPSIER3 - CAN1 RX */
  k_lpc_wake_can1_rx = 0x01000000U, /**< CRX1-DS pin (CAN1 RX) */

  /** Mask of all defined wake sources */
  k_lpc_wake_all_mask = 0x01FFFFFFU,
} rx_lpc_wake_flags_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize the Low Power Consumption driver
 *
 * @details
 * Clears the driver's internal state, latches whether the most recent reset
 * was a deep-standby wake-up (so application code can read it back via
 * rx_lpc_was_deep_standby_wake()), and pre-clears any pending deep-standby
 * wake flags (DPSIFR0..DPSIFR3) that may have been set by the preceding
 * deep-standby exit.
 *
 * This function does **not** change the operating power mode; applications
 * should call rx_lpc_set_operating_power() separately if they want to switch
 * away from the default high-speed mode.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Driver initialized successfully
 *
 * @pre May be called exactly once per reset
 * @pre System clocks must already be configured
 *
 * @post Driver state tracking is reset
 * @post Deep-standby wake flags are cleared
 *
 * @note Safe to call from single-threaded startup only.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_lpc_init(void);

/* =============================================================================
 * Operating Power Control (Active Run)
 * =============================================================================
 */

/**
 * @brief Select the operating power control (OPCCR) mode
 *
 * @details
 * Writes OPCCR.OPCM[2:0] under PRC1 protection and polls OPCCR.OPCMTSF=0
 * for transition completion (bounded retry).
 *
 * @param[in] mode Target operating power mode
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                Mode transition completed
 * @retval k_rx_err_invalid_arg   mode not one of rx_lpc_opcc_mode_t
 * @retval k_rx_err_not_initialized rx_lpc_init() has not been called
 * @retval k_rx_err_hw_timeout    OPCMTSF did not clear within bounded retries
 *
 * @pre rx_lpc_init() has been called
 * @pre Clock tree pre-conditions for the target mode are satisfied by the caller
 *
 * @post OPCCR.OPCM[2:0] reflects the requested mode
 * @post OPCCR.OPCMTSF == 0
 *
 * @warning Low-speed mode 2 requires the caller to stop HOCO first (RX72N
 *          Manual Section 11.4.4). Low-speed mode 1 requires PLL to be
 *          stopped.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_lpc_set_operating_power(rx_lpc_opcc_mode_t mode);

/* =============================================================================
 * Mode Entry
 * =============================================================================
 */

/**
 * @brief Enter Sleep mode (CPU stop, peripherals run)
 *
 * @details
 * Clears SBYCR.SSBY under PRC1 protection, then executes the WAIT
 * instruction. On RX72N, WAIT with SSBY=0 enters sleep mode. Execution
 * resumes from the instruction after WAIT when any unmasked interrupt
 * fires.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                Returned from sleep by an interrupt
 * @retval k_rx_err_not_initialized rx_lpc_init() has not been called
 *
 * @pre rx_lpc_init() has been called
 * @pre At least one interrupt source is armed and enabled in ICU
 *
 * @post SBYCR.SSBY == 0 on exit
 * @post s_last_mode == k_lpc_mode_sleep (test-visible)
 *
 * @note On host builds the WAIT instruction is replaced by a no-op.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_lpc_enter_sleep(void);

/**
 * @brief Enter Software Standby mode
 *
 * @details
 * Sets SBYCR.SSBY=1 and clears DPSBYCR.DPSBY=0 under PRC1 protection,
 * then executes WAIT. On exit the MCU resumes at the instruction after
 * WAIT (not the reset vector). RAM contents are retained.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                Returned from software standby
 * @retval k_rx_err_not_initialized rx_lpc_init() has not been called
 *
 * @pre rx_lpc_init() has been called
 * @pre A wake source (NMI, IRQn, RTC, IWDT, LVD) is enabled in the ICU
 *
 * @post SBYCR.SSBY == 1 on entry, hardware auto-clears on wake
 * @post s_last_mode == k_lpc_mode_software_standby (test-visible)
 *
 * @note Typical current draw ~3 uA at 3.3 V (RX72N datasheet Table 58.xx).
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_lpc_enter_software_standby(void);

/**
 * @brief Enter Deep Software Standby mode
 *
 * @details
 * Configures DPSIER0..DPSIER3 from @p wake_mask, sets DPSBYCR.DEEPCUT,
 * optionally sets DPSBYCR.IOKEEP, sets SBYCR.SSBY=1 and DPSBYCR.DPSBY=1
 * under PRC1 protection, then executes WAIT.
 *
 * On deep-standby wake the MCU restarts from the reset vector. This
 * function therefore does **not return** in that case on real hardware;
 * the return path only exists for host-side unit tests. After reset,
 * rx_lpc_was_deep_standby_wake() returns true and rx_lpc_get_wake_flags()
 * reports which source caused the wake.
 *
 * @param[in] wake_mask Bitwise OR of rx_lpc_wake_flags_t values; must not be 0
 * @param[in] deep_power Deep-cut power configuration (DEEPCUT[1:0])
 * @param[in] keep_io   If true, set DPSBYCR.IOKEEP=1 so I/O state is retained
 *                      across the wake-up until the application clears it
 *
 * @return rx_err_t Error code (returned only on host builds / on error)
 * @retval k_rx_ok                 Configuration accepted (host build)
 * @retval k_rx_err_invalid_arg    wake_mask == 0 or contains undefined bits
 * @retval k_rx_err_invalid_arg    deep_power not one of rx_lpc_deep_power_t
 * @retval k_rx_err_not_initialized rx_lpc_init() has not been called
 *
 * @pre rx_lpc_init() has been called
 * @pre wake_mask != 0 and (wake_mask & ~k_lpc_wake_all_mask) == 0
 * @pre Corresponding pins / peripherals configured by the caller
 *
 * @post DPSIER0..DPSIER3 reflect the enabled wake sources
 * @post DPSBYCR.DEEPCUT/IOKEEP/DPSBY configured per arguments
 * @post s_last_mode == k_lpc_mode_deep_software_standby (test-visible)
 *
 * @warning This function does not return on real hardware. Any cleanup
 *          required after deep standby must be performed at the reset
 *          vector / application startup path.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_lpc_enter_deep_software_standby(uint32_t            wake_mask,
                                                          rx_lpc_deep_power_t deep_power,
                                                          bool                keep_io);

/* =============================================================================
 * Post-Wake Diagnostics
 * =============================================================================
 */

/**
 * @brief Query whether the last reset was caused by a deep-standby wake
 *
 * @details
 * The information is latched by rx_lpc_init() from the internal state it
 * inherits from the reset path. On real hardware the caller is expected to
 * have inspected RSTSR0/1/2 (see rx_rstsr accessors) and, if appropriate,
 * to have called the test-only setter rx_lpc_test_set_deep_standby_wake()
 * or equivalent boot-path bridge. The default for a cold power-on is false.
 *
 * @return true if the most recent reset was a deep-standby wake-up
 * @return false otherwise
 *
 * @pre rx_lpc_init() has been called
 * @post Return value is stable until the next reset
 *
 * @since Version 1.0.0
 */
[[nodiscard]] bool rx_lpc_was_deep_standby_wake(void);

/**
 * @brief Read the deep-standby wake flags (DPSIFR0..DPSIFR3)
 *
 * @details
 * Returns a 32-bit bitmask describing which wake sources triggered the most
 * recent deep-standby exit. The layout matches rx_lpc_wake_flags_t. Reading
 * does not clear the flags; the driver clears them in rx_lpc_init().
 *
 * @param[out] flags Pointer to receive the wake-flag bitmask
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                Flags read successfully
 * @retval k_rx_err_invalid_arg   flags is NULL
 * @retval k_rx_err_not_initialized rx_lpc_init() has not been called
 *
 * @pre rx_lpc_init() has been called
 * @pre flags != NULL
 *
 * @post *flags contains a bitwise OR of rx_lpc_wake_flags_t values
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_lpc_get_wake_flags(uint32_t* flags);

/* =============================================================================
 * Test-Only Helpers (UNIT_TEST builds)
 * =============================================================================
 */

#ifdef UNIT_TEST
/**
 * @brief Reset all driver state (unit-test only)
 *
 * @details
 * Clears initialization flag, last-mode tracker, simulated DPSBYCR / SBYCR
 * shadow registers (in host builds), and simulated DPSIFRx flags. Intended
 * to be called from setUp() of every Unity test.
 *
 * @post Driver is returned to the exact state it has immediately after reset
 * @post rx_lpc_init() must be called again before any other API call
 *
 * @since Version 1.0.0
 */
void rx_lpc_test_reset(void);

/**
 * @brief Retrieve the last low-power mode the driver was asked to enter
 *
 * @details
 * Host-side tests use this to verify the driver reached the intended mode
 * entry path without executing a real WAIT instruction.
 *
 * @return rx_lpc_last_mode_t Last mode entered (k_lpc_mode_none if never)
 *
 * @since Version 1.0.0
 */
rx_lpc_last_mode_t rx_lpc_test_get_last_mode(void);

/**
 * @brief Force the deep-standby-wake flag (unit-test only)
 *
 * @details
 * Emulates the boot-path bridge from RSTSR* to rx_lpc_was_deep_standby_wake().
 * Must be called before rx_lpc_init() to be latched on the next init.
 *
 * @param[in] was_wake true to simulate a deep-standby-origin reset
 *
 * @since Version 1.0.0
 */
void rx_lpc_test_set_deep_standby_wake(bool was_wake);

/**
 * @brief Inject simulated wake flags (unit-test only)
 *
 * @details
 * Emulates DPSIFR0..DPSIFR3 contents as observed at reset. The value is
 * latched on the next rx_lpc_init() and cleared afterwards, mirroring the
 * real hardware (which requires the caller to clear DPSIFR* on restart).
 *
 * @param[in] flags 32-bit wake-flag bitmask to inject
 *
 * @since Version 1.0.0
 */
void rx_lpc_test_set_pending_wake_flags(uint32_t flags);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
