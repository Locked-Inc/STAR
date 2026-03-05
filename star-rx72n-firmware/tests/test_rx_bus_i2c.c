// SPDX-License-Identifier: MIT
/* tests/test_rx_bus_i2c.c */

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
 * | Write      | 6     | Success, null manager/name/data, not init, NACK          |
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
 * @author STAR Team
 * @date 2026-02-26
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
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
  k_test_write_byte_0 = 0x01, /**< First write byte */
  k_test_write_byte_1 = 0x02, /**< Second write byte */
  k_test_write_byte_2 = 0x03, /**< Third write byte */
  k_test_read_byte_0  = 0xAA, /**< First expected read byte */
  k_test_read_byte_1  = 0xBB, /**< Second expected read byte */
} test_i2c_data_t;

/**
 * @enum test_i2c_single_len_t
 * @brief Single-byte transfer size for null-parameter and NACK tests
 *
 * @details
 * Null-parameter and NACK tests use a minimal 1-byte payload to keep fixtures
 * concise. Named constant avoids the magic literal "1U" per STAR policy.
 */
typedef enum : uint16_t {
  k_test_len_one = 1U, /**< One-byte transfer length for minimal test fixtures */
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

  return UNITY_END();
}
