/**
 * @file rx_register_protection.h
 * @brief RX72N Register Protection Control (PRCR) - Write Protection Constants
 *
 * @details
 * Provides centralized definitions for the Protection Register (PRCR) unlock/lock
 * sequences used throughout the RX72N firmware to safely modify write-protected
 * system registers. The PRCR mechanism prevents accidental corruption of critical
 * configuration registers through a key-code based protection scheme.
 *
 * ## Hardware Protection Mechanism
 *
 * The RX72N implements hardware write protection on critical system registers to
 * prevent accidental modification from:
 * - Software bugs (pointer errors, array overruns)
 * - EMI-induced register corruption
 * - Malicious code (if present)
 *
 * Protected register groups:
 * - **PRC0**: Clock Generation Circuit (CGC) registers
 * - **PRC1**: Module Stop Control (MSTPCR) registers
 * - **PRC3**: Low-Voltage Detection (LVD) registers
 *
 * ## Write Protection Flow
 *
 * @msc
 * Code, PRCR, "Protected Registers";
 *
 * ... [label="Attempt Direct Write (FAILS)"];
 * Code => "Protected Registers" [label="Write value"];
 * "Protected Registers" box "Protected Registers" [label="IGNORED\n(protection active)"];
 * "Protected Registers" => Code [label="Write has no effect"];
 *
 * ... [label="Correct Unlock Sequence (SUCCEEDS)"];
 * Code => PRCR [label="Write 0xA502\n(unlock PRC1)"];
 * PRCR box PRCR [label="Protection\ndisabled"];
 * Code => "Protected Registers" [label="Write value"];
 * "Protected Registers" box "Protected Registers" [label="ACCEPTED"];
 * Code => PRCR [label="Write 0xA500\n(lock)"];
 * PRCR box PRCR [label="Protection\nre-enabled"];
 * @endmsc
 *
 * ## Key Code Mechanism
 *
 * PRCR register format (16-bit):
 * ```
 * Bits 15-8: Key Code (must be 0xA5)
 * Bit 3: PRC3 (LVD protection)
 * Bit 1: PRC1 (MSTPCR protection)
 * Bit 0: PRC0 (CGC protection)
 * ```
 *
 * **Security**: Writing any value without the 0xA5 key code in upper byte has
 * no effect (ignored). This prevents accidental unlock from bit flips or
 * pointer errors.
 *
 * ## Protected Register Groups
 *
 * | Group | Bit | Protected Registers | Purpose | STAR Usage |
 * |-------|-----|---------------------|---------|------------|
 * | PRC0 | 0 | SYSTEM.SCKCR, PLLCR, etc. | Clock frequency config | Clock init only |
 * | PRC1 | 1 | SYSTEM.MSTPCRA/B/C/D | Peripheral enable/disable | Frequent (register guard) |
 * | PRC3 | 3 | SYSTEM.LVCMPCR, LVDXCR | Low-voltage detection | Not used in STAR |
 *
 * ## Usage Patterns
 *
 * **Pattern 1: Unlock Single Group (Most Common)**
 * ```c
 * SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;  // Unlock MSTPCR only
 * SYSTEM.MSTPCRA.LONG &= ~(1 << 15);          // Enable CMT0
 * SYSTEM.PRCR.WORD = k_rx_prcr_lock;          // Re-lock immediately
 * ```
 *
 * **Pattern 2: Unlock Multiple Groups (Clock Init)**
 * ```c
 * SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc0_prc1;  // Unlock CGC + MSTPCR
 * SYSTEM.PLLCR.WORD = 0x2700;                      // Configure PLL
 * SYSTEM.SCKCR.LONG = 0x21C21211;                  // Configure clocks
 * SYSTEM.MSTPCRA.LONG = 0x00000000;                // Enable peripherals
 * SYSTEM.PRCR.WORD = k_rx_prcr_lock;               // Re-lock all
 * ```
 *
 * **Pattern 3: Critical Section (Register Guard)**
 * ```c
 * uint32_t saved_ipl = disable_interrupts();       // Atomic section
 * SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;
 * restore_mstpcr_registers();                      // Refresh corrupted regs
 * SYSTEM.PRCR.WORD = k_rx_prcr_lock;
 * restore_interrupts(saved_ipl);                   // End atomic
 * ```
 *
 * ## Performance Characteristics
 *
 * - **Write latency**: 1 bus cycle (4.17 ns @ 240 MHz ICLK)
 * - **Unlock overhead**: 2 writes (unlock + lock) = ~8 ns
 * - **Protection check**: Hardware (zero CPU overhead)
 * - **Memory mapped**: 0x000803FE (SYSTEM.PRCR)
 *
 * ## Hardware Requirements
 *
 * | Requirement | Value | Notes |
 * |-------------|-------|-------|
 * | MCU | RX72N | PRCR at 0x000803FE |
 * | Register Size | 16-bit | Must use WORD access |
 * | Key Code | 0xA5 | Upper 8 bits required |
 * | Protection Bits | PRC0, PRC1, PRC3 | Lower 4 bits |
 *
 * @par Module Dependencies:
 * - **NO dependencies** (pure constant definitions)
 * - Used by: Clock init, module stop control, register guard, power management
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1**: [OK] No code (only data definitions)
 * - **Rule 2**: [OK] No loops
 * - **Rule 3**: [OK] No dynamic allocation (compile-time constants)
 * - **Rule 4**: [OK] No functions
 * - **Rule 5**: [OK] N/A (no functions)
 * - **Rule 6**: [OK] File scope only (header file)
 * - **Rule 7**: [OK] No functions
 * - **Rule 8**: [OK] C23 typed enums for all constants
 * - **Rule 9**: [OK] No pointers
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility**: Only defines PRCR constants, no logic
 * - **Open/Closed**: Extensible (add new combinations) without modification
 * - **Liskov Substitution**: N/A (no inheritance)
 * - **Interface Segregation**: Minimal interface (single enum)
 * - **Dependency Inversion**: N/A (no dependencies)
 *
 * @par Usage Example - Module Enable:
 * @code{.c}
 * #include "rx_register_protection.h"
 * #include "rx72n_regs.h"
 *
 * // Enable CMT0 timer module (in MSTPCRA bit 15)
 * void enable_cmt0(void) {
 *     typedef enum : uint32_t {
 *         k_mstpcra_cmt0_bit = 15,  // CMT0 module stop bit
 *     } mstpcr_bits_t;
 *
 *     // Unlock MSTPCR protection
 *     SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;
 *
 *     // Clear module stop bit (0 = enabled)
 *     SYSTEM.MSTPCRA.LONG &= ~(1UL << k_mstpcra_cmt0_bit);
 *
 *     // Re-lock protection immediately
 *     SYSTEM.PRCR.WORD = k_rx_prcr_lock;
 * }
 * @endcode
 *
 * @par Usage Example - Clock Configuration:
 * @code{.c}
 * // Configure system clocks during initialization
 * void configure_system_clocks(void) {
 *     // Unlock both CGC and MSTPCR groups
 *     SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc0_prc1;
 *
 *     // Configure PLL (protected by PRC0)
 *     SYSTEM.PLLCR.WORD = 0x1300;      // PLL multiplier x10 (STAR: 24 MHz * 10 = 240 MHz)
 *     SYSTEM.PLLCR2.BYTE = 0x00;       // Enable PLL
 *
 *     // Wait for PLL stabilization
 *     while (SYSTEM.OSCOVFSR.BIT.PLOVF == 0) {
 *         // Busy wait (~2ms)
 *     }
 *
 *     // Select PLL as system clock (protected by PRC0)
 *     SYSTEM.SCKCR3.WORD = 0x0400;     // CKSEL = PLL
 *
 *     // Enable all peripheral modules (protected by PRC1)
 *     SYSTEM.MSTPCRA.LONG = 0x00000000;
 *     SYSTEM.MSTPCRB.LONG = 0x00000000;
 *
 *     // Re-lock all protection
 *     SYSTEM.PRCR.WORD = k_rx_prcr_lock;
 * }
 * @endcode
 *
 * @par Usage Example - Atomic Register Refresh (Register Guard):
 * @code{.c}
 * // Restore corrupted MSTPCR registers atomically
 * void restore_mstpcr_atomically(void) {
 *     // Brief interrupt disable for atomicity
 *     uint32_t saved_ipl = __get_ipl();
 *     __set_ipl(15);  // Disable interrupts
 *
 *     // Unlock MSTPCR group
 *     SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;
 *
 *     // Restore golden values (from register guard)
 *     SYSTEM.MSTPCRA.LONG = s_golden_mstpcra;
 *     SYSTEM.MSTPCRB.LONG = s_golden_mstpcrb;
 *     SYSTEM.MSTPCRC.LONG = s_golden_mstpcrc;
 *     SYSTEM.MSTPCRD.LONG = s_golden_mstpcrd;
 *
 *     // Re-lock protection
 *     SYSTEM.PRCR.WORD = k_rx_prcr_lock;
 *
 *     // Restore interrupts
 *     __set_ipl(saved_ipl);
 * }
 * @endcode
 *
 * @see [RX72N User's Manual](../../docs/rx72n_manual.pdf) Chapter 9: System Control
 * @see [RX72N User's Manual](../../docs/rx72n_manual.pdf) Section 9.2.2: Protection Register (PRCR)
 * @see [rx_register_guard.c](../../src/rx_register_guard.c) Uses PRCR for MSTPCR refresh
 * @see [rx_clock_power_init.c](../../src/rx_clock_power_init.c) Uses PRCR for clock config
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Protection Register (PRCR) Values
 * =============================================================================
 */

/**
 * @enum rx_prcr_values_t
 * @brief PRCR register unlock/lock values for write-protected system registers
 *
 * @details
 * Defines all valid PRCR (Protect Register) values for unlocking and locking
 * write-protected system registers on the RX72N. The PRCR uses a key code
 * mechanism (0xA5 in upper byte) combined with protection enable bits (lower
 * byte) to selectively unlock register groups.
 *
 * **Register Format** (16-bit SYSTEM.PRCR at 0x000803FE):
 * ```
 * Bits 15-8: [0xA5] Key code (required for any write to take effect)
 * Bits 7-4:  [Reserved] Always write 0
 * Bit 3:     [PRC3] Low-Voltage Detection protection (1=unlocked, 0=locked)
 * Bit 2:     [Reserved] Always write 0
 * Bit 1:     [PRC1] Module Stop Control protection (1=unlocked, 0=locked)
 * Bit 0:     [PRC0] Clock Generation Circuit protection (1=unlocked, 0=locked)
 * ```
 *
 * **Key Code Security**:
 * - Writing without 0xA5 key -> ignored (no effect)
 * - Prevents accidental unlock from pointer errors
 * - Prevents unlock from EMI-induced bit flips
 *
 * **Protection Groups**:
 * - **PRC0** (bit 0): Protects SCKCR, PLLCR, PLLCR2, SCKCR3, MOSCWTCR, etc.
 * - **PRC1** (bit 1): Protects MSTPCRA, MSTPCRB, MSTPCRC, MSTPCRD
 * - **PRC3** (bit 3): Protects LVCMPCR, LVDXCR, LVDYCR
 *
 * ## Usage Guidelines
 *
 * 1. **Minimize unlock duration**: Unlock, modify, lock immediately
 * 2. **Use narrowest scope**: Only unlock groups you need to modify
 * 3. **Atomic for critical sections**: Disable interrupts during unlock
 * 4. **Always re-lock**: Even on error paths (use cleanup pattern)
 *
 * ## STAR Project Usage Frequency
 *
 * | Constant | Frequency | Context |
 * |----------|-----------|---------|
 * | k_rx_prcr_unlock_prc1 | High | Register guard refresh (1-10 Hz) |
 * | k_rx_prcr_unlock_prc0_prc1 | Once | Clock initialization at boot |
 * | k_rx_prcr_unlock_prc1_prc3 | Never | LVD not used in STAR |
 * | k_rx_prcr_unlock_all | Never | Overly broad, avoid |
 * | k_rx_prcr_lock | High | Always called after unlock |
 *
 * @par Field Constraints:
 * | Constant | Key | PRC3 | PRC1 | PRC0 | Notes |
 * |----------|-----|------|------|------|-------|
 * | unlock_prc1 | 0xA5 | 0 | 1 | 0 | Most common (MSTPCR) |
 * | unlock_prc0_prc1 | 0xA5 | 0 | 1 | 1 | Clock init |
 * | unlock_prc1_prc3 | 0xA5 | 1 | 1 | 1 | With LVD (unused) |
 * | unlock_all | 0xA5 | 1 | 1 | 1 | Avoid (too broad) |
 * | lock | 0xA5 | 0 | 0 | 0 | Always used |
 *
 * @par Usage Example - Typical Pattern:
 * @code{.c}
 * void enable_peripheral_module(void) {
 *     // Unlock only what's needed (PRC1 for MSTPCR)
 *     SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;
 *
 *     // Modify protected register
 *     SYSTEM.MSTPCRA.LONG &= ~(1UL << 15);  // Enable CMT0
 *
 *     // Re-lock immediately
 *     SYSTEM.PRCR.WORD = k_rx_prcr_lock;
 * }
 * @endcode
 *
 * @see SYSTEM.PRCR Hardware register at 0x000803FE
 * @see rx_register_guard_refresh() Uses PRC1 unlock for MSTPCR refresh
 * @see rx_clock_power_init() Uses PRC0+PRC1 unlock for clock config
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Unlock PRC1 only (Module Stop Control registers)
   *
   * @details
   * Unlocks write protection for MSTPCRA, MSTPCRB, MSTPCRC, MSTPCRD registers
   * while keeping clock generation (PRC0) and LVD (PRC3) registers protected.
   *
   * **Value**: 0xA502
   * - Key code: 0xA5 (required)
   * - PRC3: 0 (LVD registers remain locked)
   * - PRC1: 1 (MSTPCR registers unlocked)
   * - PRC0: 0 (Clock registers remain locked)
   *
   * **Protected Registers** (when unlocked):
   * - SYSTEM.MSTPCRA (0x00080010) - Module stop control A
   * - SYSTEM.MSTPCRB (0x00080014) - Module stop control B
   * - SYSTEM.MSTPCRC (0x00080018) - Module stop control C
   * - SYSTEM.MSTPCRD (0x0008001C) - Module stop control D
   *
   * **Use Cases**:
   * - Enabling/disabling peripheral modules at runtime
   * - Register guard periodic refresh (most common)
   * - Dynamic power management
   *
   * **STAR Project Usage**: **HIGH FREQUENCY**
   * - Called during register guard refresh (1-10 Hz)
   * - Called during peripheral enable/disable
   * - ~10,000+ times per hour in typical operation
   *
   * @par Rationale:
   * This is the most commonly used unlock value because:
   * - MSTPCR registers change frequently (module enable/disable)
   * - Clock registers (PRC0) rarely change after boot
   * - Narrowest scope reduces risk of accidental corruption
   *
   * @par Example - Enable SCI0 (UART):
   * @code{.c}
   * typedef enum : uint32_t {
   *     k_mstpcrb_sci0_bit = 31,  // SCI0 in MSTPCRB bit 31
   * } mstpcrb_bits_t;
   *
   * SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;
   * SYSTEM.MSTPCRB.LONG &= ~(1UL << k_mstpcrb_sci0_bit);  // 0 = enable
   * SYSTEM.PRCR.WORD = k_rx_prcr_lock;
   * @endcode
   *
   * @warning Interrupts are NOT disabled automatically. For atomic operations,
   *          disable interrupts explicitly before unlock.
   */
  k_rx_prcr_unlock_prc1 = 0xA502,

  /**
   * @brief Unlock PRC0+PRC1 (Clock Generation + Module Stop Control)
   *
   * @details
   * Unlocks write protection for both clock generation circuit (CGC) registers
   * and module stop control (MSTPCR) registers. Used during system initialization
   * when configuring PLL, system clocks, and peripheral modules together.
   *
   * **Value**: 0xA503
   * - Key code: 0xA5 (required)
   * - PRC3: 0 (LVD registers remain locked)
   * - PRC1: 1 (MSTPCR registers unlocked)
   * - PRC0: 1 (Clock registers unlocked)
   *
   * **Protected Registers** (when unlocked):
   *
   * **PRC0 (Clock Generation)**:
   * - SYSTEM.SCKCR (0x00080020) - System clock control
   * - SYSTEM.SCKCR2 (0x00080024) - System clock control 2
   * - SYSTEM.SCKCR3 (0x00080026) - System clock control 3
   * - SYSTEM.PLLCR (0x00080028) - PLL control
   * - SYSTEM.PLLCR2 (0x0008002A) - PLL control 2
   * - SYSTEM.MOSCCR (0x00080032) - Main oscillator control
   * - SYSTEM.MOSCWTCR (0x000800A2) - Main oscillator wait control
   *
   * **PRC1 (Module Stop)**:
   * - SYSTEM.MSTPCRA/B/C/D (see k_rx_prcr_unlock_prc1)
   *
   * **Use Cases**:
   * - System clock initialization at boot
   * - PLL configuration and selection
   * - Switching clock sources (main osc <-> PLL)
   * - Enabling peripheral clocks simultaneously
   *
   * **STAR Project Usage**: **LOW FREQUENCY**
   * - Called once during rx_clock_power_init() at boot
   * - May be called during clock source transitions (rare)
   * - ~1-2 times per boot cycle
   *
   * @par Rationale:
   * Combined unlock used during initialization because:
   * - Clock and module configuration often done together
   * - Reduces number of lock/unlock cycles at boot
   * - Acceptable risk during controlled init sequence
   *
   * @par Example - Clock Initialization:
   * @code{.c}
   * void configure_240mhz_operation(void) {
   *     // Unlock both clock and module stop registers
   *     SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc0_prc1;
   *
   *     // Configure PLL (PRC0 protected)
   *     SYSTEM.PLLCR.WORD = 0x1300;     // x10 multiplier (STAR: 24 MHz * 10 = 240 MHz)
   *     SYSTEM.PLLCR2.BYTE = 0x00;      // Enable PLL
   *
   *     // Wait for PLL lock (~2ms)
   *     while (!SYSTEM.OSCOVFSR.BIT.PLOVF) {}
   *
   *     // Switch to PLL (PRC0 protected)
   *     SYSTEM.SCKCR3.WORD = 0x0400;    // Select PLL as system clock
   *     SYSTEM.SCKCR.LONG = 0x20011222; // STAR production dividers: ICK=/1 (240), PCKA=/2 (120), PCKB/C/D/FCK=/4 (60), BCK=/2 (120)
   *
   *     // Enable all peripherals (PRC1 protected)
   *     SYSTEM.MSTPCRA.LONG = 0x00000000;
   *     SYSTEM.MSTPCRB.LONG = 0x00000000;
   *
   *     // Re-lock all
   *     SYSTEM.PRCR.WORD = k_rx_prcr_lock;
   * }
   * @endcode
   *
   * @warning Only use during initialization. Runtime clock changes should use
   *          separate PRC0-only unlock (not defined here - add if needed).
   */
  k_rx_prcr_unlock_prc0_prc1 = 0xA503,

  /**
   * @brief Unlock PRC1+PRC3 (Module Stop + Low-Voltage Detection)
   *
   * @details
   * Unlocks write protection for module stop control (MSTPCR) and low-voltage
   * detection (LVD) registers. This combination is rarely needed as LVD is
   * typically configured once at boot and left unchanged.
   *
   * **Value**: 0xA50B
   * - Key code: 0xA5 (required)
   * - PRC3: 1 (LVD registers unlocked)
   * - PRC1: 1 (MSTPCR registers unlocked)
   * - PRC0: 0 (Clock registers remain locked)
   *
   * **Protected Registers** (when unlocked):
   *
   * **PRC1 (Module Stop)**: See k_rx_prcr_unlock_prc1
   *
   * **PRC3 (Low-Voltage Detection)**:
   * - SYSTEM.LVCMPCR (0x000800E0) - Voltage monitor control
   * - SYSTEM.LVDXCR (0x000800E1) - Voltage detect 1 control
   * - SYSTEM.LVDYCR (0x000800E2) - Voltage detect 2 control
   *
   * **STAR Project Usage**: **NEVER USED**
   * - LVD not implemented in STAR project
   * - 3.3V supply is stable and monitored externally
   * - Included for completeness only
   *
   * @par Rationale:
   * Not used in STAR because:
   * - LVD not required (external power monitoring)
   * - MSTPCR and LVD changes rarely coincide
   * - More specific unlocks preferred (narrower scope)
   *
   * @warning Do not use unless your application specifically needs both MSTPCR
   *          and LVD modification in same critical section.
   */
  k_rx_prcr_unlock_prc1_prc3 = 0xA50B,

  /**
   * @brief Unlock all protection groups (PRC0+PRC1+PRC3)
   *
   * @details
   * Unlocks write protection for ALL protected register groups: clock generation,
   * module stop control, and low-voltage detection. This is the broadest unlock
   * and should be **AVOIDED** in production code.
   *
   * **Value**: 0xA50B
   * - Key code: 0xA5 (required)
   * - PRC3: 1 (LVD registers unlocked) [bit 3]
   * - PRC1: 1 (MSTPCR registers unlocked) [bit 1]
   * - PRC0: 1 (Clock registers unlocked) [bit 0]
   * - Bit 2 is reserved and must be written as 0
   *
   * @note Prior revision set this to 0xA50F, which also asserts reserved bit 2
   *       (PRCR[2] must be written 0 per RX72N HW manual R01UH0824EJ0111
   *       Section 13.2.1). Corrected to 0xA50B.
   *
   * **Protected Registers** (when unlocked):
   * - All PRC0 registers (clock generation)
   * - All PRC1 registers (module stop)
   * - All PRC3 registers (LVD)
   *
   * **STAR Project Usage**: **NEVER USED**
   * - Too broad, violates principle of least privilege
   * - More specific unlocks always available
   * - Included for completeness only
   *
   * @par Rationale for AVOIDANCE:
   * - **Security**: Unlocks more than necessary (attack surface)
   * - **Safety**: Accidental corruption of unrelated registers
   * - **Best Practice**: Use narrowest scope possible
   *
   * @warning **DO NOT USE** unless you have a specific reason to modify registers
   *          across all three protection groups simultaneously. Use specific
   *          unlock combinations instead.
   *
   * @attention Code review flag: If you see this constant used, investigate why
   *            the more specific unlocks (prc1, prc0_prc1) cannot be used instead.
   */
  k_rx_prcr_unlock_all = 0xA50B,

  /**
   * @brief Lock all protection groups (re-enable write protection)
   *
   * @details
   * Locks (re-enables) write protection for ALL register groups by clearing
   * all protection enable bits while maintaining the required 0xA5 key code.
   *
   * **Value**: 0xA500
   * - Key code: 0xA5 (required for write to take effect)
   * - PRC3: 0 (LVD registers locked)
   * - PRC1: 0 (MSTPCR registers locked)
   * - PRC0: 0 (Clock registers locked)
   *
   * **Effect**: After writing this value, all protected registers become read-only
   * until the next unlock operation. Any attempt to write protected registers
   * (without first unlocking) will be silently ignored by hardware.
   *
   * **Usage**: **ALWAYS** write this value immediately after completing protected
   * register modifications. Even on error paths, ensure protection is re-locked.
   *
   * **STAR Project Usage**: **HIGH FREQUENCY**
   * - Called after every unlock operation
   * - ~10,000+ times per hour (matches unlock frequency)
   *
   * @par Rationale:
   * Always lock immediately because:
   * - Minimizes window where accidental writes could corrupt registers
   * - Protects against EMI-induced bit flips
   * - Follows secure coding best practices
   *
   * @par Example - Cleanup Pattern:
   * @code{.c}
   * rx_err_t modify_protected_register(void) {
   *     SYSTEM.PRCR.WORD = k_rx_prcr_unlock_prc1;
   *
   *     // Modify register (may fail)
   *     if (!configure_module()) {
   *         SYSTEM.PRCR.WORD = k_rx_prcr_lock;  // Lock even on error!
   *         return k_rx_err_hardware;
   *     }
   *
   *     SYSTEM.PRCR.WORD = k_rx_prcr_lock;      // Lock on success
   *     return k_rx_ok;
   * }
   * @endcode
   *
   * @par Example - RAII Pattern (C++ style cleanup):
   * @code{.c}
   * // Define cleanup macro (ensures lock on scope exit)
   * #define RX_PRCR_SCOPED_UNLOCK(group) \
   *     for (int _i = (SYSTEM.PRCR.WORD = (group), 0); \
   *          _i < 1; \
   *          _i++, SYSTEM.PRCR.WORD = k_rx_prcr_lock)
   *
   * // Usage: automatic lock at end of scope
   * void modify_with_cleanup(void) {
   *     RX_PRCR_SCOPED_UNLOCK(k_rx_prcr_unlock_prc1) {
   *         SYSTEM.MSTPCRA.LONG &= ~(1UL << 15);
   *         // Lock happens automatically when scope exits
   *     }
   * }
   * @endcode
   *
   * @note Hardware behavior: After lock, protected register writes are ignored
   *       (no bus error or exception - just silently discarded).
   */
  k_rx_prcr_lock = 0xA500,

  /**
   * @brief Mask for PRC protection bits (for readback verification)
   *
   * @details
   * When reading PRCR, the key code (upper byte) reads back as 0x00,
   * NOT as 0xA5. Only the lower 4 bits (PRC0, PRC1, PRC3) contain
   * the actual protection state.
   *
   * **Value**: 0x000F
   * - Bits 3,1,0: PRC bits (protection state)
   * - Bits 7-4, 15-8: Read as zero
   *
   * **Usage**: Use this mask when verifying PRCR readback:
   * @code{.c}
   * *prcr_reg() = k_rx_prcr_unlock_prc1;
   * // Verify unlock (key byte reads as 0x00, so mask it out)
   * if ((*prcr_reg() & k_rx_prcr_readback_mask) !=
   *     (k_rx_prcr_unlock_prc1 & k_rx_prcr_readback_mask)) {
   *     // Unlock failed
   * }
   * @endcode
   */
  k_rx_prcr_readback_mask = 0x000F,

} rx_prcr_values_t;

#ifdef __cplusplus
}
#endif
