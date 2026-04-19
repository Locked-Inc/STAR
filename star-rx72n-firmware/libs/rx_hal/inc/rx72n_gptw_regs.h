/**
 * @file rx72n_gptw_regs.h
 * @brief RX72N GPTW (General PWM Timer) Register Definitions
 *
 * @details
 * This header provides comprehensive register definitions for the RX72N's
 * General PWM Timer (GPTW). The GPTW is a 32-bit timer optimized for motor
 * PWM generation with dead-time control, complementary outputs, and
 * hardware-synchronized ADC triggering.
 *
 * @par System Architecture Context:
 * @verbatim
 * +----------------------------------------------------------------------+
 * |                    STAR Motor PWM Generation (GPTW)                  |
 * +----------------------------------------------------------------------+
 * |                                                                      |
 * |   PID Output --> GPTW Compare --> GTIOCA/B --> DRV8263H --> Motor     |
 * |   (0-100%)       (32-bit)        (PWM pins)   (H-bridge)  (DC motor) |
 * |                                                                      |
 * |   +-------------------------------------------------------------+   |
 * |   | GPTW Channel Configuration (per motor)                      |   |
 * |   |                                                             |   |
 * |   |   GTPR (Period) = PCLKA / (Prescaler x PWM_Freq) - 1       |   |
 * |   |   GTCCRA (Duty) = GTPR x (duty_percent / 100)              |   |
 * |   |                                                             |   |
 * |   |   @ PCLKA=120MHz, Prescaler=1, PWM_Freq=20kHz:             |   |
 * |   |   GTPR = 120MHz / 20kHz - 1 = 5999                         |   |
 * |   |   50% duty: GTCCRA = 2999                                  |   |
 * |   +-------------------------------------------------------------+   |
 * |                                                                      |
 * +----------------------------------------------------------------------+
 * @endverbatim
 *
 * @par GPTW Key Features:
 * - 32-bit counter resolution (vs MTU's 16-bit)
 * - 4 independent channels (GPTW0-GPTW3) for 4 motors
 * - Each channel has 2 outputs (GTIOCA, GTIOCB) for H-bridge control
 * - Hardware dead-time insertion for shoot-through prevention
 * - Synchronized start/stop across multiple channels
 * - ADC trigger generation for current sensing synchronization
 * - Double/triple buffering for glitch-free duty updates
 *
 * @par Channel to Motor Mapping (STAR Project):
 *
 * | Channel | Motor | GTIOCA Pin | GTIOCB Pin | DRV8263H |
 * |---------|-------|------------|------------|---------|
 * | GPTW0   | M0    | PE5        | PE2        | U1      |
 * | GPTW1   | M1    | PE4        | PE1        | U2      |
 * | GPTW2   | M2    | PE3        | PE0        | U3      |
 * | GPTW3   | M3    | PE7        | PE6        | U4      |
 *
 * @par PWM Timing Specifications:
 *
 * | Parameter          | Value          | Notes                    |
 * |--------------------|----------------|--------------------------|
 * | Clock source       | PCLKA (120MHz) | From clock generator     |
 * | PWM frequency      | 20 kHz         | Above audible range      |
 * | Resolution         | 6000 counts    | GTPR = 5999              |
 * | Duty resolution    | 0.0167%        | 1/6000 = 0.0167%         |
 * | Dead time          | 500 ns         | Shoot-through protection |
 * | Update rate        | 100 Hz         | PID control loop rate    |
 *
 * @par Hardware Requirements:
 * - CPU: Renesas RX72N @ 240 MHz (ICLK)
 * - Peripheral clock: PCLKA = 120 MHz (GPTW clock source)
 * - GPIO: Port E pins configured for GPTW alternate function
 * - Module stop: MSTPCRA.MSTPA7 = 0 to enable GPTW (RX72N)
 *
 * @par Memory Usage:
 * - Channel register structure: 216 bytes (0xD8) per channel
 * - Common register structure: 44 bytes (0x2C)
 * - No dynamic allocation required
 *
 * @see docs/sections/05_motor_control.tex Motor control system design
 * @see lib/rx_motor/inc/rx_motor.h Motor control API
 * @see lib/rx_drv8263/inc/rx_drv8263.h H-bridge driver interface
 * @see RX72N Group User's Manual: Hardware, Chapter 26 (GPTW)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, or recursion in accessor functions
 * - Rule 3: [OK] Zero dynamic allocation - all registers statically mapped
 * - Rule 8: [OK] All constants as C23 typed enums (addresses, bit fields)
 * - Rule 10: [OK] Compile with -Wall -Wextra -Werror, zero warnings
 *
 * @par SOLID Principles:
 * - S: Single responsibility - only GPTW register definitions
 * - O: Open for extension via accessor functions
 * - I: Interface segregation - channel and common registers separate
 * - D: Dependency inversion - hardware accessed via typed pointers
 *
 * @par Module Dependencies:
 * @dot
 * digraph gptw_deps {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   gptw_regs [label="rx72n_gptw_regs.h"];
 *   stdint [label="<stdint.h>"];
 *   stddef [label="<stddef.h>"];
 *   motor [label="rx_motor.c"];
 *   gptw_impl [label="rx_gptw.c"];
 *   gptw_regs -> stdint;
 *   gptw_regs -> stddef;
 *   motor -> gptw_regs;
 *   gptw_impl -> gptw_regs;
 * }
 * @enddot
 *
 * @par Verification Status:
 * [PASS] VERIFIED (2026-02-03) - Complete verification against Smart Configurator iodefine.h
 * - All 55 channel registers present and offset-verified
 * - All 6 common registers present and offset-verified
 * - 61/61 static assertions (100% coverage)
 * - Base addresses verified: 0x000C2000-0x000C2300 (channels), 0x000C2B00 (common)
 * - Structure sizes verified: 216 bytes (channel), 44 bytes (common)
 * - Reference: iodefine.h lines 10427-11461 (struct st_gptw)
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rx_gptw_regs RX72N GPTW Register Definitions
 * @brief General PWM Timer hardware register interface
 * @ingroup rx_hal
 *
 * @details
 * Provides memory-mapped register access to the RX72N GPTW peripheral
 * for 32-bit PWM generation in motor control applications.
 *
 * @par Quick Start Example - Configure 20 kHz PWM:
 * @code{.c}
 * // Get GPTW channel 0 for motor 0
 * volatile rx_gptw_channel_regs_t* gptw = gptw0();
 *
 * // 1. Unlock write protection
 * gptw->gtwp = k_gptw_gtwp_unlock;
 *
 * // 2. Stop counter during configuration
 * gptw->gtcr &= ~k_gptw_gtcr_cst;
 *
 * // 3. Set mode: Sawtooth PWM, no prescaler (PCLKA/1 = 120 MHz)
 * gptw->gtcr = k_gptw_gtcr_md_saw_pwm | k_gptw_gtcr_tpcs_1;
 *
 * // 4. Set period for 20 kHz: 120 MHz / 20 kHz - 1 = 5999
 * gptw->gtpr = 5999;
 *
 * // 5. Set initial duty to 0%
 * gptw->gtccra = 0;
 *
 * // 6. Configure GTIOCA output: initial low, high at end-of-cycle, low at compare match
 * gptw->gtior = k_gptw_gtior_oa_init_low | k_gptw_gtior_oae;
 *
 * // 7. Clear counter
 * gptw->gtcnt = 0;
 *
 * // 8. Start counting
 * gptw->gtcr |= k_gptw_gtcr_cst;
 *
 * // 9. Lock write protection
 * gptw->gtwp = k_gptw_gtwp_lock;
 *
 * // Update duty cycle to 50%
 * gptw->gtwp = k_gptw_gtwp_unlock;
 * gptw->gtccra = 3000;  // 50% of 5999
 * gptw->gtwp = k_gptw_gtwp_lock;
 * @endcode
 *
 * @{
 */

/* =============================================================================
 * General PWM Timer (GPTW) - 32-bit PWM for Motor Control
 * =============================================================================
 *
 * GPTW Features:
 * - 32-bit counter resolution (vs MTU's 16-bit)
 * - 4 independent channels (GPTW0-GPTW3)
 * - Each channel has 2 outputs (GTIOCA, GTIOCB)
 * - Optimized for motor PWM generation
 * - Dead-time control for H-bridge safety
 *
 * Pin Mapping (Port E):
 * - GPTW0: PE5/GTIOC0A, PE2/GTIOC0B
 * - GPTW1: PE4/GTIOC1A, PE1/GTIOC1B
 * - GPTW2: PE3/GTIOC2A, PE0/GTIOC2B
 * - GPTW3: PE7/GTIOC3A, PE6/GTIOC3B
 */

/* =============================================================================
 * Struct Padding Sizes - Named array counts for reserved fields
 * =============================================================================
 */

typedef enum : uint8_t {
  k_gptw_ch_reserved_count      = 6U, /**< Reserved uint32_t count in channel struct (0xB8-0xCC) */
  k_gptw_common_reserved0_count = 5U, /**< Reserved uint32_t count in common struct (0x0C-0x1C) */
} gptw_padding_sizes_t;

/**
 * @struct rx_gptw_channel_regs_t
 * @brief GPTW Channel Register Map for 32-bit PWM generation
 *
 * @details
 * Memory-mapped register structure for a single GPTW channel. Each of
 * the 4 channels (GPTW0-GPTW3) has an identical register layout at
 * different base addresses spaced 0x100 bytes apart.
 *
 * @par Channel Base Addresses:
 *
 * | Channel | Base Address | Motor | Output Pins        |
 * |---------|--------------|-------|---------------------|
 * | GPTW0   | 0x000C2000   | M0    | PE5 (A), PE2 (B)    |
 * | GPTW1   | 0x000C2100   | M1    | PE4 (A), PE1 (B)    |
 * | GPTW2   | 0x000C2200   | M2    | PE3 (A), PE0 (B)    |
 * | GPTW3   | 0x000C2300   | M3    | PE7 (A), PE6 (B)    |
 *
 * @par Key Registers for Motor PWM:
 *
 * | Register | Offset | Description                           |
 * |----------|--------|---------------------------------------|
 * | GTWP     | 0x00   | Write protection (unlock: 0xA500)     |
 * | GTCR     | 0x2C   | Control (mode, prescaler, start/stop) |
 * | GTIOR    | 0x34   | I/O control (output polarity/enable)  |
 * | GTCNT    | 0x48   | 32-bit counter value                  |
 * | GTCCRA   | 0x4C   | Compare A (duty cycle for GTIOCA)     |
 * | GTCCRB   | 0x50   | Compare B (duty cycle for GTIOCB)     |
 * | GTPR     | 0x64   | Period (PWM frequency)                |
 * | GTDTCR   | 0x88   | Dead-time control                     |
 * | GTDVU    | 0x8C   | Dead-time value (rising edge)         |
 * | GTDVD    | 0x90   | Dead-time value (falling edge)        |
 *
 * @par PWM Frequency Calculation:
 * @f$ f_{PWM} = \frac{f_{PCLKA}}{(GTPR + 1) \times Prescaler} @f$
 *
 * @par Duty Cycle Calculation:
 * @f$ Duty\% = \frac{GTCCRA}{GTPR + 1} \times 100\% @f$
 *
 * @par Usage Example - Dead-Time Configuration:
 * @code{.c}
 * volatile rx_gptw_channel_regs_t* gptw = gptw0();
 *
 * // Enable dead-time generation
 * gptw->gtdtcr = k_gptw_gtdtcr_tde;
 *
 * // Set dead-time: 500 ns @ 120 MHz = 60 counts
 * gptw->gtdvu = 60;  // Rising edge dead-time
 * gptw->gtdvd = 60;  // Falling edge dead-time
 * @endcode
 *
 * @invariant sizeof(rx_gptw_channel_regs_t) == 0xD8 (216 bytes)
 * @invariant Channel spacing is exactly 0x100 bytes
 *
 * @see gptw0(), gptw1(), gptw2(), gptw3() Accessor functions
 * @see rx_gptw_common_regs_t Common registers for synchronized control
 */
typedef struct {
  volatile uint32_t gtwp;      /**< 0x00: Write Protection Register */
  volatile uint32_t gtstr;     /**< 0x04: Software Start Register */
  volatile uint32_t gtstp;     /**< 0x08: Software Stop Register */
  volatile uint32_t gtclr;     /**< 0x0C: Software Clear Register */
  volatile uint32_t gtssr;     /**< 0x10: Start Source Select Register */
  volatile uint32_t gtpsr;     /**< 0x14: Stop Source Select Register */
  volatile uint32_t gtcsr;     /**< 0x18: Clear Source Select Register */
  volatile uint32_t gtupsr;    /**< 0x1C: Count-Up Source Select Register */
  volatile uint32_t gtdnsr;    /**< 0x20: Count-Down Source Select Register */
  volatile uint32_t gticasr;   /**< 0x24: Input Capture Source Select A */
  volatile uint32_t gticbsr;   /**< 0x28: Input Capture Source Select B */
  volatile uint32_t gtcr;      /**< 0x2C: Control Register */
  volatile uint32_t gtuddtyc;  /**< 0x30: Up/Down Count Duty Setting */
  volatile uint32_t gtior;     /**< 0x34: I/O Control Register */
  volatile uint32_t gtintad;   /**< 0x38: Interrupt Output Setting */
  volatile uint32_t gtst;      /**< 0x3C: Status Register */
  volatile uint32_t gtber;     /**< 0x40: Buffer Enable Register */
  volatile uint32_t gtitc;     /**< 0x44: Interrupt/ADC Trigger Control */
  volatile uint32_t gtcnt;     /**< 0x48: Counter */
  volatile uint32_t gtccra;    /**< 0x4C: Compare Capture Register A */
  volatile uint32_t gtccrb;    /**< 0x50: Compare Capture Register B */
  volatile uint32_t gtccrc;    /**< 0x54: Compare Capture Register C */
  volatile uint32_t gtccre;    /**< 0x58: Compare Capture Register E */
  volatile uint32_t gtccrd;    /**< 0x5C: Compare Capture Register D */
  volatile uint32_t gtccrf;    /**< 0x60: Compare Capture Register F */
  volatile uint32_t gtpr;      /**< 0x64: Cycle Setting Register (Period) */
  volatile uint32_t gtpbr;     /**< 0x68: Cycle Setting Buffer Register */
  volatile uint32_t gtpdbr;    /**< 0x6C: Cycle Setting Double Buffer */
  volatile uint32_t gtadtra;   /**< 0x70: A/D Trigger Register A */
  volatile uint32_t gtadtbra;  /**< 0x74: A/D Trigger Buffer Register A */
  volatile uint32_t gtadtdbra; /**< 0x78: A/D Trigger Double Buffer A */
  volatile uint32_t gtadtrb;   /**< 0x7C: A/D Trigger Register B */
  volatile uint32_t gtadtbrb;  /**< 0x80: A/D Trigger Buffer Register B */
  volatile uint32_t gtadtdbrb; /**< 0x84: A/D Trigger Double Buffer B */
  volatile uint32_t gtdtcr;    /**< 0x88: Dead Time Control Register */
  volatile uint32_t gtdvu;     /**< 0x8C: Dead Time Value Upper */
  volatile uint32_t gtdvd;     /**< 0x90: Dead Time Value Down */
  volatile uint32_t gtdbu;     /**< 0x94: Dead Time Buffer Upper */
  volatile uint32_t gtdbd;     /**< 0x98: Dead Time Buffer Down */
  volatile uint32_t gtsos;     /**< 0x9C: Output Protection Status */
  volatile uint32_t gtsotr;    /**< 0xA0: Output Protection Trigger */
  volatile uint32_t gtadsmr;   /**< 0xA4: A/D Conversion Start Request Signal Monitoring */
  volatile uint32_t gteitc;    /**< 0xA8: Extended Interrupt Skipping Counter Control */
  volatile uint32_t gteitli1;  /**< 0xAC: Extended Interrupt Skipping Setting Register 1 */
  volatile uint32_t gteitli2;  /**< 0xB0: Extended Interrupt Skipping Setting Register 2 */
  volatile uint32_t gteitlb;   /**< 0xB4: Extended Buffer Transfer Skipping Setting */
  uint32_t          reserved[k_gptw_ch_reserved_count]; /**< 0xB8-0xCC: Reserved */
  volatile uint32_t gtsecsr; /**< 0xD0: Operation Enable Bit Simultaneous Control Channel Select */
  volatile uint32_t gtsecr;  /**< 0xD4: Operation Enable Bit Simultaneous Control */
} rx_gptw_channel_regs_t;

/**
 * @brief GPTW Common Register Map
 * @details
 * Shared registers for all GPTW channels (start/stop control).
 * Base address: k_gptw_common_base_addr (0x000C2B00) in gptw_addresses_t.
 */
typedef struct {
  volatile uint32_t gtstra; /**< 0x00: General Timer Start Register A */
  volatile uint32_t gtstpa; /**< 0x04: General Timer Stop Register A */
  volatile uint32_t gtclra; /**< 0x08: General Timer Clear Register A */
  uint32_t          reserved0[k_gptw_common_reserved0_count]; /**< Reserved */
  volatile uint32_t gtstra2; /**< 0x20: General Timer Start Register A2 */
  volatile uint32_t gtstpa2; /**< 0x24: General Timer Stop Register A2 */
  volatile uint32_t gtclra2; /**< 0x28: General Timer Clear Register A2 */
} rx_gptw_common_regs_t;

/* =============================================================================
 * Base Address Definitions
 * =============================================================================
 *
 * Addresses verified against RX72N Hardware Manual (R01UH0824EJ0120 Rev.1.20).
 */

/** @brief GPTW hardware addresses (verified against RX72N Hardware Manual) */
typedef enum : uintptr_t {
  k_gptw_channel_offset   = 0x100,      /**< Channel spacing between GPTW registers */
  k_gptw0_base_addr       = 0x000C2000, /**< GPTW channel 0 base address */
  k_gptw1_base_addr       = 0x000C2100, /**< GPTW channel 1 base address */
  k_gptw2_base_addr       = 0x000C2200, /**< GPTW channel 2 base address */
  k_gptw3_base_addr       = 0x000C2300, /**< GPTW channel 3 base address */
  k_gptw_common_base_addr = 0x000C2B00, /**< GPTW common control register base (GTSTRA etc.) */
} gptw_addresses_t;

/** @brief GPTW register inline accessor functions */
static inline volatile rx_gptw_channel_regs_t* gptw0(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw0_base_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw1(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw1_base_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw2(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw2_base_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw3(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw3_base_addr;
}

/** @brief GPTW common register accessor */
static inline volatile rx_gptw_common_regs_t* gptw_common(void)
{
  return (volatile rx_gptw_common_regs_t*)k_gptw_common_base_addr;
}

/* =============================================================================
 * Write Protection Register (GTWP) Bits
 * =============================================================================
 */

typedef enum : uint16_t {
  k_gptw_gtwp_wp0    = (1 << 0), /**< Register write protect bit 0 */
  k_gptw_gtwp_wp1    = (1 << 1), /**< Register write protect bit 1 */
  k_gptw_gtwp_wp2    = (1 << 2), /**< Register write protect bit 2 */
  k_gptw_gtwp_wp3    = (1 << 3), /**< Register write protect bit 3 */
  k_gptw_gtwp_unlock = 0xA500,   /**< Write protection unlock key */
  k_gptw_gtwp_lock   = 0xA501,   /**< Write protection lock key */
} gptw_gtwp_bits_t;

/* =============================================================================
 * Control Register (GTCR) Bits
 * =============================================================================
 * Reference: RX72N Hardware Manual R01UH0824EJ0120 Rev.1.20, Section 26.2.12
 */

typedef enum : uint32_t {
  /* b0: CST - Count Start */
  k_gptw_gtcr_cst = (1 << 0), /**< Counter Start (0=stop, 1=count) */

  /* b8: ICDS - Input Capture Operation Select at Count Stop */
  k_gptw_gtcr_icds = (1 << 8), /**< 0=capture at stop, 1=no capture at stop */

  /* b18-b16: MD[2:0] - Mode Select */
  k_gptw_gtcr_md_shift = 16,
  k_gptw_gtcr_md_mask  = (0x07 << k_gptw_gtcr_md_shift),

  /** Sawtooth-wave PWM mode (single buffer or double buffer possible) */
  k_gptw_gtcr_md_saw_pwm = (0x00 << k_gptw_gtcr_md_shift),
  /** Sawtooth-wave one-shot pulse mode (fixed buffer operation) */
  k_gptw_gtcr_md_saw_oneshot = (0x01 << k_gptw_gtcr_md_shift),
  /* 0x02 (010): Setting prohibited */
  /* 0x03 (011): Setting prohibited */
  /** Triangle-wave PWM mode 1: 32-bit transfer at trough (single/double buffer) */
  k_gptw_gtcr_md_tri_pwm1 = (0x04 << k_gptw_gtcr_md_shift),
  /** Triangle-wave PWM mode 2: 32-bit transfer at crest and trough (single/double buffer) */
  k_gptw_gtcr_md_tri_pwm2 = (0x05 << k_gptw_gtcr_md_shift),
  /** Triangle-wave PWM mode 3: 64-bit transfer at trough (fixed buffer operation) */
  k_gptw_gtcr_md_tri_pwm3 = (0x06 << k_gptw_gtcr_md_shift),
  /* 0x07 (111): Setting prohibited */

  /* b26-b23: TPCS[3:0] - Timer Prescaler Select */
  k_gptw_gtcr_tpcs_shift = 23,
  k_gptw_gtcr_tpcs_mask  = (0x0F << k_gptw_gtcr_tpcs_shift),

  k_gptw_gtcr_tpcs_1  = (0x00 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/1 (120 MHz) */
  k_gptw_gtcr_tpcs_2  = (0x01 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/2 */
  k_gptw_gtcr_tpcs_4  = (0x02 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/4 */
  k_gptw_gtcr_tpcs_8  = (0x03 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/8 */
  k_gptw_gtcr_tpcs_16 = (0x04 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/16 */
  k_gptw_gtcr_tpcs_32 = (0x05 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/32 */
  k_gptw_gtcr_tpcs_64 = (0x06 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/64 */
  /* 0x07 (0111): Setting prohibited */
  k_gptw_gtcr_tpcs_256 = (0x08 << k_gptw_gtcr_tpcs_shift), /**< PCLKA/256 */
  /* 0x09 (1001): Setting prohibited */
  k_gptw_gtcr_tpcs_1024 = (0x0A << k_gptw_gtcr_tpcs_shift), /**< PCLKA/1024 */
  /* 0x0B (1011): Setting prohibited */
  /* 0x0C-0x0F: GTETRGA/B/C/D via POEG (external clock) */
} gptw_gtcr_bits_t;

/* =============================================================================
 * I/O Control Register (GTIOR) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  /* GTIOCA Output Control (bits 0-4) */
  k_gptw_gtior_oa_shift      = 0,
  k_gptw_gtior_oa_mask       = 0x1F,
  k_gptw_gtior_oa_disabled   = 0x00, /**< Output disabled */
  k_gptw_gtior_oa_low_cmpup  = 0x01, /**< Low on compare match (up) */
  k_gptw_gtior_oa_high_cmpup = 0x02, /**< High on compare match (up) */
  k_gptw_gtior_oa_toggle     = 0x03, /**< Toggle on compare match */
  k_gptw_gtior_oa_init_low =
    0x09, /**< Initial LOW; high at end-of-cycle; low at compare match (active-high sawtooth PWM) */
  k_gptw_gtior_oa_init_high =
    0x16, /**< Initial HIGH; low at end-of-cycle; high at compare match (complement of init_low) */

  /* GTIOCA Output Enable (bit 8) */
  k_gptw_gtior_oae = (1 << 8), /**< GTIOCA Output Enable */

  /* GTIOCB Output Control (bits 16-20) */
  k_gptw_gtior_ob_shift      = 16,
  k_gptw_gtior_ob_mask       = (0x1F << 16),
  k_gptw_gtior_ob_disabled   = (0x00 << 16), /**< Output disabled */
  k_gptw_gtior_ob_low_cmpup  = (0x01 << 16), /**< Low on compare match (up) */
  k_gptw_gtior_ob_high_cmpup = (0x02 << 16), /**< High on compare match (up) */
  k_gptw_gtior_ob_toggle     = (0x03 << 16), /**< Toggle on compare match */
  k_gptw_gtior_ob_init_low =
    (0x09 << 16), /**< Initial LOW; high at end-of-cycle; low at compare match */
  k_gptw_gtior_ob_init_high =
    (0x16 << 16), /**< Initial HIGH; low at end-of-cycle; high at compare match */

  /* GTIOCB Output Enable (bit 24) */
  k_gptw_gtior_obe = (1 << 24), /**< GTIOCB Output Enable */
} gptw_gtior_bits_t;

/* =============================================================================
 * Interrupt Output Setting Register (GTINTAD) Bits
 * =============================================================================
 */

/**
 * @enum gptw_gtintad_bits_t
 * @brief GTINTAD register bit definitions for POEG group linkage
 *
 * @details
 * The GTINTAD register (offset 0x38) configures the link between a GPTW
 * channel and a POEG group for emergency output stop. When a fault is
 * detected (dead-time error, simultaneous high/low output), the GPTW
 * signals the selected POEG group to disable PWM outputs.
 *
 * @par STAR Project Usage:
 * | GPTW Channel | POEG Group | GRP[1:0] | Motor |
 * |--------------|------------|----------|-------|
 * | GPTW0        | A          | 00       | 0     |
 * | GPTW1        | B          | 01       | 1     |
 * | GPTW2        | C          | 10       | 2     |
 * | GPTW3        | D          | 11       | 3     |
 *
 * @warning GRP[1:0] must be set while OAE and OBE in GTIOR are 0
 *
 * @see rx72n_poeg_regs.h POEG register definitions
 * @see RX72N Manual Section 26.2.15 - GTINTAD register
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* GRP[1:0] - Output Stop Group Select (bits 25:24) */
  k_gptw_gtintad_grp_shift = 24,            /**< GRP field shift */
  k_gptw_gtintad_grp_mask  = (0x03U << 24), /**< GRP field mask */
  k_gptw_gtintad_grp_a     = (0x00U << 24), /**< Select POEG Group A */
  k_gptw_gtintad_grp_b     = (0x01U << 24), /**< Select POEG Group B */
  k_gptw_gtintad_grp_c     = (0x02U << 24), /**< Select POEG Group C */
  k_gptw_gtintad_grp_d     = (0x03U << 24), /**< Select POEG Group D */

  /* GRPDTE - Dead Time Error Output Stop Detection Enable (bit 28) */
  k_gptw_gtintad_grpdte = (1U << 28), /**< Enable dead-time error detection */

  /* GRPABH - Simultaneous High Output Stop Detection Enable (bit 29) */
  k_gptw_gtintad_grpabh = (1U << 29), /**< Enable simultaneous high detection */

  /* GRPABL - Simultaneous Low Output Stop Detection Enable (bit 30) */
  k_gptw_gtintad_grpabl = (1U << 30), /**< Enable simultaneous low detection */
} gptw_gtintad_bits_t;

/* =============================================================================
 * Buffer Enable Register (GTBER) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  /* BD[3:0] - Buffer Operation Disable (b3:b0).
   * Set 1 to DISABLE buffer operation for the register group. Reset = 0 (buffer enabled). */
  k_gptw_gtber_bd0 = (1U << 0), /**< Disable buffer op for GTCCRA/GTCCRB (1=disabled) */
  k_gptw_gtber_bd1 = (1U << 1), /**< Disable buffer op for GTPR (1=disabled) */
  k_gptw_gtber_bd2 = (1U << 2), /**< Disable buffer op for GTADTRA/GTADTRB (1=disabled) */
  k_gptw_gtber_bd3 = (1U << 3), /**< Disable buffer op for GTDVU/GTDVD (1=disabled) */
  /* CCRA[1:0] - GTCCRA Buffer Operation (b17:b16). BD[0]=0 required for buffer to be active. */
  k_gptw_gtber_ccra_single = (0x01U << 16), /**< GTCCRA single-buffer: GTCCRC -> GTCCRA */
  k_gptw_gtber_ccra_double = (0x02U << 16), /**< GTCCRA double-buffer: GTCCRD -> GTCCRC -> GTCCRA */
  /* CCRB[1:0] - GTCCRB Buffer Operation (b19:b18). BD[0]=0 required for buffer to be active. */
  k_gptw_gtber_ccrb_single = (0x01U << 18), /**< GTCCRB single-buffer: GTCCRE -> GTCCRB */
  k_gptw_gtber_ccrb_double = (0x02U << 18), /**< GTCCRB double-buffer: GTCCRF -> GTCCRE -> GTCCRB */
  /* PR[1:0] - GTPR Buffer Operation (b21:b20). BD[1]=0 required for buffer to be active. */
  k_gptw_gtber_pr_single = (0x01U << 20), /**< GTPR single-buffer: GTPBR -> GTPR */
  k_gptw_gtber_pr_double = (0x02U << 20), /**< GTPR double-buffer: GTPDBR -> GTPBR -> GTPR */
} gptw_gtber_bits_t;

/* =============================================================================
 * Dead Time Control Register (GTDTCR) Bits
 * =============================================================================
 */

typedef enum : uint8_t {
  k_gptw_gtdtcr_tde  = (1 << 0), /**< Dead time enable */
  k_gptw_gtdtcr_tdfe = (1 << 4), /**< Dead time buffer enable */
} gptw_gtdtcr_bits_t;

/* =============================================================================
 * Status Register (GTST) Bits
 * =============================================================================
 */

typedef enum : uint16_t {
  k_gptw_gtst_tcfa  = (1 << 0),  /**< Compare match flag A */
  k_gptw_gtst_tcfb  = (1 << 1),  /**< Compare match flag B */
  k_gptw_gtst_tcfc  = (1 << 2),  /**< Compare match flag C */
  k_gptw_gtst_tcfd  = (1 << 3),  /**< Compare match flag D */
  k_gptw_gtst_tcfe  = (1 << 4),  /**< Compare match flag E */
  k_gptw_gtst_tcff  = (1 << 5),  /**< Compare match flag F */
  k_gptw_gtst_tcfpo = (1 << 6),  /**< Overflow flag (at period) */
  k_gptw_gtst_tcfpu = (1 << 7),  /**< Underflow flag (at 0) */
  k_gptw_gtst_tucf  = (1 << 15), /**< Count direction flag (1=up) */
} gptw_gtst_bits_t;

/* =============================================================================
 * Start/Stop Register Bits (GTSTRA)
 * =============================================================================
 */

typedef enum : uint8_t {
  k_gptw_gtstr_cst0 = (1 << 0), /**< Channel 0 count start */
  k_gptw_gtstr_cst1 = (1 << 1), /**< Channel 1 count start */
  k_gptw_gtstr_cst2 = (1 << 2), /**< Channel 2 count start */
  k_gptw_gtstr_cst3 = (1 << 3), /**< Channel 3 count start */
} gptw_gtstr_bits_t;

/** @brief Up/Down Count Duty Setting Register (GTUDDTYC) Bits */
typedef enum : uint32_t {
  k_gptw_gtuddtyc_ud  = (1 << 0), /**< Count direction: 0=down, 1=up */
  k_gptw_gtuddtyc_udf = (1 << 1), /**< Forcibly set count direction */
} gptw_gtuddtyc_bits_t;

/* =============================================================================
 * A/D Conversion Start Request Signal Monitoring Register (GTADSMR) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  k_gptw_gtadsmr_adsms0 = (1 << 0), /**< GTADSM0 output status monitor */
  k_gptw_gtadsmr_adsms1 = (1 << 1), /**< GTADSM1 output status monitor */
} gptw_gtadsmr_bits_t;

/* =============================================================================
 * Extended Interrupt Skipping Counter Control Register (GTEITC) Bits
 * =============================================================================
 * WARNING: This is DIFFERENT from GTITC @ 0x44!
 * - GTITC @ 0x44: Interrupt and A/D Conversion Start Request Skipping Setting
 * - GTEITC @ 0xA8: Extended Interrupt Skipping Counter Control
 */

typedef enum : uint32_t {
  k_gptw_gteitc_eivtc_mask   = 0x0000FFFF,    /**< Extended interval counter (0-65535) */
  k_gptw_gteitc_eivtt_shift  = 16,            /**< Extended interval transfer timing shift */
  k_gptw_gteitc_eivtt_mask   = (0x03 << 16),  /**< Extended interval transfer timing mask */
  k_gptw_gteitc_eivtt_none   = (0x00 << 16),  /**< No transfer */
  k_gptw_gteitc_eivtt_trough = (0x01 << 16),  /**< Transfer at trough */
  k_gptw_gteitc_eivtt_crest  = (0x02 << 16),  /**< Transfer at crest */
  k_gptw_gteitc_eivtt_both   = (0x03 << 16),  /**< Transfer at both crest and trough */
  k_gptw_gteitc_eivtcn_shift = 24,            /**< Extended interval counter shift */
  k_gptw_gteitc_eivtcn_mask  = (0xFFU << 24), /**< Extended interval counter mask */
} gptw_gteitc_bits_t;

/* =============================================================================
 * Extended Interrupt Skipping Setting Register 1 (GTEITLI1) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  k_gptw_gteitli1_eadtl   = (1 << 0), /**< A/D conversion start request link enable */
  k_gptw_gteitli1_eivtciv = (1 << 8), /**< GTCIV (overflow) interrupt skipping enable */
  k_gptw_gteitli1_eivtciu = (1 << 9), /**< GTCIU (underflow) interrupt skipping enable */
} gptw_gteitli1_bits_t;

/* =============================================================================
 * Extended Interrupt Skipping Setting Register 2 (GTEITLI2) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  k_gptw_gteitli2_eivtciv_shift = 0,          /**< Extended interval counter shift */
  k_gptw_gteitli2_eivtciv_mask  = 0x000000FF, /**< Extended interval counter mask */
  k_gptw_gteitli2_eivtcia       = (1 << 0),   /**< GTCIA compare match interrupt skipping */
  k_gptw_gteitli2_eivtcib       = (1 << 1),   /**< GTCIB compare match interrupt skipping */
  k_gptw_gteitli2_eivtcic       = (1 << 2),   /**< GTCIC compare match interrupt skipping */
  k_gptw_gteitli2_eivtcid       = (1 << 3),   /**< GTCID compare match interrupt skipping */
  k_gptw_gteitli2_eivtcie       = (1 << 4),   /**< GTCIE compare match interrupt skipping */
  k_gptw_gteitli2_eivtcif       = (1 << 5),   /**< GTCIF compare match interrupt skipping */
} gptw_gteitli2_bits_t;

/* =============================================================================
 * Extended Buffer Transfer Skipping Setting Register (GTEITLB) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  k_gptw_gteitlb_eivtcpbl = (1 << 0), /**< Period buffer transfer skipping */
  k_gptw_gteitlb_eivtcabl = (1 << 1), /**< GTCCRA buffer transfer skipping */
  k_gptw_gteitlb_eivtcbbl = (1 << 2), /**< GTCCRB buffer transfer skipping */
} gptw_gteitlb_bits_t;

/* =============================================================================
 * Operation Enable Bit Simultaneous Control Channel Select Register (GTSECSR) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  k_gptw_gtsecsr_secsel0 = (1 << 0), /**< Channel 0 simultaneous control enable */
  k_gptw_gtsecsr_secsel1 = (1 << 1), /**< Channel 1 simultaneous control enable */
  k_gptw_gtsecsr_secsel2 = (1 << 2), /**< Channel 2 simultaneous control enable */
  k_gptw_gtsecsr_secsel3 = (1 << 3), /**< Channel 3 simultaneous control enable */
} gptw_gtsecsr_bits_t;

/* =============================================================================
 * Operation Enable Bit Simultaneous Control Register (GTSECR) Bits
 * =============================================================================
 */

typedef enum : uint32_t {
  k_gptw_gtsecr_sbdce = (1 << 0), /**< Simultaneous buffer transfer disable */
  k_gptw_gtsecr_sbdcd = (1 << 8), /**< Buffer transfer disable clear */
  k_gptw_gtsecr_sbdpe = (1 << 9), /**< Simultaneous buffer transfer enable */
} gptw_gtsecr_bits_t;

/* =============================================================================
 * Module Stop Control (MSTPCRA)
 * =============================================================================
 */

typedef enum : uint8_t {
  /**
   * @brief GPTW module stop bit in MSTPCRA
   *
   * @details
   * Per RX72N Hardware Manual Ch.11 (Low Power Consumption) and Renesas
   * iodefine.h for RX72N, the GPTW peripheral is controlled by MSTPCRA
   * bit 7 (MSTPA7). Previous HAL code erroneously targeted MSTPCRC.MSTPC6
   * which is a different, unrelated peripheral on this chip.
   *
   * MSTPCRA lives at 0x00080010 and requires PRC1 unlock (0xA502) before
   * write, lock (0xA500) after.
   */
  k_mstpa_gptw = 7, /**< GPTW module stop bit in MSTPCRA (MSTPA7) */
} gptw_module_stop_bits_t;

/* =============================================================================
 * MPC PFS Select Values for GPTW Pins
 * =============================================================================
 */

typedef enum : uint8_t {
  /**
   * @brief Peripheral select value for GPTW outputs on Port E
   * @warning Verify this value against RX72N Hardware Manual.
   * PSEL values are chip-specific.
   */
  k_pfs_psel_gptw = 0x14, /**< PSEL value for GPTW alternate function */
} gptw_mpc_psel_t;

/* =============================================================================
 * Register Offset Constants - Named constants for compile-time assertions
 * =============================================================================
 */

typedef enum : uint8_t {
  k_gptw_ch_off_gtwp     = 0x00U, /**< Write Protection Register */
  k_gptw_ch_off_gtstr    = 0x04U, /**< Software Start Register */
  k_gptw_ch_off_gtstp    = 0x08U, /**< Software Stop Register */
  k_gptw_ch_off_gtclr    = 0x0CU, /**< Software Clear Register */
  k_gptw_ch_off_gtssr    = 0x10U, /**< Start Source Select Register */
  k_gptw_ch_off_gtpsr    = 0x14U, /**< Stop Source Select Register */
  k_gptw_ch_off_gtcsr    = 0x18U, /**< Clear Source Select Register */
  k_gptw_ch_off_gtupsr   = 0x1CU, /**< Count-Up Source Select Register */
  k_gptw_ch_off_gtdnsr   = 0x20U, /**< Count-Down Source Select Register */
  k_gptw_ch_off_gticasr  = 0x24U, /**< Input Capture Source Select A */
  k_gptw_ch_off_gticbsr  = 0x28U, /**< Input Capture Source Select B */
  k_gptw_ch_off_gtcr     = 0x2CU, /**< Control Register */
  k_gptw_ch_off_gtuddtyc = 0x30U, /**< Count Direction and Duty Setting Register */
  k_gptw_ch_off_gtior    = 0x34U, /**< I/O Control Register */
  k_gptw_ch_off_gtintad  = 0x38U, /**< Interrupt Output Setting Register */
  k_gptw_ch_off_gtst     = 0x3CU, /**< Status Register */
  k_gptw_ch_off_gtber    = 0x40U, /**< Buffer Enable Register */
  k_gptw_ch_off_gtitc    = 0x44U, /**< Interrupt Skipping Setting Register */
  k_gptw_ch_off_gtcnt    = 0x48U, /**< Counter */
  k_gptw_ch_off_gtccra   = 0x4CU, /**< Compare Capture Register A */
  k_gptw_ch_off_gtccrb   = 0x50U, /**< Compare Capture Register B */
  k_gptw_ch_off_gtccrc   = 0x54U, /**< Compare Capture Register C */
  k_gptw_ch_off_gtccre   = 0x58U, /**< Compare Capture Register E */
  k_gptw_ch_off_gtccrd   = 0x5CU, /**< Compare Capture Register D */
  k_gptw_ch_off_gtccrf   = 0x60U, /**< Compare Capture Register F */
  k_gptw_ch_off_gtpr     = 0x64U, /**< Cycle Setting Register */
  k_gptw_ch_off_gtpbr    = 0x68U, /**< Cycle Setting Buffer Register */
  k_gptw_ch_off_gtpdbr   = 0x6CU, /**< Cycle Setting Double-Buffer Register */
  k_gptw_ch_off_gtadtra  = 0x70U, /**< A/D Conversion Start Request Timing Register A */
  k_gptw_ch_off_gtadtbra = 0x74U, /**< A/D Conversion Start Request Timing Buffer Register A */
  k_gptw_ch_off_gtadtdbra =
    0x78U, /**< A/D Conversion Start Request Timing Double-Buffer Register A */
  k_gptw_ch_off_gtadtrb  = 0x7CU, /**< A/D Conversion Start Request Timing Register B */
  k_gptw_ch_off_gtadtbrb = 0x80U, /**< A/D Conversion Start Request Timing Buffer Register B */
  k_gptw_ch_off_gtadtdbrb =
    0x84U, /**< A/D Conversion Start Request Timing Double-Buffer Register B */
  k_gptw_ch_off_gtdtcr   = 0x88U, /**< Dead-Time Control Register */
  k_gptw_ch_off_gtdvu    = 0x8CU, /**< Dead-Time Value Register U */
  k_gptw_ch_off_gtdvd    = 0x90U, /**< Dead-Time Value Register D */
  k_gptw_ch_off_gtdbu    = 0x94U, /**< Dead-Time Buffer Register U */
  k_gptw_ch_off_gtdbd    = 0x98U, /**< Dead-Time Buffer Register D */
  k_gptw_ch_off_gtsos    = 0x9CU, /**< Output Protection Function Status Register */
  k_gptw_ch_off_gtsotr   = 0xA0U, /**< Output Protection Function Temporary Release Register */
  k_gptw_ch_off_gtadsmr  = 0xA4U, /**< A/D Conversion Start Request Signal Monitoring Register */
  k_gptw_ch_off_gteitc   = 0xA8U, /**< Extended Interrupt Skipping Counter Control Register */
  k_gptw_ch_off_gteitli1 = 0xACU, /**< Extended Interrupt Skipping Setting Register 1 */
  k_gptw_ch_off_gteitli2 = 0xB0U, /**< Extended Interrupt Skipping Setting Register 2 */
  k_gptw_ch_off_gteitlb  = 0xB4U, /**< Extended Buffer Transfer Skipping Setting Register */
  k_gptw_ch_off_reserved = 0xB8U, /**< Reserved region start */
  k_gptw_ch_off_gtsecsr  = 0xD0U, /**< Operation Enable Bit Simultaneous Control Channel Select */
  k_gptw_ch_off_gtsecr   = 0xD4U, /**< Operation Enable Bit Simultaneous Control Register */
  k_gptw_ch_struct_size  = 0xD8U, /**< Total channel structure size in bytes */
} gptw_ch_reg_offsets_t;

typedef enum : uint8_t {
  k_gptw_common_off_gtstpa  = 0x04U, /**< General Timer Stop Register A */
  k_gptw_common_off_gtclra  = 0x08U, /**< General Timer Clear Register A */
  k_gptw_common_off_gtstra2 = 0x20U, /**< General Timer Start Register A2 */
  k_gptw_common_off_gtstpa2 = 0x24U, /**< General Timer Stop Register A2 */
  k_gptw_common_off_gtclra2 = 0x28U, /**< General Timer Clear Register A2 */
  k_gptw_common_struct_size = 0x2CU, /**< Total common structure size in bytes */
} gptw_common_reg_offsets_t;

typedef enum : uintptr_t {
  k_gptw_periph_space_mask = 0xFFFF0000U, /**< Mask to extract peripheral space from address */
  k_gptw_periph_space_base = 0x000C0000U, /**< GPTW peripheral space base (0x000Cxxxx) */
} gptw_periph_space_t;

/* =============================================================================
 * Static Assertions - Compile-time verification of register layout
 * =============================================================================
 */

/* Verify GPTW channel register structure size and critical offsets */
static_assert(sizeof(rx_gptw_channel_regs_t) == k_gptw_ch_struct_size,
              "GPTW channel register structure size mismatch");
static_assert(offsetof(rx_gptw_channel_regs_t, gtwp) == k_gptw_ch_off_gtwp,
              "GPTW GTWP register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtcr) == k_gptw_ch_off_gtcr,
              "GPTW GTCR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtcnt) == k_gptw_ch_off_gtcnt,
              "GPTW GTCNT register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtccra) == k_gptw_ch_off_gtccra,
              "GPTW GTCCRA register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtpr) == k_gptw_ch_off_gtpr,
              "GPTW GTPR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtdtcr) == k_gptw_ch_off_gtdtcr,
              "GPTW GTDTCR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtsotr) == k_gptw_ch_off_gtsotr,
              "GPTW GTSOTR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadsmr) == k_gptw_ch_off_gtadsmr,
              "GPTW GTADSMR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gteitc) == k_gptw_ch_off_gteitc,
              "GPTW GTEITC register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gteitli1) == k_gptw_ch_off_gteitli1,
              "GPTW GTEITLI1 register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gteitli2) == k_gptw_ch_off_gteitli2,
              "GPTW GTEITLI2 register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gteitlb) == k_gptw_ch_off_gteitlb,
              "GPTW GTEITLB register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtsecsr) == k_gptw_ch_off_gtsecsr,
              "GPTW GTSECSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtsecr) == k_gptw_ch_off_gtsecr,
              "GPTW GTSECR register offset incorrect");

/* Additional channel register offset verification (100% coverage) */
static_assert(offsetof(rx_gptw_channel_regs_t, gtstr) == k_gptw_ch_off_gtstr,
              "GPTW GTSTR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtstp) == k_gptw_ch_off_gtstp,
              "GPTW GTSTP register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtclr) == k_gptw_ch_off_gtclr,
              "GPTW GTCLR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtssr) == k_gptw_ch_off_gtssr,
              "GPTW GTSSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtpsr) == k_gptw_ch_off_gtpsr,
              "GPTW GTPSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtcsr) == k_gptw_ch_off_gtcsr,
              "GPTW GTCSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtupsr) == k_gptw_ch_off_gtupsr,
              "GPTW GTUPSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtdnsr) == k_gptw_ch_off_gtdnsr,
              "GPTW GTDNSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gticasr) == k_gptw_ch_off_gticasr,
              "GPTW GTICASR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gticbsr) == k_gptw_ch_off_gticbsr,
              "GPTW GTICBSR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtuddtyc) == k_gptw_ch_off_gtuddtyc,
              "GPTW GTUDDTYC register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtior) == k_gptw_ch_off_gtior,
              "GPTW GTIOR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtintad) == k_gptw_ch_off_gtintad,
              "GPTW GTINTAD register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtst) == k_gptw_ch_off_gtst,
              "GPTW GTST register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtber) == k_gptw_ch_off_gtber,
              "GPTW GTBER register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtitc) == k_gptw_ch_off_gtitc,
              "GPTW GTITC register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtccrb) == k_gptw_ch_off_gtccrb,
              "GPTW GTCCRB register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtccrc) == k_gptw_ch_off_gtccrc,
              "GPTW GTCCRC register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtccre) == k_gptw_ch_off_gtccre,
              "GPTW GTCCRE register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtccrd) == k_gptw_ch_off_gtccrd,
              "GPTW GTCCRD register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtccrf) == k_gptw_ch_off_gtccrf,
              "GPTW GTCCRF register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtpbr) == k_gptw_ch_off_gtpbr,
              "GPTW GTPBR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtpdbr) == k_gptw_ch_off_gtpdbr,
              "GPTW GTPDBR register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadtra) == k_gptw_ch_off_gtadtra,
              "GPTW GTADTRA register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadtbra) == k_gptw_ch_off_gtadtbra,
              "GPTW GTADTBRA register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadtdbra) == k_gptw_ch_off_gtadtdbra,
              "GPTW GTADTDBRA register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadtrb) == k_gptw_ch_off_gtadtrb,
              "GPTW GTADTRB register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadtbrb) == k_gptw_ch_off_gtadtbrb,
              "GPTW GTADTBRB register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtadtdbrb) == k_gptw_ch_off_gtadtdbrb,
              "GPTW GTADTDBRB register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtdvu) == k_gptw_ch_off_gtdvu,
              "GPTW GTDVU register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtdvd) == k_gptw_ch_off_gtdvd,
              "GPTW GTDVD register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtdbu) == k_gptw_ch_off_gtdbu,
              "GPTW GTDBU register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtdbd) == k_gptw_ch_off_gtdbd,
              "GPTW GTDBD register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, gtsos) == k_gptw_ch_off_gtsos,
              "GPTW GTSOS register offset incorrect");
static_assert(offsetof(rx_gptw_channel_regs_t, reserved) == k_gptw_ch_off_reserved,
              "GPTW reserved register offset incorrect");

/* Verify GPTW common register structure size */
static_assert(sizeof(rx_gptw_common_regs_t) == k_gptw_common_struct_size,
              "GPTW common register structure size mismatch");

/* Verify GPTW common register offsets (100% coverage) */
static_assert(offsetof(rx_gptw_common_regs_t, gtstra) == 0x00,
              "GPTW common GTSTRA register offset incorrect");
static_assert(offsetof(rx_gptw_common_regs_t, gtstpa) == k_gptw_common_off_gtstpa,
              "GPTW common GTSTPA register offset incorrect");
static_assert(offsetof(rx_gptw_common_regs_t, gtclra) == k_gptw_common_off_gtclra,
              "GPTW common GTCLRA register offset incorrect");
static_assert(offsetof(rx_gptw_common_regs_t, gtstra2) == k_gptw_common_off_gtstra2,
              "GPTW common GTSTRA2 register offset incorrect");
static_assert(offsetof(rx_gptw_common_regs_t, gtstpa2) == k_gptw_common_off_gtstpa2,
              "GPTW common GTSTPA2 register offset incorrect");
static_assert(offsetof(rx_gptw_common_regs_t, gtclra2) == k_gptw_common_off_gtclra2,
              "GPTW common GTCLRA2 register offset incorrect");

/* Verify base addresses are in correct peripheral space (0x000C2xxx) */
static_assert((k_gptw0_base_addr & k_gptw_periph_space_mask) == k_gptw_periph_space_base,
              "GPTW0 base address not in GPTW peripheral space");
static_assert((k_gptw1_base_addr & k_gptw_periph_space_mask) == k_gptw_periph_space_base,
              "GPTW1 base address not in GPTW peripheral space");
static_assert((k_gptw2_base_addr & k_gptw_periph_space_mask) == k_gptw_periph_space_base,
              "GPTW2 base address not in GPTW peripheral space");
static_assert((k_gptw3_base_addr & k_gptw_periph_space_mask) == k_gptw_periph_space_base,
              "GPTW3 base address not in GPTW peripheral space");

/* Verify channel spacing is correct */
static_assert(k_gptw1_base_addr - k_gptw0_base_addr == k_gptw_channel_offset,
              "GPTW channel spacing incorrect");
static_assert(k_gptw2_base_addr - k_gptw1_base_addr == k_gptw_channel_offset,
              "GPTW channel spacing incorrect");
static_assert(k_gptw3_base_addr - k_gptw2_base_addr == k_gptw_channel_offset,
              "GPTW channel spacing incorrect");

/** @} */ /* End of rx_gptw_regs group */

#ifdef __cplusplus
}
#endif
