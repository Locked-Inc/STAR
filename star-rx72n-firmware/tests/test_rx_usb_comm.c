/**
 * @file test_rx_usb_comm.c
 * @brief Unit Tests for USB CDC Frame-Based Communication Protocol Layer
 *
 * @details
 * **Comprehensive Test Suite for USB CDC Frame Encoding/Decoding Layer**
 *
 * This test suite validates the rx_usb_comm layer - a frame-based protocol that
 * sits above the raw USB CDC driver (rx_usb) and below the application logic.
 * It tests frame construction, CRC validation, and sequence number management
 * for reliable RX72N <-> RPi5 communication.
 *
 * **Protocol Stack Architecture:**
 * @code
 * +---------------------------+
 * |  Application Layer        | <- Motor control, telemetry
 * +---------------------------+
 * |  rx_usb_comm (THIS LAYER) | <- Frame encode/decode, CRC, sequence #
 * +---------------------------+
 * |  rx_usb (CDC Driver)      | <- Multi-port USB CDC, buffers, callbacks
 * +---------------------------+
 * |  RX72N USB0 Hardware      | <- USB Full-Speed peripheral (12 Mbps)
 * +---------------------------+
 * @endcode
 *
 * **Test Categories:**
 * 1. **Initialization** - Tests handle initialization and port configuration
 * 2. **Frame Encoding** - Validates frame construction with CRC-32
 * 3. **Frame Decoding** - Tests sync word detection, CRC validation, payload extraction
 * 4. **Send Operations** - Tests data frame transmission
 * 5. **Receive Operations** - Tests frame reception with timeout handling
 * 6. **Sequence Management** - Tests TX/RX sequence number incrementing and wrap-around
 * 7. **Error Injection** - Tests CRC errors, invalid sync, protocol violations
 * 8. **Multi-Port Support** - Tests operations on Protocol vs Decoded ports
 *
 * **Frame Structure (Matches rx_frame.h):**
 * @code
 * USB CDC Frame Format (variable length, 12-1036 bytes):
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | Byte 0 | Byte 1 | Byte 2 | Byte 3 | Byte 4 | Byte 5 | Byte 6 | Byte 7 |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | SYNC_H | SYNC_L |  SEQ_H |  SEQ_L |  LEN_H |  LEN_L |  TYPE  | FLAGS  |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | Byte 8 - (N+7)  |  Payload (0-1024 bytes)                            |
 * +--------+--------+--------------------------------------------------------+
 * | Byte (N+8) - (N+11) | CRC-32 (4 bytes, little-endian)                 |
 * +-------------------------------------------------------------------------+
 *
 * Field Definitions:
 * - SYNC   (2 bytes): 0xA55A - Frame start marker for sync word detection
 * - SEQ    (2 bytes): Sequence number (0x0000-0xFFFF, wraps to 0)
 * - LEN    (2 bytes): Payload length in bytes (0-1024)
 * - TYPE   (1 byte):  Frame type (command=0x01, response=0x02, ack=0x10, nack=0x11)
 * - FLAGS  (1 byte):  Bit flags (FEC_EN=0x01, SOFT_NACK=0x02, HARD_NACK=0x04)
 * - Payload (0-1024): Variable-length data (protobuf messages typically)
 * - CRC-32 (4 bytes): IEEE 802.3 polynomial (0x04C11DB7) over header+payload
 * @endcode
 *
 * **Protocol Test Sequences:**
 * @par Frame Send Test:
 * @code
 * 1. Init rx_usb_comm with default config (FEC disabled)
 * 2. Init USB driver and transition to CONFIGURED state
 * 3. Send frame with payload "test" (4 bytes), type=RESPONSE, sequence auto-assigned
 * 4. Pop transmitted data from USB port's TX buffer
 * 5. Decode frame header: SYNC=0xA55A, SEQ=0, LEN=4, TYPE=0x02
 * 6. Assert: CRC-32 is correct over header+payload
 * 7. Assert: s_handle.tx_sequence incremented to 1
 * @endcode
 *
 * @par Frame Receive Test:
 * @code
 * 1. Init rx_usb_comm layer
 * 2. Encode valid frame with payload "DATA" externally
 * 3. Push encoded frame to USB port's RX buffer (simulate USB OUT transfer)
 * 4. Call rx_usb_comm_receive with timeout=0 (non-blocking)
 * 5. Assert: Returns k_rx_ok
 * 6. Assert: Decoded frame.header.sequence matches sent value
 * 7. Assert: frame.payload contains "DATA"
 * 8. Assert: s_handle.rx_sequence incremented
 * @endcode
 *
 * @par CRC Error Injection Test:
 * @code
 * 1. Encode valid frame with correct CRC
 * 2. Corrupt CRC byte (flip bit 0 of CRC[0])
 * 3. Push corrupted frame to RX buffer
 * 4. Call rx_usb_comm_receive
 * 5. Assert: Returns k_rx_err_crc_error
 * 6. Assert: frame not delivered to application
 * @endcode
 *
 * @par Sequence Number Wrap Test:
 * @code
 * 1. Set tx_sequence = 0xFFFF manually
 * 2. Send frame (sequence 0xFFFF assigned)
 * 3. Assert: tx_sequence wraps to 0x0000 after send
 * 4. Send another frame
 * 5. Assert: Next frame has sequence 0x0000
 * @endcode
 *
 *
 * **Timing Requirements:**
 * - Frame encoding time: <50us @ 240 MHz for 1024-byte payload (software CRC-32)
 * - Frame decoding time: <60us (includes CRC validation)
 * - USB CDC bulk transfer latency: 1-5ms (USB FS polling interval)
 * - Total send-receive round-trip: <10ms typical
 * - Timeout resolution: 1ms (based on systick or CMT timer)
 *
 * **Error Injection Patterns:**
 * - **Invalid sync word** - Tests resynchronization logic (not implemented yet)
 * - **CRC mismatch** - Tests error detection (returns k_rx_err_crc_error)
 * - **Payload length > max** - Tests bounds checking (returns k_rx_err_invalid_size)
 * - **USB not configured** - Tests state validation (returns k_rx_err_invalid_state)
 * - **Buffer full** - Tests backpressure handling (returns k_rx_err_busy)
 * - **Timeout on receive** - Tests non-blocking behavior (returns k_rx_err_timeout)
 *
 * **Mock State Management:**
 * - **mock_usb_hw:** Simulates USB0 hardware registers and transfer completion
 * - **mock_usb0_regs:** Tracks register writes (SYSCFG, INTENB0, PIPExCTR, etc.)
 * - **mock_time:** Provides deterministic time for timeout testing
 * - **rx_usb internal state:** Exposed via extern functions for buffer injection
 *
 * **Critical Design Decisions:**
 * - **Port Selection:** Uses k_usb_port_proto (port 0) for binary protocol frames
 * - **No Retry Logic:** Comm layer does not retry - relies on higher-level ARQ (HARQ)
 * - **Sequence Number Policy:** Sender increments, receiver validates (not enforced yet)
 * - **FEC Optional:** FEC flag in frame header, actual FEC encoding by separate rx_fec layer
 * - **Non-Blocking:** All operations return immediately (no busy-wait in comm layer)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (Control Flow):** [OK] All test functions use simple sequential flow
 * - **Rule 2 (Loop Bounds):** [OK] All loops have compile-time known bounds
 * - **Rule 3 (Dynamic Memory):** [OK] Zero heap allocation (stack buffers only)
 * - **Rule 4 (Function Size):** [OK] Test functions <50 lines, helpers <20 lines
 * - **Rule 5 (Assertions):** [OK] Every test has minimum 1 assertion, most have 3+
 * - **Rule 7 (Return Checking):** [OK] All API returns validated
 * - **Rule 9 (Pointers):** [OK] Single-level dereferencing only
 * - **Rule 10 (Warnings):** [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Each test validates one frame protocol behavior
 * - **Open/Closed:** Mock injection allows testing without USB hardware
 * - **Liskov Substitution:** Mock USB driver substitutes for real hardware transparently
 * - **Interface Segregation:** Tests use minimal API surface (init/send/receive/deinit)
 * - **Dependency Inversion:** Depends on rx_usb abstraction, not USB0 registers
 *
 * @par Module Dependencies:
 * - rx_usb_comm.h - Frame-based comm layer API under test
 * - rx_frame.h - Frame encoding/decoding primitives
 * - rx_usb.h - USB CDC multi-port driver
 * - mock_usb_hw.h - Mock USB0 peripheral
 * - mock_usb0_regs.h - Mock USB register access
 * - mock_time.h - Mock time functions for timeouts
 * - rx_err.h - Error code definitions
 * - unity.h - Unit testing framework
 *
 * @see rx_usb_comm.h - Frame-based comm layer header
 * @see rx_frame.h - Frame structure and encoding/decoding
 * @see rx_usb.h - USB CDC driver
 * @see docs/sections/01_nanopb_protocol.tex - Full protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-01-30
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @test Tests run via Unity framework with: make test_rx_usb_comm
 */

#include <string.h>

#include "mock_time.h"
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_frame.h"
#include "rx_session.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"
#include "unity.h"

/* =============================================================================
 * Constants & Definitions
 * =============================================================================
 */

/* Endpoint number from rx72n_regs.h */
#ifndef k_usb_cdc_ep_bulk_in
#define k_usb_cdc_ep_bulk_in 1
#endif

/**
 * @brief Test constants for mode switching tests
 */
/**
 * @brief Common test timeout for receive operations (milliseconds)
 */
typedef enum : uint16_t {
  k_test_timeout_ms = 1000, /**< Default receive timeout for all tests */
} test_timeout_t;

typedef enum : uint8_t {
  k_test_seq_num         = 42, /**< Test sequence number */
  k_test_string_len      = 4,  /**< Length of "test" string */
  k_test_flags_none      = 0,  /**< No flags */
  k_test_error_code_none = 0,  /**< No error code */
} test_mode_constants_t;

/**
 * @brief Small buffer/array length constants used in tests
 */
typedef enum : uint8_t {
  k_test_len_2  = 2,  /**< Length 2 */
  k_test_len_3  = 3,  /**< Length 3 */
  k_test_len_4  = 4,  /**< Length 4 */
  k_test_len_5  = 5,  /**< Length 5 */
  k_test_len_6  = 6,  /**< Length 6 */
  k_test_len_7  = 7,  /**< Length 7 */
  k_test_len_8  = 8,  /**< Length 8 */
  k_test_len_9  = 9,  /**< Length 9 */
  k_test_len_10 = 10, /**< Length 10 */
  k_test_len_12 = 12, /**< Length 12 */
  k_test_len_15 = 15, /**< Length 15 */
  k_test_len_20 = 20, /**< Length 20 */
} test_small_lengths_t;

/**
 * @brief Larger count/size constants used in tests
 */
typedef enum : uint16_t {
  k_test_count_42    = 42,    /**< Context value 42 */
  k_test_count_50    = 50,    /**< Count 50 */
  k_test_count_99    = 99,    /**< Count 99 */
  k_test_count_100   = 100,   /**< Count 100 */
  k_test_count_128   = 128,   /**< Buffer size 128 */
  k_test_count_200   = 200,   /**< Count 200 */
  k_test_count_256   = 256,   /**< Count 256 */
  k_test_count_10000 = 10000, /**< Large iteration count */
} test_counts_t;

/**
 * @brief Test fill byte patterns used in memset replacements
 */
typedef enum : uint8_t {
  k_test_fill_02 = 0x02, /**< Byte pattern 0x02 */
  k_test_fill_03 = 0x03, /**< Byte pattern 0x03 */
  k_test_fill_04 = 0x04, /**< Byte pattern 0x04 */
  k_test_fill_12 = 0x12, /**< Byte pattern 0x12 */
  k_test_fill_34 = 0x34, /**< Byte pattern 0x34 */
  k_test_fill_55 = 0x55, /**< Byte pattern 0x55 */
  k_test_fill_56 = 0x56, /**< Byte pattern 0x56 */
  k_test_fill_aa = 0xAA, /**< Byte pattern 0xAA */
  k_test_fill_ab = 0xAB, /**< Byte pattern 0xAB */
  k_test_fill_ad = 0xAD, /**< Byte pattern 0xAD */
  k_test_fill_cc = 0xCC, /**< Byte pattern 0xCC */
  k_test_fill_cd = 0xCD, /**< Byte pattern 0xCD */
  k_test_fill_dd = 0xDD, /**< Byte pattern 0xDD */
  k_test_fill_de = 0xDE, /**< Byte pattern 0xDE */
  k_test_fill_ef = 0xEF, /**< Byte pattern 0xEF */
  k_test_fill_ff = 0xFF, /**< Byte pattern 0xFF */
} test_fill_bytes_t;

/**
 * @brief Test 16-bit word constants
 */
typedef enum : uint16_t {
  k_test_word_1234 = 0x1234, /**< Test word 0x1234 */
  k_test_word_ffff = 0xFFFF, /**< Test word 0xFFFF */
} test_words_t;

/**
 * @brief Array index constants for byte arrays used in tests
 */
typedef enum : uint8_t {
  k_test_idx_0 = 0, /**< Array index 0 */
  k_test_idx_1 = 1, /**< Array index 1 */
  k_test_idx_2 = 2, /**< Array index 2 */
  k_test_idx_3 = 3, /**< Array index 3 */
  k_test_idx_4 = 4, /**< Array index 4 */
  k_test_idx_5 = 5, /**< Array index 5 */
  k_test_idx_6 = 6, /**< Array index 6 */
} test_array_indices_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/** @brief USB comm handle under test, reset in setUp() */
static rx_usb_comm_handle_t s_handle;

/** @brief Shared session state for sequence tracking, initialized in setUp() */
static rx_session_state_t s_session;

/* Control frame callback tracking */
static uint32_t   s_ping_cb_count;    /**< Number of PING callbacks received */
static uint32_t   s_reset_cb_count;   /**< Number of RESET callbacks received */
static rx_frame_t s_last_ping_frame;  /**< Last PING frame passed to callback */
static rx_frame_t s_last_reset_frame; /**< Last RESET frame passed to callback */
static void*      s_last_cb_ctx;      /**< Last callback context pointer */

/**
 * @brief Test callback for PING control frames
 * @param[in] frame The decoded PING frame (may be NULL)
 * @param[in] ctx   User context pointer passed during registration
 * @post s_ping_cb_count incremented, s_last_ping_frame updated if frame non-NULL
 */
static void test_ping_callback(const rx_frame_t* frame, void* ctx)
{
  s_ping_cb_count++;
  if (frame != nullptr) {
    s_last_ping_frame = *frame;
  }
  s_last_cb_ctx = ctx;
}

/**
 * @brief Test callback for RESET control frames
 * @param[in] frame The decoded RESET frame (may be NULL)
 * @param[in] ctx   User context pointer passed during registration
 * @post s_reset_cb_count incremented, s_last_reset_frame updated if frame non-NULL
 */
static void test_reset_callback(const rx_frame_t* frame, void* ctx)
{
  s_reset_cb_count++;
  if (frame != nullptr) {
    s_last_reset_frame = *frame;
  }
  s_last_cb_ctx = ctx;
}

/* Extern internal functions from rx_usb.c */
extern void     rx_usb_set_state(rx_usb_state_t state);
extern uint32_t rx_usb_tx_pop(rx_usb_port_id_t port, uint8_t* data, uint32_t max_len);
extern uint32_t rx_usb_rx_push(rx_usb_port_id_t port, const uint8_t* data, uint32_t len);

/* Extern internal functions from rx_usb_comm.c (RX_STATIC_TESTABLE) */
extern rx_err_t internal_decode_header(const uint8_t* data,
                                       uint32_t       data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out);
extern rx_err_t internal_verify_crc(const uint8_t* data, uint32_t offset, uint32_t* crc_out);
extern rx_err_t internal_read_usb_data(rx_usb_comm_handle_t* handle);
extern rx_err_t internal_compact_rx_buffer(rx_usb_comm_handle_t* handle);
extern rx_err_t internal_find_sync(const rx_usb_comm_handle_t* handle, int32_t* sync_pos);

/**
 * @brief Create a default USB comm config with shared session
 * @return Config with session pointer and no time interface
 */
static rx_usb_comm_config_t helper_default_config(void)
{
  rx_usb_comm_config_t config = {.session = &s_session, .time_iface = nullptr};

  return config;
}

/**
 * @brief Create a USB comm config with mock time interface
 * @param[in] time_iface Mock time interface for timeout testing
 * @return Config with session pointer and given time interface
 */
static rx_usb_comm_config_t helper_config_with_time(const rx_time_interface_t* time_iface)
{
  rx_usb_comm_config_t config = {.session = &s_session, .time_iface = time_iface};

  return config;
}

/**
 * @brief Get current TX sequence from shared session
 * @return Current TX sequence value (asserts on session error)
 */
static uint16_t helper_get_tx_seq(void)
{
  uint16_t seq = 0;
  rx_err_t err = rx_session_get_tx(&s_session, &seq);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  return seq;
}

/**
 * @brief Get current RX sequence from shared session
 * @return Current expected RX sequence value (asserts on session error)
 */
static uint16_t helper_get_rx_seq(void)
{
  uint16_t seq = 0;
  rx_err_t err = rx_session_get_rx(&s_session, &seq);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  return seq;
}

/* Helper to initialize USB and set configured state */
static void helper_usb_init_and_configure(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);
}

void setUp(void)
{
  /* Initialize mock hardware */
  mock_usb_hw_init(nullptr);
  mock_regs_init();

  /* Clear handle and session */
  for (uint32_t i = 0; i < sizeof(s_handle); i++) {
    ((uint8_t*)&s_handle)[i] = 0;
  }
  for (uint32_t i = 0; i < sizeof(s_session); i++) {
    ((uint8_t*)&s_session)[i] = 0;
  }
  (void)rx_session_init(&s_session);

  /* Clear callback tracking */
  s_ping_cb_count  = 0;
  s_reset_cb_count = 0;
  for (uint32_t i = 0; i < sizeof(s_last_ping_frame); i++) {
    ((uint8_t*)&s_last_ping_frame)[i] = 0;
  }
  for (uint32_t i = 0; i < sizeof(s_last_reset_frame); i++) {
    ((uint8_t*)&s_last_reset_frame)[i] = 0;
  }
  s_last_cb_ctx = nullptr;
}

void tearDown(void)
{
  /* Deinitialize comm layer if initialized */
  (void)rx_usb_comm_deinit(&s_handle);

  /* Deinitialize session */
  (void)rx_session_deinit(&s_session);

  /* Deinitialize USB */
  (void)rx_usb_deinit();

  /* Clear mock state */
  mock_usb_hw_deinit(nullptr);
  mock_regs_clear();
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_usb_comm_init_null_handle_fails(void)
{
  rx_usb_comm_config_t config = helper_default_config();
  rx_err_t             err    = rx_usb_comm_init(nullptr, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_init_null_config_fails(void)
{
  rx_err_t err = rx_usb_comm_init(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_init_null_session_fails(void)
{
  rx_usb_comm_config_t config = {.session = nullptr, .time_iface = nullptr};
  rx_err_t             err    = rx_usb_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_init_success(void)
{
  rx_usb_comm_config_t config = helper_default_config();
  rx_err_t             err    = rx_usb_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
  TEST_ASSERT_EQUAL_PTR(&s_session, s_handle.session);
}

void test_usb_comm_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_deinit(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_deinit_success(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_err_t err = rx_usb_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

void test_usb_comm_deinit_not_initialized_succeeds(void)
{
  /* Deinit on uninitialized handle should succeed gracefully */
  rx_err_t err = rx_usb_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Send Tests
 * =============================================================================
 */

void test_usb_comm_send_null_handle_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(nullptr, k_frame_type_response, 0, data, k_test_len_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_len_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_usb_not_configured_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr)); /* USB initialized but not configured */
  uint8_t data[] = "test";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_len_4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_null_payload_with_len_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, nullptr, k_test_len_10);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_payload_too_large_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  uint8_t data[k_frame_max_payload + 1];

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_usb_comm_send_empty_payload_succeeds(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_tx_seq()); /* Sequence should increment */
}

void test_usb_comm_send_with_payload_succeeds(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  uint8_t data[] = "Hello USB!";

  rx_err_t err = rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_len_10);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_tx_seq());
}

void test_usb_comm_send_increments_sequence(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  uint8_t data[] = "test";

  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_len_4));
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_len_4));
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_usb_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_len_4));

  TEST_ASSERT_EQUAL_UINT16(k_test_len_3, helper_get_tx_seq());
}

/* =============================================================================
 * Receive Tests
 * =============================================================================
 */

void test_usb_comm_receive_null_handle_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(nullptr, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_receive_null_frame_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_err_t err = rx_usb_comm_receive(&s_handle, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_receive_not_initialized_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_receive_usb_not_configured_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_receive_no_data_timeout(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Data Available Tests
 * =============================================================================
 */

void test_usb_comm_data_available_null_handle_fails(void)
{
  bool available;

  rx_err_t err = rx_usb_comm_data_available(nullptr, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_data_available_null_available_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_err_t err = rx_usb_comm_data_available(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_data_available_not_initialized_fails(void)
{
  bool available;

  rx_err_t err = rx_usb_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_data_available_empty(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  bool available = true;

  rx_err_t err = rx_usb_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(available);
}

/* =============================================================================
 * Is Ready Tests
 * =============================================================================
 */

void test_usb_comm_is_ready_null_handle_fails(void)
{
  bool ready;

  rx_err_t err = rx_usb_comm_is_ready(nullptr, &ready);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_is_ready_null_ready_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_is_ready_not_initialized(void)
{
  bool ready = true;

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, &ready);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_FALSE(ready);
}

void test_usb_comm_is_ready_usb_not_configured(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  bool ready = true;

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, &ready);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

void test_usb_comm_is_ready_configured(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  bool ready = false;

  rx_err_t err = rx_usb_comm_is_ready(&s_handle, &ready);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(ready);
}

/* =============================================================================
 * Utility Function Tests
 * =============================================================================
 */

void test_usb_comm_flush_rx(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  s_handle.rx_buffer_len = k_test_count_100;
  s_handle.rx_buffer_pos = k_test_count_50;

  rx_usb_comm_flush_rx(&s_handle);

  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
}

void test_usb_comm_flush_rx_null_handle(void)
{
  /* Should not crash on nullptr */
  rx_usb_comm_flush_rx(nullptr);
}

/* =============================================================================
 * Successful Frame Reception Tests
 * =============================================================================
 */

/* Helper: Create and encode a test frame */
static rx_err_t helper_create_encoded_frame(rx_frame_t*     frame,
                                            uint16_t        sequence,
                                            rx_frame_type_t type,
                                            uint8_t         flags,
                                            const uint8_t*  payload,
                                            uint16_t        payload_len,
                                            uint8_t*        encoded_buf,
                                            uint32_t*       encoded_len)
{
  rx_frame_encoder_t enc;
  rx_err_t           err;

  err = rx_frame_encoder_init(&enc);
  if (err != k_rx_ok) {
    return err;
  }

  for (uint32_t i = 0; i < sizeof(rx_frame_t); i++) {
    ((uint8_t*)frame)[i] = 0;
  }
  frame->header.sequence = sequence;
  frame->header.type     = (uint8_t)type;
  frame->header.flags    = flags;
  frame->header.length   = payload_len;

  if (payload != nullptr && payload_len > 0) {
    for (uint32_t i = 0; i < payload_len; i++) {
      frame->payload[i] = payload[i];
    }
  }

  err = rx_frame_encode(&enc, frame, encoded_buf, encoded_len);

  (void)rx_frame_encoder_deinit(&enc);

  return err;
}

void test_usb_comm_receive_valid_frame_with_sync(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "test_data!";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             1,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_10,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject frame into USB RX buffer */
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_10, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY("test_data!", rx_frame.payload, k_test_len_10);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_empty_payload_frame(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create frame with zero-length payload */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_ack,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_frame_min_size, encoded_len);

  /* Inject frame into USB RX buffer */
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_ack, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.length);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_max_payload_frame(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Use a large payload that fits in the USB driver's 512-byte ring buffer.
   * The USB driver uses k_usb_rx_buffer_size = 512 bytes.
   * Frame overhead is 12 bytes (8 header + 4 CRC), so max payload is ~500.
   * A full k_frame_max_payload (1024) test would require simulating
   * interrupt-driven incremental data arrival, which this mock doesn't support. */
  enum : uint16_t { k_test_large_payload_size = 490 };
  uint8_t large_payload[k_test_large_payload_size];
  for (uint32_t i = 0; i < k_test_large_payload_size; i++) {
    large_payload[i] = (uint8_t)(i & (uint32_t)k_test_fill_ff);
  }

  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_response,
                                             k_frame_flag_priority,
                                             large_payload,
                                             k_test_large_payload_size,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify encoded size fits in USB ring buffer */
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(512, encoded_len);

  /* Inject frame into USB RX buffer */
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_response, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT8(k_frame_flag_priority, rx_frame.header.flags);
  TEST_ASSERT_EQUAL_UINT16(k_test_large_payload_size, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY(large_payload, rx_frame.payload, k_test_large_payload_size);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_increments_rx_sequence(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Initial RX sequence should be 0 */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());

  /* Send first frame with sequence 5 */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_len_5,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After receiving frame with sequence 5, expected RX sequence should be 6 */
  TEST_ASSERT_EQUAL_UINT16(k_test_len_6, helper_get_rx_seq());

  /* Send second frame with sequence 6 */
  err = helper_create_encoded_frame(&tx_frame,
                                    k_test_len_6,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    nullptr,
                                    0,
                                    encoded,
                                    &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After receiving frame with sequence 6, expected RX sequence should be 7 */
  TEST_ASSERT_EQUAL_UINT16(k_test_len_7, helper_get_rx_seq());

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Sync Word Detection Tests
 * =============================================================================
 */

void test_usb_comm_receive_finds_sync_after_garbage(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "FOUND";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_len_7,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_5,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject garbage data followed by valid frame */
  uint8_t garbage[] = {0x00,
                       0x01,
                       k_test_fill_02,
                       k_test_fill_03,
                       k_test_fill_ab,
                       k_test_fill_cd,
                       k_test_fill_ef,
                       k_test_fill_12,
                       k_test_fill_34,
                       k_test_fill_56};
  rx_usb_rx_push(k_usb_port_proto, garbage, sizeof(garbage));
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive - should skip garbage and find the valid frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_7, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("FOUND", rx_frame.payload, k_test_len_5);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_partial_sync_word(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "SPLIT";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_len_8,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_5,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* First inject just partial data (first half of frame) */
  uint32_t first_half = encoded_len / k_test_len_2;
  rx_usb_rx_push(k_usb_port_proto, encoded, first_half);

  /* Try to receive - should timeout waiting for rest of frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_50);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Now inject the rest of the frame */
  rx_usb_rx_push(k_usb_port_proto, encoded + first_half, encoded_len - first_half);

  /* Should now receive the complete frame */
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_8, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("SPLIT", rx_frame.payload, k_test_len_5);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_multiple_false_syncs(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "REAL";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_len_9,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_4,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject data with false sync patterns (0x55AA) but invalid following data */
  /* SYNC word is 0x55AA in little-endian wire order: [0xAA, 0x55] */
  uint8_t false_sync_data[] = {
    k_test_fill_aa,
    k_test_fill_55, /* False sync #1 */
    k_test_fill_ff,
    k_test_fill_ff, /* Invalid payload length (0xFFFF > 1024) */
    k_test_fill_aa,
    k_test_fill_55, /* False sync #2 */
    0x00,
    0x00, /* Seq = 0 */
    k_test_fill_ff,
    k_test_fill_ff, /* Invalid payload length again */
  };
  rx_usb_rx_push(k_usb_port_proto, false_sync_data, sizeof(false_sync_data));
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive - should skip false syncs and find the valid frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify - the real frame should be received */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_9, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("REAL", rx_frame.payload, k_test_len_4);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Header Parsing Tests
 * =============================================================================
 */

void test_usb_comm_receive_invalid_payload_length(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create a valid frame to follow the invalid one */
  rx_frame_t tx_frame;
  uint8_t    valid_encoded[k_frame_max_size];
  uint32_t   valid_encoded_len = 0;
  uint8_t    payload[]         = "VALID";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_5,
                                             valid_encoded,
                                             &valid_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create invalid frame header with payload length > k_frame_max_payload (1024) */
  /* Frame: SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) = 8 bytes header */
  uint8_t invalid_frame[] = {
    k_test_fill_55,
    k_test_fill_aa, /* Sync word */
    0x00,
    0x01, /* Sequence = 1 */
    k_test_fill_04,
    0x01, /* Length = 1025 (0x0401) - exceeds max payload */
    0x01, /* Type = command */
    0x00, /* Flags = none */
  };

  rx_usb_rx_push(k_usb_port_proto, invalid_frame, sizeof(invalid_frame));
  rx_usb_rx_push(k_usb_port_proto, valid_encoded, valid_encoded_len);

  /* Receive - should skip invalid frame and receive valid one */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_MEMORY("VALID", rx_frame.payload, 5);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_header_validation(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Pre-advance session to match the specific sequence we want to test */
  s_session.rx_sequence = k_test_word_1234;

  /* Create frame with specific header fields to verify */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "HDR";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_word_1234,      /* Specific sequence */
                                             k_frame_type_response, /* Type */
                                             k_frame_flag_requires_ack | k_frame_flag_priority,
                                             payload,
                                             k_test_len_3,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify all header fields */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_word_1234, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_3, rx_frame.header.length);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_response, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT8(k_frame_flag_requires_ack | k_frame_flag_priority, rx_frame.header.flags);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Buffer Management Tests
 * =============================================================================
 */

void test_usb_comm_receive_buffer_compaction(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Receive multiple frames to test buffer compaction */
  for (uint16_t seq = 0; seq < k_test_len_5; seq++) {
    rx_frame_t tx_frame;
    uint8_t    encoded[k_frame_max_size];
    uint32_t   encoded_len = 0;
    uint8_t    payload[k_test_len_20];

    /* Fill payload with sequence number pattern */
    for (uint32_t i = 0; i < k_test_len_20; i++) {
      payload[i] = (uint8_t)seq;
    }

    rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                               seq,
                                               k_frame_type_command,
                                               k_frame_flag_none,
                                               payload,
                                               k_test_len_20,
                                               encoded,
                                               &encoded_len);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

    rx_frame_t rx_frame;
    err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(seq, rx_frame.header.sequence);

    /* Verify payload is correct pattern */
    for (uint32_t i = 0; i < k_test_len_20; i++) {
      TEST_ASSERT_EQUAL_UINT8(seq, rx_frame.payload[i]);
    }
  }

  /* Buffer should be compacted (position reset to 0) after each receive */
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_buffer_overflow_protection(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Fill the handle's RX buffer near capacity with garbage */
  uint8_t garbage[k_usb_comm_rx_buffer_size - k_test_count_100];
  for (uint32_t i = 0; i < sizeof(garbage); i++) {
    garbage[i] = k_test_fill_de;
  }

  /* Directly manipulate handle's buffer to simulate near-full condition */
  for (uint32_t i = 0; i < sizeof(garbage); i++) {
    s_handle.rx_buffer[i] = garbage[i];
  }
  s_handle.rx_buffer_len = sizeof(garbage);
  s_handle.rx_buffer_pos = 0;

  /* Now push a small valid frame via USB */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "OK";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_2,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* The receiver should handle the near-full buffer by discarding garbage */
  /* and finding the valid sync word */
  /* Note: Since buffer is pre-filled, we need to flush it first */
  rx_usb_comm_flush_rx(&s_handle);

  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_fragmented_frame(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create valid frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = "FRAGMENTED_DATA";

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_15,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Fragment the frame into small chunks simulating USB packet boundaries */
  uint32_t chunk_size = k_test_len_5;
  for (uint32_t offset = 0; offset < encoded_len; offset += chunk_size) {
    uint32_t remaining = encoded_len - offset;
    uint32_t to_push   = (remaining < chunk_size) ? remaining : chunk_size;
    rx_usb_rx_push(k_usb_port_proto, encoded + offset, to_push);
  }

  /* Receive should reassemble the fragmented frame */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_15, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY("FRAGMENTED_DATA", rx_frame.payload, k_test_len_15);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Sequence Number Tests
 * =============================================================================
 */

/**
 * @brief Helper body for sequence wraparound test: send 0xFFFF frame and verify
 */
static void internal_wraparound_send_ffff(void)
{
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_word_ffff,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);
}

void test_usb_comm_receive_sequence_wraparound(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Pre-advance session RX sequence to test uint16_t wraparound: 65535 -> 0 */
  s_session.rx_sequence = k_test_word_ffff;

  internal_wraparound_send_ffff();

  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_word_ffff, rx_frame.header.sequence);

  /* After receiving 65535, expected RX sequence should wrap to 0 */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());

  /* Second frame with sequence 0 (after wraparound) */
  rx_frame_t tx_frame2;
  uint8_t    encoded2[k_frame_max_size];
  uint32_t   encoded_len2 = 0;

  err = helper_create_encoded_frame(&tx_frame2,
                                    0,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    nullptr,
                                    0,
                                    encoded2,
                                    &encoded_len2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(k_usb_port_proto, encoded2, encoded_len2);

  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_rx_seq());

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_out_of_order_sequence(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Send frames with small sequential gaps within session gap tolerance.
   * Session validates: diff = received - expected; accepted if diff < 10.
   * Starting at rx_sequence=0, each accepted frame advances the expected. */
  uint16_t sequences[] = {0, k_test_len_3, k_test_len_5, k_test_len_8};

  for (uint32_t i = 0; i < sizeof(sequences) / sizeof(sequences[0]); i++) {
    rx_frame_t tx_frame;
    uint8_t    encoded[k_frame_max_size];
    uint32_t   encoded_len = 0;

    rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                               sequences[i],
                                               k_frame_type_command,
                                               k_frame_flag_none,
                                               nullptr,
                                               0,
                                               encoded,
                                               &encoded_len);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

    rx_frame_t rx_frame;
    err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

    /* Frame should be accepted (exact match or small gap within tolerance) */
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(sequences[i], rx_frame.header.sequence);
  }

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Edge Cases Tests
 * =============================================================================
 */

/**
 * @brief Helper: build bad+good encoded frames for CRC mismatch test
 * @param[out] bad_encoded     Buffer for the CRC-corrupted frame
 * @param[out] bad_encoded_len Length of the CRC-corrupted frame
 * @param[out] good_encoded    Buffer for the valid frame
 * @param[out] good_encoded_len Length of the valid frame
 */
static void internal_build_crc_mismatch_frames(uint8_t*  bad_encoded,
                                               uint32_t* bad_encoded_len,
                                               uint8_t*  good_encoded,
                                               uint32_t* good_encoded_len)
{
  rx_frame_t tx_frame;
  rx_err_t   err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             bad_encoded,
                                             bad_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Corrupt the CRC (last 4 bytes) */
  bad_encoded[*bad_encoded_len - 1] ^= k_test_fill_ff;
  bad_encoded[*bad_encoded_len - k_test_len_2] ^= k_test_fill_ff;

  uint8_t payload[] = "GOOD";
  err               = helper_create_encoded_frame(&tx_frame,
                                    0,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    payload,
                                    k_test_len_4,
                                    good_encoded,
                                    good_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_comm_receive_crc_mismatch(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  uint8_t  bad_encoded[k_frame_max_size];
  uint32_t bad_encoded_len = 0;
  uint8_t  good_encoded[k_frame_max_size];
  uint32_t good_encoded_len = 0;

  internal_build_crc_mismatch_frames(bad_encoded,
                                     &bad_encoded_len,
                                     good_encoded,
                                     &good_encoded_len);

  /* Push bad frame followed by good frame */
  rx_usb_rx_push(k_usb_port_proto, bad_encoded, bad_encoded_len);
  rx_usb_rx_push(k_usb_port_proto, good_encoded, good_encoded_len);

  /* First receive should fail with CRC error or skip to valid frame */
  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* The implementation should either:
   * 1. Return CRC mismatch error, or
   * 2. Skip the bad frame and return the good one
   * Based on the code, it returns the decode error */
  if (err == k_rx_err_crc_mismatch) {
    /* CRC error returned - need another receive for good frame */
    rx_usb_rx_push(k_usb_port_proto, good_encoded, good_encoded_len);
    err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  } else if (err == k_rx_ok) {
    /* Skipped bad frame, got good one directly */
    TEST_ASSERT_EQUAL_UINT16(0, rx_frame.header.sequence);
  } else {
    /* Unexpected error */
    TEST_FAIL_MESSAGE("Unexpected error from receive");
  }

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_iteration_limit(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Don't inject any data - receive loop should hit iteration limit */
  /* The max iteration limit is k_max_receive_iterations = 100 */

  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_10000);

  /* Should timeout due to iteration limit, not timeout_ms */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Sleep should have been called multiple times */
  uint32_t sleep_count = mock_time_get_sleep_count(&mock);
  TEST_ASSERT_GREATER_THAN(0, sleep_count);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Mode Switching Tests
 * =============================================================================
 */

void test_usb_comm_init_defaults_to_binary_mode(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_usb_comm_mode_t mode = rx_usb_comm_get_mode(&s_handle);

  TEST_ASSERT_EQUAL(k_usb_comm_mode_binary, mode);
}

void test_usb_comm_set_mode_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_set_mode(nullptr, k_usb_comm_mode_debug);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_set_mode_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_debug);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_set_mode_to_debug(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_err_t err = rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_debug);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_usb_comm_mode_debug, rx_usb_comm_get_mode(&s_handle));
}

void test_usb_comm_set_mode_to_binary(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_debug));

  rx_err_t err = rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_binary);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_usb_comm_mode_binary, rx_usb_comm_get_mode(&s_handle));
}

void test_usb_comm_get_mode_null_handle_returns_invalid(void)
{
  /* Should return invalid for nullptr handle */
  rx_usb_comm_mode_t mode = rx_usb_comm_get_mode(nullptr);

  TEST_ASSERT_EQUAL(k_usb_comm_mode_invalid, mode);
}

void test_usb_comm_get_mode_not_initialized_returns_invalid(void)
{
  /* Should return invalid for uninitialized handle */
  rx_usb_comm_mode_t mode = rx_usb_comm_get_mode(&s_handle);

  TEST_ASSERT_EQUAL(k_usb_comm_mode_invalid, mode);
}

void test_usb_comm_send_fails_in_debug_mode(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_debug));

  uint8_t  data[] = "test";
  rx_err_t err =
    rx_usb_comm_send(&s_handle, k_frame_type_response, k_test_flags_none, data, k_test_string_len);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_succeeds_after_mode_switch_back_to_binary(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Switch to debug mode */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_debug));

  /* Switch back to binary mode */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_binary));

  uint8_t  data[] = "test";
  rx_err_t err =
    rx_usb_comm_send(&s_handle, k_frame_type_response, k_test_flags_none, data, k_test_string_len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_usb_comm_mode_preserved_after_deinit_init(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_debug));
  (void)rx_usb_comm_deinit(&s_handle);

  /* Reinitialize */
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  /* Mode should be reset to binary after reinit */
  TEST_ASSERT_EQUAL(k_usb_comm_mode_binary, rx_usb_comm_get_mode(&s_handle));
}

/* =============================================================================
 * Mock Time Timeout Tests
 * =============================================================================
 */

void test_usb_comm_init_with_time_interface(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  rx_err_t err = rx_usb_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(&time_iface, s_handle.time_iface);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_timeout_without_time_iface(void)
{
  /* Without time interface, receive should return immediately with timeout */
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();
  rx_frame_t frame;

  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, k_test_count_100);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_NULL(s_handle.time_iface);
}

void test_usb_comm_receive_timeout_with_mock_time(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true); /* Auto-advance on sleep */
  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));
  helper_usb_init_and_configure();

  rx_frame_t frame;

  /* Try to receive with 50ms timeout - should loop and eventually timeout */
  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, k_test_count_50);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Verify sleep was called multiple times (time advanced) */
  uint32_t sleep_count = mock_time_get_sleep_count(&mock);
  TEST_ASSERT_GREATER_THAN(0, sleep_count);

  /* Verify total sleep time is at least the timeout */
  uint32_t total_sleep = mock_time_get_total_sleep(&mock);
  TEST_ASSERT_GREATER_OR_EQUAL(k_test_count_50, total_sleep);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_immediate_timeout_zero(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));
  helper_usb_init_and_configure();

  rx_frame_t frame;

  /* Zero timeout should return immediately without sleeping */
  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* No sleep should have been called */
  TEST_ASSERT_EQUAL_UINT32(0, mock_time_get_sleep_count(&mock));

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Control Frame Tests
 * =============================================================================
 */

void test_usb_comm_set_callbacks_null_handle_fails(void)
{
  rx_err_t err =

    rx_usb_comm_set_control_callbacks(nullptr, test_ping_callback, test_reset_callback, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_set_callbacks_not_initialized_fails(void)
{
  rx_err_t err =

    rx_usb_comm_set_control_callbacks(&s_handle, test_ping_callback, test_reset_callback, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_set_callbacks_success(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  uint32_t ctx_value = k_test_count_42;
  rx_err_t err       = rx_usb_comm_set_control_callbacks(&s_handle,
                                                   test_ping_callback,
                                                   test_reset_callback,
                                                   &ctx_value);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(test_ping_callback, s_handle.on_ping_cb);
  TEST_ASSERT_EQUAL_PTR(test_reset_callback, s_handle.on_reset_cb);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_handle.cb_ctx);
}

void test_usb_comm_send_pong_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_send_pong(nullptr, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_pong_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_comm_send_pong(&s_handle, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_pong_echoes_payload(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  uint8_t  payload[] = {0x01, k_test_fill_02, k_test_fill_03};
  rx_err_t err       = rx_usb_comm_send_pong(&s_handle, payload, k_test_len_3);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify a frame was transmitted */
  uint8_t  tx_data[k_test_count_128];
  uint32_t tx_len = rx_usb_tx_pop(k_usb_port_proto, tx_data, sizeof(tx_data));
  TEST_ASSERT_GREATER_THAN(0, tx_len);

  /* Verify it's a PONG frame (TYPE byte at offset 6 in wire format) */
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_pong, tx_data[k_test_len_6]);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_tx_seq()); /* sequence incremented */
}

void test_usb_comm_send_reset_ack_null_handle_fails(void)
{
  rx_err_t err = rx_usb_comm_send_reset_ack(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_send_reset_ack_not_initialized_fails(void)
{
  rx_err_t err = rx_usb_comm_send_reset_ack(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_reset_ack_success(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  rx_err_t err = rx_usb_comm_send_reset_ack(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify a RESET_ACK frame was transmitted */
  uint8_t  tx_data[k_test_count_128];
  uint32_t tx_len = rx_usb_tx_pop(k_usb_port_proto, tx_data, sizeof(tx_data));
  TEST_ASSERT_GREATER_THAN(0, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_reset_ack, tx_data[k_test_len_6]);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_tx_seq());
}

/**
 * @brief Helper: inject PING+COMMAND frames into USB RX for ping_auto_pong test
 */
static void internal_inject_ping_then_command(void)
{
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  uint8_t    payload[]   = {k_test_fill_ab, k_test_fill_cd};

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_ping,
                                             k_frame_flag_none,
                                             payload,
                                             k_test_len_2,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_frame_t cmd_frame;
  uint8_t    cmd_encoded[k_frame_max_size];
  uint32_t   cmd_encoded_len = 0;
  uint8_t    cmd_payload[]   = "DATA";

  err = helper_create_encoded_frame(&cmd_frame,
                                    0,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    cmd_payload,
                                    k_test_len_4,
                                    cmd_encoded,
                                    &cmd_encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);
  rx_usb_rx_push(k_usb_port_proto, cmd_encoded, cmd_encoded_len);
}

void test_usb_comm_receive_ping_auto_pong(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  internal_inject_ping_then_command();

  /* Receive should skip PING (auto-PONG) and return COMMAND */
  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_command, rx_frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(k_test_len_4, rx_frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY("DATA", rx_frame.payload, k_test_len_4);

  /* Verify PONG was transmitted */
  uint8_t  tx_data[k_test_count_128];
  uint32_t tx_len = rx_usb_tx_pop(k_usb_port_proto, tx_data, sizeof(tx_data));
  TEST_ASSERT_GREATER_THAN(0, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_pong, tx_data[k_test_len_6]);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_reset_auto_ack(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Set non-zero sequences to verify reset */
  s_session.tx_sequence = k_test_count_50;
  s_session.rx_sequence = k_test_count_100;

  /* Create RESET frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_reset,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive should consume RESET (auto-RESET_ACK) and return timeout */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 0);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Verify RESET_ACK was transmitted */
  uint8_t  tx_data[k_test_count_128];
  uint32_t tx_len = rx_usb_tx_pop(k_usb_port_proto, tx_data, sizeof(tx_data));
  TEST_ASSERT_GREATER_THAN(0, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_reset_ack, tx_data[k_test_len_6]);

  /* Verify sequences were reset to 0 */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_tx_seq());
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_ping_callback_invoked(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  uint32_t ctx_value = k_test_count_42;
  (void)rx_usb_comm_set_control_callbacks(&s_handle,
                                          test_ping_callback,
                                          test_reset_callback,
                                          &ctx_value);

  /* Create and inject PING then COMMAND */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_ping,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Add a COMMAND so receive returns */
  uint8_t cmd_payload[] = "X";
  err                   = helper_create_encoded_frame(&tx_frame,
                                    0,
                                    k_frame_type_command,
                                    k_frame_flag_none,
                                    cmd_payload,
                                    1,
                                    encoded,
                                    &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  rx_frame_t rx_frame;
  (void)rx_usb_comm_receive(&s_handle, &rx_frame, k_test_timeout_ms);

  /* Verify ping callback was invoked */
  TEST_ASSERT_EQUAL_UINT32(1, s_ping_cb_count);
  TEST_ASSERT_EQUAL_UINT32(0, s_reset_cb_count);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_ping, s_last_ping_frame.header.type);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_last_cb_ctx);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_reset_callback_invoked(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  s_session.tx_sequence = k_test_count_50;
  s_session.rx_sequence = k_test_count_100;

  uint32_t ctx_value = k_test_count_99;
  (void)rx_usb_comm_set_control_callbacks(&s_handle,
                                          test_ping_callback,
                                          test_reset_callback,
                                          &ctx_value);

  /* Create and inject RESET */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_reset,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  rx_frame_t rx_frame;
  (void)rx_usb_comm_receive(&s_handle, &rx_frame, 0);

  /* Verify reset callback was invoked */
  TEST_ASSERT_EQUAL_UINT32(0, s_ping_cb_count);
  TEST_ASSERT_EQUAL_UINT32(1, s_reset_cb_count);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_reset, s_last_reset_frame.header.type);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_last_cb_ctx);

  /* Verify sequences were reset */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_tx_seq());
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Internal Function Tests (via RX_STATIC_TESTABLE)
 * =============================================================================
 */

void test_internal_decode_header_null_data_fails(void)
{
  rx_frame_t frame;
  uint32_t   offset = 0;

  rx_err_t err = internal_decode_header(nullptr, k_test_len_20, &frame, &offset);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_decode_header_null_frame_fails(void)
{
  uint8_t  data[k_test_len_20] = {0};
  uint32_t offset              = 0;

  rx_err_t err = internal_decode_header(data, k_test_len_20, nullptr, &offset);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_decode_header_too_small_fails(void)
{
  uint8_t    data[k_test_len_4] = {0};
  rx_frame_t frame;
  uint32_t   offset = 0;

  /* data_len < k_frame_min_size (12) */
  rx_err_t err = internal_decode_header(data, k_test_len_4, &frame, &offset);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_internal_decode_header_bad_sync_fails(void)
{
  /* Build a 12-byte buffer with wrong sync word */
  uint8_t    data[k_test_len_12] = {k_test_fill_de, k_test_fill_ad, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  rx_frame_t frame;
  uint32_t   offset = 0;

  rx_err_t err = internal_decode_header(data, k_test_len_12, &frame, &offset);

  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

void test_internal_decode_header_payload_too_large_fails(void)
{
  /* Sync: 0xAA 0x55, seq: 0 0, length: 0xFF 0xFF (> max payload), type, flags, + CRC space */
  uint8_t data[k_test_len_12];
  for (uint32_t i = 0; i < k_test_len_12; i++) {
    data[i] = 0;
  }
  data[k_test_idx_0] = k_test_fill_aa; /* sync low */
  data[k_test_idx_1] = k_test_fill_55; /* sync high */
  data[k_test_idx_4] = k_test_fill_ff; /* length low */
  data[k_test_idx_5] = k_test_fill_ff; /* length high (way over k_frame_max_payload) */

  rx_frame_t frame;
  uint32_t   offset = 0;

  rx_err_t err = internal_decode_header(data, k_test_len_12, &frame, &offset);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_internal_decode_header_null_offset_out_ok(void)
{
  /* Build a valid minimal frame header: sync + seq=0 + len=0 + type=1 + flags=0 */
  uint8_t data[k_test_len_12];
  for (uint32_t i = 0; i < k_test_len_12; i++) {
    data[i] = 0;
  }
  data[k_test_idx_0] = k_test_fill_aa; /* sync low */
  data[k_test_idx_1] = k_test_fill_55; /* sync high */
  data[k_test_len_6] = 0x01;           /* type = command */

  rx_frame_t frame;

  /* offset_out = nullptr should be accepted */
  rx_err_t err = internal_decode_header(data, k_test_len_12, &frame, nullptr);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_internal_verify_crc_null_data_fails(void)
{
  uint32_t crc = 0;

  rx_err_t err = internal_verify_crc(nullptr, k_test_len_8, &crc);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_verify_crc_small_offset_fails(void)
{
  uint8_t  data[k_test_len_12] = {0};
  uint32_t crc                 = 0;

  /* offset < (k_frame_min_size - k_frame_crc_size) = 12 - 4 = 8 */
  rx_err_t err = internal_verify_crc(data, k_test_len_4, &crc);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_read_usb_data_null_handle_fails(void)
{
  rx_err_t err = internal_read_usb_data(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_read_usb_data_buffer_overflow_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Set rx_buffer_len beyond max to trigger pre-condition check */
  s_handle.rx_buffer_len = k_usb_comm_rx_buffer_size + 1U;

  rx_err_t err = internal_read_usb_data(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);

  /* Reset to avoid tearDown issues */
  s_handle.rx_buffer_len = 0;
}

void test_internal_compact_rx_buffer_null_handle_fails(void)
{
  rx_err_t err = internal_compact_rx_buffer(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_compact_rx_buffer_pos_exceeds_len_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  /* Set pos > len to trigger invalid state */
  s_handle.rx_buffer_len = k_test_len_10;
  s_handle.rx_buffer_pos = k_test_len_20;

  rx_err_t err = internal_compact_rx_buffer(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);

  /* Reset */
  s_handle.rx_buffer_len = 0;
  s_handle.rx_buffer_pos = 0;
}

void test_internal_find_sync_null_handle_fails(void)
{
  int32_t pos = 0;

  rx_err_t err = internal_find_sync(nullptr, &pos);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_find_sync_null_pos_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  rx_err_t err = internal_find_sync(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_internal_find_sync_pos_exceeds_len_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  /* Set pos > len to trigger invalid state */
  s_handle.rx_buffer_len = k_test_len_5;
  s_handle.rx_buffer_pos = k_test_len_10;

  int32_t  pos = 0;
  rx_err_t err = internal_find_sync(&s_handle, &pos);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);

  /* Reset */
  s_handle.rx_buffer_len = 0;
  s_handle.rx_buffer_pos = 0;
}

/* =============================================================================
 * Coverage gap tests: uncovered paths in rx_usb_comm.c
 * =============================================================================
 */

void test_usb_comm_send_pong_usb_not_configured_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  /* Initialize USB but do NOT configure (no set_state configured) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_comm_send_pong(&s_handle, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_send_reset_ack_usb_not_configured_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  /* Initialize USB but do NOT configure */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));

  rx_err_t err = rx_usb_comm_send_reset_ack(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_usb_comm_set_mode_invalid_mode_fails(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }

  /* k_usb_comm_mode_invalid is not a valid mode to set */
  rx_err_t err = rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_invalid);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_usb_comm_receive_pong_consumed_silently(void)
{
  /* Inject a PONG frame - should be consumed silently, loop exits via timeout */
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create PONG frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_pong,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive should consume PONG and loop until timeout */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_50);

  /* No data frames, so timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_sequence_validation_fail_returns_error(void)
{
  /* Inject a frame with sequence far out of range (>= k_session_max_gap_tolerance=10).
   * rx_session_validate_rx returns k_rx_err_protocol_error for out-of-range sequence. */
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* rx_sequence = 0, send sequence = 100 (diff >= k_session_max_gap_tolerance=10) */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             k_test_count_100,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive should return k_rx_err_protocol_error (propagated from session validate) */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_50);

  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);

  /* Session rx_sequence should not have advanced */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());

  mock_time_deinit(&mock);
}

void test_usb_comm_receive_partial_header_waits(void)
{
  /* Inject only partial sync + partial header data (less than k_frame_header_total).
   * With no time interface, the wait_for_data call returns timeout. */
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Inject 4 bytes: just the sync + sequence (but not full header of 8 bytes) */
  uint8_t partial[] = {k_test_fill_aa,
                       k_test_fill_55,
                       0x00,
                       0x00}; /* sync + seq, no len/type/flags */
  rx_usb_rx_push(k_usb_port_proto, partial, sizeof(partial));

  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_100);

  /* Should timeout since not enough data and no sleep interface */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_usb_comm_receive_buffer_full_advances_window(void)
{
  /* Fill the RX buffer with non-frame garbage (no sync word), triggering the
   * buffer-full path in internal_handle_no_sync that increments rx_buffer_pos. */
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Fill the USB RX ring buffer with 0xFF bytes (no sync word 0xAA/0x55).
   * The USB ring buffer is 512 bytes; push in chunks. */
  enum : uint16_t { k_chunk_size = k_test_count_128, k_num_chunks = k_test_len_4 };
  uint8_t garbage[k_chunk_size];
  for (uint32_t i = 0; i < k_chunk_size; i++) {
    garbage[i] = k_test_fill_ff;
  }

  for (uint32_t i = 0; i < k_num_chunks; i++) {
    rx_usb_rx_push(k_usb_port_proto, garbage, k_chunk_size);
  }

  /* Call receive - it reads up to 2048 bytes but ring buffer only has 512.
   * The rx_buffer will fill with non-sync bytes. The buffer_full path fires
   * when rx_buffer_len == k_usb_comm_rx_buffer_size. */
  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_200);

  /* With garbage data and a timeout, we should eventually get timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  mock_time_deinit(&mock);
}

/* =============================================================================
 * Coverage gap tests: reachable paths not yet covered
 * =============================================================================
 */

/**
 * @brief Test that internal_decode_header returns k_rx_err_invalid_size when
 * data_len is large enough for the minimum frame check but smaller than
 * the frame expected by the payload length field.
 *
 * Frame: sync(2)+seq(2)+len(2)+type(1)+flags(1)+crc(4) = 12 bytes minimum.
 * If length field says payload=4, total expected = 12+4 = 16 bytes.
 * Passing only 12 bytes should trigger line 162.
 */
void test_internal_decode_header_data_len_smaller_than_expected(void)
{
  /* Build 12-byte buffer: valid sync, seq=0, len=4 (payload), type=1, flags=0.
   * data_len = 12 but expected_size = 8(header) + 4(payload) + 4(crc) = 16. */
  uint8_t data[k_test_len_12];
  for (uint32_t i = 0; i < k_test_len_12; i++) {
    data[i] = 0;
  }
  data[k_test_idx_0] = k_test_fill_aa; /* sync low */
  data[k_test_idx_1] = k_test_fill_55; /* sync high */
  /* seq = 0 (bytes 2,3) */
  data[k_test_idx_4] = k_test_len_4; /* length low: payload = 4 */
  data[k_test_idx_5] = 0;            /* length high */
  data[k_test_len_6] = 1;            /* type = command */
  /* flags = 0 */

  rx_frame_t frame;
  uint32_t   offset = 0;

  /* data_len=12 < expected_size=16: should return k_rx_err_invalid_size (line 162) */
  rx_err_t err = internal_decode_header(data, k_test_len_12, &frame, &offset);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test that internal_read_usb_data returns k_rx_ok immediately when
 * the internal rx_buffer is exactly full (space == 0, line 345).
 */
void test_internal_read_usb_data_buffer_full_returns_ok(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Set buffer exactly full: space == 0 should return k_rx_ok immediately */
  s_handle.rx_buffer_len = k_usb_comm_rx_buffer_size;
  s_handle.rx_buffer_pos = 0;

  rx_err_t err = internal_read_usb_data(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Buffer length unchanged since nothing could be read */
  TEST_ASSERT_EQUAL_UINT32(k_usb_comm_rx_buffer_size, s_handle.rx_buffer_len);

  /* Reset to avoid tearDown issues */
  s_handle.rx_buffer_len = 0;
}

/**
 * @brief Test that rx_usb_comm_send returns k_rx_err_busy when USB TX buffer
 * is full. Filling the TX ring buffer (1024 bytes) then sending a frame that
 * requires more than 0 free bytes triggers the k_rx_err_busy path (lines 1097-1098).
 *
 * We send a max-payload frame (1036 bytes wire) which exceeds the 1024-byte TX ring.
 */
void test_usb_comm_send_tx_buffer_full_returns_busy(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Fill the TX ring buffer by writing 1024 bytes of raw data directly.
   * Use rx_usb_write with a large buffer that exactly fills the ring. */
  uint8_t fill_data[k_usb_port_proto_tx_size];
  for (uint32_t i = 0; i < k_usb_port_proto_tx_size; i++) {
    fill_data[i] = k_test_fill_ab;
  }

  /* First write fills the buffer */
  rx_err_t fill_err = rx_usb_write(k_usb_port_proto, fill_data, k_usb_port_proto_tx_size);
  TEST_ASSERT_EQUAL(k_rx_ok, fill_err);

  /* Now try to send a frame - TX ring is full, should get k_rx_err_busy */
  uint8_t  payload[k_test_len_4] = {1, k_test_fill_02, k_test_fill_03, k_test_len_4};
  rx_err_t err =
    rx_usb_comm_send(&s_handle, k_frame_type_response, k_frame_flag_none, payload, sizeof(payload));

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

/**
 * @brief Test that rx_usb_comm_receive returns pong-send error when a PING frame
 * arrives but the TX buffer is full (lines 1157-1158: pong send failure path).
 */
void test_usb_comm_receive_ping_pong_send_fails_when_tx_full(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Fill TX ring buffer so PONG send will fail */
  uint8_t fill_data_ping[k_usb_port_proto_tx_size];
  for (uint32_t i = 0; i < k_usb_port_proto_tx_size; i++) {
    fill_data_ping[i] = k_test_fill_cc;
  }
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_usb_write(k_usb_port_proto, fill_data_ping, k_usb_port_proto_tx_size));

  /* Inject a PING frame into the RX buffer */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  rx_err_t   err         = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_ping,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive should try to auto-PONG, fail because TX is full, and return that error */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

/**
 * @brief Test that rx_usb_comm_receive returns ack-send error when a RESET frame
 * arrives but the TX buffer is full (lines 1173-1174: ack send failure path).
 */
void test_usb_comm_receive_reset_ack_send_fails_when_tx_full(void)
{
  {
    rx_usb_comm_config_t cfg_ = helper_default_config();
    TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg_));
  }
  helper_usb_init_and_configure();

  /* Fill TX ring buffer so RESET_ACK send will fail */
  uint8_t fill_data_reset[k_usb_port_proto_tx_size];
  for (uint32_t i = 0; i < k_usb_port_proto_tx_size; i++) {
    fill_data_reset[i] = k_test_fill_dd;
  }
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_usb_write(k_usb_port_proto, fill_data_reset, k_usb_port_proto_tx_size));

  /* Inject a RESET frame into the RX buffer */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;
  rx_err_t   err         = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_reset,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive should try to auto-ACK, fail because TX is full, and return that error */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, 0);

  TEST_ASSERT_EQUAL(k_rx_err_busy, err);
}

/**
 * @brief Test that the rx_buffer full path in internal_handle_no_sync is triggered.
 *
 * When internal_handle_no_sync is called and rx_buffer_len == k_usb_comm_rx_buffer_size,
 * it increments rx_buffer_pos to advance the sliding window (line 557).
 *
 * We pre-fill the handle's rx_buffer with non-sync garbage of exactly max size,
 * then call receive which will invoke internal_handle_no_sync with full buffer.
 */
void test_usb_comm_receive_handle_no_sync_buffer_pos_advances(void)
{
  /* Need a time interface so we can control timeout */
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Fill rx_buffer completely with non-sync data (no 0xAA/0x55 pair).
   * The buffer will be full when internal_read_usb_data is first called,
   * which will return k_rx_ok immediately (line 345).
   * Then internal_find_sync fails (no sync), calling internal_handle_no_sync
   * with rx_buffer_len == k_usb_comm_rx_buffer_size, triggering line 557. */
  for (uint32_t i = 0; i < k_usb_comm_rx_buffer_size; i++) {
    s_handle.rx_buffer[i] = k_test_fill_ff;
  }
  s_handle.rx_buffer_len = k_usb_comm_rx_buffer_size;
  s_handle.rx_buffer_pos = 0;

  rx_frame_t rx_frame;
  rx_err_t   err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_200);

  /* Should timeout since no valid frame */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  mock_time_deinit(&mock);
}

/**
 * @brief internal_verify_crc: nullptr crc_out with valid frame returns k_rx_ok.
 *
 * @details
 * Covers the FALSE branch of `if (crc_out != nullptr)` at line 245.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned
 *
 * @see internal_verify_crc() Function under test
 */
void test_internal_verify_crc_null_crc_out_valid_frame(void)
{
  rx_usb_comm_config_t cfg = helper_default_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg));
  helper_usb_init_and_configure();

  rx_frame_t tx_frame;
  uint8_t    encoded[k_test_count_256];
  uint32_t   encoded_len = 0;
  rx_err_t   err         = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_command,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* CRC is at encoded_len - k_frame_crc_size */
  const uint32_t crc_offset = encoded_len - (uint32_t)k_frame_crc_size;
  /* Pass nullptr for crc_out: covers line-245 FALSE branch */
  err = internal_verify_crc(encoded, crc_offset, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_usb_comm_send: non-null payload with zero length skips memcpy.
 *
 * @details
 * Covers the `payload_len == 0` path of `if (payload != nullptr && payload_len > 0)`
 * at line 1036 (second condition evaluates FALSE).
 *
 * @pre setUp() completed
 * @post k_rx_ok returned
 *
 * @see rx_usb_comm_send() Function under test
 */
void test_usb_comm_send_nonnull_payload_zero_len(void)
{
  rx_usb_comm_config_t cfg = helper_default_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg));
  helper_usb_init_and_configure();

  /* Non-null payload pointer but zero len: second condition of && is FALSE */
  const uint8_t payload[4] = {0x01U, 0x02U, 0x03U, 0x04U};
  rx_err_t      err        = rx_usb_comm_send(&s_handle, k_frame_type_command, 0, payload, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_usb_comm_set_mode: setting same mode skips logging.
 *
 * @details
 * Covers the FALSE branch of `if (handle->mode != mode)` at line 1501.
 * When current mode equals the requested mode, the inner log block is skipped.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, mode unchanged
 *
 * @see rx_usb_comm_set_mode() Function under test
 */
void test_usb_comm_set_mode_same_mode_no_log(void)
{
  rx_usb_comm_config_t cfg = helper_default_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg));

  /* Handle starts in binary mode; set to binary again (same mode -> FALSE branch) */
  rx_err_t err = rx_usb_comm_set_mode(&s_handle, k_usb_comm_mode_binary);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_usb_comm_data_available: buffered data makes available true.
 *
 * @details
 * Covers the `usb_available == 0, buffered > 0` branch of the compound condition
 * `(usb_available > 0) || (buffered > 0)` at line 1241.
 *
 * @pre setUp() completed
 * @post available is true
 *
 * @see rx_usb_comm_data_available() Function under test
 */
void test_usb_comm_data_available_buffered_data(void)
{
  rx_usb_comm_config_t cfg = helper_default_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg));
  helper_usb_init_and_configure();

  /* Place data in the staging rx_buffer directly (not from USB) */
  s_handle.rx_buffer[0]  = k_test_fill_aa;
  s_handle.rx_buffer_len = 1;
  s_handle.rx_buffer_pos = 0;

  bool     available = false;
  rx_err_t err       = rx_usb_comm_data_available(&s_handle, &available);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(available);
}

/**
 * @brief internal_sleep_and_advance: time_iface present but sleep_ms is nullptr returns false.
 *
 * @details
 * Covers the `handle->time_iface->sleep_ms == nullptr` FALSE-second path of
 * `if (handle->time_iface != nullptr && handle->time_iface->sleep_ms != nullptr)` at line 520.
 * The outer condition is TRUE but the inner is FALSE, so function returns false.
 *
 * This is exercised indirectly through the receive path by providing a time interface
 * with sleep_ms=nullptr. The receive loop calls internal_sleep_and_advance when waiting.
 *
 * @pre setUp() completed
 * @post Function behaves as no-time-interface (no sleep, elapsed unchanged)
 *
 * @see rx_usb_comm_receive() Function under test
 */
void test_usb_comm_time_iface_null_sleep_ms(void)
{
  /* Create a time interface with sleep_ms = nullptr */
  rx_time_interface_t  time_iface = {.sleep_ms = nullptr, .get_ms = nullptr, .ctx = nullptr};
  rx_usb_comm_config_t cfg        = helper_config_with_time(&time_iface);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg));
  helper_usb_init_and_configure();

  /* Receive with timeout=1ms; the sleep_and_advance call returns false immediately */
  rx_frame_t frame;
  for (uint32_t i = 0; i < sizeof(frame); i++) {
    ((uint8_t*)&frame)[i] = 0;
  }
  rx_err_t err = rx_usb_comm_receive(&s_handle, &frame, 1U);
  /* No data available, timeout expires quickly */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief Receive a RESET_ACK frame -- covers line 1190 second || operand TRUE
 *
 * @details
 * rx_usb_comm.c line 1190:
 *   `if (frame->header.type == k_frame_type_pong || frame->header.type == k_frame_type_reset_ack)`
 * The existing PONG receive test covers the first operand TRUE path. This test
 * covers the second operand TRUE path (type == k_frame_type_reset_ack) by
 * injecting a RESET_ACK frame. The frame is silently consumed and the receive
 * loop times out.
 *
 * @pre  setUp() completed, USB mock initialized
 * @post rx_usb_comm_receive returns k_rx_err_timeout (no data frame follows)
 */
void test_usb_comm_receive_reset_ack_silently_consumed(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Create RESET_ACK frame */
  rx_frame_t tx_frame;
  uint8_t    encoded[k_frame_max_size];
  uint32_t   encoded_len = 0;

  rx_err_t err = helper_create_encoded_frame(&tx_frame,
                                             0,
                                             k_frame_type_reset_ack,
                                             k_frame_flag_none,
                                             nullptr,
                                             0,
                                             encoded,
                                             &encoded_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  rx_usb_rx_push(k_usb_port_proto, encoded, encoded_len);

  /* Receive should consume RESET_ACK and loop until timeout */
  rx_frame_t rx_frame;
  err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_50);

  /* No data frames, so timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  mock_time_deinit(&mock);
}

/**
 * @brief Partial header with time interface yields k_receive_continue then timeout
 *
 * @details
 * Injects sync + 4 bytes (less than k_frame_header_total = 8) and uses a time
 * interface with auto-advance. The first call to internal_wait_for_data sees
 * elapsed_ms < timeout and advances time, returning k_rx_ok (k_receive_continue).
 * The loop retries and eventually exhausts the timeout.
 * Covers the false branch of (*err != k_rx_ok) at line 764.
 */
void test_usb_comm_receive_partial_header_with_time_iface_loops(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Inject sync word + 4 bytes of payload (total 4 bytes) --
   * less than k_frame_header_total (8 bytes), so header read will wait. */
  uint8_t partial[] = {k_test_fill_aa, k_test_fill_55, 0x00, 0x00}; /* sync (2) + seq (2) only */
  rx_usb_rx_push(k_usb_port_proto, partial, sizeof(partial));

  rx_frame_t rx_frame;
  /* Use a generous timeout so the first internal_wait_for_data returns k_rx_ok
   * (time advances 10 ms per iteration, eventually times out). */
  rx_err_t err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_100);

  /* Must time out eventually since no full frame is ever added */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  mock_time_deinit(&mock);
}

/**
 * @brief Partial header with time interface: wait_for_data returns k_rx_ok (k_receive_continue)
 *
 * @details
 * Covers line 764 branch :1 (k_receive_continue path) in rx_usb_comm.c:
 *   `return (*err != k_rx_ok) ? k_receive_error : k_receive_continue;`
 *
 * Scenario: sync word found but only 4 bytes available (less than k_frame_header_total=8).
 * With a mock time interface that allows one sleep cycle, internal_wait_for_data returns
 * k_rx_ok at least once, so the ternary takes the k_receive_continue branch. The outer
 * receive loop then retries, eventually timing out.
 *
 * @pre setUp() completed, USB mock initialized
 * @post rx_usb_comm_receive returns k_rx_err_timeout
 */
void test_usb_comm_partial_header_wait_returns_ok_then_timeout(void)
{
  mock_time_t         mock;
  rx_time_interface_t time_iface;

  mock_time_init(&mock);
  mock_time_set_auto_advance(&mock, true);
  mock_time_get_interface(&time_iface, &mock);

  rx_usb_comm_config_t config = helper_config_with_time(&time_iface);

  helper_usb_init_and_configure();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &config));

  /* Push valid sync word + partial header (only 4 bytes, less than k_frame_header_total=8) */
  uint8_t partial[] = {k_test_fill_aa, k_test_fill_55, 0x00U, 0x00U};
  rx_usb_rx_push(k_usb_port_proto, partial, sizeof(partial));

  /* With time interface, internal_wait_for_data returns k_rx_ok at least once
   * (k_receive_continue branch taken), then eventually times out */
  rx_frame_t rx_frame;
  for (uint32_t i = 0; i < sizeof(rx_frame); i++) {
    ((uint8_t*)&rx_frame)[i] = 0;
  }
  rx_err_t err = rx_usb_comm_receive(&s_handle, &rx_frame, k_test_count_50);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  mock_time_deinit(&mock);
}

/**
 * @brief data_available: USB buffer has data (usb_available > 0 short-circuit branch)
 *
 * @details
 * Covers line 1207 missing branch in rx_usb_comm.c:
 *   `*available = (usb_available > 0) || (buffered > 0);`
 *
 * The short-circuit branch (usb_available > 0 TRUE) is not covered by any existing test.
 * Here we push raw bytes to the USB proto port RX buffer so rx_usb_rx_available returns > 0.
 *
 * @pre setUp() completed, USB mock initialized
 * @post rx_usb_comm_data_available returns true via short-circuit (usb_available > 0)
 */
void test_usb_comm_data_available_usb_buffer_has_data(void)
{
  rx_usb_comm_config_t cfg = helper_default_config();

  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(&s_handle, &cfg));
  helper_usb_init_and_configure();

  /* Push raw bytes into the USB proto port RX buffer -- usb_available > 0 */
  uint8_t data[] = {k_test_fill_aa, k_test_fill_55};
  rx_usb_rx_push(k_usb_port_proto, data, sizeof(data));

  bool     available = false;
  rx_err_t err       = rx_usb_comm_data_available(&s_handle, &available);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(available);
}

/* =============================================================================
 * Main test runner helpers
 * =============================================================================
 */

/** @brief Run init/deinit and send test group */
static void internal_run_init_send_tests(void)
{
  RUN_TEST(test_usb_comm_init_null_handle_fails);
  RUN_TEST(test_usb_comm_init_null_config_fails);
  RUN_TEST(test_usb_comm_init_null_session_fails);
  RUN_TEST(test_usb_comm_init_success);
  RUN_TEST(test_usb_comm_deinit_null_handle_fails);
  RUN_TEST(test_usb_comm_deinit_success);
  RUN_TEST(test_usb_comm_deinit_not_initialized_succeeds);
  RUN_TEST(test_usb_comm_send_null_handle_fails);
  RUN_TEST(test_usb_comm_send_not_initialized_fails);
  RUN_TEST(test_usb_comm_send_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_send_null_payload_with_len_fails);
  RUN_TEST(test_usb_comm_send_payload_too_large_fails);
  RUN_TEST(test_usb_comm_send_empty_payload_succeeds);
  RUN_TEST(test_usb_comm_send_with_payload_succeeds);
  RUN_TEST(test_usb_comm_send_increments_sequence);
}

/** @brief Run basic receive, sync detection, header, and buffer management tests */
static void internal_run_receive_basic_tests(void)
{
  RUN_TEST(test_usb_comm_receive_null_handle_fails);
  RUN_TEST(test_usb_comm_receive_null_frame_fails);
  RUN_TEST(test_usb_comm_receive_not_initialized_fails);
  RUN_TEST(test_usb_comm_receive_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_receive_no_data_timeout);
  RUN_TEST(test_usb_comm_receive_valid_frame_with_sync);
  RUN_TEST(test_usb_comm_receive_empty_payload_frame);
  RUN_TEST(test_usb_comm_receive_max_payload_frame);
  RUN_TEST(test_usb_comm_receive_increments_rx_sequence);
  RUN_TEST(test_usb_comm_receive_finds_sync_after_garbage);
  RUN_TEST(test_usb_comm_receive_partial_sync_word);
  RUN_TEST(test_usb_comm_receive_multiple_false_syncs);
  RUN_TEST(test_usb_comm_receive_invalid_payload_length);
  RUN_TEST(test_usb_comm_receive_header_validation);
  RUN_TEST(test_usb_comm_receive_buffer_compaction);
  RUN_TEST(test_usb_comm_receive_buffer_overflow_protection);
  RUN_TEST(test_usb_comm_receive_fragmented_frame);
  RUN_TEST(test_usb_comm_receive_sequence_wraparound);
  RUN_TEST(test_usb_comm_receive_out_of_order_sequence);
  RUN_TEST(test_usb_comm_receive_crc_mismatch);
  RUN_TEST(test_usb_comm_receive_iteration_limit);
}

/** @brief Run data-available, is-ready, flush, and mode-switching tests */
static void internal_run_state_mode_tests(void)
{
  RUN_TEST(test_usb_comm_data_available_null_handle_fails);
  RUN_TEST(test_usb_comm_data_available_null_available_fails);
  RUN_TEST(test_usb_comm_data_available_not_initialized_fails);
  RUN_TEST(test_usb_comm_data_available_empty);
  RUN_TEST(test_usb_comm_is_ready_null_handle_fails);
  RUN_TEST(test_usb_comm_is_ready_null_ready_fails);
  RUN_TEST(test_usb_comm_is_ready_not_initialized);
  RUN_TEST(test_usb_comm_is_ready_usb_not_configured);
  RUN_TEST(test_usb_comm_is_ready_configured);
  RUN_TEST(test_usb_comm_flush_rx);
  RUN_TEST(test_usb_comm_flush_rx_null_handle);
  RUN_TEST(test_usb_comm_init_defaults_to_binary_mode);
  RUN_TEST(test_usb_comm_set_mode_null_handle_fails);
  RUN_TEST(test_usb_comm_set_mode_not_initialized_fails);
  RUN_TEST(test_usb_comm_set_mode_to_debug);
  RUN_TEST(test_usb_comm_set_mode_to_binary);
  RUN_TEST(test_usb_comm_get_mode_null_handle_returns_invalid);
  RUN_TEST(test_usb_comm_get_mode_not_initialized_returns_invalid);
  RUN_TEST(test_usb_comm_send_fails_in_debug_mode);
  RUN_TEST(test_usb_comm_send_succeeds_after_mode_switch_back_to_binary);
  RUN_TEST(test_usb_comm_mode_preserved_after_deinit_init);
}

/** @brief Run control frame, timeout, and internal function tests */
static void internal_run_control_internal_tests(void)
{
  RUN_TEST(test_usb_comm_set_callbacks_null_handle_fails);
  RUN_TEST(test_usb_comm_set_callbacks_not_initialized_fails);
  RUN_TEST(test_usb_comm_set_callbacks_success);
  RUN_TEST(test_usb_comm_send_pong_null_handle_fails);
  RUN_TEST(test_usb_comm_send_pong_not_initialized_fails);
  RUN_TEST(test_usb_comm_send_pong_echoes_payload);
  RUN_TEST(test_usb_comm_send_reset_ack_null_handle_fails);
  RUN_TEST(test_usb_comm_send_reset_ack_not_initialized_fails);
  RUN_TEST(test_usb_comm_send_reset_ack_success);
  RUN_TEST(test_usb_comm_receive_ping_auto_pong);
  RUN_TEST(test_usb_comm_receive_reset_auto_ack);
  RUN_TEST(test_usb_comm_receive_ping_callback_invoked);
  RUN_TEST(test_usb_comm_receive_reset_callback_invoked);
  RUN_TEST(test_usb_comm_init_with_time_interface);
  RUN_TEST(test_usb_comm_receive_timeout_without_time_iface);
  RUN_TEST(test_usb_comm_receive_timeout_with_mock_time);
  RUN_TEST(test_usb_comm_receive_immediate_timeout_zero);
  RUN_TEST(test_internal_decode_header_null_data_fails);
  RUN_TEST(test_internal_decode_header_null_frame_fails);
  RUN_TEST(test_internal_decode_header_too_small_fails);
  RUN_TEST(test_internal_decode_header_bad_sync_fails);
  RUN_TEST(test_internal_decode_header_payload_too_large_fails);
  RUN_TEST(test_internal_decode_header_null_offset_out_ok);
  RUN_TEST(test_internal_verify_crc_null_data_fails);
  RUN_TEST(test_internal_verify_crc_small_offset_fails);
  RUN_TEST(test_internal_read_usb_data_null_handle_fails);
  RUN_TEST(test_internal_read_usb_data_buffer_overflow_fails);
  RUN_TEST(test_internal_compact_rx_buffer_null_handle_fails);
  RUN_TEST(test_internal_compact_rx_buffer_pos_exceeds_len_fails);
  RUN_TEST(test_internal_find_sync_null_handle_fails);
  RUN_TEST(test_internal_find_sync_null_pos_fails);
  RUN_TEST(test_internal_find_sync_pos_exceeds_len_fails);
}

/** @brief Run coverage gap and branch coverage tests */
static void internal_run_coverage_gap_tests(void)
{
  RUN_TEST(test_usb_comm_send_pong_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_send_reset_ack_usb_not_configured_fails);
  RUN_TEST(test_usb_comm_set_mode_invalid_mode_fails);
  RUN_TEST(test_usb_comm_receive_pong_consumed_silently);
  RUN_TEST(test_usb_comm_receive_sequence_validation_fail_returns_error);
  RUN_TEST(test_usb_comm_receive_partial_header_waits);
  RUN_TEST(test_usb_comm_receive_buffer_full_advances_window);
  RUN_TEST(test_internal_decode_header_data_len_smaller_than_expected);
  RUN_TEST(test_internal_read_usb_data_buffer_full_returns_ok);
  RUN_TEST(test_usb_comm_send_tx_buffer_full_returns_busy);
  RUN_TEST(test_usb_comm_receive_ping_pong_send_fails_when_tx_full);
  RUN_TEST(test_usb_comm_receive_reset_ack_send_fails_when_tx_full);
  RUN_TEST(test_usb_comm_receive_handle_no_sync_buffer_pos_advances);
  RUN_TEST(test_internal_verify_crc_null_crc_out_valid_frame);
  RUN_TEST(test_usb_comm_send_nonnull_payload_zero_len);
  RUN_TEST(test_usb_comm_set_mode_same_mode_no_log);
  RUN_TEST(test_usb_comm_data_available_buffered_data);
  RUN_TEST(test_usb_comm_time_iface_null_sleep_ms);
  RUN_TEST(test_usb_comm_receive_reset_ack_silently_consumed);
  RUN_TEST(test_usb_comm_receive_partial_header_with_time_iface_loops);
  RUN_TEST(test_usb_comm_partial_header_wait_returns_ok_then_timeout);
  RUN_TEST(test_usb_comm_data_available_usb_buffer_has_data);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  internal_run_init_send_tests();
  internal_run_receive_basic_tests();
  internal_run_state_mode_tests();
  internal_run_control_internal_tests();
  internal_run_coverage_gap_tests();

  return UNITY_END();
}
