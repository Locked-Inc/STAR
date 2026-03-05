/* lib/rx_hal/inc/rx72n_mtu_regs.h */

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
 * @author STAR Project Contributors
 * @date 2026-01-01
 * @version 1.0.0
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
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
   * @brief MTU7 base address (0x000C1A00) - shares base with MTU6
   * @details MTU7 registers are interleaved with MTU6 registers at this address.
   */
  k_mtu7_base_addr = 0x000C1A00,

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
 * @brief Standard MTU channel register map (MTU0-MTU2, MTU6-MTU7)
 *
 * @details
 * Memory-mapped register structure for standard 16-bit MTU timer channels.
 * This structure applies to MTU0, MTU1, MTU2, MTU6, and MTU7 when operating
 * in standard (non-extended) modes.
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
 * @struct rx_mtu34_channel_regs_t
 * @brief MTU3/MTU4 extended channel register map with complementary PWM support
 *
 * @details
 * Extended register structure for MTU3 and MTU4 channels which support
 * complementary PWM generation for H-bridge motor driver control. These
 * channels include additional TGR E/F registers and buffer transfer mode.
 *
 * @par Complementary PWM for Motor Control
 * MTU3 and MTU4 are designed as a paired timer unit for generating
 * complementary PWM signals with programmable dead-time:
 *
 * @verbatim
 *   MTU3 TIOC3A ----+    +-------------------------+
 *                   +--->| High-Side Gate (DRV8263H)|---> Motor +
 *   MTU4 TIOC4A ----+    +-------------------------+
 *                                ^v Dead-time
 *   MTU3 TIOC3B ----+    +-------------------------+
 *                   +--->| Low-Side Gate (DRV8263H) |---> Motor -
 *   MTU4 TIOC4B ----+    +-------------------------+
 * @endverbatim
 *
 * @par Register Memory Layout (21 bytes total)
 * @verbatim
 *   Offset  Size  Register  Description
 *   --------------------------------------------------------------
 *   0x00    1     TCR       Timer Control (prescaler, edge, clear)
 *   0x01    1     TMDR      Timer Mode (complementary PWM mode)
 *   0x02    1     TIORH     Timer I/O Control H (output A/B)
 *   0x03    1     TIORL     Timer I/O Control L (output C/D)
 *   0x04    1     TIER      Timer Interrupt Enable
 *   0x05    1     TSR       Timer Status (flags)
 *   0x06    2     TCNT      Timer Counter (16-bit)
 *   0x08    2     TGRA      Timer General Register A
 *   0x0A    2     TGRB      Timer General Register B
 *   0x0C    2     TGRC      Timer General Register C
 *   0x0E    2     TGRD      Timer General Register D
 *   0x10    2     TGRE      Timer General Register E (extended)
 *   0x12    2     TGRF      Timer General Register F (extended)
 *   0x14    1     TIER2     Timer Interrupt Enable 2
 *   0x15    1     TSR2      Timer Status 2
 *   0x16    1     TBTM      Timer Buffer Transfer Mode
 * @endverbatim
 *
 * @par Dead-Time Insertion
 * TGRE and TGRF provide dead-time values to prevent H-bridge shoot-through:
 * @f[
 * t_{dead} = \frac{\text{TGRE}}{f_{PCLKA}} = \frac{\text{TGRE}}{120\text{ MHz}}
 * @f]
 *
 * Example: 500ns dead-time requires TGRE = 60 (at 120 MHz PCLKA)
 *
 * @par Complementary PWM Example
 * @code
 * // Configure MTU3/4 for complementary PWM with 500ns dead-time
 * volatile rx_mtu34_channel_regs_t* mtu = mtu3();
 *
 * // Stop both timers
 * mtu_tstra()->tstr &= ~(k_mtu_tstr_cst3 | k_mtu_tstr_cst4);
 *
 * // Configure for complementary PWM mode
 * mtu->tmdr = 0x0D;  // Complementary PWM mode
 *
 * // Set period (TGRA) and duty cycle (TGRB)
 * mtu->tgra = 5999;  // 20 kHz period
 * mtu->tgrb = 3000;  // 50% duty cycle
 *
 * // Set dead-time (TGRE for rising edge, TGRF for falling)
 * mtu->tgre = 60;    // 500ns at 120 MHz
 * mtu->tgrf = 60;    // 500ns at 120 MHz
 *
 * // Enable buffer transfer on counter clear
 * mtu->tbtm = 0x03;
 *
 * // Start both timers synchronously
 * mtu_tstra()->tstr |= (k_mtu_tstr_cst3 | k_mtu_tstr_cst4);
 * @endcode
 *
 * @invariant Structure size must be exactly 23 bytes (0x17)
 * @invariant MTU3/MTU4 registers are interleaved at base address 0x000C1200
 *
 * @see rx_mtu_channel_regs_t for standard channel registers
 * @see RX72N Hardware Manual Section 24.2.7 (MTU3/MTU4 Registers)
 * @see RX72N Hardware Manual Section 24.3.7 (Complementary PWM Mode)
 */
typedef struct {
  /**
   * @brief Timer Control Register (TCR) - prescaler and clear source
   * @see mtu_tcr_bits_t for bit field definitions
   */
  volatile uint8_t tcr;

  /**
   * @brief Timer Mode Register (TMDR) - operating mode
   * @details Includes complementary PWM mode (0x0D) for MTU3/4.
   * @see mtu_tmdr_bits_t for mode values
   */
  volatile uint8_t tmdr;

  /**
   * @brief Timer I/O Control Register H - TIOCA/TIOCB output control
   * @see mtu_tior_bits_t for output mode values
   */
  volatile uint8_t tiorh;

  /**
   * @brief Timer I/O Control Register L - TIOCC/TIOCD output control
   * @see mtu_tior_bits_t for output mode values
   */
  volatile uint8_t tiorl;

  /**
   * @brief Timer Interrupt Enable Register (TIER)
   * @details Enables TGIA-D compare match and overflow interrupts.
   */
  volatile uint8_t tier;

  /**
   * @brief Timer Status Register (TSR) - interrupt flags
   * @details Read to check flags, write 0 to clear.
   */
  volatile uint8_t tsr;

  /**
   * @brief Timer Counter (TCNT) - 16-bit counter value
   * @details Shared between MTU3 and MTU4 for synchronized operation.
   */
  volatile uint16_t tcnt;

  /** @brief Timer General Register A - period (complementary PWM) */
  volatile uint16_t tgra;

  /** @brief Timer General Register B - duty cycle (complementary PWM) */
  volatile uint16_t tgrb;

  /** @brief Timer General Register C - buffer for TGRA */
  volatile uint16_t tgrc;

  /** @brief Timer General Register D - buffer for TGRB */
  volatile uint16_t tgrd;

  /**
   * @brief Timer General Register E - dead-time (rising edge)
   * @details MTU3/4 only. Delay before high-side turns on.
   */
  volatile uint16_t tgre;

  /**
   * @brief Timer General Register F - dead-time (falling edge)
   * @details MTU3/4 only. Delay before low-side turns on.
   */
  volatile uint16_t tgrf;

  /**
   * @brief Timer Interrupt Enable Register 2 (TIER2)
   * @details Enables TGIE and TGIF compare match interrupts.
   */
  volatile uint8_t tier2;

  /**
   * @brief Timer Status Register 2 (TSR2)
   * @details Contains TGIE and TGIF interrupt flags.
   */
  volatile uint8_t tsr2;

  /**
   * @brief Timer Buffer Transfer Mode Register (TBTM)
   * @details Controls automatic buffer transfer from TGRC->TGRA, TGRD->TGRB.
   */
  volatile uint8_t tbtm;
} rx_mtu34_channel_regs_t;

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

  /** @brief Reserved bytes at offset 0x12-0x1F (14 bytes) */
  uint8_t reserved2[14];

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
 * @brief Get pointer to MTU3 registers
 *
 * @details MTU3 and MTU4 share the same base address (0x000C1200) by hardware design.
 * This is intentional per the RX72N hardware manual - MTU3 and MTU4 are a "paired"
 * timer unit designed for complementary PWM generation.
 *
 * The MTU3/MTU4 register layout within the shared base address:
 * - MTU3 registers: Offsets 0x00-0x16 (TCR, TMDR, TIORH, TIORL, TIER, TSR, TCNT, TGRx, etc.)
 * - MTU4 registers: Offsets 0x01-0x17 (interleaved with MTU3)
 *
 * Both mtu3() and mtu4() return the same base pointer because the rx_mtu34_channel_regs_t
 * structure represents the combined MTU3/MTU4 register block. The caller accesses
 * MTU3-specific or MTU4-specific registers through the same structure.
 *
 * The static assertions at lines 283-284 verify this shared base address is intentional.
 *
 * @return Volatile pointer to MTU3/MTU4 register structure
 *
 * @see RX72N Hardware Manual Section 22.2.7 for MTU3/MTU4 register map
 */
static inline volatile rx_mtu34_channel_regs_t* mtu3(void)
{
  return (volatile rx_mtu34_channel_regs_t*)k_mtu3_base_addr;
}

/**
 * @brief Get pointer to MTU4 registers
 *
 * @details MTU3 and MTU4 share the same base address (0x000C1200) by hardware design.
 * This is intentional per the RX72N hardware manual - MTU3 and MTU4 are a "paired"
 * timer unit designed for complementary PWM generation.
 *
 * Both mtu3() and mtu4() return the same base pointer because the rx_mtu34_channel_regs_t
 * structure represents the combined MTU3/MTU4 register block. Within this block:
 * - MTU3 registers are at even offsets (TCR3 at 0x00, TMDR3 at 0x02, etc.)
 * - MTU4 registers are at odd offsets (TCR4 at 0x01, TMDR4 at 0x03, etc.)
 *
 * The static assertions at lines 283-284 verify this shared base address is intentional.
 *
 * @note For most applications, use mtu3() for both MTU3 and MTU4 access since they
 * share the register structure. The mtu4() function is provided for API symmetry.
 *
 * @return Volatile pointer to MTU3/MTU4 register structure (same as mtu3())
 *
 * @see RX72N Hardware Manual Section 22.2.7 for MTU3/MTU4 register map
 */
static inline volatile rx_mtu34_channel_regs_t* mtu4(void)
{
  return (volatile rx_mtu34_channel_regs_t*)k_mtu4_base_addr;
}

/**
 * @brief Get pointer to MTU6 registers
 * @return Volatile pointer to MTU6 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu6(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu6_base_addr;
}

/**
 * @brief Get pointer to MTU7 registers
 * @return Volatile pointer to MTU7 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu7(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu7_base_addr;
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

/* =============================================================================
 * Static Assertions - Compile-time Register Layout Verification
 * =============================================================================
 *
 * These static assertions verify that the C struct layouts exactly match
 * the hardware register addresses defined in the RX72N User's Manual.
 * Any mismatch will cause a compile-time error, preventing runtime bugs.
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 24 (MTU3a)
 *            Section 24.2: Register Descriptions, Table 24.11
 */

/** @name MTU Channel Register Offset Verification
 *  @brief Verify rx_mtu_channel_regs_t matches hardware layout
 *  @{
 */
static_assert(offsetof(rx_mtu_channel_regs_t, tcr) == 0x00, "MTU TCR register offset incorrect");
static_assert(offsetof(rx_mtu_channel_regs_t, tcnt) == 0x06, "MTU TCNT register offset incorrect");
static_assert(offsetof(rx_mtu_channel_regs_t, tgra) == 0x08, "MTU TGRA register offset incorrect");
static_assert(offsetof(rx_mtu_channel_regs_t, tgrb) == 0x0A, "MTU TGRB register offset incorrect");

/* Verify MTU3/4 extended register critical offsets */
static_assert(offsetof(rx_mtu34_channel_regs_t, tcr) == 0x00,
              "MTU3/4 TCR register offset incorrect");
static_assert(offsetof(rx_mtu34_channel_regs_t, tcnt) == 0x06,
              "MTU3/4 TCNT register offset incorrect");
static_assert(offsetof(rx_mtu34_channel_regs_t, tgra) == 0x08,
              "MTU3/4 TGRA register offset incorrect");
static_assert(offsetof(rx_mtu34_channel_regs_t, tgre) == 0x10,
              "MTU3/4 TGRE register offset incorrect");
static_assert(offsetof(rx_mtu34_channel_regs_t, tgrf) == 0x12,
              "MTU3/4 TGRF register offset incorrect");

/* Verify MTU1/2 32-bit phase counting register offsets (Ch24 Table 24.11) */
static_assert(offsetof(rx_mtu12_phase_regs_t, tcr) == 0x00,
              "MTU1/2 Phase TCR register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tmdr) == 0x01,
              "MTU1/2 Phase TMDR register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tcnt) == 0x06,
              "MTU1/2 Phase TCNT register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgra) == 0x08,
              "MTU1/2 Phase TGRA register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgrb) == 0x0A,
              "MTU1/2 Phase TGRB register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tmdr3) == 0x11,
              "MTU1/2 Phase TMDR3 register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tcntlw) == 0x20,
              "MTU1/2 Phase TCNTLW register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgralw) == 0x24,
              "MTU1/2 Phase TGRALW register offset incorrect");
static_assert(offsetof(rx_mtu12_phase_regs_t, tgrblw) == 0x28,
              "MTU1/2 Phase TGRBLW register offset incorrect");
static_assert(sizeof(rx_mtu12_phase_regs_t) == 0x2C,
              "MTU1/2 Phase register structure size incorrect (expected 44 bytes)");

/* Verify TSTR register structure */
static_assert(sizeof(rx_mtu_tstr_regs_t) == 1, "MTU TSTR register structure size mismatch");

/* Verify base addresses are in correct peripheral space (0x000C1xxx) */
static_assert((k_mtu0_base_addr & 0xFFFF0000) == 0x000C0000,
              "MTU0 base address not in MTU peripheral space");
static_assert((k_mtu3_base_addr & 0xFFFF0000) == 0x000C0000,
              "MTU3 base address not in MTU peripheral space");
static_assert((k_mtu6_base_addr & 0xFFFF0000) == 0x000C0000,
              "MTU6 base address not in MTU peripheral space");

/* Verify MTU3 and MTU4 are interleaved (MTU4 = MTU3 + 1 byte) */
static_assert(k_mtu4_base_addr == (k_mtu3_base_addr + 1U),
              "MTU3 and MTU4 must be interleaved (MTU4 = MTU3 + 1 byte)");

/* Verify MTU6 and MTU7 share the same base address */
static_assert(k_mtu6_base_addr == k_mtu7_base_addr,
              "MTU6 and MTU7 must share base address (adjacent registers)");
/** @} */ /* End of static assertion group */

/** @} */ /* End of mtu_regs defgroup */

#ifdef __cplusplus
}
#endif
