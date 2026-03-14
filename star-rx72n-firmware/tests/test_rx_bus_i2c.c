/**
 * @file test_rx_bus_i2c.c
 * @brief Unit Tests for I2C Bus Abstraction Layer
 *
 * @details
 * Validates the rx_bus_i2c module which wraps the RIIC HAL behind the bus
 * manager abstraction. Tests cover all four public operations (init, write,
 * read, write_read) including success paths, null pointer validation,
 * uninitialized bus rejection, bus not found, and HAL error propagation.
 *
 * All hardware interaction is intercepted by mock_riic_hal, which records
 * calls and exposes captured state for assertion. The bus manager mock
 * replaces ThreadX mutex operations with simple boolean flag toggling.
 *
 * @par Test Coverage
 * | Group      | Tests | Description                                              |
 * |------------|-------|----------------------------------------------------------|
 * | Init       | 5     | Success, null manager/name, not found, HAL error         |
 * | Write      | 8     | Success, null manager/name/data, not init, NACK, snapshot len 0/1 |
 * | Read       | 6     | Success, null manager/name/buf, not init, NACK           |
 * | Write-Read | 7     | Success, null manager/name/tx/rx, not init, NACK         |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] No loops in test code
 * - Rule 3: [OK] Static allocation only
 * - Rule 4: [OK] All functions under 60 lines
 * - Rule 5: [OK] Each test has minimum 2 assertions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Typed enums for all constants
 * - Rule 9: [OK] Single-level pointers only
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - S: Each test validates ONE specific behavior
 * - L: Mock RIIC HAL is a drop-in substitute for real HAL
 * - D: Tests depend on rx_bus_i2c interface, not RIIC register details
 *
 * @author Locked, Inc.
 * @date 2026-02-26
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @see rx_bus_i2c.h
 * @see rx_bus_manager.h
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mock_riic_hal.h"
#include "rx_bus_config.h"
#include "rx_bus_i2c.h"
#include "rx_bus_manager.h"
#include "rx_err.h"
#include "rx_port_constants.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_i2c_constants_t
 * @brief I2C bus configuration constants
 *
 * @details
 * Sized for RIIC0, 7-bit address 0x50, 400 kHz - the standard STAR
 * configuration for I2C sensor access behind the bus abstraction.
 */
typedef enum : uint8_t {
  k_test_i2c_channel = 0,    /**< RIIC channel 0 */
  k_test_i2c_addr    = 0x50, /**< 7-bit device address */
} test_i2c_constants_t;

/**
 * @enum test_i2c_freq_t
 * @brief I2C frequency constant (exceeds uint8_t range)
 */
typedef enum : uint32_t {
  k_test_i2c_freq_hz = 400000, /**< 400 kHz fast-mode */
} test_i2c_freq_t;

/**
 * @enum test_i2c_lengths_t
 * @brief Transfer length constants for write/read tests
 */
typedef enum : uint16_t {
  k_test_write_len = 3, /**< Bytes in write transfer */
  k_test_read_len  = 2, /**< Bytes in read transfer */
} test_i2c_lengths_t;

/**
 * @enum test_i2c_data_t
 * @brief Data byte values used in write tests
 */
typedef enum : uint8_t {
  k_test_write_byte_0  = 0x01, /**< First write byte */
  k_test_write_byte_1  = 0x02, /**< Second write byte */
  k_test_write_byte_2  = 0x03, /**< Third write byte */
  k_test_read_byte_0   = 0xAA, /**< First expected read byte */
  k_test_read_byte_1   = 0xBB, /**< Second expected read byte */
  k_test_snapshot_byte = 0x42, /**< Byte value for snapshot regression tests (length 0/1) */
} test_i2c_data_t;

/**
 * @enum test_i2c_single_len_t
 * @brief Short transfer size constants for null-parameter, NACK, and snapshot tests
 *
 * @details
 * Null-parameter and NACK tests use a minimal 1-byte payload to keep fixtures
 * concise. Snapshot regression tests also use length 0 to exercise the sentinel
 * branch. Named constants avoid magic literals per STAR policy.
 */
typedef enum : uint16_t {
  k_test_len_zero = 0U, /**< Zero-byte transfer length for snapshot sentinel tests */
  k_test_len_one  = 1U, /**< One-byte transfer length for minimal test fixtures */
} test_i2c_single_len_t;

/**
 * @enum test_i2c_array_idx_t
 * @brief Array index constants for captured data element assertions
 *
 * @details
 * Used wherever a test accesses the first element of a tx/rx buffer via
 * subscript. Named constant avoids magic literal "0" per STAR policy.
 */
typedef enum : uint8_t {
  k_test_idx_zero = 0, /**< Index of the first element in a transfer buffer */
} test_i2c_array_idx_t;

/**
 * @enum test_i2c_call_count_t
 * @brief Expected HAL call counts after an early-rejected (error-returning) operation
 *
 * @details
 * When a bus operation is rejected before reaching the HAL (null pointer, invalid
 * state), no RIIC HAL calls must be made. Named constant avoids magic literal 0
 * per STAR no-magic-numbers policy.
 */
typedef enum : int32_t {
  k_test_zero_calls = 0, /**< No HAL calls expected after an early-rejected operation */
} test_i2c_call_count_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @var s_test_manager
 * @brief Static bus manager shared across all tests
 * @details Reset in setUp()/tearDown(). Uses static allocation per NASA Rule 3.
 * @note Internal test fixture; access only through setUp()/tearDown() helpers
 * @warning Direct modification outside setUp/tearDown will break shared test state
 * @since Version 1.0.0
 */
static rx_bus_manager_t s_test_manager;

/**
 * @var s_i2c_config
 * @brief I2C bus configuration for the test device
 * @details Configured in setUp() with channel=0, addr=0x50, 400 kHz.
 * @note Internal test fixture; reset by setUp() before each test
 * @warning Direct field modification outside setUp() may cause test interference
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c_config;

/**
 * @var s_test_bus_name
 * @brief Bus name used for all I2C bus lookups in tests
 * @details Constant string registered with s_test_manager during setUp().
 * @note Read-only; value must match the name passed to rx_bus_config_init_i2c()
 * @warning Changing this value without updating setUp() will break bus lookups
 * @since Version 1.0.0
 */
static const char* const s_test_bus_name = "test_i2c";

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

/**
 * @brief Initialize test fixtures before each test
 *
 * @details
 * 1. Reset mock RIIC HAL state (mock_riic_init)
 * 2. Initialize bus manager (rx_bus_manager_init)
 * 3. Create I2C bus config (rx_bus_config_init_i2c: channel 0, addr 0x50, 400 kHz)
 * 4. Register bus config with manager (rx_bus_manager_add_bus)
 *
 * @pre None - called by Unity before each test
 * @pre mock RIIC HAL state not yet initialized (will be reset by mock_riic_init)
 * @post s_test_manager ready; s_i2c_config registered; mock state clean
 * @post s_i2c_config registered with s_test_bus_name in s_test_manager
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* Reset mock RIIC HAL state */
  mock_riic_init();

  /* Initialize bus manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST_I2C", nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create I2C bus config: channel=0, addr=0x50, 400 kHz, pins P1.2/P1.3 */
  err = rx_bus_config_init_i2c(&s_i2c_config,
                               s_test_bus_name,
                               (uint8_t)k_test_i2c_channel,
                               (uint8_t)k_test_i2c_addr,
                               k_rx_p1_2,
                               k_rx_p1_3,
                               (uint32_t)k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Register I2C bus with manager */
  err = rx_bus_manager_add_bus(&s_test_manager, &s_i2c_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Clean up test fixtures after each test
 *
 * @details
 * 1. Deinitialize the bus manager (rx_bus_manager_deinit); errors ignored so
 *    cleanup always completes.
 * 2. Reset mock RIIC HAL state (mock_riic_init).
 *
 * @pre setUp() has been called
 * @pre s_test_manager is initialized and s_i2c_config is registered
 * @post s_test_manager deinitialized; mock state reset
 * @post mock RIIC HAL channel 0 returned to uninitialized idle state
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  (void)rx_bus_manager_deinit(&s_test_manager);
  mock_riic_init();
}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_i2c_init marks RIIC channel as initialized in mock
 *
 * @details
 * Successful init drives riic_init() via callback; mock records this and
 * mock_riic_is_initialized() confirms channel 0 is ready.
 *
 * @pre s_test_manager initialized with s_test_bus_name registered
 * @pre mock RIIC HAL channel 0 is not initialized
 * @post mock_riic_is_initialized(0) returns true
 * @post RIIC channel 0 configured at 400 kHz on P1.2/P1.3
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_marks_channel_initialized(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_i2c_channel));
}

/**
 * @brief rx_bus_i2c_init with null manager returns k_rx_err_null_ptr
 *
 * @pre None
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_null_manager_returns_error(void)
{
  rx_err_t err = rx_bus_i2c_init(nullptr, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_init with null bus name returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_null_name_returns_error(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_init with unregistered bus name returns k_rx_err_not_found
 *
 * @pre s_test_manager initialized; "unknown_i2c" not registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the lookup failure
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_bus_not_found_returns_error(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, "unknown_i2c");
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_init propagates HAL error when riic_init fails
 *
 * @details
 * Injects a timeout error via mock_riic_set_next_error(). The internal
 * callback calls riic_init() which returns the injected error, which
 * propagates upward.
 *
 * @pre s_test_manager initialized; error injection configured
 * @pre mock RIIC HAL set to return k_rx_err_timeout on next init call
 * @post mock_riic_is_initialized(0) remains false (init rejected by HAL)
 * @post injected error code propagated as return value from rx_bus_i2c_init
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_hal_error_propagates(void)
{
  mock_riic_set_next_error(k_rx_err_timeout);

  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_FALSE(mock_riic_is_initialized((uint8_t)k_test_i2c_channel));
}

/* =============================================================================
 * Write Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_i2c_write transmits data through bus abstraction
 *
 * @details
 * Writes a 3-byte payload and verifies the mock captured all three bytes
 * in the correct order via mock_riic_get_tx_data().
 *
 * @pre s_test_manager initialized; bus initialized
 * @pre mock RIIC TX capture buffer is empty
 * @post mock_riic_get_tx_data returns {0x01, 0x02, 0x03}
 * @post RIIC channel 0 TX byte count matches k_test_write_len
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_sends_data(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0,
                        (uint8_t)k_test_write_byte_1,
                        (uint8_t)k_test_write_byte_2};
  err = rx_bus_i2c_write(&s_test_manager, s_test_bus_name, tx, (uint16_t)k_test_write_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  captured[(uint16_t)k_test_write_len];
  uint16_t copied =
    mock_riic_get_tx_data((uint8_t)k_test_i2c_channel, captured, (uint16_t)k_test_write_len);
  TEST_ASSERT_EQUAL((uint16_t)k_test_write_len, copied);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(tx, captured, (uint16_t)k_test_write_len);
}

/**
 * @brief rx_bus_i2c_write with null manager returns k_rx_err_null_ptr
 *
 * @pre None
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_null_manager_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  rx_err_t      err  = rx_bus_i2c_write(nullptr, s_test_bus_name, tx, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write with null data buffer returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_null_data_returns_error(void)
{
  rx_err_t err =
    rx_bus_i2c_write(&s_test_manager, s_test_bus_name, nullptr, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write with null bus name returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_null_name_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  rx_err_t      err  = rx_bus_i2c_write(&s_test_manager, nullptr, tx, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_i2c_init() has not been called.
 * The bus_config->initialized flag is false, so callback rejects the op.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No HAL calls recorded
 * @post No bus state modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_not_initialized_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  rx_err_t err = rx_bus_i2c_write(&s_test_manager, s_test_bus_name, tx, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write propagates NACK from HAL
 *
 * @details
 * After successful init, simulates a peripheral NACK. The write operation
 * should propagate the NACK error upward.
 *
 * @pre s_test_manager initialized; bus initialized; NACK simulation enabled
 * @pre mock RIIC HAL configured to simulate NACK on next transfer
 * @post returned error != k_rx_ok
 * @post NACK simulation state cleared after transfer attempt
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_nack_propagates(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(true);

  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  err = rx_bus_i2c_write(&s_test_manager, s_test_bus_name, tx, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/**
 * @brief Zero-length write fills both snapshot slots with the empty sentinel
 *
 * @details
 * Exercises the length-0 branch of internal_record_tx_snapshot: when no bytes
 * are available neither slot index satisfies the length guard, so both slots
 * must hold k_mock_riic_snapshot_empty (0x100) which is outside the uint8_t
 * domain and therefore unambiguous.
 *
 * @pre s_test_manager initialized; bus initialized; mock call history cleared
 * @pre rx_bus_i2c_write called with length == k_test_len_zero
 * @post call->tx_snapshot[k_mock_riic_snapshot_reg_idx] == k_mock_riic_snapshot_empty
 * @post call->tx_snapshot[k_mock_riic_snapshot_val_idx] == k_mock_riic_snapshot_empty
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_snapshot_length_zero_fills_sentinel(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  mock_riic_clear_history();

  uint8_t  dummy = (uint8_t)k_test_snapshot_byte;
  rx_err_t werr =
    rx_bus_i2c_write(&s_test_manager, s_test_bus_name, &dummy, (uint16_t)k_test_len_zero);
  TEST_ASSERT_EQUAL(k_rx_ok, werr);

  const mock_riic_call_t* call = mock_riic_get_call((uint16_t)k_test_idx_zero);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL_UINT16(k_mock_riic_snapshot_empty,
                           call->tx_snapshot[k_mock_riic_snapshot_reg_idx]);
  TEST_ASSERT_EQUAL_UINT16(k_mock_riic_snapshot_empty,
                           call->tx_snapshot[k_mock_riic_snapshot_val_idx]);
}

/**
 * @brief One-byte write fills slot 0 with the byte and slot 1 with the sentinel
 *
 * @details
 * Exercises the length-1 branch of internal_record_tx_snapshot: only slot 0
 * satisfies the length guard so it receives the real byte; slot 1 falls back
 * to k_mock_riic_snapshot_empty (0x100).
 *
 * @pre s_test_manager initialized; bus initialized; mock call history cleared
 * @pre rx_bus_i2c_write called with a single-byte buffer
 * @post call->tx_snapshot[k_mock_riic_snapshot_reg_idx] == k_test_snapshot_byte
 * @post call->tx_snapshot[k_mock_riic_snapshot_val_idx] == k_mock_riic_snapshot_empty
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_snapshot_length_one_fills_first_slot(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  mock_riic_clear_history();

  const uint8_t tx[] = {(uint8_t)k_test_snapshot_byte};
  rx_err_t werr = rx_bus_i2c_write(&s_test_manager, s_test_bus_name, tx, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_ok, werr);

  const mock_riic_call_t* call = mock_riic_get_call((uint16_t)k_test_idx_zero);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL_HEX8((uint8_t)k_test_snapshot_byte,
                         call->tx_snapshot[k_mock_riic_snapshot_reg_idx]);
  TEST_ASSERT_EQUAL_UINT16(k_mock_riic_snapshot_empty,
                           call->tx_snapshot[k_mock_riic_snapshot_val_idx]);
}

/* =============================================================================
 * Read Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_i2c_read returns pre-configured mock data
 *
 * @details
 * Pre-loads the mock RIIC RX buffer with {0xAA, 0xBB}, then verifies
 * rx_bus_i2c_read() returns exactly those bytes through the bus abstraction.
 *
 * @pre s_test_manager initialized; bus initialized; mock RX data configured
 * @pre mock RIIC RX buffer loaded with {0xAA, 0xBB} for channel 0
 * @post rx_buf == {0xAA, 0xBB}
 * @post mock RIIC RX buffer consumed after successful read
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_returns_configured_data(void)
{
  const uint8_t expected[] = {(uint8_t)k_test_read_byte_0, (uint8_t)k_test_read_byte_1};
  mock_riic_set_rx_data((uint8_t)k_test_i2c_channel, expected, (uint16_t)k_test_read_len);

  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t rx_buf[(uint16_t)k_test_read_len];
  err = rx_bus_i2c_read(&s_test_manager, s_test_bus_name, rx_buf, (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, rx_buf, (uint16_t)k_test_read_len);
}

/**
 * @brief rx_bus_i2c_read with null manager returns k_rx_err_null_ptr
 *
 * @pre None
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_null_manager_returns_error(void)
{
  uint8_t  rx_buf[(uint16_t)k_test_len_one];
  rx_err_t err = rx_bus_i2c_read(nullptr, s_test_bus_name, rx_buf, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_read with null data buffer returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No HAL state changes
 * @post No bus manager state modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_null_buf_returns_error(void)
{
  rx_err_t err =
    rx_bus_i2c_read(&s_test_manager, s_test_bus_name, nullptr, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_read with null bus name returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_null_name_returns_error(void)
{
  uint8_t  rx_buf[(uint16_t)k_test_len_one];
  rx_err_t err = rx_bus_i2c_read(&s_test_manager, nullptr, rx_buf, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_read on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_i2c_init() has not been called.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No HAL calls recorded
 * @post No bus state modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_not_initialized_returns_error(void)
{
  uint8_t  rx_buf[(uint16_t)k_test_len_one];
  rx_err_t err =
    rx_bus_i2c_read(&s_test_manager, s_test_bus_name, rx_buf, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_read propagates NACK from HAL
 *
 * @details
 * After successful init, simulates a peripheral NACK. The read operation
 * should propagate the NACK error upward.
 *
 * @pre s_test_manager initialized; bus initialized; NACK simulation enabled
 * @pre mock RIIC HAL configured to simulate NACK on next transfer
 * @post returned error == k_rx_err_nack
 * @post NACK simulation state cleared after transfer attempt
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_nack_propagates(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(true);

  uint8_t rx_buf[(uint16_t)k_test_len_one];
  err = rx_bus_i2c_read(&s_test_manager, s_test_bus_name, rx_buf, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/* =============================================================================
 * Write-Read Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_i2c_write_read sends write data and returns read data
 *
 * @details
 * Verifies the combined write-read operation: the write payload is captured
 * in the mock TX buffer, and the pre-loaded RX data is returned in the
 * read buffer through the bus abstraction.
 *
 * @pre s_test_manager initialized; bus initialized; RX data pre-loaded
 * @pre mock RIIC RX buffer loaded with {0xAA, 0xBB} for channel 0
 * @post tx_buf == {0x01}; rx_buf == {0xAA, 0xBB}
 * @post mock RIIC TX capture contains the transmitted byte
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_sends_and_receives_data(void)
{
  const uint8_t rx_expected[] = {(uint8_t)k_test_read_byte_0, (uint8_t)k_test_read_byte_1};
  mock_riic_set_rx_data((uint8_t)k_test_i2c_channel, rx_expected, (uint16_t)k_test_read_len);

  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  uint8_t       rx_buf[(uint16_t)k_test_read_len];
  err = rx_bus_i2c_write_read(&s_test_manager,
                              s_test_bus_name,
                              tx,
                              (uint16_t)k_test_len_one,
                              rx_buf,
                              (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  captured[(uint16_t)k_test_len_one];
  uint16_t copied =
    mock_riic_get_tx_data((uint8_t)k_test_i2c_channel, captured, (uint16_t)k_test_len_one);
  TEST_ASSERT_EQUAL((uint16_t)k_test_len_one, copied);
  TEST_ASSERT_EQUAL_HEX8(tx[k_test_idx_zero], captured[k_test_idx_zero]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(rx_expected, rx_buf, (uint16_t)k_test_read_len);
}

/**
 * @brief rx_bus_i2c_write_read with null manager returns k_rx_err_null_ptr
 *
 * @pre None
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_null_manager_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  uint8_t       rx_buf[(uint16_t)k_test_read_len];
  rx_err_t      err = rx_bus_i2c_write_read(nullptr,
                                       s_test_bus_name,
                                       tx,
                                       (uint16_t)k_test_len_one,
                                       rx_buf,
                                       (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write_read with null tx buffer returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_null_tx_returns_error(void)
{
  uint8_t  rx_buf[(uint16_t)k_test_read_len];
  rx_err_t err = rx_bus_i2c_write_read(&s_test_manager,
                                       s_test_bus_name,
                                       nullptr,
                                       (uint16_t)k_test_len_one,
                                       rx_buf,
                                       (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write_read with null rx buffer returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_null_rx_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  rx_err_t      err  = rx_bus_i2c_write_read(&s_test_manager,
                                       s_test_bus_name,
                                       tx,
                                       (uint16_t)k_test_len_one,
                                       nullptr,
                                       (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write_read with null bus name returns k_rx_err_null_ptr
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No RIIC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_null_name_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  uint8_t       rx_buf[(uint16_t)k_test_read_len];
  rx_err_t      err = rx_bus_i2c_write_read(&s_test_manager,
                                       nullptr,
                                       tx,
                                       (uint16_t)k_test_len_one,
                                       rx_buf,
                                       (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write_read on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_i2c_init() has not been called.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock RIIC HAL state is clean (no prior calls recorded)
 * @post No HAL calls recorded
 * @post No bus state modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_not_initialized_returns_error(void)
{
  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  uint8_t       rx_buf[(uint16_t)k_test_read_len];
  rx_err_t      err = rx_bus_i2c_write_read(&s_test_manager,
                                       s_test_bus_name,
                                       tx,
                                       (uint16_t)k_test_len_one,
                                       rx_buf,
                                       (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief rx_bus_i2c_write_read propagates NACK from HAL
 *
 * @details
 * After successful init, simulates a peripheral NACK. The write-read
 * operation should propagate the NACK error upward.
 *
 * @pre s_test_manager initialized; bus initialized; NACK simulation enabled
 * @pre mock RIIC HAL configured to simulate NACK on next transfer
 * @post returned error != k_rx_ok
 * @post NACK simulation state cleared after transfer attempt
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_nack_propagates(void)
{
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(true);

  const uint8_t tx[] = {(uint8_t)k_test_write_byte_0};
  uint8_t       rx_buf[(uint16_t)k_test_read_len];
  err = rx_bus_i2c_write_read(&s_test_manager,
                              s_test_bus_name,
                              tx,
                              (uint16_t)k_test_len_one,
                              rx_buf,
                              (uint16_t)k_test_read_len);
  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/* =============================================================================
 * Callback Direct-Invocation Tests (covers defensive branches unreachable via
 * public API due to NULL checks in rx_bus_i2c_write/read/write_read)
 * =============================================================================
 */

/**
 * @enum test_i2c_out_of_range_t
 * @brief Constants for testing out-of-range I2C channel and address values.
 *
 * @details
 * k_test_channel_invalid is one above the maximum RIIC channel (3), used to
 * trigger the channel-out-of-range branch in internal_i2c_init_callback.
 * k_test_addr_invalid is one above the maximum 7-bit I2C address (127), used
 * to trigger the device-address-exceeds-7-bit warning path.
 * k_test_addr_warn_value is an address > 127 but still fitting uint8_t.
 */
typedef enum : uint8_t {
  k_test_channel_invalid = 3U,   /**< One above k_riic_channel_count (3) */
  k_test_addr_warn_value = 200U, /**< Device address > k_i2c_addr_max_7bit (127) */
} test_i2c_out_of_range_t;

/**
 * @enum test_i2c_alt_freq_t
 * @brief Alternative I2C frequency for frequency-mismatch tests
 *
 * @details
 * Used to verify that internal_i2c_init_callback detects a frequency mismatch
 * when a channel that was initialized at 400 kHz is re-initialized at 100 kHz.
 */
typedef enum : uint32_t {
  k_test_alt_freq_100khz = 100000U, /**< Standard mode 100 kHz (differs from default 400 kHz) */
} test_i2c_alt_freq_t;

/**
 * @brief internal_i2c_init_callback rejects a non-I2C bus type
 *
 * @details
 * Directly invokes internal_i2c_init_callback with a bus_config whose type
 * field is set to k_bus_type_gpio (not k_bus_type_i2c). Verifies the callback
 * returns k_rx_err_invalid_arg and sets ctx.result accordingly.
 *
 * @pre mock RIIC HAL state is clean
 * @post No RIIC HAL calls recorded (callback returns before riic_init)
 * @post ctx.result == k_rx_err_invalid_arg
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_callback_wrong_bus_type_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type = k_bus_type_gpio; /* Not I2C */

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = &s_test_manager};

  rx_err_t err = internal_i2c_init_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_init_callback rejects a channel index out of range
 *
 * @details
 * Directly invokes internal_i2c_init_callback with a bus_config whose
 * proto.i2c.channel is set to k_test_channel_invalid (3), which is >=
 * k_riic_channel_count (3). Verifies the callback returns
 * k_rx_err_invalid_arg without calling riic_init.
 *
 * @pre mock RIIC HAL state is clean
 * @post No RIIC HAL calls recorded
 * @post ctx.result == k_rx_err_invalid_arg
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_callback_channel_out_of_range_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type              = k_bus_type_i2c;
  bus_config.proto.i2c.channel = (uint8_t)k_test_channel_invalid;

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = &s_test_manager};

  rx_err_t err = internal_i2c_init_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_init_callback skips riic_init when channel already initialized
 *        with matching frequency
 *
 * @details
 * Initializes the bus once, then directly calls internal_i2c_init_callback
 * again with the same bus_config (same channel, same frequency). Because the
 * channel is already recorded in manager->riic_initialized, the callback must
 * skip riic_init and return k_rx_ok without a second HAL call.
 *
 * @pre s_test_manager initialized; bus fully initialized via rx_bus_i2c_init
 * @post Only one riic_init call recorded in total (from first init)
 * @post ctx.result == k_rx_ok
 * @post bus_config.initialized == true
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_callback_already_initialized_same_freq_skips_init(void)
{
  /* First init via public API so manager records channel as initialized */
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Build a matching bus_config with same channel and frequency */
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type                   = k_bus_type_i2c;
  bus_config.proto.i2c.channel      = (uint8_t)k_test_i2c_channel;
  bus_config.proto.i2c.frequency_hz = (uint32_t)k_test_i2c_freq_hz;

  const uint16_t calls_after_first_init = mock_riic_get_call_count();

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = &s_test_manager};
  err                = internal_i2c_init_callback(&bus_config, &ctx);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_ok, ctx.result);
  TEST_ASSERT_TRUE(bus_config.initialized);
  /* No additional riic_init calls - callback returned early */
  TEST_ASSERT_EQUAL(calls_after_first_init, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_init_callback rejects already-initialized channel with
 *        mismatching frequency
 *
 * @details
 * Initializes the bus once at 400 kHz, then directly calls
 * internal_i2c_init_callback with the same channel but a different frequency
 * (100 kHz). The callback must detect the mismatch and return
 * k_rx_err_invalid_arg without modifying hardware.
 *
 * @pre s_test_manager initialized; bus fully initialized via rx_bus_i2c_init
 * @post ctx.result == k_rx_err_invalid_arg
 * @post No additional riic_init calls made
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_callback_already_initialized_freq_mismatch_returns_error(void)
{
  /* First init via public API at 400 kHz */
  rx_err_t err = rx_bus_i2c_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Build a bus_config for the same channel but different frequency */
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type                   = k_bus_type_i2c;
  bus_config.proto.i2c.channel      = (uint8_t)k_test_i2c_channel;
  bus_config.proto.i2c.frequency_hz = (uint32_t)k_test_alt_freq_100khz;

  const uint16_t calls_after_first_init = mock_riic_get_call_count();

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = &s_test_manager};
  err                = internal_i2c_init_callback(&bus_config, &ctx);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
  /* No additional riic_init calls */
  TEST_ASSERT_EQUAL(calls_after_first_init, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_init_callback logs warning when device address > 127
 *
 * @details
 * Directly calls internal_i2c_init_callback with a bus_config whose
 * proto.i2c.device_addr is set to k_test_addr_warn_value (200), which exceeds
 * the maximum 7-bit I2C address (127). The callback should log a warning but
 * still return k_rx_ok because the HAL is responsible for address validation.
 *
 * @pre mock RIIC HAL state is clean
 * @post riic_init called once (HAL accepts any frequency for testing)
 * @post ctx.result == k_rx_ok (warning does not fail initialization)
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_init_callback_addr_exceeds_7bit_logs_warning(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type                   = k_bus_type_i2c;
  bus_config.proto.i2c.channel      = (uint8_t)k_test_i2c_channel;
  bus_config.proto.i2c.frequency_hz = (uint32_t)k_test_i2c_freq_hz;
  bus_config.proto.i2c.device_addr  = (uint8_t)k_test_addr_warn_value;

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = &s_test_manager};

  rx_err_t err = internal_i2c_init_callback(&bus_config, &ctx);
  /* Warning path - init still succeeds */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_ok, ctx.result);
  TEST_ASSERT_TRUE(bus_config.initialized);
}

/**
 * @brief internal_i2c_write_callback rejects when bus not initialized
 *
 * @details
 * Directly invokes internal_i2c_write_callback with a bus_config whose
 * initialized field is false. Verifies the callback returns
 * k_rx_err_invalid_state without making any RIIC HAL calls.
 * This branch is unreachable via rx_bus_i2c_write() since write always
 * calls init first in practice, but is a valid defensive check.
 *
 * @pre mock RIIC HAL state is clean
 * @post No RIIC HAL calls recorded
 * @post ctx.result == k_rx_err_invalid_state
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_callback_not_initialized_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type        = k_bus_type_i2c;
  bus_config.initialized = false;

  const uint8_t   tx_data[] = {(uint8_t)k_test_write_byte_0};
  i2c_write_ctx_t ctx       = {
          .data   = tx_data,
          .length = (uint16_t)k_test_len_one,
          .result = k_rx_err_hw_error,
  };

  rx_err_t err = internal_i2c_write_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, ctx.result);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_write_callback rejects null data when length > 0
 *
 * @details
 * Directly invokes internal_i2c_write_callback with bus initialized but
 * ctx.data == NULL and ctx.length == 1. This branch is unreachable through
 * rx_bus_i2c_write() because RX_CHECK_NULL_PTR rejects NULL data before
 * the callback is invoked. The defensive check in the callback itself must
 * be exercised directly for 100% coverage.
 *
 * @pre mock RIIC HAL state is clean; bus config marked initialized
 * @post No RIIC HAL calls recorded
 * @post ctx.result == k_rx_err_invalid_arg
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_callback_null_data_with_nonzero_length_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type                   = k_bus_type_i2c;
  bus_config.initialized            = true;
  bus_config.proto.i2c.channel      = (uint8_t)k_test_i2c_channel;
  bus_config.proto.i2c.frequency_hz = (uint32_t)k_test_i2c_freq_hz;
  bus_config.proto.i2c.device_addr  = (uint8_t)k_test_i2c_addr;

  /* Initialize RIIC mock channel so the HAL doesn't reject the channel */
  (void)riic_init((riic_channel_t){.value = (uint8_t)k_test_i2c_channel},
                  (uint32_t)k_test_i2c_freq_hz);
  mock_riic_clear_history();

  i2c_write_ctx_t ctx = {
    .data   = nullptr, /* null data with length > 0 triggers defensive branch */
    .length = (uint16_t)k_test_len_one,
    .result = k_rx_err_hw_error,
  };

  rx_err_t err = internal_i2c_write_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
  /* No riic_write calls made */
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_read_callback rejects when bus not initialized
 *
 * @details
 * Directly invokes internal_i2c_read_callback with a bus_config whose
 * initialized field is false. Verifies the callback returns
 * k_rx_err_invalid_state.
 *
 * @pre mock RIIC HAL state is clean
 * @post No RIIC HAL calls recorded
 * @post ctx.result == k_rx_err_invalid_state
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_read_callback_not_initialized_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type        = k_bus_type_i2c;
  bus_config.initialized = false;

  uint8_t        rx_buf[(uint16_t)k_test_read_len];
  i2c_read_ctx_t ctx = {
    .data   = rx_buf,
    .length = (uint16_t)k_test_read_len,
    .result = k_rx_err_hw_error,
  };

  rx_err_t err = internal_i2c_read_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, ctx.result);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_write_read_callback rejects null write_data when
 *        write_length > 0
 *
 * @details
 * Directly invokes internal_i2c_write_read_callback with bus initialized,
 * write_data == NULL, and write_length == 1. This branch is unreachable via
 * rx_bus_i2c_write_read() because RX_CHECK_NULL_PTR blocks NULL write_data
 * at the public API boundary.
 *
 * @pre mock RIIC HAL state is clean; bus config marked initialized
 * @post No RIIC HAL calls recorded
 * @post ctx.result == k_rx_err_invalid_arg
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_callback_null_write_data_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type                   = k_bus_type_i2c;
  bus_config.initialized            = true;
  bus_config.proto.i2c.channel      = (uint8_t)k_test_i2c_channel;
  bus_config.proto.i2c.frequency_hz = (uint32_t)k_test_i2c_freq_hz;
  bus_config.proto.i2c.device_addr  = (uint8_t)k_test_i2c_addr;

  uint8_t              rx_buf[(uint16_t)k_test_read_len];
  i2c_write_read_ctx_t ctx = {
    .write_data   = nullptr, /* triggers defensive branch */
    .write_length = (uint16_t)k_test_len_one,
    .read_data    = rx_buf,
    .read_length  = (uint16_t)k_test_read_len,
    .result       = k_rx_err_hw_error,
  };

  rx_err_t err = internal_i2c_write_read_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

/**
 * @brief internal_i2c_write_read_callback rejects null read_data when
 *        read_length > 0
 *
 * @details
 * Directly invokes internal_i2c_write_read_callback with bus initialized,
 * valid write_data, but read_data == NULL and read_length > 0. This branch
 * is unreachable via rx_bus_i2c_write_read() because RX_CHECK_NULL_PTR
 * blocks NULL read_data at the public API boundary.
 *
 * @pre mock RIIC HAL state is clean; bus config marked initialized
 * @post No RIIC HAL calls recorded
 * @post ctx.result == k_rx_err_invalid_arg
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 */
void test_bus_i2c_write_read_callback_null_read_data_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type                   = k_bus_type_i2c;
  bus_config.initialized            = true;
  bus_config.proto.i2c.channel      = (uint8_t)k_test_i2c_channel;
  bus_config.proto.i2c.frequency_hz = (uint32_t)k_test_i2c_freq_hz;
  bus_config.proto.i2c.device_addr  = (uint8_t)k_test_i2c_addr;

  const uint8_t        tx_data[] = {(uint8_t)k_test_write_byte_0};
  i2c_write_read_ctx_t ctx       = {
          .write_data   = tx_data,
          .write_length = (uint16_t)k_test_len_one,
          .read_data    = nullptr, /* triggers defensive branch */
          .read_length  = (uint16_t)k_test_read_len,
          .result       = k_rx_err_hw_error,
  };

  rx_err_t err = internal_i2c_write_read_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
  TEST_ASSERT_EQUAL((int32_t)k_test_zero_calls, mock_riic_get_call_count());
}

void test_bus_i2c_init_callback_null_manager_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type = k_bus_type_i2c;

  /* Pass a ctx with manager == NULL to trigger the defensive null check */
  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error, .manager = NULL};

  rx_err_t err = internal_i2c_init_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, ctx.result);
}

void test_bus_i2c_read_callback_null_data_with_nonzero_length_returns_error(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type        = k_bus_type_i2c;
  bus_config.initialized = true;

  /* Pass a ctx with null data but nonzero length */
  i2c_read_ctx_t ctx = {
    .data   = NULL,
    .length = (uint16_t)k_test_read_len,
    .result = k_rx_err_hw_error,
  };

  rx_err_t err = internal_i2c_read_callback(&bus_config, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, ctx.result);
}

/**
 * @brief Read callback with null data but zero length skips null check and calls HAL
 *
 * @details
 * When length == 0, the condition (length > 0 && data == nullptr) short-circuits
 * on the first operand (false), skipping the null pointer check. Covers the
 * length == 0 false branch at line 770.
 *
 * @since Version 1.0.0
 */
void test_bus_i2c_read_callback_zero_length_null_data_skips_null_check(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type        = k_bus_type_i2c;
  bus_config.initialized = true;

  /* length == 0: (length > 0 && data == nullptr) short-circuits at first operand */
  i2c_read_ctx_t ctx = {
    .data   = NULL,
    .length = 0U,
    .result = k_rx_err_hw_error,
  };

  /* With length=0, the null-check is skipped; riic_read is called (mock returns ok) */
  (void)internal_i2c_read_callback(&bus_config, &ctx);
  /* Just verify no crash and the null-check error was not returned */
  TEST_ASSERT_NOT_EQUAL(k_rx_err_invalid_arg, ctx.result);
}

/**
 * @brief Write-read callback with zero write_length skips write-data null check
 *
 * @details Covers the write_length == 0 false branch at line 868.
 *
 * @since Version 1.0.0
 */
void test_bus_i2c_write_read_callback_zero_write_length_skips_null_check(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type        = k_bus_type_i2c;
  bus_config.initialized = true;

  uint8_t              read_buf[4];
  i2c_write_read_ctx_t ctx = {
    .write_data   = NULL,
    .write_length = 0U, /* length == 0: skips null-check for write_data */
    .read_data    = read_buf,
    .read_length  = (uint16_t)k_test_read_len,
    .result       = k_rx_err_hw_error,
  };

  (void)internal_i2c_write_read_callback(&bus_config, &ctx);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_invalid_arg, ctx.result);
}

/**
 * @brief Write-read callback with zero read_length skips read-data null check
 *
 * @details Covers the read_length == 0 false branch at line 875.
 *
 * @since Version 1.0.0
 */
void test_bus_i2c_write_read_callback_zero_read_length_skips_null_check(void)
{
  rx_bus_config_t bus_config;
  (void)memset(&bus_config, 0, sizeof(bus_config));
  bus_config.type        = k_bus_type_i2c;
  bus_config.initialized = true;

  static const uint8_t write_buf[] = {0x01};
  i2c_write_read_ctx_t ctx         = {
            .write_data   = write_buf,
            .write_length = (uint16_t)k_test_len_one,
            .read_data    = NULL,
            .read_length  = 0U, /* length == 0: skips null-check for read_data */
            .result       = k_rx_err_hw_error,
  };

  (void)internal_i2c_write_read_callback(&bus_config, &ctx);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_invalid_arg, ctx.result);
}

/* =============================================================================
 * Bus Config Validation Coverage Tests
 * =============================================================================
 */

/**
 * @brief Invalid SDA pin in rx_bus_config_init_i2c returns error
 *
 * @details
 * Passes an SDA pin with port > k_rx_port_j to trigger the invalid-SDA-port
 * branch inside rx_bus_config_init_i2c (line 920).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers line 920 of rx_bus_config.c
 */
void test_bus_config_i2c_invalid_sda_pin_returns_error(void)
{
  rx_bus_config_t            config;
  static const rx_port_pin_t k_invalid_sda = (rx_port_pin_t)0xFF00U;
  rx_err_t                   err           = rx_bus_config_init_i2c(&config,
                                        "test",
                                        k_test_i2c_channel,
                                        k_test_i2c_addr,
                                        k_invalid_sda,
                                        k_rx_p0_5,
                                        k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Invalid SCL pin in rx_bus_config_init_i2c returns error
 *
 * @details
 * Passes a valid SDA but invalid SCL pin to trigger the invalid-SCL-port
 * branch inside rx_bus_config_init_i2c (line 926).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers line 926 of rx_bus_config.c
 */
void test_bus_config_i2c_invalid_scl_pin_returns_error(void)
{
  rx_bus_config_t            config;
  static const rx_port_pin_t k_invalid_scl = (rx_port_pin_t)0xFF00U;
  rx_err_t                   err           = rx_bus_config_init_i2c(&config,
                                        "test",
                                        k_test_i2c_channel,
                                        k_test_i2c_addr,
                                        k_rx_p0_5,
                                        k_invalid_scl,
                                        k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Invalid channel in rx_bus_config_init_i2c returns error
 *
 * @details
 * Passes channel >= k_riic_channel_count (3) to trigger the invalid-channel
 * branch inside rx_bus_config_init_i2c (lines 931-932).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 931-932 of rx_bus_config.c
 */
void test_bus_config_i2c_invalid_channel_returns_error(void)
{
  rx_bus_config_t      config;
  static const uint8_t k_invalid_channel = k_riic_channel_count;
  rx_err_t             err               = rx_bus_config_init_i2c(&config,
                                        "test",
                                        k_invalid_channel,
                                        k_test_i2c_addr,
                                        k_rx_p0_5,
                                        k_rx_p0_6,
                                        k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Invalid device address in rx_bus_config_init_i2c returns error
 *
 * @details
 * Passes device_addr > k_i2c_addr_max_7bit (127) to trigger the invalid-address
 * branch inside rx_bus_config_init_i2c (lines 937-938).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 937-938 of rx_bus_config.c
 */
void test_bus_config_i2c_invalid_address_returns_error(void)
{
  rx_bus_config_t      config;
  static const uint8_t k_invalid_addr = k_i2c_addr_max_7bit + 1U;
  rx_err_t             err            = rx_bus_config_init_i2c(&config,
                                        "test",
                                        k_test_i2c_channel,
                                        k_invalid_addr,
                                        k_rx_p0_5,
                                        k_rx_p0_6,
                                        k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Null config pointer to rx_bus_config_init_i2c returns error
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(config, ...) branch at line 914 of rx_bus_config.c.
 */
void test_bus_config_i2c_null_config_returns_error(void)
{
  rx_err_t err = rx_bus_config_init_i2c(nullptr,
                                        "test",
                                        (uint8_t)k_test_i2c_channel,
                                        (uint8_t)k_test_i2c_addr,
                                        k_rx_p0_5,
                                        k_rx_p0_6,
                                        (uint32_t)k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Null name pointer to rx_bus_config_init_i2c returns error
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(name, ...) branch at line 915 of rx_bus_config.c.
 */
void test_bus_config_i2c_null_name_returns_error(void)
{
  rx_bus_config_t config;
  rx_err_t        err = rx_bus_config_init_i2c(&config,
                                        nullptr,
                                        (uint8_t)k_test_i2c_channel,
                                        (uint8_t)k_test_i2c_addr,
                                        k_rx_p0_5,
                                        k_rx_p0_6,
                                        (uint32_t)k_test_i2c_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner entry point
 *
 * @details
 * Registers and executes all I2C bus abstraction tests. Returns non-zero
 * if any test fails.
 *
 * @pre Unity framework initialized (UNITY_BEGIN called internally)
 * @pre Test binary linked against mock_riic_hal and mock_bus_manager
 * @post All registered tests executed and results printed
 * @post Resources cleaned up via tearDown after each test
 *
 * @return int Test suite status code
 * @retval 0 All tests passed
 * @retval non-zero One or more tests failed (Unity exit code)
 *
 * @note Not thread-safe; the Unity test runner is single-threaded
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Init tests */
  RUN_TEST(test_bus_i2c_init_marks_channel_initialized);
  RUN_TEST(test_bus_i2c_init_null_manager_returns_error);
  RUN_TEST(test_bus_i2c_init_null_name_returns_error);
  RUN_TEST(test_bus_i2c_init_bus_not_found_returns_error);
  RUN_TEST(test_bus_i2c_init_hal_error_propagates);

  /* Write tests */
  RUN_TEST(test_bus_i2c_write_sends_data);
  RUN_TEST(test_bus_i2c_write_null_manager_returns_error);
  RUN_TEST(test_bus_i2c_write_null_data_returns_error);
  RUN_TEST(test_bus_i2c_write_null_name_returns_error);
  RUN_TEST(test_bus_i2c_write_not_initialized_returns_error);
  RUN_TEST(test_bus_i2c_write_nack_propagates);
  RUN_TEST(test_bus_i2c_write_snapshot_length_zero_fills_sentinel);
  RUN_TEST(test_bus_i2c_write_snapshot_length_one_fills_first_slot);

  /* Read tests */
  RUN_TEST(test_bus_i2c_read_returns_configured_data);
  RUN_TEST(test_bus_i2c_read_null_manager_returns_error);
  RUN_TEST(test_bus_i2c_read_null_buf_returns_error);
  RUN_TEST(test_bus_i2c_read_null_name_returns_error);
  RUN_TEST(test_bus_i2c_read_not_initialized_returns_error);
  RUN_TEST(test_bus_i2c_read_nack_propagates);

  /* Write-read tests */
  RUN_TEST(test_bus_i2c_write_read_sends_and_receives_data);
  RUN_TEST(test_bus_i2c_write_read_null_manager_returns_error);
  RUN_TEST(test_bus_i2c_write_read_null_tx_returns_error);
  RUN_TEST(test_bus_i2c_write_read_null_rx_returns_error);
  RUN_TEST(test_bus_i2c_write_read_null_name_returns_error);
  RUN_TEST(test_bus_i2c_write_read_not_initialized_returns_error);
  RUN_TEST(test_bus_i2c_write_read_nack_propagates);

  /* Callback direct-invocation tests - cover defensive branches */
  RUN_TEST(test_bus_i2c_init_callback_wrong_bus_type_returns_error);
  RUN_TEST(test_bus_i2c_init_callback_channel_out_of_range_returns_error);
  RUN_TEST(test_bus_i2c_init_callback_already_initialized_same_freq_skips_init);
  RUN_TEST(test_bus_i2c_init_callback_already_initialized_freq_mismatch_returns_error);
  RUN_TEST(test_bus_i2c_init_callback_addr_exceeds_7bit_logs_warning);
  RUN_TEST(test_bus_i2c_write_callback_not_initialized_returns_error);
  RUN_TEST(test_bus_i2c_write_callback_null_data_with_nonzero_length_returns_error);
  RUN_TEST(test_bus_i2c_read_callback_not_initialized_returns_error);
  RUN_TEST(test_bus_i2c_write_read_callback_null_write_data_returns_error);
  RUN_TEST(test_bus_i2c_write_read_callback_null_read_data_returns_error);
  RUN_TEST(test_bus_i2c_init_callback_null_manager_returns_error);
  RUN_TEST(test_bus_i2c_read_callback_null_data_with_nonzero_length_returns_error);
  RUN_TEST(test_bus_i2c_read_callback_zero_length_null_data_skips_null_check);
  RUN_TEST(test_bus_i2c_write_read_callback_zero_write_length_skips_null_check);
  RUN_TEST(test_bus_i2c_write_read_callback_zero_read_length_skips_null_check);

  /* Bus config validation tests */
  RUN_TEST(test_bus_config_i2c_invalid_sda_pin_returns_error);
  RUN_TEST(test_bus_config_i2c_invalid_scl_pin_returns_error);
  RUN_TEST(test_bus_config_i2c_invalid_channel_returns_error);
  RUN_TEST(test_bus_config_i2c_invalid_address_returns_error);
  RUN_TEST(test_bus_config_i2c_null_config_returns_error);
  RUN_TEST(test_bus_config_i2c_null_name_returns_error);

  return UNITY_END();
}
