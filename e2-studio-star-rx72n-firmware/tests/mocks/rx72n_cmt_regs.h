/* tests/mocks/rx72n_cmt_regs.h */

/**
 * @file rx72n_cmt_regs.h
 * @brief Mock CMT (Compare Match Timer) Register Definitions for Host-Side Unit Testing
 *
 * @details
 * This file provides mock CMT register structures for host-side unit testing of
 * RX72N firmware. It enables testing of timer-dependent code (OneWire, periodic
 * tasks, time-critical protocols) on x86/ARM development hosts without requiring
 * actual RX72N hardware.
 *
 * ## Purpose and Design Rationale
 *
 * Real RX72N CMT registers are memory-mapped hardware peripherals at fixed addresses.
 * Host-side unit tests cannot access these addresses, so this mock replaces the
 * hardware registers with ordinary C structures (`mock_cmt_channel_t`, `mock_cmt_ctrl_t`)
 * that simulate register behavior.
 *
 * ## How It Works
 *
 * 1. **Include Path Shadowing**: When CMake configures unit tests, the `tests/mocks/`
 *    directory is added to the include path BEFORE `lib/rx_hal/inc/`. This causes
 *    `#include "rx72n_cmt_regs.h"` to resolve to THIS file instead of the real one.
 *
 * 2. **Type Aliasing**: This file defines `rx_cmt_channel_regs_t` and
 *    `rx_cmt_control_regs_t` as aliases for mock structures. Production code uses
 *    these type names, so it compiles identically on target and host.
 *
 * 3. **Mock Structures**: The actual mock register structures are defined in
 *    [mock_rx_onewire_hw.h](mock_rx_onewire_hw.h) and implemented in
 *    `mock_rx_onewire_hw.c`. These structures have the same fields as real hardware
 *    registers (CMCR, CMCNT, CMCOR, CMSTR0, CMSTR1) but reside in regular memory.
 *
 * 4. **Simulated Behavior**: Mock accessor functions (e.g., `cmt3()`) auto-increment
 *    the counter register on each access to simulate time progression, allowing tests
 *    to verify timeout logic, polling loops, and timer-dependent algorithms.
 *
 * ## Usage in Unit Tests
 *
 * ### Basic Test Setup
 * @code{.c}
 * #include "rx72n_cmt_regs.h"  // Resolves to THIS mock file
 * #include "mock_rx_onewire_hw.h"
 *
 * void test_onewire_timing(void)
 * {
 *     // Initialize mock hardware state
 *     mock_onewire_hw_init();
 *
 *     // Access CMT registers (uses mock structures)
 *     volatile rx_cmt_channel_regs_t* timer = cmt3();
 *     timer->cmcr = 0x0042;  // Configure timer (writes to mock memory)
 *     timer->cmcnt = 0;      // Reset counter
 *
 *     // Code under test uses same type names
 *     onewire_reset_pulse();
 *
 *     // Verify timer was used correctly
 *     assert(timer->cmcnt > 100);  // Check counter advanced
 *
 *     mock_onewire_hw_deinit();
 * }
 * @endcode
 *
 * ### Advanced Test: Simulating Timer Progression
 * @code{.c}
 * void test_timeout_handling(void)
 * {
 *     mock_onewire_hw_init();
 *
 *     volatile rx_cmt_channel_regs_t* timer = cmt3();
 *     timer->cmcor = 1000;  // Set timeout period
 *
 *     // Simulate time passing
 *     mock_onewire_hw_set_timer_count(900);
 *
 *     // Test timeout detection
 *     bool timed_out = (timer->cmcnt >= timer->cmcor);
 *     assert(!timed_out);  // Not yet
 *
 *     mock_onewire_hw_advance_timer(200);
 *     timed_out = (timer->cmcnt >= timer->cmcor);
 *     assert(timed_out);  // Now timed out
 *
 *     mock_onewire_hw_deinit();
 * }
 * @endcode
 *
 * ## Mock Register Memory Layout
 *
 * | Mock Structure         | Size | Fields               | Purpose                          |
 * |------------------------|------|----------------------|----------------------------------|
 * | `mock_cmt_channel_t`   | 6B   | cmcr, cmcnt, cmcor   | Single CMT timer channel         |
 * | `mock_cmt_ctrl_t`      | 4B   | cmstr0, cmstr1       | Global timer start/stop control  |
 *
 * ## Simulated Hardware Behavior
 *
 * - **Auto-Increment**: Each call to `cmt3()` advances the counter by
 *   `k_mock_onewire_timer_auto_increment` (100 ticks) to simulate time progression
 * - **Manual Control**: Tests can override counter via `mock_onewire_hw_set_timer_count()`
 *   for precise timing scenarios
 * - **Reset**: All registers initialized to 0 by `mock_onewire_hw_init()`
 *
 * ## Differences from Real Hardware
 *
 * | Feature                | Real Hardware       | Mock Implementation      |
 * |------------------------|---------------------|--------------------------|
 * | Counter increment      | Automatic (PCLK/8)  | Manual (100/access)      |
 * | Register addresses     | 0x00088000-0x0008800F | Stack/heap allocation    |
 * | Side effects           | Interrupts, DMA     | None                     |
 * | Initialization         | Module stop control | `mock_onewire_hw_init()` |
 *
 * @par Module Dependencies:
 * - [mock_rx_onewire_hw.h](mock_rx_onewire_hw.h) - Mock structure definitions and accessor functions
 * - [mock_rx_onewire_hw.c](mock_rx_onewire_hw.c) - Mock register state and simulation logic
 *
 * @see mock_rx_onewire_hw.h Mock structure definitions and simulation functions
 * @see mock_cmt_channel_t CMT channel register structure (CMCR, CMCNT, CMCOR)
 * @see mock_cmt_ctrl_t CMT control register structure (CMSTR0, CMSTR1)
 * @see cmt3() CMT3 register accessor with auto-increment behavior
 * @see mock_onewire_hw_init() Initialize mock hardware state before tests
 * @see mock_onewire_hw_set_timer_count() Control simulated timer value
 * @see lib/rx_hal/inc/rx72n_cmt_regs.h Real hardware register definitions (target build)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (No goto)**: [OK] File contains only type aliases, no control flow
 * - **Rule 2 (Bounded loops)**: [OK] No loops
 * - **Rule 3 (No dynamic allocation)**: [OK] Mock structures statically allocated in mock_rx_onewire_hw.c
 * - **Rule 4 (Short functions)**: [OK] No functions
 * - **Rule 5 (Assertions)**: N/A (type definitions only)
 * - **Rule 6 (Minimal scope)**: [OK] Typedefs at file scope (required for global visibility)
 * - **Rule 7 (Check returns)**: N/A (no function calls)
 * - **Rule 8 (Limit preprocessor)**: [OK] Uses include guards only
 * - **Rule 9 (Limit pointers)**: [OK] No pointer operations
 * - **Rule 10 (Warnings)**: [OK] Compiles with `-Wall -Wextra -Werror`
 *
 * @par SOLID Principles:
 * - **Single Responsibility (S)**: [OK] Sole purpose is CMT register type aliasing for tests
 * - **Open/Closed (O)**: [OK] Extensible via mock_rx_onewire_hw.h without modifying this file
 * - **Liskov Substitution (L)**: [OK] Mock types drop-in replacements for real register types
 * - **Interface Segregation (I)**: [OK] Minimal interface (2 type aliases only)
 * - **Dependency Inversion (D)**: [OK] High-level code depends on abstract types, not concrete mock/real implementations
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 *
 * @version 1.0.0
 * @since 1.0.0
 */

#pragma once

#include "mock_rx_onewire_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Aliases for Compatibility
 * =============================================================================
 */

/**
 * @typedef rx_cmt_channel_regs_t
 * @brief Mock CMT channel register structure alias for unit tests
 *
 * @details
 * Aliases `mock_cmt_channel_t` to `rx_cmt_channel_regs_t` so production code
 * compiles identically on target (real hardware) and host (unit tests). This
 * enables testing of timer-dependent code without modifying source.
 *
 * The mock structure contains the same fields as real RX72N CMT channel registers:
 * - **cmcr**: Compare Match Control Register (timer mode, prescaler, interrupt enable)
 * - **cmcnt**: Compare Match Counter Register (current timer count)
 * - **cmcor**: Compare Match Constant Register (timeout/period value)
 *
 * In unit tests, these registers are simulated C structures with auto-increment
 * behavior to model time progression. On target hardware, they are memory-mapped
 * I/O at fixed addresses (e.g., 0x00088000 for CMT0).
 *
 * @par Memory Layout Comparison:
 *
 * **Real Hardware (RX72N Target):**
 * | Offset | Register | Size | Access |
 * |--------|----------|------|--------|
 * | 0x00   | CMCR     | 16b  | R/W    |
 * | 0x02   | CMCNT    | 16b  | R/W    |
 * | 0x04   | CMCOR    | 16b  | R/W    |
 *
 * **Mock (x86/ARM Host):**
 * | Offset | Field | Size | Access |
 * |--------|-------|------|--------|
 * | 0x00   | cmcr  | 16b  | R/W    |
 * | 0x02   | cmcnt | 16b  | R/W    |
 * | 0x04   | cmcor | 16b  | R/W    |
 *
 * Total Size: 6 bytes (packed struct)
 *
 * @par Usage Example:
 * @code{.c}
 * // Code compiles identically on target and host
 * volatile rx_cmt_channel_regs_t* timer = cmt3();
 *
 * // Configure timer for 1ms period (PCLK/8 = 60MHz/8 = 7.5MHz)
 * timer->cmcr = 0x0042;  // CKS=01 (PCLK/8), CMIE=1 (interrupt enable)
 * timer->cmcor = 7500;   // 7500 ticks at 7.5MHz = 1ms
 * timer->cmcnt = 0;      // Reset counter
 *
 * // Start timer (writes to CMT control registers)
 * volatile rx_cmt_control_regs_t* ctrl = cmt_ctrl();
 * ctrl->cmstr0 |= (1 << 3);  // Enable CMT3
 * @endcode
 *
 * @see mock_cmt_channel_t Actual mock structure definition
 * @see cmt3() Accessor function returning pointer to mock CMT3 registers
 * @see lib/rx_hal/inc/rx72n_cmt_regs.h Real hardware register definition
 *
 * @since 1.0.0
 */
typedef mock_cmt_channel_t rx_cmt_channel_regs_t;

/**
 * @typedef rx_cmt_control_regs_t
 * @brief Mock CMT global control register structure alias for unit tests
 *
 * @details
 * Aliases `mock_cmt_ctrl_t` to `rx_cmt_control_regs_t` for test/target compatibility.
 * This structure contains global CMT timer start/stop control registers that manage
 * multiple CMT channels simultaneously.
 *
 * The mock structure contains the same fields as real RX72N CMT control registers:
 * - **cmstr0**: Compare Match Start Register 0 (controls CMT0-3 start/stop)
 * - **cmstr1**: Compare Match Start Register 1 (controls CMT4-7 on some variants)
 *
 * Each bit in CMSTR0/1 corresponds to one timer channel. Setting bit N to 1 starts
 * timer N, clearing it stops timer N.
 *
 * @par Memory Layout Comparison:
 *
 * **Real Hardware (RX72N Target):**
 * | Offset | Register | Size | Access | Address    |
 * |--------|----------|------|--------|------------|
 * | 0x00   | CMSTR0   | 16b  | R/W    | 0x00088000 |
 * | 0x02   | CMSTR1   | 16b  | R/W    | 0x00088010 |
 *
 * **Mock (x86/ARM Host):**
 * | Offset | Field   | Size | Access |
 * |--------|---------|------|--------|
 * | 0x00   | cmstr0  | 16b  | R/W    |
 * | 0x02   | cmstr1  | 16b  | R/W    |
 *
 * Total Size: 4 bytes (packed struct)
 *
 * @par CMSTR0 Bit Assignments (RX72N):
 * | Bit | Name | Description                    |
 * |-----|------|--------------------------------|
 * | 0   | STR0 | 1=Start CMT0, 0=Stop CMT0      |
 * | 1   | STR1 | 1=Start CMT1, 0=Stop CMT1      |
 * | 2   | STR2 | 1=Start CMT2, 0=Stop CMT2      |
 * | 3   | STR3 | 1=Start CMT3, 0=Stop CMT3      |
 * | 15-4| -    | Reserved (read as 0)           |
 *
 * @par Usage Example:
 * @code{.c}
 * // Start CMT3 for OneWire bit timing
 * volatile rx_cmt_control_regs_t* ctrl = cmt_ctrl();
 * ctrl->cmstr0 |= (1 << 3);  // Set STR3 bit to start CMT3
 *
 * // ... OneWire transaction ...
 *
 * // Stop CMT3 to save power
 * ctrl->cmstr0 &= ~(1 << 3);  // Clear STR3 bit to stop CMT3
 * @endcode
 *
 * @par Test Example:
 * @code{.c}
 * void test_cmt_start_stop(void)
 * {
 *     mock_onewire_hw_init();
 *
 *     volatile rx_cmt_control_regs_t* ctrl = cmt_ctrl();
 *
 *     // Initially stopped
 *     assert((ctrl->cmstr0 & (1 << 3)) == 0);
 *
 *     // Start timer
 *     ctrl->cmstr0 |= (1 << 3);
 *     assert((ctrl->cmstr0 & (1 << 3)) != 0);
 *
 *     // Stop timer
 *     ctrl->cmstr0 &= ~(1 << 3);
 *     assert((ctrl->cmstr0 & (1 << 3)) == 0);
 *
 *     mock_onewire_hw_deinit();
 * }
 * @endcode
 *
 * @see mock_cmt_ctrl_t Actual mock structure definition
 * @see cmt_ctrl() Accessor function returning pointer to mock CMT control registers
 * @see lib/rx_hal/inc/rx72n_cmt_regs.h Real hardware register definition
 *
 * @since 1.0.0
 */
typedef mock_cmt_ctrl_t rx_cmt_control_regs_t;

#ifdef __cplusplus
}
#endif
