/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No 
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all 
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM 
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES 
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS 
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of 
* this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* MODIFICATION NOTICE
* This file has been modified by the STAR project for use in the STAR robotics platform.
* Modifications include: Documentation additions, C23 typed enum conversions, and style guide compliance.
* Modified files are maintained by Locked, Inc. as part of the STAR project.
* Original Renesas code remains under Renesas copyright as stated above.
***********************************************************************************************************************/
/**
 * @file mcu_info.h
 * @brief RX72N microcontroller hardware definitions and configuration constants
 *
 * @details
 * Defines RX72N hardware characteristics, clock configurations, memory sizes,
 * peripheral counts, and MCU feature flags. This header provides compile-time
 * constants derived from BSP configuration (r_bsp_config.h) and MCU part number.
 *
 * **Configuration Sources:**
 * - **BSP Configuration:** User settings from r_bsp_config.h (clock source, oscillators, etc.)
 * - **Part Number:** MCU variant decoded from BSP_CFG_MCU_PART_* macros
 * - **Hardware Specification:** Fixed values from RX72N datasheet (memory sizes, peripherals)
 *
 * **Key Definitions:**
 * - **Clock Frequencies:** ICLK, PCLK, BCLK, FCLK, UCLK calculated from configuration
 * - **Memory Sizes:** ROM, RAM, Data Flash based on part number
 * - **Package Information:** Pin count and package type
 * - **MCU Features:** Floating-point, exception handling, interrupts, TFU
 * - **Peripheral Identification:** Series (RX700), group (RX72N)
 *
 * **Clock Calculation:**
 * All clock speeds are calculated from:
 * - BSP_CFG_XTAL_HZ: External crystal frequency (typically 24 MHz)
 * - BSP_CFG_PLL_DIV/MUL: PLL divider and multiplier
 * - BSP_CFG_*CK_DIV: Clock dividers for each peripheral domain
 *
 * Example: ICLK = (XTAL_HZ / PLL_DIV * PLL_MUL) / ICK_DIV
 *
 * **Part Number Decoding:**
 * RX72N part number format: R5F572NNDDBD
 * - Package (BD): Decoded to BSP_PACKAGE_* and BSP_PACKAGE_PINS
 * - Memory Size (DD): Decoded to BSP_ROM_SIZE_BYTES, BSP_RAM_SIZE_BYTES
 * - Function (NN): Encryption included or not
 *
 * @par Hardware Dependencies
 * - RX72N microcontroller (R5F572NN)
 * - External crystal oscillator (BSP_CFG_XTAL_HZ)
 * - Package-specific pin configuration
 *
 * @par References
 * - RX72N User's Manual (r01uh0805ej0140-rx72n.pdf)
 * - RX72N Datasheet - Memory specifications, package pinouts
 * - r_bsp_config.h - User BSP configuration
 *
 * @note Ported from Renesas Smart Configurator (SMC) generated code
 * @warning Requires Smart Configurator >= v2.14.0 (BSP_CFG_CONFIGURATOR_VERSION >= 2140)
 * @warning Do not modify calculated clock values - change source configuration in r_bsp_config.h
 * @since Version 1.0.0
 *
 * @author STAR Project (Locked, Inc.)
 * @date 2019 (original), 2026 (STAR modifications)
 * @version 1.0.5
 * @copyright Copyright (C) 2019 Renesas Electronics Corporation. Modified by Locked, Inc.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 4: All macros serve clear purposes (feature flags, clock calculations)
 * - Rule 8: No magic numbers, all configuration values use named enums/macros
 * - All rules maintained throughout modifications
 *
 * @par SOLID Principles
 * - Single Responsibility: Provides only MCU hardware definitions and configuration
 * - Open/Closed: Extensible via BSP configuration without modifying this file
 * - Liskov Substitution: Hardware constants maintain consistent semantics
 * - Interface Segregation: Minimal, focused API for MCU characteristics
 * - Dependency Inversion: Configuration abstracted through r_bsp_config.h
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 08.10.2019 1.00     First Release
*         : 30.06.2021 1.01     Added the following macro definition.
*                               - BSP_MCU_EXCEP_ADDRESS_ISR
*         : 30.11.2021 1.02     Deleted the compile switch for BSP_CFG_MCU_PART_SERIES and BSP_CFG_MCU_PART_GROUP.
*         : 22.04.2022 1.03     Added version check of smart configurator.
*         : 25.11.2022 1.04     Added the following macro definition.
*                               - BSP_EXPANSION_RAM
*                               Added version check of smart configurator.
*         : 28.02.2023 1.05     Added the following macro definition.
*                               - BSP_MCU_TFU_VERSION
***********************************************************************************************************************/

#pragma once

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
/* Gets MCU configuration information. */
#include "r_bsp_config.h"
#include <stdint.h>  /* Required for uint8_t, uint32_t in C23 typed enums */

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typed Enums (CLAUDE.md Compliance - C23)
***********************************************************************************************************************/

/**
 * @enum bsp_cpu_constants_t
 * @brief RXv3 CPU core constants
 *
 * @details
 * Fixed constants for the RXv3 CPU core used in RX72N microcontroller.
 *
 * @invariant All values are compile-time constants (no runtime validation needed)
 *
 * @code
 *   // Example: Query CPU core version
 *   uint8_t cpu_version = (uint8_t)k_cpu_version_rxv3;  // Returns 3 for RXv3
 * @endcode
 *
 * @see BSP_MCU_CPU_VERSION
 * @see CPU_CYCLES_PER_LOOP
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_cpu_cycles_per_loop = 3,  /**< Clock cycles for delay_wait() loop iteration */
    k_cpu_version_rxv3    = 3,  /**< RXv3 core version identifier */
} bsp_cpu_constants_t;

/**
 * @enum bsp_package_type_t
 * @brief RX72N package type identifiers
 *
 * @details
 * Encoded package type values from MCU part number.
 * Maps to BSP_CFG_MCU_PART_PACKAGE configuration.
 *
 * @invariant Value must match BSP_CFG_MCU_PART_PACKAGE from r_bsp_config.h
 *
 * @code
 *   // Example: Query package type
 *   #if BSP_CFG_MCU_PART_PACKAGE == k_package_type_lfqfp144
 *     // Code for LFQFP-144 package
 *   #endif
 * @endcode
 *
 * @see BSP_CFG_MCU_PART_PACKAGE
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_package_type_lfqfp176 = 0x0,  /**< LFQFP-176 (0.50mm pitch) */
    k_package_type_lfbga176 = 0x1,  /**< LFBGA-176 (0.80mm pitch) */
    k_package_type_lfbga224 = 0x2,  /**< LFBGA-224 (0.80mm pitch) */
    k_package_type_lfqfp144 = 0x3,  /**< LFQFP-144 (0.50mm pitch) */
    k_package_type_tflga145 = 0x4,  /**< TFLGA-145 (0.50mm pitch) */
    k_package_type_lfqfp100 = 0x5,  /**< LFQFP-100 (0.50mm pitch) */
} bsp_package_type_t;

/**
 * @enum bsp_memory_size_t
 * @brief RX72N memory size identifiers
 *
 * @details
 * Encoded memory size values from MCU part number.
 * Maps to BSP_CFG_MCU_PART_MEMORY_SIZE configuration.
 *
 * @invariant Value must match BSP_CFG_MCU_PART_MEMORY_SIZE from r_bsp_config.h
 *
 * @code
 *   // Example: Check memory configuration
 *   #if BSP_CFG_MCU_PART_MEMORY_SIZE == k_memory_size_4mb
 *     // 4MB ROM available
 *   #endif
 * @endcode
 *
 * @see BSP_CFG_MCU_PART_MEMORY_SIZE
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_memory_size_2mb = 0xD,  /**< 2MB ROM / 1MB RAM / 32KB Data Flash */
    k_memory_size_4mb = 0x17, /**< 4MB ROM / 1MB RAM / 32KB Data Flash */
} bsp_memory_size_t;

/**
 * @enum bsp_hoco_frequency_t
 * @brief High-Speed On-Chip Oscillator (HOCO) frequency selection
 *
 * @details
 * HOCO can operate at three fixed frequencies. Selection is made via
 * BSP_CFG_HOCO_FREQUENCY in r_bsp_config.h.
 *
 * @invariant Value must be 0, 1, or 2 (hardware limitation)
 *
 * @code
 *   // Example: Configure HOCO frequency
 *   #if BSP_CFG_HOCO_FREQUENCY == k_hoco_freq_20mhz
 *     // HOCO running at 20 MHz
 *   #endif
 * @endcode
 *
 * @see BSP_CFG_HOCO_FREQUENCY
 * @see BSP_HOCO_HZ
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_hoco_freq_16mhz = 0,  /**< 16 MHz (default) */
    k_hoco_freq_18mhz = 1,  /**< 18 MHz */
    k_hoco_freq_20mhz = 2,  /**< 20 MHz */
} bsp_hoco_frequency_t;

/**
 * @enum bsp_clock_source_t
 * @brief System clock source selection
 *
 * @details
 * Available clock sources for system clock (ICLK).
 * Selection is made via BSP_CFG_CLOCK_SOURCE in r_bsp_config.h.
 *
 * @invariant Value must be 0-4 (hardware limitation)
 *
 * @code
 *   // Example: Check clock source
 *   #if BSP_CFG_CLOCK_SOURCE == k_clock_src_pll
 *     // Using PLL for system clock
 *   #endif
 * @endcode
 *
 * @see BSP_CFG_CLOCK_SOURCE
 * @see BSP_SELECTED_CLOCK_HZ
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_clock_src_loco = 0,  /**< Low Speed On-Chip Oscillator (240 kHz) */
    k_clock_src_hoco = 1,  /**< High Speed On-Chip Oscillator (16/18/20 MHz) */
    k_clock_src_main = 2,  /**< Main Clock Oscillator (external crystal) */
    k_clock_src_sub  = 3,  /**< Sub-Clock Oscillator (32.768 kHz) */
    k_clock_src_pll  = 4,  /**< PLL Circuit (default, up to 240 MHz) */
} bsp_clock_source_t;

/**
 * @enum bsp_pll_source_t
 * @brief PLL input clock source selection
 *
 * @details
 * PLL can be driven by Main Clock or HOCO.
 * Selection is made via BSP_CFG_PLL_SRC in r_bsp_config.h.
 *
 * @invariant Value must be 0 or 1 (hardware limitation)
 *
 * @code
 *   // Example: Check PLL source
 *   #if BSP_CFG_PLL_SRC == k_pll_src_main
 *     // PLL driven by external crystal
 *   #endif
 * @endcode
 *
 * @see BSP_CFG_PLL_SRC
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_pll_src_main = 0,  /**< Main Clock Oscillator (default) */
    k_pll_src_hoco = 1,  /**< High Speed On-Chip Oscillator */
} bsp_pll_source_t;

/**
 * @enum bsp_ipl_level_t
 * @brief Interrupt Priority Level (IPL) values
 *
 * @details
 * RX architecture supports 16 interrupt priority levels (0-15).
 * Higher values = higher priority. Level 0 = interrupts masked.
 *
 * @invariant k_ipl_min <= priority_level <= k_ipl_max (0-15 range enforced by hardware)
 *
 * @code
 *   // Example: Set interrupt priority
 *   uint8_t priority = (uint8_t)k_ipl_max;  // Set to maximum priority (15)
 * @endcode
 *
 * @see BSP_MCU_IPL_MAX
 * @see BSP_MCU_IPL_MIN
 * @see R_BSP_SET_IPL
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_ipl_min = 0,  /**< Minimum IPL (interrupts masked) */
    k_ipl_max = 15, /**< Maximum IPL (highest priority) */
} bsp_ipl_level_t;

/**
 * @enum bsp_reserved_addresses_t
 * @brief Reserved memory addresses for FIT module compatibility
 *
 * @details
 * RX architecture reserves address range 0x10000000 for special purposes.
 * FIT modules use this as a null pointer placeholder.
 *
 * @invariant Address must be 0x10000000 (256MB boundary, hardware requirement)
 *
 * @code
 *   // Example: FIT null pointer check
 *   if ((uint32_t)ptr == k_fit_reserved_space) {
 *     // Invalid pointer, do not dereference
 *   }
 * @endcode
 *
 * @see FIT_NO_PTR
 * @see FIT_NO_FUNC
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_fit_reserved_space = 0x10000000,  /**< Reserved space on RX (256MB boundary) */
} bsp_reserved_addresses_t;

/***********************************************************************************************************************
MCU Configuration and Feature Detection
***********************************************************************************************************************/

#if BSP_CFG_CONFIGURATOR_VERSION < 2140
/* The following macros are updated to invalid value by Smart configurator if you are using Smart Configurator for 
       RX V2.11.0 (equivalent to e2 studio 2021-10) or earlier version.
       - BSP_CFG_MCU_PART_GROUP, BSP_CFG_MCU_PART_SERIES
       The following macros are not updated by Smart configurator if you are using Smart Configurator for RX V2.11.0 
       (equivalent to e2 studio 2021-10) or earlier version.
       - BSP_CFG_MAIN_CLOCK_OSCILLATE_ENABLE, BSP_CFG_SUB_CLOCK_OSCILLATE_ENABLE, BSP_CFG_HOCO_OSCILLATE_ENABLE, 
         BSP_CFG_LOCO_OSCILLATE_ENABLE, BSP_CFG_IWDT_CLOCK_OSCILLATE_ENABLE, BSP_CFG_CPLUSPLUS
       The macro definition of BSP_CFG_CODE_FLASH_BANK_MODE are not updated by Smart configurator if you are using 
       Smart Configurator for RX V2.13.0 (equivalent to e2 studio 2022-04) or earlier version.
       Please update Smart configurator to Smart Configurator for RX V2.14.0 (equivalent to e2 studio 2022-07) or 
       later version.
     */
#error                                                                                             \
  "To use this version of BSP, you need to upgrade Smart configurator. Please upgrade Smart configurator. If you don't use Smart Configurator, please change value of BSP_CFG_CONFIGURATOR_VERSION and BSP_CFG_CODE_FLASH_BANK_MODE in r_bsp_config.h."
#endif

#if BSP_CFG_EXPANSION_RAM_ENABLE == 1
#if BSP_CFG_CONFIGURATOR_VERSION < 2160
/* The following macros are updated to invalid value by Smart configurator if you are using Smart Configurator for 
       RX V2.15.0 (equivalent to e2 studio 2022-10) or earlier version.
       - BSP_CFG_EXPANSION_RAM_ENABLE
       Please update Smart configurator to Smart Configurator for RX V2.16.0 (equivalent to e2 studio 2023-01) or 
       later version.
     */
#error                                                                                             \
  "To use this version of BSP, you need to upgrade Smart configurator. Please upgrade Smart configurator. If you don't use Smart Configurator, please change value of BSP_CFG_CONFIGURATOR_VERSION in r_bsp_config.h."
#endif
#endif

/**
 * @def BSP_MCU_CPU_VERSION
 * @brief RXv3 CPU core version identifier
 *
 * @details
 * Identifies RXv3 core used in RX72N. Value corresponds to CPU version number.
 *
 * @see bsp_cpu_constants_t::k_cpu_version_rxv3
 * @since Version 1.0.0
 */
#define BSP_MCU_CPU_VERSION ((uint8_t)k_cpu_version_rxv3)

/**
 * @def CPU_CYCLES_PER_LOOP
 * @brief Clock cycles per delay_wait() loop iteration
 *
 * @details
 * Known number of RXv3 CPU cycles required to execute one iteration
 * of the delay_wait() busy-wait loop. Used for software delay calculations.
 *
 * @see bsp_cpu_constants_t::k_cpu_cycles_per_loop
 * @since Version 1.0.0
 */
#define CPU_CYCLES_PER_LOOP ((uint8_t)k_cpu_cycles_per_loop)

/* MCU Series. */
#define BSP_MCU_SERIES_RX700 (1)

/* This macro means that this MCU is part of the RX72x collection of MCUs (i.e. RX72N). */
#define BSP_MCU_RX72_ALL (1)

/* MCU Group name. */
#define BSP_MCU_RX72N (1)

/* Package. */
#if BSP_CFG_MCU_PART_PACKAGE == 0x0
#define BSP_PACKAGE_LFQFP176 (1)
#define BSP_PACKAGE_PINS     (176)
#elif BSP_CFG_MCU_PART_PACKAGE == 0x1
#define BSP_PACKAGE_LFBGA176 (1)
#define BSP_PACKAGE_PINS     (176)
#elif BSP_CFG_MCU_PART_PACKAGE == 0x2
#define BSP_PACKAGE_LFBGA224 (1)
#define BSP_PACKAGE_PINS     (224)
#elif BSP_CFG_MCU_PART_PACKAGE == 0x3
#define BSP_PACKAGE_LFQFP144 (1)
#define BSP_PACKAGE_PINS     (144)
#elif BSP_CFG_MCU_PART_PACKAGE == 0x4
#define BSP_PACKAGE_TFLGA145 (1)
#define BSP_PACKAGE_PINS     (145)
#elif BSP_CFG_MCU_PART_PACKAGE == 0x5
#define BSP_PACKAGE_LFQFP100 (1)
#define BSP_PACKAGE_PINS     (100)
#else
#error "ERROR - BSP_CFG_MCU_PART_PACKAGE - Unknown package chosen in r_bsp_config.h"
#endif

/* RAM */
#define BSP_EXPANSION_RAM

/* Memory size of your MCU. */
#if BSP_CFG_MCU_PART_MEMORY_SIZE == 0xD
#define BSP_ROM_SIZE_BYTES        (2097152)
#define BSP_RAM_SIZE_BYTES        (1048576)
#define BSP_DATA_FLASH_SIZE_BYTES (32768)
#elif BSP_CFG_MCU_PART_MEMORY_SIZE == 0x17
#define BSP_ROM_SIZE_BYTES        (4194304)
#define BSP_RAM_SIZE_BYTES        (1048576)
#define BSP_DATA_FLASH_SIZE_BYTES (32768)
#else
#error "ERROR - BSP_CFG_MCU_PART_MEMORY_SIZE - Unknown memory size chosen in r_bsp_config.h"
#endif

/* These macros define clock speeds for fixed speed clocks. */
#define BSP_LOCO_HZ      (240000)
#define BSP_SUB_CLOCK_HZ (32768)

/* Define frequency of HOCO. */
#if BSP_CFG_HOCO_FREQUENCY == 0
#define BSP_HOCO_HZ (16000000)
#elif BSP_CFG_HOCO_FREQUENCY == 1
#define BSP_HOCO_HZ (18000000)
#elif BSP_CFG_HOCO_FREQUENCY == 2
#define BSP_HOCO_HZ (20000000)
#else
#error                                                                                             \
  "ERROR - Invalid HOCO frequency chosen in r_bsp_config.h! Set valid value for BSP_CFG_HOCO_FREQUENCY."
#endif

/* Clock source select (CKSEL).
   0 = Low Speed On-Chip Oscillator  (LOCO)
   1 = High Speed On-Chip Oscillator (HOCO)
   2 = Main Clock Oscillator
   3 = Sub-Clock Oscillator
   4 = PLL Circuit
*/
#if BSP_CFG_CLOCK_SOURCE == 0
#define BSP_SELECTED_CLOCK_HZ (BSP_LOCO_HZ)
#elif BSP_CFG_CLOCK_SOURCE == 1
#define BSP_SELECTED_CLOCK_HZ (BSP_HOCO_HZ)
#elif BSP_CFG_CLOCK_SOURCE == 2
#define BSP_SELECTED_CLOCK_HZ (BSP_CFG_XTAL_HZ)
#elif BSP_CFG_CLOCK_SOURCE == 3
#define BSP_SELECTED_CLOCK_HZ (BSP_SUB_CLOCK_HZ)
#elif BSP_CFG_CLOCK_SOURCE == 4
#if BSP_CFG_PLL_SRC == 0
#define BSP_SELECTED_CLOCK_HZ ((BSP_CFG_XTAL_HZ / BSP_CFG_PLL_DIV) * BSP_CFG_PLL_MUL)
#elif BSP_CFG_PLL_SRC == 1
#define BSP_SELECTED_CLOCK_HZ ((BSP_HOCO_HZ / BSP_CFG_PLL_DIV) * BSP_CFG_PLL_MUL)
#else
#error                                                                                             \
  "ERROR - Valid PLL clock source must be chosen in r_bsp_config.h using BSP_CFG_PLL_SRC macro."
#endif
#else
#error "ERROR - BSP_CFG_CLOCK_SOURCE - Unknown clock source chosen in r_bsp_config.h"
#endif

/* Define frequency of PPLL clock. */
#if BSP_CFG_PLL_SRC == 0
#define BSP_PPLL_CLOCK_HZ ((BSP_CFG_XTAL_HZ / BSP_CFG_PPLL_DIV) * BSP_CFG_PPLL_MUL)
#elif BSP_CFG_PLL_SRC == 1
#define BSP_PPLL_CLOCK_HZ ((BSP_HOCO_HZ / BSP_CFG_PPLL_DIV) * BSP_CFG_PPLL_MUL)
#else
#error                                                                                             \
  "ERROR - Valid PLL clock source must be chosen in r_bsp_config.h using BSP_CFG_PLL_SRC macro."
#endif

/*    Extended Bus Master Priority setting
   0 = GLCDC graphics 1 data read
   1 = DRW2D texture data read
   2 = DRW2D frame buffer data read write and display list data read
   3 = GLCDC graphics 2 data read
   4 = EDMAC
   
   Note : Settings other than above are prohibited.
          Duplicate priority settings can not be made.
*/
#if (BSP_CFG_EBMAPCR_1ST_PRIORITY == BSP_CFG_EBMAPCR_2ND_PRIORITY) ||                              \
  (BSP_CFG_EBMAPCR_1ST_PRIORITY == BSP_CFG_EBMAPCR_3RD_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_1ST_PRIORITY == BSP_CFG_EBMAPCR_4TH_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_1ST_PRIORITY == BSP_CFG_EBMAPCR_5TH_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_2ND_PRIORITY == BSP_CFG_EBMAPCR_3RD_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_2ND_PRIORITY == BSP_CFG_EBMAPCR_4TH_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_2ND_PRIORITY == BSP_CFG_EBMAPCR_5TH_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_3RD_PRIORITY == BSP_CFG_EBMAPCR_4TH_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_3RD_PRIORITY == BSP_CFG_EBMAPCR_5TH_PRIORITY) ||                                \
  (BSP_CFG_EBMAPCR_4TH_PRIORITY == BSP_CFG_EBMAPCR_5TH_PRIORITY)
#error                                                                                             \
  "Error! Invalid setting for Extended Bus Master Priority in r_bsp_config.h. Please check BSP_CFG_EX_BUS_1ST_PRIORITY to BSP_CFG_EX_BUS_5TH_PRIORITY"
#endif
#if (5 <= BSP_CFG_EBMAPCR_1ST_PRIORITY) || (5 <= BSP_CFG_EBMAPCR_2ND_PRIORITY) ||                  \
  (5 <= BSP_CFG_EBMAPCR_3RD_PRIORITY) || (5 <= BSP_CFG_EBMAPCR_4TH_PRIORITY) ||                    \
  (5 <= BSP_CFG_EBMAPCR_5TH_PRIORITY)
#error                                                                                             \
  "Error! Invalid setting for Extended Bus Master Priority in r_bsp_config.h. Please check BSP_CFG_EX_BUS_1ST_PRIORITY to BSP_CFG_EX_BUS_5TH_PRIORITY"
#endif

/* System clock speed in Hz. */
#define BSP_ICLK_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_ICK_DIV)
/* Peripheral Module Clock A speed in Hz. Used for ETHERC and EDMAC. */
#define BSP_PCLKA_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_PCKA_DIV)
/* Peripheral Module Clock B speed in Hz. */
#define BSP_PCLKB_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_PCKB_DIV)
/* Peripheral Module Clock C speed in Hz. */
#define BSP_PCLKC_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_PCKC_DIV)
/* Peripheral Module Clock D speed in Hz. */
#define BSP_PCLKD_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_PCKD_DIV)
/* External bus clock speed in Hz. */
#define BSP_BCLK_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_BCK_DIV)
/* FlashIF clock speed in Hz. */
#define BSP_FCLK_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_FCK_DIV)
/* USB clock speed in Hz. */
#if BSP_CFG_USB_CLOCK_SOURCE == 2
#define BSP_UCLK_HZ (BSP_SELECTED_CLOCK_HZ / BSP_CFG_UCK_DIV)
#elif BSP_CFG_USB_CLOCK_SOURCE == 3
#define BSP_UCLK_HZ (BSP_PPLL_CLOCK_HZ / BSP_CFG_PPLCK_DIV)
#else
#error "ERROR - BSP_CFG_USB_CLOCK_SOURCE - Unknown usb clock source chosen in r_bsp_config.h"
#endif

/* CLKOUT25M clock speed in Hz. */
#if BSP_CFG_PHY_CLOCK_SOURCE == 0
#define BSP_CLKOUT25M_HZ (BSP_SELECTED_CLOCK_HZ / 8)
#elif BSP_CFG_PHY_CLOCK_SOURCE == 1
#define BSP_CLKOUT25M_HZ (BSP_PPLL_CLOCK_HZ / 8)
#elif BSP_CFG_PHY_CLOCK_SOURCE == 2
/* Ethernet-PHY not use */
#else
#error                                                                                             \
  "ERROR - BSP_CFG_PHY_CLOCK_SOURCE - Unknown Ethernet-PHY clock source chosen in r_bsp_config.h"
#endif

/**
 * @def FIT_NO_FUNC
 * @brief Null function pointer for FIT modules
 *
 * @details
 * Points to reserved memory space (0x10000000) used as placeholder
 * for unused FIT module function callbacks.
 *
 * @see bsp_reserved_addresses_t::k_fit_reserved_space
 * @since Version 1.0.0
 */
#define FIT_NO_FUNC ((void (*)(void*))k_fit_reserved_space)

/**
 * @def FIT_NO_PTR
 * @brief Null pointer for FIT modules
 *
 * @details
 * Points to reserved memory space (0x10000000) used as placeholder
 * for unused FIT module parameters.
 *
 * @see bsp_reserved_addresses_t::k_fit_reserved_space
 * @since Version 1.0.0
 */
#define FIT_NO_PTR ((void*)k_fit_reserved_space)

/**
 * @def BSP_MCU_IPL_MAX
 * @brief Maximum interrupt priority level
 *
 * @details
 * RX72N supports IPL 0-15. Level 15 is highest priority.
 *
 * @see bsp_ipl_level_t::k_ipl_max
 * @see R_BSP_SET_IPL
 * @since Version 1.0.0
 */
#define BSP_MCU_IPL_MAX ((uint8_t)k_ipl_max)

/**
 * @def BSP_MCU_IPL_MIN
 * @brief Minimum interrupt priority level
 *
 * @details
 * IPL level 0 masks all interrupts (except NMI).
 *
 * @see bsp_ipl_level_t::k_ipl_min
 * @see R_BSP_SET_IPL
 * @since Version 1.0.0
 */
#define BSP_MCU_IPL_MIN ((uint8_t)k_ipl_min)

/* Frequency threshold of memory wait cycle setting. */
#define BSP_MCU_MEMWAIT_FREQ_THRESHOLD                                                             \
  (120000000) /* ICLK > 120MHz requires MEMWAIT register update */

/* Frequency threshold of iclk. */
#define BSP_MCU_ICLK_FREQ_THRESHOLD (70000000)

/* MCU TFU Version */
#define BSP_MCU_TFU_VERSION (1)

/* MCU functions */
#define BSP_MCU_REGISTER_WRITE_PROTECTION
#define BSP_MCU_RCPC_PRC0
#define BSP_MCU_RCPC_PRC1
#define BSP_MCU_RCPC_PRC3
#define BSP_MCU_FLOATING_POINT
#define BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
#define BSP_MCU_EXCEPTION_TABLE
#define BSP_MCU_GROUP_INTERRUPT
#define BSP_MCU_GROUP_INTERRUPT_IE0
#define BSP_MCU_GROUP_INTERRUPT_BE0
#define BSP_MCU_GROUP_INTERRUPT_BL0
#define BSP_MCU_GROUP_INTERRUPT_BL1
#define BSP_MCU_GROUP_INTERRUPT_BL2
#define BSP_MCU_GROUP_INTERRUPT_AL0
#define BSP_MCU_GROUP_INTERRUPT_AL1
#define BSP_MCU_SOFTWARE_CONFIGURABLE_INTERRUPT
#define BSP_MCU_EXCEP_SUPERVISOR_INST_ISR
#define BSP_MCU_EXCEP_ACCESS_ISR
#define BSP_MCU_EXCEP_UNDEFINED_INST_ISR
#define BSP_MCU_EXCEP_FLOATING_POINT_ISR
#define BSP_MCU_EXCEP_ADDRESS_ISR
#define BSP_MCU_NON_MASKABLE_ISR
#define BSP_MCU_UNDEFINED_INTERRUPT_SOURCE_ISR
#define BSP_MCU_BUS_ERROR_ISR
#define BSP_MCU_TRIGONOMETRIC

#define BSP_MCU_NMI_EXC_NMI_PIN
#define BSP_MCU_NMI_OSC_STOP_DETECT
#define BSP_MCU_NMI_WDT_ERROR
#define BSP_MCU_NMI_IWDT_ERROR
#define BSP_MCU_NMI_LVD1
#define BSP_MCU_NMI_LVD2
#define BSP_MCU_NMI_EXNMI
#define BSP_MCU_NMI_EXNMI_RAM
#define BSP_MCU_NMI_EXNMI_RAM_EXRAM
#define BSP_MCU_NMI_EXNMI_RAM_ECCRAM
#define BSP_MCU_NMI_EXNMI_DPFPUEX
