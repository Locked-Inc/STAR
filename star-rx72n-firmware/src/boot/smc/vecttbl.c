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
/**
 * @file vecttbl.c
 * @brief RX72N Exception and Interrupt Vector Table and Option-Setting Memory
 *
 * @details
 * Defines the interrupt/exception vector table loaded at reset and the
 * Option-Function Select Register (OFS) values that configure MCU security,
 * endianness, and flash bank mode.
 *
 * **Vector Table Structure:**
 * - **Exception Vector Table (EXTB):** 20 reserved entries + 11 exception vectors
 * - **Reset Vector:** Single entry at 0xFFFFFFFC pointing to PowerON_Reset_PC()
 * - **Relocatable Interrupt Vector Table (INTB):** Defined elsewhere (256 vectors)
 *
 * **Exception Vectors (offset from EXTB):**
 * - 0x50: Supervisor instruction exception
 * - 0x54: Access exception (privilege violation)
 * - 0x5C: Undefined instruction exception
 * - 0x60: Address exception (unaligned access)
 * - 0x64: Floating-point exception
 * - 0x78: Non-Maskable Interrupt (NMI)
 *
 * **Option-Function Select Registers (OFS):**
 * These registers are programmed into flash at fixed addresses and configure
 * MCU behavior at startup (before any code executes):
 *
 * | Register   | Address    | Purpose |
 * |------------|------------|---------|
 * | MDE        | 0xFE7F5D00 | Endian mode, flash bank mode |
 * | OFS0       | 0xFE7F5D04 | Watchdog, voltage detection, security |
 * | OFS1       | 0xFE7F5D08 | HOCO, IWDT, startup mode |
 * | TMINF      | 0xFE7F5D10 | TM (Trusted Memory) information (0xFFFFFFFF) |
 * | BANKSEL    | 0xFE7F5D20 | Flash bank swap selection |
 * | SPCC       | 0xFE7F5D40 | Serial programmer connection control |
 * | TMEF       | 0xFE7F5D48 | Trusted mode entry flags |
 * | OSIS1-4    | 0xFE7F5D50 | ID code protection (128-bit) |
 * | FAW        | 0xFE7F5D64 | Flash access window |
 * | ROMCODE    | 0xFE7F5D70 | ROM code protection |
 *
 * **Endian Mode (MDE Register):**
 * - Little-endian: 0xFFFFFFFF (STAR project default)
 * - Big-endian: 0xFFFFFFF8
 *
 * **Flash Bank Mode (MDE Register):**
 * - Dual bank: 0xFFFFFF8F (separate bank 0 and bank 1)
 * - Linear mode: 0xFFFFFFFF (single contiguous address space)
 *
 * **Serial Programmer (SPCC Register):**
 * - Enabled: 0xFFFFFFFF (allow serial programmer after reset)
 * - Disabled: 0xF7FFFFFF (security feature - prevent debugger attach)
 *
 * @warning OFS values are **permanent** once flash is programmed - cannot change at runtime
 * @warning Incorrect OFS values can brick the MCU (especially ID code, security settings)
 *
 * @see RX72N Manual Chapter 7 - Option-Setting Memory (OFS)
 * @see RX72N Manual Chapter 14 - Exception Handling
 * @see RX72N Manual Chapter 15 - Interrupt Controller (ICU)
 * @see resetprg.c PowerON_Reset_PC() function (reset vector target)
 * @see default_interrupt_handlers.c Exception handler implementations
 *
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 *
 * @par History
 * - 08.10.2019 v1.00 - First release (Renesas)
 * - 30.06.2021 v1.01 - Added excep_address_isr (Renesas)
 * - 30.11.2021 v1.02 - Added SPCC macros (Renesas)
 * - 25.11.2022 v1.03 - Deleted invalid function definitions (Renesas)
 * - 2026       vSTAR - STAR coding standards (C23 enums, Doxygen)
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 08.10.2019 1.00     First Release
*         : 30.06.2021 1.01     Added excep_address_isr in the Exception vector table.
*         : 30.11.2021 1.02     Added the following macro definition and changed setting of SPCC register.
*                               - BSP_PRV_SPCC_SPE
*                               - k_spcc_value
*         : 25.11.2022 1.03     Deleted the invalid function definition.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
/* BSP configuration. */
#include "platform.h"

/* When using the user startup program, disable the following code. */
#if BSP_CFG_STARTUP_DISABLE == 0

/**
 * @brief Option-Function Select Register (OFS) values
 *
 * @details
 * These enums define the values programmed into the RX72N's Option-Setting Memory
 * (OFS) region at fixed flash addresses. OFS registers configure MCU behavior at
 * reset before any code executes.
 */

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/
R_BSP_POR_FUNCTION(R_BSP_POWER_ON_RESET_FUNCTION);

/**
 * @par MDE Register Values (mde_register_values_t)
 * @brief MDE Register - Endian Mode and Flash Bank Mode Configuration
 *
 * @details
 * The MDE register (address 0xFE7F5D00) controls two critical boot settings:
 *
 * **Endian Mode (bits 31-3):**
 * - 0xFFFFFFFF: Little-endian (STAR project default, matches x86/ARM)
 * - 0xFFFFFFF8: Big-endian (network byte order)
 *
 * **Flash Bank Mode (bits 6-4):**
 * - 0xFFFFFFFF: Linear mode (single contiguous 4MB address space)
 * - 0xFFFFFF8F: Dual-bank mode (2MB bank 0 + 2MB bank 1 for background programming)
 *
 * These values are ANDed together to produce the final MDE register value.
 *
 * @warning Once programmed, cannot be changed without reprogramming flash
 *
 * @see RX72N Manual Chapter 7 - Option-Setting Memory (MDE register)
 */
#ifdef __BIG
typedef enum : uint32_t {
  k_mde_endian_value = 0xFFFFFFF8, /**< Big-endian mode */
} mde_endian_values_t;
#else
typedef enum : uint32_t {
  k_mde_endian_value = 0xFFFFFFFF, /**< Little-endian mode (STAR default) */
} mde_endian_values_t;
#endif

/**
 * @enum flash_bank_mode_values_t
 * @brief Flash Bank Mode Configuration
 *
 * @details
 * Configures whether code flash operates in:
 * - **Dual-bank mode**: Enables background programming (write to bank 1 while executing from bank 0)
 * - **Linear mode**: Single contiguous address space (no background programming)
 *
 * STAR project uses linear mode for simplicity.
 *
 * @see RX72N Manual Chapter 62 - Flash Memory (Bank Mode)
 */
#if BSP_CFG_CODE_FLASH_BANK_MODE == 0
typedef enum : uint32_t {
  k_bank_mode_value = 0xFFFFFF8F, /**< Dual-bank mode (2MB x 2 banks) */
} flash_bank_mode_values_t;
#else
typedef enum : uint32_t {
  k_bank_mode_value = 0xFFFFFFFF, /**< Linear mode (4MB contiguous, STAR default) */
} flash_bank_mode_values_t;
#endif

/**
 * @enum flash_bank_start_values_t
 * @brief Flash Bank Swap Configuration (BANKSEL Register)
 *
 * @details
 * Controls which bank appears at address 0xFFE00000 after reset:
 * - Bank 0 default: Bank 0 at 0xFFE00000, Bank 1 at 0xFFC00000
 * - Bank 1 default: Bank 1 at 0xFFE00000, Bank 0 at 0xFFC00000
 *
 * Only relevant in dual-bank mode. STAR uses linear mode so this has no effect.
 *
 * @see RX72N Manual Chapter 62 - Flash Memory (BANKSEL register)
 */
#if BSP_CFG_CODE_FLASH_START_BANK == 0
typedef enum : uint32_t {
  k_start_bank_value = 0xFFFFFFFF, /**< Bank 0 at 0xFFE00000 (default) */
} flash_bank_start_values_t;
#else
typedef enum : uint32_t {
  k_start_bank_value = 0xFFFFFFF8, /**< Bank 1 at 0xFFE00000 (swapped) */
} flash_bank_start_values_t;
#endif

/**
 * @enum serial_programmer_values_t
 * @brief Serial Programmer Connection Control (SPCC Register)
 *
 * @details
 * Controls whether a serial programmer (debugger, JTAG, SWD) can connect
 * to the MCU after reset:
 * - Enabled (0xFFFFFFFF): Allow debugger connection (development mode)
 * - Disabled (0xF7FFFFFF): Block debugger connection (production/security mode)
 *
 * STAR project enables this for development/debugging.
 *
 * @warning Disabling this can make the MCU difficult to reprogram (security feature)
 *
 * @see RX72N Manual Chapter 7 - Option-Setting Memory (SPCC register)
 */
#if BSP_CFG_SERIAL_PROGRAMMER_CONECT_ENABLE == 0
typedef enum : uint32_t {
  k_spcc_value = 0xF7FFFFFF, /**< Serial programmer disabled (production) */
} serial_programmer_values_t;
#else
typedef enum : uint32_t {
  k_spcc_value = 0xFFFFFFFF, /**< Serial programmer enabled (STAR debug mode) */
} serial_programmer_values_t;
#endif

#if defined(__CCRX__)

#pragma address __MDEreg     = 0xFE7F5D00
#pragma address __OFS0reg    = 0xFE7F5D04
#pragma address __OFS1reg    = 0xFE7F5D08
#pragma address __TMINFreg   = 0xFE7F5D10
#pragma address __BANKSELreg = 0xFE7F5D20
#pragma address __SPCCreg    = 0xFE7F5D40
#pragma address __TMEFreg    = 0xFE7F5D48
#pragma address __OSIS1reg   = 0xFE7F5D50
#pragma address __OSIS2reg   = 0xFE7F5D54
#pragma address __OSIS3reg   = 0xFE7F5D58
#pragma address __OSIS4reg   = 0xFE7F5D5C
#pragma address __FAWreg     = 0xFE7F5D64
#pragma address __ROMCODEreg = 0xFE7F5D70

const uint32_t __MDEreg     = (k_mde_endian_value & k_bank_mode_value);
const uint32_t __OFS0reg    = BSP_CFG_OFS0_REG_VALUE;
const uint32_t __OFS1reg    = BSP_CFG_OFS1_REG_VALUE;
const uint32_t __TMINFreg   = 0xffffffff;
const uint32_t __BANKSELreg = k_start_bank_value;
const uint32_t __SPCCreg    = k_spcc_value;
const uint32_t __TMEFreg    = BSP_CFG_TRUSTED_MODE_FUNCTION;
const uint32_t __OSIS1reg   = BSP_CFG_ID_CODE_LONG_1;
const uint32_t __OSIS2reg   = BSP_CFG_ID_CODE_LONG_2;
const uint32_t __OSIS3reg   = BSP_CFG_ID_CODE_LONG_3;
const uint32_t __OSIS4reg   = BSP_CFG_ID_CODE_LONG_4;
const uint32_t __FAWreg     = BSP_CFG_FAW_REG_VALUE;
const uint32_t __ROMCODEreg = BSP_CFG_ROMCODE_REG_VALUE;

#elif defined(__GNUC__)

const st_ofsm_sec_ofs1_t __ofsm_sec_ofs1 __attribute__((section(".ofs1"))) = {
  (k_mde_endian_value & k_bank_mode_value), /* __MDEreg */
  BSP_CFG_OFS0_REG_VALUE,                   /* __OFS0reg */
  BSP_CFG_OFS1_REG_VALUE                    /* __OFS1reg */
};
const uint32_t __TMINFreg __attribute__((section(".ofs2")))   = 0xffffffff;
const uint32_t __BANKSELreg __attribute__((section(".ofs3"))) = k_start_bank_value;
const uint32_t __SPCCreg __attribute__((section(".ofs4")))    = k_spcc_value;
const uint32_t __TMEFreg __attribute__((section(".ofs5")))    = BSP_CFG_TRUSTED_MODE_FUNCTION;
const st_ofsm_sec_ofs6_t __ofsm_sec_ofs6 __attribute__((section(".ofs6"))) = {
  BSP_CFG_ID_CODE_LONG_1, /* __OSIS1reg */
  BSP_CFG_ID_CODE_LONG_2, /* __OSIS2reg */
  BSP_CFG_ID_CODE_LONG_3, /* __OSIS3reg */
  BSP_CFG_ID_CODE_LONG_4  /* __OSIS4reg */
};
const uint32_t __FAWreg __attribute__((section(".ofs7")))     = BSP_CFG_FAW_REG_VALUE;
const uint32_t __ROMCODEreg __attribute__((section(".ofs8"))) = BSP_CFG_ROMCODE_REG_VALUE;

#elif defined(__ICCRX__)

#pragma public_equ = "__MDE", (k_mde_endian_value & k_bank_mode_value)
#pragma public_equ = "__OFS0", BSP_CFG_OFS0_REG_VALUE
#pragma public_equ = "__OFS1", BSP_CFG_OFS1_REG_VALUE
#pragma public_equ = "__TMINF", 0xffffffff
#pragma public_equ = "__BANKSEL", k_start_bank_value
#pragma public_equ = "__SPCC", k_spcc_value
#pragma public_equ = "__TMEF", BSP_CFG_TRUSTED_MODE_FUNCTION
#pragma public_equ = "__OSIS_1", BSP_CFG_ID_CODE_LONG_1
#pragma public_equ = "__OSIS_2", BSP_CFG_ID_CODE_LONG_2
#pragma public_equ = "__OSIS_3", BSP_CFG_ID_CODE_LONG_3
#pragma public_equ = "__OSIS_4", BSP_CFG_ID_CODE_LONG_4
#pragma public_equ = "__FAW", BSP_CFG_FAW_REG_VALUE
#pragma public_equ = "__ROM_CODE", BSP_CFG_ROMCODE_REG_VALUE

#endif /* defined(__CCRX__), defined(__GNUC__), defined(__ICCRX__) */

/***********************************************************************************************************************
* The following array fills in the exception vector table.
***********************************************************************************************************************/
#if BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */
/* System configurator generates the ritble.src as interrupt & exception vector tables. */
#else /* BSP_CFG_RTOS_USED!=4 */

#if defined(__CCRX__) || defined(__GNUC__)
R_BSP_ATTRIB_SECTION_CHANGE_EXCEPTVECT void (*const Except_Vectors[])(void) = {
  /* Offset from EXTB: Reserved area - must be all 0xFF */
  (void (*)(void))0xFFFFFFFF, /* 0x00 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x04 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x08 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x0c - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x10 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x14 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x18 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x1c - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x20 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x24 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x28 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x2c - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x30 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x34 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x38 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x3c - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x40 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x44 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x48 - Reserved */
  (void (*)(void))0xFFFFFFFF, /* 0x4c - Reserved */

  /* Exception vector table */
  excep_supervisor_inst_isr,      /* 0x50  Exception(Supervisor Instruction) */
  excep_access_isr,               /* 0x54  Exception(Access exception) */
  undefined_interrupt_source_isr, /* 0x58  Reserved */
  excep_undefined_inst_isr,       /* 0x5c  Exception(Undefined Instruction) */
  excep_address_isr,              /* 0x60  Exception(Address exception) */
  excep_floating_point_isr,       /* 0x64  Exception(Floating Point) */
  undefined_interrupt_source_isr, /* 0x68  Reserved */
  undefined_interrupt_source_isr, /* 0x6c  Reserved */
  undefined_interrupt_source_isr, /* 0x70  Reserved */
  undefined_interrupt_source_isr, /* 0x74  Reserved */
  non_maskable_isr,               /* 0x78  NMI */
};
R_BSP_ATTRIB_SECTION_CHANGE_END
#endif /* defined(__CCRX__), defined(__GNUC__) */

/***********************************************************************************************************************
* The following array fills in the reset vector.
***********************************************************************************************************************/
#if defined(__CCRX__) || defined(__GNUC__)
R_BSP_ATTRIB_SECTION_CHANGE_RESETVECT void (*const Reset_Vector[])(void) = {
  R_BSP_POWER_ON_RESET_FUNCTION /* 0xfffffffc  RESET */
};
R_BSP_ATTRIB_SECTION_CHANGE_END
#endif /* defined(__CCRX__), defined(__GNUC__) */

#endif /* BSP_CFG_RTOS_USED */

#endif /* BSP_CFG_STARTUP_DISABLE == 0 */
