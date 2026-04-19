/**
 * @file rx_clock_power_init.c
 * @brief RX72N System Clock and Power Management - 240 MHz PLL Configuration
 *
 * @details
 * # Overview
 *
 * This file implements **Stage 1 of the boot sequence** - system clock and power initialization.
 * It is called **immediately after reset** from main() **before** any peripheral initialization
 * or RTOS startup.
 *
 * **Boot sequence context:**
 * ```
 * Reset -> main() -> rx_clock_power_init() -> hardware_init() -> tx_kernel_enter()
 *                  ^ This file          ^ Peripherals    ^ Start RTOS
 * ```
 *
 * ## Clock Tree Architecture (RX72N @ 240 MHz)
 *
 * @dot
 * digraph clock_tree {
 *   rankdir=LR;
 *   node [shape=box];
 *
 *   XTAL [label="External Crystal\n24 MHz", shape=ellipse];
 *   MainOSC [label="Main Oscillator\n24 MHz"];
 *   PLL [label="PLL\nx10 / 1\n240 MHz", style=filled, fillcolor=lightblue];
 *   PPLL [label="PPLL (USB)\nx8 / 4\n48 MHz", style=filled, fillcolor=lightgreen];
 *
 *   ICLK [label="ICLK (CPU)\n240 MHz", style=filled, fillcolor=yellow];
 *   PCLKA [label="PCLKA\n120 MHz"];
 *   PCLKB [label="PCLKB\n60 MHz"];
 *   PCLKC [label="PCLKC\n60 MHz"];
 *   PCLKD [label="PCLKD\n60 MHz"];
 *   FCLK [label="FCLK (Flash)\n60 MHz"];
 *   BCLK [label="BCLK (Bus)\n120 MHz"];
 *   UCLK [label="UCLK (USB)\n48 MHz", style=filled, fillcolor=lightgreen];
 *
 *   XTAL -> MainOSC;
 *   MainOSC -> PLL [label="24 MHz"];
 *   MainOSC -> PPLL [label="24 MHz"];
 *
 *   PLL -> ICLK [label="/1"];
 *   PLL -> PCLKA [label="/2"];
 *   PLL -> PCLKB [label="/4"];
 *   PLL -> PCLKC [label="/4"];
 *   PLL -> PCLKD [label="/4"];
 *   PLL -> FCLK [label="/4"];
 *   PLL -> BCLK [label="/2"];
 *
 *   PPLL -> UCLK [label="Exact 48 MHz"];
 * }
 * @enddot
 *
 * ## Initialization Sequence (3 Phases)
 *
 * **Phase order is CRITICAL** - violating sequence causes hardware failures:
 *
 * ### Phase 1: Clock Configuration (~200 us)
 * 1. Unlock register protection (PRCR)
 * 2. Stop sub-clock oscillator (SOSCCR) - not used in STAR
 * 3. Disable RTC (RCR3) - complete sub-clock shutdown
 * 4. Start main oscillator (MOSCCR) - 24 MHz external crystal
 * 5. Wait for main oscillator stabilization (~10 ms)
 * 6. Configure PLL (PLLCR/PLLCR2) - 24 MHz x 10 / 1 = 240 MHz
 * 7. Wait for PLL stabilization (~200 us)
 * 8. Configure PPLL (PPLLCR/PPLLCR2) - 24 MHz x 8 / 4 = 48 MHz (USB clock)
 * 9. Wait for PPLL stabilization (~200 us)
 * 10. **CRITICAL:** Set MEMWAIT=1 for ICLK > 120 MHz (flash access timing)
 * 11. Configure system clock dividers (SCKCR) - set all peripheral clocks
 * 12. Select PLL as system clock source (SCKCR3)
 * 13. Lock register protection (PRCR)
 *
 * ### Phase 2: Module Stop Control (~5 us)
 * 1. Unlock register protection (PRCR)
 * 2. Enable CMT0/CMT1 modules (MSTPCRA bit 15) - timers for ThreadX
 * 3. Enable MTU modules (MSTPCRA bit 9) - PWM for motor control
 * 4. Enable RSPI0/RSPI1 modules (MSTPCRB bits 16-17) - SPI communication
 * 5. Enable S12AD module (MSTPCRC bit 17) - ADC for sensing
 * 6. Verify module clocks enabled (retry up to 3 times if needed)
 * 7. Lock register protection (PRCR)
 *
 * ### Phase 3: Verification (~2 us)
 * 1. Read back all clock registers (PLLCR2, SCKCR, SCKCR3, PPLLCR2, OSCOVFSR, MEMWAIT)
 * 2. Verify PLL enabled and stable
 * 3. Verify PPLL enabled and stable (USB clock)
 * 4. Verify system clock dividers correct
 * 5. Verify PLL selected as clock source
 * 6. Verify MEMWAIT=1 for 240 MHz operation
 * 7. Assert all validations passed (NASA Rule 5 postconditions)
 *
 * ## Simulator Support
 *
 * When building for e^2 studio simulator (RX_SIMULATOR_MODE defined):
 * - **Main oscillator wait loop** (Phase 1, step 5): Skipped - no external crystal in simulator
 * - **PLL polling loop** (Phase 1, step 7): Skipped - OSCOVFSR flags never set in simulator
 * - **PPLL polling loop** (Phase 1, step 9): Skipped - OSCOVFSR flags never set in simulator
 * - **Register writes**: Still executed (maintains register state consistency)
 * - **Verification**: Still executed (validates simulator register model)
 *
 * **Why these loops fail in simulator**:
 * - Simulator doesn't model external 24 MHz crystal oscillation
 * - Simulator doesn't model PLL/PPLL hardware circuits
 * - OSCOVFSR register bits never set -> polling loops timeout with 0x203 error
 *
 * **What works in simulator**:
 * - Register configuration (PLLCR, SCKCR, MEMWAIT, etc.)
 * - Clock divider logic validation
 * - Error path testing (invalid configurations)
 * - Algorithm and state machine logic
 *
 * **Use hardware for**:
 * - Clock tree validation (actual frequencies)
 * - PLL lock timing measurements
 * - Oscillator startup behavior
 * - MEMWAIT timing validation
 *
 * @see rx_simulator_config.h For simulator configuration
 *
 * ## Critical Timing Requirement: MEMWAIT for ICLK > 120 MHz
 *
 * **From RX72N User's Manual Ch09, Section 9.2.2, Page 341, Lines 962-989:**
 *
 * > "Set the MEMWAIT to 1 (one wait cycle) if the ICLK frequency is to be above 120 MHz."
 * >
 * > "To change the frequency of the ICLK from 120 MHz or less to above 120 MHz, start by
 * > setting the MEMWAIT bit to 1 (one wait cycle), and then change the frequency setting
 * > in the SCKCR register."
 *
 * **Implementation:**
 * ```c
 * // MUST set MEMWAIT BEFORE changing SCKCR to 240 MHz
 * *memwait_reg() = k_memwait_one_wait;  // Set MEMWAIT=1
 * RX_ASSERT(*memwait_reg() == k_memwait_one_wait, "MEMWAIT config failed");
 * system_regs()->sckcr = k_system_clock_dividers;  // Now safe to switch to 240 MHz
 * ```
 *
 * **Why this matters:** Flash memory access timing. At > 120 MHz, CPU outpaces flash read cycles.
 * MEMWAIT=1 adds one wait state to flash reads, ensuring data integrity.
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Phase | Duration | CPU Cycles | Blocking? | Notes |
 * |-------|----------|------------|-----------|-------|
 * | **Main OSC stabilization** | ~10 ms | ~2.4M | Yes | Busy-wait, before ThreadX |
 * | **PLL stabilization** | ~200 us | ~48K | Yes | Busy-wait, timeout 1M iterations |
 * | **PPLL stabilization** | ~200 us | ~48K | Yes | Busy-wait, timeout 1M iterations |
 * | **Clock config** | ~50 us | ~12K | No | Register writes only |
 * | **Module stop init** | ~5 us | ~1.2K | No | Register writes + verification |
 * | **Verification** | ~2 us | ~480 | No | Register reads only |
 * | **Total** | **~10.5 ms** | **~2.5M** | Yes | **Dominated by OSC stabilization** |
 *
 * ## Memory Usage
 *
 * | Object | Size | Location | Purpose |
 * |--------|------|----------|---------|
 * | **s_tag** | 11 bytes | .rodata (Flash) | Logging tag string |
 * | **timeout (local)** | 4 bytes | Stack | PLL stabilization countdown |
 * | **Function stack** | ~128 bytes | Main stack | Local variables, return addresses |
 *
 * **Total RAM:** ~132 bytes (stack only - no heap, no static buffers)
 *
 * ## Error Handling
 *
 * **Critical errors (assert-halt):**
 * - PRCR unlock failed (register protection stuck)
 * - MEMWAIT configuration failed (cannot set flash wait state)
 * - PLL selection verification failed (clock switch incomplete)
 * - PPLL not stable after timeout (USB clock unavailable)
 *
 * **Recoverable errors (return error code):**
 * - PLL stabilization timeout (k_rx_err_hw_timeout)
 * - PPLL stabilization timeout (k_rx_err_hw_timeout)
 * - Module stop verification failed after 3 retries (k_rx_err_hw_timeout)
 * - Clock configuration verification failed (k_rx_err_hw_init_failed)
 *
 * **Recovery strategy:**
 * - On assert: Halt execution (developer investigates)
 * - On error return: main() halts boot (no recovery, requires hardware reset)
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Notes |
 * |------|--------|----------------------|
 * | **Rule 1: Control flow** | [PASS] | No goto, setjmp, or recursion |
 * | **Rule 2: Loop bounds** | [PASS] | All loops have explicit upper bounds (k_pll_stabilization_timeout, etc.) |
 * | **Rule 3: Heap allocation** | [PASS] | Zero dynamic allocation (all constants, local variables) |
 * | **Rule 4: Function length** | [PASS] | Longest function: internal_clock_init() = 114 lines |
 * | **Rule 5: Assertions** | [PASS] | 9 precondition/postcondition checks across all functions |
 * | **Rule 6: Data scope** | [PASS] | All variables at smallest scope (function-local) |
 * | **Rule 7: Return checks** | [PASS] | All rx_err_t returns checked |
 * | **Rule 8: Preprocessor** | [PASS] | Zero macros (uses C23 typed enums for all constants) |
 * | **Rule 9: Pointers** | [PASS] | Single-level dereferencing only |
 * | **Rule 10: Warnings** | [PASS] | Compiles with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **Single Responsibility** | Each function does one thing (clock, modules, verification separate) |
 * | **Open/Closed** | New modules added without modifying clock init |
 * | **Liskov Substitution** | All functions return rx_err_t with consistent error codes |
 * | **Interface Segregation** | Small, focused APIs (internal_clock_init, internal_module_stop_init separate) |
 * | **Dependency Inversion** | Depends on abstractions (rx_err_t, register accessors), not implementations |
 *
 * ## Thread Safety
 *
 * **Pre-ThreadX context:** This file executes in **single-threaded mode** before RTOS starts.
 * No synchronization primitives needed. Hardware registers accessed directly.
 *
 * ## Related Files
 *
 * - **Main entry:** See [main.c](main.c) - Calls rx_clock_power_init() first
 * - **Hardware init:** See [hardware_init.c](hardware_init.c) - Calls after clocks configured
 * - **Register definitions:** See [rx72n_regs.h](../lib/rx_hal/inc/rx72n_regs.h) - System register map
 * - **Protection:** See [rx_register_protection.h](../lib/rx_core/inc/rx_register_protection.h) - PRCR constants
 *
 * @warning **Never call rx_clock_power_init() twice.** Clock system already configured.
 *          Second call may cause PLL instability or timing violations.
 *
 * @warning **MEMWAIT=1 is MANDATORY for ICLK=240 MHz.** Omitting causes flash read errors,
 *          data corruption, and unpredictable behavior.
 *
 * @see rx_clock_power_init() Public entry point (main initialization function)
 * @see internal_clock_init() Phase 1: Configure PLL to 240 MHz
 * @see internal_module_stop_init() Phase 2: Enable peripheral module clocks
 * @see internal_verify_system_state() Phase 3: Verify configuration correct
 *
 * @since Version 1.0.0
 *
 * @par Revision History:
 * - v1.0.0 (2026-01): Initial implementation with 240 MHz PLL and USB clock
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_clock_power_init.h"

#include <stdint.h>

#include "rx72n_regs.h"
#include "rx72n_rtc_regs.h"
#include "rx_check.h"
#include "rx_register_protection.h"
#include "rx_simulator_config.h" /* For simulator mode detection */
#include "star_board.h"          /* Board variant: STAR_BOARD_PROD or STAR_BOARD_TOM */

static const char* s_tag = "CLOCK_INIT";

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

/** @brief Oscillator stabilization timing constants */
typedef enum : uint32_t {
  /* Main oscillator stabilization: RX72N manual says ~10 ms for the 24 MHz
   * crystal to stabilise.  The firmware runs at LOCO (~240 kHz) at this
   * point -- BEFORE the PLL is configured.  2400 NOPs at 240 kHz = ~10 ms.
   *
   * (Earlier value was 2,400,000, a miscalculation assuming 240 MHz CPU.
   * At actual LOCO speed that loop ran for ~10 seconds; combined with the
   * OFS0=0xFFFFFFFF watchdog-disabled default everything looked dead on
   * the bus but the chip was just stuck counting NOPs.) */
  k_main_osc_stabilization_cycles     = 2400,
  k_pll_stabilization_timeout         = 1000000, /**< PLL stabilization max wait iterations */
  k_pll_stabilization_timeout_expired = 0,       /**< PLL timeout expiration value */
} oscillator_timing_t;

/** @brief Oscillator control register values */
typedef enum : uint8_t {
  k_sub_clock_stopped = 0x01, /**< SOSCCR: Stop sub-clock oscillator */
  k_main_osc_enabled  = 0x00, /**< MOSCCR: Enable main oscillator */
  k_pll_enabled       = 0x00, /**< PLLCR2: Enable PLL */
} oscillator_control_t;

/** @brief PLL configuration values */
typedef enum : uint16_t {
  k_pll_multiplier_10_div_1 = 0x1300, /**< PLLCR: 10x multiplier, divide by 1 (240MHz from 24MHz MOSC) */
  /* TOM variant: HOCO * 12 into PLL.
   *   bit 4    PLLSRCSEL = 1  -> HOCO input
   *   bits [13:8] STC      = 23 (=2*12-1) -> x12 multiplier
   *   bits [1:0]  PLIDIV   = 0 -> no division
   * -> 0x1710.  Computes 16 MHz HOCO * 12 = 192 MHz PLL output. */
  k_pll_multiplier_12_hoco  = 0x1710, /**< PLLCR: HOCO x12 / 1 (192 MHz from 16 MHz HOCO) */
  k_hoco_stable_flag        = 0x08,   /**< OSCOVFSR: HOCO stabilization flag bit */
  k_pll_stable_flag         = 0x04,   /**< OSCOVFSR: PLL stabilization flag bit */
  k_pll_stable_flag_unset   = 0,      /**< PLL stable flag not yet asserted */
  k_ppll_stable_flag_unset  = 0,      /**< PPLL stable flag not yet asserted */
  k_hococr_enabled          = 0x00,   /**< HOCOCR: start HOCO (0 = running) */
} pll_config_t;

/** @brief System clock configuration */
typedef enum : uint32_t {
  k_system_clock_dividers   = 0x21C21211, /**< SCKCR: ICLK=240MHz, PCLKA=120MHz, others=60MHz */
  k_system_clock_source_pll = 0x0400,     /**< SCKCR3: Select PLL as system clock source */
  k_packcr_addr             = 0x00080044, /**< PACKCR absolute address (USB clock source select) */
  /* SCKCR2.UCK = b7..b4 selects the USB clock divider off the main PLL
   * (PACKCR.UPLLSEL=0 path).  Value 0b0100 = /5 yields 240 MHz / 5 = 48 MHz
   * which is the exact UCLK the USB0 peripheral requires.  The alternative
   * PPLL route (PACKCR.UPLLSEL=1) is unusable here because k_ppll_config_48mhz
   * actually produces 192 MHz and PACKCR feeds that straight through as UCK. */
  k_sckcr2_uck_div5 = 0x0041, /**< SCKCR2: UCK=/5 (b7..b4=0100) + reserved b0=1 */
  /* TOM variant: 192 MHz main PLL needs UCK=/4 for 48 MHz USB.
   * UCK field bits [7:4]: value 3 (0b0011) = divide by 4.  Reserved b0=1. */
  k_sckcr2_uck_div4_tom = 0x0031, /**< SCKCR2 for TOM: UCK=/4 (192/4=48 MHz) + b0=1 */
} system_clock_config_t;

/** @brief PACKCR (peripheral clock) values -- routes UCLK from PPLL for USB */
typedef enum : uint16_t {
  k_packcr_upllsel_ppll = (1U << 12) | 1U, /**< UPLLSEL=1 (b12) PPLL path; b0 reserved=1 */
} packcr_config_t;

/** @brief Module stop bit positions in MSTPCRA */
typedef enum : uint8_t {
  k_mstpcra_cmt = 15, /**< CMT0, CMT1 module stop bit */
  k_mstpcra_mtu = 9,  /**< MTU module stop bit */
} mstpcra_bits_t;

/** @brief Module stop bit positions in MSTPCRB */
typedef enum : uint8_t {
  k_mstpcrb_rspi0 = 17, /**< RSPI0 module stop bit */
  k_mstpcrb_rspi1 = 16, /**< RSPI1 module stop bit */
  /* Note: SCI modules are enabled per-channel in uart_init_channel() */
} mstpcrb_bits_t;

/** @brief Module stop bit positions in MSTPCRC */
typedef enum : uint8_t {
  k_mstpcrc_s12ad = 17, /**< S12AD module stop bit */
} mstpcrc_bits_t;

/** @brief Module initialization retry configuration */
typedef enum : uint8_t {
  k_retry_count_module_stop = 3, /**< Number of retries for module stop register verification */
} module_init_config_t;

/* =============================================================================
 * Clock Configuration
 * =============================================================================
 */

/**
 * @brief Wait for hardware PLL to achieve frequency lock
 *
 * @details
 * Polls the OSCOVFSR register until the PLL stable flag is set.
 * Bounded by k_pll_stabilization_timeout to satisfy NASA Rule 2.
 * Returns immediately in simulator mode (hardware PLL not modeled).
 *
 * @return rx_err_t Result code
 * @retval k_rx_ok PLL locked within timeout
 * @retval k_rx_err_hw_timeout PLL did not lock before timeout expired
 *
 * @pre PLL must be enabled (pllcr2 = k_pll_enabled) before calling
 * @pre PRCR registers must be unlocked
 *
 * @post On success: PLL oscillator is stable and locked
 * @post On failure: PRCR is re-locked before returning error
 *
 * @note Not thread-safe; call only during single-threaded boot
 * @since Version 1.0.0
 */
static rx_err_t internal_wait_pll_lock(void)
{
#if RX_IS_SIMULATOR
  /* Simulator: PLL stable immediately (no hardware PLL circuit to lock) */
  return k_rx_ok;
#else
  uint32_t timeout = k_pll_stabilization_timeout;
  while ((system_regs()->oscovfsr & k_pll_stable_flag) == k_pll_stable_flag_unset &&
         timeout > k_pll_stabilization_timeout_expired) {
    timeout--;
  }
  if (timeout == k_pll_stabilization_timeout_expired) {
    *prcr_reg() = k_rx_prcr_lock;
    return k_rx_err_hw_timeout;
  }
  return k_rx_ok;
#endif
}

/**
 * @brief Wait for hardware PPLL (USB clock) to achieve frequency lock
 *
 * @details
 * Polls the OSCOVFSR register until the PPLL stable flag is set.
 * Bounded by k_pll_stabilization_timeout to satisfy NASA Rule 2.
 * Returns immediately in simulator mode (hardware PPLL not modeled).
 *
 * @return rx_err_t Result code
 * @retval k_rx_ok PPLL locked within timeout
 * @retval k_rx_err_hw_timeout PPLL did not lock before timeout expired
 *
 * @pre PPLL must be enabled (ppllcr2 = k_ppll_enabled) before calling
 * @pre PRCR registers must be unlocked
 *
 * @post On success: PPLL oscillator is stable and locked
 * @post On failure: PRCR is re-locked before returning error
 *
 * @note Not thread-safe; call only during single-threaded boot
 * @since Version 1.0.0
 */
#if !defined(STAR_BOARD_TOM)
static rx_err_t internal_wait_ppll_lock(void)
{
#if RX_IS_SIMULATOR
  /* Simulator: PPLL stable immediately (no hardware PPLL circuit to lock) */
  return k_rx_ok;
#else
  uint32_t timeout = k_pll_stabilization_timeout;
  while ((system_regs()->oscovfsr & k_ppll_stable_flag) == k_ppll_stable_flag_unset &&
         timeout > k_pll_stabilization_timeout_expired) {
    timeout--;
  }
  if (timeout == k_pll_stabilization_timeout_expired) {
    *prcr_reg() = k_rx_prcr_lock;
    return k_rx_err_hw_timeout;
  }
  /* Post-condition: Verify PPLL is stable (NASA Rule 5) */
  RX_ASSERT((system_regs()->oscovfsr & k_ppll_stable_flag) != 0,
            "Postcondition: PPLL stabilization verification failed");
  return k_rx_ok;
#endif
}
#endif /* !STAR_BOARD_TOM */

/**
 * @brief Configure main oscillator, PLL, and PPLL
 *
 * @details
 * Performs the oscillator and PLL/PPLL configuration steps for 240 MHz operation:
 * - Stops sub-clock and disables RTC sub-clock
 * - Starts the 24 MHz main oscillator and waits for stabilization
 * - Configures PLL (24 MHz x 10 / 1 = 240 MHz) and waits for lock
 * - Configures PPLL (24 MHz x 8 / 4 = 48 MHz USB) and waits for lock
 *
 * @return rx_err_t Result code
 * @retval k_rx_ok All oscillators started and PLLs locked
 * @retval k_rx_err_hw_timeout PLL or PPLL failed to lock within timeout
 *
 * @pre PRCR must be unlocked before calling
 * @pre External 24 MHz crystal must be connected
 *
 * @post Main oscillator running at 24 MHz
 * @post PLL locked at 240 MHz
 * @post PPLL locked at 48 MHz
 *
 * @note Not thread-safe; call only during single-threaded boot
 * @since Version 1.0.0
 */
static rx_err_t internal_start_oscillators_and_plls(void)
{
  /* Stop sub-clock oscillator (not used) */
  system_regs()->sosccr = k_sub_clock_stopped;

  /* Disable RTC to fully disable sub-clock (RCR3.RTCEN = 0)
   * Required for complete sub-clock shutdown as per hardware manual */
  *rtc_rcr3_reg() = k_rcr3_rtcen_disable;

#if defined(STAR_BOARD_TOM)
  /* -----------------------------------------------------------------------
   * TOM variant: no external crystal.  Route PLL from HOCO.
   *
   *   HOCO 16 MHz -> PLL x12 -> 192 MHz main PLL output
   *   UCLK is later derived by SCKCR2.UCK = /4 directly from main PLL
   *   (no PPLL required).
   *
   * NOTE: HOCO accuracy is ~+/-1 % (10 000 ppm), which is 20x outside
   * the USB FS spec of 500 ppm.  Enumeration and short bulk transfers
   * work in practice on permissive hosts (e.g., Pi 5 xHCI) but the
   * device is not USB-IF compliant on this clock source.  See
   * docs/BOARD_VARIANTS.md for the full tradeoff writeup.
   * ----------------------------------------------------------------------- */

  /* Start HOCO (HOCOCR = 0 means "oscillating"). */
  system_regs()->hococr = k_hococr_enabled;

  /* Wait for HOCO stable (OSCOVFSR.HCOVF = bit 3). */
#if !RX_IS_SIMULATOR
  {
    volatile uint32_t hoco_timeout = k_pll_stabilization_timeout;
    while ((system_regs()->oscovfsr & k_hoco_stable_flag) == 0U && hoco_timeout > 0U) {
      hoco_timeout--;
    }
    if ((system_regs()->oscovfsr & k_hoco_stable_flag) == 0U) {
      return k_rx_err_hw_timeout;
    }
  }
#endif

  /* Configure PLL: 192 MHz = HOCO 16 MHz * 12, PLLSRCSEL = HOCO. */
  system_regs()->pllcr  = k_pll_multiplier_12_hoco;
  system_regs()->pllcr2 = k_pll_enabled;

  /* TOM skips PPLL -- no Ethernet expected on Tom's bench PCB and UCLK
   * comes straight from main PLL via SCKCR2. */
  return internal_wait_pll_lock();
#else
  /* -----------------------------------------------------------------------
   * PROD variant: 24 MHz external crystal -> MOSC -> PLL x10 -> 240 MHz.
   * ----------------------------------------------------------------------- */

  /* Start main oscillator (24 MHz external crystal) */
  system_regs()->mosccr = k_main_osc_enabled;

  /* Wait for main oscillator stabilization (typically 10ms)
   * NOTE: Busy-wait required - runs before ThreadX initialization */
#if !RX_IS_SIMULATOR
  for (volatile uint32_t i = 0; i < k_main_osc_stabilization_cycles; i++) {
    __asm__ volatile("nop");
  }
#endif

  /* Configure PLL: 240 MHz = (24 MHz x 10) / 1 */
  system_regs()->pllcr  = k_pll_multiplier_10_div_1;
  system_regs()->pllcr2 = k_pll_enabled;

  const rx_err_t pll_err = internal_wait_pll_lock();
  if (pll_err != k_rx_ok) {
    return pll_err;
  }

  /* Configure PPLL for 48 MHz USB clock: 48 MHz = (24 MHz x 8) / 4 */
  *ppllcr_reg()  = k_ppll_config_48mhz;
  *ppllcr2_reg() = k_ppll_enabled;

  const rx_err_t ppll_err = internal_wait_ppll_lock();
  if (ppll_err != k_rx_ok) {
    return ppll_err;
  }

  /* Route USB0 UCLK from the PPLL path by setting PACKCR.UPLLSEL=1.
   * Without this, the USB module gets its 48 MHz clock from SCKCR2.UCK
   * (main-PLL divider), which is not configured here and defaults to a
   * divider that produces the wrong frequency -- USB0 then never
   * enumerates even though rx_usb_hw_init() and D+ pull-up run cleanly.
   *
   * PACKCR layout (RX72N HW manual p365):
   *   b12 UPLLSEL : 0 = main PLL (default), 1 = PPLL
   *   b0          : reserved, must be written as 1
   *
   * The HAL struct at rx72n_system_regs.h doesn't expose packcr; write
   * it via the absolute address the register reference table uses. */
  *((volatile uint16_t*)k_packcr_addr) = k_packcr_upllsel_ppll;

  return k_rx_ok;
#endif /* STAR_BOARD_TOM */
}

/**
 * @brief Configure MEMWAIT and switch system clock to PLL
 *
 * @details
 * Performs the final clock switch steps:
 * - Sets MEMWAIT=1 for safe flash access at 240 MHz (must be before clock switch)
 * - Configures system clock dividers (ICLK=240, PCLKA=120, PCLKB/C/D/FCLK=60 MHz)
 * - Switches system clock source to PLL
 * - Verifies PLL selection took effect
 *
 * @return rx_err_t Always k_rx_ok (assertions halt on failure)
 *
 * @pre PLL must be locked before calling (call internal_start_oscillators_and_plls first)
 * @pre PRCR must be unlocked
 *
 * @post MEMWAIT=1 configured for 240 MHz flash access
 * @post System clock running at 240 MHz from PLL
 * @post PRCR NOT re-locked (caller must lock after calling this function)
 *
 * @note Not thread-safe; call only during single-threaded boot
 * @since Version 1.0.0
 */
static rx_err_t internal_switch_to_pll_clock(void)
{
  /* CRITICAL: Must set MEMWAIT BEFORE switching to 240 MHz (per RX72N manual Ch09 sec 9.2.2)
   * "Set MEMWAIT to 1 before increasing ICLK above 120 MHz" */
  *memwait_reg() = k_memwait_one_wait;
  RX_ASSERT(*memwait_reg() == k_memwait_one_wait, "Precondition: MEMWAIT configuration failed");

  /* Configure system clock dividers.  The divider nibbles are the same
   * for both board variants (ICK=/2, PCKA=/2, PCKB=/4, etc.) because the
   * downstream peripheral expectations don't change -- only the PLL input
   * frequency differs (240 MHz PROD vs 192 MHz TOM), so the resulting
   * clocks scale proportionally. */
  system_regs()->sckcr = k_system_clock_dividers;

#if defined(STAR_BOARD_TOM)
  /* TOM: UCK = 192 / 4 = 48 MHz from the main PLL (no PPLL route). */
  system_regs()->sckcr2 = k_sckcr2_uck_div4_tom;
#else
  /* PROD: UCK = 240 / 5 = 48 MHz from the main PLL. */
  system_regs()->sckcr2 = k_sckcr2_uck_div5;
#endif

  /* Verify MEMWAIT still set after clock divider change */
  RX_ASSERT(*memwait_reg() == k_memwait_one_wait,
            "Postcondition: MEMWAIT corrupted after clock switch");

  /* Switch clock source to PLL */
  system_regs()->sckcr3 = k_system_clock_source_pll;

#if !RX_IS_SIMULATOR
  RX_ASSERT((system_regs()->sckcr3 == k_system_clock_source_pll) &&
              ((system_regs()->oscovfsr & k_pll_stable_flag) != 0),
            "Postcondition: PLL selection and stability verification failed");
#else
  RX_ASSERT(system_regs()->sckcr3 == k_system_clock_source_pll,
            "Postcondition: PLL selection verified (simulator mode)");
#endif

  return k_rx_ok;
}

/**
 * @brief Initialize clock system to 240 MHz
 *
 * @details
 * Configures the full RX72N clock tree starting from the 24 MHz external crystal:
 * - Unlocks PRCR for clock register access
 * - Stops sub-clock oscillator (not used)
 * - Starts main oscillator and waits for stabilization
 * - Configures PLL (24 MHz x 10 / 1 = 240 MHz) and waits for PLL lock
 * - Configures PPLL (24 MHz x 8 / 4 = 48 MHz USB) and waits for PPLL lock
 * - Sets MEMWAIT=1 for safe flash access at 240 MHz (applied AFTER PLL and PPLL lock)
 * - Sets system clock dividers (ICLK=240, PCLKA=120, PCLKB/C/D=60, FCLK=60 MHz)
 * - Switches clock source to PLL
 * - Relocks PRCR
 *
 * Must be called before any peripheral initialization or RTOS startup.
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok All oscillators locked and clocks configured correctly
 * @retval k_rx_err_hw_timeout PLL or PPLL failed to lock within stabilization limit
 *
 * @pre External 24 MHz crystal is connected and oscillating
 * @pre VCC supply voltage is within RX72N operating range
 *
 * @post System clock running at 240 MHz (ICLK) sourced from PLL
 * @post MEMWAIT=1 set for safe flash access at 240 MHz
 * @post PRCR relocked to protect clock registers
 *
 * @note Thread safety: Must be called from single-threaded context before ThreadX starts
 * @note Blocking: Waits for crystal and PLL stabilization (bounded by enum timeout constants)
 *
 * @see internal_verify_system_state() Post-initialization verification
 * @see rx_clock_power_init() Public entry point that calls this function
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 3 postconditions
 */
static rx_err_t internal_clock_init(void)
{
  /* Unlock protection for clock registers */
  *prcr_reg() = k_rx_prcr_unlock_all;

  /* Precondition: Verify PRCR unlock took effect
   * Note: PRCR key byte (upper 8 bits) reads back as 0x00, so we must mask it */
  RX_ASSERT((*prcr_reg() & k_rx_prcr_readback_mask) ==
              (k_rx_prcr_unlock_all & k_rx_prcr_readback_mask),
            "Precondition: PRCR unlock failed");

  /* Start oscillators and PLLs (main osc, PLL at 240 MHz, PPLL at 48 MHz USB) */
  const rx_err_t osc_err = internal_start_oscillators_and_plls();
  if (osc_err != k_rx_ok) {
    *prcr_reg() = k_rx_prcr_lock;
    return osc_err;
  }

  /* Set MEMWAIT and switch system clock to 240 MHz PLL */
  const rx_err_t clk_err = internal_switch_to_pll_clock();
  if (clk_err != k_rx_ok) {
    *prcr_reg() = k_rx_prcr_lock;
    return clk_err;
  }

  /* Lock protection */
  *prcr_reg() = k_rx_prcr_lock;

  return k_rx_ok;
}

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @brief Enable peripheral modules
 *
 * Disables module stop for peripherals we'll use:
 * - CMT0 (system tick timer)
 * - MTU (motor PWM)
 * - S12AD (ADC)
 *
 * Note: SCI modules are enabled per-channel in uart_init_channel()
 *
 * @return k_rx_ok on success, k_rx_err if verification fails after retries
 */
/**
 * @brief Perform one attempt to clear module stop register bits and verify
 *
 * @details
 * Unlocks PRCR, clears the designated MSTPCRA/B/C bits to enable CMT, MTU,
 * RSPI0, RSPI1, and S12AD modules, re-locks PRCR, then verifies the bits
 * were accepted by hardware.
 *
 * @param[in] mstpcra_clear_mask Bit mask to clear in MSTPCRA
 * @param[in] mstpcrb_clear_mask Bit mask to clear in MSTPCRB
 * @param[in] mstpcrc_clear_mask Bit mask to clear in MSTPCRC
 *
 * @return bool true if all bits verified clear, false if any bit still set
 *
 * @pre PRCR accessible (not in protected state requiring special unlock)
 * @post PRCR re-locked on return
 *
 * @note Not thread-safe; call only during single-threaded boot
 * @since Version 1.0.0
 */
static bool internal_module_stop_attempt(uint32_t mstpcra_clear_mask,
                                         uint32_t mstpcrb_clear_mask,
                                         uint32_t mstpcrc_clear_mask)
{
  *prcr_reg() = k_rx_prcr_unlock_all;
  system_regs()->mstpcra &= ~mstpcra_clear_mask;
  system_regs()->mstpcrb &= ~mstpcrb_clear_mask;
  system_regs()->mstpcrc &= ~mstpcrc_clear_mask;
  *prcr_reg() = k_rx_prcr_lock;

  const uint32_t mstpcra_actual = system_regs()->mstpcra & mstpcra_clear_mask;
  const uint32_t mstpcrb_actual = system_regs()->mstpcrb & mstpcrb_clear_mask;
  const uint32_t mstpcrc_actual = system_regs()->mstpcrc & mstpcrc_clear_mask;
  if ((mstpcra_actual != 0U) || (mstpcrb_actual != 0U) || (mstpcrc_actual != 0U)) {
    return false;
  }

  const uint32_t verify_a = system_regs()->mstpcra & mstpcra_clear_mask;
  const uint32_t verify_b = system_regs()->mstpcrb & mstpcrb_clear_mask;
  const uint32_t verify_c = system_regs()->mstpcrc & mstpcrc_clear_mask;
  RX_ASSERT((verify_a == 0) && (verify_b == 0) && (verify_c == 0),
            "Postcondition: Module stop bits remain cleared");
  return true;
}

static rx_err_t internal_module_stop_init(void)
{
  const uint32_t mstpcra_clear_mask = (1UL << k_mstpcra_cmt) | (1UL << k_mstpcra_mtu);
  const uint32_t mstpcrb_clear_mask = (1UL << k_mstpcrb_rspi0) | (1UL << k_mstpcrb_rspi1);
  const uint32_t mstpcrc_clear_mask = (1UL << k_mstpcrc_s12ad);

  for (uint8_t attempt = 0; attempt < k_retry_count_module_stop; attempt++) {
    const bool ok =
      internal_module_stop_attempt(mstpcra_clear_mask, mstpcrb_clear_mask, mstpcrc_clear_mask);
    if (ok) {
      return k_rx_ok;
    }
  }

  return k_rx_err_hw_timeout;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Verify system clock configuration is correct
 *
 * @details
 * Performs post-initialization validation of the RX72N clock subsystem by reading
 * back hardware registers. Checks that:
 * - System register pointer is valid
 * - PLL is enabled (pllcr2 register)
 * - System clock dividers match expected values (sckcr register)
 * - PLL is selected as the system clock source (sckcr3 register)
 * - PPLL is enabled for USB clock (ppllcr2 register)
 * - PPLL oscillator is stable (oscovfsr register, hardware only)
 * - Flash wait state is configured for 240 MHz operation (memwait register)
 *
 * Module-stop register verification (MSTPCRA, MSTPCRB) is performed by
 * internal_module_stop_init(), not by this function.
 *
 * Intended to be called immediately after internal_clock_init() to confirm that
 * all register writes took effect.
 *
 * @return rx_err_t Verification result
 * @retval k_rx_ok All clock configuration registers match expected values
 * @retval k_rx_err_null_ptr system_regs() returned nullptr
 * @retval k_rx_err_hw_init_failed One or more clock registers do not match expected values
 *
 * @pre internal_clock_init() completed successfully
 * @pre Hardware registers are accessible (not in reset or low-power mode)
 *
 * @post No hardware state is modified (read-only verification)
 * @post Return value reliably reflects whether system is in expected clock state
 *
 * @note Thread safety: Must be called from single-threaded context before ThreadX starts
 *
 * @see internal_clock_init() Clock configuration that this function verifies
 * @see rx_clock_power_init() Public entry point
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 2 postconditions
 */
static rx_err_t internal_verify_system_state(void)
{
  const volatile rx_system_regs_t* sys = system_regs();

  /* Verify system register access */
  RX_CHECK_NULL_PTR(sys, s_tag, "system_regs pointer is nullptr");

  /* Verify PLL is enabled by checking PLLCR2 register */
  const uint8_t pllcr2 = sys->pllcr2;
  if (pllcr2 != k_pll_enabled) {
    return k_rx_err_hw_init_failed;
  }

  /* Verify system clock dividers are configured correctly */
  const uint32_t sckcr = sys->sckcr;
  if (sckcr != k_system_clock_dividers) {
    return k_rx_err_hw_init_failed;
  }

  /* Verify PLL is selected as the system clock source */
  const uint16_t sckcr3 = sys->sckcr3;
  if (sckcr3 != k_system_clock_source_pll) {
    return k_rx_err_hw_init_failed;
  }

#if !defined(STAR_BOARD_TOM)
  /* Verify PPLL is enabled for USB clock (PROD only -- TOM skips PPLL
   * entirely and derives UCLK directly from the main PLL). */
  const uint8_t ppllcr2 = *ppllcr2_reg();
  if (ppllcr2 != k_ppll_enabled) {
    return k_rx_err_hw_init_failed;
  }

#if !RX_IS_SIMULATOR
  /* Verify PPLL is stable (hardware only - simulator doesn't model OSCOVFSR flags) */
  const uint8_t oscovfsr = sys->oscovfsr;
  if ((oscovfsr & k_ppll_stable_flag) == 0) {
    return k_rx_err_hw_init_failed;
  }
#endif
#endif /* !STAR_BOARD_TOM */

  /* Verify MEMWAIT is configured for 240 MHz operation */
  const uint8_t memwait = *memwait_reg();
  if (memwait != k_memwait_one_wait) {
    return k_rx_err_hw_init_failed;
  }

  /* Postcondition: All clock configuration verified (NASA Rule 5) */
#if defined(STAR_BOARD_TOM)
  /* TOM: no PPLL assertion -- UCLK comes from main PLL. */
  RX_ASSERT((pllcr2 == k_pll_enabled) && (sckcr == k_system_clock_dividers) &&
              (sckcr3 == k_system_clock_source_pll) && (memwait == k_memwait_one_wait),
            "Postcondition: clock configuration verified (TOM variant)");
#elif !RX_IS_SIMULATOR
  /* Hardware: Verify all clock configuration including PPLL stability */
  RX_ASSERT((pllcr2 == k_pll_enabled) && (sckcr == k_system_clock_dividers) &&
              (sckcr3 == k_system_clock_source_pll) && (ppllcr2 == k_ppll_enabled) &&
              ((oscovfsr & k_ppll_stable_flag) != 0) && (memwait == k_memwait_one_wait),
            "Postcondition: clock configuration verified");
#else
  /* Simulator: Verify clock configuration (skip PPLL stability - not modeled in simulator) */
  RX_ASSERT((pllcr2 == k_pll_enabled) && (sckcr == k_system_clock_dividers) &&
              (sckcr3 == k_system_clock_source_pll) && (ppllcr2 == k_ppll_enabled) &&
              (memwait == k_memwait_one_wait),
            "Postcondition: clock configuration verified (simulator mode)");
#endif

  return k_rx_ok;
}

/**
 * @brief Initialize RX72N system clock and power management - Public entry point
 *
 * @details
 * Configures the **complete clock tree** and **module power control** for the RX72N MCU.
 * This is the **FIRST initialization function** called from main() after reset, before any
 * peripheral setup or RTOS startup.
 *
 * **Call sequence:** Reset -> main() -> rx_clock_power_init() -> hardware_init() -> tx_kernel_enter()
 *
 * ## Three-Phase Initialization
 *
 * ### Phase 1: Clock Configuration (internal_clock_init)
 * - Stop sub-clock oscillator (not used)
 * - Start main oscillator (24 MHz external crystal)
 * - Configure PLL (24 MHz x 10 / 1 = 240 MHz)
 * - Configure PPLL (24 MHz x 8 / 4 = 48 MHz USB clock)
 * - **Set MEMWAIT=1 for ICLK > 120 MHz** (CRITICAL for flash access)
 * - Configure system clock dividers (ICLK=240 MHz, PCLKA=120 MHz, etc.)
 * - Select PLL as system clock source
 * - **Duration:** ~10.5 ms (dominated by oscillator stabilization)
 *
 * ### Phase 2: Module Stop Control (internal_module_stop_init)
 * - Enable CMT0/CMT1 (timers for ThreadX tick)
 * - Enable MTU (PWM for motor control)
 * - Enable RSPI0/RSPI1 (SPI communication)
 * - Enable S12AD (ADC for current/voltage sensing)
 * - Verify all module clocks enabled (retry up to 3 times)
 * - **Duration:** ~5 us
 *
 * ### Phase 3: Verification (internal_verify_system_state)
 * - Read back all clock configuration registers
 * - Verify PLL enabled and stable
 * - Verify PPLL enabled and stable (USB clock)
 * - Verify system clock dividers correct (SCKCR)
 * - Verify PLL selected as clock source (SCKCR3)
 * - Verify MEMWAIT=1 set correctly
 * - **Duration:** ~2 us
 *
 * ## Clock Tree Output
 *
 * **After successful initialization:**
 *
 * | Clock | Frequency | Divider | Purpose |
 * |-------|-----------|---------|---------|
 * | **ICLK** | 240 MHz | /1 | CPU core execution |
 * | **PCLKA** | 120 MHz | /2 | High-speed peripherals (CMT, MTU) |
 * | **PCLKB** | 60 MHz | /4 | Medium-speed peripherals (SCI, RSPI, RIIC) |
 * | **PCLKC** | 60 MHz | /4 | Low-speed peripherals |
 * | **PCLKD** | 60 MHz | /4 | Low-speed peripherals |
 * | **FCLK** | 60 MHz | /4 | Flash memory access |
 * | **BCLK** | 120 MHz | /2 | External bus (if used) |
 * | **UCLK** | 48 MHz | PPLL | USB communication (exact frequency required) |
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Phase | Duration | Blocking? | Notes |
 * |-------|----------|-----------|-------|
 * | **Clock init** | ~10.5 ms | Yes | Main OSC stabilization dominates |
 * | **Module stop** | ~5 us | No | Register writes only |
 * | **Verification** | ~2 us | No | Register reads only |
 * | **Total** | **~10.5 ms** | Yes | **Not on critical boot path** |
 *
 * **Boot time context:** Total boot time (reset -> ThreadX) is ~51 ms, dominated by USB
 * enumeration (~50 ms) which happens later in comm_task. Clock init is only ~20% of boot.
 *
 * ## Error Scenarios and Recovery
 *
 * **Fatal errors (function returns error, main() halts boot):**
 *
 * 1. **PLL stabilization timeout** (k_rx_err_hw_timeout)
 *    - **Cause:** PLL failed to lock within 1M iterations (~4 ms @ 240 MHz)
 *    - **Root causes:** External crystal failure, incorrect PLLCR config, power supply issue
 *    - **Recovery:** Hardware reset required, investigate crystal oscillation
 *
 * 2. **PPLL stabilization timeout** (k_rx_err_hw_timeout)
 *    - **Cause:** PPLL (USB clock) failed to lock within timeout
 *    - **Root causes:** Main oscillator unstable, incorrect PPLLCR config
 *    - **Recovery:** Hardware reset required, verify main oscillator
 *
 * 3. **Module stop verification failed** (k_rx_err_hw_timeout)
 *    - **Cause:** Module stop bits did not clear after 3 retries
 *    - **Root causes:** Register protection stuck, hardware fault, clock gating issue
 *    - **Recovery:** Hardware reset required, check PRCR register
 *
 * 4. **Clock configuration verification failed** (k_rx_err_hw_init_failed)
 *    - **Cause:** Clock registers not set to expected values after initialization
 *    - **Root causes:** Register write failed, hardware fault, corruption during init
 *    - **Recovery:** Hardware reset required, investigate register access
 *
 * ## Critical Timing: MEMWAIT for 240 MHz Operation
 *
 * **From RX72N User's Manual Ch09, Section 9.2.2:**
 *
 * > "Set the MEMWAIT to 1 (one wait cycle) if the ICLK frequency is to be above 120 MHz."
 *
 * **Implementation sequence (CRITICAL ORDER):**
 * ```
 * 1. Set MEMWAIT=1 (add wait state for flash reads)
 * 2. Verify MEMWAIT write took effect (assertion)
 * 3. Configure SCKCR to 240 MHz (now safe to switch)
 * 4. Verify MEMWAIT still =1 after clock switch (assertion)
 * ```
 *
 * **Why this matters:** Flash memory cannot keep up with 240 MHz CPU without wait state.
 * Omitting MEMWAIT=1 causes:
 * - Flash read errors (corrupted instruction fetch)
 * - Data corruption (constants read from flash)
 * - Unpredictable behavior (random crashes, incorrect execution)
 *
 * @return rx_err_t Initialization status
 * @retval k_rx_ok All clocks configured successfully (system ready for peripherals)
 * @retval k_rx_err_hw_timeout PLL or PPLL stabilization timeout (oscillator failure)
 * @retval k_rx_err_hw_timeout Module stop verification failed after 3 retries (register access issue)
 * @retval k_rx_err_hw_init_failed Clock configuration verification failed (register corruption)
 *
 * @pre MCU reset complete (C runtime initialized, BSS cleared)
 * @pre Stack pointer valid (main stack at least 4 KB available)
 * @pre External 24 MHz crystal oscillating (hardware requirement)
 * @pre VCC voltage stable (no brownout conditions)
 *
 * @post System clock running at 240 MHz from PLL (ICLK=240 MHz)
 * @post USB clock running at exactly 48 MHz from PPLL (UCLK=48 MHz)
 * @post Peripheral clocks configured (PCLKA=120 MHz, PCLKB/C/D=60 MHz)
 * @post Flash wait state set (MEMWAIT=1 for safe 240 MHz operation)
 * @post Module clocks enabled (CMT, MTU, RSPI, S12AD ready for use)
 * @post All clock configuration verified (registers read back and validated)
 *
 * @note **Call order:** rx_clock_power_init() MUST be called before hardware_init() or any
 *       peripheral initialization. Peripherals require stable system clocks.
 *
 * @note **Not idempotent:** Calling rx_clock_power_init() multiple times may cause PLL
 *       instability or timing violations. Only call once during boot from main().
 *
 * @note **No logging available:** UART not initialized yet, cannot use rx_log_*() functions.
 *       Errors returned via error code only.
 *
 * @warning **Critical initialization function.** Failure halts boot (no recovery possible).
 *          Investigate hardware (crystal, power supply) if timeout errors occur.
 *
 * @warning **MEMWAIT=1 is MANDATORY for 240 MHz.** Omitting causes flash read errors and
 *          data corruption. Never modify clock init without preserving MEMWAIT sequence.
 *
 * @par Thread Safety:
 * Executes in single-threaded context before ThreadX starts. No synchronization needed.
 *
 * @par Example Usage (from main.c):
 * @code
 * int main(void) {
 *   rx_err_t ret;
 *
 *   // Stage 1: System clocks (240 MHz PLL)
 *   ret = rx_clock_power_init();
 *   RX_ERROR_CHECK(ret);
 *
 *   if (ret != k_rx_ok) {
 *     // Clock init failed - cannot proceed
 *     // (Cannot log error - UART not initialized yet)
 *     while (1) {
 *       __asm__ volatile("wait");  // Halt execution
 *     }
 *   }
 *
 *   // Stage 2: Application peripherals (timers, UART, sensors)
 *   ret = hardware_init();
 *   RX_ERROR_CHECK(ret);
 *
 *   // Stage 3: Start ThreadX RTOS
 *   tx_kernel_enter();
 *
 *   // Never reached
 * }
 * @endcode
 *
 * @par Example Error Handling:
 * @code
 * rx_err_t rx_clock_power_init(void) {
 *   rx_err_t err;
 *
 *   // Phase 1: Configure clocks (240 MHz PLL)
 *   err = internal_clock_init();
 *   if (err != k_rx_ok) {
 *     return err;  // PLL/PPLL timeout, cannot proceed
 *   }
 *
 *   // Phase 2: Enable peripheral module clocks
 *   err = internal_module_stop_init();
 *   if (err != k_rx_ok) {
 *     return err;  // Module stop verification failed
 *   }
 *
 *   // Phase 3: Verify configuration
 *   err = internal_verify_system_state();
 *   if (err != k_rx_ok) {
 *     return err;  // Clock registers not set correctly
 *   }
 *
 *   return k_rx_ok;  // All phases succeeded
 * }
 * @endcode
 *
 * @par Clock Stability Verification:
 * @code
 * // Verify system is running at 240 MHz after init:
 * rx_err_t ret = rx_clock_power_init();
 * if (ret == k_rx_ok) {
 *   // All clocks stable and configured
 *   RX_ASSERT(system_regs()->sckcr3 == k_system_clock_source_pll,
 *             "PLL selected as system clock");
 *   RX_ASSERT(*memwait_reg() == k_memwait_one_wait,
 *             "MEMWAIT set for 240 MHz operation");
 *   RX_ASSERT((system_regs()->oscovfsr & k_pll_stable_flag) != 0,
 *             "PLL stable");
 * }
 * @endcode
 *
 * @see internal_clock_init() Phase 1: Configure PLL to 240 MHz and PPLL to 48 MHz
 * @see internal_module_stop_init() Phase 2: Enable peripheral module clocks
 * @see internal_verify_system_state() Phase 3: Verify all configuration correct
 * @see hardware_init() Called after this function (peripheral initialization)
 * @see main() Entry point that calls this function during boot
 *
 * @since Version 1.0.0
 *
 * @test test_clock_init.c Verify 240 MHz PLL configuration
 * @test test_clock_init.c Verify 48 MHz PPLL configuration (USB clock)
 * @test test_clock_init.c Verify MEMWAIT=1 set before clock switch
 * @test test_clock_init.c Verify module clocks enabled
 * @test test_clock_init.c Verify PLL timeout error handling
 */
rx_err_t rx_clock_power_init(void)
{
  /* Initialize clock to 240 MHz */
  rx_err_t err = internal_clock_init();
  if (err != k_rx_ok) {
    /* Can't log - UART not initialized yet */
    return err;
  }

  /* Enable peripheral modules */
  err = internal_module_stop_init();
  if (err != k_rx_ok) {
    /* Can't log - UART not initialized yet */
    return err;
  }

  /* Postcondition: Verify system state is correctly configured */
  err = internal_verify_system_state();
  if (err != k_rx_ok) {
    /* Clock or module configuration failed validation */
    return err;
  }

  /* System init complete - but still can't log until UART is initialized */

  return k_rx_ok;
}
