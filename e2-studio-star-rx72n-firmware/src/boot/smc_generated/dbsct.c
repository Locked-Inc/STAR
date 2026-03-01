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
* Copyright (C) 2013 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/**
 * @file dbsct.c
 * @brief ROM/RAM Section Definitions - Data BSS Constructor Table (CCRX only)
 *
 * @details
 * Defines memory section boundaries used by the C runtime initialization
 * (_INITSCT) to copy initialized data from ROM to RAM and zero the BSS section.
 * This file is **CCRX-specific** - GNURX uses linker scripts instead.
 *
 * **Memory Sections:**
 * - **D (Data)**: Initialized variables (copied from ROM to RAM at startup)
 * - **B (BSS)**: Uninitialized variables (zeroed at startup)
 * - **R (RAM)**: RAM destination for initialized data
 * - **C (Const)**: Constant data (remains in ROM)
 * - **W (Write)**: Writable data sections
 *
 * **Section Alignment Suffixes:**
 * - No suffix: 4-byte alignment (default)
 * - `_1`: 1-byte alignment (char arrays)
 * - `_2`: 2-byte alignment (uint16_t, short)
 * - `_8`: 8-byte alignment (double, uint64_t, DPFPU data)
 *
 * **Section Table Structure:**
 * ```c
 * _DTBL[] = {
 *     { ROM_start, ROM_end, RAM_start },  // For each D section
 *     ...
 * };
 * _BTBL[] = {
 *     { BSS_start, BSS_end },  // For each B section
 *     ...
 * };
 * ```
 *
 * **Startup Sequence:**
 * 1. PowerON_Reset_PC() calls _INITSCT()
 * 2. _INITSCT() iterates through _DTBL[], copies ROM -> RAM
 * 3. _INITSCT() iterates through _BTBL[], zeros BSS
 * 4. Control returns to PowerON_Reset_PC() -> jumps to main()
 *
 * **Expansion RAM Support:**
 * If BSP_CFG_EXPANSION_RAM_ENABLE=1, additional sections are defined:
 * - DEXRAM, REXRAM, BEXRAM - for external SDRAM or SRAM
 *
 * **RTOS Support:**
 * Renesas RI600V4/PX adds special sections:
 * - DRI_ROM/RRI_RAM (RI600PX): RTOS data
 * - BRI_RAM (RI600V4): RTOS BSS
 *
 * @warning This file is CCRX-specific - do not compile with GNURX
 * @warning Linker symbols (__sectop, __secend) are compiler-specific
 *
 * @see PowerON_Reset_PC() in resetprg.c - Calls _INITSCT()
 * @see _INITSCT() C runtime function (provided by CCRX library)
 * @see rx72n.ld GNURX linker script (equivalent for GNURX builds)
 * @see RX72N Manual Chapter 4 - Address Space (memory layout)
 *
 * @copyright Copyright (c) 2026 STAR Project
 * @copyright Copyright (c) 2026 STAR Project
 *
 * @par History
 * - 28.02.2019 v3.00 - Merged all devices, added GNUC/ICCRX support (Renesas)
 * - 08.10.2019 v3.01 - Added RTOS sections (Renesas)
 * - 14.02.2020 v3.02 - Fixed unpack pragma (Renesas)
 * - 25.11.2022 v3.03 - Added expansion RAM sections (Renesas)
 * - 2026       vSTAR - Added comprehensive Doxygen documentation
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 28.02.2019 3.00     Merged processing of all devices.
*                               Added support for GNUC and ICCRX.
*                               Fixed coding style.
*                               Added definition for section of D_8, B_8, and C_8.
*         : 08.10.2019 3.01     Added section for Renesas RTOS (RI600V4 or RI600PX).
*         : 14.02.2020 3.02     Corrected pragma declaration of unpack.
*         : 25.11.2022 3.03     Added definition for section of DEXRAM, DEXRAM_2, DEXRAM_1, DEXRAM_8, REXRAM, REXRAM_2,
*                               REXRAM_1, REXRAM_8, BEXRAM, BEXRAM_2, BEXRAM_1 and BEXRAM_8.
***********************************************************************************************************************/

#if defined(__CCRX__)
/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "platform.h"

/* When using the user startup program, disable the following code. */
#if BSP_CFG_STARTUP_DISABLE == 0

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/**
 * @brief Section table structure definitions
 *
 * @details
 * Defines the structure of ROM/RAM section tables used by _INITSCT() during
 * C runtime initialization. These tables tell the runtime where to copy data
 * and where to zero BSS.
 */

/* Preprocessor directive */
#pragma unpack

/**
 * @struct st_dtbl_t
 * @brief Data section table entry - ROM to RAM copy descriptor
 *
 * @details
 * Describes one initialized data section that must be copied from ROM to RAM
 * during startup. _INITSCT() iterates through _DTBL[] and copies:
 * ```
 * memcpy(ram_s, rom_s, rom_e - rom_s);
 * ```
 *
 * **Example:**
 * For a global variable `uint32_t g_count = 42;`:
 * - Compiler places initial value (42) in ROM D section
 * - Linker creates entry: { &D_start, &D_end, &R_start }
 * - _INITSCT() copies ROM -> RAM at startup
 * - Program accesses variable from RAM
 *
 * @see _DTBL[] Array of all data section copy descriptors
 * @see _INITSCT() C runtime function that performs the copy
 */
typedef struct {
  uint8_t* rom_s; /**< Start address of initialized data in ROM (source) */
  uint8_t* rom_e; /**< End address of initialized data in ROM (exclusive) */
  uint8_t* ram_s; /**< Start address of initialized data in RAM (destination) */
} st_dtbl_t;

/**
 * @struct st_btbl_t
 * @brief BSS section table entry - Zero-initialization descriptor
 *
 * @details
 * Describes one uninitialized data section (BSS) that must be zeroed during
 * startup. _INITSCT() iterates through _BTBL[] and zeros:
 * ```
 * memset(b_s, 0, b_e - b_s);
 * ```
 *
 * **Example:**
 * For a global variable `static uint8_t s_buffer[256];` (no initializer):
 * - Compiler allocates space in BSS section (no ROM storage needed)
 * - Linker creates entry: { &B_start, &B_end }
 * - _INITSCT() zeros RAM at startup
 * - Program accesses zeroed buffer
 *
 * **C Standard Guarantee:**
 * All static and global variables without explicit initializers are
 * guaranteed to be zero-initialized (C11 Sec.6.7.910).
 *
 * @see _BTBL[] Array of all BSS section zero descriptors
 * @see _INITSCT() C runtime function that performs the zeroing
 */
typedef struct {
  uint8_t* b_s; /**< Start address of BSS section to zero (source) */
  uint8_t* b_e; /**< End address of BSS section to zero (exclusive) */
} st_btbl_t;

/**
 * @brief Global section tables
 *
 * @details
 * _DTBL[] and _BTBL[] are accessed by _INITSCT() during C runtime initialization.
 * These tables are generated by the linker based on actual sections in the program.
 */

/* Section start */
#pragma section C C$DSEC

/**
 * @var _DTBL
 * @brief Data section table - ROM to RAM copy descriptors
 *
 * @details
 * Array of st_dtbl_t entries describing all initialized data sections that
 * must be copied from ROM to RAM during startup. Entries include:
 *
 * - **D_8**: 8-byte aligned data (double, uint64_t, DPFPU)
 * - **D**: 4-byte aligned data (uint32_t, int, float, pointers)
 * - **D_2**: 2-byte aligned data (uint16_t, short)
 * - **D_1**: 1-byte aligned data (uint8_t, char, packed structs)
 * - **DEXRAM_***: Expansion RAM data sections (if enabled)
 * - **DRI_ROM**: Renesas RI600PX RTOS data (if RTOS enabled)
 *
 * _INITSCT() iterates through this table and copies each ROM range to its
 * corresponding RAM location.
 *
 * @note Table is NULL-terminated (linker adds terminator)
 * @note Order matters for alignment - 8-byte first, then 4, 2, 1
 *
 * @see st_dtbl_t Structure definition
 * @see _INITSCT() C runtime function that uses this table
 */
extern st_dtbl_t const _DTBL[] = {
#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
  {__sectop("D_8"), __secend("D_8"), __sectop("R_8")},
#endif
  {__sectop("D"), __secend("D"), __sectop("R")},
  {__sectop("D_2"), __secend("D_2"), __sectop("R_2")},
  {__sectop("D_1"), __secend("D_1"), __sectop("R_1")}
#if (defined(BSP_CFG_EXPANSION_RAM_ENABLE) && (BSP_CFG_EXPANSION_RAM_ENABLE == 1))
#ifdef BSP_EXPANSION_RAM
#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
  ,
  {__sectop("DEXRAM_8"), __secend("DEXRAM_8"), __sectop("REXRAM_8")}
#endif
  ,
  {__sectop("DEXRAM"), __secend("DEXRAM"), __sectop("REXRAM")},
  {__sectop("DEXRAM_2"), __secend("DEXRAM_2"), __sectop("REXRAM_2")},
  {__sectop("DEXRAM_1"), __secend("DEXRAM_1"), __sectop("REXRAM_1")}
#endif
#endif
#if (BSP_CFG_RTOS_USED == 4) && (BSP_CFG_RENESAS_RTOS_USED == RENESAS_RI600PX)
  ,
  {__sectop("DRI_ROM"), __secend("DRI_ROM"), __sectop("RRI_RAM")}
#endif /* Renesas RI600PX */
};

/* Section start */
#pragma section C C$BSEC

/**
 * @var _BTBL
 * @brief BSS section table - Zero-initialization descriptors
 *
 * @details
 * Array of st_btbl_t entries describing all uninitialized data sections (BSS)
 * that must be zeroed during startup. Entries include:
 *
 * - **B_8**: 8-byte aligned BSS (double, uint64_t, DPFPU)
 * - **B**: 4-byte aligned BSS (uint32_t, int, float, pointers)
 * - **B_2**: 2-byte aligned BSS (uint16_t, short)
 * - **B_1**: 1-byte aligned BSS (uint8_t, char, packed structs)
 * - **BEXRAM_***: Expansion RAM BSS sections (if enabled)
 * - **BRI_RAM**: Renesas RI600V4 RTOS BSS (if RTOS enabled)
 *
 * _INITSCT() iterates through this table and zeros each RAM range.
 *
 * **C Standard Compliance:**
 * The C standard (C11 Sec.6.7.910) guarantees that all static and global
 * variables without explicit initializers are zero-initialized. This table
 * enables the compiler to fulfill that guarantee.
 *
 * @note Table is NULL-terminated (linker adds terminator)
 * @note Order matters for alignment - 8-byte first, then 4, 2, 1
 *
 * @see st_btbl_t Structure definition
 * @see _INITSCT() C runtime function that uses this table
 */
extern st_btbl_t const _BTBL[] = {
#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
  {__sectop("B_8"), __secend("B_8")},
#endif
  {__sectop("B"), __secend("B")},
  {__sectop("B_2"), __secend("B_2")},
  {__sectop("B_1"), __secend("B_1")}
#if (defined(BSP_CFG_EXPANSION_RAM_ENABLE) && (BSP_CFG_EXPANSION_RAM_ENABLE == 1))
#ifdef BSP_EXPANSION_RAM
#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
  ,
  {__sectop("BEXRAM_8"), __secend("BEXRAM_8")}
#endif
  ,
  {__sectop("BEXRAM"), __secend("BEXRAM")},
  {__sectop("BEXRAM_2"), __secend("BEXRAM_2")},
  {__sectop("BEXRAM_1"), __secend("BEXRAM_1")}
#endif
#endif
#if (BSP_CFG_RTOS_USED == 4) && (BSP_CFG_RENESAS_RTOS_USED == RENESAS_RI600V4)
  ,
  {__sectop("BRI_RAM"), __secend("BRI_RAM")}
#endif /* Renesas RI600V4 */
};

/* Section start */
#pragma section

#if (BSP_CFG_RTOS_USED == 4) && (BSP_CFG_RENESAS_RTOS_USED == RENESAS_RI600PX)
#pragma section C CS
#endif /* Renesas RI600PX */

/**
 * @var _CTBL
 * @brief Const section table - Linker warning suppression (cosmetic only)
 *
 * @details
 * This table prevents excessive L1100 linker warning messages about unused
 * const sections. It does **not** affect program behavior - purely cosmetic.
 *
 * Lists all const (C) and write (W) section start addresses to inform the
 * linker they are intentionally referenced.
 *
 * **Sections:**
 * - **C_1, C_2, C**: Const data (1/2/4-byte alignment)
 * - **C_8**: 8-byte aligned const data (DPFPU)
 * - **W_1, W_2, W**: Writable data (1/2/4-byte alignment)
 *
 * @note Can be safely deleted - does not change program operation
 * @note Only suppresses linker warnings, no runtime effect
 */
uint8_t* const _CTBL[] = {__sectop("C_1"),
                          __sectop("C_2"),
                          __sectop("C"),
#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
                          __sectop("C_8"),
#endif
                          __sectop("W_1"),
                          __sectop("W_2"),
                          __sectop("W")};

/* Preprocessor directive */
#pragma packoption

/**
 * @var deadSpace
 * @brief Compatibility placeholder for CCRX v1.1+ L section
 *
 * @details
 * Required for compatibility with CCRX compiler version 1.1 and later.
 * The compiler introduces an "L" section for certain optimizations, and this
 * placeholder ensures the section is properly allocated.
 *
 * **Do not remove** - may cause linker errors with CCRX v1.1+.
 *
 * @warning Removing this variable may cause linker failures
 * @note Value 0xDEADDEAD is a recognizable debug pattern
 */
#pragma section C L
const uint32_t deadSpace = 0xDEADDEAD;
#pragma section

/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/

#endif /* BSP_CFG_STARTUP_DISABLE == 0 */

#endif /* defined(__CCRX__) */
