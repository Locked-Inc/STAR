/**
 * @file mock_rx72n_regs.h
 * @brief Unified Mock RX72N Hardware Register Definitions for Host-Side Unit Testing
 *
 * @details
 * This file provides a comprehensive mock of RX72N hardware register structures
 * for host-side unit testing. It serves as the central aggregator header that
 * collects all peripheral register mocks (CMT timers, system registers, module
 * stop control) into a single include, mirroring the real `lib/rx_hal/inc/rx72n_regs.h`.
 *
 * ## Purpose and Architecture
 *
 * Real RX72N firmware includes `rx72n_regs.h` to access all hardware peripherals.
 * Unit tests running on x86/ARM development hosts cannot access RX72N memory-mapped
 * registers, so this mock file replaces hardware registers with simulated C structures
 * that behave identically from the code's perspective.
 *
 * ## Test vs Production Build Selection
 *
 * **How Mock Override Works:**
 * 1. Production firmware builds: source files include `rx72n_regs.h` (real hardware)
 * 2. Unit test builds: source files use `#ifdef UNIT_TEST` guards to explicitly
 *    include `mock_rx72n_regs.h` (this file) instead
 * 3. The `UNIT_TEST` macro is defined by CMake for all test build targets
 *
 * ## Aggregated Mock Peripherals
 *
 * This file type-aliases multiple peripheral register structures:
 *
 * | Mock Type                | Real Type                | Source File            | Peripheral        |
 * |--------------------------|--------------------------|------------------------|-------------------|
 * | `mock_cmt_channel_t`     | `rx_cmt_channel_regs_t`  | mock_rx_onewire_hw.h   | CMT timer channel |
 * | `mock_cmt_ctrl_t`        | `rx_cmt_control_regs_t`  | mock_rx_onewire_hw.h   | CMT start/stop    |
 * | `mock_system_regs_t`     | `rx_system_regs_t`       | mock_rx72n_system_regs.h | System control  |
 *
 * Additional peripherals (module stop control, clock generation) are defined in
 * the included mock headers ([mock_rx72n_system_regs.h](mock_rx72n_system_regs.h)).
 *
 * ## Mock Register Behavior
 *
 * ### CMT Timer Simulation
 * - **Auto-increment**: Counter advances by 100 ticks per access (simulates time)
 * - **Manual control**: Tests can override via `mock_onewire_hw_set_timer_count()`
 * - **Usage**: OneWire bit timing, periodic tasks, timeout detection
 *
 * ### System Register Simulation
 * - **Module stop control**: MSTPCRA/B/C/D registers enable/disable peripherals
 * - **Protection register**: PRCR controls write access to system registers
 * - **Usage**: Peripheral initialization, power management, clock gating
 *
 * ## Usage in Unit Tests
 *
 * ### Basic Test Setup
 * @code{.c}
 * #include "mock_rx72n_regs.h"
 * #include "rx_onewire.h"  // Driver under test
 *
 * void test_onewire_init(void)
 * {
 *     // Initialize all mock hardware
 *     mock_onewire_hw_init();
 *
 *     // Test driver initialization (uses mock registers internally)
 *     rx_err_t err = rx_onewire_init();
 *     assert(err == k_rx_ok);
 *
 *     // Verify driver configured hardware correctly
 *     volatile rx_cmt_channel_regs_t* timer = cmt3();
 *     assert(timer->cmcr != 0);  // Timer should be configured
 *
 *     // Verify module stop bit cleared
 *     volatile rx_system_regs_t* sys = system_regs();
 *     assert((sys->mstpcrb & (1 << 4)) == 0);  // CMT3 enabled
 *
 *     mock_onewire_hw_deinit();
 * }
 * @endcode
 *
 * ### Multi-Peripheral Test
 * @code{.c}
 * void test_system_clock_config(void)
 * {
 *     mock_onewire_hw_init();
 *
 *     // Access system registers via mock
 *     volatile rx_system_regs_t* sys = system_regs();
 *
 *     // Simulate protected register access
 *     volatile uint16_t* prcr = prcr_reg();
 *     *prcr = 0xA502;  // Unlock system registers (PRKEY=0xA5, PRC1=1)
 *
 *     // Configure clock (mock allows writes)
 *     sys->sckcr = 0x01020304;
 *
 *     // Verify write succeeded
 *     assert(sys->sckcr == 0x01020304);
 *
 *     // Re-lock registers
 *     *prcr = 0xA500;
 *
 *     mock_onewire_hw_deinit();
 * }
 * @endcode
 *
 * ### Timer Progression Test
 * @code{.c}
 * void test_timeout_detection(void)
 * {
 *     mock_onewire_hw_init();
 *
 *     // Configure timer
 *     volatile rx_cmt_channel_regs_t* timer = cmt3();
 *     timer->cmcor = 500;  // 500 tick timeout
 *     timer->cmcnt = 0;
 *
 *     // Simulate time passing
 *     mock_onewire_hw_advance_timer(400);
 *     assert(timer->cmcnt < timer->cmcor);  // Not timed out
 *
 *     mock_onewire_hw_advance_timer(200);
 *     assert(timer->cmcnt >= timer->cmcor);  // Timed out
 *
 *     mock_onewire_hw_deinit();
 * }
 * @endcode
 *
 * ## Included Mock Headers
 *
 * This file includes two mock header files:
 * 1. **[mock_rx_onewire_hw.h](mock_rx_onewire_hw.h)** - CMT timer mock structures and simulation functions
 * 2. **[rx72n_system_regs.h](rx72n_system_regs.h)** - System register mocks (module stop, clock, reset status)
 *
 * These headers define the actual mock structures and provide accessor functions
 * (e.g., `cmt3()`, `system_regs()`, `prcr_reg()`).
 *
 * ## Mock Initialization Requirements
 *
 * **CRITICAL**: Tests MUST call `mock_onewire_hw_init()` before accessing mock registers:
 * - Initializes all register fields to 0 (simulates power-on reset state)
 * - Resets timer counter and shadow variables
 * - Prepares mock state for clean test execution
 *
 * **CRITICAL**: Tests MUST call `mock_onewire_hw_deinit()` after test completion:
 * - Clears mock state to prevent test interference
 * - Ensures next test starts with clean slate
 *
 * Failure to init/deinit mocks may cause non-deterministic test failures.
 *
 * ## Differences from Real Hardware
 *
 * | Feature                | Real Hardware (RX72N)      | Mock (x86/ARM Host)        |
 * |------------------------|----------------------------|----------------------------|
 * | Register location      | Memory-mapped I/O (MMIO)   | Stack/heap C structures    |
 * | Timer counter          | Auto-increment (hardware)  | Manual/simulated increment |
 * | Write protection       | PRCR enforced by hardware  | Software simulation        |
 * | Module stop control    | Gates peripheral clocks    | Sets flag bits only        |
 * | Reset behavior         | Hardware reset circuit     | `mock_onewire_hw_init()`   |
 * | Interrupt generation   | IRQ to CPU                 | Not simulated              |
 *
 * ## What This Mock Enables Testing
 *
 * [OK] Peripheral initialization sequences
 * [OK] Register bit manipulation (set/clear/toggle)
 * [OK] Module stop control logic
 * [OK] Protection register unlock/lock sequences
 * [OK] Timer configuration and timeout detection
 * [OK] Multi-peripheral interaction (timer + system registers)
 *
 * [X] Interrupt timing (no IRQ simulation)
 * [X] Hardware side effects (DMA triggers, pin state changes)
 * [X] Electrical timing (actual microsecond delays)
 *
 * @par Module Dependencies:
 * - [mock_rx_onewire_hw.h](mock_rx_onewire_hw.h) - CMT timer mock structures and simulation
 * - [mock_rx_onewire_hw.c](mock_rx_onewire_hw.c) - Mock register state storage
 * - [rx72n_system_regs.h](rx72n_system_regs.h) - System register mocks (this directory)
 *
 * @see mock_rx_onewire_hw.h CMT timer mock structures and functions
 * @see rx72n_system_regs.h System register mock structures and functions
 * @see mock_onewire_hw_init() Initialize mock hardware before tests
 * @see mock_onewire_hw_deinit() Clean up mock hardware after tests
 * @see lib/rx_hal/inc/rx72n_regs.h Real hardware register definitions (target build)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (No goto)**: [OK] File contains only type aliases and includes
 * - **Rule 2 (Bounded loops)**: [OK] No loops
 * - **Rule 3 (No dynamic allocation)**: [OK] Mock structures statically allocated
 * - **Rule 4 (Short functions)**: [OK] No functions
 * - **Rule 5 (Assertions)**: N/A (type definitions only)
 * - **Rule 6 (Minimal scope)**: [OK] Typedefs at file scope (required for global visibility)
 * - **Rule 7 (Check returns)**: N/A (no function calls)
 * - **Rule 8 (Limit preprocessor)**: [OK] Include guards and conditional typedef only
 * - **Rule 9 (Limit pointers)**: [OK] No pointer operations
 * - **Rule 10 (Warnings)**: [OK] Compiles with `-Wall -Wextra -Werror`
 *
 * @par SOLID Principles:
 * - **Single Responsibility (S)**: [OK] Sole purpose is aggregating register type aliases
 * - **Open/Closed (O)**: [OK] Extensible by adding new mock headers without modifying this file
 * - **Liskov Substitution (L)**: [OK] Mock types are drop-in replacements for real hardware types
 * - **Interface Segregation (I)**: [OK] Minimal interface (type aliases only, no functions)
 * - **Dependency Inversion (D)**: [OK] Production code depends on abstract register types, not concrete implementations
 *
 * @author Locked, Inc.
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @version 1.0.0
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

/* Include mock hardware stubs */
#include "mock_rx72n_dmac_regs.h"
#include "mock_rx72n_system_regs.h"
#include "mock_rx72n_tmr_regs.h"
#include "mock_rx72n_tpu_regs.h"
#include "mock_rx_onewire_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Aliases for Compatibility with Real Code
 * =============================================================================
 */

/**
 * @typedef rx_cmt_channel_regs_t
 * @brief Mock CMT channel register structure (aliased from mock_cmt_channel_t)
 *
 * @details
 * Provides host-side simulation of RX72N CMT (Compare Match Timer) channel registers
 * for unit testing. See [rx72n_cmt_regs.h](rx72n_cmt_regs.h) for detailed documentation.
 *
 * **Fields:**
 * - `cmcr` (16-bit): Timer control (mode, prescaler, interrupt enable)
 * - `cmcnt` (16-bit): Current timer count (auto-increments in mock)
 * - `cmcor` (16-bit): Compare match constant (timeout value)
 *
 * @see mock_cmt_channel_t Actual mock structure definition
 * @see rx72n_cmt_regs.h Detailed CMT mock documentation
 * @see cmt3() Accessor function for mock CMT3 registers
 *
 * @since Version 1.0.0
 */
typedef mock_cmt_channel_t rx_cmt_channel_regs_t;

/**
 * @typedef rx_cmt_control_regs_t
 * @brief Mock CMT global control register structure (aliased from mock_cmt_ctrl_t)
 *
 * @details
 * Provides host-side simulation of RX72N CMT global control registers for timer
 * start/stop operations. See [rx72n_cmt_regs.h](rx72n_cmt_regs.h) for detailed
 * documentation.
 *
 * **Fields:**
 * - `cmstr0` (16-bit): Start register for CMT0-3 (bit N starts timer N)
 * - `cmstr1` (16-bit): Start register for CMT4-7 (RX72N has up to 8 timers)
 *
 * @see mock_cmt_ctrl_t Actual mock structure definition
 * @see rx72n_cmt_regs.h Detailed CMT control register documentation
 * @see cmt_ctrl() Accessor function for mock CMT control registers
 *
 * @since Version 1.0.0
 */
typedef mock_cmt_ctrl_t rx_cmt_control_regs_t;

/**
 * @def RX_SYSTEM_REGS_T_DEFINED
 * @brief Include guard for rx_system_regs_t typedef to prevent redefinition
 *
 * @details
 * This macro ensures `rx_system_regs_t` is defined only once across multiple
 * mock header includes. Both this file and [rx72n_system_regs.h](rx72n_system_regs.h)
 * may define the same typedef, so this guard prevents compilation errors.
 *
 * **Why Needed:**
 * - This file includes `rx72n_system_regs.h` which may already define `rx_system_regs_t`
 * - If so, we skip the typedef here to avoid "redefinition" error
 * - If not, we define it here for backward compatibility
 *
 * @see rx_system_regs_t System register structure typedef
 * @see rx72n_system_regs.h System register mock header
 *
 * @since Version 1.0.0
 */
#ifndef RX_SYSTEM_REGS_T_DEFINED
#define RX_SYSTEM_REGS_T_DEFINED

/**
 * @typedef rx_system_regs_t
 * @brief Mock system register structure (aliased from mock_system_regs_t)
 *
 * @details
 * Provides host-side simulation of RX72N system control registers for unit testing.
 * This structure contains critical system configuration registers:
 *
 * **Module Stop Control (Power Management):**
 * - `mstpcra`, `mstpcrb`, `mstpcrc`, `mstpcrd` (32-bit each) - Module stop registers
 *   - Bit N=1: Module N is stopped (clock gated, low power)
 *   - Bit N=0: Module N is running (clock enabled)
 *   - See [rx72n_system_regs.h](rx72n_system_regs.h) for bit assignments
 *
 * **Clock Control:**
 * - `sckcr` (32-bit): System clock control (dividers for ICLK, PCLK, BCLK, etc.)
 * - `sckcr2`, `sckcr3` (16-bit each): Extended clock control
 * - `pllcr`, `pllcr2` (16-bit, 8-bit): PLL configuration
 *
 * **System Control:**
 * - `mdmonr` (16-bit): Mode monitor (boot mode, endianness)
 * - `syscr0`, `syscr1` (16-bit each): System control (RAM wait states, external bus)
 * - `sbycr` (16-bit): Standby control (sleep mode configuration)
 *
 * See [rx72n_system_regs.h](rx72n_system_regs.h) for complete field documentation.
 *
 * @par Memory Layout:
 * Total size: ~40 bytes (platform-dependent due to reserved fields)
 *
 * @see mock_system_regs_t Actual mock structure definition
 * @see rx72n_system_regs.h Detailed system register documentation
 * @see system_regs() Accessor function for mock system registers
 * @see prcr_reg() Accessor for protection register (controls write access)
 *
 * @since Version 1.0.0
 */
typedef mock_system_regs_t rx_system_regs_t;
#endif

/* Module stop bits are defined in rx72n_system_regs.h */

#ifdef __cplusplus
}
#endif
