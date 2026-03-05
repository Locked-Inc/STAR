/* star-rx72n-firmware/tests/test_rx_spi_comm.c */

/**
 * @file test_rx_spi_comm.c
 * @brief Unit Tests for SPI Frame-Based Communication Protocol (RPi5 <-> RX72N)
 *
 * @details
 * **Comprehensive Test Suite for SPI Frame Protocol with Handshake and CRC Validation**
 *
 * This test suite validates the rx_spi_comm layer - a frame-based SPI protocol that
 * enables reliable bidirectional communication between RPi5 (controller) and RX72N
 * (peripheral). Tests verify frame encoding, CRC validation, sequence management,
 * hardware handshake timing, and FEC integration for 10 Mbps data transfer.
 *
 * **SPI Communication Architecture:**
 * @code
 * RPi5 (Linux SPI Controller)          RX72N (RSPI Peripheral)
 * +----------------------+             +----------------------+
 * | spidev0.0            |   SCLK  ->   | RSPI Channel 0      |
 * | (10 MHz, Mode 1)     |   COPI  ->   | (Peripheral Mode)   |
 * |                      |   CIPO  <-   |                     |
 * |                      |   CS#   ->   | GPIO (software CS)  |
 * | GPIO (READY line) <-  |   RDY   <-   | GPIO out            |
 * +----------------------+             +----------------------+
 *         v Frame Protocol                     v
 * +----------------------+             +----------------------+
 * | rx_spi_comm (Linux)  |             | rx_spi_comm (RX72N) |
 * | - Frame encode       |             | - Frame decode      |
 * | - CRC-32 validation  |             | - CRC-32 check      |
 * | - Handshake polling  |             | - Ready signal      |
 * +----------------------+             +----------------------+
 * @endcode
 *
 * **Test Categories:**
 * 1. **Initialization** - Tests handle initialization and RSPI channel configuration
 * 2. **Frame Encoding** - Validates frame construction with CRC-32 and FEC parity
 * 3. **Frame Decoding** - Tests sync word detection, CRC validation, payload extraction
 * 4. **Send Operations** - Tests data/ACK/NACK transmission with handshake
 * 5. **Receive Operations** - Tests frame reception with timeout handling
 * 6. **Sequence Management** - Tests TX/RX sequence number incrementing and wrap-around
 * 7. **Data Available** - Tests non-blocking RX buffer fullness check
 * 8. **Handshake Protocol** - Tests READY line polling and timeout behavior
 * 9. **Error Injection** - Tests CRC errors, invalid sync, timeout, transfer failures
 * 10. **FEC Integration** - Tests optional Forward Error Correction flag handling
 * 11. **Channel Configuration** - Tests multi-channel RSPI support
 *
 * **SPI Frame Structure (Identical to USB CDC Frames):**
 * @code
 * Frame Format (variable length, 12-1036 bytes):
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
 * - SYNC   (2 bytes): 0xA55A - Frame start marker
 * - SEQ    (2 bytes): Sequence number (0x0000-0xFFFF, wraps to 0)
 * - LEN    (2 bytes): Payload length in bytes (0-1024)
 * - TYPE   (1 byte):  k_frame_type_command=0x01, k_frame_type_response=0x02,
 *                     k_frame_type_ack=0x10, k_frame_type_nack=0x11
 * - FLAGS  (1 byte):  k_frame_flag_fec_enabled=0x01, k_frame_flag_soft_nack=0x02
 * - Payload (0-1024): Variable-length data (protobuf messages)
 * - CRC-32 (4 bytes): IEEE 802.3 polynomial (0x04C11DB7)
 * @endcode
 *
 * **Hardware Handshake Protocol:**
 * @code
 * RPi5 (Controller)                RX72N (Peripheral)
 * ==================               ==================
 * 1. Wait for RDY line HIGH    <-   RX buffer empty -> Set RDY=HIGH
 * 2. Assert CS# LOW
 * 3. Transfer frame bytes
 * 4. Deassert CS# HIGH
 * 5. RX72N processes frame     ->   Parse, validate CRC, route to buffer
 * 6. Loop to step 1                If buffer full -> Set RDY=LOW
 *
 * Timing Constraints:
 * - RDY line poll timeout: 100ms (configurable via rx_spi_comm_send timeout param)
 * - CS# assertion to first clock: 50ns min (t_CSS)
 * - Last clock to CS# deassertion: 50ns min (t_CSH)
 * - Inter-frame gap (CS# HIGH): 400ns min (allows RX72N ISR to drain buffer)
 * @endcode
 *
 * **Protocol Test Sequences:**
 * @par Frame Send with Handshake Test:
 * @code
 * 1. Init rx_spi_comm with default config (channel 0, FEC disabled)
 * 2. Init RSPI peripheral via mock (sets ready state)
 * 3. Set mock RDY line = HIGH (peripheral ready to receive)
 * 4. Send frame with payload "Hello SPI!" (10 bytes)
 * 5. Assert: rspi_peripheral_write_ready polled (mock call tracking)
 * 6. Assert: rspi_peripheral_transfer called once
 * 7. Assert: tx_sequence incremented from 0 to 1
 * 8. Pop TX data from mock and validate SYNC, SEQ, LEN, CRC
 * @endcode
 *
 * @par Frame Receive with Valid CRC Test:
 * @code
 * 1. Init rx_spi_comm layer
 * 2. Init RSPI channel
 * 3. Encode valid frame with payload "TEST" externally (using rx_frame_encode)
 * 4. Inject encoded frame into mock RSPI RX buffer
 * 5. Call rx_spi_comm_receive with timeout=0 (non-blocking)
 * 6. Assert: Returns k_rx_ok
 * 7. Assert: Decoded frame.header.sequence matches sent value (42)
 * 8. Assert: frame.header.type == k_frame_type_command
 * 9. Assert: frame.payload contains "TEST"
 * 10. Assert: rx_sequence incremented to (received_seq + 1)
 * @endcode
 *
 * @par CRC Error Injection Test:
 * @code
 * 1. Encode valid frame with correct CRC
 * 2. Corrupt CRC byte (set CRC[0] = 0x00, 0x00, 0x00, 0x00 - invalid)
 * 3. Inject corrupted frame into RSPI RX buffer
 * 4. Call rx_spi_comm_receive
 * 5. Assert: Returns k_rx_err_protocol_error (CRC mismatch detected by rx_frame_decode)
 * @endcode
 *
 * @par Handshake Timeout Test:
 * @code
 * 1. Init rx_spi_comm
 * 2. Set mock RDY line = LOW (peripheral NOT ready - buffer full)
 * 3. Call rx_spi_comm_send with payload
 * 4. Assert: Returns k_rx_err_timeout (after polling rspi_peripheral_write_ready)
 * 5. Assert: rspi_peripheral_transfer was NOT called (no data sent)
 * 6. Assert: rspi_peripheral_write_ready was called multiple times (polling occurred)
 * @endcode
 *
 * @par Sequence Number Wrap Test:
 * @code
 * 1. Set tx_sequence = 0xFFFF manually
 * 2. Send frame (assigns sequence 0xFFFF)
 * 3. Assert: tx_sequence wraps to 0x0000 after send
 * 4. Send another frame
 * 5. Assert: Next frame has sequence 0x0000 (wrap confirmed)
 * @endcode
 *
 * @par FEC Flag Test:
 * @code
 * 1. Init with config.fec_enabled = true
 * 2. Send frame with payload "test"
 * 3. Pop TX data from mock and decode FLAGS byte (offset 7)
 * 4. Assert: (FLAGS & k_frame_flag_fec_enabled) != 0
 * 5. Indicates FEC parity bytes follow payload (handled by rx_fec layer)
 * @endcode
 *
 * **Timing Requirements:**
 * - SPI clock frequency: 10 MHz (100ns per bit)
 * - Frame transfer time (1024 bytes): ~820us (1024 * 8 bits / 10 MHz)
 * - Minimum frame (12 bytes): ~10us
 * - RDY line poll interval: 10us (software loop with volatile read)
 * - RDY line timeout: 100ms default (configurable)
 * - RSPI interrupt latency: <5us (RX FIFO threshold ISR)
 * - Total frame round-trip (send + ACK): <2ms typical
 *
 * **Error Injection Patterns:**
 * - **Invalid sync word** - Tests resync logic (returns k_rx_err_protocol_error)
 * - **CRC mismatch** - Tests error detection (returns k_rx_err_protocol_error)
 * - **Payload length > max** - Tests bounds checking (returns k_rx_err_invalid_size)
 * - **nullptr payload with len > 0** - Tests input validation (returns k_rx_err_invalid_arg)
 * - **Handshake timeout** - Tests RDY line polling (returns k_rx_err_timeout)
 * - **SPI transfer failure** - Tests HAL error propagation (returns k_rx_err_spi_error)
 * - **Channel not initialized** - Tests state validation (returns k_rx_err_invalid_state)
 *
 * **Mock State Management:**
 * - **mock_rspi:** Simulates RSPI peripheral registers, TX/RX FIFOs, transfer state
 * - **Call tracking:** Records function calls (rspi_peripheral_transfer, write_ready, etc.)
 * - **Injectable responses:** Allows test-controlled return codes (k_rx_ok, k_rx_err_timeout)
 * - **RDY line simulation:** Controls write_ready return value (true/false)
 * - **RX buffer injection:** mock_rspi_inject_rx_data() simulates incoming frames
 *
 * **Critical Design Decisions:**
 * - **Software Chip Select:** CS# controlled via GPIO, not RSPI hardware CS (for flexibility)
 * - **Polling RDY Line:** No interrupt-driven handshake (simplifies implementation)
 * - **No Retry Logic:** Comm layer does not retry - relies on higher-level ARQ (HARQ)
 * - **Sequence Number Policy:** Sender increments, receiver tracks (no validation yet)
 * - **FEC Optional:** FEC flag in frame header, actual FEC encoding by separate rx_fec layer
 * - **Non-Blocking:** All operations return immediately (no busy-wait in comm layer)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (Control Flow):** [OK] All test functions use simple sequential flow
 * - **Rule 2 (Loop Bounds):** [OK] All loops have compile-time known bounds
 * - **Rule 3 (Dynamic Memory):** [OK] Zero heap allocation (stack buffers only)
 * - **Rule 4 (Function Size):** [OK] Test functions <50 lines, helpers <25 lines
 * - **Rule 5 (Assertions):** [OK] Every test has minimum 1 assertion, most have 3+
 * - **Rule 7 (Return Checking):** [OK] All API returns validated
 * - **Rule 9 (Pointers):** [OK] Single-level dereferencing only
 * - **Rule 10 (Warnings):** [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Each test validates one SPI protocol behavior
 * - **Open/Closed:** Mock injection allows testing without RSPI hardware
 * - **Liskov Substitution:** Mock RSPI substitutes for real peripheral transparently
 * - **Interface Segregation:** Tests use minimal API (init/send/receive/deinit)
 * - **Dependency Inversion:** Depends on RSPI abstraction, not RSPI registers
 *
 * @par Module Dependencies:
 * - rx_spi_comm.h - Frame-based SPI comm layer API under test
 * - rx_frame.h - Frame encoding/decoding primitives
 * - mock_rspi.h - Mock RSPI peripheral driver
 * - rx_err.h - Error code definitions
 * - unity.h - Unit testing framework
 *
 * @see rx_spi_comm.h - SPI comm layer header
 * @see rx_frame.h - Frame structure and encoding/decoding
 * @see mock_rspi.h - Mock RSPI driver for testing
 * @see RX72N User's Manual section 33 - RSPI module specification
 * @see docs/sections/01_nanopb_protocol.tex - Full protocol specification
 *
 * @author STAR Team
 * @date 2026-01-30
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @test Tests run via Unity framework with: make test_rx_spi_comm
 */

#include <string.h>

#include "hardware.h"
#include "mock_rspi.h"
#include "rx_frame.h"
#include "rx_session.h"
#include "rx_spi_comm.h"
#include "unity.h"

/* =============================================================================
 * Mock Assertion Helpers
 * =============================================================================
 */

/**
 * @brief Assert mock RX data injection succeeds
 * @param[in] channel RSPI channel
 * @param[in] data    Data to inject
 * @param[in] len     Data length
 */
static void internal_assert_inject_rx(rspi_channel_t channel, const uint8_t* data, uint32_t len)
{
  TEST_ASSERT_EQUAL(k_rx_ok, mock_rspi_inject_rx_data(nullptr, channel, data, len));
}

/**
 * @brief Assert mock TX data readback succeeds
 * @param[in]  channel    RSPI channel
 * @param[out] data       Buffer to receive TX data
 * @param[in]  max_len    Buffer size
 * @param[out] actual_len Actual bytes read
 */
static void internal_assert_get_tx(rspi_channel_t channel,
                                   uint8_t*       data,
                                   uint32_t       max_len,
                                   uint32_t*      actual_len)
{
  TEST_ASSERT_EQUAL(k_rx_ok, mock_rspi_get_tx_data(nullptr, channel, data, max_len, actual_len));
}

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Test constant values */
typedef enum : uint16_t {
  k_test_sequence_a     = 42,
  k_test_sequence_b     = 123,
  k_test_sequence_max   = 0xFFFF,
  k_test_timeout_zero   = 0,
  k_test_timeout_short  = 100,
  k_test_payload_small  = 4,
  k_test_payload_medium = 64,
  k_test_payload_large  = 256,
} test_constants_t;

/** @brief Retransmit test constant values */
typedef enum : uint16_t {
  k_test_retransmit_max_retries_custom    = 5,
  k_test_retransmit_ack_timeout_custom_ms = 100,
  k_test_retransmit_max_backoff_custom_ms = 500,
  k_test_retransmit_max_backoff_low_ms    = 100,
  k_test_retransmit_high_max_retries      = 10,
  k_test_retransmit_single_retry          = 1,
  k_test_retransmit_limit_check_time_ms   = 200,
  k_test_retransmit_flags_offset          = 7,
} test_retransmit_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_spi_comm_handle_t s_handle;
static rx_session_state_t   s_session;

/* Control frame callback tracking */
static uint32_t   s_ping_cb_count;
static rx_frame_t s_last_ping_frame;
static uint32_t   s_reset_cb_count;
static rx_frame_t s_last_reset_frame;
static void*      s_last_cb_ctx;

/* Retransmit callback tracking */
static uint32_t s_ack_cb_count;
static uint16_t s_last_ack_sequence;
static uint32_t s_nack_cb_count;
static uint16_t s_last_nack_sequence;
static void*    s_last_retransmit_cb_ctx;

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

/**
 * @brief Initialize RSPI channel via mock so channel is ready
 */
static void helper_init_rspi_channel(rspi_channel_t channel)
{
  const rspi_config_t config = {
    .spi_mode  = (rspi_mode_t)k_spi_comm_default_mode,
    .use_16bit = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rspi_init_peripheral(channel, &config));
}

/**
 * @brief Create and encode a valid frame for injection
 *
 * @param type Frame type
 * @param sequence Sequence number
 * @param payload Payload data (can be nullptr)
 * @param payload_len Payload length
 * @param out_buffer Output buffer
 * @param out_len Output length
 */
static void helper_create_encoded_frame(rx_frame_type_t type,
                                        uint16_t        sequence,
                                        const uint8_t*  payload,
                                        uint32_t        payload_len,
                                        uint8_t*        out_buffer,
                                        uint32_t*       out_len)
{
  rx_frame_encoder_t encoder;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&encoder));

  rx_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.header.sequence = sequence;
  frame.header.length   = (uint16_t)payload_len;
  frame.header.type     = (uint8_t)type;
  frame.header.flags    = k_frame_flag_none;

  if (payload != nullptr && payload_len > 0) {
    memcpy(frame.payload, payload, payload_len);
  }

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&encoder, &frame, out_buffer, out_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&encoder));
}

/**
 * @brief ACK notification callback for retransmit tests
 */
static void test_ack_callback(uint16_t sequence, void* ctx)
{
  s_ack_cb_count++;
  s_last_ack_sequence      = sequence;
  s_last_retransmit_cb_ctx = ctx;
}

/**
 * @brief NACK notification callback for retransmit tests
 */
static void test_nack_callback(uint16_t sequence, void* ctx)
{
  s_nack_cb_count++;
  s_last_nack_sequence     = sequence;
  s_last_retransmit_cb_ctx = ctx;
}

void setUp(void)
{
  /* Initialize mock hardware */
  mock_rspi_init(nullptr);

  /* Initialize shared session */
  (void)rx_session_init(&s_session);

  /* Clear handle */
  memset(&s_handle, 0, sizeof(s_handle));

  /* Reset callback tracking */
  s_ping_cb_count  = 0;
  s_reset_cb_count = 0;
  memset(&s_last_ping_frame, 0, sizeof(s_last_ping_frame));
  memset(&s_last_reset_frame, 0, sizeof(s_last_reset_frame));
  s_last_cb_ctx = NULL;

  /* Reset retransmit callback tracking */
  s_ack_cb_count           = 0;
  s_nack_cb_count          = 0;
  s_last_ack_sequence      = 0;
  s_last_nack_sequence     = 0;
  s_last_retransmit_cb_ctx = NULL;
}

void tearDown(void)
{
  /* Deinitialize comm layer if initialized */
  (void)rx_spi_comm_deinit(&s_handle);

  /* Deinitialize session */
  (void)rx_session_deinit(&s_session);

  /* Clear mock state */
  mock_rspi_deinit(nullptr);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_spi_comm_init_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_init(nullptr, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_init_success_default_config(void)
{
  rx_err_t err = rx_spi_comm_init(&s_handle,
                                  &(rx_spi_comm_config_t){.session     = &s_session,
                                                          .channel     = k_rspi_channel_0,
                                                          .spi_mode    = 0,
                                                          .fec_enabled = false});

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
  TEST_ASSERT_EQUAL_UINT8(k_rspi_channel_0, s_handle.channel);
  TEST_ASSERT_FALSE(s_handle.fec_enabled);
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_tx_seq());
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());
}

void test_spi_comm_init_with_custom_channel(void)
{
  rx_spi_comm_config_t config = {
    .session     = &s_session,
    .channel     = k_rspi_channel_1,
    .spi_mode    = 0,
    .fec_enabled = false,
  };

  rx_err_t err = rx_spi_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_rspi_channel_1, s_handle.channel);
}

void test_spi_comm_init_with_fec_enabled(void)
{
  rx_spi_comm_config_t config = {
    .session     = &s_session,
    .channel     = k_rspi_channel_0,
    .spi_mode    = 0,
    .fec_enabled = true,
  };

  rx_err_t err = rx_spi_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.fec_enabled);
}

void test_spi_comm_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_deinit(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_deinit_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));

  rx_err_t err = rx_spi_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

void test_spi_comm_deinit_not_initialized_succeeds(void)
{
  /* Deinit on uninitialized handle should succeed gracefully */
  rx_err_t err = rx_spi_comm_deinit(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Send Tests
 * =============================================================================
 */

void test_spi_comm_send_null_handle_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(nullptr, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_not_initialized_fails(void)
{
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_null_payload_with_len_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, nullptr, 10);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_payload_too_large_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  uint8_t data[k_frame_max_payload + 1];

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, sizeof(data));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_spi_comm_send_empty_payload_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_tx_seq());
}

void test_spi_comm_send_with_payload_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  uint8_t data[] = "Hello SPI!";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 10);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_tx_seq());
}

void test_spi_comm_send_increments_sequence(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  uint8_t data[] = "test";

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));

  TEST_ASSERT_EQUAL_UINT16(3, helper_get_tx_seq());
}

void test_spi_comm_send_sequence_wraps(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  s_session.tx_sequence = k_test_sequence_max;
  uint8_t data[]        = "test";

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));

  /* 0xFFFF + 1 wraps to 0x0000 */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_tx_seq());
}

void test_spi_comm_send_with_fec_flag(void)
{
  rx_spi_comm_config_t config = {.session     = &s_session,
                                 .channel     = k_rspi_channel_0,
                                 .spi_mode    = 0,
                                 .fec_enabled = true};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify FEC flag is set in the transmitted frame */
  uint8_t  tx_data[64];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  /* Frame: [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)]... */
  /* Flags are at offset 7 (0-based) */
  TEST_ASSERT_GREATER_THAN(8, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_fec_enabled, tx_data[7] & k_frame_flag_fec_enabled);
}

void test_spi_comm_send_transfer_error_propagates(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_set_transfer_return(nullptr, k_rx_err_timeout);
  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_spi_comm_send_large_payload(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[k_test_payload_large];
  for (uint32_t i = 0; i < k_test_payload_large; i++) {
    data[i] = (uint8_t)(i & 0xFF);
  }

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, k_test_payload_large);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_spi_comm_send_missing_host_ack_times_out(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_clear_calls(nullptr);
  mock_rspi_set_write_ready(nullptr, k_rspi_channel_0, false);

  uint8_t data[] = "test";

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  uint32_t transfer_calls = mock_rspi_get_call_count(nullptr, "rspi_peripheral_transfer");
  TEST_ASSERT_EQUAL_UINT32(0, transfer_calls);

  uint32_t ready_calls = mock_rspi_get_call_count(nullptr, "rspi_peripheral_write_ready");
  TEST_ASSERT_TRUE(ready_calls > 0);
}

/* =============================================================================
 * Send ACK/NACK Tests
 * =============================================================================
 */

void test_spi_comm_send_ack_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_send_ack(nullptr, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_ack_not_initialized_fails(void)
{
  rx_err_t err = rx_spi_comm_send_ack(&s_handle, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_ack_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  rx_err_t err = rx_spi_comm_send_ack(&s_handle, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify ACK frame was transmitted */
  uint8_t  tx_data[32];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  TEST_ASSERT_EQUAL(k_frame_min_size, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_ack, tx_data[6]);
}

void test_spi_comm_send_ack_transfer_error_propagates(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_set_transfer_return(nullptr, k_rx_err_timeout);

  rx_err_t err = rx_spi_comm_send_ack(&s_handle, k_test_sequence_a);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_spi_comm_send_nack_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_send_nack(nullptr, k_test_sequence_a, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_nack_not_initialized_fails(void)
{
  rx_err_t err = rx_spi_comm_send_nack(&s_handle, k_test_sequence_a, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_nack_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  rx_err_t err = rx_spi_comm_send_nack(&s_handle, k_test_sequence_b, k_frame_flag_soft_nack);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify NACK frame was transmitted */
  uint8_t  tx_data[32];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  TEST_ASSERT_EQUAL(k_frame_min_size, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_nack, tx_data[6]);
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_soft_nack, tx_data[7] & k_frame_flag_soft_nack);
}

void test_spi_comm_send_nack_transfer_error_propagates(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_set_transfer_return(nullptr, k_rx_err_timeout);

  rx_err_t err = rx_spi_comm_send_nack(&s_handle, k_test_sequence_a, 0);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Receive Tests
 * =============================================================================
 */

void test_spi_comm_receive_null_handle_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_spi_comm_receive(nullptr, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_receive_null_frame_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));

  rx_err_t err = rx_spi_comm_receive(&s_handle, nullptr, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_receive_not_initialized_fails(void)
{
  rx_frame_t frame;

  rx_err_t err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_receive_no_data_timeout(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  rx_frame_t frame;

  /* No data injected, should timeout immediately */
  rx_err_t err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_spi_comm_receive_available_check_error_propagates(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_set_available_return(nullptr, k_rx_err_spi_error);
  rx_frame_t frame;

  rx_err_t err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_spi_error, err);
}

void test_spi_comm_receive_valid_frame_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Create and inject a valid frame (seq=0 matches initial session rx_sequence) */
  uint8_t  payload[] = "TEST";
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command, 0, payload, 4, encoded_frame, &encoded_len);

  internal_assert_inject_rx(k_rspi_channel_0, encoded_frame, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, frame.header.sequence);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(4, frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY(payload, frame.payload, 4);
}

void test_spi_comm_receive_updates_rx_sequence(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Session starts at rx_sequence=0, so receive frame with matching sequence */
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command, 0, nullptr, 0, encoded_frame, &encoded_len);

  internal_assert_inject_rx(k_rspi_channel_0, encoded_frame, encoded_len);

  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero));

  /* RX sequence should be updated to received sequence + 1 */
  TEST_ASSERT_EQUAL_UINT16(1, helper_get_rx_seq());
}

void test_spi_comm_receive_invalid_sync_word(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Create a frame with invalid sync word */
  uint8_t bad_frame[k_frame_min_size] = {
    0x00,
    0x00, /* Invalid SYNC */
    0x00,
    0x01, /* SEQ = 1 */
    0x00,
    0x00, /* LEN = 0 */
    0x01, /* TYPE = command */
    0x00, /* FLAGS = none */
    0x00,
    0x00,
    0x00,
    0x00, /* CRC (invalid) */
  };

  internal_assert_inject_rx(k_rspi_channel_0, bad_frame, sizeof(bad_frame));

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

void test_spi_comm_receive_transfer_error_propagates(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Inject valid frame to pass availability check */
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command, 0, nullptr, 0, encoded_frame, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded_frame, encoded_len);

  /* But make transfer fail */
  mock_rspi_set_transfer_return(nullptr, k_rx_err_spi_error);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL(k_rx_err_spi_error, err);
}

/* =============================================================================
 * Data Available Tests
 * =============================================================================
 */

void test_spi_comm_data_available_null_handle_fails(void)
{
  bool available;

  rx_err_t err = rx_spi_comm_data_available(nullptr, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_data_available_null_available_fails(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));

  rx_err_t err = rx_spi_comm_data_available(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_data_available_not_initialized_fails(void)
{
  bool available;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_data_available_empty(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  bool available = true;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(available);
}

void test_spi_comm_data_available_with_data(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);
  bool available = false;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(available);
}

void test_spi_comm_data_available_hal_error_propagates(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_set_available_return(nullptr, k_rx_err_spi_error);
  bool available;

  rx_err_t err = rx_spi_comm_data_available(&s_handle, &available);

  TEST_ASSERT_EQUAL(k_rx_err_spi_error, err);
}

/* Sequence management now handled by rx_session_state_t.
 * See test_rx_session.c for sequence utility tests.
 */

void test_spi_comm_init_null_config_fails(void)
{
  rx_err_t err = rx_spi_comm_init(&s_handle, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_init_null_session_fails(void)
{
  rx_spi_comm_config_t config = {
    .session     = nullptr,
    .channel     = k_rspi_channel_0,
    .spi_mode    = 0,
    .fec_enabled = false,
  };

  rx_err_t err = rx_spi_comm_init(&s_handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_rx_sequence_wraparound(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Pre-advance session RX sequence to test wraparound at 0xFFFF -> 0 */
  s_session.rx_sequence = k_test_sequence_max;

  /* Receive a frame with sequence 0xFFFF (exact match with pre-advanced session) */
  uint8_t  encoded_frame[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_command,
                              k_test_sequence_max,
                              nullptr,
                              0,
                              encoded_frame,
                              &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded_frame, encoded_len);

  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero));

  /* RX sequence should wrap to 0 (0xFFFF + 1 = 0x0000) */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());
}

/* =============================================================================
 * Channel Configuration Tests
 * =============================================================================
 */

void test_spi_comm_uses_configured_channel(void)
{
  rx_spi_comm_config_t config = {.session     = &s_session,
                                 .channel     = k_rspi_channel_1,
                                 .spi_mode    = 0,
                                 .fec_enabled = false};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_1);
  uint8_t data[] = "test";

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));

  /* Verify transfer was called on channel 1 */
  mock_rspi_call_t call;
  rx_err_t         err = mock_rspi_get_last_call(nullptr, "rspi_peripheral_transfer", &call);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_rspi_channel_1, call.channel);
}

/* =============================================================================
 * Buffer Size Tests
 * =============================================================================
 */

void test_spi_comm_buffer_size_constants(void)
{
  /* Verify buffer size constants are reasonable */
  TEST_ASSERT_EQUAL(2048, k_spi_comm_rx_buffer_size);
  TEST_ASSERT_EQUAL(2048, k_spi_comm_tx_buffer_size);
  TEST_ASSERT_GREATER_OR_EQUAL(k_frame_max_size, k_spi_comm_tx_buffer_size);
}

void test_spi_comm_max_payload_fits_in_buffer(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Create maximum size payload */
  uint8_t data[k_frame_max_payload];
  memset(data, 0xAA, k_frame_max_payload);

  rx_err_t err = rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, k_frame_max_payload);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Mock Verification Tests
 * =============================================================================
 */

void test_spi_comm_transfer_is_called_on_send(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_clear_calls(nullptr);
  uint8_t data[] = "test";

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));

  TEST_ASSERT_TRUE(mock_rspi_was_called(nullptr, "rspi_peripheral_transfer"));
}

void test_spi_comm_available_is_called_on_receive(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  mock_rspi_clear_calls(nullptr);
  rx_frame_t frame;

  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_TRUE(mock_rspi_was_called(nullptr, "rspi_peripheral_read_available"));
}

void test_spi_comm_transfer_count(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_init(&s_handle,
                                     &(rx_spi_comm_config_t){.session     = &s_session,
                                                             .channel     = k_rspi_channel_0,
                                                             .spi_mode    = 0,
                                                             .fec_enabled = false}));
  helper_init_rspi_channel(k_rspi_channel_0);
  uint8_t data[] = "test";

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_send(&s_handle, k_frame_type_response, 0, data, 4));

  /* Each send calls transfer once */
  uint32_t count = mock_rspi_get_call_count(nullptr, "rspi_peripheral_transfer");
  TEST_ASSERT_EQUAL_UINT32(3, count);
}

/* =============================================================================
 * Control Frame Callback Helpers
 * =============================================================================
 */

static void test_ping_callback(const rx_frame_t* frame, void* ctx)
{
  s_ping_cb_count++;
  if (frame != NULL) {
    memcpy(&s_last_ping_frame, frame, sizeof(rx_frame_t));
  }
  s_last_cb_ctx = ctx;
}

static void test_reset_callback(const rx_frame_t* frame, void* ctx)
{
  s_reset_cb_count++;
  if (frame != NULL) {
    memcpy(&s_last_reset_frame, frame, sizeof(rx_frame_t));
  }
  s_last_cb_ctx = ctx;
}

/* =============================================================================
 * Set Control Callbacks Tests
 * =============================================================================
 */

void test_spi_comm_set_callbacks_null_handle_fails(void)
{
  rx_err_t err =

    rx_spi_comm_set_control_callbacks(NULL, test_ping_callback, test_reset_callback, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_set_callbacks_success(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  uint32_t ctx_val = 42;

  rx_err_t err =
    rx_spi_comm_set_control_callbacks(&s_handle, test_ping_callback, test_reset_callback, &ctx_val);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(test_ping_callback, s_handle.on_ping_cb);
  TEST_ASSERT_EQUAL_PTR(test_reset_callback, s_handle.on_reset_cb);
  TEST_ASSERT_EQUAL_PTR(&ctx_val, s_handle.control_cb_ctx);
}

/* =============================================================================
 * Send PONG Tests
 * =============================================================================
 */

void test_spi_comm_send_pong_null_handle_fails(void)
{
  uint8_t payload[] = {0x01};

  rx_err_t err = rx_spi_comm_send_pong(NULL, payload, 1);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_pong_not_initialized_fails(void)
{
  uint8_t payload[] = {0x01};

  rx_err_t err = rx_spi_comm_send_pong(&s_handle, payload, 1);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_pong_echoes_payload(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t  payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  rx_err_t err       = rx_spi_comm_send_pong(&s_handle, payload, k_test_payload_small);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify PONG frame was transmitted */
  uint8_t  tx_data[64];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  /* PONG frame: 8 header + 4 payload + 4 CRC = 16 bytes */
  TEST_ASSERT_EQUAL_UINT32(16, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_pong, tx_data[6]);

  /* PONG should echo the payload (starts at offset 8) */
  TEST_ASSERT_EQUAL_MEMORY(payload, &tx_data[8], k_test_payload_small);
}

/* =============================================================================
 * Send RESET_ACK Tests
 * =============================================================================
 */

void test_spi_comm_send_reset_ack_null_handle_fails(void)
{
  rx_err_t err = rx_spi_comm_send_reset_ack(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_spi_comm_send_reset_ack_not_initialized_fails(void)
{
  rx_err_t err = rx_spi_comm_send_reset_ack(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_spi_comm_send_reset_ack_success(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  rx_err_t err = rx_spi_comm_send_reset_ack(&s_handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify RESET_ACK frame was transmitted */
  uint8_t  tx_data[32];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  /* RESET_ACK frame: 8 header + 0 payload + 4 CRC = 12 bytes */
  TEST_ASSERT_EQUAL_UINT32(k_frame_min_size, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_reset_ack, tx_data[6]);
}

/* =============================================================================
 * Receive PING Auto-PONG Tests
 * =============================================================================
 */

void test_spi_comm_receive_ping_auto_pong(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Create PING frame with 4-byte counter payload */
  uint8_t  ping_payload[] = {0x00, 0x00, 0x00, 0x01};
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ping,
                              0,
                              ping_payload,
                              k_test_payload_small,
                              encoded,
                              &encoded_len);

  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* PING consumed, no more data available -> timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Verify PONG was transmitted (last SPI transfer was the PONG send) */
  uint8_t  tx_data[64];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  /* PONG frame: 8 header + 4 payload + 4 CRC = 16 bytes */
  TEST_ASSERT_EQUAL_UINT32(16, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_pong, tx_data[6]);

  /* PONG should echo the PING payload (offset 8) */
  TEST_ASSERT_EQUAL_MEMORY(ping_payload, &tx_data[8], k_test_payload_small);
}

void test_spi_comm_receive_ping_then_command(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Create PING frame */
  uint8_t  ping_payload[] = {0x00, 0x00, 0x00, 0x02};
  uint8_t  ping_encoded[64];
  uint32_t ping_len = 0;
  helper_create_encoded_frame(k_frame_type_ping,
                              0,
                              ping_payload,
                              k_test_payload_small,
                              ping_encoded,
                              &ping_len);

  /* Create COMMAND frame */
  uint8_t  cmd_payload[] = "DATA";
  uint8_t  cmd_encoded[64];
  uint32_t cmd_len = 0;
  helper_create_encoded_frame(k_frame_type_command,
                              1,
                              cmd_payload,
                              k_test_payload_small,
                              cmd_encoded,
                              &cmd_len);

  /*
   * Concatenate PING + padding + COMMAND into injection buffer.
   *
   * SPI is full-duplex: the PONG send transfer also reads from the RX
   * buffer.  The PONG wire frame is the same size as the PING (both carry
   * a 4-byte payload), so insert that many padding bytes between the two
   * frames so the PONG send consumes the padding instead of the COMMAND.
   */
  const uint32_t pong_wire_len = ping_len; /* PONG echoes same payload size */
  uint8_t        combined[192];
  uint32_t       offset = 0;

  memcpy(combined + offset, ping_encoded, ping_len);
  offset += ping_len;
  memset(combined + offset, 0, pong_wire_len); /* padding consumed by PONG TX */
  offset += pong_wire_len;
  memcpy(combined + offset, cmd_encoded, cmd_len);
  offset += cmd_len;
  internal_assert_inject_rx(k_rspi_channel_0, combined, offset);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* PING consumed internally, COMMAND returned to caller */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_command, frame.header.type);
  TEST_ASSERT_EQUAL_UINT16(k_test_payload_small, frame.header.length);
  TEST_ASSERT_EQUAL_MEMORY("DATA", frame.payload, k_test_payload_small);
}

/* =============================================================================
 * Receive RESET Auto-ACK Tests
 * =============================================================================
 */

void test_spi_comm_receive_reset_auto_ack(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Set non-zero sequences to verify reset */
  s_session.tx_sequence = 50;
  s_session.rx_sequence = 100;

  /* Create RESET frame (no payload) */
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_reset, 0, NULL, 0, encoded, &encoded_len);

  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* RESET consumed, no more data -> timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  /* Verify RESET_ACK was transmitted */
  uint8_t  tx_data[32];
  uint32_t tx_len = 0;
  internal_assert_get_tx(k_rspi_channel_0, tx_data, sizeof(tx_data), &tx_len);

  TEST_ASSERT_EQUAL_UINT32(k_frame_min_size, tx_len);
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_reset_ack, tx_data[6]);

  /* Verify sequences were reset to 0 */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_tx_seq());
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());
}

/* =============================================================================
 * Control Frame Callback Tests
 * =============================================================================
 */

void test_spi_comm_receive_ping_callback_invoked(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  uint32_t ctx_value = 42;
  (void)rx_spi_comm_set_control_callbacks(&s_handle,
                                          test_ping_callback,
                                          test_reset_callback,
                                          &ctx_value);

  /* Create and inject PING */
  uint8_t  ping_payload[] = {0x00, 0x00, 0x00, 0x03};
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ping,
                              0,
                              ping_payload,
                              k_test_payload_small,
                              encoded,
                              &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Verify ping callback was invoked exactly once */
  TEST_ASSERT_EQUAL_UINT32(1, s_ping_cb_count);
  TEST_ASSERT_EQUAL_UINT32(0, s_reset_cb_count);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_ping, s_last_ping_frame.header.type);
  TEST_ASSERT_EQUAL_MEMORY(ping_payload, s_last_ping_frame.payload, k_test_payload_small);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_last_cb_ctx);
}

void test_spi_comm_receive_reset_callback_invoked(void)
{
  (void)rx_spi_comm_init(&s_handle,
                         &(rx_spi_comm_config_t){.session     = &s_session,
                                                 .channel     = k_rspi_channel_0,
                                                 .spi_mode    = 0,
                                                 .fec_enabled = false});
  helper_init_rspi_channel(k_rspi_channel_0);

  s_session.tx_sequence = 50;
  s_session.rx_sequence = 100;

  uint32_t ctx_value = 99;
  (void)rx_spi_comm_set_control_callbacks(&s_handle,
                                          test_ping_callback,
                                          test_reset_callback,
                                          &ctx_value);

  /* Create and inject RESET */
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_reset, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Verify reset callback was invoked exactly once */
  TEST_ASSERT_EQUAL_UINT32(0, s_ping_cb_count);
  TEST_ASSERT_EQUAL_UINT32(1, s_reset_cb_count);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_reset, s_last_reset_frame.header.type);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_last_cb_ctx);

  /* Verify sequences were reset */
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_tx_seq());
  TEST_ASSERT_EQUAL_UINT16(0, helper_get_rx_seq());
}

/* =============================================================================
 * Retransmit Configuration Tests
 * =============================================================================
 */

void test_retransmit_disabled_by_default(void)
{
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));

  TEST_ASSERT_FALSE(s_handle.auto_retransmit);
  TEST_ASSERT_FALSE(s_handle.retry_pending);
}

void test_retransmit_enabled_via_config(void)
{
  rx_spi_comm_config_t config = {
    .session         = &s_session,
    .channel         = 0,
    .spi_mode        = 0,
    .fec_enabled     = false,
    .auto_retransmit = true,
    .retransmit_config =
      {
        .max_retries    = k_test_retransmit_max_retries_custom,
        .ack_timeout_ms = k_test_retransmit_ack_timeout_custom_ms,
        .max_backoff_ms = k_test_retransmit_max_backoff_custom_ms,
      },
  };

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));

  TEST_ASSERT_TRUE(s_handle.auto_retransmit);
  TEST_ASSERT_EQUAL_UINT8(k_test_retransmit_max_retries_custom,
                          s_handle.retransmit_cfg.max_retries);
  TEST_ASSERT_EQUAL_UINT16(k_test_retransmit_ack_timeout_custom_ms,
                           s_handle.retransmit_cfg.ack_timeout_ms);
  TEST_ASSERT_EQUAL_UINT16(k_test_retransmit_max_backoff_custom_ms,
                           s_handle.retransmit_cfg.max_backoff_ms);
}

void test_retransmit_defaults_applied_for_zero_values(void)
{
  rx_spi_comm_config_t config = {
    .session           = &s_session,
    .channel           = 0,
    .spi_mode          = 0,
    .fec_enabled       = false,
    .auto_retransmit   = true,
    .retransmit_config = {0},
  };

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));

  TEST_ASSERT_TRUE(s_handle.auto_retransmit);
  TEST_ASSERT_EQUAL_UINT8(k_retransmit_default_max_retries, s_handle.retransmit_cfg.max_retries);
  TEST_ASSERT_EQUAL_UINT16(k_retransmit_default_ack_timeout_ms,
                           s_handle.retransmit_cfg.ack_timeout_ms);
  TEST_ASSERT_EQUAL_UINT16(k_retransmit_default_max_backoff_ms,
                           s_handle.retransmit_cfg.max_backoff_ms);
}

/* =============================================================================
 * Retransmit Send Buffering Tests
 * =============================================================================
 */

void test_retransmit_send_buffers_frame(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));

  TEST_ASSERT_TRUE(s_handle.retry_pending);
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.retry_sequence);
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);
  TEST_ASSERT_TRUE(s_handle.retry_wire_len > 0);
}

void test_retransmit_send_not_buffered_when_off(void)
{
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));

  TEST_ASSERT_FALSE(s_handle.retry_pending);
  TEST_ASSERT_EQUAL_UINT32(0, s_handle.retry_wire_len);
}

void test_retransmit_send_only_requires_ack_buffered(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_none,
                                     data,
                                     k_test_payload_small));

  TEST_ASSERT_FALSE(s_handle.retry_pending);
}

void test_retransmit_send_overwrites_on_new_send(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data1[] = "aaaa";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data1,
                                     k_test_payload_small));
  TEST_ASSERT_EQUAL_UINT16(0, s_handle.retry_sequence);

  uint8_t data2[] = "bbbb";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data2,
                                     k_test_payload_small));
  TEST_ASSERT_EQUAL_UINT16(1, s_handle.retry_sequence);
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);
  TEST_ASSERT_TRUE(s_handle.retry_pending);
}

/* =============================================================================
 * Retransmit ACK Consumption Tests
 * =============================================================================
 */

void test_retransmit_ack_clears_retry(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  TEST_ASSERT_TRUE(s_handle.retry_pending);

  /* Inject ACK frame with matching sequence (0) */
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* ACK consumed, no more data -> timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_FALSE(s_handle.retry_pending);
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);
}

void test_retransmit_ack_wrong_seq_ignored(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));

  /* Inject ACK with wrong sequence (99 instead of 0) */
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ack, k_test_sequence_b, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* ACK consumed (still consumed in loop) but retry NOT cleared */
  TEST_ASSERT_TRUE(s_handle.retry_pending);
}

void test_retransmit_ack_callback_invoked(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint32_t ctx_value = k_test_sequence_a;
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_set_retransmit_callbacks(&s_handle,
                                                         test_ack_callback,
                                                         test_nack_callback,
                                                         &ctx_value));

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));

  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL_UINT32(1, s_ack_cb_count);
  TEST_ASSERT_EQUAL_UINT16(0, s_last_ack_sequence);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_last_retransmit_cb_ctx);
  TEST_ASSERT_EQUAL_UINT32(0, s_nack_cb_count);
}

void test_retransmit_ack_not_consumed_when_off(void)
{
  /* Init WITHOUT auto_retransmit */
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* Inject ACK frame */
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* ACK returned to caller as regular frame */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_ack, frame.header.type);
}

void test_retransmit_ack_non_pending_ignored(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  /* NO send -> no pending retry */
  TEST_ASSERT_FALSE(s_handle.retry_pending);

  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_ack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* ACK consumed (auto_retransmit is on), but nothing to clear */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_FALSE(s_handle.retry_pending);
}

/* =============================================================================
 * Retransmit NACK Handling Tests
 * =============================================================================
 */

void test_retransmit_nack_triggers_retry(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);

  /* Inject NACK with matching sequence */
  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_nack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* Retransmit occurred: retry_count incremented, still pending */
  TEST_ASSERT_EQUAL_UINT8(1, s_handle.retry_count);
  TEST_ASSERT_TRUE(s_handle.retry_pending);
}

void test_retransmit_nack_retransmit_flag_set(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));

  /* Before NACK: retransmit flag should NOT be set */
  TEST_ASSERT_EQUAL_HEX8(0,
                         s_handle.retry_buffer[k_test_retransmit_flags_offset] &
                           k_frame_flag_retransmit);

  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_nack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* After retransmit: retransmit flag should be set in retry buffer */
  TEST_ASSERT_NOT_EQUAL(0,
                        s_handle.retry_buffer[k_test_retransmit_flags_offset] &
                          k_frame_flag_retransmit);
}

void test_retransmit_nack_callback_invoked(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint32_t ctx_value = 77;
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_set_retransmit_callbacks(&s_handle,
                                                         test_ack_callback,
                                                         test_nack_callback,
                                                         &ctx_value));

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));

  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_nack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  (void)rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  TEST_ASSERT_EQUAL_UINT32(1, s_nack_cb_count);
  TEST_ASSERT_EQUAL_UINT16(0, s_last_nack_sequence);
  TEST_ASSERT_EQUAL_PTR(&ctx_value, s_last_retransmit_cb_ctx);
  TEST_ASSERT_EQUAL_UINT32(0, s_ack_cb_count);
}

void test_retransmit_nack_not_consumed_when_off(void)
{
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t  encoded[64];
  uint32_t encoded_len = 0;
  helper_create_encoded_frame(k_frame_type_nack, 0, NULL, 0, encoded, &encoded_len);
  internal_assert_inject_rx(k_rspi_channel_0, encoded, encoded_len);

  rx_frame_t frame;
  rx_err_t   err = rx_spi_comm_receive(&s_handle, &frame, k_test_timeout_zero);

  /* NACK returned to caller as regular frame */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(k_frame_type_nack, frame.header.type);
}

/* =============================================================================
 * Retransmit Timeout (process_retransmits) Tests
 * =============================================================================
 */

void test_retransmit_process_triggers_after_timeout(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  s_handle.retry_send_time_ms = 0;

  /* Call with time > ack_timeout_ms (default 50) */
  rx_err_t err = rx_spi_comm_process_retransmits(&s_handle, 51);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(1, s_handle.retry_count);
  TEST_ASSERT_TRUE(s_handle.retry_pending);
}

void test_retransmit_process_exponential_backoff(void)
{
  rx_spi_comm_config_t config = {
    .session         = &s_session,
    .auto_retransmit = true,
    .retransmit_config =
      {
        .max_retries    = k_test_retransmit_high_max_retries,
        .ack_timeout_ms = k_retransmit_default_ack_timeout_ms,
        .max_backoff_ms = k_retransmit_default_max_backoff_ms,
      },
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  s_handle.retry_send_time_ms = 0;

  /* First timeout: 50ms (50 << 0) -- too early at 49 */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 49));
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);

  /* First timeout: triggers at 50ms */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 50));
  TEST_ASSERT_EQUAL_UINT8(1, s_handle.retry_count);

  /* Second timeout: 100ms (50 << 1), from retry_send_time=50 -- too early at 149 */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 149));
  TEST_ASSERT_EQUAL_UINT8(1, s_handle.retry_count);

  /* Second timeout: triggers at 150ms (50 + 100) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 150));
  TEST_ASSERT_EQUAL_UINT8(2, s_handle.retry_count);
}

void test_retransmit_process_max_backoff_cap(void)
{
  rx_spi_comm_config_t config = {
    .session         = &s_session,
    .auto_retransmit = true,
    .retransmit_config =
      {
        .max_retries    = k_test_retransmit_high_max_retries,
        .ack_timeout_ms = k_retransmit_default_ack_timeout_ms,
        .max_backoff_ms = k_test_retransmit_max_backoff_low_ms,
      },
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  s_handle.retry_send_time_ms = 0;

  /* First timeout: min(50 << 0, 100) = 50ms */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 50));
  TEST_ASSERT_EQUAL_UINT8(1, s_handle.retry_count);

  /* Second timeout: min(50 << 1, 100) = 100ms from send_time=50 */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 150));
  TEST_ASSERT_EQUAL_UINT8(2, s_handle.retry_count);

  /* Third timeout: min(50 << 2, 100) = min(200, 100) = 100ms (capped) */
  /* From send_time=150, need elapsed >= 100, so time >= 250 */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 249));
  TEST_ASSERT_EQUAL_UINT8(2, s_handle.retry_count); /* 249-150=99 < 100, too early */

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 250));
  TEST_ASSERT_EQUAL_UINT8(3, s_handle.retry_count); /* 250-150=100 >= 100, triggered */
}

void test_retransmit_process_no_action_before_timeout(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  s_handle.retry_send_time_ms = 0;

  rx_err_t err = rx_spi_comm_process_retransmits(&s_handle, 0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);
  TEST_ASSERT_TRUE(s_handle.retry_pending);
}

void test_retransmit_process_noop_when_nothing_pending(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));

  rx_err_t err = rx_spi_comm_process_retransmits(&s_handle, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Retransmit Retry Limit Tests
 * =============================================================================
 */

void test_retransmit_retry_limit_returns_error(void)
{
  rx_spi_comm_config_t config = {
    .session           = &s_session,
    .auto_retransmit   = true,
    .retransmit_config = {.max_retries = k_test_retransmit_single_retry},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  s_handle.retry_send_time_ms = 0;

  /* First retransmit succeeds (retry_count 0 -> 1) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 50));
  TEST_ASSERT_EQUAL_UINT8(1, s_handle.retry_count);

  /* Second attempt: retry_count (1) >= max_retries (1) -> limit exceeded */
  rx_err_t err = rx_spi_comm_process_retransmits(&s_handle, k_test_retransmit_limit_check_time_ms);

  TEST_ASSERT_EQUAL(k_rx_err_retry_limit, err);
}

void test_retransmit_retry_limit_clears_pending(void)
{
  rx_spi_comm_config_t config = {
    .session           = &s_session,
    .auto_retransmit   = true,
    .retransmit_config = {.max_retries = k_test_retransmit_single_retry},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  s_handle.retry_send_time_ms = 0;

  /* First retransmit (retry_count 0 -> 1) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_process_retransmits(&s_handle, 50));

  /* Limit exceeded */
  (void)rx_spi_comm_process_retransmits(&s_handle, 200);

  TEST_ASSERT_FALSE(s_handle.retry_pending);
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);
}

/* =============================================================================
 * Retransmit Set Auto Retransmit Runtime Tests
 * =============================================================================
 */

void test_retransmit_set_auto_retransmit_null_handle(void)
{
  rx_err_t err = rx_spi_comm_set_auto_retransmit(nullptr, true, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_retransmit_set_auto_retransmit_enables(void)
{
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));
  TEST_ASSERT_FALSE(s_handle.auto_retransmit);

  rx_spi_comm_retransmit_config_t cfg = {
    .max_retries    = k_test_retransmit_max_retries_custom,
    .ack_timeout_ms = k_test_retransmit_ack_timeout_custom_ms,
    .max_backoff_ms = k_test_retransmit_max_backoff_custom_ms,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_set_auto_retransmit(&s_handle, true, &cfg));

  TEST_ASSERT_TRUE(s_handle.auto_retransmit);
  TEST_ASSERT_EQUAL_UINT8(k_test_retransmit_max_retries_custom,
                          s_handle.retransmit_cfg.max_retries);
  TEST_ASSERT_EQUAL_UINT16(k_test_retransmit_ack_timeout_custom_ms,
                           s_handle.retransmit_cfg.ack_timeout_ms);
}

void test_retransmit_set_auto_retransmit_disables_clears_pending(void)
{
  rx_spi_comm_config_t config = {.session = &s_session, .auto_retransmit = true};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &config));
  helper_init_rspi_channel(k_rspi_channel_0);

  uint8_t data[] = "test";
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_spi_comm_send(&s_handle,
                                     k_frame_type_response,
                                     k_frame_flag_requires_ack,
                                     data,
                                     k_test_payload_small));
  TEST_ASSERT_TRUE(s_handle.retry_pending);

  /* Disable -- should clear pending */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_set_auto_retransmit(&s_handle, false, nullptr));

  TEST_ASSERT_FALSE(s_handle.auto_retransmit);
  TEST_ASSERT_FALSE(s_handle.retry_pending);
  TEST_ASSERT_EQUAL_UINT8(0, s_handle.retry_count);
}

/* =============================================================================
 * Retransmit Callback Registration Tests
 * =============================================================================
 */

void test_retransmit_set_callbacks_null_handle(void)
{
  rx_err_t err = rx_spi_comm_set_retransmit_callbacks(nullptr, NULL, NULL, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_retransmit_set_callbacks_success(void)
{
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));

  rx_err_t err =
    rx_spi_comm_set_retransmit_callbacks(&s_handle, test_ack_callback, test_nack_callback, NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(test_ack_callback, s_handle.on_ack_cb);
  TEST_ASSERT_EQUAL_PTR(test_nack_callback, s_handle.on_nack_cb);
}

/* =============================================================================
 * Retransmit Process API Validation Tests
 * =============================================================================
 */

void test_retransmit_process_null_handle(void)
{
  rx_err_t err = rx_spi_comm_process_retransmits(nullptr, 0);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_retransmit_process_ok_when_disabled(void)
{
  rx_spi_comm_config_t default_cfg = {.session = &s_session};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(&s_handle, &default_cfg));

  rx_err_t err = rx_spi_comm_process_retransmits(&s_handle, 1000);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_spi_comm_init_null_handle_fails);
  RUN_TEST(test_spi_comm_init_success_default_config);
  RUN_TEST(test_spi_comm_init_with_custom_channel);
  RUN_TEST(test_spi_comm_init_with_fec_enabled);
  RUN_TEST(test_spi_comm_deinit_null_handle_fails);
  RUN_TEST(test_spi_comm_deinit_success);
  RUN_TEST(test_spi_comm_deinit_not_initialized_succeeds);

  /* Send tests */
  RUN_TEST(test_spi_comm_send_null_handle_fails);
  RUN_TEST(test_spi_comm_send_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_null_payload_with_len_fails);
  RUN_TEST(test_spi_comm_send_payload_too_large_fails);
  RUN_TEST(test_spi_comm_send_empty_payload_succeeds);
  RUN_TEST(test_spi_comm_send_with_payload_succeeds);
  RUN_TEST(test_spi_comm_send_increments_sequence);
  RUN_TEST(test_spi_comm_send_sequence_wraps);
  RUN_TEST(test_spi_comm_send_with_fec_flag);
  RUN_TEST(test_spi_comm_send_transfer_error_propagates);
  RUN_TEST(test_spi_comm_send_large_payload);
  RUN_TEST(test_spi_comm_send_missing_host_ack_times_out);

  /* Send ACK/NACK tests */
  RUN_TEST(test_spi_comm_send_ack_null_handle_fails);
  RUN_TEST(test_spi_comm_send_ack_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_ack_success);
  RUN_TEST(test_spi_comm_send_ack_transfer_error_propagates);
  RUN_TEST(test_spi_comm_send_nack_null_handle_fails);
  RUN_TEST(test_spi_comm_send_nack_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_nack_success);
  RUN_TEST(test_spi_comm_send_nack_transfer_error_propagates);

  /* Receive tests */
  RUN_TEST(test_spi_comm_receive_null_handle_fails);
  RUN_TEST(test_spi_comm_receive_null_frame_fails);
  RUN_TEST(test_spi_comm_receive_not_initialized_fails);
  RUN_TEST(test_spi_comm_receive_no_data_timeout);
  RUN_TEST(test_spi_comm_receive_available_check_error_propagates);
  RUN_TEST(test_spi_comm_receive_valid_frame_success);
  RUN_TEST(test_spi_comm_receive_updates_rx_sequence);
  RUN_TEST(test_spi_comm_receive_invalid_sync_word);
  RUN_TEST(test_spi_comm_receive_transfer_error_propagates);

  /* Data available tests */
  RUN_TEST(test_spi_comm_data_available_null_handle_fails);
  RUN_TEST(test_spi_comm_data_available_null_available_fails);
  RUN_TEST(test_spi_comm_data_available_not_initialized_fails);
  RUN_TEST(test_spi_comm_data_available_empty);
  RUN_TEST(test_spi_comm_data_available_with_data);
  RUN_TEST(test_spi_comm_data_available_hal_error_propagates);

  /* Session integration tests */
  RUN_TEST(test_spi_comm_init_null_config_fails);
  RUN_TEST(test_spi_comm_init_null_session_fails);
  RUN_TEST(test_spi_comm_rx_sequence_wraparound);

  /* Channel configuration tests */
  RUN_TEST(test_spi_comm_uses_configured_channel);

  /* Buffer size tests */
  RUN_TEST(test_spi_comm_buffer_size_constants);
  RUN_TEST(test_spi_comm_max_payload_fits_in_buffer);

  /* Mock verification tests */
  RUN_TEST(test_spi_comm_transfer_is_called_on_send);
  RUN_TEST(test_spi_comm_available_is_called_on_receive);
  RUN_TEST(test_spi_comm_transfer_count);

  /* Set control callbacks tests */
  RUN_TEST(test_spi_comm_set_callbacks_null_handle_fails);
  RUN_TEST(test_spi_comm_set_callbacks_success);

  /* Send PONG tests */
  RUN_TEST(test_spi_comm_send_pong_null_handle_fails);
  RUN_TEST(test_spi_comm_send_pong_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_pong_echoes_payload);

  /* Send RESET_ACK tests */
  RUN_TEST(test_spi_comm_send_reset_ack_null_handle_fails);
  RUN_TEST(test_spi_comm_send_reset_ack_not_initialized_fails);
  RUN_TEST(test_spi_comm_send_reset_ack_success);

  /* Receive PING auto-PONG tests */
  RUN_TEST(test_spi_comm_receive_ping_auto_pong);
  RUN_TEST(test_spi_comm_receive_ping_then_command);

  /* Receive RESET auto-ACK tests */
  RUN_TEST(test_spi_comm_receive_reset_auto_ack);

  /* Control frame callback tests */
  RUN_TEST(test_spi_comm_receive_ping_callback_invoked);
  RUN_TEST(test_spi_comm_receive_reset_callback_invoked);

  /* Retransmit configuration tests */
  RUN_TEST(test_retransmit_disabled_by_default);
  RUN_TEST(test_retransmit_enabled_via_config);
  RUN_TEST(test_retransmit_defaults_applied_for_zero_values);

  /* Retransmit send buffering tests */
  RUN_TEST(test_retransmit_send_buffers_frame);
  RUN_TEST(test_retransmit_send_not_buffered_when_off);
  RUN_TEST(test_retransmit_send_only_requires_ack_buffered);
  RUN_TEST(test_retransmit_send_overwrites_on_new_send);

  /* Retransmit ACK consumption tests */
  RUN_TEST(test_retransmit_ack_clears_retry);
  RUN_TEST(test_retransmit_ack_wrong_seq_ignored);
  RUN_TEST(test_retransmit_ack_callback_invoked);
  RUN_TEST(test_retransmit_ack_not_consumed_when_off);
  RUN_TEST(test_retransmit_ack_non_pending_ignored);

  /* Retransmit NACK handling tests */
  RUN_TEST(test_retransmit_nack_triggers_retry);
  RUN_TEST(test_retransmit_nack_retransmit_flag_set);
  RUN_TEST(test_retransmit_nack_callback_invoked);
  RUN_TEST(test_retransmit_nack_not_consumed_when_off);

  /* Retransmit timeout (process_retransmits) tests */
  RUN_TEST(test_retransmit_process_triggers_after_timeout);
  RUN_TEST(test_retransmit_process_exponential_backoff);
  RUN_TEST(test_retransmit_process_max_backoff_cap);
  RUN_TEST(test_retransmit_process_no_action_before_timeout);
  RUN_TEST(test_retransmit_process_noop_when_nothing_pending);

  /* Retransmit retry limit tests */
  RUN_TEST(test_retransmit_retry_limit_returns_error);
  RUN_TEST(test_retransmit_retry_limit_clears_pending);

  /* Retransmit set auto retransmit runtime tests */
  RUN_TEST(test_retransmit_set_auto_retransmit_null_handle);
  RUN_TEST(test_retransmit_set_auto_retransmit_enables);
  RUN_TEST(test_retransmit_set_auto_retransmit_disables_clears_pending);

  /* Retransmit callback registration tests */
  RUN_TEST(test_retransmit_set_callbacks_null_handle);
  RUN_TEST(test_retransmit_set_callbacks_success);

  /* Retransmit process API validation tests */
  RUN_TEST(test_retransmit_process_null_handle);
  RUN_TEST(test_retransmit_process_ok_when_disabled);

  return UNITY_END();
}
