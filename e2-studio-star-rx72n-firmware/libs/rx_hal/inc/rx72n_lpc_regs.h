/* lib/rx_hal/inc/rx72n_lpc_regs.h */

/**
 * @file rx72n_lpc_regs.h
 * @brief RX72N Low Power Consumption Register Definitions
 *
 * @details
 * Low Power Consumption (LPC) register definitions for operating power control,
 * deep software standby mode, and wake-up interrupt configuration on the RX72N
 * microcontroller. This header complements rx72n_system_regs.h which contains
 * the main standby (SBYCR) and module stop (MSTPCRA-D) registers.
 *
 * @par Register Groups
 * | Address Range             | Description                         |
 * |---------------------------|-------------------------------------|
 * | 0x000800A0 - 0x000800A1   | Operating power control (OPCCR/RSTCKCR) |
 * | 0x0008C280 - 0x0008C28D   | Deep standby control & interrupt    |
 * | 0x0008C2A0 - 0x0008C2BF   | Deep standby backup RAM (32 bytes)  |
 *
 * @par Low Power Modes
 * The RX72N supports several low power modes:
 * - **Sleep Mode**: CPU stopped, peripherals run (lowest latency wakeup)
 * - **All-Module Clock Stop Mode**: CPU and all modules stopped
 * - **Software Standby Mode**: Most oscillators stopped, RAM retained
 * - **Deep Software Standby Mode**: Lowest power, backup RAM only
 *
 * @par Operating Power Control Modes
 * | Mode                 | ICLK Max  | Description              |
 * |----------------------|-----------|--------------------------|
 * | High-speed (000b)    | 240 MHz   | Full performance         |
 * | Low-speed 1 (110b)   | 1 MHz     | Low power, PLL off       |
 * | Low-speed 2 (111b)   | 264 kHz   | Lowest power, HOCO off   |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 3: No dynamic memory allocation
 * - Rule 8: All constants use C23 typed enums
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par Manual References
 * - Chapter 11: Low Power Consumption
 * - Table 11.1: Low Power Consumption Functions
 * - Table 11.2: Mode Transitions
 *
 * @par Verification Status
 * VERIFIED (2026-01-29) - All register addresses verified against
 * RX72N Manual Rev.1.20, Chapter 11.
 *
 * @see rx72n_system_regs.h Main system registers (SBYCR, MSTPCRA-D)
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Address Definitions
 * =============================================================================
 */

/**
 * @enum rx_lpc_addresses_t
 * @brief Low Power Consumption register addresses
 *
 * @details
 * Addresses for operating power control and deep standby registers.
 * These registers control low power mode behavior and wake-up sources.
 *
 * @par Memory Map
 * | Address      | Register       | Size   | Description                    |
 * |--------------|----------------|--------|--------------------------------|
 * | 0x000800A0   | OPCCR          | 8-bit  | Operating Power Control        |
 * | 0x000800A1   | RSTCKCR        | 8-bit  | Sleep Mode Return Clock Switch |
 * | 0x0008C280   | DPSBYCR        | 8-bit  | Deep Standby Control           |
 * | 0x0008C282-5 | DPSIER0-3      | 8-bit  | Deep Standby Int Enable        |
 * | 0x0008C286-9 | DPSIFR0-3      | 8-bit  | Deep Standby Int Flags         |
 * | 0x0008C28A-D | DPSIEGR0-3     | 8-bit  | Deep Standby Int Edge          |
 * | 0x0008C2A0-BF| DPSBKRy        | 8-bit  | Deep Standby Backup RAM        |
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* Operating Power Control Registers @ 0x000800Ax */
  k_opccr_addr   = 0x000800A0U, /**< Operating Power Control Register */
  k_rstckcr_addr = 0x000800A1U, /**< Sleep Mode Return Clock Switch Register */

  /* Deep Standby Registers @ 0x0008C28x */
  k_dpsbycr_base_addr = 0x0008C280U, /**< Deep Standby Control base */
  k_dpsbycr_addr      = 0x0008C280U, /**< Deep Standby Control Register */
  k_dpsier0_addr      = 0x0008C282U, /**< Deep Standby Interrupt Enable 0 */
  k_dpsier1_addr      = 0x0008C283U, /**< Deep Standby Interrupt Enable 1 */
  k_dpsier2_addr      = 0x0008C284U, /**< Deep Standby Interrupt Enable 2 */
  k_dpsier3_addr      = 0x0008C285U, /**< Deep Standby Interrupt Enable 3 */
  k_dpsifr0_addr      = 0x0008C286U, /**< Deep Standby Interrupt Flag 0 */
  k_dpsifr1_addr      = 0x0008C287U, /**< Deep Standby Interrupt Flag 1 */
  k_dpsifr2_addr      = 0x0008C288U, /**< Deep Standby Interrupt Flag 2 */
  k_dpsifr3_addr      = 0x0008C289U, /**< Deep Standby Interrupt Flag 3 */
  k_dpsiegr0_addr     = 0x0008C28AU, /**< Deep Standby Interrupt Edge 0 */
  k_dpsiegr1_addr     = 0x0008C28BU, /**< Deep Standby Interrupt Edge 1 */
  k_dpsiegr2_addr     = 0x0008C28CU, /**< Deep Standby Interrupt Edge 2 */
  k_dpsiegr3_addr     = 0x0008C28DU, /**< Deep Standby Interrupt Edge 3 */

  /* Deep Standby Backup RAM @ 0x0008C2A0-0x0008C2BF (32 bytes) */
  k_dpsbkr_base_addr = 0x0008C2A0U, /**< Deep Standby Backup RAM base */
} rx_lpc_addresses_t;

/**
 * @enum rx_lpc_sizes_t
 * @brief Low Power Consumption register and buffer sizes
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsbkr_size    = 32U, /**< Deep standby backup RAM size (bytes) */
  k_dps_regs_count = 4U,  /**< Number of DPSIERx/DPSIFRx/DPSIEGRx registers */
} rx_lpc_sizes_t;

/* =============================================================================
 * OPCCR - Operating Power Control Register (@ 0x000800A0)
 * =============================================================================
 */

/**
 * @enum rx_opccr_bits_t
 * @brief OPCCR bit definitions
 *
 * @details
 * Operating Power Control Register controls operating power mode for
 * normal operation, sleep mode, and all-module clock stop mode.
 *
 * @par Operating Power Control Modes (OPCM[2:0])
 * | Value | Mode           | ICLK Max  | Notes                    |
 * |-------|----------------|-----------|--------------------------|
 * | 000b  | High-speed     | 240 MHz   | Full performance         |
 * | 110b  | Low-speed 1    | 1 MHz     | PLL must be stopped      |
 * | 111b  | Low-speed 2    | 264 kHz   | HOCO/PLL must be stopped |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* OPCM[2:0] - Operating Power Control Mode Select (bits 2:0) */
  k_opccr_opcm_highspeed = 0x00U, /**< High-speed operating mode (240 MHz max) */
  k_opccr_opcm_lowspeed1 = 0x06U, /**< Low-speed operating mode 1 (1 MHz max) */
  k_opccr_opcm_lowspeed2 = 0x07U, /**< Low-speed operating mode 2 (264 kHz max) */
  k_opccr_opcm_mask      = 0x07U, /**< OPCM[2:0] bit mask */

  /* OPCMTSF - Operating Power Control Mode Transition Status Flag (bit 4) */
  k_opccr_opcmtsf = 0x10U, /**< 1 = Mode transition in progress */
} rx_opccr_bits_t;

/* =============================================================================
 * RSTCKCR - Sleep Mode Return Clock Source Switching Register (@ 0x000800A1)
 * =============================================================================
 */

/**
 * @enum rx_rstckcr_bits_t
 * @brief RSTCKCR bit definitions
 *
 * @details
 * Controls clock source switching at the time of release from sleep mode.
 * When enabled, the selected clock source is automatically started
 * upon wake-up from sleep mode.
 *
 * @par RSTCKSEL[2:0] - Clock Source Select
 * | Value | Clock Source              |
 * |-------|---------------------------|
 * | 001b  | HOCO (High-speed on-chip) |
 * | 010b  | Main clock oscillator     |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* RSTCKSEL[2:0] - Sleep Mode Return Clock Source Select (bits 2:0) */
  k_rstckcr_rstcksel_hoco = 0x01U, /**< HOCO selected for wake-up */
  k_rstckcr_rstcksel_main = 0x02U, /**< Main oscillator selected for wake-up */
  k_rstckcr_rstcksel_mask = 0x07U, /**< RSTCKSEL[2:0] bit mask */

  /* RSTCKEN - Sleep Mode Return Clock Source Switching Enable (bit 7) */
  k_rstckcr_rstcken = 0x80U, /**< 1 = Enable clock source switching */
} rx_rstckcr_bits_t;

/* =============================================================================
 * DPSBYCR - Deep Standby Control Register (@ 0x0008C280)
 * =============================================================================
 */

/**
 * @enum rx_dpsbycr_bits_t
 * @brief DPSBYCR bit definitions
 *
 * @details
 * Controls deep software standby mode entry and power supply configuration.
 *
 * @par DEEPCUT[1:0] - Power Supply Control
 * | Value | Standby RAM | USB Resume | LVD    | Notes              |
 * |-------|-------------|------------|--------|--------------------|
 * | 00b   | Powered     | Enabled    | Active | USB wake-up OK     |
 * | 01b   | Not powered | Disabled   | Active | RAM data lost      |
 * | 11b   | Not powered | Disabled   | Off    | Lowest power       |
 *
 * @note 10b is a prohibited setting for DEEPCUT[1:0].
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* DEEPCUT[1:0] - Deep Cut Power Control (bits 1:0) */
  k_dpsbycr_deepcut_ram_usb_on  = 0x00U, /**< Standby RAM & USB powered */
  k_dpsbycr_deepcut_ram_usb_off = 0x01U, /**< Standby RAM & USB not powered */
  k_dpsbycr_deepcut_lvd_off     = 0x03U, /**< LVD stopped, lowest power */
  k_dpsbycr_deepcut_mask        = 0x03U, /**< DEEPCUT[1:0] bit mask */

  /* IOKEEP - I/O Port Retention (bit 6) */
  k_dpsbycr_iokeep = 0x40U, /**< 1 = Retain I/O state after deep standby exit */

  /* DPSBY - Deep Software Standby (bit 7) */
  k_dpsbycr_dpsby = 0x80U, /**< 1 = Enter deep standby (with SBYCR.SSBY=1) */
} rx_dpsbycr_bits_t;

/* =============================================================================
 * DPSIER0 - Deep Standby Interrupt Enable Register 0 (@ 0x0008C282)
 * =============================================================================
 */

/**
 * @enum rx_dpsier0_bits_t
 * @brief DPSIER0 bit definitions (IRQ0-7 wake-up enable)
 *
 * @details
 * Enables external IRQ0-DS to IRQ7-DS pins as wake-up sources from
 * deep software standby mode.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsier0_dirq0e = 0x01U, /**< IRQ0-DS pin enable */
  k_dpsier0_dirq1e = 0x02U, /**< IRQ1-DS pin enable */
  k_dpsier0_dirq2e = 0x04U, /**< IRQ2-DS pin enable */
  k_dpsier0_dirq3e = 0x08U, /**< IRQ3-DS pin enable */
  k_dpsier0_dirq4e = 0x10U, /**< IRQ4-DS pin enable */
  k_dpsier0_dirq5e = 0x20U, /**< IRQ5-DS pin enable */
  k_dpsier0_dirq6e = 0x40U, /**< IRQ6-DS pin enable */
  k_dpsier0_dirq7e = 0x80U, /**< IRQ7-DS pin enable */
} rx_dpsier0_bits_t;

/* =============================================================================
 * DPSIER1 - Deep Standby Interrupt Enable Register 1 (@ 0x0008C283)
 * =============================================================================
 */

/**
 * @enum rx_dpsier1_bits_t
 * @brief DPSIER1 bit definitions (IRQ8-15 wake-up enable)
 *
 * @details
 * Enables external IRQ8-DS to IRQ15-DS pins as wake-up sources from
 * deep software standby mode.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsier1_dirq8e  = 0x01U, /**< IRQ8-DS pin enable */
  k_dpsier1_dirq9e  = 0x02U, /**< IRQ9-DS pin enable */
  k_dpsier1_dirq10e = 0x04U, /**< IRQ10-DS pin enable */
  k_dpsier1_dirq11e = 0x08U, /**< IRQ11-DS pin enable */
  k_dpsier1_dirq12e = 0x10U, /**< IRQ12-DS pin enable */
  k_dpsier1_dirq13e = 0x20U, /**< IRQ13-DS pin enable */
  k_dpsier1_dirq14e = 0x40U, /**< IRQ14-DS pin enable */
  k_dpsier1_dirq15e = 0x80U, /**< IRQ15-DS pin enable */
} rx_dpsier1_bits_t;

/* =============================================================================
 * DPSIER2 - Deep Standby Interrupt Enable Register 2 (@ 0x0008C284)
 * =============================================================================
 */

/**
 * @enum rx_dpsier2_bits_t
 * @brief DPSIER2 bit definitions (LVD, RTC, NMI, I2C, USB wake-up enable)
 *
 * @details
 * Enables various peripheral wake-up sources from deep software standby mode.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsier2_dlvd1ie  = 0x01U, /**< LVD1 (Voltage Monitor 1) enable */
  k_dpsier2_dlvd2ie  = 0x02U, /**< LVD2 (Voltage Monitor 2) enable */
  k_dpsier2_drtciie  = 0x04U, /**< RTC periodic interrupt enable */
  k_dpsier2_drtcaie  = 0x08U, /**< RTC alarm interrupt enable */
  k_dpsier2_dnmie    = 0x10U, /**< NMI pin enable (write-once) */
  k_dpsier2_driicdie = 0x20U, /**< SDA2-DS (I2C data) enable */
  k_dpsier2_driiccie = 0x40U, /**< SCL2-DS (I2C clock) enable */
  k_dpsier2_dusbie   = 0x80U, /**< USB suspend/resume enable */
} rx_dpsier2_bits_t;

/* =============================================================================
 * DPSIER3 - Deep Standby Interrupt Enable Register 3 (@ 0x0008C285)
 * =============================================================================
 */

/**
 * @enum rx_dpsier3_bits_t
 * @brief DPSIER3 bit definitions (CAN wake-up enable)
 *
 * @details
 * Enables CAN receive pin as wake-up source from deep software standby mode.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsier3_dcanie = 0x01U, /**< CRX1-DS (CAN1 receive) enable */
} rx_dpsier3_bits_t;

/* =============================================================================
 * DPSIFR0 - Deep Standby Interrupt Flag Register 0 (@ 0x0008C286)
 * =============================================================================
 */

/**
 * @enum rx_dpsifr0_bits_t
 * @brief DPSIFR0 bit definitions (IRQ0-7 wake-up flags)
 *
 * @details
 * Indicates which IRQ0-DS to IRQ7-DS pin triggered wake-up from
 * deep software standby mode. Write 0 after reading 1 to clear.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsifr0_dirq0f = 0x01U, /**< IRQ0-DS wake-up flag */
  k_dpsifr0_dirq1f = 0x02U, /**< IRQ1-DS wake-up flag */
  k_dpsifr0_dirq2f = 0x04U, /**< IRQ2-DS wake-up flag */
  k_dpsifr0_dirq3f = 0x08U, /**< IRQ3-DS wake-up flag */
  k_dpsifr0_dirq4f = 0x10U, /**< IRQ4-DS wake-up flag */
  k_dpsifr0_dirq5f = 0x20U, /**< IRQ5-DS wake-up flag */
  k_dpsifr0_dirq6f = 0x40U, /**< IRQ6-DS wake-up flag */
  k_dpsifr0_dirq7f = 0x80U, /**< IRQ7-DS wake-up flag */
} rx_dpsifr0_bits_t;

/* =============================================================================
 * DPSIFR1 - Deep Standby Interrupt Flag Register 1 (@ 0x0008C287)
 * =============================================================================
 */

/**
 * @enum rx_dpsifr1_bits_t
 * @brief DPSIFR1 bit definitions (IRQ8-15 wake-up flags)
 *
 * @details
 * Indicates which IRQ8-DS to IRQ15-DS pin triggered wake-up from
 * deep software standby mode. Write 0 after reading 1 to clear.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsifr1_dirq8f  = 0x01U, /**< IRQ8-DS wake-up flag */
  k_dpsifr1_dirq9f  = 0x02U, /**< IRQ9-DS wake-up flag */
  k_dpsifr1_dirq10f = 0x04U, /**< IRQ10-DS wake-up flag */
  k_dpsifr1_dirq11f = 0x08U, /**< IRQ11-DS wake-up flag */
  k_dpsifr1_dirq12f = 0x10U, /**< IRQ12-DS wake-up flag */
  k_dpsifr1_dirq13f = 0x20U, /**< IRQ13-DS wake-up flag */
  k_dpsifr1_dirq14f = 0x40U, /**< IRQ14-DS wake-up flag */
  k_dpsifr1_dirq15f = 0x80U, /**< IRQ15-DS wake-up flag */
} rx_dpsifr1_bits_t;

/* =============================================================================
 * DPSIFR2 - Deep Standby Interrupt Flag Register 2 (@ 0x0008C288)
 * =============================================================================
 */

/**
 * @enum rx_dpsifr2_bits_t
 * @brief DPSIFR2 bit definitions (LVD, RTC, NMI, I2C, USB wake-up flags)
 *
 * @details
 * Indicates which peripheral triggered wake-up from deep software
 * standby mode. Write 0 after reading 1 to clear.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsifr2_dlvd1if  = 0x01U, /**< LVD1 wake-up flag */
  k_dpsifr2_dlvd2if  = 0x02U, /**< LVD2 wake-up flag */
  k_dpsifr2_drtciif  = 0x04U, /**< RTC periodic interrupt flag */
  k_dpsifr2_drtcaif  = 0x08U, /**< RTC alarm interrupt flag */
  k_dpsifr2_dnmif    = 0x10U, /**< NMI wake-up flag */
  k_dpsifr2_driicdif = 0x20U, /**< SDA2-DS wake-up flag */
  k_dpsifr2_driiccif = 0x40U, /**< SCL2-DS wake-up flag */
  k_dpsifr2_dusbif   = 0x80U, /**< USB suspend/resume flag */
} rx_dpsifr2_bits_t;

/* =============================================================================
 * DPSIFR3 - Deep Standby Interrupt Flag Register 3 (@ 0x0008C289)
 * =============================================================================
 */

/**
 * @enum rx_dpsifr3_bits_t
 * @brief DPSIFR3 bit definitions (CAN wake-up flag)
 *
 * @details
 * Indicates CAN receive triggered wake-up from deep software standby mode.
 * Write 0 after reading 1 to clear.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsifr3_dcanif = 0x01U, /**< CRX1-DS wake-up flag */
} rx_dpsifr3_bits_t;

/* =============================================================================
 * DPSIEGR0 - Deep Standby Interrupt Edge Register 0 (@ 0x0008C28A)
 * =============================================================================
 */

/**
 * @enum rx_dpsiegr0_bits_t
 * @brief DPSIEGR0 bit definitions (IRQ0-7 edge select)
 *
 * @details
 * Selects edge polarity for IRQ0-DS to IRQ7-DS wake-up detection.
 * 0 = Falling edge, 1 = Rising edge.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsiegr0_dirq0eg = 0x01U, /**< IRQ0-DS edge select */
  k_dpsiegr0_dirq1eg = 0x02U, /**< IRQ1-DS edge select */
  k_dpsiegr0_dirq2eg = 0x04U, /**< IRQ2-DS edge select */
  k_dpsiegr0_dirq3eg = 0x08U, /**< IRQ3-DS edge select */
  k_dpsiegr0_dirq4eg = 0x10U, /**< IRQ4-DS edge select */
  k_dpsiegr0_dirq5eg = 0x20U, /**< IRQ5-DS edge select */
  k_dpsiegr0_dirq6eg = 0x40U, /**< IRQ6-DS edge select */
  k_dpsiegr0_dirq7eg = 0x80U, /**< IRQ7-DS edge select */
} rx_dpsiegr0_bits_t;

/* =============================================================================
 * DPSIEGR1 - Deep Standby Interrupt Edge Register 1 (@ 0x0008C28B)
 * =============================================================================
 */

/**
 * @enum rx_dpsiegr1_bits_t
 * @brief DPSIEGR1 bit definitions (IRQ8-15 edge select)
 *
 * @details
 * Selects edge polarity for IRQ8-DS to IRQ15-DS wake-up detection.
 * 0 = Falling edge, 1 = Rising edge.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsiegr1_dirq8eg  = 0x01U, /**< IRQ8-DS edge select */
  k_dpsiegr1_dirq9eg  = 0x02U, /**< IRQ9-DS edge select */
  k_dpsiegr1_dirq10eg = 0x04U, /**< IRQ10-DS edge select */
  k_dpsiegr1_dirq11eg = 0x08U, /**< IRQ11-DS edge select */
  k_dpsiegr1_dirq12eg = 0x10U, /**< IRQ12-DS edge select */
  k_dpsiegr1_dirq13eg = 0x20U, /**< IRQ13-DS edge select */
  k_dpsiegr1_dirq14eg = 0x40U, /**< IRQ14-DS edge select */
  k_dpsiegr1_dirq15eg = 0x80U, /**< IRQ15-DS edge select */
} rx_dpsiegr1_bits_t;

/* =============================================================================
 * DPSIEGR2 - Deep Standby Interrupt Edge Register 2 (@ 0x0008C28C)
 * =============================================================================
 */

/**
 * @enum rx_dpsiegr2_bits_t
 * @brief DPSIEGR2 bit definitions (LVD, NMI, I2C edge select)
 *
 * @details
 * Selects edge polarity for various peripheral wake-up detection.
 * For LVD: 0 = VCC fall (VCC < Vdet), 1 = VCC rise (VCC >= Vdet).
 * For pins: 0 = Falling edge, 1 = Rising edge.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsiegr2_dlvd1eg  = 0x01U, /**< LVD1 edge: 0=fall, 1=rise */
  k_dpsiegr2_dlvd2eg  = 0x02U, /**< LVD2 edge: 0=fall, 1=rise */
  k_dpsiegr2_dnmieg   = 0x10U, /**< NMI edge select */
  k_dpsiegr2_driicdeg = 0x20U, /**< SDA2-DS edge select */
  k_dpsiegr2_driicceg = 0x40U, /**< SCL2-DS edge select */
} rx_dpsiegr2_bits_t;

/* =============================================================================
 * DPSIEGR3 - Deep Standby Interrupt Edge Register 3 (@ 0x0008C28D)
 * =============================================================================
 */

/**
 * @enum rx_dpsiegr3_bits_t
 * @brief DPSIEGR3 bit definitions (CAN edge select)
 *
 * @details
 * Selects edge polarity for CAN receive wake-up detection.
 * 0 = Falling edge, 1 = Rising edge.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dpsiegr3_dcanieg = 0x01U, /**< CRX1-DS edge select */
} rx_dpsiegr3_bits_t;

/* =============================================================================
 * Deep Standby Register Structure (@ 0x0008C280-0x0008C28D)
 * =============================================================================
 */

/**
 * @struct rx_dps_regs_t
 * @brief Deep Standby Register Block (non-contiguous)
 *
 * @details
 * The deep standby registers are NOT all contiguous in memory:
 * - 0x0008C280: DPSBYCR
 * - 0x0008C281: Reserved
 * - 0x0008C282-0x0008C28D: DPSIER, DPSIFR, DPSIEGR registers
 *
 * This structure covers the contiguous portion from 0x0008C280 onwards.
 *
 * @invariant sizeof(rx_dps_regs_t) == 14 bytes
 *
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  volatile uint8_t dpsbycr;   /**< Deep Standby Control @ 0x00 (0x0008C280) */
  uint8_t          reserved1; /**< Reserved @ 0x01 */
  volatile uint8_t dpsier0;   /**< Deep Standby Int Enable 0 @ 0x02 (0x0008C282) */
  volatile uint8_t dpsier1;   /**< Deep Standby Int Enable 1 @ 0x03 (0x0008C283) */
  volatile uint8_t dpsier2;   /**< Deep Standby Int Enable 2 @ 0x04 (0x0008C284) */
  volatile uint8_t dpsier3;   /**< Deep Standby Int Enable 3 @ 0x05 (0x0008C285) */
  volatile uint8_t dpsifr0;   /**< Deep Standby Int Flag 0 @ 0x06 (0x0008C286) */
  volatile uint8_t dpsifr1;   /**< Deep Standby Int Flag 1 @ 0x07 (0x0008C287) */
  volatile uint8_t dpsifr2;   /**< Deep Standby Int Flag 2 @ 0x08 (0x0008C288) */
  volatile uint8_t dpsifr3;   /**< Deep Standby Int Flag 3 @ 0x09 (0x0008C289) */
  volatile uint8_t dpsiegr0;  /**< Deep Standby Int Edge 0 @ 0x0A (0x0008C28A) */
  volatile uint8_t dpsiegr1;  /**< Deep Standby Int Edge 1 @ 0x0B (0x0008C28B) */
  volatile uint8_t dpsiegr2;  /**< Deep Standby Int Edge 2 @ 0x0C (0x0008C28C) */
  volatile uint8_t dpsiegr3;  /**< Deep Standby Int Edge 3 @ 0x0D (0x0008C28D) */
} rx_dps_regs_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to OPCCR register
 *
 * @return Volatile pointer to Operating Power Control Register
 *
 * @details
 * OPCCR controls operating power mode for power optimization.
 *
 * @par Example - Enter Low-Speed Mode 1:
 * @code
 * // Switch to LOCO, then enter low-speed mode 1
 * *opccr_reg() = k_opccr_opcm_lowspeed1;
 * while (*opccr_reg() & k_opccr_opcmtsf) { }  // Wait for transition
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile uint8_t* opccr_reg(void)
{
  return (volatile uint8_t*)k_opccr_addr;
}

/**
 * @brief Get pointer to RSTCKCR register
 *
 * @return Volatile pointer to Sleep Mode Return Clock Source Register
 *
 * @details
 * RSTCKCR controls automatic clock source switching on sleep mode exit.
 *
 * @par Example - Auto-switch to HOCO on wake-up:
 * @code
 * *rstckcr_reg() = k_rstckcr_rstcken | k_rstckcr_rstcksel_hoco;
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile uint8_t* rstckcr_reg(void)
{
  return (volatile uint8_t*)k_rstckcr_addr;
}

/**
 * @brief Get pointer to Deep Standby registers
 *
 * @return Volatile pointer to Deep Standby register structure
 *
 * @details
 * Provides access to DPSBYCR, DPSIERx, DPSIFRx, and DPSIEGRx registers.
 *
 * @par Example - Configure IRQ0 as wake-up source:
 * @code
 * volatile rx_dps_regs_t* dps = dps_regs();
 *
 * dps->dpsier0 = k_dpsier0_dirq0e;      // Enable IRQ0-DS wake-up
 * dps->dpsiegr0 = k_dpsiegr0_dirq0eg;   // Rising edge trigger
 * dps->dpsifr0 = 0x00;                  // Clear flags before entering
 *
 * dps->dpsbycr = k_dpsbycr_dpsby;       // Enable deep standby
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile rx_dps_regs_t* dps_regs(void)
{
  return (volatile rx_dps_regs_t*)k_dpsbycr_base_addr;
}

/**
 * @brief Get pointer to Deep Standby Backup RAM
 *
 * @return Volatile pointer to 32-byte backup RAM array
 *
 * @details
 * DPSBKRy (y=0-31) is 32 bytes of RAM retained during deep software
 * standby mode. Use this to preserve critical state across deep sleep.
 *
 * @warning Backup RAM is undefined after power-on; initialize before use.
 *
 * @par Example - Save/restore state across deep standby:
 * @code
 * // Before entering deep standby
 * volatile uint8_t* bkram = dpsbkr();
 * bkram[0] = magic_byte_1;
 * bkram[1] = magic_byte_2;
 * bkram[2] = (uint8_t)(counter >> 8);
 * bkram[3] = (uint8_t)(counter & 0xFF);
 *
 * // After waking from deep standby
 * if (bkram[0] == magic_byte_1 && bkram[1] == magic_byte_2) {
 *     counter = ((uint16_t)bkram[2] << 8) | bkram[3];
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dpsbkr(void)
{
  return (volatile uint8_t*)k_dpsbkr_base_addr;
}

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify OPCCR/RSTCKCR addresses (Ch11 Section 11.2.6/11.2.7) */
static_assert(k_opccr_addr == 0x000800A0, "OPCCR address incorrect (expected 0x000800A0)");
static_assert(k_rstckcr_addr == 0x000800A1, "RSTCKCR address incorrect (expected 0x000800A1)");

/* Verify Deep Standby Control Register address (Ch11 Section 11.2.8) */
static_assert(k_dpsbycr_addr == 0x0008C280, "DPSBYCR address incorrect (expected 0x0008C280)");

/* Verify DPSIER addresses (Ch11 Sections 11.2.9-11.2.12) */
static_assert(k_dpsier0_addr == 0x0008C282, "DPSIER0 address incorrect (expected 0x0008C282)");
static_assert(k_dpsier1_addr == 0x0008C283, "DPSIER1 address incorrect (expected 0x0008C283)");
static_assert(k_dpsier2_addr == 0x0008C284, "DPSIER2 address incorrect (expected 0x0008C284)");
static_assert(k_dpsier3_addr == 0x0008C285, "DPSIER3 address incorrect (expected 0x0008C285)");

/* Verify DPSIFR addresses (Ch11 Sections 11.2.13-11.2.16) */
static_assert(k_dpsifr0_addr == 0x0008C286, "DPSIFR0 address incorrect (expected 0x0008C286)");
static_assert(k_dpsifr1_addr == 0x0008C287, "DPSIFR1 address incorrect (expected 0x0008C287)");
static_assert(k_dpsifr2_addr == 0x0008C288, "DPSIFR2 address incorrect (expected 0x0008C288)");
static_assert(k_dpsifr3_addr == 0x0008C289, "DPSIFR3 address incorrect (expected 0x0008C289)");

/* Verify DPSIEGR addresses (Ch11 Sections 11.2.17-11.2.20) */
static_assert(k_dpsiegr0_addr == 0x0008C28A, "DPSIEGR0 address incorrect (expected 0x0008C28A)");
static_assert(k_dpsiegr1_addr == 0x0008C28B, "DPSIEGR1 address incorrect (expected 0x0008C28B)");
static_assert(k_dpsiegr2_addr == 0x0008C28C, "DPSIEGR2 address incorrect (expected 0x0008C28C)");
static_assert(k_dpsiegr3_addr == 0x0008C28D, "DPSIEGR3 address incorrect (expected 0x0008C28D)");

/* Verify Deep Standby Backup RAM address (Ch11 Section 11.2.21) */
static_assert(k_dpsbkr_base_addr == 0x0008C2A0,
              "DPSBKR base address incorrect (expected 0x0008C2A0)");

/* Verify structure size and offsets */
static_assert(sizeof(rx_dps_regs_t) == 14, "rx_dps_regs_t size incorrect (expected 14 bytes)");
static_assert(offsetof(rx_dps_regs_t, dpsbycr) == 0, "DPSBYCR offset incorrect (expected 0)");
static_assert(offsetof(rx_dps_regs_t, dpsier0) == 2, "DPSIER0 offset incorrect (expected 2)");
static_assert(offsetof(rx_dps_regs_t, dpsifr0) == 6, "DPSIFR0 offset incorrect (expected 6)");
static_assert(offsetof(rx_dps_regs_t, dpsiegr0) == 10, "DPSIEGR0 offset incorrect (expected 10)");
static_assert(offsetof(rx_dps_regs_t, dpsiegr3) == 13, "DPSIEGR3 offset incorrect (expected 13)");

/* Verify address relationships */
static_assert((k_dpsier0_addr - k_dpsbycr_addr) == 2, "DPSIER0 must be at offset 2 from DPSBYCR");
static_assert((k_dpsifr0_addr - k_dpsbycr_addr) == 6, "DPSIFR0 must be at offset 6 from DPSBYCR");
static_assert((k_dpsiegr0_addr - k_dpsbycr_addr) == 10,
              "DPSIEGR0 must be at offset 10 from DPSBYCR");
static_assert((k_dpsbkr_base_addr - k_dpsbycr_addr) == 0x20,
              "DPSBKR must be at offset 0x20 from DPSBYCR");

#ifdef __cplusplus
}
#endif
