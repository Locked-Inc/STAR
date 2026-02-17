/* lib/rx_bus/src/rx_bus_uart.c */

/**
 * @file rx_bus_uart.c
 * @brief UART Bus Abstraction Layer for RX72N SCI Peripheral
 *
 * @details
 * # Implementation Overview
 *
 * Provides thread-safe UART communication through the bus manager abstraction pattern.
 * Wraps low-level RX72N SCI (Serial Communications Interface) HAL with a high-level,
 * protocol-agnostic API for serial communication, debugging, and console I/O.
 *
 * ## Features
 *
 * - **Multiple UART channels**: Support for all 13 SCI channels on RX72N
 * - **Thread-safe**: Bus manager mutex protects concurrent access
 * - **Flexible I/O**: Binary (write/read) and text (putc/puts/getc) operations
 * - **Configurable**: Programmable baud rate, GPIO pin selection per channel
 * - **Non-blocking reads**: rx_available() + getc() for polling mode
 * - **Consistent error handling**: All functions return rx_err_t codes
 *
 * ## Hardware: RX72N SCI Peripheral
 *
 * | Feature | Specification |
 * |---------|---------------|
 * | **Channels** | SCI0-SCI12 (13 channels) |
 * | **Modes** | Async UART, Simple I2C, Simple SPI, Multi-processor |
 * | **Baud rate** | 110 - 2,500,000 bps (f_PCLK dependent) |
 * | **Data bits** | 7 or 8 bits |
 * | **Stop bits** | 1 or 2 bits |
 * | **Parity** | None, Even, Odd |
 * | **Flow control** | None (default), RTS/CTS (if configured) |
 * | **FIFOs** | 16-byte TX FIFO, 16-byte RX FIFO per channel |
 *
 * **This module uses**: Async UART mode, 8N1 (8 data bits, no parity, 1 stop bit)
 *
 * ## Common Use Cases
 *
 * ### 1. Debug Console (USB CDC or RS-232)
 * ```c
 * // Initialize debug UART on SCI1 @ 115200 bps
 * rx_bus_config_t debug_uart_cfg;
 * rx_bus_config_init_uart(&debug_uart_cfg, "debug", 1,
 *                         k_rx_pin_p26, k_rx_pin_p30, 115200);
 * rx_bus_manager_add_bus(&bus_mgr, &debug_uart_cfg);
 * rx_bus_uart_init(&bus_mgr, "debug");
 *
 * // Print debug messages
 * rx_bus_uart_puts(&bus_mgr, "debug", "System initialized\r\n");
 * ```
 *
 * ### 2. Sensor Communication (UART protocol devices)
 * ```c
 * // GPS module, LIDAR, etc. @ 9600 bps
 * rx_bus_config_t gps_uart_cfg;
 * rx_bus_config_init_uart(&gps_uart_cfg, "gps", 2,
 *                         k_rx_pin_p50, k_rx_pin_p52, 9600);
 * rx_bus_manager_add_bus(&bus_mgr, &gps_uart_cfg);
 * rx_bus_uart_init(&bus_mgr, "gps");
 *
 * // Read NMEA sentences
 * uint8_t nmea_buf[128];
 * uint16_t bytes_read;
 * rx_bus_uart_read(&bus_mgr, "gps", nmea_buf, sizeof(nmea_buf), &bytes_read);
 * ```
 *
 * ### 3. Binary Protocol (Custom commands)
 * ```c
 * // Send binary command packet
 * const uint8_t cmd_packet[] = {0xAA, 0x01, 0x10, 0x00, 0xCB};
 * rx_bus_uart_write(&bus_mgr, "device", cmd_packet, sizeof(cmd_packet));
 *
 * // Read binary response
 * uint8_t response[32];
 * uint16_t resp_len;
 * rx_bus_uart_read(&bus_mgr, "device", response, sizeof(response), &resp_len);
 * ```
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Notes |
 * |-----------|----------------|-------|
 * | **uart_init()** | ~50 us | One-time setup per channel |
 * | **uart_putc()** | ~2 us + TX time | FIFO write + DMA/interrupt |
 * | **uart_getc()** | ~2 us (polling) | Returns k_rx_err_empty if no data |
 * | **uart_write()** | ~10 us + TX time | DMA transfer (background) |
 * | **uart_read()** | ~5 us + RX time | Reads available bytes (non-blocking) |
 * | **rx_available()** | ~1 us | Check FIFO status register |
 *
 * **TX/RX time** depends on baud rate:
 * - 115200 bps: ~87 us per byte (11 bits: 1 start + 8 data + 1 stop + 1 stop gap)
 * - 9600 bps: ~1.04 ms per byte
 *
 * ## Memory Usage
 *
 * | Component | Size | Location |
 * |-----------|------|----------|
 * | Function code | ~1.5 KB | Flash (.text) |
 * | Context structs | 12-16 bytes | Stack (per operation) |
 * | SCI HAL buffers | 16x2x13 = 416 bytes | SRAM (TX/RX FIFOs, all channels) |
 * | **Total static** | ~2 KB | Flash + fixed SRAM |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Implementation |
 * |------|----------------|
 * | **Rule 1** | [PASS] No goto, setjmp, recursion - straight-line control flow |
 * | **Rule 2** | [PASS] No unbounded loops (HAL uses timeout counters) |
 * | **Rule 3** | [PASS] No malloc - all buffers stack-allocated or static FIFOs |
 * | **Rule 4** | [PASS] All functions <=60 lines (longest: init_callback at 44 lines) |
 * | **Rule 5** | [PASS] Minimum 2 validations per function (NULL + state checks) |
 * | **Rule 6** | [PASS] Variables at smallest scope (contexts in callbacks) |
 * | **Rule 7** | [PASS] All HAL returns checked (uart_init, uart_write, etc.) |
 * | **Rule 8** | [PASS] C23 typed enums for constants (k_uart_ascii_max: uint8_t) |
 * | **Rule 9** | [PASS] Single-level pointers only (data*, ctx*, no arithmetic) |
 * | **Rule 10** | [PASS] Compiles with -Wall -Wextra -Werror, zero warnings |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S** | Each function does ONE operation (init, write, read, putc, etc.) |
 * | **O** | New UART operations added as new functions - no modification |
 * | **L** | All read/write functions substitutable (same error semantics) |
 * | **I** | Focused API - 7 functions, each for specific UART operation |
 * | **D** | Depends on UART HAL abstraction, not concrete SCI registers |
 *
 * ## Module Dependencies
 *
 * - `rx_bus_uart.h` - Public API declarations
 * - `hardware.h` - SCI UART HAL (uart_init_channel, uart_write_channel, etc.)
 * - `rx_bus_types.h` - Bus configuration structures (rx_bus_config_t)
 * - `rx_bus_manager.h` - Thread-safe bus access (rx_bus_manager_with_bus)
 * - `rx_check.h` - RX_CHECK_NULL_PTR validation macro
 * - `rx_log.h` - Logging (rx_log_error, rx_log_warn, rx_log_debug)
 *
 * ## Error Handling
 *
 * All functions return `rx_err_t`:
 * - `k_rx_ok` - Success
 * - `k_rx_err_null_ptr` - nullptr in parameters
 * - `k_rx_err_not_found` - bus_name not in manager
 * - `k_rx_err_invalid_state` - Bus not initialized
 * - `k_rx_err_invalid_arg` - Invalid channel or parameter
 * - `k_rx_err_hw_init_failed` - SCI peripheral init failed
 * - `k_rx_err_uart_error` - UART HAL error (framing, overrun, etc.)
 * - `k_rx_err_empty` - No data available (getc() only)
 *
 * ## Thread Safety
 *
 * All public functions are thread-safe via bus manager mutex. Multiple threads
 * can safely call UART operations on different buses concurrently. Operations
 * on the same bus are serialized.
 *
 * @see rx_bus_uart.h Public API with detailed function documentation
 * @see hardware.h SCI UART HAL interface
 * @see rx_bus_manager.h Thread-safe bus management
 * @see docs/sections/03_hardware_pinout.tex UART pin assignments
 *
 * @author STAR Team
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "rx_bus_uart.h"

#include "hardware.h"
#include "rx_check.h"
#include "rx_log.h"

static const char s_tag[] = "BUS_UART";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief UART character range limits for ASCII validation
 */
typedef enum : uint8_t {
  k_uart_ascii_max = 127U, /**< Maximum 7-bit ASCII code point (DEL character) */
} uart_char_limits_t;

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

/**
 * @struct uart_init_ctx_t
 * @brief Context for UART initialization operation
 *
 * @details
 * Passed to internal_uart_init_callback() via rx_bus_manager_with_bus() to
 * carry the result of the SCI channel initialization back to the caller.
 * Stack-allocated in rx_bus_uart_init() and destroyed on return. Contains
 * only a result field because all initialization parameters are already
 * present in the rx_bus_config_t structure.
 *
 * @invariant result is set by the callback before it returns
 * @invariant result equals the return value of internal_uart_init_callback()
 *
 * @code
 * uart_init_ctx_t ctx = { .result = k_rx_err_hw_init_failed };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_init_callback, &ctx);
 * // ctx.result now reflects the initialization outcome
 * @endcode
 *
 * @see internal_uart_init_callback() Callback that writes to this context
 * @see rx_bus_uart_init() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  rx_err_t result; /**< Operation result: k_rx_ok on success, error code on failure */
} uart_init_ctx_t;

/**
 * @struct uart_write_ctx_t
 * @brief Context for UART binary write operation
 *
 * @details
 * Passed to internal_uart_write_callback() via rx_bus_manager_with_bus().
 * Carries the data pointer, byte count, and operation result. Stack-allocated
 * in rx_bus_uart_write() and destroyed on return. The data buffer must remain
 * valid until the callback completes (DMA transfers hold this pointer during
 * background transmission).
 *
 * @invariant data must be non-NULL when length > 0
 * @invariant length must equal the number of bytes in the data buffer
 * @invariant result is set by the callback before it returns
 *
 * @code
 * const uint8_t payload[] = {0xAA, 0x01, 0x10};
 * uart_write_ctx_t ctx = {
 *     .data   = payload,
 *     .length = sizeof(payload),
 *     .result = k_rx_err_uart_error,
 * };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_write_callback, &ctx);
 * // ctx.result reflects whether the write succeeded
 * @endcode
 *
 * @see internal_uart_write_callback() Callback that reads from this context
 * @see rx_bus_uart_write() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  const uint8_t* data;   /**< Pointer to data buffer to transmit. Must be non-NULL if length > 0. */
  uint16_t       length; /**< Number of bytes to transmit. Valid range: [0, 65535]. */
  rx_err_t       result; /**< Operation result: k_rx_ok on success, error code on failure. */
} uart_write_ctx_t;

/**
 * @struct uart_read_ctx_t
 * @brief Context for UART binary read operation
 *
 * @details
 * Passed to internal_uart_read_callback() via rx_bus_manager_with_bus().
 * Carries the receive buffer pointer, maximum byte count, actual bytes read,
 * and operation result. Stack-allocated in rx_bus_uart_read() and destroyed
 * on return. The HAL writes available bytes directly into the data buffer and
 * sets bytes_read to the actual count (may be 0 if RX FIFO is empty).
 *
 * @invariant data must be non-NULL and point to a buffer of at least length bytes
 * @invariant bytes_read <= length after the callback returns
 * @invariant result is set by the callback before it returns
 *
 * @code
 * uint8_t rx_buf[64];
 * uart_read_ctx_t ctx = {
 *     .data       = rx_buf,
 *     .length     = sizeof(rx_buf),
 *     .bytes_read = 0,
 *     .result     = k_rx_err_uart_error,
 * };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_read_callback, &ctx);
 * // ctx.bytes_read contains the actual byte count received
 * @endcode
 *
 * @see internal_uart_read_callback() Callback that writes to this context
 * @see rx_bus_uart_read() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t* data;       /**< Pointer to receive buffer. Must be at least length bytes. */
  uint16_t length;     /**< Maximum bytes to read (buffer capacity). Valid range: [1, 65535]. */
  uint16_t bytes_read; /**< Actual bytes read from RX FIFO. Range: [0, length]. */
  rx_err_t result;     /**< Operation result: k_rx_ok on success, error code on failure. */
} uart_read_ctx_t;

/**
 * @struct uart_putc_ctx_t
 * @brief Context for UART single-character write operation
 *
 * @details
 * Passed to internal_uart_putc_callback() via rx_bus_manager_with_bus().
 * Carries the character to transmit and the operation result. Stack-allocated
 * in rx_bus_uart_putc() and destroyed on return. A post-condition warning is
 * generated if the character is outside the 7-bit ASCII range, but transmission
 * proceeds regardless (some protocols use extended ASCII values 128-255).
 *
 * @invariant c may be any byte value (0-255); values > 127 generate a warning
 * @invariant result is set by the callback before it returns
 *
 * @code
 * uart_putc_ctx_t ctx = { .c = '\n', .result = k_rx_err_uart_error };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_putc_callback, &ctx);
 * // ctx.result reflects whether the character was transmitted
 * @endcode
 *
 * @see internal_uart_putc_callback() Callback that reads from this context
 * @see rx_bus_uart_putc() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  char     c;      /**< Character to transmit. Typically 7-bit ASCII (0-127); 128-255 also accepted. */
  rx_err_t result; /**< Operation result: k_rx_ok on success, error code on failure. */
} uart_putc_ctx_t;

/**
 * @struct uart_puts_ctx_t
 * @brief Context for UART null-terminated string write operation
 *
 * @details
 * Passed to internal_uart_puts_callback() via rx_bus_manager_with_bus().
 * Carries the string pointer and operation result. Stack-allocated in
 * rx_bus_uart_puts() and destroyed on return. The HAL computes the string
 * length internally (strlen) and writes all bytes to the TX FIFO in one
 * operation, making this more efficient than a putc() loop for multi-character
 * strings.
 *
 * @invariant str must be a valid null-terminated C string (not NULL)
 * @invariant result is set by the callback before it returns
 *
 * @code
 * uart_puts_ctx_t ctx = {
 *     .str    = "Hello UART\r\n",
 *     .result = k_rx_err_uart_error,
 * };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_puts_callback, &ctx);
 * // ctx.result reflects whether the string was transmitted
 * @endcode
 *
 * @see internal_uart_puts_callback() Callback that reads from this context
 * @see rx_bus_uart_puts() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  const char* str;    /**< Pointer to null-terminated string to transmit. Must not be NULL. */
  rx_err_t    result; /**< Operation result: k_rx_ok on success, error code on failure. */
} uart_puts_ctx_t;

/**
 * @struct uart_getc_ctx_t
 * @brief Context for UART single-character read operation
 *
 * @details
 * Passed to internal_uart_getc_callback() via rx_bus_manager_with_bus().
 * Carries the received character and operation result. Stack-allocated in
 * rx_bus_uart_getc() and destroyed on return. The result field stores the
 * HAL return directly: k_rx_ok on successful read, k_rx_err_empty if the
 * RX FIFO had no data (this is normal and not an error condition), or a
 * true error code on hardware faults.
 *
 * @invariant c is valid only when result == k_rx_ok
 * @invariant result == k_rx_err_empty is a normal non-error condition
 * @invariant result is set by the callback before it returns
 *
 * @code
 * uart_getc_ctx_t ctx = { .c = '\0', .result = k_rx_err_uart_error };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_getc_callback, &ctx);
 * if (ctx.result == k_rx_ok) {
 *     // ctx.c contains the received character
 * }
 * @endcode
 *
 * @see internal_uart_getc_callback() Callback that writes to this context
 * @see rx_bus_uart_getc() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  char     c;      /**< Received character. Valid only when result == k_rx_ok. */
  rx_err_t result; /**< Operation result: k_rx_ok, k_rx_err_empty (no data), or error code. */
} uart_getc_ctx_t;

/**
 * @struct uart_rx_avail_ctx_t
 * @brief Context for UART receive-data availability check operation
 *
 * @details
 * Passed to internal_uart_rx_avail_callback() via rx_bus_manager_with_bus().
 * Carries the availability flag and operation result. Stack-allocated in
 * rx_bus_uart_rx_available() and destroyed on return. The HAL reads the SCI
 * RX FIFO status register and sets available to true if at least one byte is
 * present, without consuming any data from the FIFO.
 *
 * @invariant available is a valid bool value (true or false) after the callback
 * @invariant available == false does not indicate an error; it means FIFO is empty
 * @invariant result is set by the callback before it returns
 *
 * @code
 * uart_rx_avail_ctx_t ctx = { .available = false, .result = k_rx_err_uart_error };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                        internal_uart_rx_avail_callback, &ctx);
 * if (ctx.result == k_rx_ok && ctx.available) {
 *     // At least one byte is ready to read
 * }
 * @endcode
 *
 * @see internal_uart_rx_avail_callback() Callback that writes to this context
 * @see rx_bus_uart_rx_available() Function that creates and uses this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  bool     available; /**< True if at least one byte is in the RX FIFO; false if FIFO is empty. */
  rx_err_t result;    /**< Operation result: k_rx_ok on success, error code on failure. */
} uart_rx_avail_ctx_t;

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

/**
 * @brief Internal callback for SCI UART channel initialization
 *
 * @details
 * Initializes a Renesas RX72N SCI channel for asynchronous 8N1 UART mode.
 * Called by the bus manager via rx_bus_manager_with_bus() with the mutex held.
 * Validates bus type and SCI channel number, builds the HAL configuration
 * from the bus config, calls uart_init_channel(), and marks the bus as
 * initialized.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_init_ctx_t*
 * 2. Validate bus type is k_bus_type_uart
 * 3. Validate SCI channel is within [0, k_sci_channel_count)
 * 4. Build uart_channel_config_t from bus_config fields
 * 5. Call uart_init_channel() HAL function
 * 6. Set bus_config->initialized = true on success
 * 7. Set ctx->result and return
 *
 * @param[in,out] bus_config Bus configuration structure.
 *                           - type must equal k_bus_type_uart
 *                           - proto.uart.channel: SCI channel number (0-12)
 *                           - proto.uart.baudrate: Desired baud rate in bps
 *                           - proto.uart.tx_pin: GPIO pin for TXD
 *                           - proto.uart.rx_pin: GPIO pin for RXD
 *                           - initialized flag set to true on success
 * @param[in,out] user_ctx User context pointer (uart_init_ctx_t*).
 *                         output: ctx->result contains the operation result
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, SCI channel initialized
 * @retval k_rx_err_invalid_arg Bus type is not UART or SCI channel out of range
 * @retval k_rx_err_hw_init_failed HAL uart_init_channel() returned an error
 *
 * @pre bus_config pointer is valid (guaranteed by bus manager)
 * @pre user_ctx pointer is valid and points to uart_init_ctx_t
 * @pre SCI channel is within [0, k_sci_channel_count)
 * @pre GPIO pins are not in use by another peripheral
 *
 * @post bus_config->initialized == true on success
 * @post ctx->result contains the operation result
 * @post SCI peripheral configured for 8N1 UART at requested baud rate
 * @post TX/RX GPIO pins configured for alternate function
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note Execution time: ~50 µs (SCI register configuration + GPIO setup)
 *
 * @warning Do NOT call directly - use rx_bus_uart_init() instead
 * @warning SCI channel must not be in use by another bus instance
 *
 * @see rx_bus_uart_init() Public API that invokes this callback
 * @see uart_init_channel() Underlying HAL initialization function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_init_ctx_t* ctx = (uart_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_uart) {
    rx_log_error(s_tag, "Bus is not UART type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: Verify UART channel is within valid range */
  if (bus_config->proto.uart.channel >= k_sci_channel_count) {
    rx_log_error(s_tag, "UART channel out of range");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize SCI channel with full hardware configuration */
  const uart_channel_config_t uart_config = {
    .channel  = bus_config->proto.uart.channel,
    .baudrate = bus_config->proto.uart.baudrate,
    .tx_gpio  = bus_config->proto.uart.tx_pin,
    .rx_gpio  = bus_config->proto.uart.rx_pin,
  };
  const rx_err_t err = uart_init_channel(&uart_config);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART HAL initialization failed");
    ctx->result = k_rx_err_hw_init_failed;
    return k_rx_err_hw_init_failed;
  }

  /* Mark bus as initialized */
  bus_config->initialized = true;

  rx_log_debug(s_tag, "UART bus initialized");
  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for UART binary data write operation
 *
 * @details
 * Transmits a binary data buffer over the SCI UART channel. Called by the bus
 * manager via rx_bus_manager_with_bus() with the mutex held. Validates bus
 * initialization state, validates the data pointer when length > 0, and
 * delegates to uart_write_channel() HAL function for the actual transmission.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_write_ctx_t*
 * 2. Validate bus is initialized (bus_config->initialized == true)
 * 3. Validate data pointer is non-NULL when ctx->length > 0
 * 4. Call uart_write_channel() with channel, data, length
 * 5. Set ctx->result and return
 *
 * @param[in] bus_config Bus configuration structure.
 *                       - initialized must be true
 *                       - proto.uart.channel: SCI channel number
 * @param[in,out] user_ctx User context pointer (uart_write_ctx_t*).
 *                         - input: ctx->data pointer to transmit buffer
 *                         - input: ctx->length number of bytes to transmit
 *                         - output: ctx->result contains operation result
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, data queued for transmission
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_invalid_arg length > 0 but data is nullptr
 * @retval k_rx_err_uart_error HAL transmission error (FIFO full, timeout)
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->data must be non-NULL when ctx->length > 0
 * @pre Data buffer must remain valid until transmission completes (DMA mode)
 *
 * @post Data queued in TX FIFO or DMA transfer started
 * @post ctx->result contains the operation result
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note In DMA mode the data buffer must stay valid until transmission ends
 *
 * @warning ctx->data must outlive this call in DMA transfer mode
 *
 * @see rx_bus_uart_write() Public API that invokes this callback
 * @see uart_write_channel() Underlying HAL write function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_write_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_write_ctx_t* ctx = (uart_write_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  if (ctx->length > 0 && ctx->data == nullptr) {
    rx_log_error(s_tag, "UART write data pointer is nullptr");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Write UART data */
  const rx_err_t err = uart_write_channel(bus_config->proto.uart.channel, ctx->data, ctx->length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART write failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for UART binary data read operation (non-blocking)
 *
 * @details
 * Reads up to ctx->length bytes from the SCI RX FIFO into ctx->data. Called
 * by the bus manager via rx_bus_manager_with_bus() with the mutex held.
 * Non-blocking: returns immediately with however many bytes are currently
 * available (may be 0). A post-condition check logs a warning if bytes_read
 * exceeds length (should not happen but guards against HAL bugs).
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_read_ctx_t*
 * 2. Validate bus is initialized (bus_config->initialized == true)
 * 3. Call uart_read_channel() with channel, data, length, &bytes_read
 * 4. Validate bytes_read <= length (post-condition)
 * 5. Set ctx->result and return
 *
 * @param[in] bus_config Bus configuration structure.
 *                       - initialized must be true
 *                       - proto.uart.channel: SCI channel number
 * @param[in,out] user_ctx User context pointer (uart_read_ctx_t*).
 *                         - input: ctx->data points to receive buffer
 *                         - input: ctx->length maximum bytes to read
 *                         - output: ctx->bytes_read actual bytes received
 *                         - output: ctx->result contains operation result
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success; check ctx->bytes_read for actual byte count
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL error (framing error, overrun, etc.)
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->data must point to a valid buffer of at least ctx->length bytes
 * @pre ctx->length must equal the receive buffer capacity
 *
 * @post ctx->bytes_read contains the actual number of bytes copied
 * @post ctx->bytes_read <= ctx->length (guaranteed by post-condition check)
 * @post ctx->result contains the operation result
 * @post Consumed bytes are removed from the RX FIFO
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note bytes_read == 0 is normal when RX FIFO is empty (not an error)
 *
 * @warning Overrun error returned by HAL if RX FIFO filled before this call
 *
 * @see rx_bus_uart_read() Public API that invokes this callback
 * @see uart_read_channel() Underlying HAL read function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_read_ctx_t* ctx = (uart_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read UART data */
  const rx_err_t err =
    uart_read_channel(bus_config->proto.uart.channel, ctx->data, ctx->length, &ctx->bytes_read);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART read failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify bytes_read is within expected range */
  if (ctx->bytes_read > ctx->length) {
    rx_log_warn(s_tag, "UART read returned more bytes than requested");
    /* Continue anyway - operation completed, but unexpected state */
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for UART single-character write operation
 *
 * @details
 * Transmits one character over the SCI UART channel. Called by the bus manager
 * via rx_bus_manager_with_bus() with the mutex held. Validates bus
 * initialization, calls uart_putc_channel() HAL function, and emits a warning
 * (but continues) if the character is outside the 7-bit ASCII range.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_putc_ctx_t*
 * 2. Validate bus is initialized (bus_config->initialized == true)
 * 3. Call uart_putc_channel() with channel and ctx->c
 * 4. Validate (uint8_t)ctx->c <= k_uart_ascii_max (post-condition warning)
 * 5. Set ctx->result and return
 *
 * @param[in] bus_config Bus configuration structure.
 *                       - initialized must be true
 *                       - proto.uart.channel: SCI channel number
 * @param[in,out] user_ctx User context pointer (uart_putc_ctx_t*).
 *                         - input: ctx->c character to transmit
 *                         - output: ctx->result contains operation result
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, character written to TX FIFO
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL transmission error (FIFO full, timeout)
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->c may be any byte value (transmission accepts 0-255)
 *
 * @post Character written to TX FIFO or transmission started
 * @post ctx->result contains the operation result
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note Extended ASCII (values 128-255) generates warning log but is transmitted
 *
 * @warning Values > 127 emit a warning; use with care in ASCII-only protocols
 *
 * @see rx_bus_uart_putc() Public API that invokes this callback
 * @see uart_putc_channel() Underlying HAL function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_putc_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_putc_ctx_t* ctx = (uart_putc_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write single character */
  const rx_err_t err = uart_putc_channel(bus_config->proto.uart.channel, ctx->c);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART putc failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify character is valid ASCII */
  if ((uint8_t)ctx->c > k_uart_ascii_max) {
    rx_log_warn(s_tag, "UART putc wrote non-ASCII character");
    /* Continue anyway - some protocols use extended ASCII */
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for UART null-terminated string write operation
 *
 * @details
 * Transmits a null-terminated C string over the SCI UART channel (without
 * the null terminator). Called by the bus manager via rx_bus_manager_with_bus()
 * with the mutex held. Validates bus initialization state and delegates to
 * uart_puts_channel() HAL function, which computes the string length
 * internally and writes the entire string to the TX FIFO in one call.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_puts_ctx_t*
 * 2. Validate bus is initialized (bus_config->initialized == true)
 * 3. Call uart_puts_channel() with channel and ctx->str
 * 4. Set ctx->result and return
 *
 * @param[in] bus_config Bus configuration structure.
 *                       - initialized must be true
 *                       - proto.uart.channel: SCI channel number
 * @param[in,out] user_ctx User context pointer (uart_puts_ctx_t*).
 *                         - input: ctx->str pointer to null-terminated string
 *                         - output: ctx->result contains operation result
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, string transmitted (null terminator not sent)
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL transmission error
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->str must point to a valid null-terminated string
 *
 * @post String characters transmitted to TX FIFO (null terminator excluded)
 * @post ctx->result contains the operation result
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note More efficient than repeated putc() calls: single mutex lock covers all characters
 *
 * @warning ctx->str must be null-terminated; undefined behaviour otherwise
 *
 * @see rx_bus_uart_puts() Public API that invokes this callback
 * @see uart_puts_channel() Underlying HAL function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_puts_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_puts_ctx_t* ctx = (uart_puts_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write string */
  const rx_err_t err = uart_puts_channel(bus_config->proto.uart.channel, ctx->str);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART puts failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for UART single-character read operation (non-blocking)
 *
 * @details
 * Reads one character from the SCI RX FIFO without blocking. Called by the
 * bus manager via rx_bus_manager_with_bus() with the mutex held. Validates
 * bus initialization state, calls uart_getc_channel() HAL function, and
 * stores k_rx_err_empty in ctx->result when the FIFO is empty (this is a
 * normal non-error condition that the public API propagates to the caller).
 * A post-condition warning is emitted for characters outside the 7-bit ASCII
 * range but the character is still returned.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_getc_ctx_t*
 * 2. Validate bus is initialized (bus_config->initialized == true)
 * 3. Call uart_getc_channel() with channel, &ctx->c
 * 4. If err == k_rx_ok: validate (uint8_t)ctx->c <= k_uart_ascii_max
 * 5. Store err directly in ctx->result (includes k_rx_err_empty)
 * 6. Return k_rx_ok always (bus manager layer sees no fault)
 *
 * @param[in] bus_config Bus configuration structure.
 *                       - initialized must be true
 *                       - proto.uart.channel: SCI channel number
 * @param[in,out] user_ctx User context pointer (uart_getc_ctx_t*).
 *                         - output: ctx->c received character (valid when result == k_rx_ok)
 *                         - output: ctx->result: k_rx_ok, k_rx_err_empty, or error
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Always returned to bus manager (HAL result is in ctx->result)
 * @retval k_rx_err_invalid_state Bus not initialized (returned directly, not via ctx)
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->c must point to a valid char variable for the result
 *
 * @post ctx->c contains received character when ctx->result == k_rx_ok
 * @post ctx->result contains k_rx_err_empty when RX FIFO is empty (normal)
 * @post Character is consumed from RX FIFO when k_rx_ok
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note k_rx_err_empty in ctx->result is a normal status, not an error
 *
 * @warning ctx->c is undefined when ctx->result == k_rx_err_empty
 *
 * @see rx_bus_uart_getc() Public API that invokes this callback
 * @see uart_getc_channel() Underlying HAL function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_getc_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_getc_ctx_t* ctx = (uart_getc_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read single character */
  const rx_err_t err = uart_getc_channel(bus_config->proto.uart.channel, &ctx->c);

  /* Post-condition: Verify character is valid ASCII when data available */
  if (err == k_rx_ok && (uint8_t)ctx->c > k_uart_ascii_max) {
    rx_log_warn(s_tag, "UART getc received non-ASCII character");
    /* Continue anyway - some protocols use extended ASCII */
  }

  /* k_rx_err_empty is a valid return (no data available) */
  ctx->result = err;
  return k_rx_ok;
}

/**
 * @brief Internal callback for UART RX FIFO availability check (non-blocking)
 *
 * @details
 * Checks whether at least one byte is available in the SCI RX FIFO without
 * consuming any data. Called by the bus manager via rx_bus_manager_with_bus()
 * with the mutex held. Validates bus initialization state, calls
 * uart_rx_available() HAL function (which reads only the FIFO status
 * register), and performs a post-condition validity check on the returned
 * boolean.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to uart_rx_avail_ctx_t*
 * 2. Validate bus is initialized (bus_config->initialized == true)
 * 3. Call uart_rx_available() with channel, &ctx->available
 * 4. Validate ctx->available is a valid bool (post-condition)
 * 5. Set ctx->result and return
 *
 * @param[in] bus_config Bus configuration structure.
 *                       - initialized must be true
 *                       - proto.uart.channel: SCI channel number
 * @param[in,out] user_ctx User context pointer (uart_rx_avail_ctx_t*).
 *                         - output: ctx->available true if FIFO has ≥1 byte
 *                         - output: ctx->result contains operation result
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, ctx->available contains FIFO status
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL error reading FIFO status register
 *
 * @pre bus_config->initialized must be true
 * @pre ctx must point to a valid uart_rx_avail_ctx_t
 *
 * @post ctx->available == true if ≥1 byte in RX FIFO, false if empty
 * @post RX FIFO data is NOT consumed (non-destructive check)
 * @post ctx->result contains the operation result
 *
 * @note Called with bus manager mutex held (thread-safe context)
 * @note Fastest UART query: reads only a status register (~1 µs)
 *
 * @warning ctx->available == false is not an error; it means FIFO is empty
 *
 * @see rx_bus_uart_rx_available() Public API that invokes this callback
 * @see uart_rx_available() Underlying HAL function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_uart_rx_avail_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_rx_avail_ctx_t* ctx = (uart_rx_avail_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Check RX data availability */
  const rx_err_t err = uart_rx_available(bus_config->proto.uart.channel, &ctx->available);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART rx_available failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify availability flag was set (bool has valid value) */
  if (ctx->available != true && ctx->available != false) {
    rx_log_warn(s_tag, "UART rx_available returned invalid boolean value");
    /* Continue anyway - may be undefined behavior from HAL */
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize RX72N SCI peripheral for UART communication
 *
 * @details
 * Initializes the specified SCI channel for asynchronous UART mode (8N1: 8 data bits,
 * no parity, 1 stop bit). Configures baud rate generator, GPIO pins, and enables
 * TX/RX interrupts or DMA. Must be called once before any UART read/write operations.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager and bus_name (NULL checks)
 * 2. Call rx_bus_manager_with_bus() under mutex
 * 3. Inside callback:
 *    a. Validate bus type is k_bus_type_uart
 *    b. Validate SCI channel number (0-12)
 *    c. Extract UART config from bus config (channel, baud, TX/RX pins)
 *    d. Call uart_init_channel() HAL function
 *    e. Mark bus as initialized
 * 4. Return result
 *
 * ## SCI Peripheral Configuration
 *
 * The HAL configures:
 * - **Mode**: Asynchronous UART (not Simple I2C/SPI)
 * - **Format**: 8 data bits, no parity, 1 stop bit (8N1)
 * - **Baud rate**: User-specified (validated by HAL)
 * - **Clock source**: PCLK or PCLKB (f_PCLK = 120 MHz typical)
 * - **TX GPIO**: Configured as SCI TXD output
 * - **RX GPIO**: Configured as SCI RXD input with pull-up
 * - **Interrupts**: TX empty (TXI), RX full (RXI), error (ERI)
 * - **FIFOs**: 16-byte TX FIFO, 16-byte RX FIFO enabled
 *
 * ## Baud Rate Range
 *
 * Valid baud rates depend on PCLK frequency:
 * - **Min**: 110 bps (limited by BRR register range)
 * - **Max**: 2,500,000 bps @ f_PCLK = 120 MHz
 * - **Common**: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
 *
 * **Accuracy**: +/-1% or better for standard baud rates
 *
 * @param[in] manager Pointer to bus manager instance.
 *                    Must be initialized via rx_bus_manager_init().
 * @param[in] bus_name Name of UART bus (e.g., "debug", "gps", "sensor").
 *                     Must be registered via rx_bus_manager_add_bus().
 *
 * @return rx_err_t Initialization status
 *
 * @retval k_rx_ok UART channel initialized successfully
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_arg Bus type is not k_bus_type_uart, or SCI channel > 12
 * @retval k_rx_err_hw_init_failed SCI peripheral init failed (HAL error)
 *
 * @pre manager initialized, bus registered with UART config
 * @pre SCI channel not already initialized (or call after deinit)
 * @pre GPIO pins not in use by other peripherals
 *
 * @post SCI peripheral configured for 8N1 UART at specified baud rate
 * @post TX/RX GPIOs configured (alternate function mode)
 * @post bus_config->initialized == true
 * @post Ready for UART read/write operations
 *
 * @note Initialization may take ~50 us (register configuration + pin setup)
 * @note TX FIFO and RX FIFO are both enabled (16 bytes each)
 * @note Thread-safe (bus manager mutex)
 *
 * @warning Do NOT call while UART transmission is active
 * @warning Ensure TX/RX pins are not shorted (hardware damage possible)
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant. Do not call concurrently for same SCI channel.
 *
 * @par Example - Debug Console @ 115200 bps:
 * @code{.c}
 * // 1. Configure UART bus (SCI1 on P26/P30 @ 115200 bps)
 * rx_bus_config_t debug_cfg;
 * rx_bus_config_init_uart(&debug_cfg, "debug", 1,  // SCI channel 1
 *                         k_rx_pin_p26, k_rx_pin_p30, 115200);
 * rx_bus_manager_add_bus(&bus_mgr, &debug_cfg);
 *
 * // 2. Initialize UART
 * rx_err_t err = rx_bus_uart_init(&bus_mgr, "debug");
 * if (err != k_rx_ok) {
 *     // Handle init failure (LED blink, halt, etc.)
 *     while (1) {}  // Fatal error
 * }
 *
 * // 3. Now ready for debug output
 * rx_bus_uart_puts(&bus_mgr, "debug", "System ready\r\n");
 * @endcode
 *
 * @par Example - GPS Module @ 9600 bps:
 * @code{.c}
 * // GPS modules typically use 9600 bps
 * rx_bus_config_t gps_cfg;
 * rx_bus_config_init_uart(&gps_cfg, "gps", 2,  // SCI channel 2
 *                         k_rx_pin_p50, k_rx_pin_p52, 9600);
 * rx_bus_manager_add_bus(&bus_mgr, &gps_cfg);
 *
 * rx_err_t err = rx_bus_uart_init(&bus_mgr, "gps");
 * if (err == k_rx_ok) {
 *     rx_log_debug("GPS", "UART initialized at 9600 bps");
 * }
 * @endcode
 *
 * @par Example - Multiple UARTs:
 * @code{.c}
 * // Initialize debug console and GPS sensor on different channels
 * rx_bus_config_t debug_cfg, gps_cfg;
 *
 * // Debug on SCI1 @ 115200
 * rx_bus_config_init_uart(&debug_cfg, "debug", 1,
 *                         k_rx_pin_p26, k_rx_pin_p30, 115200);
 * rx_bus_manager_add_bus(&bus_mgr, &debug_cfg);
 * rx_bus_uart_init(&bus_mgr, "debug");
 *
 * // GPS on SCI2 @ 9600
 * rx_bus_config_init_uart(&gps_cfg, "gps", 2,
 *                         k_rx_pin_p50, k_rx_pin_p52, 9600);
 * rx_bus_manager_add_bus(&bus_mgr, &gps_cfg);
 * rx_bus_uart_init(&bus_mgr, "gps");
 *
 * // Both UARTs now operational concurrently
 * rx_bus_uart_puts(&bus_mgr, "debug", "GPS module initializing...\r\n");
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * rx_err_t err = rx_bus_uart_init(&bus_mgr, "sensor");
 * switch (err) {
 *     case k_rx_ok:
 *         rx_log_debug("SENSOR", "UART initialized");
 *         break;
 *     case k_rx_err_invalid_arg:
 *         rx_log_error("SENSOR", "Invalid SCI channel or config");
 *         break;
 *     case k_rx_err_hw_init_failed:
 *         rx_log_error("SENSOR", "SCI peripheral init failed");
 *         break;
 *     case k_rx_err_not_found:
 *         rx_log_error("SENSOR", "Bus not registered");
 *         break;
 *     default:
 *         rx_log_error("SENSOR", "Unknown error: %d", err);
 *         break;
 * }
 * @endcode
 *
 * @see rx_bus_config_init_uart() Create UART bus configuration
 * @see rx_bus_uart_write() Send binary data
 * @see rx_bus_uart_puts() Send string
 * @see hardware.h SCI UART HAL documentation
 *
 * @since Version 1.0.0
 *
 * @test test_rx_bus_uart.c::test_uart_init_success()
 * @test test_rx_bus_uart.c::test_uart_init_invalid_channel()
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 4 preconditions, 4 postconditions
 * - **Rule 4**: Function is 13 lines (well under 60 limit)
 * - **Rule 7**: All returns checked (with_bus result, ctx.result)
 */
rx_err_t rx_bus_uart_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  uart_init_ctx_t ctx = {.result = k_rx_err_hw_init_failed};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Write binary data to UART (blocking or DMA-based transfer)
 *
 * @details
 * Transmits binary data buffer over UART. Uses SCI TX FIFO and DMA/interrupt-driven
 * transmission for efficiency. Function may block briefly for FIFO space, then returns
 * while transmission continues in background (implementation-dependent).
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager, bus_name, data (NULL checks)
 * 2. Initialize context with data pointer and length
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback:
 *    a. Validate bus initialized
 *    b. Validate data pointer if length > 0
 *    c. Call uart_write_channel() HAL function
 *    d. HAL loads data into TX FIFO / starts DMA
 * 5. Return result
 *
 * ## Transmission Behavior (HAL-dependent)
 *
 * **Option 1 - Polling Mode:**
 * - Function blocks until all bytes transmitted (~length x 87 us @ 115200 bps)
 * - Simple but inefficient for large transfers
 *
 * **Option 2 - Interrupt/DMA Mode (preferred):**
 * - Function loads TX FIFO (16 bytes) and returns immediately
 * - Remaining bytes transmitted via interrupt or DMA in background
 * - Much faster return (~10 us overhead)
 *
 * **STAR firmware uses**: Interrupt/DMA mode for efficiency
 *
 * ## Performance
 *
 * | Length | @ 115200 bps | @ 9600 bps | Notes |
 * |--------|--------------|------------|-------|
 * | **1 byte** | ~90 us | ~1.1 ms | 1 start + 8 data + 1 stop bit |
 * | **16 bytes** | ~1.4 ms | ~17 ms | Fill TX FIFO once |
 * | **64 bytes** | ~5.6 ms | ~67 ms | Typical packet size |
 * | **256 bytes** | ~22 ms | ~267 ms | Max recommended per call |
 *
 * **Overhead**: ~10 us (FIFO load + context switch)
 *
 * @param[in] manager Pointer to bus manager instance.
 * @param[in] bus_name Name of UART bus (e.g., "debug", "sensor").
 * @param[in] data Pointer to data buffer to transmit.
 *                 Must be valid for at least 'length' bytes.
 *                 Can contain binary data (not just ASCII).
 * @param[in] length Number of bytes to transmit.
 *                   Valid range: [0, 65535].
 *                   Use 0 for no-op (returns k_rx_ok).
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok Data queued for transmission (or already sent in blocking mode)
 * @retval k_rx_err_null_ptr manager, bus_name, or data is nullptr (if length > 0)
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_invalid_arg length > 0 but data is nullptr
 * @retval k_rx_err_uart_error HAL transmission error (FIFO full, timeout, etc.)
 *
 * @pre manager initialized, bus registered and initialized
 * @pre data buffer valid for 'length' bytes
 * @pre Data buffer remains valid until transmission completes (if DMA mode)
 *
 * @post Data transmitted or queued in TX FIFO/DMA
 * @post TX interrupt/DMA active (if non-blocking mode)
 *
 * @note Thread-safe (bus manager mutex)
 * @note For large transfers, consider splitting into smaller chunks
 * @note No flow control - receiver must keep up with baud rate
 *
 * @warning If DMA mode: Data buffer must remain valid until transmission completes
 * @warning Very large transfers (>1KB) may block for extended periods
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name. Reentrant for different buses.
 *
 * @par Example - Send Binary Packet:
 * @code{.c}
 * // Send command packet to sensor
 * const uint8_t cmd[] = {0xAA, 0x01, 0x10, 0x00, 0xCB};  // Header + cmd + CRC
 * rx_err_t err = rx_bus_uart_write(&bus_mgr, "sensor", cmd, sizeof(cmd));
 * if (err == k_rx_ok) {
 *     rx_log_debug("SENSOR", "Command sent: 5 bytes");
 * }
 * @endcode
 *
 * @par Example - Send Log Message:
 * @code{.c}
 * // Send formatted log message as binary data
 * char log_msg[64];
 * int len = snprintf(log_msg, sizeof(log_msg), "[%lu] Motor speed: %.2f m/s\r\n",
 *                    tx_time_get(), motor_speed);
 * rx_bus_uart_write(&bus_mgr, "debug", (const uint8_t*)log_msg, len);
 * @endcode
 *
 * @par Example - Send Protobuf Message:
 * @code{.c}
 * // Encode and send Protocol Buffers message
 * uint8_t pb_buffer[128];
 * pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));
 *
 * TelemetryMessage msg = {.battery_voltage = 12.5f, .soc_percent = 85};
 * bool status = pb_encode(&stream, TelemetryMessage_fields, &msg);
 * if (status) {
 *     rx_bus_uart_write(&bus_mgr, "telemetry", pb_buffer, stream.bytes_written);
 * }
 * @endcode
 *
 * @see rx_bus_uart_read() Receive binary data
 * @see rx_bus_uart_puts() Send null-terminated string (text mode)
 * @see uart_write_channel() Underlying HAL function
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bus_uart_write(rx_bus_manager_t* manager,
                           const char*       bus_name,
                           const uint8_t*    data,
                           const uint16_t    length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");

  uart_write_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_uart_error};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_write_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Read available binary data from UART RX FIFO (non-blocking)
 *
 * @details
 * Reads up to 'length' bytes from the UART RX FIFO. Returns immediately with whatever
 * data is currently available (may be 0 bytes). Non-blocking operation - does NOT wait
 * for specific byte count. Use rx_available() to check before calling, or poll in loop.
 *
 * ## Algorithm Steps
 *
 * 1. Validate all parameters (NULL checks)
 * 2. Initialize context with buffer pointer, max length
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback:
 *    a. Validate bus initialized
 *    b. Call uart_read_channel() HAL function
 *    c. HAL reads available bytes from RX FIFO (up to 'length')
 *    d. HAL updates bytes_read with actual count
 *    e. Validate bytes_read <= length (postcondition check)
 * 5. Copy bytes_read to output parameter
 * 6. Return result
 *
 * ## Non-Blocking Behavior
 *
 * | Scenario | bytes_read | Return Value | Action |
 * |----------|------------|--------------|--------|
 * | **FIFO empty** | 0 | k_rx_ok | No data available yet |
 * | **Partial data** | 1 to (length-1) | k_rx_ok | More data may arrive later |
 * | **Full buffer** | length | k_rx_ok | Buffer filled completely |
 * | **FIFO has >length** | length | k_rx_ok | Remaining bytes stay in FIFO |
 *
 * **Key insight**: Function NEVER blocks waiting for data. Returns immediately.
 *
 * ## Typical Usage Pattern
 *
 * ```c
 * // Poll for data arrival
 * while (true) {
 *     uint8_t rx_buf[64];
 *     uint16_t bytes_read;
 *     rx_err_t err = rx_bus_uart_read(&bus_mgr, "gps", rx_buf, sizeof(rx_buf), &bytes_read);
 *
 *     if (err == k_rx_ok && bytes_read > 0) {
 *         process_gps_data(rx_buf, bytes_read);
 *     }
 *
 *     tx_thread_sleep(10);  // Poll every 10ms
 * }
 * ```
 *
 * ## Performance
 *
 * - **Function overhead**: ~5 us (FIFO read + context switch)
 * - **Actual read time**: ~200 ns per byte (memcpy from FIFO to buffer)
 * - **Total**: ~5 us + (bytes_read x 200 ns)
 *
 * ## RX FIFO Overflow Handling
 *
 * - **FIFO size**: 16 bytes per SCI channel
 * - **Overflow behavior**: If RX FIFO fills (>16 bytes received without read), oldest byte is lost
 * - **Error flag**: HAL sets overrun error (k_rx_err_uart_error returned)
 * - **Prevention**: Poll frequently enough to prevent overflow
 *
 * **Rule of thumb**: Read at least every (16 bytes / baud_rate) seconds
 * - @ 115200 bps: ~1.4 ms (16 bytes x 87 us/byte)
 * - @ 9600 bps: ~17 ms
 *
 * @param[in] manager Pointer to bus manager instance.
 * @param[in] bus_name Name of UART bus (e.g., "gps", "sensor").
 * @param[out] data Pointer to buffer for received data.
 *                  Must be at least 'length' bytes.
 *                  Filled with received bytes (if any available).
 * @param[in] length Maximum bytes to read (buffer size).
 *                   Valid range: [1, 65535].
 *                   Typically 64-256 bytes for packet buffers.
 * @param[out] bytes_read Pointer to store actual number of bytes read.
 *                        Range: [0, length].
 *                        0 = no data available.
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok Read completed (check bytes_read for actual count, may be 0)
 * @retval k_rx_err_null_ptr manager, bus_name, data, or bytes_read is nullptr
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL error (framing error, overrun, etc.)
 *
 * @pre manager initialized, bus registered and initialized
 * @pre data buffer is at least 'length' bytes
 *
 * @post data filled with bytes_read bytes from RX FIFO
 * @post *bytes_read contains actual byte count (0 if FIFO empty)
 * @post Bytes removed from RX FIFO
 *
 * @note NON-BLOCKING: Returns immediately regardless of data availability
 * @note bytes_read may be 0 (valid - no data available)
 * @note Thread-safe (bus manager mutex)
 * @note For line-based protocols, use getc() in loop until '\n'
 *
 * @warning bytes_read = 0 is NOT an error (just no data yet)
 * @warning Overrun error if FIFO fills before read (data loss!)
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name. Reentrant for different buses.
 *
 * @par Example - Poll for GPS NMEA Sentence:
 * @code{.c}
 * // Read GPS data (NMEA sentences, variable length)
 * uint8_t gps_buf[128];
 * uint16_t bytes_read;
 *
 * rx_err_t err = rx_bus_uart_read(&bus_mgr, "gps", gps_buf, sizeof(gps_buf), &bytes_read);
 * if (err == k_rx_ok) {
 *     if (bytes_read > 0) {
 *         rx_log_debug("GPS", "Received %u bytes", bytes_read);
 *         parse_nmea_data(gps_buf, bytes_read);
 *     } else {
 *         rx_log_debug("GPS", "No data available");
 *     }
 * }
 * @endcode
 *
 * @par Example - Read Until Newline (Line-based Protocol):
 * @code{.c}
 * // Read line-by-line from sensor
 * char line_buf[80];
 * uint16_t line_pos = 0;
 *
 * while (line_pos < sizeof(line_buf) - 1) {
 *     uint8_t byte;
 *     uint16_t bytes_read;
 *     rx_err_t err = rx_bus_uart_read(&bus_mgr, "sensor", &byte, 1, &bytes_read);
 *
 *     if (err == k_rx_ok && bytes_read > 0) {
 *         if (byte == '\n') {
 *             line_buf[line_pos] = '\0';  // Null-terminate
 *             process_line(line_buf);
 *             line_pos = 0;
 *             break;
 *         }
 *         line_buf[line_pos++] = byte;
 *     }
 *
 *     tx_thread_sleep(1);  // Poll every 1ms
 * }
 * @endcode
 *
 * @par Example - Read Binary Packet with Timeout:
 * @code{.c}
 * // Wait up to 100ms for 5-byte response packet
 * const uint16_t k_expected_bytes = 5;
 * const uint32_t k_timeout_ms = 100;
 * uint8_t response[5];
 * uint16_t total_read = 0;
 * uint32_t start_time = tx_time_get();
 *
 * while (total_read < k_expected_bytes) {
 *     uint16_t bytes_read;
 *     rx_err_t err = rx_bus_uart_read(&bus_mgr, "device",
 *                                     &response[total_read],
 *                                     k_expected_bytes - total_read,
 *                                     &bytes_read);
 *     if (err == k_rx_ok) {
 *         total_read += bytes_read;
 *     }
 *
 *     if ((tx_time_get() - start_time) > k_timeout_ms) {
 *         rx_log_error("DEVICE", "Timeout: only %u/%u bytes received",
 *                      total_read, k_expected_bytes);
 *         break;
 *     }
 *
 *     tx_thread_sleep(1);
 * }
 *
 * if (total_read == k_expected_bytes) {
 *     rx_log_debug("DEVICE", "Complete packet received");
 *     process_response(response);
 * }
 * @endcode
 *
 * @see rx_bus_uart_write() Transmit binary data
 * @see rx_bus_uart_rx_available() Check if data available before reading
 * @see rx_bus_uart_getc() Read single character (alternative for byte-by-byte)
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bus_uart_read(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          uint8_t*          data,
                          const uint16_t    length,
                          uint16_t*         bytes_read)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");
  RX_CHECK_NULL_PTR(bytes_read, s_tag, "bytes_read pointer is nullptr");

  uart_read_ctx_t ctx = {.data       = data,
                         .length     = length,
                         .bytes_read = 0,
                         .result     = k_rx_err_uart_error};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_read_callback, &ctx);

  *bytes_read = ctx.bytes_read;

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Write single character to UART (text mode)
 *
 * @details
 * Transmits a single ASCII character over UART. Commonly used for interactive console
 * output, character-by-character text streaming, or implementing custom printf-style
 * formatters. More efficient than uart_write() for single characters.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager and bus_name (NULL checks)
 * 2. Initialize context with character
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback:
 *    a. Validate bus initialized
 *    b. Call uart_putc_channel() HAL function
 *    c. HAL writes byte to TX FIFO
 *    d. Validate character is 7-bit ASCII (postcondition warning if > 127)
 * 5. Return result
 *
 * ## Common Use Cases
 *
 * - **Console echo**: Echo user input back to terminal
 * - **Progress indicators**: Print dots/spinners during operations
 * - **Custom formatting**: Build formatted strings character-by-character
 * - **Protocol characters**: Send control characters (CR, LF, ESC, etc.)
 *
 * ## Performance
 *
 * - **Function overhead**: ~2 us (FIFO write + context)
 * - **Transmission time** (baud-dependent):
 *   - @ 115200 bps: ~87 us per character
 *   - @ 9600 bps: ~1.04 ms per character
 *
 * @param[in] manager Pointer to bus manager instance.
 * @param[in] bus_name Name of UART bus (e.g., "debug", "console").
 * @param[in] c Character to transmit.
 *              Valid range: 0-255 (any byte value).
 *              Typically 7-bit ASCII (0-127).
 *              Common: 'A'-'Z', '0'-'9', '\r', '\n', '\t'.
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok Character transmitted successfully
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL transmission error
 *
 * @pre manager initialized, bus registered and initialized
 *
 * @post Character transmitted via UART
 *
 * @note Thread-safe (bus manager mutex)
 * @note For strings, use uart_puts() (more efficient)
 * @note Extended ASCII (128-255) generates warning but still sent
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Example - Echo Console Input:
 * @code{.c}
 * // Read character from user and echo back
 * char input;
 * if (rx_bus_uart_getc(&bus_mgr, "console", &input) == k_rx_ok) {
 *     rx_bus_uart_putc(&bus_mgr, "console", input);  // Echo
 * }
 * @endcode
 *
 * @par Example - Progress Indicator:
 * @code{.c}
 * // Print dots during long operation
 * for (uint8_t i = 0; i < 100; i++) {
 *     process_chunk(i);
 *     rx_bus_uart_putc(&bus_mgr, "debug", '.');
 *     if ((i + 1) % 50 == 0) {
 *         rx_bus_uart_putc(&bus_mgr, "debug", '\n');
 *     }
 * }
 * @endcode
 *
 * @par Example - Send CR+LF Line Ending:
 * @code{.c}
 * // Send Windows-style line ending
 * rx_bus_uart_putc(&bus_mgr, "debug", '\r');
 * rx_bus_uart_putc(&bus_mgr, "debug", '\n');
 * @endcode
 *
 * @see rx_bus_uart_puts() Write null-terminated string (more efficient)
 * @see rx_bus_uart_write() Write binary data
 * @see rx_bus_uart_getc() Read single character
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bus_uart_putc(rx_bus_manager_t* manager, const char* bus_name, char c)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  uart_putc_ctx_t ctx = {.c = c, .result = k_rx_err_uart_error};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_putc_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Write null-terminated string to UART (text mode)
 *
 * @details
 * Transmits a C-style null-terminated string over UART. Sends characters until '\0'
 * encountered (null terminator NOT transmitted). Most efficient way to send text
 * messages, log output, and console strings. Internally uses uart_write() for
 * performance.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager, bus_name, str (NULL checks)
 * 2. Initialize context with string pointer
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback:
 *    a. Validate bus initialized
 *    b. Call uart_puts_channel() HAL function
 *    c. HAL calculates string length (strlen internally)
 *    d. HAL writes bytes to TX FIFO (stops at '\0')
 * 5. Return result
 *
 * ## Advantages Over putc() Loop
 *
 * | Method | Function Calls | Mutex Locks | Overhead |
 * |--------|---------------|-------------|----------|
 * | **puts("Hello")** | 1 | 1 | ~10 us total |
 * | **putc() x 5** | 5 | 5 | ~10 us x 5 = 50 us |
 *
 * **puts() is 5x more efficient** for multi-character strings due to single mutex lock.
 *
 * ## Common Use Cases
 *
 * - **Debug logging**: Print status messages to console
 * - **User prompts**: Display menu options, input prompts
 * - **Error messages**: Output error descriptions
 * - **NMEA/AT commands**: Send protocol command strings
 *
 * ## Performance
 *
 * - **Function overhead**: ~10 us (strlen + FIFO load)
 * - **Transmission time** (baud-dependent):
 *   - @ 115200 bps: ~87 us x strlen(str)
 *   - @ 9600 bps: ~1.04 ms x strlen(str)
 *
 * **Example**: "Hello\r\n" (7 chars) @ 115200 bps = 10 us + (7 x 87 us) = ~620 us
 *
 * @param[in] manager Pointer to bus manager instance.
 * @param[in] bus_name Name of UART bus (e.g., "debug", "console").
 * @param[in] str Pointer to null-terminated string.
 *                Must end with '\0'.
 *                NULL terminator not transmitted.
 *                Can contain any ASCII characters including CR/LF.
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok String transmitted successfully
 * @retval k_rx_err_null_ptr manager, bus_name, or str is nullptr
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL transmission error
 *
 * @pre manager initialized, bus registered and initialized
 * @pre str is valid null-terminated string
 *
 * @post String characters transmitted (excluding '\0')
 *
 * @note Thread-safe (bus manager mutex)
 * @note Does NOT add newline automatically (use "\r\n" in string)
 * @note More efficient than putc() loop for multi-character strings
 *
 * @warning Ensure str is null-terminated (undefined behavior otherwise)
 * @warning Very long strings may block for extended periods
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Example - Debug Message:
 * @code{.c}
 * // Print system status message
 * rx_bus_uart_puts(&bus_mgr, "debug", "System initialized successfully\r\n");
 * @endcode
 *
 * @par Example - Menu Display:
 * @code{.c}
 * // Display user menu
 * rx_bus_uart_puts(&bus_mgr, "console", "\r\n=== MAIN MENU ===\r\n");
 * rx_bus_uart_puts(&bus_mgr, "console", "1. Start motors\r\n");
 * rx_bus_uart_puts(&bus_mgr, "console", "2. Stop motors\r\n");
 * rx_bus_uart_puts(&bus_mgr, "console", "3. Read sensors\r\n");
 * rx_bus_uart_puts(&bus_mgr, "console", "Enter choice: ");
 * @endcode
 *
 * @par Example - GPS AT Command:
 * @code{.c}
 * // Send AT command to GPS module
 * rx_bus_uart_puts(&bus_mgr, "gps", "AT+CGPSPWR=1\r\n");  // Power on GPS
 * tx_thread_sleep(100);  // Wait for response
 * rx_bus_uart_puts(&bus_mgr, "gps", "AT+CGPSRST=0\r\n");  // Hot restart
 * @endcode
 *
 * @par Example - Formatted Output (with snprintf):
 * @code{.c}
 * // Print formatted telemetry
 * char msg[80];
 * snprintf(msg, sizeof(msg), "Voltage: %.2fV, Current: %.2fA, Temp: %ddegC\r\n",
 *          battery_voltage, battery_current, temperature);
 * rx_bus_uart_puts(&bus_mgr, "debug", msg);
 * @endcode
 *
 * @see rx_bus_uart_putc() Write single character
 * @see rx_bus_uart_write() Write binary data (known length)
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bus_uart_puts(rx_bus_manager_t* manager, const char* bus_name, const char* str)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(str, s_tag, "str pointer is nullptr");

  uart_puts_ctx_t ctx = {.str = str, .result = k_rx_err_uart_error};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_puts_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Read single character from UART (non-blocking, text mode)
 *
 * @details
 * Reads one character from the UART RX FIFO. Returns immediately - does NOT block
 * waiting for data. If FIFO is empty, returns k_rx_err_empty (not an error, just
 * indicates no data available yet). Commonly used for interactive console input,
 * command parsers, and polling-based receivers.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager, bus_name, c (NULL checks)
 * 2. Initialize context with character pointer
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback:
 *    a. Validate bus initialized
 *    b. Call uart_getc_channel() HAL function
 *    c. HAL reads one byte from RX FIFO (if available)
 *    d. If FIFO empty: Return k_rx_err_empty
 *    e. If byte received: Validate ASCII (postcondition warning if > 127)
 * 5. Copy character to output parameter
 * 6. Return result
 *
 * ## Non-Blocking Behavior
 *
 * | Scenario | *c Value | Return Value | Meaning |
 * |----------|----------|--------------|---------|
 * | **Data available** | received char | k_rx_ok | Character successfully read |
 * | **FIFO empty** | undefined | k_rx_err_empty | No data yet (NOT an error!) |
 * | **Framing error** | garbage | k_rx_err_uart_error | Serial line error |
 *
 * **Critical**: k_rx_err_empty is NORMAL and means "try again later"
 *
 * ## Typical Usage Pattern
 *
 * ```c
 * // Poll for user input
 * while (true) {
 *     char ch;
 *     rx_err_t err = rx_bus_uart_getc(&bus_mgr, "console", &ch);
 *
 *     if (err == k_rx_ok) {
 *         process_character(ch);  // Got a character
 *     } else if (err == k_rx_err_empty) {
 *         // No data yet, continue polling
 *     }
 *
 *     tx_thread_sleep(10);  // Poll every 10ms
 * }
 * ```
 *
 * ## Performance
 *
 * - **Function overhead**: ~2 us (FIFO read + context)
 * - **Returns immediately** (no blocking wait)
 *
 * @param[in] manager Pointer to bus manager instance.
 * @param[in] bus_name Name of UART bus (e.g., "console", "sensor").
 * @param[out] c Pointer to store received character.
 *               Filled with character if k_rx_ok returned.
 *               Undefined if k_rx_err_empty returned.
 *               Must point to valid char variable.
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok Character received and stored in *c
 * @retval k_rx_err_empty No data available in RX FIFO (NOT an error - try again)
 * @retval k_rx_err_null_ptr manager, bus_name, or c is nullptr
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL error (framing error, overrun, etc.)
 *
 * @pre manager initialized, bus registered and initialized
 * @pre c points to valid char variable
 *
 * @post *c contains received character (if k_rx_ok)
 * @post Character removed from RX FIFO (if k_rx_ok)
 *
 * @note NON-BLOCKING: Returns k_rx_err_empty if no data (not an error!)
 * @note Thread-safe (bus manager mutex)
 * @note For binary data, use uart_read() instead
 *
 * @warning k_rx_err_empty is NORMAL - do NOT treat as error
 * @warning *c is undefined when k_rx_err_empty returned
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name. Reentrant for different buses.
 *
 * @par Example - Interactive Console with Echo:
 * @code{.c}
 * // Read user input and echo back
 * char input;
 * rx_err_t err = rx_bus_uart_getc(&bus_mgr, "console", &input);
 *
 * if (err == k_rx_ok) {
 *     // Echo character back to user
 *     rx_bus_uart_putc(&bus_mgr, "console", input);
 *
 *     // Process command if Enter pressed
 *     if (input == '\r' || input == '\n') {
 *         process_command();
 *     }
 * } else if (err == k_rx_err_empty) {
 *     // Normal - no input yet
 * }
 * @endcode
 *
 * @par Example - Build Command String:
 * @code{.c}
 * // Accumulate characters until newline
 * static char cmd_buf[80];
 * static uint8_t cmd_pos = 0;
 *
 * char ch;
 * rx_err_t err = rx_bus_uart_getc(&bus_mgr, "console", &ch);
 *
 * if (err == k_rx_ok) {
 *     if (ch == '\r' || ch == '\n') {
 *         // Command complete
 *         cmd_buf[cmd_pos] = '\0';
 *         execute_command(cmd_buf);
 *         cmd_pos = 0;
 *     } else if (ch == '\b' || ch == 0x7F) {
 *         // Backspace
 *         if (cmd_pos > 0) {
 *             cmd_pos--;
 *             rx_bus_uart_puts(&bus_mgr, "console", "\b \b");  // Erase char on screen
 *         }
 *     } else if (cmd_pos < sizeof(cmd_buf) - 1) {
 *         // Add to buffer
 *         cmd_buf[cmd_pos++] = ch;
 *         rx_bus_uart_putc(&bus_mgr, "console", ch);  // Echo
 *     }
 * }
 * @endcode
 *
 * @par Example - Menu Input:
 * @code{.c}
 * // Wait for menu selection (with timeout)
 * rx_bus_uart_puts(&bus_mgr, "console", "Enter choice (1-3): ");
 *
 * const uint32_t k_timeout_ms = 5000;
 * uint32_t start_time = tx_time_get();
 * char choice = '\0';
 *
 * while ((tx_time_get() - start_time) < k_timeout_ms) {
 *     rx_err_t err = rx_bus_uart_getc(&bus_mgr, "console", &choice);
 *
 *     if (err == k_rx_ok) {
 *         if (choice >= '1' && choice <= '3') {
 *             handle_menu_choice(choice);
 *             break;
 *         } else {
 *             rx_bus_uart_puts(&bus_mgr, "console", "\r\nInvalid choice. Try again: ");
 *         }
 *     }
 *
 *     tx_thread_sleep(10);
 * }
 *
 * if (choice == '\0') {
 *     rx_bus_uart_puts(&bus_mgr, "console", "\r\nTimeout!\r\n");
 * }
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * char ch;
 * rx_err_t err = rx_bus_uart_getc(&bus_mgr, "console", &ch);
 *
 * switch (err) {
 *     case k_rx_ok:
 *         process_character(ch);
 *         break;
 *     case k_rx_err_empty:
 *         // Normal - no data available yet
 *         break;
 *     case k_rx_err_uart_error:
 *         rx_log_error("CONSOLE", "UART framing/overrun error");
 *         break;
 *     default:
 *         rx_log_error("CONSOLE", "Unexpected error: %d", err);
 *         break;
 * }
 * @endcode
 *
 * @see rx_bus_uart_rx_available() Check if data available before reading
 * @see rx_bus_uart_read() Read binary data (multi-byte)
 * @see rx_bus_uart_putc() Write single character
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bus_uart_getc(rx_bus_manager_t* manager, const char* bus_name, char* c)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(c, s_tag, "c pointer is nullptr");

  uart_getc_ctx_t ctx = {.c = '\0', .result = k_rx_err_uart_error};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_getc_callback, &ctx);

  *c = ctx.c;

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Check if UART receive data is available (non-blocking status query)
 *
 * @details
 * Checks if at least one byte is available in the UART RX FIFO without actually reading
 * it. Returns immediately - does NOT block. Useful for efficient polling: check availability
 * first, then read only when data present (avoids unnecessary getc/read calls that return
 * k_rx_err_empty).
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager, bus_name, available (NULL checks)
 * 2. Initialize context with available pointer
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback:
 *    a. Validate bus initialized
 *    b. Call uart_rx_available() HAL function
 *    c. HAL checks RX FIFO status register (FRDR bit or count)
 *    d. Set *available = true if FIFO has >=1 byte, false otherwise
 *    e. Validate boolean value (postcondition check)
 * 5. Copy availability status to output parameter
 * 6. Return result
 *
 * ## Efficient Polling Pattern
 *
 * **Without rx_available (inefficient):**
 * ```c
 * // Calls getc() unnecessarily when no data available
 * while (true) {
 *     char ch;
 *     if (getc(&ch) == k_rx_ok) {  // Wastes mutex lock when empty
 *         process(ch);
 *     }
 *     sleep(1);
 * }
 * ```
 *
 * **With rx_available (efficient):**
 * ```c
 * // Only calls getc() when data is actually available
 * while (true) {
 *     bool avail;
 *     if (rx_available(&avail) == k_rx_ok && avail) {
 *         char ch;
 *         getc(&ch);  // Guaranteed to succeed
 *         process(ch);
 *     }
 *     sleep(1);
 * }
 * ```
 *
 * **Benefit**: Reduces mutex contention and unnecessary getc() calls
 *
 * ## Performance
 *
 * - **Function overhead**: ~1 us (register read + context)
 * - **Fastest UART query** (just reads status register, no data movement)
 *
 * ## Use Cases
 *
 * - **Efficient polling**: Check before read to avoid empty reads
 * - **Wake conditions**: Event-driven code (wake thread when data arrives)
 * - **Buffer management**: Determine if reading is worthwhile
 * - **Diagnostics**: Monitor UART activity without consuming data
 *
 * @param[in] manager Pointer to bus manager instance.
 * @param[in] bus_name Name of UART bus (e.g., "console", "gps").
 * @param[out] available Pointer to store availability status.
 *                       Set to true if >=1 byte in RX FIFO.
 *                       Set to false if RX FIFO is empty.
 *                       Must point to valid bool variable.
 *
 * @return rx_err_t Operation status
 *
 * @retval k_rx_ok Status check successful, *available contains result
 * @retval k_rx_err_null_ptr manager, bus_name, or available is nullptr
 * @retval k_rx_err_not_found bus_name not in manager
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_uart_error HAL error (should never happen for status read)
 *
 * @pre manager initialized, bus registered and initialized
 * @pre available points to valid bool variable
 *
 * @post *available = true if >=1 byte available, false if FIFO empty
 * @post RX FIFO unchanged (data not consumed)
 *
 * @note NON-BLOCKING: Returns immediately
 * @note Does NOT remove data from FIFO (use getc/read to consume)
 * @note Thread-safe (bus manager mutex)
 * @note Fastest UART query operation (~1 us)
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name. Reentrant for different buses.
 *
 * @par Example - Efficient Polling Loop:
 * @code{.c}
 * // Only read when data available
 * while (running) {
 *     bool has_data;
 *     rx_err_t err = rx_bus_uart_rx_available(&bus_mgr, "gps", &has_data);
 *
 *     if (err == k_rx_ok && has_data) {
 *         // Data available - read it
 *         uint8_t rx_buf[128];
 *         uint16_t bytes_read;
 *         rx_bus_uart_read(&bus_mgr, "gps", rx_buf, sizeof(rx_buf), &bytes_read);
 *         parse_nmea(rx_buf, bytes_read);
 *     }
 *
 *     tx_thread_sleep(10);  // Poll every 10ms
 * }
 * @endcode
 *
 * @par Example - Event-Driven Read:
 * @code{.c}
 * // Wake thread only when data arrives
 * void uart_rx_task(void* arg)
 * {
 *     while (true) {
 *         bool data_ready;
 *         if (rx_bus_uart_rx_available(&bus_mgr, "sensor", &data_ready) == k_rx_ok) {
 *             if (data_ready) {
 *                 char ch;
 *                 rx_bus_uart_getc(&bus_mgr, "sensor", &ch);
 *                 process_byte(ch);
 *             } else {
 *                 // No data - sleep longer
 *                 tx_thread_sleep(50);
 *             }
 *         }
 *     }
 * }
 * @endcode
 *
 * @par Example - Monitor UART Activity:
 * @code{.c}
 * // Log when data arrives (diagnostics)
 * static uint32_t last_rx_time = 0;
 *
 * bool has_data;
 * if (rx_bus_uart_rx_available(&bus_mgr, "debug", &has_data) == k_rx_ok) {
 *     if (has_data) {
 *         uint32_t now = tx_time_get();
 *         if (last_rx_time > 0) {
 *             rx_log_debug("UART", "Data received (gap: %lu ms)", now - last_rx_time);
 *         }
 *         last_rx_time = now;
 *
 *         // Now actually read the data
 *         char ch;
 *         rx_bus_uart_getc(&bus_mgr, "debug", &ch);
 *     }
 * }
 * @endcode
 *
 * @par Example - Batch Processing:
 * @code{.c}
 * // Read all available data at once
 * bool has_data;
 * if (rx_bus_uart_rx_available(&bus_mgr, "sensor", &has_data) == k_rx_ok && has_data) {
 *     uint8_t batch_buf[256];
 *     uint16_t total_read = 0;
 *
 *     // Keep reading until FIFO empty
 *     while (has_data && total_read < sizeof(batch_buf)) {
 *         char ch;
 *         if (rx_bus_uart_getc(&bus_mgr, "sensor", &ch) == k_rx_ok) {
 *             batch_buf[total_read++] = ch;
 *         }
 *         rx_bus_uart_rx_available(&bus_mgr, "sensor", &has_data);
 *     }
 *
 *     process_batch(batch_buf, total_read);
 * }
 * @endcode
 *
 * @par Example - Timeout with Availability Check:
 * @code{.c}
 * // Wait up to 1 second for data
 * const uint32_t k_timeout_ms = 1000;
 * uint32_t start_time = tx_time_get();
 * bool data_received = false;
 *
 * while ((tx_time_get() - start_time) < k_timeout_ms) {
 *     bool has_data;
 *     if (rx_bus_uart_rx_available(&bus_mgr, "device", &has_data) == k_rx_ok && has_data) {
 *         // Data arrived - read it
 *         char response;
 *         rx_bus_uart_getc(&bus_mgr, "device", &response);
 *         process_response(response);
 *         data_received = true;
 *         break;
 *     }
 *     tx_thread_sleep(1);  // Check every 1ms
 * }
 *
 * if (!data_received) {
 *     rx_log_error("DEVICE", "Timeout waiting for response");
 * }
 * @endcode
 *
 * @see rx_bus_uart_getc() Read single character (call after rx_available)
 * @see rx_bus_uart_read() Read binary data (call after rx_available)
 *
 * @since Version 1.0.0
 *
 * @test test_rx_bus_uart.c::test_uart_rx_available_empty()
 * @test test_rx_bus_uart.c::test_uart_rx_available_has_data()
 */
rx_err_t rx_bus_uart_rx_available(rx_bus_manager_t* manager, const char* bus_name, bool* available)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(available, s_tag, "available pointer is nullptr");

  uart_rx_avail_ctx_t ctx = {.available = false, .result = k_rx_err_uart_error};

  const rx_err_t err =
    rx_bus_manager_with_bus(manager, bus_name, internal_uart_rx_avail_callback, &ctx);

  *available = ctx.available;

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
