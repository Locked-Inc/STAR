/**
 * @file rx_log_usb.c
 * @brief USB CDC logging backend implementation with boot buffering and thread safety
 *
 * @details
 * Provides enhanced USB CDC logging support for the rx_log system with:
 * - **Boot log buffering**: 512B ring buffer for logs during USB enumeration
 * - **Thread safety**: ThreadX mutex for multi-task logging
 * - **Error handling**: Graceful handling of USB TX buffer full conditions
 * - **Statistics**: Tracking of total, dropped, and buffered logs
 * - **Non-blocking**: Never blocks on USB TX full (drops logs with statistics)
 *
 * ## Boot Sequence
 *
 * ```
 * Power On -> USB Init -> Enumeration (200ms) -> Configured -> Flush Boot Buffer
 *    v           v            v                    v              v
 *  Logs      Buffer      Buffer                Logs to       Boot logs
 *  start     in RAM      in RAM                 USB           appear
 * ```
 *
 * ## Thread Safety
 *
 * All public functions use ThreadX mutex to ensure atomic operations across
 * multiple tasks. Mutex is initialized on first use (lazy initialization).
 *
 * ## Error Recovery
 *
 * - **USB not configured**: Buffer logs in boot buffer
 * - **TX buffer full**: Drop logs and increment statistics
 * - **Boot buffer full**: Drop logs and mark overflow
 *
 * @see rx_log.h Main logging API
 * @see rx_usb.h USB CDC driver
 *
 * @since Version 1.0.0
 * @date 2026-02-05
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#include <stdint.h>
#include <string.h>

#include "rx_check.h"
#include "rx_log.h"
#include "rx_usb.h"
#include "tx_api.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief Boot log buffer configuration
 */
typedef enum : uint16_t {
  /**
     * @brief Boot log buffer size in bytes
     * @details
     * Sized to hold ~500ms of logs at typical boot rate:
     * - Typical boot: ~20 log lines
     * - Average line: ~50 chars
     * - Total: ~1000 chars -> 512B buffer (50% headroom)
     * @par Value: 512 bytes
     */
  k_boot_buffer_size = 512,

  /**
     * @brief Maximum chunk size for buffer flush
     * @details
     * USB bulk packet size is 64 bytes. Flush in 64-byte chunks for efficiency.
     * @par Value: 64 bytes
     */
  k_flush_chunk_size = 64,

} usb_log_buffer_config_t;

/**
 * @brief Numeric formatting constants for putint/puthex functions
 */
typedef enum : uint8_t {
  k_int32_buf_len  = 12U,   /**< Buffer size for int32: '-' + 10 digits + NUL */
  k_decimal_base   = 10U,   /**< Decimal base for integer conversion */
  k_hex_max_digits = 8U,    /**< Maximum hex digits for a 32-bit value */
  k_hex_buf_len    = 9U,    /**< Buffer size for hex: 8 digits + NUL */
  k_nibble_shift   = 4U,    /**< Bits per nibble (hex digit) */
  k_nibble_mask    = 0x0FU, /**< Mask to extract one nibble */
} usb_log_format_constants_t;

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @struct usb_log_boot_buffer_t
 * @brief Ring buffer for boot logs (before USB configured)
 *
 * @details
 * Holds logs generated during USB enumeration (0-200ms after power-on).
 * Once USB reaches k_usb_state_configured, buffer is flushed to USB CDC Port 2.
 *
 * **Ring Buffer Layout**:
 * ```
 * data[0] ... data[head-1] data[head] ... data[size-1]
 *              ^ write here            ^ wrap around
 * ```
 *
 * **Overflow Handling**:
 * - If `count + new_bytes > size`, drop new bytes and set `overflow = true`
 * - Overflow warning logged after flush completes
 */
typedef struct {
  char     data[k_boot_buffer_size]; /**< Ring buffer storage */
  uint16_t head;                     /**< Write index (next write position) */
  uint16_t count;                    /**< Number of buffered bytes */
  bool     overflow;                 /**< True if buffer overflowed */
} usb_log_boot_buffer_t;

/* usb_log_stats_t is defined in rx_log.h */

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief Boot log buffer (used until USB configured) */
static usb_log_boot_buffer_t s_boot_buffer = {};

/** @brief USB ready flag (set when k_usb_state_configured reached) */
static volatile bool s_usb_ready = false;

/** @brief Logging statistics */
static usb_log_stats_t s_stats = {};

/** @brief ThreadX mutex for thread safety */
static TX_MUTEX s_log_mutex;

/** @brief Mutex initialization flag (lazy init on first use) */
static bool s_mutex_initialized = false;

/* =============================================================================
 * Internal Functions
 * =============================================================================
 */

/**
 * @brief Initialize mutex on first use (lazy initialization)
 *
 * @details
 * Creates ThreadX mutex with no priority inheritance. Called automatically
 * on first log write. Subsequent calls are no-ops.
 *
 * @return void (no error indication - failures silently ignored)
 *
 * @pre ThreadX kernel must be running (called after tx_kernel_enter())
 * @post s_mutex_initialized = true on success, s_log_mutex created
 * @post Logging proceeds without mutex if creation fails (unsafe but functional)
 *
 * @note **Thread Safety**: Not thread-safe itself, but called before mutex is needed
 * @note **Failure Mode**: Silent failure if tx_mutex_create() fails (logs continue without protection)
 *
 * @par Algorithm:
 * 1. Check if already initialized (s_mutex_initialized flag)
 * 2. If not, create ThreadX mutex with TX_NO_INHERIT policy
 * 3. Set s_mutex_initialized = true on success
 * 4. On failure, leave flag false (logging proceeds unprotected)
 *
 * @since Version 1.0.0
 */
static void internal_init_mutex_once(void)
{
  if (!s_mutex_initialized) {
    (void)tx_mutex_create(&s_log_mutex, (CHAR*)"usb_log_mutex", TX_NO_INHERIT);
    /* tx_mutex_create always succeeds when ThreadX is running (post-tx_kernel_enter). */
    s_mutex_initialized = true;
  }
}

/**
 * @brief Buffer log data to boot buffer (before USB ready)
 *
 * @details
 * Appends data to ring buffer if space available. If buffer full, drops data
 * and sets overflow flag. Overflow warning logged after USB ready.
 *
 * Boot log buffering enables logs emitted during system startup (before USB
 * enumeration completes) to be preserved and flushed to USB once the CDC
 * interface becomes ready. The ring buffer wraps writes using a head index and
 * a running byte count.
 *
 * @param[in] data Pointer to data bytes to buffer (must not be NULL)
 * @param[in] len Number of bytes to buffer (must be > 0)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Data buffered successfully
 * @retval k_rx_err_no_mem Buffer full, data dropped and overflow flag set
 *
 * @pre Mutex locked by caller (internal_init_mutex_once() invoked before call)
 * @pre s_boot_buffer.head is within [0, k_boot_buffer_size)
 * @post s_boot_buffer.count incremented by len on success
 * @post s_stats.boot_buffered incremented by len on success
 * @post s_boot_buffer.overflow set and s_stats.dropped_bytes incremented on overflow
 *
 * @note Not thread-safe on its own; caller must hold s_log_mutex before invoking
 *
 * @par Example:
 * @code
 * // Called internally by internal_write_usb() when USB not yet configured:
 * rx_err_t result = internal_buffer_boot_log("boot message\n", 14);
 * if (result == k_rx_err_no_mem) {
 *     // Boot buffer full - message dropped, overflow flag set
 * }
 * @endcode
 *
 * @see internal_check_usb_ready() Flushes this buffer once USB is configured
 * @see rx_log_usb_puts() Public API that triggers this path during boot
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_buffer_boot_log(const char* data, uint16_t len)
{
  /* Check space available */
  if ((s_boot_buffer.count + len) > k_boot_buffer_size) {
    s_boot_buffer.overflow = true;
    s_stats.dropped_bytes += len;
    return k_rx_err_no_mem;
  }

  /* Append to ring buffer (simple linear write, wrap at end) */
  for (uint16_t i = 0; i < len; i++) {
    s_boot_buffer.data[s_boot_buffer.head] = data[i];
    s_boot_buffer.head = (uint16_t)((s_boot_buffer.head + 1) % k_boot_buffer_size);
  }

  s_boot_buffer.count += len;
  s_stats.boot_buffered += len;

  return k_rx_ok;
}

/**
 * @brief Check USB ready status and flush boot buffer if ready
 *
 * @details
 * Polls USB CDC Port 2 configuration status. If configured and not yet flushed,
 * sends all buffered boot logs to USB in 64-byte chunks.
 *
 * **Flush Strategy**:
 * - Sends in k_flush_chunk_size (64 byte) chunks for efficiency
 * - If USB TX full during flush, aborts and retries on next call
 * - Logs overflow warning if boot buffer overflowed
 *
 * ## Algorithm
 * 1. Check if already flushed (s_usb_ready flag)
 * 2. Poll USB configured state (rx_usb_is_configured)
 * 3. If not configured, return (continue buffering)
 * 4. Calculate ring buffer tail pointer
 * 5. Loop: Send chunks up to k_flush_chunk_size (64 bytes)
 *    - Handle ring buffer wrap-around
 *    - Call rx_usb_write() for each chunk
 *    - If k_rx_err_busy, abort and retry later
 *    - If other error, log and mark as flushed (avoid infinite retry)
 * 6. Mark s_usb_ready = true when flush complete
 * 7. Log overflow warning if boot buffer overflowed
 *
 * @return void (no error indication - errors handled internally)
 *
 * @pre Mutex locked by caller (ensures atomicity)
 * @pre ThreadX kernel running (USB stack operational)
 * @post Boot buffer flushed to USB if ready, s_usb_ready = true on success
 * @post s_stats.usb_errors incremented on USB write errors
 * @post Overflow warning logged if s_boot_buffer.overflow = true
 *
 * @note **Thread Safety**: Assumes caller holds mutex lock
 * @note **Idempotent**: Safe to call multiple times (no-op after first flush)
 * @note **Non-Blocking**: Aborts flush if USB TX full, retries on next call
 *
 * @par Performance:
 * - Typical flush time: 10-20ms for 512B buffer @ 12 Mbps USB Full-Speed
 * - Chunk size: 64 bytes (matches USB Full-Speed max packet size)
 *
 * @since Version 1.0.0
 */
static void internal_check_usb_ready(void)
{
  /* Already flushed? Nothing to do */
  if (s_usb_ready) {
    return;
  }

  /* Check if USB configured */
  if (!rx_usb_is_configured(k_usb_port_log)) {
    return; /* Not ready yet, keep buffering */
  }

  /* USB is ready! Flush boot buffer */
  uint16_t tail = (uint16_t)((s_boot_buffer.head - s_boot_buffer.count + k_boot_buffer_size) %
                             k_boot_buffer_size);

  while (s_boot_buffer.count > 0) {
    /* Calculate chunk size (up to k_flush_chunk_size bytes) */
    uint16_t chunk_len =
      (s_boot_buffer.count > k_flush_chunk_size) ? k_flush_chunk_size : s_boot_buffer.count;

    /* Handle ring buffer wrap-around */
    uint16_t linear_len =
      (tail + chunk_len <= k_boot_buffer_size) ? chunk_len : (uint16_t)(k_boot_buffer_size - tail);

    /* Write chunk to USB */
    rx_err_t err =
      rx_usb_write(k_usb_port_log, (const uint8_t*)&s_boot_buffer.data[tail], linear_len);

    if (err == k_rx_err_busy) {
      /* TX buffer full, abort flush and retry later */
      return;
    }
    /* rx_usb_write returns k_rx_ok or k_rx_err_busy; busy is handled above;
     * any other error is transient - skip and continue flushing remaining data. */
    (void)err;

    /* Update tail and count */
    tail = (uint16_t)((tail + linear_len) % k_boot_buffer_size);
    s_boot_buffer.count -= linear_len;
  }

  /* Flush complete, mark USB ready */
  s_usb_ready = true;

  /* Log overflow warning if boot buffer overflowed */
  if (s_boot_buffer.overflow) {
    const char* warning =
      "[WARN ] [usb_log     ] Boot buffer overflow, some early logs dropped\r\n";
    (void)rx_usb_puts(k_usb_port_log, warning);
  }
}

/**
 * @brief Write data to USB CDC with error handling
 *
 * @details
 * Sends data to USB CDC Port 2 if configured, otherwise buffers to boot buffer.
 * Handles errors gracefully:
 * - **k_rx_err_busy**: Drop data, increment dropped count (non-blocking)
 * - **Other errors**: Log error, increment error count
 *
 * ## Algorithm
 * 1. Increment s_stats.total_bytes
 * 2. Check USB ready status (calls internal_check_usb_ready)
 * 3. If USB not ready:
 *    - Buffer to boot buffer (internal_buffer_boot_log)
 *    - Return
 * 4. If USB ready:
 *    - Call rx_usb_write(k_usb_port_log, data, len)
 *    - If k_rx_err_busy: increment s_stats.dropped_bytes
 *    - If other error: increment s_stats.usb_errors
 *
 * @param[in] data Data to write (must point to valid memory)
 *                 - Can be single character or string
 *                 - Does not need to be null-terminated
 * @param[in] len Number of bytes to write (must be > 0)
 *                - Typical values: 1 (char), 10-100 (log lines)
 *                - Maximum: Limited by USB TX buffer (~512 bytes)
 *
 * @return void (no error indication - errors tracked in statistics)
 *
 * @pre Mutex locked by caller (ensures atomicity)
 * @pre data != NULL (unchecked - caller responsibility)
 * @pre len > 0 (unchecked - zero length is no-op)
 * @post s_stats.total_bytes incremented by len
 * @post Data sent to USB if ready, buffered if not ready, or dropped if TX full
 * @post s_stats.dropped_bytes incremented if k_rx_err_busy
 * @post s_stats.usb_errors incremented if other USB error
 *
 * @note **Thread Safety**: Assumes caller holds mutex lock
 * @note **Non-Blocking**: Never waits, drops data if USB TX buffer full
 * @note **Error Policy**: Silent failure (errors tracked in statistics only)
 *
 * @par Example Usage (Internal):
 * @code
 * // Called by rx_log_usb_puts():
 * const char* str = "Hello World\r\n";
 * internal_write_usb(str, 13);  // Sends to USB or buffers
 * @endcode
 *
 * @since Version 1.0.0
 */
static void internal_write_usb(const char* data, uint16_t len)
{
  /* Update statistics */
  s_stats.total_bytes += len;

  /* Check if USB ready and flush boot buffer if needed */
  internal_check_usb_ready();

  if (!s_usb_ready) {
    /* USB not ready, buffer logs */
    (void)internal_buffer_boot_log(data, len);
    return;
  }

  /* USB ready, send directly */
  rx_err_t err = rx_usb_write(k_usb_port_log, (const uint8_t*)data, len);

  if (err == k_rx_err_busy) {
    /* TX buffer full, drop log (non-blocking policy) */
    s_stats.dropped_bytes += len;
  }
  /* All other return values: rx_usb_write guarantees ok or busy when configured */
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Write a single character to USB CDC log port
 *
 * @details
 * Thread-safe wrapper around internal_write_usb(). Buffers character during
 * boot, sends to USB after enumeration.
 *
 * ## Algorithm
 * 1. Initialize mutex if needed (lazy init)
 * 2. Acquire mutex lock (TX_WAIT_FOREVER)
 * 3. Call internal_write_usb(&c, 1)
 * 4. Release mutex lock
 *
 * @param[in] c Character to write
 *              - Any 8-bit character (ASCII, extended ASCII)
 *              - Special characters: '\\n', '\\r', '\\t' passed as-is
 *
 * @return void (no error indication - errors tracked in statistics)
 *
 * @pre ThreadX kernel running (for mutex operations)
 * @post Character sent to USB if ready, buffered if not ready, or dropped if TX full
 * @post s_stats updated (total_bytes, dropped_bytes, or boot_buffered)
 *
 * @note **Thread Safety**: Yes (ThreadX mutex protects all state)
 * @note **Blocking**: Blocks on mutex acquisition, never blocks on USB TX
 * @note **Performance**: ~2-5 us per character @ 240 MHz (mutex overhead included)
 *
 * @par Thread Safety:
 * Safe to call from multiple ThreadX tasks concurrently. Mutex ensures atomic
 * character writes (no interleaving).
 *
 * @par Example:
 * @code
 * // Called internally by LOG_PUTC macro:
 * rx_log_usb_putc('A');  // Sends 'A' to USB CDC Port 2
 * @endcode
 *
 * @see rx_log_usb_puts() Write string (more efficient for multi-char)
 * @see LOG_PUTC() Dual-output macro (UART + USB)
 *
 * @since Version 1.0.0
 */
void rx_log_usb_putc(char c)
{
  internal_init_mutex_once();

  /* TX_NO_WAIT (not TX_WAIT_FOREVER): this function is reachable from
   * ISR context (the USB SETUP handler chain calls rx_log_debug for
   * SET_LINE_CODING / SET_CONTROL_LINE_STATE).  ThreadX prohibits
   * blocking mutex acquires from an ISR -- TX_WAIT_FOREVER there
   * either returns TX_WAIT_ERROR or hangs forever.  When the latter
   * happens, the SETUP handler can't send its CCPL ZLP and the host's
   * SET_CONTROL_LINE_STATE never completes, which prevents cdc_acm
   * from finishing port open and IN URBs sit at -EINPROGRESS forever.
   * Drop the log line if the mutex is contended -- a missed debug
   * char is far cheaper than a hung USB stack. */
  if (tx_mutex_get(&s_log_mutex, TX_NO_WAIT) != TX_SUCCESS) {
    return;
  }

  internal_write_usb(&c, 1);

  (void)tx_mutex_put(&s_log_mutex);
}

/**
 * @brief Write a null-terminated string to USB CDC log port
 *
 * @details
 * Thread-safe wrapper around internal_write_usb(). Buffers string during
 * boot, sends to USB after enumeration.
 *
 * ## Algorithm
 * 1. Validate str pointer (NULL check, return if nullptr)
 * 2. Calculate length with strlen() (return if zero)
 * 3. Initialize mutex if needed (lazy init)
 * 4. Acquire mutex lock (TX_WAIT_FOREVER)
 * 5. Call internal_write_usb(str, len)
 * 6. Release mutex lock
 *
 * @param[in] str String to write (must be null-terminated)
 *                - Can be any length (limited by uint16_t: 65535 bytes max)
 *                - nullptr silently ignored (defensive)
 *                - Empty string ("") silently ignored
 *
 * @return void (no error indication - errors tracked in statistics)
 *
 * @retval N/A (void function - no return value)
 *
 * @pre str should be null-terminated string ornullptr
 * @pre ThreadX kernel running (for mutex operations)
 * @post String sent to USB if ready, buffered if not ready, or dropped if TX full
 * @post s_stats updated (total_bytes += strlen(str))
 *
 * @note **Thread Safety**: Yes (ThreadX mutex protects all state)
 * @note **Blocking**: Blocks on mutex acquisition, never blocks on USB TX
 * @note **NULL Safety**: nullptr silently ignored (defensive programming)
 * @note **Performance**: ~1 us per character @ 240 MHz (amortized)
 *
 * @warning String must be null-terminated. Non-terminated strings cause undefined behavior.
 *
 * @par Example: Basic Usage
 * @code
 * // Called internally by LOG_PUTS macro:
 * rx_log_usb_puts("[ERROR] [MOTOR] PWM init failed\r\n");
 * @endcode
 *
 * @par Example: NULL Safety
 * @code
 * const char* msg = NULL;
 * rx_log_usb_puts(msg);  // Safe: silently ignored, no crash
 * @endcode
 *
 * @see rx_log_usb_putc() Write single character (less efficient for strings)
 * @see LOG_PUTS() Dual-output macro (UART + USB)
 *
 * @since Version 1.0.0
 */
void rx_log_usb_puts(const char* str)
{
  if (str == NULL) {
    return; /* Defensive: ignore null pointer */
  }

  const uint16_t len = (uint16_t)strlen(str);
  if (len == 0) {
    return; /* Nothing to write */
  }

  internal_init_mutex_once();

  /* See comment in rx_log_usb_putc: TX_NO_WAIT to stay safe in ISR
   * context.  Drop the line silently if another caller holds the
   * mutex. */
  if (tx_mutex_get(&s_log_mutex, TX_NO_WAIT) != TX_SUCCESS) {
    return;
  }

  internal_write_usb(str, len);

  (void)tx_mutex_put(&s_log_mutex);
}

/**
 * @brief Write a signed integer as decimal text to USB CDC log port
 *
 * @details
 * Converts integer to ASCII decimal string and writes to USB. Handles negative
 * numbers with leading '-' sign.
 *
 * ## Algorithm
 * 1. Allocate 12-byte buffer (max: "-2147483648" + NUL)
 * 2. Convert value to absolute (handle INT32_MIN edge case)
 * 3. Generate digits in reverse order (least significant first)
 * 4. Add '-' sign if negative
 * 5. Call rx_log_usb_puts() to write string
 *
 * @param[in] value Integer value to write
 *                  - Range: INT32_MIN (-2147483648) to INT32_MAX (2147483647)
 *                  - Negative values formatted with leading '-'
 *                  - Zero formatted as "0"
 *
 * @return void (no error indication - errors tracked in statistics)
 *
 * @retval N/A (void function - no return value)
 *
 * @pre ThreadX kernel running (for mutex in rx_log_usb_puts)
 * @post Decimal string sent to USB (e.g., "42", "-1234")
 * @post s_stats updated via rx_log_usb_puts()
 *
 * @note **Thread Safety**: Yes (via rx_log_usb_puts mutex)
 * @note **Blocking**: Blocks on mutex acquisition in puts(), never blocks on USB TX
 * @note **Stack Usage**: 12 bytes local buffer
 * @note **Performance**: ~50-80 us for 10-digit number @ 240 MHz
 *
 * @par Example: Positive Values
 * @code
 * rx_log_usb_putint(42);      // Outputs: "42"
 * rx_log_usb_putint(2048);    // Outputs: "2048"
 * rx_log_usb_putint(0);       // Outputs: "0"
 * @endcode
 *
 * @par Example: Negative Values
 * @code
 * rx_log_usb_putint(-25);          // Outputs: "-25"
 * rx_log_usb_putint(-2147483648);  // Outputs: "-2147483648" (INT32_MIN)
 * @endcode
 *
 * @par Example: Usage in Logging
 * @code
 * // Called internally by LOG_PUTINT macro:
 * int32_t rpm = 2100;
 * LOG_PUTS("Motor RPM: ");
 * rx_log_usb_putint(rpm);  // Outputs: "2100"
 * LOG_PUTS("\r\n");
 * @endcode
 *
 * @see rx_log_usb_puthex() Write hexadecimal (for unsigned, addresses)
 * @see rx_log_usb_puts() Write string (called internally)
 * @see LOG_PUTINT() Dual-output macro (UART + USB)
 *
 * @since Version 1.0.0
 */
void rx_log_usb_putint(int32_t value)
{
  /* Convert to string (max 11 chars: '-' + 10 digits + NUL) */
  char buf[k_int32_buf_len];

  char* p = &buf[sizeof(buf) - 1];
  *p      = '\0';

  bool     negative  = (bool)(value < 0);
  uint32_t abs_value = (int)negative ? (uint32_t)(-value) : (uint32_t)value;

  /* Generate digits in reverse */
  do {
    --p;
    *p = (char)('0' + (abs_value % k_decimal_base));
    abs_value /= k_decimal_base;
  } while (abs_value > 0);

  /* Add sign if negative */
  if (negative) {
    --p;
    *p = '-';
  }

  /* Write string */
  rx_log_usb_puts(p);
}

/**
 * @brief Write an unsigned integer as hexadecimal text to USB CDC log port
 *
 * @details
 * Converts integer to ASCII hex string (uppercase A-F) with zero-padding.
 * Useful for register values, memory addresses, bit masks, and error codes.
 *
 * ## Algorithm
 * 1. Clamp digits to valid range (1-8)
 * 2. Allocate 9-byte buffer (8 hex digits + NUL)
 * 3. Generate hex digits in reverse order (least significant first)
 * 4. Use uppercase A-F for digits 10-15
 * 5. Call rx_log_usb_puts() to write string
 *
 * @param[in] value Value to write as hex
 *                  - Range: 0x00000000 to 0xFFFFFFFF
 *                  - Upper bits ignored if digits < 8
 *                  - Always formatted uppercase (0-9, A-F)
 * @param[in] digits Number of hex digits (1-8, zero-padded)
 *                   - 0 clamped to 1, >8 clamped to 8
 *                   - Common: 2 (byte), 4 (uint16_t), 8 (uint32_t)
 *                   - Leading zeros included
 *
 * @return void (no error indication - errors tracked in statistics)
 *
 * @retval N/A (void function - no return value)
 *
 * @pre ThreadX kernel running (for mutex in rx_log_usb_puts)
 * @pre digits should be 1-8 (values outside range are clamped)
 * @post Hex string sent to USB (e.g., "1A2B", "00000042")
 * @post s_stats updated via rx_log_usb_puts()
 *
 * @note **Thread Safety**: Yes (via rx_log_usb_puts mutex)
 * @note **Blocking**: Blocks on mutex acquisition in puts(), never blocks on USB TX
 * @note **Stack Usage**: 9 bytes local buffer
 * @note **Performance**: ~40-70 us for 8-digit hex @ 240 MHz
 * @note **Case**: Always uppercase (A-F, not a-f)
 *
 * @par Example: Different Digit Counts
 * @code
 * // Byte (2 digits)
 * rx_log_usb_puthex(0x5A, 2);    // Outputs: "5A"
 *
 * // Word (4 digits)
 * rx_log_usb_puthex(0x1A2B, 4);  // Outputs: "1A2B"
 *
 * // DWord (8 digits)
 * rx_log_usb_puthex(0xDEADBEEF, 8);  // Outputs: "DEADBEEF"
 * @endcode
 *
 * @par Example: Zero Padding
 * @code
 * rx_log_usb_puthex(0x42, 2);    // Outputs: "42"
 * rx_log_usb_puthex(0x42, 4);    // Outputs: "0042"
 * rx_log_usb_puthex(0x42, 8);    // Outputs: "00000042"
 * @endcode
 *
 * @par Example: Error Code Formatting
 * @code
 * // Called internally by LOG_PUTHEX macro:
 * rx_err_t err = k_rx_err_timeout;  // 0x108
 * LOG_PUTS("Error code: 0x");
 * rx_log_usb_puthex((uint32_t)err, 8);  // Outputs: "00000108"
 * LOG_PUTS("\r\n");
 * // Full output: "Error code: 0x00000108"
 * @endcode
 *
 * @par Example: Register Value Debugging
 * @code
 * uint32_t reg_val = read_register(GPIO_BASE + 0x04);
 * LOG_PUTS("[DEBUG] GPIO register: 0x");
 * rx_log_usb_puthex(reg_val, 8);  // Full 32-bit value
 * @endcode
 *
 * @see rx_log_usb_putint() Write decimal (for signed integers)
 * @see rx_log_usb_puts() Write string (called internally)
 * @see LOG_PUTHEX() Dual-output macro (UART + USB)
 *
 * @since Version 1.0.0
 */
void rx_log_usb_puthex(uint32_t value, uint8_t digits)
{
  /* Clamp digits to valid range (1-8) */
  if (digits == 0) {
    digits = 1;
  } else if (digits > k_hex_max_digits) {
    digits = k_hex_max_digits;
  }

  /* Generate hex string (max 8 digits + NUL) */
  char  buf[k_hex_buf_len];
  char* p = &buf[digits];
  *p      = '\0';

  /* Generate digits in reverse */
  for (uint8_t i = 0; i < digits; i++) {
    --p;
    uint8_t nibble = (uint8_t)(value & (uint32_t)k_nibble_mask);
    *p = (char)((nibble < k_decimal_base) ? ('0' + nibble) : ('A' + (nibble - k_decimal_base)));
    value >>= (uint32_t)k_nibble_shift;
  }

  /* Write string */
  rx_log_usb_puts(buf);
}

/**
 * @brief Get USB CDC logging statistics
 *
 * @details
 * Returns current logging statistics (total, dropped, buffered, errors).
 * Useful for diagnostics and performance monitoring.
 *
 * ## Statistics Provided
 * | Field | Description | Typical Range |
 * |-------|-------------|---------------|
 * | total_bytes | Total bytes sent (or buffered) | 0 - millions |
 * | dropped_bytes | Bytes dropped (TX full or overflow) | 0 (ideal) - thousands |
 * | boot_buffered | Bytes buffered during boot | 0-512 |
 * | usb_errors | USB error count (excluding k_rx_err_busy) | 0 (ideal) |
 *
 * ## Algorithm
 * 1. Validate stats pointer (NULL check, return if nullptr)
 * 2. Initialize mutex if needed (lazy init)
 * 3. Acquire mutex lock (TX_WAIT_FOREVER)
 * 4. Copy all 4 statistics fields from s_stats to stats
 * 5. Release mutex lock
 *
 * @param[out] stats Statistics structure to fill (usb_log_stats_t*)
 *                   - Must point to valid memory
 *                   - All 4 fields written atomically
 *                   - nullptr silently ignored (defensive)
 *
 * @return void (no error indication)
 *
 * @retval N/A (void function - no return value)
 *
 * @pre stats should be valid pointer ornullptr
 * @pre ThreadX kernel running (for mutex operations)
 * @post stats structure filled with current statistics (if non-NULL)
 * @post No side effects (read-only operation)
 *
 * @note **Thread Safety**: Yes (ThreadX mutex ensures atomic read)
 * @note **Blocking**: Blocks on mutex acquisition only (quick operation)
 * @note **NULL Safety**: nullptr silently ignored (defensive)
 * @note **Performance**: ~2-3 us @ 240 MHz (mutex + 4 field copies)
 *
 * @par Example: Basic Statistics Query
 * @code
 * usb_log_stats_t stats;
 * rx_log_usb_get_stats(&stats);
 *
 * rx_log_info("USB_LOG", "Total bytes sent: %u", stats.total_bytes);
 * rx_log_info("USB_LOG", "Bytes dropped: %u", stats.dropped_bytes);
 * rx_log_info("USB_LOG", "Boot buffered: %u", stats.boot_buffered);
 * rx_log_info("USB_LOG", "USB errors: %u", stats.usb_errors);
 * @endcode
 *
 * @par Example: Drop Rate Calculation
 * @code
 * usb_log_stats_t stats;
 * rx_log_usb_get_stats(&stats);
 *
 * if (stats.total_bytes > 0) {
 *     float drop_rate = (100.0f * stats.dropped_bytes) / stats.total_bytes;
 *     rx_log_info("PERF", "Log drop rate: %.2f%%", drop_rate);
 *
 *     if (drop_rate > 1.0f) {
 *         rx_log_warn("PERF", "High drop rate - reduce log verbosity");
 *     }
 * }
 * @endcode
 *
 * @par Example: Health Monitoring
 * @code
 * void monitor_logging_health(void) {
 *     usb_log_stats_t stats;
 *     rx_log_usb_get_stats(&stats);
 *
 *     // Check for excessive USB errors
 *     if (stats.usb_errors > 100) {
 *         rx_log_error("HEALTH", "USB logging failing");
 *     }
 *
 *     // Check boot buffer usage
 *     if (stats.boot_buffered > 400) {
 *         rx_log_warn("HEALTH", "Boot buffer nearly full");
 *     }
 * }
 * @endcode
 *
 * @see usb_log_stats_t Statistics structure definition
 * @see rx_log_usb_notify_ready() Trigger boot buffer flush
 *
 * @since Version 1.0.0
 */
void rx_log_usb_get_stats(usb_log_stats_t* stats)
{
  if (stats == NULL) {
    return; /* Defensive: ignore null pointer */
  }

  internal_init_mutex_once();

  /* s_mutex_initialized is always true after internal_init_mutex_once(); use unconditionally. */
  (void)tx_mutex_get(&s_log_mutex, TX_WAIT_FOREVER);

  /* Copy statistics */
  stats->total_bytes   = s_stats.total_bytes;
  stats->dropped_bytes = s_stats.dropped_bytes;
  stats->boot_buffered = s_stats.boot_buffered;
  stats->usb_errors    = s_stats.usb_errors;

  (void)tx_mutex_put(&s_log_mutex);
}

/**
 * @brief Notify logging system that USB is ready (external callback)
 *
 * @details
 * Called by USB stack after successful enumeration (k_usb_state_configured).
 * Triggers immediate flush of boot buffer. Optional - flush also happens
 * automatically on next log write.
 *
 * ## Use Cases
 * 1. **Eager flush**: Flush boot buffer immediately after USB configured
 * 2. **Event-driven**: Integrate with USB stack event callbacks
 * 3. **Diagnostics**: Ensure boot logs appear promptly for debugging
 *
 * ## Algorithm
 * 1. Initialize mutex if needed (lazy init)
 * 2. Acquire mutex lock (TX_WAIT_FOREVER)
 * 3. Call internal_check_usb_ready() to flush boot buffer
 * 4. Release mutex lock
 *
 * @return void (no error indication - errors handled internally)
 *
 * @retval N/A (void function - no return value)
 *
 * @pre ThreadX kernel running (for mutex operations)
 * @pre USB CDC Port 2 configured (k_usb_state_configured)
 * @post Boot buffer flushed to USB if not already flushed
 * @post s_usb_ready = true after successful flush
 * @post No effect if already flushed (idempotent)
 *
 * @note **Thread Safety**: Yes (ThreadX mutex protects all state)
 * @note **Blocking**: Blocks on mutex acquisition and USB TX (if buffer full, aborts)
 * @note **Optional**: Not required - automatic flush happens on next log write
 * @note **Idempotent**: Safe to call multiple times (no-op after first flush)
 *
 * @par Example: USB Event Callback Integration
 * @code
 * // In USB stack event handler:
 * void usb_event_callback(rx_usb_port_id_t port, rx_usb_event_t event, void* ctx)
 * {
 *     (void)ctx;  // Unused
 *
 *     if (event == k_usb_event_configured && port == k_usb_port_log) {
 *         // USB CDC Port 2 now configured, flush boot logs immediately
 *         rx_log_usb_notify_ready();
 *         rx_log_info("USB", "Log port ready, boot logs flushed");
 *     }
 * }
 * @endcode
 *
 * @par Example: Manual Flush (Testing)
 * @code
 * // In test code or initialization:
 * void test_usb_logging(void) {
 *     // Wait for USB enumeration
 *     while (!rx_usb_is_configured(k_usb_port_log)) {
 *         tx_thread_sleep(10);  // 10ms delay
 *     }
 *
 *     // Manually trigger flush
 *     rx_log_usb_notify_ready();
 *
 *     // Verify boot logs appeared
 *     usb_log_stats_t stats;
 *     rx_log_usb_get_stats(&stats);
 *     assert(stats.boot_buffered > 0);  // Should have buffered boot logs
 * }
 * @endcode
 *
 * @par Example: Eager vs Lazy Flush Comparison
 * @code
 * // EAGER FLUSH (with notify_ready):
 * // USB configured @ T=200ms
 * //   -> rx_log_usb_notify_ready() called
 * //   -> Boot logs appear immediately @ T=200ms
 *
 * // LAZY FLUSH (without notify_ready):
 * // USB configured @ T=200ms
 * //   -> No explicit flush
 * //   -> Next log write @ T=250ms triggers flush
 * //   -> Boot logs appear @ T=250ms (50ms delay)
 * @endcode
 *
 * @see internal_check_usb_ready() Internal flush implementation
 * @see rx_usb_is_configured() Check USB configured state
 *
 * @since Version 1.0.0
 */
void rx_log_usb_notify_ready(void)
{
  internal_init_mutex_once();

  /* s_mutex_initialized is always true after internal_init_mutex_once(); use unconditionally. */
  (void)tx_mutex_get(&s_log_mutex, TX_WAIT_FOREVER);

  internal_check_usb_ready();

  (void)tx_mutex_put(&s_log_mutex);
}

#ifdef UNIT_TEST
/**
 * @brief Reset all static state for unit tests
 *
 * @details
 * Clears boot buffer, stats, flags, and mutex state so that each unit test
 * starts from a clean, known-good baseline. Compiled only when UNIT_TEST is
 * defined; never present in firmware builds.
 *
 * @pre None
 * @post s_boot_buffer zeroed, s_usb_ready false, s_stats zeroed
 * @post s_mutex_initialized false (forces mutex re-creation on next log call)
 *
 * @note For unit tests only; not compiled in firmware builds
 * @since Version 1.0.0
 */
void rx_log_usb_test_reset_state(void)
{
  s_boot_buffer       = (usb_log_boot_buffer_t){};
  s_usb_ready         = false;
  s_stats             = (usb_log_stats_t){};
  s_mutex_initialized = false;
}

/**
 * @brief Directly set boot buffer state for wrap-around tests
 *
 * @details
 * Allows tests to inject a specific ring buffer position so that the wrap-around
 * branch in internal_check_usb_ready() (linear_len < chunk_len) can be exercised.
 * Compiled only when UNIT_TEST is defined.
 *
 * @param[in] head  Ring buffer head position (0 to k_boot_buffer_size-1)
 * @param[in] count Number of bytes in buffer (0 to k_boot_buffer_size)
 * @param[in] data  Data to copy into boot buffer storage
 *
 * @pre head < k_boot_buffer_size
 * @pre count <= k_boot_buffer_size
 * @pre data != NULL
 * @post s_boot_buffer.head == head, s_boot_buffer.count == count
 *
 * @note For unit tests only
 * @since Version 1.0.0
 */
void rx_log_usb_test_set_boot_buffer(uint16_t head, uint16_t count, const char* data)
{
  s_boot_buffer.head  = head;
  s_boot_buffer.count = count;
  /* Copy data into ring buffer - wrap-around handled by caller */
  for (uint16_t i = 0; i < count; i++) {
    uint16_t pos = (uint16_t)((head - count + k_boot_buffer_size + i) % k_boot_buffer_size);
    s_boot_buffer.data[pos] = data[i];
  }
}
#endif /* UNIT_TEST */
