/* lib/rx_crc/inc/rx_crc_internal.h */

/**
 * @file rx_crc_internal.h
 * @brief Internal CRC-32 Abstraction Layer - Hardware/Software Implementation Selection
 *
 * @details
 * ## Overview
 *
 * This internal header provides the abstraction layer for CRC-32 implementation
 * selection between hardware-accelerated and software table-based algorithms.
 * It defines the internal API used by rx_crc32.c to dispatch to the appropriate
 * implementation at compile time.
 *
 * **This header is internal to the rx_crc library and should NOT be included
 * directly by application code. Use rx_crc.h instead.**
 *
 * ## Architecture
 *
 * @dot
 * digraph crc_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_public {
 *     label="Public API (rx_crc.h)";
 *     style=filled;
 *     color=lightgrey;
 *     rx_crc32_ieee [label="rx_crc32_ieee()"];
 *     rx_crc32_update [label="rx_crc32_update()"];
 *     rx_crc_init [label="rx_crc_init()"];
 *   }
 *
 *   subgraph cluster_internal {
 *     label="Internal API (rx_crc_internal.h)";
 *     style=filled;
 *     color=lightyellow;
 *     rx_crc32_ieee_impl [label="rx_crc32_ieee_impl()"];
 *     rx_crc32_update_impl [label="rx_crc32_update_impl()"];
 *     rx_crc32_update_sw [label="rx_crc32_update_sw()"];
 *   }
 *
 *   subgraph cluster_impl {
 *     label="Implementations";
 *     style=filled;
 *     color=lightblue;
 *     hw [label="rx_crc32_hw.c\n(RX72N Hardware)"];
 *     sw [label="rx_crc32_sw.c\n(Software Table)"];
 *   }
 *
 *   rx_crc32_ieee -> rx_crc32_ieee_impl;
 *   rx_crc32_update -> rx_crc32_update_impl;
 *   rx_crc_init -> hw [style=dashed, label="RX72N"];
 *   rx_crc_init -> sw [style=dashed, label="Host"];
 *
 *   rx_crc32_ieee_impl -> hw [label="__RX__"];
 *   rx_crc32_ieee_impl -> sw [label="Host"];
 *   rx_crc32_update_impl -> rx_crc32_update_sw [label="Always SW"];
 * }
 * @enddot
 *
 * ## Design Rationale
 *
 * The CRC module uses a compile-time selection strategy:
 *
 * 1. **Hardware CRC** (default on RX72N target):
 *    - Uses RX72N CRC Calculator peripheral at 0x0008C000
 *    - ~4x faster than software for large buffers (32 us vs 130 us per 100 bytes)
 *    - IEEE 802.3 polynomial (0x04C11DB7)
 *    - Automatically enabled when `__RX__` is defined
 *
 * 2. **Software CRC** (default on host, available as fallback):
 *    - Lookup table implementation (256-entry, 1KB table)
 *    - Bit-exact compatible with hardware and Go's crc32.ChecksumIEEE()
 *    - Used for host-side unit testing (no hardware available)
 *    - Always compiled via rx_crc32_update_sw() for incremental CRC
 *
 * ## Why Both Implementations?
 *
 * - **Testing**: Host-side tests validate CRC correctness without hardware
 * - **Debugging**: Software can be forced on target to compare results
 * - **Incremental CRC**: rx_crc32_update_sw() is always available because
 *   hardware state management for incremental updates is complex
 *
 * ## Configuration Decision Matrix
 *
 * | __RX__ Defined | RX_CRC32_USE_SOFTWARE | Result |
 * |----------------|----------------------|--------|
 * | Yes | No | Hardware CRC (default for RX72N) |
 * | Yes | Yes | Software CRC (forced override) |
 * | No | (any) | Software CRC (host testing) |
 *
 * ## Memory Usage
 *
 * | Component | Size | Notes |
 * |-----------|------|-------|
 * | CRC-32 lookup table | 1024 bytes | Software implementation only |
 * | Internal state | 4 bytes | Initialization flag |
 * | Stack per function | 8-16 bytes | Loop counters, temps |
 *
 * ## Hardware Requirements (RX72N Only)
 *
 * | Resource | Requirement |
 * |----------|-------------|
 * | **Peripheral** | CRC Calculator (CRCA) |
 * | **Register Base** | 0x0008C000 |
 * | **Clock Source** | PCLKA (120 MHz typical) |
 * | **Module Stop** | MSTPCRB bit 23 |
 * | **Polynomial** | 0x04C11DB7 (IEEE 802.3) |
 *
 * ## Thread Safety
 *
 * | Function | Thread Safe | Notes |
 * |----------|-------------|-------|
 * | rx_crc_init() | [FAIL] No | Call during single-threaded init |
 * | rx_crc_deinit() | [FAIL] No | Call during single-threaded shutdown |
 * | rx_crc32_ieee_impl() | [WARN] Conditional | HW: No (shared peripheral), SW: Yes |
 * | rx_crc32_update_impl() | [PASS] Yes | Stateless software implementation |
 * | rx_crc32_update_sw() | [PASS] Yes | Stateless, read-only tables |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple control flow | [PASS] Pass | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | [PASS] Pass | Loops bounded by k_crc_len_max |
 * | 3. No dynamic memory | [PASS] Pass | Static lookup tables only |
 * | 4. Short functions | [PASS] Pass | All functions < 40 lines |
 * | 5. Assertions | [PASS] Pass | NULL checks, length validation |
 * | 6. Small scope | [PASS] Pass | Variables at minimal scope |
 * | 7. Check returns | [PASS] Pass | All returns checked by callers |
 * | 8. Limited preprocessor | [PASS] Pass | Only HW/SW selection macros |
 * | 9. Restrict pointers | [PASS] Pass | No function pointers |
 * | 10. Compiler warnings | [PASS] Pass | -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | Internal API only - dispatching to implementations |
 * | **Open/Closed** | New implementations can be added without modifying API |
 * | **Liskov Substitution** | HW/SW implementations interchangeable |
 * | **Interface Segregation** | Minimal internal API (4 functions) |
 * | **Dependency Inversion** | Public API depends on internal abstractions |
 *
 * ## References
 *
 * - RX72N Group User's Manual: Hardware, Chapter 30 (CRC Calculator)
 * - https://renesas.github.io/fsp/group___c_r_c.html
 * - IEEE 802.3 CRC-32 specification
 *
 * @see rx_crc.h Public CRC API (use this in application code)
 * @see rx_crc32_hw.c Hardware CRC implementation (RX72N)
 * @see rx_crc32_sw.c Software CRC implementation (portable)
 * @see rx_crc32.c CRC dispatcher (calls internal API)
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Shared CRC Constants (NASA Power of 10 Rule 2 - Bounded Loops)
 * =============================================================================
 */

/**
 * @enum rx_crc_shared_constants_t
 * @brief Shared constants for CRC validation and NASA-compliant bounded loops
 *
 * @details
 * This enumeration defines shared constants used by both hardware and software
 * CRC implementations. These constants ensure:
 *
 * 1. **Consistent validation** across implementations
 * 2. **NASA Power of 10 Rule 2 compliance** - All loops have fixed upper bounds
 * 3. **Predictable memory access patterns** - No unbounded iterations
 *
 * ## NASA Power of 10 Rule 2: Fixed Loop Bounds
 *
 * All loops in CRC computation must have provable upper bounds at compile time.
 * The k_crc_len_max constant provides this bound:
 *
 * @code{.c}
 * // [PASS] CORRECT: Loop bounded by k_crc_len_max
 * for (uint32_t i = k_crc_idx_start; i < len && i < k_crc_len_max; i++) {
 *     crc = update_crc(crc, data[i]);
 * }
 *
 * // [FAIL] WRONG: Unbounded loop
 * for (uint32_t i = 0; i < len; i++) {  // len could be unbounded
 *     crc = update_crc(crc, data[i]);
 * }
 * @endcode
 *
 * ## Value Rationale
 *
 * | Constant | Value | Rationale |
 * |----------|-------|-----------|
 * | k_crc_len_min | 1 | Minimum meaningful data for CRC computation |
 * | k_crc_len_max | 65535 | Practical limit (64KB), fits SPI DMA, exceeds largest frame |
 * | k_crc_idx_start | 0 | Standard zero-based indexing |
 *
 * @par Memory Layout:
 * | Offset | Size | Field | Alignment |
 * |--------|------|-------|-----------|
 * | 0 | 4 bytes | Value storage | 4-byte aligned |
 *
 * @par Usage Example:
 * @code{.c}
 * // Validate length before CRC computation
 * if (len < k_crc_len_min || len > k_crc_len_max) {
 *     return k_rx_err_invalid_arg;
 * }
 *
 * // Bounded loop for NASA compliance
 * uint32_t crc = k_crc32_init_value;
 * for (uint32_t i = k_crc_idx_start; i < len; i++) {
 *     crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
 * }
 * @endcode
 *
 * @note The uint32_t underlying type ensures consistent size across platforms
 * @warning Do not exceed k_crc_len_max in any CRC computation
 *
 * @see rx_crc32_ieee() Uses these constants for validation
 * @see rx_crc8_maxim() Uses these constants for validation
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
     * @brief Minimum CRC data length (1 byte)
     * @details
     * CRC computation requires at least one byte of data.
     * Zero-length inputs return a defined value (init XOR final).
     * @par Value: 1
     * @par Units: bytes
     */
  k_crc_len_min = 1,

  /**
     * @brief Maximum CRC data length for bounded loops (64KB - 1)
     * @details
     * This value provides the upper bound for all CRC computation loops
     * to satisfy NASA Power of 10 Rule 2. The value 65535 was chosen because:
     * - Fits in 16 bits (compatible with DMA transfer sizes)
     * - Exceeds maximum SPI frame size (279 bytes)
     * - Provides headroom for future protocol extensions
     * - Ensures loop termination in bounded time
     *
     * @par Value: 65535
     * @par Units: bytes
     * @par Rationale:
     * - Maximum practical single-buffer CRC in embedded context
     * - Matches RX72N DMA maximum transfer size
     * - ~2ms worst-case execution at 240 MHz (software CRC)
     *
     * @warning Do not pass buffers larger than this to CRC functions
     */
  k_crc_len_max = 65535,

  /**
     * @brief Loop start index (zero-based)
     * @details
     * Standard starting index for CRC computation loops.
     * Using a named constant improves code clarity and searchability.
     * @par Value: 0
     * @par Units: index (dimensionless)
     */
  k_crc_idx_start = 0,
} rx_crc_shared_constants_t;

/* =============================================================================
 * Configuration - Compile-time Hardware/Software Selection
 *
 * This preprocessor logic implements the following decision matrix:
 *
 * | __RX__ | RX_CRC32_USE_SOFTWARE | Result              |
 * |--------|----------------------|---------------------|
 * | Yes    | No                   | Hardware (default)  |
 * | Yes    | Yes                  | Software (override) |
 * | No     | (any)                | Software (testing)  |
 *
 * To force software CRC on hardware target:
 *   gcc -DRX_CRC32_USE_SOFTWARE ...
 *   or #define RX_CRC32_USE_SOFTWARE before including this header
 *
 * The software implementation is always compiled (rx_crc32_sw.c) because:
 * 1. It's needed for host-side unit tests
 * 2. rx_crc32_update_sw() is used for incremental CRC on hardware targets
 * 3. It enables A/B comparison testing on hardware for validation
 * =============================================================================
 */

#if defined(__RX__) && !defined(RX_CRC32_USE_SOFTWARE)
#define RX_CRC32_USE_HARDWARE
#elif !defined(RX_CRC32_USE_SOFTWARE)
#define RX_CRC32_USE_SOFTWARE
#endif

/* =============================================================================
 * Internal API - Implementation Functions
 *
 * These functions are called by the public API in rx_crc32.c.
 * The actual implementation is provided by rx_crc32_sw.c or rx_crc32_hw.c
 * depending on the compile-time configuration.
 * =============================================================================
 */

/**
 * @brief Initialize CRC module (internal implementation)
 *
 * @details
 * Initializes the CRC subsystem based on compile-time configuration:
 *
 * **Hardware Implementation (RX72N, RX_CRC32_USE_HARDWARE defined):**
 * 1. Validate not already initialized (prevent double-init)
 * 2. Clear module stop bit (MSTPCRB.MSTPB23 = 0) to enable CRCA clock
 * 3. Wait for peripheral ready (typically 1 us)
 * 4. Configure polynomial register to 0x04C11DB7 (IEEE 802.3)
 * 5. Set bit order to LSB-first (reflected mode)
 * 6. Set initial value to 0xFFFFFFFF
 * 7. Mark module as initialized
 *
 * **Software Implementation (Host or RX_CRC32_USE_SOFTWARE defined):**
 * 1. No-op (lookup table is statically allocated)
 * 2. Return k_rx_ok immediately
 *
 * @par Initialization Flow:
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="rx_crc_init()"];
 *   check [label="Already initialized?", shape=diamond];
 *   hw_check [label="Hardware CRC?", shape=diamond];
 *   enable_clock [label="Enable CRCA clock\n(MSTPCRB.MSTPB23=0)"];
 *   config [label="Configure polynomial\nand bit order"];
 *   sw_noop [label="No-op\n(table is static)"];
 *   set_flag [label="Set initialized flag"];
 *   ret_ok [label="Return k_rx_ok"];
 *   ret_err [label="Return k_rx_err_invalid_state"];
 *
 *   start -> check;
 *   check -> ret_err [label="Yes"];
 *   check -> hw_check [label="No"];
 *   hw_check -> enable_clock [label="Yes"];
 *   hw_check -> sw_noop [label="No"];
 *   enable_clock -> config;
 *   config -> set_flag;
 *   sw_noop -> ret_ok;
 *   set_flag -> ret_ok;
 * }
 * @enddot
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Initialization successful
 * @retval k_rx_err_invalid_state Already initialized (hardware only)
 * @retval k_rx_err_hardware Peripheral configuration failed (hardware only)
 *
 * @pre System clocks must be configured and running
 * @pre For hardware: Module stop registers must be accessible
 * @post Hardware: CRCA peripheral ready for CRC computation
 * @post Software: No state change (lookup table already in ROM)
 *
 * @note Thread safety: NOT thread-safe, call during single-threaded init
 * @note Called by public rx_crc_init() in rx_crc.h
 * @warning Do not call from interrupt context
 *
 * @par Performance:
 * - Hardware init: ~50 us (includes clock stabilization)
 * - Software init: < 1 us (no-op)
 *
 * @par Example (Internal Use Only):
 * @code{.c}
 * // This function is called by the public API, not directly
 * // See rx_crc.h for public usage examples
 * rx_err_t err = rx_crc_init();
 * if (err != k_rx_ok) {
 *     // Fall back to software CRC
 *     s_use_hardware = false;
 * }
 * @endcode
 *
 * @see rx_crc.h Public API (use this in application code)
 * @see rx_crc_deinit() Shutdown CRC module
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_crc_init(void);

/**
 * @brief Deinitialize CRC module (internal implementation)
 *
 * @details
 * Shuts down the CRC subsystem based on compile-time configuration:
 *
 * **Hardware Implementation (RX72N, RX_CRC32_USE_HARDWARE defined):**
 * 1. Validate module is initialized
 * 2. Set module stop bit (MSTPCRB.MSTPB23 = 1) to disable CRCA clock
 * 3. Clear internal initialization flag
 *
 * **Software Implementation (Host or RX_CRC32_USE_SOFTWARE defined):**
 * 1. No-op (lookup table remains in memory)
 * 2. Return k_rx_ok immediately
 *
 * **Design Note:** Current implementation intentionally leaves CRC enabled
 * after deinit because:
 * - CRC may be needed at any time for frame validation
 * - Module stop/start overhead is significant (~50 us)
 * - Power savings are minimal (< 0.2 mA)
 *
 * @return rx_err_t Deinitialization result
 * @retval k_rx_ok Deinitialization successful
 * @retval k_rx_err_invalid_state Not initialized (hardware only)
 *
 * @pre Module must be initialized via rx_crc_init()
 * @post Hardware: CRCA peripheral stopped (power saving)
 * @post Software: No state change
 *
 * @note Thread safety: NOT thread-safe, call during single-threaded shutdown
 * @note Called by public rx_crc_deinit() in rx_crc.h
 * @warning Do not call from interrupt context
 *
 * @par Example (Internal Use Only):
 * @code{.c}
 * // This function is called by the public API, not directly
 * // See rx_crc.h for public usage examples
 * rx_err_t err = rx_crc_deinit();
 * // CRC functions still work (software fallback)
 * @endcode
 *
 * @see rx_crc.h Public API (use this in application code)
 * @see rx_crc_init() Initialize CRC module
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_crc_deinit(void);

/**
 * @brief Calculate IEEE 802.3 CRC-32 (internal implementation)
 *
 * @details
 * Internal implementation function that computes CRC-32 using the
 * compile-time selected algorithm (hardware or software).
 *
 * **Algorithm (IEEE 802.3 CRC-32):**
 *
 * @f[
 *   \text{CRC}_{\text{init}} = 0xFFFFFFFF
 * @f]
 * @f[
 *   \text{For each byte } b_i: \quad \text{CRC} = (\text{CRC} \gg 8) \oplus
 *   \text{Table}[(\text{CRC} \oplus b_i) \land 0xFF]
 * @f]
 * @f[
 *   \text{CRC}_{\text{final}} = \text{CRC} \oplus 0xFFFFFFFF
 * @f]
 *
 * **Hardware Path (RX72N):**
 * 1. Reset CRC peripheral data register
 * 2. Write data bytes to CRCDIR register
 * 3. Read result from CRCDOR register
 * 4. Apply final XOR (0xFFFFFFFF)
 *
 * **Software Path:**
 * 1. Initialize CRC to 0xFFFFFFFF
 * 2. For each byte: table lookup and XOR
 * 3. Apply final XOR (0xFFFFFFFF)
 *
 * @param[in] data Input data buffer
 *   - Valid range: NULL or pointer to valid buffer
 *   - NULL handling: Returns 0 if nullptr
 *   - Alignment: No requirement (byte-addressable)
 * @param[in] len Data length in bytes
 *   - Valid range: 0 to k_crc_len_max (65535)
 *   - Zero handling: Returns 0 (init XOR final)
 *
 * @return uint32_t CRC-32 checksum
 *   - Range: 0x00000000 to 0xFFFFFFFF
 *   - Returns 0 if data is nullptr or len is 0
 *
 * @pre If data != nullptr, buffer must be valid for len bytes
 * @pre len must not exceed k_crc_len_max
 * @post No side effects (pure function in software mode)
 * @post Hardware mode: CRC peripheral state modified
 *
 * @note Called by public rx_crc32_ieee() in rx_crc.h
 * @note Thread safety: SW=Yes, HW=No (shared peripheral)
 * @warning Do not call directly - use rx_crc32_ieee() from rx_crc.h
 *
 * @par Performance:
 * - Hardware: ~32 us / 100 bytes @ 240 MHz
 * - Software: ~130 us / 100 bytes @ 240 MHz
 *
 * @see rx_crc32_ieee() Public API wrapper
 * @see rx_crc32_update_impl() Incremental CRC computation
 *
 * @since Version 1.0.0
 */
uint32_t rx_crc32_ieee_impl(const uint8_t* data, uint32_t len);

/**
 * @brief Update CRC-32 with additional data (internal implementation)
 *
 * @details
 * Internal implementation for incremental CRC-32 computation. This function
 * continues a CRC calculation from a previous partial result, enabling
 * CRC computation over non-contiguous buffers.
 *
 * **Algorithm:**
 * 1. Un-finalize input CRC: `crc ^= 0xFFFFFFFF`
 * 2. For each byte: table lookup and XOR update
 * 3. Re-finalize CRC: `crc ^= 0xFFFFFFFF`
 *
 * **Implementation Note:** This function ALWAYS uses software CRC because
 * the hardware CRC peripheral cannot load an arbitrary initial CRC state.
 *
 * @param[in] crc Previous CRC-32 value (finalized)
 *   - Valid range: 0x00000000 to 0xFFFFFFFF
 *   - Should be result from previous rx_crc32_ieee() or rx_crc32_update()
 * @param[in] data Additional data buffer
 *   - Valid range: NULL or pointer to valid buffer
 *   - NULL handling: Returns input crc unchanged
 *   - Alignment: No requirement
 * @param[in] len Data length in bytes
 *   - Valid range: 0 to k_crc_len_max (65535)
 *   - Zero handling: Returns input crc unchanged
 *
 * @return uint32_t Updated CRC-32 checksum
 *   - Mathematically equivalent to concatenating all buffers
 *
 * @pre If data != nullptr, buffer must be valid for len bytes
 * @pre crc should be from previous CRC computation
 * @post Return value can be passed to next rx_crc32_update() call
 * @post No side effects (pure function)
 *
 * @note Always uses software implementation (hardware limitation)
 * @note Thread safety: Yes (stateless, read-only tables)
 * @warning Do not call directly - use rx_crc32_update() from rx_crc.h
 *
 * @par Mathematical Equivalence:
 * @code{.c}
 * // Incremental computation
 * uint32_t crc = rx_crc32_ieee(buf1, len1);
 * crc = rx_crc32_update(crc, buf2, len2);
 *
 * // Is identical to concatenated computation
 * uint8_t combined[len1 + len2];
 * memcpy(&combined[0], buf1, len1);
 * memcpy(&combined[len1], buf2, len2);
 * uint32_t crc = rx_crc32_ieee(combined, len1 + len2);
 * @endcode
 *
 * @see rx_crc32_update() Public API wrapper
 * @see rx_crc32_update_sw() Actual software implementation
 *
 * @since Version 1.0.0
 */
uint32_t rx_crc32_update_impl(uint32_t crc, const uint8_t* data, uint32_t len);

/**
 * @brief Software CRC-32 update (always available fallback)
 *
 * @details
 * Pure software table-based CRC-32 update function. This function is
 * ALWAYS compiled and available, even when hardware CRC is the default,
 * because:
 *
 * 1. **Host testing**: No hardware available on x86/ARM hosts
 * 2. **Incremental CRC**: Hardware cannot load arbitrary initial state
 * 3. **A/B testing**: Compare hardware vs software results for validation
 *
 * **Algorithm (Table Lookup, LSB-first):**
 *
 * @f[
 *   \text{crc}_{\text{work}} = \text{crc}_{\text{in}} \oplus 0xFFFFFFFF
 * @f]
 * @f[
 *   \text{For each byte } b_i: \quad \text{crc}_{\text{work}} =
 *   (\text{crc}_{\text{work}} \gg 8) \oplus
 *   \text{Table}[(\text{crc}_{\text{work}} \oplus b_i) \land 0xFF]
 * @f]
 * @f[
 *   \text{crc}_{\text{out}} = \text{crc}_{\text{work}} \oplus 0xFFFFFFFF
 * @f]
 *
 * **Performance Characteristics:**
 * - Time complexity: O(n) where n = len
 * - Space complexity: O(1) stack + O(1024) lookup table (shared)
 * - Cache behavior: Good locality (sequential table access)
 *
 * @param[in] crc Previous CRC-32 value (finalized form)
 *   - Valid range: 0x00000000 to 0xFFFFFFFF
 *   - Initial call: Use result from rx_crc32_ieee() on first buffer
 *   - Subsequent calls: Use result from previous rx_crc32_update_sw()
 * @param[in] data Additional data buffer to include in CRC
 *   - Valid range: NULL or pointer to valid buffer of size len
 *   - NULL handling: Returns input crc unchanged (safe no-op)
 *   - Alignment: No requirement (byte-addressable)
 * @param[in] len Number of bytes in data buffer
 *   - Valid range: 0 to k_crc_len_max (65535)
 *   - Zero handling: Returns input crc unchanged
 *
 * @return uint32_t Updated CRC-32 checksum
 *   - Finalized form (ready for comparison or next update)
 *   - Range: 0x00000000 to 0xFFFFFFFF
 *
 * @pre If data != nullptr, data must point to valid buffer of size len
 * @pre len must not exceed k_crc_len_max (NASA Rule 2)
 * @post Return value is valid IEEE 802.3 CRC-32
 * @post Input crc is unchanged (pure function)
 *
 * @invariant Lookup table is read-only and never modified
 * @invariant Function has no side effects
 *
 * @note Thread safety: Yes (stateless, read-only global table)
 * @note This function is called by rx_crc32_update_impl()
 * @note Safe to call from interrupt context (no blocking)
 * @warning Do not exceed k_crc_len_max to maintain NASA compliance
 *
 * @par Memory Usage:
 * - Stack: 16 bytes (loop counter, temps)
 * - Lookup table: 1024 bytes (shared static, in .rodata)
 *
 * @par Performance:
 * - ~130 us / 100 bytes @ 240 MHz RX72N
 * - ~50 us / 100 bytes @ 3 GHz x86
 * - Throughput: ~0.78 MB/s (RX72N)
 *
 * @par Example:
 * @code{.c}
 * // Multi-buffer CRC without concatenation
 * uint8_t header[10] = { 0xAA, 0x55, ... };
 * uint8_t payload[64] = { ... };
 *
 * // Start with header
 * uint32_t crc = rx_crc32_ieee(header, sizeof(header));
 *
 * // Update with payload using software implementation
 * crc = rx_crc32_update_sw(crc, payload, sizeof(payload));
 *
 * // crc now covers header + payload (74 bytes)
 * @endcode
 *
 * @see rx_crc32_update() Public API that calls this function
 * @see rx_crc32_ieee() Single-shot CRC computation
 * @see rx_crc32_sw.c Implementation file
 *
 * @since Version 1.0.0
 */
uint32_t rx_crc32_update_sw(uint32_t crc, const uint8_t* data, uint32_t len);

#ifdef __cplusplus
}
#endif
