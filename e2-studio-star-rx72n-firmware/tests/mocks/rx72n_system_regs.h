/* tests/mocks/rx72n_system_regs.h */

/**
 * @file rx72n_system_regs.h
 * @brief Mock RX72N System Control Register Definitions for Host-Side Unit Testing
 *
 * @details
 * This file provides comprehensive mock implementations of RX72N system control
 * registers for host-side unit testing. It simulates critical system peripherals
 * including module stop control (clock gating), protection registers (write access
 * control), and reset status registers (boot diagnostics).
 *
 * ## Purpose and Design Philosophy
 *
 * Real RX72N firmware uses system registers to:
 * - **Module Stop Control**: Gate peripheral clocks to save power (MSTPCRA/B/C/D)
 * - **Write Protection**: Prevent accidental writes to critical registers (PRCR)
 * - **Reset Detection**: Determine why MCU reset occurred (RSTSR0/1/2)
 * - **Clock Configuration**: Set up PLL, dividers, oscillators (SCKCR, PLLCR)
 * - **Power Modes**: Configure sleep/standby modes (SBYCR, SYSCR0/1)
 *
 * Host-side unit tests cannot access RX72N memory-mapped registers, so this mock
 * file replaces them with ordinary C structures and accessor functions that simulate
 * register behavior. This enables testing of:
 * - Peripheral initialization sequences (module stop bit clearing)
 * - Protection register unlock/lock logic
 * - Reset source detection
 * - Clock configuration algorithms
 *
 * ## Include Path Shadowing
 *
 * **How This File Replaces Real Hardware:**
 * 1. Production builds: `lib/rx_hal/inc/rx72n_system_regs.h` is used (real hardware)
 * 2. Unit test builds: CMake prepends `tests/mocks/` to include path
 * 3. `#include "rx72n_system_regs.h"` resolves to THIS file instead of real one
 * 4. Production code unchanged - same include statements work for target and host
 *
 * **CMakeLists.txt Configuration:**
 * @code{.cmake}
 * add_executable(test_system_init test_system_init.c)
 * target_include_directories(test_system_init PRIVATE
 *     ${CMAKE_SOURCE_DIR}/tests/mocks      # THIS file used
 *     ${CMAKE_SOURCE_DIR}/lib/rx_hal/inc   # Real file shadowed
 * )
 * @endcode
 *
 * ## Mock Register Categories
 *
 * This file mocks the following RX72N system register groups:
 *
 * ### 1. Module Stop Control Registers (Power Management)
 * - **MSTPCRA** (0x80000): Module Stop Control Register A (DMA, EXDMAC)
 * - **MSTPCRB** (0x80004): Module Stop Control Register B (USB, CAN, I2C, SPI, SCI)
 * - **MSTPCRC** (0x80008): Module Stop Control Register C (RAM, ROM, TMR, CMT)
 * - **MSTPCRD** (0x8000C): Module Stop Control Register D (MTU, GPT, POEG)
 *
 * Each bit controls one peripheral module. Setting bit=1 stops the module (clock
 * gated), bit=0 enables it. Default after reset is usually all 1s (all stopped).
 *
 * ### 2. Protection Register (Write Access Control)
 * - **PRCR** (0x803FE): Protect Register - controls write access to system registers
 *   - Bits 15-8: PRKEY (0xA5 = unlock, other = lock)
 *   - Bit 0: PRC0 (clock generation circuit protection)
 *   - Bit 1: PRC1 (low-power mode protection)
 *   - Bit 3: PRC3 (LVD protection)
 *
 * ### 3. Reset Status Registers (Boot Diagnostics)
 * - **RSTSR0** (0x8C290): Power-on reset, LVD reset flags
 * - **RSTSR1** (0x8C291): Cold/warm start determination
 * - **RSTSR2** (0x800C0): Watchdog, software reset flags
 *
 * ### 4. System Control Registers (Configuration)
 * - **SCKCR** (0x80020): System clock control (dividers)
 * - **SCKCR2/3**: Extended clock control
 * - **PLLCR/2**: PLL configuration
 * - **SYSCR0/1**: System control (RAM wait, external bus)
 * - **SBYCR**: Standby control (sleep mode config)
 * - **MDMONR**: Mode monitor (boot mode, endianness)
 *
 * ## Mock Implementation Strategy
 *
 * This file uses three approaches to mock system registers:
 *
 * ### 1. Inline Accessor Functions (PRCR)
 * ```c
 * static inline volatile uint16_t* prcr_reg(void) {
 *     return &g_mock_prcr;
 * }
 * ```
 * - **When**: Single register, simple access pattern
 * - **Why**: Zero overhead, no function call, easy to inline in tests
 *
 * ### 2. External Accessor Functions (RSTSR)
 * ```c
 * volatile rx_rstsr01_regs_t* rstsr01(void);  // Implemented in .c file
 * ```
 * - **When**: Multiple related registers, complex initialization
 * - **Why**: Allows mock state management, easier to extend
 *
 * ### 3. Type Aliasing (System Registers)
 * ```c
 * typedef mock_system_regs_t rx_system_regs_t;
 * ```
 * - **When**: Large register block used by multiple modules
 * - **Why**: Shares implementation with other mocks (mock_rx_onewire_hw.h)
 *
 * ## Usage in Unit Tests
 *
 * ### Test 1: Module Stop Control (Peripheral Enable)
 * @code{.c}
 * #include "rx72n_system_regs.h"
 *
 * void test_usb_module_enable(void)
 * {
 *     // Initialize mock system registers
 *     volatile rx_system_regs_t* sys = system_regs();
 *     sys->mstpcrb = 0xFFFFFFFF;  // All modules stopped initially
 *
 *     // Unlock protection register
 *     volatile uint16_t* prcr = prcr_reg();
 *     *prcr = 0xA502;  // PRKEY=0xA5, PRC1=1 (enable writes)
 *
 *     // Clear USB0 module stop bit (enable USB peripheral)
 *     sys->mstpcrb &= ~(1U << k_mstpb_usb0);
 *
 *     // Verify USB module enabled
 *     assert((sys->mstpcrb & (1U << k_mstpb_usb0)) == 0);
 *
 *     // Re-lock protection
 *     *prcr = 0xA500;
 * }
 * @endcode
 *
 * ### Test 2: Reset Source Detection
 * @code{.c}
 * void test_reset_source_detection(void)
 * {
 *     // Simulate power-on reset
 *     volatile rx_rstsr01_regs_t* rst01 = rstsr01();
 *     volatile uint8_t* rst2 = rstsr2();
 *
 *     rst01->rstsr0 = k_rstsr0_porf;  // Set power-on reset flag
 *     rst01->rstsr1 = k_rstsr1_cwsf;  // Cold start
 *     *rst2 = 0;                       // No watchdog/software reset
 *
 *     // Test reset detection logic
 *     bool is_power_on = (rst01->rstsr0 & k_rstsr0_porf) != 0;
 *     bool is_cold_start = (rst01->rstsr1 & k_rstsr1_cwsf) != 0;
 *     bool is_watchdog = (*rst2 & k_rstsr2_iwdtrf) != 0;
 *
 *     assert(is_power_on);
 *     assert(is_cold_start);
 *     assert(!is_watchdog);
 *
 *     // Clear flags (write 0 to clear on real hardware)
 *     rst01->rstsr0 &= ~k_rstsr0_porf;
 *     rst01->rstsr1 &= ~k_rstsr1_cwsf;
 *
 *     assert(rst01->rstsr0 == 0);
 * }
 * @endcode
 *
 * ### Test 3: Multi-Module Initialization Sequence
 * @code{.c}
 * void test_peripheral_init_sequence(void)
 * {
 *     volatile rx_system_regs_t* sys = system_regs();
 *     volatile uint16_t* prcr = prcr_reg();
 *
 *     // 1. Unlock system registers
 *     *prcr = 0xA502;
 *
 *     // 2. Enable multiple peripherals
 *     sys->mstpcrb &= ~((1U << k_mstpb_usb0) | (1U << k_mstpb_crc));
 *
 *     // 3. Verify both modules enabled
 *     assert((sys->mstpcrb & (1U << k_mstpb_usb0)) == 0);
 *     assert((sys->mstpcrb & (1U << k_mstpb_crc)) == 0);
 *
 *     // 4. Configure clock (example)
 *     sys->sckcr = 0x01020304;  // Set dividers
 *
 *     // 5. Re-lock registers
 *     *prcr = 0xA500;
 *
 *     // 6. Verify lock (writes should be ignored on real hardware)
 *     // Note: Mock doesn't enforce PRCR protection, tests must verify logic
 * }
 * @endcode
 *
 * ## Mock Memory Layout
 *
 * ### rx_system_regs_t Structure
 * | Offset | Field     | Size | Description                          |
 * |--------|-----------|------|--------------------------------------|
 * | 0x00   | mdmonr    | 16b  | Mode monitor register                |
 * | 0x02   | reserved0 | 4B   | Reserved area                        |
 * | 0x06   | syscr0    | 16b  | System control 0 (RAM wait states)   |
 * | 0x08   | syscr1    | 16b  | System control 1 (external bus)      |
 * | 0x0A   | reserved1 | 2B   | Reserved area                        |
 * | 0x0C   | sbycr     | 16b  | Standby control (sleep modes)        |
 * | 0x0E   | reserved2 | 2B   | Reserved area                        |
 * | 0x10   | mstpcra   | 32b  | Module stop control A                |
 * | 0x14   | mstpcrb   | 32b  | Module stop control B                |
 * | 0x18   | mstpcrc   | 32b  | Module stop control C                |
 * | 0x1C   | mstpcrd   | 32b  | Module stop control D                |
 * | 0x20   | sckcr     | 32b  | System clock control                 |
 * | 0x24   | sckcr2    | 16b  | System clock control 2               |
 * | 0x26   | sckcr3    | 16b  | System clock control 3               |
 * | 0x28   | pllcr     | 16b  | PLL control                          |
 * | 0x2A   | pllcr2    | 8b   | PLL control 2                        |
 *
 * **Total Size**: ~43 bytes (platform-dependent padding)
 *
 * ### rx_rstsr01_regs_t Structure (Contiguous Reset Status)
 * | Offset | Field   | Size | Description          |
 * |--------|---------|------|----------------------|
 * | 0x00   | rstsr0  | 8b   | Reset status 0       |
 * | 0x01   | rstsr1  | 8b   | Reset status 1       |
 *
 * **Total Size**: 2 bytes
 *
 * ## Differences from Real Hardware
 *
 * | Feature                | Real Hardware (RX72N)       | Mock (x86/ARM Host)           |
 * |------------------------|-----------------------------|-------------------------------|
 * | Register location      | MMIO at fixed addresses     | C structures in memory        |
 * | PRCR protection        | Enforced by hardware        | Not enforced (tests must verify) |
 * | Module stop effects    | Gates peripheral clocks     | Sets flag bits only           |
 * | Reset flags            | Set by reset controller     | Manually set by test setup    |
 * | Clock configuration    | Controls oscillator/PLL     | No effect (no clock hardware) |
 * | Initialization         | Hardware reset circuit      | Test setup code               |
 *
 * ## What This Mock Enables Testing
 *
 * ✓ Module stop bit manipulation (peripheral enable/disable)
 * ✓ Protection register unlock/lock sequences
 * ✓ Reset source detection logic
 * ✓ Clock configuration register writes
 * ✓ Multi-peripheral initialization sequences
 * ✓ Error handling (e.g., timeout if module stop bit not cleared)
 *
 * ✗ Actual clock frequency changes (no oscillator)
 * ✗ Power consumption measurement (no power gating)
 * ✗ Write protection enforcement (PRCR not simulated)
 * ✗ Reset circuit behavior (no hardware reset)
 *
 * ## Mock State Management
 *
 * **External Variables** (defined in test implementation or mock .c file):
 * - `g_mock_prcr` - Protection register storage (16-bit)
 * - `g_mock_rstsr01` - Reset status registers 0 and 1 (2 bytes)
 * - `g_mock_rstsr2` - Reset status register 2 (1 byte)
 * - `g_mock_system_regs` - Full system register block (~43 bytes)
 *
 * **Initialization**: Tests must initialize mock state before use:
 * @code{.c}
 * // Example initialization in test setup
 * g_mock_prcr = 0xA500;           // Locked state
 * g_mock_system_regs.mstpcrb = 0xFFFFFFFF;  // All modules stopped
 * @endcode
 *
 * @par Module Dependencies:
 * - Standard C library: `<stdint.h>`, `<stdbool.h>` - Integer types
 * - No external dependencies (self-contained mock)
 *
 * @see rx72n_regs.h Unified mock header that includes this file
 * @see mock_rx_onewire_hw.h CMT timer mocks (shares mock_system_regs_t)
 * @see lib/rx_hal/inc/rx72n_system_regs.h Real hardware register definitions
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (No goto)**: ✓ No goto statements
 * - **Rule 2 (Bounded loops)**: ✓ No loops
 * - **Rule 3 (No dynamic allocation)**: ✓ All structures statically allocated
 * - **Rule 4 (Short functions)**: ✓ Inline accessors are 2-3 lines each
 * - **Rule 5 (Assertions)**: N/A (mock definitions only, no validation logic)
 * - **Rule 6 (Minimal scope)**: ✓ Typedefs/enums at file scope (required for visibility)
 * - **Rule 7 (Check returns)**: N/A (accessor functions return pointers, always valid)
 * - **Rule 8 (Limit preprocessor)**: ✓ C23 typed enums used, minimal macros (include guards only)
 * - **Rule 9 (Limit pointers)**: ✓ Single-level pointers in accessor return types
 * - **Rule 10 (Warnings)**: ✓ Compiles with `-Wall -Wextra -Werror`
 *
 * @par SOLID Principles:
 * - **Single Responsibility (S)**: ✓ Sole purpose is system register mocking for tests
 * - **Open/Closed (O)**: ✓ Extensible by adding new enums/accessors without modifying existing code
 * - **Liskov Substitution (L)**: ✓ Mock register types are drop-in replacements for real hardware types
 * - **Interface Segregation (I)**: ✓ Separate accessors for PRCR, RSTSR, system regs (no "fat" interface)
 * - **Dependency Inversion (D)**: ✓ Production code depends on abstract register types, not concrete mock implementation
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 *
 * @version 1.0.0
 * @since 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * System Register Base Addresses (for compatibility)
 * =============================================================================
 */

/**
 * @enum rx_system_addresses_t
 * @brief System register base addresses (documentation-only, not used in mock)
 *
 * @details
 * These are the actual memory-mapped addresses used by real RX72N hardware.
 * **The mock implementation does NOT use these addresses** - mock registers
 * are ordinary C structures allocated in test memory (stack/heap).
 *
 * This enum is retained for documentation purposes and to maintain API
 * compatibility with production code. Tests can reference these constants
 * for address arithmetic or validation logic, but they do not map to actual
 * hardware locations when running on host.
 *
 * ## Real Hardware Memory Map (RX72N)
 *
 * | Address    | Register Block      | Size  | Description                    |
 * |------------|---------------------|-------|--------------------------------|
 * | 0x00080000 | System registers    | ~1KB  | Clock, power, module stop      |
 * | 0x000803FE | PRCR                | 16b   | Protection register            |
 * | 0x0008C290 | RSTSR0/1            | 16b   | Reset status 0 and 1           |
 * | 0x000800C0 | RSTSR2              | 8b    | Reset status 2                 |
 *
 * @see lib/rx_hal/inc/rx72n_system_regs.h Real hardware address definitions
 * @see RX72N User's Manual Section 4.3 Memory Map
 *
 * @since 1.0.0
 */
typedef enum : uint32_t {
  k_system_base_addr = 0x00080000, /**< System register base address (real RX72N MMIO) */
  k_prcr_addr        = 0x000803FE, /**< PRCR (Protection Register) address (real RX72N MMIO) */
} rx_system_addresses_t;

/* =============================================================================
 * Reset Status Register Structures and Constants
 * =============================================================================
 */

/**
 * @enum rx_rstsr_addresses_t
 * @brief Reset status register addresses (documentation-only, not used in mock)
 *
 * @details
 * These are the actual memory-mapped addresses of reset status registers on
 * real RX72N hardware. **The mock implementation does NOT use these addresses** -
 * mock registers are ordinary C variables accessed via `rstsr01()` and `rstsr2()`
 * accessor functions.
 *
 * This enum is retained for documentation and API compatibility with production code.
 *
 * ## Real Hardware Layout (RX72N)
 *
 * | Address    | Register | Size | Description                               |
 * |------------|----------|------|-------------------------------------------|
 * | 0x0008C290 | RSTSR0   | 8b   | Power-on, LVD reset flags                 |
 * | 0x0008C291 | RSTSR1   | 8b   | Cold/warm start flag                      |
 * | 0x000800C0 | RSTSR2   | 8b   | Watchdog, software reset flags            |
 *
 * **Note**: RSTSR0 and RSTSR1 are contiguous in memory (can be read as 16-bit value).
 *
 * @see rstsr01() Mock accessor for RSTSR0 and RSTSR1
 * @see rstsr2() Mock accessor for RSTSR2
 * @see RX72N User's Manual Section 8.2.8 Reset Status Registers
 *
 * @since 1.0.0
 */
typedef enum : uint32_t {
  k_rstsr0_addr = 0x0008C290, /**< Reset Status Register 0 address (real RX72N MMIO) */
  k_rstsr1_addr = 0x0008C291, /**< Reset Status Register 1 address (real RX72N MMIO) */
  k_rstsr2_addr = 0x000800C0, /**< Reset Status Register 2 address (real RX72N MMIO) */
} rx_rstsr_addresses_t;

/**
 * @struct rx_rstsr01_regs_t
 * @brief Mock structure for Reset Status Registers 0 and 1 (contiguous pair)
 *
 * @details
 * Groups RSTSR0 and RSTSR1 into a single structure because they are contiguous
 * in real RX72N hardware (addresses 0x0008C290 and 0x0008C291). This allows
 * reading both registers as a single 16-bit value if needed.
 *
 * ## Purpose of Reset Status Registers
 *
 * These read-only status flags indicate what caused the last MCU reset. Software
 * typically reads these during boot initialization to:
 * - Detect unexpected resets (watchdog, voltage drop)
 * - Distinguish cold start (power-on) from warm start (software reset)
 * - Log reset events for diagnostics
 * - Take recovery actions (e.g., restore saved state after watchdog reset)
 *
 * ## Register Behavior
 *
 * **Real Hardware:**
 * - Flags set automatically by reset controller circuit
 * - Flags cleared by writing 0 to the bit (write-0-to-clear)
 * - Multiple flags can be set if multiple reset sources occurred
 *
 * **Mock:**
 * - Flags manually set by test setup code (simulate reset conditions)
 * - Flags cleared by writing 0 (same as hardware)
 * - No automatic flag setting (tests control all state)
 *
 * ## Memory Layout
 *
 * | Offset | Field   | Size | Flags                    |
 * |--------|---------|------|--------------------------|
 * | 0x00   | rstsr0  | 8b   | PORF, LVD0RF, LVD1RF...  |
 * | 0x01   | rstsr1  | 8b   | CWSF                     |
 *
 * Total Size: 2 bytes (tightly packed)
 *
 * @par Usage Example:
 * @code{.c}
 * // Read reset status on boot
 * volatile rx_rstsr01_regs_t* rst = rstsr01();
 *
 * if (rst->rstsr0 & k_rstsr0_porf) {
 *     printf("Power-on reset detected\n");
 *     // First boot - initialize all state
 * }
 *
 * if (rst->rstsr1 & k_rstsr1_cwsf) {
 *     printf("Cold start (not warm start)\n");
 * }
 *
 * // Clear flags (write 0 to clear)
 * rst->rstsr0 = 0;
 * rst->rstsr1 = 0;
 * @endcode
 *
 * @see rstsr0_bits_t RSTSR0 bit flag definitions
 * @see rstsr1_bits_t RSTSR1 bit flag definitions
 * @see rstsr01() Mock accessor function
 *
 * @since 1.0.0
 */
typedef struct {
  volatile uint8_t rstsr0; /**< Reset Status Register 0 (power-on, LVD flags) */
  volatile uint8_t rstsr1; /**< Reset Status Register 1 (cold/warm start flag) */
} rx_rstsr01_regs_t;

/**
 * @enum rstsr0_bits_t
 * @brief RSTSR0 (Reset Status Register 0) bit flag definitions
 *
 * @details
 * Defines bit masks for RSTSR0 flags. These flags indicate hardware reset sources
 * such as power-on reset and low-voltage detection (LVD).
 *
 * ## Flag Descriptions
 *
 * ### PORF (Power-On Reset Flag)
 * - **Set when**: VCC drops below power-on reset threshold (~1.65V typical)
 * - **Typical cause**: Initial power-up, power supply failure, brownout
 * - **Action**: Perform full system initialization (all peripherals, RAM, etc.)
 *
 * ### LVD0RF (Voltage Monitoring 0 Reset Flag)
 * - **Set when**: VCC drops below LVD0 threshold (configurable, e.g., 2.85V)
 * - **Typical cause**: Voltage dip, power supply issue
 * - **Action**: Check power supply, log event, may need re-calibration
 *
 * ## Usage Pattern
 * @code{.c}
 * volatile rx_rstsr01_regs_t* rst = rstsr01();
 *
 * // Check for power-on reset
 * if (rst->rstsr0 & k_rstsr0_porf) {
 *     // Full initialization required
 *     init_all_peripherals();
 * }
 *
 * // Check for low-voltage reset
 * if (rst->rstsr0 & k_rstsr0_lvd0rf) {
 *     // Log event, check power supply
 *     log_error("LVD reset detected - check power");
 * }
 *
 * // Clear all RSTSR0 flags
 * rst->rstsr0 = 0;
 * @endcode
 *
 * @see rx_rstsr01_regs_t Reset status register structure
 * @see RX72N User's Manual Section 8.2.8.1 RSTSR0
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_rstsr0_porf   = 0x01, /**< Bit 0: Power-On Reset Detect Flag (1=occurred, 0=not occurred) */
  k_rstsr0_lvd0rf = 0x02, /**< Bit 1: Voltage Monitoring 0 Reset Detect Flag (1=occurred) */
} rstsr0_bits_t;

/**
 * @enum rstsr1_bits_t
 * @brief RSTSR1 (Reset Status Register 1) bit flag definitions
 *
 * @details
 * Defines bit masks for RSTSR1 flags. Currently defines only the cold/warm start
 * determination flag (CWSF).
 *
 * ## Flag Description
 *
 * ### CWSF (Cold/Warm Start Determination Flag)
 * - **Set (1)**: Cold start - power was lost, registers/RAM initialized to defaults
 * - **Clear (0)**: Warm start - power maintained, registers/RAM may retain state
 * - **Typical use**: Determine if saved state in backup RAM is valid
 *
 * ## Cold Start vs Warm Start
 *
 * | Event                | CWSF | RAM State      | Action                        |
 * |----------------------|------|----------------|-------------------------------|
 * | Power-on reset       | 1    | Uninitialized  | Full initialization           |
 * | Software reset       | 0    | Preserved      | Restore state, quick reboot   |
 * | Watchdog reset       | 0    | Preserved      | Log fault, restore state      |
 *
 * ## Usage Pattern
 * @code{.c}
 * volatile rx_rstsr01_regs_t* rst = rstsr01();
 *
 * if (rst->rstsr1 & k_rstsr1_cwsf) {
 *     // Cold start - cannot trust RAM contents
 *     printf("Cold start detected - full initialization\n");
 *     init_all_state();
 * } else {
 *     // Warm start - may be able to restore state
 *     printf("Warm start - attempting state recovery\n");
 *     if (validate_backup_state()) {
 *         restore_state_from_backup_ram();
 *     } else {
 *         init_all_state();
 *     }
 * }
 *
 * // Clear flag
 * rst->rstsr1 = 0;
 * @endcode
 *
 * @see rx_rstsr01_regs_t Reset status register structure
 * @see RX72N User's Manual Section 8.2.8.2 RSTSR1
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_rstsr1_cwsf = 0x01, /**< Bit 0: Cold/Warm Start Flag (1=cold, 0=warm) */
} rstsr1_bits_t;

/**
 * @enum rstsr2_bits_t
 * @brief RSTSR2 (Reset Status Register 2) bit flag definitions
 *
 * @details
 * Defines bit masks for RSTSR2 flags. These flags indicate software-triggered
 * and watchdog timer reset sources.
 *
 * ## Flag Descriptions
 *
 * ### IWDTRF (Independent Watchdog Timer Reset Flag)
 * - **Set when**: Independent watchdog timer expired (firmware failed to refresh)
 * - **Typical cause**: Infinite loop, deadlock, firmware crash
 * - **Action**: Log fault, enter safe mode, attempt recovery
 *
 * ### WDTRF (Watchdog Timer Reset Flag)
 * - **Set when**: Standard watchdog timer expired
 * - **Typical cause**: Same as IWDT (firmware hang/crash)
 * - **Action**: Log fault, safe mode, recovery
 *
 * ### SWRF (Software Reset Flag)
 * - **Set when**: Software initiated reset via system control register
 * - **Typical cause**: Firmware update, configuration change, controlled restart
 * - **Action**: Normal - may restore state and continue operation
 *
 * ## Usage Pattern
 * @code{.c}
 * volatile uint8_t* rst2 = rstsr2();
 *
 * // Check for watchdog reset (unexpected)
 * if (*rst2 & k_rstsr2_iwdtrf) {
 *     log_error("IWDT reset - firmware hung!");
 *     enter_safe_mode();
 * }
 *
 * // Check for software reset (expected)
 * if (*rst2 & k_rstsr2_swrf) {
 *     printf("Software reset - normal restart\n");
 *     // May restore state from backup
 * }
 *
 * // Clear all RSTSR2 flags
 * *rst2 = 0;
 * @endcode
 *
 * @see rstsr2() Mock accessor for RSTSR2 register
 * @see RX72N User's Manual Section 8.2.8.3 RSTSR2
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_rstsr2_iwdtrf = 0x01, /**< Bit 0: Independent Watchdog Timer Reset Flag (1=occurred) */
  k_rstsr2_wdtrf  = 0x02, /**< Bit 1: Watchdog Timer Reset Flag (1=occurred) */
  k_rstsr2_swrf   = 0x04, /**< Bit 2: Software Reset Flag (1=occurred) */
} rstsr2_bits_t;

/**
 * @brief Get pointer to mock RSTSR0/RSTSR1 register structure
 *
 * @details
 * Returns a pointer to the mock reset status registers (RSTSR0 and RSTSR1).
 * These registers indicate what caused the last MCU reset (power-on, LVD,
 * cold/warm start).
 *
 * ## Mock Behavior
 *
 * - **Implementation**: Returns pointer to static `g_mock_rstsr01` variable
 *   defined in test code or mock implementation file
 * - **Initialization**: Tests must manually set flags to simulate reset conditions
 * - **Clearing**: Flags cleared by writing 0 to bits (same as hardware)
 *
 * ## Real Hardware Behavior
 *
 * - Flags automatically set by reset controller circuit
 * - Read-only flags (cannot set via software, only clear)
 * - Flags persist until cleared by software
 *
 * @return Volatile pointer to mock RSTSR0/1 register structure (never NULL)
 *
 * @pre Mock hardware state must be initialized (test setup code)
 * @post Returned pointer remains valid until test teardown
 *
 * @note **Thread Safety**: Not thread-safe, tests must provide synchronization
 * @note **Re-entrancy**: Reentrant (returns pointer to same static variable)
 *
 * @warning Do not free or delete returned pointer (points to static storage)
 * @warning Mock does not enforce read-only behavior - tests can write all bits
 *
 * @par Usage Example:
 * @code{.c}
 * // Test setup - simulate power-on reset
 * volatile rx_rstsr01_regs_t* rst = rstsr01();
 * rst->rstsr0 = k_rstsr0_porf | k_rstsr0_lvd0rf;
 * rst->rstsr1 = k_rstsr1_cwsf;
 *
 * // Code under test reads reset status
 * if (rst->rstsr0 & k_rstsr0_porf) {
 *     perform_full_initialization();
 * }
 *
 * // Clear flags (write 0)
 * rst->rstsr0 = 0;
 * rst->rstsr1 = 0;
 * @endcode
 *
 * @see rx_rstsr01_regs_t Reset status register structure
 * @see rstsr0_bits_t RSTSR0 flag definitions
 * @see rstsr1_bits_t RSTSR1 flag definitions
 *
 * @since 1.0.0
 */
volatile rx_rstsr01_regs_t* rstsr01(void);

/**
 * @brief Get pointer to mock RSTSR2 register
 *
 * @details
 * Returns a pointer to the mock reset status register 2 (RSTSR2). This register
 * indicates watchdog timer and software reset events.
 *
 * ## Mock Behavior
 *
 * - **Implementation**: Returns pointer to static `g_mock_rstsr2` variable
 *   defined in test code or mock implementation file
 * - **Initialization**: Tests must manually set flags to simulate reset conditions
 * - **Clearing**: Flags cleared by writing 0 to bits (same as hardware)
 *
 * ## Real Hardware Behavior
 *
 * - Flags automatically set by reset controller circuit
 * - Read-only flags (cannot set via software, only clear)
 * - Flags persist until cleared by software
 *
 * @return Volatile pointer to mock RSTSR2 register (8-bit, never NULL)
 *
 * @pre Mock hardware state must be initialized (test setup code)
 * @post Returned pointer remains valid until test teardown
 *
 * @note **Thread Safety**: Not thread-safe, tests must provide synchronization
 * @note **Re-entrancy**: Reentrant (returns pointer to same static variable)
 *
 * @warning Do not free or delete returned pointer (points to static storage)
 * @warning Mock does not enforce read-only behavior - tests can write all bits
 *
 * @par Usage Example:
 * @code{.c}
 * // Test setup - simulate watchdog reset
 * volatile uint8_t* rst2 = rstsr2();
 * *rst2 = k_rstsr2_iwdtrf;
 *
 * // Code under test checks reset source
 * if (*rst2 & k_rstsr2_iwdtrf) {
 *     log_error("IWDT reset detected");
 *     enter_safe_mode();
 * }
 *
 * // Clear flags
 * *rst2 = 0;
 * @endcode
 *
 * @see rstsr2_bits_t RSTSR2 flag definitions
 * @see rstsr01() Accessor for RSTSR0 and RSTSR1
 *
 * @since 1.0.0
 */
volatile uint8_t* rstsr2(void);

/* =============================================================================
 * System Register Structure (minimal for compatibility)
 * =============================================================================
 */

/**
 * @def RX_SYSTEM_REGS_T_DEFINED
 * @brief Include guard for rx_system_regs_t typedef to prevent redefinition
 *
 * @details
 * This macro ensures `rx_system_regs_t` is defined only once across multiple
 * header includes. Multiple mock headers may define this type, so the guard
 * prevents "redefinition" compilation errors.
 *
 * @see rx_system_regs_t System register structure typedef
 *
 * @since 1.0.0
 */
#ifndef RX_SYSTEM_REGS_T_DEFINED
#define RX_SYSTEM_REGS_T_DEFINED

/**
 * @struct rx_system_regs_t
 * @brief Mock RX72N system control register structure
 *
 * @details
 * Simulates the RX72N system register block for host-side unit testing. This
 * structure contains registers for clock configuration, module stop control
 * (power gating), and system mode control.
 *
 * ## Register Categories
 *
 * ### Mode and System Control
 * - **mdmonr**: Mode Monitor Register (boot mode, endianness detection)
 * - **syscr0/1**: System Control Registers (RAM wait states, external bus config)
 * - **sbycr**: Standby Control Register (sleep/standby mode configuration)
 *
 * ### Module Stop Control (Power Management)
 * - **mstpcra/b/c/d**: Module Stop Control Registers (32-bit each)
 *   - Each bit controls one peripheral module
 *   - Bit=1: Module stopped (clock gated, low power)
 *   - Bit=0: Module running (clock enabled)
 *   - Default after reset: typically all 1s (all modules stopped)
 *
 * ### Clock Configuration
 * - **sckcr**: System Clock Control Register (ICLK, PCLK, BCLK, FCLK dividers)
 * - **sckcr2/3**: Extended clock control (additional dividers, clock sources)
 * - **pllcr/2**: PLL Control Registers (PLL enable, dividers, multipliers)
 *
 * ## Mock vs Real Hardware
 *
 * | Feature                | Real Hardware       | Mock Implementation      |
 * |------------------------|---------------------|--------------------------|
 * | Register location      | MMIO at 0x00080000  | C structure in memory    |
 * | Module stop effects    | Gates clocks        | Sets flag only           |
 * | PLL configuration      | Controls oscillator | No effect                |
 * | Protection enforcement | PRCR register       | Not enforced             |
 * | Initialization         | Hardware reset      | Test setup code          |
 *
 * ## Memory Layout
 *
 * | Offset | Field     | Size | Description                          | Access |
 * |--------|-----------|------|--------------------------------------|--------|
 * | 0x00   | mdmonr    | 16b  | Mode monitor                         | RO     |
 * | 0x02   | reserved0 | 4B   | Reserved area                        | -      |
 * | 0x06   | syscr0    | 16b  | System control 0                     | R/W    |
 * | 0x08   | syscr1    | 16b  | System control 1                     | R/W    |
 * | 0x0A   | reserved1 | 2B   | Reserved area                        | -      |
 * | 0x0C   | sbycr     | 16b  | Standby control                      | R/W    |
 * | 0x0E   | reserved2 | 2B   | Reserved area                        | -      |
 * | 0x10   | mstpcra   | 32b  | Module stop control A (DMA, EXDMAC)  | R/W    |
 * | 0x14   | mstpcrb   | 32b  | Module stop control B (USB, SCI)     | R/W    |
 * | 0x18   | mstpcrc   | 32b  | Module stop control C (RAM, TMR)     | R/W    |
 * | 0x1C   | mstpcrd   | 32b  | Module stop control D (MTU, GPT)     | R/W    |
 * | 0x20   | sckcr     | 32b  | System clock control                 | R/W    |
 * | 0x24   | sckcr2    | 16b  | System clock control 2               | R/W    |
 * | 0x26   | sckcr3    | 16b  | System clock control 3               | R/W    |
 * | 0x28   | pllcr     | 16b  | PLL control                          | R/W    |
 * | 0x2A   | pllcr2    | 8b   | PLL control 2                        | R/W    |
 *
 * **Total Size**: ~43 bytes (platform-dependent due to reserved field padding)
 *
 * @par Field Constraints and Relationships:
 *
 * | Field    | Valid Range         | Notes                                     |
 * |----------|---------------------|-------------------------------------------|
 * | mdmonr   | 0x0000-0xFFFF       | Read-only, reflects boot mode pins        |
 * | mstpcra  | 0x00000000-0xFFFFFFFF | Bit N controls module N                 |
 * | mstpcrb  | 0x00000000-0xFFFFFFFF | See rx_module_stop_bits_b_t for mapping |
 * | mstpcrc  | 0x00000000-0xFFFFFFFF | Bit N controls module N                 |
 * | mstpcrd  | 0x00000000-0xFFFFFFFF | Bit N controls module N                 |
 * | sckcr    | Depends on dividers | See RX72N manual for valid combinations   |
 *
 * @par Usage Example - Module Stop Control:
 * @code{.c}
 * // Access mock system registers
 * volatile rx_system_regs_t* sys = system_regs();
 *
 * // Enable USB0 peripheral (clear module stop bit)
 * sys->mstpcrb &= ~(1U << k_mstpb_usb0);
 *
 * // Verify USB0 enabled
 * assert((sys->mstpcrb & (1U << k_mstpb_usb0)) == 0);
 *
 * // Disable USB0 to save power (set module stop bit)
 * sys->mstpcrb |= (1U << k_mstpb_usb0);
 * @endcode
 *
 * @par Usage Example - Clock Configuration:
 * @code{.c}
 * volatile rx_system_regs_t* sys = system_regs();
 *
 * // Configure system clocks (example: ICLK=240MHz, PCLK=120MHz)
 * sys->sckcr = (0 << 24) |  // ICK[3:0]=0000 (ICLK = PLL/1 = 240MHz)
 *              (1 << 8)  |  // PCK[3:0]=0001 (PCLK = PLL/2 = 120MHz)
 *              (2 << 4);    // BCK[3:0]=0010 (BCLK = PLL/4 = 60MHz)
 *
 * // Enable PLL
 * sys->pllcr = 0x0F01;  // PLL multiplier = 16, divider = 2
 * @endcode
 *
 * @par Usage Example - Reserved Field Padding:
 * @code{.c}
 * // Reserved fields maintain register offset compatibility with real hardware
 * volatile rx_system_regs_t* sys = system_regs();
 *
 * // syscr0 is at offset 0x06 (after 2-byte mdmonr + 4-byte reserved0)
 * size_t syscr0_offset = offsetof(rx_system_regs_t, syscr0);
 * assert(syscr0_offset == 6);
 * @endcode
 *
 * @see system_regs() Mock accessor function (if defined in mock_rx_onewire_hw.h)
 * @see prcr_reg() Protection register accessor
 * @see rx_module_stop_bits_b_t MSTPCRB bit assignments
 * @see RX72N User's Manual Section 9 System Control
 *
 * @since 1.0.0
 */
typedef struct {
  volatile uint16_t mdmonr;   /**< Mode Monitor Register (boot mode, endianness) */
  uint8_t reserved0[4];       /**< Reserved (maintains offset compatibility with hardware) */
  volatile uint16_t syscr0;   /**< System Control Register 0 (RAM wait states, RAME) */
  volatile uint16_t syscr1;   /**< System Control Register 1 (external bus configuration) */
  uint8_t reserved1[2];       /**< Reserved (maintains offset compatibility with hardware) */
  volatile uint16_t sbycr;    /**< Standby Control Register (sleep/standby mode config) */
  uint8_t reserved2[2];       /**< Reserved (maintains offset compatibility with hardware) */
  volatile uint32_t mstpcra;  /**< Module Stop Control Register A (DMA, EXDMAC modules) */
  volatile uint32_t mstpcrb;  /**< Module Stop Control Register B (USB, CAN, I2C, SPI, SCI modules) */
  volatile uint32_t mstpcrc;  /**< Module Stop Control Register C (RAM, ROM, TMR, CMT modules) */
  volatile uint32_t mstpcrd;  /**< Module Stop Control Register D (MTU, GPT, POEG modules) */
  volatile uint32_t sckcr;    /**< System Clock Control Register (ICLK, PCLK, BCLK, FCLK dividers) */
  volatile uint16_t sckcr2;   /**< System Clock Control Register 2 (USB, SDCLK dividers) */
  volatile uint16_t sckcr3;   /**< System Clock Control Register 3 (clock out configuration) */
  volatile uint16_t pllcr;    /**< PLL Control Register (PLL divider, multiplier) */
  volatile uint8_t pllcr2;    /**< PLL Control Register 2 (PLL enable/disable) */
} rx_system_regs_t;
#endif /* RX_SYSTEM_REGS_T_DEFINED */

/**
 * @enum rx_module_stop_bits_b_t
 * @brief MSTPCRB (Module Stop Control Register B) bit assignments
 *
 * @details
 * Defines bit positions for peripheral modules controlled by MSTPCRB register.
 * Each bit controls clock gating for one peripheral module:
 * - **Bit=1**: Module stopped (clock gated, low power, peripheral inactive)
 * - **Bit=0**: Module running (clock enabled, peripheral ready for use)
 *
 * ## Module Stop Control Usage
 *
 * **Why Module Stop Control Exists:**
 * - **Power savings**: Disabling unused peripherals reduces power consumption
 * - **Default state**: Most modules stopped after reset (MSTPCRB ≈ 0xFFFFFFFF)
 * - **Initialization requirement**: Must clear module stop bit BEFORE configuring peripheral
 *
 * **Typical Initialization Sequence:**
 * 1. Unlock protection register (PRCR)
 * 2. Clear module stop bit (enable peripheral clock)
 * 3. Re-lock protection register
 * 4. Configure peripheral registers
 * 5. Enable peripheral interrupts (if needed)
 *
 * ## MSTPCRB Module Assignments (RX72N)
 *
 * | Bit | Module | Description                               | STAR Usage |
 * |-----|--------|-------------------------------------------|------------|
 * | 19  | USB0   | USB 2.0 Full-Speed module                 | ✓ Used     |
 * | 23  | CRC    | CRC calculator (hardware accelerator)     | ✓ Used     |
 * | 25  | SCI2   | Serial Communication Interface 2          | Unused     |
 * | 26  | SCI1   | Serial Communication Interface 1          | Unused     |
 * | 27  | SCI0   | Serial Communication Interface 0          | Unused     |
 *
 * **Note**: This enum defines only modules actually used in STAR project. See
 * RX72N User's Manual Section 9.2.4 for complete MSTPCRB bit assignments.
 *
 * ## Module Stop Bit Encoding
 *
 * Bit positions are chosen to match RX72N hardware exactly (no translation needed):
 * @code{.c}
 * // Enable USB0 (clear bit 19 in MSTPCRB)
 * volatile rx_system_regs_t* sys = system_regs();
 * sys->mstpcrb &= ~(1U << k_mstpb_usb0);
 *
 * // Check if USB0 enabled
 * bool usb_enabled = (sys->mstpcrb & (1U << k_mstpb_usb0)) == 0;
 * @endcode
 *
 * @par Usage Example - USB Initialization:
 * @code{.c}
 * void usb_init(void)
 * {
 *     volatile rx_system_regs_t* sys = system_regs();
 *     volatile uint16_t* prcr = prcr_reg();
 *
 *     // 1. Unlock protection
 *     *prcr = 0xA502;  // PRKEY=0xA5, PRC1=1
 *
 *     // 2. Enable USB0 module (clear module stop bit)
 *     sys->mstpcrb &= ~(1U << k_mstpb_usb0);
 *
 *     // 3. Re-lock protection
 *     *prcr = 0xA500;
 *
 *     // 4. Configure USB peripheral (registers now accessible)
 *     usb_configure_endpoints();
 * }
 * @endcode
 *
 * @par Usage Example - Power Saving:
 * @code{.c}
 * void power_save_mode(void)
 * {
 *     volatile rx_system_regs_t* sys = system_regs();
 *     volatile uint16_t* prcr = prcr_reg();
 *
 *     *prcr = 0xA502;
 *
 *     // Disable unused peripherals to save power
 *     sys->mstpcrb |= (1U << k_mstpb_usb0);  // Stop USB0
 *     sys->mstpcrb |= (1U << k_mstpb_crc);   // Stop CRC
 *
 *     *prcr = 0xA500;
 *
 *     // System now in lower power state
 * }
 * @endcode
 *
 * @see rx_system_regs_t System register structure containing mstpcrb
 * @see prcr_reg() Protection register accessor (must unlock before writing mstpcrb)
 * @see RX72N User's Manual Section 9.2.4 MSTPCRB Register
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_mstpb_usb0 = 19, /**< Bit 19: USB0 module stop bit (1=stopped, 0=running) */
  k_mstpb_crc  = 23, /**< Bit 23: CRC calculator module stop bit (1=stopped, 0=running) */
} rx_module_stop_bits_b_t;

/* =============================================================================
 * PRCR Mock (Protection Register)
 * =============================================================================
 */

/**
 * @var g_mock_prcr
 * @brief Mock storage for PRCR (Protection Register)
 *
 * @details
 * External 16-bit variable simulating the RX72N Protection Register (PRCR).
 * This register controls write access to critical system registers including
 * module stop control (MSTPCRA/B/C/D), clock configuration (SCKCR, PLLCR),
 * and power mode control.
 *
 * ## PRCR Register Format (16-bit)
 *
 * | Bits   | Field | Description                                      |
 * |--------|-------|--------------------------------------------------|
 * | 15-8   | PRKEY | Protection Key (0xA5 = unlock, other = lock)     |
 * | 7-4    | -     | Reserved (read as 0)                             |
 * | 3      | PRC3  | LVD register protection (1=writable, 0=protected)|
 * | 2      | -     | Reserved (read as 0)                             |
 * | 1      | PRC1  | Low-power mode protection (1=writable)           |
 * | 0      | PRC0  | Clock generation protection (1=writable)         |
 *
 * ## Unlock/Lock Sequence
 *
 * **Unlock (Enable Writes):**
 * @code{.c}
 * volatile uint16_t* prcr = prcr_reg();
 * *prcr = 0xA502;  // PRKEY=0xA5, PRC1=1, PRC0=0
 * // System registers now writable
 * @endcode
 *
 * **Lock (Disable Writes):**
 * @code{.c}
 * *prcr = 0xA500;  // PRKEY=0xA5, all PRC bits=0
 * // System registers protected again
 * @endcode
 *
 * ## Mock vs Real Hardware
 *
 * | Feature                | Real Hardware          | Mock Implementation       |
 * |------------------------|------------------------|---------------------------|
 * | Write protection       | Enforced by hardware   | NOT enforced (flag only)  |
 * | PRKEY validation       | Checked on every write | NOT checked               |
 * | PRC bit effects        | Gates register writes  | No effect                 |
 * | Initialization         | 0xA500 (locked)        | Test setup responsibility |
 *
 * **CRITICAL**: Mock does NOT enforce write protection. Tests must verify that
 * code correctly unlocks PRCR before writing system registers, but the mock will
 * not prevent writes if PRCR is locked.
 *
 * @note **Storage Class**: `extern` - must be defined in test code or mock .c file
 * @note **Volatility**: `volatile` - simulates hardware register behavior
 * @note **Initialization**: Tests must initialize to 0xA500 (locked) in setup
 *
 * @see prcr_reg() Inline accessor function returning pointer to this variable
 * @see RX72N User's Manual Section 9.2.2 PRCR Register
 *
 * @since 1.0.0
 */
extern volatile uint16_t g_mock_prcr;

/**
 * @brief Get pointer to mock PRCR (Protection Register)
 *
 * @details
 * Returns a pointer to the mock Protection Register (PRCR), which controls write
 * access to critical system registers. This inline accessor provides the same
 * interface as the real hardware accessor, enabling transparent testing of
 * protection register unlock/lock sequences.
 *
 * ## PRCR Protection Mechanism (Real Hardware)
 *
 * RX72N hardware prevents accidental writes to critical system registers (clock
 * configuration, module stop control, power modes) by requiring a two-step unlock:
 * 1. Write PRKEY=0xA5 to bits 15-8
 * 2. Set appropriate PRC bit(s) to 1 (PRC0, PRC1, PRC3)
 *
 * Protected registers become writable only when both conditions are met. Writing
 * any value other than 0xA5 to PRKEY immediately re-locks all registers.
 *
 * ## Protected Register Categories
 *
 * | PRC Bit | Protected Registers              | Example Registers           |
 * |---------|----------------------------------|-----------------------------|
 * | PRC0    | Clock generation circuit         | SCKCR, PLLCR, MOSCCR        |
 * | PRC1    | Low-power mode, module stop      | MSTPCRA/B/C/D, SBYCR, SYSCR |
 * | PRC3    | LVD (Low-Voltage Detection)      | LVCMPCR, LVDLVLR            |
 *
 * ## Mock Behavior and Limitations
 *
 * **What the Mock Does:**
 * - Stores PRCR value in `g_mock_prcr` (16-bit global variable)
 * - Allows reading/writing via returned pointer
 * - Maintains state across multiple accesses
 *
 * **What the Mock Does NOT Do:**
 * - ✗ Does NOT enforce write protection (system registers writable regardless of PRCR)
 * - ✗ Does NOT validate PRKEY (accepts any value)
 * - ✗ Does NOT automatically lock after writes (stays in set state)
 *
 * **Testing Implications:**
 * Tests can verify that code ATTEMPTS to unlock PRCR correctly, but cannot verify
 * that writes would be BLOCKED if PRCR is locked. Tests must check:
 * 1. PRCR unlocked before writing system registers
 * 2. Correct PRKEY and PRC bits used
 * 3. PRCR re-locked after critical section
 *
 * @return Volatile pointer to 16-bit mock PRCR register (never NULL)
 *
 * @pre `g_mock_prcr` must be defined in test code or mock implementation file
 * @post Returned pointer remains valid for lifetime of `g_mock_prcr` variable
 *
 * @note **Thread Safety**: Not thread-safe, tests must provide synchronization
 * @note **Re-entrancy**: Reentrant (returns pointer to same global variable)
 * @note **Performance**: Zero overhead (inline function, direct pointer return)
 *
 * @warning Do not free or delete returned pointer (points to global storage)
 * @warning Mock does not enforce protection - tests must verify unlock/lock logic
 *
 * @par Usage Example - Typical Protection Sequence:
 * @code{.c}
 * void configure_system_clocks(void)
 * {
 *     volatile uint16_t* prcr = prcr_reg();
 *     volatile rx_system_regs_t* sys = system_regs();
 *
 *     // 1. Unlock protection (PRC0=1 for clock registers)
 *     *prcr = 0xA501;  // PRKEY=0xA5, PRC0=1
 *
 *     // 2. Configure clock (now writable on real hardware)
 *     sys->sckcr = 0x01020304;
 *
 *     // 3. Re-lock protection
 *     *prcr = 0xA500;  // Clear all PRC bits
 * }
 * @endcode
 *
 * @par Usage Example - Test Verification:
 * @code{.c}
 * void test_prcr_unlock_sequence(void)
 * {
 *     g_mock_prcr = 0xA500;  // Initialize to locked state
 *
 *     volatile uint16_t* prcr = prcr_reg();
 *
 *     // Verify initial state
 *     assert(*prcr == 0xA500);
 *
 *     // Code under test should unlock PRCR
 *     configure_system_clocks();
 *
 *     // Verify final state (should be re-locked)
 *     assert(*prcr == 0xA500);
 * }
 * @endcode
 *
 * @par Usage Example - Module Stop Control:
 * @code{.c}
 * void enable_usb_peripheral(void)
 * {
 *     volatile uint16_t* prcr = prcr_reg();
 *     volatile rx_system_regs_t* sys = system_regs();
 *
 *     // Unlock (PRC1=1 for module stop registers)
 *     *prcr = 0xA502;  // PRKEY=0xA5, PRC1=1
 *
 *     // Clear USB0 module stop bit
 *     sys->mstpcrb &= ~(1U << k_mstpb_usb0);
 *
 *     // Re-lock
 *     *prcr = 0xA500;
 * }
 * @endcode
 *
 * @see g_mock_prcr Global variable storage for mock PRCR register
 * @see rx_system_regs_t System register structure with protected registers
 * @see RX72N User's Manual Section 9.2.2 PRCR Register
 *
 * @since 1.0.0
 */
static inline volatile uint16_t* prcr_reg(void)
{
  return &g_mock_prcr;
}

#ifdef __cplusplus
}
#endif
