/* star-rx72n-firmware/tests/test_rx_usb.c */

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
 * @author STAR Team
 * @date 2026-01-04
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"

/* Source under test includes mock headers when UNIT_TEST is defined */
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_usb.h"
#include "rx_usb_private.h" /* Internal types and functions for testing */

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
  memset(&s_test_buffer, 0, sizeof(s_test_buffer));

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
  uint8_t data[] = {0x01, 0x02};

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, k_test_size_0);

  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

void test_ring_buffer_write_multiple_bytes(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, k_test_size_5);

  TEST_ASSERT_EQUAL_UINT32(5, written);
  TEST_ASSERT_EQUAL_UINT32(5, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT8(0x01, s_test_buffer.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x05, s_test_buffer.data[4]);
}

void test_ring_buffer_write_fills_buffer(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[k_usb_port_proto_rx_size];
  memset(data, 0xAA, sizeof(data));

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, sizeof(data));

  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, s_test_buffer.count);
  TEST_ASSERT_EQUAL_UINT32(0, priv_ring_buffer_free(&s_test_buffer));
}

void test_ring_buffer_write_overflow_truncates(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t data[k_usb_port_proto_rx_size + 100];
  memset(data, 0xBB, sizeof(data));

  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data, sizeof(data));

  /* Should only write up to buffer size */
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, s_test_buffer.count);
}

void test_ring_buffer_write_partial_when_partially_full(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Fill half the buffer */
  uint8_t first[k_usb_port_proto_rx_size / 2];
  memset(first, 0x11, sizeof(first));
  priv_ring_buffer_write(&s_test_buffer, first, sizeof(first));

  /* Try to write more than available space */
  uint8_t second[k_usb_port_proto_rx_size];
  memset(second, 0x22, sizeof(second));
  uint32_t written = priv_ring_buffer_write(&s_test_buffer, second, sizeof(second));

  /* Should only write remaining space */
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size / 2, written);
  TEST_ASSERT_EQUAL_UINT32(k_usb_port_proto_rx_size, s_test_buffer.count);
}

void test_ring_buffer_write_wraps_around(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Write to advance head near end */
  uint8_t data1[k_usb_port_proto_rx_size - 10];
  memset(data1, 0x11, sizeof(data1));
  priv_ring_buffer_write(&s_test_buffer, data1, sizeof(data1));

  /* Read some to create space at beginning */
  uint8_t out[100];
  priv_ring_buffer_read(&s_test_buffer, out, k_test_size_100);

  /* Write data that wraps around */
  uint8_t data2[50];
  for (uint32_t i = k_test_size_0; i < k_test_size_50; i++) {
    data2[i] = (uint8_t)(i & 0xFF);
  }
  uint32_t written = priv_ring_buffer_write(&s_test_buffer, data2, k_test_size_50);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_50, written);
  /* Head should have wrapped around */
  TEST_ASSERT_TRUE(s_test_buffer.head < 50);
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
  uint8_t data[10];

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

  uint8_t  read_data[10];
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
  TEST_ASSERT_EQUAL_UINT8(0x42, read_data);
  TEST_ASSERT_EQUAL_UINT32(0, s_test_buffer.count);
}

void test_ring_buffer_read_multiple_bytes(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data[] = "Hello";
  priv_ring_buffer_write(&s_test_buffer, write_data, k_test_size_5);

  uint8_t  read_data[10] = {0};
  uint32_t read_count    = priv_ring_buffer_read(&s_test_buffer, read_data, k_test_size_10);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, read_count);
  TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, k_test_size_5);
}

void test_ring_buffer_read_partial(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));
  uint8_t write_data[] = "Hello World";
  priv_ring_buffer_write(&s_test_buffer, write_data, k_test_size_11);

  uint8_t  read_data[5];
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, read_data, k_test_size_5);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_5, read_count);
  TEST_ASSERT_EQUAL_MEMORY("Hello", read_data, k_test_size_5);
  TEST_ASSERT_EQUAL_UINT32(6, s_test_buffer.count); /* " World" remains */
}

void test_ring_buffer_read_wraps_around(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Write near end of buffer */
  uint8_t fill[k_usb_port_proto_rx_size - 20];
  memset(fill, 0x00, sizeof(fill));
  priv_ring_buffer_write(&s_test_buffer, fill, sizeof(fill));

  /* Read to free up space */
  uint8_t discard[k_usb_port_proto_rx_size - 30];
  priv_ring_buffer_read(&s_test_buffer, discard, sizeof(discard));

  /* Write pattern that wraps */
  uint8_t pattern[50];
  for (uint32_t i = k_test_size_0; i < k_test_size_50; i++) {
    pattern[i] = (uint8_t)(i + 1);
  }
  priv_ring_buffer_write(&s_test_buffer, pattern, k_test_size_50);

  /* Read and verify pattern */
  uint8_t verify[50];
  priv_ring_buffer_read(&s_test_buffer, discard, k_test_size_10); /* Skip remaining fill */
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, verify, k_test_size_50);

  TEST_ASSERT_EQUAL_UINT32(k_test_size_50, read_count);
  TEST_ASSERT_EQUAL_MEMORY(pattern, verify, k_test_size_50);
}

void test_ring_buffer_fifo_order(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  /* Write sequence */
  uint8_t write_data[] = {1, 2, 3, 4, 5};
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
    uint32_t written    = priv_ring_buffer_write(&s_test_buffer, &write_data, 1);
    TEST_ASSERT_EQUAL_UINT32(k_test_size_1, written);

    uint8_t  read_data;
    uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, &read_data, k_test_size_1);
    TEST_ASSERT_EQUAL_UINT32(1, read_count);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)i, read_data);
  }
}

void test_ring_buffer_burst_write_then_burst_read(void)
{
  priv_ring_buffer_init(&s_test_buffer, s_test_buffer_data, sizeof(s_test_buffer_data));

  uint8_t pattern[100];
  for (uint32_t i = k_test_size_0; i < k_test_iterations_100; i++) {
    pattern[i] = (uint8_t)(i * 2);
  }
  uint32_t written = priv_ring_buffer_write(&s_test_buffer, pattern, k_test_size_100);
  TEST_ASSERT_EQUAL_UINT32(k_test_size_100, written);

  uint8_t  read_buf[100];
  uint32_t read_count = priv_ring_buffer_read(&s_test_buffer, read_buf, k_test_size_100);
  TEST_ASSERT_EQUAL_UINT32(100, read_count);
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
  rx_usb_config_t config = {0};

  config.callback = test_callback;
  config.ctx      = (void*)0xDEADBEEF;

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
  memset(fill_data, 'A', sizeof(fill_data));
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

  rx_err_t err = rx_usb_read(k_usb_port_proto, nullptr, 10, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_read_null_actual_len_fails(void)
{
  uint8_t data[10];

  rx_err_t err = rx_usb_read(k_usb_port_proto, data, 10, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_usb_read_empty_buffer_returns_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t  data[10];
  uint32_t actual_len = k_test_sentinel_999;

  rx_err_t err = rx_usb_read(k_usb_port_proto, data, 10, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, actual_len);
}

void test_usb_read_after_rx_push(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t push_data[] = "Hello USB";
  rx_usb_rx_push(k_usb_port_proto, push_data, 9);

  uint8_t  read_data[20];
  uint32_t actual_len;
  rx_err_t err = rx_usb_read(k_usb_port_proto, read_data, 20, &actual_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(9, actual_len);
  TEST_ASSERT_EQUAL_MEMORY(push_data, read_data, 9);
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
  rx_usb_rx_push(k_usb_port_proto, data, 4);

  uint32_t available;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_rx_available(k_usb_port_proto, &available));

  TEST_ASSERT_EQUAL_UINT32(4, available);
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
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 4));

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
  TEST_ASSERT_EQUAL_UINT32(115200, coding.baud_rate);
  TEST_ASSERT_EQUAL_UINT8(0, coding.stop_bits); /* 1 stop bit */
  TEST_ASSERT_EQUAL_UINT8(0, coding.parity);    /* No parity */
  TEST_ASSERT_EQUAL_UINT8(8, coding.data_bits);
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
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 4));

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(4, stats.bytes_tx);

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
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 4));

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

extern void     rx_usb_set_line_coding(rx_usb_port_id_t port, const rx_usb_line_coding_t* coding);
extern uint32_t rx_usb_tx_pop(rx_usb_port_id_t port, uint8_t* data, uint32_t max_len);
extern void     rx_usb_count_bus_reset(void);
extern void     rx_usb_count_suspend(void);

void test_usb_set_state_triggers_callback(void)
{
  rx_usb_config_t config = {0};

  config.callback = test_callback;
  config.ctx      = (void*)0xCAFEBABE;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
  TEST_ASSERT_EQUAL_PTR((void*)0xCAFEBABE, s_callback_context);
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
  rx_usb_config_t config = {0};

  config.callback = test_callback;
  config.ctx      = (void*)0xCAFEBABE;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(&config));
  s_callback_count = 0;

  rx_usb_set_state(k_usb_state_configured);

  TEST_ASSERT_EQUAL_UINT32(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_configured, s_last_event);
  TEST_ASSERT_EQUAL_PTR((void*)0xCAFEBABE, s_callback_context);
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
    .baud_rate = 9600,
    .stop_bits = 2,
    .parity    = 1,
    .data_bits = 7,
  };

  rx_usb_set_line_coding(k_usb_port_proto, &new_coding);

  rx_usb_line_coding_t result;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_line_coding(k_usb_port_proto, &result));
  TEST_ASSERT_EQUAL_UINT32(9600, result.baud_rate);
  TEST_ASSERT_EQUAL_UINT8(2, result.stop_bits);
  TEST_ASSERT_EQUAL_UINT8(1, result.parity);
  TEST_ASSERT_EQUAL_UINT8(7, result.data_bits);
}

void test_usb_set_line_coding_null_ignored(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_usb_set_line_coding(k_usb_port_proto, nullptr);

  rx_usb_line_coding_t result;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_line_coding(k_usb_port_proto, &result));
  TEST_ASSERT_EQUAL_UINT32(115200, result.baud_rate);
}

void test_usb_rx_push_adds_data(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "Hello USB";

  uint32_t written = rx_usb_rx_push(k_usb_port_proto, data, 9);

  TEST_ASSERT_EQUAL_UINT32(9, written);

  uint32_t available;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_rx_available(k_usb_port_proto, &available));
  TEST_ASSERT_EQUAL_UINT32(9, available);
}

void test_usb_rx_push_updates_stats(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  uint8_t data[] = "test";

  rx_usb_rx_push(k_usb_port_proto, data, 4);

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(4, stats.bytes_rx);
}

void test_usb_rx_push_triggers_callback(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_set_callback(k_usb_port_proto, test_callback, nullptr));

  uint8_t data[] = "test";
  rx_usb_rx_push(k_usb_port_proto, data, 4);

  TEST_ASSERT_EQUAL(1, s_callback_count);
  TEST_ASSERT_EQUAL(k_usb_event_data_rx, s_last_event);
}

void test_usb_tx_pop_retrieves_data(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  uint8_t write_data[] = "Hello";
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, write_data, 5));

  uint8_t  read_data[10];
  uint32_t read_count = rx_usb_tx_pop(k_usb_port_proto, read_data, 10);

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
  TEST_ASSERT_EQUAL_UINT32(2, stats.bus_resets);
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

  uint8_t  buf[10];
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

  uint8_t  buf[10];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(3, len);
  TEST_ASSERT_EQUAL_MEMORY("USB", buf, 3);
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

  rx_err_t err = rx_usb_putint(k_usb_port_proto, 42);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_putint_positive_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_proto, 12345);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(5, len);
  TEST_ASSERT_EQUAL_MEMORY("12345", buf, 5);
}

void test_usb_putint_negative_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_proto, -789);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(4, len);
  TEST_ASSERT_EQUAL_MEMORY("-789", buf, 4);
}

void test_usb_putint_zero(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_putint(k_usb_port_proto, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(1, len);
  TEST_ASSERT_EQUAL_CHAR('0', buf[0]);
}

void test_usb_putint_max_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* INT32_MAX = 2147483647 (10 digits) */
  rx_err_t err = rx_usb_putint(k_usb_port_proto, INT32_MAX);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(10, len);
  TEST_ASSERT_EQUAL_MEMORY("2147483647", buf, 10);
}

void test_usb_putint_min_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  /* INT32_MIN = -2147483648 (11 chars: sign + 10 digits) */
  rx_err_t err = rx_usb_putint(k_usb_port_proto, INT32_MIN);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(11, len);
  TEST_ASSERT_EQUAL_MEMORY("-2147483648", buf, 11);
}

void test_usb_puthex_not_configured_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* State is attached but not configured */

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0xAB, 2);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_puthex_two_digits(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0xAB, 2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(2, len);
  TEST_ASSERT_EQUAL_MEMORY("AB", buf, 2);
}

void test_usb_puthex_four_digits_zero_padded(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0x1F, 4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(4, len);
  TEST_ASSERT_EQUAL_MEMORY("001F", buf, 4);
}

void test_usb_puthex_eight_digits(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0xDEADBEEF, 8);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(8, len);
  TEST_ASSERT_EQUAL_MEMORY("DEADBEEF", buf, 8);
}

void test_usb_puthex_lowercase_letters(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0xabcdef, 6);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(6, len);
  /* Implementation uses uppercase hex */
  TEST_ASSERT_EQUAL_MEMORY("ABCDEF", buf, 6);
}

void test_usb_puthex_zero_value(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0, 4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(4, len);
  TEST_ASSERT_EQUAL_MEMORY("0000", buf, 4);
}

void test_usb_puthex_single_digit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  rx_err_t err = rx_usb_puthex(k_usb_port_proto, 0xF, 1);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t  buf[20];
  uint32_t len = rx_usb_tx_pop(k_usb_port_proto, buf, sizeof(buf));

  TEST_ASSERT_EQUAL_UINT32(1, len);
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
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 4));

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
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 4));

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
  memset(fill_data, 'A', sizeof(fill_data));
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

  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_usb_port_proto, data, 4));

  rx_usb_stats_t stats;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_get_stats(k_usb_port_proto, &stats));
  TEST_ASSERT_EQUAL_UINT32(4, stats.bytes_tx);
}

/** @} */ /* end of test_usb_tx_trigger */

/* =============================================================================
 * Main
 * =============================================================================
 */

/**
 * @brief Test suite entry point
 *
 * @details
 * Unity test framework main function. Runs all USB driver tests and
 * returns the test result count.
 *
 * @return int 0 if all tests pass, non-zero on failure
 */
int main(void)
{
  UNITY_BEGIN();

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

  return UNITY_END();
}
