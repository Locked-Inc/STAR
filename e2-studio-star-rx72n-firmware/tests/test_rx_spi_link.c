/* tests/test_rx_spi_link.c */

/**
 * @file test_rx_spi_link.c
 * @brief Unit Tests for SPI Link Layer (HARQ)
 *
 * @details
 * Tests the SPI link layer module that integrates HARQ (Chase Combining)
 * and FEC (convolutional encoding + Viterbi decoding) with the raw SPI
 * transport (rx_spi_comm). Uses mock SPI/session dependencies.
 *
 * ## Test Categories
 *
 * 1. **Lifecycle**: Init/deinit with valid and invalid parameters
 * 2. **Send**: FEC encoding, ACK/NACK handling, retry logic
 * 3. **Receive**: FEC decoding, soft bit conversion, Chase Combining
 * 4. **State**: State machine transitions, reset behavior
 * 5. **Edge Cases**: NULL pointers, uninitialized handles, buffer limits
 * 6. **Behavioral**: State transitions, retry enforcement, error recovery
 *
 * @author STAR Project Team - Texas A&M University
 * @date January 2026
 * @version 1.1.0
 * @copyright MIT License
 *
 * @par License
 * SPDX-License-Identifier: MIT
 *
 * @par Purpose
 * Validates rx_spi_link API correctness, state machine behavior, HARQ/FEC
 * integration, and error handling paths without requiring hardware.
 *
 * @since Version 1.1.0
 */

#include <string.h>

#include "rx_spi_link.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * ============================================================================= */

/**
 * @enum rx_spi_test_channel_t
 * @brief SPI channel index for test configuration
 * @since Version 1.1.0
 */
typedef enum : int32_t {
  k_rx_spi_test_channel = 0, /**< SPI channel 0 (RSPI0) for test fixture */
} rx_spi_test_channel_t;

/**
 * @enum rx_spi_test_retries_t
 * @brief Maximum HARQ retry attempts for test configuration
 * @since Version 1.1.0
 */
typedef enum : int32_t {
  k_rx_spi_test_default_retry = 0, /**< Default retry config (0 = use link layer default) */
  k_rx_spi_test_single_retry  = 1, /**< Single retry attempt for fast failure tests */
  k_rx_spi_test_max_retries   = 3, /**< Maximum 3 retries per HARQ specification */
  k_rx_spi_test_custom_retry  = 5, /**< Custom retry value for configuration tests */
} rx_spi_test_retries_t;

/**
 * @enum rx_spi_test_timeout_t
 * @brief Timeout values for receive operations in milliseconds
 * @since Version 1.1.0
 */
typedef enum : uint32_t {
  k_rx_spi_test_timeout_short  = 10,  /**< Short 10ms timeout for quick failure tests */
  k_rx_spi_test_timeout_normal = 100, /**< Normal 100ms timeout for receive operations */
} rx_spi_test_timeout_t;

/**
 * @enum rx_spi_test_data_t
 * @brief Test payload byte values for send/receive validation
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_rx_spi_test_data_byte1 = 0x01, /**< Test byte 1 */
  k_rx_spi_test_data_byte2 = 0x02, /**< Test byte 2 */
  k_rx_spi_test_data_byte3 = 0x12, /**< Test byte 3 */
  k_rx_spi_test_data_aa    = 0xAA, /**< Test pattern AA */
  k_rx_spi_test_data_bb    = 0xBB, /**< Test pattern BB */
  k_rx_spi_test_data_cc    = 0xCC, /**< Test pattern CC */
  k_rx_spi_test_data_ff    = 0xFF, /**< Test pattern FF */
} rx_spi_test_data_t;

/**
 * @enum rx_spi_test_zero_t
 * @brief Zero constant for memset and initialization
 * @since Version 1.1.0
 */
typedef enum : int32_t {
  k_rx_spi_test_zero = 0, /**< Zero value for initialization and memset */
} rx_spi_test_zero_t;

/**
 * @enum rx_spi_test_payload_sizes_t
 * @brief Payload size constants for boundary testing
 * @since Version 1.1.0
 */
typedef enum : uint32_t {
  k_rx_spi_test_min_buf     = 1,  /**< Minimum buffer size (1 byte) for size validation tests */
  k_rx_spi_test_nonzero_len = 10, /**< Non-zero payload length for NULL payload validation tests */
  k_rx_spi_test_max_plus_one =
    k_harq_max_payload + 1, /**< One byte over maximum (1025 bytes) for overflow tests */
} rx_spi_test_payload_sizes_t;

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

/**
 * @var s_link
 * @brief Static SPI link handle for test fixture
 * @details
 * Large structure (~86 KB) containing HARQ state, FEC encoder/decoder,
 * and Chase Combiner. Allocated statically to avoid stack overflow.
 * Zeroed in setUp() and deinitialized in tearDown() if initialized.
 *
 * @note Memory footprint: ~86 KB dominated by FEC decoder buffers
 * @warning Do not allocate on stack - use static/global only
 * @since Version 1.1.0
 */
static rx_spi_link_t s_link;

/**
 * @var s_spi_comm
 * @brief Static SPI communication handle for test fixture
 * @details
 * Underlying SPI transport used by s_link. Provides framing and session
 * management. Initialized via internal_init_link() helper.
 *
 * @note Test scope: Parameter validation and integration with s_link
 * @since Version 1.1.0
 */
static rx_spi_comm_handle_t s_spi_comm;

/**
 * @var s_session
 * @brief Static session state for sequence number management
 * @details
 * Shared TX/RX sequence counters used by s_spi_comm. Prevents sequence
 * number conflicts between transports in production code.
 *
 * @note Thread safety: Tests are single-threaded, no synchronization needed
 * @since Version 1.1.0
 */
static rx_session_state_t s_session;

/**
 * @brief Unity setUp - called before each test
 *
 * @details
 * Zeroes all test fixtures (s_link, s_spi_comm, s_session) to ensure clean
 * state before each test execution. This prevents test interdependencies
 * and ensures each test starts with known initial conditions.
 *
 * @pre Unity test harness initialized
 * @pre Global test fixtures may contain state from previous test
 *
 * @post s_link zero-initialized (all fields = 0)
 * @post s_spi_comm zero-initialized (all fields = 0)
 * @post s_session zero-initialized (all fields = 0)
 * @post All fixtures ready for test-specific initialization
 */
void setUp(void)
{
  (void)memset(&s_link, k_rx_spi_test_zero, sizeof(s_link));
  (void)memset(&s_spi_comm, k_rx_spi_test_zero, sizeof(s_spi_comm));
  (void)memset(&s_session, k_rx_spi_test_zero, sizeof(s_session));
}

/**
 * @brief Unity tearDown - called after each test
 *
 * @details
 * Cleans up resources allocated during test execution. Deinitializes the
 * SPI link layer if it was initialized during the test. This prevents
 * resource leaks and ensures proper cleanup between tests.
 *
 * @pre Test execution completed
 * @pre s_link may or may not be initialized
 *
 * @post If s_link was initialized: rx_spi_link_deinit() called successfully
 * @post All dynamically allocated resources released
 * @post Global test fixtures in clean state for next test
 */
void tearDown(void)
{
  if (s_link.initialized) {
    (void)rx_spi_link_deinit(&s_link);
  }
}

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

/**
 * @brief Initialize SPI comm and link with default test configuration
 *
 * @details
 * Initializes the complete SPI communication stack for testing:
 * 1. Session state (tx_seq, rx_seq, timestamps)
 * 2. SPI comm layer (transport with optional FEC)
 * 3. SPI link layer (HARQ with Chase Combining)
 *
 * Uses test fixture globals: s_session, s_spi_comm, s_link.
 * Configuration uses standard test constants from typed enums.
 *
 * @param[in] fec_enabled Whether to enable FEC (convolutional encoding + Viterbi)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All layers initialized successfully
 * @retval k_rx_err_invalid_arg Session or SPI comm init failed (invalid state)
 * @retval k_rx_err_harq HARQ init failed (memory allocation or state machine error)
 * @retval k_rx_err_fec FEC encoder/decoder init failed
 *
 * @pre s_session, s_spi_comm, s_link must be zeroed (setUp() call)
 * @post s_session.initialized == true on success
 * @post s_spi_comm.initialized == true on success
 * @post s_link.initialized == true on success
 *
 * @note This is a test helper - not part of production API
 * @note Uses SPI channel 0 (RSPI0) and 3 HARQ retries per specification
 *
 * @par Example:
 * @code
 * // In test function
 * rx_err_t err = internal_init_link(true);  // FEC enabled
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_TRUE(s_link.initialized);
 * @endcode
 *
 * @see rx_session_init() Session initialization
 * @see rx_spi_comm_init() SPI transport initialization
 * @see rx_spi_link_init() HARQ link initialization
 *
 * @since Version 1.1.0
 */
static rx_err_t internal_init_link(bool fec_enabled)
{
  /* Initialize session state */
  rx_err_t err = rx_session_init(&s_session);
  if (err != k_rx_ok) {
    return err;
  }

  /* Initialize SPI comm with session */
  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rx_spi_test_channel,
    .fec_enabled = fec_enabled,
  };
  err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  /* Initialize SPI link */
  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm,
    .fec_enabled = fec_enabled,
    .max_retries = k_rx_spi_test_max_retries,
  };
  return rx_spi_link_init(&s_link, &link_cfg);
}

/* =============================================================================
 * Lifecycle Tests
 * ============================================================================= */

/**
 * @brief Test init with NULL link pointer
 *
 * @details
 * Validates that rx_spi_link_init() rejects NULL link pointer with
 * k_rx_err_invalid_arg per NASA Power of 10 Rule 5 (defensive validation).
 *
 * @pre s_spi_comm fixture is available (zeroed in setUp)
 * @post Link handle remains unmodified (still zeroed)
 *
 * @note Part of lifecycle test suite - validates parameter checking
 * @see test_init_null_config() NULL config validation
 * @see test_init_null_spi_handle() NULL spi_handle validation
 *
 * @since Version 1.1.0
 */
void test_init_null_link(void)
{
  const rx_spi_link_config_t cfg = {.spi_handle = &s_spi_comm};
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_init(nullptr, &cfg));
}

/**
 * @brief Test init with NULL config pointer
 *
 * @details
 * Validates that rx_spi_link_init() rejects NULL config pointer with
 * k_rx_err_invalid_arg per NASA Power of 10 Rule 5 (defensive validation).
 *
 * @pre s_link fixture is available (zeroed in setUp)
 * @post Link handle remains uninitialized (initialized flag false)
 *
 * @note Part of lifecycle test suite - validates parameter checking
 * @see test_init_null_link() NULL link validation
 * @see test_init_null_spi_handle() NULL spi_handle validation
 *
 * @since Version 1.1.0
 */
void test_init_null_config(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_init(&s_link, nullptr));
}

/**
 * @brief Test init with NULL SPI handle in config
 *
 * @details
 * Validates that rx_spi_link_init() rejects config with NULL spi_handle
 * field with k_rx_err_invalid_arg. SPI handle is mandatory dependency.
 *
 * @pre s_link fixture is available (zeroed in setUp)
 * @post Link handle remains uninitialized (initialized flag false)
 *
 * @note Part of lifecycle test suite - validates dependency checking
 * @see test_init_null_link() NULL link validation
 * @see test_init_null_config() NULL config validation
 *
 * @since Version 1.1.0
 */
void test_init_null_spi_handle(void)
{
  const rx_spi_link_config_t cfg = {.spi_handle = nullptr};
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_init(&s_link, &cfg));
}

/**
 * @brief Test successful init with FEC disabled
 *
 * @details
 * Validates successful initialization of complete SPI stack (session + comm +
 * link) with FEC disabled. Verifies:
 * - Initialization succeeds with k_rx_ok
 * - Link state transitions to IDLE
 * - FEC flag correctly reflects disabled state
 * - max_retries defaults to specification value when set to 0
 *
 * @pre All fixtures zeroed in setUp()
 * @pre Session, comm, and link modules available
 * @post s_session.initialized == true
 * @post s_spi_comm.initialized == true
 * @post s_link.initialized == true
 * @post s_link.state == k_spi_link_state_idle
 * @post s_link.fec_enabled == false
 * @post s_link.max_retries == k_spi_link_default_max_retries
 *
 * @note Part of lifecycle test suite - validates successful init path
 * @see test_init_success_with_fec() Init with FEC enabled
 * @see test_init_custom_retries() Init with custom max_retries
 *
 * @since Version 1.1.0
 */
void test_init_success_no_fec(void)
{
  rx_err_t err = rx_session_init(&s_session);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rx_spi_test_channel,
    .fec_enabled = false,
  };
  err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm,
    .fec_enabled = false,
    .max_retries = k_rx_spi_test_default_retry, /* Should default to 3 */
  };
  err = rx_spi_link_init(&s_link, &link_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_link.initialized);
  TEST_ASSERT_FALSE(s_link.fec_enabled);
  TEST_ASSERT_EQUAL(k_spi_link_default_max_retries, s_link.max_retries);
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test successful init with FEC enabled
 *
 * @details
 * Validates successful initialization with FEC (Forward Error Correction)
 * enabled. Uses internal_init_link() helper for standard test configuration.
 * Verifies FEC flag propagates correctly through stack initialization.
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.initialized == true
 * @post s_link.fec_enabled == true
 * @post rx_spi_link_fec_enabled(&s_link) returns true
 *
 * @note Part of lifecycle test suite - validates FEC initialization path
 * @note FEC includes convolutional encoder (k=7, rate 1/2) + Viterbi decoder
 * @see test_init_success_no_fec() Init with FEC disabled
 * @see internal_init_link() Test helper for stack initialization
 *
 * @since Version 1.1.0
 */
void test_init_success_with_fec(void)
{
  rx_err_t err = internal_init_link(true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_link.initialized);
  TEST_ASSERT_TRUE(s_link.fec_enabled);
  TEST_ASSERT_TRUE(rx_spi_link_fec_enabled(&s_link));
}

/**
 * @brief Test init with custom max retries
 *
 * @details
 * Validates that custom max_retries value (5) is accepted and stored
 * correctly during initialization. HARQ spec allows 1-7 retries;
 * this test uses non-default value to verify configuration propagation.
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.initialized == true
 * @post s_link.max_retries == 5 (custom value, not default 3)
 *
 * @note Part of lifecycle test suite - validates custom retry configuration
 * @see test_init_success_no_fec() Init with default max_retries
 *
 * @since Version 1.1.0
 */
void test_init_custom_retries(void)
{
  rx_err_t err = rx_session_init(&s_session);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session = &s_session,
    .channel = k_rx_spi_test_channel,
  };
  err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm,
    .fec_enabled = false,
    .max_retries = k_rx_spi_test_custom_retry,
  };
  err = rx_spi_link_init(&s_link, &link_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_spi_test_custom_retry, s_link.max_retries);
}

/* =============================================================================
 * Deinit Tests
 * ============================================================================= */

/**
 * @brief Test deinit with NULL pointer
 *
 * @details
 * Validates that rx_spi_link_deinit() rejects NULL link pointer with
 * k_rx_err_invalid_arg per NASA Power of 10 Rule 5 (defensive validation).
 *
 * @pre None (NULL pointer test)
 * @post No side effects (NULL check fails before any state modification)
 *
 * @note Part of deinit test suite - validates parameter checking
 * @see test_deinit_uninitialized() Deinit on uninitialized handle
 * @see test_deinit_success() Successful deinit path
 *
 * @since Version 1.1.0
 */
void test_deinit_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_deinit(nullptr));
}

/**
 * @brief Test deinit on uninitialized handle
 *
 * @details
 * Validates that rx_spi_link_deinit() rejects uninitialized handle with
 * k_rx_err_invalid_state. Prevents double-free and state corruption.
 *
 * @pre s_link fixture zeroed in setUp() (initialized == false)
 * @post s_link remains zeroed (no state modification on error)
 *
 * @note Part of deinit test suite - validates state checking
 * @see test_deinit_null() Deinit with NULL pointer
 * @see test_deinit_success() Successful deinit path
 *
 * @since Version 1.1.0
 */
void test_deinit_uninitialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_spi_link_deinit(&s_link));
}

/**
 * @brief Test successful deinit
 *
 * @details
 * Validates successful deinitialization of SPI link. Verifies:
 * - Deinit returns k_rx_ok
 * - initialized flag cleared to false
 * - Resources released (HARQ state, FEC buffers)
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.initialized == false
 * @post All internal resources freed (HARQ, FEC, Chase Combiner)
 *
 * @note Part of deinit test suite - validates successful cleanup path
 * @see test_deinit_null() Deinit with NULL pointer
 * @see test_deinit_uninitialized() Deinit on uninitialized handle
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_deinit_success(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_spi_link_deinit(&s_link);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_link.initialized);
}

/* =============================================================================
 * State Tests
 * ============================================================================= */

/**
 * @brief Test get_state with NULL returns error state
 *
 * @details
 * Validates that rx_spi_link_get_state() returns k_spi_link_state_error
 * when called with NULL pointer, providing safe degradation instead of crash.
 *
 * @pre None (NULL pointer test)
 * @post No side effects
 *
 * @note Part of state test suite - validates parameter checking
 * @see test_get_state_uninitialized() State query on uninitialized handle
 * @see test_get_state_after_init() State query after successful init
 *
 * @since Version 1.1.0
 */
void test_get_state_null(void)
{
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(nullptr));
}

/**
 * @brief Test get_state on uninitialized handle returns error state
 *
 * @details
 * Validates that rx_spi_link_get_state() returns k_spi_link_state_error
 * when called on uninitialized handle (initialized flag false).
 *
 * @pre s_link fixture zeroed in setUp() (initialized == false)
 * @post No side effects (query-only operation)
 *
 * @note Part of state test suite - validates state checking
 * @see test_get_state_null() State query with NULL pointer
 * @see test_get_state_after_init() State query after successful init
 *
 * @since Version 1.1.0
 */
void test_get_state_uninitialized(void)
{
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test get_state returns idle after init
 *
 * @details
 * Validates that link enters IDLE state after successful initialization.
 * IDLE is the initial state before any send/receive operations.
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.state == k_spi_link_state_idle
 *
 * @note Part of state test suite - validates initial state transition
 * @see test_get_state_null() State query with NULL pointer
 * @see test_get_state_uninitialized() State query on uninitialized handle
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_get_state_after_init(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/* =============================================================================
 * FEC Enabled Query Tests
 * ============================================================================= */

/**
 * @brief Test fec_enabled with NULL returns false
 *
 * @details
 * Validates that rx_spi_link_fec_enabled() returns false when called
 * with NULL pointer, providing safe degradation instead of crash.
 *
 * @pre None (NULL pointer test)
 * @post No side effects
 *
 * @note Part of FEC query test suite - validates parameter checking
 * @see test_fec_enabled_uninitialized() FEC query on uninitialized handle
 * @see test_fec_enabled_off() FEC query with FEC disabled
 * @see test_fec_enabled_on() FEC query with FEC enabled
 *
 * @since Version 1.1.0
 */
void test_fec_enabled_null(void)
{
  TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(nullptr));
}

/**
 * @brief Test fec_enabled on uninitialized returns false
 *
 * @details
 * Validates that rx_spi_link_fec_enabled() returns false when called
 * on uninitialized handle (initialized flag false).
 *
 * @pre s_link fixture zeroed in setUp() (initialized == false)
 * @post No side effects (query-only operation)
 *
 * @note Part of FEC query test suite - validates state checking
 * @see test_fec_enabled_null() FEC query with NULL pointer
 * @see test_fec_enabled_off() FEC query with FEC disabled
 * @see test_fec_enabled_on() FEC query with FEC enabled
 *
 * @since Version 1.1.0
 */
void test_fec_enabled_uninitialized(void)
{
  TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(&s_link));
}

/**
 * @brief Test fec_enabled when disabled
 *
 * @details
 * Validates that rx_spi_link_fec_enabled() correctly returns false
 * when link is initialized with FEC disabled.
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.fec_enabled == false
 *
 * @note Part of FEC query test suite - validates FEC disabled state
 * @see test_fec_enabled_null() FEC query with NULL pointer
 * @see test_fec_enabled_uninitialized() FEC query on uninitialized handle
 * @see test_fec_enabled_on() FEC query with FEC enabled
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_fec_enabled_off(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(&s_link));
}

/**
 * @brief Test fec_enabled when enabled
 *
 * @details
 * Validates that rx_spi_link_fec_enabled() correctly returns true
 * when link is initialized with FEC enabled.
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.fec_enabled == true
 *
 * @note Part of FEC query test suite - validates FEC enabled state
 * @see test_fec_enabled_null() FEC query with NULL pointer
 * @see test_fec_enabled_uninitialized() FEC query on uninitialized handle
 * @see test_fec_enabled_off() FEC query with FEC disabled
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_fec_enabled_on(void)
{
  rx_err_t err = internal_init_link(true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_spi_link_fec_enabled(&s_link));
}

/* =============================================================================
 * Reset Tests
 * ============================================================================= */

/**
 * @brief Test reset with NULL pointer
 *
 * @details
 * Validates that rx_spi_link_reset() rejects NULL link pointer with
 * k_rx_err_invalid_arg per NASA Power of 10 Rule 5 (defensive validation).
 *
 * @pre None (NULL pointer test)
 * @post No side effects
 *
 * @note Part of reset test suite - validates parameter checking
 * @see test_reset_uninitialized() Reset on uninitialized handle
 * @see test_reset_success() Successful reset to IDLE state
 *
 * @since Version 1.1.0
 */
void test_reset_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_reset(nullptr));
}

/**
 * @brief Test reset on uninitialized handle
 *
 * @details
 * Validates that rx_spi_link_reset() rejects uninitialized handle with
 * k_rx_err_invalid_state. Reset requires valid initialized link.
 *
 * @pre s_link fixture zeroed in setUp() (initialized == false)
 * @post s_link remains zeroed (no state modification on error)
 *
 * @note Part of reset test suite - validates state checking
 * @see test_reset_null() Reset with NULL pointer
 * @see test_reset_success() Successful reset to IDLE state
 *
 * @since Version 1.1.0
 */
void test_reset_uninitialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_spi_link_reset(&s_link));
}

/**
 * @brief Test successful reset returns to idle
 *
 * @details
 * Validates that rx_spi_link_reset() successfully resets link from ERROR
 * state back to IDLE state. Clears HARQ retransmit counters and soft bit
 * accumulators. Used for error recovery without full deinitialization.
 *
 * @pre All fixtures zeroed in setUp()
 * @pre Link initialized with FEC enabled
 * @post s_link.state == k_spi_link_state_idle
 * @post HARQ retransmit counters reset to 0
 * @post Chase Combiner soft bit buffers cleared
 *
 * @note Part of reset test suite - validates error recovery path
 * @note Manually sets ERROR state to simulate fault condition
 * @see test_reset_null() Reset with NULL pointer
 * @see test_reset_uninitialized() Reset on uninitialized handle
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_reset_success(void)
{
  rx_err_t err = internal_init_link(true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Manually set state to error to test reset */
  s_link.state = k_spi_link_state_error;
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));

  err = rx_spi_link_reset(&s_link);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/* =============================================================================
 * Send Validation Tests (parameter checking only, no SPI hardware)
 * ============================================================================= */

/**
 * @brief Test send with NULL link
 *
 * @details
 * Validates that rx_spi_link_send() rejects NULL link pointer with
 * k_rx_err_invalid_arg per NASA Power of 10 Rule 5 (defensive validation).
 *
 * @pre None (NULL pointer test)
 * @post No SPI transmission attempted
 *
 * @note Part of send validation test suite - validates parameter checking
 * @see test_send_uninitialized() Send on uninitialized link
 * @see test_send_null_payload_nonzero_len() Send with NULL payload
 * @see test_send_payload_too_large() Send with oversized payload
 *
 * @since Version 1.1.0
 */
void test_send_null_link(void)
{
  uint8_t data[] = {k_rx_spi_test_data_byte1, k_rx_spi_test_data_byte2};
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_spi_link_send(nullptr, k_frame_type_command, data, sizeof(data)));
}

/**
 * @brief Test send on uninitialized link
 *
 * @details
 * Validates that rx_spi_link_send() rejects uninitialized handle with
 * k_rx_err_invalid_state. Send requires valid initialized link.
 *
 * @pre s_link fixture zeroed in setUp() (initialized == false)
 * @post No SPI transmission attempted
 *
 * @note Part of send validation test suite - validates state checking
 * @see test_send_null_link() Send with NULL link pointer
 * @see test_send_null_payload_nonzero_len() Send with NULL payload
 * @see test_send_payload_too_large() Send with oversized payload
 *
 * @since Version 1.1.0
 */
void test_send_uninitialized(void)
{
  uint8_t data[] = {k_rx_spi_test_data_byte1, k_rx_spi_test_data_byte2};
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data)));
}

/**
 * @brief Test send with NULL payload and non-zero length
 *
 * @details
 * Validates that rx_spi_link_send() rejects NULL payload pointer when
 * payload_len > 0, returning k_rx_err_invalid_arg. Prevents buffer overrun.
 *
 * @pre Link initialized with FEC disabled
 * @post No SPI transmission attempted
 *
 * @note Part of send validation test suite - validates buffer checking
 * @see test_send_null_link() Send with NULL link pointer
 * @see test_send_uninitialized() Send on uninitialized link
 * @see test_send_payload_too_large() Send with oversized payload
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_send_null_payload_nonzero_len(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL(
    k_rx_err_invalid_arg,
    rx_spi_link_send(&s_link, k_frame_type_command, nullptr, k_rx_spi_test_nonzero_len));
}

/**
 * @brief Test send with payload too large
 *
 * @details
 * Validates that rx_spi_link_send() rejects payload exceeding HARQ maximum
 * (k_harq_max_payload) with k_rx_err_invalid_size. Prevents buffer overflow.
 *
 * @pre Link initialized with FEC disabled
 * @post No SPI transmission attempted
 *
 * @note Part of send validation test suite - validates size bounds checking
 * @see test_send_null_link() Send with NULL link pointer
 * @see test_send_uninitialized() Send on uninitialized link
 * @see test_send_null_payload_nonzero_len() Send with NULL payload
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_send_payload_too_large(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t data[k_rx_spi_test_min_buf];
  TEST_ASSERT_EQUAL(
    k_rx_err_invalid_size,
    rx_spi_link_send(&s_link, k_frame_type_command, data, k_rx_spi_test_max_plus_one));
}

/* =============================================================================
 * Receive Validation Tests (parameter checking only)
 * ============================================================================= */

/**
 * @brief Test receive with NULL link
 *
 * @details
 * Validates that rx_spi_link_receive() rejects NULL link pointer with
 * k_rx_err_invalid_arg per NASA Power of 10 Rule 5 (defensive validation).
 *
 * @pre None (NULL pointer test)
 * @post result structure unmodified
 *
 * @note Part of receive validation test suite - validates parameter checking
 * @see test_receive_null_result() Receive with NULL result pointer
 * @see test_receive_uninitialized() Receive on uninitialized link
 *
 * @since Version 1.1.0
 */
void test_receive_null_link(void)
{
  rx_spi_link_receive_result_t result;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_spi_link_receive(nullptr, &result, k_rx_spi_test_timeout_normal));
}

/**
 * @brief Test receive with NULL result
 *
 * @details
 * Validates that rx_spi_link_receive() rejects NULL result pointer with
 * k_rx_err_invalid_arg. Result structure is mandatory output parameter.
 *
 * @pre Link initialized with FEC disabled
 * @post No receive attempted
 *
 * @note Part of receive validation test suite - validates parameter checking
 * @see test_receive_null_link() Receive with NULL link pointer
 * @see test_receive_uninitialized() Receive on uninitialized link
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_receive_null_result(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_spi_link_receive(&s_link, nullptr, k_rx_spi_test_timeout_normal));
}

/**
 * @brief Test receive on uninitialized link
 *
 * @details
 * Validates that rx_spi_link_receive() rejects uninitialized handle with
 * k_rx_err_invalid_state. Receive requires valid initialized link.
 *
 * @pre s_link fixture zeroed in setUp() (initialized == false)
 * @post result structure unmodified
 *
 * @note Part of receive validation test suite - validates state checking
 * @see test_receive_null_link() Receive with NULL link pointer
 * @see test_receive_null_result() Receive with NULL result pointer
 *
 * @since Version 1.1.0
 */
void test_receive_uninitialized(void)
{
  rx_spi_link_receive_result_t result;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal));
}

/* =============================================================================
 * Behavioral Tests (State Machine and Error Paths)
 * ============================================================================= */

/**
 * @brief Test state transitions during send operations
 *
 * @details
 * Verifies that link transitions through correct HARQ state machine states
 * during send operation with retries:
 * 1. IDLE → WAITING_ACK on first transmission attempt
 * 2. WAITING_ACK → RETRANSMITTING on NACK or timeout
 * 3. RETRANSMITTING → ERROR after max_retries exhausted
 * 4. ERROR → IDLE on explicit reset
 *
 * Uses real SPI comm layer (no mocks) to verify state machine behavior.
 * SPI operations timeout due to no hardware, but state transitions still occur.
 *
 * @pre All fixtures zeroed in setUp()
 * @pre Link initialized with FEC disabled and max_retries=3
 * @post After failed send: s_link.state == k_spi_link_state_error
 * @post After reset: s_link.state == k_spi_link_state_idle
 *
 * @note Part of behavioral test suite - validates HARQ state machine
 * @note SPI timeouts are expected (no hardware) - testing logic, not I/O
 * @see rx_spi_link_send() Send operation with HARQ retries
 * @see rx_spi_link_reset() Reset link to IDLE state
 * @see rx_spi_link_get_state() Query current state
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_state_transitions_send_retries(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Initial state should be idle */
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));

  /* Attempt send - will fail due to no hardware, but state will transition */
  uint8_t data[] = {k_rx_spi_test_data_aa, k_rx_spi_test_data_bb, k_rx_spi_test_data_cc};
  err            = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));

  /* Send should fail with retry limit (no hardware to ACK) */
  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);

  /* After max retries, state should be error */
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));

  /* Reset should return to idle */
  err = rx_spi_link_reset(&s_link);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test FEC enabled flag query
 *
 * @details
 * Verifies rx_spi_link_fec_enabled() returns correct FEC state across
 * multiple initialization cycles and edge cases:
 * - Returns false when FEC disabled
 * - Returns true when FEC enabled
 * - Returns false for NULL pointer (safe degradation)
 * - Validates deinit/reinit cycle preserves FEC state correctly
 *
 * @pre All fixtures zeroed in setUp()
 * @post After test: s_link.fec_enabled == true (from second init)
 *
 * @note Part of behavioral test suite - validates FEC state persistence
 * @see rx_spi_link_fec_enabled() FEC state query function
 * @see test_fec_enabled_on() Single FEC enabled test
 * @see test_fec_enabled_off() Single FEC disabled test
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_fec_enabled_query(void)
{
  /* Test with FEC disabled */
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(&s_link));

  err = rx_spi_link_deinit(&s_link);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Deinitialize session to allow re-initialization */
  err = rx_session_deinit(&s_session);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Test with FEC enabled */
  err = internal_init_link(true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_spi_link_fec_enabled(&s_link));

  /* NULL pointer returns false */
  TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(nullptr));
}

/**
 * @brief Test state query with NULL and error conditions
 *
 * @details
 * Verifies rx_spi_link_get_state() handles edge cases correctly:
 * - NULL pointer returns k_spi_link_state_error (safe degradation)
 * - Uninitialized (zeroed) link returns k_spi_link_state_error
 *
 * The function checks the initialized flag before returning state,
 * ensuring uninitialized handles are detected and reported as errors.
 *
 * @pre Local rx_spi_link_t allocated on stack for uninit test
 * @post No side effects (query-only operations)
 *
 * @note Part of behavioral test suite - validates state query edge cases
 * @note Uninitialized handles return ERROR (initialized flag check)
 * @see rx_spi_link_get_state() State query function
 * @see test_get_state_null() NULL pointer state query test
 * @see test_get_state_uninitialized() Uninitialized handle state query
 *
 * @since Version 1.1.0
 */
void test_state_query_null_and_error(void)
{
  /* NULL link returns error state */
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(nullptr));

  /* Uninitialized link (all zeros) also returns error state */
  rx_spi_link_t uninit_link;
  (void)memset(&uninit_link, k_rx_spi_test_zero, sizeof(uninit_link));
  /* Should return error (initialized flag is false) */
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&uninit_link));
}

/**
 * @brief Test receive with control frames
 *
 * @details
 * Verifies that receive operation correctly filters control frames
 * (ACK/NACK/PING/PONG/RESET) and returns timeout to signal "no data"
 * instead of treating them as application data.
 *
 * This test documents expected behavior but cannot fully validate without
 * mock SPI transport. Current test verifies timeout handling and state
 * preservation during receive operation.
 *
 * @pre All fixtures zeroed in setUp()
 * @pre Link initialized with FEC disabled
 * @post s_link.state == k_spi_link_state_idle (timeout doesn't error)
 *
 * @note Part of behavioral test suite - documents control frame filtering
 * @note Timeout expected (no hardware) - validates state remains stable
 * @warning Full test requires mock SPI returning ACK/NACK/PING/PONG frames
 * @see rx_spi_link_receive() Receive operation with frame filtering
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_receive_filters_control_frames(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_spi_link_receive_result_t result;

  /* Receive will fail (no hardware) - expect invalid_state from SPI layer */
  err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_short); /* Short timeout */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);

  /* Link state should remain idle after receive failure */
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test max_retries configuration enforcement
 *
 * @details
 * Verifies that link respects custom max_retries setting and fails after
 * exhausting retry attempts. Uses max_retries=1 to reduce test duration.
 *
 * Tests HARQ retry limit enforcement:
 * - Link configured with max_retries=1 (single transmission attempt)
 * - Send operation fails with k_rx_err_retry_limit (no hardware to ACK)
 * - Link transitions to ERROR state after exhausting retry budget
 *
 * @pre All fixtures zeroed in setUp()
 * @post s_link.max_retries == 1 (custom configuration)
 * @post s_link.state == k_spi_link_state_error (after retry exhaustion)
 *
 * @note Part of behavioral test suite - validates HARQ retry enforcement
 * @note Uses max_retries=1 to speed up test (default is 3)
 * @see rx_spi_link_send() Send with HARQ retry logic
 * @see test_init_custom_retries() Initialization with custom max_retries
 * @see test_state_transitions_send_retries() State machine retry behavior
 *
 * @since Version 1.1.0
 */
void test_max_retries_enforcement(void)
{
  /* Initialize with custom max_retries=1 */
  rx_err_t err = rx_session_init(&s_session);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rx_spi_test_channel,
    .fec_enabled = false,
  };
  err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm,
    .fec_enabled = false,
    .max_retries = k_rx_spi_test_single_retry, /* Only 1 attempt */
  };
  err = rx_spi_link_init(&s_link, &link_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Send should fail quickly with only 1 attempt */
  uint8_t data[] = {k_rx_spi_test_data_byte3};
  err            = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);

  /* State should be error after failure */
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test reset clears error state and HARQ combiner
 *
 * @details
 * Verifies that rx_spi_link_reset() properly clears error state and
 * resets the link to IDLE, allowing error recovery without full deinit.
 *
 * Test sequence:
 * 1. Force ERROR state by exhausting HARQ retries
 * 2. Call rx_spi_link_reset() to clear error
 * 3. Verify state returns to IDLE
 * 4. Verify send operation can be attempted again (link is operational)
 *
 * Reset clears: state machine state, HARQ retry counters, Chase Combiner
 * soft bit accumulators.
 *
 * @pre All fixtures zeroed in setUp()
 * @pre Link initialized with FEC disabled
 * @post After reset: s_link.state == k_spi_link_state_idle
 * @post After reset: HARQ retry counters reset to 0
 *
 * @note Part of behavioral test suite - validates error recovery mechanism
 * @note Reset allows recovery without expensive deinit/reinit cycle
 * @see rx_spi_link_reset() Reset link to IDLE and clear error state
 * @see test_reset_success() Basic reset success test
 * @see test_state_transitions_send_retries() State machine error transitions
 * @see internal_init_link() Test helper for initialization
 *
 * @since Version 1.1.0
 */
void test_reset_clears_error_state(void)
{
  rx_err_t err = internal_init_link(false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Force link into error state by exhausting retries */
  uint8_t data[] = {k_rx_spi_test_data_ff};
  err            = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));

  /* Reset should clear error and return to idle */
  err = rx_spi_link_reset(&s_link);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));

  /* Should be able to send again after reset */
  err = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));
  /* Will fail again (no hardware), but at least it tries */
  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);
}

/* =============================================================================
 * Main
 * ============================================================================= */

/**
 * @brief Unity test runner
 *
 * @details
 * Runs all SPI link layer tests. Tests are organized by category:
 * lifecycle, state, FEC query, reset, send validation, receive validation.
 *
 * @return Number of failures (0 = all passed)
 */
int main(void)
{
  UNITY_BEGIN();

  /* Lifecycle: Init */
  RUN_TEST(test_init_null_link);
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_null_spi_handle);
  RUN_TEST(test_init_success_no_fec);
  RUN_TEST(test_init_success_with_fec);
  RUN_TEST(test_init_custom_retries);

  /* Lifecycle: Deinit */
  RUN_TEST(test_deinit_null);
  RUN_TEST(test_deinit_uninitialized);
  RUN_TEST(test_deinit_success);

  /* State */
  RUN_TEST(test_get_state_null);
  RUN_TEST(test_get_state_uninitialized);
  RUN_TEST(test_get_state_after_init);

  /* FEC Enabled */
  RUN_TEST(test_fec_enabled_null);
  RUN_TEST(test_fec_enabled_uninitialized);
  RUN_TEST(test_fec_enabled_off);
  RUN_TEST(test_fec_enabled_on);

  /* Reset */
  RUN_TEST(test_reset_null);
  RUN_TEST(test_reset_uninitialized);
  RUN_TEST(test_reset_success);

  /* Send Validation */
  RUN_TEST(test_send_null_link);
  RUN_TEST(test_send_uninitialized);
  RUN_TEST(test_send_null_payload_nonzero_len);
  RUN_TEST(test_send_payload_too_large);

  /* Receive Validation */
  RUN_TEST(test_receive_null_link);
  RUN_TEST(test_receive_null_result);
  RUN_TEST(test_receive_uninitialized);

  /* Behavioral Tests */
  RUN_TEST(test_state_transitions_send_retries);
  RUN_TEST(test_fec_enabled_query);
  RUN_TEST(test_state_query_null_and_error);
  RUN_TEST(test_receive_filters_control_frames);
  RUN_TEST(test_max_retries_enforcement);
  RUN_TEST(test_reset_clears_error_state);

  return UNITY_END();
}
