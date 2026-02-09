/* lib/rx_hal/inc/rx_mpc.h */

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
 *   ┌─────────────────────────────────────────────────────────────────────┐
 *   │                         Application Layer                          │
 *   │   ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  │
 *   │   │ Motor   │  │ Encoder │  │  UART   │  │   SPI   │  │   ADC   │  │
 *   │   │ Control │  │ Driver  │  │ Debug   │  │  Comm   │  │ Sensing │  │
 *   │   └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  │
 *   └────────┼───────────┼───────────┼───────────┼───────────┼───────────┘
 *            │           │           │           │           │
 *   ┌────────▼───────────▼───────────▼───────────▼───────────▼───────────┐
 *   │                         rx_mpc.h API                               │
 *   │   ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐  │
 *   │   │rx_mpc_set_  │ │rx_mpc_set_  │ │rx_mpc_set_  │ │rx_mpc_set_  │  │
 *   │   │  mtu_pwm()  │ │  sci()      │ │  rspi()     │ │  adc()      │  │
 *   │   └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘  │
 *   │          │               │               │               │         │
 *   │   ┌──────▼───────────────▼───────────────▼───────────────▼──────┐  │
 *   │   │              rx_mpc_set_peripheral()                        │  │
 *   │   │           (core configuration function)                     │  │
 *   │   └─────────────────────────┬───────────────────────────────────┘  │
 *   └─────────────────────────────┼──────────────────────────────────────┘
 *                                 │
 *   ┌─────────────────────────────▼──────────────────────────────────────┐
 *   │                       rx72n_mpc_regs.h                             │
 *   │   ┌────────────┐      ┌────────────┐      ┌────────────────────┐   │
 *   │   │   PWPR     │      │  PFS Regs  │      │   mpc() accessor   │   │
 *   │   │ (Protect)  │─────►│ P00PFS-    │◄─────│   (0x0008C100)     │   │
 *   │   │            │      │ PJ5PFS     │      │                    │   │
 *   │   └────────────┘      └────────────┘      └────────────────────┘   │
 *   └────────────────────────────────────────────────────────────────────┘
 *                                 │
 *   ┌─────────────────────────────▼──────────────────────────────────────┐
 *   │                      Physical Pins                                 │
 *   │   PE5 (PWM+) ────► GPTW0A     PB7 (TXD9) ────► SCI9-TX            │
 *   │   PE4 (PWM-) ────► GPTW0B     PB6 (RXD9) ────► SCI9-RX            │
 *   │   P24 (ENC-A) ───► MTCLKA     PA0-PA4 ───────► RSPI0              │
 *   │   P25 (ENC-B) ───► MTCLKB     P40-P47 ───────► ADC (AN000-007)    │
 *   └────────────────────────────────────────────────────────────────────┘
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
 * @par STAR Project Pin Assignments (100-pin LFQFP)
 * | Package Pin | Port.Pin | Function | PSEL | Driver Call |
 * |-------------|----------|----------|------|-------------|
 * | 97 | PE5 | GPTW0A (Motor0 PWM+) | 0x07 | rx_mpc_set_peripheral() |
 * | 96 | PE4 | GPTW0B (Motor0 PWM-) | 0x07 | rx_mpc_set_peripheral() |
 * | 60 | P24 | MTCLKA (Encoder0 A) | 0x02 | rx_mpc_set_mtu_encoder() |
 * | 61 | P25 | MTCLKB (Encoder0 B) | 0x02 | rx_mpc_set_mtu_encoder() |
 * | 72 | PB7 | SCI9-TXD (Debug TX) | 0x0A | rx_mpc_set_sci() |
 * | 74 | PB6 | SCI9-RXD (Debug RX) | 0x0A | rx_mpc_set_sci() |
 * | 32 | PA0 | RSPI0-COPI | 0x0D | rx_mpc_set_rspi() |
 * | 33 | PA1 | RSPI0-CIPO | 0x0D | rx_mpc_set_rspi() |
 * | 35 | PA3 | RSPI0-CLK | 0x0D | rx_mpc_set_rspi() |
 * | 34 | PA4 | RSPI0-SSL0 | 0x0D | rx_mpc_set_rspi() |
 * | USB | - | USB D+/D- | - | (Dedicated USB pins) |
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
 * | Pin config | ~1 µs | PWPR unlock/lock overhead |
 * | Bulk config (10 pins) | ~10 µs | Sequential calls |
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
 * 3. **MPC pin configuration** ◄ This module
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
 *         .psel = 0x07             // GPTW function
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
 * - **Rule 1**: ✓ No goto, setjmp, longjmp, or recursion
 * - **Rule 2**: ✓ All loops have fixed upper bounds (switch/case only)
 * - **Rule 3**: ✓ No dynamic memory allocation
 * - **Rule 4**: ✓ All functions < 60 lines
 * - **Rule 5**: ✓ Minimum 2 assertions per function (parameter validation)
 * - **Rule 6**: ✓ Variables declared at smallest scope
 * - **Rule 7**: ✓ All return values checked
 * - **Rule 8**: ✓ C23 typed enums for all constants, no magic numbers
 * - **Rule 9**: ⚠️ Function pointers not used (simple direct calls)
 * - **Rule 10**: ✓ Compiles with -Wall -Wextra -Werror
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
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#ifndef STAR_RX72N_MPC_H
#define STAR_RX72N_MPC_H

#include <stdbool.h>
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
 *     .psel = 0x07             // GPTW function select
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
   * @note On 100-pin package, ports F, G, H are not available
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
   * | 0x07 | GPTW output (GTIOC) | PE4, PE5 |
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
 *        ┌──────┬──────┬──────┬─────────────────┐
 *        │ ASEL │ ISEL │ Rsvd │    PSEL[4:0]    │
 *        └──────┴──────┴──────┴─────────────────┘
 *                              ◄── These values ──►
 * @endverbatim
 *
 * @par PSEL Values by Peripheral (STAR Project)
 * | Peripheral | PSEL | Pins Used | Function |
 * |------------|------|-----------|----------|
 * | MTU3a MTIOC | 0x01 | P14-P17, P24-P27 | PWM output |
 * | MTU3a MTCLK | 0x02 | P24, P25, PC0, PC1 | Clock/encoder input |
 * | MTU3a Phase | 0x03 | P24, P25 | Phase counting mode |
 * | GPTW GTIOC | 0x07 | PE4, PE5 | PWM output |
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
   * @brief MTU clock input (MTCLK)
   * @details
   * Connects pin to MTU3a external clock input for counting external pulses.
   * @par Typical Pins: P24 (MTCLKA), P25 (MTCLKB), PC0 (MTCLKC), PC1 (MTCLKD)
   * @par Application: Encoder pulse counting (count mode)
   */
  k_psel_mtu_clk = 0x02,

  /**
   * @brief MTU encoder phase counting input
   * @details
   * Connects pin to MTU3a phase counting mode inputs for quadrature encoders.
   * Uses pairs of pins (A/B) for direction-aware counting.
   * @par Typical Pins: P24/P25 pair, PC0/PC1 pair
   * @par Application: Quadrature encoder interface with direction detection
   */
  k_psel_mtu_phase = 0x03,

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
   * @par Typical Pins: PA3 (RSPI0-RSPCK)
   * @par Application: SPI bus clock for RPi5 communication
   */
  k_psel_rspi_clk = 0x0D,

  /**
   * @brief RSPI Controller Out Peripheral In (COPI, formerly MOSI)
   * @details
   * Connects pin to RSPI data output line in controller mode.
   * @par Typical Pins: PA0 (RSPI0-MOSIA)
   * @par Application: SPI transmit data to peripherals
   * @note Uses OSHWA-approved inclusive terminology
   */
  k_psel_rspi_copi = 0x0D,

  /**
   * @brief RSPI Controller In Peripheral Out (CIPO, formerly MISO)
   * @details
   * Connects pin to RSPI data input line in controller mode.
   * @par Typical Pins: PA1 (RSPI0-MISOA)
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
   * @par Application: Motor current sensing, battery voltage monitoring
   * @note Use rx_mpc_set_adc() for proper configuration (sets ASEL bit)
   */
  k_psel_adc = 0x00,
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
 * @retval k_rx_err_invalid_arg Port not available on package (F/G/H on 100-pin)
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
 * - Execution time: ~1 µs @ 240 MHz (PWPR unlock/lock overhead)
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
 * rx_err_t err = rx_mpc_set_gpio(k_port_f_pin_0);  // Port F not on 100-pin
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
 * - Rule 5: ✓ 2 preconditions (clock, module stop), 3 postconditions
 * - Rule 7: ✓ All internal function returns checked
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
 * - Execution time: ~1 µs @ 240 MHz
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
 *     .psel = 0x07             // GPTW function
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
 *     { k_port_e_pin_5, 0x07 },  // GPTW0A
 *     { k_port_e_pin_4, 0x07 },  // GPTW0B
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
 * - Rule 5: ✓ 4 preconditions, 3 postconditions
 * - Rule 7: ✓ All return values checked
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
 * @note STAR project typically uses GPTW for PWM (rx_mpc_set_peripheral with 0x07)
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
 *    ┌────────────┐         ┌─────────────────────┐
 *    │  Phase A   │────────►│ MTCLKA (Count Up)   │
 *    │  Phase B   │────────►│ MTCLKB (Count Down) │
 *    │  (Index Z) │────────►│ (Optional reset)    │
 *    └────────────┘         └─────────────────────┘
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
 *    ┌──────┬──────┬──────┬─────────────────┐
 *    │ ASEL │ ISEL │ Rsvd │    PSEL[4:0]    │
 *    │  1   │  0   │  0   │     0x00        │
 *    └──────┴──────┴──────┴─────────────────┘
 *       ▲
 *       └── Set to 1 for analog input
 * @endverbatim
 *
 * @par Common ADC Pins (STAR Project)
 * | Pin | ADC Channel | Typical Use |
 * |-----|-------------|-------------|
 * | P40 | AN000 | Motor 0 current sense |
 * | P41 | AN001 | Motor 1 current sense |
 * | P42 | AN002 | Motor 2 current sense |
 * | P43 | AN003 | Motor 3 current sense |
 * | P44 | AN004 | Battery voltage |
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
 * @note External pull-up resistors required (typically 4.7kΩ)
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
 * Works for all SPI signals: RSPCK (clock), MOSI/COPI (data out),
 * MISO/CIPO (data in), and SSLn (chip select).
 *
 * @par STAR Project SPI Configuration (RPi5 Communication)
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
 *    ┌─────────────────┐            ┌─────────────────┐
 *    │ RSPI0           │            │ SPI             │
 *    │  PA0 (COPI) ────┼────────────┼─► CIPO          │
 *    │  PA1 (CIPO) ◄───┼────────────┼── COPI          │
 *    │  PA3 (CLK)  ◄───┼────────────┼── CLK           │
 *    │  PA4 (CS)   ◄───┼────────────┼── CS            │
 *    └─────────────────┘            └─────────────────┘
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

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MPC_H */
