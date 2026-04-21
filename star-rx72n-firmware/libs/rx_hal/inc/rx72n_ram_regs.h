/**
 * @file rx72n_ram_regs.h
 * @brief RX72N RAM and Expansion RAM register definitions
 *
 * @details
 * This header defines register structures and addresses for the two
 * parity-checked static RAM regions on the RX72N MCU:
 *
 * - **RAM** (512 KB at 0x00000000-0x0007FFFF): 1-bit parity checking, Memory Bus 1
 * - **Expansion RAM** (512 KB at 0x00800000-0x0087FFFF): 1-bit parity checking, Memory Bus 3
 *
 * The ECC-protected 32 KB region (ECCRAM) has its own header,
 * rx72n_eccram_regs.h, since its control block and usage pattern
 * differ substantially from the parity-only regions.
 *
 * All register addresses are verified against the RX72N User's Manual Chapter 60.
 *
 * @par Memory Map (from Manual Ch60, Table 60.1):
 * | Type          | Address Range             | Size   | Error Checking            |
 * |---------------|---------------------------|--------|---------------------------|
 * | RAM           | 0x00000000 - 0x0007FFFF   | 512 KB | 1-bit parity detection    |
 * | Expansion RAM | 0x00800000 - 0x0087FFFF   | 512 KB | 1-bit parity detection    |
 *
 * @par Register Base Addresses:
 * - RAM registers: 0x00081200
 * - Expansion RAM registers: 0x00081240
 *
 * @note All RAM regions operate after reset without module stop.
 * @note Data is NOT retained during deep software standby mode.
 * @note When ICLK > 120 MHz, MEMWAIT must be set to 1 for Expansion RAM.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: Static assertions verify all register addresses
 * - Rule 8: All constants use typed enums, no macros for values
 *
 * @see rx72n_eccram_regs.h ECC-protected RAM definitions
 * @see RX72N User's Manual Chapter 60 (RAM)
 * @see RX72N User's Manual Chapter 11 (Low Power Consumption) for MSTPCRC
 * @see RX72N User's Manual Section 9.2.2 (MEMWAIT register)
 *
 * @since Version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * MEMORY REGION CONSTANTS
 * ============================================================================ */

/**
 * @enum rx_ram_region_addr_t
 * @brief RAM region base addresses and sizes
 *
 * @details
 * Defines the address ranges for all RAM regions on the RX72N.
 * These are from Manual Ch60, Table 60.1.
 */
typedef enum : uintptr_t {
  /** @brief RAM start address (512 KB, Memory Bus 1) */
  k_rx_ram_base_addr = 0x00000000,
  /** @brief RAM end address (inclusive) */
  k_rx_ram_end_addr = 0x0007FFFF,
  /** @brief RAM size in bytes */
  k_rx_ram_size = 0x00080000, /**< 512 KB */

  /** @brief Expansion RAM start address (512 KB, Memory Bus 3) */
  k_rx_exram_base_addr = 0x00800000,
  /** @brief Expansion RAM end address (inclusive) */
  k_rx_exram_end_addr = 0x0087FFFF,
  /** @brief Expansion RAM size in bytes */
  k_rx_exram_size = 0x00080000, /**< 512 KB */
} rx_ram_region_addr_t;

/* ============================================================================
 * REGISTER BASE ADDRESSES
 * ============================================================================ */

/**
 * @enum rx_ram_reg_addr_t
 * @brief RAM control register base addresses
 *
 * @details
 * Defines register base addresses for RAM control, verified against Ch60.
 */
typedef enum : uintptr_t {
  /** @brief RAM control registers base address */
  k_rx_ram_reg_base_addr = 0x00081200,
  /** @brief Expansion RAM control registers base address */
  k_rx_exram_reg_base_addr = 0x00081240,
} rx_ram_reg_addr_t;

/* ============================================================================
 * RAM REGISTER OFFSETS
 * ============================================================================ */

/**
 * @enum rx_ram_reg_offset_t
 * @brief RAM register offsets from base 0x00081200
 *
 * @details
 * Offsets verified against Manual Ch60, sections 60.2.1-60.2.4.
 */
typedef enum : uint8_t {
  /** @brief RAMMODE @ 0x00081200 - RAM Operating Mode Control Register */
  k_rx_ram_offset_rammode = 0x00,
  /** @brief RAMSTS @ 0x00081201 - RAM Error Status Register */
  k_rx_ram_offset_ramsts = 0x01,
  /** @brief RAMPRCR @ 0x00081204 - RAM Protection Register */
  k_rx_ram_offset_ramprcr = 0x04,
  /** @brief RAMECAD @ 0x00081208 - RAM Error Address Capture Register */
  k_rx_ram_offset_ramecad = 0x08,
} rx_ram_reg_offset_t;

/**
 * @enum rx_exram_reg_offset_t
 * @brief Expansion RAM register offsets from base 0x00081240
 *
 * @details
 * Offsets verified against Manual Ch60, sections 60.2.5-60.2.8.
 */
typedef enum : uint8_t {
  /** @brief EXRAMMODE @ 0x00081240 - Expansion RAM Operating Mode Control Register */
  k_rx_exram_offset_exrammode = 0x00,
  /** @brief EXRAMSTS @ 0x00081241 - Expansion RAM Error Status Register */
  k_rx_exram_offset_exramsts = 0x01,
  /** @brief EXRAMPRCR @ 0x00081244 - Expansion RAM Protection Register */
  k_rx_exram_offset_exramprcr = 0x04,
  /** @brief EXRAMECAD @ 0x00081248 - Expansion RAM Error Address Capture Register */
  k_rx_exram_offset_exramecad = 0x08,
} rx_exram_reg_offset_t;

/* ============================================================================
 * BIT FIELD DEFINITIONS
 * ============================================================================ */

/**
 * @enum rx_rammode_bits_t
 * @brief RAMMODE register bit definitions
 *
 * @details
 * RAM Operating Mode Select bits from Manual Ch60, section 60.2.1.
 */
typedef enum : uint8_t {
  /** @brief Parity checking disabled (RAMMODE[1:0] = 00b) */
  k_rx_rammode_parity_disabled = 0x00,
  /** @brief Parity checking enabled (RAMMODE[1:0] = 01b) */
  k_rx_rammode_parity_enabled = 0x01,
} rx_rammode_bits_t;

/**
 * @enum rx_ramsts_bits_t
 * @brief RAMSTS register bit definitions
 *
 * @details
 * RAM Error Status Flag from Manual Ch60, section 60.2.2.
 */
typedef enum : uint8_t {
  /** @brief RAM error flag bit position */
  k_rx_ramsts_ramerr_bit = 0,
  /** @brief RAM error flag mask */
  k_rx_ramsts_ramerr_mask = 0x01,
} rx_ramsts_bits_t;

/**
 * @enum rx_ramprcr_bits_t
 * @brief RAMPRCR register bit definitions
 *
 * @details
 * RAM Protection Register from Manual Ch60, section 60.2.4.
 * Write 0xF1 to enable writing to RAMMODE register.
 */
typedef enum : uint8_t {
  /** @brief RAMPRCR bit position (enable RAMMODE writes) */
  k_rx_ramprcr_ramprcr_bit = 0,
  /** @brief RAMPRCR enable mask */
  k_rx_ramprcr_ramprcr_mask = 0x01,
  /** @brief Key byte: KW[6:0] = 1111000b placed in bits[7:1] of register */
  k_rx_ramprcr_key = 0xF0,
  /** @brief Complete value to enable RAMMODE writes (key + RAMPRCR=1) */
  k_rx_ramprcr_unlock = 0xF1,
  /** @brief Complete value to disable RAMMODE writes (key + RAMPRCR=0) */
  k_rx_ramprcr_lock = 0xF0,
} rx_ramprcr_bits_t;

/**
 * @enum rx_exrammode_bits_t
 * @brief EXRAMMODE register bit definitions
 *
 * @details
 * Expansion RAM Operating Mode Select from Manual Ch60, section 60.2.5.
 */
typedef enum : uint8_t {
  /** @brief Parity checking disabled (EXRAMMODE[1:0] = 00b) */
  k_rx_exrammode_parity_disabled = 0x00,
  /** @brief Parity checking enabled (EXRAMMODE[1:0] = 01b) */
  k_rx_exrammode_parity_enabled = 0x01,
} rx_exrammode_bits_t;

/**
 * @enum rx_exramsts_bits_t
 * @brief EXRAMSTS register bit definitions
 *
 * @details
 * Expansion RAM Error Status Flag from Manual Ch60, section 60.2.6.
 */
typedef enum : uint8_t {
  /** @brief Expansion RAM error flag bit position */
  k_rx_exramsts_exramerr_bit = 0,
  /** @brief Expansion RAM error flag mask */
  k_rx_exramsts_exramerr_mask = 0x01,
} rx_exramsts_bits_t;

/**
 * @enum rx_exramprcr_bits_t
 * @brief EXRAMPRCR register bit definitions
 *
 * @details
 * Expansion RAM Protection Register from Manual Ch60, section 60.2.8.
 * Write 0xF1 to enable writing to EXRAMMODE register.
 */
typedef enum : uint8_t {
  /** @brief EXRAMPRCR bit position */
  k_rx_exramprcr_exramprcr_bit = 0,
  /** @brief EXRAMPRCR enable mask */
  k_rx_exramprcr_exramprcr_mask = 0x01,
  /** @brief Key byte: KW[6:0] = 1111000b placed in bits[7:1] of register */
  k_rx_exramprcr_key = 0xF0,
  /** @brief Complete value to enable EXRAMMODE writes */
  k_rx_exramprcr_unlock = 0xF1,
  /** @brief Complete value to disable EXRAMMODE writes */
  k_rx_exramprcr_lock = 0xF0,
} rx_exramprcr_bits_t;

/**
 * @enum rx_ram_mstpcrc_bits_t
 * @brief MSTPCRC register bits for parity-RAM module stop control
 *
 * @details
 * Module stop control from Manual Ch60, section 60.4.1.
 * Setting these bits stops clock supply to the respective RAM areas.
 *
 * @note The MSTPC6 bit for ECCRAM lives in rx72n_eccram_regs.h.
 */
typedef enum : uint32_t {
  /** @brief MSTPC0 - RAM module stop bit (stops RAM clock) */
  k_rx_mstpcrc_ram_bit = 0,
  /** @brief MSTPC2 - Expansion RAM module stop bit */
  k_rx_mstpcrc_exram_bit = 2,
} rx_ram_mstpcrc_bits_t;

/* ============================================================================
 * REGISTER STRUCTURES
 * ============================================================================ */

/**
 * @struct rx_ram_regs_t
 * @brief RAM control register structure
 *
 * @details
 * Memory-mapped structure for RAM control registers at 0x00081200.
 * Verified against Manual Ch60, sections 60.2.1-60.2.4.
 */
typedef struct {
  volatile uint8_t  rammode;      /**< 0x00: RAM Operating Mode Control Register */
  volatile uint8_t  ramsts;       /**< 0x01: RAM Error Status Register */
  volatile uint8_t  reserved1[2]; /**< 0x02-0x03: Reserved */
  volatile uint8_t  ramprcr;      /**< 0x04: RAM Protection Register */
  volatile uint8_t  reserved2[3]; /**< 0x05-0x07: Reserved */
  volatile uint32_t ramecad;      /**< 0x08: RAM Error Address Capture Register */
} rx_ram_regs_t;

/**
 * @struct rx_exram_regs_t
 * @brief Expansion RAM control register structure
 *
 * @details
 * Memory-mapped structure for Expansion RAM control registers at 0x00081240.
 * Verified against Manual Ch60, sections 60.2.5-60.2.8.
 */
typedef struct {
  volatile uint8_t  exrammode;    /**< 0x00: Expansion RAM Operating Mode Control Register */
  volatile uint8_t  exramsts;     /**< 0x01: Expansion RAM Error Status Register */
  volatile uint8_t  reserved1[2]; /**< 0x02-0x03: Reserved */
  volatile uint8_t  exramprcr;    /**< 0x04: Expansion RAM Protection Register */
  volatile uint8_t  reserved2[3]; /**< 0x05-0x07: Reserved */
  volatile uint32_t exramecad;    /**< 0x08: Expansion RAM Error Address Capture Register */
} rx_exram_regs_t;

/* ============================================================================
 * INLINE ACCESSOR FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get pointer to RAM control registers
 *
 * @details
 * Returns a pointer to the RAM control register structure at 0x00081200.
 *
 * @return Pointer to rx_ram_regs_t structure
 *
 * @par Example:
 * @code
 * // Enable parity checking for RAM
 * ram_regs()->ramprcr = k_rx_ramprcr_unlock;  // Unlock
 * ram_regs()->rammode = k_rx_rammode_parity_enabled;
 * ram_regs()->ramprcr = k_rx_ramprcr_lock;    // Lock
 * @endcode
 */
static inline rx_ram_regs_t* ram_regs(void)
{
  return (rx_ram_regs_t*)k_rx_ram_reg_base_addr;
}

/**
 * @brief Get pointer to Expansion RAM control registers
 *
 * @details
 * Returns a pointer to the Expansion RAM control register structure at 0x00081240.
 *
 * @return Pointer to rx_exram_regs_t structure
 *
 * @par Example:
 * @code
 * // Enable parity checking for Expansion RAM
 * exram_regs()->exramprcr = k_rx_exramprcr_unlock;  // Unlock
 * exram_regs()->exrammode = k_rx_exrammode_parity_enabled;
 * exram_regs()->exramprcr = k_rx_exramprcr_lock;    // Lock
 * @endcode
 */
static inline rx_exram_regs_t* exram_regs(void)
{
  return (rx_exram_regs_t*)k_rx_exram_reg_base_addr;
}

/* ============================================================================
 * STATIC ASSERTIONS - Address Verification
 * ============================================================================ */

/* RAM register structure size and offset verification */
static_assert(sizeof(rx_ram_regs_t) == 12, "rx_ram_regs_t size must be 12 bytes");
static_assert(offsetof(rx_ram_regs_t, rammode) == 0x00, "RAMMODE offset must be 0x00");
static_assert(offsetof(rx_ram_regs_t, ramsts) == 0x01, "RAMSTS offset must be 0x01");
static_assert(offsetof(rx_ram_regs_t, ramprcr) == 0x04, "RAMPRCR offset must be 0x04");
static_assert(offsetof(rx_ram_regs_t, ramecad) == 0x08, "RAMECAD offset must be 0x08");

/* Expansion RAM register structure size and offset verification */
static_assert(sizeof(rx_exram_regs_t) == 12, "rx_exram_regs_t size must be 12 bytes");
static_assert(offsetof(rx_exram_regs_t, exrammode) == 0x00, "EXRAMMODE offset must be 0x00");
static_assert(offsetof(rx_exram_regs_t, exramsts) == 0x01, "EXRAMSTS offset must be 0x01");
static_assert(offsetof(rx_exram_regs_t, exramprcr) == 0x04, "EXRAMPRCR offset must be 0x04");
static_assert(offsetof(rx_exram_regs_t, exramecad) == 0x08, "EXRAMECAD offset must be 0x08");

/* Base address verification */
static_assert(k_rx_ram_reg_base_addr == 0x00081200, "RAM register base must be 0x00081200");
static_assert(k_rx_exram_reg_base_addr == 0x00081240,
              "Expansion RAM register base must be 0x00081240");

/* Memory region verification */
static_assert(k_rx_ram_base_addr == 0x00000000, "RAM base must be 0x00000000");
static_assert(k_rx_ram_size == 0x00080000, "RAM size must be 512 KB (0x80000)");
static_assert(k_rx_exram_base_addr == 0x00800000, "Expansion RAM base must be 0x00800000");
static_assert(k_rx_exram_size == 0x00080000, "Expansion RAM size must be 512 KB (0x80000)");

/* Protection register key values */
static_assert(k_rx_ramprcr_unlock == 0xF1, "RAMPRCR unlock value must be 0xF1");
static_assert(k_rx_exramprcr_unlock == 0xF1, "EXRAMPRCR unlock value must be 0xF1");

#ifdef __cplusplus
}
#endif
