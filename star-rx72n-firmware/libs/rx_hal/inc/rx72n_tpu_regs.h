/**
 * @file rx72n_tpu_regs.h
 * @brief RX72N 16-Bit Timer Pulse Unit (TPU) Register Definitions
 *
 * @details
 * Register definitions for the TPU module which provides 6 channels of 16-bit
 * timers with quadrature encoder phase counting capability. In STAR, the TPU
 * is used for rear wheel encoder decoding via hardware phase counting mode.
 *
 * @par STAR Project Quadrature Encoder System
 * @dot
 * digraph tpu_system {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_encoders {
 *     label="Rear Wheel Encoders";
 *     style=filled;
 *     color=lightblue;
 *
 *     enc2 [label="Encoder 2\n(Back Right)\n341 PPR"];
 *     enc3 [label="Encoder 3\n(Back Left)\n341 PPR"];
 *   }
 *
 *   subgraph cluster_pins {
 *     label="External Clock Pins";
 *     style=filled;
 *     color=lightyellow;
 *
 *     tclka [label="TCLKA\nPC2 (pin 70)\nA-Phase"];
 *     tclkb [label="TCLKB\nPA3 (pin 94)\nB-Phase"];
 *     tclkc [label="TCLKC\nPC0 (pin 75)\nA-Phase"];
 *     tclkd [label="TCLKD\nPB3 (pin 82)\nB-Phase"];
 *   }
 *
 *   subgraph cluster_tpu {
 *     label="TPU Phase Counting (0x00088100)";
 *     style=filled;
 *     color=lightgreen;
 *
 *     tpu1 [label="TPU1\nPhase Count Mode 4\n(4x resolution)"];
 *     tpu2 [label="TPU2\nPhase Count Mode 4\n(4x resolution)"];
 *   }
 *
 *   subgraph cluster_output {
 *     label="Motor Control";
 *     style=filled;
 *     color=lightsalmon;
 *
 *     ctrl [label="motor_control_task\n250 Hz loop"];
 *   }
 *
 *   enc2 -> tclka [label="A"];
 *   enc2 -> tclkb [label="B"];
 *   enc3 -> tclkc [label="A"];
 *   enc3 -> tclkd [label="B"];
 *   tclka -> tpu1;
 *   tclkb -> tpu1;
 *   tclkc -> tpu2;
 *   tclkd -> tpu2;
 *   tpu1 -> ctrl [label="TCNT + TCFD"];
 *   tpu2 -> ctrl [label="TCNT + TCFD"];
 * }
 * @enddot
 *
 * @par TPU Channel Assignments in STAR
 * | Channel | Type     | Use in STAR         | Phase Count | Clock Pins        |
 * |---------|----------|---------------------|-------------|-------------------|
 * | TPU0    | Extended | Not used            | No          | -                 |
 * | TPU1    | Basic    | Back-right encoder  | Yes (mode 4)| TCLKA + TCLKB    |
 * | TPU2    | Basic    | Back-left encoder   | Yes (mode 4)| TCLKC + TCLKD    |
 * | TPU3    | Extended | Not used            | No          | -                 |
 * | TPU4    | Basic    | Not used (paired 2) | Yes         | TCLKC + TCLKD    |
 * | TPU5    | Basic    | Not used (paired 1) | Yes         | TCLKA + TCLKB    |
 *
 * @par Phase Counting Mode 4 (4x Resolution)
 * Counts on all edges of both A-phase and B-phase inputs, providing 4x the
 * base encoder resolution. For 341 PPR encoders: 341 * 4 = 1364 counts/rev.
 *
 * | A-Phase Edge | B-Phase Level | Direction |
 * |--------------|---------------|-----------|
 * | Rising       | Low           | Up        |
 * | Rising       | High          | Down      |
 * | Falling      | Low           | Down      |
 * | Falling      | High          | Up        |
 * | (same for B-phase edges with A-phase level swapped) |
 *
 * @par Key Features
 * - 6 channels of 16-bit timers (TPU0-TPU5)
 * - Hardware quadrature phase counting on TPU1/2/4/5
 * - 4 phase counting modes (1x, 2x, 3x, 4x resolution)
 * - TSR.TCFD direction flag for up/down detection
 * - Overflow/underflow interrupt support
 * - Noise filter on I/O pins
 * - Synchronous operation between channels
 *
 * @par Hardware Requirements
 * | Parameter      | Value            | Notes                            |
 * |----------------|------------------|----------------------------------|
 * | Control Base   | 0x00088100       | TSTR, TSYR, NFCR registers       |
 * | Channel Spacing| 0x10 bytes       | TPU0=0x110, TPU1=0x120, etc.     |
 * | Module Stop    | MSTPCRA.MSTPA13  | 0=run, 1=stop                    |
 * | Clock          | PCLKB (60 MHz)   | For noise filter and prescaler   |
 *
 * @par Register Map Overview
 * | Address        | Register         | Size | Description                  |
 * |----------------|------------------|------|------------------------------|
 * | 0x00088100     | TSTR             | 8    | Timer Start Register         |
 * | 0x00088101     | TSYR             | 8    | Timer Synchronous Register   |
 * | 0x00088108-0D  | NFCRn            | 8x6  | Noise Filter Control (0-5)   |
 * | 0x00088110-1F  | TPU0 registers   | 16   | Extended channel (4 TGR)     |
 * | 0x00088120-2F  | TPU1 registers   | 16   | Basic channel (2 TGR)        |
 * | 0x00088130-3F  | TPU2 registers   | 16   | Basic channel (2 TGR)        |
 * | 0x00088140-4F  | TPU3 registers   | 16   | Extended channel (4 TGR)     |
 * | 0x00088150-5F  | TPU4 registers   | 16   | Basic channel (2 TGR)        |
 * | 0x00088160-6F  | TPU5 registers   | 16   | Basic channel (2 TGR)        |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: N/A (no loops in register definitions)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: All accessor functions are single-statement
 * - Rule 5: N/A (hardware layer)
 * - Rule 6: Minimal scope - inline accessors only
 * - Rule 7: N/A (no return values to check)
 * - Rule 8: All constants use C23 typed enums
 * - Rule 9: No function pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single responsibility - only TPU register definitions
 * - **O**: Open for extension via additional channel configurations
 * - **L**: N/A (no inheritance hierarchy)
 * - **I**: Minimal interface - only necessary accessor functions
 * - **D**: N/A (hardware register layer)
 *
 * @par Module Dependencies
 * - `<stdint.h>` - Fixed-width integer types
 * - `<stddef.h>` - offsetof macro for static assertions
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 28: 16-Bit Timer Pulse Unit (TPUa), pages 1442-1508
 *
 * @see rx72n_mtu_regs.h MTU registers (front wheel encoders)
 * @see rx72n_regs.h Main register include file
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * TPU - 16-Bit Timer Pulse Unit for Quadrature Encoder Phase Counting
 * =============================================================================
 */

/**
 * @enum rx_tpu_addresses_t
 * @brief TPU base addresses
 *
 * @details
 * Base addresses for the TPU control block and individual channel register
 * blocks. The control block contains TSTR, TSYR, and NFCR registers shared
 * across all channels. Each channel has a 16-byte register block.
 *
 * @par Memory Map
 * | Address      | Block    | Description                               |
 * |--------------|----------|-------------------------------------------|
 * | 0x00088100   | Control  | TSTR, TSYR, NFCR (shared across channels) |
 * | 0x00088110   | TPU0     | Extended channel (TIORH/L, TGRA-TGRD)    |
 * | 0x00088120   | TPU1     | Basic channel (TIOR, TGRA-TGRB)          |
 * | 0x00088130   | TPU2     | Basic channel (TIOR, TGRA-TGRB)          |
 * | 0x00088140   | TPU3     | Extended channel (TIORH/L, TGRA-TGRD)    |
 * | 0x00088150   | TPU4     | Basic channel (TIOR, TGRA-TGRB)          |
 * | 0x00088160   | TPU5     | Basic channel (TIOR, TGRA-TGRB)          |
 *
 * @note Manual: Ch28.2 - Register Descriptions
 * @see tpu_control() Accessor for control block
 * @see tpu1() Accessor for TPU1 channel (rear-left encoder)
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief TPU control block base address (0x00088100)
   * @details Contains TSTR, TSYR at +0x00/+0x01 and NFCR[0-5] at +0x08
   */
  k_tpu_control_base_addr = 0x00088100,

  /**
   * @brief TPU0 channel base address (0x00088110) - Extended channel
   * @details Extended channel with TIORH/TIORL and TGRA through TGRD
   */
  k_tpu0_base_addr = 0x00088110,

  /**
   * @brief TPU1 channel base address (0x00088120) - Basic channel
   * @details Back-right encoder, phase counting capable (TCLKA/TCLKB)
   */
  k_tpu1_base_addr = 0x00088120,

  /**
   * @brief TPU2 channel base address (0x00088130) - Basic channel
   * @details Back-left encoder, phase counting capable (TCLKC/TCLKD)
   */
  k_tpu2_base_addr = 0x00088130,

  /**
   * @brief TPU3 channel base address (0x00088140) - Extended channel
   * @details Extended channel with TIORH/TIORL and TGRA through TGRD
   */
  k_tpu3_base_addr = 0x00088140,

  /**
   * @brief TPU4 channel base address (0x00088150) - Basic channel
   * @details Phase counting capable, paired with TPU2 (TCLKC/TCLKD)
   */
  k_tpu4_base_addr = 0x00088150,

  /**
   * @brief TPU5 channel base address (0x00088160) - Basic channel
   * @details Phase counting capable, paired with TPU1 (TCLKA/TCLKB)
   */
  k_tpu5_base_addr = 0x00088160,

  /**
   * @brief Channel register block spacing (0x10 bytes between channels)
   * @details Each TPU channel occupies a 16-byte register block
   */
  k_tpu_channel_spacing = 0x10,
} rx_tpu_addresses_t;

/* =============================================================================
 * Control Block Registers (TSTR, TSYR, NFCR)
 * =============================================================================
 */

/**
 * @enum rx_tpu_channel_count_t
 * @brief TPU channel count constant
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_channel_count = 6, /**< Total TPU channels (TPU0-TPU5) */
} rx_tpu_channel_count_t;

/**
 * @struct rx_tpu_control_regs_t
 * @brief TPU Control Block Register Structure
 *
 * @details
 * Contains the shared control registers for all TPU channels: the Timer
 * Start Register (TSTR), Timer Synchronous Register (TSYR), and per-channel
 * Noise Filter Control Registers (NFCR).
 *
 * @par Memory Layout Table
 * | Offset | Size | Field       | Type      | Description                     |
 * |--------|------|-------------|-----------|---------------------------------|
 * | 0x00   | 1    | tstr        | uint8_t   | Timer Start Register            |
 * | 0x01   | 1    | tsyr        | uint8_t   | Timer Synchronous Register      |
 * | 0x02   | 6    | reserved    | uint8_t[] | Reserved                        |
 * | 0x08   | 6    | nfcr[0..5]  | uint8_t[] | Noise Filter Control (per ch)   |
 * | **Total** | **14** |          |           |                                 |
 *
 * @invariant sizeof(rx_tpu_control_regs_t) == 14 bytes
 *
 * @see tpu_control() Accessor function
 * @see rx_tpu_tstr_bits_t TSTR bit definitions
 * @see rx_tpu_tsyr_bits_t TSYR bit definitions
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Timer Start Register (TSTR) @ offset 0x00
   *
   * @details
   * Starts or stops TCNT operation for TPU0 through TPU5. Each bit
   * independently controls one channel's counter.
   *
   * @par Bit Layout (8-bit)
   * | Bits  | Field | R/W | Description                   |
   * |-------|-------|-----|-------------------------------|
   * | 0     | CST0  | R/W | TPU0 counter start (1=run)    |
   * | 1     | CST1  | R/W | TPU1 counter start (1=run)    |
   * | 2     | CST2  | R/W | TPU2 counter start (1=run)    |
   * | 3     | CST3  | R/W | TPU3 counter start (1=run)    |
   * | 4     | CST4  | R/W | TPU4 counter start (1=run)    |
   * | 5     | CST5  | R/W | TPU5 counter start (1=run)    |
   * | 7:6   | -     | R/W | Reserved (write 0)            |
   *
   * @see rx_tpu_tstr_bits_t Bit definitions
   */
  volatile uint8_t tstr;

  /**
   * @brief Timer Synchronous Register (TSYR) @ offset 0x01
   *
   * @details
   * Selects independent or synchronous operation for TCNT of TPU0
   * through TPU5. When synchronous, channels share counter writes.
   *
   * @par Bit Layout (8-bit)
   * | Bits  | Field  | R/W | Description                     |
   * |-------|--------|-----|---------------------------------|
   * | 0     | SYNC0  | R/W | TPU0 synchronous (1=enabled)    |
   * | 1     | SYNC1  | R/W | TPU1 synchronous (1=enabled)    |
   * | 2     | SYNC2  | R/W | TPU2 synchronous (1=enabled)    |
   * | 3     | SYNC3  | R/W | TPU3 synchronous (1=enabled)    |
   * | 4     | SYNC4  | R/W | TPU4 synchronous (1=enabled)    |
   * | 5     | SYNC5  | R/W | TPU5 synchronous (1=enabled)    |
   * | 7:6   | -      | R/W | Reserved (write 0)              |
   *
   * @see rx_tpu_tsyr_bits_t Bit definitions
   */
  volatile uint8_t tsyr;

  /**
   * @brief Reserved bytes @ offset 0x02-0x07
   */
  volatile uint8_t reserved[6];

  /**
   * @brief Noise Filter Control Registers (NFCR) @ offset 0x08-0x0D
   *
   * @details
   * Per-channel noise filter configuration. Index 0 = TPU0.NFCR through
   * index 5 = TPU5.NFCR. Set only while TCNT is stopped.
   *
   * @par Bit Layout (8-bit, per channel)
   * | Bits | Field      | R/W | Description                        |
   * |------|------------|-----|------------------------------------|
   * | 0    | NFAEN      | R/W | Noise filter enable TIOCA pin      |
   * | 1    | NFBEN      | R/W | Noise filter enable TIOCB pin      |
   * | 2    | NFCEN      | R/W | Noise filter enable TIOCC (0,3)    |
   * | 3    | NFDEN      | R/W | Noise filter enable TIOCD (0,3)    |
   * | 5:4  | NFCS[1:0]  | R/W | Noise filter clock select          |
   * | 7:6  | -          | R   | Reserved (read as 0)               |
   *
   * @warning Set NFCR only while TCNT is stopped (CSTn = 0)
   * @see rx_tpu_nfcr_bits_t Bit definitions
   */
  volatile uint8_t nfcr[6];
} rx_tpu_control_regs_t;

/* =============================================================================
 * Per-Channel Register Structures
 * =============================================================================
 */

/**
 * @struct rx_tpu_ext_regs_t
 * @brief TPU Extended Channel Register Structure (TPU0 and TPU3)
 *
 * @details
 * Register layout for TPU0 and TPU3 which have two I/O control registers
 * (TIORH + TIORL) and four timer general registers (TGRA through TGRD).
 * These channels do NOT support phase counting mode.
 *
 * @par Memory Layout Table
 * | Offset | Size | Field | Type     | Description                        |
 * |--------|------|-------| ---------|------------------------------------|
 * | 0x00   | 1    | tcr   | uint8_t  | Timer Control Register             |
 * | 0x01   | 1    | tmdr  | uint8_t  | Timer Mode Register                |
 * | 0x02   | 1    | tiorh | uint8_t  | Timer I/O Control Register H       |
 * | 0x03   | 1    | tiorl | uint8_t  | Timer I/O Control Register L       |
 * | 0x04   | 1    | tier  | uint8_t  | Timer Interrupt Enable Register    |
 * | 0x05   | 1    | tsr   | uint8_t  | Timer Status Register              |
 * | 0x06   | 2    | tcnt  | uint16_t | Timer Counter                      |
 * | 0x08   | 2    | tgra  | uint16_t | Timer General Register A           |
 * | 0x0A   | 2    | tgrb  | uint16_t | Timer General Register B           |
 * | 0x0C   | 2    | tgrc  | uint16_t | Timer General Register C           |
 * | 0x0E   | 2    | tgrd  | uint16_t | Timer General Register D           |
 * | **Total** | **16** |    |          |                                    |
 *
 * @invariant sizeof(rx_tpu_ext_regs_t) == 16 bytes
 *
 * @note TPU0 and TPU3 do NOT support phase counting mode
 * @note Buffer operation available: TGRA+TGRC and TGRB+TGRD pairs
 *
 * @see tpu0() Accessor for TPU0
 * @see tpu3() Accessor for TPU3
 * @since Version 1.0.0
 */
typedef struct {
  volatile uint8_t  tcr;   /**< Timer Control Register (TCR) @ +0x00 */
  volatile uint8_t  tmdr;  /**< Timer Mode Register (TMDR) @ +0x01 */
  volatile uint8_t  tiorh; /**< Timer I/O Control Register H (TIORH) @ +0x02 */
  volatile uint8_t  tiorl; /**< Timer I/O Control Register L (TIORL) @ +0x03 */
  volatile uint8_t  tier;  /**< Timer Interrupt Enable Register (TIER) @ +0x04 */
  volatile uint8_t  tsr;   /**< Timer Status Register (TSR) @ +0x05 */
  volatile uint16_t tcnt;  /**< Timer Counter (TCNT) @ +0x06, 16-bit */
  volatile uint16_t tgra;  /**< Timer General Register A (TGRA) @ +0x08 */
  volatile uint16_t tgrb;  /**< Timer General Register B (TGRB) @ +0x0A */
  volatile uint16_t tgrc;  /**< Timer General Register C (TGRC) @ +0x0C */
  volatile uint16_t tgrd;  /**< Timer General Register D (TGRD) @ +0x0E */
} rx_tpu_ext_regs_t;

/**
 * @struct rx_tpu_regs_t
 * @brief TPU Basic Channel Register Structure (TPU1, TPU2, TPU4, TPU5)
 *
 * @details
 * Register layout for TPU1/2/4/5 which have a single I/O control register
 * (TIOR) and two timer general registers (TGRA and TGRB). These channels
 * support phase counting mode for quadrature encoder decoding.
 *
 * @par Memory Layout Table
 * | Offset | Size | Field     | Type     | Description                      |
 * |--------|------|-----------|----------|----------------------------------|
 * | 0x00   | 1    | tcr       | uint8_t  | Timer Control Register           |
 * | 0x01   | 1    | tmdr      | uint8_t  | Timer Mode Register              |
 * | 0x02   | 1    | tior      | uint8_t  | Timer I/O Control Register       |
 * | 0x03   | 1    | reserved  | uint8_t  | Reserved                         |
 * | 0x04   | 1    | tier      | uint8_t  | Timer Interrupt Enable Register  |
 * | 0x05   | 1    | tsr       | uint8_t  | Timer Status Register            |
 * | 0x06   | 2    | tcnt      | uint16_t | Timer Counter (up/down in phase) |
 * | 0x08   | 2    | tgra      | uint16_t | Timer General Register A         |
 * | 0x0A   | 2    | tgrb      | uint16_t | Timer General Register B         |
 * | 0x0C   | 4    | reserved2 | uint8_t[]| Reserved                         |
 * | **Total** | **16** |        |          |                                  |
 *
 * @invariant sizeof(rx_tpu_regs_t) == 16 bytes
 *
 * @par Phase Counting Mode Usage
 * @code{.c}
 * // Configure TPU1 for phase counting mode 4 (4x resolution)
 * volatile rx_tpu_regs_t* ch = tpu1();
 *
 * // 1. Stop counter first
 * tpu_control()->tstr &= (uint8_t)~k_tpu_tstr_cst1;
 *
 * // 2. Set phase counting mode 4 in TMDR
 * ch->tmdr = k_tpu_tmdr_md_phase_count_4;
 *
 * // 3. Start counter
 * tpu_control()->tstr |= k_tpu_tstr_cst1;
 *
 * // 4. Read counter and direction
 * uint16_t count = ch->tcnt;
 * bool counting_up = (ch->tsr & k_tpu_tsr_tcfd) != 0;
 * @endcode
 *
 * @see tpu1() Accessor for back-right encoder
 * @see tpu2() Accessor for back-left encoder
 * @since Version 1.0.0
 */
typedef struct {
  volatile uint8_t  tcr;          /**< Timer Control Register (TCR) @ +0x00 */
  volatile uint8_t  tmdr;         /**< Timer Mode Register (TMDR) @ +0x01 */
  volatile uint8_t  tior;         /**< Timer I/O Control Register (TIOR) @ +0x02 */
  volatile uint8_t  reserved;     /**< Reserved @ +0x03 */
  volatile uint8_t  tier;         /**< Timer Interrupt Enable Register (TIER) @ +0x04 */
  volatile uint8_t  tsr;          /**< Timer Status Register (TSR) @ +0x05 */
  volatile uint16_t tcnt;         /**< Timer Counter (TCNT) @ +0x06, up/down in phase mode */
  volatile uint16_t tgra;         /**< Timer General Register A (TGRA) @ +0x08 */
  volatile uint16_t tgrb;         /**< Timer General Register B (TGRB) @ +0x0A */
  volatile uint8_t  reserved2[4]; /**< Reserved @ +0x0C-0x0F */
} rx_tpu_regs_t;

/* =============================================================================
 * Inline Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to TPU control block registers
 *
 * @details
 * Returns a volatile pointer to the TPU control register block at address
 * 0x00088100. Contains TSTR (timer start), TSYR (synchronous), and
 * NFCR (noise filter) registers shared across all channels.
 *
 * @return Volatile pointer to TPU control register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @par Example:
 * @code{.c}
 * // Start TPU1 and TPU2 counters simultaneously
 * tpu_control()->tstr |= (k_tpu_tstr_cst1 | k_tpu_tstr_cst2);
 * @endcode
 *
 * @see k_tpu_control_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_control_regs_t* tpu_control(void)
{
  return (volatile rx_tpu_control_regs_t*)k_tpu_control_base_addr;
}

/**
 * @brief Get pointer to TPU0 channel registers (extended)
 *
 * @details
 * Returns a volatile pointer to TPU0 extended channel registers at address
 * 0x00088110. TPU0 has TIORH/TIORL and TGRA through TGRD.
 *
 * @return Volatile pointer to TPU0 extended register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note TPU0 does NOT support phase counting mode
 *
 * @see k_tpu0_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_ext_regs_t* tpu0(void)
{
  return (volatile rx_tpu_ext_regs_t*)k_tpu0_base_addr;
}

/**
 * @brief Get pointer to TPU1 channel registers (basic, phase counting)
 *
 * @details
 * Returns a volatile pointer to TPU1 basic channel registers at address
 * 0x00088120. TPU1 supports phase counting mode using TCLKA (A-phase)
 * and TCLKB (B-phase) external clock pins. Used for back-right encoder.
 *
 * @return Volatile pointer to TPU1 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @par Example:
 * @code{.c}
 * // Read back-right encoder count and direction
 * uint16_t count = tpu1()->tcnt;
 * bool forward = (tpu1()->tsr & k_tpu_tsr_tcfd) != 0;
 * @endcode
 *
 * @see k_tpu1_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_regs_t* tpu1(void)
{
  return (volatile rx_tpu_regs_t*)k_tpu1_base_addr;
}

/**
 * @brief Get pointer to TPU2 channel registers (basic, phase counting)
 *
 * @details
 * Returns a volatile pointer to TPU2 basic channel registers at address
 * 0x00088130. TPU2 supports phase counting mode using TCLKC (A-phase)
 * and TCLKD (B-phase) external clock pins. Used for back-left encoder.
 *
 * @return Volatile pointer to TPU2 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @see k_tpu2_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_regs_t* tpu2(void)
{
  return (volatile rx_tpu_regs_t*)k_tpu2_base_addr;
}

/**
 * @brief Get pointer to TPU3 channel registers (extended)
 *
 * @details
 * Returns a volatile pointer to TPU3 extended channel registers at address
 * 0x00088140. TPU3 has TIORH/TIORL and TGRA through TGRD.
 *
 * @return Volatile pointer to TPU3 extended register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note TPU3 does NOT support phase counting mode
 *
 * @see k_tpu3_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_ext_regs_t* tpu3(void)
{
  return (volatile rx_tpu_ext_regs_t*)k_tpu3_base_addr;
}

/**
 * @brief Get pointer to TPU4 channel registers (basic, phase counting)
 *
 * @details
 * Returns a volatile pointer to TPU4 basic channel registers at address
 * 0x00088150. TPU4 supports phase counting mode using TCLKC/TCLKD pins
 * (paired with TPU2).
 *
 * @return Volatile pointer to TPU4 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @see k_tpu4_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_regs_t* tpu4(void)
{
  return (volatile rx_tpu_regs_t*)k_tpu4_base_addr;
}

/**
 * @brief Get pointer to TPU5 channel registers (basic, phase counting)
 *
 * @details
 * Returns a volatile pointer to TPU5 basic channel registers at address
 * 0x00088160. TPU5 supports phase counting mode using TCLKA/TCLKB pins
 * (paired with TPU1).
 *
 * @return Volatile pointer to TPU5 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre TPU module enabled (MSTPCRA.MSTPA13 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @see k_tpu5_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_tpu_regs_t* tpu5(void)
{
  return (volatile rx_tpu_regs_t*)k_tpu5_base_addr;
}

/* =============================================================================
 * TSTR - Timer Start Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_tstr_bits_t
 * @brief TSTR (Timer Start Register) bit definitions
 *
 * @details
 * Each CSTn bit independently starts (1) or stops (0) the corresponding
 * TPUn channel counter. Multiple channels can be started simultaneously
 * by OR-ing the appropriate bits.
 *
 * @note Manual: Ch28.2.8 - Timer Start Register (TSTR)
 * @see rx_tpu_control_regs_t::tstr Register field
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tstr_cst0 = (1U << 0), /**< TPU0 counter start (1=run, 0=stop) */
  k_tpu_tstr_cst1 = (1U << 1), /**< TPU1 counter start (back-right encoder) */
  k_tpu_tstr_cst2 = (1U << 2), /**< TPU2 counter start (back-left encoder) */
  k_tpu_tstr_cst3 = (1U << 3), /**< TPU3 counter start */
  k_tpu_tstr_cst4 = (1U << 4), /**< TPU4 counter start */
  k_tpu_tstr_cst5 = (1U << 5), /**< TPU5 counter start */
} rx_tpu_tstr_bits_t;

/**
 * @enum rx_tpu_tsyr_bits_t
 * @brief TSYR (Timer Synchronous Register) bit definitions
 *
 * @details
 * Each SYNCn bit enables (1) or disables (0) synchronous operation for the
 * corresponding TPUn channel. When multiple channels are set to synchronous,
 * their TCNTs can be written simultaneously.
 *
 * @note Manual: Ch28.2.9 - Timer Synchronous Register (TSYR)
 * @see rx_tpu_control_regs_t::tsyr Register field
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tsyr_sync0 = (1U << 0), /**< TPU0 synchronous operation enable */
  k_tpu_tsyr_sync1 = (1U << 1), /**< TPU1 synchronous operation enable */
  k_tpu_tsyr_sync2 = (1U << 2), /**< TPU2 synchronous operation enable */
  k_tpu_tsyr_sync3 = (1U << 3), /**< TPU3 synchronous operation enable */
  k_tpu_tsyr_sync4 = (1U << 4), /**< TPU4 synchronous operation enable */
  k_tpu_tsyr_sync5 = (1U << 5), /**< TPU5 synchronous operation enable */
} rx_tpu_tsyr_bits_t;

/* =============================================================================
 * TCR - Timer Control Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_tcr_shift_t
 * @brief TCR (Timer Control Register) bit field positions
 *
 * @details
 * Bit positions for the Timer Control Register fields: prescaler select,
 * clock edge select, and counter clear source select.
 *
 * @note In phase counting mode, TPSC and CKEG settings are ignored
 * @see rx_tpu_tcr_cclr_t Counter clear source values
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tcr_tpsc_shift = 0, /**< TPSC[2:0] - Timer Prescaler Select (bits 2:0) */
  k_tpu_tcr_ckeg_shift = 3, /**< CKEG[1:0] - Input Clock Edge Select (bits 4:3) */
  k_tpu_tcr_cclr_shift = 5, /**< CCLR[2:0] - Counter Clear Source (bits 7:5) */
} rx_tpu_tcr_shift_t;

/**
 * @enum rx_tpu_tcr_cclr_t
 * @brief TCR CCLR[2:0] counter clear source values (for basic channels)
 *
 * @details
 * Selects the event that clears TCNT. In phase counting mode, only TGRA
 * and TGRB compare match clearing is available.
 *
 * @note Bit 7 is reserved for TPU1/2/4/5 (basic channels)
 * @see rx_tpu_tcr_shift_t::k_tpu_tcr_cclr_shift Field position
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tcr_cclr_disabled   = (0U << 5), /**< TCNT free-running (no clear) */
  k_tpu_tcr_cclr_tgra_match = (1U << 5), /**< Clear on TGRA compare match */
  k_tpu_tcr_cclr_tgrb_match = (2U << 5), /**< Clear on TGRB compare match */
  k_tpu_tcr_cclr_sync_clear = (3U << 5), /**< Synchronous clear */
} rx_tpu_tcr_cclr_t;

/**
 * @enum rx_tpu_tcr_masks_t
 * @brief TCR register field masks
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tcr_tpsc_mask = 0x07, /**< TPSC[2:0] field mask (bits 2:0) */
  k_tpu_tcr_ckeg_mask = 0x18, /**< CKEG[1:0] field mask (bits 4:3) */
  k_tpu_tcr_cclr_mask = 0xE0, /**< CCLR[2:0] field mask (bits 7:5) */
} rx_tpu_tcr_masks_t;

/* =============================================================================
 * TMDR - Timer Mode Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_tmdr_md_t
 * @brief TMDR MD[3:0] mode select values
 *
 * @details
 * Selects the operating mode for the TPU channel. Phase counting modes
 * (1-4) are only available on TPU1, TPU2, TPU4, and TPU5.
 *
 * @par Phase Counting Mode Comparison
 * | Mode | Resolution | Count Edges                       |
 * |------|------------|-----------------------------------|
 * | 1    | 1x         | A-phase rising only               |
 * | 2    | 1x         | B-phase rising only               |
 * | 3    | 2x         | A-phase rising + falling          |
 * | 4    | 4x         | All edges of both A and B phases  |
 *
 * @note Phase counting mode cannot be set for TPU0 and TPU3
 * @note Manual: Ch28.2.2 - Timer Mode Register (TMDR)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tmdr_md_normal        = 0x00, /**< Normal operation */
  k_tpu_tmdr_md_pwm1          = 0x02, /**< PWM mode 1 */
  k_tpu_tmdr_md_pwm2          = 0x03, /**< PWM mode 2 */
  k_tpu_tmdr_md_phase_count_1 = 0x04, /**< Phase counting mode 1 (1x) */
  k_tpu_tmdr_md_phase_count_2 = 0x05, /**< Phase counting mode 2 (1x) */
  k_tpu_tmdr_md_phase_count_3 = 0x06, /**< Phase counting mode 3 (2x) */
  k_tpu_tmdr_md_phase_count_4 = 0x07, /**< Phase counting mode 4 (4x) - STAR default */
} rx_tpu_tmdr_md_t;

/**
 * @enum rx_tpu_tmdr_shift_t
 * @brief TMDR bit field positions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tmdr_md_shift     = 0, /**< MD[3:0] - Mode Select (bits 3:0) */
  k_tpu_tmdr_bfa_shift    = 4, /**< BFA - Buffer Operation A (bit 4, TPU0/3 only) */
  k_tpu_tmdr_bfb_shift    = 5, /**< BFB - Buffer Operation B (bit 5, TPU0/3 only) */
  k_tpu_tmdr_icselb_shift = 6, /**< ICSELB - TGRB input capture select (bit 6) */
  k_tpu_tmdr_icseld_shift = 7, /**< ICSELD - TGRD input capture select (bit 7, TPU0/3) */
} rx_tpu_tmdr_shift_t;

/**
 * @enum rx_tpu_tmdr_masks_t
 * @brief TMDR register field masks
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tmdr_md_mask = 0x0F, /**< MD[3:0] field mask (bits 3:0) */
} rx_tpu_tmdr_masks_t;

/* =============================================================================
 * TIER - Timer Interrupt Enable Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_tier_bits_t
 * @brief TIER (Timer Interrupt Enable Register) bit definitions
 *
 * @details
 * Controls interrupt generation for timer compare match, overflow, and
 * underflow events. In phase counting mode, TCIEV (overflow) and TCIEU
 * (underflow) are the most relevant interrupts.
 *
 * @par Bit Layout (8-bit)
 * | Bit | Field | Description                              |
 * |-----|-------|------------------------------------------|
 * | 0   | TGIEA | TGRA compare match/input capture IRQ     |
 * | 1   | TGIEB | TGRB compare match/input capture IRQ     |
 * | 2   | TGIEC | TGRC compare match/input capture (0,3)   |
 * | 3   | TGIED | TGRD compare match/input capture (0,3)   |
 * | 4   | TCIEV | Overflow interrupt enable                |
 * | 5   | TCIEU | Underflow interrupt enable (1,2,4,5)     |
 * | 6   | -     | Reserved (read as 1, write 1)            |
 * | 7   | TTGE  | A/D conversion start request enable      |
 *
 * @note TGIEC/TGIED reserved on TPU1/2/4/5 (read as 0, write 0)
 * @note TCIEU reserved on TPU0/TPU3 (read as 0, write 0)
 * @note TTGE reserved on TPU5 (read as 0, write 0)
 * @note Manual: Ch28.2.4 - Timer Interrupt Enable Register (TIER)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tier_tgiea = (1U << 0), /**< TGRA interrupt enable */
  k_tpu_tier_tgieb = (1U << 1), /**< TGRB interrupt enable */
  k_tpu_tier_tgiec = (1U << 2), /**< TGRC interrupt enable (TPU0/3 only) */
  k_tpu_tier_tgied = (1U << 3), /**< TGRD interrupt enable (TPU0/3 only) */
  k_tpu_tier_tciev = (1U << 4), /**< Overflow interrupt enable (all channels) */
  k_tpu_tier_tcieu = (1U << 5), /**< Underflow interrupt enable (TPU1/2/4/5) */
  k_tpu_tier_ttge  = (1U << 7), /**< A/D conversion start request enable */
} rx_tpu_tier_bits_t;

/* =============================================================================
 * TSR - Timer Status Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_tsr_bits_t
 * @brief TSR (Timer Status Register) bit definitions
 *
 * @details
 * Status flags for timer events and counting direction. The TCFD flag
 * is critical for phase counting mode - it indicates whether TCNT is
 * currently counting up (forward) or down (reverse).
 *
 * @par Bit Layout (8-bit)
 * | Bit | Field | Description                              |
 * |-----|-------|------------------------------------------|
 * | 0   | TGFA  | TGRA compare match/input capture flag    |
 * | 1   | TGFB  | TGRB compare match/input capture flag    |
 * | 2   | TGFC  | TGRC compare match/input capture (0,3)   |
 * | 3   | TGFD  | TGRD compare match/input capture (0,3)   |
 * | 4   | TCFV  | Overflow flag (all channels)             |
 * | 5   | TCFU  | Underflow flag (TPU1/2/4/5 only)         |
 * | 6   | -     | Reserved (read as 1, write 1)            |
 * | 7   | TCFD  | Counting Direction Flag (TPU1/2/4/5)     |
 *
 * @note Flags (bits 0-5) are cleared by writing 0 to them
 * @note TCFD is read-only: 1=counting up, 0=counting down
 * @note Bit 6 reset value is 1 (must write 1 to preserve)
 * @note TGFC/TGFD reserved on TPU1/2/4/5 (read as 0, write 0)
 * @note TCFU/TCFD reserved on TPU0/TPU3 (read as 0/1 respectively)
 * @note Manual: Ch28.2.5 - Timer Status Register (TSR)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tsr_tgfa = (1U << 0), /**< TGRA compare match/input capture occurred */
  k_tpu_tsr_tgfb = (1U << 1), /**< TGRB compare match/input capture occurred */
  k_tpu_tsr_tgfc = (1U << 2), /**< TGRC compare match/input capture (TPU0/3) */
  k_tpu_tsr_tgfd = (1U << 3), /**< TGRD compare match/input capture (TPU0/3) */
  k_tpu_tsr_tcfv = (1U << 4), /**< Overflow flag (TCNT wrapped 0xFFFF->0x0000) */
  k_tpu_tsr_tcfu = (1U << 5), /**< Underflow flag (TCNT wrapped 0x0000->0xFFFF) */
  k_tpu_tsr_tcfd = (1U << 7), /**< Counting direction: 1=up, 0=down (read-only) */
} rx_tpu_tsr_bits_t;

/**
 * @enum rx_tpu_tsr_reserved_t
 * @brief TSR reserved bit value (bit 6 must be written as 1)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_tsr_reserved_bit6 = (1U << 6), /**< Bit 6: read as 1, write 1 */
} rx_tpu_tsr_reserved_t;

/* =============================================================================
 * NFCR - Noise Filter Control Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tpu_nfcr_bits_t
 * @brief NFCR (Noise Filter Control Register) bit definitions
 *
 * @details
 * Controls digital noise filtering on TIOC I/O pins. Each channel has
 * independent filter enable bits for each pin and a shared clock select.
 *
 * @note NFCEN/NFDEN are reserved on TPU1/2/4/5 (only TPU0/3 have C/D pins)
 * @note Set NFCR only while TCNT is stopped (CSTn = 0)
 * @note Manual: Ch28.2.10 - Noise Filter Control Register (NFCR)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_nfcr_nfaen = (1U << 0), /**< Noise filter enable for TIOCA pin */
  k_tpu_nfcr_nfben = (1U << 1), /**< Noise filter enable for TIOCB pin */
  k_tpu_nfcr_nfcen = (1U << 2), /**< Noise filter enable for TIOCC (TPU0/3) */
  k_tpu_nfcr_nfden = (1U << 3), /**< Noise filter enable for TIOCD (TPU0/3) */
} rx_tpu_nfcr_bits_t;

/**
 * @enum rx_tpu_nfcr_clock_t
 * @brief NFCR NFCS[1:0] noise filter clock select values
 *
 * @details
 * Selects the sampling clock for the digital noise filter. The filter
 * requires 3 consecutive samples at the selected level to pass.
 *
 * @par Filter Delay at 60 MHz PCLKB
 * | NFCS | Clock     | 3-Sample Delay                    |
 * |------|-----------|-----------------------------------|
 * | 00   | PCLKB/1   | ~50 ns                            |
 * | 01   | PCLKB/8   | ~400 ns                           |
 * | 10   | PCLKB/32  | ~1.6 us                           |
 * | 11   | Count clk | Depends on prescaler setting      |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_nfcr_nfcs_pclk_div1   = (0U << 4), /**< Sample @ PCLKB/1 (~50ns) */
  k_tpu_nfcr_nfcs_pclk_div8   = (1U << 4), /**< Sample @ PCLKB/8 (~400ns) */
  k_tpu_nfcr_nfcs_pclk_div32  = (2U << 4), /**< Sample @ PCLKB/32 (~1.6us) */
  k_tpu_nfcr_nfcs_count_clock = (3U << 4), /**< Sample @ count clock source */
} rx_tpu_nfcr_clock_t;

/**
 * @enum rx_tpu_nfcr_masks_t
 * @brief NFCR register field masks
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tpu_nfcr_nfcs_mask   = 0x30, /**< NFCS[1:0] field mask (bits 5:4) */
  k_tpu_nfcr_enable_mask = 0x0F, /**< All noise filter enable bits (bits 3:0) */
} rx_tpu_nfcr_masks_t;

/* =============================================================================
 * Module Stop Bit Definition
 * =============================================================================
 */

/**
 * @enum rx_tpu_mstpcra_t
 * @brief MSTPCRA bit for TPU module stop control
 *
 * @details
 * The TPU module is controlled by MSTPA13 in the Module Stop Control
 * Register A (MSTPCRA). Write 0 to enable, 1 to stop.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_tpu_mstpcra_mstpa13 = (1U << 13), /**< MSTPA13: TPU module stop (1=stop) */
} rx_tpu_mstpcra_t;

/* =============================================================================
 * TPU Interrupt Source Numbers (Software Configurable Interrupt B)
 * =============================================================================
 */

/**
 * @enum rx_tpu_intb_source_t
 * @brief TPU Software Configurable Interrupt B source numbers
 *
 * @details
 * TPU interrupts are routed through the ICU Software Configurable Interrupt B
 * mechanism. These source numbers are written to SLIBRn registers (n=128-207)
 * to map them to interrupt vector numbers 128-207.
 *
 * For phase counting mode, the most relevant sources are:
 * - TCIxV (overflow): TCNT wrapped 0xFFFF -> 0x0000 while counting up
 * - TCIxU (underflow): TCNT wrapped 0x0000 -> 0xFFFF while counting down
 *
 * @par Configuration Example
 * @code{.c}
 * // Map TPU1 overflow to vector 128 (INTB128)
 * ICU.SLIBR128.BYTE = k_tpu_intb_tci1v;
 * // Map TPU1 underflow to vector 129 (INTB129)
 * ICU.SLIBR129.BYTE = k_tpu_intb_tci1u;
 * @endcode
 *
 * @note Manual: Ch15 Table 15.3 - Interrupt Sources for Software Configurable
 *       Interrupt B, and Ch28.4 - Interrupt Sources
 * @warning These are NOT vector numbers - they are source numbers for SLIBR
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* TPU0 interrupt sources */
  k_tpu_intb_tgi0a = 15, /**< TPU0 TGRA compare match/input capture */
  k_tpu_intb_tgi0b = 16, /**< TPU0 TGRB compare match/input capture */
  k_tpu_intb_tgi0c = 17, /**< TPU0 TGRC compare match/input capture */
  k_tpu_intb_tgi0d = 18, /**< TPU0 TGRD compare match/input capture */
  k_tpu_intb_tci0v = 19, /**< TPU0 overflow */

  /* TPU1 interrupt sources (back-right encoder) */
  k_tpu_intb_tgi1a = 20, /**< TPU1 TGRA compare match/input capture */
  k_tpu_intb_tgi1b = 21, /**< TPU1 TGRB compare match/input capture */
  k_tpu_intb_tci1v = 22, /**< TPU1 overflow (phase counting up wrap) */
  k_tpu_intb_tci1u = 23, /**< TPU1 underflow (phase counting down wrap) */

  /* TPU2 interrupt sources (back-left encoder) */
  k_tpu_intb_tgi2a = 24, /**< TPU2 TGRA compare match/input capture */
  k_tpu_intb_tgi2b = 25, /**< TPU2 TGRB compare match/input capture */
  k_tpu_intb_tci2v = 26, /**< TPU2 overflow (phase counting up wrap) */
  k_tpu_intb_tci2u = 27, /**< TPU2 underflow (phase counting down wrap) */

  /* TPU3 interrupt sources */
  k_tpu_intb_tgi3a = 28, /**< TPU3 TGRA compare match/input capture */
  k_tpu_intb_tgi3b = 29, /**< TPU3 TGRB compare match/input capture */
  k_tpu_intb_tgi3c = 30, /**< TPU3 TGRC compare match/input capture */
  k_tpu_intb_tgi3d = 31, /**< TPU3 TGRD compare match/input capture */
  k_tpu_intb_tci3v = 32, /**< TPU3 overflow */

  /* TPU4 interrupt sources */
  k_tpu_intb_tgi4a = 33, /**< TPU4 TGRA compare match/input capture */
  k_tpu_intb_tgi4b = 34, /**< TPU4 TGRB compare match/input capture */
  k_tpu_intb_tci4v = 35, /**< TPU4 overflow */
  k_tpu_intb_tci4u = 36, /**< TPU4 underflow (phase counting) */

  /* TPU5 interrupt sources */
  k_tpu_intb_tgi5a = 37, /**< TPU5 TGRA compare match/input capture */
  k_tpu_intb_tgi5b = 38, /**< TPU5 TGRB compare match/input capture */
  k_tpu_intb_tci5v = 39, /**< TPU5 overflow */
  k_tpu_intb_tci5u = 40, /**< TPU5 underflow (phase counting) */
} rx_tpu_intb_source_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* --- Base Address Verification (Manual Ch28.2) --- */
static_assert(k_tpu_control_base_addr == 0x00088100, "TPU control base address incorrect");
static_assert(k_tpu0_base_addr == 0x00088110, "TPU0 base address incorrect");
static_assert(k_tpu1_base_addr == 0x00088120, "TPU1 base address incorrect");
static_assert(k_tpu2_base_addr == 0x00088130, "TPU2 base address incorrect");
static_assert(k_tpu3_base_addr == 0x00088140, "TPU3 base address incorrect");
static_assert(k_tpu4_base_addr == 0x00088150, "TPU4 base address incorrect");
static_assert(k_tpu5_base_addr == 0x00088160, "TPU5 base address incorrect");

/* --- Channel Spacing Verification --- */
static_assert((k_tpu1_base_addr - k_tpu0_base_addr) == k_tpu_channel_spacing,
              "TPU0-TPU1 spacing incorrect");
static_assert((k_tpu2_base_addr - k_tpu1_base_addr) == k_tpu_channel_spacing,
              "TPU1-TPU2 spacing incorrect");
static_assert((k_tpu3_base_addr - k_tpu2_base_addr) == k_tpu_channel_spacing,
              "TPU2-TPU3 spacing incorrect");
static_assert((k_tpu4_base_addr - k_tpu3_base_addr) == k_tpu_channel_spacing,
              "TPU3-TPU4 spacing incorrect");
static_assert((k_tpu5_base_addr - k_tpu4_base_addr) == k_tpu_channel_spacing,
              "TPU4-TPU5 spacing incorrect");

/* --- Control Block Structure Verification --- */
static_assert(sizeof(rx_tpu_control_regs_t) == 14, "TPU control register structure size incorrect");
static_assert(offsetof(rx_tpu_control_regs_t, tstr) == 0x00, "TSTR offset incorrect");
static_assert(offsetof(rx_tpu_control_regs_t, tsyr) == 0x01, "TSYR offset incorrect");
static_assert(offsetof(rx_tpu_control_regs_t, nfcr) == 0x08, "NFCR array offset incorrect");

/* --- Extended Channel Structure (TPU0/TPU3) Verification --- */
static_assert(sizeof(rx_tpu_ext_regs_t) == 16,
              "TPU extended channel register structure size incorrect");
static_assert(offsetof(rx_tpu_ext_regs_t, tcr) == 0x00, "TCR offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tmdr) == 0x01, "TMDR offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tiorh) == 0x02, "TIORH offset incorrect");
static_assert(offsetof(rx_tpu_ext_regs_t, tiorl) == 0x03, "TIORL offset incorrect");
static_assert(offsetof(rx_tpu_ext_regs_t, tier) == 0x04, "TIER offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tsr) == 0x05, "TSR offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tcnt) == 0x06, "TCNT offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tgra) == 0x08, "TGRA offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tgrb) == 0x0A, "TGRB offset incorrect (extended)");
static_assert(offsetof(rx_tpu_ext_regs_t, tgrc) == 0x0C, "TGRC offset incorrect");
static_assert(offsetof(rx_tpu_ext_regs_t, tgrd) == 0x0E, "TGRD offset incorrect");

/* --- Basic Channel Structure (TPU1/2/4/5) Verification --- */
static_assert(sizeof(rx_tpu_regs_t) == 16, "TPU basic channel register structure size incorrect");
static_assert(offsetof(rx_tpu_regs_t, tcr) == 0x00, "TCR offset incorrect (basic)");
static_assert(offsetof(rx_tpu_regs_t, tmdr) == 0x01, "TMDR offset incorrect (basic)");
static_assert(offsetof(rx_tpu_regs_t, tior) == 0x02, "TIOR offset incorrect");
static_assert(offsetof(rx_tpu_regs_t, tier) == 0x04, "TIER offset incorrect (basic)");
static_assert(offsetof(rx_tpu_regs_t, tsr) == 0x05, "TSR offset incorrect (basic)");
static_assert(offsetof(rx_tpu_regs_t, tcnt) == 0x06, "TCNT offset incorrect (basic)");
static_assert(offsetof(rx_tpu_regs_t, tgra) == 0x08, "TGRA offset incorrect (basic)");
static_assert(offsetof(rx_tpu_regs_t, tgrb) == 0x0A, "TGRB offset incorrect (basic)");

/* --- Bit Field Value Verification --- */
static_assert(k_tpu_tstr_cst0 == 0x01, "TSTR CST0 bit value incorrect");
static_assert(k_tpu_tstr_cst5 == 0x20, "TSTR CST5 bit value incorrect");
static_assert(k_tpu_tsr_tcfd == 0x80, "TSR TCFD bit value incorrect");
static_assert(k_tpu_tsr_tcfv == 0x10, "TSR TCFV bit value incorrect");
static_assert(k_tpu_tsr_tcfu == 0x20, "TSR TCFU bit value incorrect");
static_assert(k_tpu_tmdr_md_phase_count_4 == 0x07, "TMDR phase counting mode 4 value incorrect");
static_assert(k_tpu_tier_tciev == 0x10, "TIER TCIEV bit value incorrect");
static_assert(k_tpu_tier_tcieu == 0x20, "TIER TCIEU bit value incorrect");

/* --- NFCR Bit Verification --- */
static_assert(k_tpu_nfcr_nfaen == 0x01, "NFCR NFAEN bit value incorrect");
static_assert(k_tpu_nfcr_nfcs_pclk_div32 == 0x20, "NFCR NFCS PCLK/32 value incorrect");

#ifdef __cplusplus
}
#endif
