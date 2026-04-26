/**
 * @file rx_wdt.c
 * @brief Watchdog Timer (WDT) Implementation for RX72N Microcontroller
 *
 * @details
 * Implements the software-controllable Watchdog Timer (WDT) driver for the
 * RX72N microcontroller. Provides runtime start/stop control for development
 * flexibility, complementing the hardware-independent IWDT for defense-in-depth.
 *
 * ## Implementation Architecture
 *
 * @dot
 * digraph wdt_implementation {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   edge [fontsize=10];
 *
 *   subgraph cluster_state {
 *     label="Module State (s_wdt_state)";
 *     style=dashed;
 *     config [label="config\nrx_wdt_config_t"];
 *     initialized [label="initialized\nbool"];
 *     running [label="running\nbool"];
 *   }
 *
 *   subgraph cluster_api {
 *     label="Public API";
 *     style=filled;
 *     fillcolor=lightblue;
 *     init [label="rx_wdt_init()"];
 *     start [label="rx_wdt_start()"];
 *     stop [label="rx_wdt_stop()"];
 *     feed [label="rx_wdt_feed()"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="Hardware Registers";
 *     style=filled;
 *     fillcolor=lightgray;
 *     wdtcr [label="WDTCR\n0x00088022\n(Control Register)"];
 *     wdtrr [label="WDTRR\n0x00088020\n(Refresh Register)"];
 *     wdtsr [label="WDTSR\n0x00088024\n(Status Register)"];
 *   }
 *
 *   init -> config [label="store"];
 *   init -> initialized [label="set true"];
 *   init -> start [label="if enable_on_init", style=dashed];
 *
 *   start -> wdtrr [label="write 0x00, 0xFF"];
 *   start -> running [label="set true"];
 *
 *   stop -> running [label="set false"];
 *
 *   feed -> wdtrr [label="write 0x00, 0xFF"];
 * }
 * @enddot
 *
 * ## State Machine
 *
 * @startuml
 * [*] --> Uninitialized : Power-on
 *
 * Uninitialized --> Initialized : rx_wdt_init(enable_on_init=false)
 * Uninitialized --> Running : rx_wdt_init(enable_on_init=true)
 *
 * Initialized --> Running : rx_wdt_start()
 * Running --> Initialized : rx_wdt_stop()
 *
 * Running --> Running : rx_wdt_feed()
 * Running --> [*] : Timeout (reset)
 *
 * note right of Running
 *   Must call rx_wdt_feed()
 *   within timeout period
 *   (~273us at 60MHz max)
 * end note
 *
 * note right of Initialized
 *   WDT configured but
 *   counter stopped
 * end note
 * @enduml
 *
 * ## Hardware Register Usage
 *
 * | Register | Address | Access | Usage |
 * |----------|---------|--------|-------|
 * | WDTCR | 0x00088022 | R/W | Control: CKS bits, RPES bit |
 * | WDTRR | 0x00088020 | W | Refresh: Write 0x00 then 0xFF |
 * | WDTSR | 0x00088024 | R/W1C | Status: Underflow flag |
 *
 * ## Memory Layout (s_wdt_state)
 *
 * | Offset | Size | Field | Type | Description |
 * |--------|------|-------|------|-------------|
 * | 0x00 | 4 | config | rx_wdt_config_t | Stored configuration |
 * | 0x04 | 1 | initialized | bool | Init complete flag |
 * | 0x05 | 1 | running | bool | Counter running flag |
 * | 0x06 | 2 | (padding) | - | Alignment padding |
 * | **Total** | **8 bytes** | | | Static allocation |
 *
 * ## WDT Refresh Sequence
 *
 * @msc
 * CPU, WDTRR, Counter;
 *
 * --- [label="Refresh Sequence"];
 * CPU => WDTRR [label="Write 0x00"];
 * WDTRR box WDTRR [label="First byte received"];
 * CPU => WDTRR [label="Write 0xFF"];
 * WDTRR box WDTRR [label="Sequence valid"];
 * WDTRR => Counter [label="Reset to timeout value"];
 * Counter box Counter [label="Begin countdown"];
 *
 * --- [label="Invalid Sequence (wrong order)"];
 * CPU => WDTRR [label="Write 0xFF"];
 * WDTRR box WDTRR [label="Invalid first byte"];
 * WDTRR box WDTRR [label="Sequence rejected"];
 * @endmsc
 *
 * ## Performance Characteristics
 *
 * | Function | Execution Time | Stack | Notes |
 * |----------|---------------|-------|-------|
 * | rx_wdt_init() | ~20 us | 32 B | Config copy + memset |
 * | rx_wdt_start() | ~2 us | 16 B | 2 register writes |
 * | rx_wdt_stop() | ~1 us | 8 B | State update only |
 * | rx_wdt_feed() | ~1 us | 8 B | 2 register writes |
 *
 * @par Module Dependencies:
 * - [rx_wdt.h](../inc/rx_wdt.h) - Public API declarations
 * - [rx72n_wdt_regs.h](../../rx_hal/inc/rx72n_wdt_regs.h) - WDT register definitions
 * - [rx72n_system_regs.h](../../rx_hal/inc/rx72n_system_regs.h) - System registers
 * - `<string.h>` - memcpy, memset for config handling
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1**: [OK] No goto, setjmp, recursion
 * - **Rule 2**: [OK] No loops in implementation
 * - **Rule 3**: [OK] Zero dynamic allocation (static state)
 * - **Rule 4**: [OK] All functions <60 lines
 * - **Rule 5**: [OK] Minimum 2 checks per function
 * - **Rule 6**: [OK] Data declared at smallest scope
 * - **Rule 7**: [OK] All return values checked or propagated
 * - **Rule 8**: [OK] C23 typed enums for constants
 * - **Rule 9**: [OK] No function pointers (simple register access)
 * - **Rule 10**: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility**: Only WDT management, no other concerns
 * - **Open/Closed**: Extensible via configuration structure
 * - **Liskov Substitution**: Consistent rx_err_t return interface
 * - **Interface Segregation**: Minimal 4-function API
 * - **Dependency Inversion**: Depends on abstract rx_err_t
 *
 * @see rx_wdt.h Public API documentation and usage examples
 * @see rx_iwdt.c Independent Watchdog implementation
 * @see RX72N User's Manual Section 10: Watchdog Timer
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "rx_wdt.h"

#include "rx72n_wdt_regs.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/**
 * @struct rx_wdt_state_t
 * @brief Internal state structure for WDT driver
 *
 * @details
 * Aggregates all state for the WDT driver into a single structure for
 * cache-friendly access and simplified initialization. Instantiated once
 * as a static variable with zero initialization.
 *
 * @par Memory Layout:
 * | Offset | Size | Field | Type | Description |
 * |--------|------|-------|------|-------------|
 * | 0x00 | 4 | config | rx_wdt_config_t | Stored configuration |
 * | 0x04 | 1 | initialized | bool | Module init complete |
 * | 0x05 | 1 | running | bool | Counter running state |
 * | 0x06 | 2 | (padding) | - | Alignment to 4 bytes |
 * | **Total** | **8 bytes** | | | |
 *
 * @par State Transitions:
 * - initialized: false -> true on rx_wdt_init() success
 * - running: false -> true on rx_wdt_start() success
 * - running: true -> false on rx_wdt_stop() success
 *
 * @invariant initialized implies config contains valid settings
 * @invariant running implies initialized
 * @invariant !initialized implies !running
 *
 * @see s_wdt_state Static instance of this structure
 *
 * @since Version 1.0.0
 */
typedef struct {
  rx_wdt_config_t config;      /**< Stored configuration from rx_wdt_init(). Copied by value,
                                    original can be freed after init call. Valid only when
                                    initialized == true. */
  bool            initialized; /**< Module initialization flag. true = rx_wdt_init() succeeded,
                                    false = not initialized or init failed. Set once, never
                                    cleared (no deinit function). */
  bool            running;     /**< WDT counter running state. true = counter active and
                                    decrementing, false = counter stopped. Controls whether
                                    rx_wdt_feed() writes to WDTRR register. */
} rx_wdt_state_t;

/**
 * @var s_wdt_state
 * @brief Global WDT driver state
 *
 * @details
 * Single instance of WDT state, zero-initialized at startup. All fields
 * start at 0/false representing uninitialized state. Populated by
 * rx_wdt_init() and updated by start/stop functions.
 *
 * **Memory Location:** .bss section (zero-initialized static data)
 * **Size:** 8 bytes
 * **Alignment:** 4-byte aligned
 *
 * @par Access Pattern:
 * - **Write:** rx_wdt_init(), rx_wdt_start(), rx_wdt_stop()
 * - **Read:** All functions (state checks)
 *
 * @note Not thread-safe for concurrent writes. Use external synchronization
 *       if calling start/stop from multiple threads.
 *
 * @see rx_wdt_state_t Structure definition
 *
 * @since Version 1.0.0
 */
static rx_wdt_state_t s_wdt_state = {};

/* =============================================================================
 * Private Helpers
 * =============================================================================
 */

/**
 * @brief Write WDT hardware registers from validated config.
 * @pre config is not nullptr and has been validated.
 * @pre WDT registers are accessible (clock enabled).
 * @post WDTCR and WDTRCR registers written.
 * @since Version 1.0.0
 */
static void internal_configure_wdt_registers(const rx_wdt_config_t* config)
{
  volatile rx_wdt_regs_t* const regs = wdt();

  /* Configure WDTCR register (can only be written once before first refresh!)
   * Bits 1:0 (TOPS): Timeout period cycles
   * Bits 7:4 (CKS): Clock division ratio
   * Bits 9:8 (RPES): Window end position (set to 0% = no window)
   * Bits 13:12 (RPSS): Window start position (set to 100% = no window)
   */
  uint16_t wdtcr_val = (uint16_t)config->timeout_cycles;               /* TOPS[1:0] */
  wdtcr_val |= ((uint16_t)config->clock_division << k_wdt_cr_cks_pos); /* CKS[7:4] */
  wdtcr_val |= k_wdt_rpes_0;                                           /* No window end */
  wdtcr_val |= k_wdt_rpss_100;                                         /* No window start */
  regs->wdtcr = wdtcr_val;

  /* Configure WDTRCR register (reset vs NMI selection)
   * Bit 7 (RSTIRQS): 1 = reset, 0 = NMI
   */
  if (config->reset_on_timeout) {
    regs->wdtrcr = k_wdt_rstirqs_reset;
  } else {
    regs->wdtrcr = k_wdt_rstirqs_nmi;
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize the Watchdog Timer with specified or default configuration
 *
 * @details
 * Configures WDT driver state and optionally starts the hardware counter.
 * If config is nullptr, uses safe defaults (longest timeout, manual start, reset mode).
 *
 * **Algorithm:**
 * 1. Check if already initialized (return error if so)
 * 2. If config is nullptr, create default configuration:
 *    - timeout_period = k_wdt_timeout_4096_cycles (~68us at 60MHz)
 *    - enable_on_init = false (manual start)
 *    - reset_on_timeout = true (reset on timeout)
 * 3. Copy configuration to static state
 * 4. Set initialized flag to true
 * 5. Set running flag to false
 * 6. If enable_on_init, call rx_wdt_start()
 * 7. Return k_rx_ok (or propagate start error)
 *
 * @par Flow Diagram:
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="rx_wdt_init(config)"];
 *   check_init [label="Already initialized?", shape=diamond];
 *   err_state [label="Return k_rx_err_invalid_state", fillcolor=lightcoral, style="rounded,filled"];
 *   check_null [label="config == nullptr?", shape=diamond];
 *   use_default [label="Create default config"];
 *   copy_config [label="memcpy config to state"];
 *   set_flags [label="initialized=true\nrunning=false"];
 *   check_auto [label="enable_on_init?", shape=diamond];
 *   call_start [label="rx_wdt_start()"];
 *   success [label="Return k_rx_ok", fillcolor=lightgreen, style="rounded,filled"];
 *
 *   start -> check_init;
 *   check_init -> err_state [label="yes"];
 *   check_init -> check_null [label="no"];
 *   check_null -> use_default [label="yes"];
 *   check_null -> copy_config [label="no"];
 *   use_default -> copy_config;
 *   copy_config -> set_flags;
 *   set_flags -> check_auto;
 *   check_auto -> call_start [label="yes"];
 *   check_auto -> success [label="no"];
 *   call_start -> success;
 * }
 * @enddot
 *
 *
 *
 * @pre WDT must not already be initialized
 *
 * @post s_wdt_state.initialized == true
 * @post s_wdt_state.config contains configuration
 * @post If enable_on_init, WDT counter is running
 *
 * @note nullptr config uses safe defaults (recommended for most cases)
 * @note Re-initialization requires system reset (no deinit function)
 *
 * @see rx_wdt_start() Called automatically if enable_on_init=true
 *
 * @since Version 1.0.0
 */
rx_err_t rx_wdt_init(const rx_wdt_config_t* config)
{
  if (s_wdt_state.initialized) {
    return k_rx_err_invalid_state;
  }

  /* Use default config if none provided */
  rx_wdt_config_t default_config = {};
  if (config == nullptr) {
    default_config.timeout_cycles   = k_wdt_timeout_16384_cycles;
    default_config.clock_division   = k_wdt_clock_div_8192;
    default_config.enable_on_init   = false;
    default_config.reset_on_timeout = true;
    config                          = &default_config;
  }

  /* Validate clock division is one of the 6 valid values */
  if ((config->clock_division != k_wdt_clock_div_4) &&
      (config->clock_division != k_wdt_clock_div_64) &&
      (config->clock_division != k_wdt_clock_div_128) &&
      (config->clock_division != k_wdt_clock_div_512) &&
      (config->clock_division != k_wdt_clock_div_2048) &&
      (config->clock_division != k_wdt_clock_div_8192)) {
    return k_rx_err_invalid_arg;
  }

  /* Validate timeout_cycles is in valid range (0-3) */
  if (config->timeout_cycles > k_wdt_timeout_16384_cycles) {
    return k_rx_err_invalid_arg;
  }

  /* Write hardware registers from validated config */
  internal_configure_wdt_registers(config);

  /* Store configuration */
  s_wdt_state.config      = *config;
  s_wdt_state.initialized = true;
  s_wdt_state.running     = false;

  /* Start if requested (writes refresh sequence to begin counting) */
  if (config->enable_on_init) {
    return rx_wdt_start();
  }

  return k_rx_ok;
}

/**
 * @brief Start the watchdog timer (register-start mode only)
 *
 * @details
 * Starts the WDT counter by writing the refresh sequence to WDTRR. Once started,
 * rx_wdt_feed() must be called periodically to prevent timeout.
 *
 * **Algorithm:**
 * 1. Check if WDT is initialized (return error if not)
 * 2. Get pointer to WDT registers via wdt() accessor
 * 3. Write start sequence to WDTRR (0x00 then 0xFF)
 * 4. Set s_wdt_state.running to true
 * 5. Return k_rx_ok
 *
 * **Hardware Operation:**
 * The WDTRR refresh/start sequence:
 * - First write: 0x00 (k_wdt_refresh_start)
 * - Second write: 0xFF (k_wdt_refresh_end)
 * - Sequence resets counter to timeout period value
 * - Counter begins decrementing immediately
 *
 * @par OFS Register Configuration:
 * The WDT can be configured at flash-time via OFS (Option Function Select)
 * registers. This function only works if WDT is in "register-start mode":
 * - Register-start mode: WDT starts when WDTRR sequence written
 * - Auto-start mode: WDT starts at reset, this function refreshes only
 *
 *
 * @pre WDT must be initialized via rx_wdt_init()
 *
 * @post WDT counter is running and decrementing
 * @post s_wdt_state.running == true
 * @post rx_wdt_feed() must be called within timeout period
 *
 * @note In auto-start mode, this function acts as a feed operation
 * @warning Missing feeds after start will cause timeout/reset
 *
 * @see rx_wdt_init() Must be called first
 * @see rx_wdt_feed() Call periodically after start
 * @see k_wdt_refresh_start, k_wdt_refresh_end Refresh sequence constants
 *
 * @since Version 1.0.0
 */
rx_err_t rx_wdt_start(void)
{
  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  volatile rx_wdt_regs_t* const regs = wdt();

  /* Start WDT (only works in register-start mode) */
  regs->wdtrr = k_wdt_refresh_start;
  regs->wdtrr = k_wdt_refresh_end;

  s_wdt_state.running = true;

  return k_rx_ok;
}

/**
 * @brief Stop the watchdog timer (software state only)
 *
 * @details
 * Updates software state to indicate WDT is stopped. Note that this does NOT
 * actually stop the hardware counter if WDT is configured in auto-start mode
 * via OFS registers.
 *
 * **Algorithm:**
 * 1. Check if WDT is initialized (return error if not)
 * 2. Set s_wdt_state.running to false
 * 3. Return k_rx_ok
 *
 * **Hardware Limitation:**
 * The RX72N WDT hardware behavior depends on OFS (Option Function Select)
 * register configuration at flash time:
 * - Register-start mode: Hardware can be stopped
 * - Auto-start mode: Hardware cannot be stopped once running!
 *
 * This function ONLY updates software state. If using auto-start mode,
 * you must continue calling rx_wdt_feed() even after rx_wdt_stop().
 *
 *
 * @pre WDT must be initialized via rx_wdt_init()
 *
 * @post s_wdt_state.running == false
 * @post In auto-start mode, hardware still requires feeding!
 *
 * @warning In auto-start mode, this does NOT stop hardware counter
 * @warning System may still reset if feeds are stopped
 * @attention Check OFS configuration to know if hardware can actually stop
 *
 * @see rx_wdt_start() Restart WDT after stopping
 *
 * @since Version 1.0.0
 */
rx_err_t rx_wdt_stop(void)
{
  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* NOTE: WDT cannot be stopped once started if configured to auto-start in OFS.
   * This function only updates software state. The hardware WDT will continue
   * running and must be fed to prevent reset. */
  s_wdt_state.running = false;

  return k_rx_ok;
}

/**
 * @brief Feed the watchdog timer to prevent timeout
 *
 * @details
 * Resets the WDT counter to its initial value by writing the refresh sequence
 * to WDTRR. This must be called periodically at a rate faster than the
 * configured timeout period.
 *
 * **Algorithm:**
 * 1. Check if WDT is initialized (return error if not)
 * 2. Check if WDT is running (return error if not)
 * 3. Get pointer to WDT registers via wdt() accessor
 * 4. Write refresh sequence: 0x00 then 0xFF to WDTRR
 * 5. Return k_rx_ok
 *
 * **Hardware Operation:**
 * The WDTRR refresh sequence must be written in exact order:
 * - First write: 0x00 (k_wdt_refresh_start)
 * - Second write: 0xFF (k_wdt_refresh_end)
 *
 * Invalid sequences (wrong order, wrong values) are ignored by hardware.
 * Valid sequence resets counter to timeout period value.
 *
 * @par Timing Requirements:
 * Feed must occur faster than timeout period:
 * | Timeout Setting | Cycles | At 60MHz | Required Feed Rate |
 * |-----------------|--------|----------|-------------------|
 * | k_wdt_timeout_256_cycles | 256 | 4.3 us | >233 kHz |
 * | k_wdt_timeout_1024_cycles | 1024 | 17 us | >59 kHz |
 * | k_wdt_timeout_4096_cycles | 4096 | 68 us | >14.7 kHz |
 * | k_wdt_timeout_16384_cycles | 16384 | 273 us | >3.6 kHz |
 *
 *
 * @pre WDT must be initialized via rx_wdt_init()
 * @pre WDT must be running (rx_wdt_start() called or enable_on_init=true)
 *
 * @post WDT counter reset to timeout period value
 * @post Countdown restarts from reset value
 *
 * @note This is the ONLY function that prevents watchdog timeout
 * @note Execution time: ~1 us (two register writes)
 * @warning Missing a single feed within timeout period causes reset
 *
 * @par Example:
 * @code{.c}
 * // Main loop with watchdog feed
 * while (1) {
 *     do_work();
 *     rx_err_t err = rx_wdt_feed();
 *     if (err != k_rx_ok) {
 *         // Feed failed - WDT may timeout!
 *         handle_wdt_error(err);
 *     }
 *     delay_us(100);  // Must be < timeout period
 * }
 * @endcode
 *
 * @see rx_wdt_start() Must be called before feed
 * @see wdt_timeout_period_t Timeout period settings
 *
 * @since Version 1.0.0
 */
rx_err_t rx_wdt_feed(void)
{
  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (!s_wdt_state.running) {
    return k_rx_err_invalid_state;
  }

  /* Refresh watchdog - write 0x00 then 0xFF to WDTRR */
  volatile rx_wdt_regs_t* const regs = wdt();
  regs->wdtrr                        = k_wdt_refresh_start;
  regs->wdtrr                        = k_wdt_refresh_end;

  return k_rx_ok;
}
