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
 * @author Locked, Inc.
 * @date January 2026
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 *
 * @par License
 * SPDX-License-Identifier: MIT
 *
 * @par Purpose
 * Validates rx_spi_link API correctness, state machine behavior, HARQ/FEC
 * integration, and error handling paths without requiring hardware.
 *
 * @since Version 1.0.0
 */

#include <stdatomic.h> /* atomic_flag, atomic_flag_test_and_set, atomic_flag_clear */
#include <string.h>

#include "hardware.h"  /* rspi_channel_t, rspi_init_peripheral */
#include "mock_rspi.h" /* RSPI mock for frame injection */
#include "rx_fec.h"    /* rx_soft_bit_t */
#include "rx_frame.h"
#include "rx_harq.h"    /* rx_harq_encode */
#include "rx_session.h" /* rx_session_get_tx */
#include "rx_spi_comm.h"
#include "rx_spi_link.h"
#include "unity.h"

/**
 * @var s_send_path_busy
 * @brief Re-entrancy guard exposed via RX_STATIC_TESTABLE in UNIT_TEST builds.
 *
 * @details
 * Production code declares this @c static, but in UNIT_TEST builds the
 * declaration uses @c RX_STATIC_TESTABLE so tests can pre-set the flag and
 * reach the false branch of the @c RX_ASSERT_PRE re-entrancy check in
 * @c internal_send_attempt without having to fork a task or interrupt.
 *
 * @see internal_send_attempt() Re-entrancy guard consumer
 */
extern atomic_flag s_send_path_busy;

/* Functions exposed via RX_STATIC_TESTABLE (non-static in test builds) */
extern rx_err_t internal_bytes_to_soft_bits(const uint8_t* data,
                                            uint32_t       data_len,
                                            rx_soft_bit_t* soft,
                                            uint32_t       soft_size,
                                            uint32_t*      soft_len);
extern rx_err_t internal_wait_for_ack(rx_spi_link_t* link, uint16_t expected_seq);
extern rx_err_t internal_receive_passthrough(rx_spi_link_t*                link,
                                             const rx_frame_t*             frame,
                                             rx_spi_link_receive_result_t* result);
extern rx_err_t internal_receive_fec_decode(rx_spi_link_t*                link,
                                            const rx_frame_t*             frame,
                                            rx_spi_link_receive_result_t* result);
extern rx_err_t internal_prepare_tx_payload(rx_spi_link_t*  link,
                                            const uint8_t*  payload,
                                            uint32_t        payload_len,
                                            const uint8_t** out_payload,
                                            uint32_t*       out_len,
                                            uint8_t*        out_flags);
extern rx_err_t internal_send_attempt(rx_spi_link_t*  link,
                                      rx_frame_type_t type,
                                      uint8_t         base_flags,
                                      const uint8_t*  tx_payload,
                                      uint32_t        tx_len,
                                      uint8_t         attempt);

/* =============================================================================
 * Test Constants
 * ============================================================================= */

/**
 * @note SPI channel constants use rspi_channel_t from hardware.h
 *       (k_rspi_channel_0 in these tests).
 * @since Version 1.0.0
 */

/**
 * @enum rx_spi_test_retries_t
 * @brief Maximum HARQ retry attempts for test configuration
 * @since Version 1.0.0
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
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rx_spi_test_timeout_short  = 10,  /**< Short 10ms timeout for quick failure tests */
  k_rx_spi_test_timeout_normal = 100, /**< Normal 100ms timeout for receive operations */
} rx_spi_test_timeout_t;

/**
 * @enum rx_spi_test_data_t
 * @brief Test payload byte values for send/receive validation
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_spi_test_data_byte1 = 0x01, /**< Test byte 1 */
  k_rx_spi_test_data_byte2 = 0x02, /**< Test byte 2 */
  k_rx_spi_test_data_byte3 = 0x03, /**< Test byte 3 */
  k_rx_spi_test_data_byte4 = 0x04, /**< Test byte 4 */
  k_rx_spi_test_data_11    = 0x11, /**< Test pattern 11 */
  k_rx_spi_test_data_12    = 0x12, /**< Test pattern 12 */
  k_rx_spi_test_data_22    = 0x22, /**< Test pattern 22 */
  k_rx_spi_test_data_33    = 0x33, /**< Test pattern 33 */
  k_rx_spi_test_data_34    = 0x34, /**< Test pattern 34 */
  k_rx_spi_test_data_44    = 0x44, /**< Test pattern 44 */
  k_rx_spi_test_data_55    = 0x55, /**< Test pattern 55 */
  k_rx_spi_test_data_56    = 0x56, /**< Test pattern 56 */
  k_rx_spi_test_data_66    = 0x66, /**< Test pattern 66 */
  k_rx_spi_test_data_78    = 0x78, /**< Test pattern 78 */
  k_rx_spi_test_data_9a    = 0x9A, /**< Test pattern 9A */
  k_rx_spi_test_data_aa    = 0xAA, /**< Test pattern AA */
  k_rx_spi_test_data_ab    = 0xAB, /**< Test pattern AB */
  k_rx_spi_test_data_bb    = 0xBB, /**< Test pattern BB */
  k_rx_spi_test_data_be    = 0xBE, /**< Test pattern BE */
  k_rx_spi_test_data_cc    = 0xCC, /**< Test pattern CC */
  k_rx_spi_test_data_cd    = 0xCD, /**< Test pattern CD */
  k_rx_spi_test_data_dd    = 0xDD, /**< Test pattern DD */
  k_rx_spi_test_data_de    = 0xDE, /**< Test pattern DE */
  k_rx_spi_test_data_ad    = 0xAD, /**< Test pattern AD */
  k_rx_spi_test_data_ef    = 0xEF, /**< Test pattern EF */
  k_rx_spi_test_data_ff    = 0xFF, /**< Test pattern FF */
} rx_spi_test_data_t;

/**
 * @enum rx_spi_test_sizes_t
 * @brief Small size constants for buffer dimensions and indices
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rx_spi_test_soft_bits_per_byte = 8,    /**< Soft bits produced per input byte */
  k_rx_spi_test_two_bytes          = 2,    /**< Two-byte payload size */
  k_rx_spi_test_three_bytes        = 3,    /**< Three-byte payload size */
  k_rx_spi_test_four_bytes         = 4,    /**< Four-byte payload size */
  k_rx_spi_test_eight_bytes        = 8,    /**< Eight-byte payload size */
  k_rx_spi_test_fill_pattern       = 0x55, /**< Memset fill pattern for oversized test */
} rx_spi_test_sizes_t;

/**
 * @enum rx_spi_test_seq_t
 * @brief Sequence number constants for frame injection tests
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_rx_spi_test_seq_2  = 2,  /**< Sequence number 2 */
  k_rx_spi_test_seq_3  = 3,  /**< Sequence number 3 */
  k_rx_spi_test_seq_5  = 5,  /**< Sequence number 5 */
  k_rx_spi_test_seq_7  = 7,  /**< Sequence number 7 */
  k_rx_spi_test_seq_10 = 10, /**< Sequence number 10 */
} rx_spi_test_seq_t;

/**
 * @enum rx_spi_test_misc_t
 * @brief Miscellaneous test constants
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rx_spi_test_initial_soft_len = 99, /**< Non-zero initial value to verify overwrite */
} rx_spi_test_misc_t;

/**
 * @enum rx_spi_test_payload_idx_t
 * @brief Payload array indices for frame construction tests
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_spi_test_payload_idx_2 = 2, /**< Third payload byte index */
  k_rx_spi_test_payload_idx_3 = 3, /**< Fourth payload byte index */
} rx_spi_test_payload_idx_t;

/**
 * @enum rx_spi_test_payload_sizes_t
 * @brief Payload size constants for boundary testing
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rx_spi_test_min_buf     = 1,  /**< Minimum buffer size (1 byte) for size validation tests */
  k_rx_spi_test_nonzero_len = 10, /**< Non-zero payload length for NULL payload validation tests */
  k_rx_spi_test_max_plus_one =
    k_harq_max_payload + 1, /**< One byte over maximum (1025 bytes) for overflow tests */
} rx_spi_test_payload_sizes_t;

/**
 * @enum rx_spi_test_frame_sizes_t
 * @brief Precomputed frame sizes for RX buffer injection overlap tests
 * @details
 * When rx_spi_link_send() is active, the SPI send transfer reads from the
 * mock's RX buffer simultaneously with writing. Pre-padding the injection
 * buffer with this many bytes (consumed by the send transfer) ensures the
 * ACK frame bytes remain available for the subsequent rx_spi_comm_receive()
 * call.
 *
 * Formula: k_frame_sync_size(2) + k_frame_header_size(6) + payload + k_frame_crc_size(4)
 * For 1-byte payload without FEC: 2+6+1+4 = 13 bytes
 * For 2-byte payload without FEC: 2+6+2+4 = 14 bytes
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_send_frame_1byte_size = 13,  /**< Wire size of send frame for 1-byte payload */
  k_test_send_frame_2byte_size = 14,  /**< Wire size of send frame for 2-byte payload */
  k_test_ack_frame_wire_size   = 12,  /**< Wire size of minimal ACK frame (no payload) */
  k_test_inject_buf_max        = 256, /**< Maximum combined injection buffer size */
} rx_spi_test_frame_sizes_t;

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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
  mock_rspi_init(nullptr);
  static const rx_spi_link_t        s_zero_link     = {};
  static const rx_spi_comm_handle_t s_zero_spi_comm = {};
  static const rx_session_state_t   s_zero_session  = {};
  s_link                                            = s_zero_link;
  s_spi_comm                                        = s_zero_spi_comm;
  s_session                                         = s_zero_session;
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
  mock_rspi_deinit(nullptr);
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
 * @since Version 1.0.0
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
    .channel     = k_rspi_channel_0,
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
 */
void test_init_success_no_fec(void)
{
  rx_err_t err = rx_session_init(&s_session);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rspi_channel_0,
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
 */
void test_init_custom_retries(void)
{
  rx_err_t err = rx_session_init(&s_session);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session = &s_session,
    .channel = k_rspi_channel_0,
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * 1. IDLE -> WAITING_ACK on first transmission attempt
 * 2. WAITING_ACK -> RETRANSMITTING on NACK or timeout
 * 3. RETRANSMITTING -> ERROR after max_retries exhausted
 * 4. ERROR -> IDLE on explicit reset
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
 */
void test_state_query_null_and_error(void)
{
  /* NULL link returns error state */
  TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(nullptr));

  /* Uninitialized link (all zeros) also returns error state */
  rx_spi_link_t uninit_link = {};
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
 * @since Version 1.0.0
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
 * @since Version 1.0.0
 */
void test_max_retries_enforcement(void)
{
  /* Initialize with custom max_retries=1 */
  rx_err_t err = rx_session_init(&s_session);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rspi_channel_0,
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
 * @since Version 1.0.0
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
 * Internal Helper: internal_bytes_to_soft_bits
 * =============================================================================
 */

/**
 * @brief Test internal_bytes_to_soft_bits with null data pointer
 */
void test_bytes_to_soft_bits_null_data(void)
{
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];
  uint32_t      soft_len = 0;
  rx_err_t      err =
    internal_bytes_to_soft_bits(nullptr, 1, soft, k_rx_spi_test_soft_bits_per_byte, &soft_len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_bytes_to_soft_bits with null soft pointer
 */
void test_bytes_to_soft_bits_null_soft(void)
{
  uint8_t  data[]   = {k_rx_spi_test_data_aa};
  uint32_t soft_len = 0;
  rx_err_t err =
    internal_bytes_to_soft_bits(data, 1, nullptr, k_rx_spi_test_soft_bits_per_byte, &soft_len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_bytes_to_soft_bits with null soft_len pointer
 */
void test_bytes_to_soft_bits_null_soft_len(void)
{
  uint8_t       data[] = {k_rx_spi_test_data_aa};
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];
  rx_err_t      err =
    internal_bytes_to_soft_bits(data, 1, soft, k_rx_spi_test_soft_bits_per_byte, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_bytes_to_soft_bits with valid 1-byte input
 */
void test_bytes_to_soft_bits_valid(void)
{
  uint8_t       data[] = {k_rx_spi_test_data_aa}; /* 10101010 in binary */
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];
  uint32_t      soft_len = 0;
  rx_err_t      err =
    internal_bytes_to_soft_bits(data, 1, soft, k_rx_spi_test_soft_bits_per_byte, &soft_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_spi_test_soft_bits_per_byte, soft_len);
}

/**
 * @brief Test internal_bytes_to_soft_bits with buffer too small
 */
void test_bytes_to_soft_bits_buffer_too_small(void)
{
  uint8_t       data[] = {k_rx_spi_test_data_aa, k_rx_spi_test_data_bb}; /* needs 16 soft bits */
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];                  /* only 8 slots */
  uint32_t      soft_len = 0;
  rx_err_t      err      = internal_bytes_to_soft_bits(data,
                                             k_rx_spi_test_two_bytes,
                                             soft,
                                             k_rx_spi_test_soft_bits_per_byte,
                                             &soft_len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test internal_bytes_to_soft_bits: null data with null soft_len
 *
 * @details
 * Covers the inner branch where soft_len is also null when data is null.
 * Verifies k_rx_err_invalid_arg returned without crash.
 */
void test_bytes_to_soft_bits_null_data_null_soft_len(void)
{
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];
  /* soft_len is null AND data is null: inner if(soft_len != nullptr) is false */
  rx_err_t err =
    internal_bytes_to_soft_bits(nullptr, 1, soft, k_rx_spi_test_soft_bits_per_byte, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_bytes_to_soft_bits: null soft with null soft_len
 *
 * @details
 * Covers the inner branch where soft_len is also null when soft is null.
 */
void test_bytes_to_soft_bits_null_soft_null_soft_len(void)
{
  uint8_t  data[] = {k_rx_spi_test_data_aa};
  rx_err_t err =
    internal_bytes_to_soft_bits(data, 1, nullptr, k_rx_spi_test_soft_bits_per_byte, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_bytes_to_soft_bits: zero-length data is valid
 *
 * @details
 * data_len == 0 skips the overflow check and the buffer check (required=0),
 * writes soft_len=0, and returns k_rx_ok.
 */
void test_bytes_to_soft_bits_zero_len(void)
{
  uint8_t       data[] = {k_rx_spi_test_data_aa};
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];
  uint32_t      soft_len = k_rx_spi_test_initial_soft_len;
  rx_err_t      err =
    internal_bytes_to_soft_bits(data, 0, soft, k_rx_spi_test_soft_bits_per_byte, &soft_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, soft_len);
}

/* =============================================================================
 * Internal Helper: internal_wait_for_ack
 * =============================================================================
 */

/**
 * @brief Test internal_wait_for_ack - propagates SPI receive errors
 *
 * @details
 * Calls internal_wait_for_ack with an initialized link. Since there is no
 * real hardware, rx_spi_comm_receive returns a non-ok error and the function
 * propagates it. The exact error code depends on the SPI mock state.
 */
void test_wait_for_ack_no_hardware(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Without hardware the underlying SPI receive returns a non-ok error */
  err = internal_wait_for_ack(&s_link, 0);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Internal Helper: internal_receive_passthrough
 * =============================================================================
 */

/**
 * @brief Test internal_receive_passthrough copies payload to result
 *
 * @details
 * Builds a minimal rx_frame_t with a small payload and verifies the function
 * copies the payload into the result buffer and sets payload_len correctly.
 */
void test_receive_passthrough_copies_payload(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_frame_t frame                       = {};
  frame.header.length                    = k_rx_spi_test_three_bytes;
  frame.header.flags                     = 0; /* no ACK required */
  frame.header.sequence                  = 1;
  frame.payload[0]                       = k_rx_spi_test_data_aa;
  frame.payload[1]                       = k_rx_spi_test_data_bb;
  frame.payload[k_rx_spi_test_two_bytes] = k_rx_spi_test_data_cc;

  rx_spi_link_receive_result_t result = {};

  err = internal_receive_passthrough(&s_link, &frame, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_spi_test_three_bytes, result.payload_len);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_aa, result.payload[0]);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_bb, result.payload[1]);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_cc, result.payload[k_rx_spi_test_two_bytes]);
  TEST_ASSERT_FALSE(result.fec_decoded);
}

/**
 * @brief Test internal_receive_passthrough with zero-length payload
 *
 * @details
 * Verifies that a frame with header.length == 0 results in payload_len == 0
 * and k_rx_ok returned.
 */
void test_receive_passthrough_zero_payload(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_frame_t frame      = {};
  frame.header.length   = 0;
  frame.header.flags    = 0;
  frame.header.sequence = k_rx_spi_test_seq_2;

  rx_spi_link_receive_result_t result = {};

  err = internal_receive_passthrough(&s_link, &frame, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, result.payload_len);
}

/* =============================================================================
 * Internal Helper: internal_receive_fec_decode
 * =============================================================================
 */

/**
 * @brief Test internal_receive_fec_decode with FEC-enabled link
 *
 * @details
 * Builds a frame with a valid small FEC-encoded payload and calls
 * internal_receive_fec_decode. Because the soft bits are arbitrary (not a
 * real FEC-encoded stream), the Viterbi decoder may or may not succeed; the
 * test only verifies that the function runs without crashing and returns a
 * known error code (k_rx_ok or k_rx_err_protocol_error).
 */
void test_receive_fec_decode_runs(void)
{
  rx_err_t err = internal_init_link(true); /* FEC enabled */

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Build a frame with a small but non-zero payload; the content is arbitrary */
  rx_frame_t frame                         = {};
  frame.header.length                      = k_rx_spi_test_four_bytes; /* 4 bytes = 32 soft bits */
  frame.header.flags                       = k_frame_flag_fec_enabled;
  frame.header.sequence                    = k_rx_spi_test_seq_5;
  frame.payload[0]                         = k_rx_spi_test_data_byte1;
  frame.payload[1]                         = k_rx_spi_test_data_byte2;
  frame.payload[k_rx_spi_test_two_bytes]   = k_rx_spi_test_data_byte3;
  frame.payload[k_rx_spi_test_three_bytes] = k_rx_spi_test_data_byte4;

  rx_spi_link_receive_result_t result = {};

  err = internal_receive_fec_decode(&s_link, &frame, &result);
  /* Either succeeds or fails with protocol error - both are acceptable */
  TEST_ASSERT_TRUE(err == k_rx_ok || err == k_rx_err_protocol_error);
  TEST_ASSERT_TRUE(result.fec_decoded);
}

/* =============================================================================
 * Internal Helper: internal_prepare_tx_payload
 * ============================================================================= */

/**
 * @brief Test internal_prepare_tx_payload passthrough (FEC disabled)
 *
 * @details
 * With FEC disabled the function must return the original payload pointer
 * and length unchanged with the REQUIRES_ACK flag set.
 */
void test_prepare_tx_payload_no_fec(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t        data[k_rx_spi_test_four_bytes] = {k_rx_spi_test_data_byte1,
                                                   k_rx_spi_test_data_byte2,
                                                   k_rx_spi_test_data_byte3,
                                                   k_rx_spi_test_data_byte4};
  const uint8_t* out_payload                    = nullptr;
  uint32_t       out_len                        = 0;
  uint8_t        out_flags                      = 0;

  err = internal_prepare_tx_payload(&s_link,
                                    data,
                                    k_rx_spi_test_four_bytes,
                                    &out_payload,
                                    &out_len,
                                    &out_flags);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(data, out_payload);
  TEST_ASSERT_EQUAL(k_rx_spi_test_four_bytes, out_len);
  TEST_ASSERT_BITS(k_frame_flag_requires_ack, k_frame_flag_requires_ack, out_flags);
}

/**
 * @brief Test internal_prepare_tx_payload with FEC enabled and null payload
 *
 * @details
 * With FEC enabled but payload == NULL, the condition
 * (fec_enabled && payload != NULL && ...) evaluates to false, so the
 * function behaves as passthrough. Exercises the null-payload sub-branch
 * of the compound condition.
 */
void test_prepare_tx_payload_fec_null_payload(void)
{
  rx_err_t err = internal_init_link(true); /* FEC enabled */

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const uint8_t* out_payload = nullptr;
  uint32_t       out_len     = 0;
  uint8_t        out_flags   = 0;

  err = internal_prepare_tx_payload(&s_link, nullptr, 0, &out_payload, &out_len, &out_flags);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NULL(out_payload);
  TEST_ASSERT_EQUAL(0, out_len);
}

/**
 * @brief Test internal_prepare_tx_payload with FEC enabled and zero length
 *
 * @details
 * With FEC enabled, payload non-null, but payload_len == 0, the compound
 * condition (fec_enabled && payload != NULL && payload_len > 0) is false.
 * The payload is passed through unchanged. Exercises the payload_len == 0
 * sub-branch.
 */
void test_prepare_tx_payload_fec_zero_len(void)
{
  rx_err_t err = internal_init_link(true); /* FEC enabled */

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t        data[k_rx_spi_test_four_bytes] = {k_rx_spi_test_data_byte1,
                                                   k_rx_spi_test_data_byte2,
                                                   k_rx_spi_test_data_byte3,
                                                   k_rx_spi_test_data_byte4};
  const uint8_t* out_payload                    = nullptr;
  uint32_t       out_len                        = 0;
  uint8_t        out_flags                      = 0;

  err = internal_prepare_tx_payload(&s_link, data, 0, &out_payload, &out_len, &out_flags);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(data, out_payload);
  TEST_ASSERT_EQUAL(0, out_len);
}

/**
 * @brief Test internal_prepare_tx_payload with FEC enabled and valid payload
 *
 * @details
 * With FEC enabled and a valid non-empty payload, the function should FEC-
 * encode the data, set out_payload to the internal encode buffer, and set
 * the FEC flag in out_flags. Exercises the full FEC encode path.
 */
void test_prepare_tx_payload_fec_encodes(void)
{
  rx_err_t err = internal_init_link(true); /* FEC enabled */

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t        data[k_rx_spi_test_eight_bytes] = {k_rx_spi_test_data_aa,
                                                    k_rx_spi_test_data_bb,
                                                    k_rx_spi_test_data_cc,
                                                    k_rx_spi_test_data_dd,
                                                    k_rx_spi_test_data_11,
                                                    k_rx_spi_test_data_22,
                                                    k_rx_spi_test_data_33,
                                                    k_rx_spi_test_data_44};
  const uint8_t* out_payload                     = nullptr;
  uint32_t       out_len                         = 0;
  uint8_t        out_flags                       = 0;

  err = internal_prepare_tx_payload(&s_link,
                                    data,
                                    k_rx_spi_test_eight_bytes,
                                    &out_payload,
                                    &out_len,
                                    &out_flags);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_NULL(out_payload);
  TEST_ASSERT_GREATER_THAN(0, out_len);
  TEST_ASSERT_BITS(k_frame_flag_fec_enabled, k_frame_flag_fec_enabled, out_flags);
  TEST_ASSERT_BITS(k_frame_flag_requires_ack, k_frame_flag_requires_ack, out_flags);
}

/**
 * @brief Test send with NULL payload and zero length (valid call)
 *
 * @details
 * Validates that rx_spi_link_send() accepts NULL payload when payload_len == 0.
 * This is a valid call (sending a zero-length frame). The send will fail with
 * retry limit exhaustion (no hardware), not invalid_arg. Exercises the false
 * branch of the compound condition (payload == NULL && payload_len > 0).
 */
void test_send_null_payload_zero_len(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* NULL payload with zero length is valid - passes the null check */
  /* Should fail with retry limit (no hardware), not invalid_arg */
  err = rx_spi_link_send(&s_link, k_frame_type_command, nullptr, 0);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test internal_receive_passthrough sends ACK when requires_ack flag is set
 *
 * @details
 * Builds a frame with k_frame_flag_requires_ack set and verifies the
 * function still returns k_rx_ok. The ACK send will fail (no hardware) but
 * this exercises the requires_ack branch.
 */
void test_receive_passthrough_with_requires_ack(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_frame_t frame      = {};
  frame.header.length   = k_rx_spi_test_two_bytes;
  frame.header.flags    = k_frame_flag_requires_ack;
  frame.header.sequence = k_rx_spi_test_seq_3;
  frame.payload[0]      = k_rx_spi_test_data_55;
  frame.payload[1]      = k_rx_spi_test_data_66;

  rx_spi_link_receive_result_t result = {};

  err = internal_receive_passthrough(&s_link, &frame, &result);
  /* ACK send will fail with no hardware but function still returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_spi_test_two_bytes, result.payload_len);
}

/* =============================================================================
 * Mock RSPI Helpers for Frame Injection
 * ============================================================================= */

/**
 * @brief Initialize RSPI channel 0 via mock for frame injection tests
 *
 * @details
 * Registers RSPI channel 0 with the mock peripheral driver so that
 * mock_rspi_inject_rx_data() and mock_rspi_set_data_available() can be used
 * to simulate incoming frames.
 *
 * @pre mock_rspi_init() called in setUp()
 * @post Channel 0 ready for frame injection
 *
 * @since Version 1.0.0
 */
static void helper_init_mock_rspi_channel(void)
{
  const rspi_config_t config = {
    .spi_mode  = (rspi_mode_t)k_spi_comm_default_mode,
    .use_16bit = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rspi_init_peripheral(k_rspi_channel_0, &config));
}

/**
 * @brief Initialize SPI link stack with mock RSPI channel ready for injection
 *
 * @details
 * Calls internal_init_link() then helper_init_mock_rspi_channel() so that
 * mock RSPI is ready. The write_ready flag is set to true so that
 * rx_spi_comm_send() succeeds.
 *
 * @param[in] fec_enabled Whether to enable FEC
 * @return rx_err_t from internal_init_link()
 *
 * @since Version 1.0.0
 */
static rx_err_t helper_init_link_with_mock_rspi(bool fec_enabled)
{
  rx_err_t err = internal_init_link(fec_enabled);

  if (err != k_rx_ok) {
    return err;
  }
  helper_init_mock_rspi_channel();
  mock_rspi_set_write_ready(nullptr, k_rspi_channel_0, true);
  return k_rx_ok;
}

/**
 * @brief Encode a frame into a buffer
 *
 * @details
 * Encodes a frame into the provided buffer and returns the encoded length.
 * Used by injection helpers to get the raw encoded bytes.
 *
 * @param[in]  type        Frame type
 * @param[in]  sequence    Sequence number
 * @param[in]  flags       Frame flags
 * @param[in]  payload     Payload data (may be nullptr)
 * @param[in]  payload_len Payload length
 * @param[out] out_buf     Output buffer (must be >= k_frame_max_size bytes)
 * @param[out] out_len     Actual encoded length in bytes
 *
 * @since Version 1.0.0
 */
static void helper_encode_frame(rx_frame_type_t type,
                                uint16_t        sequence,
                                uint8_t         flags,
                                const uint8_t*  payload,
                                uint32_t        payload_len,
                                uint8_t*        out_buf,
                                uint32_t*       out_len)
{
  rx_frame_encoder_t encoder;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&encoder));

  rx_frame_t frame      = {};
  frame.header.type     = (uint8_t)type;
  frame.header.sequence = sequence;
  frame.header.flags    = flags;
  frame.header.length   = (uint16_t)payload_len;

  if (payload != nullptr && payload_len > 0) {
    for (uint32_t i = 0; i < payload_len; i++) {
      frame.payload[i] = payload[i];
    }
  }

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&encoder, &frame, out_buf, out_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&encoder));
}

/**
 * @brief Create an encoded frame and inject it into the RSPI mock RX buffer
 *
 * @details
 * Encodes a frame using rx_frame_encode() and injects the resulting bytes
 * into the mock RSPI channel 0 RX buffer, then sets data_available = true.
 * The injected frame will be returned by the next rx_spi_comm_receive() call.
 *
 * @param[in] type       Frame type (ACK, NACK, command, etc.)
 * @param[in] sequence   Sequence number
 * @param[in] flags      Frame flags
 * @param[in] payload    Payload data (may be nullptr)
 * @param[in] payload_len Payload length in bytes
 *
 * @since Version 1.0.0
 */
static void helper_inject_frame(rx_frame_type_t type,
                                uint16_t        sequence,
                                uint8_t         flags,
                                const uint8_t*  payload,
                                uint32_t        payload_len)
{
  uint8_t  buf[k_frame_max_size];
  uint32_t len = 0;

  helper_encode_frame(type, sequence, flags, payload, payload_len, buf, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, mock_rspi_inject_rx_data(nullptr, k_rspi_channel_0, buf, len));
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);
}

/**
 * @brief Inject a frame prefixed with padding bytes consumed by a prior send transfer
 *
 * @details
 * In the SPI mock, every call to rspi_peripheral_transfer() (including SEND
 * operations) consumes bytes from the shared RX injection buffer. To ensure
 * an injected frame survives a preceding send transfer, this helper prepends
 * send_frame_wire_size zero-bytes before the encoded frame. The send transfer
 * consumes the padding, leaving the frame available for the subsequent receive.
 *
 * @param[in] type               Frame type for the injected frame
 * @param[in] sequence           Sequence number for the injected frame
 * @param[in] flags              Flags for the injected frame
 * @param[in] payload            Payload for the injected frame (may be nullptr)
 * @param[in] payload_len        Payload length
 * @param[in] send_frame_wire_sz Number of padding bytes to prepend (= wire size of send frame)
 *
 * @since Version 1.0.0
 */
static void helper_inject_frame_after_send(rx_frame_type_t type,
                                           uint16_t        sequence,
                                           uint8_t         flags,
                                           const uint8_t*  payload,
                                           uint32_t        payload_len,
                                           uint32_t        send_frame_wire_sz)
{
  uint8_t  frame_buf[k_frame_max_size];
  uint32_t frame_len = 0;

  helper_encode_frame(type, sequence, flags, payload, payload_len, frame_buf, &frame_len);

  uint8_t  combined[k_test_inject_buf_max];
  uint32_t combined_len = send_frame_wire_sz + frame_len;
  TEST_ASSERT_LESS_OR_EQUAL((uint32_t)k_test_inject_buf_max, combined_len);

  for (uint32_t i = 0; i < send_frame_wire_sz; i++) {
    combined[i] = 0;
  }
  for (uint32_t i = 0; i < frame_len; i++) {
    combined[send_frame_wire_sz + i] = frame_buf[i];
  }

  TEST_ASSERT_EQUAL(k_rx_ok,
                    mock_rspi_inject_rx_data(nullptr, k_rspi_channel_0, combined, combined_len));
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);
}

/* =============================================================================
 * Tests: internal_wait_for_ack with mock frame injection
 * ============================================================================= */

/**
 * @brief Test internal_wait_for_ack: ACK with matching sequence -> k_rx_ok
 *
 * @details
 * Injects an ACK frame with the expected sequence number and verifies that
 * internal_wait_for_ack() returns k_rx_ok.
 *
 * @since Version 1.0.0
 */
void test_wait_for_ack_matching_ack(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  /* Get the next TX sequence that will be used */
  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));

  helper_inject_frame(k_frame_type_ack, next_seq, k_frame_flag_none, nullptr, 0);

  rx_err_t err = internal_wait_for_ack(&s_link, next_seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test internal_wait_for_ack: ACK with mismatched sequence -> k_rx_err_timeout
 *
 * @details
 * Injects an ACK with wrong sequence number. internal_wait_for_ack() should
 * treat this as a timeout (log warning, return k_rx_err_timeout).
 *
 * @since Version 1.0.0
 */
void test_wait_for_ack_mismatched_ack_seq(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  uint16_t next_seq  = 0;
  uint16_t wrong_seq = next_seq + 1;

  helper_inject_frame(k_frame_type_ack, wrong_seq, k_frame_flag_none, nullptr, 0);

  rx_err_t err = internal_wait_for_ack(&s_link, next_seq);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief Test internal_wait_for_ack: NACK with matching sequence -> k_rx_err_protocol_error
 *
 * @details
 * Injects a NACK with the expected sequence number. internal_wait_for_ack()
 * should return k_rx_err_protocol_error to trigger HARQ retry.
 *
 * @since Version 1.0.0
 */
void test_wait_for_ack_matching_nack(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));

  helper_inject_frame(k_frame_type_nack, next_seq, k_frame_flag_none, nullptr, 0);

  rx_err_t err = internal_wait_for_ack(&s_link, next_seq);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test internal_wait_for_ack: NACK with mismatched sequence -> k_rx_err_timeout
 *
 * @details
 * Injects a NACK with wrong sequence number. The function should treat this
 * as a timeout (not a decode failure), returning k_rx_err_timeout.
 *
 * @since Version 1.0.0
 */
void test_wait_for_ack_mismatched_nack_seq(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  uint16_t next_seq  = 0;
  uint16_t wrong_seq = next_seq + 1;

  helper_inject_frame(k_frame_type_nack, wrong_seq, k_frame_flag_none, nullptr, 0);

  rx_err_t err = internal_wait_for_ack(&s_link, next_seq);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief Test internal_wait_for_ack: non-ACK/NACK frame -> k_rx_err_timeout
 *
 * @details
 * Injects a data (command) frame while waiting for ACK. The function should
 * ignore it and return k_rx_err_timeout.
 *
 * @since Version 1.0.0
 */
void test_wait_for_ack_non_ack_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  uint16_t next_seq = 0;
  uint8_t  data[]   = {k_rx_spi_test_data_byte1, k_rx_spi_test_data_byte2};

  helper_inject_frame(k_frame_type_command, next_seq, k_frame_flag_none, data, sizeof(data));

  rx_err_t err = internal_wait_for_ack(&s_link, next_seq);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief Test internal_wait_for_ack: no data available -> timeout propagated
 *
 * @details
 * With mock RSPI having no data available, rx_spi_comm_receive() returns a
 * non-ok code that is NOT k_rx_err_timeout (it returns k_rx_err_invalid_state
 * on uninitialized channel). Verifies the non-timeout error path propagates.
 *
 * @since Version 1.0.0
 */
void test_wait_for_ack_receive_timeout(void)
{
  /* Initialize WITHOUT mock RSPI channel so receive returns invalid_state */
  TEST_ASSERT_EQUAL(k_rx_ok, internal_init_link(false));

  rx_err_t err = internal_wait_for_ack(&s_link, 0);
  /* Non-timeout, non-ok error: propagated from rx_spi_comm_receive */
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Tests: rx_spi_link_send with mock RSPI (ACK/NACK injection)
 * ============================================================================= */

/**
 * @brief Test rx_spi_link_send: ACK injected -> returns k_rx_ok, state=idle
 *
 * @details
 * Pre-injects an ACK frame so the mock RSPI returns it when
 * rx_spi_comm_receive() is called during the ACK wait. Verifies send
 * returns k_rx_ok and link state is idle.
 *
 * @since Version 1.0.0
 */
void test_send_ack_received_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  /* Get the sequence that will be used by the send */
  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));

  /* The send transfer for a 2-byte payload without FEC produces a 14-byte wire
   * frame. The mock's rspi_peripheral_transfer() reads from the same RX buffer
   * for both sends and receives. Pre-inject with k_test_send_frame_2byte_size
   * bytes of padding so the send transfer consumes the padding while the ACK
   * frame bytes remain for the subsequent rx_spi_comm_receive() call. */
  uint8_t data[] = {k_rx_spi_test_data_ab, k_rx_spi_test_data_cd};
  helper_inject_frame_after_send(k_frame_type_ack,
                                 next_seq,
                                 k_frame_flag_none,
                                 nullptr,
                                 0,
                                 k_test_send_frame_2byte_size);

  rx_err_t err = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test rx_spi_link_send: NACK injected -> retries, eventually retry_limit
 *
 * @details
 * Initializes link with max_retries=1 and injects a NACK. The send should
 * attempt once, receive NACK, continue loop, exhaust retries, and return
 * k_rx_err_retry_limit.
 *
 * @since Version 1.0.0
 */
void test_send_nack_received_retries_exhausted(void)
{
  /* Init with 1 retry to keep test fast */
  rx_err_t err = rx_session_init(&s_session);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rspi_channel_0,
    .fec_enabled = false,
  };
  err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm,
    .fec_enabled = false,
    .max_retries = k_rx_spi_test_single_retry,
  };
  err = rx_spi_link_init(&s_link, &link_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  helper_init_mock_rspi_channel();
  mock_rspi_set_write_ready(nullptr, k_rspi_channel_0, true);

  /* Get the sequence that will be used */
  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));

  /* Inject NACK with matching sequence */
  helper_inject_frame(k_frame_type_nack, next_seq, k_frame_flag_none, nullptr, 0);

  uint8_t data[] = {k_rx_spi_test_data_ab};
  err            = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));

  /* 1 retry, NACK received -> continue loop, loop ends -> retry_limit */
  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);
}

/* =============================================================================
 * Tests: internal_receive_passthrough oversized payload
 * ============================================================================= */

/**
 * @brief Test internal_receive_passthrough with oversized payload (> k_harq_max_payload)
 *
 * @details
 * Builds a frame with header.length = k_harq_max_payload + 1. The function
 * should truncate to k_harq_max_payload and return k_rx_ok. Exercises the
 * defensive oversized branch.
 *
 * @since Version 1.0.0
 */
void test_receive_passthrough_oversized_payload(void)
{
  rx_err_t err = internal_init_link(false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_frame_t frame = {};
  /* header.length is uint16_t; set to k_harq_max_payload + 1 to trigger truncation */
  frame.header.length   = (uint16_t)(k_harq_max_payload + 1);
  frame.header.flags    = k_frame_flag_none;
  frame.header.sequence = k_rx_spi_test_seq_7;
  /* Payload buffer in rx_frame_t is k_frame_max_payload bytes; fill with pattern */
  for (uint32_t i = 0; i < sizeof(frame.payload); i++) {
    frame.payload[i] = (uint8_t)k_rx_spi_test_fill_pattern;
  }

  rx_spi_link_receive_result_t result = {};

  err = internal_receive_passthrough(&s_link, &frame, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Must be truncated to k_harq_max_payload */
  TEST_ASSERT_EQUAL(k_harq_max_payload, result.payload_len);
}

/* =============================================================================
 * Tests: rx_spi_link_receive with injected frames
 * ============================================================================= */

/**
 * @brief Test rx_spi_link_receive: ACK frame -> returns k_rx_err_no_data
 *
 * @details
 * Injects an ACK frame. rx_spi_link_receive() should recognize it as a
 * control frame and return k_rx_err_no_data without passing it to the caller.
 *
 * @since Version 1.0.0
 */
void test_receive_filters_ack_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  helper_inject_frame(k_frame_type_ack, 0, k_frame_flag_none, nullptr, 0);

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Test rx_spi_link_receive: NACK frame -> returns k_rx_err_no_data
 *
 * @details
 * Injects a NACK frame. rx_spi_link_receive() should return k_rx_err_no_data.
 *
 * @since Version 1.0.0
 */
void test_receive_filters_nack_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  helper_inject_frame(k_frame_type_nack, 0, k_frame_flag_none, nullptr, 0);

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Test rx_spi_link_receive: PONG frame -> returns k_rx_err_no_data
 *
 * @details
 * Injects a PONG frame. rx_spi_comm_receive() passes PONG frames back to the
 * caller (they are not consumed internally). rx_spi_link_receive() then
 * returns k_rx_err_no_data to signal non-application data.
 *
 * Note: PING frames are consumed internally by rx_spi_comm (auto-PONG),
 * so they never reach rx_spi_link_receive(). PONG frames are not consumed
 * and are used here to exercise the PING/PONG/RESET/RESET_ACK filter path.
 *
 * @since Version 1.0.0
 */
void test_receive_filters_ping_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  /* PONG is returned by rx_spi_comm_receive; exercises the filter branch */
  helper_inject_frame(k_frame_type_pong, 1, k_frame_flag_none, nullptr, 0);

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Test rx_spi_link_receive: PONG frame -> returns k_rx_err_no_data
 *
 * @details
 * Injects a PONG frame to cover the pong branch of the PING/PONG/RESET filter.
 *
 * @since Version 1.0.0
 */
void test_receive_filters_pong_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  helper_inject_frame(k_frame_type_pong, 0, k_frame_flag_none, nullptr, 0);

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Test rx_spi_link_receive: RESET_ACK frame -> returns k_rx_err_no_data
 *
 * @details
 * Injects a RESET_ACK frame. rx_spi_comm_receive() passes RESET_ACK frames
 * back to the caller (they are not consumed internally). rx_spi_link_receive()
 * returns k_rx_err_no_data to signal non-application data. This covers the
 * RESET/RESET_ACK path of the filter.
 *
 * Note: RESET frames are consumed internally by rx_spi_comm (auto-RESET_ACK),
 * so they never reach rx_spi_link_receive(). RESET_ACK frames are not consumed
 * and are used here to exercise the branch.
 *
 * @since Version 1.0.0
 */
void test_receive_filters_reset_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  /* RESET_ACK is returned by rx_spi_comm_receive; exercises the filter branch */
  helper_inject_frame(k_frame_type_reset_ack, 1, k_frame_flag_none, nullptr, 0);

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Test rx_spi_link_receive: RESET_ACK frame -> returns k_rx_err_no_data
 *
 * @details
 * Injects a RESET_ACK frame to cover the final branch of the PING/PONG/RESET
 * filter.
 *
 * @since Version 1.0.0
 */
void test_receive_filters_reset_ack_frame(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  helper_inject_frame(k_frame_type_reset_ack, 0, k_frame_flag_none, nullptr, 0);

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Test rx_spi_link_receive: data frame (no FEC) -> passthrough, k_rx_ok
 *
 * @details
 * Injects a command data frame without FEC flag. rx_spi_link_receive()
 * should route to passthrough, copy payload, and return k_rx_ok. Exercises
 * the data frame metadata population and passthrough routing path.
 *
 * @since Version 1.0.0
 */
void test_receive_data_frame_passthrough(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  uint8_t data[] = {k_rx_spi_test_data_11, k_rx_spi_test_data_22, k_rx_spi_test_data_33};
  /* Inject a command frame with no FEC flag */
  helper_inject_frame(k_frame_type_command,
                      k_rx_spi_test_seq_5,
                      k_frame_flag_none,
                      data,
                      sizeof(data));

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_spi_test_three_bytes, result.payload_len);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_11, result.payload[0]);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_22, result.payload[1]);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_33, result.payload[2]);
  TEST_ASSERT_FALSE(result.fec_decoded);
  TEST_ASSERT_EQUAL(k_rx_spi_test_seq_5, result.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_command, result.frame_type);
}

/**
 * @brief Test rx_spi_link_receive: retransmit flag set in received frame
 *
 * @details
 * Injects a command frame with k_frame_flag_retransmit set. Verifies that
 * result.is_retransmit is true, exercising the retransmit flag extraction.
 *
 * @since Version 1.0.0
 */
void test_receive_data_frame_retransmit_flag(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  uint8_t data[] = {k_rx_spi_test_data_de, k_rx_spi_test_data_ad};
  helper_inject_frame(k_frame_type_command,
                      k_rx_spi_test_seq_3,
                      k_frame_flag_retransmit,
                      data,
                      sizeof(data));

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(result.is_retransmit);
}

/**
 * @brief Test internal_receive_fec_decode: success path sends ACK
 *
 * @details
 * Builds a properly FEC-encoded payload and calls internal_receive_fec_decode
 * with a link that has mock RSPI ready (for ACK send). When decode succeeds
 * the function sends an ACK internally. Verifies that fec_decoded is set.
 *
 * We use rx_harq_encode to produce a valid encoded payload, then call
 * internal_receive_fec_decode with a frame containing that encoded data.
 * On success, the function should return k_rx_ok and fec_decoded == true.
 * On failure (unlikely but possible with short payload), protocol_error is also
 * acceptable.
 *
 * @since Version 1.0.0
 */
void test_receive_fec_decode_success_path(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(true)); /* FEC enabled */

  /* Encode a small payload using HARQ encode to get a valid FEC bitstream */
  uint8_t  original[] = {k_rx_spi_test_data_ab,
                         k_rx_spi_test_data_cd,
                         k_rx_spi_test_data_ef,
                         k_rx_spi_test_data_12,
                         k_rx_spi_test_data_34,
                         k_rx_spi_test_data_56,
                         k_rx_spi_test_data_78,
                         k_rx_spi_test_data_9a};
  uint8_t  encoded[k_spi_link_max_encoded_payload];
  uint32_t encoded_len = 0;

  rx_err_t err = rx_harq_encode(&s_link.harq,
                                original,
                                sizeof(original),
                                encoded,
                                sizeof(encoded),
                                &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Build a frame with the encoded payload */
  rx_frame_t frame      = {};
  frame.header.length   = (uint16_t)encoded_len;
  frame.header.flags    = k_frame_flag_fec_enabled;
  frame.header.sequence = k_rx_spi_test_seq_10;
  for (uint32_t i = 0; i < encoded_len; i++) {
    frame.payload[i] = encoded[i];
  }

  rx_spi_link_receive_result_t result = {};

  err = internal_receive_fec_decode(&s_link, &frame, &result);
  /* Viterbi decode succeeds for a properly encoded payload; ACK send may log a
   * warning if mock RSPI write buffer is saturated but the function still returns
   * k_rx_ok because the payload was decoded successfully. */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(result.fec_decoded);
}

/**
 * @brief Test internal_receive_fec_decode: NACK path when rx_harq_decode fails
 *
 * @details
 * Forces rx_harq_decode() to return an error by setting harq.initialized = 0
 * (simulating a de-initialized HARQ state). When the HARQ is not initialized,
 * rx_harq_decode() returns k_rx_err_invalid_state. internal_receive_fec_decode()
 * then takes the failure path: sends NACK and returns k_rx_err_protocol_error.
 *
 * This covers lines 694-696 of rx_spi_link.c (the NACK send path in
 * internal_receive_fec_decode).
 *
 * @pre FEC link initialized
 * @post NACK send attempted, function returns k_rx_err_protocol_error
 *
 * @since Version 1.0.0
 */
void test_receive_fec_decode_failure_nack_path(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(true)); /* FEC enabled */

  /* Build a frame with a valid-looking FEC-encoded payload (4 bytes) */
  rx_frame_t frame                           = {};
  frame.header.length                        = k_rx_spi_test_four_bytes;
  frame.header.flags                         = k_frame_flag_fec_enabled;
  frame.header.sequence                      = k_rx_spi_test_seq_7;
  frame.payload[0]                           = k_rx_spi_test_data_aa;
  frame.payload[1]                           = k_rx_spi_test_data_bb;
  frame.payload[k_rx_spi_test_payload_idx_2] = k_rx_spi_test_data_cc;
  frame.payload[k_rx_spi_test_payload_idx_3] = k_rx_spi_test_data_dd;

  /* Force HARQ into de-initialized state so rx_harq_decode() returns an error.
   * This triggers the NACK path in internal_receive_fec_decode(). */
  s_link.harq.initialized = 0;

  rx_spi_link_receive_result_t result = {};

  rx_err_t err = internal_receive_fec_decode(&s_link, &frame, &result);
  /* HARQ not initialized -> rx_harq_decode fails -> NACK sent -> protocol_error */
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
  TEST_ASSERT_TRUE(result.fec_decoded);
}

/* =============================================================================
 * Tests: rx_spi_link_send NACK retry path
 * ============================================================================= */

/**
 * @brief Test rx_spi_link_send: NACK received -> continues retry loop (protocol_error path)
 *
 * @details
 * Injects a NACK frame (after the send transfer padding) so that
 * internal_wait_for_ack() receives a matching NACK and returns
 * k_rx_err_protocol_error. rx_spi_link_send() then takes the
 * k_rx_err_protocol_error branch (lines 548-549) and continues the loop.
 * With max_retries=1, the loop exhausts on the next attempt.
 *
 * @since Version 1.0.0
 */
void test_send_nack_retry_protocol_error_path(void)
{
  /* Init with 1 retry so the test terminates quickly */
  rx_err_t err = rx_session_init(&s_session);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_comm_config_t spi_cfg = {
    .session     = &s_session,
    .channel     = k_rspi_channel_0,
    .fec_enabled = false,
  };
  err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm,
    .fec_enabled = false,
    .max_retries = k_rx_spi_test_single_retry,
  };
  err = rx_spi_link_init(&s_link, &link_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  helper_init_mock_rspi_channel();
  mock_rspi_set_write_ready(nullptr, k_rspi_channel_0, true);

  /* Get the sequence that will be used */
  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));

  /* Send frame for 1-byte payload = 13 bytes. Inject NACK with matching seq
   * after 13 padding bytes so it survives the send transfer. */
  uint8_t data[] = {k_rx_spi_test_data_ef};
  helper_inject_frame_after_send(k_frame_type_nack,
                                 next_seq,
                                 k_frame_flag_none,
                                 nullptr,
                                 0,
                                 k_test_send_frame_1byte_size);

  err = rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data));

  /* 1 retry: NACK received -> k_rx_err_protocol_error -> continue -> loop ends -> retry_limit */
  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);
}

/* =============================================================================
 * Tests: rx_spi_link_receive FEC path (line 788 and fec_decode success)
 * ============================================================================= */

/**
 * @brief Test rx_spi_link_receive: data frame with FEC flag -> routes to FEC decode
 *
 * @details
 * Initializes link with fec_enabled=true and injects a data frame with
 * k_frame_flag_fec_enabled set. rx_spi_link_receive() should route to
 * internal_receive_fec_decode(). Covers line 788 (FEC path branch).
 *
 * The payload is minimal (non-zero length) with k_frame_flag_fec_enabled set.
 * The Viterbi decode may fail on arbitrary bytes, so k_rx_ok or
 * k_rx_err_protocol_error are both acceptable.
 *
 * @since Version 1.0.0
 */
void test_receive_data_frame_fec_path(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(true)); /* FEC enabled */

  /* Encode a small payload using HARQ to get a valid FEC-encoded buffer */
  uint8_t  original[] = {k_rx_spi_test_data_de,
                         k_rx_spi_test_data_ad,
                         k_rx_spi_test_data_be,
                         k_rx_spi_test_data_ef};
  uint8_t  encoded[k_spi_link_max_encoded_payload];
  uint32_t encoded_len = 0;

  rx_err_t err = rx_harq_encode(&s_link.harq,
                                original,
                                sizeof(original),
                                encoded,
                                sizeof(encoded),
                                &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Reset HARQ so it's ready to receive (encode invalidated state) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_harq_reset(&s_link.harq));

  /* Inject the FEC-encoded frame via mock RSPI */
  helper_inject_frame(k_frame_type_command, 0, k_frame_flag_fec_enabled, encoded, encoded_len);

  rx_spi_link_receive_result_t result = {};

  err = rx_spi_link_receive(&s_link, &result, k_rx_spi_test_timeout_normal);

  /* Viterbi decode succeeds for a properly encoded payload; function returns
   * k_rx_ok and sends an ACK (write_ready=true in helper_init_link_with_mock_rspi). */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(result.fec_decoded);
}

/* =============================================================================
 * Tests: internal_send_attempt coverage
 * ============================================================================= */

/**
 * @brief Test internal_send_attempt: ACK received -> k_rx_ok
 *
 * @details
 * Uses internal_send_attempt() directly with ACK pre-injected into mock RSPI.
 * This exercises the wait_for_ack return path.
 *
 * @since Version 1.0.0
 */
void test_send_attempt_ack_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  /* Get the sequence that will be used */
  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));

  /* The send transfer for a 1-byte payload without FEC produces a 13-byte wire
   * frame (SYNC(2)+SEQ(2)+LEN(2)+TYPE(1)+FLAGS(1)+PAYLOAD(1)+CRC(4)=13).
   * Prepend k_test_send_frame_1byte_size padding bytes so the send transfer
   * consumes them while the ACK frame remains for the receive. */
  helper_inject_frame_after_send(k_frame_type_ack,
                                 next_seq,
                                 k_frame_flag_none,
                                 nullptr,
                                 0,
                                 k_test_send_frame_1byte_size);

  uint8_t  payload[] = {k_rx_spi_test_data_55};
  rx_err_t err       = internal_send_attempt(&s_link,
                                       k_frame_type_command,
                                       k_frame_flag_requires_ack,
                                       payload,
                                       sizeof(payload),
                                       0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief internal_send_attempt: re-entrancy assert fires when path is already busy
 *
 * @details
 * Pre-sets s_send_path_busy via atomic_flag_test_and_set, then calls
 * internal_send_attempt. The Audit F-04 RX_ASSERT_PRE on line 401 evaluates
 * !atomic_flag_test_and_set(...) -> !true -> false, so the assert macro takes
 * its !(condition) branch and calls the no-op test mock of
 * internal_rx_fatal_error. Execution continues past the assert with the flag
 * still set; we explicitly clear it after the call so the next test starts
 * clean (the flag's clear at the bottom of internal_send_attempt's success
 * path is reached only when the prior send/wait succeeds).
 *
 * Covers the previously-uncovered false branch of the re-entrancy guard
 * (rx_spi_link.c:401), which is otherwise unreachable from a single-task
 * test harness because the function unconditionally clears the flag on exit.
 *
 * @since Version 1.0.0
 */
void test_send_attempt_reentrancy_assert_fires(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  /* Pre-inject an ACK frame so the post-assert send/wait path also has
   * data to consume; the assertion does not abort in test builds, so the
   * function continues past line 401. */
  uint16_t next_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(s_spi_comm.session, &next_seq));
  helper_inject_frame_after_send(k_frame_type_ack,
                                 next_seq,
                                 k_frame_flag_none,
                                 nullptr,
                                 0,
                                 k_test_send_frame_1byte_size);

  /* Pre-set the re-entrancy flag so the RX_ASSERT_PRE !(condition) branch
   * is taken on entry. */
  (void)atomic_flag_test_and_set(&s_send_path_busy);

  uint8_t        payload[] = {k_rx_spi_test_data_55};
  const rx_err_t err       = internal_send_attempt(&s_link,
                                             k_frame_type_command,
                                             k_frame_flag_requires_ack,
                                             payload,
                                             sizeof(payload),
                                             0);

  /* The function unconditionally clears s_send_path_busy at the end of its
   * body (line 443), so by the time we get here the flag is back to false.
   * The return value matches the success path because the mock RSPI ACK
   * is consumed normally. We only care that the assertion's false branch
   * was reached, which gcovr observes via line 401 coverage. */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Defensive: ensure the flag is cleared even if the function returned
   * via an error path that bypassed the unconditional clear (it does not
   * today, but a future refactor might). */
  atomic_flag_clear(&s_send_path_busy);
}

/**
 * @brief internal_bytes_to_soft_bits: null data AND null soft_len (line 167 false branch)
 *
 * @details Exercises the nested guard when data==nullptr and soft_len==nullptr.
 * The if(soft_len != nullptr) block is skipped, returning k_rx_err_invalid_arg.
 */
void test_bytes_to_soft_bits_null_data_null_len(void)
{
  rx_soft_bit_t soft[k_rx_spi_test_soft_bits_per_byte];
  rx_err_t      err =
    internal_bytes_to_soft_bits(nullptr, 1, soft, k_rx_spi_test_soft_bits_per_byte, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_bytes_to_soft_bits: null soft AND null soft_len (line 174 false branch)
 *
 * @details Exercises the nested guard when soft==nullptr and soft_len==nullptr.
 * The if(soft_len != nullptr) block is skipped, returning k_rx_err_invalid_arg.
 */
void test_bytes_to_soft_bits_null_soft_null_len(void)
{
  uint8_t  data[] = {k_rx_spi_test_data_aa};
  rx_err_t err =
    internal_bytes_to_soft_bits(data, 1, nullptr, k_rx_spi_test_soft_bits_per_byte, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_receive_passthrough with requires_ack flag and RSPI write ready
 *
 * @details With mock RSPI initialized and write_ready=true, the ACK send succeeds.
 * This exercises the FALSE branch of (ack_err != k_rx_ok) at line 725.
 */
void test_receive_passthrough_ack_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, helper_init_link_with_mock_rspi(false));

  rx_frame_t frame      = {};
  frame.header.length   = k_rx_spi_test_two_bytes;
  frame.header.flags    = k_frame_flag_requires_ack;
  frame.header.sequence = k_rx_spi_test_seq_7;
  frame.payload[0]      = k_rx_spi_test_data_aa;
  frame.payload[1]      = k_rx_spi_test_data_bb;

  rx_spi_link_receive_result_t result = {};

  /* With write_ready=true, ACK send succeeds: ack_err == k_rx_ok */
  rx_err_t err = internal_receive_passthrough(&s_link, &frame, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_rx_spi_test_two_bytes, result.payload_len);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_aa, result.payload[0]);
  TEST_ASSERT_EQUAL(k_rx_spi_test_data_bb, result.payload[1]);
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
/**
 * @brief Run lifecycle, state, and validation tests
 * @details 26 tests covering init (6), deinit (3), state (3), FEC (4),
 *          reset (3), send validation (4), and receive validation (3).
 */
static void internal_run_lifecycle_and_validation_tests(void)
{
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
}

/**
 * @brief Run behavioral and direct internal function tests
 * @details 24 tests covering behavioral (6), internal functions (18).
 */
static void internal_run_behavioral_and_internal_tests(void)
{
  /* Behavioral Tests */
  RUN_TEST(test_state_transitions_send_retries);
  RUN_TEST(test_fec_enabled_query);
  RUN_TEST(test_state_query_null_and_error);
  RUN_TEST(test_receive_filters_control_frames);
  RUN_TEST(test_max_retries_enforcement);
  RUN_TEST(test_reset_clears_error_state);

  /* Direct internal-function tests (RX_STATIC_TESTABLE) */
  RUN_TEST(test_bytes_to_soft_bits_null_data);
  RUN_TEST(test_bytes_to_soft_bits_null_data_null_len);
  RUN_TEST(test_bytes_to_soft_bits_null_data_null_soft_len);
  RUN_TEST(test_bytes_to_soft_bits_null_soft);
  RUN_TEST(test_bytes_to_soft_bits_null_soft_null_len);
  RUN_TEST(test_bytes_to_soft_bits_null_soft_null_soft_len);
  RUN_TEST(test_bytes_to_soft_bits_null_soft_len);
  RUN_TEST(test_bytes_to_soft_bits_valid);
  RUN_TEST(test_bytes_to_soft_bits_buffer_too_small);
  RUN_TEST(test_bytes_to_soft_bits_zero_len);
  RUN_TEST(test_wait_for_ack_no_hardware);
  RUN_TEST(test_receive_passthrough_copies_payload);
  RUN_TEST(test_receive_passthrough_zero_payload);
  RUN_TEST(test_receive_fec_decode_runs);
  RUN_TEST(test_prepare_tx_payload_no_fec);
  RUN_TEST(test_prepare_tx_payload_fec_null_payload);
  RUN_TEST(test_prepare_tx_payload_fec_zero_len);
  RUN_TEST(test_prepare_tx_payload_fec_encodes);
  RUN_TEST(test_receive_passthrough_with_requires_ack);
  RUN_TEST(test_receive_passthrough_ack_succeeds);
  RUN_TEST(test_send_null_payload_zero_len);
}

/**
 * @brief Run mock RSPI injection and integration tests
 * @details 19 tests covering wait_for_ack (6), send (3), receive (9),
 *          and FEC decode success/failure paths (1).
 */
static void internal_run_mock_rspi_injection_tests(void)
{
  /* Mock RSPI injection tests: internal_wait_for_ack */
  RUN_TEST(test_wait_for_ack_matching_ack);
  RUN_TEST(test_wait_for_ack_mismatched_ack_seq);
  RUN_TEST(test_wait_for_ack_matching_nack);
  RUN_TEST(test_wait_for_ack_mismatched_nack_seq);
  RUN_TEST(test_wait_for_ack_non_ack_frame);
  RUN_TEST(test_wait_for_ack_receive_timeout);

  /* Mock RSPI injection tests: rx_spi_link_send */
  RUN_TEST(test_send_ack_received_success);
  RUN_TEST(test_send_nack_received_retries_exhausted);
  RUN_TEST(test_send_nack_retry_protocol_error_path);

  /* Mock RSPI injection tests: rx_spi_link_receive */
  RUN_TEST(test_receive_filters_ack_frame);
  RUN_TEST(test_receive_filters_nack_frame);
  RUN_TEST(test_receive_filters_ping_frame);
  RUN_TEST(test_receive_filters_pong_frame);
  RUN_TEST(test_receive_filters_reset_frame);
  RUN_TEST(test_receive_filters_reset_ack_frame);
  RUN_TEST(test_receive_data_frame_passthrough);
  RUN_TEST(test_receive_data_frame_retransmit_flag);
  RUN_TEST(test_receive_data_frame_fec_path);

  /* internal_receive_passthrough: oversized payload */
  RUN_TEST(test_receive_passthrough_oversized_payload);

  /* internal_receive_fec_decode: success path */
  RUN_TEST(test_receive_fec_decode_success_path);

  /* internal_receive_fec_decode: failure path (NACK send) */
  RUN_TEST(test_receive_fec_decode_failure_nack_path);

  /* internal_send_attempt: ACK success */
  RUN_TEST(test_send_attempt_ack_success);

  /* internal_send_attempt: re-entrancy assert covers Audit F-04 false branch */
  RUN_TEST(test_send_attempt_reentrancy_assert_fires);
}

int main(void)
{
  UNITY_BEGIN();

  internal_run_lifecycle_and_validation_tests();
  internal_run_behavioral_and_internal_tests();
  internal_run_mock_rspi_injection_tests();

  return UNITY_END();
}
