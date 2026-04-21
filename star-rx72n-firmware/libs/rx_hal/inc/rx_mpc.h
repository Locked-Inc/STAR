/**
 * @file rx_mpc.h
 * @brief Multi-Function Pin Controller (MPC) Driver API for RX72N
 *
 * @details
 * High-level driver for configuring RX72N GPIO pin multiplexing. The MPC
 * module controls which peripheral function is connected to each physical
 * pin through Pin Function Select (PFS) registers.
 *
 * @par System Architecture
 * @verbatim
 *                        MPC Driver Architecture
 *   +---------------------------------------------------------------------+
 *   |                         Application Layer                          |
 *   |   +---------+  +---------+  +---------+  +---------+  +---------+  |
 *   |   | Motor   |  | Encoder |  |  UART   |  |   SPI   |  |   ADC   |  |
 *   |   | Control |  | Driver  |  | Debug   |  |  Comm   |  | Sensing |  |
 *   |   +----+----+  +----+----+  +----+----+  +----+----+  +----+----+  |
 *   +--------+-----------+-----------+-----------+-----------+-----------+
 *            |           |           |           |           |
 *   +--------v-----------v-----------v-----------v-----------v-----------+
 *   |                         rx_mpc.h API                               |
 *   |   +-------------+ +-------------+ +-------------+ +-------------+  |
 *   |   |rx_mpc_set_  | |rx_mpc_set_  | |rx_mpc_set_  | |rx_mpc_set_  |  |
 *   |   |  mtu_pwm()  | |  sci()      | |  rspi()     | |  gpio()     |  |
 *   |   +------+------+ +------+------+ +------+------+ +------+------+  |
 *   |          |               |               |               |         |
 *   |   +------v---------------v---------------v---------------v------+  |
 *   |   |              rx_mpc_set_peripheral()                        |  |
 *   |   |     (core PSEL configuration function -- peripheral path)    |  |
 *   |   +-------------------------+-----------------------------------+  |
 *   |                             |                                       |
 *   |   Note: rx_mpc_set_irq() and rx_mpc_set_adc() bypass               |
 *   |   rx_mpc_set_peripheral() and write ISEL/ASEL bits directly        |
 *   |   via internal_write_pfs() (ISEL=0x40, ASEL=0x80 exceed PSEL       |
 *   |   5-bit field; they are separate bits in the PFS register).        |
 *   +-----------------------------+--------------------------------------+
 *                                 |
 *   +-----------------------------v--------------------------------------+
 *   |                       rx72n_mpc_regs.h                             |
 *   |   +------------+      +------------+      +--------------------+   |
 *   |   |   PWPR     |      |  PFS Regs  |      |   mpc() accessor   |   |
 *   |   | (Protect)  |----->| P00PFS-    |<-----|   (0x0008C100)     |   |
 *   |   |            |      | PJ5PFS     |      |                    |   |
 *   |   +------------+      +------------+      +--------------------+   |
 *   +--------------------------------------------------------------------+
 *                                 |
 *   +-----------------------------v--------------------------------------+
 *   |                      Physical Pins                                 |
 *   |   Pin (function) --> Peripheral    Pin (function) --> Peripheral   |
 *   |   (Configured via PFS register PSEL field)                        |
 *   |   @see hardware_config.h for application-level pin assignments    |
 *   +--------------------------------------------------------------------+
 * @endverbatim
 *
 * @par Pin Configuration Flow
 * @msc
 *   Application, MPC_Driver, PWPR, PFS_Register, Physical_Pin;
 *
 *   --- [label="Pin Configuration Sequence"];
 *   Application => MPC_Driver [label="rx_mpc_set_peripheral(&config)"];
 *   MPC_Driver box MPC_Driver [label="Validate pin/psel"];
 *   MPC_Driver => PWPR [label="Unlock (B0WI=0, PFSWE=1)"];
 *   MPC_Driver => PFS_Register [label="Write PSEL value"];
 *   MPC_Driver => PWPR [label="Lock (PFSWE=0, B0WI=1)"];
 *   MPC_Driver => Application [label="k_rx_ok"];
 *
 *   --- [label="Pin Now Active"];
 *   Physical_Pin box Physical_Pin [label="Connected to peripheral"];
 * @endmsc
 *
 * @par Pin Assignments
 * Application-level pin assignments are defined in hardware_config.h.
 * This driver provides the MPC register interface for any valid pin/PSEL
 * combination. See the RX72N Hardware Manual pin function tables for
 * valid (pin, PSEL) pairs.
 *
 * @see hardware_config.h Application-level pin assignments
 *
 * @par Hardware Requirements
 * | Resource | Requirement | Notes |
 * |----------|-------------|-------|
 * | MPC Base | 0x0008C100 | Fixed hardware address |
 * | PCLKB | Active | Required for register access |
 * | MSTPCRA | Bit 9 = 0 | MPC module stop cleared |
 *
 * @par Performance Characteristics
 * | Operation | Typical Time | Notes |
 * |-----------|-------------|-------|
 * | Pin config | ~1 us | PWPR unlock/lock overhead |
 * | Bulk config (10 pins) | ~10 us | Sequential calls |
 *
 * @par Memory Usage
 * | Category | Size | Notes |
 * |----------|------|-------|
 * | Code | ~800 bytes | All functions |
 * | Static data | 4 bytes | Tag string only |
 * | Stack (per call) | 16 bytes | Local variables |
 *
 * @par Thread Safety
 * **Not thread-safe.** The MPC PWPR unlock/lock sequence is non-atomic.
 * If multiple threads call MPC functions concurrently, race conditions can
 * occur where one thread's unlock is clobbered by another's lock. Use
 * external mutex protection or configure all pins during single-threaded
 * initialization before starting RTOS.
 *
 * @par Initialization Order
 * MPC configuration should occur early in system initialization:
 * 1. Clock configuration (PCLKB must be running)
 * 2. Module stop clear (if MPC in module stop)
 * 3. **MPC pin configuration** < This module
 * 4. GPIO direction configuration (PMR, PDR registers)
 * 5. Peripheral driver initialization
 *
 * @par Usage Example
 * @code{.c}
 * #include "rx_mpc.h"
 * #include "rx_port_constants.h"
 *
 * void hardware_init_pins(void)
 * {
 *     rx_err_t err;
 *
 *     // Configure motor PWM pins (GPTW channel 0)
 *     const rx_mpc_peripheral_config_t pwm_config = {
 *         .pin  = k_port_e_pin_5,  // PE5 = GPTW0A
 *         .psel = k_psel_gptw      // GPTW function (0x1E)
 *     };
 *     err = rx_mpc_set_peripheral(&pwm_config);
 *     if (err != k_rx_ok) {
 *         // Handle error
 *     }
 *
 *     // Configure encoder pins (MTU phase counting)
 *     err = rx_mpc_set_mtu_encoder(k_port_2_pin_4);  // P24 = MTCLKA
 *     err = rx_mpc_set_mtu_encoder(k_port_2_pin_5);  // P25 = MTCLKB
 *
 *     // Configure debug UART (SCI9)
 *     err = rx_mpc_set_sci(k_port_b_pin_7);  // PB7 = TXD9
 *     err = rx_mpc_set_sci(k_port_b_pin_6);  // PB6 = RXD9
 *
 *     // Configure SPI communication (RSPI0)
 *     err = rx_mpc_set_rspi(k_port_a_pin_0);  // PA0 = COPI
 *     err = rx_mpc_set_rspi(k_port_a_pin_1);  // PA1 = CIPO
 *     err = rx_mpc_set_rspi(k_port_a_pin_3);  // PA3 = CLK
 *     err = rx_mpc_set_rspi(k_port_a_pin_4);  // PA4 = SSL0
 * }
 * @endcode
 *
 * @par Error Handling Example
 * @code{.c}
 * rx_err_t configure_pin_safe(rx_port_pin_t pin, uint8_t psel)
 * {
 *     // Validate PSEL range before calling
 *     if (psel > 0x1F) {
 *         rx_log_error("MPC", "PSEL exceeds 5-bit maximum");
 *         return k_rx_err_invalid_arg;
 *     }
 *
 *     rx_mpc_peripheral_config_t config = {
 *         .pin  = pin,
 *         .psel = psel
 *     };
 *
 *     rx_err_t err = rx_mpc_set_peripheral(&config);
 *     if (err != k_rx_ok) {
 *         rx_log_error("MPC", "Pin configuration failed");
 *         // Configuration failed - pin remains in previous state
 *         return err;
 *     }
 *
 *     // Remember: Also need to set PMR bit to enable peripheral mode!
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No goto, setjmp, longjmp, or recursion
 * - **Rule 2**: [OK] All loops have fixed upper bounds (switch/case only)
 * - **Rule 3**: [OK] No dynamic memory allocation
 * - **Rule 4**: [OK] All functions < 60 lines
 * - **Rule 5**: [OK] Minimum 2 assertions per function (parameter validation)
 * - **Rule 6**: [OK] Variables declared at smallest scope
 * - **Rule 7**: [OK] All return values checked
 * - **Rule 8**: [OK] C23 typed enums for all constants, no magic numbers
 * - **Rule 9**: [WARN] Function pointers not used (simple direct calls)
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: MPC driver handles only pin mux configuration
 * - **Open/Closed**: New peripherals added via rx_mpc_set_peripheral() without
 *   modifying existing code
 * - **Interface Segregation**: Convenience functions (set_sci, set_rspi) separate
 *   from low-level set_peripheral()
 * - **Dependency Inversion**: Higher-level drivers depend on this abstraction,
 *   not on raw register writes
 *
 * @par Module Dependencies
 * @dot
 * digraph mpc_deps {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   rx_mpc [label="rx_mpc.h\n(this module)"];
 *   rx_err [label="rx_err.h\n(error codes)"];
 *   rx_port [label="rx_port_constants.h\n(pin definitions)"];
 *   rx_mpc_regs [label="rx72n_mpc_regs.h\n(register access)"];
 *   rx_check [label="rx_check.h\n(validation macros)"];
 *   rx_log [label="rx_log.h\n(error logging)"];
 *
 *   rx_mpc -> rx_err;
 *   rx_mpc -> rx_port;
 *   rx_mpc -> rx_mpc_regs [style=dashed, label="impl only"];
 *   rx_mpc -> rx_check [style=dashed, label="impl only"];
 *   rx_mpc -> rx_log [style=dashed, label="impl only"];
 * }
 * @enddot
 *
 * @see rx72n_mpc_regs.h Low-level MPC register definitions
 * @see rx_port_constants.h Pin enumeration definitions
 * @see docs/sections/03_hardware_pinout.tex Complete STAR pinout documentation
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Structures
 * =============================================================================
 */

/**
 * @struct rx_mpc_peripheral_config_t
 * @brief MPC peripheral configuration structure for pin function assignment
 *
 * @details
 * This structure groups related parameters to prevent accidental parameter
 * swapping when calling rx_mpc_set_peripheral(). Using a struct provides:
 * - Type safety (compiler catches wrong parameter types)
 * - Named parameters (self-documenting code)
 * - Extensibility (can add fields without changing function signature)
 *
 * @par Memory Layout
 * | Offset | Size | Field | Type | Alignment |
 * |--------|------|-------|------|-----------|
 * | 0 | 2 | pin | rx_port_pin_t (uint16_t) | 2 |
 * | 2 | 1 | psel | uint8_t | 1 |
 * | 3 | 1 | (padding) | - | - |
 * | **Total** | **4 bytes** | | | |
 *
 * @par Field Relationships
 * - `pin` determines which PFS register to modify
 * - `psel` determines what value to write to that register
 * - The (pin, psel) pair must be valid per RX72N hardware manual appendix
 *
 * @par Usage Example - Basic Configuration
 * @code{.c}
 * // Configure PE5 for GPTW0A (motor PWM output)
 * rx_mpc_peripheral_config_t pwm_config = {
 *     .pin  = k_port_e_pin_5,  // PE5
 *     .psel = k_psel_gptw      // GPTW function select (0x1E)
 * };
 * rx_err_t err = rx_mpc_set_peripheral(&pwm_config);
 * @endcode
 *
 * @par Usage Example - Multiple Pins
 * @code{.c}
 * // Configure all SPI pins using array
 * const rx_mpc_peripheral_config_t spi_pins[] = {
 *     { .pin = k_port_a_pin_0, .psel = 0x0D },  // COPI
 *     { .pin = k_port_a_pin_1, .psel = 0x0D },  // CIPO
 *     { .pin = k_port_a_pin_3, .psel = 0x0D },  // CLK
 *     { .pin = k_port_a_pin_4, .psel = 0x0D },  // SSL0
 * };
 *
 * for (uint8_t i = 0; i < 4; i++) {
 *     rx_err_t err = rx_mpc_set_peripheral(&spi_pins[i]);
 *     if (err != k_rx_ok) {
 *         // Handle error
 *     }
 * }
 * @endcode
 *
 * @invariant pin must be a valid rx_port_pin_t value from rx_port_constants.h
 * @invariant psel must be in range [0, 31] (5-bit field)
 * @invariant (pin, psel) combination must be valid per hardware manual
 *
 * @see rx_mpc_set_peripheral() Function that uses this structure
 * @see rx_port_constants.h Pin enumeration definitions
 * @see rx_pin_psel_t Common PSEL values for STAR project
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief GPIO pin to configure for peripheral function
   * @details
   * Encoded pin identifier from rx_port_constants.h combining port and
   * pin number. Use the k_port_X_pin_Y constants for type safety.
   * @par Valid Values
   * Any valid rx_port_pin_t constant, e.g.:
   * - k_port_a_pin_0 through k_port_a_pin_7
   * - k_port_b_pin_0 through k_port_b_pin_7
   * - k_port_e_pin_0 through k_port_e_pin_7
   * - etc.
   * @note On 144-pin package, ports G, H are not available
   * @warning Using invalid pin values returns k_rx_err_invalid_arg
   */
  rx_port_pin_t pin;

  /**
   * @brief Peripheral function select code (PSEL field value)
   * @details
   * Value written to the PSEL[4:0] field of the PFS register. This
   * determines which peripheral function the pin is connected to.
   *
   * @par Valid Range: [0, 31] (0x00 - 0x1F)
   *
   * @par Common Values (varies by pin, check hardware manual):
   * | PSEL | Function | Typical Pins |
   * |------|----------|--------------|
   * | 0x00 | Hi-Z/GPIO | All |
   * | 0x01 | MTU I/O compare (MTIOC) | P14-P17, P24-P27 |
   * | 0x02 | MTU clock input (MTCLK) | P24, P25, PC0, PC1 |
   * | 0x03 | MTU phase counter | P24, P25 |
   * | 0x1E | GPTW output (GTIOC) | PE3, PE7, P23, P22, etc. |
   * | 0x0A | SCI (TXD/RXD) | PB6, PB7 |
   * | 0x0D | RSPI (CLK/COPI/CIPO/SSL) | PA0-PA4 |
   * | 0x0F | RIIC (SCL/SDA) | P12, P13 |
   *
   * @warning Wrong PSEL value for a pin may cause hardware malfunction
   * @see rx_pin_psel_t Predefined PSEL constants
   */
  uint8_t psel;
} rx_mpc_peripheral_config_t;

/* =============================================================================
 * Pin Function Codes
 * =============================================================================
 */

/**
 * @enum rx_pin_function_t
 * @brief High-level pin function mode selection
 *
 * @details
 * Specifies whether a pin is configured for GPIO operation or peripheral
 * function mode. This is a conceptual abstraction - the actual hardware
 * configuration is done through PFS register PSEL field and PORT PMR bit.
 *
 * @par Pin Configuration State Machine
 * @startuml
 * [*] --> GPIO : Power-on default
 * GPIO --> Peripheral : rx_mpc_set_peripheral()
 * Peripheral --> GPIO : rx_mpc_set_gpio()
 * GPIO --> GPIO : rx_mpc_set_gpio()
 * Peripheral --> Peripheral : rx_mpc_set_peripheral()\n(change function)
 * @enduml
 *
 * @note This enum is for documentation/API clarity. Actual hardware uses
 *       PSEL field values directly.
 *
 * @see rx_mpc_set_gpio() Configure pin for GPIO mode
 * @see rx_mpc_set_peripheral() Configure pin for peripheral mode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief GPIO mode - pin controlled by PORT registers
   * @details
   * Pin is disconnected from all peripheral functions. Direction and
   * data controlled by PORT PDR and PODR registers. This is the
   * power-on default for all pins.
   * @par PFS Register Value: 0x00 (PSEL=0, ISEL=0, ASEL=0)
   */
  k_pin_function_gpio = 0x00,

  /**
   * @brief Peripheral function mode - pin connected to peripheral
   * @details
   * Pin is connected to a peripheral function as determined by the
   * PSEL field. The PORT PMR bit must also be set to 1 to actually
   * enable peripheral mode.
   * @par PFS Register Value: PSEL field set to peripheral code
   * @note This is a placeholder value; actual PSEL varies by function
   */
  k_pin_function_periph = 0x01,
} rx_pin_function_t;

/**
 * @enum rx_pin_psel_t
 * @brief Peripheral Select (PSEL) codes for common STAR project functions
 *
 * @details
 * These values are written to the PSEL[4:0] field of PFS registers to select
 * which peripheral function a pin is connected to. Values are pin-specific -
 * these constants represent the most common values used in the STAR project.
 *
 * @par PSEL Field in PFS Register
 * @verbatim
 *   Bit:   7      6      5      4  3  2  1  0
 *        +------+------+------+-----------------+
 *        | ASEL | ISEL | Rsvd |    PSEL[4:0]    |
 *        +------+------+------+-----------------+
 *                              <-- These values -->
 * @endverbatim
 *
 * @par PSEL Values by Peripheral (STAR Project)
 * | Peripheral | PSEL | Pins Used | Function |
 * |------------|------|-----------|----------|
 * | MTU3a MTIOC | 0x01 | P14-P17, P24-P27 | PWM output |
 * | MTU3a MTCLK | 0x02 | P24, P25, PC0, PC1 | Clock/encoder input |
 * | MTU3a Phase | 0x03 | P24, P25 | Phase counting mode |
 * | GPTW GTIOC | 0x1E | PE3, PE7, P23, P22, etc. | PWM output |
 * | SCI TXD/RXD | 0x0A | PB6, PB7 | UART data |
 * | RSPI signals | 0x0D | PA0-PA4 | SPI bus |
 * | RIIC SDA/SCL | 0x0F | P12, P13 | I2C bus |
 *
 * @warning PSEL values are pin-specific! A value that works for one pin
 *          may select a completely different function on another pin.
 *          Always verify against the RX72N Hardware Manual pin function tables.
 *
 * @par Usage Example
 * @code{.c}
 * // Configure PB7 for SCI9 TXD
 * rx_mpc_peripheral_config_t config = {
 *     .pin  = k_port_b_pin_7,
 *     .psel = k_psel_sci_tx  // 0x0A
 * };
 * rx_mpc_set_peripheral(&config);
 * @endcode
 *
 * @see rx_mpc_set_peripheral() Use these values with this function
 * @see RX72N Hardware Manual, Appendix for complete pin function tables
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief MTU I/O Compare/PWM output (MTIOC)
   * @details
   * Connects pin to MTU3a compare match output for PWM generation.
   * @par Typical Pins: P14-P17 (MTU3), P24-P27 (MTU4)
   * @par Application: Motor PWM outputs when using MTU3a timer
   */
  k_psel_mtu_ioc = 0x01,

  /**
   * @brief MTU clock input (MTCLKA/B/C/D)
   * @details
   * Connects pin to MTU3a external clock input. On RX72N, PSEL = 0x02
   * (0b000010) is the generic "MTCLK" function and is the same value whether
   * the MTU is configured for normal external-clock counting or for phase
   * counting (quadrature encoder) mode. The counting mode is selected by
   * MTU.TMDR, not by PSEL -- callers must not pick a different PSEL just
   * because they want phase counting.
   *
   * Reference: RX72N Group Hardware Manual (R01UH0824EJ0111), chapter 23
   * "Multi-Function Pin Controller (MPC)". For the 144-pin LFQFP package:
   *   - Table 23.5 (P1x pins): row PSEL = 000010b -> MTCLKA on P14,
   *     MTCLKB on P15
   *   - Table 23.6 (P2x pins): row PSEL = 000010b -> MTCLKC on P22,
   *     MTCLKD on P23, MTCLKA on P24, MTCLKB on P25
   *   - Table 23.17 (PAx pins): row PSEL = 000010b -> MTCLKC on PA1,
   *     MTCLKD on PA3, MTCLKA on PA4, MTCLKB on PA6
   *   - Table 23.19 (PCx pins): row PSEL = 000010b -> MTCLKC on PC4,
   *     MTCLKD on PC5, MTCLKA on PC6, MTCLKB on PC7
   * Every MTCLK pin on this package resolves to PSEL = 0b00'0010 = 0x02.
   *
   * @par Typical Pins: P24/P25 (MTCLKA/B), PA1/PC5 (MTCLKC/D)
   * @par Application: Encoder pulse counting, quadrature phase counting
   */
  k_psel_mtu_clk = 0x02,

  /**
   * @brief MTU encoder phase counting input (alias for k_psel_mtu_clk)
   * @details
   * Same PSEL value on RX72N as k_psel_mtu_clk -- both MTU external clock
   * counting and MTU phase counting route through PSEL=0x02, and the
   * distinction is selected later via MTU.TMDR (phase counting mode bits).
   * The split constants exist only so calling code reads clearly; do not
   * assume they are different values.
   *
   * @par Typical Pins: P24/P25 pair, PA1/PC5 pair (same as k_psel_mtu_clk)
   * @par Application: Quadrature encoder interface with direction detection
   *
   * @note Previously 0x03 -- that was wrong and caused MTU encoders to stay
   *       at TCNT=0x0000 because PSEL=0x03 on MTU-candidate pins selects an
   *       unrelated function (TPU on Port C pins, or nothing elsewhere).
   *       See chapter 23 of the RX72N Group Hardware Manual
   *       (R01UH0824EJ0111), Tables 23.5, 23.6, 23.17, 23.19.
   */
  k_psel_mtu_phase = 0x02,

  /**
   * @brief TPU external clock input on Port C pins (TCLKA/B/C/D on PC0-PC3)
   * @details
   * RX72N assigns TPU external clock to alternate function PSEL=0x03 on
   * Port C pins only. Non-Port-C candidates for the same TCLK signal use a
   * different PSEL (see k_psel_tpu_clk_alt).
   *
   * Reference: RX72N Group Hardware Manual (R01UH0824EJ0111), chapter 23
   * "Multi-Function Pin Controller (MPC)", Table 23.19 (PCx pins). For the
   * 144-pin LFQFP package the row PSEL = 000011b maps to:
   *   PC0 -> TCLKC,  PC1 -> TCLKD,  PC2 -> TCLKA,  PC3 -> TCLKB
   * All four TPU TCLK inputs on Port C resolve to PSEL = 0b00'0011 = 0x03.
   *
   * @par Typical Pins: PC2 (TCLKA), PC3 (TCLKB), PC0 (TCLKC), PC1 (TCLKD)
   * @par Application: Rear-wheel quadrature encoder Phase A inputs on Port C
   *
   * @note rx_mpc_set_tpu_encoder() picks between this and k_psel_tpu_clk_alt
   *       automatically based on the port number of the pin argument.
   */
  k_psel_tpu_clk_portc = 0x03,

  /**
   * @brief TPU external clock input on non-Port-C pins (TCLKA/B/C/D)
   * @details
   * RX72N assigns TPU external clock to alternate function PSEL=0x04 on
   * Port 1, Port A, and Port B candidates for the TCLK signals. This differs
   * from the Port C candidates (k_psel_tpu_clk_portc = 0x03) and the
   * difference is silicon-level -- the same TCLK signal is routed through
   * different PFS alternate-function slots on different ports.
   *
   * Reference: RX72N Group Hardware Manual (R01UH0824EJ0111), chapter 23
   * "Multi-Function Pin Controller (MPC)". For the 144-pin LFQFP package
   * the row PSEL = 000100b maps to:
   *   - Table 23.5 (P1x pins): P14 -> TCLKA, P15 -> TCLKB,
   *                             P16 -> TCLKC, P17 -> TCLKD
   *   - Table 23.17 (PAx pins): PA3 -> TCLKB
   *   - Table 23.18 (PBx pins): PB2 -> TCLKC, PB3 -> TCLKD
   * All non-Port-C TPU TCLK inputs resolve to PSEL = 0b00'0100 = 0x04.
   *
   * @par Typical Pins: PA3 (TCLKB), PB3 (TCLKD) -- STAR rear-wheel Phase B
   * @par Application: Rear-wheel quadrature encoder Phase B inputs
   *
   * @note Before this split, rx_mpc_set_tpu_encoder() used PSEL=0x03 for
   *       every TPU pin, which silently misconfigured PA3/PB3 (the two
   *       non-Port-C pins on rear wheels) so one half of each rear-wheel
   *       encoder never incremented.
   */
  k_psel_tpu_clk_alt = 0x04,

  /**
   * @brief SCI transmit data output (TXD)
   * @details
   * Connects pin to SCI channel transmit data output.
   * @par Typical Pins: PB7 (SCI9-TXD for debug UART)
   * @par Application: UART transmit, debug console output
   * @note Same PSEL value used for both TX and RX on SCI pins
   */
  k_psel_sci_tx = 0x0A,

  /**
   * @brief SCI receive data input (RXD)
   * @details
   * Connects pin to SCI channel receive data input.
   * @par Typical Pins: PB6 (SCI9-RXD for debug UART)
   * @par Application: UART receive, debug console input
   * @note Same PSEL value as TX (pin determines actual function)
   */
  k_psel_sci_rx = 0x0A,

  /**
   * @brief RIIC clock line (SCL)
   * @details
   * Connects pin to RIIC (I2C) clock line with open-drain output.
   * @par Typical Pins: P12 (RIIC0-SCL)
   * @par Application: I2C bus clock for sensors/peripherals
   * @note RIIC automatically configures open-drain when enabled
   */
  k_psel_riic_scl = 0x0F,

  /**
   * @brief RIIC data line (SDA)
   * @details
   * Connects pin to RIIC (I2C) data line with open-drain output.
   * @par Typical Pins: P13 (RIIC0-SDA)
   * @par Application: I2C bus data for sensors/peripherals
   * @note Same PSEL value as SCL (pin determines actual function)
   */
  k_psel_riic_sda = 0x0F,

  /**
   * @brief RSPI clock (RSPCK)
   * @details
   * Connects pin to RSPI serial clock output (controller mode) or
   * input (peripheral mode).
   * @par Typical Pins: PA5 (RSPI0-RSPCKA); see RX72N HW manual R01UH0824EJ0111 Ch 23 Table 23.17 (MPC PAn pin function select)
   * @par Application: SPI bus clock for RPi5 communication
   */
  k_psel_rspi_clk = 0x0D,

  /**
   * @brief RSPI Controller Out Peripheral In (COPI, formerly MOSI)
   * @details
   * Connects pin to RSPI data output line in controller mode.
   * @par Typical Pins: PA6 (RSPI0-MOSIA / COPI); see RX72N HW manual R01UH0824EJ0111 Ch 23 Table 23.17 (MPC PAn pin function select)
   * @par Application: SPI transmit data to peripherals
   * @note Uses OSHWA-approved inclusive terminology
   */
  k_psel_rspi_copi = 0x0D,

  /**
   * @brief RSPI Controller In Peripheral Out (CIPO, formerly MISO)
   * @details
   * Connects pin to RSPI data input line in controller mode.
   * @par Typical Pins: PA7 (RSPI0-MISOA / CIPO); see RX72N HW manual R01UH0824EJ0111 Ch 23 Table 23.17 (MPC PAn pin function select)
   * @par Application: SPI receive data from peripherals
   * @note Uses OSHWA-approved inclusive terminology
   */
  k_psel_rspi_cipo = 0x0D,

  /**
   * @brief ADC analog input mode
   * @details
   * Disables digital input buffer for analog signal measurement.
   * Actually sets ASEL bit in PFS, not PSEL field.
   * @par Typical Pins: P40-P47 (AN000-AN007)
   * @par Application: Motor current sensing, analog input measurement
   * @note Use rx_mpc_set_adc() for proper configuration (sets ASEL bit)
   */
  k_psel_adc = 0x00,

  /**
   * @brief USB VBUS detection input
   *
   * @details
   * Connects pin to USB VBUS detection for 5V presence sensing.
   * Required for USB enumeration and power monitoring.
   *
   * @par Typical Pins: P1.6 (USB0_VBUS on RX72N)
   * @par Application: USB CDC debug interface, RPi5 communication
   *
   * @note Active-high: pin reads 1 when 5V present on USB bus
   * @note Requires additional USB PHY and controller configuration
   *
   * @since Version 1.0.0
   */
  k_psel_usb_vbus = 0x11,

  /**
   * @brief GPTW complementary PWM output (GTIOC)
   *
   * @details
   * Connects pin to GPTW (General PWM Timer) complementary PWM output
   * with dead-time insertion for motor control. GPTW provides 4 channels
   * (GPTW0-GPTW3) with phase-staggered PWM outputs.
   *
   * @par Typical Pins:
   * - P2.3, P1.7 (GPTW0 - Motor 0 IN2/IN1)
   * - P2.2, PC.3 (GPTW1 - Motor 1 IN2/IN1)
   * - PE.3, P8.6 (GPTW2 - Motor 2 IN2/IN1)
   * - PE.7, PC.6 (GPTW3 - Motor 3 IN2/IN1)
   *
   * @par Application: Motor control with direction/PWM signals for DRV8263H
   * @par Feature: 90-degree phase staggering reduces peak current draw
   *
   * @since Version 1.0.0
   */
  k_psel_gptw = 0x1E, /* 0b011110: per RX72N HW manual Tables 23.4, 23.6,
                       * 23.10, 23.14, 23.16 -- the GTIOC* function on every
                       * port (1, 2, 8, A, C, D, E) uses PSEL=0x1E. The old
                       * value 0x14 selected an unrelated function. */

} rx_pin_psel_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Configure pin for GPIO (General Purpose I/O) function
 *
 * @details
 * Sets the pin to GPIO mode by writing 0x00 to its PFS register (PSEL=0,
 * ISEL=0, ASEL=0). This disconnects the pin from all peripheral functions
 * and allows it to be controlled by PORT PDR/PODR registers.
 *
 * @par Algorithm Steps
 * 1. Extract port number and pin number from encoded pin parameter
 * 2. Validate port is in valid range [0, J]
 * 3. Validate pin number is in valid range [0, 7]
 * 4. Calculate PFS register address for this pin
 * 5. Unlock PWPR write protection (B0WI=0, PFSWE=1)
 * 6. Write 0x00 to PFS register (GPIO mode)
 * 7. Lock PWPR write protection (PFSWE=0, B0WI=1)
 * 8. Return success
 *
 * @param[in] pin GPIO pin identifier (rx_port_pin_t from rx_port_constants.h)
 *                - Encodes both port (A-J) and pin number (0-7)
 *                - Use k_port_X_pin_Y constants for type safety
 *                - Example: k_port_e_pin_5 for PE5
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for GPIO mode
 * @retval k_rx_err_invalid_arg Port number exceeds valid range (> Port J)
 * @retval k_rx_err_invalid_arg Pin number exceeds 7
 * @retval k_rx_err_invalid_arg Port not available on package (G/H on 144-pin)
 *
 * @pre PCLKB clock must be running (MPC register access requires it)
 * @pre MPC module stop bit must be cleared (MSTPCRA.MSTPA9 = 0)
 *
 * @post PFS register for specified pin contains 0x00
 * @post Pin is disconnected from all peripheral functions
 * @post PWPR is locked (B0WI=1) after operation
 *
 * @invariant MPC base address 0x0008C100 remains constant
 * @invariant Other pins' PFS registers are not modified
 *
 * @note Not thread-safe. Use mutex protection if calling from multiple threads.
 * @note This only sets PFS register. To complete GPIO setup, also configure
 *       PORT PMR (peripheral mode = 0) and PDR (direction) registers.
 *
 * @warning Calling on a pin that doesn't exist on the package returns error
 *          but does not cause hardware fault.
 *
 * @par Performance
 * - Execution time: ~1 us @ 240 MHz (PWPR unlock/lock overhead)
 * - No loops or blocking operations
 *
 * @par Thread Safety
 * Not thread-safe. The PWPR unlock/lock sequence is non-atomic.
 *
 * @par Example - Single Pin
 * @code{.c}
 * #include "rx_mpc.h"
 * #include "rx_port_constants.h"
 *
 * // Configure PE5 for GPIO (was previously PWM output)
 * rx_err_t err = rx_mpc_set_gpio(k_port_e_pin_5);
 * if (err != k_rx_ok) {
 *     rx_log_error("INIT", "Failed to set PE5 to GPIO mode");
 *     return err;
 * }
 *
 * // Now configure PORT registers for GPIO operation
 * // (set PMR=0, configure PDR for direction, etc.)
 * @endcode
 *
 * @par Example - Error Handling
 * @code{.c}
 * // Attempt to configure invalid pin
 * rx_err_t err = rx_mpc_set_gpio(k_port_g_pin_0);  // Port G not on 144-pin
 * if (err == k_rx_err_invalid_arg) {
 *     rx_log_warn("MPC", "Pin not available on this package");
 * }
 * @endcode
 *
 * @see rx_mpc_set_peripheral() Configure pin for peripheral function
 * @see rx_port_constants.h Pin enumeration definitions
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 5: [OK] 2 preconditions (clock, module stop), 3 postconditions
 * - Rule 7: [OK] All internal function returns checked
 *
 * @callgraph
 * @callergraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_gpio(rx_port_pin_t pin);

/**
 * @brief Configure pin for peripheral function with specified PSEL code
 *
 * @details
 * Core function for pin multiplexer configuration. Writes the specified
 * PSEL value to the pin's PFS register, connecting the pin to a peripheral
 * function. Uses a configuration structure to prevent parameter swapping.
 *
 * @par Algorithm Steps
 * 1. Validate config pointer is not nullptr
 * 2. Validate PSEL value is in range [0, 31]
 * 3. Extract port number and pin number from config->pin
 * 4. Validate extracted port and pin are in valid ranges
 * 5. Calculate PFS register address for this pin
 * 6. Unlock PWPR write protection (B0WI=0, PFSWE=1)
 * 7. Write config->psel to PFS register
 * 8. Lock PWPR write protection (PFSWE=0, B0WI=1)
 * 9. Return success
 *
 * @par Pin Configuration Flow
 * @dot
 * digraph config_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="rx_mpc_set_peripheral()"];
 *   validate_ptr [label="Validate config != nullptr"];
 *   validate_psel [label="Validate PSEL <= 31"];
 *   extract [label="Extract port, pin"];
 *   validate_pin [label="Validate port, pin range"];
 *   get_pfs [label="Get PFS register address"];
 *   unlock [label="Unlock PWPR"];
 *   write [label="Write PSEL to PFS"];
 *   lock [label="Lock PWPR"];
 *   success [label="Return k_rx_ok", fillcolor=lightgreen, style=filled];
 *   error [label="Return k_rx_err_invalid_arg", fillcolor=lightcoral, style=filled];
 *
 *   start -> validate_ptr;
 *   validate_ptr -> validate_psel [label="OK"];
 *   validate_ptr -> error [label="NULL"];
 *   validate_psel -> extract [label="OK"];
 *   validate_psel -> error [label="> 31"];
 *   extract -> validate_pin;
 *   validate_pin -> get_pfs [label="OK"];
 *   validate_pin -> error [label="invalid"];
 *   get_pfs -> unlock;
 *   unlock -> write;
 *   write -> lock;
 *   lock -> success;
 * }
 * @enddot
 *
 * @param[in] config Pointer to MPC peripheral configuration structure
 *                   - config->pin: Target pin (rx_port_pin_t constant)
 *                   - config->psel: PSEL value to write (0-31)
 *                   - Must not be nullptr
 *                   - Must remain valid only during function call
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for peripheral function
 * @retval k_rx_err_invalid_arg config pointer is nullptr
 * @retval k_rx_err_invalid_arg config->psel exceeds 31 (5-bit field max)
 * @retval k_rx_err_invalid_arg Port number exceeds valid range
 * @retval k_rx_err_invalid_arg Pin number exceeds 7
 * @retval k_rx_err_invalid_arg Port not available on package
 *
 * @pre config pointer must be non-NULL
 * @pre config->psel must be in range [0, 31]
 * @pre config->pin must be a valid rx_port_pin_t value
 * @pre PCLKB clock must be running
 *
 * @post PFS register for specified pin contains config->psel value
 * @post Pin is connected to peripheral function specified by PSEL
 * @post PWPR is locked (B0WI=1) after operation
 *
 * @invariant config structure is not modified (const pointer)
 * @invariant Other pins' PFS registers are not modified
 *
 * @note Not thread-safe. Use mutex protection if calling from multiple threads.
 * @note This only sets PFS register. For peripheral operation, also set
 *       PORT PMR bit to 1 to enable peripheral mode.
 * @note PSEL values are pin-specific. Verify correct value in hardware manual.
 *
 * @warning Wrong PSEL value may connect pin to unexpected peripheral!
 * @warning Not all PSEL values are valid for all pins.
 *
 * @par Performance
 * - Execution time: ~1 us @ 240 MHz
 * - No loops or blocking operations
 *
 * @par Thread Safety
 * Not thread-safe. The PWPR unlock/lock sequence is non-atomic.
 *
 * @par Example - Motor PWM Configuration
 * @code{.c}
 * #include "rx_mpc.h"
 * #include "rx_port_constants.h"
 *
 * // Configure PE5 for GPTW0A (Motor 0 PWM Phase+)
 * rx_mpc_peripheral_config_t pwm_config = {
 *     .pin  = k_port_e_pin_5,  // PE5
 *     .psel = k_psel_gptw      // GPTW function (0x1E)
 * };
 *
 * rx_err_t err = rx_mpc_set_peripheral(&pwm_config);
 * if (err != k_rx_ok) {
 *     rx_log_error("MOTOR", "PWM pin configuration failed");
 *     return err;
 * }
 *
 * // Don't forget: Set PORT PMR bit to enable peripheral mode!
 * @endcode
 *
 * @par Example - Batch Configuration
 * @code{.c}
 * // Configure multiple pins in a loop
 * const rx_mpc_peripheral_config_t pins[] = {
 *     { k_port_e_pin_5, k_psel_gptw },  // GPTW0A (0x1E)
 *     { k_port_e_pin_4, k_psel_gptw },  // GPTW0B (0x1E)
 *     { k_port_2_pin_4, 0x02 },  // MTCLKA
 *     { k_port_2_pin_5, 0x02 },  // MTCLKB
 * };
 *
 * for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
 *     rx_err_t err = rx_mpc_set_peripheral(&pins[i]);
 *     if (err != k_rx_ok) {
 *         rx_log_error("MPC", "Pin config failed");
 *         return err;
 *     }
 * }
 * @endcode
 *
 * @see rx_mpc_set_gpio() Configure pin for GPIO mode
 * @see rx_mpc_peripheral_config_t Configuration structure definition
 * @see rx_pin_psel_t Common PSEL value constants
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 5: [OK] 4 preconditions, 3 postconditions
 * - Rule 7: [OK] All return values checked
 *
 * @callgraph
 * @callergraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_peripheral(const rx_mpc_peripheral_config_t* config);

/**
 * @brief Configure pin for MTU3a timer PWM output (MTIOC)
 *
 * @details
 * Convenience function that configures a pin for MTU3a I/O compare output,
 * used for PWM generation. Internally calls rx_mpc_set_peripheral() with
 * PSEL = k_psel_mtu_ioc (0x01).
 *
 * @par Common MTU PWM Pins
 * | Pin | MTU Channel | Output | Typical Use |
 * |-----|-------------|--------|-------------|
 * | P14 | MTU3 | MTIOC3A | Motor 0 Phase A |
 * | P15 | MTU3 | MTIOC3B | Motor 0 Phase B |
 * | P16 | MTU3 | MTIOC3C | Motor 0 Phase C |
 * | P17 | MTU3 | MTIOC3D | Motor 0 Phase D |
 * | P24 | MTU4 | MTIOC4A | Motor 1 Phase A |
 * | P25 | MTU4 | MTIOC4B | Motor 1 Phase B |
 * | P26 | MTU4 | MTIOC4C | Motor 1 Phase C |
 * | P27 | MTU4 | MTIOC4D | Motor 1 Phase D |
 *
 * @param[in] pin GPIO pin identifier for MTU PWM output
 *                - Must be a pin that supports MTIOC function
 *                - Use k_port_X_pin_Y constants
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for MTU PWM
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must support MTU I/O compare function (check hardware manual)
 * @pre PCLKB clock must be running
 *
 * @post PFS register contains PSEL = 0x01 (MTU IOC)
 * @post Pin ready for MTU3a compare match output
 *
 * @note Also requires MTU3a timer configuration and PORT PMR=1
 * @note STAR project uses GPTW for motor PWM; see rx_mpc_set_gptw() which uses PSEL = k_psel_gptw (0x1E)
 *
 * @par Example
 * @code{.c}
 * // Configure P14-P17 for MTU3 PWM outputs
 * rx_mpc_set_mtu_pwm(k_port_1_pin_4);  // MTIOC3A
 * rx_mpc_set_mtu_pwm(k_port_1_pin_5);  // MTIOC3B
 * rx_mpc_set_mtu_pwm(k_port_1_pin_6);  // MTIOC3C
 * rx_mpc_set_mtu_pwm(k_port_1_pin_7);  // MTIOC3D
 * @endcode
 *
 * @see rx_mpc_set_peripheral() Underlying implementation
 * @see rx_mtu.h MTU timer driver
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_mtu_pwm(rx_port_pin_t pin);

/**
 * @brief Configure pin for MTU3a encoder/phase counter input (MTCLK)
 *
 * @details
 * Configures a pin for MTU3a phase counting mode input, used with
 * quadrature encoders. Sets PSEL = k_psel_mtu_phase (0x03) for
 * phase counting with direction detection.
 *
 * @par Quadrature Encoder Interface
 * @verbatim
 *    Encoder Output          MTU3a Phase Counting
 *    +------------+         +---------------------+
 *    |  Phase A   |-------->| MTCLKA (Count Up)   |
 *    |  Phase B   |-------->| MTCLKB (Count Down) |
 *    |  (Index Z) |-------->| (Optional reset)    |
 *    +------------+         +---------------------+
 *                           Counter increments/decrements
 *                           based on phase relationship
 * @endverbatim
 *
 * @par Common Encoder Pins (STAR Project)
 * | Pin Pair | MTU Input | Encoder |
 * |----------|-----------|---------|
 * | P24/P25 | MTCLKA/MTCLKB | Motor 0 |
 * | PC0/PC1 | MTCLKC/MTCLKD | Motor 1 |
 *
 * @param[in] pin GPIO pin identifier for encoder input
 *                - Must be a pin that supports MTCLK function
 *                - Configure in pairs (A+B) for quadrature
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for encoder input
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must support MTU clock/phase function
 * @pre PCLKB clock must be running
 *
 * @post PFS register contains PSEL = 0x03 (MTU phase)
 * @post Pin ready for quadrature encoder input
 *
 * @note Configure both Phase A and Phase B pins for proper operation
 * @note Also requires MTU3a timer configuration in phase counting mode
 *
 * @par Example - Quadrature Encoder Setup
 * @code{.c}
 * // Configure encoder pins for Motor 0
 * rx_err_t err;
 *
 * err = rx_mpc_set_mtu_encoder(k_port_2_pin_4);  // P24 = Phase A
 * if (err != k_rx_ok) return err;
 *
 * err = rx_mpc_set_mtu_encoder(k_port_2_pin_5);  // P25 = Phase B
 * if (err != k_rx_ok) return err;
 *
 * // Now configure MTU1 for phase counting mode
 * // (see rx_mtu_encoder.h)
 * @endcode
 *
 * @see rx_mpc_set_peripheral() Underlying implementation
 * @see rx_mtu_encoder.h Encoder driver using MTU phase counting
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_mtu_encoder(rx_port_pin_t pin);

/**
 * @brief Configure pin for TPU encoder/phase counter input (TCLK)
 *
 * @details
 * Configures a pin for TPU phase counting mode input, used with
 * quadrature encoders on the rear wheels. Sets PSEL = k_psel_mtu_phase
 * (0x03), which is the generic "timer phase counting input" function
 * on the RX72N. The routing to TPU (vs MTU) depends on which timer
 * module has phase counting mode enabled.
 *
 * This function exists as a separate API from rx_mpc_set_mtu_encoder()
 * for documentation clarity, even though both write the same PSEL value.
 *
 * @par TPU Encoder Pins (STAR Project - Rear Wheels)
 * | Pin Pair | TPU Input | Encoder |
 * |----------|-----------|---------|
 * | PC2/PA3 | TCLKA/TCLKB | Motor 2 (Rear Left) |
 * | PC0/PB3 | TCLKC/TCLKD | Motor 3 (Rear Right) |
 *
 * @param[in] pin GPIO pin identifier for TPU encoder input
 *                - Must be a pin that supports TCLK function
 *                - Configure in pairs (A+B) for quadrature
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for TPU encoder input
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must support TPU clock/phase function
 * @pre PCLKB clock must be running
 *
 * @post PFS register contains PSEL = 0x03 (timer phase counting)
 * @post Pin ready for quadrature encoder input via TPU
 *
 * @note Configure both Phase A and Phase B pins for proper operation
 * @note Also requires TPU timer configuration in phase counting mode
 *
 * @par Example - Rear Wheel Encoder Setup
 * @code{.c}
 * rx_err_t err;
 *
 * // Motor 2 (Rear Left) - TPU1
 * err = rx_mpc_set_tpu_encoder(k_rx_pc_2);  // PC2 = TCLKA (Phase A)
 * if (err != k_rx_ok) return err;
 * err = rx_mpc_set_tpu_encoder(k_rx_pa_3);  // PA3 = TCLKB (Phase B)
 * if (err != k_rx_ok) return err;
 *
 * // Motor 3 (Rear Right) - TPU2
 * err = rx_mpc_set_tpu_encoder(k_rx_pc_0);  // PC0 = TCLKC (Phase A)
 * if (err != k_rx_ok) return err;
 * err = rx_mpc_set_tpu_encoder(k_rx_pb_3);  // PB3 = TCLKD (Phase B)
 * if (err != k_rx_ok) return err;
 * @endcode
 *
 * @see rx_mpc_set_mtu_encoder() Front wheel encoder pin configuration
 * @see rx_mpc_set_peripheral() Underlying implementation
 * @see rx_encoder_tpu.h TPU encoder driver using phase counting
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_tpu_encoder(rx_port_pin_t pin);

/**
 * @brief Configure pin for ADC analog input
 *
 * @details
 * Configures a pin for analog-to-digital converter input by setting
 * the ASEL bit in the PFS register. This disables the digital input
 * buffer to prevent noise and current leakage during analog measurement.
 *
 * @par ADC Pin Configuration
 * @verbatim
 *    PFS Register for ADC:
 *    +------+------+------+-----------------+
 *    | ASEL | ISEL | Rsvd |    PSEL[4:0]    |
 *    |  1   |  0   |  0   |     0x00        |
 *    +------+------+------+-----------------+
 *       ^
 *       +-- Set to 1 for analog input
 * @endverbatim
 *
 * @par Common ADC Pins (STAR Project)
 * | Pin | ADC Channel | Typical Use |
 * |-----|-------------|-------------|
 * | P40 | AN000 | Motor 0 current sense |
 * | P41 | AN001 | Motor 1 current sense |
 * | P42 | AN002 | Motor 2 current sense |
 * | P43 | AN003 | Motor 3 current sense |
 * | P44 | AN004 | Analog input (unused) |
 * | P45 | AN005 | Temperature sensor |
 *
 * @param[in] pin GPIO pin identifier for ADC input
 *                - Must be a pin connected to ADC channel
 *                - Typically P40-P47 for AN000-AN007
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for ADC input
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must be connected to ADC channel (not all pins support ADC)
 * @pre PCLKB clock must be running
 *
 * @post PFS register has ASEL = 1 (digital buffer disabled)
 * @post Pin ready for analog signal input
 *
 * @note Digital input buffer disabled - do not use as GPIO input
 * @note Also requires S12ADC configuration for actual conversion
 *
 * @warning Using a non-ADC pin with this function sets ASEL but has no effect
 *          on analog capability (pin hardware determines ADC availability)
 *
 * @par Example - Current Sensing Setup
 * @code{.c}
 * // Configure ADC pins for motor current sensing
 * rx_mpc_set_adc(k_port_4_pin_0);  // AN000 = Motor 0 current
 * rx_mpc_set_adc(k_port_4_pin_1);  // AN001 = Motor 1 current
 * rx_mpc_set_adc(k_port_4_pin_2);  // AN002 = Motor 2 current
 * rx_mpc_set_adc(k_port_4_pin_3);  // AN003 = Motor 3 current
 *
 * // Now configure S12ADC for conversion
 * // (see adc.h)
 * @endcode
 *
 * @see rx72n_adc_regs.h ADC register definitions
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_adc(rx_port_pin_t pin);

/**
 * @brief Configure pin for SCI (Serial Communication Interface) UART function
 *
 * @details
 * Configures a pin for SCI UART operation by setting PSEL = 0x0A.
 * Works for both TXD (transmit) and RXD (receive) pins - the same
 * PSEL value is used, with the specific function determined by the
 * pin's hardware connection.
 *
 * @par STAR Project UART Configuration (Debug Console)
 * | Pin | SCI Channel | Function | Notes |
 * |-----|-------------|----------|-------|
 * | PB7 | SCI9 | TXD9 | Debug output |
 * | PB6 | SCI9 | RXD9 | Debug input |
 *
 * @param[in] pin GPIO pin identifier for SCI function
 *                - Must be a pin that supports SCI TXD/RXD
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for SCI UART
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must support SCI function (check hardware manual)
 * @pre PCLKB clock must be running
 *
 * @post PFS register contains PSEL = 0x0A
 * @post Pin ready for UART operation
 *
 * @note Configure both TXD and RXD pins for bidirectional UART
 * @note Also requires SCI channel configuration and PORT PMR=1
 *
 * @par Example - Debug UART Setup
 * @code{.c}
 * // Configure debug UART pins (SCI9)
 * rx_mpc_set_sci(k_port_b_pin_7);  // PB7 = TXD9
 * rx_mpc_set_sci(k_port_b_pin_6);  // PB6 = RXD9
 *
 * // Now configure SCI9 for UART mode
 * // (see uart.h)
 * @endcode
 *
 * @see rx72n_sci_regs.h SCI register definitions
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_sci(rx_port_pin_t pin);

/**
 * @brief Configure pin for RIIC (I2C) bus function
 *
 * @details
 * Configures a pin for RIIC (Renesas I2C) operation by setting PSEL = 0x0F.
 * Works for both SCL (clock) and SDA (data) pins. RIIC hardware automatically
 * configures the open-drain output when the channel is enabled.
 *
 * @par I2C Bus Pins
 * | Pin | RIIC Channel | Function | Notes |
 * |-----|--------------|----------|-------|
 * | P12 | RIIC0 | SCL0 | Clock line |
 * | P13 | RIIC0 | SDA0 | Data line |
 *
 * @param[in] pin GPIO pin identifier for RIIC function
 *                - Must be a pin that supports RIIC SCL/SDA
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for RIIC I2C
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must support RIIC function
 * @pre PCLKB clock must be running
 *
 * @post PFS register contains PSEL = 0x0F
 * @post Pin ready for I2C operation
 *
 * @note Configure both SCL and SDA pins
 * @note External pull-up resistors required (typically 4.7kOhm)
 * @note Also requires RIIC channel configuration
 *
 * @par Example - I2C Bus Setup
 * @code{.c}
 * // Configure I2C pins (RIIC0)
 * rx_mpc_set_riic(k_port_1_pin_2);  // P12 = SCL0
 * rx_mpc_set_riic(k_port_1_pin_3);  // P13 = SDA0
 *
 * // Now configure RIIC0
 * // (see riic.h)
 * @endcode
 *
 * @see rx72n_riic_regs.h RIIC register definitions
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_riic(rx_port_pin_t pin);

/**
 * @brief Configure pin for RSPI (SPI) bus function
 *
 * @details
 * Configures a pin for RSPI (Renesas SPI) operation by setting PSEL = 0x0D.
 * Works for all SPI signals: RSPCK (clock), COPI (data out),
 * CIPO (data in), and SSLn (chip select).
 *
 * @par STAR Project SPI Configuration (RPi5 Communication)
 * **Note:** RSPI0 Signal names (MOSIA/MISOA) are RX72N datasheet hardware names.
 * Function names (COPI/CIPO) are project terminology per OSHWA standards.
 *
 * | Pin | RSPI0 Signal | Function | Description |
 * |-----|--------------|----------|-------------|
 * | PA0 | MOSIA | COPI | Controller Out, Peripheral In |
 * | PA1 | MISOA | CIPO | Controller In, Peripheral Out |
 * | PA3 | RSPCKA | CLK | SPI clock |
 * | PA4 | SSLA0 | CS | Chip select (active low) |
 *
 * @par SPI Bus Topology
 * @verbatim
 *    RX72N (Controller)              RPi5 (Controller)
 *    +-----------------+            +-----------------+
 *    | RSPI0           |            | SPI             |
 *    |  PA0 (COPI) ----+------------+-> CIPO          |
 *    |  PA1 (CIPO) <---+------------+-- COPI          |
 *    |  PA3 (CLK)  <---+------------+-- CLK           |
 *    |  PA4 (CS)   <---+------------+-- CS            |
 *    +-----------------+            +-----------------+
 *    Note: RX72N is SPI peripheral, RPi5 is controller
 * @endverbatim
 *
 * @param[in] pin GPIO pin identifier for RSPI function
 *                - Must be a pin that supports RSPI signals
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for RSPI SPI
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 *
 * @pre Pin must support RSPI function
 * @pre PCLKB clock must be running
 *
 * @post PFS register contains PSEL = 0x0D
 * @post Pin ready for SPI operation
 *
 * @note Configure all 4 SPI pins (CLK, COPI, CIPO, CS)
 * @note Also requires RSPI channel configuration
 * @note Uses OSHWA-approved inclusive terminology (COPI/CIPO)
 *
 * @par Example - SPI Bus Setup
 * @code{.c}
 * // Configure SPI pins for RPi5 communication (RSPI0)
 * rx_mpc_set_rspi(k_port_a_pin_0);  // PA0 = COPI
 * rx_mpc_set_rspi(k_port_a_pin_1);  // PA1 = CIPO
 * rx_mpc_set_rspi(k_port_a_pin_3);  // PA3 = CLK
 * rx_mpc_set_rspi(k_port_a_pin_4);  // PA4 = SSL0 (CS)
 *
 * // Now configure RSPI0
 * // (see rspi.h)
 * @endcode
 *
 * @see rx72n_rspi_regs.h RSPI register definitions
 * @see rx_spi_comm.h SPI communication driver
 *
 * @since Version 1.0.0
 * @callgraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_rspi(rx_port_pin_t pin);

/**
 * @brief Configure pin for GPTW (General PWM Timer) complementary output
 *
 * @details
 * Configures a pin for GPTW operation by setting PSEL = 0x1E.
 * GPTW provides complementary PWM output with dead-time insertion
 * for motor control applications.
 *
 * GPTW channels support:
 * - 20 kHz PWM frequency (ultrasonic, inaudible)
 * - 1 us dead-time (prevents DRV8263H shoot-through)
 * - 90-degree phase staggering (reduces peak current)
 * - IN2/IN1 mode (direction and PWM signals per DRV8263H)
 *
 * @par STAR Project GPTW Pin Allocation
 * | Pin  | GPTW Ch | Signal | Motor | Description          |
 * |------|---------|--------|-------|----------------------|
 * | P2.3 | GPTW0   | GTIOC0A | 0    | Motor 0 Direction (IN2) |
 * | P1.7 | GPTW0   | GTIOC0B | 0    | Motor 0 PWM (IN1)      |
 * | P2.2 | GPTW1   | GTIOC1A | 1    | Motor 1 Direction (IN2) |
 * | PC.3 | GPTW1   | GTETRGC | 1    | Motor 1 PWM (IN1) - GTETRGC at 0x1E on PC3 per Table 23.19 |
 * | PE.3 | GPTW2   | GTIOC2A | 2    | Motor 2 Direction (IN2) |
 * | P8.6 | GPTW2   | GTIOC2B | 2    | Motor 2 PWM (IN1)      |
 * | PE.7 | GPTW3   | GTIOC3A | 3    | Motor 3 Direction (IN2) |
 * | PC.6 | GPTW3   | GTIOC3A | 3    | Motor 3 PWM (IN1) - GTIOC3A at 0x1E on PC6 per Table 23.19 |
 *
 * @param[in] pin GPIO pin identifier for GPTW function
 *                Must be a pin that supports GPTW output (see manual)
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for GPTW
 * @retval k_rx_err_null_ptr Config pointer is NULL (should never happen)
 * @retval k_rx_err_invalid_arg Invalid port or pin number (>= 16)
 * @retval k_rx_err_hw_error PWPR write-protect unlock failed
 *
 * @pre Pin must support GPTW function (check RX72N hardware manual)
 * @pre PCLKB clock must be running (MPC registers require clock)
 * @pre Must be called during single-threaded initialization
 *
 * @post PFS register configured with PSEL = 0x1E
 * @post PWPR register locked (write protection re-enabled)
 * @post Pin configured for GPTW PSEL; caller MUST subsequently set the
 *       corresponding PORTn.PMR bit (this function does NOT touch PMR)
 *       before GPTW can drive the pin.
 *
 * @note Thread Safety: Not thread-safe. Call during initialization only.
 * @note Configure both direction (A/IN2) and PWM (B/IN1) pins for each motor
 * @note GPTW channel configuration required for actual PWM operation
 *
 * @warning Motor safety: Configure pins before enabling GPTW channels
 *
 * @par Example - Configure All Motor PWM Pins
 * @code{.c}
 * // Configure 8 GPTW pins (4 motors x 2 pins = IN2 + IN1)
 * const rx_port_pin_t gptw_pins[] = {
 *     k_rx_p2_3, k_rx_p1_7,  // Motor 0 IN2/IN1
 *     k_rx_p2_2, k_rx_pc_3,  // Motor 1 IN2/IN1
 *     k_rx_pe_3, k_rx_p8_6,  // Motor 2 IN2/IN1
 *     k_rx_pe_7, k_rx_pc_6   // Motor 3 IN2/IN1
 * };
 *
 * for (uint8_t i = 0; i < 8; i++) {
 *     rx_err_t err = rx_mpc_set_gptw(gptw_pins[i]);
 *     if (err != k_rx_ok) {
 *         rx_log_error("GPIO", "GPTW pin config failed");
 *         return err;
 *     }
 * }
 *
 * // Now safe to initialize GPTW channels
 * rx_gptw_init_all_staggered(&config);
 * @endcode
 *
 * @see rx_mpc_set_peripheral() Core pin configuration function
 * @see rx_gptw_init_all_staggered() GPTW timer initialization
 * @see RX72N Manual Chapter 23 - Multi-Function Pin Controller
 * @see RX72N Manual Chapter 29 - General PWM Timer
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, recursion (delegates to rx_mpc_set_peripheral)
 * - Rule 3: [OK] No dynamic allocation (stack-only config struct)
 * - Rule 4: [OK] Function is 6 lines (well under 60 line limit)
 * - Rule 7: [OK] Return value from rx_mpc_set_peripheral checked by caller
 */
[[nodiscard]] rx_err_t rx_mpc_set_gptw(rx_port_pin_t pin);

/**
 * @brief Configure pin for USB VBUS detection function
 *
 * @details
 * Configures a pin for USB VBUS detection by setting PSEL = 0x11.
 * VBUS detection is used for USB enumeration and power presence sensing.
 *
 * The VBUS detect pin monitors the 5V power rail on the USB bus:
 * - Pin reads HIGH (1) when USB cable connected and 5V present
 * - Pin reads LOW (0) when USB cable disconnected or unpowered
 *
 * Required for USB CDC communication with RPi5 host.
 *
 * @par STAR Project USB Configuration
 * | Pin  | Function   | Description                    |
 * |------|------------|--------------------------------|
 * | P1.6 | USB0_VBUS  | VBUS detect (5V presence)      |
 *
 * @param[in] pin GPIO pin identifier for USB VBUS detect
 *                Typically P1.6 for USB0_VBUS on RX72N
 *
 * @return Error code indicating success or failure
 * @retval k_rx_ok Pin successfully configured for USB VBUS
 * @retval k_rx_err_null_ptr Config pointer is NULL (should never happen)
 * @retval k_rx_err_invalid_arg Invalid port or pin number
 * @retval k_rx_err_hw_error PWPR write-protect unlock failed
 *
 * @pre Pin must support USB VBUS function (P1.6 on RX72N)
 * @pre PCLKB clock must be running
 * @pre Must be called during single-threaded initialization
 *
 * @post PFS register configured with PSEL = 0x11
 * @post PWPR register locked
 * @post Pin configured for USB VBUS PSEL; caller MUST subsequently set
 *       the corresponding PORTn.PMR bit (this function does NOT touch
 *       PMR) before the USB peripheral sees the VBUS signal.
 *
 * @note Thread Safety: Not thread-safe. Call during initialization only.
 * @note Active-high detection (pin = 1 when 5V present)
 * @note Also requires USB PHY and USB0 controller configuration
 *
 * @par Example - USB VBUS Setup
 * @code{.c}
 * // Configure USB VBUS detect pin (P1.6)
 * rx_err_t err = rx_mpc_set_usb_vbus(k_rx_p1_6);
 * if (err != k_rx_ok) {
 *     rx_log_error("USB", "VBUS pin configuration failed");
 *     return err;
 * }
 *
 * // Now configure USB0 controller
 * err = usb_init(USB_MODE_CDC);
 * @endcode
 *
 * @see rx_mpc_set_peripheral() Core pin configuration function
 * @see rx_usb.h USB controller driver
 * @see RX72N Manual Chapter 23 - Multi-Function Pin Controller
 * @see RX72N Manual Chapter 31 - USB Controller
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_mpc_set_usb_vbus(rx_port_pin_t pin);

/**
 * @brief Configure pin for IRQ (external interrupt) function
 *
 * @details
 * Sets pin function to IRQ mode via MPC (Multi-Function Pin Controller).
 * This enables hardware edge detection on IRQ0-IRQ15 pins for use with
 * the Interrupt Controller Unit (ICU).
 *
 * **PFS Register for IRQ Function:**
 * - IRQ pins: ISEL bit (bit 6) = 0x40 written directly to PFS register
 * - This is NOT a PSEL value; rx_mpc_set_peripheral() is NOT used
 * - Verify in RX72N Manual Section 20.3 (MPC), PFS register layout
 *
 * **Supported Pins:**
 * - P00-P07: IRQ8-IRQ15
 * - Other IRQ pins per RX72N Manual Table 20.7
 *
 * @par STAR Project IRQ Usage (HC-SR04 Ultrasonic Sensors)
 * | Pin  | IRQ Number | Sensor   | Location     |
 * |------|------------|----------|--------------|
 * | P03  | IRQ11      | Sonar 0  | Front-Left   |
 * | P02  | IRQ10      | Sonar 1  | Front-Right  |
 * | P01  | IRQ9       | Sonar 2  | Back-Left    |
 * | P00  | IRQ8       | Sonar 3  | Back-Right   |
 *
 * @param[in] pin GPIO pin identifier (must support IRQ function)
 *                Use k_rx_p0_X constants for IRQ8-15
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Pin configured for IRQ function
 * @retval k_rx_err_invalid_arg Invalid port or pin number (propagated from internal_write_pfs())
 *
 * @note internal_mpc_unlock() and internal_mpc_lock() do not propagate errors; only
 *       internal_write_pfs() can return k_rx_err_invalid_arg on invalid port/pin values.
 *
 * @pre Pin must have IRQ multiplexing capability (P00-P07 for IRQ8-15)
 * @pre PCLKB running (MPC requires clock)
 * @pre Must be called during single-threaded initialization
 *
 * @post Pin ISEL bit set (0x40) in PFS register (IRQ input mode)
 * @post PSEL field remains 0 (no conflicting peripheral function)
 * @post PWPR register locked
 * @post Pin ready for ICU edge detection configuration
 *
 * @note Thread Safety: Not thread-safe. Call during initialization only.
 * @note Call this BEFORE enabling ICU interrupt
 *
 * @warning Does NOT configure ICU registers (edge detect, priority, etc.)
 * @warning Configure ICU separately for edge detection and priority as required
 *
 * @par Example - IRQ Pin Setup
 * @code{.c}
 * // Configure P03 for IRQ11 (external interrupt input)
 * rx_err_t err = rx_mpc_set_irq(k_rx_p0_3);
 * if (err != k_rx_ok) {
 *     rx_log_error("IRQ", "IRQ pin configuration failed");
 *     return err;
 * }
 *
 * // Now configure ICU for edge detection (caller-specific step)
 * @endcode
 *
 * @invariant MPC base address remains constant throughout operation
 * @invariant Only target pin's PFS register is modified (other pins' PFS unchanged)
 *
 * @see rx_mpc_set_gpio() Set pin back to GPIO mode
 * @see RX72N Manual Section 20.3 (MPC), PFS register bit definitions
 * @see RX72N Manual Chapter 15 (ICU - Interrupt Controller Unit)
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 5: [OK] 3 preconditions, 4 postconditions
 * - Rule 7: [OK] Caller checks return value ([[nodiscard]])
 * - Rule 8: [OK] k_pfs_isel from pfs_bits_t enum (no magic numbers)
 *
 * @callgraph
 * @callergraph
 */
[[nodiscard]] rx_err_t rx_mpc_set_irq(rx_port_pin_t pin);

#ifdef __cplusplus
}
#endif
