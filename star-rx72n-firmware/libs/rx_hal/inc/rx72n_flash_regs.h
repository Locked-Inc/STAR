/* lib/rx_hal/inc/rx72n_flash_regs.h */

/**
 * @file rx72n_flash_regs.h
 * @brief RX72N Flash Memory Register Definitions
 *
 * @details
 * Register definitions for the Flash Memory module including ROM cache control,
 * flash programming/erase operations (FACI), and access protection. The RX72N
 * has 4MB code flash and 32KB data flash with hardware-accelerated operations.
 *
 * @par System Architecture
 * @dot
 * digraph flash_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_flash {
 *     label="Flash Memory System";
 *     style=filled;
 *     color=lightgrey;
 *
 *     code [label="Code Flash\n4 MB\n0xFFC00000-0xFFFFFFFF"];
 *     data [label="Data Flash\n32 KB\n0x00100000-0x00107FFF"];
 *     faci [label="FACI\n(Flash Application\nCommand Interface)" shape=ellipse];
 *     cache [label="ROM Cache\n8 KB"];
 *   }
 *
 *   cpu [label="CPU"];
 *   regs [label="Control Registers\n0x007FE0xx"];
 *
 *   cpu -> cache [label="Fetch"];
 *   cache -> code [label="Miss"];
 *   cpu -> regs [label="Configure"];
 *   regs -> faci [label="Commands"];
 *   faci -> code [label="Program/Erase"];
 *   faci -> data [label="Program/Erase"];
 * }
 * @enddot
 *
 * @par Flash Memory Specifications
 * | Parameter       | Code Flash    | Data Flash    |
 * |-----------------|---------------|---------------|
 * | Capacity        | 4 MB          | 32 KB         |
 * | Address Range   | FFC00000-FFFF FFFF | 0010 0000-0010 7FFF |
 * | Program Unit    | 128 bytes     | 4 bytes       |
 * | Erase Unit      | 8/32 KB block | 64 bytes      |
 * | Read Latency    | 1-3 cycles    | FCLK based    |
 *
 * @par FACI Commands (via 0x007E0000)
 * | Command         | Code  | Description                       |
 * |-----------------|-------|-----------------------------------|
 * | Program         | 0xE8  | Write data to flash               |
 * | Block Erase     | 0x20  | Erase a single block              |
 * | Blank Check     | 0x71  | Verify area is erased             |
 * | Status Clear    | 0x50  | Clear error status                |
 * | Forced Stop     | 0xB3  | Abort current operation           |
 * | PE Suspend      | 0xB0  | Suspend program/erase             |
 * | PE Resume       | 0xD0  | Resume suspended operation        |
 *
 * @par Module Dependencies
 * - Module stop: MSTPCRB.MSTPB31 (FCU) - already running at boot
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 62: Flash Memory (FLASH), pages 2992-3065
 *
 * @note The flash sequencer (FACI) must be in P/E mode before issuing commands
 * @warning Programming/erasing code flash while executing from it is restricted
 * @attention Enable FWEPROR before any programming operations
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 3: No dynamic memory allocation
 * - Rule 8: All constants use C23 typed enums
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @see rx_flash.h Higher-level flash API (when implemented)
 * @see rx72n_regs.h Main register include file
 *
 * @author STAR Team
 * @date 2026-01-28
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
 * ROM Cache Control Registers - Base Address 0x00081000
 * =============================================================================
 */

/**
 * @enum rx_rom_cache_addresses_t
 * @brief ROM cache register addresses
 *
 * @details
 * Base addresses for ROM cache control registers. The ROM cache improves
 * code fetch performance with 8KB direct-mapped cache.
 *
 * @par Memory Map
 * | Address      | Register | Description                       |
 * |--------------|----------|-----------------------------------|
 * | 0x00081000   | ROMCE    | ROM Cache Enable                  |
 * | 0x00081004   | ROMCIV   | ROM Cache Invalidate              |
 * | 0x00081040   | NCRG0    | Non-Cacheable Area 0 Address      |
 * | 0x00081044   | NCRC0    | Non-Cacheable Area 0 Control      |
 * | 0x00081048   | NCRG1    | Non-Cacheable Area 1 Address      |
 * | 0x0008104C   | NCRC1    | Non-Cacheable Area 1 Control      |
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_rom_cache_base_addr = 0x00081000, /**< ROM cache register base */
  k_romce_addr          = 0x00081000, /**< ROM Cache Enable Register */
  k_romciv_addr         = 0x00081004, /**< ROM Cache Invalidate Register */
  k_ncrg0_addr          = 0x00081040, /**< Non-Cacheable Area 0 Address */
  k_ncrc0_addr          = 0x00081044, /**< Non-Cacheable Area 0 Control */
  k_ncrg1_addr          = 0x00081048, /**< Non-Cacheable Area 1 Address */
  k_ncrc1_addr          = 0x0008104C, /**< Non-Cacheable Area 1 Control */
} rx_rom_cache_addresses_t;

/**
 * @enum rx_romce_bits_t
 * @brief ROM Cache Enable Register (ROMCE) bit definitions
 *
 * @details
 * ROMCE @ 0x00081000 controls ROM cache enable/disable.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_romce_romcen = (1U << 0), /**< ROM Cache Enable (1=enabled, 0=disabled) */
} rx_romce_bits_t;

/**
 * @enum rx_romciv_bits_t
 * @brief ROM Cache Invalidate Register (ROMCIV) bit definitions
 *
 * @details
 * ROMCIV @ 0x00081004 invalidates all cache lines.
 * Read: 0=complete/not started, 1=in progress
 * Write: 1=start invalidation, 0=no effect
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_romciv_romciv = (1U << 0), /**< ROM Cache Invalidate */
} rx_romciv_bits_t;

/**
 * @brief Get pointer to ROM Cache Enable Register (ROMCE)
 * @return Volatile pointer to ROMCE register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* romce_reg(void)
{
  return (volatile uint16_t*)k_romce_addr;
}

/**
 * @brief Get pointer to ROM Cache Invalidate Register (ROMCIV)
 * @return Volatile pointer to ROMCIV register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* romciv_reg(void)
{
  return (volatile uint16_t*)k_romciv_addr;
}

/* =============================================================================
 * Flash P/E Protect Register - Address 0x0008C296
 * =============================================================================
 */

/**
 * @enum rx_fwepror_addresses_t
 * @brief Flash Write/Erase Protect Register addresses
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_fwepror_addr = 0x0008C296, /**< Flash P/E Protect Register */
} rx_fwepror_addresses_t;

/**
 * @enum rx_fwepror_bits_t
 * @brief Flash P/E Protect Register (FWEPROR) bit definitions
 *
 * @details
 * FWEPROR @ 0x0008C296 enables/disables flash program and erase operations.
 * - FLWE = 00b: Program, block erase, blank check disabled
 * - FLWE = 01b: Program, block erase, blank check enabled
 * - FLWE = 10b: Disabled
 * - FLWE = 11b: Disabled
 *
 * @warning Register is reset on deep software standby mode entry
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_fwepror_flwe_mask    = 0x03, /**< FLWE[1:0] mask */
  k_fwepror_flwe_disable = 0x00, /**< Program/erase disabled */
  k_fwepror_flwe_enable  = 0x01, /**< Program/erase enabled */
  k_fwepror_reset_value  = 0x02, /**< Value after reset */
} rx_fwepror_bits_t;

/**
 * @brief Get pointer to Flash P/E Protect Register (FWEPROR)
 * @return Volatile pointer to FWEPROR register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* fwepror_reg(void)
{
  return (volatile uint8_t*)k_fwepror_addr;
}

/* =============================================================================
 * FACI Registers - Base Address 0x007FE000
 * =============================================================================
 */

/**
 * @enum rx_faci_addresses_t
 * @brief FACI (Flash Application Command Interface) register addresses
 *
 * @details
 * FACI registers control flash programming and erase operations.
 * All registers are in the 0x007FE0xx range.
 *
 * @par Memory Map
 * | Address      | Register | Size | Description                       |
 * |--------------|----------|------|-----------------------------------|
 * | 0x007FE010   | FASTAT   | 8    | Flash Access Status               |
 * | 0x007FE014   | FAEINT   | 8    | Flash Access Error Int Enable     |
 * | 0x007FE018   | FRDYIE   | 8    | Flash Ready Interrupt Enable      |
 * | 0x007FE030   | FSADDR   | 32   | FACI Start Address                |
 * | 0x007FE034   | FEADDR   | 32   | FACI End Address                  |
 * | 0x007FE080   | FSTATR   | 32   | Flash Status Register             |
 * | 0x007FE084   | FENTRYR  | 16   | Flash P/E Mode Entry              |
 * | 0x007FE08C   | FSUINITR | 16   | Flash Sequencer Setup Init        |
 * | 0x007FE0A0   | FCMDR    | 16   | FACI Command Register             |
 * | 0x007FE0D0   | FBCCNT   | 8    | Blank Check Control               |
 * | 0x007FE0D4   | FBCSTAT  | 8    | Blank Check Status                |
 * | 0x007FE0D8   | FPSADDR  | 32   | Flash Processing Switch Address   |
 * | 0x007FE0DC   | FAWMON   | 32   | Flash Access Window Monitor       |
 * | 0x007FE0E0   | FCPSR    | 16   | Flash Sequencer Processing Switch |
 * | 0x007FE0E4   | FPCKAR   | 16   | Flash Sequencer Clock Notify      |
 * | 0x007FE0E8   | FSUACR   | 16   | Flash Startup Area Select         |
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_faci_base_addr = 0x007FE000, /**< FACI register base (conceptual) */
  k_fastat_addr    = 0x007FE010, /**< Flash Access Status Register */
  k_faeint_addr    = 0x007FE014, /**< Flash Access Error Interrupt Enable */
  k_frdyie_addr    = 0x007FE018, /**< Flash Ready Interrupt Enable */
  k_fsaddr_addr    = 0x007FE030, /**< FACI Start Address Register */
  k_feaddr_addr    = 0x007FE034, /**< FACI End Address Register */
  k_fstatr_addr    = 0x007FE080, /**< Flash Status Register */
  k_fentryr_addr   = 0x007FE084, /**< Flash P/E Mode Entry Register */
  k_fsuinitr_addr  = 0x007FE08C, /**< Flash Sequencer Setup Init Register */
  k_fcmdr_addr     = 0x007FE0A0, /**< FACI Command Register */
  k_fbccnt_addr    = 0x007FE0D0, /**< Blank Check Control Register */
  k_fbcstat_addr   = 0x007FE0D4, /**< Blank Check Status Register */
  k_fpsaddr_addr   = 0x007FE0D8, /**< Flash Processing Switch Address */
  k_fawmon_addr    = 0x007FE0DC, /**< Flash Access Window Monitor */
  k_fcpsr_addr     = 0x007FE0E0, /**< Flash Sequencer Processing Switch */
  k_fpckar_addr    = 0x007FE0E4, /**< Flash Sequencer Clock Frequency */
  k_fsuacr_addr    = 0x007FE0E8, /**< Flash Startup Area Select */
  k_faci_cmd_area  = 0x007E0000, /**< FACI Command Issuing Area */
} rx_faci_addresses_t;

/**
 * @enum rx_eepfclk_addresses_t
 * @brief Data Flash Memory Access Frequency register address
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_eepfclk_addr = 0x007FC040, /**< Data Flash Access Frequency Setting */
} rx_eepfclk_addresses_t;

/* =============================================================================
 * FASTAT - Flash Access Status Register @ 0x007FE010
 * =============================================================================
 */

/**
 * @enum rx_fastat_bits_t
 * @brief Flash Access Status Register (FASTAT) bit definitions
 *
 * @details
 * FASTAT @ 0x007FE010 indicates flash access violations.
 *
 * @par Bit Layout (8-bit)
 * | Bit | Name   | R/W | Description                           |
 * |-----|--------|-----|---------------------------------------|
 * | 7   | CFAE   | R/W | Code Flash Access Violation           |
 * | 6:5 | -      | R   | Reserved                              |
 * | 4   | CMDLK  | R   | Command Lock (sequencer locked)       |
 * | 3   | DFAE   | R/W | Data Flash Access Violation           |
 * | 2:0 | -      | R   | Reserved                              |
 *
 * @note Write 0 to clear CFAE/DFAE after reading 1
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_fastat_dfae_shift  = 3, /**< DFAE bit position */
  k_fastat_cmdlk_shift = 4, /**< CMDLK bit position */
  k_fastat_cfae_shift  = 7, /**< CFAE bit position */

  k_fastat_dfae  = (1U << 3), /**< Data Flash Access Violation */
  k_fastat_cmdlk = (1U << 4), /**< Command Lock Flag */
  k_fastat_cfae  = (1U << 7), /**< Code Flash Access Violation */
} rx_fastat_bits_t;

/**
 * @brief Get pointer to Flash Access Status Register (FASTAT)
 * @return Volatile pointer to FASTAT register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* fastat_reg(void)
{
  return (volatile uint8_t*)k_fastat_addr;
}

/* =============================================================================
 * FAEINT - Flash Access Error Interrupt Enable @ 0x007FE014
 * =============================================================================
 */

/**
 * @enum rx_faeint_bits_t
 * @brief Flash Access Error Interrupt Enable Register (FAEINT) bit definitions
 *
 * @details
 * FAEINT @ 0x007FE014 enables FIFERR interrupt generation.
 * Reset value: 0x98 (all interrupts enabled)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_faeint_dfaeie_shift  = 3, /**< DFAEIE bit position */
  k_faeint_cmdlkie_shift = 4, /**< CMDLKIE bit position */
  k_faeint_cfaeie_shift  = 7, /**< CFAEIE bit position */

  k_faeint_dfaeie  = (1U << 3), /**< Data Flash Access Violation IE */
  k_faeint_cmdlkie = (1U << 4), /**< Command Lock Interrupt Enable */
  k_faeint_cfaeie  = (1U << 7), /**< Code Flash Access Violation IE */

  k_faeint_reset_value = 0x98, /**< Reset value (all enabled) */
} rx_faeint_bits_t;

/**
 * @brief Get pointer to Flash Access Error Interrupt Enable (FAEINT)
 * @return Volatile pointer to FAEINT register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* faeint_reg(void)
{
  return (volatile uint8_t*)k_faeint_addr;
}

/* =============================================================================
 * FRDYIE - Flash Ready Interrupt Enable @ 0x007FE018
 * =============================================================================
 */

/**
 * @enum rx_frdyie_bits_t
 * @brief Flash Ready Interrupt Enable Register (FRDYIE) bit definitions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_frdyie_frdyie = (1U << 0), /**< Flash Ready Interrupt Enable */
} rx_frdyie_bits_t;

/**
 * @brief Get pointer to Flash Ready Interrupt Enable (FRDYIE)
 * @return Volatile pointer to FRDYIE register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* frdyie_reg(void)
{
  return (volatile uint8_t*)k_frdyie_addr;
}

/* =============================================================================
 * FSTATR - Flash Status Register @ 0x007FE080
 * =============================================================================
 */

/**
 * @enum rx_fstatr_bits_t
 * @brief Flash Status Register (FSTATR) bit definitions
 *
 * @details
 * FSTATR @ 0x007FE080 indicates flash sequencer state.
 * This is the primary register for monitoring flash operations.
 *
 * @par Bit Layout (32-bit)
 * | Bit   | Name      | Description                              |
 * |-------|-----------|------------------------------------------|
 * | 6     | FLWEERR   | Flash P/E Protect Error                  |
 * | 8     | PRGSPD    | Program Suspend Status                   |
 * | 9     | ERSSPD    | Erase Suspend Status                     |
 * | 10    | DBFULL    | Data Buffer Full                         |
 * | 11    | SUSRDY    | Suspend Ready                            |
 * | 12    | PRGERR    | Program Error                            |
 * | 13    | ERSERR    | Erase Error                              |
 * | 14    | ILGLERR   | Illegal Command/Access Error             |
 * | 15    | FRDY      | Flash Ready (1=ready, 0=busy)            |
 * | 20    | OTERR     | Other Error                              |
 * | 21    | SECERR    | Security Error                           |
 * | 22    | FESETERR  | FENTRY Setting Error                     |
 * | 23    | ILGCOMERR | Illegal Command Error                    |
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* Bit positions */
  k_fstatr_flweerr_shift   = 6,
  k_fstatr_prgspd_shift    = 8,
  k_fstatr_ersspd_shift    = 9,
  k_fstatr_dbfull_shift    = 10,
  k_fstatr_susrdy_shift    = 11,
  k_fstatr_prgerr_shift    = 12,
  k_fstatr_erserr_shift    = 13,
  k_fstatr_ilglerr_shift   = 14,
  k_fstatr_frdy_shift      = 15,
  k_fstatr_oterr_shift     = 20,
  k_fstatr_secerr_shift    = 21,
  k_fstatr_feseterr_shift  = 22,
  k_fstatr_ilgcomerr_shift = 23,

  /* Bit masks */
  k_fstatr_flweerr   = (1UL << 6),  /**< Flash P/E Protect Error */
  k_fstatr_prgspd    = (1UL << 8),  /**< Program Suspend Status */
  k_fstatr_ersspd    = (1UL << 9),  /**< Erase Suspend Status */
  k_fstatr_dbfull    = (1UL << 10), /**< Data Buffer Full */
  k_fstatr_susrdy    = (1UL << 11), /**< Suspend Ready */
  k_fstatr_prgerr    = (1UL << 12), /**< Program Error */
  k_fstatr_erserr    = (1UL << 13), /**< Erase Error */
  k_fstatr_ilglerr   = (1UL << 14), /**< Illegal Error */
  k_fstatr_frdy      = (1UL << 15), /**< Flash Ready */
  k_fstatr_oterr     = (1UL << 20), /**< Other Error */
  k_fstatr_secerr    = (1UL << 21), /**< Security Error */
  k_fstatr_feseterr  = (1UL << 22), /**< FENTRY Setting Error */
  k_fstatr_ilgcomerr = (1UL << 23), /**< Illegal Command Error */

  /* Combined error mask */
  k_fstatr_error_mask = (k_fstatr_flweerr | k_fstatr_prgerr | k_fstatr_erserr | k_fstatr_ilglerr |
                         k_fstatr_oterr | k_fstatr_secerr | k_fstatr_feseterr | k_fstatr_ilgcomerr),

  k_fstatr_reset_value = 0x00008000, /**< Reset value (FRDY=1) */
} rx_fstatr_bits_t;

/**
 * @brief Get pointer to Flash Status Register (FSTATR)
 * @return Volatile pointer to FSTATR register
 * @since Version 1.0.0
 */
static inline volatile uint32_t* fstatr_reg(void)
{
  return (volatile uint32_t*)k_fstatr_addr;
}

/* =============================================================================
 * FENTRYR - Flash P/E Mode Entry Register @ 0x007FE084
 * =============================================================================
 */

/**
 * @enum rx_fentryr_bits_t
 * @brief Flash P/E Mode Entry Register (FENTRYR) bit definitions
 *
 * @details
 * FENTRYR @ 0x007FE084 controls entry into P/E (Program/Erase) mode.
 * Write with KEY=0xAA to modify FENTRYC/FENTRYD bits.
 *
 * @par Entry Values
 * - 0xAA01: Enter code flash P/E mode
 * - 0xAA80: Enter data flash P/E mode
 * - 0xAA00: Exit P/E mode (return to read mode)
 *
 * @warning Writing 0xAA81 causes ILGLERR error
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_fentryr_fentryc   = (1U << 0), /**< Code Flash P/E Mode Entry */
  k_fentryr_fentryd   = (1U << 7), /**< Data Flash P/E Mode Entry */
  k_fentryr_key_shift = 8,         /**< Key code position */
  k_fentryr_key       = 0xAA00,    /**< Key code (write only) */

  k_fentryr_code_pe   = 0xAA01, /**< Enter code flash P/E mode */
  k_fentryr_data_pe   = 0xAA80, /**< Enter data flash P/E mode */
  k_fentryr_read_mode = 0xAA00, /**< Exit P/E mode */
} rx_fentryr_bits_t;

/**
 * @brief Get pointer to Flash P/E Mode Entry Register (FENTRYR)
 * @return Volatile pointer to FENTRYR register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* fentryr_reg(void)
{
  return (volatile uint16_t*)k_fentryr_addr;
}

/* =============================================================================
 * FSUINITR - Flash Sequencer Setup Initialization @ 0x007FE08C
 * =============================================================================
 */

/**
 * @enum rx_fsuinitr_bits_t
 * @brief Flash Sequencer Setup Initialization Register (FSUINITR) bit defs
 *
 * @details
 * FSUINITR @ 0x007FE08C initializes flash sequencer setup registers.
 * Write with KEY=0x2D to enable SUINIT write.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_fsuinitr_suinit = (1U << 0), /**< Setup Initialization */
  k_fsuinitr_key    = 0x2D00,    /**< Key code for write enable */

  k_fsuinitr_init_cmd = 0x2D01, /**< Initialize flash sequencer setup */
} rx_fsuinitr_bits_t;

/**
 * @brief Get pointer to Flash Sequencer Setup Init Register (FSUINITR)
 * @return Volatile pointer to FSUINITR register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* fsuinitr_reg(void)
{
  return (volatile uint16_t*)k_fsuinitr_addr;
}

/* =============================================================================
 * FSADDR / FEADDR - FACI Address Registers @ 0x007FE030 / 0x007FE034
 * =============================================================================
 */

/**
 * @brief Get pointer to FACI Start Address Register (FSADDR)
 * @return Volatile pointer to FSADDR register
 * @since Version 1.0.0
 */
static inline volatile uint32_t* fsaddr_reg(void)
{
  return (volatile uint32_t*)k_fsaddr_addr;
}

/**
 * @brief Get pointer to FACI End Address Register (FEADDR)
 * @return Volatile pointer to FEADDR register
 * @since Version 1.0.0
 */
static inline volatile uint32_t* feaddr_reg(void)
{
  return (volatile uint32_t*)k_feaddr_addr;
}

/* =============================================================================
 * FCMDR - FACI Command Register @ 0x007FE0A0
 * =============================================================================
 */

/**
 * @enum rx_fcmdr_bits_t
 * @brief FACI Command Register (FCMDR) bit definitions
 *
 * @details
 * FCMDR @ 0x007FE0A0 contains the two most recent commands received.
 * Read-only register for debugging.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_fcmdr_pcmdr_mask  = 0xFF00, /**< Previous command mask */
  k_fcmdr_pcmdr_shift = 8,      /**< Previous command position */
  k_fcmdr_cmdr_mask   = 0x00FF, /**< Current command mask */
} rx_fcmdr_bits_t;

/**
 * @brief Get pointer to FACI Command Register (FCMDR)
 * @return Volatile pointer to FCMDR register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* fcmdr_reg(void)
{
  return (volatile uint16_t*)k_fcmdr_addr;
}

/* =============================================================================
 * FACI Commands - Written to 0x007E0000
 * =============================================================================
 */

/**
 * @enum rx_faci_commands_t
 * @brief FACI command codes
 *
 * @details
 * Commands are written to the FACI command issuing area @ 0x007E0000.
 * Command sequences vary; see manual Ch62 Section 62.8.1 for details.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_faci_cmd_program      = 0xE8, /**< Program command */
  k_faci_cmd_block_erase  = 0x20, /**< Block erase command */
  k_faci_cmd_pe_suspend   = 0xB0, /**< P/E suspend command */
  k_faci_cmd_pe_resume    = 0xD0, /**< P/E resume command */
  k_faci_cmd_status_clear = 0x50, /**< Status clear command */
  k_faci_cmd_forced_stop  = 0xB3, /**< Forced stop command */
  k_faci_cmd_blank_check  = 0x71, /**< Blank check command */
  k_faci_cmd_config_set   = 0x40, /**< Configuration set command */
  k_faci_cmd_lockbit_read = 0x71, /**< Lock bit read command */
  k_faci_cmd_d0           = 0xD0, /**< Command terminator */
} rx_faci_commands_t;

/**
 * @brief Get pointer to FACI Command Issuing Area
 *
 * @details
 * Commands are issued by writing to this 4-byte area.
 * The write sequence depends on the command type.
 *
 * @return Volatile pointer to FACI command area
 * @since Version 1.0.0
 */
static inline volatile uint8_t* faci_cmd_area(void)
{
  return (volatile uint8_t*)k_faci_cmd_area;
}

/* =============================================================================
 * FPCKAR - Flash Sequencer Clock Frequency Notification @ 0x007FE0E4
 * =============================================================================
 */

/**
 * @enum rx_fpckar_bits_t
 * @brief Flash Sequencer Clock Notification Register (FPCKAR) bit definitions
 *
 * @details
 * FPCKAR @ 0x007FE0E4 notifies the flash sequencer of FCLK frequency.
 * Must be set before issuing FACI commands for correct timing.
 * Write with KEY=0x1E to modify PCKA field.
 *
 * @par PCKA Values
 * Set PCKA = FCLK frequency in MHz (e.g., 60 for 60 MHz FCLK)
 * Valid range: 4 to 60 MHz
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_fpckar_pcka_mask = 0x00FF, /**< PCKA frequency field mask */
  k_fpckar_key_shift = 8,      /**< Key code position */
  k_fpckar_key       = 0x1E00, /**< Key code for write enable */
} rx_fpckar_bits_t;

/**
 * @brief Get pointer to Flash Sequencer Clock Register (FPCKAR)
 * @return Volatile pointer to FPCKAR register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* fpckar_reg(void)
{
  return (volatile uint16_t*)k_fpckar_addr;
}

/* =============================================================================
 * EEPFCLK - Data Flash Memory Access Frequency @ 0x007FC040
 * =============================================================================
 */

/**
 * @brief Get pointer to Data Flash Access Frequency Register (EEPFCLK)
 * @return Volatile pointer to EEPFCLK register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* eepfclk_reg(void)
{
  return (volatile uint8_t*)k_eepfclk_addr;
}

/* =============================================================================
 * FBCCNT - Blank Check Control @ 0x007FE0D0
 * =============================================================================
 */

/**
 * @enum rx_fbccnt_bits_t
 * @brief Blank Check Control Register (FBCCNT) bit definitions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_fbccnt_bcdir = (1U << 0), /**< Blank Check Direction (0=forward, 1=reverse) */
} rx_fbccnt_bits_t;

/**
 * @brief Get pointer to Blank Check Control Register (FBCCNT)
 * @return Volatile pointer to FBCCNT register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* fbccnt_reg(void)
{
  return (volatile uint8_t*)k_fbccnt_addr;
}

/* =============================================================================
 * FBCSTAT - Blank Check Status @ 0x007FE0D4
 * =============================================================================
 */

/**
 * @enum rx_fbcstat_bits_t
 * @brief Blank Check Status Register (FBCSTAT) bit definitions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_fbcstat_bcst = (1U << 0), /**< Blank Check Status (0=blank, 1=not blank) */
} rx_fbcstat_bits_t;

/**
 * @brief Get pointer to Blank Check Status Register (FBCSTAT)
 * @return Volatile pointer to FBCSTAT register
 * @since Version 1.0.0
 */
static inline volatile uint8_t* fbcstat_reg(void)
{
  return (volatile uint8_t*)k_fbcstat_addr;
}

/* =============================================================================
 * Additional Register Accessors
 * =============================================================================
 */

/**
 * @brief Get pointer to Flash Processing Switch Address (FPSADDR)
 * @return Volatile pointer to FPSADDR register
 * @since Version 1.0.0
 */
static inline volatile uint32_t* fpsaddr_reg(void)
{
  return (volatile uint32_t*)k_fpsaddr_addr;
}

/**
 * @brief Get pointer to Flash Access Window Monitor (FAWMON)
 * @return Volatile pointer to FAWMON register
 * @since Version 1.0.0
 */
static inline volatile uint32_t* fawmon_reg(void)
{
  return (volatile uint32_t*)k_fawmon_addr;
}

/**
 * @brief Get pointer to Flash Sequencer Processing Switch (FCPSR)
 * @return Volatile pointer to FCPSR register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* fcpsr_reg(void)
{
  return (volatile uint16_t*)k_fcpsr_addr;
}

/**
 * @brief Get pointer to Flash Startup Area Select (FSUACR)
 * @return Volatile pointer to FSUACR register
 * @since Version 1.0.0
 */
static inline volatile uint16_t* fsuacr_reg(void)
{
  return (volatile uint16_t*)k_fsuacr_addr;
}

/* =============================================================================
 * Unique ID Registers - Read-only chip identification @ FE7F 7D90h-7D9Ch
 * =============================================================================
 */

/**
 * @enum rx_uidr_addresses_t
 * @brief Unique ID Register addresses (Ch62 Section 62.4.23)
 *
 * @details
 * UIDR0-3 contain a 128-bit unique identifier for each chip.
 * These are read-only registers in the FLASHCONST area (on-chip ROM).
 *
 * @par Memory Map (FLASHCONST area)
 * | Address      | Register | Description                       |
 * |--------------|----------|-----------------------------------|
 * | 0xFE7F7D90   | UIDR0    | Unique ID bits 31-0               |
 * | 0xFE7F7D94   | UIDR1    | Unique ID bits 63-32              |
 * | 0xFE7F7D98   | UIDR2    | Unique ID bits 95-64              |
 * | 0xFE7F7D9C   | UIDR3    | Unique ID bits 127-96             |
 *
 * @par Access Requirements
 * - **Must read in 32-bit units** (as per manual specification)
 * - **SYSCR0.ROME must be 1** (on-chip ROM enabled) - this is the default
 *   in single-chip mode, so no special setup needed for normal firmware
 * - ROMCODE/SPCC security settings do NOT block CPU self-reads (only
 *   external programmer access)
 *
 * @note Values are factory-programmed and unique to each chip
 * @note These addresses are in FLASHCONST area (FE7F xxxx), not the typical
 *       FACI register space (007F xxxx) - this is expected per manual
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_uidr0_addr = 0xFE7F7D90, /**< Unique ID Register 0 (bits 31-0) */
  k_uidr1_addr = 0xFE7F7D94, /**< Unique ID Register 1 (bits 63-32) */
  k_uidr2_addr = 0xFE7F7D98, /**< Unique ID Register 2 (bits 95-64) */
  k_uidr3_addr = 0xFE7F7D9C, /**< Unique ID Register 3 (bits 127-96) */
} rx_uidr_addresses_t;

/**
 * @brief Get pointer to Unique ID Register 0 (UIDR0)
 * @return Volatile pointer to UIDR0 register (read-only)
 * @note Access may be restricted depending on security settings
 * @since Version 1.0.0
 */
static inline volatile const uint32_t* uidr0_reg(void)
{
  return (volatile const uint32_t*)k_uidr0_addr;
}

/**
 * @brief Get pointer to Unique ID Register 1 (UIDR1)
 * @return Volatile pointer to UIDR1 register (read-only)
 * @since Version 1.0.0
 */
static inline volatile const uint32_t* uidr1_reg(void)
{
  return (volatile const uint32_t*)k_uidr1_addr;
}

/**
 * @brief Get pointer to Unique ID Register 2 (UIDR2)
 * @return Volatile pointer to UIDR2 register (read-only)
 * @since Version 1.0.0
 */
static inline volatile const uint32_t* uidr2_reg(void)
{
  return (volatile const uint32_t*)k_uidr2_addr;
}

/**
 * @brief Get pointer to Unique ID Register 3 (UIDR3)
 * @return Volatile pointer to UIDR3 register (read-only)
 * @since Version 1.0.0
 */
static inline volatile const uint32_t* uidr3_reg(void)
{
  return (volatile const uint32_t*)k_uidr3_addr;
}

/* =============================================================================
 * Flash Memory Address Constants
 * =============================================================================
 */

/**
 * @enum rx_flash_memory_addresses_t
 * @brief Flash memory region base addresses
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_code_flash_start = 0xFFC00000, /**< Code flash start (4 MB device) */
  k_code_flash_end   = 0xFFFFFFFF, /**< Code flash end */
  k_data_flash_start = 0x00100000, /**< Data flash start */
  k_data_flash_end   = 0x00107FFF, /**< Data flash end */
} rx_flash_memory_addresses_t;

/**
 * @enum rx_flash_memory_layout_t
 * @brief Flash memory sizes, block sizes, and program units
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_code_flash_size = 0x00400000, /**< Code flash size (4 MB) */
  k_data_flash_size = 0x00008000, /**< Data flash size (32 KB) */

  k_code_flash_block_8k  = 0x00002000, /**< 8 KB block (blocks 0-7) */
  k_code_flash_block_32k = 0x00008000, /**< 32 KB block (blocks 8+) */
  k_data_flash_block_64  = 0x00000040, /**< 64 byte block (data flash) */

  k_code_flash_program_unit = 128U, /**< Code flash program unit (bytes) */
  k_data_flash_program_unit = 4U,   /**< Data flash program unit (bytes) */
} rx_flash_memory_layout_t;

/* =============================================================================
 * Static Assertions - Verify Register Addresses at Compile Time
 * =============================================================================
 */

/* ROM Cache register addresses (Ch62 Section 62.4.1-62.4.4) */
static_assert(k_romce_addr == 0x00081000, "ROMCE address mismatch");
static_assert(k_romciv_addr == 0x00081004, "ROMCIV address mismatch");
static_assert(k_ncrg0_addr == 0x00081040, "NCRG0 address mismatch");
static_assert(k_ncrc0_addr == 0x00081044, "NCRC0 address mismatch");
static_assert(k_ncrg1_addr == 0x00081048, "NCRG1 address mismatch");
static_assert(k_ncrc1_addr == 0x0008104C, "NCRC1 address mismatch");

/* Flash P/E Protect register (Ch62 Section 62.4.5) */
static_assert(k_fwepror_addr == 0x0008C296, "FWEPROR address mismatch");

/* FACI registers (Ch62 Section 62.4.6-62.4.22) */
static_assert(k_fastat_addr == 0x007FE010, "FASTAT address mismatch");
static_assert(k_faeint_addr == 0x007FE014, "FAEINT address mismatch");
static_assert(k_frdyie_addr == 0x007FE018, "FRDYIE address mismatch");
static_assert(k_fsaddr_addr == 0x007FE030, "FSADDR address mismatch");
static_assert(k_feaddr_addr == 0x007FE034, "FEADDR address mismatch");
static_assert(k_fstatr_addr == 0x007FE080, "FSTATR address mismatch");
static_assert(k_fentryr_addr == 0x007FE084, "FENTRYR address mismatch");
static_assert(k_fsuinitr_addr == 0x007FE08C, "FSUINITR address mismatch");
static_assert(k_fcmdr_addr == 0x007FE0A0, "FCMDR address mismatch");
static_assert(k_fbccnt_addr == 0x007FE0D0, "FBCCNT address mismatch");
static_assert(k_fbcstat_addr == 0x007FE0D4, "FBCSTAT address mismatch");
static_assert(k_fpsaddr_addr == 0x007FE0D8, "FPSADDR address mismatch");
static_assert(k_fawmon_addr == 0x007FE0DC, "FAWMON address mismatch");
static_assert(k_fcpsr_addr == 0x007FE0E0, "FCPSR address mismatch");
static_assert(k_fpckar_addr == 0x007FE0E4, "FPCKAR address mismatch");
static_assert(k_fsuacr_addr == 0x007FE0E8, "FSUACR address mismatch");

/* FACI command area (Ch62 Section 62.2) */
static_assert(k_faci_cmd_area == 0x007E0000, "FACI command area address mismatch");

/* Data flash frequency register (Ch62 Section 62.4.22) */
static_assert(k_eepfclk_addr == 0x007FC040, "EEPFCLK address mismatch");

/* Unique ID registers (Ch62 Section 62.4.23) - FLASHCONST area
 * These are in on-chip ROM area (FE7F xxxx). Readable when SYSCR0.ROME=1 (default).
 * Must be read in 32-bit units. ROMCODE/SPCC do NOT block CPU self-reads. */
static_assert(k_uidr0_addr == 0xFE7F7D90, "UIDR0 address mismatch");
static_assert(k_uidr1_addr == 0xFE7F7D94, "UIDR1 address mismatch");
static_assert(k_uidr2_addr == 0xFE7F7D98, "UIDR2 address mismatch");
static_assert(k_uidr3_addr == 0xFE7F7D9C, "UIDR3 address mismatch");

/* Flash memory regions */
static_assert(k_code_flash_start == 0xFFC00000, "Code flash start address mismatch");
static_assert(k_data_flash_start == 0x00100000, "Data flash start address mismatch");

#ifdef __cplusplus
}
#endif
