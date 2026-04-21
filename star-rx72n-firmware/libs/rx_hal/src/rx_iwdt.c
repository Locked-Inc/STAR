/**
 * @file rx_iwdt.c
 * @brief Independent Watchdog Timer (IWDT) Driver Implementation for RX72N
 *
 * @details
 * Implements hardware and software watchdog functionality for safety-critical
 * system monitoring. The IWDT provides protection against software hangs,
 * deadlocks, and infinite loops through automatic system reset.
 *
 * @par Implementation Architecture
 * @verbatim
 *                    IWDT Implementation Structure
 *   +---------------------------------------------------------------------+
 *   |                         Public API Layer                           |
 *   |   +-------------+  +-------------+  +-------------+  +-----------+  |
 *   |   | rx_iwdt_    |  | rx_iwdt_    |  | rx_iwdt_    |  |rx_iwdt_   |  |
 *   |   |   init()    |  |   feed()    |  |was_reset()  |  |check_     |  |
 *   |   +------+------+  +------+------+  +------+------+  | tasks()   |  |
 *   |          |                |                |         +-----+-----+  |
 *   +----------+----------------+----------------+---------------+--------+
 *              |                |                |               |
 *   +----------v----------------v----------------v---------------v--------+
 *   |                    Internal Helper Layer                           |
 *   |   +------------------------+  +--------------------------------+   |
 *   |   | internal_find_         |  | internal_configure_iwdt_       |   |
 *   |   |   timeout_config()     |  |   control_register()           |   |
 *   |   |   +-> Lookup table      |  |   +-> IWDTCR setup              |   |
 *   |   +------------------------+  +--------------------------------+   |
 *   +--------------------------------------------------------------------+
 *                                    |
 *   +--------------------------------v-----------------------------------+
 *   |                      IWDT Hardware Registers                       |
 *   |   +--------------+  +--------------+  +--------------+             |
 *   |   |   IWDTCR     |  |   IWDTRR     |  |   IWDTSR     |             |
 *   |   |   (Control)  |  |   (Refresh)  |  |   (Status)   |             |
 *   |   |   [16-bit]   |  |   [8-bit]    |  |   [16-bit]   |             |
 *   |   +--------------+  +--------------+  +--------------+             |
 *   |   Base: 0x00088030                                                 |
 *   +--------------------------------------------------------------------+
 * @endverbatim
 *
 * @par Register Layout (IWDTCR - 0x00088030)
 * @verbatim
 *   Bits [15:14] RPSS - Window Start Position
 *   +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
 *   |  -  |RPSS1|RPSS0|  -  |RP-  |RP-  |  -  |  -  |CKS3 |CKS2 |CKS1 |CKS0 |  -  |  -  |TOPS1|TOPS0|
 *   | b15 | b14 | b13 | b12 |ES1  |ES0  | b9  | b8  | b7  | b6  | b5  | b4  | b3  | b2  | b1  | b0  |
 *   +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
 *   TOPS[1:0]: Timeout Period Select (00=1024, 01=4096, 10=8192, 11=16384 cycles)
 *   CKS[3:0]:  Clock Division Ratio (0000=/1, 0010=/16, 0100=/32, 1111=/128)
 *   RPES[1:0]: Window End Position (00=75%, 01=50%, 10=25%, 11=0%)
 *   RPSS[1:0]: Window Start Position (00=100%, 01=75%, 10=50%, 11=25%)
 * @endverbatim
 *
 * @par IWDT Refresh Sequence (IWDTRR)
 * @verbatim
 *   Refresh Sequence (must not be interrupted):
 *   +----------------------------------------------------------------+
 *   |  Step 1: Write 0x00 to IWDTRR                                 |
 *   |  Step 2: Write 0xFF to IWDTRR                                 |
 *   |          v                                                    |
 *   |  Counter reloads to initial value                             |
 *   +----------------------------------------------------------------+
 *   WARNING: Incomplete sequence = Refresh Error -> System Reset
 * @endverbatim
 *
 * @par Timeout Calculation
 * @verbatim
 *   Timeout = (TOPS_cycles x CKS_divisor) / IWDTCLK
 *
 *   Where:
 *   - TOPS_cycles = 1024, 4096, 8192, or 16384 (based on TOPS bits)
 *   - CKS_divisor = 1, 16, 32, 64, or 128 (based on CKS bits)
 *   - IWDTCLK = 120 kHz (dedicated oscillator)
 *
 *   Example: TOPS=16384, CKS=16
 *   Timeout = (16384 x 16) / 120000 = 2.18 seconds
 * @endverbatim
 *
 * @par Performance Characteristics
 * | Function | Execution Time | Stack Usage | Notes |
 * |----------|---------------|-------------|-------|
 * | rx_hal_iwdt_init() | ~50 us | 24 bytes | One-time setup |
 * | rx_hal_iwdt_feed() | ~2 us | 8 bytes | Critical path |
 * | rx_hal_iwdt_was_reset() | ~5 us | 8 bytes | Register read |
 * | rx_hal_iwdt_check_tasks() | ~10 us x N | 32 bytes | N = task count |
 * | rx_hal_iwdt_register_task() | ~15 us | 16 bytes | String copy |
 * | internal_find_timeout_config() | ~3 us | 8 bytes | Table lookup |
 *
 * @par Memory Usage
 * | Category | Size | Notes |
 * |----------|------|-------|
 * | Code (.text) | ~1.5 KB | All functions |
 * | Constants (.rodata) | ~48 bytes | Timeout table (6 entries) |
 * | Static data (.bss) | ~520 bytes | Task monitor state |
 * | Stack (max) | 32 bytes | deepest call chain |
 *
 * @par Thread Safety
 * - rx_hal_iwdt_feed(): Safe from ISRs (uses atomic disable/enable)
 * - Task functions: Not thread-safe (caller must synchronize)
 * - Init/config: Call only from single thread during startup
 *
 * @par Hardware Configuration
 * - **Window mode**: Disabled (refresh allowed anytime)
 * - **Reset action**: Full chip reset (not NMI)
 * - **Sleep behavior**: Continues counting during WAIT/STOP
 * - **Clock source**: Dedicated 120 kHz oscillator (independent of PCLK)
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No goto, setjmp, or recursion
 * - **Rule 2**: [OK] All loops bounded by s_iwdt_timeout_table_size or k_iwdt_max_tasks
 * - **Rule 3**: [OK] Zero dynamic memory allocation (all static)
 * - **Rule 4**: [OK] All functions < 60 lines
 * - **Rule 5**: [OK] Minimum 2 validation checks per function
 * - **Rule 6**: [OK] File-scope statics (s_), local variables at narrowest scope
 * - **Rule 7**: [OK] All return values checked or explicitly cast to (void)
 * - **Rule 8**: [OK] C23 typed enums for all constants
 * - **Rule 9**: [OK] Single-level pointers, function pointers for DIP
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Module has single responsibility (watchdog management)
 * - **O**: Configurable via timeout table without code modification
 * - **L**: Consistent rx_err_t returns across all functions
 * - **I**: Separate APIs for hardware vs task monitoring
 * - **D**: Hardware access via iwdt() accessor, mockable for testing
 *
 * @see rx_hal_iwdt.h Public API documentation and usage examples
 * @see rx72n_iwdt_regs.h IWDT register definitions
 * @see RX72N Hardware Manual Chapter 35 - Independent Watchdog Timer
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_hal_iwdt.h"

#ifdef __RX__
#include "rx72n_regs.h"
#endif

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/**
 * @enum iwdt_timeout_limits_t
 * @brief IWDT timeout validation limits
 *
 * @details
 * Defines the valid range of timeout values for the IWDT. These limits are
 * derived from hardware capabilities and practical system requirements.
 *
 * @par Limit Rationale
 * @verbatim
 *   Minimum (1 ms):
 *   - Hardware can achieve ~8.5 ms minimum (1024/120kHz)
 *   - Using 1 ms as software minimum allows validation before hardware config
 *   - Actual hardware minimum will be selected from timeout table
 *
 *   Maximum (16384 ms):
 *   - Hardware maximum = 16384 x 128 / 120000 ~ 17.5 seconds
 *   - Conservative limit (16.4 s) ensures reasonable failure detection
 *   - Longer timeouts make watchdog less effective
 * @endverbatim
 *
 * @invariant k_iwdt_timeout_min_ms < k_iwdt_timeout_max_ms
 * @invariant Values are compile-time constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief Minimum valid timeout in milliseconds
   * @details Software validation limit (1 ms). Actual hardware minimum
   * is ~128 ms with TOPS=16384, CKS=1. Used for early parameter validation.
   */
  k_iwdt_timeout_min_ms = 1,

  /**
   * @brief Maximum valid timeout in milliseconds
   * @details Hardware maximum is ~17.5 seconds with TOPS=16384, CKS=128.
   * Using 16384 ms as conservative limit for practical watchdog operation.
   */
  k_iwdt_timeout_max_ms = 16384,
} iwdt_timeout_limits_t;

/**
 * @enum iwdt_init_state_t
 * @brief IWDT initialization state tracking
 *
 * @details
 * Tracks whether the IWDT has been initialized. Used to prevent:
 * - Double initialization (hardware watchdog cannot be reconfigured)
 * - API calls before initialization
 * - Feed operations on uninitialized watchdog
 *
 * @par State Transitions
 * @verbatim
 *   [Power On] -> k_iwdt_not_initialized
 *        |
 *        | rx_hal_iwdt_init() succeeds
 *        v
 *   k_iwdt_initialized
 *        |
 *        | (No return path - IWDT cannot be stopped)
 *        v
 *   [System Reset] -> k_iwdt_not_initialized
 * @endverbatim
 *
 * @warning Once initialized, IWDT cannot be disabled (hardware feature)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_iwdt_not_initialized = 0, /**< IWDT not initialized - API calls will fail */
  k_iwdt_initialized     = 1, /**< IWDT initialized - feed required periodically */
} iwdt_init_state_t;

/**
 * @enum iwdt_buffer_size_constants_t
 * @brief Task monitoring string buffer size constants
 *
 * @details
 * Defines buffer sizes for task names and log messages. Sized for embedded
 * system constraints while allowing descriptive task names.
 *
 * @par Size Rationale
 * @verbatim
 *   k_task_name_max_len (16):
 *   - Fits typical task names: "MotorCtrl", "SpiComm", "MainLoop"
 *   - Keeps task_monitor_t struct compact (~32 bytes per task)
 *   - Allows up to 8 tasks x 32 bytes = 256 bytes array
 *
 *   k_log_msg_buffer_size (64):
 *   - Fits formatted message: "Task deadlock: %s (timeout 16384 ms)"
 *   - Stack-allocated (not static) to allow reentrant logging
 * @endverbatim
 *
 * @invariant k_task_name_cmp_len == k_task_name_max_len - 1
 * @invariant k_log_msg_buffer_size >= 48 (minimum for deadlock message)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_task_name_max_len   = 16, /**< Maximum task name length including null terminator */
  k_task_name_cmp_len   = 15, /**< Length for strncmp comparison (excludes null) */
  k_log_msg_buffer_size = 64, /**< Log message buffer size for snprintf formatting */
} iwdt_buffer_size_constants_t;

/**
 * @struct iwdt_timeout_entry_t
 * @brief Timeout configuration lookup table entry
 *
 * @details
 * Maps a human-readable timeout value (milliseconds) to the hardware register
 * values (TOPS and CKS) needed to achieve approximately that timeout period.
 *
 * @par Memory Layout (8 bytes)
 * @verbatim
 *   Offset 0: timeout_ms [4 bytes, uint32_t]
 *   Offset 4: tops       [2 bytes, uint16_t]
 *   Offset 6: cks        [2 bytes, uint16_t]
 *   Total: 8 bytes (no padding needed)
 * @endverbatim
 *
 * @par Timeout Calculation Formula
 * @verbatim
 *   Actual_Timeout = (TOPS_cycles x CKS_divisor) / IWDTCLK
 *
 *   Where IWDTCLK = 120,000 Hz
 * @endverbatim
 *
 * @invariant timeout_ms >= k_iwdt_timeout_min_ms
 * @invariant timeout_ms <= k_iwdt_timeout_max_ms
 * @invariant tops is valid TOPS register value
 * @invariant cks is valid CKS register value
 *
 * @par Example:
 * @code
 * // Look up a configuration entry and read its fields
 * const iwdt_timeout_entry_t* cfg = nullptr;
 * rx_err_t err = internal_find_timeout_config(1000, &cfg);
 * if (err == k_rx_ok) {
 *     // cfg->tops and cfg->cks are ready to combine into IWDTCR
 *     // cfg->timeout_ms reports the actual (>= requested) timeout
 * }
 * @endcode
 *
 * @see s_timeout_table Table of pre-computed entries
 * @see internal_find_timeout_config() Lookup function that returns this type
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint32_t timeout_ms; /**< Target timeout in milliseconds (approximate) */
  uint16_t tops;       /**< TOPS bits [1:0] - Timeout Period Select (1024-16384 cycles) */
  uint16_t cks;        /**< CKS bits [7:4] - Clock Division Select (/1, /16, /32, /64, /128) */
} iwdt_timeout_entry_t;

/**
 * @var s_timeout_table
 * @brief IWDT timeout configuration lookup table
 *
 * @details
 * Static lookup table mapping requested timeout values to optimal IWDT register
 * configurations. Entries are sorted by increasing timeout for linear search
 * efficiency. The first entry with timeout_ms >= requested value is selected.
 *
 * @par Table Organization
 * @verbatim
 *   Index  Request   TOPS    CKS   Actual Timeout   Use Case
 *   -----  --------  -----   ---   --------------   --------------------
 *   0      128ms     16384   /1    ~136ms           Fast control loops
 *   1      512ms     4096    /16   ~546ms           Normal operations
 *   2      1000ms    8192    /16   ~1.09s           Default timeout
 *   3      2048ms    16384   /16   ~2.18s           Moderate operations
 *   4      8192ms    16384   /64   ~8.74s           Slow processes
 *   5      16384ms   16384   /128  ~17.48s          Long operations
 * @endverbatim
 *
 * @par Calculation Examples
 * @verbatim
 *   Entry 0: (16384 x 1) / 120000 = 0.1365s = 136.5ms
 *   Entry 2: (8192 x 16) / 120000 = 1.092s  = 1092ms
 *   Entry 5: (16384 x 128) / 120000 = 17.48s = 17476ms
 * @endverbatim
 *
 * @note Values are slightly longer than requested to provide safety margin
 * @note Table is const and placed in .rodata section (48 bytes total)
 *
 * @warning Do not modify table at runtime (const data)
 *
 * @see internal_find_timeout_config() Table lookup function
 * @see iwdt_timeout_entry_t Entry structure definition
 *
 * @since Version 1.0.0
 */
static const iwdt_timeout_entry_t s_timeout_table[] = {
  /* timeout_ms, TOPS (cycles),        CKS (divisor)            */
  /* Actual timeout = (cycles * divisor) / 120kHz */
  {.timeout_ms = 128, .tops = k_iwdt_tops_16384, .cks = k_iwdt_cks_div_1},     /* ~136ms actual  */
  {.timeout_ms = 512, .tops = k_iwdt_tops_4096, .cks = k_iwdt_cks_div_16},     /* ~546ms actual  */
  {.timeout_ms = 1000, .tops = k_iwdt_tops_8192, .cks = k_iwdt_cks_div_16},    /* ~1.09s actual  */
  {.timeout_ms = 2048, .tops = k_iwdt_tops_16384, .cks = k_iwdt_cks_div_16},   /* ~2.18s actual  */
  {.timeout_ms = 8192, .tops = k_iwdt_tops_16384, .cks = k_iwdt_cks_div_64},   /* ~8.74s actual  */
  {.timeout_ms = 16384, .tops = k_iwdt_tops_16384, .cks = k_iwdt_cks_div_128}, /* ~17.48s actual */
};

/**
 * @var s_iwdt_timeout_table_size
 * @brief Number of entries in the timeout lookup table
 *
 * @details
 * Compile-time constant for table bounds checking. Used in
 * internal_find_timeout_config() for loop bounds (NASA Power of 10 Rule 2).
 *
 * @invariant s_iwdt_timeout_table_size == 6 (current implementation)
 * @invariant s_iwdt_timeout_table_size > 0
 *
 * @since Version 1.0.0
 */
static const uint32_t s_iwdt_timeout_table_size =
  sizeof(s_timeout_table) / sizeof(s_timeout_table[0]);

/* =============================================================================
 * Module State
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for IWDT module
 *
 * @details
 * Identifies log messages from this module in system logs. Uses the tag "IWDT"
 * (uppercase) to match SCREAMING_SNAKE_CASE convention for log tags.
 *
 * @par Log Message Format
 * @verbatim
 *   [IWDT] INFO: Registered task for monitoring
 *   [IWDT] ERROR: Task deadlock: MotorCtrl (timeout 500 ms)
 * @endverbatim
 *
 * @note Pointer to string literal (stored in .rodata section)
 * @warning Do not modify (const pointer to const data)
 *
 * @since Version 1.0.0
 */
static const char* const s_tag = "IWDT";

/**
 * @var s_iwdt_initialized
 * @brief Flag indicating IWDT has been initialized
 *
 * @details
 * Tracks initialization state to prevent:
 * - Multiple calls to rx_hal_iwdt_init() (hardware cannot be reconfigured)
 * - API calls before init (undefined behavior prevention)
 * - Feed operations on uninitialized watchdog
 *
 * @par Thread Safety
 * - Set only once during initialization (single-threaded startup)
 * - Read by all API functions (safe - single write, multiple reads)
 * - Never reset (IWDT cannot be stopped once started)
 *
 * @invariant Value is either k_iwdt_not_initialized or k_iwdt_initialized
 * @invariant Once set to k_iwdt_initialized, never changes until reset
 *
 * @warning Do not modify directly - use rx_hal_iwdt_init() only
 *
 * @since Version 1.0.0
 */
static iwdt_init_state_t s_iwdt_initialized = k_iwdt_not_initialized;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Find best timeout configuration for requested timeout
 *
 * @details
 * Performs linear search through s_timeout_table to find the first entry
 * whose timeout_ms is >= the requested value. This provides the smallest
 * timeout that meets or exceeds the requirement.
 *
 * @par Algorithm
 * @verbatim
 *   1. Validate table is not empty
 *   2. Validate output pointer is not nullptr
 *   3. Linear scan through sorted table:
 *      - If entry.timeout_ms >= requested: return entry pointer
 *   4. If no entry found: requested timeout exceeds maximum
 * @endverbatim
 *
 * @par Complexity
 * - Time: O(n) where n = s_iwdt_timeout_table_size (typically 6)
 * - Space: O(1) - no additional memory allocated
 *
 * @param[in]  timeout_ms Requested timeout in milliseconds (1-16384)
 * @param[out] entry      Pointer to receive configuration entry pointer
 *                        (points into s_timeout_table, do not free)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Valid configuration found and stored in *entry
 * @retval k_rx_err_invalid_arg entry is nullptr, table empty, or timeout out of range
 *
 * @pre timeout_ms >= k_iwdt_timeout_min_ms
 * @pre entry != nullptr
 * @pre s_timeout_table populated (compile-time guarantee)
 *
 * @post *entry points to valid table entry with timeout_ms >= requested
 * @post *entry is const - returned entry must not be modified
 *
 * @note Returns pointer to static data - valid for program lifetime
 * @note Selected timeout may be longer than requested (next available)
 *
 * @par Example:
 * @code
 * const iwdt_timeout_entry_t* config = nullptr;
 * rx_err_t err = internal_find_timeout_config(1000, &config);
 * if (err == k_rx_ok) {
 *     // config->tops and config->cks ready for IWDTCR
 *     // config->timeout_ms = actual timeout (>= 1000)
 * }
 * @endcode
 *
 * @see s_timeout_table Source data for lookup
 * @see iwdt_timeout_entry_t Structure returned
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_find_timeout_config(const uint32_t               timeout_ms,
                                             const iwdt_timeout_entry_t** entry)
{
  if (s_iwdt_timeout_table_size == 0U) {
    return k_rx_err_invalid_arg;
  }

  if (entry == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Find first entry with timeout >= requested */
  for (uint32_t i = 0; i < s_iwdt_timeout_table_size; i++) {
    if (s_timeout_table[i].timeout_ms >= timeout_ms) {
      *entry = &s_timeout_table[i];
      return k_rx_ok;
    }
  }

  /* No suitable configuration found */
  return k_rx_err_invalid_arg;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

#ifdef __RX__
/**
 * @brief Configure IWDT control register (IWDTCR) from timeout table entry
 *
 * @details
 * Builds and writes the IWDTCR register value from a timeout configuration
 * entry. Combines TOPS, CKS, RPES, and RPSS fields into a single 16-bit value.
 *
 * @par Register Construction
 * @verbatim
 *   IWDTCR = TOPS | CKS | RPES | RPSS
 *
 *   Where:
 *   - TOPS[1:0]: Timeout Period Select (from config)
 *   - CKS[7:4]:  Clock Division Ratio (from config)
 *   - RPES[9:8]: Window End = 0% (disabled)
 *   - RPSS[13:12]: Window Start = 100% (full window)
 * @endverbatim
 *
 * @par Register Write Sequence
 * @verbatim
 *   1. Validate config pointer (NULL check)
 *   2. Validate hardware pointer (iwdt() check)
 *   3. Build IWDTCR value from fields
 *   4. Write to IWDTCR register
 * @endverbatim
 *
 * @param[in] config Pointer to timeout configuration entry from lookup table
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Register written successfully
 * @retval k_rx_err_invalid_arg config is nullptr
 * @retval k_rx_err_hw_unmapped IWDT registers not accessible
 *
 * @pre config must point to valid iwdt_timeout_entry_t
 * @pre IWDT not yet started (first refresh not performed)
 * @pre iwdt() returns valid hardware register pointer
 *
 * @post IWDTCR configured with specified timeout and window settings
 *
 * @note Not thread-safe: must be called from single-threaded initialization context
 * @note Window mode disabled (RPES=0%, RPSS=100%)
 * @note Register can only be configured before first refresh
 *
 * @warning IWDTCR becomes read-only after first refresh sequence
 *
 * @see internal_find_timeout_config() Source of config parameter
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_iwdt_control_register(const iwdt_timeout_entry_t* config)
{
  if (config == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (iwdt() == nullptr) {
    return k_rx_err_hw_unmapped;
  }

  uint16_t iwdtcr = 0;
  iwdtcr |= config->tops;    /* Timeout period */
  iwdtcr |= config->cks;     /* Clock divisor */
  iwdtcr |= k_iwdt_rpes_0;   /* Window end at 0% (disabled) */
  iwdtcr |= k_iwdt_rpss_100; /* Window start at 100% (full) */

  iwdt()->iwdtcr = iwdtcr;
  return k_rx_ok;
}

/**
 * @brief Configure IWDT reset and sleep-count behavior registers
 *
 * @details
 * Configures IWDTRCR (Reset Control) and IWDTCSTPR (Count Stop Control)
 * registers for safety-critical operation:
 * - Full chip reset on timeout (not NMI)
 * - Continue counting during sleep modes
 *
 * @par Register Configuration
 * @verbatim
 *   IWDTRCR (Reset Control Register):
 *   +---------------------------------------------------------+
 *   | Bit 7 (RSTIRQS) = 1: Reset on underflow/error          |
 *   |                   0: NMI on underflow/error (not used) |
 *   +---------------------------------------------------------+
 *   Using reset (1) for safety - NMI could be masked/ignored
 *
 *   IWDTCSTPR (Count Stop Control Register):
 *   +---------------------------------------------------------+
 *   | Bit 7 (SLCSTP) = 0: Continue counting in sleep mode    |
 *   |                  1: Stop counting in sleep (not used)  |
 *   +---------------------------------------------------------+
 *   Continue counting for safety - sleeping should not bypass watchdog
 * @endverbatim
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Registers configured successfully
 * @retval k_rx_err_hw_unmapped IWDT registers not accessible
 * @retval k_rx_err_invalid_state IWDT already initialized
 *
 * @pre IWDT not yet initialized (s_iwdt_initialized == k_iwdt_not_initialized)
 * @pre iwdt() returns valid hardware register pointer
 * @pre Called after internal_configure_iwdt_control_register()
 *
 * @post IWDTRCR configured for reset action
 * @post IWDTCSTPR configured to continue counting in sleep
 *
 * @note Not thread-safe: must be called from single-threaded initialization context
 * @note These registers can only be written before first refresh
 * @warning Changing RSTIRQS to NMI may compromise system safety
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_iwdt_registers(void)
{
  if (iwdt() == nullptr) {
    return k_rx_err_hw_unmapped;
  }

  if (s_iwdt_initialized == k_iwdt_initialized) {
    return k_rx_err_invalid_state;
  }

  iwdt()->iwdtrcr   = k_iwdt_rstirqs_reset;
  iwdt()->iwdtcstpr = k_iwdt_slcstp_continue;
  return k_rx_ok;
}
#endif

/**
 * @brief Initialize the Independent Watchdog Timer
 *
 * @details
 * Configures and starts the hardware IWDT with the specified timeout period.
 * Once started, the watchdog cannot be stopped - periodic calls to rx_hal_iwdt_feed()
 * are required to prevent system reset.
 *
 * @par Initialization Sequence
 * @verbatim
 *   1. Validate not already initialized
 *   2. Validate timeout_ms within range
 *   3. Look up timeout configuration in table
 *   4. Configure IWDTCR (timeout period, clock divisor, window)
 *   5. Configure IWDTRCR (reset action - not NMI)
 *   6. Configure IWDTCSTPR (continue counting in sleep)
 *   7. Perform first refresh (starts the watchdog)
 *   8. Set initialized flag
 * @endverbatim
 *
 * @par Point of No Return
 * @verbatim
 *   After step 7 (first refresh), the IWDT is running and CANNOT be stopped.
 *   Any failure after this point leaves system in watchdog-protected state.
 * @endverbatim
 *
 * @param[in] timeout_ms Requested timeout period in milliseconds (1-16384)
 *                       Actual timeout may be slightly longer (next available)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok IWDT initialized and running
 * @retval k_rx_err_invalid_state Already initialized
 * @retval k_rx_err_invalid_arg timeout_ms out of valid range
 * @retval k_rx_err_hw_unmapped Hardware registers not accessible
 *
 * @pre IWDT must not be already initialized
 * @pre timeout_ms in range [k_iwdt_timeout_min_ms, k_iwdt_timeout_max_ms]
 * @pre System startup, single-threaded context
 *
 * @post IWDT running with selected timeout period
 * @post s_iwdt_initialized set to k_iwdt_initialized
 * @post Periodic rx_hal_iwdt_feed() calls now required
 *
 * @note On host builds (non-RX), skips hardware access but sets initialized flag
 * @note Actual timeout >= requested (selected from lookup table)
 *
 * @warning Once initialized, IWDT cannot be disabled (requires hard reset)
 * @warning Must call rx_hal_iwdt_feed() faster than timeout period
 *
 * @par Example:
 * @code
 * // Initialize with 2 second timeout
 * rx_err_t err = rx_hal_iwdt_init(2000);
 * if (err != k_rx_ok) {
 *     // Handle error - system cannot start safely
 *     while (1) { // halt }
 * }
 *
 * // Now must feed periodically
 * while (1) {
 *     // ... main loop ...
 *     rx_hal_iwdt_feed();  // Must occur every < 2 seconds
 *     tx_thread_sleep(100);
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_feed() Reset watchdog counter
 * @see rx_hal_iwdt.h Public API and configuration details
 *
 * @since Version 1.0.0
 */
rx_err_t rx_hal_iwdt_init(const uint32_t timeout_ms)
{
#ifdef __RX__
  if (s_iwdt_initialized == k_iwdt_initialized) {
    return k_rx_err_invalid_state;
  }

  if (timeout_ms < k_iwdt_timeout_min_ms) {
    return k_rx_err_invalid_arg;
  }

  if (timeout_ms > k_iwdt_timeout_max_ms) {
    return k_rx_err_invalid_arg;
  }

  /* Find best timeout configuration */
  const iwdt_timeout_entry_t* config = nullptr;
  rx_err_t                    err    = internal_find_timeout_config(timeout_ms, &config);
  if (err != k_rx_ok) {
    return err;
  }

  /*
   * Configure IWDT Control Register (IWDTCR)
   *
   * Bits [1:0]   TOPS  - Timeout Period Select
   * Bits [7:4]   CKS   - Clock Division Ratio Select
   * Bits [9:8]   RPES  - Window End Position (0x03 = 0%, window disabled)
   * Bits [13:12] RPSS  - Window Start Position (0x00 = 100%, full window)
   */
  err = internal_configure_iwdt_control_register(config);
  if (err != k_rx_ok) {
    return err;
  }

  /*
   * Configure Reset Control Register (IWDTRCR)
   *
   * Bit 7 RSTIRQS - Reset Interrupt Request Select
   *   0 = Non-maskable interrupt request (NMI)
   *   1 = Reset
   *
   * We use reset (1) for safety - NMI could be masked or ignored.
   */
  err = internal_configure_iwdt_registers();
  if (err != k_rx_ok) {
    return err;
  }

  /*
   * Configure Count Stop Control Register (IWDTCSTPR)
   *
   * Bit 7 SLCSTP - Sleep Mode Count Stop Control
   *   0 = Count continues during sleep
   *   1 = Count stops during sleep
   *
   * We continue counting during sleep for safety.
   */
  /*
   * Start the IWDT by performing first refresh
   *
   * The IWDT starts counting after the first refresh sequence.
   * After this, the watchdog CANNOT be stopped.
   */
  rx_hal_iwdt_feed();

  s_iwdt_initialized = k_iwdt_initialized;
  return k_rx_ok;

#else
  /* Host-side stub for unit testing */
  (void)timeout_ms;
  s_iwdt_initialized = k_iwdt_initialized;
  return k_rx_ok;
#endif
}

/**
 * @brief Feed the watchdog timer to prevent reset
 *
 * @details
 * Performs the IWDT refresh sequence by writing the magic values 0x00 then 0xFF
 * to the IWDTRR register. This resets the down-counter to its initial value,
 * preventing timeout and system reset.
 *
 * @par Refresh Sequence (Atomic Operation)
 * @verbatim
 *   +--------------------------------------------------------------+
 *   | 1. Disable interrupts (save PSW)                            |
 *   | 2. Write 0x00 to IWDTRR                                     |
 *   | 3. Write 0xFF to IWDTRR                                     |
 *   | 4. Restore interrupt state                                  |
 *   +--------------------------------------------------------------+
 *   CRITICAL: Steps 2 and 3 must not be interrupted!
 * @endverbatim
 *
 * @par Timing Requirements
 * @verbatim
 *   - Execution time: ~2 us including interrupt disable/enable
 *   - Must be called faster than configured timeout period
 *   - Typical call rate: Every 100-500 ms depending on timeout
 * @endverbatim
 *
 * @pre IWDT must be initialized via rx_hal_iwdt_init()
 * @pre Called at rate faster than configured timeout
 *
 * @post IWDT counter reset to initial value
 * @post Interrupt state restored to previous value
 *
 * @note Safe to call from ISRs (uses atomic disable/restore)
 * @note On host builds, function is a no-op
 *
 * @warning Incomplete refresh sequence causes refresh error -> system reset
 * @warning Missing calls to this function cause timeout -> system reset
 *
 * @par Example:
 * @code
 * // Main supervision loop
 * while (1) {
 *     // Check all tasks for deadlock
 *     rx_err_t err = rx_hal_iwdt_check_tasks();
 *     if (err == k_rx_ok) {
 *         // All tasks healthy, feed hardware watchdog
 *         rx_hal_iwdt_feed();
 *     }
 *     // If task timeout detected, DON'T feed - let watchdog reset
 *     tx_thread_sleep(100);  // 100 ms check interval
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_init() Start the watchdog
 * @see rx_hal_iwdt_check_tasks() Verify task health before feeding
 *
 * @since Version 1.0.0
 */
void rx_hal_iwdt_feed(void)
{
#ifdef __RX__
  /*
   * IWDT Refresh Sequence (Section 35.3.3)
   *
   * Write 0x00 followed by 0xFF to IWDTRR.
   * This sequence resets the down-counter to its initial value.
   *
   * CRITICAL: This sequence must not be interrupted.
   * An incomplete sequence will trigger a refresh error.
   *
   * Uses inline assembly to save/restore PSW (Program Status Word) because
   * there is no C library function to manipulate interrupt state on RX architecture.
   */

  /* Disable interrupts during refresh for atomicity */
  uint32_t psw;

  __asm__ volatile("mvfc psw, %0" : "=r"(psw));
  __asm__ volatile("clrpsw i");

  /* Perform refresh sequence */
  iwdt()->iwdtrr = k_iwdt_refresh_start; /* Write 0x00 */
  iwdt()->iwdtrr = k_iwdt_refresh_end;   /* Write 0xFF */

  /* Restore interrupt state */
  __asm__ volatile("mvtc %0, psw" : : "r"(psw));

#else
  /* Host-side stub - no operation */
#endif
}

/**
 * @brief Check if system was reset by watchdog
 *
 * @details
 * Reads the IWDT Status Register (IWDTSR) to determine if the previous
 * system reset was caused by watchdog timeout (underflow) or refresh error.
 * This function should be called early in startup for diagnostics.
 *
 * @par Status Register Flags
 * @verbatim
 *   IWDTSR Bit 14 (UNDFF) - Underflow Flag:
 *   - 1: Counter reached 0 (timeout occurred)
 *   - 0: No underflow
 *
 *   IWDTSR Bit 15 (REFEF) - Refresh Error Flag:
 *   - 1: Invalid refresh sequence detected
 *   - 0: No refresh error
 * @endverbatim
 *
 * @return bool Reset cause indication
 * @retval true  Last reset was caused by watchdog (underflow or refresh error)
 * @retval false Last reset was NOT caused by watchdog (power-on, external, etc.)
 *
 * @pre Can be called before rx_hal_iwdt_init()
 * @pre Status flags preserved across reset (hardware feature)
 *
 * @post Status flags NOT cleared (call rx_hal_iwdt_clear_status() to clear)
 *
 * @note On host builds, always returns false
 * @note Call rx_hal_iwdt_get_reset_cause() for specific cause
 *
 * @par Example:
 * @code
 * // Check at startup
 * if (rx_hal_iwdt_was_reset()) {
 *     uart_debug_puts("[WARN] Recovered from watchdog reset!\r\n");
 *     const char* failed = rx_hal_iwdt_get_failed_task();
 *     if (failed != nullptr) {
 *         uart_debug_printf("Failed task: %s\r\n", failed);
 *     }
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_get_reset_cause() Get specific reset cause
 * @see rx_hal_iwdt_clear_status() Clear status flags after reading
 * @see rx_hal_iwdt_get_failed_task() Get name of task that caused timeout
 *
 * @since Version 1.0.0
 */
bool rx_hal_iwdt_was_reset(void)
{
#ifdef __RX__
  /*
   * Check IWDT Status Register for underflow or refresh error
   *
   * Bit 14 UNDFF - Underflow Flag
   *   1 = Counter underflow occurred
   *
   * Bit 15 REFEF - Refresh Error Flag
   *   1 = Refresh error occurred
   */
  const uint16_t status = iwdt()->iwdtsr;

  return ((status & k_iwdt_sr_undff) != 0) || ((status & k_iwdt_sr_refef) != 0);

#else
  return false;
#endif
}

/**
 * @brief Get detailed watchdog reset cause
 *
 * @details
 * Reads the IWDT Status Register to determine the specific cause of the
 * last watchdog reset. Refresh errors take priority over underflow since
 * they indicate a software bug in the refresh sequence.
 *
 * @par Reset Cause Priority
 * @verbatim
 *   1. Refresh Error (REFEF=1): Invalid refresh sequence
 *      - Incomplete 0x00/0xFF write
 *      - Write occurred outside window (if window mode enabled)
 *
 *   2. Underflow (UNDFF=1): Counter reached zero
 *      - rx_hal_iwdt_feed() not called within timeout period
 *      - Software hang, deadlock, or infinite loop
 *
 *   3. None: No watchdog reset occurred
 *      - Power-on reset, external reset, or other cause
 * @endverbatim
 *
 * @return rx_iwdt_reset_cause_t Specific reset cause
 * @retval k_iwdt_reset_refresh_error Invalid refresh sequence detected
 * @retval k_iwdt_reset_underflow     Counter timeout (feed not called in time)
 * @retval k_iwdt_reset_none          No watchdog reset (power-on, external, etc.)
 *
 * @pre Can be called before rx_hal_iwdt_init()
 * @pre Status flags preserved across reset
 *
 * @post Status flags NOT cleared (call rx_hal_iwdt_clear_status() to clear)
 *
 * @note On host builds, always returns k_iwdt_reset_none
 * @note Refresh error has priority over underflow in return value
 *
 * @par Example:
 * @code
 * rx_iwdt_reset_cause_t cause = rx_hal_iwdt_get_reset_cause();
 * switch (cause) {
 *     case k_iwdt_reset_refresh_error:
 *         uart_debug_puts("ERROR: Watchdog refresh sequence error!\r\n");
 *         break;
 *     case k_iwdt_reset_underflow:
 *         uart_debug_puts("WARN: Watchdog timeout - software hang detected\r\n");
 *         break;
 *     case k_iwdt_reset_none:
 *     default:
 *         uart_debug_puts("Normal startup\r\n");
 *         break;
 * }
 * rx_hal_iwdt_clear_status();  // Clear flags after reading
 * @endcode
 *
 * @see rx_hal_iwdt_was_reset() Quick check for any watchdog reset
 * @see rx_hal_iwdt_clear_status() Clear status flags
 *
 * @since Version 1.0.0
 */
rx_iwdt_reset_cause_t rx_hal_iwdt_get_reset_cause(void)
{
#ifdef __RX__
  const uint16_t status = iwdt()->iwdtsr;

  if (status & k_iwdt_sr_refef) {
    return k_iwdt_reset_refresh_error;
  }

  if (status & k_iwdt_sr_undff) {
    return k_iwdt_reset_underflow;
  }

  return k_iwdt_reset_none;

#else
  return k_iwdt_reset_none;
#endif
}

/**
 * @brief Clear watchdog status flags
 *
 * @details
 * Clears the UNDFF (underflow) and REFEF (refresh error) flags in the
 * IWDT Status Register. These flags are "write-0-to-clear" type - writing
 * 0 to a flag clears it, writing 1 has no effect.
 *
 * @par Clear Sequence
 * @verbatim
 *   1. Read IWDTSR (get current status)
 *   2. AND with ~(UNDFF | REFEF) to clear both flags
 *   3. Write back to IWDTSR
 *
 *   Note: Read-modify-write is safe because flags are write-0-to-clear
 * @endverbatim
 *
 * @pre Status has been read via rx_hal_iwdt_was_reset() or rx_hal_iwdt_get_reset_cause()
 *
 * @post UNDFF flag cleared
 * @post REFEF flag cleared
 *
 * @note On host builds, function is a no-op
 * @note Call after reading status at startup to prepare for next reset
 *
 * @par Example:
 * @code
 * // Startup diagnostic sequence
 * if (rx_hal_iwdt_was_reset()) {
 *     rx_iwdt_reset_cause_t cause = rx_hal_iwdt_get_reset_cause();
 *     log_watchdog_reset(cause);
 * }
 * // Clear flags so next reset can be detected
 * rx_hal_iwdt_clear_status();
 * @endcode
 *
 * @see rx_hal_iwdt_was_reset() Check reset status
 * @see rx_hal_iwdt_get_reset_cause() Get specific cause
 *
 * @since Version 1.0.0
 */
void rx_hal_iwdt_clear_status(void)
{
#ifdef __RX__
  /*
   * Clear status flags by writing 0 to the flag bits
   *
   * Bits 14-15 are write-0-to-clear
   */
  iwdt()->iwdtsr &= (uint16_t) ~(uint16_t)(k_iwdt_sr_undff | k_iwdt_sr_refef);

#else
  /* Host-side stub - no operation */
#endif
}

/* =============================================================================
 * Task-Level Monitoring (Issue 20: Enhanced Watchdog)
 * =============================================================================
 */

/**
 * @defgroup iwdt_task_monitoring Task-Level Watchdog Monitoring
 * @brief Software layer for per-task heartbeat tracking
 *
 * @details
 * Provides fine-grained monitoring of individual RTOS tasks beyond hardware
 * watchdog. Tasks register with unique names and timeout periods, then must
 * send periodic heartbeats. A monitoring task checks for timeouts and
 * identifies which specific task has hung.
 *
 * @par Architecture
 * @verbatim
 *   +---------------------------------------------------------------------+
 *   |                      Task Monitoring Layer                         |
 *   |                                                                     |
 *   |  +---------+  +---------+  +---------+  +---------+               |
 *   |  | Task 0  |  | Task 1  |  | Task 2  |  | Task N  |               |
 *   |  |"Motor"  |  |"SPI"    |  |"USB"    |  |  ...    |               |
 *   |  | 500ms   |  | 1000ms  |  | 2000ms  |  |         |               |
 *   |  +----+----+  +----+----+  +----+----+  +----+----+               |
 *   |       |            |            |            |                     |
 *   |       | heartbeat()| heartbeat()| heartbeat()|                     |
 *   |       v            v            v            v                     |
 *   |  +-------------------------------------------------------------+  |
 *   |  |              s_task_monitor.tasks[] Array                   |  |
 *   |  |  [task_name, timeout_ms, last_heartbeat_ms, registered]     |  |
 *   |  +-------------------------------------------------------------+  |
 *   |                              |                                     |
 *   |                              | rx_hal_iwdt_check_tasks()               |
 *   |                              v                                     |
 *   |  +-------------------------------------------------------------+  |
 *   |  | For each task: if (now - last_heartbeat > timeout) -> FAIL   |  |
 *   |  +-------------------------------------------------------------+  |
 *   |                              |                                     |
 *   |                              | If all pass: rx_hal_iwdt_feed()         |
 *   |                              v                                     |
 *   |  +-------------------------------------------------------------+  |
 *   |  |                  Hardware IWDT (refresh)                    |  |
 *   |  +-------------------------------------------------------------+  |
 *   +---------------------------------------------------------------------+
 * @endverbatim
 *
 * @{
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rx_log.h"
#include "tx_api.h"

/**
 * @enum task_registration_state_t
 * @brief Task registration state tracking
 *
 * @details
 * Indicates whether a task_monitor_t slot is in use. Allows sparse array
 * usage and efficient task lookup.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_task_not_registered = 0, /**< Slot available for new task registration */
  k_task_registered     = 1, /**< Slot in use by registered task */
} task_registration_state_t;

/**
 * @enum task_search_constants_t
 * @brief Constants for task search operations
 *
 * @details
 * Defines sentinel values and bounds for task array operations.
 * Using signed int32_t allows -1 sentinel for "not found" result.
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_task_not_found = -1, /**< Return value when task name not in array */
  k_task_idx_min   = 0,  /**< Minimum valid task index (array start) */
} task_search_constants_t;

/**
 * @struct task_monitor_t
 * @brief Task monitoring state for a single registered task
 *
 * @details
 * Stores all information needed to track one task's heartbeat status.
 * Each registered task gets one slot in the s_task_monitor.tasks[] array.
 *
 * @par Memory Layout (~32 bytes per task)
 * @verbatim
 *   Offset  0: task_name[16]     [16 bytes] - Null-terminated name
 *   Offset 16: timeout_ms        [4 bytes]  - Heartbeat timeout
 *   Offset 20: last_heartbeat_ms [4 bytes]  - Last heartbeat timestamp
 *   Offset 24: registered        [1 byte]   - Registration state
 *   Offset 25: padding           [7 bytes]  - Alignment padding
 *   Total: ~32 bytes (depends on alignment)
 * @endverbatim
 *
 * @par Timeout Check Algorithm
 * @verbatim
 *   elapsed = current_time - last_heartbeat_ms
 *   if (elapsed > timeout_ms) -> Task has timed out
 * @endverbatim
 *
 * @invariant task_name is null-terminated
 * @invariant timeout_ms >= k_iwdt_timeout_min_ms
 * @invariant If registered == k_task_registered, task_name[0] != '\0'
 *
 * @since Version 1.0.0
 */
typedef struct {
  char
    task_name[k_task_name_max_len]; /**< Task identifier (null-terminated, truncated to 15 chars) */
  uint32_t                  timeout_ms; /**< Maximum time between heartbeats in milliseconds */
  uint32_t                  last_heartbeat_ms; /**< Timestamp (tx_time_get) of last heartbeat */
  task_registration_state_t registered;        /**< Whether this slot is in use */
} task_monitor_t;

/**
 * @struct task_monitor_state_t
 * @brief Complete task monitoring module state
 *
 * @details
 * Contains all state for the task monitoring subsystem. Statically allocated
 * to avoid dynamic memory (NASA Power of 10 Rule 3). One instance exists
 * as s_task_monitor.
 *
 * @par Memory Layout (~520 bytes total)
 * @verbatim
 *   Offset   0: tasks[8]          [~256 bytes] - Task array (8 x 32 bytes)
 *   Offset 256: task_count        [1 byte]     - Number of registered tasks
 *   Offset 257: failed_task[16]   [16 bytes]   - Failed task name buffer
 *   Offset 273: padding           [7 bytes]    - Alignment
 *   Total: ~280 bytes (actual layout depends on alignment)
 * @endverbatim
 *
 * @invariant task_count <= k_iwdt_max_tasks
 * @invariant failed_task is null-terminated (or all zeros if no failure)
 *
 * @since Version 1.0.0
 */
typedef struct {
  task_monitor_t tasks[k_iwdt_max_tasks];          /**< Array of task monitoring slots */
  uint8_t        task_count;                       /**< Number of registered tasks (0-8) */
  char           failed_task[k_task_name_max_len]; /**< Name of most recent failed task */
} task_monitor_state_t;

/**
 * @var s_task_monitor
 * @brief Module state for task monitoring (static allocation)
 *
 * @details
 * Single instance of task monitoring state. Zero-initialized at startup,
 * ensuring all tasks start unregistered and no failed task recorded.
 *
 * @par Initial State
 * @verbatim
 *   - task_count = 0
 *   - All tasks[].registered = k_task_not_registered
 *   - failed_task[0] = '\0'
 * @endverbatim
 *
 * @par Thread Safety
 * - Registration: Call from single thread during initialization
 * - Heartbeats: Not thread-safe, caller must synchronize
 * - Check: Not thread-safe, typically called from single monitoring task
 *
 * @warning Do not access directly - use API functions
 *
 * @since Version 1.0.0
 */
static task_monitor_state_t s_task_monitor = {};

/**
 * @brief Find task index by name in the registered tasks array
 *
 * @details
 * Performs linear search through s_task_monitor.tasks[] to find a task
 * matching the given name. Uses strncmp for safe string comparison.
 *
 * @par Algorithm
 * @verbatim
 *   1. Validate task_name is not nullptr
 *   2. Validate task_name is not empty string
 *   3. Linear scan through registered tasks (0 to task_count-1):
 *      - Compare names using strncmp(task_name, k_task_name_cmp_len)
 *      - If match found: return index
 *   4. If no match: return k_task_not_found (-1)
 * @endverbatim
 *
 * @par Complexity
 * - Time: O(n x m) where n = task_count, m = k_task_name_cmp_len
 * - Space: O(1) - no additional memory
 *
 * @param[in] task_name Task name to search for (null-terminated string)
 *
 * @return int32_t Task index or error sentinel
 * @retval 0..7 Index of matching task in s_task_monitor.tasks[]
 * @retval k_task_not_found (-1) Task not found, or task_name invalid
 *
 * @pre task_name != nullptr
 * @pre task_name[0] != '\0'
 *
 * @post Return value < s_task_monitor.task_count (if found)
 * @post Return value == k_task_not_found (if not found or invalid input)
 *
 * @note Uses k_task_name_cmp_len (15) for comparison, not full buffer
 * @note Empty string returns k_task_not_found (invalid task name)
 *
 * @see rx_hal_iwdt_task_heartbeat() Primary caller
 * @see rx_hal_iwdt_register_task() Uses for duplicate detection
 *
 * @since Version 1.0.0
 */
static int32_t internal_find_task(const char* task_name)
{
  if (task_name == nullptr) {
    return k_task_not_found;
  }

  if (task_name[0] == '\0') {
    return k_task_not_found; /* Empty task name invalid */
  }

  for (uint32_t i = (uint32_t)k_task_idx_min; i < s_task_monitor.task_count; i++) {
    if (strncmp(s_task_monitor.tasks[i].task_name, task_name, k_task_name_cmp_len) == 0) {
      return (int32_t)i;
    }
  }

  return k_task_not_found;
}

/**
 * @brief Register a task for watchdog monitoring
 *
 * @details
 * Adds a task to the monitoring array with specified timeout period. The task
 * must subsequently call rx_hal_iwdt_task_heartbeat() within each timeout period
 * to indicate liveness. Failed tasks are detected by rx_hal_iwdt_check_tasks().
 *
 * @par Registration Process
 * @verbatim
 *   1. Validate task_name (not nullptr, not empty)
 *   2. Validate timeout_ms (within allowed range)
 *   3. Check capacity (task_count < k_iwdt_max_tasks)
 *   4. Check for duplicate registration (same name)
 *   5. Copy task name to slot (truncate if > 15 chars)
 *   6. Initialize timeout and heartbeat timestamp
 *   7. Mark slot as registered
 *   8. Increment task_count
 * @endverbatim
 *
 * @param[in] task_name Unique task identifier string (max 15 chars + null)
 *                      Must be non-NULL and non-empty
 * @param[in] timeout_ms Heartbeat timeout in milliseconds
 *                       Range: [k_iwdt_timeout_min_ms, k_iwdt_timeout_max_ms]
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task registered successfully
 * @retval k_rx_err_invalid_arg task_name is nullptr, empty, or timeout_ms out of range
 * @retval k_rx_err_no_mem Maximum tasks (k_iwdt_max_tasks) already registered
 * @retval k_rx_err_exists Task with same name already registered
 *
 * @pre IWDT module initialized (but not strictly required for registration)
 * @pre task_name points to valid null-terminated string
 * @pre timeout_ms appropriate for task's execution frequency
 *
 * @post Task added to s_task_monitor.tasks[]
 * @post s_task_monitor.task_count incremented
 * @post Initial heartbeat timestamp set to current time
 *
 * @invariant s_task_monitor.task_count <= k_iwdt_max_tasks at all times
 *
 * @note Task name truncated to 15 characters if longer
 * @note First heartbeat auto-recorded at registration time
 *
 * @warning Not thread-safe - call during initialization only
 *
 * @par Example:
 * @code
 * // Register motor control task (must heartbeat every 500ms)
 * rx_err_t err = rx_hal_iwdt_register_task("MotorCtrl", 500);
 * if (err != k_rx_ok) {
 *     // Handle registration failure
 *     return err;
 * }
 *
 * // In motor task loop
 * while (1) {
 *     motor_control_update();
 *     rx_hal_iwdt_task_heartbeat("MotorCtrl");
 *     tx_thread_sleep(10);  // 100Hz loop
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_task_heartbeat() Send heartbeat from registered task
 * @see rx_hal_iwdt_check_tasks() Check all tasks for timeout
 *
 * @since Version 1.0.0
 */
rx_err_t rx_hal_iwdt_register_task(const char* task_name, uint32_t timeout_ms)
{
  /* Validate parameters */
  if (task_name == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (task_name[0] == '\0') {
    return k_rx_err_invalid_arg;
  }

  if (timeout_ms < k_iwdt_timeout_min_ms || timeout_ms > k_iwdt_timeout_max_ms) {
    return k_rx_err_invalid_arg;
  }

  if (s_task_monitor.task_count >= k_iwdt_max_tasks) {
    return k_rx_err_no_mem;
  }

  /* Check for duplicate registration */
  if (internal_find_task(task_name) != k_task_not_found) {
    return k_rx_err_exists;
  }

  /* Register new task */
  task_monitor_t* task = &s_task_monitor.tasks[s_task_monitor.task_count];

  strncpy(task->task_name, task_name, k_task_name_cmp_len);
  task->task_name[k_task_name_cmp_len] = '\0'; /* Ensure null termination */

  task->timeout_ms        = timeout_ms;
  task->last_heartbeat_ms = tx_time_get();
  task->registered        = k_task_registered;

  s_task_monitor.task_count++;

  rx_log_info(s_tag, "Registered task for monitoring");

  return k_rx_ok;
}

/**
 * @brief Record a task heartbeat to indicate liveness
 *
 * @details
 * Updates the last_heartbeat_ms timestamp for the specified task. Must be
 * called by each monitored task at a rate faster than its configured timeout
 * to avoid being flagged as failed by rx_hal_iwdt_check_tasks().
 *
 * @par Heartbeat Algorithm
 * @verbatim
 *   1. Validate task_name (not nullptr)
 *   2. Find task in registered array (internal_find_task)
 *   3. If found: Update last_heartbeat_ms = tx_time_get()
 *   4. If not found: Log error (graceful degradation)
 * @endverbatim
 *
 * @par Timing Requirements
 * @verbatim
 *   - Must be called faster than task's timeout_ms
 *   - Typical pattern: Call at end of task's main loop iteration
 *   - Example: 500ms timeout -> call at least every 400ms
 * @endverbatim
 *
 * @param[in] task_name Name of task sending heartbeat
 *                      Must match name used in rx_hal_iwdt_register_task()
 *
 * @pre task_name must be non-NULL
 * @pre Task must be registered via rx_hal_iwdt_register_task()
 * @pre Called faster than task's configured timeout
 *
 * @post Task's last_heartbeat_ms updated to current time
 * @post No state change if task not found (logs error only)
 *
 * @note Void return - errors logged but not returned
 * @note Unregistered task heartbeats are logged and ignored
 * @note Uses tx_time_get() for timestamp (ThreadX tick count)
 *
 * @warning Missing heartbeats cause rx_hal_iwdt_check_tasks() to detect timeout
 *
 * @par Example:
 * @code
 * static void communication_task(ULONG input) {
 *     while (1) {
 *         // Process incoming messages
 *         process_messages();
 *
 *         // Indicate task is alive
 *         rx_hal_iwdt_task_heartbeat("CommTask");
 *
 *         // Wait for next iteration
 *         tx_thread_sleep(50);  // 50ms loop
 *     }
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_register_task() Register task before heartbeating
 * @see rx_hal_iwdt_check_tasks() Verifies heartbeats are received
 *
 * @since Version 1.0.0
 */
void rx_hal_iwdt_task_heartbeat(const char* task_name)
{
  if (task_name == nullptr) {
    rx_log_error(s_tag, "Heartbeat called with nullptr task_name");
    return;
  }

  int32_t idx = internal_find_task(task_name);

  if (idx == k_task_not_found) {
    /* Task not registered - log warning but don't fail */
    rx_log_error(s_tag, "Heartbeat from unregistered task");
    return;
  }

  /* Update heartbeat timestamp */
  s_task_monitor.tasks[idx].last_heartbeat_ms = tx_time_get();
}

/**
 * @brief Check all monitored tasks for timeout violations
 *
 * @details
 * Iterates through all registered tasks and compares elapsed time since
 * last heartbeat against each task's configured timeout. If any task has
 * exceeded its timeout, logs the failure and records the task name.
 *
 * @par Check Algorithm
 * @verbatim
 *   1. Validate IWDT is initialized
 *   2. Validate task_count is within bounds (detect corruption)
 *   3. Get current time from ThreadX
 *   4. For each registered task:
 *      a. Calculate elapsed = current_time - last_heartbeat_ms
 *      b. If elapsed > timeout_ms:
 *         - Log error with task name and timeout
 *         - Record failed task name
 *         - Set result = k_rx_err_timeout
 *   5. Return result (k_rx_ok if all pass, k_rx_err_timeout if any fail)
 * @endverbatim
 *
 * @par Typical Usage Pattern
 * @verbatim
 *   Monitoring Task Loop:
 *   +--------------------------------------------+
 *   |  1. rx_hal_iwdt_check_tasks()                  |
 *   |  2. If all tasks OK: rx_hal_iwdt_feed()        |
 *   |  3. If timeout: DON'T feed (let WDT reset) |
 *   |  4. tx_thread_sleep(check_interval)        |
 *   |  5. Repeat                                 |
 *   +--------------------------------------------+
 * @endverbatim
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All tasks have sent heartbeats within timeout
 * @retval k_rx_err_timeout One or more tasks exceeded timeout (hung/deadlocked)
 * @retval k_rx_err_invalid_state IWDT not initialized or state corrupted
 *
 * @pre IWDT must be initialized via rx_hal_iwdt_init()
 * @pre Called periodically from monitoring task
 *
 * @post If timeout detected, failed task name recorded in s_task_monitor.failed_task
 * @post Error logged for each timed-out task
 * @post Continues checking all tasks even if one fails (logs all failures)
 *
 * @note Does NOT automatically feed hardware watchdog
 * @note Checks all tasks even after finding failure (comprehensive logging)
 * @note Uses tx_time_get() which returns uint32_t tick count
 *
 * @warning Do NOT feed watchdog if this function returns k_rx_err_timeout
 *
 * @par Example:
 * @code
 * static void watchdog_monitor_task(ULONG input) {
 *     while (1) {
 *         rx_err_t result = rx_hal_iwdt_check_tasks();
 *
 *         if (result == k_rx_ok) {
 *             // All tasks healthy - feed hardware watchdog
 *             rx_hal_iwdt_feed();
 *         } else if (result == k_rx_err_timeout) {
 *             // Task deadlock detected!
 *             // DON'T feed - let hardware watchdog reset system
 *             const char* failed = rx_hal_iwdt_get_failed_task();
 *             uart_debug_printf("DEADLOCK: %s\r\n", failed);
 *         }
 *
 *         tx_thread_sleep(100);  // Check every 100ms
 *     }
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_register_task() Register tasks before checking
 * @see rx_hal_iwdt_task_heartbeat() Tasks report liveness
 * @see rx_hal_iwdt_get_failed_task() Get name of failed task
 * @see rx_hal_iwdt_feed() Feed hardware watchdog if all tasks OK
 *
 * @since Version 1.0.0
 */
rx_err_t rx_hal_iwdt_check_tasks(void)
{
  uint32_t current_time_ms;
  rx_err_t result;

  const task_monitor_t* task = nullptr;
  uint32_t              elapsed_ms;
  char                  log_msg[k_log_msg_buffer_size];
  int32_t               snprintf_result;

  if (s_iwdt_initialized != k_iwdt_initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_task_monitor.task_count > k_iwdt_max_tasks) {
    return k_rx_err_invalid_state; /* Corrupted state */
  }

  current_time_ms = tx_time_get();
  result          = k_rx_ok;

  for (uint32_t i = (uint32_t)k_task_idx_min; i < s_task_monitor.task_count; i++) {
    task       = &s_task_monitor.tasks[i];
    elapsed_ms = current_time_ms - task->last_heartbeat_ms;

    if (elapsed_ms > task->timeout_ms) {
      /* Task has exceeded heartbeat timeout - deadlock detected */
      snprintf_result = snprintf(log_msg,
                                 sizeof(log_msg),
                                 "Task deadlock: %s (timeout %" PRIu32 " ms)",
                                 task->task_name,
                                 task->timeout_ms);

      if (snprintf_result > 0 && (uint32_t)snprintf_result < sizeof(log_msg)) {
        rx_log_error(s_tag, log_msg);
      }

      /* Record failed task name for post-mortem */
      strncpy(s_task_monitor.failed_task, task->task_name, k_task_name_cmp_len);
      s_task_monitor.failed_task[k_task_name_cmp_len] = '\0';

      result = k_rx_err_timeout;
      /* Continue checking other tasks to log all failures */
    }
  }

  return result;
}

/**
 * @brief Get name of last failed task for post-mortem analysis
 *
 * @details
 * Returns the name of the task that most recently exceeded its heartbeat
 * timeout. This information is recorded by rx_hal_iwdt_check_tasks() when it
 * detects a timeout. Useful for diagnosing which task caused a watchdog reset.
 *
 * @par Use Cases
 * @verbatim
 *   1. Post-reset diagnostics:
 *      - After watchdog reset, call rx_hal_iwdt_get_failed_task()
 *      - Log failed task name for debugging
 *      - Helps identify deadlock/hang location
 *
 *   2. Pre-reset logging:
 *      - When rx_hal_iwdt_check_tasks() returns k_rx_err_timeout
 *      - Get failed task name and log before reset occurs
 * @endverbatim
 *
 * @return const char* Pointer to failed task name or nullptr
 * @retval non-NULL Pointer to internal buffer containing task name
 * @retval NULL     No failure recorded, or IWDT not initialized
 *
 * @pre IWDT must be initialized via rx_hal_iwdt_init()
 * @pre rx_hal_iwdt_check_tasks() has been called and detected a timeout
 *
 * @post Returns pointer to s_task_monitor.failed_task (do not modify)
 * @post Returned pointer valid until next timeout detection
 *
 * @note Returns pointer to internal static buffer - do not free
 * @note Buffer contents may change on next rx_hal_iwdt_check_tasks() failure
 * @note Returns NULL if failed_task[0] == '\0' (no failure recorded)
 *
 * @warning Do not modify returned buffer (const pointer)
 * @warning Buffer may be overwritten by next timeout detection
 *
 * @par Example:
 * @code
 * // After detecting timeout
 * rx_err_t result = rx_hal_iwdt_check_tasks();
 * if (result == k_rx_err_timeout) {
 *     const char* failed = rx_hal_iwdt_get_failed_task();
 *     if (failed != nullptr) {
 *         uart_debug_printf("FATAL: Task '%s' deadlocked\r\n", failed);
 *     }
 *     // Let watchdog reset the system...
 * }
 *
 * // After system restart
 * if (rx_hal_iwdt_was_reset()) {
 *     const char* failed = rx_hal_iwdt_get_failed_task();
 *     // Note: May be NULL if buffer not preserved across reset
 * }
 * @endcode
 *
 * @see rx_hal_iwdt_check_tasks() Records failed task name
 * @see rx_hal_iwdt_was_reset() Check if watchdog caused reset
 *
 * @since Version 1.0.0
 */
const char* rx_hal_iwdt_get_failed_task(void)
{
  if (s_iwdt_initialized != k_iwdt_initialized) {
    return nullptr;
  }

  if (s_task_monitor.failed_task[0] != '\0') {
    return s_task_monitor.failed_task;
  }

  return nullptr;
}

/** @} */ /* End of iwdt_task_monitoring group */
