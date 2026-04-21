/**
 * @file rx72n_mtu_regs.h
 * @brief RX72N Multi-Function Timer Unit (MTU3a) register definitions
 *
 * @details
 * This file provides complete hardware register definitions for the RX72N's
 * Multi-Function Timer Unit version 3a (MTU3a). The MTU3a is a highly flexible
 * timer peripheral supporting PWM generation, pulse counting, phase counting
 * (quadrature decoding), and general-purpose timing functions.
 *
 * @par System Architecture
 * @verbatim
 *   STAR Robot Motor Control System
 *   ================================
 *
 *   +---------------------------------------------------------------------+
 *   |                       MTU3a Timer Architecture                      |
 *   |                                                                     |
 *   |  +---------+ +---------+ +---------+     +---------+ +---------+   |
 *   |  |  MTU0   | |  MTU1   | |  MTU2   | ... |  MTU6   | |  MTU7   |   |
 *   |  | 16-bit  | | 16-bit  | | 16-bit  |     | 16-bit  | | 16-bit  |   |
 *   |  |  PWM    | | Encoder | | Encoder |     |Compl PWM| |Compl PWM|   |
 *   |  +----+----+ +----+----+ +----+----+     +----+----+ +----+----+   |
 *   |       |           |           |               |           |        |
 *   |       |     +-----+-----------+-----+         +-----+-----+        |
 *   |       |     |  32-bit Cascaded Mode |               |              |
 *   |       |     |  (MTU1+MTU2 for       |               |              |
 *   |       |     |   Hall Encoders)      |               |              |
 *   |       |     +-----------------------+               |              |
 *   |       |                                             |              |
 *   |  +----+----+  +-------------------------------------+----+         |
 *   |  | TIOCA0  |  |          Complementary PWM               |         |
 *   |  | TIOCB0  |  |  (Dead-time insertion for H-bridge)      |         |
 *   |  +----+----+  +------------------------------------------+         |
 *   |       |                                                            |
 *   +-------+------------------------------------------------------------+
 *           |
 *           v
 *   +-------------------------------------------------------------------+
 *   |                    Motor Control Application                      |
 *   |                                                                   |
 *   |  +-----------+  +-----------------+  +-------------------------+  |
 *   |  |   PWM     |  | Quadrature      |  |   Position/Velocity     |  |
 *   |  |Generation |  | Encoder Input   |  |   Feedback Loop         |  |
 *   |  |(MTU0-4)   |  |(MTU1+MTU2 32b)  |  |   (PID Controller)      |  |
 *   |  +-----+-----+  +--------+--------+  +------------+------------+  |
 *   |        |                 |                        |               |
 *   |        v                 v                        v               |
 *   |  +--------------------------------------------------------------+ |
 *   |  |                     DRV8263H Motor Driver                     | |
 *   |  |            (4x DC Motors with Hall Encoders)                 | |
 *   |  +--------------------------------------------------------------+ |
 *   +-------------------------------------------------------------------+
 * @endverbatim
 *
 * @par MTU3a Channel Capabilities
 * | Channel | Counter | PWM | Phase Count | Complementary | Buffer |
 * |---------|---------|-----|-------------|---------------|--------|
 * | MTU0    | 16-bit  | Yes | No          | No            | Yes    |
 * | MTU1    | 16/32   | Yes | Yes         | No            | No     |
 * | MTU2    | 16/32   | Yes | Yes         | No            | No     |
 * | MTU3    | 16-bit  | Yes | No          | Yes (w/MTU4)  | Yes    |
 * | MTU4    | 16-bit  | Yes | No          | Yes (w/MTU3)  | Yes    |
 * | MTU5    | 16-bit  | Waveform generator (special)     | No     |
 * | MTU6    | 16-bit  | Yes | No          | Yes (w/MTU7)  | Yes    |
 * | MTU7    | 16-bit  | Yes | No          | Yes (w/MTU6)  | Yes    |
 * | MTU8    | 32-bit  | Yes | No          | No            | Yes    |
 *
 * @par STAR Motor Encoder Configuration
 * The STAR robot uses 4x brushed DC gearmotors with Hall effect quadrature
 * encoders (341 PPR per revolution). MTU1 and MTU2 are configured in 32-bit
 * cascaded phase counting mode for high-resolution position tracking:
 *
 * @f[
 * \text{Position (revolutions)} = \frac{\text{TCNTLW}}{341 \times 4}
 * @f]
 *
 * With 32-bit counter at 341 PPR x 4 edges = 1364 counts/rev:
 * - Range: +/-3,146,879 revolutions before overflow
 * - At 210 RPM max: 10.4 days continuous operation without wraparound
 *
 * @par Clock Configuration
 * MTU timers derive their clock from PCLKA (Peripheral Clock A):
 * @f[
 * f_{timer} = \frac{f_{PCLKA}}{prescaler} = \frac{120\text{ MHz}}{prescaler}
 * @f]
 *
 * Available prescaler values: 1, 4, 16, 64, 256, 1024
 *
 * @par PWM Frequency Calculation
 * For PWM mode 1 with compare match on TGRA:
 * @f[
 * f_{PWM} = \frac{f_{PCLKA}}{prescaler \times (TGRA + 1)}
 * @f]
 *
 * Example: 20 kHz PWM at 120 MHz PCLKA, prescaler=1:
 * @f[
 * TGRA = \frac{120\text{ MHz}}{1 \times 20\text{ kHz}} - 1 = 5999
 * @f]
 *
 * @par Hardware Reference
 * - RX72N Group User's Manual: Hardware, Chapter 24 (MTU3a)
 * - Section 24.2: Register Descriptions
 * - Section 24.3: Operations (Timer modes, PWM, Phase counting)
 * - Section 24.4: Interrupts
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion in inline functions
 * - Rule 2: [OK] Fixed loop bounds (N/A - register definitions only)
 * - Rule 3: [OK] Static allocation only (all definitions are compile-time)
 * - Rule 5: [OK] Static assertions verify register layout at compile time
 * - Rule 8: [OK] C23 typed enums eliminate preprocessor for constants
 * - Rule 10: [OK] Header compiles cleanly with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: MTU register definitions only, no driver logic
 * - **Open/Closed**: Extend via new enums; don't modify existing values
 * - **Interface Segregation**: Separate structs for different channel types
 * - **Dependency Inversion**: Higher-level drivers depend on these abstractions
 *
 * @par Related Modules
 * - [rx_mtu.c](rx_mtu_8c.html): MTU HAL driver implementation
 * - [rx_mtu_encoder.h](rx__mtu__encoder_8h.html): Quadrature encoder abstraction
 * - [rx72n_gptw_regs.h](rx72n__gptw__regs_8h.html): Alternative PWM timer (GPTW)
 * - [rx72n_port_regs.h](rx72n__port__regs_8h.html): GPIO for timer I/O pins
 * - [rx72n_mpc.h](rx72n__mpc_8h.html): Pin function selection for MTU I/O
 *
 * @see RX72N Hardware Manual Chapter 24 for complete MTU3a specification
 * @see DOXYGEN_ROADMAP.md for documentation standards
 *
 * @author Locked, Inc. Contributors
 * @date 2026-01-01
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @defgroup mtu_regs MTU Register Definitions
 * @{
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Multi-Function Timer Unit (MTU3a) - Base Addresses
 * =============================================================================
 */

/**
 * @enum rx_mtu_addresses_t
 * @brief MTU3a peripheral base addresses (RX72N memory map Chapter 4)
 *
 * @details
 * Base addresses for all MTU3a timer channels. The MTU3a module occupies
 * memory region 0x000C1200-0x000C1CFF in the RX72N peripheral address space.
 *
 * @par Memory Map Overview
 * @verbatim
 *   Address Range        Channel    Notes
 *   -----------------------------------------------------------------
 *   0x000C1200-0x000C127F  MTU3/4   Interleaved (complementary PWM pair)
 *   0x000C1280-0x000C12FF  TSTRA    Timer Start Register A (MTU0-4, 8)
 *   0x000C1300-0x000C137F  MTU0     General-purpose timer
 *   0x000C1380-0x000C13FF  MTU1     Phase counting capable
 *   0x000C1400-0x000C147F  MTU2     Phase counting capable
 *   0x000C1600-0x000C16FF  MTU8     32-bit native counter
 *   0x000C1A00-0x000C1A7F  MTU6/7   Interleaved (complementary PWM pair)
 *   0x000C1A80-0x000C1AFF  TSTRB    Timer Start Register B (MTU6-7)
 *   0x000C1C80-0x000C1CFF  MTU5     Waveform generator (U/V/W phases)
 * @endverbatim
 *
 * @par Hardware Design Notes
 * - MTU3/MTU4 share base address 0x000C1200 (interleaved registers)
 * - MTU6/MTU7 share base address 0x000C1A00 (interleaved registers)
 * - Interleaving enables synchronized complementary PWM generation
 * - MTU1/MTU2 can cascade to form 32-bit counter (TCNTLW)
 *
 * @par Code Example
 * @code
 * // Access MTU0 timer counter
 * volatile rx_mtu_channel_regs_t* mtu = mtu0();
 * uint16_t count = mtu->tcnt;
 *
 * // Configure MTU0 for 20 kHz PWM
 * mtu->tcr = k_mtu_tcr_tpsc_1 | k_mtu_tcr_cclr_tgra;  // PCLKA/1, clear on TGRA
 * mtu->tgra = 5999;  // Period: 120 MHz / 6000 = 20 kHz
 * mtu->tgrb = 3000;  // 50% duty cycle
 * @endcode
 *
 * @see RX72N Hardware Manual Chapter 4 (Address Map)
 * @see RX72N Hardware Manual Section 24.2 (MTU Register Descriptions)
 */
typedef enum : uintptr_t {
  /** @brief MTU0 base address (0x000C1300) - general purpose 16-bit timer */
  k_mtu0_base_addr = 0x000C1300,

  /** @brief MTU1 base address (0x000C1380) - 16-bit with phase counting, cascadable to 32-bit */
  k_mtu1_base_addr = 0x000C1380,

  /** @brief MTU2 base address (0x000C1400) - 16-bit with phase counting, cascadable to 32-bit */
  k_mtu2_base_addr = 0x000C1400,

  /** @brief MTU3 base address (0x000C1200) - complementary PWM pair with MTU4 */
  k_mtu3_base_addr = 0x000C1200,

  /**
   * @brief MTU4 base address (MTU3 + 1) - interleaved with MTU3
   * @details MTU4 registers are at odd offsets relative to MTU3 at even offsets.
   * This interleaving enables synchronized complementary PWM operation.
   */
  k_mtu4_base_addr = (k_mtu3_base_addr + 1U),

  /** @brief MTU5U base address (0x000C1C80) - waveform generator U-phase */
  k_mtu5u_base_addr = 0x000C1C80,

  /** @brief MTU5V base address (0x000C1C90) - waveform generator V-phase */
  k_mtu5v_base_addr = 0x000C1C90,

  /** @brief MTU5W base address (0x000C1CA0) - waveform generator W-phase */
  k_mtu5w_base_addr = 0x000C1CA0,

  /**
   * @brief MTU6 base address (0x000C1A00) - complementary PWM pair with MTU7
   * @details MTU6 and MTU7 share the same base address with interleaved registers.
   */
  k_mtu6_base_addr = 0x000C1A00,

  /**
   * @brief MTU7 base address (MTU6 + 1) - interleaved with MTU6
   * @details MTU7 registers are at odd offsets relative to MTU6 at even offsets.
   * This interleaving enables synchronized complementary PWM operation.
   * (Manual: MTU7.TCR = 0x000C1A01 = MTU6 base + 1)
   */
  k_mtu7_base_addr = (k_mtu6_base_addr + 1U),

  /** @brief MTU8 base address (0x000C1600) - native 32-bit counter */
  k_mtu8_base_addr = 0x000C1600,

  /** @brief TSTRA base address (0x000C1280) - timer start control for MTU0-4, MTU8 */
  k_mtu_tstra_base_addr = 0x000C1280,

  /** @brief TSTRB base address (0x000C1A80) - timer start control for MTU6-7 */
  k_mtu_tstrb_base_addr = 0x000C1A80,
} rx_mtu_addresses_t;

/**
 * @brief Bit value constants for shift operations
 *
 * Named constants to eliminate magic numbers in bit shift expressions.
 */
typedef enum : uint8_t {
  k_mtu_bit_value_set = 1, /**< Bit value to shift (replaces 1, 1U, 0x01 in shifts) */
} mtu_bit_values_t;

/**
 * @struct rx_mtu_channel_regs_t
 * @brief Standard MTU channel register map (MTU0-MTU2)
 *
 * @details
 * Memory-mapped register structure for consecutive-layout 16-bit MTU channels.
 * This structure applies to MTU0, MTU1, and MTU2 only.
 * MTU6 and MTU7 have an interleaved register layout identical to MTU3/MTU4
 * and must be accessed via mtu6() which returns rx_mtu3_channel_regs_t*.
 *
 * @par Register Memory Layout (18 bytes total)
 * @verbatim
 *   Offset  Size  Register  Description
 *   --------------------------------------------------------------
 *   0x00    1     TCR       Timer Control (prescaler, edge, clear source)
 *   0x01    1     TMDR      Timer Mode (normal, PWM1, PWM2, phase count)
 *   0x02    1     TIORH     Timer I/O Control H (TIOCA/TIOCB output mode)
 *   0x03    1     TIORL     Timer I/O Control L (TIOCC/TIOCD output mode)
 *   0x04    1     TIER      Timer Interrupt Enable (TGIA-D, overflow)
 *   0x05    1     TSR       Timer Status (interrupt flags, read to clear)
 *   0x06    2     TCNT      Timer Counter (16-bit, counts up/down)
 *   0x08    2     TGRA      Timer General Register A (compare/capture)
 *   0x0A    2     TGRB      Timer General Register B (compare/capture)
 *   0x0C    2     TGRC      Timer General Register C (compare/capture)
 *   0x0E    2     TGRD      Timer General Register D (compare/capture)
 * @endverbatim
 *
 * @par Timer Operating Modes
 * The TMDR register MD bits select the operating mode:
 * - Normal (0x00): Free-running counter with compare match
 * - PWM Mode 1 (0x02): TGRA=period, TGRB=duty cycle
 * - PWM Mode 2 (0x03): TGRC/TGRD as buffer registers
 * - Phase Counting (0x04-0x07): Quadrature encoder input
 *
 * @par PWM Generation Example
 * @code
 * // Configure MTU0 for 20 kHz PWM, 50% duty cycle
 * volatile rx_mtu_channel_regs_t* mtu = mtu0();
 *
 * // Stop timer during configuration
 * mtu_tstra()->tstr &= ~k_mtu_tstr_cst0;
 *
 * // Configure prescaler and clear source
 * mtu->tcr = k_mtu_tcr_tpsc_1 | k_mtu_tcr_cclr_tgra;
 *
 * // Set PWM mode 1
 * mtu->tmdr = k_mtu_tmdr_md_pwm1;
 *
 * // Configure output: low initially, high on compare match
 * mtu->tiorh = k_mtu_tior_init_low;
 *
 * // Set period and duty cycle (120 MHz / 6000 = 20 kHz)
 * mtu->tgra = 5999;  // Period
 * mtu->tgrb = 3000;  // 50% duty (high for 3000 counts)
 *
 * // Clear counter and start timer
 * mtu->tcnt = 0;
 * mtu_tstra()->tstr |= k_mtu_tstr_cst0;
 * @endcode
 *
 * @invariant Structure size must be exactly 18 bytes (0x10 offset)
 * @invariant All registers are volatile for hardware access
 *
 * @see rx_mtu34_channel_regs_t for extended MTU3/MTU4 with TGR E/F
 * @see rx_mtu12_phase_regs_t for 32-bit phase counting mode
 * @see RX72N Hardware Manual Section 24.2 (Register Descriptions)
 */
typedef struct {
  /**
   * @brief Timer Control Register (TCR) - prescaler, edge, clear source
   * @details Controls clock source selection and counter clear conditions.
   * @see mtu_tcr_bits_t for bit field definitions
   */
  volatile uint8_t tcr;

  /**
   * @brief Timer Mode Register (TMDR) - operating mode selection
   * @details Selects normal, PWM, or phase counting mode.
   * @see mtu_tmdr_bits_t for mode values
   */
  volatile uint8_t tmdr;

  /**
   * @brief Timer I/O Control Register H - TIOCA/TIOCB output control
   * @details Controls output pin behavior on compare match A/B.
   * @see mtu_tior_bits_t for output mode values
   */
  volatile uint8_t tiorh;

  /**
   * @brief Timer I/O Control Register L - TIOCC/TIOCD output control
   * @details Controls output pin behavior on compare match C/D.
   * @see mtu_tior_bits_t for output mode values
   */
  volatile uint8_t tiorl;

  /**
   * @brief Timer Interrupt Enable Register (TIER)
   * @details Enables interrupts for compare match and overflow events.
   */
  volatile uint8_t tier;

  /**
   * @brief Timer Status Register (TSR) - interrupt flags
   * @details Contains interrupt flags; read to check, write 0 to clear.
   *
   * @warning Per RX72N HW manual R01UH0824EJ0111 Section 21.2 (MTU register
   *          tables), MTU0 has NO TSR at this offset -- accessing this field
   *          via the MTU0 base (0x000C1300) targets a reserved byte and the
   *          result is undefined. This field is only valid on MTU channels
   *          that define a TSR (e.g., MTU1, MTU2, MTU3/4 TSR copies); callers
   *          must gate access by channel before reading/writing tsr.
   */
  volatile uint8_t tsr;

  /**
   * @brief Timer Counter (TCNT) - 16-bit counter value
   * @details Current counter value. Cleared by compare match or overflow
   * depending on TCR.CCLR setting.
   */
  volatile uint16_t tcnt;

  /**
   * @brief Timer General Register A (TGRA) - compare/capture
   * @details In PWM mode: defines period. In capture mode: stores captured value.
   */
  volatile uint16_t tgra;

  /**
   * @brief Timer General Register B (TGRB) - compare/capture
   * @details In PWM mode: defines duty cycle (when to toggle output).
   */
  volatile uint16_t tgrb;

  /**
   * @brief Timer General Register C (TGRC) - compare/capture
   * @details Additional compare register; can buffer TGRA in some modes.
   */
  volatile uint16_t tgrc;

  /**
   * @brief Timer General Register D (TGRD) - compare/capture
   * @details Additional compare register; can buffer TGRB in some modes.
   */
  volatile uint16_t tgrd;
} rx_mtu_channel_regs_t;

/**
 * @enum mtu3_padding_sizes_t
 * @brief Named padding array sizes for rx_mtu3_channel_regs_t reserved fields
 *
 * @details
 * Each constant names the number of reserved bytes between two adjacent MTU3
 * registers in the hardware layout.  The comments show which hardware registers
 * occupy those byte ranges (verified against RX72N Ch 5 I/O Register Address
 * Table).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief 2 reserved bytes at +0x06-0x07: MTU4.TIORH, MTU4.TIORL */
  k_mtu3_pad_after_tiorl = 2U,

  /** @brief 7 reserved bytes at +0x09-0x0F: MTU4.TIER, MTU.TOERA, reserved x2,
   *         MTU.TGCRA, MTU.TOCR1A, MTU.TOCR2A */
  k_mtu3_pad_after_tier = 7U,

  /** @brief 6 reserved bytes at +0x12-0x17: MTU4.TCNT(2), MTU.TCDRA(2), MTU.TDDRA(2) */
  k_mtu3_pad_after_tcnt = 6U,

  /** @brief 8 reserved bytes at +0x1C-0x23: MTU4.TGRA(2), MTU4.TGRB(2),
   *         MTU.TCNTSA(2), MTU.TCBRA(2) */
  k_mtu3_pad_after_tgrb = 8U,

  /** @brief 4 reserved bytes at +0x28-0x2B: MTU4.TGRC(2), MTU4.TGRD(2) */
  k_mtu3_pad_after_tgrd = 4U,

  /** @brief 11 reserved bytes at +0x2D-0x37: MTU4.TSR, reserved x2, MTU.TITCR1A,
   *         MTU.TITCNT1A, MTU.TBTERA, reserved, MTU.TDERA, reserved, MTU.TOLBRA,
   *         reserved */
  k_mtu3_pad_after_tsr = 11U,

  /** @brief 19 reserved bytes at +0x39-0x4B: MTU4.TBTM, MTU.TITMRA, MTU.TITCR2A,
   *         MTU.TITCNT2A, reserved, MTU4.TADCR(2), reserved(2), MTU4.TADCORA(2),
   *         MTU4.TADCORB(2), MTU4.TADCOBRA(2), MTU4.TADCOBRB(2) */
  k_mtu3_pad_after_tbtm = 19U,

  /** @brief 37 reserved bytes at +0x4D-0x71: MTU4.TCR2, reserved x18, MTU.TWCRA,
   *         reserved x15, MTU.TMDR2A, reserved */
  k_mtu3_pad_after_tcr2 = 37U,
} mtu3_padding_sizes_t;

/**
 * @struct rx_mtu3_channel_regs_t
 * @brief MTU3 register map accessed via base address 0x000C1200
 *
 * @details
 * Memory-mapped register structure for MTU3 channel, which supports
 * complementary PWM generation for H-bridge motor driver control along
 * with standard timer/counter and phase-counting encoder modes.
 *
 * The MTU3/MTU4 peripheral block places MTU3 and MTU4 registers at
 * INTERLEAVED addresses starting at 0x000C1200. MTU3 registers are NOT
 * at consecutive byte offsets -- there are large gaps between them where
 * MTU4-specific registers and shared MTU3/4 control registers reside.
 *
 * @warning This struct is ONLY valid for MTU3 (mtu3() base 0x000C1200).
 *          MTU4 registers have DIFFERENT offsets relative to the MTU4 base
 *          address (0x000C1201) and CANNOT use this struct.  For MTU4,
 *          compute each register address individually using the Ch 5 I/O
 *          address table (e.g. MTU4.TIORH = 0x000C1206, not base+0x04).
 *          See RX72N Hardware Manual Ch 5 I/O Register Address Table.
 *
 * @par Register Memory Layout (Ch 5 I/O Register Address Table, verified)
 * @verbatim
 *   Offset  Size  Register  Address       Description
 *   ----------------------------------------------------------
 *   0x00    1     tcr       0x000C1200    Timer Control (prescaler, clear source)
 *   0x01    -     [MTU4.TCR]              (reserved -- do not access via this struct)
 *   0x02    1     tmdr1     0x000C1202    Timer Mode Register 1 (normal/PWM/phase)
 *   0x03    -     [MTU4.TMDR1]            (reserved)
 *   0x04    1     tiorh     0x000C1204    Timer I/O Control H (TIOCA/TIOCB output)
 *   0x05    1     tiorl     0x000C1205    Timer I/O Control L (TIOCC/TIOCD output)
 *   0x06    -     [MTU4.TIORH]            (reserved)
 *   0x07    -     [MTU4.TIORL]            (reserved)
 *   0x08    1     tier      0x000C1208    Timer Interrupt Enable
 *   0x09-0x0F -   [shared/MTU4]           (reserved -- TOERA, TGCRA, TOCR1A, TOCR2A)
 *   0x10    2     tcnt      0x000C1210    Timer Counter (16-bit)
 *   0x12-0x17 -  [shared/MTU4]           (reserved -- MTU4.TCNT, TCDRA, TDDRA)
 *   0x18    2     tgra      0x000C1218    Timer General Register A (period/capture)
 *   0x1A    2     tgrb      0x000C121A    Timer General Register B (duty/capture)
 *   0x1C-0x23 -  [shared/MTU4]           (reserved -- MTU4.TGRA/B, TCNTSA, TCBRA)
 *   0x24    2     tgrc      0x000C1224    Timer General Register C (buffer/capture)
 *   0x26    2     tgrd      0x000C1226    Timer General Register D (buffer/capture)
 *   0x28-0x2B -  [MTU4.TGRC/D]           (reserved)
 *   0x2C    1     tsr       0x000C122C    Timer Status Register (interrupt flags)
 *   0x2D-0x37 -  [shared/MTU4]           (reserved -- MTU4.TSR, TITCR1A, TBTERA, etc.)
 *   0x38    1     tbtm      0x000C1238    Timer Buffer Transfer Mode
 *   0x39-0x4B -  [shared/MTU4]           (reserved -- MTU4.TBTM, TITMRA, MTU4.TADCR etc.)
 *   0x4C    1     tcr2      0x000C124C    Timer Control Register 2 (phase count edge)
 *   0x4D-0x71 -  [shared/MTU4]           (reserved -- MTU4.TCR2, TWCRA, TMDR2A, etc.)
 *   0x72    2     tgre      0x000C1272    Timer General Register E (dead-time/extended)
 * @endverbatim
 *
 * @note MTU3 has NO TGRF register. Only MTU4 has TGRF (at 0x000C1276).
 *
 * @par Dead-Time Insertion via TGRE
 * TGRE provides dead-time values to prevent H-bridge shoot-through.
 * MTU4.TGRE and MTU4.TGRF must be set separately using direct address access.
 * @f[
 * t_{dead} = \frac{\text{TGRE}}{f_{PCLKA}} = \frac{\text{TGRE}}{120\text{ MHz}}
 * @f]
 *
 * Example: 500ns dead-time requires TGRE = 60 (at 120 MHz PCLKA)
 *
 * @par Complementary PWM Example
 * @code
 * // Configure MTU3/4 for complementary PWM with 500ns dead-time
 * volatile rx_mtu3_channel_regs_t* mtu = mtu3();
 *
 * // Stop both timers
 * mtu_tstra()->tstr &= ~(k_mtu_tstr_cst3 | k_mtu_tstr_cst4);
 *
 * // Configure for complementary PWM mode (via TMDR1)
 * mtu->tmdr1 = 0x0D;  // Complementary PWM mode
 *
 * // Set period (TGRA) and duty cycle (TGRB)
 * mtu->tgra = 5999;   // 20 kHz period at 120 MHz PCLKA
 * mtu->tgrb = 3000;   // 50% duty cycle
 *
 * // Set dead-time for MTU3 (TGRE at 0x000C1272)
 * mtu->tgre = 60;     // 500ns at 120 MHz
 * // MTU4.TGRF must be set via direct address (MTU4 uses different struct offsets):
 * *(volatile uint16_t*)0x000C1276U = 60;  // MTU4.TGRF = 500ns dead-time
 *
 * // Enable buffer transfer on counter clear
 * mtu->tbtm = 0x03;
 *
 * // Start both timers synchronously
 * mtu_tstra()->tstr |= (k_mtu_tstr_cst3 | k_mtu_tstr_cst4);
 * @endcode
 *
 * @invariant Struct size is 0x74 bytes (116) covering MTU3 registers up to TGRE
 * @invariant Only valid when cast from mtu3() base address (0x000C1200)
 * @invariant MTU4 registers CANNOT use this struct (different relative offsets)
 *
 * @see rx_mtu_channel_regs_t for MTU0/1/2 standard (consecutive) channel struct
 * @see mtu6() for MTU6 access using this interleaved struct
 * @see RX72N Hardware Manual Ch 5 I/O Register Address Table (MTU3a registers)
 * @see RX72N Hardware Manual Section 24.2 (MTU3/MTU4 Register Descriptions)
 */
typedef struct {
  /** @brief TCR -- Timer Control Register (+0x00 = 0x000C1200) */
  volatile uint8_t tcr;

  /** @brief Reserved -- MTU4.TCR at this address (+0x01 = 0x000C1201) */
  volatile uint8_t reserved_01;

  /** @brief TMDR1 -- Timer Mode Register 1 (+0x02 = 0x000C1202) */
  volatile uint8_t tmdr1;

  /** @brief Reserved -- MTU4.TMDR1 at this address (+0x03 = 0x000C1203) */
  volatile uint8_t reserved_03;

  /** @brief TIORH -- Timer I/O Control H: TIOCA/TIOCB output modes (+0x04 = 0x000C1204) */
  volatile uint8_t tiorh;

  /** @brief TIORL -- Timer I/O Control L: TIOCC/TIOCD output modes (+0x05 = 0x000C1205) */
  volatile uint8_t tiorl;

  /** @brief Reserved -- MTU4.TIORH and MTU4.TIORL occupy +0x06 and +0x07 */
  volatile uint8_t reserved_06[k_mtu3_pad_after_tiorl];

  /** @brief TIER -- Timer Interrupt Enable Register (+0x08 = 0x000C1208) */
  volatile uint8_t tier;

  /**
   * @brief Reserved (+0x09-0x0F) -- MTU4.TIER, MTU.TOERA, reserved x2,
   *        MTU.TGCRA, MTU.TOCR1A, MTU.TOCR2A occupy these addresses
   */
  volatile uint8_t reserved_09[k_mtu3_pad_after_tier];

  /** @brief TCNT -- Timer Counter 16-bit (+0x10 = 0x000C1210) */
  volatile uint16_t tcnt;

  /**
   * @brief Reserved (+0x12-0x17) -- MTU4.TCNT(2), MTU.TCDRA(2),
   *        MTU.TDDRA(2) occupy these addresses
   */
  volatile uint8_t reserved_12[k_mtu3_pad_after_tcnt];

  /** @brief TGRA -- Timer General Register A: period in PWM mode (+0x18 = 0x000C1218) */
  volatile uint16_t tgra;

  /** @brief TGRB -- Timer General Register B: duty cycle in PWM mode (+0x1A = 0x000C121A) */
  volatile uint16_t tgrb;

  /**
   * @brief Reserved (+0x1C-0x23) -- MTU4.TGRA(2), MTU4.TGRB(2),
   *        MTU.TCNTSA(2), MTU.TCBRA(2) occupy these addresses
   */
  volatile uint8_t reserved_1c[k_mtu3_pad_after_tgrb];

  /** @brief TGRC -- Timer General Register C: buffer for TGRA (+0x24 = 0x000C1224) */
  volatile uint16_t tgrc;

  /** @brief TGRD -- Timer General Register D: buffer for TGRB (+0x26 = 0x000C1226) */
  volatile uint16_t tgrd;

  /** @brief Reserved (+0x28-0x2B) -- MTU4.TGRC(2), MTU4.TGRD(2) */
  volatile uint8_t reserved_28[k_mtu3_pad_after_tgrd];

  /** @brief TSR -- Timer Status Register: interrupt flags, write 0 to clear (+0x2C = 0x000C122C) */
  volatile uint8_t tsr;

  /**
   * @brief Reserved (+0x2D-0x37) -- MTU4.TSR, reserved x2, MTU.TITCR1A,
   *        MTU.TITCNT1A, MTU.TBTERA, reserved, MTU.TDERA, reserved,
   *        MTU.TOLBRA, reserved occupy these addresses
   */
  volatile uint8_t reserved_2d[k_mtu3_pad_after_tsr];

  /** @brief TBTM -- Timer Buffer Transfer Mode Register (+0x38 = 0x000C1238) */
  volatile uint8_t tbtm;

  /**
   * @brief Reserved (+0x39-0x4B) -- MTU4.TBTM, MTU.TITMRA, MTU.TITCR2A,
   *        MTU.TITCNT2A, reserved, MTU4.TADCR(2), reserved(2),
   *        MTU4.TADCORA(2), MTU4.TADCORB(2), MTU4.TADCOBRA(2),
   *        MTU4.TADCOBRB(2) occupy these addresses
   */
  volatile uint8_t reserved_39[k_mtu3_pad_after_tbtm];

  /** @brief TCR2 -- Timer Control Register 2: phase count clock edge (+0x4C = 0x000C124C) */
  volatile uint8_t tcr2;

  /**
   * @brief Reserved (+0x4D-0x71) -- MTU4.TCR2, reserved x18, MTU.TWCRA,
   *        reserved x15, MTU.TMDR2A, reserved occupy these addresses
   */
  volatile uint8_t reserved_4d[k_mtu3_pad_after_tcr2];

  /**
   * @brief TGRE -- Timer General Register E: dead-time / extended TGR (+0x72 = 0x000C1272)
   * @details MTU3 has TGRE only. MTU4 has both TGRE (0x000C1274) and TGRF (0x000C1276).
   *          MTU3 does NOT have a TGRF register.
   */
  volatile uint16_t tgre;
} rx_mtu3_channel_regs_t;

/**
 * @enum mtu12_phase_padding_sizes_t
 * @brief Named padding array sizes for rx_mtu12_phase_regs_t reserved fields
 *
 * @details
 * Each constant names the number of reserved bytes between two adjacent
 * registers in the 32-bit phase counting register layout.
 * Verified against RX72N Hardware Manual Ch 5 I/O Register Address Table.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief 14 reserved bytes at offset 0x12-0x1F (gap before 32-bit long-word regs) */
  k_mtu12p_pad_before_tcntlw = 14U,
} mtu12_phase_padding_sizes_t;

/**
 * @struct rx_mtu12_phase_regs_t
 * @brief MTU1/MTU2 extended register map for 32-bit phase counting mode
 *
 * @details
 * Extended register structure for MTU1 and MTU2 when operating in 32-bit
 * cascaded phase counting mode. This mode is ideal for quadrature encoder
 * applications requiring high-resolution position tracking over extended
 * ranges without software overflow handling.
 *
 * @par STAR Robot Encoder Configuration
 * The STAR robot uses 4x brushed DC gearmotors with Hall effect quadrature
 * encoders. MTU1 and MTU2 are cascaded to provide 32-bit position tracking:
 *
 * @verbatim
 *   Encoder Signals              MTU1/MTU2 32-bit Cascaded Mode
 *   ------------------------    ---------------------------------
 *
 *   Motor 1 (FL)                 +--------------------------------+
 *   +-------------+              |        32-bit TCNTLW           |
 *   | Hall A ---->+-> MTCLKA --->|  [MTU2.TCNT | MTU1.TCNT]      |
 *   | Hall B ---->+-> MTCLKB --->|  Upper 16    Lower 16         |
 *   +-------------+              |                                |
 *   341 PPR x 4 edges            |  Position = TCNTLW / 1364      |
 *   = 1364 counts/rev            |  Range: +/-3.1M revolutions      |
 *                                +--------------------------------+
 * @endverbatim
 *
 * @par 32-bit Cascaded Mode Benefits
 * - **Extended Range**: 2^32 counts vs 2^16 counts (65536x more range)
 * - **Atomic Access**: TCNTLW, TGRALW, TGRBLW are 32-bit atomic operations
 * - **No Race Conditions**: No need to disable interrupts during read
 * - **Hardware Implementation**: Zero software overhead for overflow tracking
 *
 * @par Position Calculation
 * For STAR robot Hall encoders (341 PPR, 4x quadrature = 1364 counts/rev):
 * @f[
 * \text{Position (revolutions)} = \frac{\text{TCNTLW}}{1364}
 * @f]
 * @f[
 * \text{Position (degrees)} = \frac{\text{TCNTLW} \times 360deg}{1364}
 * @f]
 *
 * @par Overflow Analysis
 * At maximum motor speed (210 RPM = 3.5 rev/sec):
 * @f[
 * t_{overflow} = \frac{2^{31}}{1364 \times 3.5} = 449,686 \text{ seconds} = 10.4 \text{ days}
 * @f]
 *
 * @par Register Memory Layout (44 bytes total)
 * @verbatim
 *   Offset  Size  Register   Description
 *   --------------------------------------------------------------
 *   0x00    1     TCR        Timer Control (prescaler, edge)
 *   0x01    1     TMDR       Timer Mode (phase counting mode)
 *   0x02    1     TIORH      Timer I/O Control H
 *   0x03    1     TIORL      Timer I/O Control L
 *   0x04    1     TIER       Timer Interrupt Enable
 *   0x05    1     TSR        Timer Status (flags)
 *   0x06    2     TCNT       Timer Counter (16-bit, lower word)
 *   0x08    2     TGRA       Timer General Register A (16-bit)
 *   0x0A    2     TGRB       Timer General Register B (16-bit)
 *   0x0C    2     TGRC       Timer General Register C
 *   0x0E    2     TGRD       Timer General Register D
 *   0x10    1     Reserved   -
 *   0x11    1     TMDR3      32-bit mode control (LWA, PHCKSEL)
 *   0x12    14    Reserved   -
 *   0x20    4     TCNTLW     32-bit cascaded counter (atomic)
 *   0x24    4     TGRALW     32-bit general register A (atomic)
 *   0x28    4     TGRBLW     32-bit general register B (atomic)
 * @endverbatim
 *
 * @par 32-bit Mode Configuration Example
 * @code
 * // Configure MTU1+MTU2 for 32-bit quadrature encoder (Hall sensors)
 * volatile rx_mtu12_phase_regs_t* mtu = mtu1_phase();
 *
 * // Stop timers during configuration
 * mtu_tstra()->tstr &= ~(k_mtu_tstr_cst1 | k_mtu_tstr_cst2);
 *
 * // Configure for phase counting mode 4 (count on both edges)
 * mtu->tmdr = 0x04;  // Phase counting mode 4
 *
 * // Enable 32-bit cascaded mode (LWA bit) with both-edge counting
 * mtu->tmdr3 = k_mtu_tmdr3_lwa | k_mtu_tmdr3_phcksel;
 *
 * // Clear 32-bit counter
 * mtu->tcntlw = 0;
 *
 * // Start both timers
 * mtu_tstra()->tstr |= (k_mtu_tstr_cst1 | k_mtu_tstr_cst2);
 *
 * // Read 32-bit position (atomic, no race condition)
 * int32_t position = (int32_t)mtu->tcntlw;
 * @endcode
 *
 * @invariant Structure size must be exactly 44 bytes (0x2C)
 * @invariant TMDR3 at offset 0x11 controls 32-bit cascaded mode
 * @invariant TCNTLW/TGRALW/TGRBLW are 32-bit aligned for atomic access
 *
 * @see mtu_tmdr3_bits_t for TMDR3 bit definitions
 * @see rx_mtu_channel_regs_t for standard 16-bit mode
 * @see RX72N Hardware Manual Section 24.2.25 (TMDR3 Register)
 * @see RX72N Hardware Manual Section 24.2.26 (32-bit Registers)
 * @see RX72N Hardware Manual Section 24.3.5 (Phase Counting Mode)
 */
typedef struct {
  /* ----------- Standard 16-bit registers (0x00-0x0F) ----------- */

  /**
   * @brief Timer Control Register (TCR) - prescaler and edge selection
   * @details For phase counting, TCR.CKEG selects which edge of MTCLKA/B to count.
   */
  volatile uint8_t tcr;

  /**
   * @brief Timer Mode Register (TMDR) - phase counting mode select
   * @details Mode 4-7 select different phase counting configurations.
   * Mode 4: Count up/down on MTCLKA, direction from MTCLKB.
   */
  volatile uint8_t tmdr;

  /** @brief Timer I/O Control Register H (typically unused in phase counting) */
  volatile uint8_t tiorh;

  /** @brief Timer I/O Control Register L (typically unused in phase counting) */
  volatile uint8_t tiorl;

  /**
   * @brief Timer Interrupt Enable Register (TIER)
   * @details Enable overflow/underflow interrupts for position limit detection.
   */
  volatile uint8_t tier;

  /**
   * @brief Timer Status Register (TSR) - interrupt flags
   * @details Contains overflow/underflow flags for position monitoring.
   */
  volatile uint8_t tsr;

  /**
   * @brief Timer Counter (TCNT) - lower 16 bits of cascaded counter
   * @details Use TCNTLW for atomic 32-bit access when LWA bit is set.
   */
  volatile uint16_t tcnt;

  /** @brief Timer General Register A - 16-bit compare (use TGRALW for 32-bit) */
  volatile uint16_t tgra;

  /** @brief Timer General Register B - 16-bit compare (use TGRBLW for 32-bit) */
  volatile uint16_t tgrb;

  /** @brief Timer General Register C - 16-bit compare */
  volatile uint16_t tgrc;

  /** @brief Timer General Register D - 16-bit compare */
  volatile uint16_t tgrd;

  /* ----------- Extended 32-bit mode registers (0x10-0x2B) ----------- */

  /** @brief Reserved byte at offset 0x10 */
  uint8_t reserved1;

  /**
   * @brief Timer Mode Register 3 (TMDR3) - 32-bit mode control
   * @details
   * - Bit 0 (LWA): Long Word Access enable (32-bit cascaded mode)
   * - Bit 1 (PHCKSEL): Phase counting clock select (both edges)
   * @see mtu_tmdr3_bits_t for bit definitions
   */
  volatile uint8_t tmdr3;

  /** @brief Reserved bytes at offset 0x12-0x1F (14 bytes, before 32-bit long-word regs) */
  uint8_t reserved2[k_mtu12p_pad_before_tcntlw];

  /**
   * @brief 32-bit Cascaded Counter (TCNTLW) - atomic position value
   * @details
   * Combined MTU1.TCNT (lower 16 bits) and MTU2.TCNT (upper 16 bits).
   * Provides atomic 32-bit read/write without race conditions.
   * Sign-extended for bidirectional counting (use int32_t cast).
   *
   * @note Only valid when TMDR3.LWA = 1 (32-bit mode enabled)
   */
  volatile uint32_t tcntlw;

  /**
   * @brief 32-bit General Register A (TGRALW) - atomic compare value
   * @details 32-bit compare register for position threshold interrupts.
   * @note Only valid when TMDR3.LWA = 1
   */
  volatile uint32_t tgralw;

  /**
   * @brief 32-bit General Register B (TGRBLW) - atomic compare value
   * @details 32-bit compare register for position threshold interrupts.
   * @note Only valid when TMDR3.LWA = 1
   */
  volatile uint32_t tgrblw;
} rx_mtu12_phase_regs_t;

/**
 * @brief MTU Start Register Map
 * @details
 * Timer start/stop control registers.
 * Base addresses:
 * - TSTRA: 0x000C1280 (MTU0-4, MTU8)
 * - TSTRB: 0x000C1A80 (MTU6-7)
 */
typedef struct {
  volatile uint8_t tstr; /**< Timer Start Register (channel enable bits) */
} rx_mtu_tstr_regs_t;

/* =============================================================================
 * Inline Register Accessors
 * =============================================================================
 */

/**
 * @brief Get pointer to MTU0 registers
 *
 * @details
 * Returns a volatile pointer to MTU0's register structure at base address
 * 0x000C1300. MTU0 is a general-purpose 16-bit timer commonly used for
 * PWM generation or interval timing.
 *
 * @return Volatile pointer to MTU0 register structure
 *
 * @par Example
 * @code
 * // Read MTU0 counter value
 * uint16_t count = mtu0()->tcnt;
 *
 * // Configure MTU0 for 20 kHz PWM
 * mtu0()->tcr = k_mtu_tcr_tpsc_1 | k_mtu_tcr_cclr_tgra;
 * mtu0()->tgra = 5999;  // Period: 120 MHz / 6000 = 20 kHz
 * @endcode
 *
 * @note Thread-safe: returns constant hardware address
 * @see rx_mtu_channel_regs_t for register layout
 */
static inline volatile rx_mtu_channel_regs_t* mtu0(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu0_base_addr;
}

/**
 * @brief Get pointer to MTU1 registers (16-bit mode)
 *
 * @details
 * Returns a volatile pointer to MTU1's standard 16-bit register structure.
 * For 32-bit cascaded phase counting mode, use mtu1_phase() instead.
 *
 * @return Volatile pointer to MTU1 register structure
 *
 * @see mtu1_phase() for 32-bit cascaded phase counting
 * @see rx_mtu_channel_regs_t for register layout
 */
static inline volatile rx_mtu_channel_regs_t* mtu1(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu1_base_addr;
}

/**
 * @brief Get pointer to MTU2 registers (16-bit mode)
 *
 * @details
 * Returns a volatile pointer to MTU2's standard 16-bit register structure.
 * For 32-bit cascaded phase counting mode, use mtu2_phase() instead.
 *
 * @return Volatile pointer to MTU2 register structure
 *
 * @see mtu2_phase() for 32-bit cascaded phase counting
 * @see rx_mtu_channel_regs_t for register layout
 */
static inline volatile rx_mtu_channel_regs_t* mtu2(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu2_base_addr;
}

/**
 * @brief Get pointer to MTU1 extended registers for 32-bit phase counting mode
 *
 * @details
 * Provides access to MTU1's extended register set including TMDR3 and 32-bit
 * cascaded counter registers (TCNTLW, TGRALW, TGRBLW). Use this accessor when
 * configuring or reading 32-bit cascaded phase counting mode.
 *
 * For standard 16-bit phase counting, use mtu1() instead.
 *
 * @return Volatile pointer to MTU1 extended register structure
 *
 * @see rx_mtu12_phase_regs_t for complete register map
 * @see RX72N Hardware Manual Section 24.2.25 for TMDR3 register
 * @see RX72N Hardware Manual Section 24.2.26 for 32-bit registers
 */
static inline volatile rx_mtu12_phase_regs_t* mtu1_phase(void)
{
  return (volatile rx_mtu12_phase_regs_t*)k_mtu1_base_addr;
}

/**
 * @brief Get pointer to MTU2 extended registers for 32-bit phase counting mode
 *
 * @details
 * Provides access to MTU2's extended register set including TMDR3 and 32-bit
 * cascaded counter registers (TCNTLW, TGRALW, TGRBLW). Use this accessor when
 * configuring or reading 32-bit cascaded phase counting mode.
 *
 * For standard 16-bit phase counting, use mtu2() instead.
 *
 * @return Volatile pointer to MTU2 extended register structure
 *
 * @see rx_mtu12_phase_regs_t for complete register map
 * @see RX72N Hardware Manual Section 24.2.25 for TMDR3 register
 * @see RX72N Hardware Manual Section 24.2.26 for 32-bit registers
 */
static inline volatile rx_mtu12_phase_regs_t* mtu2_phase(void)
{
  return (volatile rx_mtu12_phase_regs_t*)k_mtu2_base_addr;
}

/**
 * @brief Get pointer to MTU3 registers via rx_mtu3_channel_regs_t struct
 *
 * @details
 * Returns a pointer to MTU3's register space cast to rx_mtu3_channel_regs_t.
 * The struct has been carefully padded to match the actual hardware layout
 * verified against the RX72N Ch 5 I/O Register Address Table.
 *
 * MTU3 and MTU4 registers are interleaved at 0x000C1200: MTU3 at
 * 0x000C1200, MTU4 at 0x000C1201, with shared MTU3/4 registers interspersed.
 * The struct fields (tcr, tmdr1, tiorh, tiorl, tier, tcnt, tgra, tgrb, tgrc,
 * tgrd, tsr, tbtm, tcr2, tgre) are placed at the correct byte offsets for
 * MTU3.  The gap bytes in the struct are reserved/MTU4/shared-register space.
 *
 * @warning Do NOT use this pointer for MTU4 register access.  MTU4 registers
 *          are at different offsets relative to the MTU4 base (0x000C1201).
 *          Access MTU4 registers using the individual address constants or
 *          by computing addresses manually from k_mtu4_base_addr.
 *
 * @return Volatile pointer to MTU3 register structure (base 0x000C1200)
 *
 * @see rx_mtu3_channel_regs_t for complete register layout with addresses
 * @see RX72N Hardware Manual Ch 5 I/O Register Address Table (MTU3a)
 */
static inline volatile rx_mtu3_channel_regs_t* mtu3(void)
{
  return (volatile rx_mtu3_channel_regs_t*)k_mtu3_base_addr;
}

/**
 * @brief Get raw base address pointer for MTU4 register access
 *
 * @details
 * Returns the MTU4 base address (0x000C1201) as a void pointer.  MTU4 registers
 * are at DIFFERENT relative offsets than MTU3 registers, so the rx_mtu3_channel_regs_t
 * struct CANNOT be used for MTU4.
 *
 * MTU4 register addresses from the RX72N Ch 5 I/O Address Table:
 * - MTU4.TCR   = 0x000C1201  (k_mtu4_base_addr + 0x00)
 * - MTU4.TMDR1 = 0x000C1203  (k_mtu4_base_addr + 0x02)
 * - MTU4.TIORH = 0x000C1206  (k_mtu4_base_addr + 0x05) -- NOTE: +0x05, not +0x04
 * - MTU4.TIORL = 0x000C1207  (k_mtu4_base_addr + 0x06)
 * - MTU4.TIER  = 0x000C1209  (k_mtu4_base_addr + 0x08)
 * - MTU4.TCNT  = 0x000C1212  (k_mtu4_base_addr + 0x11) -- NOTE: odd address
 * - MTU4.TGRA  = 0x000C121C  (k_mtu4_base_addr + 0x1B)
 * - MTU4.TGRB  = 0x000C121E  (k_mtu4_base_addr + 0x1D)
 * - MTU4.TGRC  = 0x000C1228  (k_mtu4_base_addr + 0x27)
 * - MTU4.TGRD  = 0x000C122A  (k_mtu4_base_addr + 0x29)
 * - MTU4.TSR   = 0x000C122D  (k_mtu4_base_addr + 0x2C)
 * - MTU4.TBTM  = 0x000C1239  (k_mtu4_base_addr + 0x38)
 * - MTU4.TCR2  = 0x000C124D  (k_mtu4_base_addr + 0x4C)
 * - MTU4.TGRE  = 0x000C1274  (k_mtu4_base_addr + 0x73)
 * - MTU4.TGRF  = 0x000C1276  (k_mtu4_base_addr + 0x75) -- MTU3 has no TGRF
 *
 * Access MTU4 registers by casting the returned pointer to the appropriate
 * volatile integer pointer and applying the correct offset, e.g.:
 *   *(volatile uint8_t*)((uintptr_t)mtu4() + 0x05U) = value;  // MTU4.TIORH
 *
 * @return Volatile void pointer to MTU4 base address (0x000C1201)
 *
 * @see RX72N Hardware Manual Ch 5 I/O Register Address Table (MTU3a)
 */
static inline volatile void* mtu4(void)
{
  return (volatile void*)k_mtu4_base_addr;
}

/**
 * @brief Get pointer to MTU6 registers (interleaved layout, identical to MTU3)
 *
 * @details
 * MTU6/MTU7 registers are interleaved at 0x000C1A00 in the same pattern as
 * MTU3/MTU4 at 0x000C1200. MTU6 registers are at even offsets; MTU7 registers
 * are at odd offsets. Use rx_mtu3_channel_regs_t to access MTU6 fields at
 * their correct byte offsets (TCR=+0x00, TMDR1=+0x02, TIORH=+0x04, etc.).
 *
 * @warning Do NOT cast this pointer to rx_mtu_channel_regs_t -- that struct
 *          assumes consecutive layout (TMDR at +0x01) which is wrong for MTU6.
 *
 * @return Volatile pointer to MTU6 register structure (interleaved layout)
 *
 * @see rx_mtu3_channel_regs_t for register layout and field offsets
 * @see mtu7() for raw MTU7 base pointer (void*)
 */
static inline volatile rx_mtu3_channel_regs_t* mtu6(void)
{
  return (volatile rx_mtu3_channel_regs_t*)k_mtu6_base_addr;
}

/**
 * @brief Get raw base address pointer for MTU7 register access
 *
 * @details
 * Returns the MTU7 base address (k_mtu6_base_addr + 1 = 0x000C1A01) as a
 * void pointer. MTU7 registers are at DIFFERENT relative offsets than MTU6
 * registers (same situation as MTU4 relative to MTU3), so no struct type
 * can be directly applied. Access MTU7 registers by computing addresses
 * manually from k_mtu7_base_addr using the Ch 5 I/O Address Table.
 *
 * @return Volatile void pointer to MTU7 base address (0x000C1A01)
 *
 * @see mtu6() for MTU6 access via rx_mtu3_channel_regs_t
 * @see RX72N Hardware Manual Ch 5 I/O Register Address Table (MTU3a)
 */
static inline volatile void* mtu7(void)
{
  return (volatile void*)k_mtu7_base_addr;
}

/**
 * @brief Get pointer to MTU TSTRA registers (MTU0-4, MTU8)
 * @return Volatile pointer to MTU TSTRA register structure
 */
static inline volatile rx_mtu_tstr_regs_t* mtu_tstra(void)
{
  return (volatile rx_mtu_tstr_regs_t*)k_mtu_tstra_base_addr;
}

/**
 * @brief Get pointer to MTU TSTRB registers (MTU6-7)
 * @return Volatile pointer to MTU TSTRB register structure
 */
static inline volatile rx_mtu_tstr_regs_t* mtu_tstrb(void)
{
  return (volatile rx_mtu_tstr_regs_t*)k_mtu_tstrb_base_addr;
}

/**
 * @enum mtu_tcr_bits_t
 * @brief Timer Control Register (TCR) bit field definitions
 *
 * @details
 * The TCR register controls timer clock source selection, counting edge,
 * and counter clear source. Each MTU channel has its own TCR register.
 *
 * @par Register Bit Layout
 * @verbatim
 *   Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
 *   -----------------------------------------------------------------
 *   |   CCLR[2:0]   |  CKEG[1:0]  |       TPSC[2:0]            |
 *   | Counter Clear | Clock Edge  | Timer Prescaler            |
 *   -----------------------------------------------------------------
 * @endverbatim
 *
 * @par Prescaler Values (TPSC)
 * | Value | Prescaler | Frequency (120 MHz PCLKA) |
 * |-------|-----------|---------------------------|
 * | 0x00  | PCLKA/1   | 120 MHz                   |
 * | 0x01  | PCLKA/4   | 30 MHz                    |
 * | 0x02  | PCLKA/16  | 7.5 MHz                   |
 * | 0x03  | PCLKA/64  | 1.875 MHz                 |
 *
 * @par Counter Clear Source (CCLR)
 * | Value | Source | Description |
 * |-------|--------|-------------|
 * | 0x01  | TGRA   | Clear on TGRA compare match |
 * | 0x02  | TGRB   | Clear on TGRB compare match |
 * | 0x03  | Sync   | Synchronous clear (MTU3/4)  |
 *
 * @see rx_mtu_channel_regs_t::tcr
 * @see RX72N Hardware Manual Section 24.2.1 (TCR Register)
 */
typedef enum : uint8_t {
  /** @brief Counter Clear Source bit position (bits 5-7) */
  k_mtu_tcr_cclr_shift = 5,

  /** @brief Timer Prescaler mask - bits 0-2 */
  k_mtu_tcr_tpsc_mask = 0x07,

  /** @brief Clock Edge mask - bits 3-4 */
  k_mtu_tcr_ckeg_mask = 0x18,

  /** @brief Counter Clear Source mask - bits 5-7 */
  k_mtu_tcr_cclr_mask = 0xE0,

  /** @brief Prescaler PCLKA/1 (120 MHz @ 120 MHz PCLKA) */
  k_mtu_tcr_tpsc_1 = 0x00,

  /** @brief Prescaler PCLKA/4 (30 MHz @ 120 MHz PCLKA) */
  k_mtu_tcr_tpsc_4 = 0x01,

  /** @brief Prescaler PCLKA/16 (7.5 MHz @ 120 MHz PCLKA) */
  k_mtu_tcr_tpsc_16 = 0x02,

  /** @brief Prescaler PCLKA/64 (1.875 MHz @ 120 MHz PCLKA) */
  k_mtu_tcr_tpsc_64 = 0x03,

  /** @brief Clear counter on TGRA compare match */
  k_mtu_tcr_cclr_tgra = (k_mtu_bit_value_set << k_mtu_tcr_cclr_shift),
} mtu_tcr_bits_t;

/**
 * @enum mtu_tmdr_bits_t
 * @brief Timer Mode Register (TMDR) bit field definitions
 *
 * @details
 * The TMDR register selects the timer operating mode: normal operation,
 * PWM generation, phase counting, or complementary PWM.
 *
 * @par Register Bit Layout
 * @verbatim
 *   Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
 *   -----------------------------------------------------------------
 *   |   -   |   -   |  BFB  |  BFA  |         MD[3:0]            |
 *   | Rsrvd | Rsrvd |BufferB|BufferA|      Mode Select           |
 *   -----------------------------------------------------------------
 * @endverbatim
 *
 * @par Operating Modes (MD[3:0])
 * | Value | Mode | Description |
 * |-------|------|-------------|
 * | 0x00  | Normal | Free-running with compare match |
 * | 0x02  | PWM 1 | TGRA=period, TGRB=duty |
 * | 0x03  | PWM 2 | TGRC/D as buffer registers |
 * | 0x04  | Phase 1 | Up/down on MTCLKA, dir from MTCLKB |
 * | 0x05  | Phase 2 | Up on MTCLKA, down on MTCLKB |
 * | 0x06  | Phase 3 | Count on MTCLKA edges only |
 * | 0x07  | Phase 4 | Count on MTCLKA and MTCLKB edges |
 * | 0x0D  | Compl PWM | Complementary PWM (MTU3/4, 6/7) |
 *
 * @par Buffer Transfer Mode
 * When BFA or BFB is set, buffer registers (TGRC->TGRA, TGRD->TGRB) are
 * automatically transferred on counter clear, enabling glitch-free
 * duty cycle updates during PWM operation.
 *
 * @see rx_mtu_channel_regs_t::tmdr
 * @see RX72N Hardware Manual Section 24.2.2 (TMDR Register)
 */
typedef enum : uint8_t {
  /** @brief Buffer mode A bit position */
  k_mtu_tmdr_bfa_shift = 4,

  /** @brief Buffer mode B bit position */
  k_mtu_tmdr_bfb_shift = 5,

  /** @brief Mode select mask (bits 0-3) */
  k_mtu_tmdr_md_mask = 0x0F,

  /** @brief Normal mode - free-running counter with compare match */
  k_mtu_tmdr_md_normal = 0x00,

  /** @brief PWM mode 1 - TGRA defines period, TGRB defines duty cycle */
  k_mtu_tmdr_md_pwm1 = 0x02,

  /** @brief PWM mode 2 - TGRC/TGRD as buffer registers for glitch-free updates */
  k_mtu_tmdr_md_pwm2 = 0x03,

  /** @brief Enable TGRC->TGRA buffer transfer on counter clear */
  k_mtu_tmdr_bfa = (k_mtu_bit_value_set << k_mtu_tmdr_bfa_shift),

  /** @brief Enable TGRD->TGRB buffer transfer on counter clear */
  k_mtu_tmdr_bfb = (k_mtu_bit_value_set << k_mtu_tmdr_bfb_shift),
} mtu_tmdr_bits_t;

/**
 * @brief Timer Mode Register 3 (TMDR3) bits for MTU1/MTU2 32-bit cascaded mode
 * @details
 * TMDR3 register controls 32-bit cascaded counter operation for MTU1/MTU2 in
 * phase counting mode. Available at offset 0x11 in rx_mtu12_phase_regs_t.
 *
 * Key features:
 * - LWA: Enables 32-bit cascaded counter (TCNTLW, TGRALW, TGRBLW)
 * - PHCKSEL: Selects phase counting clock edge (both edges vs single edge)
 *
 * @see RX72N Hardware Manual Section 24.2.25 (TMDR3 Register)
 */
typedef enum : uint8_t {
  k_mtu_tmdr3_lwa_shift     = 0, /**< Long Word Access bit position */
  k_mtu_tmdr3_phcksel_shift = 1, /**< Phase Counting Clock Select bit position */
  k_mtu_tmdr3_lwa =
    (k_mtu_bit_value_set << k_mtu_tmdr3_lwa_shift), /**< Enable 32-bit cascaded counter (LWA) */
  k_mtu_tmdr3_phcksel =
    (k_mtu_bit_value_set
     << k_mtu_tmdr3_phcksel_shift), /**< Phase counting clock select (both edges when set) */
} mtu_tmdr3_bits_t;

/**
 * @enum mtu_tior_bits_t
 * @brief Timer I/O Control Register (TIOR) bit field definitions
 *
 * @details
 * The TIOR register controls the behavior of timer output pins on compare
 * match events. TIORH controls TIOCA/TIOCB outputs, TIORL controls TIOCC/TIOCD.
 *
 * @par Register Bit Layout (TIORH)
 * @verbatim
 *   Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
 *   -----------------------------------------------------------------
 *   |       IOB[3:0]           |          IOA[3:0]              |
 *   |   TIOCB Output Mode      |      TIOCA Output Mode         |
 *   -----------------------------------------------------------------
 * @endverbatim
 *
 * @par Output Modes
 * | Value | Initial | On Match | PWM Application |
 * |-------|---------|----------|-----------------|
 * | 0x02  | Low     | High     | Active-high PWM |
 * | 0x05  | High    | Low      | Active-low PWM  |
 * | 0x03  | -       | Toggle   | 50% duty square |
 *
 * @par PWM Configuration Example
 * @code
 * // Configure TIOCA0 for active-high PWM output
 * mtu0()->tiorh = k_mtu_tior_init_low;  // Low initially, high on match
 * @endcode
 *
 * @see rx_mtu_channel_regs_t::tiorh
 * @see rx_mtu_channel_regs_t::tiorl
 * @see RX72N Hardware Manual Section 24.2.4 (TIOR Register)
 */
typedef enum : uint8_t {
  /** @brief I/O Control A mask - bits 0-3 (TIOCA output mode) */
  k_mtu_tior_ioa_mask = 0x0F,

  /** @brief I/O Control B mask - bits 4-7 (TIOCB output mode) */
  k_mtu_tior_iob_mask = 0xF0,

  /** @brief Initial output low, set high on compare match (active-high PWM) */
  k_mtu_tior_init_low = 0x02,

  /** @brief Initial output high, set low on compare match (active-low PWM) */
  k_mtu_tior_init_high = 0x05,

  /** @brief Toggle output on compare match (50% duty square wave) */
  k_mtu_tior_toggle = 0x03,
} mtu_tior_bits_t;

/**
 * @enum mtu_tstr_bits_t
 * @brief Timer Start Register A (TSTRA) bit definitions - controls MTU0-4, MTU8
 *
 * @details
 * TSTRA (at 0x000C1280) controls the start/stop state of MTU channels 0-4 and 8.
 * Setting a CSTn bit starts the corresponding timer; clearing it stops the timer.
 *
 * @par Register Bit Layout (TSTRA)
 * @verbatim
 *   Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
 *   -----------------------------------------------------------------
 *   | CST4  | CST3  |   -   |   -   | CST8  | CST2  | CST1  | CST0  |
 *   | MTU4  | MTU3  | Rsrvd | Rsrvd | MTU8  | MTU2  | MTU1  | MTU0  |
 *   -----------------------------------------------------------------
 * @endverbatim
 *
 * @par Synchronized Start Example
 * @code
 * // Start MTU1 and MTU2 simultaneously for 32-bit cascaded mode
 * mtu_tstra()->tstr |= (k_mtu_tstr_cst1 | k_mtu_tstr_cst2);
 *
 * // Stop all timers for configuration
 * mtu_tstra()->tstr = 0;
 * @endcode
 *
 * @see rx_mtu_tstr_regs_t
 * @see mtu_tstra() for register accessor
 * @see RX72N Hardware Manual Section 24.2.15 (TSTRA Register)
 */
typedef enum : uint8_t {
  /** @brief Counter Start 0 bit position (MTU0) */
  k_mtu_tstr_cst0_shift = 0,

  /** @brief Counter Start 1 bit position (MTU1) */
  k_mtu_tstr_cst1_shift = 1,

  /** @brief Counter Start 2 bit position (MTU2) */
  k_mtu_tstr_cst2_shift = 2,

  /** @brief Counter Start 8 bit position (MTU8) */
  k_mtu_tstr_cst8_shift = 3,

  /** @brief Counter Start 3 bit position (MTU3) */
  k_mtu_tstr_cst3_shift = 6,

  /** @brief Counter Start 4 bit position (MTU4) */
  k_mtu_tstr_cst4_shift = 7,

  /** @brief Start MTU0 counter (TSTRA bit 0) */
  k_mtu_tstr_cst0 = (k_mtu_bit_value_set << k_mtu_tstr_cst0_shift),

  /** @brief Start MTU1 counter (TSTRA bit 1) */
  k_mtu_tstr_cst1 = (k_mtu_bit_value_set << k_mtu_tstr_cst1_shift),

  /** @brief Start MTU2 counter (TSTRA bit 2) */
  k_mtu_tstr_cst2 = (k_mtu_bit_value_set << k_mtu_tstr_cst2_shift),

  /** @brief Start MTU3 counter (TSTRA bit 6) */
  k_mtu_tstr_cst3 = (k_mtu_bit_value_set << k_mtu_tstr_cst3_shift),

  /** @brief Start MTU4 counter (TSTRA bit 7) */
  k_mtu_tstr_cst4 = (k_mtu_bit_value_set << k_mtu_tstr_cst4_shift),

  /** @brief Start MTU8 counter (TSTRA bit 3) */
  k_mtu_tstr_cst8 = (k_mtu_bit_value_set << k_mtu_tstr_cst8_shift),
} mtu_tstr_bits_t;

/**
 * @enum mtu_tstrb_bits_t
 * @brief Timer Start Register B (TSTRB) bit definitions - controls MTU6-7
 *
 * @details
 * TSTRB (at 0x000C1A80) controls the start/stop state of MTU channels 6 and 7.
 * These channels are typically used together for complementary PWM generation.
 *
 * @par Register Bit Layout (TSTRB)
 * @verbatim
 *   Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
 *   -----------------------------------------------------------------
 *   | CST7  | CST6  |   -   |   -   |   -   |   -   |   -   |   -   |
 *   | MTU7  | MTU6  |              Reserved                        |
 *   -----------------------------------------------------------------
 * @endverbatim
 *
 * @see rx_mtu_tstr_regs_t
 * @see mtu_tstrb() for register accessor
 * @see RX72N Hardware Manual Section 24.2.16 (TSTRB Register)
 */
typedef enum : uint8_t {
  /** @brief Counter Start 6 bit position (MTU6) */
  k_mtu_tstr_cst6_shift = 6,

  /** @brief Counter Start 7 bit position (MTU7) */
  k_mtu_tstr_cst7_shift = 7,

  /** @brief Start MTU6 counter (TSTRB bit 6) */
  k_mtu_tstr_cst6 = (k_mtu_bit_value_set << k_mtu_tstr_cst6_shift),

  /** @brief Start MTU7 counter (TSTRB bit 7) */
  k_mtu_tstr_cst7 = (k_mtu_bit_value_set << k_mtu_tstr_cst7_shift),
} mtu_tstrb_bits_t;

/**
 * @enum mtu_channel_reg_offsets_t
 * @brief Byte offsets of registers within rx_mtu_channel_regs_t
 *
 * @details
 * Named constants used in compile-time layout verification.
 * Applies to standard MTU channels (MTU0, MTU1, MTU2, MTU6, MTU7).
 * Verified against RX72N Hardware Manual Ch 5 I/O Register Address Table.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief TCR at offset 0x00 (first register in channel block) */
  k_mtu_ch_off_tcr = 0x00U,
  /** @brief TCNT at offset 0x06 */
  k_mtu_ch_off_tcnt = 0x06U,
  /** @brief TGRA at offset 0x08 */
  k_mtu_ch_off_tgra = 0x08U,
  /** @brief TGRB at offset 0x0A */
  k_mtu_ch_off_tgrb = 0x0AU,
} mtu_channel_reg_offsets_t;

/**
 * @enum mtu3_reg_offsets_t
 * @brief Byte offsets of MTU3 registers from base 0x000C1200
 *
 * @details
 * Named constants for compile-time layout verification of rx_mtu3_channel_regs_t.
 * The MTU3/MTU4 interleaved layout means offsets are sparse -- see the struct
 * definition for the full picture of which bytes belong to MTU4 vs. shared regs.
 * All values verified against RX72N Hardware Manual Ch 5 I/O Register Address Table.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief TCR at offset 0x00 = 0x000C1200 */
  k_mtu3_off_tcr = 0x00U,
  /** @brief TMDR1 at offset 0x02 = 0x000C1202 */
  k_mtu3_off_tmdr1 = 0x02U,
  /** @brief TIORH at offset 0x04 = 0x000C1204 */
  k_mtu3_off_tiorh = 0x04U,
  /** @brief TIORL at offset 0x05 = 0x000C1205 */
  k_mtu3_off_tiorl = 0x05U,
  /** @brief TIER at offset 0x08 = 0x000C1208 */
  k_mtu3_off_tier = 0x08U,
  /** @brief TCNT at offset 0x10 = 0x000C1210 */
  k_mtu3_off_tcnt = 0x10U,
  /** @brief TGRA at offset 0x18 = 0x000C1218 */
  k_mtu3_off_tgra = 0x18U,
  /** @brief TGRB at offset 0x1A = 0x000C121A */
  k_mtu3_off_tgrb = 0x1AU,
  /** @brief TGRC at offset 0x24 = 0x000C1224 */
  k_mtu3_off_tgrc = 0x24U,
  /** @brief TGRD at offset 0x26 = 0x000C1226 */
  k_mtu3_off_tgrd = 0x26U,
  /** @brief TSR at offset 0x2C = 0x000C122C */
  k_mtu3_off_tsr = 0x2CU,
  /** @brief TBTM at offset 0x38 = 0x000C1238 */
  k_mtu3_off_tbtm = 0x38U,
  /** @brief TCR2 at offset 0x4C = 0x000C124C */
  k_mtu3_off_tcr2 = 0x4CU,
  /** @brief TGRE at offset 0x72 = 0x000C1272 */
  k_mtu3_off_tgre = 0x72U,
  /** @brief Total size of rx_mtu3_channel_regs_t = 0x74 bytes (116) */
  k_mtu3_struct_size = 0x74U,
} mtu3_reg_offsets_t;

/**
 * @enum mtu12_phase_reg_offsets_t
 * @brief Byte offsets of registers within rx_mtu12_phase_regs_t
 *
 * @details
 * Named constants for compile-time layout verification of the 32-bit phase
 * counting register struct.  Verified against RX72N Hardware Manual
 * Ch 5 I/O Register Address Table and Ch 24 Table 24.11.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief TCR at offset 0x00 */
  k_mtu12p_off_tcr = 0x00U,
  /** @brief TMDR at offset 0x01 */
  k_mtu12p_off_tmdr = 0x01U,
  /** @brief TCNT at offset 0x06 */
  k_mtu12p_off_tcnt = 0x06U,
  /** @brief TGRA at offset 0x08 */
  k_mtu12p_off_tgra = 0x08U,
  /** @brief TGRB at offset 0x0A */
  k_mtu12p_off_tgrb = 0x0AU,
  /** @brief TMDR3 at offset 0x11 (32-bit mode enable register) */
  k_mtu12p_off_tmdr3 = 0x11U,
  /** @brief TCNTLW at offset 0x20 (32-bit counter in long-word mode) */
  k_mtu12p_off_tcntlw = 0x20U,
  /** @brief TGRALW at offset 0x24 (32-bit compare A in long-word mode) */
  k_mtu12p_off_tgralw = 0x24U,
  /** @brief TGRBLW at offset 0x28 (32-bit compare B in long-word mode) */
  k_mtu12p_off_tgrblw = 0x28U,
  /** @brief Total size of rx_mtu12_phase_regs_t = 0x2C bytes (44) */
  k_mtu12p_struct_size = 0x2CU,
} mtu12_phase_reg_offsets_t;

/**
 * @enum mtu_periph_space_t
 * @brief Address space mask and expected base for MTU peripheral verification
 *
 * @details
 * Used in static_assert to verify MTU base addresses fall in the correct
 * peripheral address space (0x000Cxxxx), as required by the RX72N memory map.
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief Mask selecting the upper 16 bits of a 32-bit address */
  k_mtu_periph_space_mask = 0xFFFF0000U,
  /** @brief Expected upper 16 bits of all MTU base addresses */
  k_mtu_periph_space_base = 0x000C0000U,
} mtu_periph_space_t;

/* =============================================================================
 * Static Assertions - Compile-time Register Layout Verification
 * =============================================================================
 *
 * These static assertions verify that the C struct layouts exactly match
 * the hardware register addresses defined in the RX72N User's Manual.
 * Any mismatch will cause a compile-time error, preventing runtime bugs.
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 5 I/O Register
 *            Address Table (MTU3a section), and Chapter 24 (MTU3a).
 *
 * All MTU3 offsets verified against Ch 5 I/O Register Address Table:
 *   TCR=0x000C1200, TMDR1=0x000C1202, TIORH=0x000C1204, TIORL=0x000C1205,
 *   TIER=0x000C1208, TCNT=0x000C1210, TGRA=0x000C1218, TGRB=0x000C121A,
 *   TGRC=0x000C1224, TGRD=0x000C1226, TSR=0x000C122C, TBTM=0x000C1238,
 *   TCR2=0x000C124C, TGRE=0x000C1272
 */

/** @name MTU Channel Register Offset Verification
 *  @brief Verify rx_mtu_channel_regs_t matches hardware layout
 *  @{
 */
static_assert(offsetof(rx_mtu_channel_regs_t, tcr) == k_mtu_ch_off_tcr,
              "MTU TCR register offset incorrect");
static_assert(offsetof(rx_mtu_channel_regs_t, tcnt) == k_mtu_ch_off_tcnt,
              "MTU TCNT register offset incorrect");
static_assert(offsetof(rx_mtu_channel_regs_t, tgra) == k_mtu_ch_off_tgra,
              "MTU TGRA register offset incorrect");
static_assert(offsetof(rx_mtu_channel_regs_t, tgrb) == k_mtu_ch_off_tgrb,
              "MTU TGRB register offset incorrect");

/* Verify MTU3 register offsets match Ch 5 I/O Register Address Table.
 * These offsets reflect the actual sparse/interleaved hardware layout, not
 * the previous wrong consecutive-byte layout.
 * (MTU3 base = 0x000C1200; each offset added to base gives the hardware addr) */
static_assert(offsetof(rx_mtu3_channel_regs_t, tcr) == k_mtu3_off_tcr,
              "MTU3 TCR register offset incorrect -- expected 0x000C1200");
static_assert(offsetof(rx_mtu3_channel_regs_t, tmdr1) == k_mtu3_off_tmdr1,
              "MTU3 TMDR1 register offset incorrect -- expected 0x000C1202");
static_assert(offsetof(rx_mtu3_channel_regs_t, tiorh) == k_mtu3_off_tiorh,
              "MTU3 TIORH register offset incorrect -- expected 0x000C1204");
static_assert(offsetof(rx_mtu3_channel_regs_t, tiorl) == k_mtu3_off_tiorl,
              "MTU3 TIORL register offset incorrect -- expected 0x000C1205");
static_assert(offsetof(rx_mtu3_channel_regs_t, tier) == k_mtu3_off_tier,
              "MTU3 TIER register offset incorrect -- expected 0x000C1208");
static_assert(offsetof(rx_mtu3_channel_regs_t, tcnt) == k_mtu3_off_tcnt,
              "MTU3 TCNT register offset incorrect -- expected 0x000C1210");
static_assert(offsetof(rx_mtu3_channel_regs_t, tgra) == k_mtu3_off_tgra,
              "MTU3 TGRA register offset incorrect -- expected 0x000C1218");
static_assert(offsetof(rx_mtu3_channel_regs_t, tgrb) == k_mtu3_off_tgrb,
              "MTU3 TGRB register offset incorrect -- expected 0x000C121A");
static_assert(offsetof(rx_mtu3_channel_regs_t, tgrc) == k_mtu3_off_tgrc,
              "MTU3 TGRC register offset incorrect -- expected 0x000C1224");
static_assert(offsetof(rx_mtu3_channel_regs_t, tgrd) == k_mtu3_off_tgrd,
              "MTU3 TGRD register offset incorrect -- expected 0x000C1226");
static_assert(offsetof(rx_mtu3_channel_regs_t, tsr) == k_mtu3_off_tsr,
              "MTU3 TSR register offset incorrect -- expected 0x000C122C");
static_assert(offsetof(rx_mtu3_channel_regs_t, tbtm) == k_mtu3_off_tbtm,
              "MTU3 TBTM register offset incorrect -- expected 0x000C1238");
static_assert(offsetof(rx_mtu3_channel_regs_t, tcr2) == k_mtu3_off_tcr2,
              "MTU3 TCR2 register offset incorrect -- expected 0x000C124C");
static_assert(offsetof(rx_mtu3_channel_regs_t, tgre) == k_mtu3_off_tgre,
              "MTU3 TGRE register offset incorrect -- expected 0x000C1272");
static_assert(sizeof(rx_mtu3_channel_regs_t) == k_mtu3_struct_size,
              "MTU3 register structure size incorrect (expected 0x74 = 116 bytes)");

/* Verify MTU1/2 32-bit phase counting register offsets (Ch24 Table 24.11) */
static_assert(offsetof(rx_mtu12_phase_regs_t, tcr) == k_mtu12p_off_tcr,
              "MTU1/2 Phase TCR register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tmdr) == k_mtu12p_off_tmdr,
              "MTU1/2 Phase TMDR register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tcnt) == k_mtu12p_off_tcnt,
              "MTU1/2 Phase TCNT register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgra) == k_mtu12p_off_tgra,
              "MTU1/2 Phase TGRA register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgrb) == k_mtu12p_off_tgrb,
              "MTU1/2 Phase TGRB register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tmdr3) == k_mtu12p_off_tmdr3,
              "MTU1/2 Phase TMDR3 register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tcntlw) == k_mtu12p_off_tcntlw,
              "MTU1/2 Phase TCNTLW register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgralw) == k_mtu12p_off_tgralw,
              "MTU1/2 Phase TGRALW register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgrblw) == k_mtu12p_off_tgrblw,
              "MTU1/2 Phase TGRBLW register offset incorrect");
static_assert(sizeof(rx_mtu12_phase_regs_t) == k_mtu12p_struct_size,
              "MTU1/2 Phase register structure size incorrect (expected 44 bytes)");

/* Verify TSTR register structure */
static_assert(sizeof(rx_mtu_tstr_regs_t) == 1, "MTU TSTR register structure size mismatch");

/* Verify base addresses are in correct peripheral space (0x000C1xxx) */
static_assert((k_mtu0_base_addr & k_mtu_periph_space_mask) == k_mtu_periph_space_base,
              "MTU0 base address not in MTU peripheral space");
static_assert((k_mtu3_base_addr & k_mtu_periph_space_mask) == k_mtu_periph_space_base,
              "MTU3 base address not in MTU peripheral space");
static_assert((k_mtu6_base_addr & k_mtu_periph_space_mask) == k_mtu_periph_space_base,
              "MTU6 base address not in MTU peripheral space");

/* Verify MTU3 and MTU4 are interleaved (MTU4 = MTU3 + 1 byte) */
static_assert(k_mtu4_base_addr == (k_mtu3_base_addr + 1U),
              "MTU3 and MTU4 must be interleaved (MTU4 = MTU3 + 1 byte)");

/* Verify MTU6 and MTU7 are interleaved (MTU7 = MTU6 + 1 byte) */
static_assert(k_mtu7_base_addr == (k_mtu6_base_addr + 1U),
              "MTU6 and MTU7 must be interleaved (MTU7 = MTU6 + 1 byte)");
/** @} */ /* End of static assertion group */

/** @} */ /* End of mtu_regs defgroup */

#ifdef __cplusplus
}
#endif
