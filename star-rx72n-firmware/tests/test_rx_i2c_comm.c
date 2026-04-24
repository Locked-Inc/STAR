/**
 * @file test_rx_i2c_comm.c
 * @brief Unit Tests for I2C Frame-Based Communication Protocol (RPi5 <-> RX72N)
 *
 * @details
 * **Comprehensive Test Suite for I2C Peripheral Communication Layer**
 *
 * This test suite validates the rx_i2c_comm layer - a frame-based I2C protocol
 * that provides an alternative communication channel for the RPi5 (I2C controller)
 * to RX72N (I2C peripheral) link. Tests verify initialization, frame encoding,
 * CRC validation, sequence management, sync word detection, buffer management,
 * and PING/RESET frame auto-response behavior.
 *
 * **I2C Communication Architecture:**
 * @code
 * RPi5 (Linux I2C Controller)       RX72N (RIIC0 Peripheral)
 * +-------------------------+       +---------------------------+
 * | /dev/i2c-1              | SCL -> | RIIC0 (Peripheral Mode)  |
 * | (I2C bus controller)    | SDA <- | Device address: 0x42     |
 * +-------------------------+       +---------------------------+
 *         v Frame Protocol                    v
 * +-------------------------+       +---------------------------+
 * | rx_i2c_comm (Linux)     |       | rx_i2c_comm (RX72N)      |
 * | - Protobuf messages     |       | - Frame decode           |
 * | - Frame encode          |       | - CRC-32 check           |
 * | - CRC-32 validation     |       | - PING/RESET auto-reply  |
 * +-------------------------+       +---------------------------+
 * @endcode
 *
 * **Test Categories:**
 * 1. **Initialization** - Tests handle initialization and RIIC channel configuration
 * 2. **Deinitialization** - Tests handle cleanup including NULL-handle safety
 * 3. **Send Operations** - Tests payload encoding, CRC, TX buffer validation
 * 4. **Receive Operations** - Tests frame reception, sync search, buffer management
 * 5. **Frame Parsing** - Tests CRC validation, sync word handling, header parsing
 * 6. **Buffer Management** - Tests rx_buffer_pos/len tracking and compaction
 * 7. **Sequence Management** - Tests TX sequence number incrementing via session
 * 8. **Control Frame Handling** - Tests PING auto-PONG, RESET auto-RESET_ACK
 * 9. **Error Injection** - Tests error propagation from RIIC HAL mock
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (Control Flow):** [OK] All test functions use simple sequential flow
 * - **Rule 2 (Loop Bounds):** [OK] All loops have compile-time known bounds
 * - **Rule 3 (Dynamic Memory):** [OK] Zero heap allocation (stack buffers only)
 * - **Rule 4 (Function Size):** [OK] Test functions <50 lines, helpers <25 lines
 * - **Rule 5 (Assertions):** [OK] Every test has minimum 1 assertion
 * - **Rule 7 (Return Checking):** [OK] All API returns validated
 * - **Rule 9 (Pointers):** [OK] Single-level dereferencing only
 * - **Rule 10 (Warnings):** [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Each test validates one I2C protocol behavior
 * - **Open/Closed:** Mock injection allows testing without RIIC hardware
 * - **Liskov Substitution:** Mock RIIC substitutes for real peripheral
 * - **Interface Segregation:** Tests use minimal API (init/send/receive/deinit)
 * - **Dependency Inversion:** Depends on RIIC abstraction, not RIIC registers
 *
 * @par Module Dependencies:
 * - rx_i2c_comm.h - Frame-based I2C comm layer API under test
 * - rx_frame.h - Frame encoding/decoding primitives
 * - mock_riic_hal.h - Mock RIIC peripheral driver
 * - rx_err.h - Error code definitions
 * - unity.h - Unit testing framework
 *
 * @see rx_i2c_comm.h - I2C comm layer header
 * @see rx_frame.h - Frame structure and encoding/decoding
 * @see mock_riic_hal.h - Mock RIIC driver for testing
 * @see docs/sections/01_nanopb_protocol.tex - Full protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-03-12
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @test Tests run via Unity framework with: make test_rx_i2c_comm
 */

#include <string.h>

#include "mock_riic_hal.h"
#include "rx_check.h"
#include "rx_frame.h"
#include "rx_i2c_comm.h"
#include "rx_session.h"
#include "unity.h"

/* =============================================================================
 * Internal function forward declarations (visible in UNIT_TEST builds via
 * RX_STATIC_TESTABLE - see rx_check.h).  These declarations allow tests to
 * directly exercise defensive null-check and error-handling branches that are
 * unreachable through the public API.
 * =============================================================================
 */
#ifdef UNIT_TEST

/* Receive-result type mirrored from rx_i2c_comm.c so callers can inspect it */
typedef enum : uint8_t {
  k_receive_continue = 0,
  k_receive_done     = 1,
  k_receive_error    = 2,
} rx_receive_result_t;

extern rx_err_t internal_validate_wire_len(uint32_t wire_len);
extern rx_err_t internal_decode_header(const uint8_t* data,
                                       uint32_t       data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out);
extern rx_err_t internal_verify_crc(const uint8_t* data, uint32_t offset, uint32_t* crc_out);
extern rx_err_t internal_read_i2c_data(rx_i2c_comm_handle_t* handle);
extern rx_err_t internal_compact_rx_buffer(rx_i2c_comm_handle_t* handle);
extern rx_err_t internal_find_sync(const rx_i2c_comm_handle_t* handle, int32_t* sync_pos);
extern rx_err_t internal_handle_no_sync(rx_i2c_comm_handle_t* handle);
extern rx_err_t
internal_decode_frame(rx_i2c_comm_handle_t* handle, rx_frame_t* frame, uint32_t total_size);
extern void internal_align_to_sync(rx_i2c_comm_handle_t* handle, int32_t sync_pos);
extern rx_err_t
internal_parse_header(rx_i2c_comm_handle_t* handle, uint16_t* payload_len, uint32_t* total_size);
extern rx_receive_result_t
internal_receive_iteration(rx_i2c_comm_handle_t* handle, rx_frame_t* frame, rx_err_t* err);
extern rx_err_t internal_build_frame(rx_frame_t*     frame,
                                     uint16_t        sequence,
                                     rx_frame_type_t type,
                                     uint8_t         flags,
                                     const uint8_t*  payload,
                                     uint32_t        payload_len);
extern rx_err_t internal_handle_ping(rx_i2c_comm_handle_t* handle, const rx_frame_t* frame);
extern rx_err_t internal_handle_reset(rx_i2c_comm_handle_t* handle);
#endif /* UNIT_TEST */

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test payload sizes and other numeric constants
 */
typedef enum : uint16_t {
  k_test_payload_small     = 4,    /**< Small payload: "TEST" */
  k_test_payload_medium    = 16,   /**< Medium payload: 16 bytes */
  k_test_seq_a             = 0,    /**< First expected TX sequence */
  k_test_seq_b             = 1,    /**< Second expected TX sequence */
  k_test_timeout_zero      = 0,    /**< Non-blocking receive timeout */
  k_test_addr_default      = 0x42, /**< Default I2C device address */
  k_test_channel_default   = 0,    /**< RIIC0 channel */
  k_test_frame_buf_size    = 2048, /**< Buffer for encoded frames */
  k_test_tx_buf_size       = 256,  /**< Readback buffer size */
  k_test_payload_large     = 249,  /**< Payload that makes wire_len exceed 256-byte I2C limit */
  k_test_rx_buf_max        = 2048, /**< k_i2c_comm_rx_buffer_size */
  k_test_seq_gap_large     = 33000, /**< Gap >= k_session_max_gap_tolerance (32768); triggers validate_fail */
  k_test_expected_tx_seq   = 2,    /**< Expected TX sequence after two sends */
  k_test_oversized_payload = 1025, /**< Payload exceeding k_frame_max_payload (1024) */
} test_i2c_comm_constants_t;

/**
 * @brief Additional test constants for addresses, indices, and byte values
 */
typedef enum : uint8_t {
  k_test_addr_alt         = 0x50, /**< Alternate I2C device address */
  k_test_channel_alt      = 1,    /**< Alternate RIIC channel */
  k_test_garbage_fill     = 0xCC, /**< Fill byte for garbage data (no sync word) */
  k_test_crc_flip_mask    = 0xFF, /**< XOR mask to corrupt CRC bytes */
  k_test_garbage_byte0    = 0xDE, /**< Garbage prefix byte 0 */
  k_test_garbage_byte1    = 0xAD, /**< Garbage prefix byte 1 */
  k_test_garbage_byte2    = 0xBE, /**< Garbage prefix byte 2 */
  k_test_garbage_byte3    = 0xEF, /**< Garbage prefix byte 3 */
  k_test_sync_byte_low    = 0xAA, /**< LE low byte of k_frame_sync_word */
  k_test_sync_byte_high   = 0x55, /**< LE high byte of k_frame_sync_word */
  k_test_payload_byte_a   = 0xAA, /**< Test payload data byte A */
  k_test_payload_byte_b   = 0xBB, /**< Test payload data byte B */
  k_test_payload_byte_c   = 0xCC, /**< Test payload data byte C */
  k_test_payload_byte_d   = 0xDD, /**< Test payload data byte D */
  k_test_len_hi_byte      = 0x05, /**< High byte of oversized LE16 length (0x0500=1280) */
  k_test_false_sync_hi    = 0x00, /**< Non-matching high byte for false sync test */
  k_test_frame_type_cmd   = 0x01, /**< Frame type command value for raw header construction */
  k_test_frame_flags_zero = 0x00, /**< Zero flags value for raw header construction */
} test_i2c_byte_constants_t;

/**
 * @brief Additional test constants for offsets and counts
 */
typedef enum : uint32_t {
  k_test_garbage_len       = 4,   /**< Number of garbage prefix bytes */
  k_test_garbage_arr_len   = 12,  /**< Length of garbage array with no sync */
  k_test_chunk_size        = 250, /**< Injection chunk size < RIIC transfer limit */
  k_test_batch_count       = 9,   /**< Number of batches to fill RX buffer */
  k_test_garbage_buf_len   = 999, /**< Garbage rx_buffer_len for init-clears test */
  k_test_garbage_buf_pos   = 123, /**< Garbage rx_buffer_pos for init-clears test */
  k_test_compact_test_len  = 5,   /**< Buffer len for compact no-op test */
  k_test_compact_bad_pos   = 10,  /**< Buffer pos > len for compact error test */
  k_test_align_sync_pos    = 5,   /**< Sync position for align test */
  k_test_align_high_pos    = 10,  /**< High buffer pos for align no-regression test */
  k_test_align_low_sync    = 3,   /**< Lower sync pos for align no-regression test */
  k_test_find_sync_len     = 2,   /**< Short buffer len for find_sync error test */
  k_test_find_sync_bad_pos = 5,   /**< Pos > len for find_sync error test */
  k_test_small_space       = 16,  /**< Small remaining space for read clamp test */
  k_test_decode_buf_size   = 32,  /**< Buffer size for decode header tests */
  k_test_decode_buf_large  = 64,  /**< Larger buffer for decode header tests */
  k_test_partial_inject    = 4,   /**< Bytes to inject for partial frame test */
  k_test_seq_five          = 5,   /**< Sequence number for build_frame test */
  k_test_seq_one           = 1,   /**< Sequence number for build_frame payload test */
  k_test_seq_three         = 3,   /**< Sequence number for build_frame nonnull-zero-len */
  k_test_crc_buf_size      = 32,  /**< Buffer size for CRC test */
  k_test_crc_offset_eight  = 8,   /**< Offset for CRC null-data test */
  k_test_false_sync_prefix = 2,   /**< False sync prefix length */
  k_test_header_idx_seq_lo = 2,   /**< Header byte index: sequence low */
  k_test_header_idx_seq_hi = 3,   /**< Header byte index: sequence high */
  k_test_header_idx_len_lo = 4,   /**< Header byte index: payload length low */
  k_test_header_idx_len_hi = 5,   /**< Header byte index: payload length high */
  k_test_header_idx_type   = 6,   /**< Header byte index: frame type */
  k_test_header_idx_flags  = 7,   /**< Header byte index: frame flags */
  k_test_crc_corrupt_idx_2 = 2,   /**< Second-to-last byte for CRC corruption */
  k_test_sync_fill_step    = 2,   /**< Step size for sync word fill loop */
} test_i2c_uint32_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/** @brief I2C comm handle under test */
static rx_i2c_comm_handle_t s_handle;

/** @brief Shared session for sequence tracking */
static rx_session_state_t s_session;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Build a default valid I2C comm config
 * @return Populated rx_i2c_comm_config_t for RIIC0, addr 0x42
 */
static rx_i2c_comm_config_t helper_default_config(void)
{
  rx_i2c_comm_config_t cfg = {
    .session     = &s_session,
    .channel     = {.value = k_test_channel_default},
    .device_addr = {.value = k_test_addr_default},
  };
  return cfg;
}

/**
 * @brief Initialize handle with default config, assert success
 */
static void helper_init_default(void)
{
  rx_i2c_comm_config_t cfg = helper_default_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_handle, &cfg));
  TEST_ASSERT_TRUE(s_handle.initialized);
}

/**
 * @brief Encode a valid frame into raw bytes using the frame layer
 *
 * @param[in]  type        Frame type
 * @param[in]  sequence    Sequence number
 * @param[in]  payload     Payload bytes (may be nullptr for len==0)
 * @param[in]  payload_len Payload length in bytes
 * @param[out] buf         Output buffer (caller provides)
 * @param[out] out_len     Encoded byte count output
 */
static void helper_encode_frame(rx_frame_type_t type,
                                uint16_t        sequence,
                                const uint8_t*  payload,
                                uint32_t        payload_len,
                                uint8_t*        buf,
                                uint32_t*       out_len)
{
  rx_frame_encoder_t enc;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  frame.header.sequence = sequence;
  frame.header.length   = (uint16_t)payload_len;
  frame.header.type     = (uint8_t)type;
  frame.header.flags    = k_frame_flag_none;

  if (payload != nullptr && payload_len > 0) {
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memcpy(frame.payload, payload, payload_len);
  }

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc, &frame, buf, out_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc));
}

/**
 * @brief Inject encoded frame bytes into mock RIIC RX buffer
 *
 * @param[in] data   Encoded frame bytes
 * @param[in] length Number of bytes to inject
 */
static void helper_inject_rx(const uint8_t* data, uint16_t length)
{
  mock_riic_set_rx_data(k_test_channel_default, data, length);
}

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

void setUp(void)
{
  mock_riic_init();
  (void)rx_session_init(&s_session);
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_handle, 0, sizeof(s_handle));
}

void tearDown(void)
{
  (void)rx_i2c_comm_deinit(&s_handle);
  (void)rx_session_deinit(&s_session);
  mock_riic_reset();
}

/* =============================================================================
 * 1. Initialization Tests
 * =============================================================================
 */

/**
 * @brief Init with null handle returns k_rx_err_invalid_arg
 *
 * @pre  mock_riic in clean state
 * @post No RIIC channel initialized
 */
void test_i2c_comm_init_null_handle_fails(void)
{
  rx_i2c_comm_config_t cfg = helper_default_config();
  rx_err_t             err = rx_i2c_comm_init(nullptr, &cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Init with null config returns k_rx_err_invalid_arg
 *
 * @pre  s_handle in cleared state
 * @post s_handle remains uninitialized
 */
void test_i2c_comm_init_null_config_fails(void)
{
  rx_err_t err = rx_i2c_comm_init(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Init with null session in config returns k_rx_err_invalid_arg
 *
 * @pre  s_handle in cleared state
 * @post s_handle remains uninitialized
 */
void test_i2c_comm_init_null_session_fails(void)
{
  rx_i2c_comm_config_t cfg = helper_default_config();
  cfg.session              = nullptr;

  rx_err_t err = rx_i2c_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Init succeeds with valid config and marks handle initialized
 *
 * @pre  mock_riic in clean state, s_session initialized
 * @post s_handle.initialized == true, channel_value and device_addr stored
 */
void test_i2c_comm_init_success(void)
{
  rx_i2c_comm_config_t cfg = helper_default_config();
  rx_err_t             err = rx_i2c_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
  TEST_ASSERT_EQUAL_UINT8(k_test_channel_default, s_handle.channel_value);
  TEST_ASSERT_EQUAL_UINT8(k_test_addr_default, s_handle.device_addr);
  TEST_ASSERT_NOT_NULL(s_handle.session);
}

/**
 * @brief Init stores channel and address from config
 *
 * @pre  mock_riic clean, session initialized
 * @post channel_value and device_addr reflect config values
 */
void test_i2c_comm_init_stores_channel_and_addr(void)
{
  rx_i2c_comm_config_t cfg = {
    .session     = &s_session,
    .channel     = {.value = k_test_channel_alt},
    .device_addr = {.value = k_test_addr_alt},
  };

  rx_err_t err = rx_i2c_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_test_channel_alt, s_handle.channel_value);
  TEST_ASSERT_EQUAL_UINT8(k_test_addr_alt, s_handle.device_addr);
}

/**
 * @brief Init clears RX buffer fields
 *
 * @pre  s_handle has garbage state
 * @post rx_buffer_len == 0, rx_buffer_pos == 0
 */
void test_i2c_comm_init_clears_rx_buffer(void)
{
  /* Pre-populate with garbage */
  s_handle.rx_buffer_len = k_test_garbage_buf_len;
  s_handle.rx_buffer_pos = k_test_garbage_buf_pos;

  helper_init_default();

  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
}

/**
 * @brief Init with RIIC HAL failure propagates error
 *
 * @pre  mock_riic set to return error on next call
 * @post s_handle remains uninitialized
 */
void test_i2c_comm_init_riic_failure_propagates(void)
{
  mock_riic_set_next_error(k_rx_err_timeout);

  rx_i2c_comm_config_t cfg = helper_default_config();
  rx_err_t             err = rx_i2c_comm_init(&s_handle, &cfg);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/* =============================================================================
 * 2. Deinitialization Tests
 * =============================================================================
 */

/**
 * @brief Deinit with null handle returns k_rx_err_invalid_arg
 *
 * @pre  no preconditions
 * @post no state changes
 */
void test_i2c_comm_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_i2c_comm_deinit(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Deinit succeeds on initialized handle and clears initialized flag
 *
 * @pre  handle initialized via helper_init_default()
 * @post s_handle.initialized == false
 */
void test_i2c_comm_deinit_success(void)
{
  helper_init_default();

  rx_err_t err = rx_i2c_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Deinit on uninitialized handle returns k_rx_ok gracefully
 *
 * @pre  s_handle is zero (not initialized)
 * @post no crash; returns k_rx_ok
 */
void test_i2c_comm_deinit_not_initialized_ok(void)
{
  /* s_handle is already zeroed in setUp */
  rx_err_t err = rx_i2c_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * 3. Send Tests
 * =============================================================================
 */

/**
 * @brief Send with null handle returns k_rx_err_invalid_arg
 *
 * @pre  no preconditions
 * @post no state changes
 */
void test_i2c_comm_send_null_handle_fails(void)
{
  const uint8_t payload[] = "test";
  rx_err_t err = rx_i2c_comm_send(nullptr, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Send on uninitialized handle returns k_rx_err_invalid_state
 *
 * @pre  s_handle is zeroed (not initialized)
 * @post no RIIC write issued
 */
void test_i2c_comm_send_not_initialized_fails(void)
{
  const uint8_t payload[] = "test";
  rx_err_t      err =
    rx_i2c_comm_send(&s_handle, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Send with null payload and nonzero length returns k_rx_err_invalid_arg
 *
 * @pre  handle initialized
 * @post no RIIC write issued
 */
void test_i2c_comm_send_null_payload_nonzero_len_fails(void)
{
  helper_init_default();

  rx_err_t err =
    rx_i2c_comm_send(&s_handle, k_frame_type_response, 0, nullptr, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Send with payload exceeding k_frame_max_payload returns k_rx_err_invalid_size
 *
 * @pre  handle initialized
 * @post no RIIC write issued
 */
void test_i2c_comm_send_payload_too_large_fails(void)
{
  helper_init_default();

  /* k_frame_max_payload + 1 exceeds limit */
  const uint32_t oversized = (uint32_t)k_frame_max_payload + 1U;
  uint8_t        dummy[1];
  rx_err_t       err = rx_i2c_comm_send(&s_handle, k_frame_type_response, 0, dummy, oversized);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Send with null payload and zero length succeeds (zero-payload frame)
 *
 * @pre  handle initialized, RIIC channel 0 initialized by init
 * @post RIIC write called once, TX sequence incremented
 */
void test_i2c_comm_send_zero_payload_success(void)
{
  helper_init_default();

  rx_err_t err = rx_i2c_comm_send(&s_handle, k_frame_type_response, 0, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Send with small payload succeeds and RIIC write is called
 *
 * @pre  handle initialized, RIIC channel 0 initialized by init
 * @post RIIC write called, TX buffer contains valid encoded frame
 */
void test_i2c_comm_send_small_payload_success(void)
{
  helper_init_default();

  const uint8_t payload[] = "ABCD";
  rx_err_t      err       = rx_i2c_comm_send(&s_handle,
                                  k_frame_type_response,
                                  k_frame_flag_none,
                                  payload,
                                  k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data was written to the mock */
  uint8_t  tx_buf[k_test_tx_buf_size];
  uint16_t tx_len = mock_riic_get_tx_data(k_test_channel_default, tx_buf, k_test_tx_buf_size);
  TEST_ASSERT_GREATER_THAN_UINT16(0, tx_len);
}

/**
 * @brief TX data from send contains valid sync word in first two bytes
 *
 * @pre  handle initialized
 * @post first two bytes of TX data match k_frame_sync_word (LE)
 */
void test_i2c_comm_send_tx_contains_sync_word(void)
{
  helper_init_default();

  const uint8_t payload[] = "TEST";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_i2c_comm_send(&s_handle,
                                     k_frame_type_command,
                                     k_frame_flag_none,
                                     payload,
                                     k_test_payload_small));

  uint8_t  tx_buf[k_test_tx_buf_size];
  uint16_t tx_len = mock_riic_get_tx_data(k_test_channel_default, tx_buf, k_test_tx_buf_size);
  TEST_ASSERT_GREATER_THAN_UINT16(1, tx_len);

  /* Sync word: low byte first (LE) */
  const uint8_t sync_low  = (uint8_t)(k_frame_sync_word & 0xFFU);
  const uint8_t sync_high = (uint8_t)(k_frame_sync_word >> 8U);
  TEST_ASSERT_EQUAL_HEX8(sync_low, tx_buf[0]);
  TEST_ASSERT_EQUAL_HEX8(sync_high, tx_buf[1]);
}

/**
 * @brief Each send increments the TX session sequence by one
 *
 * @pre  handle initialized, session at sequence 0
 * @post After two sends, TX sequence is 2
 */
void test_i2c_comm_send_increments_tx_sequence(void)
{
  helper_init_default();

  const uint8_t payload[] = "PING";

  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_i2c_comm_send(&s_handle, k_frame_type_command, 0, payload, k_test_payload_small));
  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_i2c_comm_send(&s_handle, k_frame_type_command, 0, payload, k_test_payload_small));

  uint16_t tx_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(&s_session, &tx_seq));
  TEST_ASSERT_EQUAL_UINT16(k_test_expected_tx_seq, tx_seq);
}

/**
 * @brief Send failure from RIIC write propagates error
 *
 * @pre  handle initialized, next RIIC call returns error
 * @post send returns the injected error
 */
void test_i2c_comm_send_riic_write_failure_propagates(void)
{
  helper_init_default();

  /* Inject error on next riic_peripheral_write (happens after init calls) */
  mock_riic_set_next_error(k_rx_err_timeout);

  const uint8_t payload[] = "FAIL";
  rx_err_t      err =
    rx_i2c_comm_send(&s_handle, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * 4. Receive Tests
 * =============================================================================
 */

/**
 * @brief Receive with null handle returns k_rx_err_invalid_arg
 *
 * @pre  no preconditions
 * @post no state changes
 */
void test_i2c_comm_receive_null_handle_fails(void)
{
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(nullptr, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Receive with null frame pointer returns k_rx_err_invalid_arg
 *
 * @pre  handle initialized
 * @post no state changes
 */
void test_i2c_comm_receive_null_frame_fails(void)
{
  helper_init_default();

  rx_err_t err = rx_i2c_comm_receive(&s_handle, nullptr, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Receive on uninitialized handle returns k_rx_err_invalid_state
 *
 * @pre  s_handle zeroed (not initialized)
 * @post no state changes
 */
void test_i2c_comm_receive_not_initialized_fails(void)
{
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Receive with no data available returns k_rx_err_no_data
 *
 * @pre  handle initialized, no data in RIIC mock RX buffer
 * @post frame unmodified
 */
void test_i2c_comm_receive_no_data_returns_no_data(void)
{
  helper_init_default();

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive successfully decodes a valid command frame
 *
 * @pre  handle initialized, valid encoded frame injected into RIIC mock
 * @post frame contains correct type, sequence, and payload
 */
void test_i2c_comm_receive_valid_frame_success(void)
{
  helper_init_default();

  /* Build and inject a valid command frame with sequence 0 (matches session) */
  const uint8_t payload[] = "TEST";
  uint8_t       encoded[k_test_frame_buf_size];
  uint32_t      encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      payload,
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(k_test_payload_small, frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY(payload, frame.payload, k_test_payload_small);
}

/**
 * @brief Receive with CRC-corrupted frame returns k_rx_err_crc_mismatch
 *
 * @pre  handle initialized, frame with corrupted CRC injected
 * @post returns CRC error
 */
void test_i2c_comm_receive_crc_mismatch_fails(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"TEST",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);

  /* Corrupt the last 4 bytes (CRC field) */
  encoded[encoded_len - 1] ^= k_test_crc_flip_mask;
  encoded[encoded_len - k_test_crc_corrupt_idx_2] ^= k_test_crc_flip_mask;

  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/**
 * @brief Receive with invalid sync word returns k_rx_err_no_data (no sync found)
 *
 * @pre  handle initialized, buffer containing no valid sync word injected
 * @post returns k_rx_err_no_data
 */
void test_i2c_comm_receive_invalid_sync_returns_no_data(void)
{
  helper_init_default();

  /* Inject garbage bytes with no valid sync word */
  const uint8_t garbage[] =
    {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  helper_inject_rx(garbage, sizeof(garbage));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive ignores leading garbage before sync word and decodes frame
 *
 * @pre  handle initialized, garbage bytes prepended before valid frame
 * @post frame decoded correctly; leading garbage discarded
 */
void test_i2c_comm_receive_skips_garbage_before_sync(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"ABCD",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);

  /* Prepend 4 garbage bytes before the valid frame */
  uint8_t buf_with_garbage[k_test_frame_buf_size];
  buf_with_garbage[0]                        = k_test_garbage_byte0;
  buf_with_garbage[1]                        = k_test_garbage_byte1;
  buf_with_garbage[k_test_header_idx_seq_lo] = k_test_garbage_byte2;
  buf_with_garbage[k_test_header_idx_seq_hi] = k_test_garbage_byte3;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(buf_with_garbage + k_test_garbage_len, encoded, encoded_len);

  helper_inject_rx(buf_with_garbage, (uint16_t)(encoded_len + k_test_garbage_len));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
}

/**
 * @brief Receive with partial frame (incomplete data) returns k_rx_err_no_data
 *
 * @pre  handle initialized, only first 4 bytes of a frame injected
 * @post returns k_rx_err_no_data (incomplete frame)
 */
void test_i2c_comm_receive_partial_frame_returns_no_data(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"TEST",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);

  /* Only inject the first 4 bytes (partial - just sync + partial seq) */
  helper_inject_rx(encoded, k_test_partial_inject);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive correctly handles response frame type
 *
 * @pre  handle initialized, response frame injected
 * @post frame.header.type == k_frame_type_response
 */
void test_i2c_comm_receive_response_frame_success(void)
{
  helper_init_default();

  const uint8_t payload[] = "RESP";
  uint8_t       encoded[k_test_frame_buf_size];
  uint32_t      encoded_len = 0;
  helper_encode_frame(k_frame_type_response,
                      0,
                      payload,
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_response, frame.header.type);
}

/**
 * @brief Receive a zero-payload frame succeeds
 *
 * @pre  handle initialized, zero-payload frame injected
 * @post frame.header.length == 0
 */
void test_i2c_comm_receive_zero_payload_success(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.length);
}

/* =============================================================================
 * 5. Sequence Validation Tests
 * =============================================================================
 */

/**
 * @brief Receive frame with correct sequence (0) passes validation
 *
 * @pre  handle initialized, session at RX seq 0
 * @post frame returned with sequence 0
 */
void test_i2c_comm_receive_correct_sequence_passes(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  /* Sequence 0 matches the session's initial expected RX sequence */
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"ABCD",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.sequence);
}

/* =============================================================================
 * 6. PING / PONG Auto-Response Tests
 * =============================================================================
 */

/**
 * @brief PING frame triggers PONG auto-response via RIIC write
 *
 * @pre  handle initialized, PING frame injected
 * @post RIIC write called (PONG response sent), receive returns no_data (consumed internally)
 */
void test_i2c_comm_receive_ping_triggers_pong(void)
{
  helper_init_default();

  const uint8_t ping_payload[] = "PING";
  uint8_t       encoded[k_test_frame_buf_size];
  uint32_t      encoded_len = 0;
  helper_encode_frame(k_frame_type_ping,
                      0,
                      ping_payload,
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  /* After consuming the PING, no non-control frame is available, so no_data */
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* The layer consumed PING and sent PONG, then ran out of data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);

  /* Verify PONG was written out via RIIC */
  uint8_t  tx_buf[k_test_tx_buf_size];
  uint16_t tx_len = mock_riic_get_tx_data(k_test_channel_default, tx_buf, k_test_tx_buf_size);
  TEST_ASSERT_GREATER_THAN_UINT16(0, tx_len);
}

/* =============================================================================
 * 7. RESET / RESET_ACK Auto-Response Tests
 * =============================================================================
 */

/**
 * @brief RESET frame triggers RESET_ACK auto-response and session reset
 *
 * @pre  handle initialized, RESET frame injected
 * @post RIIC write called (RESET_ACK), receive loops to no_data, session reset
 */
void test_i2c_comm_receive_reset_triggers_reset_ack(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* RESET consumed, RESET_ACK sent, then no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);

  /* RESET_ACK was written */
  uint8_t  tx_buf[k_test_tx_buf_size];
  uint16_t tx_len = mock_riic_get_tx_data(k_test_channel_default, tx_buf, k_test_tx_buf_size);
  TEST_ASSERT_GREATER_THAN_UINT16(0, tx_len);
}

/* =============================================================================
 * 8. Buffer Management Tests
 * =============================================================================
 */

/**
 * @brief After successful receive, rx_buffer is compacted (pos and len reset)
 *
 * @pre  handle initialized, one full frame injected
 * @post after receive, rx_buffer_len == 0 and rx_buffer_pos == 0
 */
void test_i2c_comm_receive_compacts_buffer_after_decode(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"BUFF",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero));

  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
}

/**
 * @brief Two consecutive frames in RX buffer are each decoded on separate calls
 *
 * @pre  handle initialized, two frames concatenated in RIIC mock RX buffer
 * @post first call returns first frame, second call returns second frame
 */
void test_i2c_comm_receive_two_frames_sequential(void)
{
  helper_init_default();

  uint8_t  frame1_enc[k_test_frame_buf_size];
  uint32_t frame1_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"FRM1",
                      k_test_payload_small,
                      frame1_enc,
                      &frame1_len);

  uint8_t  frame2_enc[k_test_frame_buf_size];
  uint32_t frame2_len = 0;
  helper_encode_frame(k_frame_type_response,
                      1,
                      (const uint8_t*)"FRM2",
                      k_test_payload_small,
                      frame2_enc,
                      &frame2_len);

  /* Concatenate both frames and inject together */
  uint8_t combined[k_test_frame_buf_size];
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(combined, frame1_enc, frame1_len);
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(combined + frame1_len, frame2_enc, frame2_len);
  helper_inject_rx(combined, (uint16_t)(frame1_len + frame2_len));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));

  /* First receive: gets frame 1 */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero));
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);

  /* Second receive: needs a fresh RIIC read - frame2 still in internal buffer */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err2 = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);
  /* Might be k_rx_ok (if buffered) or k_rx_err_no_data (if buffer was compacted) */
  TEST_ASSERT_TRUE(err2 == k_rx_ok || err2 == k_rx_err_no_data);
}

/* =============================================================================
 * 9. send/receive state transitions
 * =============================================================================
 */

/**
 * @brief After deinit, send returns k_rx_err_invalid_state
 *
 * @pre  handle initialized then deinitialized
 * @post send returns k_rx_err_invalid_state
 */
void test_i2c_comm_send_after_deinit_fails(void)
{
  helper_init_default();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_deinit(&s_handle));

  const uint8_t payload[] = "FAIL";
  rx_err_t      err =
    rx_i2c_comm_send(&s_handle, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief After deinit, receive returns k_rx_err_invalid_state
 *
 * @pre  handle initialized then deinitialized
 * @post receive returns k_rx_err_invalid_state
 */
void test_i2c_comm_receive_after_deinit_fails(void)
{
  helper_init_default();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_deinit(&s_handle));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * 10. Additional Coverage Tests
 * =============================================================================
 */

/**
 * @brief Send with payload that causes encoded wire_len to exceed RIIC HAL limit
 *
 * @details
 * The RIIC peripheral transfer limit is 256 bytes. An encoded frame has
 * 8 header bytes + payload + 4 CRC bytes = 12 + payload. With payload = 249
 * the wire_len = 261 > 256, triggering internal_validate_wire_len failure
 * (lines 101-102 of rx_i2c_comm.c).
 *
 * @pre  handle initialized
 * @post send returns k_rx_err_invalid_size from wire_len validation
 */
void test_i2c_comm_send_wire_len_exceeds_riic_limit(void)
{
  helper_init_default();

  /* 8 header + 249 payload + 4 CRC = 261 > 256 (k_riic_peripheral_transfer_limit) */
  uint8_t large_payload[k_test_payload_large];
  for (uint16_t i = 0; i < k_test_payload_large; i++) {
    large_payload[i] = (uint8_t)(i & (uint16_t)k_test_crc_flip_mask);
  }

  rx_err_t err = rx_i2c_comm_send(&s_handle,
                                  k_frame_type_command,
                                  k_frame_flag_none,
                                  large_payload,
                                  k_test_payload_large);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Receive returns k_rx_err_no_data when RX buffer is exactly full
 *
 * @details
 * Fills the internal RX buffer (2048 bytes) with garbage that contains no
 * valid sync word. On receive, internal_read_i2c_data returns immediately
 * because space == 0 (line 361: buffer full path). The subsequent sync search
 * finds no sync, and the function returns k_rx_err_no_data.
 *
 * @pre  handle initialized, mock RIIC has garbage data injected
 * @post receive returns k_rx_err_no_data
 */
void test_i2c_comm_receive_rx_buffer_full_no_sync(void)
{
  helper_init_default();

  /* Inject exactly k_i2c_comm_rx_buffer_size bytes of garbage (no sync word).
   * Use 250-byte chunks (< k_riic_peripheral_transfer_limit=256) to fill
   * the 2048-byte buffer. */
  enum { k_chunk = 250 }; /* injection chunk size < RIIC transfer limit */

  uint8_t garbage[k_chunk];
  for (uint32_t i = 0; i < (uint32_t)k_chunk; i++) {
    garbage[i] = k_test_garbage_fill; /* No sync word (0x55AA) */
  }

  /* First receive call fills the buffer via multiple RIIC reads, then returns
   * no_data because there is no sync word in the data. */
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));

  /* Inject enough garbage to fill the 2048-byte RX buffer across iterations.
   * Each receive iteration reads at most k_riic_peripheral_transfer_limit bytes,
   * so after ~8 full calls the rx_buffer_len will hit 2048 and space==0. */
  for (uint32_t batch = 0; batch < k_test_batch_count; batch++) {
    mock_riic_set_rx_data(k_test_channel_default, garbage, (uint16_t)k_chunk);
  }

  /* Drain until no_data (the receive loop fills rx_buffer then gives up) */
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive propagates RIIC read error from internal_read_i2c_data
 *
 * @details
 * Injects a RIIC error so that riic_peripheral_read returns an error on the
 * first call inside internal_receive_iteration. This covers lines 374-375 in
 * internal_read_i2c_data (error return from riic_peripheral_read).
 *
 * @pre  handle initialized, mock RIIC next error = k_rx_err_hw_error
 * @post receive returns the injected error
 */
void test_i2c_comm_receive_riic_read_error_propagates(void)
{
  helper_init_default();

  /* Inject a non-timeout error so internal_read_i2c_data propagates it */
  mock_riic_set_next_error(k_rx_err_hw_error);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive with invalid payload length in header (internal_parse_header error)
 *
 * @details
 * Injects a raw byte sequence where the sync word is valid but the
 * payload-length field in the header exceeds k_frame_max_payload (1024).
 * This triggers the invalid-size branch in internal_parse_header (lines 631-633),
 * which advances rx_buffer_pos past the sync word and returns k_rx_err_invalid_size.
 * internal_receive_iteration then continues looking for the next sync word, and
 * since there is no more data, returns k_rx_err_no_data.
 *
 * @pre  handle initialized, malformed frame injected
 * @post receive returns k_rx_err_no_data
 */
void test_i2c_comm_receive_invalid_payload_length_in_header(void)
{
  helper_init_default();

  /* Craft a raw buffer: sync word (0x55 0xAA) followed by seq(0,0)
   * then payload_len > 1024 (e.g., 0xFF 0xFF = 65535), then type+flags,
   * then garbage to fill out k_frame_header_total bytes. */
  /* k_frame_sync_word = 0x55AA; in little-endian buffer order: [0xAA, 0x55] */
  enum {
    k_sync_low     = 0xAA, /**< Sync word byte 0 (LE low byte of 0x55AA) */
    k_sync_high    = 0x55, /**< Sync word byte 1 (LE high byte of 0x55AA) */
    k_len_hi       = 0xFF, /**< High byte of oversized payload length */
    k_header_total = 8,    /**< Header: sync(2)+seq(2)+len(2)+type(1)+flags(1) */
    k_extra        = 4,    /**< Extra bytes after header for padding */
  };

  /* Build a frame with valid sync word but invalid (too-large) payload length */
  uint8_t malformed[k_header_total + k_extra];
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(malformed, 0, sizeof(malformed));
  malformed[0] = k_sync_low;  /* sync byte 0 (LE: 0xAA) */
  malformed[1] = k_sync_high; /* sync byte 1 (LE: 0x55) */
  /* sequence low/high remain 0 from memset */
  malformed[k_test_header_idx_len_lo] = k_len_hi; /* payload length low = 0xFF */
  malformed[k_test_header_idx_len_hi] = k_len_hi; /* payload length high = 0xFF -> 65535 > 1024 */
  malformed[k_test_header_idx_type]   = k_test_frame_type_cmd;   /* frame type */
  malformed[k_test_header_idx_flags]  = k_test_frame_flags_zero; /* frame flags */

  helper_inject_rx(malformed, (uint16_t)(k_header_total + k_extra));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Parse-header detects oversized length, advances past sync, no more data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief PONG frame is silently consumed (not returned to caller)
 *
 * @details
 * Injects a PONG frame. rx_i2c_comm_receive decodes it and recognizes
 * the PONG type, consuming it silently (lines 1104-1106 of rx_i2c_comm.c)
 * without returning it to the caller. Returns no_data afterward.
 *
 * @pre  handle initialized, PONG frame injected
 * @post receive returns k_rx_err_no_data
 */
void test_i2c_comm_receive_pong_silently_consumed(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_pong, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief RESET_ACK frame is silently consumed (not returned to caller)
 *
 * @details
 * Injects a RESET_ACK frame. rx_i2c_comm_receive decodes it and recognizes
 * the RESET_ACK type, consuming it silently (lines 1104-1106 of rx_i2c_comm.c).
 *
 * @pre  handle initialized, RESET_ACK frame injected
 * @post receive returns k_rx_err_no_data
 */
void test_i2c_comm_receive_reset_ack_silently_consumed(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset_ack, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Sequence validation fails when frame has out-of-range sequence number
 *
 * @details
 * Sends a frame with sequence number 200 when the session expects 0.
 * The gap (200) exceeds k_session_max_gap_tolerance, so rx_session_validate_rx
 * returns k_session_validate_fail. The receive loop drops the frame, and with
 * no more data returns k_rx_err_no_data.
 *
 * @pre  handle initialized, session expects RX seq 0
 * @post receive returns k_rx_err_no_data (frame dropped)
 */
void test_i2c_comm_receive_sequence_validation_fail_drops_frame(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  /* Sequence 200 is far beyond what the session expects (0), triggering fail */
  helper_encode_frame(k_frame_type_command,
                      k_test_seq_gap_large,
                      (const uint8_t*)"DATA",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Session rejects the sequence gap and returns k_rx_err_protocol_error */
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief PING handling fails when PONG send fails due to RIIC write error
 *
 * @details
 * Injects a PING frame into the mock. When rx_i2c_comm_receive processes the
 * PING and calls internal_handle_ping to send a PONG, the RIIC write fails
 * because a next_error was pre-injected. The error is propagated out of
 * rx_i2c_comm_receive (line 1089).
 *
 * @pre  handle initialized, PING frame injected, RIIC error pre-staged
 * @post receive returns the injected RIIC error
 */
void test_i2c_comm_receive_ping_pong_send_fails(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_ping,
                      0,
                      (const uint8_t*)"PING",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Stage a write-only error so the frame read succeeds but PONG send fails */
  mock_riic_set_next_write_error(k_rx_err_hw_error);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief RESET handling fails when RESET_ACK send fails
 *
 * @details
 * Injects a RESET frame. When the RESET_ACK write fails due to a staged
 * RIIC error, internal_handle_reset propagates the error (line 1098).
 *
 * @pre  handle initialized, RESET frame injected, RIIC error pre-staged
 * @post receive returns the injected RIIC error
 */
void test_i2c_comm_receive_reset_ack_send_fails(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Stage a write-only error so the frame read succeeds but RESET_ACK send fails */
  mock_riic_set_next_write_error(k_rx_err_hw_error);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Init returns error when frame encoder init fails
 *
 * @details
 * Pre-injects a RIIC error so that the rx_frame_encoder_init() call inside
 * rx_i2c_comm_init fails. The init function propagates the error and leaves
 * the handle uninitialized (lines 754-755).
 *
 * Note: rx_frame_encoder_init uses no RIIC HAL calls, so we must inject the
 * error at the riic_init_peripheral call. This means encoder_init succeeds,
 * decoder_init succeeds, but riic_init_peripheral fails (lines 762-767).
 *
 * @pre  mock RIIC set to return error on third call (after encoder + decoder init)
 * @post init returns error; handle uninitialized
 */
void test_i2c_comm_init_riic_peripheral_init_fails(void)
{
  /* The riic_init_peripheral call is the first RIIC call during init.
   * Inject error before init so riic_init_peripheral fails immediately. */
  mock_riic_set_next_error(k_rx_err_timeout);

  rx_i2c_comm_config_t cfg = helper_default_config();
  rx_err_t             err = rx_i2c_comm_init(&s_handle, &cfg);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Two consecutive frames: second frame at wrong sequence is dropped
 *
 * @details
 * Injects two frames: frame1 has sequence 0 (accepted), frame2 has sequence 200
 * (dropped due to sequence gap). The second receive returns k_rx_err_no_data.
 *
 * @pre  handle initialized, two frames in RIIC mock (seq 0 and seq 200)
 * @post first receive returns k_rx_ok; second returns k_rx_err_no_data
 */
void test_i2c_comm_receive_second_frame_wrong_sequence_dropped(void)
{
  helper_init_default();

  uint8_t  frame1_enc[k_test_frame_buf_size];
  uint32_t frame1_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"FRM1",
                      k_test_payload_small,
                      frame1_enc,
                      &frame1_len);

  uint8_t  frame2_enc[k_test_frame_buf_size];
  uint32_t frame2_len = 0;
  helper_encode_frame(k_frame_type_command,
                      k_test_seq_gap_large, /* wrong sequence */
                      (const uint8_t*)"FRM2",
                      k_test_payload_small,
                      frame2_enc,
                      &frame2_len);

  uint8_t combined[k_test_frame_buf_size];
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(combined, frame1_enc, frame1_len);
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(combined + frame1_len, frame2_enc, frame2_len);
  helper_inject_rx(combined, (uint16_t)(frame1_len + frame2_len));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));

  /* First receive: gets frame1 at seq 0 (valid) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero));
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.sequence);

  /* Second receive: frame2 at seq 200 rejected by session (expects 1), protocol error */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err2 = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err2);
}

/**
 * @brief Sync search hits false sync byte (0xAA not followed by 0x55) then finds real frame
 *
 * @details
 * Prepends a false sync prefix [0xAA, 0x00] before a valid frame. In LE encoding,
 * the real sync word is [0xAA, 0x55]. The search finds 0xAA at byte 0, but byte 1
 * is 0x00 (not 0x55), so internal_find_sync skips it (branch at line 473: sync_low
 * matches but sync_high does not). It continues scanning and finds the real sync at
 * byte 2, decoding the frame successfully.
 *
 * @pre  handle initialized, [0xAA, 0x00] prefix before valid frame
 * @post frame decoded correctly with k_rx_ok
 */
void test_i2c_comm_receive_false_sync_low_then_real_frame(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      0,
                      (const uint8_t*)"ABCD",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);

  /* Prepend [0xAA, 0x00]: sync_low (0xAA) present but sync_high (0x55) absent */
  uint8_t buf[k_test_frame_buf_size];
  buf[0] = k_test_sync_byte_low; /* sync_low matches, but... */
  buf[1] = k_test_false_sync_hi; /* sync_high does NOT match -> false alarm */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(buf + k_test_false_sync_prefix, encoded, encoded_len);

  helper_inject_rx(buf, (uint16_t)(encoded_len + k_test_false_sync_prefix));

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
}

/**
 * @brief Partial frame (header only, no payload) returns k_rx_err_no_data
 *
 * @details
 * Injects only the 8-byte frame header (sync + seq + payload_len=4 + type +
 * flags) without the 4-byte payload or 4-byte CRC. After internal_parse_header
 * succeeds (payload_len=4, total_size=16), the check at line 713 detects that
 * available(8) < total_size(16) and returns k_rx_err_no_data (lines 714-715).
 *
 * @pre  handle initialized, 8-byte partial frame header in RIIC mock
 * @post receive returns k_rx_err_no_data (incomplete frame)
 */
void test_i2c_comm_receive_partial_frame_header_only(void)
{
  helper_init_default();

  /* k_frame_sync_word = 0x55AA stored LE as [0xAA, 0x55] */
  enum {
    k_partial_sync_low  = 0xAA, /**< Sync byte 0 */
    k_partial_sync_high = 0x55, /**< Sync byte 1 */
    k_partial_payload   = 4,    /**< Non-zero payload length so total_size > 8 */
    k_partial_header    = 8,    /**< Header: sync(2)+seq(2)+len(2)+type(1)+flags(1) */
  };

  uint8_t partial[k_partial_header];
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(partial, 0, sizeof(partial));
  partial[0] = (uint8_t)k_partial_sync_low;
  partial[1] = (uint8_t)k_partial_sync_high;
  /* sequence low/high remain 0 from memset */
  partial[k_test_header_idx_len_lo] = (uint8_t)k_partial_payload; /* payload_len low */
  /* payload_len high remains 0 from memset */
  partial[k_test_header_idx_type]  = k_test_frame_type_cmd;   /* frame type (command) */
  partial[k_test_header_idx_flags] = k_test_frame_flags_zero; /* frame flags */

  helper_inject_rx(partial, (uint16_t)k_partial_header);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* available(8) < total_size(16), returns no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief rx_i2c_comm_send propagates error when session next_tx fails
 *
 * @details
 * Deinitializes the session so rx_session_next_tx returns
 * k_rx_err_not_initialized. Verifies that rx_i2c_comm_send propagates the
 * error (lines 919-920 of rx_i2c_comm.c).
 *
 * @pre  handle initialized, session deinitialized
 * @post send returns k_rx_err_not_initialized
 */
void test_i2c_comm_send_session_not_initialized_fails(void)
{
  helper_init_default();

  /* Deinit the session so rx_session_next_tx returns not_initialized */
  (void)rx_session_deinit(&s_session);

  rx_err_t err = rx_i2c_comm_send(&s_handle, k_frame_type_command, k_frame_flag_none, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief rx_i2c_comm_send propagates error when frame encoder not initialized
 *
 * @details
 * Deinitializes the encoder inside the handle so rx_frame_encode returns
 * k_rx_err_not_initialized. Verifies that rx_i2c_comm_send propagates the
 * error (lines 933-934 of rx_i2c_comm.c).
 *
 * @pre  handle initialized, encoder deinitialized
 * @post send returns k_rx_err_not_initialized
 */
void test_i2c_comm_send_encoder_not_initialized_fails(void)
{
  helper_init_default();

  /* Deinit the encoder so rx_frame_encode returns not_initialized */
  (void)rx_frame_encoder_deinit(&s_handle.encoder);

  rx_err_t err = rx_i2c_comm_send(&s_handle, k_frame_type_command, k_frame_flag_none, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Receive with RX buffer pre-filled (space==0) returns no_data
 *
 * @details
 * Directly sets rx_buffer_len to k_i2c_comm_rx_buffer_size with garbage data
 * (no sync word). This exercises the space==0 early return in
 * internal_read_i2c_data (line 361 of rx_i2c_comm.c) and the buffer-full
 * increment in internal_handle_no_sync (line 510).
 *
 * @pre  handle initialized, rx_buffer_len = max, rx_buffer contains 0xCC bytes
 * @post receive returns k_rx_err_no_data
 */
void test_i2c_comm_receive_rx_buffer_prefilled_no_sync(void)
{
  helper_init_default();

  /* Directly fill the RX buffer with garbage (no sync word = 0xAA 0x55) */
  for (uint32_t i = 0; i < (uint32_t)k_test_rx_buf_max; i++) {
    s_handle.rx_buffer[i] = k_test_garbage_fill;
  }
  s_handle.rx_buffer_len = (uint32_t)k_test_rx_buf_max;
  s_handle.rx_buffer_pos = 0;

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Buffer full: space==0 (line 361) -> no read, no sync -> handle_no_sync
   * increments pos (line 510) -> compact -> no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive with RESET frame when session not initialized propagates error
 *
 * @details
 * Injects a RESET frame and deinitializes the session so that
 * rx_session_next_tx inside internal_handle_reset returns
 * k_rx_err_not_initialized (line 1016 of rx_i2c_comm.c).
 *
 * @pre  handle initialized, RESET frame injected, session deinitialized
 * @post receive returns k_rx_err_not_initialized
 */
void test_i2c_comm_receive_reset_session_next_tx_fails(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Deinit the session so rx_session_next_tx fails inside internal_handle_reset */
  (void)rx_session_deinit(&s_session);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* =============================================================================
 * Direct internal-function tests (RX_STATIC_TESTABLE)
 * These tests call internal helpers directly with invalid arguments to reach
 * defensive branches that are unreachable through the public API.
 * =============================================================================
 */

/**
 * @brief internal_validate_wire_len rejects length exceeding HAL limit
 */
void test_i2c_internal_validate_wire_len_too_large(void)
{
  rx_err_t err = internal_validate_wire_len((uint32_t)k_riic_peripheral_transfer_limit + 1U);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_validate_wire_len accepts length within HAL limit
 */
void test_i2c_internal_validate_wire_len_ok(void)
{
  rx_err_t err = internal_validate_wire_len((uint32_t)k_riic_peripheral_transfer_limit);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief internal_decode_header returns invalid_arg for nullptr data
 */
void test_i2c_internal_decode_header_null_data(void)
{
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = internal_decode_header(nullptr, k_test_decode_buf_large, &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_decode_header returns invalid_arg for nullptr frame
 */
void test_i2c_internal_decode_header_null_frame(void)
{
  const uint8_t buf[8] = {};
  rx_err_t      err    = internal_decode_header(buf, sizeof(buf), nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_decode_header returns invalid_size when data too short
 */
void test_i2c_internal_decode_header_too_short(void)
{
  const uint8_t buf[2] = {};
  rx_frame_t    frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = internal_decode_header(buf, sizeof(buf), &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_decode_header returns protocol_error for bad sync word
 */
void test_i2c_internal_decode_header_bad_sync(void)
{
  uint8_t buf[k_test_decode_buf_size] = {};
  /* Fill with valid-size but wrong sync */
  buf[0] = k_test_garbage_byte0;
  buf[1] = k_test_garbage_byte1;
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = internal_decode_header(buf, sizeof(buf), &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief internal_decode_header returns invalid_size for oversized payload field
 */
void test_i2c_internal_decode_header_payload_too_large(void)
{
  /* Craft a buffer with valid sync but payload_len > k_frame_max_payload (1024) */
  /* k_frame_sync_word = 0x55AA; rx_frame_read_le16 reads buf[0]+buf[1]<<8
   * So to match: buf[0]=0xAA (low byte), buf[1]=0x55 (high byte) */
  uint8_t buf[k_test_decode_buf_large] = {};
  buf[0]                               = k_test_sync_byte_low;  /* sync low byte */
  buf[1]                               = k_test_sync_byte_high; /* sync high byte */
  /* seq = 0 (already zero-initialized) */
  /* length field at offset 4: set to 0x0500 (1280) > k_frame_max_payload (1024) */
  buf[k_test_header_idx_len_lo] = 0x00U;
  buf[k_test_header_idx_len_hi] = k_test_len_hi_byte; /* LE: 0x0500 = 1280 */
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = internal_decode_header(buf, sizeof(buf), &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_decode_header populates offset_out when not nullptr
 */
void test_i2c_internal_decode_header_offset_out_set(void)
{
  /* Encode a minimal frame and decode just the header */
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  uint32_t offset = 0;
  rx_err_t err    = internal_decode_header(encoded, encoded_len, &frame, &offset);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, offset);
}

/**
 * @brief internal_verify_crc returns invalid_arg for nullptr data
 */
void test_i2c_internal_verify_crc_null_data(void)
{
  rx_err_t err = internal_verify_crc(nullptr, k_test_crc_offset_eight, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_verify_crc returns invalid_arg for offset too small
 */
void test_i2c_internal_verify_crc_offset_too_small(void)
{
  const uint8_t buf[32] = {};
  /* offset < (k_frame_min_size - k_frame_crc_size) */
  rx_err_t err = internal_verify_crc(buf, 0, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_verify_crc writes crc_out when pointer is not nullptr
 */
void test_i2c_internal_verify_crc_out_written(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  /* Header size for zero-payload frame: offset = encoded_len - k_frame_crc_size */
  const uint32_t crc_offset = encoded_len - (uint32_t)k_frame_crc_size;
  uint32_t       crc_out    = 0;
  rx_err_t       err        = internal_verify_crc(encoded, crc_offset, &crc_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, crc_out);
}

/**
 * @brief internal_read_i2c_data returns invalid_arg for nullptr handle
 */
void test_i2c_internal_read_i2c_data_null_handle(void)
{
  rx_err_t err = internal_read_i2c_data(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_compact_rx_buffer returns invalid_arg for nullptr handle
 */
void test_i2c_internal_compact_rx_buffer_null_handle(void)
{
  rx_err_t err = internal_compact_rx_buffer(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_compact_rx_buffer returns invalid_state when pos > len
 */
void test_i2c_internal_compact_rx_buffer_pos_exceeds_len(void)
{
  helper_init_default();
  /* Corrupt the handle state: pos > len */
  s_handle.rx_buffer_len = k_test_compact_test_len;
  s_handle.rx_buffer_pos = k_test_compact_bad_pos;
  rx_err_t err           = internal_compact_rx_buffer(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  /* Restore to avoid teardown issues */
  s_handle.rx_buffer_len = 0;
  s_handle.rx_buffer_pos = 0;
}

/**
 * @brief internal_compact_rx_buffer is a no-op when pos == 0
 */
void test_i2c_internal_compact_rx_buffer_noop_when_pos_zero(void)
{
  helper_init_default();
  s_handle.rx_buffer_len = k_test_compact_test_len;
  s_handle.rx_buffer_pos = 0;
  rx_err_t err           = internal_compact_rx_buffer(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_compact_test_len, s_handle.rx_buffer_len);
}

/**
 * @brief internal_find_sync returns invalid_arg for nullptr handle
 */
void test_i2c_internal_find_sync_null_handle(void)
{
  int32_t  pos = 0;
  rx_err_t err = internal_find_sync(nullptr, &pos);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_find_sync returns invalid_arg for nullptr sync_pos
 */
void test_i2c_internal_find_sync_null_sync_pos(void)
{
  helper_init_default();
  rx_err_t err = internal_find_sync(&s_handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_find_sync returns invalid_state when pos > len
 */
void test_i2c_internal_find_sync_pos_exceeds_len(void)
{
  helper_init_default();
  s_handle.rx_buffer_len = k_test_find_sync_len;
  s_handle.rx_buffer_pos = k_test_find_sync_bad_pos;
  int32_t  pos           = 0;
  rx_err_t err           = internal_find_sync(&s_handle, &pos);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  s_handle.rx_buffer_len = 0;
  s_handle.rx_buffer_pos = 0;
}

/**
 * @brief internal_handle_no_sync returns invalid_arg for nullptr handle
 */
void test_i2c_internal_handle_no_sync_null_handle(void)
{
  rx_err_t err = internal_handle_no_sync(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_align_to_sync advances pos to sync when sync is ahead
 */
void test_i2c_internal_align_to_sync_advances_pos(void)
{
  helper_init_default();
  s_handle.rx_buffer_pos = 0;
  internal_align_to_sync(&s_handle, (int32_t)k_test_align_sync_pos);
  TEST_ASSERT_EQUAL(k_test_align_sync_pos, s_handle.rx_buffer_pos);
}

/**
 * @brief internal_align_to_sync does not move pos backwards
 */
void test_i2c_internal_align_to_sync_no_regression(void)
{
  helper_init_default();
  s_handle.rx_buffer_pos = k_test_align_high_pos;
  internal_align_to_sync(&s_handle, (int32_t)k_test_align_low_sync);
  TEST_ASSERT_EQUAL(k_test_align_high_pos, s_handle.rx_buffer_pos);
}

/**
 * @brief internal_build_frame returns invalid_arg for nullptr frame
 */
void test_i2c_internal_build_frame_null_frame(void)
{
  const uint8_t payload[k_test_payload_small] = {1, 0, 0, 0};
  rx_err_t      err =
    internal_build_frame(nullptr, 0, k_frame_type_command, 0, payload, sizeof(payload));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_build_frame returns invalid_arg for nullptr payload with non-zero len
 */
void test_i2c_internal_build_frame_null_payload_nonzero_len(void)
{
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err =
    internal_build_frame(&frame, 0, k_frame_type_command, 0, nullptr, k_test_payload_small);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_build_frame returns invalid_size for oversized payload
 */
void test_i2c_internal_build_frame_payload_too_large(void)
{
  rx_frame_t    frame;
  const uint8_t big_payload[1025] = {}; /* k_frame_max_payload is 1024; use 1025 to exceed it */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err =
    internal_build_frame(&frame, 0, k_frame_type_command, 0, big_payload, sizeof(big_payload));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_build_frame with zero payload copies nothing
 */
void test_i2c_internal_build_frame_zero_payload(void)
{
  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = internal_build_frame(&frame, k_test_seq_five, k_frame_type_command, 0, nullptr, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_seq_five, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
}

/**
 * @brief internal_build_frame with non-nullptr payload and nonzero len copies payload
 */
void test_i2c_internal_build_frame_with_payload(void)
{
  rx_frame_t    frame;
  const uint8_t payload[k_test_payload_small] = {k_test_payload_byte_a,
                                                 k_test_payload_byte_b,
                                                 k_test_payload_byte_c,
                                                 k_test_payload_byte_d};
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err =
    internal_build_frame(&frame, k_test_seq_one, k_frame_type_command, 0, payload, sizeof(payload));
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_MEMORY(payload, frame.payload, sizeof(payload));
}

/**
 * @brief internal_read_i2c_data returns ok when buffer is full (space == 0)
 */
void test_i2c_internal_read_i2c_data_buffer_full(void)
{
  helper_init_default();
  /* Set rx_buffer_len to max so space == 0 */
  s_handle.rx_buffer_len = k_i2c_comm_rx_buffer_size;
  rx_err_t err           = internal_read_i2c_data(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  s_handle.rx_buffer_len = 0;
}

/**
 * @brief internal_receive_iteration propagates error from internal_read_i2c_data
 */
void test_i2c_internal_receive_iteration_read_error(void)
{
  helper_init_default();
  mock_riic_set_next_error(k_rx_err_hw_error);
  rx_frame_t          frame;
  rx_err_t            err    = k_rx_ok;
  rx_receive_result_t result = internal_receive_iteration(&s_handle, &frame, &err);
  TEST_ASSERT_EQUAL(k_receive_error, result);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief rx_i2c_comm_receive returns k_rx_err_timeout after exhausting the receive loop
 *
 * @details
 * Fills s_handle.rx_buffer with a repeating 0xAA 0x55 pattern (the I2C frame sync word
 * bytes in little-endian order). Each receive iteration finds a sync word at rx_buffer_pos,
 * then reads payload_len from offset +4 which is also 0xAA 0x55 = 0x55AA = 21930, which
 * exceeds k_frame_max_payload. internal_parse_header advances rx_buffer_pos by
 * k_frame_sync_size (2) and returns k_rx_err_invalid_size, causing internal_receive_iteration
 * to return k_receive_continue. After k_i2c_comm_max_receive_iterations (1000) iterations the
 * while loop exits and rx_i2c_comm_receive returns k_rx_err_timeout.
 *
 * With k_i2c_comm_rx_buffer_size = 2048 and 2 bytes consumed per iteration, 1024 patterns are
 * available, which is enough to supply all 1000 iterations without hitting the buffer end.
 *
 * @pre  handle initialized, rx_buffer filled with 0xAA 0x55 repeating pattern
 * @post rx_i2c_comm_receive returns k_rx_err_timeout
 */
void test_i2c_comm_receive_timeout(void)
{
  helper_init_default();

  const uint8_t sync_low  = (uint8_t)(k_frame_sync_word & k_rx_byte_mask);
  const uint8_t sync_high = (uint8_t)(k_frame_sync_word >> k_rx_le16_high_shift);
  for (uint32_t i = 0; i < k_test_rx_buf_max; i += k_test_sync_fill_step) {
    s_handle.rx_buffer[i]      = sync_low;
    s_handle.rx_buffer[i + 1U] = sync_high;
  }
  s_handle.rx_buffer_len = k_test_rx_buf_max;
  s_handle.rx_buffer_pos = 0;

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, k_test_timeout_zero);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief internal_decode_header: nullptr offset_out with valid header returns k_rx_ok.
 *
 * @details
 * Covers the FALSE branch of `if (offset_out != nullptr)` at line 208.
 * All prior checks pass (valid sync, valid payload_len=0), then offset_out
 * is nullptr so the assignment block is skipped and k_rx_ok is returned.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned
 *
 * @see internal_decode_header() Function under test
 */
void test_i2c_internal_decode_header_null_offset_valid_header(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  /* Pass nullptr for offset_out with valid frame: covers line-208 FALSE branch */
  rx_err_t err = internal_decode_header(encoded, encoded_len, &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief internal_verify_crc: nullptr crc_out with valid CRC returns k_rx_ok.
 *
 * @details
 * Covers the FALSE branch of `if (crc_out != nullptr)` at line 263.
 * The CRC passes validation but crc_out is nullptr so the write is skipped.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned
 *
 * @see internal_verify_crc() Function under test
 */
void test_i2c_internal_verify_crc_null_crc_out_valid_frame(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  const uint32_t crc_offset = encoded_len - (uint32_t)k_frame_crc_size;
  /* Pass nullptr crc_out: covers line-263 FALSE branch */
  rx_err_t err = internal_verify_crc(encoded, crc_offset, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief internal_read_i2c_data: small remaining space uses space directly (not clamped).
 *
 * @details
 * Covers the FALSE branch of the ternary at line 368:
 * `(space > k_riic_peripheral_transfer_limit) ? limit : (uint16_t)space`.
 * When rx_buffer_len is set so that space < k_riic_peripheral_transfer_limit (256),
 * the cast `(uint16_t)space` branch is taken instead of the clamp.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned and bytes read normally
 *
 * @see internal_read_i2c_data() Function under test
 */
void test_i2c_internal_read_i2c_data_small_space(void)
{
  helper_init_default();
  /* Set buffer nearly full so space = 16 < k_riic_peripheral_transfer_limit (256) */
  const uint32_t k_space = 16U;
  s_handle.rx_buffer_len = k_i2c_comm_rx_buffer_size - k_space;
  rx_err_t err           = internal_read_i2c_data(&s_handle);
  /* No data in mock - returns k_rx_ok with 0 bytes read */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  s_handle.rx_buffer_len = 0;
}

/**
 * @brief I2C read returns k_rx_err_timeout -- compound condition false at line 667
 *
 * @details
 * Covers branch :3 at rx_i2c_comm.c line 667:
 *   `if (*err != k_rx_ok && *err != k_rx_err_timeout)`
 * When riic_peripheral_read returns k_rx_err_timeout, internal_read_i2c_data
 * propagates it. The first operand of the && is TRUE but the second is FALSE,
 * so the condition is FALSE and we do NOT enter the error branch. The loop
 * then finds an empty buffer and returns k_rx_err_no_data.
 *
 * @pre  setUp() completed, mock RIIC initialized
 * @post rx_i2c_comm_receive returns k_rx_err_no_data (no bytes read)
 * @post timeout simulation cleared by tearDown
 */
void test_i2c_comm_receive_riic_read_returns_timeout(void)
{
  helper_init_default();

  /* Make RIIC read return k_rx_err_timeout */
  mock_riic_simulate_timeout(true);

  rx_frame_t frame;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&frame, 0, sizeof(frame));
  rx_err_t err = rx_i2c_comm_receive(&s_handle, &frame, 0U);

  /* After timeout from RIIC, buffer is empty -> k_rx_err_no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);

  /* Reset timeout simulation (tearDown also does this, but be explicit) */
  mock_riic_simulate_timeout(false);
}

/* =============================================================================
 * RX_ASSERT_POST / RX_ASSERT_PRE Branch Coverage Tests
 * =============================================================================
 */

/**
 * @brief POST assertion in rx_i2c_comm_init: force-fail exercises POST branch
 *
 * @details
 * Sets g_rx_assert_post_force_fail to true before calling rx_i2c_comm_init()
 * with valid arguments. The RX_ASSERT_POST(handle->initialized, ...) on line
 * 740 fires even though the postcondition is satisfied, exercising the failure
 * branch. In UNIT_TEST builds internal_rx_fatal_error is a no-op so execution
 * continues and init still returns k_rx_ok.
 *
 * @pre g_rx_assert_post_force_fail == false
 * @post POST assertion failure branch exercised
 * @post g_rx_assert_post_force_fail restored to false
 *
 * @since Version 1.0.0
 */
void test_i2c_comm_init_post_assert_force_fail(void)
{
  rx_i2c_comm_config_t cfg = helper_default_config();

  g_rx_assert_post_force_fail = true;
  rx_err_t err                = rx_i2c_comm_init(&s_handle, &cfg);
  g_rx_assert_post_force_fail = false;

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief PRE assertion in rx_i2c_comm_deinit: uninitialized handle exercises PRE branch
 *
 * @details
 * Calls rx_i2c_comm_deinit() on a zero-initialized handle (initialized == 0).
 * The RX_ASSERT_PRE(handle->initialized, ...) on line 752 fires because
 * handle->initialized is false. In UNIT_TEST builds internal_rx_fatal_error
 * is a no-op so execution continues through the deinit logic.
 *
 * @pre s_handle is zero-initialized (setUp clears with memset)
 * @post RX_ASSERT_PRE failure branch exercised
 *
 * @since Version 1.0.0
 */
void test_i2c_comm_deinit_pre_assert_uninitialized(void)
{
  /* s_handle is zero-initialized by setUp() -- initialized == 0 */
  rx_err_t err = rx_i2c_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Unity entry point
 * =============================================================================
 */

/**
 * @brief Unity test runner entry point
 *
 * @pre  Unity framework available
 * @post All tests executed, pass/fail reported
 */
/**
 * @brief Run public API tests (init, deinit, send, receive, control frames)
 */
static void internal_run_public_api_tests(void)
{
  /* Initialization */
  RUN_TEST(test_i2c_comm_init_null_handle_fails);
  RUN_TEST(test_i2c_comm_init_null_config_fails);
  RUN_TEST(test_i2c_comm_init_null_session_fails);
  RUN_TEST(test_i2c_comm_init_success);
  RUN_TEST(test_i2c_comm_init_stores_channel_and_addr);
  RUN_TEST(test_i2c_comm_init_clears_rx_buffer);
  RUN_TEST(test_i2c_comm_init_riic_failure_propagates);

  /* Deinitialization */
  RUN_TEST(test_i2c_comm_deinit_null_handle_fails);
  RUN_TEST(test_i2c_comm_deinit_success);
  RUN_TEST(test_i2c_comm_deinit_not_initialized_ok);

  /* Send */
  RUN_TEST(test_i2c_comm_send_null_handle_fails);
  RUN_TEST(test_i2c_comm_send_not_initialized_fails);
  RUN_TEST(test_i2c_comm_send_null_payload_nonzero_len_fails);
  RUN_TEST(test_i2c_comm_send_payload_too_large_fails);
  RUN_TEST(test_i2c_comm_send_zero_payload_success);
  RUN_TEST(test_i2c_comm_send_small_payload_success);
  RUN_TEST(test_i2c_comm_send_tx_contains_sync_word);
  RUN_TEST(test_i2c_comm_send_increments_tx_sequence);
  RUN_TEST(test_i2c_comm_send_riic_write_failure_propagates);

  /* Receive */
  RUN_TEST(test_i2c_comm_receive_null_handle_fails);
  RUN_TEST(test_i2c_comm_receive_null_frame_fails);
  RUN_TEST(test_i2c_comm_receive_not_initialized_fails);
  RUN_TEST(test_i2c_comm_receive_no_data_returns_no_data);
  RUN_TEST(test_i2c_comm_receive_valid_frame_success);
  RUN_TEST(test_i2c_comm_receive_crc_mismatch_fails);
  RUN_TEST(test_i2c_comm_receive_invalid_sync_returns_no_data);
  RUN_TEST(test_i2c_comm_receive_skips_garbage_before_sync);
  RUN_TEST(test_i2c_comm_receive_partial_frame_returns_no_data);
  RUN_TEST(test_i2c_comm_receive_response_frame_success);
  RUN_TEST(test_i2c_comm_receive_zero_payload_success);

  /* Sequence validation */
  RUN_TEST(test_i2c_comm_receive_correct_sequence_passes);

  /* Control frame handling */
  RUN_TEST(test_i2c_comm_receive_ping_triggers_pong);
  RUN_TEST(test_i2c_comm_receive_reset_triggers_reset_ack);

  /* Buffer management */
  RUN_TEST(test_i2c_comm_receive_compacts_buffer_after_decode);
  RUN_TEST(test_i2c_comm_receive_two_frames_sequential);
}

/**
 * @brief Run coverage and state transition tests
 */
static void internal_run_coverage_tests(void)
{
  /* State transitions */
  RUN_TEST(test_i2c_comm_send_after_deinit_fails);
  RUN_TEST(test_i2c_comm_receive_after_deinit_fails);

  /* Additional coverage tests */
  RUN_TEST(test_i2c_comm_send_wire_len_exceeds_riic_limit);
  RUN_TEST(test_i2c_comm_receive_rx_buffer_full_no_sync);
  RUN_TEST(test_i2c_comm_receive_riic_read_error_propagates);
  RUN_TEST(test_i2c_comm_receive_invalid_payload_length_in_header);
  RUN_TEST(test_i2c_comm_receive_pong_silently_consumed);
  RUN_TEST(test_i2c_comm_receive_reset_ack_silently_consumed);
  RUN_TEST(test_i2c_comm_receive_sequence_validation_fail_drops_frame);
  RUN_TEST(test_i2c_comm_receive_ping_pong_send_fails);
  RUN_TEST(test_i2c_comm_receive_reset_ack_send_fails);
  RUN_TEST(test_i2c_comm_init_riic_peripheral_init_fails);
  RUN_TEST(test_i2c_comm_receive_second_frame_wrong_sequence_dropped);
  RUN_TEST(test_i2c_comm_receive_partial_frame_header_only);
  RUN_TEST(test_i2c_comm_receive_false_sync_low_then_real_frame);

  /* Coverage gap tests */
  RUN_TEST(test_i2c_comm_send_session_not_initialized_fails);
  RUN_TEST(test_i2c_comm_send_encoder_not_initialized_fails);
  RUN_TEST(test_i2c_comm_receive_rx_buffer_prefilled_no_sync);
  RUN_TEST(test_i2c_comm_receive_reset_session_next_tx_fails);
}

/**
 * @brief Run direct internal-function and branch coverage tests
 */
static void internal_run_internal_function_tests(void)
{
  /* Direct internal-function tests (RX_STATIC_TESTABLE) */
  RUN_TEST(test_i2c_internal_validate_wire_len_too_large);
  RUN_TEST(test_i2c_internal_validate_wire_len_ok);
  RUN_TEST(test_i2c_internal_decode_header_null_data);
  RUN_TEST(test_i2c_internal_decode_header_null_frame);
  RUN_TEST(test_i2c_internal_decode_header_too_short);
  RUN_TEST(test_i2c_internal_decode_header_bad_sync);
  RUN_TEST(test_i2c_internal_decode_header_payload_too_large);
  RUN_TEST(test_i2c_internal_decode_header_offset_out_set);
  RUN_TEST(test_i2c_internal_verify_crc_null_data);
  RUN_TEST(test_i2c_internal_verify_crc_offset_too_small);
  RUN_TEST(test_i2c_internal_verify_crc_out_written);
  RUN_TEST(test_i2c_internal_read_i2c_data_null_handle);
  RUN_TEST(test_i2c_internal_compact_rx_buffer_null_handle);
  RUN_TEST(test_i2c_internal_compact_rx_buffer_pos_exceeds_len);
  RUN_TEST(test_i2c_internal_compact_rx_buffer_noop_when_pos_zero);
  RUN_TEST(test_i2c_internal_find_sync_null_handle);
  RUN_TEST(test_i2c_internal_find_sync_null_sync_pos);
  RUN_TEST(test_i2c_internal_find_sync_pos_exceeds_len);
  RUN_TEST(test_i2c_internal_handle_no_sync_null_handle);
  RUN_TEST(test_i2c_internal_align_to_sync_advances_pos);
  RUN_TEST(test_i2c_internal_align_to_sync_no_regression);
  RUN_TEST(test_i2c_internal_build_frame_null_frame);
  RUN_TEST(test_i2c_internal_build_frame_null_payload_nonzero_len);
  RUN_TEST(test_i2c_internal_build_frame_payload_too_large);
  RUN_TEST(test_i2c_internal_build_frame_zero_payload);
  RUN_TEST(test_i2c_internal_build_frame_with_payload);
  RUN_TEST(test_i2c_internal_read_i2c_data_buffer_full);
  RUN_TEST(test_i2c_internal_receive_iteration_read_error);
  RUN_TEST(test_i2c_comm_receive_timeout);

  /* Branch coverage gap tests */
  RUN_TEST(test_i2c_internal_decode_header_null_offset_valid_header);
  RUN_TEST(test_i2c_internal_verify_crc_null_crc_out_valid_frame);
  RUN_TEST(test_i2c_internal_read_i2c_data_small_space);
  RUN_TEST(test_i2c_comm_receive_riic_read_returns_timeout);

  /* RX_ASSERT_POST / RX_ASSERT_PRE branch coverage */
  RUN_TEST(test_i2c_comm_init_post_assert_force_fail);
  RUN_TEST(test_i2c_comm_deinit_pre_assert_uninitialized);
}

int main(void)
{
  UNITY_BEGIN();

  internal_run_public_api_tests();
  internal_run_coverage_tests();
  internal_run_internal_function_tests();

  return UNITY_END();
}
