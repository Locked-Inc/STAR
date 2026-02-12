/* lib/rx_hal/inc/rx72n_port_regs.h */

/**
 * @file rx72n_port_regs.h
 * @brief RX72N PORT (GPIO) register definitions for digital pin control
 *
 * @details
 * This file provides complete hardware register definitions for the RX72N's
 * General Purpose I/O (GPIO) port peripheral. The PORT module controls
 * digital pin direction, output state, input reading, and special modes
 * like open-drain and pull-up resistors.
 *
 * @par Critical Architecture Note
 * @warning RX72N PORT registers are organized by REGISTER TYPE, not by PORT!
 *
 * @verbatim
 *   Traditional MCU Layout          RX72N Layout (Type-Grouped)
 *   ──────────────────────          ──────────────────────────────
 *   PORT0: PDR0, PODR0, ...         PDR:  PDR0, PDR1, PDR2, ... (contiguous)
 *   PORT1: PDR1, PODR1, ...         PODR: PODR0, PODR1, PODR2, ... (contiguous)
 *   PORT2: PDR2, PODR2, ...         PIDR: PIDR0, PIDR1, PIDR2, ... (contiguous)
 *   ...                             ...
 * @endverbatim
 *
 * This struct provides a PORT-centric view with correct padding to handle
 * the type-grouped hardware layout transparently.
 *
 * @par System Architecture - STAR Robot GPIO Usage
 * @verbatim
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                    RX72N GPIO Architecture (144-pin LFQFP)              │
 *   │                                                                         │
 *   │  ┌─────────────────────────────────────────────────────────────────────┐│
 *   │  │                        PORT Module                                  ││
 *   │  │                                                                     ││
 *   │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  ││
 *   │  │  │ Port 0   │ │ Port 1   │ │ Port 2   │ │ Port 3   │ │ Port 4   │  ││
 *   │  │  │ 6 pins   │ │ P12-P17  │ │ P20-P27  │ │ P30-P37  │ │ P40-P47  │  ││
 *   │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘  ││
 *   │  │                                                                     ││
 *   │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  ││
 *   │  │  │ Port 5   │ │ Port 6   │ │ Port 7   │ │ Port 8   │ │ Port 9   │  ││
 *   │  │  │ P50-P56  │ │ P60-P67  │ │ P70-P77  │ │ 6 pins   │ │ P90-P93  │  ││
 *   │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘  ││
 *   │  │                                                                     ││
 *   │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  ││
 *   │  │  │ Port A   │ │ Port B   │ │ Port C   │ │ Port D   │ │ Port E   │  ││
 *   │  │  │ PA0-PA7  │ │ PB0-PB7  │ │ PC0-PC7  │ │ PD0-PD7  │ │ PE0-PE7  │  ││
 *   │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘  ││
 *   │  │                                                                     ││
 *   │  │  ┌──────────┐ ┌──────────┐  Unavailable on 144-pin:                 ││
 *   │  │  │ Port F   │ │ Port J   │  Ports G, H, K, L, M, N, Q               ││
 *   │  │  │ PF5 only │ │ PJ3,PJ5  │                                          ││
 *   │  │  └──────────┘ └──────────┘                                          ││
 *   │  └─────────────────────────────────────────────────────────────────────┘│
 *   │                                                                         │
 *   │  STAR Robot GPIO Assignments (from 03_hardware_pinout.tex):             │
 *   │  ┌────────────────────────────────────────────────────────────────────┐ │
 *   │  │ Motor Control:                                                     │ │
 *   │  │   - PWM outputs: TIOCA0-3, TIOCB0-3 (MTU channels)                 │ │
 *   │  │   - Enable pins: GPIO outputs to DRV8243                           │ │
 *   │  │   - Fault inputs: GPIO inputs from DRV8243                         │ │
 *   │  │                                                                     │ │
 *   │  │ Encoder Inputs:                                                    │ │
 *   │  │   - MTCLKA/B: Quadrature encoder A/B signals                       │ │
 *   │  │                                                                     │ │
 *   │  │ Communication:                                                     │ │
 *   │  │   - SPI (RSPI0): COPI, CIPO, SCK, CS                               │ │
 *   │  │   - UART (SCI): TXD, RXD                                           │ │
 *   │  │   - I2C (RIIC): SDA, SCL                                           │ │
 *   │  │   - USB: USB_DP, USB_DM                                            │ │
 *   │  │                                                                     │ │
 *   │  │ Status LEDs:                                                       │ │
 *   │  │   - System status, error indicators                                │ │
 *   │  └────────────────────────────────────────────────────────────────────┘ │
 *   └─────────────────────────────────────────────────────────────────────────┘
 * @endverbatim
 *
 * @par Register Memory Map
 * | Register | Base Address | Description |
 * |----------|--------------|-------------|
 * | PDR      | 0x0008C000   | Port Direction (0=input, 1=output) |
 * | PODR     | 0x0008C020   | Port Output Data (output level) |
 * | PIDR     | 0x0008C040   | Port Input Data (read pin state) |
 * | PMR      | 0x0008C060   | Port Mode (0=GPIO, 1=peripheral) |
 * | ODR0/1   | 0x0008C080   | Open Drain Control |
 * | PCR      | 0x0008C0C0   | Pull-up Control |
 * | DSCR     | 0x0008C0E0   | Drive Capacity Control |
 * | DSCR2    | 0x0008C128   | Drive Capacity Control 2 |
 *
 * @par GPIO Configuration Example
 * @code
 * // Configure PD0 as output (LED)
 * portd()->pmr &= ~(1 << 0);  // GPIO mode (not peripheral)
 * portd()->pdr |= (1 << 0);   // Output direction
 * portd()->podr |= (1 << 0);  // Set high (LED on)
 *
 * // Configure PA0 as input with pull-up (button)
 * porta()->pmr &= ~(1 << 0);  // GPIO mode
 * porta()->pdr &= ~(1 << 0);  // Input direction
 * porta()->pcr |= (1 << 0);   // Enable pull-up
 *
 * // Read button state
 * bool pressed = (porta()->pidr & (1 << 0)) == 0;  // Active-low
 * @endcode
 *
 * @par Package Support
 * This file supports the 144-pin LFQFP package (R5F572NNHxFB).
 * Available ports and pins:
 * - Port 0: P00-P03, P05, P07 (6 pins)
 * - Port 1: P12-P17 (6 pins)
 * - Port 2: P20-P27 (full 8 pins)
 * - Port 3: P30-P37 (full 8 pins, P35 input-only)
 * - Port 4: P40-P47 (full 8 pins)
 * - Port 5: P50-P56 (7 pins)
 * - Port 6: P60-P67 (full 8 pins)
 * - Port 7: P70-P77 (full 8 pins)
 * - Port 8: P80-P83, P86-P87 (6 pins)
 * - Port 9: P90-P93 (4 pins)
 * - Port A: PA0-PA7 (full 8 pins)
 * - Port B: PB0-PB7 (full 8 pins)
 * - Port C: PC0-PC7 (full 8 pins)
 * - Port D: PD0-PD7 (full 8 pins)
 * - Port E: PE0-PE7 (full 8 pins)
 * - Port F: PF5 (1 pin)
 * - Port J: PJ3, PJ5 (2 pins)
 *
 * @par Hardware Reference
 * - RX72N Group User's Manual: Hardware, Chapter 21 (I/O Ports)
 * - Section 21.2: Register Descriptions
 * - Section 21.3: Pin Functions
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 3: [OK] Static allocation only (all definitions compile-time)
 * - Rule 5: [OK] Static assertions verify register layout at compile time
 * - Rule 8: [OK] C23 typed enums eliminate preprocessor constants
 * - Rule 10: [OK] Header compiles cleanly with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: GPIO register definitions only, no driver logic
 * - **Open/Closed**: Extend via new enums; don't modify existing values
 * - **Interface Segregation**: Single struct handles all port register types
 * - **Dependency Inversion**: Higher-level drivers depend on these abstractions
 *
 * @par Related Modules
 * - [rx72n_mpc.h](rx72n__mpc_8h.html): Multi-function Pin Controller (pin function select)
 * - [rx72n_mtu_regs.h](rx72n__mtu__regs_8h.html): Timer I/O pins
 * - [rx72n_sci_regs.h](rx72n__sci__regs_8h.html): UART pins
 * - [rx72n_rspi_regs.h](rx72n__rspi__regs_8h.html): SPI pins
 *
 * @see RX72N Hardware Manual Chapter 21 for complete I/O Ports specification
 * @see DOXYGEN_ROADMAP.md for documentation standards
 *
 * @author STAR Project Contributors
 * @date 2026-01-05
 * @version 1.0.0
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 *
 * @defgroup port_regs PORT Register Definitions
 * @{
 */

#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Port Register Base Addresses
 * =============================================================================
 */

/**
 * @enum rx_port_reg_bases_t
 * @brief PORT register block base addresses (organized by register type)
 *
 * @details
 * The RX72N organizes port registers by type rather than by port number.
 * All PDR registers are contiguous, all PODR registers are contiguous, etc.
 * This enum provides base addresses for each register type block.
 *
 * @par Memory Map Overview
 * @verbatim
 *   Address Range        Register Type    Description
 *   ──────────────────────────────────────────────────────────────────
 *   0x0008C000-0x0008C017  PDR[]          Port Direction Registers
 *   0x0008C020-0x0008C037  PODR[]         Port Output Data Registers
 *   0x0008C040-0x0008C057  PIDR[]         Port Input Data Registers
 *   0x0008C060-0x0008C077  PMR[]          Port Mode Registers
 *   0x0008C080-0x0008C0B7  ODR0/ODR1[]    Open Drain Control (16-bit per port)
 *   0x0008C0C0-0x0008C0D7  PCR[]          Pull-up Control Registers
 *   0x0008C0E0-0x0008C0F7  DSCR[]         Drive Capacity Registers
 *   0x0008C128-0x0008C13F  DSCR2[]        Drive Capacity 2 Registers
 * @endverbatim
 *
 * @par Register Access Pattern
 * To access a specific port's register, add the port offset to the base:
 * - PORT0.PDR = k_port_pdr_base + k_port_offset_0 = 0x0008C000
 * - PORTB.PDR = k_port_pdr_base + k_port_offset_b = 0x0008C00B
 * - PORTD.PODR = k_port_podr_base + k_port_offset_d = 0x0008C02D
 *
 * @see rx_port_offsets_t for port offset values
 * @see RX72N Hardware Manual Section 21.2 (Register Descriptions)
 */
typedef enum : uint32_t {
  /**
   * @brief PDR (Port Direction Register) base address
   * @details All PDRn registers at 0x0008C000 + port_offset.
   * PDR bit = 0: Input, PDR bit = 1: Output.
   */
  k_port_pdr_base = 0x0008C000,

  /**
   * @brief PODR (Port Output Data Register) base address
   * @details All PODRn registers at 0x0008C020 + port_offset.
   * PODR bit = 0: Output low, PODR bit = 1: Output high.
   */
  k_port_podr_base = 0x0008C020,

  /**
   * @brief PIDR (Port Input Data Register) base address
   * @details All PIDRn registers at 0x0008C040 + port_offset.
   * Read-only. Returns current pin logic level.
   */
  k_port_pidr_base = 0x0008C040,

  /**
   * @brief PMR (Port Mode Register) base address
   * @details All PMRn registers at 0x0008C060 + port_offset.
   * PMR bit = 0: GPIO mode, PMR bit = 1: Peripheral function.
   */
  k_port_pmr_base = 0x0008C060,

  /**
   * @brief ODR0 (Open Drain Control 0) base address
   * @details ODR0/ODR1 form a 16-bit register per port at 0x0008C080 + 2*port_offset.
   * ODR bit = 0: CMOS output, ODR bit = 1: Open-drain output.
   * @note ODR1 immediately follows ODR0; accessed as 16-bit odr field in struct.
   */
  k_port_odr0_base = 0x0008C080,

  /**
   * @brief PCR (Pull-up Control Register) base address
   * @details All PCRn registers at 0x0008C0C0 + port_offset.
   * PCR bit = 0: Pull-up disabled, PCR bit = 1: Pull-up enabled.
   */
  k_port_pcr_base = 0x0008C0C0,

  /**
   * @brief DSCR (Drive Capacity Control Register) base address
   * @details All DSCRn registers at 0x0008C0E0 + port_offset.
   * DSCR bit = 0: Normal drive, DSCR bit = 1: High drive.
   */
  k_port_dscr_base = 0x0008C0E0,

  /**
   * @brief DSCR2 (Drive Capacity Control Register 2) base address
   * @details All DSCR2n registers at 0x0008C128 + port_offset.
   * DSCR2 bit = 0: Normal drive (same as DSCR).
   * DSCR2 bit = 1: Enhanced high drive capacity.
   * @note Not available on PORT4.
   */
  k_port_dscr2_base = 0x0008C128,
} rx_port_reg_bases_t;

/**
 * @enum rx_port_offsets_t
 * @brief Port number offsets for register access (144-pin LFQFP)
 *
 * @details
 * Offset values to add to register base addresses for accessing specific ports.
 * The 144-pin LFQFP package has broader port availability compared to the
 * 144-pin package. Ports G, H, K, L, M, N, Q are not bonded out.
 *
 * @par Available Pins by Port (144-pin LFQFP)
 * | Port | Offset | Available Pins | Notes |
 * |------|--------|----------------|-------|
 * | 0    | 0x00   | P00-P03, P05, P07 | 6 pins (no P04, P06) |
 * | 1    | 0x01   | P12-P17        | 6 pins (no P10, P11) |
 * | 2    | 0x02   | P20-P27        | Full 8 pins |
 * | 3    | 0x03   | P30-P37        | Full 8 pins, P35 input-only |
 * | 4    | 0x04   | P40-P47        | Full 8 pins |
 * | 5    | 0x05   | P50-P56        | 7 pins (no P57) |
 * | 6    | 0x06   | P60-P67        | Full 8 pins |
 * | 7    | 0x07   | P70-P77        | Full 8 pins |
 * | 8    | 0x08   | P80-P83, P86-P87 | 6 pins (no P84, P85) |
 * | 9    | 0x09   | P90-P93        | 4 pins (no P94-P97) |
 * | A    | 0x0A   | PA0-PA7        | Full 8 pins |
 * | B    | 0x0B   | PB0-PB7        | Full 8 pins |
 * | C    | 0x0C   | PC0-PC7        | Full 8 pins |
 * | D    | 0x0D   | PD0-PD7        | Full 8 pins |
 * | E    | 0x0E   | PE0-PE7        | Full 8 pins |
 * | F    | 0x0F   | PF5            | 1 pin only |
 * | J    | 0x12   | PJ3, PJ5       | 2 pins only |
 *
 * @par Usage Example
 * @code
 * // Calculate PORTB PDR address
 * uint32_t portb_pdr_addr = k_port_pdr_base + k_port_offset_b;  // 0x0008C00B
 *
 * // Calculate PORTD PODR address
 * uint32_t portd_podr_addr = k_port_podr_base + k_port_offset_d;  // 0x0008C02D
 * @endcode
 *
 * @see rx_port_reg_bases_t for register base addresses
 * @see RX72N Hardware Manual Chapter 21.1.1 (Package Pin Availability)
 */
typedef enum : uint8_t {
  /** @brief Port 0 offset - P00-P03, P05, P07 (6 pins) */
  k_port_offset_0 = 0x00,

  /** @brief Port 1 offset - P12-P17 (6 pins) */
  k_port_offset_1 = 0x01,

  /** @brief Port 2 offset - P20-P27 (8 pins) */
  k_port_offset_2 = 0x02,

  /** @brief Port 3 offset - P30-P37 (8 pins, P35 input-only) */
  k_port_offset_3 = 0x03,

  /** @brief Port 4 offset - P40-P47 (8 pins) */
  k_port_offset_4 = 0x04,

  /** @brief Port 5 offset - P50-P56 (7 pins) */
  k_port_offset_5 = 0x05,

  /** @brief Port 6 offset - P60-P67 (8 pins) */
  k_port_offset_6 = 0x06,

  /** @brief Port 7 offset - P70-P77 (8 pins) */
  k_port_offset_7 = 0x07,

  /** @brief Port 8 offset - P80-P83, P86-P87 (6 pins) */
  k_port_offset_8 = 0x08,

  /** @brief Port 9 offset - P90-P93 (4 pins) */
  k_port_offset_9 = 0x09,

  /** @brief Port A offset - PA0-PA7 (8 pins) */
  k_port_offset_a = 0x0A,

  /** @brief Port B offset - Full: PB0-PB7 (8 pins) */
  k_port_offset_b = 0x0B,

  /** @brief Port C offset - Full: PC0-PC7 (8 pins) */
  k_port_offset_c = 0x0C,

  /** @brief Port D offset - Full: PD0-PD7 (8 pins) */
  k_port_offset_d = 0x0D,

  /** @brief Port E offset - PE0-PE7 (8 pins) */
  k_port_offset_e = 0x0E,

  /** @brief Port F offset - PF5 (1 pin) */
  k_port_offset_f = 0x0F,

  /** @brief Port J offset - PJ3, PJ5 (2 pins) */
  k_port_offset_j = 0x12,
} rx_port_offsets_t;

/* =============================================================================
 * Port Register Structure
 * =============================================================================
 */

/**
 * @struct rx_port_regs_t
 * @brief Port Register Map providing logical per-port view of RX72N GPIO
 *
 * @details
 * This structure provides a convenient PORT-centric view of the GPIO registers,
 * abstracting away the hardware's type-grouped layout. Padding bytes ensure
 * correct alignment between register types.
 *
 * @par Register Memory Layout (297 bytes total per port view)
 * @verbatim
 *   Offset  Size  Register  Description
 *   ──────────────────────────────────────────────────────────────────
 *   0x00    1     PDR       Port Direction (0=input, 1=output)
 *   0x01    31    [pad]     Reserved padding to PODR
 *   0x20    1     PODR      Port Output Data (0=low, 1=high)
 *   0x21    31    [pad]     Reserved padding to PIDR
 *   0x40    1     PIDR      Port Input Data (read-only, current pin level)
 *   0x41    31    [pad]     Reserved padding to PMR
 *   0x60    1     PMR       Port Mode (0=GPIO, 1=peripheral function)
 *   0x61    95    [pad]     Reserved (includes ODR space, see note below)
 *   0xC0    1     PCR       Pull-up Control (0=disabled, 1=enabled)
 *   0xC1    31    [pad]     Reserved padding to DSCR
 *   0xE0    1     DSCR      Drive Capacity (0=normal, 1=high drive)
 *   0xE1    71    [pad]     Reserved padding to DSCR2
 *   0x128   1     DSCR2     Drive Capacity 2 (enhanced high drive)
 * @endverbatim
 *
 * @note ODR (Open Drain Control) registers are NOT part of this structure.
 * They use word addressing and are accessed via separate port*_odr() functions
 * at hardware addresses: 0x0008C080 + (port_offset * 2)
 *
 * @par GPIO Configuration Sequence
 * Typical GPIO output configuration sequence:
 * 1. Set PMR = 0 (GPIO mode, not peripheral)
 * 2. Set PDR = 1 (output direction)
 * 3. Set PODR = desired output level
 * 4. Optionally set DSCR = 1 for high drive current
 *
 * Typical GPIO input configuration sequence:
 * 1. Set PMR = 0 (GPIO mode)
 * 2. Set PDR = 0 (input direction)
 * 3. Optionally set PCR = 1 for internal pull-up
 * 4. Read PIDR for current pin state
 *
 * @par LED Blink Example
 * @code
 * // Configure PD0 as output and blink LED
 * volatile rx_port_regs_t* pd = portd();
 *
 * pd->pmr &= ~(1 << 0);   // GPIO mode
 * pd->pdr |= (1 << 0);    // Output direction
 *
 * while (1) {
 *     pd->podr ^= (1 << 0);  // Toggle LED
 *     delay_ms(500);
 * }
 * @endcode
 *
 * @par Button Read Example
 * @code
 * // Configure PA0 as input with pull-up and read button
 * volatile rx_port_regs_t* pa = porta();
 *
 * pa->pmr &= ~(1 << 0);   // GPIO mode
 * pa->pdr &= ~(1 << 0);   // Input direction
 * pa->pcr |= (1 << 0);    // Enable pull-up (button connects to GND)
 *
 * bool pressed = ((pa->pidr & (1 << 0)) == 0);  // Active-low
 * @endcode
 *
 * @invariant Structure size must be exactly 297 bytes (0x129)
 * @invariant All register offsets must match hardware layout exactly
 * @invariant Structure must be packed to ensure correct padding sizes
 *
 * @note Uses __attribute__((packed)) to prevent compiler padding optimizations
 *
 * @warning This structure assumes PORT0 padding layout (offset=0x00).
 * The type-grouped hardware layout places ODR at varying struct offsets
 * for different ports (0x80 + port_offset*2). ODR is accessed via
 * separate port*_odr() functions to ensure correct word addressing.
 *
 * @warning Do not use sizeof() for port address calculations; use offset enum
 *
 * @note PORT3 lacks DSCR (has only DSCR2).
 * @note PORT4 lacks both DSCR and DSCR2 (hardware limitation).
 * @note All other ports (0-2, 5, A-E, J) have both DSCR and DSCR2.
 *
 * @see rx_port_reg_bases_t for register base addresses
 * @see rx_port_offsets_t for port offset values
 * @see RX72N Hardware Manual Section 21.2 (Register Descriptions)
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief Port Direction Register (PDR) - +0x00
   * @details Configures each pin as input (0) or output (1).
   * @note Before writing PDR, ensure PMR = 0 for GPIO mode.
   */
  volatile uint8_t pdr;

  /** @brief Reserved padding (0x01-0x1F) to align PODR at +0x20 */
  volatile uint8_t _pad1[0x1F];

  /**
   * @brief Port Output Data Register (PODR) - +0x20
   * @details Sets output level: 0 = low, 1 = high.
   * @note Only effective when PDR bit = 1 (output direction).
   */
  volatile uint8_t podr;

  /** @brief Reserved padding (0x21-0x3F) to align PIDR at +0x40 */
  volatile uint8_t _pad2[0x1F];

  /**
   * @brief Port Input Data Register (PIDR) - +0x40
   * @details Read-only register returning current pin logic level.
   * @note Always readable regardless of PDR setting.
   */
  volatile uint8_t pidr;

  /** @brief Reserved padding (0x41-0x5F) to align PMR at +0x60 */
  volatile uint8_t _pad3[0x1F];

  /**
   * @brief Port Mode Register (PMR) - +0x60
   * @details Selects GPIO (0) or peripheral function (1) mode.
   * @note Set PMR = 0 before configuring GPIO direction/output.
   * @see MPC (Multi-function Pin Controller) for peripheral selection.
   */
  volatile uint8_t pmr;

  /** @brief Reserved padding (0x61-0xBF) to align PCR at +0xC0 */
  volatile uint8_t _pad4[0x5F];

  /**
   * @brief Pull-up Control Register (PCR) - +0xC0
   * @details Enables internal pull-up resistor (approx 50kΩ typical).
   * PCR bit = 0: Pull-up disabled.
   * PCR bit = 1: Pull-up enabled.
   * @note Only effective when PDR bit = 0 (input direction).
   */
  volatile uint8_t pcr;

  /** @brief Reserved padding (0xC1-0xDF) to align DSCR at +0xE0 */
  volatile uint8_t _pad5[0x1F];

  /**
   * @brief Drive Capacity Control Register (DSCR) - +0xE0
   * @details Configures output drive strength.
   * DSCR bit = 0: Normal drive capacity.
   * DSCR bit = 1: High drive capacity.
   * @note High drive useful for long traces or high capacitance loads.
   */
  volatile uint8_t dscr;

  /** @brief Reserved padding (0xE1-0x127) to align DSCR2 at +0x128 */
  volatile uint8_t _pad6[0x47];

  /**
   * @brief Drive Capacity Control Register 2 (DSCR2) - +0x128
   * @details Secondary drive capacity control for high-current applications.
   * DSCR2 bit = 0: Normal drive capacity (same as DSCR).
   * DSCR2 bit = 1: Enhanced high drive capacity.
   * @note Not available on PORT4 (hardware limitation).
   * @note PORT3 lacks DSCR but has DSCR2.
   */
  volatile uint8_t dscr2;
} rx_port_regs_t;

/* =============================================================================
 * Static Assertions - Compile-time Register Layout Verification
 * =============================================================================
 *
 * These static assertions verify that the rx_port_regs_t struct layout
 * exactly matches the RX72N hardware register addresses. Any mismatch
 * will cause a compile-time error, preventing runtime GPIO bugs.
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 21 (I/O Ports)
 *            Section 21.2: Register Descriptions
 */

/** @name PORT Register Offset Verification
 *  @brief Verify rx_port_regs_t matches hardware layout
 *  @{
 */
/* Note: struct size is 0x129 including DSCR2, ODR is accessed separately via word-addressed functions */
static_assert(sizeof(rx_port_regs_t) == 0x129,
               "Port register struct size must be 0x129 (297) bytes");
static_assert(offsetof(rx_port_regs_t, pdr) == 0x00,
               "PDR must be at offset 0x00");
static_assert(offsetof(rx_port_regs_t, podr) == 0x20,
               "PODR must be at offset 0x20");
static_assert(offsetof(rx_port_regs_t, pidr) == 0x40,
               "PIDR must be at offset 0x40");
static_assert(offsetof(rx_port_regs_t, pmr) == 0x60,
               "PMR must be at offset 0x60");
static_assert(offsetof(rx_port_regs_t, pcr) == 0xC0,
               "PCR must be at offset 0xC0");
static_assert(offsetof(rx_port_regs_t, dscr) == 0xE0,
               "DSCR must be at offset 0xE0");
static_assert(offsetof(rx_port_regs_t, dscr2) == 0x128,
               "DSCR2 must be at offset 0x128");
/** @} */ /* End of register offset verification group */

/** @name PORT Register Base Address Verification
 *  @brief Verify register base addresses match hardware memory map
 *  @{
 */
static_assert((uint32_t)k_port_dscr2_base == 0x0008C128,
               "DSCR2 base address must be 0x0008C128 (DSCR + 0x48)");
/** @} */ /* End of base address verification group */

/* =============================================================================
 * Inline Register Accessors
 * =============================================================================
 *
 * These inline functions provide type-safe access to each port's registers.
 * Using functions instead of macros provides:
 * - Type safety (returns proper volatile pointer)
 * - Debugger visibility (can set breakpoints)
 * - Namespace cleanliness (no macro pollution)
 */

/**
 * @brief Get pointer to PORT0 registers
 *
 * @details
 * Returns a volatile pointer to PORT0's register structure. PORT0 has
 * P00-P03, P05, P07 available on the 144-pin LFQFP package (6 pins).
 *
 * @return Volatile pointer to PORT0 register structure
 *
 * @par Register Addresses
 * - PDR:  0x0008C000
 * - PODR: 0x0008C020
 * - PIDR: 0x0008C040
 * - PMR:  0x0008C060
 *
 * @par Available Pins (144-pin)
 * P00-P03, P05, P07 (6 pins)
 *
 * @note Thread-safe: returns constant hardware address
 * @see rx_port_regs_t for register layout
 */
static inline volatile rx_port_regs_t* port0(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_0);
}

/**
 * @brief Get pointer to PORT1 registers
 * @return Volatile pointer to PORT1 register structure
 * @note PORT1 PDR = 0x0008C001, PODR = 0x0008C021, PIDR = 0x0008C041
 * @note 144-pin: Only P12-P17 available
 */
static inline volatile rx_port_regs_t* port1(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_1);
}

/**
 * @brief Get pointer to PORT2 registers
 * @return Volatile pointer to PORT2 register structure
 * @note PORT2 PDR = 0x0008C002, PODR = 0x0008C022, PIDR = 0x0008C042
 * @note 144-pin: P20-P27 available
 */
static inline volatile rx_port_regs_t* port2(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_2);
}

/**
 * @brief Get pointer to PORT3 registers
 * @return Volatile pointer to PORT3 register structure
 * @note PORT3 PDR = 0x0008C003, PODR = 0x0008C023, PIDR = 0x0008C043
 * @note 144-pin: P30-P37 available (P35 is input-only)
 */
static inline volatile rx_port_regs_t* port3(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_3);
}

/**
 * @brief Get pointer to PORT4 registers
 * @return Volatile pointer to PORT4 register structure
 * @note PORT4 PDR = 0x0008C004, PODR = 0x0008C024, PIDR = 0x0008C044
 * @note 144-pin: P40-P47 available
 */
static inline volatile rx_port_regs_t* port4(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_4);
}

/**
 * @brief Get pointer to PORT5 registers
 * @return Volatile pointer to PORT5 register structure
 * @note PORT5 PDR = 0x0008C005, PODR = 0x0008C025, PIDR = 0x0008C045
 * @note 144-pin: P50-P56 available (7 pins)
 */
static inline volatile rx_port_regs_t* port5(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_5);
}

/**
 * @brief Get pointer to PORT6 registers
 * @return Volatile pointer to PORT6 register structure
 * @note PORT6 PDR = 0x0008C006, PODR = 0x0008C026, PIDR = 0x0008C046
 * @note 144-pin: P60-P67 available (8 pins)
 */
static inline volatile rx_port_regs_t* port6(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_6);
}

/**
 * @brief Get pointer to PORT7 registers
 * @return Volatile pointer to PORT7 register structure
 * @note PORT7 PDR = 0x0008C007, PODR = 0x0008C027, PIDR = 0x0008C047
 * @note 144-pin: P70-P77 available (8 pins)
 */
static inline volatile rx_port_regs_t* port7(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_7);
}

/**
 * @brief Get pointer to PORT8 registers
 * @return Volatile pointer to PORT8 register structure
 * @note PORT8 PDR = 0x0008C008, PODR = 0x0008C028, PIDR = 0x0008C048
 * @note 144-pin: P80-P83, P86-P87 available (6 pins)
 */
static inline volatile rx_port_regs_t* port8(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_8);
}

/**
 * @brief Get pointer to PORT9 registers
 * @return Volatile pointer to PORT9 register structure
 * @note PORT9 PDR = 0x0008C009, PODR = 0x0008C029, PIDR = 0x0008C049
 * @note 144-pin: P90-P93 available (4 pins)
 */
static inline volatile rx_port_regs_t* port9(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_9);
}

/**
 * @brief Get pointer to PORTA registers
 * @return Volatile pointer to PORTA register structure
 * @note PORTA PDR = 0x0008C00A, PODR = 0x0008C02A, PIDR = 0x0008C04A
 * @note 144-pin: PA0-PA7 available
 */
static inline volatile rx_port_regs_t* porta(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_a);
}

/**
 * @brief Get pointer to PORTB registers
 * @return Volatile pointer to PORTB register structure
 * @note PORTB PDR = 0x0008C00B, PODR = 0x0008C02B, PIDR = 0x0008C04B
 * @note 144-pin: PB0-PB7 available
 */
static inline volatile rx_port_regs_t* portb(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_b);
}

/**
 * @brief Get pointer to PORTC registers
 * @return Volatile pointer to PORTC register structure
 * @note PORTC PDR = 0x0008C00C, PODR = 0x0008C02C, PIDR = 0x0008C04C
 * @note 144-pin: PC0-PC7 available
 */
static inline volatile rx_port_regs_t* portc(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_c);
}

/**
 * @brief Get pointer to PORTD registers
 * @return Volatile pointer to PORTD register structure
 * @note PORTD PDR = 0x0008C00D, PODR = 0x0008C02D, PIDR = 0x0008C04D
 * @note 144-pin: PD0-PD7 available
 */
static inline volatile rx_port_regs_t* portd(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_d);
}

/**
 * @brief Get pointer to PORTE registers
 * @return Volatile pointer to PORTE register structure
 * @note PORTE PDR = 0x0008C00E, PODR = 0x0008C02E, PIDR = 0x0008C04E
 * @note 144-pin: PE0-PE7 available
 */
static inline volatile rx_port_regs_t* porte(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_e);
}

/**
 * @brief Get pointer to PORTF registers
 * @return Volatile pointer to PORTF register structure
 * @note PORTF PDR = 0x0008C00F, PODR = 0x0008C02F, PIDR = 0x0008C04F
 * @note 144-pin: PF5 available (1 pin)
 */
static inline volatile rx_port_regs_t* portf(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_f);
}

/**
 * @brief Get pointer to PORTJ registers
 * @return Volatile pointer to PORTJ register structure
 * @note PORTJ PDR = 0x0008C012, PODR = 0x0008C032, PIDR = 0x0008C052
 * @note 144-pin: PJ3, PJ5 available (2 pins)
 */
static inline volatile rx_port_regs_t* portj(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_j);
}

/* =============================================================================
 * ODR (Open Drain) Register Accessors
 * =============================================================================
 *
 * ODR registers use WORD (16-bit) addressing, not byte addressing like other
 * PORT registers. Each port's ODR is at: 0x0008C080 + (port_offset * 2)
 *
 * Hardware addresses (from RX72N manual Ch22):
 *   PORT0.ODR: 0x0008C080 + (0x00 * 2) = 0x0008C080
 *   PORT1.ODR: 0x0008C080 + (0x01 * 2) = 0x0008C082
 *   PORTA.ODR: 0x0008C080 + (0x0A * 2) = 0x0008C094
 *   PORTJ.ODR: 0x0008C080 + (0x12 * 2) = 0x0008C0A4
 *
 * These inline accessors ensure correct addressing for all ports.
 */

/**
 * @brief Get pointer to PORT0 ODR register
 * @return Volatile pointer to 16-bit ODR register (combines ODR0+ODR1)
 * @note PORT0.ODR = 0x0008C080
 */
static inline volatile uint16_t* port0_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_0 * 2));
}

/**
 * @brief Get pointer to PORT1 ODR register  
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT1.ODR = 0x0008C082 (word addressing: base + offset*2)
 */
static inline volatile uint16_t* port1_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_1 * 2));
}

/**
 * @brief Get pointer to PORT2 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT2.ODR = 0x0008C084
 */
static inline volatile uint16_t* port2_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_2 * 2));
}

/**
 * @brief Get pointer to PORT3 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT3.ODR = 0x0008C086
 */
static inline volatile uint16_t* port3_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_3 * 2));
}

/**
 * @brief Get pointer to PORT4 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT4.ODR = 0x0008C088
 */
static inline volatile uint16_t* port4_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_4 * 2));
}

/**
 * @brief Get pointer to PORT5 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT5.ODR = 0x0008C08A
 */
static inline volatile uint16_t* port5_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_5 * 2));
}

/**
 * @brief Get pointer to PORT6 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT6.ODR = 0x0008C08C (offset 0x06 * 2)
 */
static inline volatile uint16_t* port6_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_6 * 2));
}

/**
 * @brief Get pointer to PORT7 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT7.ODR = 0x0008C08E (offset 0x07 * 2)
 */
static inline volatile uint16_t* port7_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_7 * 2));
}

/**
 * @brief Get pointer to PORT8 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT8.ODR = 0x0008C090 (offset 0x08 * 2)
 */
static inline volatile uint16_t* port8_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_8 * 2));
}

/**
 * @brief Get pointer to PORT9 ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORT9.ODR = 0x0008C092 (offset 0x09 * 2)
 */
static inline volatile uint16_t* port9_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_9 * 2));
}

/**
 * @brief Get pointer to PORTA ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTA.ODR = 0x0008C094 (offset 0x0A * 2)
 */
static inline volatile uint16_t* porta_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_a * 2));
}

/**
 * @brief Get pointer to PORTB ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTB.ODR = 0x0008C096
 */
static inline volatile uint16_t* portb_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_b * 2));
}

/**
 * @brief Get pointer to PORTC ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTC.ODR = 0x0008C098
 */
static inline volatile uint16_t* portc_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_c * 2));
}

/**
 * @brief Get pointer to PORTD ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTD.ODR = 0x0008C09A
 */
static inline volatile uint16_t* portd_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_d * 2));
}

/**
 * @brief Get pointer to PORTE ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTE.ODR = 0x0008C09C
 */
static inline volatile uint16_t* porte_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_e * 2));
}

/**
 * @brief Get pointer to PORTF ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTF.ODR = 0x0008C09E (offset 0x0F * 2)
 */
static inline volatile uint16_t* portf_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_f * 2));
}

/**
 * @brief Get pointer to PORTJ ODR register
 * @return Volatile pointer to 16-bit ODR register
 * @note PORTJ.ODR = 0x0008C0A4 (offset 0x12 * 2)
 */
static inline volatile uint16_t* portj_odr(void)
{
  return (volatile uint16_t*)(k_port_odr0_base + (k_port_offset_j * 2));
}

/** @} */ /* End of port_regs defgroup */

#ifdef __cplusplus
}
#endif
