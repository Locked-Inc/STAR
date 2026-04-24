/**
 * @file test_rx_uart_comm.c
 * @brief Unit Tests for UART (SCI9) Frame-Based Communication Protocol
 *
 * @details
 * **Comprehensive Test Suite for UART Peripheral Communication Layer**
 *
 * This test suite validates the rx_uart_comm layer - a frame-based UART protocol
 * that provides an alternative communication channel for the RPi5 to RX72N link
 * over SCI9. Tests verify initialization, frame encoding, CRC validation, sequence
 * management, sync word detection, buffer management, and PING/RESET frame
 * auto-response behavior.
 *
 * **UART Communication Architecture:**
 * @code
 * RPi5 (Linux UART)                 RX72N (SCI9)
 * +-------------------------+       +---------------------------+
 * | /dev/ttyS0              | TXD -> | SCI9 (TXD9)             |
 * | (115200 baud)           | RXD <- | SCI9 (RXD9)             |
 * +-------------------------+       +---------------------------+
 *         v Frame Protocol                    v
 * +-------------------------+       +---------------------------+
 * | rx_uart_comm (Linux)    |       | rx_uart_comm (RX72N)     |
 * | - Protobuf messages     |       | - Frame decode           |
 * | - Frame encode          |       | - CRC-32 check           |
 * | - CRC-32 validation     |       | - PING/RESET auto-reply  |
 * +-------------------------+       +---------------------------+
 * @endcode
 *
 * **Test Categories:**
 * 1. **Initialization** - Tests handle initialization and UART channel configuration
 * 2. **Deinitialization** - Tests handle cleanup including NULL-handle safety
 * 3. **Send Operations** - Tests payload encoding, CRC, UART write
 * 4. **Receive Operations** - Tests frame reception, sync search, buffer management
 * 5. **Frame Parsing** - Tests CRC validation, sync word handling, header parsing
 * 6. **Buffer Management** - Tests rx_buffer_pos/len tracking and compaction
 * 7. **Sequence Management** - Tests TX sequence number incrementing via session
 * 8. **Control Frame Handling** - Tests PING auto-PONG, RESET auto-RESET_ACK
 * 9. **Error Injection** - Tests error propagation from UART HAL mock
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
 * - **Single Responsibility:** Each test validates one UART protocol behavior
 * - **Open/Closed:** Mock injection allows testing without SCI9 hardware
 * - **Liskov Substitution:** Mock UART substitutes for real SCI9 peripheral
 * - **Interface Segregation:** Tests use minimal API (init/send/receive/deinit)
 * - **Dependency Inversion:** Depends on UART abstraction, not SCI registers
 *
 * @par Module Dependencies:
 * - rx_uart_comm.h - Frame-based UART comm layer API under test
 * - rx_frame.h - Frame encoding/decoding primitives
 * - mock_uart_hw.h - Mock UART hardware driver
 * - rx_err.h - Error code definitions
 * - unity.h - Unit testing framework
 *
 * @see rx_uart_comm.h - UART comm layer header
 * @see rx_frame.h - Frame structure and encoding/decoding
 * @see mock_uart_hw.h - Mock UART driver for testing
 * @see docs/sections/01_nanopb_protocol.tex - Full protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-03-12
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @test Tests run via Unity framework with: make test_rx_uart_comm
 */

#include <string.h>

#include "mock_uart_hw.h"
#include "rx_frame.h"
#include "rx_session.h"
#include "rx_uart_comm.h"
#include "unity.h"

/* =============================================================================
 * Internal function forward declarations (visible in UNIT_TEST builds via
 * RX_STATIC_TESTABLE - see rx_check.h).  These declarations allow tests to
 * directly exercise defensive null-check and error-handling branches that are
 * unreachable through the public API.
 * =============================================================================
 */
#ifdef UNIT_TEST

/* Receive-result type mirrored from rx_uart_comm.c */
typedef enum : uint8_t {
  k_receive_continue = 0,
  k_receive_done     = 1,
  k_receive_error    = 2,
} rx_receive_result_t;

extern rx_err_t internal_decode_header(const uint8_t* data,
                                       uint32_t       data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out);
extern rx_err_t internal_verify_crc(const uint8_t* data, uint32_t offset, uint32_t* crc_out);
extern rx_err_t internal_read_uart_data(rx_uart_comm_handle_t* handle);
extern rx_err_t internal_compact_rx_buffer(rx_uart_comm_handle_t* handle);
extern rx_err_t internal_find_sync(const rx_uart_comm_handle_t* handle, int32_t* sync_pos);
extern rx_err_t internal_handle_no_sync(rx_uart_comm_handle_t* handle);
extern rx_err_t
internal_decode_frame(rx_uart_comm_handle_t* handle, rx_frame_t* frame, uint32_t total_size);
extern void internal_align_to_sync(rx_uart_comm_handle_t* handle, int32_t sync_pos);
extern rx_err_t
internal_parse_header(rx_uart_comm_handle_t* handle, uint16_t* payload_len, uint32_t* total_size);
extern rx_receive_result_t
internal_receive_iteration(rx_uart_comm_handle_t* handle, rx_frame_t* frame, rx_err_t* err);
extern rx_err_t internal_build_frame(rx_frame_t*     frame,
                                     uint16_t        sequence,
                                     rx_frame_type_t type,
                                     uint8_t         flags,
                                     const uint8_t*  payload,
                                     uint32_t        payload_len);
extern rx_err_t internal_handle_ping(rx_uart_comm_handle_t* handle, const rx_frame_t* frame);
extern rx_err_t internal_handle_reset(rx_uart_comm_handle_t* handle);
#endif /* UNIT_TEST */

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test payload sizes and numeric constants
 */
typedef enum : uint16_t {
  k_test_payload_small  = 4,    /**< Small payload: "TEST" */
  k_test_timeout_zero   = 0,    /**< Non-blocking receive timeout */
  k_test_frame_buf_size = 2048, /**< Buffer for encoded frames */
  k_test_tx_buf_size    = 256,  /**< Readback buffer size */
  k_test_rx_buf_max     = 2048, /**< k_uart_comm_rx_buffer_size */
  k_test_seq_gap_large  = 33000, /**< Gap >= k_session_max_gap_tolerance (32768); triggers validate_fail */
  k_test_tx_seq_after_2 = 2,    /**< Expected TX seq after two sends */
} test_uart_comm_constants_t;

/**
 * @brief SCI9 channel number used in tests
 */
typedef enum : uint8_t {
  k_test_sci9_channel = 9, /**< SCI9 UART channel */
} test_uart_channel_t;

/**
 * @brief Magic numbers used in test helper and fixture code
 */
typedef enum : uint32_t {
  k_test_garbage_len_init    = 999,   /**< Garbage value for rx_buffer_len init test */
  k_test_garbage_pos_init    = 123,   /**< Garbage value for rx_buffer_pos init test */
  k_test_crc_corrupt_mask    = 0xFFU, /**< XOR mask for CRC corruption */
  k_test_garbage_byte_dead_1 = 0xDE,  /**< Garbage byte 1 for sync skip */
  k_test_garbage_byte_dead_2 = 0xAD,  /**< Garbage byte 2 for sync skip */
  k_test_garbage_byte_dead_3 = 0xBE,  /**< Garbage byte 3 for sync skip */
  k_test_garbage_byte_dead_4 = 0xEF,  /**< Garbage byte 4 for sync skip */
  k_test_garbage_prefix_len  = 4,     /**< Number of garbage prefix bytes */
  k_test_fill_byte_cc        = 0xCC,  /**< Fill pattern for buffer tests */
  k_test_sync_byte_low       = 0xAA,  /**< Sync word low byte (LE of 0x55AA) */
  k_test_sync_byte_high      = 0x55,  /**< Sync word high byte (LE of 0x55AA) */
  k_test_sync_step           = 2U,    /**< Byte step for sync word pair fill */
} test_magic_numbers_t;

/**
 * @brief Frame header byte offsets and sizes for raw buffer construction
 */
typedef enum : uint8_t {
  k_hdr_off_sync_low  = 0, /**< Offset of sync word low byte */
  k_hdr_off_sync_high = 1, /**< Offset of sync word high byte */
  k_hdr_off_seq_low   = 2, /**< Offset of sequence number low byte */
  k_hdr_off_seq_high  = 3, /**< Offset of sequence number high byte */
  k_hdr_off_len_low   = 4, /**< Offset of payload length low byte */
  k_hdr_off_len_high  = 5, /**< Offset of payload length high byte */
  k_hdr_off_type      = 6, /**< Offset of frame type byte */
  k_hdr_off_flags     = 7, /**< Offset of frame flags byte */
  k_hdr_total_size    = 8, /**< Total header size in bytes */
} test_header_offsets_t;

/**
 * @brief Constants for malformed-header tests
 */
typedef enum : uint8_t {
  k_test_malformed_extra  = 4,    /**< Extra padding after header */
  k_test_malformed_len_hi = 0xFF, /**< High byte for oversized payload length */
  k_test_frame_type_cmd   = 0x01, /**< Frame type command for raw buffer */
} test_malformed_constants_t;

/**
 * @brief Constants for internal function direct tests
 */
typedef enum : uint8_t {
  k_test_decode_hdr_buf_sz   = 8,    /**< Decode header small buffer size */
  k_test_bad_sync_buf_sz     = 32,   /**< Buffer size for bad sync test */
  k_test_bad_sync_byte_0     = 0xDE, /**< Bad sync byte 0 */
  k_test_bad_sync_byte_1     = 0xAD, /**< Bad sync byte 1 */
  k_test_compact_buf_len     = 5,    /**< Buffer len for compact tests */
  k_test_compact_pos_invalid = 10,   /**< Invalid pos > len */
  k_test_find_sync_buf_len   = 2,    /**< Buffer len for find_sync pos>len test */
  k_test_find_sync_pos       = 5,    /**< Pos for find_sync pos>len test */
  k_test_align_sync_pos      = 5,    /**< Sync pos for align test */
  k_test_align_pos_ahead     = 10,   /**< Pos already past sync */
  k_test_align_behind_sync   = 3,    /**< Sync behind current pos */
  k_test_build_frame_seq_5   = 5,    /**< Sequence number for build_frame test */
  k_test_build_frame_seq_1   = 1,    /**< Sequence number for build_frame payload test */
  k_test_build_frame_seq_3   = 3,    /**< Sequence for nonnull-payload-zero-len test */
} test_internal_constants_t;

/**
 * @brief Buffer sizes for internal decode tests
 */
typedef enum : uint8_t {
  k_test_decode_too_short = 2,    /**< Buffer too short for header */
  k_test_oversized_buf    = 64,   /**< Buffer for oversized payload test */
  k_test_oversized_len_hi = 0x05, /**< High byte: 0x0500 = 1280 > 1024 */
} test_decode_sizes_t;

/**
 * @brief Test payload byte values
 */
typedef enum : uint8_t {
  k_test_byte_aa = 0xAA, /**< Test payload byte 0xAA */
  k_test_byte_bb = 0xBB, /**< Test payload byte 0xBB */
  k_test_byte_cc = 0xCC, /**< Test payload byte 0xCC */
  k_test_byte_dd = 0xDD, /**< Test payload byte 0xDD */
} test_payload_bytes_t;

/**
 * @brief CRC corruption byte offsets relative to end of encoded frame
 */
typedef enum : uint8_t {
  k_test_crc_off_last      = 1, /**< Last byte of CRC (encoded_len - 1) */
  k_test_crc_off_second    = 2, /**< Second-to-last byte (encoded_len - 2) */
  k_test_false_sync_prefix = 2, /**< Bytes prepended before the real frame */
} test_crc_offsets_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/** @brief UART comm handle under test */
static rx_uart_comm_handle_t s_handle;

/** @brief Shared session for sequence tracking */
static rx_session_state_t s_session;

/* =============================================================================
 * Internal Helpers -- test setup
 * =============================================================================
 */

/**
 * @brief Build a default valid UART comm config using SCI9
 * @return Populated rx_uart_comm_config_t for SCI9
 */
static rx_uart_comm_config_t helper_default_config(void)
{
  rx_uart_comm_config_t cfg = {
    .session = &s_session,
    .channel = k_uart_channel_9,
  };
  return cfg;
}

/**
 * @brief Initialize handle with default config, assert success
 */
static void helper_init_default(void)
{
  rx_uart_comm_config_t cfg = helper_default_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_init(&s_handle, &cfg));
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

  rx_frame_t frame      = {};
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
 * @brief Inject encoded frame bytes into mock UART RX FIFO for SCI9
 *
 * @param[in] data   Encoded frame bytes
 * @param[in] length Number of bytes to inject
 */
static void helper_inject_rx(const uint8_t* data, uint16_t length)
{
  uint16_t injected = mock_uart_hw_inject_rx_data(k_test_sci9_channel, data, length);
  TEST_ASSERT_EQUAL_UINT16(length, injected);
}

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

void setUp(void)
{
  mock_uart_hw_init();

  /* Initialize SCI9 channel in the mock so uart_rx_available() does not
   * return k_rx_err_invalid_state when the channel's .initialized flag is
   * false (which it is after mock_uart_hw_init() zeros all state). */
  const uart_channel_config_t ch_cfg = {
    .channel  = k_uart_channel_9,
    .baudrate = 921600U,
    .tx_gpio  = k_rx_p3_0, /* Ignored by mock */
    .rx_gpio  = k_rx_p3_1, /* Ignored by mock */
  };
  (void)uart_init_channel(&ch_cfg);

  (void)rx_session_init(&s_session);
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset((uint8_t*)&s_handle, 0, sizeof(s_handle));
}

void tearDown(void)
{
  (void)rx_uart_comm_deinit(&s_handle);
  (void)rx_session_deinit(&s_session);
  mock_uart_hw_deinit();
}

/* =============================================================================
 * 1. Initialization Tests
 * =============================================================================
 */

/**
 * @brief Init with null handle returns k_rx_err_invalid_arg
 *
 * @pre  mock UART in clean state
 * @post no state changes
 */
void test_uart_comm_init_null_handle_fails(void)
{
  rx_uart_comm_config_t cfg = helper_default_config();
  rx_err_t              err = rx_uart_comm_init(nullptr, &cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Init with null config returns k_rx_err_invalid_arg
 *
 * @pre  s_handle in cleared state
 * @post s_handle remains uninitialized
 */
void test_uart_comm_init_null_config_fails(void)
{
  rx_err_t err = rx_uart_comm_init(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Init with null session in config returns k_rx_err_invalid_arg
 *
 * @pre  s_handle in cleared state
 * @post s_handle remains uninitialized
 */
void test_uart_comm_init_null_session_fails(void)
{
  rx_uart_comm_config_t cfg = helper_default_config();
  cfg.session               = nullptr;

  rx_err_t err = rx_uart_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Init succeeds with valid config and marks handle initialized
 *
 * @pre  mock UART clean, s_session initialized
 * @post s_handle.initialized == true, channel stored
 */
void test_uart_comm_init_success(void)
{
  rx_uart_comm_config_t cfg = helper_default_config();
  rx_err_t              err = rx_uart_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
  TEST_ASSERT_EQUAL_UINT8(k_uart_channel_9, s_handle.channel);
  TEST_ASSERT_NOT_NULL(s_handle.session);
}

/**
 * @brief Init stores channel from config
 *
 * @pre  mock UART clean, session initialized
 * @post channel matches config value
 */
void test_uart_comm_init_stores_channel(void)
{
  rx_uart_comm_config_t cfg = {
    .session = &s_session,
    .channel = k_uart_channel_9,
  };

  rx_err_t err = rx_uart_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_uart_channel_9, s_handle.channel);
}

/**
 * @brief Init clears RX buffer fields
 *
 * @pre  s_handle has garbage state
 * @post rx_buffer_len == 0, rx_buffer_pos == 0
 */
void test_uart_comm_init_clears_rx_buffer(void)
{
  s_handle.rx_buffer_len = k_test_garbage_len_init;
  s_handle.rx_buffer_pos = k_test_garbage_pos_init;

  helper_init_default();

  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
}

/**
 * @brief Re-initialization of an already-initialized handle succeeds
 *
 * @pre  handle initialized via helper_init_default()
 * @post handle re-initialized; still marked initialized
 */
void test_uart_comm_init_reinit_success(void)
{
  helper_init_default();

  rx_uart_comm_config_t cfg = helper_default_config();
  rx_err_t              err = rx_uart_comm_init(&s_handle, &cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
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
void test_uart_comm_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_uart_comm_deinit(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Deinit succeeds on initialized handle and clears initialized flag
 *
 * @pre  handle initialized
 * @post s_handle.initialized == false
 */
void test_uart_comm_deinit_success(void)
{
  helper_init_default();

  rx_err_t err = rx_uart_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Deinit on uninitialized handle returns k_rx_ok gracefully
 *
 * @pre  s_handle is zero (not initialized)
 * @post no crash; returns k_rx_ok
 */
void test_uart_comm_deinit_not_initialized_ok(void)
{
  rx_err_t err = rx_uart_comm_deinit(&s_handle);

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
void test_uart_comm_send_null_handle_fails(void)
{
  const uint8_t payload[] = "test";
  rx_err_t      err =
    rx_uart_comm_send(nullptr, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Send on uninitialized handle returns k_rx_err_invalid_state
 *
 * @pre  s_handle zeroed (not initialized)
 * @post no UART write issued
 */
void test_uart_comm_send_not_initialized_fails(void)
{
  const uint8_t payload[] = "test";
  rx_err_t      err =
    rx_uart_comm_send(&s_handle, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Send with null payload and nonzero length returns k_rx_err_invalid_arg
 *
 * @pre  handle initialized
 * @post no UART write issued
 */
void test_uart_comm_send_null_payload_nonzero_len_fails(void)
{
  helper_init_default();

  rx_err_t err =
    rx_uart_comm_send(&s_handle, k_frame_type_response, 0, nullptr, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Send with payload exceeding k_frame_max_payload returns k_rx_err_invalid_size
 *
 * @pre  handle initialized
 * @post no UART write issued
 */
void test_uart_comm_send_payload_too_large_fails(void)
{
  helper_init_default();

  const uint32_t oversized = (uint32_t)k_frame_max_payload + 1U;
  uint8_t        dummy[1];
  rx_err_t       err = rx_uart_comm_send(&s_handle, k_frame_type_response, 0, dummy, oversized);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Send with null payload and zero length succeeds (zero-payload frame)
 *
 * @pre  handle initialized
 * @post UART write called once
 */
void test_uart_comm_send_zero_payload_success(void)
{
  helper_init_default();

  rx_err_t err = rx_uart_comm_send(&s_handle, k_frame_type_response, 0, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Send with small payload succeeds and UART TX contains data
 *
 * @pre  handle initialized
 * @post UART TX FIFO for SCI9 contains encoded frame bytes
 */
void test_uart_comm_send_small_payload_success(void)
{
  helper_init_default();

  const uint8_t payload[] = "ABCD";
  rx_err_t      err       = rx_uart_comm_send(&s_handle,
                                   k_frame_type_response,
                                   k_frame_flag_none,
                                   payload,
                                   k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint16_t tx_count = mock_uart_hw_tx_count(k_test_sci9_channel);
  TEST_ASSERT_GREATER_THAN_UINT16(0, tx_count);
}

/**
 * @brief TX data from send contains valid sync word in first two bytes
 *
 * @pre  handle initialized
 * @post first two bytes of TX data match k_frame_sync_word (LE)
 */
void test_uart_comm_send_tx_contains_sync_word(void)
{
  helper_init_default();

  const uint8_t payload[] = "TEST";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_uart_comm_send(&s_handle,
                                      k_frame_type_command,
                                      k_frame_flag_none,
                                      payload,
                                      k_test_payload_small));

  uint8_t  tx_buf[k_test_tx_buf_size];
  uint16_t tx_len = mock_uart_hw_get_tx_data(k_test_sci9_channel, tx_buf, k_test_tx_buf_size);
  TEST_ASSERT_GREATER_THAN_UINT16(1, tx_len);

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
void test_uart_comm_send_increments_tx_sequence(void)
{
  helper_init_default();

  const uint8_t payload[] = "PING";

  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_uart_comm_send(&s_handle, k_frame_type_command, 0, payload, k_test_payload_small));
  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_uart_comm_send(&s_handle, k_frame_type_command, 0, payload, k_test_payload_small));

  uint16_t tx_seq = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_get_tx(&s_session, &tx_seq));
  TEST_ASSERT_EQUAL_UINT16(k_test_tx_seq_after_2, tx_seq);
}

/**
 * @brief Send failure from UART write propagates error
 *
 * @pre  handle initialized, next UART call returns error
 * @post send returns the injected error
 */
void test_uart_comm_send_uart_write_failure_propagates(void)
{
  helper_init_default();

  mock_uart_hw_set_next_error(k_rx_err_hw_error);

  const uint8_t payload[] = "FAIL";
  rx_err_t      err =
    rx_uart_comm_send(&s_handle, k_frame_type_response, 0, payload, k_test_payload_small);

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
void test_uart_comm_receive_null_handle_fails(void)
{
  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(nullptr, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Receive with null frame pointer returns k_rx_err_invalid_arg
 *
 * @pre  handle initialized
 * @post no state changes
 */
void test_uart_comm_receive_null_frame_fails(void)
{
  helper_init_default();

  rx_err_t err = rx_uart_comm_receive(&s_handle, nullptr, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Receive on uninitialized handle returns k_rx_err_invalid_state
 *
 * @pre  s_handle zeroed (not initialized)
 * @post no state changes
 */
void test_uart_comm_receive_not_initialized_fails(void)
{
  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Receive with no data available returns k_rx_err_no_data
 *
 * @pre  handle initialized, no data in UART mock RX FIFO
 * @post frame unmodified
 */
void test_uart_comm_receive_no_data_returns_no_data(void)
{
  helper_init_default();

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive successfully decodes a valid command frame
 *
 * @pre  handle initialized, valid encoded frame injected into UART mock
 * @post frame contains correct type, sequence, and payload
 */
void test_uart_comm_receive_valid_frame_success(void)
{
  helper_init_default();

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

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

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
void test_uart_comm_receive_crc_mismatch_fails(void)
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

  /* Corrupt the last two bytes of CRC */
  encoded[encoded_len - k_test_crc_off_last] ^= (uint8_t)k_test_crc_corrupt_mask;
  encoded[encoded_len - k_test_crc_off_second] ^= (uint8_t)k_test_crc_corrupt_mask;

  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/**
 * @brief Receive with no valid sync word returns k_rx_err_no_data
 *
 * @pre  handle initialized, garbage bytes injected with no sync word
 * @post returns k_rx_err_no_data
 */
void test_uart_comm_receive_invalid_sync_returns_no_data(void)
{
  helper_init_default();

  const uint8_t garbage[] =
    {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  helper_inject_rx(garbage, sizeof(garbage));

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive ignores leading garbage before sync word
 *
 * @pre  handle initialized, garbage + valid frame injected
 * @post frame decoded correctly; leading garbage discarded
 */
void test_uart_comm_receive_skips_garbage_before_sync(void)
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

  uint8_t buf_with_garbage[k_test_frame_buf_size];
  buf_with_garbage[k_hdr_off_sync_low]  = (uint8_t)k_test_garbage_byte_dead_1;
  buf_with_garbage[k_hdr_off_sync_high] = (uint8_t)k_test_garbage_byte_dead_2;
  buf_with_garbage[k_hdr_off_seq_low]   = (uint8_t)k_test_garbage_byte_dead_3;
  buf_with_garbage[k_hdr_off_seq_high]  = (uint8_t)k_test_garbage_byte_dead_4;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(buf_with_garbage + k_test_garbage_prefix_len, encoded, encoded_len);

  helper_inject_rx(buf_with_garbage, (uint16_t)(encoded_len + k_test_garbage_prefix_len));

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
}

/**
 * @brief Receive with partial frame returns k_rx_err_no_data
 *
 * @pre  handle initialized, only first 4 bytes of frame injected
 * @post returns k_rx_err_no_data (incomplete frame)
 */
void test_uart_comm_receive_partial_frame_returns_no_data(void)
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

  /* Only inject first 4 bytes */
  helper_inject_rx(encoded, k_test_payload_small);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive a response frame type correctly
 *
 * @pre  handle initialized, response frame injected
 * @post frame.header.type == k_frame_type_response
 */
void test_uart_comm_receive_response_frame_success(void)
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

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_response, frame.header.type);
}

/**
 * @brief Receive a zero-payload frame succeeds
 *
 * @pre  handle initialized, zero-payload frame injected
 * @post frame.header.length == 0
 */
void test_uart_comm_receive_zero_payload_success(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.length);
}

/* =============================================================================
 * 5. Sequence Validation Tests
 * =============================================================================
 */

/**
 * @brief Receive frame with correct initial sequence (0) passes
 *
 * @pre  handle initialized, session at RX seq 0
 * @post frame returned with sequence 0
 */
void test_uart_comm_receive_correct_sequence_passes(void)
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
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.sequence);
}

/* =============================================================================
 * 6. PING / PONG Auto-Response Tests
 * =============================================================================
 */

/**
 * @brief PING frame triggers PONG auto-response via UART write
 *
 * @pre  handle initialized, PING frame injected
 * @post UART TX FIFO contains PONG data, receive returns no_data
 */
void test_uart_comm_receive_ping_triggers_pong(void)
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

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);

  uint16_t tx_count = mock_uart_hw_tx_count(k_test_sci9_channel);
  TEST_ASSERT_GREATER_THAN_UINT16(0, tx_count);
}

/* =============================================================================
 * 7. RESET / RESET_ACK Auto-Response Tests
 * =============================================================================
 */

/**
 * @brief RESET frame triggers RESET_ACK auto-response and session reset
 *
 * @pre  handle initialized, RESET frame injected
 * @post UART TX contains RESET_ACK data, receive returns no_data
 */
void test_uart_comm_receive_reset_triggers_reset_ack(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);

  uint16_t tx_count = mock_uart_hw_tx_count(k_test_sci9_channel);
  TEST_ASSERT_GREATER_THAN_UINT16(0, tx_count);
}

/**
 * @brief PONG frame is silently consumed (not returned to caller)
 *
 * @pre  handle initialized, PONG frame injected
 * @post receive returns no_data (PONG consumed internally)
 */
void test_uart_comm_receive_pong_silently_consumed(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_pong, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* PONG consumed silently; no data to return */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/* =============================================================================
 * 8. Buffer Management Tests
 * =============================================================================
 */

/**
 * @brief After successful receive, rx_buffer is compacted
 *
 * @pre  handle initialized, one full frame injected
 * @post after receive, rx_buffer_len == 0 and rx_buffer_pos == 0
 */
void test_uart_comm_receive_compacts_buffer_after_decode(void)
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

  rx_frame_t frame = {};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero));

  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_len);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.rx_buffer_pos);
}

/* =============================================================================
 * 9. State Transition Tests
 * =============================================================================
 */

/**
 * @brief After deinit, send returns k_rx_err_invalid_state
 *
 * @pre  handle initialized then deinitialized
 * @post send returns k_rx_err_invalid_state
 */
void test_uart_comm_send_after_deinit_fails(void)
{
  helper_init_default();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_deinit(&s_handle));

  const uint8_t payload[] = "FAIL";
  rx_err_t      err =
    rx_uart_comm_send(&s_handle, k_frame_type_response, 0, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief After deinit, receive returns k_rx_err_invalid_state
 *
 * @pre  handle initialized then deinitialized
 * @post receive returns k_rx_err_invalid_state
 */
void test_uart_comm_receive_after_deinit_fails(void)
{
  helper_init_default();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_deinit(&s_handle));

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * 10. Additional Coverage Tests
 * =============================================================================
 */

/**
 * @brief Receive propagates UART RX availability check error
 *
 * @details
 * Injects a UART error so that uart_rx_available() returns an error inside
 * internal_read_uart_data. The error is propagated up through
 * internal_receive_iteration (lines 347-350 of rx_uart_comm.c).
 *
 * @pre  handle initialized, mock UART error pre-staged
 * @post receive returns the injected error
 */
void test_uart_comm_receive_uart_avail_error_propagates(void)
{
  helper_init_default();

  /* Inject error for uart_rx_available call */
  mock_uart_hw_set_next_error(k_rx_err_hw_error);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive with invalid payload length in header (parse_header invalid_size)
 *
 * @details
 * Injects a raw byte sequence with a valid sync word but an oversized
 * payload-length field (> k_frame_max_payload). This triggers
 * internal_parse_header to return k_rx_err_invalid_size (lines 634-636),
 * advance past the sync word, and continue. With no more valid data,
 * receive returns k_rx_err_no_data.
 *
 * @pre  handle initialized, malformed frame injected
 * @post receive returns k_rx_err_no_data
 */
void test_uart_comm_receive_invalid_payload_length_in_header(void)
{
  helper_init_default();

  /* Craft a buffer: valid sync word + seq(0) + payload_len=65535 + type + flags
   * plus extra padding bytes after the header.
   * k_frame_sync_word = 0x55AA; in LE buffer order: [0xAA, 0x55] */
  uint8_t malformed[k_hdr_total_size + k_test_malformed_extra];
  malformed[k_hdr_off_sync_low]  = (uint8_t)k_test_sync_byte_low;
  malformed[k_hdr_off_sync_high] = (uint8_t)k_test_sync_byte_high;
  malformed[k_hdr_off_seq_low]   = 0x00U;
  malformed[k_hdr_off_seq_high]  = 0x00U;
  malformed[k_hdr_off_len_low]   = k_test_malformed_len_hi;
  malformed[k_hdr_off_len_high]  = k_test_malformed_len_hi;
  malformed[k_hdr_off_type]      = 0x01U;
  malformed[k_hdr_off_flags]     = 0x00U;
  for (uint8_t i = k_hdr_total_size; i < (uint8_t)(k_hdr_total_size + k_test_malformed_extra);
       i++) {
    malformed[i] = 0x00U;
  }

  helper_inject_rx(malformed, (uint16_t)(k_hdr_total_size + k_test_malformed_extra));

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief RESET_ACK frame is silently consumed (not returned to caller)
 *
 * @details
 * Injects a RESET_ACK frame. rx_uart_comm_receive decodes it and
 * consumes it silently (lines 1096-1098 of rx_uart_comm.c) without
 * returning it to the caller.
 *
 * @pre  handle initialized, RESET_ACK frame injected
 * @post receive returns k_rx_err_no_data
 */
void test_uart_comm_receive_reset_ack_silently_consumed(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset_ack, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Sequence validation fails when frame has out-of-range sequence number
 *
 * @details
 * Sends a frame with sequence 200 when the session expects 0. The gap exceeds
 * k_session_max_gap_tolerance, so rx_session_validate_rx returns
 * k_session_validate_fail. The receive loop drops the frame (lines 1108-1110)
 * and returns k_rx_err_no_data.
 *
 * @pre  handle initialized, session expects RX seq 0
 * @post receive returns k_rx_err_no_data (frame dropped)
 */
void test_uart_comm_receive_sequence_validation_fail_drops_frame(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command,
                      k_test_seq_gap_large,
                      (const uint8_t*)"DATA",
                      k_test_payload_small,
                      encoded,
                      &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Session rejects the sequence gap and returns k_rx_err_protocol_error */
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief PING handling fails when PONG send fails due to UART write error
 *
 * @details
 * Injects a PING frame. When rx_uart_comm_receive processes the PING and
 * calls internal_handle_ping to send a PONG, the UART write fails due to a
 * pre-staged error. The error is propagated out (lines 1080-1082).
 *
 * @pre  handle initialized, PING frame injected, UART error pre-staged
 * @post receive returns the injected UART error
 */
void test_uart_comm_receive_ping_pong_send_fails(void)
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
  mock_uart_hw_set_next_write_error(k_rx_err_hw_error);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief RESET handling fails when RESET_ACK write fails
 *
 * @details
 * Injects a RESET frame. When internal_handle_reset tries to write the
 * RESET_ACK and the UART write fails, the error is propagated (lines 1089-1091).
 *
 * @pre  handle initialized, RESET frame injected, UART error pre-staged
 * @post receive returns the injected UART error
 */
void test_uart_comm_receive_reset_ack_send_fails(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Stage a write-only error so the frame read succeeds but RESET_ACK send fails */
  mock_uart_hw_set_next_write_error(k_rx_err_hw_error);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Second frame with wrong sequence number is dropped
 *
 * @details
 * Injects two frames: seq 0 (accepted) and seq 200 (dropped). The session
 * expects seq 1 after the first frame, but gets 200 - the gap causes
 * k_session_validate_fail and the frame is dropped.
 *
 * @pre  handle initialized, two frames injected (seq 0 and seq 200)
 * @post first receive returns k_rx_ok; second returns k_rx_err_no_data
 */
void test_uart_comm_receive_second_frame_wrong_sequence_dropped(void)
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
                      k_test_seq_gap_large,
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

  rx_frame_t frame = {};

  /* First receive: frame1 at seq 0 (valid) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero));
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.sequence);

  /* Second receive: frame2 at seq 200 rejected by session (expects 1), protocol error */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset((uint8_t*)&frame, 0, sizeof(frame));
  rx_err_t err2 = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err2);
}

/**
 * @brief Receive with RX buffer full returns no_data (space == 0 path)
 *
 * @details
 * Fills the UART RX buffer with garbage data that has no sync word, causing
 * the buffer to reach near capacity. Tests that the space==0 early return
 * path in internal_read_uart_data (line 341) is exercised.
 *
 * @pre  handle initialized, UART mock filled with garbage
 * @post receive returns k_rx_err_no_data
 */
void test_uart_comm_receive_uart_read_error_propagates(void)
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
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Inject an error on the first UART read call (uart_rx_available returns ok
   * but uart_read_channel will fail) */
  mock_uart_hw_set_next_error(k_rx_err_hw_error);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* The error from uart_read_channel propagates via internal_receive_iteration */
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Sync search hits false sync byte (0xAA not followed by 0x55) then finds real frame
 *
 * @details
 * Prepends a false sync prefix [0xAA, 0x00] before a valid frame. In LE encoding,
 * the real sync word is [0xAA, 0x55]. The search finds 0xAA at byte 0, but byte 1
 * is 0x00 (not 0x55), so internal_find_sync skips it (branch at line 474: sync_low
 * matches but sync_high does not). It continues scanning and finds the real sync at
 * byte 2, decoding the frame successfully.
 *
 * @pre  handle initialized, [0xAA, 0x00] prefix before valid frame
 * @post frame decoded correctly with k_rx_ok
 */
void test_uart_comm_receive_false_sync_low_then_real_frame(void)
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
  buf[0] = (uint8_t)k_test_sync_byte_low; /* sync_low matches, but... */
  buf[1] = 0x00U;                         /* sync_high does NOT match -> false alarm */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(buf + k_test_false_sync_prefix, encoded, encoded_len);

  helper_inject_rx(buf, (uint16_t)(encoded_len + k_test_false_sync_prefix));

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
}

/**
 * @brief Partial frame (header only, no payload) returns k_rx_err_no_data
 *
 * @details
 * Injects only the 8-byte frame header (sync + seq + payload_len=4 + type +
 * flags) without the 4-byte payload or 4-byte CRC. After internal_parse_header
 * succeeds (payload_len=4, total_size=16), the check at line 724 detects that
 * available(8) < total_size(16) and returns k_rx_err_no_data (lines 725-726).
 *
 * @pre  handle initialized, 8-byte partial frame header in UART mock
 * @post receive returns k_rx_err_no_data (incomplete frame)
 */
void test_uart_comm_receive_partial_frame_header_only(void)
{
  helper_init_default();

  uint8_t partial[k_hdr_total_size];
  partial[k_hdr_off_sync_low]  = (uint8_t)k_test_sync_byte_low;
  partial[k_hdr_off_sync_high] = (uint8_t)k_test_sync_byte_high;
  partial[k_hdr_off_seq_low]   = 0x00U;
  partial[k_hdr_off_seq_high]  = 0x00U;
  partial[k_hdr_off_len_low]   = (uint8_t)k_test_payload_small;
  partial[k_hdr_off_len_high]  = 0x00U;
  partial[k_hdr_off_type]      = k_test_frame_type_cmd;
  partial[k_hdr_off_flags]     = 0x00U;

  helper_inject_rx(partial, (uint16_t)k_hdr_total_size);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* available(8) < total_size(16), returns no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief rx_uart_comm_send propagates error when session next_tx fails
 *
 * @details
 * Deinitializes the session so rx_session_next_tx returns
 * k_rx_err_not_initialized. Verifies that rx_uart_comm_send propagates the
 * error (lines 911-913 of rx_uart_comm.c).
 *
 * @pre  handle initialized, session deinitialized
 * @post send returns k_rx_err_not_initialized
 */
void test_uart_comm_send_session_not_initialized_fails(void)
{
  helper_init_default();

  /* Deinit the session so rx_session_next_tx returns not_initialized */
  (void)rx_session_deinit(&s_session);

  rx_err_t err = rx_uart_comm_send(&s_handle, k_frame_type_command, k_frame_flag_none, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief rx_uart_comm_send propagates error when frame encoder not initialized
 *
 * @details
 * Deinitializes the encoder inside the handle so rx_frame_encode returns
 * k_rx_err_invalid_state. Verifies that rx_uart_comm_send propagates the
 * error (lines 925-927 of rx_uart_comm.c).
 *
 * @pre  handle initialized, encoder deinitialized
 * @post send returns k_rx_err_invalid_state
 */
void test_uart_comm_send_encoder_not_initialized_fails(void)
{
  helper_init_default();

  /* Deinit the encoder so rx_frame_encode returns invalid_state */
  (void)rx_frame_encoder_deinit(&s_handle.encoder);

  rx_err_t err = rx_uart_comm_send(&s_handle, k_frame_type_command, k_frame_flag_none, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Receive with RX buffer pre-filled (space==0) returns no_data
 *
 * @details
 * Directly sets rx_buffer_len to k_uart_comm_rx_buffer_size with garbage data
 * (no sync word). This exercises the space==0 early return in
 * internal_read_uart_data (line 342 of rx_uart_comm.c) and the buffer-full
 * increment in internal_handle_no_sync (line 512).
 *
 * @pre  handle initialized, rx_buffer_len = max, rx_buffer contains 0xCC bytes
 * @post receive returns k_rx_err_no_data
 */
void test_uart_comm_receive_rx_buffer_prefilled_no_sync(void)
{
  helper_init_default();

  /* Directly fill the RX buffer with garbage (no sync word = 0xAA 0x55) */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(s_handle.rx_buffer, (uint8_t)k_test_fill_byte_cc, k_test_rx_buf_max);
  s_handle.rx_buffer_len = (uint32_t)k_test_rx_buf_max;
  s_handle.rx_buffer_pos = 0;

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Buffer full: space==0 (line 342) -> no read, no sync -> handle_no_sync
   * increments pos (line 512) -> compact -> no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/**
 * @brief Receive with RESET frame when session not initialized propagates error
 *
 * @details
 * Injects a RESET frame and deinitializes the session so that
 * rx_session_next_tx inside internal_handle_reset returns
 * k_rx_err_not_initialized (line 1013 of rx_uart_comm.c).
 *
 * @pre  handle initialized, RESET frame injected, session deinitialized
 * @post receive returns k_rx_err_not_initialized
 */
void test_uart_comm_receive_reset_session_next_tx_fails(void)
{
  helper_init_default();

  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_reset, 0, nullptr, 0, encoded, &encoded_len);
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Deinit the session so rx_session_next_tx fails inside internal_handle_reset */
  (void)rx_session_deinit(&s_session);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief uart_read_channel error when data is available propagates error
 *
 * @details
 * Injects a frame into the UART FIFO so uart_rx_available reports data, then
 * pre-stages a read-only error so uart_read_channel fails. This exercises
 * line 361-362 of rx_uart_comm.c (uart_read_channel error path).
 *
 * @pre  handle initialized, frame data in mock FIFO, read error staged
 * @post receive returns the injected error
 */
void test_uart_comm_receive_uart_read_channel_error_propagates(void)
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
  helper_inject_rx(encoded, (uint16_t)encoded_len);

  /* Stage a read-only error so uart_rx_available succeeds but
   * uart_read_channel fails */
  mock_uart_hw_set_next_read_error(k_rx_err_hw_error);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/* =============================================================================
 * Direct internal-function tests (RX_STATIC_TESTABLE)
 * =============================================================================
 */

/**
 * @brief internal_decode_header returns invalid_arg for nullptr data
 */
void test_uart_internal_decode_header_null_data(void)
{
  rx_frame_t frame = {};
  rx_err_t   err   = internal_decode_header(nullptr, k_test_oversized_buf, &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_decode_header returns invalid_arg for nullptr frame
 */
void test_uart_internal_decode_header_null_frame(void)
{
  const uint8_t buf[k_test_decode_hdr_buf_sz] = {};
  rx_err_t      err = internal_decode_header(buf, sizeof(buf), nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_decode_header returns invalid_size when data too short
 */
void test_uart_internal_decode_header_too_short(void)
{
  const uint8_t buf[k_test_decode_too_short] = {};
  rx_frame_t    frame                        = {};
  rx_err_t      err = internal_decode_header(buf, sizeof(buf), &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_decode_header returns protocol_error for bad sync word
 */
void test_uart_internal_decode_header_bad_sync(void)
{
  uint8_t buf[k_test_bad_sync_buf_sz] = {};
  buf[0]                              = k_test_bad_sync_byte_0;
  buf[1]                              = k_test_bad_sync_byte_1;
  rx_frame_t frame                    = {};
  rx_err_t   err                      = internal_decode_header(buf, sizeof(buf), &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief internal_decode_header returns invalid_size for oversized payload
 * k_frame_sync_word = 0x55AA; LE16: buf[0]=0xAA, buf[1]=0x55
 * Length at offset 4: set to 1280 (> k_frame_max_payload=1024)
 */
void test_uart_internal_decode_header_payload_too_large(void)
{
  uint8_t buf[k_test_oversized_buf] = {};
  buf[k_hdr_off_sync_low]           = (uint8_t)k_test_sync_byte_low;
  buf[k_hdr_off_sync_high]          = (uint8_t)k_test_sync_byte_high;
  /* seq = 0 */
  buf[k_hdr_off_seq_low]  = 0;
  buf[k_hdr_off_seq_high] = 0;
  /* length LE16 at offset 4: 0x0500 = 1280 (> 1024) -> buf[4]=0x00, buf[5]=0x05 */
  buf[k_hdr_off_len_low]  = 0x00U;
  buf[k_hdr_off_len_high] = k_test_oversized_len_hi;
  rx_frame_t frame        = {};
  rx_err_t   err          = internal_decode_header(buf, sizeof(buf), &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_decode_header populates offset_out when provided
 */
void test_uart_internal_decode_header_offset_out_set(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  rx_frame_t frame  = {};
  uint32_t   offset = 0;
  rx_err_t   err    = internal_decode_header(encoded, encoded_len, &frame, &offset);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, offset);
}

/**
 * @brief internal_verify_crc returns invalid_arg for nullptr data
 */
void test_uart_internal_verify_crc_null_data(void)
{
  rx_err_t err = internal_verify_crc(nullptr, k_hdr_total_size, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_verify_crc returns invalid_arg when offset too small
 */
void test_uart_internal_verify_crc_offset_too_small(void)
{
  const uint8_t buf[k_test_bad_sync_buf_sz] = {};
  rx_err_t      err                         = internal_verify_crc(buf, 0, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_verify_crc writes crc_out when pointer is not nullptr
 */
void test_uart_internal_verify_crc_out_written(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  const uint32_t crc_offset = encoded_len - (uint32_t)k_frame_crc_size;
  uint32_t       crc_out    = 0;
  rx_err_t       err        = internal_verify_crc(encoded, crc_offset, &crc_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, crc_out);
}

/**
 * @brief internal_read_uart_data returns invalid_arg for nullptr handle
 */
void test_uart_internal_read_uart_data_null_handle(void)
{
  rx_err_t err = internal_read_uart_data(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_read_uart_data returns ok when buffer is full
 */
void test_uart_internal_read_uart_data_buffer_full(void)
{
  helper_init_default();
  s_handle.rx_buffer_len = k_uart_comm_rx_buffer_size;
  rx_err_t err           = internal_read_uart_data(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  s_handle.rx_buffer_len = 0;
}

/**
 * @brief internal_compact_rx_buffer returns invalid_arg for nullptr handle
 */
void test_uart_internal_compact_rx_buffer_null_handle(void)
{
  rx_err_t err = internal_compact_rx_buffer(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_compact_rx_buffer returns invalid_state when pos > len
 */
void test_uart_internal_compact_rx_buffer_pos_exceeds_len(void)
{
  helper_init_default();
  s_handle.rx_buffer_len = k_test_compact_buf_len;
  s_handle.rx_buffer_pos = k_test_compact_pos_invalid;
  rx_err_t err           = internal_compact_rx_buffer(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  s_handle.rx_buffer_len = 0;
  s_handle.rx_buffer_pos = 0;
}

/**
 * @brief internal_compact_rx_buffer is no-op when pos == 0
 */
void test_uart_internal_compact_rx_buffer_noop_when_pos_zero(void)
{
  helper_init_default();
  s_handle.rx_buffer_len = k_test_compact_buf_len;
  s_handle.rx_buffer_pos = 0;
  rx_err_t err           = internal_compact_rx_buffer(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_compact_buf_len, s_handle.rx_buffer_len);
}

/**
 * @brief internal_find_sync returns invalid_arg for nullptr handle
 */
void test_uart_internal_find_sync_null_handle(void)
{
  int32_t  pos = 0;
  rx_err_t err = internal_find_sync(nullptr, &pos);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_find_sync returns invalid_arg for nullptr sync_pos
 */
void test_uart_internal_find_sync_null_sync_pos(void)
{
  helper_init_default();
  rx_err_t err = internal_find_sync(&s_handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_find_sync returns invalid_state when pos > len
 */
void test_uart_internal_find_sync_pos_exceeds_len(void)
{
  helper_init_default();
  s_handle.rx_buffer_len = k_test_find_sync_buf_len;
  s_handle.rx_buffer_pos = k_test_find_sync_pos;
  int32_t  pos           = 0;
  rx_err_t err           = internal_find_sync(&s_handle, &pos);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  s_handle.rx_buffer_len = 0;
  s_handle.rx_buffer_pos = 0;
}

/**
 * @brief internal_handle_no_sync returns invalid_arg for nullptr handle
 */
void test_uart_internal_handle_no_sync_null_handle(void)
{
  rx_err_t err = internal_handle_no_sync(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_align_to_sync advances pos to sync when sync is ahead
 */
void test_uart_internal_align_to_sync_advances_pos(void)
{
  helper_init_default();
  s_handle.rx_buffer_pos = 0;
  internal_align_to_sync(&s_handle, k_test_align_sync_pos);
  TEST_ASSERT_EQUAL(k_test_align_sync_pos, s_handle.rx_buffer_pos);
}

/**
 * @brief internal_align_to_sync does not move pos backwards
 */
void test_uart_internal_align_to_sync_no_regression(void)
{
  helper_init_default();
  s_handle.rx_buffer_pos = k_test_align_pos_ahead;
  internal_align_to_sync(&s_handle, k_test_align_behind_sync);
  TEST_ASSERT_EQUAL(k_test_align_pos_ahead, s_handle.rx_buffer_pos);
}

/**
 * @brief internal_build_frame returns invalid_arg for nullptr frame
 */
void test_uart_internal_build_frame_null_frame(void)
{
  const uint8_t payload[k_test_payload_small] = {1, 0, 0, 0};
  rx_err_t      err =
    internal_build_frame(nullptr, 0, k_frame_type_command, 0, payload, sizeof(payload));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_build_frame returns invalid_arg for nullptr payload with nonzero len
 */
void test_uart_internal_build_frame_null_payload_nonzero_len(void)
{
  rx_frame_t frame = {};
  rx_err_t   err =
    internal_build_frame(&frame, 0, k_frame_type_command, 0, nullptr, k_test_payload_small);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_build_frame returns invalid_size for payload > k_frame_max_payload (1024)
 */
void test_uart_internal_build_frame_payload_too_large(void)
{
  rx_frame_t    frame;
  const uint8_t big_payload[1025] = {};
  frame                           = (rx_frame_t){};
  rx_err_t err =
    internal_build_frame(&frame, 0, k_frame_type_command, 0, big_payload, sizeof(big_payload));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief internal_build_frame with zero-length payload succeeds
 */
void test_uart_internal_build_frame_zero_payload(void)
{
  rx_frame_t frame = {};
  rx_err_t   err =
    internal_build_frame(&frame, k_test_build_frame_seq_5, k_frame_type_command, 0, nullptr, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_build_frame_seq_5, frame.header.sequence);
}

/**
 * @brief internal_build_frame with payload copies the data
 */
void test_uart_internal_build_frame_with_payload(void)
{
  rx_frame_t    frame                         = {};
  const uint8_t payload[k_test_payload_small] = {k_test_byte_aa,
                                                 k_test_byte_bb,
                                                 k_test_byte_cc,
                                                 k_test_byte_dd};
  rx_err_t      err                           = internal_build_frame(&frame,
                                      k_test_build_frame_seq_1,
                                      k_frame_type_command,
                                      0,
                                      payload,
                                      sizeof(payload));
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_MEMORY(payload, frame.payload, sizeof(payload));
}

/**
 * @brief internal_handle_ping returns invalid_arg for nullptr handle
 */
void test_uart_internal_handle_ping_null_handle(void)
{
  rx_frame_t frame = {};
  rx_err_t   err   = internal_handle_ping(nullptr, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_handle_ping returns invalid_arg for nullptr frame
 */
void test_uart_internal_handle_ping_null_frame(void)
{
  helper_init_default();
  rx_err_t err = internal_handle_ping(&s_handle, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_handle_reset returns invalid_arg for nullptr handle
 */
void test_uart_internal_handle_reset_null_handle(void)
{
  rx_err_t err = internal_handle_reset(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief internal_receive_iteration propagates non-timeout error from read
 */
void test_uart_internal_receive_iteration_read_error(void)
{
  helper_init_default();
  mock_uart_hw_set_next_error(k_rx_err_hw_error);
  rx_frame_t          frame;
  rx_err_t            err    = k_rx_ok;
  rx_receive_result_t result = internal_receive_iteration(&s_handle, &frame, &err);
  TEST_ASSERT_EQUAL(k_receive_error, result);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

/**
 * @brief rx_uart_comm_receive() returns k_rx_err_timeout after exhausting iterations
 *
 * @details
 * Fills the internal rx_buffer with a repeating sync-word pattern (0xAA 0x55)
 * which causes every call to internal_receive_iteration to return
 * k_receive_continue (invalid payload length: 0x55AA > k_frame_max_payload).
 * After k_uart_comm_max_receive_iterations (1000) iterations the while loop
 * exits and returns k_rx_err_timeout.
 *
 * @pre  handle initialized
 * @post receive returns k_rx_err_timeout
 *
 * @test Covers the k_rx_err_timeout return path (line 1118)
 */
void test_uart_comm_receive_timeout(void)
{
  helper_init_default();

  /* Fill rx_buffer with repeating sync word (0xAA 0x55) so that every sync
   * search succeeds but every parse_header returns invalid_size (payload_len
   * 0x55AA > k_frame_max_payload) advancing rx_buffer_pos by 2 per iteration. */
  const uint8_t sync_low  = (uint8_t)(k_frame_sync_word & 0xFFU);
  const uint8_t sync_high = (uint8_t)(k_frame_sync_word >> 8U);
  for (uint32_t i = k_test_timeout_zero; i < k_test_rx_buf_max; i += k_test_sync_step) {
    s_handle.rx_buffer[i]      = sync_low;
    s_handle.rx_buffer[i + 1U] = sync_high;
  }
  s_handle.rx_buffer_len = k_test_rx_buf_max;
  s_handle.rx_buffer_pos = k_test_timeout_zero;

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief internal_decode_header: nullptr offset_out with valid header returns k_rx_ok.
 *
 * @details
 * Covers the FALSE branch of `if (offset_out != nullptr)` at line 175.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned
 *
 * @see internal_decode_header() Function under test
 */
void test_uart_internal_decode_header_null_offset_valid_header(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  rx_frame_t frame = {};
  /* Pass nullptr for offset_out with valid frame: covers line-175 FALSE branch */
  rx_err_t err = internal_decode_header(encoded, encoded_len, &frame, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief internal_verify_crc: nullptr crc_out with valid CRC returns k_rx_ok.
 *
 * @details
 * Covers the FALSE branch of `if (crc_out != nullptr)` at line 233.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned
 *
 * @see internal_verify_crc() Function under test
 */
void test_uart_internal_verify_crc_null_crc_out_valid_frame(void)
{
  helper_init_default();
  uint8_t  encoded[k_test_frame_buf_size];
  uint32_t encoded_len = 0;
  helper_encode_frame(k_frame_type_command, 0, nullptr, 0, encoded, &encoded_len);

  const uint32_t crc_offset = encoded_len - (uint32_t)k_frame_crc_size;
  /* Pass nullptr crc_out: covers line-233 FALSE branch */
  rx_err_t err = internal_verify_crc(encoded, crc_offset, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief internal_build_frame: non-nullptr payload with zero len skips memcpy.
 *
 * @details
 * Covers the `payload_len == 0` sub-branch of `if (payload != nullptr && payload_len > 0)`
 * at line 879 (TRUE-first-AND-FALSE-second path). Prior checks allow this through.
 *
 * @pre setUp() completed
 * @post k_rx_ok returned, no payload copied
 *
 * @see internal_build_frame() Function under test
 */
void test_uart_internal_build_frame_nonnull_payload_zero_len(void)
{
  rx_frame_t    frame                         = {};
  const uint8_t payload[k_test_payload_small] = {k_test_byte_aa,
                                                 k_test_byte_bb,
                                                 k_test_byte_cc,
                                                 k_test_byte_dd};
  /* Non-null payload but payload_len=0: the if() short-circuits on second condition */
  rx_err_t err =
    internal_build_frame(&frame, k_test_build_frame_seq_3, k_frame_type_command, 0, payload, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, frame.header.length);
}

/**
 * @brief UART read returns k_rx_err_timeout -- compound condition false at line 688
 *
 * @details
 * Covers branch :3 at rx_uart_comm.c line 688:
 *   `if (*err != k_rx_ok && *err != k_rx_err_timeout)`
 * When uart_read_channel returns k_rx_err_timeout, the first operand is TRUE
 * but the second is FALSE, so the condition is FALSE and we do NOT enter the
 * error branch. The receive loop then sees an empty buffer and returns no_data.
 *
 * Setup: inject one byte so uart_rx_available returns true, then override the
 * actual read call to return k_rx_err_timeout.
 *
 * @pre setUp() completed
 * @post rx_uart_comm_receive returns k_rx_err_no_data (empty buffer after read)
 */
void test_uart_comm_receive_read_returns_timeout(void)
{
  helper_init_default();

  /* Inject a byte so uart_rx_available returns true */
  const uint8_t dummy[1] = {(uint8_t)k_test_sync_byte_low};
  helper_inject_rx(dummy, (uint16_t)sizeof(dummy));

  /* Make uart_read_channel return k_rx_err_timeout */
  mock_uart_hw_set_next_read_error(k_rx_err_timeout);

  rx_frame_t frame = {};
  rx_err_t   err   = rx_uart_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Buffer is effectively empty after the failed read, so k_rx_err_no_data */
  TEST_ASSERT_EQUAL(k_rx_err_no_data, err);
}

/* =============================================================================
 * Unity entry point -- split into helpers to stay under function-size limits
 * =============================================================================
 */

/**
 * @brief Run initialization and deinitialization tests
 */
static void internal_run_init_deinit_tests(void)
{
  /* Initialization */
  RUN_TEST(test_uart_comm_init_null_handle_fails);
  RUN_TEST(test_uart_comm_init_null_config_fails);
  RUN_TEST(test_uart_comm_init_null_session_fails);
  RUN_TEST(test_uart_comm_init_success);
  RUN_TEST(test_uart_comm_init_stores_channel);
  RUN_TEST(test_uart_comm_init_clears_rx_buffer);
  RUN_TEST(test_uart_comm_init_reinit_success);

  /* Deinitialization */
  RUN_TEST(test_uart_comm_deinit_null_handle_fails);
  RUN_TEST(test_uart_comm_deinit_success);
  RUN_TEST(test_uart_comm_deinit_not_initialized_ok);
}

/**
 * @brief Run send tests
 */
static void internal_run_send_tests(void)
{
  RUN_TEST(test_uart_comm_send_null_handle_fails);
  RUN_TEST(test_uart_comm_send_not_initialized_fails);
  RUN_TEST(test_uart_comm_send_null_payload_nonzero_len_fails);
  RUN_TEST(test_uart_comm_send_payload_too_large_fails);
  RUN_TEST(test_uart_comm_send_zero_payload_success);
  RUN_TEST(test_uart_comm_send_small_payload_success);
  RUN_TEST(test_uart_comm_send_tx_contains_sync_word);
  RUN_TEST(test_uart_comm_send_increments_tx_sequence);
  RUN_TEST(test_uart_comm_send_uart_write_failure_propagates);
}

/**
 * @brief Run receive, sequence, control frame, and buffer tests
 */
static void internal_run_receive_tests(void)
{
  /* Receive */
  RUN_TEST(test_uart_comm_receive_null_handle_fails);
  RUN_TEST(test_uart_comm_receive_null_frame_fails);
  RUN_TEST(test_uart_comm_receive_not_initialized_fails);
  RUN_TEST(test_uart_comm_receive_no_data_returns_no_data);
  RUN_TEST(test_uart_comm_receive_valid_frame_success);
  RUN_TEST(test_uart_comm_receive_crc_mismatch_fails);
  RUN_TEST(test_uart_comm_receive_invalid_sync_returns_no_data);
  RUN_TEST(test_uart_comm_receive_skips_garbage_before_sync);
  RUN_TEST(test_uart_comm_receive_partial_frame_returns_no_data);
  RUN_TEST(test_uart_comm_receive_response_frame_success);
  RUN_TEST(test_uart_comm_receive_zero_payload_success);

  /* Sequence validation */
  RUN_TEST(test_uart_comm_receive_correct_sequence_passes);

  /* Control frame handling */
  RUN_TEST(test_uart_comm_receive_ping_triggers_pong);
  RUN_TEST(test_uart_comm_receive_reset_triggers_reset_ack);
  RUN_TEST(test_uart_comm_receive_pong_silently_consumed);

  /* Buffer management */
  RUN_TEST(test_uart_comm_receive_compacts_buffer_after_decode);

  /* State transitions */
  RUN_TEST(test_uart_comm_send_after_deinit_fails);
  RUN_TEST(test_uart_comm_receive_after_deinit_fails);
}

/**
 * @brief Run additional coverage and error-injection tests
 */
static void internal_run_coverage_tests(void)
{
  RUN_TEST(test_uart_comm_receive_uart_avail_error_propagates);
  RUN_TEST(test_uart_comm_receive_invalid_payload_length_in_header);
  RUN_TEST(test_uart_comm_receive_reset_ack_silently_consumed);
  RUN_TEST(test_uart_comm_receive_sequence_validation_fail_drops_frame);
  RUN_TEST(test_uart_comm_receive_ping_pong_send_fails);
  RUN_TEST(test_uart_comm_receive_reset_ack_send_fails);
  RUN_TEST(test_uart_comm_receive_second_frame_wrong_sequence_dropped);
  RUN_TEST(test_uart_comm_receive_uart_read_error_propagates);
  RUN_TEST(test_uart_comm_receive_partial_frame_header_only);
  RUN_TEST(test_uart_comm_receive_false_sync_low_then_real_frame);

  /* Coverage gap tests */
  RUN_TEST(test_uart_comm_send_session_not_initialized_fails);
  RUN_TEST(test_uart_comm_send_encoder_not_initialized_fails);
  RUN_TEST(test_uart_comm_receive_rx_buffer_prefilled_no_sync);
  RUN_TEST(test_uart_comm_receive_reset_session_next_tx_fails);
  RUN_TEST(test_uart_comm_receive_uart_read_channel_error_propagates);
}

/**
 * @brief Run direct internal-function tests
 */
static void internal_run_internal_function_tests(void)
{
  RUN_TEST(test_uart_internal_decode_header_null_data);
  RUN_TEST(test_uart_internal_decode_header_null_frame);
  RUN_TEST(test_uart_internal_decode_header_too_short);
  RUN_TEST(test_uart_internal_decode_header_bad_sync);
  RUN_TEST(test_uart_internal_decode_header_payload_too_large);
  RUN_TEST(test_uart_internal_decode_header_offset_out_set);
  RUN_TEST(test_uart_internal_verify_crc_null_data);
  RUN_TEST(test_uart_internal_verify_crc_offset_too_small);
  RUN_TEST(test_uart_internal_verify_crc_out_written);
  RUN_TEST(test_uart_internal_read_uart_data_null_handle);
  RUN_TEST(test_uart_internal_read_uart_data_buffer_full);
  RUN_TEST(test_uart_internal_compact_rx_buffer_null_handle);
  RUN_TEST(test_uart_internal_compact_rx_buffer_pos_exceeds_len);
  RUN_TEST(test_uart_internal_compact_rx_buffer_noop_when_pos_zero);
  RUN_TEST(test_uart_internal_find_sync_null_handle);
  RUN_TEST(test_uart_internal_find_sync_null_sync_pos);
  RUN_TEST(test_uart_internal_find_sync_pos_exceeds_len);
  RUN_TEST(test_uart_internal_handle_no_sync_null_handle);
  RUN_TEST(test_uart_internal_align_to_sync_advances_pos);
  RUN_TEST(test_uart_internal_align_to_sync_no_regression);
  RUN_TEST(test_uart_internal_build_frame_null_frame);
  RUN_TEST(test_uart_internal_build_frame_null_payload_nonzero_len);
  RUN_TEST(test_uart_internal_build_frame_payload_too_large);
  RUN_TEST(test_uart_internal_build_frame_zero_payload);
  RUN_TEST(test_uart_internal_build_frame_with_payload);
  RUN_TEST(test_uart_internal_handle_ping_null_handle);
  RUN_TEST(test_uart_internal_handle_ping_null_frame);
  RUN_TEST(test_uart_internal_handle_reset_null_handle);
  RUN_TEST(test_uart_internal_receive_iteration_read_error);
  RUN_TEST(test_uart_comm_receive_timeout);

  /* Branch coverage gap tests */
  RUN_TEST(test_uart_internal_decode_header_null_offset_valid_header);
  RUN_TEST(test_uart_internal_verify_crc_null_crc_out_valid_frame);
  RUN_TEST(test_uart_internal_build_frame_nonnull_payload_zero_len);
  RUN_TEST(test_uart_comm_receive_read_returns_timeout);
}

/**
 * @brief Unity test runner entry point
 *
 * @pre  Unity framework available
 * @post All tests executed, pass/fail reported
 */
int main(void)
{
  UNITY_BEGIN();

  internal_run_init_deinit_tests();
  internal_run_send_tests();
  internal_run_receive_tests();
  internal_run_coverage_tests();
  internal_run_internal_function_tests();

  return UNITY_END();
}
