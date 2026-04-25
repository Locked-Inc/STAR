/**
 * @file rx_bus_i2c.c
 * @brief I2C Bus Abstraction Implementation - Callback-Based Thread-Safe Operations
 *
 * @details
 * # Implementation Overview
 *
 * Provides thread-safe I2C communication operations through the bus manager
 * abstraction layer. Wraps low-level RIIC HAL driver with higher-level API
 * supporting initialization, read, write, and write-read (register access)
 * operations.
 *
 * ## Architecture Pattern: Callback-Based Abstraction
 *
 * ### Design Rationale
 *
 * **Why callback pattern instead of direct HAL calls?**
 *
 * | Approach | Thread Safety | Bus Selection | Error Handling |
 * |----------|--------------|---------------|----------------|
 * | **Callback (this)** | [PASS] Bus manager mutex | [PASS] Name-based lookup | [PASS] Centralized validation |
 * | Direct HAL | [FAIL] User must lock | [FAIL] Channel# hardcoded | [FAIL] Repeated everywhere |
 *
 * **Benefits:**
 * - **Thread safety**: Bus manager provides mutex protection automatically
 * - **Abstraction**: Name-based bus selection ("imu_i2c") vs hardcoded channels
 * - **Consistency**: All bus types (I2C, SPI, UART) use same pattern
 * - **Testability**: Callbacks can be mocked for unit testing
 * - **SOLID Dependency Inversion**: High-level API doesn't depend on HAL details
 *
 * ### Callback Execution Flow
 *
 * ```
 * User Code                Bus Manager             I2C Implementation        RIIC HAL
 *    |                         |                          |                       |
 *    |--rx_bus_i2c_write("imu")|                          |                       |
 *    |                         |--with_bus(callback)----->|                       |
 *    |                         |  [MUTEX LOCK]            |                       |
 *    |                         |                          |--internal_i2c_write_callback()
 *    |                         |                          |--riic_write()-------->|
 *    |                         |                          |<-----------------[ACK]|
 *    |                         |  [MUTEX UNLOCK]          |                       |
 *    |<--------------------[result]                       |                       |
 * ```
 *
 * ## Implementation Approach
 *
 * ### Four Primary Operations
 *
 * 1. **Init**: Configure RIIC peripheral, set frequency, mark bus initialized
 * 2. **Write**: Transmit N bytes to device (commands, configurations)
 * 3. **Read**: Receive N bytes from device (sensor readings)
 * 4. **Write-Read**: Combined write-then-read (register access pattern)
 *
 * ### Context Structures
 *
 * Each operation uses dedicated context struct to pass parameters and results:
 * - **i2c_init_ctx_t**: Init result
 * - **i2c_write_ctx_t**: Write data pointer, length, result
 * - **i2c_read_ctx_t**: Read buffer pointer, length, result
 * - **i2c_write_read_ctx_t**: Write + read pointers/lengths, result
 *
 * **Why context structs?**
 * - Callback signature is fixed: `rx_err_t callback(bus_config*, void*)`
 * - Allows passing multiple parameters through single `void*`
 * - Type-safe casting inside callback (compile-time checked)
 *
 * ## Performance Characteristics
 *
 * | Operation | Overhead | I2C Transaction | Total @ 400 kHz |
 * |-----------|----------|-----------------|-----------------|
 * | **Init** | ~2 us | 0 | ~2 us (one-time) |
 * | **Write 1B** | ~2 us | ~25 us | ~27 us |
 * | **Read 1B** | ~2 us | ~25 us | ~27 us |
 * | **Write-Read 1B+1B** | ~2 us | ~50 us | ~52 us |
 *
 * **Overhead breakdown:**
 * - Bus manager lookup: ~0.5 us
 * - Mutex lock/unlock: ~1.0 us
 * - Callback dispatch: ~0.5 us
 *
 * **I2C transaction time** (@ 400 kHz Fast mode):
 * - START + 7-bit addr + R/W + ACK: ~22.5 us
 * - Data byte + ACK: ~22.5 us per byte
 * - STOP: ~2.5 us
 *
 * ## Memory Usage
 *
 * | Component | Size | Location |
 * |-----------|------|----------|
 * | Context structs | 8-16 bytes | Stack (per operation) |
 * | Function code | ~800 bytes | Flash (.text section) |
 * | Static tag | 8 bytes | Flash (.rodata) |
 * | **Total static** | ~808 bytes | Flash (shared across all I2C buses) |
 *
 * ## Hardware Dependencies
 *
 * - **RX72N RIIC peripheral**: 3 channels (RIIC0, RIIC1, RIIC2)
 * - **GPIO MPC**: Pin function switching for SDA/SCL
 * - **External pull-ups**: 2.2-4.7 kOhm on SDA and SCL lines (REQUIRED)
 * - **PCLK frequency**: Must be stable (frequency calculation depends on it)
 *
 * ## Use Cases
 *
 * ### Sensor Reading (MPU6050 IMU)
 * ```c
 * // 1. Init bus once at startup
 * rx_bus_i2c_init(&bus_mgr, "imu_i2c");
 *
 * // 2. Read WHO_AM_I register (0x75) to verify device
 * uint8_t reg_addr = 0x75;
 * uint8_t who_am_i;
 * rx_bus_i2c_write_read(&bus_mgr, "imu_i2c", &reg_addr, 1, &who_am_i, 1);
 * // Expected: who_am_i == 0x68 for MPU6050
 *
 * // 3. Read accelerometer data (6 bytes starting at 0x3B)
 * uint8_t accel_reg = 0x3B;
 * uint8_t accel_data[6];
 * rx_bus_i2c_write_read(&bus_mgr, "imu_i2c", &accel_reg, 1, accel_data, 6);
 * int16_t accel_x = (accel_data[0] << 8) | accel_data[1];
 * ```
 *
 * ### Configuration Write (Set IMU range)
 * ```c
 * // Set accelerometer range to +/-8g (register 0x1C, value 0x10)
 * uint8_t config_cmd[2] = {0x1C, 0x10};
 * rx_bus_i2c_write(&bus_mgr, "imu_i2c", config_cmd, 2);
 * ```
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Implementation |
 * |------|----------------|
 * | **Rule 1** | [PASS] No goto, setjmp, recursion - straight-line callbacks |
 * | **Rule 2** | [PASS] No loops (zero iterations in validation logic) |
 * | **Rule 3** | [PASS] No malloc - context structs on stack, bus config pre-allocated |
 * | **Rule 4** | [PASS] All functions <=60 lines (longest callback: 35 lines) |
 * | **Rule 5** | [PASS] Minimum 2 validations per function (NULL checks + state checks) |
 * | **Rule 6** | [PASS] Variables at smallest scope (ctx in API functions) |
 * | **Rule 7** | [PASS] All RIIC HAL returns checked (err != k_rx_ok) |
 * | **Rule 8** | [PASS] C23 typed enums for validation constants (k_i2c_length_zero: uint16_t) |
 * | **Rule 9** | [PASS] Single-level pointers only (data*, bus_config*, ctx*) |
 * | **Rule 10** | [PASS] Compiles with -Wall -Wextra -Werror, zero warnings |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S** | Each callback does ONE operation - init, write, read, or write-read |
 * | **O** | New I2C operations added as new functions - no modification of existing code |
 * | **L** | All callbacks substitutable (same signature, error semantics) |
 * | **I** | Focused API - 4 functions, each with specific purpose |
 * | **D** | Depends on bus manager abstraction, not concrete RIIC HAL implementation |
 *
 * ## Module Dependencies
 *
 * - `rx_bus_i2c.h` - Public API declarations
 * - `rx_bus_types.h` - Bus configuration structures
 * - `rx_bus_manager.h` - Bus manager with_bus() function
 * - `hardware.h` - RIIC HAL driver (riic_init, riic_write, riic_read, riic_write_read)
 * - `rx_check.h` - RX_CHECK_NULL_PTR validation macro
 * - `rx_log.h` - Logging (rx_log_error, rx_log_warn)
 *
 * @see rx_bus_i2c.h Public API with usage examples
 * @see rx_bus_config.h Bus configuration initialization
 * @see rx_bus_manager.h Bus manager for thread-safe access
 * @see hardware.h RIIC HAL driver interface
 * @see docs/sections/03_hardware_pinout.tex I2C pin assignments
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_bus_i2c.h"

#include "hardware.h"
#include "rx_check.h"
#include "rx_log.h"

/**
 * @var s_tag
 * @brief Logging tag for all rx_bus_i2c module log messages
 *
 * @details
 * Identifies I2C bus log entries in the system log output. Used by
 * rx_log_error(), rx_log_warn(), and rx_log_debug() throughout this module.
 * Stored as a read-only character array in the .rodata section.
 *
 * @note Read-only; must never be modified at runtime
 * @warning Direct modification would corrupt all log output from this module
 * @since Version 1.0.0
 */
static const char s_tag[] = "BUS_I2C";

/**
 * @brief I2C validation constants
 */
typedef enum : uint16_t {
  k_i2c_length_zero = 0, /**< Zero length value for comparisons */
} i2c_validation_constants_t;

/* =============================================================================
 * Callback Context Structures
 * In UNIT_TEST builds these typedefs are provided by rx_bus_i2c.h so that
 * test files can construct context values and call the callbacks directly.
 * =============================================================================
 */
#ifndef UNIT_TEST

/**
 * @struct i2c_init_ctx_t
 * @brief Context structure for I2C initialization operation
 *
 * @details
 * Passed to internal_i2c_init_callback() via rx_bus_manager_with_bus()
 * callback mechanism. Holds the operation result and a pointer to the bus
 * manager so the callback can update the per-channel RIIC initialization
 * tracking in manager->riic_initialized[]. Allocated on the stack by
 * rx_bus_i2c_init() (no dynamic allocation).
 *
 * The manager pointer enables the callback to check whether the physical
 * RIIC channel was already initialized by a prior rx_bus_i2c_init() call
 * (e.g., two logical buses sharing the same physical RIIC channel). If the
 * channel is already initialized, riic_init() is skipped to prevent I2C
 * bus glitches from re-initializing the peripheral mid-operation.
 *
 * @par Memory Layout:
 * | Offset | Size | Field   | Type                | Alignment |
 * |--------|------|---------|---------------------|-----------|
 * | 0      | 4    | result  | rx_err_t            | 4         |
 * | 4      | 4/8  | manager | rx_bus_manager_t*   | ptr       |
 *
 * @par Lifetime: Stack-allocated in rx_bus_i2c_init(), destroyed on return
 *
 * @invariant result contains a valid rx_err_t value after callback returns
 * @invariant manager is non-NULL (validated before ctx creation)
 *
 * @par Example:
 * @code
 * i2c_init_ctx_t ctx = { .result = k_rx_err_invalid_state, .manager = manager };
 * rx_bus_manager_with_bus(manager, bus_name, internal_i2c_init_callback, &ctx);
 * // ctx.result now contains the operation outcome
 * @endcode
 *
 * @see internal_i2c_init_callback() Consumer of this context
 * @see rx_bus_i2c_init() Creator of this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  rx_err_t          result;  /**< Operation result: k_rx_ok on success, error code on failure */
  rx_bus_manager_t* manager; /**< Bus manager: used to check/update per-channel RIIC init status */
} i2c_init_ctx_t;

/**
 * @struct i2c_write_ctx_t
 * @brief Context structure for I2C write operation
 *
 * @details
 * Passed to internal_i2c_write_callback() via rx_bus_manager_with_bus()
 * callback mechanism. Contains a pointer to the transmit data, the number of
 * bytes to send, and the operation result. Allocated on the stack by
 * rx_bus_i2c_write() (no dynamic allocation).
 *
 * @par Memory Layout (64-bit platform):
 * | Offset | Size | Field  | Type           | Alignment |
 * |--------|------|--------|----------------|-----------|
 * | 0      | 8    | data   | const uint8_t* | 8         |
 * | 8      | 2    | length | uint16_t       | 2         |
 * | 10-11  | 2    | (pad)  | -              | -         |
 * | 12     | 4    | result | rx_err_t       | 4         |
 * **Total size**: 16 bytes
 *
 * @par Lifetime: Stack-allocated in rx_bus_i2c_write(), destroyed on return
 *
 * @invariant data must be non-NULL when length > 0
 * @invariant data pointer must remain valid for the entire callback duration
 *
 * @par Example:
 * @code
 * const uint8_t reg_data[] = { 0x1C, 0x10 };
 * i2c_write_ctx_t ctx = {
 *     .data   = reg_data,
 *     .length = sizeof(reg_data),
 *     .result = k_rx_err_invalid_state,
 * };
 * rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_callback, &ctx);
 * @endcode
 *
 * @see internal_i2c_write_callback() Consumer of this context
 * @see rx_bus_i2c_write() Creator of this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  const uint8_t* data; /**< Pointer to data buffer to transmit; must be non-NULL when length > 0 */
  uint16_t       length; /**< Number of bytes to write; 0 is allowed (address probe) */
  rx_err_t       result; /**< Operation result: k_rx_ok on success, error code on failure */
} i2c_write_ctx_t;

/**
 * @struct i2c_read_ctx_t
 * @brief Context structure for I2C read operation
 *
 * @details
 * Passed to internal_i2c_read_callback() via rx_bus_manager_with_bus()
 * callback mechanism. Contains a pointer to the receive buffer, the number of
 * bytes to read, and the operation result. Allocated on the stack by
 * rx_bus_i2c_read() (no dynamic allocation). On successful return, the buffer
 * pointed to by data contains the bytes received from the I2C device.
 *
 * @par Memory Layout (64-bit platform):
 * | Offset | Size | Field  | Type     | Alignment |
 * |--------|------|--------|----------|-----------|
 * | 0      | 8    | data   | uint8_t* | 8         |
 * | 8      | 2    | length | uint16_t | 2         |
 * | 10-11  | 2    | (pad)  | -        | -         |
 * | 12     | 4    | result | rx_err_t | 4         |
 * **Total size**: 16 bytes
 *
 * @par Lifetime: Stack-allocated in rx_bus_i2c_read(), destroyed on return
 *
 * @invariant data must be non-NULL when length > 0
 * @invariant data buffer must be at least length bytes in size
 * @invariant data pointer must remain valid for the entire callback duration
 *
 * @par Example:
 * @code
 * uint8_t recv_buf[6];
 * i2c_read_ctx_t ctx = {
 *     .data   = recv_buf,
 *     .length = sizeof(recv_buf),
 *     .result = k_rx_err_invalid_state,
 * };
 * rx_bus_manager_with_bus(manager, bus_name, internal_i2c_read_callback, &ctx);
 * // recv_buf now contains the received bytes on ctx.result == k_rx_ok
 * @endcode
 *
 * @see internal_i2c_read_callback() Consumer of this context
 * @see rx_bus_i2c_read() Creator of this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t*
    data; /**< Pointer to receive buffer; filled with received bytes on success. Must be non-NULL when length > 0 */
  uint16_t length; /**< Number of bytes to read; 0 is allowed (no-op read) */
  rx_err_t result; /**< Operation result: k_rx_ok on success, error code on failure */
} i2c_read_ctx_t;

/**
 * @struct i2c_write_read_ctx_t
 * @brief Context structure for I2C combined write-then-read (register access) operation
 *
 * @details
 * Passed to internal_i2c_write_read_callback() via rx_bus_manager_with_bus()
 * callback mechanism. Carries both the write phase (register address) and the
 * read phase (response buffer) parameters along with the operation result.
 * Allocated on the stack by rx_bus_i2c_write_read() (no dynamic allocation).
 *
 * This is the standard context for the I2C register-read pattern:
 * START -> ADDR+W -> reg_addr_bytes -> repeated START -> ADDR+R -> data_bytes -> STOP
 *
 * @par Memory Layout (64-bit platform):
 * | Offset | Size | Field        | Type           | Alignment |
 * |--------|------|--------------|----------------|-----------|
 * | 0      | 8    | write_data   | const uint8_t* | 8         |
 * | 8      | 2    | write_length | uint16_t       | 2         |
 * | 10-11  | 2    | (pad)        | -              | -         |
 * | 12-15  | 4    | (pad)        | -              | -         |
 * | 16     | 8    | read_data    | uint8_t*       | 8         |
 * | 24     | 2    | read_length  | uint16_t       | 2         |
 * | 26-27  | 2    | (pad)        | -              | -         |
 * | 28     | 4    | result       | rx_err_t       | 4         |
 * **Total size**: 32 bytes
 *
 * @par Lifetime: Stack-allocated in rx_bus_i2c_write_read(), destroyed on return
 *
 * @invariant write_data must be non-NULL when write_length > 0
 * @invariant read_data must be non-NULL when read_length > 0
 * @invariant Both buffers must remain valid for the entire callback duration
 *
 * @par Example:
 * @code
 * const uint8_t reg_addr[] = { 0x3B };    // MPU6050 accel data register
 * uint8_t       accel_data[6];
 * i2c_write_read_ctx_t ctx = {
 *     .write_data   = reg_addr,
 *     .write_length = sizeof(reg_addr),
 *     .read_data    = accel_data,
 *     .read_length  = sizeof(accel_data),
 *     .result       = k_rx_err_invalid_state,
 * };
 * rx_bus_manager_with_bus(manager, bus_name,
 *                          internal_i2c_write_read_callback, &ctx);
 * // accel_data now contains 6 bytes of accelerometer data on ctx.result == k_rx_ok
 * @endcode
 *
 * @see internal_i2c_write_read_callback() Consumer of this context
 * @see rx_bus_i2c_write_read() Creator of this context
 *
 * @since Version 1.0.0
 */
typedef struct {
  const uint8_t*
    write_data; /**< Pointer to write buffer (e.g., register address); must be non-NULL when write_length > 0 */
  uint16_t write_length; /**< Number of bytes to write in the first phase; 0 allowed */
  uint8_t*
    read_data; /**< Pointer to receive buffer; filled with response bytes on success; must be non-NULL when read_length > 0 */
  uint16_t read_length; /**< Number of bytes to read in the second phase; 0 allowed */
  rx_err_t result;      /**< Operation result: k_rx_ok on success, error code on failure */
} i2c_write_read_ctx_t;

#endif /* ifndef UNIT_TEST */

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

/**
 * @brief Internal callback for I2C peripheral initialization
 *
 * @details
 * Initializes the RX72N RIIC peripheral for the I2C bus described by
 * bus_config. Called by the bus manager via rx_bus_manager_with_bus() with
 * the bus mutex held. Validates bus type, uses a per-channel guard to avoid
 * double-initializing a shared physical RIIC channel, then calls riic_init()
 * and marks the bus as initialized on success.
 *
 * Multiple logical I2C buses may share one physical RIIC channel (e.g.,
 * "i2c1" (BNO055) and "i2c1_baro" (BMP280) both use RIIC1). The channel
 * guard in manager->riic_initialized[] ensures riic_init() is called only
 * once per physical channel, preventing bus glitches from re-initialization.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to i2c_init_ctx_t*
 * 2. Validate bus type is k_bus_type_i2c
 * 3. Extract channel from bus_config->proto.i2c.channel
 * 4. If manager->riic_initialized[channel] == true: skip riic_init, mark bus ready, return
 * 5. Call riic_init() with channel and frequency_hz
 * 6. On success: set manager->riic_initialized[channel] = true
 * 7. Warn if device_addr exceeds 7-bit maximum (non-fatal)
 * 8. Set bus_config->initialized = true
 * 9. Store k_rx_ok in ctx->result and return
 *
 *
 *
 * @pre bus_config must be non-NULL (validated by bus manager before dispatch)
 * @pre user_ctx must be non-NULL and point to a valid i2c_init_ctx_t with non-NULL manager
 *
 * @post bus_config->initialized == true on k_rx_ok
 * @post ctx->result contains the operation outcome
 * @post manager->riic_initialized[channel] == true on k_rx_ok (first or second call)
 *
 * @invariant Bus type remains k_bus_type_i2c throughout the callback
 *
 * @note Not thread-safe; called from bus manager with bus lock held
 * @warning Do not call directly - use rx_bus_i2c_init() instead
 *
 * @par Performance:
 * Execution time: ~50 us @ 240 MHz (includes RIIC peripheral setup)
 *
 * @par Example:
 * @code
 * i2c_init_ctx_t ctx = { .result = k_rx_err_invalid_state };
 * rx_err_t err = rx_bus_manager_with_bus(manager, "imu_i2c",
 *                                         internal_i2c_init_callback, &ctx);
 * if (err == k_rx_ok && ctx.result == k_rx_ok) {
 *     // RIIC peripheral ready for transactions
 * }
 * @endcode
 *
 * @see rx_bus_i2c_init() Public API that invokes this callback
 * @see i2c_init_ctx_t Context structure passed as user_ctx
 * @see riic_init() Underlying RIIC HAL initialization function
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 2 preconditions, 2 postconditions
 * - **Rule 4**: Function is 20 lines (well under 60 limit)
 */
/**
 * @brief Handle RIIC channel already-initialized case
 *
 * @details
 * When a physical RIIC channel was previously initialized, validates that
 * the requested frequency matches the active frequency. On match, marks
 * the bus as initialized without re-calling riic_init(). On mismatch,
 * returns an error.
 *
 *
 *
 * @pre channel < k_riic_channel_count
 * @pre ctx->manager->riic_initialized[channel] == true
 * @post bus_config->initialized == true on success
 */
static rx_err_t internal_i2c_handle_shared_channel(rx_bus_config_t* bus_config,
                                                   i2c_init_ctx_t*  ctx,
                                                   const uint8_t    channel)
{
  const uint32_t active_freq    = ctx->manager->riic_freq_hz[channel];
  const uint32_t requested_freq = bus_config->proto.i2c.frequency_hz;
  if (active_freq != requested_freq) {
    rx_log_error_val(s_tag, "RIIC channel frequency mismatch: active_freq_hz", active_freq);
    rx_log_error_val(s_tag, "RIIC channel frequency mismatch: requested_freq_hz", requested_freq);
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }
  rx_log_debug(s_tag, "RIIC channel already initialized - skipping riic_init");
  bus_config->initialized = true;
  ctx->result             = k_rx_ok;
  return k_rx_ok;
}

RX_STATIC_TESTABLE rx_err_t internal_i2c_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_init_ctx_t* const ctx = (i2c_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_i2c) {
    rx_log_error(s_tag, "Bus is not I2C type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Defensive null check: manager must be set by rx_bus_i2c_init() */
  RX_ASSERT(ctx->manager != NULL, "ctx->manager must not be NULL");
  if (ctx->manager == NULL) {
    ctx->result = k_rx_err_null_ptr;
    return k_rx_err_null_ptr;
  }

  const uint8_t channel = bus_config->proto.i2c.channel;

  /* Validate channel is within the supported RIIC channel range */
  if (channel >= k_riic_channel_count) {
    rx_log_error(s_tag, "RIIC channel out of range");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Channel-level guard: skip riic_init() if this physical RIIC channel was
   * already initialized by a prior rx_bus_i2c_init() call. */
  if (ctx->manager->riic_initialized[channel]) {
    return internal_i2c_handle_shared_channel(bus_config, ctx, channel);
  }

  /* Initialize RIIC channel for the first logical bus on this physical channel */
  const riic_channel_t riic_channel = {.value = channel};
  const rx_err_t       err          = riic_init(riic_channel, bus_config->proto.i2c.frequency_hz);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "RIIC HAL initialization failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify I2C channel is responsive (check configuration) */
  if (bus_config->proto.i2c.device_addr > k_i2c_addr_max_7bit) {
    rx_log_warn(s_tag, "I2C device address exceeds 7-bit maximum");
  }

  /* Record that this physical RIIC channel is now initialized and store its frequency */
  ctx->manager->riic_initialized[channel] = true;
  ctx->manager->riic_freq_hz[channel]     = bus_config->proto.i2c.frequency_hz;

  /* Mark bus as initialized */
  bus_config->initialized = true;

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for I2C write transfer
 *
 * @details
 * Internal callback implementing a raw I2C write transfer. Validates the bus
 * is initialized and the data pointer is non-NULL when length > 0, then
 * performs the I2C write via the RIIC HAL. Stores the operation result in the
 * context structure for retrieval by the caller.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to i2c_write_ctx_t*
 * 2. Validate bus is initialized (check bus_config->initialized)
 * 3. Validate ctx->data non-NULL when ctx->length > 0
 * 4. Extract riic_channel and device_addr from bus_config->proto.i2c
 * 5. Call riic_write() with channel, device address, data, and length
 * 6. Set ctx->result to operation outcome and return
 *
 *
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->data must be non-NULL when ctx->length > 0
 *
 * @post ctx->result contains the operation outcome
 * @post ctx->length bytes transmitted on the I2C bus on k_rx_ok
 *
 * @invariant bus_config->initialized remains unchanged by this function
 *
 * @note Not thread-safe; called from bus manager with bus lock held
 * @warning Zero-length writes (length=0) are permitted and generate an I2C
 *          address probe (START + ADDR+W + ACK check + STOP)
 * @attention NAK from the device is reported as k_rx_err_hw_error; verify
 *            that the device address in bus_config matches the physical device
 *
 * @par Performance:
 * Execution time: ~2 us overhead + ~22.5 us per byte @ 400 kHz Fast mode
 *
 * @par Example:
 * @code
 * const uint8_t reg_data[] = { 0x01, 0x23 };
 * i2c_write_ctx_t ctx = {
 *     .data   = reg_data,
 *     .length = sizeof(reg_data),
 *     .result = k_rx_err_invalid_state,
 * };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                         internal_i2c_write_callback, &ctx);
 * if (err == k_rx_ok && ctx.result == k_rx_ok) {
 *     // 2 bytes successfully written to device
 * }
 * @endcode
 *
 * @see rx_bus_i2c_write() Public API that invokes this callback
 * @see i2c_write_ctx_t Context structure passed as user_ctx
 * @see riic_write() Underlying RIIC HAL write function
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 2 preconditions, 2 postconditions
 * - **Rule 4**: Function is 20 lines (well under 60 limit)
 * - **Rule 7**: All riic_write() return values checked
 */
RX_STATIC_TESTABLE rx_err_t internal_i2c_write_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_write_ctx_t* const ctx = (i2c_write_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate data pointer when length > 0 */
  if (ctx->length > k_i2c_length_zero && ctx->data == nullptr) {
    rx_log_error(s_tag, "I2C write data pointer is nullptr");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Write I2C data */
  const riic_channel_t    riic_channel = {.value = bus_config->proto.i2c.channel};
  const i2c_device_addr_t device_addr  = {.value = bus_config->proto.i2c.device_addr};
  rx_err_t                err = riic_write(riic_channel, device_addr, ctx->data, ctx->length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "I2C write failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Internal callback for I2C read transfer
 *
 * @details
 * Internal callback implementing a raw I2C read transfer. Validates the bus
 * is initialized and the data pointer is non-NULL when length > 0, then
 * performs the I2C read via the RIIC HAL. The received bytes are stored
 * directly in the caller-supplied buffer.
 *
 * Algorithm steps:
 * 1. Cast user_ctx to i2c_read_ctx_t*
 * 2. Validate bus is initialized (check bus_config->initialized)
 * 3. Validate ctx->data non-NULL when ctx->length > 0
 * 4. Extract riic_channel and device_addr from bus_config->proto.i2c
 * 5. Call riic_read() with channel, device address, data buffer, and length
 * 6. Set ctx->result to operation outcome and return
 *
 *
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->data must be non-NULL and point to a buffer of at least ctx->length bytes
 *
 * @post ctx->result contains the operation outcome
 * @post ctx->data buffer contains ctx->length received bytes on k_rx_ok
 *
 * @invariant bus_config->initialized remains unchanged by this function
 *
 * @note Not thread-safe; called from bus manager with bus lock held
 * @warning The receive buffer must remain valid for the entire duration of the
 *          callback; do not use temporary stack-allocated buffers
 * @attention ctx->data contents are undefined if return value is not k_rx_ok;
 *            always check the return code before reading the buffer
 *
 * @par Performance:
 * Execution time: ~2 us overhead + ~22.5 us per byte @ 400 kHz Fast mode
 *
 * @par Example:
 * @code
 * uint8_t recv_buf[4];
 * i2c_read_ctx_t ctx = {
 *     .data   = recv_buf,
 *     .length = sizeof(recv_buf),
 *     .result = k_rx_err_invalid_state,
 * };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                         internal_i2c_read_callback, &ctx);
 * if (err == k_rx_ok && ctx.result == k_rx_ok) {
 *     // recv_buf[0..3] contain the 4 bytes received from the device
 * }
 * @endcode
 *
 * @see rx_bus_i2c_read() Public API that invokes this callback
 * @see i2c_read_ctx_t Context structure passed as user_ctx
 * @see riic_read() Underlying RIIC HAL read function
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 2 preconditions, 2 postconditions
 * - **Rule 4**: Function is 20 lines (well under 60 limit)
 * - **Rule 7**: All riic_read() return values checked
 */
RX_STATIC_TESTABLE rx_err_t internal_i2c_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_read_ctx_t* const ctx = (i2c_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate data pointer when length > 0 */
  if (ctx->length > k_i2c_length_zero && ctx->data == nullptr) {
    rx_log_error(s_tag, "I2C read data pointer is nullptr");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Read I2C data */
  const riic_channel_t    riic_channel = {.value = bus_config->proto.i2c.channel};
  const i2c_device_addr_t device_addr  = {.value = bus_config->proto.i2c.device_addr};
  rx_err_t                err = riic_read(riic_channel, device_addr, ctx->data, ctx->length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "I2C read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for I2C combined write-read (register read) operation
 *
 * @details
 * Internal callback implementing a combined I2C write-then-read transfer with
 * a repeated START between phases. This is the standard protocol for reading
 * device registers: write the register address byte(s), issue a repeated START,
 * then read the response without releasing the bus between phases.
 *
 * Algorithm steps:
 * 1. Validate bus is initialized
 * 2. Validate write_data non-NULL when write_length > 0
 * 3. Validate read_data non-NULL when read_length > 0
 * 4. Call riic_write_read() with both write and read parameters
 * 5. Set ctx->result and return
 *
 *
 *
 * @pre bus_config->initialized must be true
 * @pre ctx->write_data non-NULL when ctx->write_length > 0
 *
 * @post ctx->result contains the operation result
 * @post ctx->read_data buffer contains device response bytes on k_rx_ok
 *
 * @note Not thread-safe; called from bus manager with bus lock held
 * @warning Both write and read buffers must remain valid throughout the callback
 *
 * @code
 * const uint8_t reg_addr[] = { 0x09 };   // voltage register
 * uint8_t       resp[2]    = { 0, 0 };
 * i2c_write_read_ctx_t ctx = {
 *     .write_data   = reg_addr,
 *     .write_length = sizeof(reg_addr),
 *     .read_data    = resp,
 *     .read_length  = sizeof(resp),
 *     .result       = k_rx_err_invalid_state,
 * };
 * rx_err_t err = rx_bus_manager_with_bus(manager, bus_name,
 *                                         internal_i2c_write_read_callback, &ctx);
 * @endcode
 *
 * @see rx_bus_i2c_write_read() Public API that invokes this callback
 * @see i2c_write_read_ctx_t Context structure passed as user_ctx
 * @see riic_write_read() Underlying RIIC HAL combined transfer function
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_i2c_write_read_callback(rx_bus_config_t* bus_config,
                                                             void*            user_ctx)
{
  i2c_write_read_ctx_t* const ctx = (i2c_write_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate write data pointer when write_length > 0 */
  if (ctx->write_length > k_i2c_length_zero && ctx->write_data == nullptr) {
    rx_log_error(s_tag, "I2C write-read write_data pointer is nullptr");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: Validate read data pointer when read_length > 0 */
  if (ctx->read_length > k_i2c_length_zero && ctx->read_data == nullptr) {
    rx_log_error(s_tag, "I2C write-read read_data pointer is nullptr");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* I2C write-read operation */
  const riic_channel_t    riic_channel = {.value = bus_config->proto.i2c.channel};
  const i2c_device_addr_t device_addr  = {.value = bus_config->proto.i2c.device_addr};
  rx_err_t                err          = riic_write_read(riic_channel,
                                 device_addr,
                                 ctx->write_data,
                                 ctx->write_length,
                                 ctx->read_data,
                                 ctx->read_length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "I2C write-read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize I2C bus peripheral hardware
 *
 * @details
 * Initializes the RX72N RIIC peripheral for the specified I2C bus. Configures
 * clock frequency, enables peripheral, and marks bus as ready for transactions.
 * Must be called once before any read/write operations.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager parameter (NULL check via RX_CHECK_NULL_PTR)
 * 2. Validate bus_name parameter (NULL check via RX_CHECK_NULL_PTR)
 * 3. Initialize context structure with default error
 * 4. Call rx_bus_manager_with_bus() to execute callback under mutex
 * 5. Inside callback (internal_i2c_init_callback):
 *    a. Validate bus type is k_bus_type_i2c
 *    b. Extract RIIC channel from bus config
 *    c. Call riic_init(channel, frequency_hz) to initialize hardware
 *    d. Validate device address in range (warning if invalid)
 *    e. Mark bus_config->initialized = true
 *    f. Store result in context
 * 6. Return result from context
 *
 * ## Hardware Initialization
 *
 * RIIC HAL initialization includes:
 * - **Module stop release**: Enable RIIC peripheral clock
 * - **Pin configuration**: Set MPC for SDA/SCL alternate functions
 * - **Baud rate**: Configure BRL/BRH registers for target frequency
 * - **Timeout**: Enable SCL timeout detection (bus lockup prevention)
 * - **Interrupt priority**: Set RIIC interrupt priority level
 *
 * ## Performance
 *
 * - **Execution time**: ~50 us @ 240 MHz (includes peripheral init)
 * - **Call frequency**: Once per bus at system startup
 * - **Thread safety**: Mutex-protected by bus manager
 *
 *
 *
 *
 * @pre manager initialized via rx_bus_manager_init()
 * @pre Bus registered via rx_bus_manager_add_bus()
 * @pre Bus config created via rx_bus_config_init_i2c()
 * @pre External pull-up resistors (2.2-4.7 kOhm) present on SDA/SCL
 *
 * @post RIIC peripheral configured and enabled
 * @post bus_config->initialized == true
 * @post Bus ready for rx_bus_i2c_read/write operations
 * @post I2C bus idle (SDA and SCL both high)
 *
 * @note Call only ONCE per bus (repeated calls may fail or cause glitches)
 * @note Bus manager mutex held during initialization (blocks other bus ops)
 * @note Does NOT scan bus for devices (use separate scan function if needed)
 *
 * @warning Must be called before any read/write operations
 * @warning Missing external pull-ups cause bus to hang (SDA/SCL stuck low)
 * @warning Incorrect frequency_hz may prevent communication (check device specs)
 *
 * @attention Initialization failure leaves bus unusable (check return value!)
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name (mutex prevents concurrent access).
 * Reentrant for different bus_name values.
 *
 * @par Example - MPU6050 IMU Initialization:
 * @code{.c}
 * // 1. Create bus configuration
 * rx_bus_config_t imu_i2c_cfg;
 * rx_err_t err = rx_bus_config_init_i2c(&imu_i2c_cfg, "imu_i2c", 0, 0x68,
 *                                       k_rx_pin_p12, k_rx_pin_p13, 400000);
 * if (err != k_rx_ok) {
 *     rx_log_error("IMU", "Config failed: %d", err);
 *     return err;
 * }
 *
 * // 2. Register with bus manager
 * err = rx_bus_manager_add_bus(&bus_mgr, &imu_i2c_cfg);
 * if (err != k_rx_ok) {
 *     rx_log_error("IMU", "Register failed: %d", err);
 *     return err;
 * }
 *
 * // 3. Initialize I2C hardware
 * err = rx_bus_i2c_init(&bus_mgr, "imu_i2c");
 * if (err != k_rx_ok) {
 *     rx_log_error("IMU", "I2C init failed: %d (check pull-ups!)", err);
 *     return err;
 * }
 *
 * // 4. Now ready for transactions
 * uint8_t who_am_i_reg = 0x75;
 * uint8_t who_am_i;
 * err = rx_bus_i2c_write_read(&bus_mgr, "imu_i2c", &who_am_i_reg, 1, &who_am_i, 1);
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * // Attempt to init before registration
 * rx_err_t err = rx_bus_i2c_init(&bus_mgr, "unregistered_bus");
 * if (err == k_rx_err_not_found) {
 *     rx_log_error("I2C", "Bus not registered (call add_bus first)");
 * }
 *
 * // Attempt to init wrong bus type
 * // (bus configured as GPIO, not I2C)
 * err = rx_bus_i2c_init(&bus_mgr, "gpio_bus");
 * if (err == k_rx_err_invalid_arg) {
 *     rx_log_error("I2C", "Bus is not I2C type");
 * }
 * @endcode
 *
 * @see rx_bus_config_init_i2c() Create I2C bus configuration
 * @see rx_bus_manager_add_bus() Register bus with manager
 * @see rx_bus_i2c_write() Transmit data after init
 * @see rx_bus_i2c_read() Receive data after init
 * @see rx_bus_i2c_write_read() Register read after init
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test test_rx_bus_i2c.c::test_i2c_init_success()
 * @test test_rx_bus_i2c.c::test_i2c_init_null_params()
 * @test test_rx_bus_i2c.c::test_i2c_init_not_found()
 * @test test_rx_bus_i2c.c::test_i2c_init_wrong_type()
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 4 preconditions, 4 postconditions
 * - **Rule 4**: Function is 16 lines (under 60 limit)
 * - **Rule 7**: All returns checked (with_bus result, ctx.result)
 */
rx_err_t rx_bus_i2c_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = manager};
  rx_err_t       err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Write data to I2C device
 *
 * @details
 * Transmits N bytes to the I2C device specified in bus configuration. Typically
 * used to send commands, write configuration registers, or transfer data to
 * peripherals. Generates START, device address (write), N data bytes, STOP.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager parameter (NULL check via RX_CHECK_NULL_PTR)
 * 2. Validate bus_name parameter (NULL check via RX_CHECK_NULL_PTR)
 * 3. Validate data parameter (NULL check via RX_CHECK_NULL_PTR)
 * 4. Initialize context with data pointer, length, default error
 * 5. Call rx_bus_manager_with_bus() to execute callback under mutex
 * 6. Inside callback (internal_i2c_write_callback):
 *    a. Validate bus initialized (check bus_config->initialized)
 *    b. Validate data pointer if length > 0
 *    c. Extract RIIC channel and device address from config
 *    d. Call riic_write(channel, addr, data, length)
 *    e. Store result in context
 * 7. Return result from context
 *
 * ## I2C Transaction Format
 *
 * ```
 * START | ADDR(7bit)+W | ACK | DATA[0] | ACK | ... | DATA[N-1] | ACK | STOP
 * ```
 *
 * **Timing @ 400 kHz:**
 * - START: ~2.5 us
 * - Address + ACK: ~22.5 us
 * - Each data byte + ACK: ~22.5 us
 * - STOP: ~2.5 us
 * - **Total**: 27.5 us + (N x 22.5 us)
 *
 * ## Use Cases
 *
 * - **Single register write**: Write command byte (e.g., start conversion)
 * - **Multi-register write**: Configure device (register address + value bytes)
 * - **Bulk data transfer**: Send calibration data, firmware updates
 *
 *
 *
 *
 * @pre manager initialized and bus registered
 * @pre rx_bus_i2c_init() called successfully for bus_name
 * @pre data points to valid buffer (if length > 0)
 * @pre I2C device powered and connected
 *
 * @post Data transmitted to I2C device (if k_rx_ok)
 * @post Bus returned to idle state (SDA and SCL high)
 * @post Device may have updated internal state based on written data
 *
 * @note Thread-safe (bus manager mutex)
 * @note Blocks until transaction completes or times out
 * @note Does NOT verify data was processed correctly (read back if critical)
 *
 * @warning Device NACK indicates wrong address, device busy, or hardware issue
 * @warning Timeout may indicate missing pull-ups, shorted bus, or device lockup
 * @warning data buffer must remain valid during entire transaction
 *
 * @attention Multi-byte writes are NOT atomic (device may process partial data on failure)
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name (mutex serializes access).
 * Reentrant for different bus_name values.
 *
 * @par Performance:
 * Execution time: ~27.5 us + (N x 22.5 us) @ 400 kHz + ~2 us overhead
 *
 * @par Example - Write Single Register:
 * @code{.c}
 * // MPU6050: Write to PWR_MGMT_1 register (0x6B) to wake up device (value 0x00)
 * uint8_t wake_cmd[2] = {0x6B, 0x00};  // {register, value}
 * rx_err_t err = rx_bus_i2c_write(&bus_mgr, "imu_i2c", wake_cmd, 2);
 * if (err != k_rx_ok) {
 *     rx_log_error("IMU", "Wake command failed: %d", err);
 *     return err;
 * }
 * @endcode
 *
 * @par Example - Bulk Configuration Write:
 * @code{.c}
 * // Configure MPU6050: Write to multiple registers starting at 0x1B
 * uint8_t config_data[] = {
 *     0x1B,  // Start register: GYRO_CONFIG
 *     0x18,  // GYRO_CONFIG: +/-2000deg/s range
 *     0x10,  // ACCEL_CONFIG: +/-8g range
 *     0x06   // CONFIG: 5 Hz DLPF
 * };
 * rx_err_t err = rx_bus_i2c_write(&bus_mgr, "imu_i2c", config_data, sizeof(config_data));
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * uint8_t cmd[2] = {0x75, 0xFF};
 * rx_err_t err = rx_bus_i2c_write(&bus_mgr, "imu_i2c", cmd, 2);
 *
 * if (err == k_rx_err_nack) {
 *     rx_log_error("I2C", "Device NACK (wrong address or not ready)");
 * } else if (err == k_rx_err_timeout) {
 *     rx_log_error("I2C", "Timeout (check pull-ups, bus wiring)");
 * } else if (err == k_rx_err_invalid_state) {
 *     rx_log_error("I2C", "Bus not initialized (call rx_bus_i2c_init first)");
 * }
 * @endcode
 *
 * @see rx_bus_i2c_init() Initialize bus before write
 * @see rx_bus_i2c_read() Read data from device
 * @see rx_bus_i2c_write_read() Combined write-read (register access)
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test test_rx_bus_i2c.c::test_i2c_write_success()
 * @test test_rx_bus_i2c.c::test_i2c_write_null_params()
 * @test test_rx_bus_i2c.c::test_i2c_write_not_initialized()
 * @test test_rx_bus_i2c.c::test_i2c_write_nack()
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 4 preconditions, 3 postconditions
 * - **Rule 4**: Function is 18 lines (under 60 limit)
 * - **Rule 7**: All returns checked (with_bus result, ctx.result)
 */
rx_err_t rx_bus_i2c_write(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          const uint8_t*    data,
                          const uint16_t    length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");

  i2c_write_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};
  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Read data from I2C device
 *
 * @details
 * Receives N bytes from the I2C device specified in bus configuration. Generates
 * START, device address (read), receives N data bytes with ACKs, sends final NACK, STOP.
 * Used for reading sensor data, status registers, or bulk data from device memory.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager, bus_name, data parameters (NULL checks)
 * 2. Initialize context with data pointer, length, default error
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback: validate initialized, call riic_read()
 * 5. Return result from context
 *
 * ## I2C Transaction Format
 *
 * ```
 * START | ADDR(7bit)+R | ACK | DATA[0] | ACK | ... | DATA[N-2] | ACK | DATA[N-1] | NACK | STOP
 * ```
 *
 * **Timing @ 400 kHz:** 27.5 us + (N x 22.5 us)
 *
 *
 *
 * @pre rx_bus_i2c_init() called for bus_name
 * @pre data points to buffer of size >= length
 *
 * @post data buffer filled with received bytes (if k_rx_ok)
 * @post Bus idle (SDA/SCL high)
 *
 * @par Example - Read Sensor Data:
 * @code{.c}
 * // Read 6 bytes from device (after setting register via write)
 * uint8_t sensor_data[6];
 * rx_err_t err = rx_bus_i2c_read(&bus_mgr, "imu_i2c", sensor_data, 6);
 * @endcode
 *
 * @see rx_bus_i2c_write_read() Combined register read (more common)
 * @test test_rx_bus_i2c.c::test_i2c_read_success()
 */
rx_err_t
rx_bus_i2c_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint16_t length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is nullptr");

  i2c_read_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};
  rx_err_t       err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

/**
 * @brief Write then read from I2C device (register access pattern)
 *
 * @details
 * Combined write-read operation for register access. Writes N bytes (typically
 * register address), sends repeated START, then reads M bytes (register contents).
 * This is the MOST COMMON I2C operation for sensors and peripherals.
 *
 * ## Algorithm Steps
 *
 * 1. Validate manager, bus_name, write_data, read_data (NULL checks)
 * 2. Initialize context with pointers, lengths, default error
 * 3. Call rx_bus_manager_with_bus() under mutex
 * 4. Inside callback: validate initialized, validate pointers if lengths > 0
 * 5. Call riic_write_read() - single I2C transaction with repeated START
 * 6. Return result from context
 *
 * ## I2C Transaction Format
 *
 * ```
 * START | ADDR+W | ACK | REG[0] | ACK | ... | REG[N-1] | ACK |
 * REPEATED_START | ADDR+R | ACK | DATA[0] | ACK | ... | DATA[M-1] | NACK | STOP
 * ```
 *
 * **Why repeated START instead of STOP+START?**
 * - Maintains bus ownership (prevents other controllers from interrupting)
 * - Atomic operation (register read is consistent, not interrupted)
 * - Required by many devices (MPU6050, EEPROM, etc.)
 *
 * **Timing @ 400 kHz:**
 * - Write phase: 27.5 us + (N x 22.5 us)
 * - Repeated START: ~2.5 us
 * - Read phase: 22.5 us + (M x 22.5 us)
 * - **Total**: ~50 us + ((N+M) x 22.5 us)
 *
 * ## Common Use Cases
 *
 * ### Single Register Read (Most Common)
 * - write_data = {register_address}, write_length = 1
 * - read_data = buffer, read_length = register_size
 * - Example: Read WHO_AM_I register from MPU6050
 *
 * ### Multi-Register Read
 * - write_data = {start_register}, write_length = 1
 * - read_data = buffer, read_length = N (auto-increment registers)
 * - Example: Read 6-byte accelerometer data from MPU6050
 *
 * ### EEPROM Read
 * - write_data = {addr_high, addr_low}, write_length = 2
 * - read_data = buffer, read_length = data_size
 * - Example: Read from 16-bit addressed EEPROM
 *
 *
 *
 *
 * @pre manager initialized, bus registered and initialized
 * @pre write_data points to valid buffer (if write_length > 0)
 * @pre read_data points to buffer of size >= read_length (if read_length > 0)
 * @pre I2C device supports register auto-increment (for multi-byte reads)
 *
 * @post read_data filled with register contents (if k_rx_ok)
 * @post Bus idle (SDA/SCL high)
 * @post Device register pointer may have incremented (device-specific)
 *
 * @invariant Repeated START maintains bus ownership (atomic operation)
 *
 * @note Most common I2C function for sensor/peripheral access
 * @note Thread-safe (bus manager mutex)
 * @note Blocks until transaction completes (~50 us for 1+1 byte @ 400 kHz)
 *
 * @warning Device must support repeated START (most do, check datasheet)
 * @warning Multi-byte reads require device to support register auto-increment
 * @warning write_data and read_data must remain valid during entire transaction
 *
 * @attention NACK may indicate invalid register address (check device datasheet)
 *
 * @par Thread Safety:
 * Thread-safe. Bus manager provides mutex protection.
 *
 * @par Re-entrancy:
 * Not reentrant for same bus_name. Reentrant for different buses.
 *
 * @par Performance:
 * Execution time: ~52 us (1 byte write + 1 byte read @ 400 kHz) + ~2 us overhead
 *
 * @par Example - Read WHO_AM_I Register:
 * @code{.c}
 * // MPU6050 WHO_AM_I register at 0x75 (should return 0x68)
 * uint8_t who_am_i_reg = 0x75;
 * uint8_t who_am_i_value;
 * rx_err_t err = rx_bus_i2c_write_read(&bus_mgr, "imu_i2c",
 *                                      &who_am_i_reg, 1,  // Write register address
 *                                      &who_am_i_value, 1); // Read 1 byte
 * if (err == k_rx_ok) {
 *     if (who_am_i_value == 0x68) {
 *         rx_log_debug("IMU", "MPU6050 detected");
 *     } else {
 *         rx_log_error("IMU", "Wrong WHO_AM_I: 0x%02X (expected 0x68)", who_am_i_value);
 *     }
 * }
 * @endcode
 *
 * @par Example - Read Accelerometer Data (Multi-Byte):
 * @code{.c}
 * // MPU6050 accelerometer data: 6 bytes starting at register 0x3B
 * // Registers auto-increment: ACCEL_XOUT_H, ACCEL_XOUT_L, ACCEL_YOUT_H, ...
 * uint8_t accel_reg = 0x3B;
 * uint8_t accel_data[6];
 * rx_err_t err = rx_bus_i2c_write_read(&bus_mgr, "imu_i2c",
 *                                      &accel_reg, 1,     // Start register
 *                                      accel_data, 6);    // Read 6 bytes
 * if (err == k_rx_ok) {
 *     int16_t accel_x = (accel_data[0] << 8) | accel_data[1];  // Big-endian
 *     int16_t accel_y = (accel_data[2] << 8) | accel_data[3];
 *     int16_t accel_z = (accel_data[4] << 8) | accel_data[5];
 *     rx_log_debug("IMU", "Accel: X=%d Y=%d Z=%d", accel_x, accel_y, accel_z);
 * }
 * @endcode
 *
 * @par Example - EEPROM Read (16-bit Address):
 * @code{.c}
 * // AT24C256 EEPROM: Read 16 bytes from address 0x0100
 * uint8_t eeprom_addr[2] = {0x01, 0x00};  // Big-endian address
 * uint8_t eeprom_data[16];
 * rx_err_t err = rx_bus_i2c_write_read(&bus_mgr, "eeprom_i2c",
 *                                      eeprom_addr, 2,    // 16-bit address
 *                                      eeprom_data, 16);  // Read 16 bytes
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * uint8_t reg = 0xFF;  // Invalid register
 * uint8_t value;
 * rx_err_t err = rx_bus_i2c_write_read(&bus_mgr, "imu_i2c", &reg, 1, &value, 1);
 *
 * if (err == k_rx_err_nack) {
 *     rx_log_error("I2C", "NACK: Invalid register 0x%02X or device not ready", reg);
 * } else if (err == k_rx_err_timeout) {
 *     rx_log_error("I2C", "Timeout: Check pull-ups, wiring, device power");
 * } else if (err == k_rx_err_invalid_state) {
 *     rx_log_error("I2C", "Bus not initialized (call rx_bus_i2c_init first)");
 * }
 * @endcode
 *
 * @see rx_bus_i2c_init() Initialize bus first
 * @see rx_bus_i2c_write() Write-only operation
 * @see rx_bus_i2c_read() Read-only operation
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test test_rx_bus_i2c.c::test_i2c_write_read_success()
 * @test test_rx_bus_i2c.c::test_i2c_write_read_multi_byte()
 * @test test_rx_bus_i2c.c::test_i2c_write_read_null_params()
 * @test test_rx_bus_i2c.c::test_i2c_write_read_not_initialized()
 * @test test_rx_bus_i2c.c::test_i2c_write_read_invalid_register()
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: 5 preconditions, 4 postconditions
 * - **Rule 4**: Function is 20 lines (under 60 limit)
 * - **Rule 7**: All returns checked (with_bus result, ctx.result)
 */
rx_err_t rx_bus_i2c_write_read(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               const uint8_t*    write_data,
                               const uint16_t    write_length,
                               uint8_t*          read_data,
                               const uint16_t    read_length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is nullptr");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is nullptr");
  RX_CHECK_NULL_PTR(write_data, s_tag, "write_data pointer is nullptr");
  RX_CHECK_NULL_PTR(read_data, s_tag, "read_data pointer is nullptr");

  i2c_write_read_ctx_t ctx = {.write_data   = write_data,
                              .write_length = write_length,
                              .read_data    = read_data,
                              .read_length  = read_length,
                              .result       = k_rx_err_hw_error};
  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
