/**
 * @file rx72n_regs.h
 * @brief RX72N Hardware Register Definitions - Main Aggregator Header
 *
 * @details
 * ## Overview
 * Central aggregator header that includes all RX72N peripheral register definitions
 * required for STAR motor controller firmware. Provides a single include point for
 * accessing all hardware registers across the entire RX72N microcontroller.
 *
 * **Purpose**: Simplifies HAL implementation by consolidating all register headers.
 * **Based On**: Renesas RX72N Group User's Manual: Hardware (Rev.1.00, Feb 2019)
 * **Target MCU**: R5F572NNHxFB (144-pin LFQFP, 4MB Flash, 512KB SRAM)
 *
 * ## Architecture
 *
 * This file acts as a facade, grouping related peripheral register definitions:
 *
 * @dot
 * digraph register_hierarchy {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   app [label="Application Code\n(motor_control.c, usb.c)", shape=component];
 *   hal [label="hardware.h\n(HAL API)", style="filled", fillcolor="lightblue"];
 *   regs [label="rx72n_regs.h\n(This File)", style="filled", fillcolor="yellow"];
 *
 *   subgraph cluster_peripherals {
 *     label="Individual Peripheral Register Headers";
 *     style=dashed;
 *
 *     sys [label="rx72n_system_regs.h\n(SYSTEM, MODULE STOP)"];
 *     port [label="rx72n_port_regs.h\n(GPIO)"];
 *     adc [label="rx72n_adc_regs.h\n(S12AD)"];
 *     sci [label="rx72n_sci_regs.h\n(UART)"];
 *     riic [label="rx72n_riic_regs.h\n(I2C)"];
 *     rspi [label="rx72n_rspi_regs.h\n(SPI)"];
 *     cmt [label="rx72n_cmt_regs.h\n(TIMER)"];
 *     icu [label="rx72n_icu_regs.h\n(INTERRUPTS)"];
 *     usb [label="rx72n_usb_regs.h\n(USB)"];
 *     mtu [label="rx72n_mtu_regs.h\n(ENCODER)"];
 *     gptw [label="rx72n_gptw_regs.h\n(PWM)"];
 *     mpc [label="rx72n_mpc_regs.h\n(PIN MUX)"];
 *   }
 *
 *   app -> hal;
 *   hal -> regs;
 *   regs -> sys;
 *   regs -> port;
 *   regs -> adc;
 *   regs -> sci;
 *   regs -> riic;
 *   regs -> rspi;
 *   regs -> cmt;
 *   regs -> icu;
 *   regs -> usb;
 *   regs -> mtu;
 *   regs -> gptw;
 *   regs -> mpc;
 * }
 * @enddot
 *
 * ## Included Peripheral Register Files
 *
 * | Header File | Peripheral | Base Address | Purpose in STAR |
 * |-------------|------------|--------------|-----------------|
 * | rx72n_system_regs.h | System Control | 0x00080000 | Clock config, module stop control |
 * | rx72n_port_regs.h | GPIO Ports | 0x0008C000 | Digital I/O, motor control signals |
 * | rx72n_adc_regs.h | S12AD0/1 | 0x00089000 | Current sensing, voltage measurement |
 * | rx72n_sci_regs.h | SCI0-12 | 0x0008A000 | UART debug output (SCI9) |
 * | rx72n_riic_regs.h | RIIC0-2 | 0x00088300 | I2C sensors (IMU) |
 * | rx72n_rspi_regs.h | RSPI0-2 | 0x000D0000 | SPI: RPi5 comm (RSPI1 not used) |
 * | rx72n_cmt_regs.h | CMT0-3 | 0x00088000 | ThreadX system tick (100 Hz) |
 * | rx72n_icu_regs.h | ICU | 0x00087000 | Interrupt routing and priorities |
 * | rx72n_iwdt_regs.h | IWDT | 0x00088030 | Independent watchdog (safety) |
 * | rx72n_wdt_regs.h | WDT | 0x00088020 | Watchdog timer |
 * | rx72n_crc_regs.h | CRC | 0x00088280 | Hardware CRC-32 (Protocol validation) |
 * | rx72n_usb_regs.h | USB0 | 0x000A0000 | USB 2.0 full-speed device mode |
 * | rx72n_mtu_regs.h | MTU0-4 | 0x000C1200 | Quadrature encoder counting |
 * | rx72n_gptw_regs.h | GPTW0-3 | 0x000C2000 | High-resolution PWM (motors) |
 * | rx72n_mpc_regs.h | MPC | 0x0008C100 | Pin function multiplexing |
 * | rx72n_flash_regs.h | FLASH | 0x007FE000 | Flash programming, ROM cache |
 * | rx72n_ram_regs.h | RAM | 0x00081200 | RAM/EXRAM/ECCRAM control, parity, ECC |
 * | rx72n_clock.h | Clock Defs | - | Frequency constants (240 MHz) |
 *
 * ## RX72N Memory Map (Relevant Sections)
 *
 * ```
 * 0x00000000 +-----------------------------+
 *            | Internal ROM (4MB Flash)    |
 * 0x00400000 +-----------------------------+
 *            | Reserved                    |
 * 0x00800000 +-----------------------------+
 *            | Peripheral Registers        |
 *            |   CMT: 0x00088000          |
 *            |   ICU: 0x00087000          |
 *            |   ADC: 0x00089000          |
 *            |   SCI: 0x0008A000          |
 *            |   PORT: 0x0008C000         |
 *            |   RSPI: 0x000D0000         |
 * 0x000D4000 +-----------------------------+
 *            | Reserved                    |
 * 0x00800000 +-----------------------------+
 *            | Internal RAM (512KB SRAM)   |
 * 0x00880000 +-----------------------------+
 *            | Reserved                    |
 * 0xFFFFFFFF +-----------------------------+
 * ```
 *
 * ## Usage Guidelines
 *
 * ### Recommended Practice
 * **Application code should NOT include this file directly.** Instead, use the HAL API
 * provided by `hardware.h` which abstracts register access with type-safe functions.
 *
 * @code{.c}
 * // WRONG - Direct register access (bypasses HAL safety checks)
 * #include "rx72n_regs.h"
 * PORT3.PDR.BIT.B0 = 1;  // No validation, no error checking
 *
 * // CORRECT - Use HAL API (safe, validated, portable)
 * #include "hardware.h"
 * gpio_set_output(k_port_c_pin_0);  // Type-safe, validated, error-checked
 * @endcode
 *
 * ### When Direct Register Access is Acceptable
 * Only use direct register access when:
 * 1. Implementing new HAL drivers (inside lib/rx_hal/src/)
 * 2. Low-level initialization code (clock setup, early boot)
 * 3. Interrupt handlers requiring minimal latency
 * 4. Performance-critical code after profiling shows HAL overhead is significant
 *
 * ### Register Access Pattern (for HAL implementation)
 * @code{.c}
 * #include "rx72n_regs.h"
 *
 * // Example: Configure CMT0 for 10ms period @ 240 MHz
 * void timer_init(void) {
 *   // 1. Enable module clock
 *   SYSTEM.MSTPCRA.BIT.MSTPA15 = 0;  // CMT0 module stop release
 *
 *   // 2. Stop timer
 *   CMT0.CMSTR.BIT.STR = 0;
 *
 *   // 3. Configure
 *   CMT0.CMCR.WORD = 0x0042;  // PCLK/128, compare match interrupt
 *   CMT0.CMCOR = 18750;       // 240MHz / 128 / 100Hz - 1
 *
 *   // 4. Start timer
 *   CMT0.CMSTR.BIT.STR = 1;
 * }
 * @endcode
 *
 * ## Design Rationale
 *
 * ### Why Separate Register Headers?
 * | Approach | Pros | Cons |
 * |----------|------|------|
 * | **Single monolithic file** | Easy to find everything | >10,000 lines, slow compilation, merge conflicts |
 * | **Separate files (this approach)** | Modular, fast compilation, maintainable | Requires aggregator (this file) |
 *
 * ### Benefits of This Architecture
 * 1. **Compile-time optimization**: Only includes necessary peripheral headers
 * 2. **Maintainability**: Each peripheral in separate file (~200-500 lines each)
 * 3. **Testability**: Individual headers can be tested/validated independently
 * 4. **Team collaboration**: Multiple developers can work on different peripherals without conflicts
 *
 * ## Register Access Performance
 *
 * | Operation | Cycles @ 240 MHz | Time | Notes |
 * |-----------|------------------|------|-------|
 * | Single register write | 1-2 | 4-8 ns | Direct memory store |
 * | Single register read | 1-2 | 4-8 ns | Direct memory load |
 * | Bit-field write | 4-6 | 17-25 ns | Read-modify-write sequence |
 * | Protected register write | 10-15 | 42-63 ns | Includes unlock/lock sequence |
 *
 * ## Safety Considerations
 *
 * ### Write-Protected Registers
 * Some critical registers require unlock sequence before modification:
 * - **System registers**: Clock config, module stop control
 * - **Port function selection**: MPC registers
 * - **Watchdog**: WDT/IWDT configuration
 *
 * **Always use HAL functions** for these registers to ensure correct unlock/lock sequence.
 *
 * ### Reserved Bits
 * - **Never write to reserved bits** (may cause undefined behavior or peripheral malfunction)
 * - **Read as 0, write as 0** for all reserved bits
 * - HAL functions handle this automatically
 *
 * ## Dependencies
 * - Individual peripheral register headers (rx72n_*_regs.h)
 * - `<stdint.h>` for fixed-width integer types (uint8_t, uint16_t, uint32_t)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp, recursion (pure header file with includes and definitions)
 * - **Rule 2 [N/A]:** No loops (header file only)
 * - **Rule 3 [YES]:** No dynamic allocation (register definitions are compile-time constants)
 * - **Rule 4 [N/A]:** No functions (header file only)
 * - **Rule 5 [N/A]:** No functions to validate (header file only)
 * - **Rule 6 [YES]:** All definitions at file scope (standard C practice)
 * - **Rule 7 [N/A]:** No function calls (header file only)
 * - **Rule 8 [YES]:** No macros except include guards
 * - **Rule 9 [YES]:** Register definitions use typed pointers (single level of dereferencing)
 * - **Rule 10 [YES]:** Compiles without warnings with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Only aggregates register definitions (no logic, no functions)
 * - **O (Open/Closed):** New peripherals added by including new headers (no modification needed)
 * - **L (Liskov Substitution):** N/A (no polymorphism in C header files)
 * - **I (Interface Segregation):** Each peripheral in separate header (users include only what they need)
 * - **D (Dependency Inversion):** HAL (hardware.h) depends on this abstraction, not concrete implementations
 *
 * @par Module Dependencies:
 * ```
 * rx72n_regs.h (this file)
 *   +--> rx72n_system_regs.h
 *   +--> rx72n_port_regs.h
 *   +--> rx72n_adc_regs.h
 *   +--> rx72n_sci_regs.h
 *   +--> rx72n_riic_regs.h
 *   +--> rx72n_rspi_regs.h
 *   +--> rx72n_cmt_regs.h
 *   +--> rx72n_icu_regs.h
 *   +--> rx72n_iwdt_regs.h
 *   +--> rx72n_wdt_regs.h
 *   +--> rx72n_crc_regs.h
 *   +--> rx72n_usb_regs.h
 *   +--> rx72n_mtu_regs.h
 *   +--> rx72n_gptw_regs.h
 *   +--> rx72n_mpc_regs.h
 *   +--> rx72n_flash_regs.h
 *   +--> rx72n_ram_regs.h
 *   +--> rx72n_clock.h
 *
 * Used by:
 *   +--> hardware.h (HAL API implementation)
 * ```
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see hardware.h for type-safe HAL API (recommended for application code)
 * @see Renesas RX72N Group User's Manual: Hardware (Rev.1.00)
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * System Control Registers
 * =============================================================================
 */

#include "rx72n_system_regs.h"

/* =============================================================================
 * Port Registers (GPIO)
 * =============================================================================
 */

#include "rx72n_port_regs.h"

/* =============================================================================
 * 12-bit A/D Converter (S12ADFa) - For Current Sensing
 * =============================================================================
 */

#include "rx72n_adc_regs.h"

/* =============================================================================
 * Serial Communication Interface (SCI) - For UART/Debug
 * =============================================================================
 */

#include "rx72n_sci_regs.h"

/* =============================================================================
 * I2C Bus Interface (RIIC) - For I2C Communication
 * =============================================================================
 */

#include "rx72n_riic_regs.h"

/* =============================================================================
 * Renesas Serial Peripheral Interface (RSPI)
 * For SPI to RPi5 (RSPI1 not used in current design)
 * =============================================================================
 */

#include "rx72n_rspi_regs.h"

/* =============================================================================
 * Compare Match Timer (CMT) - For ThreadX System Tick
 * =============================================================================
 */

#include "rx72n_cmt_regs.h"

/* =============================================================================
 * Interrupt Controller (ICU)
 * =============================================================================
 */

#include "rx72n_icu_regs.h"

/* =============================================================================
 * Independent Watchdog Timer (IWDT)
 * =============================================================================
 */

#include "rx72n_iwdt_regs.h"

/* =============================================================================
 * Watchdog Timer (WDT)
 * =============================================================================
 */

#include "rx72n_wdt_regs.h"

/* =============================================================================
 * CRC Calculator - Hardware CRC-32 Acceleration
 * =============================================================================
 */

#include "rx72n_crc_regs.h"

/* =============================================================================
 * Clock Frequency Accuracy Measurement Circuit (CAC)
 * =============================================================================
 */

#include "rx72n_cac_regs.h"

/* =============================================================================
 * USB 2.0 Full-Speed Host/Function Module (USB0) -- REMOVED
 * =============================================================================
 * The rx72n_usb_regs.h header was deleted alongside libs/rx_usb/ when USB0
 * was purged. The peripheral exists in silicon but is no longer used; if it
 * is ever re-enabled, restore the register header from git history.  USB0
 * bring-up work continues on the feat/usb0 branch.
 */

/* =============================================================================
 * Multi-Function Timer Unit (MTU) - Encoder Quadrature Counting
 * =============================================================================
 */

#include "rx72n_mtu_regs.h"

/* =============================================================================
 * General PWM Timer (GPTW) - Motor PWM Generation
 * =============================================================================
 */

#include "rx72n_gptw_regs.h"

/* =============================================================================
 * GPTW Port Output Enable (POEG) - Emergency Motor Stop
 * =============================================================================
 */

#include "rx72n_poeg_regs.h"

/* =============================================================================
 * 16-Bit Timer Pulse Unit (TPU) - Quadrature Encoder Phase Counting
 * =============================================================================
 */

#include "rx72n_tpu_regs.h"

/* =============================================================================
 * 8-Bit Timer (TMRb) - Four 8-bit Channels / Two 16-bit Cascaded Pairs
 * =============================================================================
 */

#include "rx72n_tmr_regs.h"

/* =============================================================================
 * Voltage Detection Circuit (LVDA) - Brownout Protection
 * =============================================================================
 */

#include "rx72n_lvda_regs.h"

/* =============================================================================
 * Multi-Function Pin Controller (MPC)
 * =============================================================================
 */

#include "rx72n_mpc_regs.h"

/* =============================================================================
 * Flash Memory (FLASH) - Programming/Erase and ROM Cache
 * =============================================================================
 */

#include "rx72n_flash_regs.h"

/* =============================================================================
 * RAM Control (RAM/EXRAM) - Parity-checked Memory Configuration
 * =============================================================================
 */

#include "rx72n_ram_regs.h"

/* =============================================================================
 * ECCRAM Control - ECC-protected RAM (SEC-DED)
 * =============================================================================
 */

#include "rx72n_eccram_regs.h"

/* =============================================================================
 * DMA Controller (DMAC) - High-throughput DMA Transfers
 * =============================================================================
 */

#include "rx72n_dmac_regs.h"

/* =============================================================================
 * Data Transfer Controller (DTC) - Lightweight Interrupt-driven Transfers
 * =============================================================================
 */

#include "rx72n_dtc_regs.h"

/* =============================================================================
 * Memory Protection Unit (MPU) - User Mode Memory Access Control
 * =============================================================================
 */

#include "rx72n_mpu_regs.h"

/* =============================================================================
 * Low Power Consumption (LPC) - Deep Standby and Operating Power Control
 * =============================================================================
 */

#include "rx72n_lpc_regs.h"

/* =============================================================================
 * Event Link Controller (ELC) - Hardware Event Routing
 * =============================================================================
 */

#include "rx72n_elc_regs.h"

/* =============================================================================
 * Clock Frequencies
 * =============================================================================
 */

#include "rx72n_clock.h"

#ifdef __cplusplus
}
#endif
