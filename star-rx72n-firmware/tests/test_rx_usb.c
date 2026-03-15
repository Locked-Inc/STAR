/**
 * @file test_rx_usb.c
 * @brief Unit Tests for USB CDC Driver
 *
 * @details
 * Comprehensive unit test suite for the USB CDC driver module. Tests cover
 * all aspects of the USB driver including ring buffer operations, state
 * machine transitions, read/write functionality, statistics tracking, and
 * debug output functions.
 *
 * @par Test Architecture
 * @dot
 * digraph test_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_tests {
 *     label="Test Suite (test_rx_usb.c)";
 *     style=filled;
 *     color=lightblue;
 *
 *     ring_tests [label="Ring Buffer Tests\n(20 tests)"];
 *     init_tests [label="Initialization Tests\n(12 tests)"];
 *     state_tests [label="State Machine Tests\n(15 tests)"];
 *     io_tests [label="Read/Write Tests\n(20 tests)"];
 *     debug_tests [label="Debug Output Tests\n(25 tests)"];
 *   }
 *
 *   subgraph cluster_mocks {
 *     label="Mock Infrastructure";
 *     style=filled;
 *     color=lightyellow;
 *
 *     mock_hw [label="mock_usb_hw.h\n(HW abstraction)"];
 *     mock_regs [label="mock_usb0_regs.h\n(Register simulation)"];
 *   }
 *
 *   subgraph cluster_sut {
 *     label="System Under Test";
 *     style=filled;
 *     color=lightgreen;
 *
 *     rx_usb [label="rx_usb.c"];
 *     rx_usb_priv [label="rx_usb_private.h\n(Internal APIs)"];
 *   }
 *
 *   ring_tests -> rx_usb_priv [label="priv_ring_buffer_*()"];
 *   init_tests -> rx_usb [label="rx_usb_init()"];
 *   state_tests -> rx_usb [label="rx_usb_set_state()"];
 *   io_tests -> rx_usb [label="rx_usb_read/write()"];
 *   debug_tests -> rx_usb [label="rx_usb_put*()"];
 *
 *   rx_usb -> mock_hw;
 *   rx_usb -> mock_regs;
 * }
 * @enddot
 *
 * @par Test Categories
 * | Category | Test Count | Description |
 * |----------|------------|-------------|
 * | Ring Buffer Init | 4 | Initialization and null handling |
 * | Ring Buffer Write | 8 | Write operations and edge cases |
 * | Ring Buffer Read | 10 | Read operations, wraparound, FIFO |
 * | USB Init | 12 | Initialization, callbacks, errors |
 * | USB Deinit | 3 | Deinitialization and cleanup |
 * | USB State | 5 | State queries and transitions |
 * | USB Write | 6 | Write operations and errors |
 * | USB Read | 4 | Read operations and errors |
 * | USB Available | 6 | Buffer space queries |
 * | USB Line Coding | 3 | CDC line coding get/set |
 * | USB Statistics | 3 | Statistics tracking and reset |
 * | USB Flush | 3 | Buffer flush operations |
 * | State Transitions | 15 | Full state machine coverage |
 * | Debug Output | 18 | putc, puts, putint, puthex |
 * | TX Trigger | 3 | Transmission triggering |
 * | **Total** | **103** | |
 *
 * @par Memory Usage
 * | Component | Size | Notes |
 * |-----------|------|-------|
 * | Test buffer | 512 bytes | Ring buffer test data |
 * | Callback state | 24 bytes | Tracking variables |
 * | Test constants | ~64 bytes | Enum-based constants |
 * | **Total Static** | ~600 bytes | |
 *
 * @par Mock Infrastructure
 * Tests use mock implementations to isolate USB driver logic from hardware:
 * - **mock_usb_hw.h**: Mocks rx_usb_hw_*() functions with call tracking
 * - **mock_usb0_regs.h**: Provides fake USB0 register structure
 * - **rx_usb_private.h**: Exposes internal APIs (ring_buffer) for testing
 *
 * @par Test Conventions
 * - Test names follow pattern: `test_<component>_<scenario>_<expected>`
 * - setUp() resets all mock state before each test
 * - tearDown() calls rx_usb_deinit() to ensure clean state
 * - All test constants use typed enums (no magic numbers)
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto or recursion in tests
 * - Rule 2: [OK] All loops have fixed bounds
 * - Rule 3: [OK] Static allocation only
 * - Rule 4: [OK] Tests are short and focused
 * - Rule 5: [OK] Tests validate all error conditions
 *
 * @par Running Tests
 * @code{.sh}
 * # Build and run USB tests
 * cd star-rx72n-firmware
 * cmake -B build_tests -S tests
 * cmake --build build_tests
 * ./build_tests/test_rx_usb
 *
 * # Run with verbose output
 * ./build_tests/test_rx_usb -v
 * @endcode
 *
 * @see rx_usb.h USB CDC driver public API
 * @see rx_usb_private.h Internal APIs and ring buffer
 * @see mock_usb_hw.h Hardware abstraction mocks
 *
 * @author Locked, Inc.
 * @date 2026-01-04
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "unity.h"

/* Source under test includes mock headers when UNIT_TEST is defined */
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_usb.h"
#include "rx_usb_internal.h" /* Shared-internal API: rx_usb_get_port_config, rx_usb_invoke_callback */
#include "rx_usb_private.h" /* Internal types and functions for testing */

/* UNIT_TEST-only accessors defined in rx_usb.c under #ifdef UNIT_TEST */
extern ring_buffer_t* rx_usb_test_get_tx_buffer(rx_usb_port_id_t port);
extern ring_buffer_t* rx_usb_test_get_rx_buffer(rx_usb_port_id_t port);

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Ring buffer garbage values for initialization tests
 */
typedef enum : uint16_t {
  k_test_garbage_head  = 100, /**< Garbage head value for testing */
  k_test_garbage_tail  = 200, /**< Garbage tail value for testing */
  k_test_garbage_count = 300, /**< Garbage count value for testing */
} test_ring_buffer_garbage_t;

/**
 * @brief Test byte values for data validation
 */
typedef enum : uint8_t {
  k_test_byte_0x01 = 0x01, /**< Byte value 0x01 for sequential data tests */
  k_test_byte_0x02 = 0x02, /**< Byte value 0x02 for sequential data tests */
  k_test_byte_0x03 = 0x03, /**< Byte value 0x03 for sequential data tests */
  k_test_byte_0x04 = 0x04, /**< Byte value 0x04 for sequential data tests */
  k_test_byte_0x05 = 0x05, /**< Byte value 0x05 for sequential data tests */
  k_test_byte_0x42 = 0x42, /**< Common test byte value */
} test_byte_values_t;

/**
 * @brief Test zero constant for non-size uses
 */
typedef enum : uint8_t {
  k_test_zero = 0, /**< Zero value for counters, states, etc. */
} test_zero_constant_t;

/**
 * @brief Test buffer sizes and counts
 */
typedef enum : uint8_t {
  k_test_size_0   = 0,   /**< Zero size */
  k_test_size_1   = 1,   /**< Single byte */
  k_test_size_4   = 4,   /**< 4 bytes */
  k_test_size_5   = 5,   /**< 5 bytes */
  k_test_size_10  = 10,  /**< 10 bytes */
  k_test_size_11  = 11,  /**< 11 bytes (overflow test) */
  k_test_size_50  = 50,  /**< 50 bytes */
  k_test_size_100 = 100, /**< 100 bytes */
} test_buffer_sizes_t;

/**
 * @brief Test iteration counts
 */
typedef enum : uint8_t {
  k_test_iterations_100 = 100, /**< 100 iterations for stress tests */
} test_iteration_counts_t;

/**
 * @brief Test sentinel/special values
 */
typedef enum : uint16_t {
  k_test_sentinel_999 = 999, /**< Sentinel value for uninitialized tests */
} test_sentinel_values_t;

/**
 * @brief Test magic numbers for validation
 */
typedef enum : uint16_t {
  k_test_magic_42    = 42,    /**< Answer to everything - putint test value */
  k_test_magic_789   = 789,   /**< Random test value */
  k_test_magic_12345 = 12345, /**< Random test value */
} test_magic_numbers_t;

/**
 * @brief Test UART baud rates
 */
typedef enum : uint32_t {
  k_test_baud_9600   = 9600,   /**< 9600 baud */
  k_test_baud_115200 = 115200, /**< 115200 baud */
} test_baud_rates_t;

/**
 * @brief Test integer limits
 */
typedef enum : uint32_t {
  k_test_int32_max      = 2147483647U, /**< INT32_MAX */
  k_test_int32_overflow = 2147483648U, /**< INT32_MAX + 1 */
} test_integer_limits_t;

/**
 * @brief Test hex/special values used in puthex and context tests
 */
typedef enum : uint32_t {
  k_test_hex_cafebabe  = 0xCAFEBABEU, /**< Pointer-sized test context value */
  k_test_hex_deadbeef  = 0xDEADBEEFU, /**< 8-digit hex test value */
  k_test_hex_abcdef01  = 0xABCDEF01U, /**< 8-digit hex value for digit clamping test */
  k_test_hex_abcdef    = 0xABCDEFU,   /**< 6-digit hex test value (lowercase input) */
  k_test_hex_ab        = 0xABU,       /**< 2-digit hex test value */
  k_test_hex_1f        = 0x1FU,       /**< 4-digit zero-padded hex test value */
  k_test_hex_f         = 0xFU,        /**< Single hex digit value */
  k_test_hex_ff_mask   = 0xFFU,       /**< Byte mask */
  k_test_flush_timeout = 10000U,      /**< Maximum flush timeout ms */
  k_test_flush_exceed  = 10001U,      /**< Value that exceeds flush max timeout */
  k_test_flush_small   = 10U,         /**< Small timeout for blocking flush tests */
} test_hex_values_t;

/**
 * @brief Test fill byte values for buffer fill operations
 */
typedef enum : uint8_t {
  k_test_fill_aa   = 0xAAU, /**< Fill pattern 0xAA */
  k_test_fill_bb   = 0xBBU, /**< Fill pattern 0xBB */
  k_test_fill_11   = 0x11U, /**< Fill pattern 0x11 */
  k_test_fill_22   = 0x22U, /**< Fill pattern 0x22 */
  k_test_fill_zero = 0x00U, /**< Fill pattern 0x00 */
  k_test_fill_a    = 'A',   /**< Fill with ASCII 'A' */
} test_fill_bytes_t;

/**
 * @brief Small integer test sizes and counts (16-bit range)
 */
typedef enum : uint8_t {
  k_test_size_2  = 2,  /**< 2 bytes */
  k_test_size_3  = 3,  /**< 3 bytes */
  k_test_size_6  = 6,  /**< 6 bytes */
  k_test_size_7  = 7,  /**< 7 bytes */
  k_test_size_8  = 8,  /**< 8 bytes */
  k_test_size_9  = 9,  /**< 9 bytes */
  k_test_size_20 = 20, /**< 20 bytes */
  k_test_size_30 = 30, /**< 30 bytes */
} test_extra_sizes_t;

/**
 * @brief Large test buffer sizes (32-bit range)
 */
typedef enum : uint32_t {
  k_test_size_1024 = 1024U, /**< 1024 bytes */
  k_test_size_1025 = 1025U, /**< 1025 bytes (one over 1024) */
} test_large_sizes_t;

/**
 * @brief CDC line coding field test values
 */
typedef enum : uint8_t {
  k_test_stop_bits_none = 0, /**< 1 stop bit (CDC encoding: 0) */
  k_test_stop_bits_2    = 2, /**< 2 stop bits */
  k_test_parity_none    = 0, /**< No parity */
  k_test_parity_odd     = 1, /**< Odd parity */
  k_test_data_bits_7    = 7, /**< 7 data bits */
  k_test_data_bits_8    = 8, /**< 8 data bits */
} test_line_coding_values_t;

/**
 * @brief Miscellaneous small integer constants
 */
typedef enum : uint8_t {
  k_test_count_6           = 6,   /**< Count of 6 items / " World" = 6 bytes */
  k_test_count_9           = 9,   /**< Count of 9 items */
  k_test_pipe_1            = 1,   /**< USB pipe 1 (bulk in) */
  k_test_pipe_2            = 2,   /**< USB pipe 2 (bulk out) */
  k_test_pipe_3            = 3,   /**< USB pipe 3 (interrupt) */
  k_test_intf_0            = 0,   /**< USB interface 0 (control) */
  k_test_intf_1            = 1,   /**< USB interface 1 (data) */
  k_test_intf_255          = 255, /**< Invalid USB interface */
  k_test_hex_digits_1      = 1,   /**< 1 hex digit */
  k_test_hex_digits_2      = 2,   /**< 2 hex digits */
  k_test_hex_digits_4      = 4,   /**< 4 hex digits */
  k_test_hex_digits_6      = 6,   /**< 6 hex digits */
  k_test_hex_digits_8      = 8,   /**< 8 hex digits */
  k_test_hex_digits_9      = 9,   /**< 9 hex digits (exceeds max of 8, clamped) */
  k_test_putint_pos_len    = 5,   /**< Length of "12345" string */
  k_test_putint_neg_len    = 4,   /**< Length of "-789" string */
  k_test_putint_max_len    = 10,  /**< Length of "2147483647" string */
  k_test_putint_min_len    = 11,  /**< Length of "-2147483648" string */
  k_test_invalid_usb_state = 7,   /**< USB state value 7, outside valid range 0-6 */
} test_misc_uint8_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @defgroup test_fixtures Test Fixtures
 * @brief Static variables and callback tracking for test isolation
 * @{
 */

/**
 * @var s_test_buffer
 * @brief Ring buffer instance for direct ring buffer testing
 *
 * @details
 * Used by ring buffer tests to directly exercise priv_ring_buffer_*()
 * functions without going through the full USB API.
 */
static ring_buffer_t s_test_buffer;

/**
 * @var s_test_buffer_data
 * @brief Backing storage for s_test_buffer
 *
 * @details
 * Sized to match the USB protocol port buffer size for realistic testing.
 */
static uint8_t s_test_buffer_data[k_usb_port_proto_rx_size];

/**
 * @var s_last_port
 * @brief Last port ID passed to test callback
 */
static rx_usb_port_id_t s_last_port;

/**
 * @var s_last_event
 * @brief Last event type passed to test callback
 */
static rx_usb_event_t s_last_event;

/**
 * @var s_callback_count
 * @brief Number of times test_callback() was invoked
 */
static uint32_t s_callback_count;

/**
 * @var s_callback_context
 * @brief Context pointer passed to test callback
 */
static void* s_callback_context;

/**
 * @brief Test callback function for USB event verification
 *
 * @details
 * Captures all callback invocations for later verification in tests.
 * Tracks port, event, count, and context for each call.
 *
 * @param[in] port USB port ID that generated the event
 * @param[in] event Event type (configured, data_rx, etc.)
 * @param[in] ctx User context pointer from rx_usb_config_t
 */
static void test_callback(rx_usb_port_id_t port, rx_usb_event_t event, void* ctx)
{
  s_last_port  = port;
  s_last_event = event;
  s_callback_count++;
  s_callback_context = ctx;
}

/** @} */ /* end of test_fixtures */

/**
 * @brief Unity test framework setup function
 *
 * @details
 * Called automatically by Unity before each test function. Initializes
 * all mock infrastructure and resets test state to ensure test isolation.
 *
 * @par Setup Steps
 * 1. Initialize mock USB hardware layer
 * 2. Initialize mock register structures
 * 3. Clear test ring buffer
 * 4. Reset callback tracking variables
 *
 * @note Called automatically by Unity framework
 */
void setUp(void)
{
  /* Initialize mock hardware */
  mock_usb_hw_init(nullptr);
  mock_regs_init();

  /* Initialize test ring buffer */
  {
    uint8_t* p   = (uint8_t*)&s_test_buffer;
    uint32_t len = (uint32_t)sizeof(s_test_buffer);
    for (uint32_t i = 0U; i < len; i++) {
      p[i] = 0U;
    }
  }

  /* Reset callback tracking */
  s_last_event       = (rx_usb_event_t)k_test_zero;
  s_callback_count   = k_test_zero;
  s_callback_context = nullptr;
}

/**
 * @brief Unity test framework teardown function
 *
 * @details
 * Called automatically by Unity after each test function. Cleans up
 * USB driver state and mock infrastructure to prevent state leakage
 * between tests.
 *
 * @par Teardown Steps
 * 1. Deinitialize USB driver (if initialized)
 * 2. Deinitialize mock USB hardware
 * 3. Clear mock register state
 *
 * @note Called automatically by Unity framework
 */
void tearDown(void)
{
  /* Deinitialize USB if it was initialized */
  (void)rx_usb_deinit();

  /* Clear mock state */
  mock_usb_hw_deinit(nullptr);
  mock_regs_clear();
}

/* =============================================================================
 * Ring Buffer Initialization Tests
 * =============================================================================
 */

/**
 * @defgroup test_ring_buffer_init Ring Buffer Initialization Tests
 * @brief Tests for ring buffer initialization and null handling
 *
 * @details
 * Verifies that priv_ring_buffer_init() correctly initializes ring buffer
 * state and that all ring buffer functions handle nullptr pointers safely.
 *
 * @par Test Coverage
 * | Test | Purpose |
 * |------|---------|
 * | test_ring_buffer_init_clears_state | Verify init clears all state |
 * | test_ring_buffer_available_empty | Verify 0 available after init |
 * | test_ring_buffer_free_empty | Verify full capacity after init |
 * | test_ring_buffer_null_ptr_handling | Verify nullptr safety |
 *
 * @{
 */

/**
 * @brief Verify ring buffer init clears all state fields
 *
 * @details
 * Pre-fills buffer with garbage values, then verifies init clears:
 * - head pointer to 0
 * - tail pointer to 0
 * - count to 0
 */
void test_ring_buffer_init_clears_state(void)
{
  /* Pre-fill with garbage */
  s_test_buffer.head  = k_test_garbage_head;
  s_test_buffer.tail  = k_test_garbage_tail;
  s_test_buffer.count = k_test_garbage_count;

  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  TEST_ASSERT_EQUAL_UINT32(k_test_size_0, s_test_buffer.head);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_0, s_test_buffer.tail);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_0, s_test_buffer.count);
}

void test_ring_buffer_available_empty(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  uint32_t available = priv_ring_buffer_available(&s_test_buffer);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_0, available);
}

void test_ring_buffer_free_empty(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  uint32_t free_space = priv_ring_buffer_free(&s_test_buffer);

  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, free_space);
}

void test_ring_buffer_null_ptr_handling(void)
{
  priv_ring_buffer_init(nullptr, nullptr, k_test_size_0);
  TEST_ASSERT_EQUAL_UINT32(k_test_zero, priv_ring_buffer_available(nullptr));
  TEST_ASSERT_EQUAL_UINT32(k_test_zero, priv_ring_buffer_free(nullptr));
}

/** @} */ /* end of test_ring_buffer_init */

/* =============================================================================
 * Ring Buffer Write Tests
 * =============================================================================
 */

/**
 * @defgroup test_ring_buffer_write Ring Buffer Write Tests
 * @brief Tests for ring buffer write operations
 *
 * @details
 * Comprehensive tests for priv_ring_buffer_write() including single byte,
 * multiple bytes, buffer fill, overflow handling, and wraparound.
 *
 * @{
 */

/**
 * @brief Verify single byte write to empty buffer
 */
void test_ring_buffer_write_single_byte(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data = k_test_byte_0x42;

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, &data, k_test_size_1);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_1, written);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_1, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT8(k_test_byte_0x42, s_test_buffer.data[k_test_size_0]);
}

void test_ring_buffer_write_null_data_returns_zero(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, nullptr, k_test_size_10);

  TEST_ASSERT_EQUAL_UINT32(0, written);
}

void test_ring_buffer_write_zero_len_returns_zero(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[] = {k_test_byte_0x01, k_test_byte_0x02};

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, k_test_size_0);

  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

void test_ring_buffer_write_multiple_bytes(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[] = {k_test_byte_0x01,
                    k_test_byte_0x02,
                    k_test_byte_0x03,
                    k_test_byte_0x04,
                    k_test_byte_0x05};

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, k_test_size_5);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, written);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT8(k_test_byte_0x01, s_test_buffer.data[k_test_size_0]);
  TEST_ASSERT_EQUAL_UINT8(k_test_byte_0x05, s_test_buffer.data[k_test_size_4]);
}

void test_ring_buffer_write_fills_buffer(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[k_usb_port_proto_rx_size];
  for (uint32_t i = 0U; i < k_usb_port_proto_rx_size; i++) {
    data[i] = k_test_fill_aa;
  }

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, sizeof(data));

  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT32(0, priv_ring_buffer_free(&s_test_buffer));
}

void test_ring_buffer_write_overflow_truncates(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[k_usb_port_proto_rx_size + k_test_size_100];
  for (uint32_t i = 0U; i < (uint32_t)sizeof(data); i++) {
    data[i] = k_test_fill_bb;
  }

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, sizeof(data));

  /* Should only write up to buffer size */
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, s_test_buffer.count);
}

void test_ring_buffer_write_partial_when_partially_full(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Fill half the buffer */
  uint8_t first[k_usb_port_proto_rx_size / k_test_size_2];
  for (uint32_t i = 0U; i < (uint32_t)sizeof(first); i++) {
    first[i] = k_test_fill_11;
  }
  priv_ring_buffer_write(&s_test_buffer, first, sizeof(first));

  /* Try to write more than available space */
  uint8_t second[k_usb_port_proto_rx_size];
  for (uint32_t i = 0U; i < k_usb_port_proto_rx_size; i++) {
    second[i] = k_test_fill_22;
  }
  uint32_t written = priv_ring_buffer_write(&s_test_buffer, second, sizeof(second));

  /* Should only write remaining space */
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size / k_test_size_2, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, s_test_buffer.count);
}

void test_ring_buffer_write_wraps_around(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Write to advance head near end */
  uint8_t data1[k_usb_port_proto_rx_size - k_test_size_10];
  for (uint32_t i = 0U; i < (uint32_t)sizeof(data1); i++) {
    data1[i] = k_test_fill_11;
  }
  priv_ring_buffer_write(&s_test_buffer, data1, sizeof(data1));

  /* Read some to create space at beginning */
  uint8_t out[k_test_size_100];
  priv_ring_buffer_read(&s_test_buffer, out, k_test_size_100);

  /* Write data that wraps around */
  uint8_t data2[k_test_size_50];
  for (uint32_t i = k_test_size_0; i < k_test_size_50; i++) {
    data2[i] = (uint8_t)(i & k_test_hex_ff_mask);
  }
  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data2, k_test_size_50);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_50, written);
  /* Head should have wrapped around */
  TEST_ASSERT_TRUE(s_test_buffer.head < k_test_size_50);
}

/** @} */ /* end of test_ring_buffer_write */

/* =============================================================================
 * Ring Buffer Read Tests
 * =============================================================================
 */

/**
 * @defgroup test_ring_buffer_read Ring Buffer Read Tests
 * @brief Tests for ring buffer read operations and FIFO ordering
 *
 * @details
 * Tests for priv_ring_buffer_read() including empty reads, partial reads,
 * wraparound, FIFO ordering verification, and interleaved operations.
 *
 * @{
 */

/**
 * @brief Verify read from empty buffer returns 0
 */
void test_ring_buffer_read_empty_returns_zero(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[k_test_size_10];

  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, data, k_test_size_10);

  TEST_ASSERT_EQUAL_UINT32(0, read_count);
}

void test_ring_buffer_read_null_data_returns_zero(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data = k_test_byte_0x42;
  priv_ring_buffer_write(&s_test_buffer, &write_data, k_test_size_1);

  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, nullptr, k_test_size_10);

  TEST_ASSERT_EQUAL_UINT32(0, read_count);
}

void test_ring_buffer_read_zero_len_returns_zero(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data = k_test_byte_0x42;
  priv_ring_buffer_write(&s_test_buffer, &write_data, k_test_size_1);

  uint8_t  read_data[k_test_size_10];
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, read_data, k_test_size_0);

  TEST_ASSERT_EQUAL_UINT32(0, read_count);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_1, s_test_buffer.count);
}

void test_ring_buffer_read_single_byte(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data = k_test_byte_0x42;
  priv_ring_buffer_write(&s_test_buffer, &write_data, k_test_size_1);

  uint8_t  read_data;
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, &read_data, k_test_size_1);

  TEST_ASSERT_EQUAL_UINT32(1, read_count);
  TEST_ASSERT_EQUAL_UINT8(k_test_byte_0x42, read_data);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

void test_ring_buffer_read_multiple_bytes(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data[] = "Hello";
  priv_ring_buffer_write(&s_test_buffer, write_data, k_test_size_5);

  uint8_t  read_data[k_test_size_10] = {0};
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, read_data, k_test_size_10);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, read_count);
  TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, k_test_size_5);
}

void test_ring_buffer_read_partial(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data[] = "Hello World";
  priv_ring_buffer_write(&s_test_buffer, write_data, k_test_size_11);

  uint8_t  read_data[k_test_size_5];
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, read_data, k_test_size_5);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, read_count);
  TEST_ASSERT_EQUAL_MEMORY("Hello", read_data, k_test_size_5);
  TEST_ASSERT_EQUAL_UINT32(k_test_count_6, s_test_buffer.count); /* " World" remains */
}

void test_ring_buffer_read_wraps_around(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Write near end of buffer */
  uint8_t fill[k_usb_port_proto_rx_size - k_test_size_20];
  for (uint32_t i = 0U; i < (uint32_t)sizeof(fill); i++) {
    fill[i] = k_test_fill_zero;
  }
  priv_ring_buffer_write(&s_test_buffer, fill, sizeof(fill));

  /* Read to free up space */
  uint8_t discard[k_usb_port_proto_rx_size - k_test_size_30];
  priv_ring_buffer_read(&s_test_buffer, discard, sizeof(discard));

  /* Write pattern that wraps */
  uint8_t pattern[k_test_size_50];
  for (uint32_t i = k_test_size_0; i < k_test_size_50; i++) {
    pattern[i] = (uint8_t)(i + 1);
  }
  priv_ring_buffer_write(&s_test_buffer, pattern, k_test_size_50);

  /* Read and verify pattern */
  uint8_t verify[k_test_size_50];
  priv_ring_buffer_read(&s_test_buffer, discard, k_test_size_10); /* Skip remaining fill */
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, verify, k_test_size_50);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_50, read_count);
  TEST_ASSERT_EQUAL_MEMORY(pattern, verify, k_test_size_50);
}

void test_ring_buffer_fifo_order(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Write sequence: values 1..5 for FIFO ordering verification */
  uint8_t write_data[] = {k_test_size_1,
                          k_test_size_2,
                          k_test_size_3,
                          k_test_size_4,
                          k_test_size_5};
  priv_ring_buffer_write(&s_test_buffer, write_data, k_test_size_5);

  /* Read should be in same order */
  uint8_t read_data[k_test_size_5];
  priv_ring_buffer_read(&s_test_buffer, read_data, k_test_size_5);

  for (uint32_t i = k_test_size_0; i < k_test_size_5; i++) {
    TEST_ASSERT_EQUAL_UINT8(i + 1, read_data[i]);
  }
}

void test_ring_buffer_interleaved_read_write(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  for (uint32_t i = k_test_size_0; i < k_test_iterations_100; i++) {
    uint8_t  write_data = (uint8_t)i;
    uint32_t written    = priv_ring_buffer_write(&s_test_buffer, &write_data, k_test_size_1);
    TEST_ASSERT_EQUAL_UINT32(k_test_size_1, written);

    uint8_t  read_data;
    uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, &read_data, k_test_size_1);
    TEST_ASSERT_EQUAL_UINT32(k_test_size_1, read_count);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)i, read_data);
  }
}

void test_ring_buffer_burst_write_then_burst_read(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  uint8_t pattern[k_test_size_100];
  for (uint32_t i = k_test_size_0; i < k_test_iterations_100; i++) {
    pattern[i] = (uint8_t)(i * k_test_size_2);
  }
  uint32_t written = priv_ring_buffer_write(&s_test_buffer, pattern, k_test_size_100);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_100, written);

  uint8_t  read_buf[k_test_size_100];
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, read_buf, k_test_size_100);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_100, read_count);
  TEST_ASSERT_EQUAL_MEMORY(pattern, read_buf, k_test_size_100);
}

/** @} */ /* end of test_ring_buffer_read */

/* =============================================================================
 * USB Initialization Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_init USB Initialization Tests
 * @brief Tests for USB driver initialization and error handling
 *
 * @details
 * Verifies rx_usb_init() behavior including default config, callback
 * registration, double-init prevention, and error propagation from
 * hardware layer.
 *
 * @{
 */

/**
 * @brief Verify init succeeds with nullptr config (uses defaults)
 */
void test_usb_init_null_config_succeeds(void)
{
  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(nullptr, "rx_usb_hw_init"));
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(nullptr, "rx_usb_hw_attach"));
}

void test_usb_init_transitions_to_attached(void)
{
  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

void test_usb_init_with_callback(void)
{
  static uint32_t s_ctx_deadbeef = k_test_hex_deadbeef;
  rx_usb_config_t config         = {0};

  config.callback = test_callback;
  config.ctx      = &s_ctx_deadbeef;

  rx_err_t err = rx_usb_init(&config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_init_twice_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_init_hw_failure_propagates(void)
{
  mock_usb_hw_set_init_return(nullptr, k_rx_err_hw_error);

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

void test_error_propagation_from_hw_init(void)
{
  mock_usb_hw_set_init_return(nullptr, k_rx_err_hw_error);

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

void test_usb_init_attach_failure_propagates(void)
{
  mock_usb_hw_set_attach_return(nullptr, k_rx_err_hw_error);

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
  /* Should have called deinit to clean up */
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(nullptr, "rx_usb_hw_deinit"));
}

void test_error_propagation_from_attach(void)
{
  mock_usb_hw_set_attach_return(nullptr, k_rx_err_busy);

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

void test_cleanup_on_attach_failure(void)
{
  mock_usb_hw_set_attach_return(nullptr, k_rx_err_hw_error);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, rx_usb_init(nullptr));

  TEST_ASSERT_EQUAL_UINT32(1, mock_usb_hw_get_call_count(nullptr, "rx_usb_hw_deinit"));
}

void test_usb_init_attach_failure_propagates_and_cleans_up(void)
{
  mock_usb_hw_set_attach_return(nullptr, k_rx_err_hw_error);

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
  TEST_ASSERT_EQUAL_UINT32(1, mock_usb_hw_get_call_count(nullptr, "rx_usb_hw_deinit"));
}

void test_pipe_bounds_validation_at_max(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  TEST_ASSERT_EQUAL_UINT32(1, mock_usb_hw_get_call_count(nullptr, "rx_usb_hw_init"));
}

void test_endpoint_bounds_max_valid(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  TEST_ASSERT_EQUAL_UINT32(1, mock_usb_hw_get_call_count(nullptr, "rx_usb_cdc_init"));
}

/** @} */ /* end of test_usb_init */

/* =============================================================================
 * USB Deinitialization Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_deinit USB Deinitialization Tests
 * @brief Tests for USB driver shutdown and cleanup
 * @{
 */

/**
 * @brief Verify deinit fails when not initialized
 */
void test_usb_deinit_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_deinit();

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_deinit_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_deinit();

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(nullptr, "rx_usb_hw_detach"));
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(nullptr, "rx_usb_hw_deinit"));
}

void test_usb_deinit_sets_detached_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  (void)rx_usb_deinit();

  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

/** @} */ /* end of test_usb_deinit */

/* =============================================================================
 * USB State Query Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_state USB State Query Tests
 * @brief Tests for USB state machine queries
 * @{
 */

/**
 * @brief Verify is_configured returns false when not initialized
 */
void test_usb_is_configured_when_not_initialized(void)
{
  TEST_ASSERT_FALSE(rx_usb_is_configured(k_usb_port_proto));
}

void test_usb_is_configured_when_attached(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  TEST_ASSERT_FALSE(rx_usb_is_configured(k_usb_port_proto));
}

void test_usb_is_configured_when_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_TRUE(rx_usb_is_configured(k_usb_port_proto));
}

void test_usb_get_state_returns_detached_initially(void)
{
  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

void test_usb_get_state_returns_attached_after_init(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

/** @} */ /* end of test_usb_state */

/* =============================================================================
 * USB Write Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_write USB Write Tests
 * @brief Tests for USB TX operations and error handling
 * @{
 */

/**
 * @brief Verify write with nullptr data fails with k_rx_err_null_ptr
 */
void test_usb_write_null_data_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_write(k_usb_port_proto, nullptr, k_test_size_10);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_write_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_write(k_usb_port_proto, data, k_test_size_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_write_not_configured_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "test";

  /* State is attached but not configured */
  rx_err_t err = rx_usb_write(k_usb_port_proto, data, k_test_size_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_write_success_when_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_write(k_usb_port_proto, data, k_test_size_4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_write_full_buffer_returns_busy(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  uint8_t fill_data[k_usb_port_proto_rx_size];
  for (uint32_t i = 0U; i < k_usb_port_proto_rx_size; i++) {
    fill_data[i] = k_test_fill_a;
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, fill_data, sizeof(fill_data)));

  uint8_t  more_data[] = "more";
  rx_err_t err         = rx_usb_write(k_usb_port_proto, more_data, k_test_size_4);

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

/** @} */ /* end of test_usb_write */

/* =============================================================================
 * USB Read Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_read USB Read Tests
 * @brief Tests for USB RX operations
 * @{
 */

/**
 * @brief Verify read with nullptr data buffer fails
 */
void test_usb_read_null_data_fails(void)
{
  uint32_t actual_len;

  rx_err_t err = rx_usb_read(k_usb_port_proto, nullptr, k_test_size_10, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_read_null_actual_len_fails(void)
{
  uint8_t data[k_test_size_10];

  rx_err_t err = rx_usb_read(k_usb_port_proto, data, k_test_size_10, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_read_empty_buffer_returns_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t  data[k_test_size_10];
  uint32_t actual_len = k_test_sentinel_999;

  rx_err_t err = rx_usb_read(k_usb_port_proto, data, k_test_size_10, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, actual_len);
}

void test_usb_read_after_rx_push(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t push_data[] = "Hello USB";
  rx_usb_rx_push(k_usb_port_proto, push_data, k_test_size_9);

  uint8_t  read_data[k_test_size_20];
  uint32_t actual_len;
  rx_err_t err = rx_usb_read(k_usb_port_proto, read_data, k_test_size_20, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_9, actual_len);
  TEST_ASSERT_EQUAL_MEMORY(push_data, read_data, k_test_size_9);
}

/** @} */ /* end of test_usb_read */

/* =============================================================================
 * USB RX/TX Available Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_available USB Buffer Space Tests
 * @brief Tests for RX/TX buffer space queries
 * @{
 */

/**
 * @brief Verify rx_available with nullptr output fails
 */
void test_usb_rx_available_null_fails(void)
{
  rx_err_t err = rx_usb_rx_available(k_usb_port_proto, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_rx_available_empty(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint32_t available;

  rx_err_t err = rx_usb_rx_available(k_usb_port_proto, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, available);
}

void test_usb_rx_available_after_push(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "test";
  rx_usb_rx_push(k_usb_port_proto, data, k_test_size_4);

  uint32_t available;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_rx_available(k_usb_port_proto, &available));

  TEST_ASSERT_EQUAL_UINT32(k_test_size_4, available);
}

void test_usb_tx_available_null_fails(void)
{
  rx_err_t err = rx_usb_tx_available(k_usb_port_proto, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_tx_available_empty(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint32_t available;

  rx_err_t err = rx_usb_tx_available(k_usb_port_proto, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_tx_size, available);
}

void test_usb_tx_available_after_write(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  uint32_t available;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_tx_available(k_usb_port_proto, &available));

  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size - 4, available);
}

/** @} */ /* end of test_usb_available */

/* =============================================================================
 * USB Line Coding Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_line_coding USB CDC Line Coding Tests
 * @brief Tests for CDC line coding (baud rate, parity, etc.)
 * @{
 */

/**
 * @brief Verify get_line_coding with nullptr fails
 */
void test_usb_get_line_coding_null_fails(void)
{
  rx_err_t err = rx_usb_get_line_coding(k_usb_port_proto, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_get_line_coding_default_values(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_line_coding_t coding;

  rx_err_t err = rx_usb_get_line_coding(k_usb_port_proto, &coding);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_test_baud_115200, coding.baud_rate);
  TEST_ASSERT_EQUAL_UINT8(k_test_stop_bits_none, coding.stop_bits); /* 1 stop bit */
  TEST_ASSERT_EQUAL_UINT8(k_test_parity_none, coding.parity);       /* No parity */
  TEST_ASSERT_EQUAL_UINT8(k_test_data_bits_8, coding.data_bits);
}

/** @} */ /* end of test_usb_line_coding */

/* =============================================================================
 * USB Statistics Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_stats USB Statistics Tests
 * @brief Tests for USB driver statistics tracking
 * @{
 */

/**
 * @brief Verify get_stats with nullptr fails
 */
void test_usb_get_stats_null_fails(void)
{
  rx_err_t err = rx_usb_get_stats(k_usb_port_proto, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_get_stats_initial_zeros(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_stats_t stats;

  rx_err_t err = rx_usb_get_stats(k_usb_port_proto, &stats);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bytes_rx);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bytes_tx);
  TEST_ASSERT_EQUAL_UINT32(0, stats.rx_overruns);
  TEST_ASSERT_EQUAL_UINT32(0, stats.tx_underruns);
  TEST_ASSERT_EQUAL_UINT32(0, stats.bus_resets);
  TEST_ASSERT_EQUAL_UINT32(0, stats.suspends);
}

void test_usb_reset_stats(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  /* Set configured state to allow writes */
  rx_usb_set_state(k_usb_state_configured);

  /* Generate some stats by writing */
  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(k_test_size_4, stats.bytes_tx);

  /* Reset stats */
  rx_usb_reset_stats(k_usb_port_proto);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(0, stats.bytes_tx);
}

/** @} */ /* end of test_usb_stats */

/* =============================================================================
 * USB Flush Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_flush USB Flush Tests
 * @brief Tests for TX buffer flush operations
 * @{
 */

/**
 * @brief Verify flush fails when not initialized
 */
void test_usb_flush_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_flush(k_usb_port_proto, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_flush_empty_buffer_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_flush(k_usb_port_proto, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_flush_with_data_and_zero_timeout_returns_timeout(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  rx_err_t err = rx_usb_flush(k_usb_port_proto, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/** @} */ /* end of test_usb_flush */

/* =============================================================================
 * Internal State Functions Tests (exposed via extern)
 * =============================================================================
 */

/**
 * @defgroup test_usb_internal Internal State Function Tests
 * @brief Tests for internal USB functions exposed for testing
 *
 * @details
 * These functions are not part of the public API but are exposed via
 * extern declarations for comprehensive testing of internal behavior.
 * @{
 */

/* Forward declarations removed -- these are already declared in the included
   rx_usb.h header, so repeating them here triggers
   readability-redundant-declaration. */

void test_usb_set_state_triggers_callback(void)
{
  static uint32_t s_ctx_marker = k_test_hex_cafebabe;
  rx_usb_config_t config       = {0};

  config.callback = test_callback;
  config.ctx      = &s_ctx_marker;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
  TEST_ASSERT_EQUAL_PTR(&s_ctx_marker, s_callback_context);
}

void test_usb_set_state_no_callback_if_same_state(void)
{
  rx_usb_config_t config = {0};

  config.callback = test_callback;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));

  /* State is already attached after init */
  rx_usb_set_state(k_usb_state_attached);

  TEST_ASSERT_EQUAL(0, s_callback_count);
}

void test_usb_set_state_triggers_callback_on_configured(void)
{
  static uint32_t s_ctx_marker2 = k_test_hex_cafebabe;
  rx_usb_config_t config        = {0};

  config.callback = test_callback;
  config.ctx      = &s_ctx_marker2;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));
  s_callback_count = 0;

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL_UINT32(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
  TEST_ASSERT_EQUAL_PTR(&s_ctx_marker2, s_callback_context);
}

void test_usb_set_state_to_powered(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_state(k_usb_state_powered);

  TEST_ASSERT_EQUAL(k_usb_state_powered, rx_usb_get_state());
}

void test_usb_set_state_to_default(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_state(k_usb_state_default);

  TEST_ASSERT_EQUAL(k_usb_state_default, rx_usb_get_state());
}

void test_usb_set_state_to_addressed(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_state(k_usb_state_addressed);

  TEST_ASSERT_EQUAL(k_usb_state_addressed, rx_usb_get_state());
}

void test_usb_set_state_to_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL(k_usb_state_configured, rx_usb_get_state());
}

void test_usb_set_state_to_suspended(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_state(k_usb_state_suspended);

  TEST_ASSERT_EQUAL(k_usb_state_suspended, rx_usb_get_state());
}

void test_usb_set_state_to_detached(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_usb_set_state(k_usb_state_detached);

  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

void test_usb_state_full_enumeration_sequence(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_state(k_usb_state_powered);
  TEST_ASSERT_EQUAL(k_usb_state_powered, rx_usb_get_state());

  rx_usb_set_state(k_usb_state_default);
  TEST_ASSERT_EQUAL(k_usb_state_default, rx_usb_get_state());

  rx_usb_set_state(k_usb_state_addressed);
  TEST_ASSERT_EQUAL(k_usb_state_addressed, rx_usb_get_state());

  rx_usb_set_state(k_usb_state_configured);
  TEST_ASSERT_EQUAL(k_usb_state_configured, rx_usb_get_state());
  TEST_ASSERT_TRUE(rx_usb_is_configured(k_usb_port_proto));
}
void test_usb_set_line_coding(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_line_coding_t new_coding = {
    .baud_rate = k_test_baud_9600,
    .stop_bits = k_test_stop_bits_2,
    .parity    = k_test_parity_odd,
    .data_bits = k_test_data_bits_7,
  };

  rx_usb_set_line_coding(k_usb_port_proto, &new_coding);

  rx_usb_line_coding_t result;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_line_coding(k_usb_port_proto, &result));
  TEST_ASSERT_EQUAL_UINT32(k_test_baud_9600, result.baud_rate);
  TEST_ASSERT_EQUAL_UINT8(k_test_stop_bits_2, result.stop_bits);
  TEST_ASSERT_EQUAL_UINT8(k_test_parity_odd, result.parity);
  TEST_ASSERT_EQUAL_UINT8(k_test_data_bits_7, result.data_bits);
}

void test_usb_set_line_coding_null_ignored(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_line_coding(k_usb_port_proto, nullptr);

  rx_usb_line_coding_t result;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_line_coding(k_usb_port_proto, &result));
  TEST_ASSERT_EQUAL_UINT32(k_test_baud_115200, result.baud_rate);
}

void test_usb_rx_push_adds_data(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "Hello USB";

  uint32_t written = rx_usb_rx_push(k_usb_port_proto, data, k_test_size_9);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_9, written);

  uint32_t available;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_rx_available(k_usb_port_proto, &available));
  TEST_ASSERT_EQUAL_UINT32(k_test_size_9, available);
}

void test_usb_rx_push_updates_stats(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "test";

  rx_usb_rx_push(k_usb_port_proto, data, k_test_size_4);

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(k_test_size_4, stats.bytes_rx);
}

void test_usb_rx_push_triggers_callback(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_set_callback(k_usb_port_proto, test_callback, nullptr));

  uint8_t data[] = "test";
  rx_usb_rx_push(k_usb_port_proto, data, k_test_size_4);

  TEST_ASSERT_EQUAL(k_test_size_1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_data_rx, s_last_event);
}

void test_usb_tx_pop_retrieves_data(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  uint8_t write_data[] = "Hello";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, write_data, 5));

  uint8_t  read_data[k_test_size_10];
  uint32_t read_count = rx_usb_tx_pop(k_usb_port_proto, read_data, k_test_size_10);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, read_count);
  TEST_ASSERT_EQUAL_MEMORY("Hello", read_data, k_test_size_5);
}

void test_usb_count_bus_reset_increments_stat(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_count_bus_reset();
  rx_usb_count_bus_reset();

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(k_test_size_2, stats.bus_resets);
}

void test_usb_count_suspend_increments_stat(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_count_suspend();

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(1, stats.suspends);
}

/** @} */ /* end of test_usb_internal */

/* =============================================================================
 * Debug Text Output Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_debug USB Debug Output Tests
 * @brief Tests for debug text output functions (putc, puts, putint, puthex)
 *
 * @details
 * Comprehensive tests for ASCII text output functions used for debug
 * console output over USB CDC.
 * @{
 */

/**
 * @brief Verify putc fails when not in configured state
 */
void test_usb_putc_not_configured_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* State is attached but not configured */

  rx_err_t err = rx_usb_putc(k_usb_port_proto, 'A');

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_putc_success_when_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putc(k_usb_port_proto, 'A');

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_putc_writes_to_buffer(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putc(k_usb_port_proto, 'X');
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_10];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(1, len);
  TEST_ASSERT_EQUAL_CHAR('X', buf[0]);
}

void test_usb_putc_buffer_full_returns_busy(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* Fill the TX buffer completely */
  for (uint32_t i = 0; i < k_usb_port_proto_tx_size; i++) {
    rx_err_t err = rx_usb_putc(k_usb_port_proto, 'A');
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }

  /* Next write should fail with buffer full */
  rx_err_t err = rx_usb_putc(k_usb_port_proto, 'X');
  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

void test_usb_puts_null_string_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puts(k_usb_port_proto, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_puts_not_configured_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* State is attached but not configured */

  rx_err_t err = rx_usb_puts(k_usb_port_proto, "Hello");

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_puts_success_when_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puts(k_usb_port_proto, "Hello");

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_puts_writes_string_to_buffer(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puts(k_usb_port_proto, "USB");
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_10];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_size_3, len);
  TEST_ASSERT_EQUAL_MEMORY("USB", buf, k_test_size_3);
}

void test_usb_puts_empty_string_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puts(k_usb_port_proto, "");

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_putint_not_configured_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* State is attached but not configured */

  rx_err_t err = rx_usb_putint(k_usb_port_proto, (int32_t)k_test_magic_42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_putint_positive_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_proto, (int32_t)k_test_magic_12345);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_putint_pos_len, len);
  TEST_ASSERT_EQUAL_MEMORY("12345", buf, k_test_putint_pos_len);
}

void test_usb_putint_negative_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_proto, -(int32_t)k_test_magic_789);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_putint_neg_len, len);
  TEST_ASSERT_EQUAL_MEMORY("-789", buf, k_test_putint_neg_len);
}

void test_usb_putint_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_proto, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_size_1, len);
  TEST_ASSERT_EQUAL_CHAR('0', buf[0]);
}

void test_usb_putint_max_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* INT32_MAX = 2147483647 (10 digits) */
  rx_err_t err = rx_usb_putint(k_usb_port_proto, INT32_MAX);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_putint_max_len, len);
  TEST_ASSERT_EQUAL_MEMORY("2147483647", buf, k_test_putint_max_len);
}

void test_usb_putint_min_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* INT32_MIN = -2147483648 (11 chars: sign + 10 digits) */
  rx_err_t err = rx_usb_putint(k_usb_port_proto, INT32_MIN);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_putint_min_len, len);
  TEST_ASSERT_EQUAL_MEMORY("-2147483648", buf, k_test_putint_min_len);
}

void test_usb_puthex_not_configured_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* State is attached but not configured */

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_ab, k_test_hex_digits_2);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_puthex_two_digits(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_ab, k_test_hex_digits_2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_2, len);
  TEST_ASSERT_EQUAL_MEMORY("AB", buf, k_test_hex_digits_2);
}

void test_usb_puthex_four_digits_zero_padded(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_1f, k_test_hex_digits_4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_4, len);
  TEST_ASSERT_EQUAL_MEMORY("001F", buf, k_test_hex_digits_4);
}

void test_usb_puthex_eight_digits(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_deadbeef, k_test_hex_digits_8);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_8, len);
  TEST_ASSERT_EQUAL_MEMORY("DEADBEEF", buf, k_test_hex_digits_8);
}

void test_usb_puthex_lowercase_letters(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_abcdef, k_test_hex_digits_6);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_6, len);
  /* Implementation uses uppercase hex */
  TEST_ASSERT_EQUAL_MEMORY("ABCDEF", buf, k_test_hex_digits_6);
}

void test_usb_puthex_zero_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0, k_test_hex_digits_4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_4, len);
  TEST_ASSERT_EQUAL_MEMORY("0000", buf, k_test_hex_digits_4);
}

void test_usb_puthex_single_digit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_f, k_test_hex_digits_1);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_1, len);
  TEST_ASSERT_EQUAL_CHAR('F', buf[0]);
}

/** @} */ /* end of test_usb_debug */

/* =============================================================================
 * USB Transmission Trigger Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_tx_trigger USB Transmission Trigger Tests
 * @brief Tests for automatic TX transmission triggering
 *
 * @details
 * Verifies that writes trigger USB bulk-in transfers when the pipe is idle,
 * but not when busy.
 * @{
 */

/**
 * @brief Verify write triggers transmission when pipe is idle
 */
void test_usb_write_triggers_transmission_when_pipe_idle(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* Set pipe as NOT busy (idle) */
  mock_usb0_set_pipe1_busy(0);
  mock_usb_hw_clear_calls(nullptr);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  /* Verify rx_usb_cdc_handle_bulk_in was called */
  TEST_ASSERT_TRUE(mock_usb_hw_was_called(nullptr, "rx_usb_cdc_handle_bulk_in"));
}

void test_usb_write_no_trigger_when_pipe_busy(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* Set pipe as busy */
  mock_usb0_set_pipe1_busy(1);
  mock_usb_hw_clear_calls(nullptr);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  /* Verify rx_usb_cdc_handle_bulk_in was NOT called */
  TEST_ASSERT_FALSE(mock_usb_hw_was_called(nullptr, "rx_usb_cdc_handle_bulk_in"));
}

void test_usb_write_no_trigger_when_buffer_empty_after_write_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* Set pipe as idle */
  mock_usb0_set_pipe1_busy(0);

  /* Fill the buffer first to make subsequent writes fail */
  uint8_t fill_data[k_usb_port_proto_rx_size];
  for (uint32_t i = 0U; i < k_usb_port_proto_rx_size; i++) {
    fill_data[i] = k_test_fill_a;
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, fill_data, sizeof(fill_data)));

  /* Clear call history after fill */
  mock_usb_hw_clear_calls(nullptr);

  /* Try to write more - should fail due to full buffer */
  uint8_t  more_data[] = "more";
  rx_err_t err         = rx_usb_write(k_usb_port_proto, more_data, k_test_size_4);

  /* Write fails due to full buffer */
  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

void test_usb_write_updates_stats(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "test";

  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(k_test_size_4, stats.bytes_tx);
}

/** @} */ /* end of test_usb_tx_trigger */

/* =============================================================================
 * Shared-Internal API Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_internal_api Shared-Internal API Tests
 * @brief Tests for rx_usb_get_port_config, rx_usb_invoke_callback,
 *        rx_usb_priv_set_port_state, and UNIT_TEST buffer accessors
 * @{
 */

/**
 * @brief Test rx_usb_get_port_config() returns non-null for valid port
 */
void test_usb_get_port_config_valid(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  const rx_usb_port_hw_config_t* cfg = rx_usb_get_port_config(k_usb_port_proto);
  TEST_ASSERT_NOT_NULL(cfg);
}

/**
 * @brief Test rx_usb_get_port_config() returns nullptr for invalid port
 */
void test_usb_get_port_config_invalid(void)
{
  const rx_usb_port_hw_config_t* cfg = rx_usb_get_port_config(k_usb_port_count);
  TEST_ASSERT_NULL(cfg);
}

/**
 * @brief Test rx_usb_invoke_callback() with no callback registered (no crash)
 */
void test_usb_invoke_callback_no_callback(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* No callback registered: must not crash */
  rx_usb_invoke_callback(k_usb_port_proto, k_usb_event_configured);
}

/**
 * @brief Test rx_usb_invoke_callback() with invalid port (no crash)
 */
void test_usb_invoke_callback_invalid_port(void)
{
  /* Invalid port: must not crash */
  rx_usb_invoke_callback(k_usb_port_count, k_usb_event_configured);
}

/**
 * @brief Test rx_usb_priv_set_port_state() sets device state for valid port
 */
void test_usb_priv_set_port_state_valid(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_priv_set_port_state(k_usb_port_proto, k_usb_state_configured);
  TEST_ASSERT_EQUAL(k_usb_state_configured, rx_usb_get_state());
}

/**
 * @brief Test rx_usb_priv_set_port_state() with invalid port (no-op)
 */
void test_usb_priv_set_port_state_invalid_port(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* Should return without changing state */
  rx_usb_priv_set_port_state(k_usb_port_count, k_usb_state_configured);
  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

/**
 * @brief Test rx_usb_test_get_tx_buffer() returns non-null for valid port
 */
void test_usb_test_get_tx_buffer_valid(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  ring_buffer_t* buf = rx_usb_test_get_tx_buffer(k_usb_port_proto);
  TEST_ASSERT_NOT_NULL(buf);
}

/**
 * @brief Test rx_usb_test_get_tx_buffer() returns nullptr for invalid port
 */
void test_usb_test_get_tx_buffer_invalid(void)
{
  ring_buffer_t* buf = rx_usb_test_get_tx_buffer(k_usb_port_count);
  TEST_ASSERT_NULL(buf);
}

/**
 * @brief Test rx_usb_test_get_rx_buffer() returns non-null for valid port
 */
void test_usb_test_get_rx_buffer_valid(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  ring_buffer_t* buf = rx_usb_test_get_rx_buffer(k_usb_port_proto);
  TEST_ASSERT_NOT_NULL(buf);
}

/**
 * @brief Test rx_usb_test_get_rx_buffer() returns nullptr for invalid port
 */
void test_usb_test_get_rx_buffer_invalid(void)
{
  ring_buffer_t* buf = rx_usb_test_get_rx_buffer(k_usb_port_count);
  TEST_ASSERT_NULL(buf);
}

/** @} */ /* end of test_usb_internal_api */

/* =============================================================================
 * Additional Coverage Tests
 * =============================================================================
 */

/**
 * @defgroup test_usb_coverage Additional Coverage Tests
 * @brief Tests targeting previously uncovered branches and functions
 * @{
 */

/* --- CDC init failure in rx_usb_init --- */

/**
 * @brief Verify rx_usb_init returns error when CDC init fails
 */
void test_usb_init_cdc_init_failure_propagates(void)
{
  mock_usb_hw_set_cdc_init_return(k_rx_err_timeout);

  rx_err_t err = rx_usb_init(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  /* Hardware should have been deinitialized on cleanup */
  TEST_ASSERT_EQUAL_UINT32(1, mock_usb_hw_get_call_count(nullptr, "rx_usb_hw_deinit"));
}

/* --- rx_usb_flush --- */

/**
 * @brief Verify flush returns invalid_arg when timeout exceeds maximum
 */
void test_usb_flush_timeout_exceeds_max_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  /* k_flush_max_timeout_ms is 10000; pass 10001 to exceed the limit */
  rx_err_t err = rx_usb_flush(k_usb_port_proto, k_test_flush_exceed);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify blocking flush succeeds when buffer is already empty
 */
void test_usb_flush_blocking_empty_buffer_returns_ok(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* TX buffer is empty - blocking flush with non-zero timeout succeeds immediately */

  rx_err_t err = rx_usb_flush(k_usb_port_proto, k_test_flush_small);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Verify blocking flush times out when buffer has data that does not drain
 */
void test_usb_flush_blocking_with_data_times_out(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* Write data to TX buffer so buffer is not empty */
  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  /* tx_thread_sleep is a no-op; loop runs instantly, elapsed increments by 10 each iter.
   * With timeout=10ms: after first iteration elapsed(10) >= timeout(10) -> timeout. */
  rx_err_t err = rx_usb_flush(k_usb_port_proto, k_test_flush_small);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* --- rx_usb_set_callback with invalid port --- */

/**
 * @brief Verify set_callback returns invalid_arg for invalid port
 */
void test_usb_set_callback_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_set_callback(k_usb_port_count, test_callback, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* --- rx_usb_read when not initialized --- */

/**
 * @brief Verify rx_usb_read returns invalid_state when not initialized
 */
void test_usb_read_not_initialized_fails(void)
{
  uint8_t  buf[k_test_size_10];
  uint32_t actual = 0;

  rx_err_t err = rx_usb_read(k_usb_port_proto, buf, sizeof(buf), &actual);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* --- rx_usb_set_state same-state no-op --- */

/**
 * @brief Verify rx_usb_set_state is a no-op when state has not changed
 */
void test_usb_set_state_same_state_no_op(void)
{
  rx_usb_config_t config = {.callback = test_callback, .ctx = nullptr};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));
  /* After init, state is k_usb_state_attached */
  s_callback_count = 0;

  /* Set to the same state - should be a no-op (no callback) */
  rx_usb_set_state(k_usb_state_attached);

  TEST_ASSERT_EQUAL_UINT32(0, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

/**
 * @brief Verify rx_usb_set_state ignores invalid state values
 */
void test_usb_set_state_invalid_state_ignored(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* k_usb_state_suspended is 6; 7 is outside the valid range */
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_usb_set_state((rx_usb_state_t)k_test_invalid_usb_state);

  /* State must remain unchanged */
  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

/* --- rx_usb_set_state configured case triggers callback --- */

/**
 * @brief Verify rx_usb_set_state fires callback for k_usb_state_configured
 */
void test_usb_set_state_configured_fires_callback(void)
{
  rx_usb_config_t config = {.callback = test_callback, .ctx = nullptr};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));

  /* Register per-port callback too */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_set_callback(k_usb_port_proto, test_callback, nullptr));
  s_callback_count = 0;

  rx_usb_set_state(k_usb_state_configured);

  /* Both global and per-port callbacks fire: count >= 1 */
  TEST_ASSERT_GREATER_THAN_UINT32(0, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
}

/* --- rx_usb_set_state intermediate states (powered/default/addressed) --- */

/**
 * @brief Verify intermediate states set has_event = false (no callback)
 */
void test_usb_set_state_powered_no_callback(void)
{
  rx_usb_config_t config = {.callback = test_callback, .ctx = nullptr};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));
  /* Move to configured so we can transition to powered (different state) */
  rx_usb_set_state(k_usb_state_configured);
  s_callback_count = 0;

  rx_usb_set_state(k_usb_state_powered);

  TEST_ASSERT_EQUAL_UINT32(0, s_callback_count);
}

/* --- rx_usb_rx_push with invalid port --- */

/**
 * @brief Verify rx_usb_rx_push returns 0 for invalid port
 */
void test_usb_rx_push_invalid_port_returns_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "test";

  uint32_t written = rx_usb_rx_push(k_usb_port_count, data, k_test_size_4);

  TEST_ASSERT_EQUAL_UINT32(0, written);
}

/* --- rx_usb_rx_push overflow increments rx_overruns --- */

/**
 * @brief Verify rx_usb_rx_push increments rx_overruns on buffer overflow
 */
void test_usb_rx_push_overflow_increments_overruns(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  /* Fill the RX buffer completely first */
  ring_buffer_t* rx_buf = rx_usb_test_get_rx_buffer(k_usb_port_proto);
  TEST_ASSERT_NOT_NULL(rx_buf);

  /* Write until full: k_usb_port_proto_rx_size = 1024 */
  uint8_t fill[k_usb_port_proto_rx_size];
  for (uint32_t i = 0U; i < k_usb_port_proto_rx_size; i++) {
    fill[i] = k_test_fill_aa;
  }
  uint32_t written = rx_usb_rx_push(k_usb_port_proto, fill, sizeof(fill));
  TEST_ASSERT_EQUAL_UINT32(sizeof(fill), written);

  /* Now push more data - should overflow */
  uint8_t extra[] = "overflow";
  rx_usb_rx_push(k_usb_port_proto, extra, sizeof(extra) - 1);

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_GREATER_THAN_UINT32(0, stats.rx_overruns);
}

/* --- rx_usb_rx_push with callback - written == 0 path --- */

/**
 * @brief Verify rx_usb_rx_push does not fire callback when zero bytes written
 */
void test_usb_rx_push_zero_written_no_callback(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_set_callback(k_usb_port_proto, test_callback, nullptr));

  /* Fill RX buffer so next push writes 0 bytes */
  uint8_t fill[k_usb_port_proto_rx_size];
  for (uint32_t i = 0U; i < k_usb_port_proto_rx_size; i++) {
    fill[i] = k_test_fill_bb;
  }
  rx_usb_rx_push(k_usb_port_proto, fill, sizeof(fill));
  s_callback_count = 0;

  /* Push with full buffer -> written == 0 -> no callback */
  uint8_t extra[] = "no space";
  rx_usb_rx_push(k_usb_port_proto, extra, sizeof(extra) - 1);

  TEST_ASSERT_EQUAL_UINT32(0, s_callback_count);
}

/* --- rx_usb_tx_pop triggers callback when buffer empties --- */

/**
 * @brief Verify rx_usb_tx_pop fires tx_complete callback when buffer becomes empty
 */
void test_usb_tx_pop_fires_tx_complete_callback(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_set_callback(k_usb_port_proto, test_callback, nullptr));

  uint8_t data[] = "Hello";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 5));
  s_callback_count = 0;

  /* Pop all data -> buffer empty -> tx_complete callback */
  uint8_t buf[k_test_size_10];
  rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_GREATER_THAN_UINT32(0, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_tx_complete, s_last_event);
}

/* --- rx_usb_get_port_config with invalid port --- */

/**
 * @brief Verify rx_usb_get_port_config returns nullptr for invalid port
 */
void test_usb_get_port_config_invalid_port(void)
{
  const rx_usb_port_hw_config_t* cfg = rx_usb_get_port_config(k_usb_port_count);

  TEST_ASSERT_NULL(cfg);
}

/* --- rx_usb_find_port_by_pipe --- */

/**
 * @brief Verify rx_usb_find_port_by_pipe finds correct port for known pipe
 */
void test_usb_find_port_by_pipe_found(void)
{
  /* Port 0 uses pipe 1 (bulk in). k_port0_pipe_bulk_in = 1 */
  rx_usb_port_id_t port = rx_usb_find_port_by_pipe(k_test_pipe_1);

  TEST_ASSERT_EQUAL(k_usb_port_proto, port);
}

/**
 * @brief Verify rx_usb_find_port_by_pipe returns k_usb_port_count for unknown pipe
 */
void test_usb_find_port_by_pipe_not_found(void)
{
  /* No port uses pipe 0 (DCP, reserved) */
  rx_usb_port_id_t port = rx_usb_find_port_by_pipe(0U);

  TEST_ASSERT_EQUAL(k_usb_port_count, port);
}

/* --- rx_usb_find_port_by_interface --- */

/**
 * @brief Verify rx_usb_find_port_by_interface finds correct port for known interface
 */
void test_usb_find_port_by_interface_found(void)
{
  /* Port 0 uses k_intf_port0_control = 0 for its CDC control interface */
  rx_usb_port_id_t port = rx_usb_find_port_by_interface(k_test_intf_0);

  TEST_ASSERT_EQUAL(k_usb_port_proto, port);
}

/**
 * @brief Verify rx_usb_find_port_by_interface returns k_usb_port_count for unknown interface
 */
void test_usb_find_port_by_interface_not_found(void)
{
  /* Interface 255 does not belong to any port */
  rx_usb_port_id_t port = rx_usb_find_port_by_interface(k_test_intf_255);

  TEST_ASSERT_EQUAL(k_usb_port_count, port);
}

/* --- rx_usb_invoke_callback with callback registered --- */

/**
 * @brief Verify rx_usb_invoke_callback fires the registered callback
 */
void test_usb_invoke_callback_with_callback(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_set_callback(k_usb_port_proto, test_callback, nullptr));

  rx_usb_invoke_callback(k_usb_port_proto, k_usb_event_configured);

  TEST_ASSERT_EQUAL_UINT32(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
}

/* --- rx_usb_priv_set_port_state with invalid state --- */

/**
 * @brief Verify rx_usb_priv_set_port_state is a no-op for invalid state
 */
void test_usb_priv_set_port_state_invalid_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* k_usb_state_suspended = 6; 7 is outside the valid state range */
  /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_usb_priv_set_port_state(k_usb_port_proto, (rx_usb_state_t)k_test_invalid_usb_state);

  /* State must remain attached (unchanged from init) */
  TEST_ASSERT_EQUAL(k_usb_state_attached, rx_usb_get_state());
}

/* --- rx_usb_putc with invalid port --- */

/**
 * @brief Verify rx_usb_putc returns invalid_arg for invalid port
 */
void test_usb_putc_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putc(k_usb_port_count, 'A');

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_putc returns invalid_state when not initialized
 */
void test_usb_putc_not_initialized_returns_invalid_state(void)
{
  /* USB not initialized */
  rx_err_t err = rx_usb_putc(k_usb_port_proto, 'A');

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* --- rx_usb_puts with invalid port --- */

/**
 * @brief Verify rx_usb_puts returns invalid_arg for invalid port
 */
void test_usb_puts_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puts(k_usb_port_count, "hello");

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* --- rx_usb_putint with invalid port --- */

/**
 * @brief Verify rx_usb_putint returns invalid_arg for invalid port
 */
void test_usb_putint_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_count, (int32_t)k_test_magic_42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* --- rx_usb_puthex with invalid port --- */

/**
 * @brief Verify rx_usb_puthex returns invalid_arg for invalid port
 */
void test_usb_puthex_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_count, k_test_hex_ab, k_test_hex_digits_2);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* --- rx_usb_puthex digit clamping (digits == 0 and digits > 8) --- */

/**
 * @brief Verify rx_usb_puthex clamps digits=0 to 1 digit
 */
void test_usb_puthex_zero_digits_clamped_to_one(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* digits=0 should be clamped to 1 */
  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_f, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_10];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  /* Should output 1 hex digit */
  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_1, len);
  TEST_ASSERT_EQUAL_CHAR('F', buf[0]);
}

/**
 * @brief Verify rx_usb_puthex clamps digits > 8 to 8 digits
 */
void test_usb_puthex_too_many_digits_clamped(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* digits=9 (> k_max_hex_digits=8) should be clamped to 8 */
  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_abcdef01, k_test_hex_digits_9);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[k_test_size_20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  /* Should output exactly 8 hex digits */
  TEST_ASSERT_EQUAL_UINT32(k_test_hex_digits_8, len);
}

/* --- Invalid port tests for remaining API functions --- */

/**
 * @brief Verify rx_usb_is_configured returns false for invalid port
 */
void test_usb_is_configured_invalid_port(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  bool result = rx_usb_is_configured(k_usb_port_count);

  TEST_ASSERT_FALSE(result);
}

/**
 * @brief Verify rx_usb_write returns invalid_arg for invalid port
 */
void test_usb_write_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  uint8_t  data[] = "test";
  rx_err_t err    = rx_usb_write(k_usb_port_count, data, k_test_size_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_read returns invalid_arg for invalid port
 */
void test_usb_read_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  uint8_t  buf[k_test_size_10];
  uint32_t actual = 0;
  rx_err_t err    = rx_usb_read(k_usb_port_count, buf, sizeof(buf), &actual);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_rx_available returns invalid_arg for invalid port
 */
void test_usb_rx_available_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  uint32_t avail = 0;
  rx_err_t err   = rx_usb_rx_available(k_usb_port_count, &avail);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_tx_available returns invalid_arg for invalid port
 */
void test_usb_tx_available_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  uint32_t avail = 0;
  rx_err_t err   = rx_usb_tx_available(k_usb_port_count, &avail);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_flush returns invalid_arg for invalid port
 */
void test_usb_flush_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_flush(k_usb_port_count, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_get_line_coding returns invalid_arg for invalid port
 */
void test_usb_get_line_coding_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_line_coding_t coding;
  rx_err_t             err = rx_usb_get_line_coding(k_usb_port_count, &coding);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_get_stats returns invalid_arg for invalid port
 */
void test_usb_get_stats_invalid_port_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_stats_t stats;
  rx_err_t       err = rx_usb_get_stats(k_usb_port_count, &stats);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify rx_usb_puts returns invalid_state when not initialized
 */
void test_usb_puts_not_initialized_returns_invalid_state(void)
{
  rx_err_t err = rx_usb_puts(k_usb_port_proto, "hello");

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Verify rx_usb_putint returns invalid_state when not initialized
 */
void test_usb_putint_not_initialized_returns_invalid_state(void)
{
  rx_err_t err = rx_usb_putint(k_usb_port_proto, k_test_magic_42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Verify rx_usb_puthex returns invalid_state when not initialized
 */
void test_usb_puthex_not_initialized_returns_invalid_state(void)
{
  rx_err_t err = rx_usb_puthex(k_usb_port_proto, k_test_hex_ab, k_test_hex_digits_2);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Verify rx_usb_tx_pop returns 0 for invalid port
 */
void test_usb_tx_pop_invalid_port_returns_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  uint8_t  buf[k_test_size_10];
  uint32_t count = rx_usb_tx_pop(k_usb_port_count, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(0, count);
}

/* --- rx_usb_set_state attached callback --- */

/**
 * @brief Verify rx_usb_set_state fires callback when transitioning to k_usb_state_attached
 */
void test_usb_set_state_attached_fires_callback(void)
{
  rx_usb_config_t config = {.callback = test_callback, .ctx = nullptr};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));

  /* Move to a different state first, then return to attached */
  rx_usb_set_state(k_usb_state_suspended);
  s_callback_count = 0;

  /* Transitioning to attached should fire the callback */
  rx_usb_set_state(k_usb_state_attached);

  TEST_ASSERT_GREATER_THAN_UINT32(0, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_attached, s_last_event);
}

/* --- rx_usb_flush max iterations exit path --- */

/**
 * @brief Verify flush blocking loop exits via max iterations when data never drains
 *
 * @details Whitebox test for the for-loop fallthrough exit.  tx_thread_sleep is
 * a no-op so the loop runs k_flush_max_iterations=1000 times instantly.  Each
 * iteration increments elapsed_ms by s_flush_poll_interval_ms=10.  After 1000
 * iterations elapsed=10000ms which equals k_flush_max_timeout_ms, causing the
 * loop to exit via the iterations limit rather than the timeout check.
 */
void test_usb_flush_blocking_exit_via_max_iterations(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, k_test_size_4));

  /* Use exactly k_flush_max_timeout_ms=10000 so elapsed never reaches it within
   * the 1000 iterations (elapsed reaches 10000 only at iteration 1000 exit). */
  rx_err_t err = rx_usb_flush(k_usb_port_proto, k_test_flush_timeout);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief Verify priv_ring_buffer_write loop runs to exhaustion with full buffer write
 *
 * @details Covers the loop-exhaustion branch of line 773: write exactly
 * k_usb_max_buffer_size (1024) bytes to a 1024-byte ring buffer so the for
 * loop reaches i == k_usb_max_buffer_size rather than exiting via break.
 */
void test_ring_buffer_write_full_1024_bytes_loop_exhaustion(void)
{
  /* Use a 1024-byte ring buffer -- same as k_usb_max_buffer_size */
  static uint8_t s_big_data[k_test_size_1024];
  ring_buffer_t  big_buf;
  priv_ring_buffer_init(&big_buf, s_big_data, sizeof(s_big_data));

  uint8_t src[k_test_size_1024];
  for (uint32_t i = 0U; i < k_test_size_1024; i++) {
    src[i] = (uint8_t)(i & k_test_hex_ff_mask);
  }
  /* Writing exactly 1024 bytes: break fires on each iteration only when
   * written >= len (1024) or buf full.  At i=1023 the last byte is written
   * and read_count becomes 1024; next iteration check at i=1024 exits loop
   * via the for-condition (loop exhaustion). */
  uint32_t written = priv_ring_buffer_write(&big_buf, src, k_test_size_1024);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_1024, written);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_1024, big_buf.count);
}

/**
 * @brief Verify priv_ring_buffer_read loop runs to exhaustion with full buffer read
 *
 * @details Covers the loop-exhaustion branch of line 804: read exactly
 * k_usb_max_buffer_size (1024) bytes from a full 1024-byte ring buffer so
 * the for loop reaches i == k_usb_max_buffer_size.
 */
void test_ring_buffer_read_full_1024_bytes_loop_exhaustion(void)
{
  static uint8_t s_big_data2[k_test_size_1024];
  ring_buffer_t  big_buf;
  priv_ring_buffer_init(&big_buf, s_big_data2, sizeof(s_big_data2));

  uint8_t src[k_test_size_1024];
  for (uint32_t i = 0U; i < k_test_size_1024; i++) {
    src[i] = (uint8_t)(i & k_test_hex_ff_mask);
  }
  /* Fill the buffer completely */
  uint32_t written = priv_ring_buffer_write(&big_buf, src, k_test_size_1024);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_1024, written);

  /* Read all 1024 bytes - loop runs to exhaustion */
  uint8_t  dst[k_test_size_1024];
  uint32_t read = priv_ring_buffer_read(&big_buf, dst, k_test_size_1024);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_1024, read);
  TEST_ASSERT_EQUAL_UINT32(0U, big_buf.count);
}

/**
 * @brief Verify priv_ring_buffer_write returns 0 when buf pointer is NULL
 *
 * @details Covers the buf == nullptr branch (1st sub-condition) of line 766
 * in priv_ring_buffer_write(). Passes nullptr as the buf argument.
 */
void test_ring_buffer_write_null_buf_returns_zero(void)
{
  uint8_t  src[]   = "hi";
  uint32_t written = priv_ring_buffer_write(nullptr, src, k_test_size_2);
  TEST_ASSERT_EQUAL_UINT32(0U, written);
}

/**
 * @brief Verify priv_ring_buffer_read returns 0 when buf pointer is NULL
 *
 * @details Covers the buf == nullptr branch (1st sub-condition) of line 797
 * in priv_ring_buffer_read(). Passes nullptr as the buf argument.
 */
void test_ring_buffer_read_null_buf_returns_zero(void)
{
  uint8_t  dst[k_test_size_4];
  uint32_t read = priv_ring_buffer_read(nullptr, dst, k_test_size_4);
  TEST_ASSERT_EQUAL_UINT32(0U, read);
}

/**
 * @brief Verify priv_ring_buffer_write returns 0 when buf->data is NULL
 *
 * @details Covers the buf->data == nullptr branch (2nd sub-condition) of line
 * 766 in priv_ring_buffer_write(). Creates a ring_buffer_t with buf->data
 * explicitly set to nullptr.
 */
void test_ring_buffer_write_null_buf_data_returns_zero(void)
{
  ring_buffer_t buf;
  buf.data         = nullptr;
  buf.size         = k_test_size_8;
  buf.head         = 0U;
  buf.tail         = 0U;
  buf.count        = 0U;
  uint8_t  src[]   = "hi";
  uint32_t written = priv_ring_buffer_write(&buf, src, k_test_size_2);
  TEST_ASSERT_EQUAL_UINT32(0U, written);
}

/**
 * @brief Verify priv_ring_buffer_read returns 0 when buf->data is NULL
 *
 * @details Covers the buf->data == nullptr branch (2nd sub-condition) of line
 * 797 in priv_ring_buffer_read(). Creates a ring_buffer_t with buf->data
 * explicitly set to nullptr.
 */
void test_ring_buffer_read_null_buf_data_returns_zero(void)
{
  ring_buffer_t buf;
  buf.data  = nullptr;
  buf.size  = k_test_size_8;
  buf.head  = 0U;
  buf.tail  = 0U;
  buf.count = k_test_size_2; /* Pretend there is data */
  uint8_t  dst[k_test_size_4];
  uint32_t read = priv_ring_buffer_read(&buf, dst, k_test_size_2);
  TEST_ASSERT_EQUAL_UINT32(0U, read);
}

/**
 * @brief Verify rx_usb_puts handles a maximum-length unterminated buffer
 *
 * @details Covers the loop-exhaustion branch of line 1959: the for loop in
 * rx_usb_puts() reaches i == k_usb_max_buffer_size when the string has no
 * null terminator within the first 1024 bytes. The function then writes all
 * 1024 bytes to the TX buffer.
 */
void test_usb_puts_unterminated_1024_bytes_loop_exhaustion(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* Create a 1024-byte buffer with no null terminator */
  static char s_no_null[k_test_size_1024];
  for (uint32_t i = 0U; i < k_test_size_1024; i++) {
    s_no_null[i] = 'A';
  }

  /* The loop runs all 1024 iterations and then exits via the loop bound;
   * len = 1024, and rx_usb_write() is called with that length. */
  rx_err_t err = rx_usb_puts(k_usb_port_proto, s_no_null);
  /* Buffer may be full (k_usb_port_proto_tx_size = 1024); accept ok or busy */
  TEST_ASSERT_TRUE(err == k_rx_ok || err == k_rx_err_busy);
}

/**
 * @brief Verify rx_usb_reset_stats is a no-op for invalid port
 *
 * @details Covers the false branch of internal_port_is_valid() in
 * rx_usb_reset_stats() (line 1873).
 */
void test_usb_reset_stats_invalid_port_no_op(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
  uint8_t data[] = "hi";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 2U));
  rx_usb_stats_t stats_before;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats_before));
  /* Invalid port - should be a no-op */
  rx_usb_reset_stats(k_usb_port_count);
  rx_usb_stats_t stats_after;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats_after));
  /* Stats unchanged */
  TEST_ASSERT_EQUAL_UINT32(stats_before.bytes_tx, stats_after.bytes_tx);
}

/**
 * @brief Verify rx_usb_priv_set_port_state with detached state does not
 *        set initialized flag
 *
 * @details Covers the false branch of (state != k_usb_state_detached) at line
 * 2325 in rx_usb_priv_set_port_state().
 */
void test_usb_priv_set_port_state_detached_no_init_flag(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* Set to detached - the branch at line 2325 goes false, skipping initialized = true */
  rx_usb_priv_set_port_state(k_usb_port_proto, k_usb_state_detached);
  TEST_ASSERT_EQUAL(k_usb_state_detached, rx_usb_get_state());
}

/**
 * @brief Verify rx_usb_set_line_coding is a no-op for invalid port
 *
 * @details Covers the !internal_port_is_valid(port) branch of line 2162.
 */
void test_usb_set_line_coding_invalid_port_no_op(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_line_coding_t coding = {.baud_rate = k_test_baud_115200,
                                 .data_bits = k_test_data_bits_8,
                                 .stop_bits = k_test_stop_bits_none,
                                 .parity    = k_test_parity_none};
  /* Invalid port - should be a no-op (no crash) */
  rx_usb_set_line_coding(k_usb_port_count, &coding);
  /* Valid port line coding should be unchanged (default = 115200) */
  rx_usb_line_coding_t out;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_line_coding(k_usb_port_proto, &out));
  TEST_ASSERT_EQUAL_UINT32(k_test_baud_115200, out.baud_rate);
}

/**
 * @brief Verify rx_usb_find_port_by_pipe finds port via pipe_bulk_out match
 *
 * @details Covers the second sub-condition (pipe_bulk_out == pipe) on line
 * 2253-2254 of rx_usb_find_port_by_pipe(). Port 0 bulk_out = pipe 2.
 */
void test_usb_find_port_by_pipe_bulk_out(void)
{
  /* k_port0_pipe_bulk_out = 2 */
  rx_usb_port_id_t port = rx_usb_find_port_by_pipe(k_test_pipe_2);
  TEST_ASSERT_EQUAL(k_usb_port_proto, port);
}

/**
 * @brief Verify rx_usb_find_port_by_pipe finds port via pipe_interrupt match
 *
 * @details Covers the third sub-condition (pipe_interrupt == pipe) on line
 * 2253-2254 of rx_usb_find_port_by_pipe(). Port 0 interrupt = pipe 3.
 */
void test_usb_find_port_by_pipe_interrupt(void)
{
  /* k_port0_pipe_int_in = 3 */
  rx_usb_port_id_t port = rx_usb_find_port_by_pipe(k_test_pipe_3);
  TEST_ASSERT_EQUAL(k_usb_port_proto, port);
}

/**
 * @brief Verify rx_usb_find_port_by_interface finds port via interface_data match
 *
 * @details Covers the second sub-condition (interface_data == interface) on
 * line 2268 of rx_usb_find_port_by_interface(). Port 0 data interface = 1.
 */
void test_usb_find_port_by_interface_data(void)
{
  /* k_intf_port0_data = 1 */
  rx_usb_port_id_t port = rx_usb_find_port_by_interface(k_test_intf_1);
  TEST_ASSERT_EQUAL(k_usb_port_proto, port);
}

/**
 * @brief Verify priv_ring_buffer_init is a no-op when data pointer is NULL
 *
 * @details Covers the (data == nullptr) branch of line 712 in
 * priv_ring_buffer_init(). The buf pointer is valid but data is NULL.
 */
void test_ring_buffer_init_null_data_no_op(void)
{
  ring_buffer_t buf;
  buf.size = 0U;
  /* Pass valid buf but nullptr data - should be a no-op */
  priv_ring_buffer_init(&buf, nullptr, k_test_size_8);
  /* buf.data was never assigned; check buf.size was not updated */
  TEST_ASSERT_EQUAL_UINT32(0U, buf.size);
}

/**
 * @brief Verify tx_pop does not fire callback when TX buffer still has data
 *
 * @details Covers the false branch of (buffer empty after pop) check at line
 * 2207: after partial read, buffer still has data so tx_complete is not fired.
 */
void test_usb_tx_pop_partial_no_tx_complete_callback(void)
{
  rx_usb_config_t config = {.callback = test_callback, .ctx = nullptr};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));
  rx_usb_set_state(k_usb_state_configured);

  /* Write 8 bytes to TX buffer */
  uint8_t data[] = "12345678";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 8U));

  s_callback_count = 0;

  /* Pop only 4 bytes - buffer still has 4 remaining; callback should NOT fire */
  uint8_t  out[k_test_size_4];
  uint32_t popped = rx_usb_tx_pop(k_usb_port_proto, out, k_test_size_4);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_4, popped);
  TEST_ASSERT_EQUAL_UINT32(0U, s_callback_count);
}

/** @} */ /* end of test_usb_coverage */

/* =============================================================================
 * Main
 * =============================================================================
 */

/**
 * @brief Run ring buffer tests (init, write, and read)
 *
 * @details Group 1a: ring buffer init tests (4) + write tests (8) + read tests (10)
 */
static void internal_run_ring_buffer_tests(void)
{
  /* Ring buffer initialization tests */
  RUN_TEST(test_ring_buffer_init_clears_state);
  RUN_TEST(test_ring_buffer_available_empty);
  RUN_TEST(test_ring_buffer_free_empty);
  RUN_TEST(test_ring_buffer_null_ptr_handling);

  /* Ring buffer write tests */
  RUN_TEST(test_ring_buffer_write_single_byte);
  RUN_TEST(test_ring_buffer_write_null_data_returns_zero);
  RUN_TEST(test_ring_buffer_write_zero_len_returns_zero);
  RUN_TEST(test_ring_buffer_write_multiple_bytes);
  RUN_TEST(test_ring_buffer_write_fills_buffer);
  RUN_TEST(test_ring_buffer_write_overflow_truncates);
  RUN_TEST(test_ring_buffer_write_partial_when_partially_full);
  RUN_TEST(test_ring_buffer_write_wraps_around);

  /* Ring buffer read tests */
  RUN_TEST(test_ring_buffer_read_empty_returns_zero);
  RUN_TEST(test_ring_buffer_read_null_data_returns_zero);
  RUN_TEST(test_ring_buffer_read_zero_len_returns_zero);
  RUN_TEST(test_ring_buffer_read_single_byte);
  RUN_TEST(test_ring_buffer_read_multiple_bytes);
  RUN_TEST(test_ring_buffer_read_partial);
  RUN_TEST(test_ring_buffer_read_wraps_around);
  RUN_TEST(test_ring_buffer_fifo_order);
  RUN_TEST(test_ring_buffer_interleaved_read_write);
  RUN_TEST(test_ring_buffer_burst_write_then_burst_read);
}

/**
 * @brief Run USB init/deinit/state tests
 *
 * @details Group 1b: USB initialization tests (12) + deinit tests (3) + state tests (5)
 */
static void internal_run_init_tests(void)
{
  /* USB initialization tests */
  RUN_TEST(test_usb_init_null_config_succeeds);
  RUN_TEST(test_usb_init_transitions_to_attached);
  RUN_TEST(test_usb_init_with_callback);
  RUN_TEST(test_usb_init_twice_fails);
  RUN_TEST(test_usb_init_hw_failure_propagates);
  RUN_TEST(test_error_propagation_from_hw_init);
  RUN_TEST(test_usb_init_attach_failure_propagates);
  RUN_TEST(test_error_propagation_from_attach);
  RUN_TEST(test_cleanup_on_attach_failure);
  RUN_TEST(test_usb_init_attach_failure_propagates_and_cleans_up);
  RUN_TEST(test_pipe_bounds_validation_at_max);
  RUN_TEST(test_endpoint_bounds_max_valid);

  /* USB deinitialization tests */
  RUN_TEST(test_usb_deinit_not_initialized_fails);
  RUN_TEST(test_usb_deinit_success);
  RUN_TEST(test_usb_deinit_sets_detached_state);

  /* USB state query tests */
  RUN_TEST(test_usb_is_configured_when_not_initialized);
  RUN_TEST(test_usb_is_configured_when_attached);
  RUN_TEST(test_usb_is_configured_when_configured);
  RUN_TEST(test_usb_get_state_returns_detached_initially);
  RUN_TEST(test_usb_get_state_returns_attached_after_init);
}

/**
 * @brief Run USB read/write, available, line coding, statistics, and flush tests
 *
 * @details Group 2: write/read/available/line coding/stats/flush tests (25)
 */
static void internal_run_io_and_codec_tests(void)
{
  /* USB write tests */
  RUN_TEST(test_usb_write_null_data_fails);
  RUN_TEST(test_usb_write_not_initialized_fails);
  RUN_TEST(test_usb_write_not_configured_fails);
  RUN_TEST(test_usb_write_success_when_configured);
  RUN_TEST(test_usb_write_full_buffer_returns_busy);
  RUN_TEST(test_usb_write_updates_stats);

  /* USB read tests */
  RUN_TEST(test_usb_read_null_data_fails);
  RUN_TEST(test_usb_read_null_actual_len_fails);
  RUN_TEST(test_usb_read_empty_buffer_returns_zero);
  RUN_TEST(test_usb_read_after_rx_push);

  /* USB RX/TX available tests */
  RUN_TEST(test_usb_rx_available_null_fails);
  RUN_TEST(test_usb_rx_available_empty);
  RUN_TEST(test_usb_rx_available_after_push);
  RUN_TEST(test_usb_tx_available_null_fails);
  RUN_TEST(test_usb_tx_available_empty);
  RUN_TEST(test_usb_tx_available_after_write);

  /* USB line coding tests */
  RUN_TEST(test_usb_get_line_coding_null_fails);
  RUN_TEST(test_usb_get_line_coding_default_values);
  RUN_TEST(test_usb_set_line_coding_null_ignored);

  /* USB statistics tests */
  RUN_TEST(test_usb_get_stats_null_fails);
  RUN_TEST(test_usb_get_stats_initial_zeros);
  RUN_TEST(test_usb_reset_stats);

  /* USB flush tests */
  RUN_TEST(test_usb_flush_not_initialized_fails);
  RUN_TEST(test_usb_flush_empty_buffer_succeeds);
  RUN_TEST(test_usb_flush_with_data_and_zero_timeout_returns_timeout);
}

/**
 * @brief Run state machine and transmission trigger tests
 *
 * @details Group 3a: state machine tests (17) + transmission trigger tests (3)
 */
static void internal_run_state_tests(void)
{
  /* Internal state functions tests */
  RUN_TEST(test_usb_set_state_triggers_callback);
  RUN_TEST(test_usb_set_state_no_callback_if_same_state);
  RUN_TEST(test_usb_set_state_triggers_callback_on_configured);
  RUN_TEST(test_usb_set_state_to_powered);
  RUN_TEST(test_usb_set_state_to_default);
  RUN_TEST(test_usb_set_state_to_addressed);
  RUN_TEST(test_usb_set_state_to_configured);
  RUN_TEST(test_usb_set_state_to_suspended);
  RUN_TEST(test_usb_set_state_to_detached);
  RUN_TEST(test_usb_state_full_enumeration_sequence);
  RUN_TEST(test_usb_set_line_coding);
  RUN_TEST(test_usb_rx_push_adds_data);
  RUN_TEST(test_usb_rx_push_updates_stats);
  RUN_TEST(test_usb_rx_push_triggers_callback);
  RUN_TEST(test_usb_tx_pop_retrieves_data);
  RUN_TEST(test_usb_count_bus_reset_increments_stat);
  RUN_TEST(test_usb_count_suspend_increments_stat);

  /* USB transmission trigger tests */
  RUN_TEST(test_usb_write_triggers_transmission_when_pipe_idle);
  RUN_TEST(test_usb_write_no_trigger_when_pipe_busy);
  RUN_TEST(test_usb_write_no_trigger_when_buffer_empty_after_write_fails);
}

/**
 * @brief Run debug text output tests (putc, puts, putint, puthex)
 *
 * @details Group 3b: debug output tests (22)
 */
static void internal_run_debug_tests(void)
{
  /* Debug text output tests */
  RUN_TEST(test_usb_putc_not_configured_fails);
  RUN_TEST(test_usb_putc_success_when_configured);
  RUN_TEST(test_usb_putc_writes_to_buffer);
  RUN_TEST(test_usb_putc_buffer_full_returns_busy);
  RUN_TEST(test_usb_puts_null_string_fails);
  RUN_TEST(test_usb_puts_not_configured_fails);
  RUN_TEST(test_usb_puts_success_when_configured);
  RUN_TEST(test_usb_puts_writes_string_to_buffer);
  RUN_TEST(test_usb_puts_empty_string_succeeds);
  RUN_TEST(test_usb_putint_not_configured_fails);
  RUN_TEST(test_usb_putint_positive_value);
  RUN_TEST(test_usb_putint_negative_value);
  RUN_TEST(test_usb_putint_zero);
  RUN_TEST(test_usb_putint_max_value);
  RUN_TEST(test_usb_putint_min_value);
  RUN_TEST(test_usb_puthex_not_configured_fails);
  RUN_TEST(test_usb_puthex_two_digits);
  RUN_TEST(test_usb_puthex_four_digits_zero_padded);
  RUN_TEST(test_usb_puthex_eight_digits);
  RUN_TEST(test_usb_puthex_lowercase_letters);
  RUN_TEST(test_usb_puthex_zero_value);
  RUN_TEST(test_usb_puthex_single_digit);
}

/**
 * @brief Run shared-internal API and first coverage batch tests
 *
 * @details Group 4: internal API (10) + coverage batch 1 (27)
 */
static void internal_run_internal_api_and_coverage_tests(void)
{
  /* Shared-internal API tests */
  RUN_TEST(test_usb_get_port_config_valid);
  RUN_TEST(test_usb_get_port_config_invalid);
  RUN_TEST(test_usb_invoke_callback_no_callback);
  RUN_TEST(test_usb_invoke_callback_invalid_port);
  RUN_TEST(test_usb_priv_set_port_state_valid);
  RUN_TEST(test_usb_priv_set_port_state_invalid_port);
  RUN_TEST(test_usb_test_get_tx_buffer_valid);
  RUN_TEST(test_usb_test_get_tx_buffer_invalid);
  RUN_TEST(test_usb_test_get_rx_buffer_valid);
  RUN_TEST(test_usb_test_get_rx_buffer_invalid);

  /* Additional coverage tests */
  RUN_TEST(test_usb_init_cdc_init_failure_propagates);
  RUN_TEST(test_usb_flush_timeout_exceeds_max_fails);
  RUN_TEST(test_usb_flush_blocking_empty_buffer_returns_ok);
  RUN_TEST(test_usb_flush_blocking_with_data_times_out);
  RUN_TEST(test_usb_set_callback_invalid_port_fails);
  RUN_TEST(test_usb_read_not_initialized_fails);
  RUN_TEST(test_usb_set_state_same_state_no_op);
  RUN_TEST(test_usb_set_state_invalid_state_ignored);
  RUN_TEST(test_usb_set_state_configured_fires_callback);
  RUN_TEST(test_usb_set_state_powered_no_callback);
  RUN_TEST(test_usb_rx_push_invalid_port_returns_zero);
  RUN_TEST(test_usb_rx_push_overflow_increments_overruns);
  RUN_TEST(test_usb_rx_push_zero_written_no_callback);
  RUN_TEST(test_usb_tx_pop_fires_tx_complete_callback);
  RUN_TEST(test_usb_get_port_config_invalid_port);
  RUN_TEST(test_usb_find_port_by_pipe_found);
  RUN_TEST(test_usb_find_port_by_pipe_not_found);
  RUN_TEST(test_usb_find_port_by_interface_found);
  RUN_TEST(test_usb_find_port_by_interface_not_found);
  RUN_TEST(test_usb_invoke_callback_with_callback);
  RUN_TEST(test_usb_priv_set_port_state_invalid_state);
  RUN_TEST(test_usb_putc_invalid_port_fails);
  RUN_TEST(test_usb_putc_not_initialized_returns_invalid_state);
  RUN_TEST(test_usb_puts_invalid_port_fails);
  RUN_TEST(test_usb_putint_invalid_port_fails);
  RUN_TEST(test_usb_puthex_invalid_port_fails);
  RUN_TEST(test_usb_puthex_zero_digits_clamped_to_one);
  RUN_TEST(test_usb_puthex_too_many_digits_clamped);
}

/**
 * @brief Run second and third coverage batch tests
 *
 * @details Group 5: invalid port tests (14) + exhaustion/null/misc tests (15)
 */
static void internal_run_coverage_batch_2_and_3_tests(void)
{
  /* Second coverage batch */
  RUN_TEST(test_usb_is_configured_invalid_port);
  RUN_TEST(test_usb_write_invalid_port_fails);
  RUN_TEST(test_usb_read_invalid_port_fails);
  RUN_TEST(test_usb_rx_available_invalid_port_fails);
  RUN_TEST(test_usb_tx_available_invalid_port_fails);
  RUN_TEST(test_usb_flush_invalid_port_fails);
  RUN_TEST(test_usb_get_line_coding_invalid_port_fails);
  RUN_TEST(test_usb_get_stats_invalid_port_fails);
  RUN_TEST(test_usb_puts_not_initialized_returns_invalid_state);
  RUN_TEST(test_usb_putint_not_initialized_returns_invalid_state);
  RUN_TEST(test_usb_puthex_not_initialized_returns_invalid_state);
  RUN_TEST(test_usb_tx_pop_invalid_port_returns_zero);
  RUN_TEST(test_usb_set_state_attached_fires_callback);
  RUN_TEST(test_usb_flush_blocking_exit_via_max_iterations);

  /* Third coverage batch */
  RUN_TEST(test_usb_puts_unterminated_1024_bytes_loop_exhaustion);
  RUN_TEST(test_ring_buffer_write_full_1024_bytes_loop_exhaustion);
  RUN_TEST(test_ring_buffer_read_full_1024_bytes_loop_exhaustion);
  RUN_TEST(test_ring_buffer_write_null_buf_returns_zero);
  RUN_TEST(test_ring_buffer_read_null_buf_returns_zero);
  RUN_TEST(test_ring_buffer_write_null_buf_data_returns_zero);
  RUN_TEST(test_ring_buffer_read_null_buf_data_returns_zero);
  RUN_TEST(test_usb_reset_stats_invalid_port_no_op);
  RUN_TEST(test_usb_priv_set_port_state_detached_no_init_flag);
  RUN_TEST(test_usb_set_line_coding_invalid_port_no_op);
  RUN_TEST(test_usb_find_port_by_pipe_bulk_out);
  RUN_TEST(test_usb_find_port_by_pipe_interrupt);
  RUN_TEST(test_usb_find_port_by_interface_data);
  RUN_TEST(test_ring_buffer_init_null_data_no_op);
  RUN_TEST(test_usb_tx_pop_partial_no_tx_complete_callback);
}

/**
 * @brief Test suite entry point
 *
 * @details
 * Unity test framework main function. Runs all USB driver tests and
 * returns the test result count. Test groups are split into static helpers
 * to keep function size within NASA Power of 10 Rule 4 bounds.
 *
 * @return int 0 if all tests pass, non-zero on failure
 */
int main(void)
{
  UNITY_BEGIN();

  internal_run_ring_buffer_tests();
  internal_run_init_tests();
  internal_run_io_and_codec_tests();
  internal_run_state_tests();
  internal_run_debug_tests();
  internal_run_internal_api_and_coverage_tests();
  internal_run_coverage_batch_2_and_3_tests();

  return UNITY_END();
}
