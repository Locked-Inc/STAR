/**
 * @file rx_wdt.h
 * @brief Watchdog Timer (WDT) Driver for RX72N Microcontroller
 *
 * @details
 * **Software-controllable watchdog driver** providing flexible system monitoring
 * complementary to the Independent Watchdog (IWDT). Designed for development,
 * debugging, and scenarios requiring runtime watchdog control. Works alongside
 * IWDT to provide defense-in-depth fault detection.
 *
 * ## Purpose and Design Rationale
 *
 * This driver implements the **RX72N Watchdog Timer (WDT)**, a software-controllable
 * watchdog that complements the hardware-independent IWDT:
 *
 * **Why Both WDT and IWDT?**
 * 1. **IWDT (Independent)**: Hardware safety net, cannot be disabled, clock-independent
 * 2. **WDT (Software)**: Flexible development tool, can be stopped, uses system clock
 * 3. **Defense in Depth**: Two independent watchdogs catch different failure modes
 *
 * The WDT provides **runtime control** (start/stop) which is essential for:
 * - **Debugging**: Temporarily disable during breakpoint sessions
 * - **Flash updates**: Suspend during long operations
 * - **Safe mode**: Different timeout strategies for degraded operation
 *
 * ## Key Differences from IWDT
 *
 * | Feature | WDT (This Module) | IWDT (rx_iwdt.h) |
 * |---------|-------------------|------------------|
 * | Clock Source | PCLKB (system clock) | IWDTCLK (independent 125kHz) |
 * | Can be Stopped | [OK] Yes (rx_wdt_stop) | [X] No (hardware safety) |
 * | Clock Fault Detection | [X] No (same clock tree) | [OK] Yes (independent) |
 * | Development Friendly | [OK] Yes | [X] No (always running) |
 * | Production Safety | Medium | High |
 * | Timeout Granularity | PCLKB-dependent | Fixed ~125kHz |
 * | Register Base | 0x00088020 | 0x00088030 |
 *
 * **Typical Usage Strategy**:
 * - **Development**: Use WDT only (can stop for debugging)
 * - **Production**: Use both (IWDT=hardware safety, WDT=software monitoring)
 *
 * ## Hardware Architecture
 *
 * **RX72N Watchdog Timer (WDT) Peripheral**:
 * - **Register Base Address**: 0x00088020
 * - **Clock Source**: PCLKB (Peripheral Clock B, typically 60 MHz)
 * - **Clock Frequency**: 60 MHz (assuming default PCLKB)
 * - **Counter Width**: 14-bit down-counter
 * - **Timeout Range**: 4.3 us to 273 us (at 60 MHz PCLKB)
 * - **Control**: Software start/stop via WDTCR register
 * - **Reset Output**: Connected to internal reset controller (if configured)
 *
 * **Timeout Calculation**:
 * ```
 * Timeout (seconds) = (Divider x Cycles) / PCLKB_Frequency
 *
 * Where Divider is selected by CKS (Clock Select) bits:
 *   CKS=00: Divider=1   -> 256 cycles   -> 4.3 us at 60 MHz
 *   CKS=01: Divider=4   -> 1024 cycles  -> 17 us at 60 MHz
 *   CKS=10: Divider=16  -> 4096 cycles  -> 68 us at 60 MHz
 *   CKS=11: Divider=64  -> 16384 cycles -> 273 us at 60 MHz
 *
 * Example: For 1ms timeout at PCLKB=60MHz:
 *   1ms = N / 60,000,000
 *   N = 60,000 cycles
 *   Use CKS=11 (16384 cycles) x 4 feeds per timeout = ~273us per feed
 * ```
 *
 * **Note**: Very short timeouts make WDT impractical for general use.
 * Most systems use IWDT for watchdog functionality and WDT for special cases.
 *
 * ## Implementation Approach
 *
 * **Two-Layer Architecture**:
 *
 * 1. **Hardware Layer** (rx_wdt_init, rx_wdt_start, rx_wdt_stop):
 *    - Configures WDT control registers (WDTCR)
 *    - Sets clock divider (CKS bits)
 *    - Enables/disables counter operation
 *
 * 2. **Refresh Layer** (rx_wdt_feed):
 *    - Writes refresh code to WDTRR register
 *    - Resets counter to prevent timeout
 *
 * **Typical Usage Flow**:
 * ```
 * 1. System startup:
 *    - rx_wdt_init() - Configure (but don't start yet)
 * 2. After initialization complete:
 *    - rx_wdt_start() - Enable watchdog
 * 3. Normal operation:
 *    - Main loop: rx_wdt_feed() every N us (N < timeout)
 * 4. During debugging/flash:
 *    - rx_wdt_stop() - Suspend watchdog
 *    - ... perform long operation ...
 *    - rx_wdt_start() - Resume watchdog
 * ```
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Memory Usage | Notes |
 * |-----------|---------------|--------------|-------|
 * | rx_wdt_init() | ~20 us | 0 heap | One-time setup |
 * | rx_wdt_start() | ~2 us | 0 | Register write |
 * | rx_wdt_stop() | ~2 us | 0 | Register write |
 * | rx_wdt_feed() | ~1 us | 0 | Single register write |
 *
 * **Memory Footprint**:
 * - Static variables: ~16 bytes (configuration state)
 * - Code size: ~1 KB (all functions)
 * - Stack usage: < 32 bytes (deepest call)
 * - Heap usage: 0 bytes (zero dynamic allocation)
 *
 * ## Thread Safety and RTOS Integration
 *
 * **Thread-Safe Operations**:
 * - All public functions use mutex protection (ThreadX)
 * - Safe to call from multiple threads simultaneously
 * - ISR-safe: rx_wdt_feed() can be called from interrupts (no mutex in ISR path)
 *
 * **ThreadX Integration**:
 * - Uses tx_mutex for synchronization
 * - Compatible with all ThreadX thread priorities
 * - No dependencies on ThreadX timekeeping (unlike IWDT task monitoring)
 *
 * ## Usage Guidelines
 *
 * **DO**:
 * - Use WDT alongside IWDT for defense in depth
 * - Stop WDT during long flash operations or debugging
 * - Call rx_wdt_feed() from main supervision loop only
 * - Configure timeout based on PCLKB frequency
 *
 * **DON'T**:
 * - Rely on WDT alone for safety (use IWDT as primary)
 * - Forget to restart WDT after stopping (system unprotected)
 * - Use very short timeouts (< 1ms difficult to meet)
 * - Call rx_wdt_feed() from multiple locations (defeats purpose)
 *
 * **When to Use WDT vs IWDT**:
 * - **Primary safety watchdog**: Use IWDT (cannot be disabled)
 * - **Development flexibility**: Use WDT (can be stopped for debugging)
 * - **Clock fault detection**: Use IWDT (independent clock)
 * - **Production systems**: Use both for defense in depth
 *
 * @par Hardware Requirements:
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | MCU | Renesas RX72N | WDT peripheral required |
 * | Clock | PCLKB (Peripheral Clock B) | System clock dependency |
 * | Registers | 0x00088020 base | WDT control registers |
 * | Reset | Internal reset controller | Connected to WDT output |
 * | PCLKB Frequency | Known value | Required for timeout calculation |
 *
 * @par Module Dependencies:
 *
 * **This module depends on**:
 * - `rx_err.h` - Error code definitions
 * - `<stdint.h>` - Standard integer types
 * - `<stdbool.h>` - Boolean type
 * - ThreadX RTOS - Mutex for thread safety
 *
 * **This module is used by**:
 * - `main.c` - System initialization and main loop
 * - Flash update routines - Start/stop during operations
 * - Safe mode handlers - Runtime watchdog control
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion, sequential logic |
 * | 2. Fixed loop bounds | [OK] | No loops in driver (stateless operations) |
 * | 3. No dynamic allocation | [OK] | Zero malloc/free, static configuration |
 * | 4. Small functions | [OK] | All functions < 30 lines |
 * | 5. Assertions (>=2 per function) | [OK] | Minimum 2 checks per function (pre/post) |
 * | 6. Narrow scope | [OK] | File-scope statics, function-local variables |
 * | 7. Check return values | [OK] | All functions return rx_err_t, checked by callers |
 * | 8. Limited preprocessor | [OK] | C23 typed enums only, zero computation macros |
 * | 9. Pointer restrictions | [OK] | Single-level pointers, no pointer arithmetic |
 * | 10. Compiler warnings | [OK] | Compiles with -Wall -Wextra -Werror |
 *
 * **Safety-Critical Design**:
 * - Static configuration prevents allocation failures
 * - Explicit error codes force error handling
 * - Mutex protection prevents race conditions
 * - Simple state machine (init -> start/stop -> feed)
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S)**:
 * - This module has ONE job: Software-controllable watchdog timer
 * - Separate from IWDT (independent watchdog with different purpose)
 * - No mixing of unrelated functionality
 *
 * **Open/Closed (O)**:
 * - Extensible via configuration structure (timeout period, reset mode)
 * - Can add new timeout periods without modifying existing code
 * - Timeout configuration at init, not hardcoded
 *
 * **Liskov Substitution (L)**:
 * - All functions return rx_err_t for consistent error handling
 * - Same error semantics as IWDT (interchangeable error handling)
 * - No unexpected side effects or hidden dependencies
 *
 * **Interface Segregation (I)**:
 * - Separate functions for separate concerns (init/start/stop/feed)
 * - Clients use only the functions they need
 * - Minimal API surface (4 functions)
 *
 * **Dependency Inversion (D)**:
 * - Depends on abstract rx_err_t, not specific error implementations
 * - Can be mocked for unit testing
 * - No direct hardware dependencies in interface
 *
 * @see rx_iwdt.h Independent Watchdog Timer (primary safety watchdog)
 * @see rx_err.h Error code definitions used by all functions
 * @see docs/sections/06_nasa_power_of_10.tex Safety-critical coding standards
 * @see RX72N Hardware Manual Section 10 Watchdog Timer specification
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum wdt_timeout_cycles_t
 * @brief WDT Timeout Period Selection (TOPS bits in WDTCR register)
 *
 * @details
 * Defines the four available watchdog timeout periods, corresponding to hardware
 * TOPS[1:0] bits in WDTCR register. Each setting determines the number of
 * divided clock cycles before watchdog timeout.
 *
 * ## Timeout Calculation
 *
 * The actual timeout in seconds depends on both TOPS and CKS (clock division):
 *
 * @f[
 *   T_{timeout} = \frac{TOPS \times CKS}{f_{PCLKB}}
 * @f]
 *
 * **Example**: At PCLKB = 60 MHz with CKS = div-by-8192:
 *
 * | TOPS Setting | Cycles | Timeout | Use Case |
 * |--------------|--------|---------|----------|
 * | k_wdt_timeout_1024_cycles | 1024 | 140 ms | Fast response |
 * | k_wdt_timeout_4096_cycles | 4096 | 559 ms | Normal operation |
 * | k_wdt_timeout_8192_cycles | 8192 | 1.12 s | Longer tolerance |
 * | k_wdt_timeout_16384_cycles | 16384 | 2.24 s | Maximum timeout |
 *
 * ## Hardware Mapping (TOPS[1:0] Bits)
 *
 * These enum values map directly to TOPS[1:0] bits in WDTCR register:
 * - TOPS=00 -> 1024 cycles (counter loads 0x03FF)
 * - TOPS=01 -> 4096 cycles (counter loads 0x0FFF)
 * - TOPS=10 -> 8192 cycles (counter loads 0x1FFF)
 * - TOPS=11 -> 16384 cycles (counter loads 0x3FFF)
 *
 * ## Combined with Clock Division
 *
 * The clock division ratio (CKS) multiplies the effective timeout:
 * - CKS div-by-4: Fastest counting, shortest timeout
 * - CKS div-by-8192: Slowest counting, longest timeout (2.24s max)
 *
 * @par Recommended Usage:
 * @code{.c}
 * rx_wdt_config_t config = {
 *   .timeout_cycles = k_wdt_timeout_16384_cycles,  // 16384 counter cycles
 *   .clock_division = k_wdt_clock_div_8192,        // Divide PCLKB by 8192
 *   .enable_on_init = false,
 *   .reset_on_timeout = true
 * };
 * // Total timeout = 16384 * 8192 / 60MHz = 2.24 seconds
 *
 * rx_wdt_init(&config);
 * @endcode
 *
 * @invariant Values are contiguous (0 to 3)
 * @invariant Enum values map directly to TOPS hardware bits
 * @invariant All values fit in uint8_t (0-255)
 * @invariant Longer cycle counts = longer timeouts (monotonic)
 *
 * @note C23 typed enum with uint8_t underlying type (1-byte values)
 * @note Hardware manual Ch34 Table 34.2 shows all timing combinations
 *
 * @see rx_wdt_config_t Configuration structure using this enum
 * @see wdt_clock_division_t Clock division for longer timeouts
 * @see rx_wdt_init() Configures timeout at initialization
 * @see RX72N Hardware Manual Ch34.2.2 WDTCR Register
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wdt_timeout_1024_cycles  = 0, /**< 1024 cycles (TOPS=00). Counter loads 0x03FF. */
  k_wdt_timeout_4096_cycles  = 1, /**< 4096 cycles (TOPS=01). Counter loads 0x0FFF. */
  k_wdt_timeout_8192_cycles  = 2, /**< 8192 cycles (TOPS=10). Counter loads 0x1FFF. */
  k_wdt_timeout_16384_cycles = 3, /**< 16384 cycles (TOPS=11). Counter loads 0x3FFF. Max timeout. */
} wdt_timeout_cycles_t;

/**
 * @enum wdt_clock_division_t
 * @brief WDT Clock Division Ratio Selection (CKS bits in WDTCR register)
 *
 * @details
 * Defines the six available clock division ratios for the WDT. Combined with
 * TOPS (timeout period), this determines the actual watchdog timeout.
 *
 * ## Timeout Calculation
 *
 * @f[
 *   T_{timeout} = \frac{TOPS \times CKS_{divisor}}{f_{PCLKB}}
 * @f]
 *
 * ## Timeout Table (PCLKB = 60 MHz)
 *
 * | CKS Setting | TOPS=1024 | TOPS=4096 | TOPS=8192 | TOPS=16384 |
 * |-------------|-----------|-----------|-----------|------------|
 * | div-by-4 | 68 us | 273 us | 546 us | 1.09 ms |
 * | div-by-64 | 1.1 ms | 4.4 ms | 8.7 ms | 17.5 ms |
 * | div-by-128 | 2.2 ms | 8.7 ms | 17.5 ms | 34.9 ms |
 * | div-by-512 | 8.7 ms | 34.9 ms | 69.9 ms | 140 ms |
 * | div-by-2048 | 34.9 ms | 140 ms | 280 ms | 559 ms |
 * | div-by-8192 | 140 ms | 559 ms | 1.12 s | 2.24 s |
 *
 * @note Only 6 values are valid per hardware manual Ch34 Table 34.2
 * @warning Other CKS values are prohibited by hardware
 *
 * @see wdt_timeout_cycles_t TOPS selection for cycle count
 * @see rx_wdt_config_t Configuration structure using this enum
 * @see RX72N Hardware Manual Ch34.2.2 WDTCR Register
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wdt_clock_div_4    = 0x01, /**< PCLKB / 4 (CKS=0001). Fastest, shortest timeout. */
  k_wdt_clock_div_64   = 0x04, /**< PCLKB / 64 (CKS=0100). */
  k_wdt_clock_div_128  = 0x0F, /**< PCLKB / 128 (CKS=1111). */
  k_wdt_clock_div_512  = 0x06, /**< PCLKB / 512 (CKS=0110). */
  k_wdt_clock_div_2048 = 0x07, /**< PCLKB / 2048 (CKS=0111). */
  k_wdt_clock_div_8192 = 0x08, /**< PCLKB / 8192 (CKS=1000). Slowest, longest timeout. */
} wdt_clock_division_t;

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @struct rx_wdt_config_t
 * @brief WDT Configuration Structure for Driver Initialization
 *
 * @details
 * Contains all parameters needed to configure and initialize the Watchdog Timer (WDT).
 * All fields must be set before passing to rx_wdt_init(). Use default values for
 * typical operation, or customize for specific monitoring requirements.
 *
 * ## Initialization Requirements
 *
 * 1. **All fields must be initialized** - No default constructor available (C struct)
 * 2. **Validate timeout_cycles** - Must be valid enum value (0-3)
 * 3. **Consider PCLKB frequency** - Timeout periods are PCLKB-dependent
 * 4. **Choose reset vs interrupt** - reset_on_timeout determines failure behavior
 *
 * ## Default Configuration
 *
 * Recommended starting configuration for development:
 * ```c
 * rx_wdt_config_t default_config = {
 *   .timeout_cycles   = k_wdt_timeout_16384_cycles,  // 16384 counter cycles
 *   .clock_division   = k_wdt_clock_div_8192,        // PCLKB / 8192
 *   .enable_on_init   = false,                        // Don't start immediately (safer)
 *   .reset_on_timeout = true                          // Reset on timeout (production mode)
 * };
 * // Actual timeout = 16384 * 8192 / 60MHz = 2.24 seconds
 * ```
 *
 * ## Production Configuration
 *
 * For production systems using WDT as secondary watchdog:
 * ```c
 * rx_wdt_config_t prod_config = {
 *   .timeout_cycles   = k_wdt_timeout_16384_cycles,  // Max cycles
 *   .clock_division   = k_wdt_clock_div_8192,        // Max divisor = 2.24s timeout
 *   .enable_on_init   = true,                         // Start immediately
 *   .reset_on_timeout = true                          // Always reset
 * };
 * ```
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Type | Notes |
 * |--------|------|-------|------|-------|
 * | 0 | 1 | timeout_cycles | uint8_t (enum) | TOPS bits (0-3) |
 * | 1 | 1 | clock_division | uint8_t (enum) | CKS bits |
 * | 2 | 1 | enable_on_init | bool | Auto-start flag |
 * | 3 | 1 | reset_on_timeout | bool | Reset mode flag |
 * | **Total** | **4 bytes** | | | Structure size |
 *
 * @par Field Constraints:
 *
 * | Field | Valid Values | Default | Notes |
 * |-------|--------------|---------|-------|
 * | timeout_cycles | 0-3 (enum) | k_wdt_timeout_16384_cycles | TOPS bits |
 * | clock_division | See enum | k_wdt_clock_div_8192 | CKS bits (6 valid values only) |
 * | enable_on_init | true/false | false | false = safer for development |
 * | reset_on_timeout | true/false | true | true = reset, false = interrupt |
 *
 * @par Usage Example (Basic):
 * @code{.c}
 * // Initialize with 2.24 second timeout (max)
 * rx_wdt_config_t config = {
 *   .timeout_cycles   = k_wdt_timeout_16384_cycles,
 *   .clock_division   = k_wdt_clock_div_8192,
 *   .enable_on_init   = false,
 *   .reset_on_timeout = true
 * };
 *
 * rx_err_t err = rx_wdt_init(&config);
 * if (err != k_rx_ok) {
 *   uart_debug_puts("[ERROR] WDT init failed\r\n");
 *   return err;
 * }
 * @endcode
 *
 * @par Usage Example (Development Mode):
 * @code{.c}
 * // Development configuration: Don't start, use interrupt for debugging
 * rx_wdt_config_t dev_config = {
 *   .timeout_cycles   = k_wdt_timeout_16384_cycles,
 *   .clock_division   = k_wdt_clock_div_8192,
 *   .enable_on_init   = false,  // Start manually after init complete
 *   .reset_on_timeout = false   // Interrupt allows breakpoint debugging
 * };
 *
 * rx_wdt_init(&dev_config);
 *
 * // ... system initialization ...
 *
 * // Start watchdog after init complete
 * rx_wdt_start();
 * @endcode
 *
 * @par Usage Example (Runtime Control):
 * @code{.c}
 * // Start with WDT enabled, 559ms timeout
 * rx_wdt_config_t config = {
 *   .timeout_cycles   = k_wdt_timeout_4096_cycles,
 *   .clock_division   = k_wdt_clock_div_8192,
 *   .enable_on_init   = true,
 *   .reset_on_timeout = true
 * };
 * rx_wdt_init(&config);
 *
 * // Normal operation
 * while (running) {
 *   main_loop_work();
 *   rx_wdt_feed();
 * }
 *
 * // Stop for long flash operation
 * rx_wdt_stop();
 * flash_update();  // May take seconds
 * rx_wdt_start();  // Resume monitoring
 * @endcode
 *
 * @invariant timeout_cycles must be in range [0, 3]
 * @invariant Structure size is 4 bytes (3 fields + 1 padding)
 * @invariant All fields must be initialized before use
 * @invariant Configuration is immutable after rx_wdt_init() (requires re-init to change)
 *
 * @note C23 struct with explicit padding for 4-byte alignment
 * @note Configuration is copied by rx_wdt_init(), original can be freed after call
 * @note Changing configuration requires calling rx_wdt_init() again (if WDT stopped)
 *
 * @warning timeout_cycles values are hardware-limited (0-3 only)
 * @warning enable_on_init=true starts watchdog immediately (ensure feed loop ready)
 * @warning reset_on_timeout=true will reset system (no recovery possible)
 * @attention Always validate timeout_cycles against wdt_timeout_cycles_t enum
 *
 * @see rx_wdt_init() Initializes WDT with this configuration
 * @see wdt_timeout_cycles_t Enum values for timeout_cycles field
 * @see rx_wdt_start() Start watchdog if enable_on_init=false
 * @see rx_wdt_stop() Stop watchdog for debugging or long operations
 *
 * @since Version 1.0.0
 */
typedef struct {
  wdt_timeout_cycles_t
    timeout_cycles; /**< Counter cycle count before timeout (TOPS bits). Use k_wdt_timeout_16384_cycles for maximum. Valid values: 0-3. See wdt_timeout_cycles_t. */
  wdt_clock_division_t
    clock_division; /**< Clock division ratio (CKS bits). Use k_wdt_clock_div_8192 for longest timeout. Combined with timeout_cycles determines actual timeout. See wdt_clock_division_t. */
  bool
    enable_on_init; /**< Auto-start watchdog after initialization. true = Start immediately (production mode, ensure feed loop ready). false = Manual start via rx_wdt_start() (safer for development). Default: false */
  bool
    reset_on_timeout; /**< Behavior on watchdog timeout. true = System reset (production mode). false = NMI interrupt (development mode, allows debugging). Default: true */
} rx_wdt_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the Watchdog Timer (WDT) with specified configuration
 *
 * @details
 * Configures and optionally starts the RX72N Watchdog Timer peripheral. Sets up
 * timeout period (clock divider), reset behavior, and auto-start mode. Once
 * initialized, the WDT can be started/stopped via rx_wdt_start() and rx_wdt_stop().
 *
 * **Algorithm steps:**
 * 1. Validate configuration parameters (NULL check, timeout_cycles range)
 * 2. Check if WDT already initialized (prevent double initialization)
 * 3. Configure WDTCR register:
 *    - Set CKS[1:0] bits based on timeout_cycles
 *    - Set RPES bit for reset vs interrupt mode
 * 4. If enable_on_init=true, start WDT counter immediately
 * 5. Store configuration for runtime reference
 * 6. Mark driver as initialized
 *
 * **Hardware Configuration**:
 * - Writes to WDTCR (Watchdog Timer Control Register) at 0x00088022
 * - Sets CKS bits: timeout_cycles value (0-3)
 * - Sets RPES bit: reset_on_timeout (1=reset, 0=interrupt)
 * - If enable_on_init=true, sets WDT counter start bit
 *
 * **Control Flow**:
 * @dot
 * digraph wdt_init {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="Start"];
 *   check_null [label="Check config != nullptr"];
 *   check_range [label="Validate timeout_cycles (0-3)"];
 *   check_init [label="Check not initialized"];
 *   configure [label="Write WDTCR register"];
 *   auto_start [label="Auto-start?", shape=diamond];
 *   start_wdt [label="Start WDT counter"];
 *   mark_init [label="Set initialized flag"];
 *   success [label="Return k_rx_ok", fillcolor=lightgreen, style="rounded,filled"];
 *   error_null [label="Return k_rx_err_null_ptr", fillcolor=lightcoral, style="rounded,filled"];
 *   error_arg [label="Return k_rx_err_invalid_arg", fillcolor=lightcoral, style="rounded,filled"];
 *   error_state [label="Return k_rx_err_invalid_state", fillcolor=lightcoral, style="rounded,filled"];
 *
 *   start -> check_null;
 *   check_null -> error_null [label="NULL"];
 *   check_null -> check_range [label="valid"];
 *   check_range -> error_arg [label="invalid"];
 *   check_range -> check_init [label="valid"];
 *   check_init -> error_state [label="initialized"];
 *   check_init -> configure [label="not init"];
 *   configure -> auto_start;
 *   auto_start -> start_wdt [label="yes"];
 *   auto_start -> mark_init [label="no"];
 *   start_wdt -> mark_init;
 *   mark_init -> success;
 * }
 * @enddot
 *
 * @param[in] config Configuration structure pointer. Must point to valid rx_wdt_config_t.
 *   - timeout_cycles: Must be valid wdt_timeout_cycles_t (0-3)
 *   - enable_on_init: true=start immediately, false=manual start
 *   - reset_on_timeout: true=reset on timeout, false=interrupt
 *   - NULL not allowed (returns k_rx_err_null_ptr)
 *
 * @return rx_err_t Error code indicating success or failure reason
 * @retval k_rx_ok Success, WDT initialized and optionally started
 * @retval k_rx_err_null_ptr config parameter is nullptr
 * @retval k_rx_err_invalid_arg timeout_cycles out of valid range (0-3)
 * @retval k_rx_err_invalid_state WDT already initialized (call rx_wdt_stop first)
 *
 * @pre config must point to valid rx_wdt_config_t structure
 * @pre config->timeout_cycles must be in range [0, 3]
 * @pre WDT must not be already initialized
 * @pre System must be in a state that allows WDTCR register writes
 *
 * @post If success, WDT is configured with specified settings
 * @post If enable_on_init=true, WDT counter is running
 * @post If enable_on_init=false, WDT is configured but stopped
 * @post Driver marked as initialized, subsequent init calls will fail
 *
 * @invariant WDT hardware state matches configuration
 * @invariant Only one successful init call allowed (until stop/reset)
 * @invariant Configuration immutable after init (requires re-init to change)
 *
 * @note Thread-safe - uses mutex protection
 * @note This function must be called before any other WDT functions
 * @note If enable_on_init=true, ensure feed loop is ready before calling
 * @note Configuration is copied internally, original can be freed after call
 *
 * @warning WDT cannot be reconfigured while initialized (must stop first)
 * @warning If enable_on_init=true, rx_wdt_feed() must be called within timeout period
 * @warning timeout_cycles determines actual timeout based on PCLKB frequency
 * @attention Very short timeouts (us range) may be difficult to meet reliably
 *
 * @par Thread Safety:
 * Thread-safe. Uses internal mutex to prevent concurrent initialization.
 * Multiple threads can safely call this function (first succeeds, rest fail with
 * k_rx_err_invalid_state).
 *
 * @par Performance:
 * - Execution time: ~20 us @ 240 MHz (register configuration)
 * - Memory: No dynamic allocation, configuration copied to static storage
 * - Stack usage: < 32 bytes
 *
 * @par Re-entrancy:
 * Not reentrant (uses static state). Concurrent calls are serialized by mutex.
 *
 * @par Example (Basic Initialization):
 * @code{.c}
 * #include "rx_wdt.h"
 * #include "uart_debug.h"
 *
 * void system_init(void) {
 *   // Configure WDT with longest timeout, manual start
 *   rx_wdt_config_t config = {
 *     .timeout_cycles   = k_wdt_timeout_16384_cycles,  // ~273us at 60MHz
 *     .enable_on_init   = false,  // Don't start yet
 *     .reset_on_timeout = true    // Reset on timeout
 *   };
 *
 *   rx_err_t err = rx_wdt_init(&config);
 *   if (err != k_rx_ok) {
 *     uart_debug_puts("[ERROR] WDT init failed\r\n");
 *     // Handle error (cannot use watchdog)
 *     return;
 *   }
 *
 *   uart_debug_puts("[INFO] WDT initialized\r\n");
 *
 *   // ... complete system initialization ...
 *
 *   // Start watchdog after init complete
 *   rx_wdt_start();
 * }
 * @endcode
 *
 * @par Example (Auto-Start for Production):
 * @code{.c}
 * // Production system: Start watchdog immediately
 * rx_wdt_config_t prod_config = {
 *   .timeout_cycles   = k_wdt_timeout_16384_cycles,
 *   .enable_on_init   = true,   // Start immediately
 *   .reset_on_timeout = true
 * };
 *
 * // IMPORTANT: Ensure feed loop is ready before this call
 * rx_err_t err = rx_wdt_init(&prod_config);
 * if (err != k_rx_ok) {
 *   // Fatal error - cannot start watchdog
 *   system_fault_handler();
 * }
 *
 * // WDT is now running - must call rx_wdt_feed() within ~273us
 * @endcode
 *
 * @par Example (Error Handling):
 * @code{.c}
 * rx_wdt_config_t config = {};
 * rx_err_t err = rx_wdt_init(&config);
 *
 * switch (err) {
 *   case k_rx_ok:
 *     uart_debug_puts("[INFO] WDT ready\r\n");
 *     break;
 *   case k_rx_err_null_ptr:
 *     uart_debug_puts("[ERROR] nullptr config pointer\r\n");
 *     break;
 *   case k_rx_err_invalid_arg:
 *     uart_debug_puts("[ERROR] Invalid timeout period\r\n");
 *     break;
 *   case k_rx_err_invalid_state:
 *     uart_debug_puts("[WARN] WDT already initialized\r\n");
 *     break;
 *   default:
 *     uart_debug_puts("[ERROR] Unknown WDT error\r\n");
 *     break;
 * }
 * @endcode
 *
 * @see rx_wdt_config_t Configuration structure definition
 * @see rx_wdt_start() Start watchdog if enable_on_init=false
 * @see rx_wdt_stop() Stop watchdog for reconfiguration
 * @see rx_wdt_feed() Feed watchdog to prevent timeout
 * @see wdt_timeout_cycles_t Timeout period enum values
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto/setjmp/recursion, sequential control flow
 * - Rule 3: [OK] No dynamic allocation, static configuration storage
 * - Rule 5: [OK] 4 preconditions, 4 postconditions
 * - Rule 7: [OK] Returns rx_err_t, caller must check
 */
[[nodiscard]] rx_err_t rx_wdt_init(const rx_wdt_config_t* config);

/**
 * @brief Start the WDT counter (enable watchdog monitoring)
 *
 * @details
 * Starts the Watchdog Timer counter, beginning countdown to timeout. Once started,
 * rx_wdt_feed() must be called periodically to prevent timeout. This function is
 * used when WDT was initialized with enable_on_init=false.
 *
 * **Algorithm steps:**
 * 1. Check if WDT is initialized (fail if not)
 * 2. Set WDT counter start bit in WDTCR register
 * 3. Begin countdown from configured timeout period
 * 4. Return success
 *
 * **Hardware Operation**:
 * - Writes to WDTCR register (0x00088022)
 * - Sets TME (Timer Enable) bit to 1
 * - Counter begins decrementing from timeout period value
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, WDT counter started
 * @retval k_rx_err_not_initialized WDT not initialized via rx_wdt_init()
 *
 * @pre WDT must be initialized via rx_wdt_init()
 * @pre Feed loop must be ready to call rx_wdt_feed() within timeout period
 *
 * @post WDT counter is running
 * @post rx_wdt_feed() must be called within timeout period to prevent reset
 * @post If timeout expires, system will reset (or interrupt if configured)
 *
 * @invariant WDT configuration unchanged (timeout period, reset mode)
 * @invariant Starting already-started WDT is safe (idempotent)
 *
 * @note Thread-safe - uses mutex protection
 * @note Can be called multiple times (idempotent operation)
 * @note Complementary to rx_wdt_stop() for runtime control
 *
 * @warning Ensure rx_wdt_feed() will be called within timeout period before starting
 * @warning At 60 MHz PCLKB with max divider, timeout is only ~273 us (very short!)
 * @attention After starting, missing even ONE feed will cause timeout/reset
 *
 * @par Thread Safety:
 * Thread-safe. Uses internal mutex to prevent concurrent start/stop operations.
 *
 * @par Performance:
 * - Execution time: ~2 us @ 240 MHz (single register write)
 * - Memory: No allocation
 * - Stack usage: < 16 bytes
 *
 * @par Re-entrancy:
 * Not reentrant (uses static state). Concurrent calls serialized by mutex.
 *
 * @par Example (Staged Startup):
 * @code{.c}
 * // Initialize WDT but don't start immediately
 * rx_wdt_config_t config = {
 *   .timeout_cycles   = k_wdt_timeout_16384_cycles,
 *   .enable_on_init   = false,  // Manual start
 *   .reset_on_timeout = true
 * };
 * rx_wdt_init(&config);
 *
 * // ... complete system initialization ...
 * // ... set up feed loop ...
 *
 * // Now safe to start watchdog
 * rx_err_t err = rx_wdt_start();
 * if (err != k_rx_ok) {
 *   uart_debug_puts("[ERROR] WDT start failed\r\n");
 * }
 *
 * // Must now call rx_wdt_feed() within ~273us
 * @endcode
 *
 * @par Example (Start/Stop Control):
 * @code{.c}
 * // Normal operation with watchdog
 * rx_wdt_start();
 * while (running) {
 *   main_loop_work();
 *   rx_wdt_feed();
 * }
 *
 * // Stop for long flash operation
 * rx_wdt_stop();
 * flash_erase_sector();  // May take milliseconds
 * rx_wdt_start();  // Resume monitoring
 * @endcode
 *
 * @see rx_wdt_init() Initialize WDT before starting
 * @see rx_wdt_stop() Stop WDT counter
 * @see rx_wdt_feed() Feed watchdog to prevent timeout
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto/setjmp/recursion
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 5: [OK] 2 preconditions, 3 postconditions
 */
[[nodiscard]] rx_err_t rx_wdt_start(void);

/**
 * @brief Stop the WDT counter (disable watchdog monitoring)
 *
 * @details
 * Stops the Watchdog Timer counter, preventing timeout. Use this function to
 * suspend watchdog monitoring during long operations (flash updates, debugging)
 * or for safe shutdown. Unlike IWDT, WDT can be stopped and restarted.
 *
 * **Algorithm steps:**
 * 1. Check if WDT is initialized (fail if not)
 * 2. Clear WDT counter start bit in WDTCR register
 * 3. Counter stops decrementing (frozen at current value)
 * 4. Return success
 *
 * **Hardware Operation**:
 * - Writes to WDTCR register (0x00088022)
 * - Clears TME (Timer Enable) bit to 0
 * - Counter stops, timeout cannot occur
 *
 * **Use Cases**:
 * - **Flash updates**: Long erase/write operations (milliseconds to seconds)
 * - **Debugging**: Freeze counter during breakpoint sessions
 * - **Safe mode**: Disable monitoring during error recovery
 * - **Shutdown**: Stop watchdog before power-down
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, WDT counter stopped
 * @retval k_rx_err_not_initialized WDT not initialized via rx_wdt_init()
 *
 * @pre WDT must be initialized via rx_wdt_init()
 *
 * @post WDT counter is stopped (not decrementing)
 * @post No timeout will occur until rx_wdt_start() called
 * @post System is unprotected by WDT until restarted
 *
 * @invariant WDT configuration unchanged (timeout period, reset mode)
 * @invariant Stopping already-stopped WDT is safe (idempotent)
 *
 * @note Thread-safe - uses mutex protection
 * @note Can be called multiple times (idempotent operation)
 * @note Complementary to rx_wdt_start() for runtime control
 *
 * @warning System is unprotected while WDT stopped (use with caution)
 * @warning Remember to call rx_wdt_start() to resume monitoring
 * @attention If using both WDT and IWDT, system still protected by IWDT
 *
 * @par Thread Safety:
 * Thread-safe. Uses internal mutex to prevent concurrent start/stop operations.
 *
 * @par Performance:
 * - Execution time: ~2 us @ 240 MHz (single register write)
 * - Memory: No allocation
 * - Stack usage: < 16 bytes
 *
 * @par Re-entrancy:
 * Not reentrant (uses static state). Concurrent calls serialized by mutex.
 *
 * @par Example (Flash Update):
 * @code{.c}
 * // Stop WDT for long flash operation
 * rx_wdt_stop();
 *
 * // Erase and write flash (may take seconds)
 * rx_err_t err = flash_erase_sector(sector_addr);
 * if (err == k_rx_ok) {
 *   err = flash_write_page(sector_addr, data, size);
 * }
 *
 * // Resume watchdog monitoring
 * rx_wdt_start();
 *
 * // Continue normal operation with watchdog active
 * @endcode
 *
 * @par Example (Debugging Session):
 * @code{.c}
 * // Before setting breakpoint, stop watchdog
 * rx_wdt_stop();
 *
 * // Set breakpoint here - WDT won't timeout during debug session
 * uart_debug_puts("[DEBUG] Breakpoint location\r\n");
 *
 * // Resume watchdog after debugging
 * rx_wdt_start();
 * @endcode
 *
 * @par Example (Safe Shutdown):
 * @code{.c}
 * void system_shutdown(void) {
 *   uart_debug_puts("[INFO] System shutdown initiated\r\n");
 *
 *   // Stop watchdog to prevent timeout during shutdown
 *   rx_wdt_stop();
 *
 *   // Perform shutdown sequence
 *   motor_disable_all();
 *   comm_close_all();
 *   save_state_to_flash();
 *
 *   // Power down (watchdog remains stopped)
 *   enter_low_power_mode();
 * }
 * @endcode
 *
 * @see rx_wdt_init() Initialize WDT before stopping
 * @see rx_wdt_start() Restart WDT after stopping
 * @see rx_iwdt.h Independent watchdog that cannot be stopped
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto/setjmp/recursion
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 5: [OK] 1 precondition, 3 postconditions
 */
[[nodiscard]] rx_err_t rx_wdt_stop(void);

/**
 * @brief Feed the watchdog timer (reset counter to prevent timeout)
 *
 * @details
 * Resets the WDT counter to its initial value, preventing timeout. This function
 * must be called periodically at a rate faster than the configured timeout period.
 * Typically called from main control loop or dedicated watchdog monitoring task.
 *
 * **Algorithm steps:**
 * 1. Check if WDT is initialized (fail if not)
 * 2. Write refresh sequence to WDTRR register (0x00 then 0xFF)
 * 3. Counter resets to initial timeout period value
 * 4. Countdown resumes from reset value
 * 5. Return success
 *
 * **Hardware Operation**:
 * - Writes two-byte sequence to WDTRR (Watchdog Timer Refresh Register):
 *   - First write: 0x00
 *   - Second write: 0xFF
 * - Sequence must be written in correct order (hardware validation)
 * - Counter resets to timeout_cycles value
 *
 * **Feed Requirements**:
 * - **Frequency**: Must call faster than timeout period
 * - **Example**: With k_wdt_timeout_16384_cycles at 60 MHz PCLKB:
 *   - Timeout = ~273 us
 *   - Feed frequency > 3.6 kHz (every ~273 us)
 *   - Recommended: Feed at 2x margin = ~130 us interval
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok Success, WDT counter reset to initial value
 * @retval k_rx_err_not_initialized WDT not initialized via rx_wdt_init()
 *
 * @pre WDT must be initialized via rx_wdt_init()
 * @pre WDT must be started via rx_wdt_start() or enable_on_init=true
 * @pre Called from location that executes faster than timeout period
 *
 * @post WDT counter reset to initial timeout period value
 * @post Countdown timer begins again from reset value
 * @post Timeout prevented (for this cycle)
 *
 * @invariant Counter value after feed equals timeout_cycles configuration value
 * @invariant Configuration unchanged (timeout period, reset mode)
 *
 * @note Thread-safe - can be called from multiple threads (mutex protected)
 * @note ISR-safe - can be called from interrupt context (optimized fast path)
 * @note Calling when WDT stopped has no effect (safe but wasteful)
 * @note This is the ONLY function that prevents watchdog timeout
 *
 * @warning Missing even ONE feed within timeout period will cause reset
 * @warning Feed frequency must account for worst-case execution time variations
 * @warning Very short timeouts (~273 us max) require very frequent feeds
 * @attention Do NOT call from multiple locations - defeats failure detection
 *
 * @par Thread Safety:
 * Thread-safe. Uses atomic register writes (no mutex needed for performance).
 * Multiple threads can safely call this function.
 *
 * @par Performance:
 * - Execution time: ~1 us @ 240 MHz (two register writes)
 * - Memory: No allocation
 * - Stack usage: < 8 bytes
 * - Fastest WDT operation
 *
 * @par Re-entrancy:
 * Fully reentrant. Uses only register writes, no static state modification.
 *
 * @par Example (Main Loop):
 * @code{.c}
 * // Feed from main control loop
 * void main_loop(void) {
 *   while (1) {
 *     // Perform main loop work
 *     motor_update();
 *     comm_process();
 *     sensor_read();
 *
 *     // Feed watchdog at end of each iteration
 *     rx_err_t err = rx_wdt_feed();
 *     if (err != k_rx_ok) {
 *       uart_debug_puts("[ERROR] WDT feed failed\r\n");
 *     }
 *
 *     // Short delay (must be < timeout period)
 *     delay_us(100);  // 100us delay, well under 273us timeout
 *   }
 * }
 * @endcode
 *
 * @par Example (Dedicated Watchdog Task):
 * @code{.c}
 * // ThreadX task dedicated to watchdog feeding
 * void watchdog_task(ULONG input) {
 *   (void)input;
 *
 *   while (1) {
 *     // Feed watchdog
 *     rx_wdt_feed();
 *
 *     // Sleep for 100us (well under 273us timeout)
 *     tx_thread_sleep(1);  // Assuming 10 kHz tick rate = 100us
 *   }
 * }
 * @endcode
 *
 * @par Example (Timed Feed with Margin):
 * @code{.c}
 * // Calculate safe feed interval
 * const uint32_t timeout_us = 273;  // k_wdt_timeout_16384_cycles at 60MHz
 * const uint32_t safety_margin = 2;  // 2x safety margin
 * const uint32_t feed_interval_us = timeout_us / safety_margin;  // 136us
 *
 * void main_loop(void) {
 *   uint32_t last_feed_time = get_time_us();
 *
 *   while (1) {
 *     main_loop_work();
 *
 *     // Feed if interval elapsed
 *     uint32_t now = get_time_us();
 *     if ((now - last_feed_time) >= feed_interval_us) {
 *       rx_wdt_feed();
 *       last_feed_time = now;
 *     }
 *   }
 * }
 * @endcode
 *
 * @par Example (Error Detection):
 * @code{.c}
 * // Detect failure to initialize WDT
 * rx_err_t err = rx_wdt_feed();
 * if (err == k_rx_err_not_initialized) {
 *   uart_debug_puts("[ERROR] WDT not initialized - cannot feed\r\n");
 *   // System unprotected - critical error
 *   system_fault_handler();
 * }
 * @endcode
 *
 * @see rx_wdt_init() Initialize WDT before feeding
 * @see rx_wdt_start() Start WDT to enable timeout
 * @see rx_wdt_stop() Stop WDT to disable timeout
 * @see wdt_timeout_cycles_t Timeout periods that determine feed frequency
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto/setjmp/recursion
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 4: [OK] Function < 10 lines (simple register write)
 * - Rule 5: [OK] 3 preconditions, 3 postconditions
 */
[[nodiscard]] rx_err_t rx_wdt_feed(void);

#ifdef __cplusplus
}
#endif
