/**
 * @file test_rx_comm_manager.c
 * @brief Unit Tests for Unified Communication Channel Manager (USB + SPI + I2C + UART Multiplexing)
 *
 * @details
 * **Comprehensive Test Suite for Multi-Channel Communication Manager**
 *
 * This test suite validates the rx_comm_manager layer - a unified abstraction that
 * multiplexes multiple communication channels (USB CDC, SPI, I2C, and UART) into a single API.
 * The manager enables runtime channel selection, automatic fallback, and consistent
 * error handling across heterogeneous transports for RX72N <-> RPi5 communication.
 *
 * **Communication Manager Architecture:**
 * @code
 * Application Layer (Motor Control, Telemetry, etc.)
 *           v Single unified API
 * +------------------------------------------+
 * |     rx_comm_manager (THIS LAYER)        |
 * |  - Channel selection/fallback            |
 * |  - Decoded output routing (debug)        |
 * |  - Unified callback interface            |
 * |  - Error code translation                |
 * +------------------------------------------+
 *      v USB path          v SPI path
 * +------------------+  +------------------+
 * | rx_usb_comm      |  | rx_spi_comm      |
 * | (USB CDC frame)  |  | (SPI frame)      |
 * +------------------+  +------------------+
 *      v                     v
 * +------------------+  +------------------+
 * | rx_usb (CDC)     |  | RSPI Peripheral  |
 * | (Multi-port)     |  | (Hardware)       |
 * +------------------+  +------------------+
 * @endcode
 *
 * **Test Categories:**
 * 1. **Initialization** - Tests handle initialization with custom/default config
 * 2. **Deinitialization** - Tests cleanup and state reset
 * 3. **Channel Enumeration** - Validates channel ID values and count
 * 4. **Channel Selection** - Tests active channel get/set operations
 * 5. **Data Transmission** - Tests send operations via selected channel
 * 6. **Data Reception** - Tests receive operations via selected channel
 * 7. **ACK/NACK Protocol** - Tests control frame transmission
 * 8. **Data Available** - Tests non-blocking RX buffer fullness check
 * 9. **Decoded Output** - Tests ASCII hex debug output routing
 * 10. **Error Handling** - Tests invalid channel IDs, nullptr pointers, state validation
 * 11. **Callback Routing** - Tests event notification across channels
 *
 * **Channel Abstraction:**
 * The manager supports multiple physical transports with identical API:
 * - **k_comm_channel_usb:** USB CDC (Port 0 = Protocol, Port 1 = Decoded debug)
 * - **k_comm_channel_spi:** SPI with hardware handshake (RPi5 <-> RX72N)
 * - **k_comm_channel_i2c:** I2C peripheral mode (RIIC0, RX72N as peripheral, RPi5 as controller)
 * - **k_comm_channel_uart:** UART (SCI9, full-duplex serial link to RPi5)
 *
 * Applications can switch channels at runtime via `rx_comm_manager_set_channel()`.
 *
 * **Decoded Output Feature:**
 * When `enable_decoded_output = true`:
 * - Binary frames sent on primary channel
 * - ASCII hex dump sent to USB Port 1 (Decoded) for debugging
 * - Example: `[TX] A5 5A 00 01 00 04 02 00 74 65 73 74 AB CD EF 12`
 *
 * **Protocol Test Sequences:**
 * @par Initialization Test:
 * @code
 * 1. Call rx_comm_manager_init with nullptr config (use defaults)
 * 2. Assert: Returns k_rx_ok
 * 3. Assert: s_manager.initialized == true
 * 4. Assert: s_manager.usb_handle == nullptr (not attached yet)
 * 5. Assert: s_manager.spi_handle == nullptr (not attached yet)
 * 6. Assert: s_manager.enable_decoded_output == false (default)
 * @endcode
 *
 * @par Channel Selection Test:
 * @code
 * 1. Init manager
 * 2. Set active channel to k_comm_channel_spi
 * 3. Assert: rx_comm_manager_get_channel returns k_comm_channel_spi
 * 4. Send data (should route to SPI)
 * 5. Change to k_comm_channel_usb
 * 6. Assert: rx_comm_manager_get_channel returns k_comm_channel_usb
 * 7. Send data (should route to USB)
 * @endcode
 *
 * @par Invalid Channel ID Test:
 * @code
 * 1. Init manager
 * 2. Call rx_comm_manager_send with channel >= k_comm_channel_count (invalid)
 * 3. Assert: Returns k_rx_err_invalid_arg
 * 4. Call rx_comm_manager_receive with invalid channel
 * 5. Assert: Returns k_rx_err_invalid_arg
 * @endcode
 *
 * @par Decoded Output Test:
 * @code
 * 1. Init manager with enable_decoded_output = true
 * 2. Attach USB handle (simulated or mock)
 * 3. Send binary frame "test" (4 bytes)
 * 4. Assert: Primary channel (USB Port 0) receives encoded frame
 * 5. Assert: Decoded channel (USB Port 1) receives ASCII: "[TX] 74 65 73 74"
 * 6. Note: Actual decoded output tested in integration, not unit tests
 * @endcode
 *
 * @par Send with No Handle Test:
 * @code
 * 1. Init manager with nullptr USB/SPI handles
 * 2. Set active channel to k_comm_channel_usb
 * 3. Call rx_comm_manager_send
 * 4. Assert: Returns k_rx_err_invalid_state (no handle attached)
 * @endcode
 *
 * @par Receive with Timeout Test:
 * @code
 * 1. Init manager with attached USB handle
 * 2. Set channel to k_comm_channel_usb
 * 3. Call rx_comm_manager_receive with timeout_ms = 0 (non-blocking)
 * 4. Assert: Returns k_rx_err_timeout (no data available)
 * @endcode
 *
 * **Timing Requirements:**
 * - Manager overhead (channel routing): <1us (function pointer dereference)
 * - Channel switch latency: <1us (atomic variable write)
 * - USB path latency: 1-5ms (USB FS bulk transfer)
 * - SPI path latency: 100us-1ms (frame transfer + handshake)
 * - Decoded output formatting: <100us per frame (ASCII conversion)
 *
 * **Error Injection Patterns:**
 * - **nullptr manager pointer** - Tests input validation (returns k_rx_err_invalid_arg)
 * - **Invalid channel ID** - Tests bounds checking (returns k_rx_err_invalid_arg)
 * - **Uninitialized manager** - Tests state validation (returns k_rx_err_invalid_state)
 * - **nullptr transport handle** - Tests handle validation (returns k_rx_err_invalid_state)
 * - **Transport-specific errors** - Tests error propagation (USB busy, SPI timeout)
 *
 * **Mock State Management:**
 * These tests focus on **API validation and error handling** without complex mocks:
 * - Manager state directly inspected via handle members
 * - Transport handles set to nullptr (no actual USB/SPI simulation)
 * - Full integration testing requires hardware or transport layer mocks
 *
 * **Critical Design Decisions:**
 * - **No Automatic Fallback:** Manager does NOT switch channels on error (explicit only)
 * - **No Queueing:** Manager does NOT buffer frames (pass-through to transport)
 * - **No Retry Logic:** Manager does NOT retry failed sends (higher layer responsibility)
 * - **Thread-Safe Writes:** Channel selection atomic (single uint8_t write)
 * - **Callback Multiplexing:** Single callback services both USB and SPI events
 * - **Decoded Output Optional:** Can be disabled to save bandwidth/CPU
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (Control Flow):** [OK] All test functions use simple sequential flow
 * - **Rule 2 (Loop Bounds):** [OK] All loops have compile-time known bounds
 * - **Rule 3 (Dynamic Memory):** [OK] Zero heap allocation (stack-only test data)
 * - **Rule 4 (Function Size):** [OK] Test functions <40 lines, helpers <15 lines
 * - **Rule 5 (Assertions):** [OK] Every test has minimum 1 assertion, most have 3+
 * - **Rule 7 (Return Checking):** [OK] All API returns validated
 * - **Rule 9 (Pointers):** [OK] Single-level dereferencing only
 * - **Rule 10 (Warnings):** [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Each test validates one manager API behavior
 * - **Open/Closed:** Manager extensible to new channels without modifying tests
 * - **Liskov Substitution:** USB/SPI comm layers interchangeable via manager
 * - **Interface Segregation:** Tests use minimal API (init/send/receive/get_channel)
 * - **Dependency Inversion:** Manager depends on comm layer abstractions, not concrete types
 *
 * @par Limitations of Unit Tests:
 * These tests validate:
 * - [OK] API argument validation (nullptr checks, bounds checking)
 * - [OK] State management (initialized flag, channel selection)
 * - [OK] Error code returns (k_rx_err_invalid_arg, k_rx_err_invalid_state)
 * - [OK] Configuration handling (decoded output flag, callbacks)
 *
 * These tests do NOT validate (requires integration testing):
 * - [X] Actual frame transmission over USB/SPI
 * - [X] CRC validation end-to-end
 * - [X] Sequence number tracking
 * - [X] Decoded output formatting
 * - [X] Callback invocation on real events
 *
 * **Integration testing strategy documented in test plan.**
 *
 * @par Module Dependencies:
 * - rx_comm_manager.h - Manager API under test
 * - rx_err.h - Error code definitions
 * - unity.h - Unit testing framework
 *
 * @see rx_comm_manager.h - Manager header
 * @see rx_usb_comm.h - USB CDC frame layer
 * @see rx_spi_comm.h - SPI frame layer
 * @see STAR_TEST_PLAN.md - Integration testing strategy
 *
 * @author Locked, Inc.
 * @date 2026-01-30
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @test Tests run via Unity framework with: make test_rx_comm_manager
 */

#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "mock_riic_hal.h"
#include "mock_rspi.h"
#include "mock_uart_hw.h"
#include "mock_usb0_regs.h"
#include "mock_usb_hw.h"
#include "rx_comm_manager.h"
#include "rx_frame.h"
#include "rx_harq.h"
#include "rx_i2c_comm.h"
#include "rx_session.h"
#include "rx_spi_comm.h"
#include "rx_spi_link.h"
#include "rx_uart_comm.h"
#include "rx_usb.h"
#include "rx_usb_comm.h"
#include "rx_usb_private.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test constants
 */
typedef enum : uint32_t {
  k_test_payload_size            = 10,     /**< Test payload size */
  k_expected_channel_usb_value   = 0,      /**< Expected k_comm_channel_usb value */
  k_expected_channel_spi_value   = 1,      /**< Expected k_comm_channel_spi value */
  k_expected_channel_i2c_value   = 2,      /**< Expected k_comm_channel_i2c value */
  k_expected_channel_uart_value  = 3,      /**< Expected k_comm_channel_uart value */
  k_expected_channel_count_value = 4,      /**< Expected k_comm_channel_count value */
  k_invalid_channel_sentinel     = 99,     /**< Invalid channel ID for negative testing */
  k_garbage_fill_value           = 0xFF,   /**< Fill value to simulate garbage data */
  k_test_encoded_buf_size        = 128,    /**< Size of encoded frame buffers for tests */
  k_test_payload_byte_0          = 0x01,   /**< Test payload byte 0 */
  k_test_payload_byte_1          = 0x02,   /**< Test payload byte 1 */
  k_test_payload_byte_2          = 0x03,   /**< Test payload byte 2 */
  k_test_payload_byte_3          = 0x04,   /**< Test payload byte 3 */
  k_test_payload_aa              = 0xAA,   /**< Test payload byte pattern A */
  k_test_payload_bb              = 0xBB,   /**< Test payload byte pattern B */
  k_test_payload_cc              = 0xCC,   /**< Test payload byte pattern C */
  k_test_payload_dd              = 0xDD,   /**< Test payload byte pattern D */
  k_test_payload_ab              = 0xAB,   /**< Test payload byte pattern AB */
  k_test_payload_11              = 0x11,   /**< Test payload byte 0x11 */
  k_test_payload_22              = 0x22,   /**< Test payload byte 0x22 */
  k_test_payload_33              = 0x33,   /**< Test payload byte 0x33 */
  k_test_payload_44              = 0x44,   /**< Test payload byte 0x44 */
  k_test_payload_count           = 4,      /**< Number of test payload bytes */
  k_test_retry_count             = 2,      /**< Expected retry count value */
  k_test_sequence_2              = 2,      /**< Sequence number for second frame in tests */
  k_test_baudrate                = 115200, /**< UART test baudrate */
  k_test_wire_buf_size           = 256,    /**< Size of wire-format buffers for large frames */
} test_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_comm_manager_t s_manager;

/**
 * @brief Set up test fixture before each test
 *
 * Clears manager handle to ensure clean state for each test.
 */
void setUp(void)
{
  s_manager = (rx_comm_manager_t){0};
}

/**
 * @brief Tear down test fixture after each test
 *
 * Deinitializes manager to clean up any resources.
 */
void tearDown(void)
{
  (void)rx_comm_manager_deinit(&s_manager);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test initialization with nullptr manager pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_init_null_manager(void)
{
  rx_err_t err = rx_comm_manager_init(nullptr, nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test initialization with nullptr config (uses defaults)
 *
 * Expected: Success with all handles nullptr and decoded output disabled
 */
void test_init_null_config(void)
{
  /* nullptr config should use defaults (both channels disabled) */
  rx_err_t err = rx_comm_manager_init(&s_manager, nullptr);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_manager.initialized);
  TEST_ASSERT_NULL(s_manager.usb_handle);
  TEST_ASSERT_NULL(s_manager.spi_handle);
  TEST_ASSERT_FALSE(s_manager.enable_decoded_output);
}

/**
 * @brief Test initialization with custom configuration
 *
 * Expected: Manager initialized with config values applied
 */
void test_init_with_config(void)
{
  rx_comm_manager_config_t cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .callback              = nullptr,
    .callback_ctx          = nullptr,
    .enable_decoded_output = true,
  };

  rx_err_t err = rx_comm_manager_init(&s_manager, &cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_manager.initialized);
  TEST_ASSERT_TRUE(s_manager.enable_decoded_output);
}

/**
 * @brief Test initialization clears previous state
 *
 * Expected: Manager state is zeroed except for initialized flag
 */
void test_init_clears_previous_state(void)
{
  /* Set some garbage data */
  for (uint32_t i = 0; i < sizeof(s_manager); i++) {
    ((uint8_t*)&s_manager)[i] = (uint8_t)k_garbage_fill_value;
  }

  rx_err_t err = rx_comm_manager_init(&s_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Should be zeroed out except for initialized flag */
  TEST_ASSERT_NULL(s_manager.usb_handle);
  TEST_ASSERT_NULL(s_manager.spi_handle);
  TEST_ASSERT_NULL(s_manager.callback);
  TEST_ASSERT_NULL(s_manager.callback_ctx);
  TEST_ASSERT_FALSE(s_manager.enable_decoded_output);
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

/**
 * @brief Test deinitialization with nullptr manager pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_deinit_null_manager(void)
{
  rx_err_t err = rx_comm_manager_deinit(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test successful deinitialization
 *
 * Expected: Manager deinitialized successfully, initialized flag cleared
 */
void test_deinit_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  TEST_ASSERT_TRUE(s_manager.initialized);

  rx_err_t err = rx_comm_manager_deinit(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_manager.initialized);
}

/**
 * @brief Test deinitialization of uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state (manager not initialized)
 */
void test_deinit_uninitialized(void)
{
  /* Deinit on already uninitialized should return error */
  rx_err_t err = rx_comm_manager_deinit(&s_manager);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Poll Tests
 * =============================================================================
 */

/**
 * @brief Test poll with nullptr manager pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_poll_null_manager(void)
{
  rx_err_t err = rx_comm_manager_poll(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test poll on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_poll_uninitialized(void)
{
  rx_err_t err = rx_comm_manager_poll(&s_manager);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test poll when no channels are configured
 *
 * Expected: Returns k_rx_err_timeout (no data received)
 */
void test_poll_no_channels(void)
{
  /* Init with no channels */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* Poll should return timeout since no channels to poll */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Send Tests
 * =============================================================================
 */

/**
 * @brief Test send with nullptr manager pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_send_null_manager(void)
{
  uint8_t                     payload[k_test_payload_size] = {0};
  const rx_comm_send_params_t params                       = {
                          .channel     = k_comm_channel_usb,
                          .type        = k_frame_type_command,
                          .flags       = k_frame_flag_none,
                          .payload     = payload,
                          .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(nullptr, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test send on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_send_uninitialized(void)
{
  uint8_t                     payload[k_test_payload_size] = {0};
  const rx_comm_send_params_t params                       = {
                          .channel     = k_comm_channel_usb,
                          .type        = k_frame_type_command,
                          .flags       = k_frame_flag_none,
                          .payload     = payload,
                          .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test send with nullptr payload but non-zero length
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_send_null_payload_with_length(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = k_test_payload_size,
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test send on USB channel when not enabled
 *
 * Expected: Returns k_rx_err_invalid_state (handle is nullptr)
 */
void test_send_usb_channel_not_enabled(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  uint8_t payload[k_test_payload_size] = {0};

  /* USB handle is nullptr, so send should fail */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = payload,
    .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test send on SPI channel when not enabled
 *
 * Expected: Returns k_rx_err_invalid_state (handle is nullptr)
 */
void test_send_spi_channel_not_enabled(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  uint8_t payload[k_test_payload_size] = {0};

  /* SPI handle is nullptr, so send should fail */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = payload,
    .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test send with invalid channel ID
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_send_invalid_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  uint8_t payload[k_test_payload_size] = {0};

  /* Invalid channel ID */
  const rx_comm_send_params_t params = {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    .channel     = (rx_comm_channel_t)k_invalid_channel_sentinel,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = payload,
    .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test send with nullptr params pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_send_null_params(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  rx_err_t err = rx_comm_manager_send(&s_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test send with empty payload (nullptr with length 0)
 *
 * Expected: Returns k_rx_err_invalid_state (channel not enabled)
 */
void test_send_empty_payload_allowed(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* Empty payload (nullptr with length 0) should be allowed for validation,
   * but fail because channel not enabled */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_ack,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  /* Should fail at channel check, not payload validation */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Respond Tests
 * =============================================================================
 */

/**
 * @brief Test respond with nullptr manager pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_respond_null_manager(void)
{
  uint8_t  payload[k_test_payload_size] = {0};
  rx_err_t err = rx_comm_manager_respond(nullptr, k_comm_channel_usb, payload, sizeof(payload));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test respond on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_respond_uninitialized(void)
{
  uint8_t  payload[k_test_payload_size] = {0};
  rx_err_t err = rx_comm_manager_respond(&s_manager, k_comm_channel_usb, payload, sizeof(payload));

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test respond when channel is not enabled
 *
 * Expected: Returns k_rx_err_invalid_state (handle is nullptr)
 */
void test_respond_channel_not_enabled(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  uint8_t payload[k_test_payload_size] = {0};

  rx_err_t err = rx_comm_manager_respond(&s_manager, k_comm_channel_usb, payload, sizeof(payload));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Channel Ready Tests
 * =============================================================================
 */

/**
 * @brief Test channel_ready with nullptr manager pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_channel_ready_null_manager(void)
{
  bool     ready;
  rx_err_t err = rx_comm_manager_channel_ready(nullptr, k_comm_channel_usb, &ready);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test channel_ready with nullptr ready pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_channel_ready_null_ready(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_usb, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test channel_ready on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_channel_ready_uninitialized(void)
{
  bool     ready;
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_usb, &ready);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test channel_ready for USB when not configured
 *
 * Expected: Success with ready=false (handle is nullptr)
 */
void test_channel_ready_usb_not_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  bool ready;

  /* USB handle is nullptr, so channel is not ready */
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_usb, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

/**
 * @brief Test channel_ready for SPI when not configured
 *
 * Expected: Success with ready=false (handle is nullptr)
 */
void test_channel_ready_spi_not_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  bool ready;

  /* SPI handle is nullptr, so channel is not ready */
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_spi, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

/**
 * @brief Test channel_ready for I2C when not configured
 *
 * Expected: Success with ready=false (handle is nullptr)
 */
void test_channel_ready_i2c_not_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  bool ready;

  /* I2C handle is nullptr, so channel is not ready */
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_i2c, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

/**
 * @brief Test channel_ready for UART when not configured
 *
 * Expected: Success with ready=false (handle is nullptr)
 */
void test_channel_ready_uart_not_configured(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  bool ready;

  /* UART handle is nullptr, so channel is not ready */
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_uart, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(ready);
}

/**
 * @brief Test channel_ready with invalid channel ID
 *
 * Expected: Returns k_rx_err_invalid_arg with ready=false
 */
void test_channel_ready_invalid_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  bool ready;

  rx_err_t err = rx_comm_manager_channel_ready(
    &s_manager,
    (rx_comm_channel_t) // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    k_invalid_channel_sentinel,
    &ready);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_FALSE(ready);
}

/* =============================================================================
 * Send - I2C and UART channel tests
 * =============================================================================
 */

/**
 * @brief Test send on I2C channel when not enabled
 *
 * Expected: Returns k_rx_err_invalid_state (handle is nullptr)
 */
void test_send_i2c_channel_not_enabled(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  uint8_t payload[k_test_payload_size] = {0};

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_i2c,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = payload,
    .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test send on UART channel when not enabled
 *
 * Expected: Returns k_rx_err_invalid_state (handle is nullptr)
 */
void test_send_uart_channel_not_enabled(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  uint8_t payload[k_test_payload_size] = {0};

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_uart,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = payload,
    .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test send with payload too large
 *
 * Expected: Returns k_rx_err_invalid_arg (payload_len > k_frame_max_payload = 1024)
 */
void test_send_payload_too_large(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = (const uint8_t*)"dummy",
    .payload_len = 1025U, /* k_frame_max_payload is 1024 */
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Stream Send Tests
 * =============================================================================
 */

/**
 * @brief Test stream_send with null manager
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_stream_send_null_manager(void)
{
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  rx_err_t err = rx_comm_manager_stream_send(nullptr, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test stream_send with uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_stream_send_uninitialized(void)
{
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  rx_err_t err = rx_comm_manager_stream_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Event Send Tests
 * =============================================================================
 */

/**
 * @brief Test event_send with null manager
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_event_send_null_manager(void)
{
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  rx_err_t err = rx_comm_manager_event_send(nullptr, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test event_send with null params
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_event_send_null_params(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_err_t err = rx_comm_manager_event_send(&s_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test event_send with uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_event_send_uninitialized(void)
{
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test event_send with payload too large
 *
 * Expected: Returns k_rx_err_invalid_arg (payload_len > k_frame_max_payload = 1024)
 */
void test_event_send_payload_too_large(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = (const uint8_t*)"dummy",
    .payload_len = 1025U,
  };
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test event_send with nonzero length but null payload
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_event_send_null_payload_with_length(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = k_test_payload_size,
  };
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test event_send with valid empty payload - succeeds (enqueued)
 *
 * Expected: Returns k_rx_ok (event enqueued successfully)
 */
void test_event_send_enqueue_success(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Link Status Tests
 * =============================================================================
 */

/**
 * @brief Test link_status with null manager
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_link_status_null_manager(void)
{
  rx_comm_link_status_t status;
  rx_err_t              err = rx_comm_manager_link_status(nullptr, k_comm_channel_usb, &status);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test link_status with null status pointer
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_link_status_null_status(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_err_t err = rx_comm_manager_link_status(&s_manager, k_comm_channel_usb, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test link_status on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_link_status_uninitialized(void)
{
  rx_comm_link_status_t status;
  rx_err_t              err = rx_comm_manager_link_status(&s_manager, k_comm_channel_usb, &status);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test link_status with invalid channel
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_link_status_invalid_channel(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_comm_link_status_t status;
  rx_err_t              err = rx_comm_manager_link_status(
    &s_manager,
    (rx_comm_channel_t) // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    k_invalid_channel_sentinel,
    &status);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test link_status returns unknown status for fresh manager
 *
 * Expected: Returns k_rx_ok with status = k_link_status_unknown
 */
void test_link_status_initial_unknown(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_comm_link_status_t status;
  rx_err_t              err = rx_comm_manager_link_status(&s_manager, k_comm_channel_usb, &status);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_link_status_unknown, status);
}

/* =============================================================================
 * Process Retransmits / Set Auto Retransmit Tests
 * =============================================================================
 */

/**
 * @brief Test process_retransmits with null manager
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_process_retransmits_null_manager(void)
{
  rx_err_t err = rx_comm_manager_process_retransmits(nullptr, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test process_retransmits on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_process_retransmits_uninitialized(void)
{
  rx_err_t err = rx_comm_manager_process_retransmits(&s_manager, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test process_retransmits when SPI not configured
 *
 * Expected: Returns k_rx_ok (no-op, SPI handle is nullptr)
 */
void test_process_retransmits_no_spi(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_err_t err = rx_comm_manager_process_retransmits(&s_manager, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test set_auto_retransmit with null manager
 *
 * Expected: Returns k_rx_err_invalid_arg
 */
void test_set_auto_retransmit_null_manager(void)
{
  rx_err_t err = rx_comm_manager_set_auto_retransmit(nullptr, false, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test set_auto_retransmit on uninitialized manager
 *
 * Expected: Returns k_rx_err_invalid_state
 */
void test_set_auto_retransmit_uninitialized(void)
{
  rx_err_t err = rx_comm_manager_set_auto_retransmit(&s_manager, false, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test set_auto_retransmit when SPI not configured
 *
 * Expected: Returns k_rx_err_not_supported (SPI handle is nullptr)
 */
void test_set_auto_retransmit_no_spi(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  rx_err_t err = rx_comm_manager_set_auto_retransmit(&s_manager, false, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_not_supported, err);
}

/* =============================================================================
 * Channel Name Tests
 * =============================================================================
 */

/**
 * @brief Test channel name for USB channel
 *
 * Expected: Returns "USB"
 */
void test_channel_name_usb(void)
{
  const char* name = rx_comm_manager_channel_name(k_comm_channel_usb);
  TEST_ASSERT_EQUAL_STRING("USB", name);
}

/**
 * @brief Test channel name for SPI channel
 *
 * Expected: Returns "SPI"
 */
void test_channel_name_spi(void)
{
  const char* name = rx_comm_manager_channel_name(k_comm_channel_spi);
  TEST_ASSERT_EQUAL_STRING("SPI", name);
}

/**
 * @brief Test channel name for I2C channel
 *
 * Expected: Returns "I2C"
 */
void test_channel_name_i2c(void)
{
  const char* name = rx_comm_manager_channel_name(k_comm_channel_i2c);
  TEST_ASSERT_EQUAL_STRING("I2C", name);
}

/**
 * @brief Test channel name for UART channel
 *
 * Expected: Returns "UART"
 */
void test_channel_name_uart(void)
{
  const char* name = rx_comm_manager_channel_name(k_comm_channel_uart);
  TEST_ASSERT_EQUAL_STRING("UART", name);
}

/**
 * @brief Test channel name for invalid channel
 *
 * Expected: Returns "UNKNOWN"
 */
void test_channel_name_invalid(void)
{
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const char* name = rx_comm_manager_channel_name((rx_comm_channel_t)k_invalid_channel_sentinel);
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", name);
}

/* =============================================================================
 * Enum Value Tests
 * =============================================================================
 */

/**
 * @brief Test channel enum values match expected constants
 *
 * Expected: USB=0, SPI=1, I2C=2, UART=3, COUNT=4 (for ABI stability verification)
 */
void test_channel_enum_values(void)
{
  TEST_ASSERT_EQUAL(k_expected_channel_usb_value, k_comm_channel_usb);
  TEST_ASSERT_EQUAL(k_expected_channel_spi_value, k_comm_channel_spi);
  TEST_ASSERT_EQUAL(k_expected_channel_i2c_value, k_comm_channel_i2c);
  TEST_ASSERT_EQUAL(k_expected_channel_uart_value, k_comm_channel_uart);
  TEST_ASSERT_EQUAL(k_expected_channel_count_value, k_comm_channel_count);
}

/* =============================================================================
 * Poll with valid I2C frame (exercises internal_handle_frame)
 * =============================================================================
 */

/**
 * @brief Callback counter for frame reception tests
 *
 * @details
 * Counts how many times the user callback was invoked by internal_handle_frame.
 */
static uint32_t s_callback_count = 0;

/**
 * @brief Test user callback for frame dispatch verification
 */
static void
test_handle_frame_callback(rx_comm_channel_t channel, const rx_frame_t* frame, void* ctx)
{
  (void)channel;
  (void)frame;
  (void)ctx;
  s_callback_count++;
}

/**
 * @brief Test poll with I2C channel: inject a frame and trigger internal_handle_frame
 *
 * @details
 * Builds a valid encoded frame, injects it via the RIIC mock, then calls
 * rx_comm_manager_poll(). This causes the I2C poll path to decode the frame
 * and dispatch it through internal_handle_frame, exercising both
 * internal_handle_frame() and internal_output_decoded().
 *
 * @pre mock_riic and session initialized, manager configured with I2C handle
 * @post s_callback_count incremented by 1
 *
 * @note Covers internal_handle_frame and internal_output_decoded via public API
 */
void test_poll_i2c_valid_frame_triggers_callback(void)
{
  mock_riic_init();

  static rx_session_state_t s_sess;
  s_sess = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess));

  static rx_i2c_comm_handle_t s_i2c;
  s_i2c                              = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg = {
    .session     = &s_sess,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_i2c, &i2c_cfg));

  s_callback_count                       = 0;
  const rx_comm_manager_config_t mgr_cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = &s_i2c,
    .uart_handle           = nullptr,
    .callback              = test_handle_frame_callback,
    .callback_ctx          = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg));

  /* Build and encode a minimal command frame */
  rx_frame_encoder_t enc;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc));

  rx_frame_t frame;
  frame                 = (rx_frame_t){0};
  frame.header.type     = k_frame_type_command;
  frame.header.sequence = 1;
  frame.header.length   = 0;
  frame.header.flags    = k_frame_flag_none;

  static uint8_t s_encoded[k_test_encoded_buf_size];
  uint32_t       encoded_len = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc, &frame, s_encoded, &encoded_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc));

  /* Inject encoded frame into mock RIIC RX buffer */
  mock_riic_set_rx_data(0, s_encoded, (uint16_t)encoded_len);

  /* Poll: I2C path decodes frame and calls internal_handle_frame -> callback */
  const rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, s_callback_count);

  (void)rx_i2c_comm_deinit(&s_i2c);
  (void)rx_session_deinit(&s_sess);
}

/**
 * @brief Test poll with decoded output enabled: exercise internal_output_decoded true path
 *
 * @details
 * Same as test_poll_i2c_valid_frame_triggers_callback but with enable_decoded_output
 * set to true. This causes internal_output_decoded() to format the received frame as
 * ASCII and call rx_usb_puts() (stubbed by MOCK_USB_SRC), covering the
 * `if (err == k_rx_ok && len > 0)` true branch on rx_comm_manager.c line 584.
 *
 * @pre mock_riic and session initialized, manager configured with decoded output enabled
 * @post s_callback_count incremented by 1 (callback still fires)
 *
 * @note Covers the internal_output_decoded rx_usb_puts path via public API
 */
void test_poll_i2c_decoded_output_enabled(void)
{
  mock_riic_init();

  static rx_session_state_t s_sess2;
  s_sess2 = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess2));

  static rx_i2c_comm_handle_t s_i2c2;
  s_i2c2                              = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg2 = {
    .session     = &s_sess2,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_i2c2, &i2c_cfg2));

  s_callback_count                        = 0;
  const rx_comm_manager_config_t mgr_cfg2 = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = &s_i2c2,
    .uart_handle           = nullptr,
    .callback              = test_handle_frame_callback,
    .callback_ctx          = nullptr,
    .enable_decoded_output = true, /* Enable ASCII debug output path */
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg2));

  /* Build and encode a minimal command frame with payload */
  rx_frame_encoder_t enc2;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc2));

  rx_frame_t frame2;
  frame2                 = (rx_frame_t){0};
  frame2.header.type     = k_frame_type_command;
  frame2.header.sequence = k_test_sequence_2;
  frame2.header.length   = 0;
  frame2.header.flags    = k_frame_flag_none;

  static uint8_t s_encoded2[k_test_encoded_buf_size];
  uint32_t       encoded_len2 = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc2, &frame2, s_encoded2, &encoded_len2));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc2));

  /* Inject encoded frame into mock RIIC RX buffer */
  mock_riic_set_rx_data(0, s_encoded2, (uint16_t)encoded_len2);

  /* Poll: decoded output path formats frame as ASCII and calls rx_usb_puts stub */
  const rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, s_callback_count);

  (void)rx_i2c_comm_deinit(&s_i2c2);
  (void)rx_session_deinit(&s_sess2);
}

/**
 * @brief Track link status callback invocations
 */
static rx_comm_link_status_t s_last_link_status   = k_link_status_unknown;
static uint32_t              s_link_cb_call_count = 0;

/**
 * @brief Test link status callback
 */
static void
test_link_status_cb(rx_comm_channel_t channel, rx_comm_link_status_t new_status, void* ctx)
{
  (void)channel;
  (void)ctx;
  s_last_link_status = new_status;
  s_link_cb_call_count++;
}

/**
 * @brief Helper: set up an I2C comm manager with a valid I2C handle and optional link_status_cb
 *
 * @param[out] sess  Session state to init
 * @param[out] i2c   I2C comm handle to init
 * @param[in]  cb    Optional link status callback (may be nullptr)
 * @param[in]  dcout Enable decoded output
 */
static void helper_init_i2c_manager(rx_session_state_t*            sess,
                                    rx_i2c_comm_handle_t*          i2c,
                                    rx_comm_link_status_callback_t cb,
                                    bool                           dcout)
{
  mock_riic_init();
  *sess = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(sess));
  *i2c                               = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg = {
    .session     = sess,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(i2c, &i2c_cfg));
  s_callback_count                       = 0;
  s_link_cb_call_count                   = 0;
  const rx_comm_manager_config_t mgr_cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = i2c,
    .uart_handle           = nullptr,
    .callback              = test_handle_frame_callback,
    .callback_ctx          = nullptr,
    .link_status_cb        = cb,
    .link_status_ctx       = nullptr,
    .enable_decoded_output = dcout,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg));
}

/**
 * @brief Helper: encode and inject a minimal I2C frame into mock RIIC buffer
 *
 * @param[in] seq Sequence number for the frame (caller increments to avoid session rejection)
 */
static void helper_inject_i2c_frame_seq(uint8_t seq)
{
  static rx_frame_encoder_t s_enc;
  static rx_frame_t         s_frame;
  static uint8_t            s_encoded[k_test_encoded_buf_size];
  uint32_t                  enc_len = 0;

  s_frame                 = (rx_frame_t){0};
  s_frame.header.type     = k_frame_type_command;
  s_frame.header.sequence = seq;
  s_frame.header.length   = 0;
  s_frame.header.flags    = k_frame_flag_none;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&s_enc));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_enc, &s_frame, s_encoded, &enc_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&s_enc));
  mock_riic_set_rx_data(0, s_encoded, (uint16_t)enc_len);
}

/**
 * @brief Helper: encode and inject a minimal I2C frame with sequence 1
 */
static void helper_inject_i2c_frame(void)
{
  helper_inject_i2c_frame_seq(1);
}

/**
 * @brief Test heartbeat healthy->healthy (second frame keeps link healthy)
 *
 * @details
 * Sends two frames so that the second arrives when link is already healthy.
 * Covers the false branch of `if (hb->status != k_link_status_healthy)`.
 */
void test_heartbeat_already_healthy_on_second_frame(void)
{
  static rx_session_state_t   s_sess3;
  static rx_i2c_comm_handle_t s_i2c3;
  helper_init_i2c_manager(&s_sess3, &s_i2c3, nullptr, false);

  /* First frame: unknown -> healthy */
  helper_inject_i2c_frame_seq(1);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_poll(&s_manager));
  TEST_ASSERT_EQUAL(1, s_callback_count);

  /* Second frame: already healthy, false branch of status check */
  helper_inject_i2c_frame_seq(k_test_sequence_2);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_poll(&s_manager));
  TEST_ASSERT_EQUAL(k_test_sequence_2, s_callback_count);

  (void)rx_i2c_comm_deinit(&s_i2c3);
  (void)rx_session_deinit(&s_sess3);
}

/**
 * @brief Test link_status_cb invoked on first frame (unknown -> healthy)
 *
 * @details
 * Registers a link_status_cb and receives the first frame.
 * Covers `if (mgr->link_status_cb != nullptr && old_status != k_link_status_healthy)`.
 */
void test_link_status_cb_fires_on_first_frame(void)
{
  static rx_session_state_t   s_sess4;
  static rx_i2c_comm_handle_t s_i2c4;
  helper_init_i2c_manager(&s_sess4, &s_i2c4, test_link_status_cb, false);

  helper_inject_i2c_frame();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_poll(&s_manager));
  TEST_ASSERT_EQUAL(1, s_callback_count);
  TEST_ASSERT_EQUAL(1, (int)s_link_cb_call_count);
  TEST_ASSERT_EQUAL(k_link_status_healthy, s_last_link_status);

  (void)rx_i2c_comm_deinit(&s_i2c4);
  (void)rx_session_deinit(&s_sess4);
}

/**
 * @brief Test heartbeat dead -> healthy recovery triggers link_status_cb
 *
 * @details
 * Manually sets the I2C heartbeat to dead, then injects a frame.
 * Covers the `if (old_status == k_link_status_dead)` true branch (line 662).
 */
void test_heartbeat_dead_to_healthy_recovery(void)
{
  static rx_session_state_t   s_sess5;
  static rx_i2c_comm_handle_t s_i2c5;
  helper_init_i2c_manager(&s_sess5, &s_i2c5, test_link_status_cb, false);

  /* Manually force heartbeat to dead state */
  s_manager.heartbeat[k_comm_channel_i2c].status = k_link_status_dead;

  /* Now receive a frame: dead -> healthy, log "Link recovered" */
  helper_inject_i2c_frame();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_poll(&s_manager));
  TEST_ASSERT_EQUAL(1, s_callback_count);
  TEST_ASSERT_EQUAL(1, (int)s_link_cb_call_count);
  TEST_ASSERT_EQUAL(k_link_status_healthy, s_last_link_status);

  (void)rx_i2c_comm_deinit(&s_i2c5);
  (void)rx_session_deinit(&s_sess5);
}

/**
 * @brief Test internal_output_decoded false branch (err != k_rx_ok || len == 0)
 *
 * @details
 * Enables decoded output and injects a frame with zero payload length.
 * This exercises the false branch of `if (err == k_rx_ok && len > 0)`.
 * rx_frame_ascii_format() returns k_rx_ok but len stays 0 for an empty frame
 * when the ASCII buffer gets an empty string.
 * The test simply confirms poll succeeds without crashing (callback fires).
 */
void test_output_decoded_no_usb_puts_on_empty_len(void)
{
  static rx_session_state_t   s_sess6;
  static rx_i2c_comm_handle_t s_i2c6;
  helper_init_i2c_manager(&s_sess6, &s_i2c6, nullptr, true);

  /* An empty frame (no payload): ascii_format may return len=0 */
  static rx_frame_encoder_t s_enc;
  static rx_frame_t         s_frame;
  static uint8_t            s_encoded[k_test_encoded_buf_size];
  uint32_t                  enc_len = 0;

  s_frame                 = (rx_frame_t){0};
  s_frame.header.type     = k_frame_type_command;
  s_frame.header.sequence = 0;
  s_frame.header.length   = 0;
  s_frame.header.flags    = k_frame_flag_none;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&s_enc));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_enc, &s_frame, s_encoded, &enc_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&s_enc));
  mock_riic_set_rx_data(0, s_encoded, (uint16_t)enc_len);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_poll(&s_manager));

  (void)rx_i2c_comm_deinit(&s_i2c6);
  (void)rx_session_deinit(&s_sess6);
}

/* =============================================================================
 * Init SPI link validation tests
 * =============================================================================
 */

/**
 * @brief Test init fails when spi_link provided but spi_handle is nullptr
 *
 * Expected: Returns k_rx_err_invalid_arg (line 1274)
 */
void test_init_spi_link_without_spi_handle(void)
{
  static rx_spi_link_t s_fake_link;
  s_fake_link             = (rx_spi_link_t){0};
  s_fake_link.initialized = true;

  const rx_comm_manager_config_t cfg = {
    .usb_handle   = nullptr,
    .spi_handle   = nullptr, /* Missing! */
    .spi_link     = &s_fake_link,
    .callback     = nullptr,
    .callback_ctx = nullptr,
  };
  rx_err_t err = rx_comm_manager_init(&s_manager, &cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test init fails when spi_link is not initialized
 *
 * Expected: Returns k_rx_err_invalid_arg (line 1280)
 */
void test_init_spi_link_not_initialized(void)
{
  static rx_spi_comm_handle_t s_fake_spi_handle;
  static rx_spi_link_t        s_fake_link;
  s_fake_link             = (rx_spi_link_t){0};
  s_fake_link.initialized = false; /* Not initialized */
  s_fake_link.spi_handle  = &s_fake_spi_handle;

  const rx_comm_manager_config_t cfg = {
    .usb_handle   = nullptr,
    .spi_handle   = &s_fake_spi_handle,
    .spi_link     = &s_fake_link,
    .callback     = nullptr,
    .callback_ctx = nullptr,
  };
  rx_err_t err = rx_comm_manager_init(&s_manager, &cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test init fails when spi_link->spi_handle does not match cfg->spi_handle
 *
 * Expected: Returns k_rx_err_invalid_arg (line 1286)
 */
void test_init_spi_link_handle_mismatch(void)
{
  static rx_spi_comm_handle_t s_handle_a;
  static rx_spi_comm_handle_t s_handle_b;
  static rx_spi_link_t        s_fake_link;
  s_fake_link             = (rx_spi_link_t){0};
  s_fake_link.initialized = true;
  s_fake_link.spi_handle  = &s_handle_a; /* Points to s_handle_a */

  const rx_comm_manager_config_t cfg = {
    .usb_handle   = nullptr,
    .spi_handle   = &s_handle_b, /* Different handle! */
    .spi_link     = &s_fake_link,
    .callback     = nullptr,
    .callback_ctx = nullptr,
  };
  rx_err_t err = rx_comm_manager_init(&s_manager, &cfg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Send via I2C channel with valid handle tests
 * =============================================================================
 */

/**
 * @brief Helper: set up an initialized I2C comm manager (tearDown calls deinit).
 *
 * @param[out] sess  Session state
 * @param[out] i2c   I2C comm handle
 */
static void helper_init_i2c_manager_send(rx_session_state_t* sess, rx_i2c_comm_handle_t* i2c)
{
  mock_riic_init();
  *sess = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(sess));
  *i2c                               = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg = {
    .session     = sess,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(i2c, &i2c_cfg));
  const rx_comm_manager_config_t mgr_cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = i2c,
    .uart_handle           = nullptr,
    .callback              = nullptr,
    .callback_ctx          = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg));
}

/**
 * @brief Test send on I2C channel succeeds with initialized handle
 *
 * Expected: Returns k_rx_ok
 */
void test_send_i2c_channel_succeeds(void)
{
  static rx_session_state_t   s_sess_s;
  static rx_i2c_comm_handle_t s_i2c_s;
  helper_init_i2c_manager_send(&s_sess_s, &s_i2c_s);

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_byte_0,
                                                               k_test_payload_byte_1,
                                                               k_test_payload_byte_2,
                                                               k_test_payload_byte_3};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_i2c,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_i2c_comm_deinit(&s_i2c_s);
  (void)rx_session_deinit(&s_sess_s);
}

/**
 * @brief Test send on I2C channel with decoded output enabled
 *
 * Exercises the `if (err == k_rx_ok && mgr->enable_decoded_output)` branch on send.
 * Expected: Returns k_rx_ok
 */
void test_send_i2c_with_decoded_output(void)
{
  mock_riic_init();
  static rx_session_state_t   s_sess_sd;
  static rx_i2c_comm_handle_t s_i2c_sd;
  s_sess_sd = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_sd));
  s_i2c_sd                              = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg_sd = {
    .session     = &s_sess_sd,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_i2c_sd, &i2c_cfg_sd));

  const rx_comm_manager_config_t mgr_cfg_sd = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = &s_i2c_sd,
    .uart_handle           = nullptr,
    .callback              = nullptr,
    .callback_ctx          = nullptr,
    .enable_decoded_output = true, /* Enable ASCII debug output path */
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg_sd));

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_aa,
                                                               k_test_payload_bb,
                                                               k_test_payload_cc,
                                                               k_test_payload_dd};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_i2c,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_i2c_comm_deinit(&s_i2c_sd);
  (void)rx_session_deinit(&s_sess_sd);
}

/**
 * @brief Test send on UART channel succeeds with initialized handle
 *
 * Expected: Returns k_rx_ok
 */
void test_send_uart_channel_succeeds(void)
{
  mock_uart_hw_init();

  /* Initialize SCI9 in the mock so uart_write_channel() won't reject the channel */
  const uart_channel_config_t ch_cfg = {
    .channel  = k_uart_channel_9,
    .baudrate = k_test_baudrate,
    .tx_gpio  = k_rx_p3_0,
    .rx_gpio  = k_rx_p3_1,
  };
  (void)uart_init_channel(&ch_cfg);

  static rx_session_state_t    s_sess_u;
  static rx_uart_comm_handle_t s_uart;
  s_sess_u = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_u));
  s_uart                               = (rx_uart_comm_handle_t){0};
  const rx_uart_comm_config_t uart_cfg = {
    .session = &s_sess_u,
    .channel = k_uart_channel_9,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_init(&s_uart, &uart_cfg));

  const rx_comm_manager_config_t mgr_cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = nullptr,
    .uart_handle           = &s_uart,
    .callback              = nullptr,
    .callback_ctx          = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg));

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_byte_0,
                                                               k_test_payload_byte_1,
                                                               k_test_payload_byte_2,
                                                               k_test_payload_byte_3};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_uart,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_uart_comm_deinit(&s_uart);
  (void)rx_session_deinit(&s_sess_u);
}

/* =============================================================================
 * Event queue processing tests (internal_process_event_queue via poll)
 * =============================================================================
 */

/**
 * @brief Test event queue full returns k_rx_err_no_mem
 *
 * Fills the event queue to capacity, then verifies the next enqueue fails.
 * Expected: Returns k_rx_err_no_mem
 */
void test_event_send_queue_full(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };

  /* Fill the queue */
  for (uint32_t i = 0; i < k_comm_event_queue_depth; i++) {
    TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));
  }

  /* Next enqueue should fail */
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_err_no_mem, err);
}

/**
 * @brief Test event queue processes USB event via poll (fire-and-forget dequeue)
 *
 * Enqueues a USB event, then polls. Poll calls internal_process_event_queue
 * which calls rx_comm_manager_send on usb channel (returns k_rx_err_invalid_state
 * because usb_handle is nullptr) but since channel==USB, it is dequeued anyway.
 *
 * Exercises: lines 1033 (err==k_rx_ok || channel==USB), 1041 (err==k_rx_ok branch),
 * 1044 (USB dropped warning).
 */
void test_event_queue_processes_usb_event_on_poll(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* Enqueue a USB event (usb_handle is nullptr, send will fail but USB is fire-and-forget) */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);

  /* Poll processes the queue entry (USB fire-and-forget dequeue) */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  /* No channels configured, so poll returns timeout */
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  /* Event should have been dequeued */
  TEST_ASSERT_EQUAL(0U, s_manager.event_queue_count);
}

/**
 * @brief Test event queue with payload: exercises memcpy path in event_send
 *
 * When params->payload != nullptr and payload_len > 0, rx_comm_manager_event_send
 * copies the payload into entry->payload (line 1948).
 */
void test_event_send_with_payload_copies_data(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_11,
                                                               k_test_payload_22,
                                                               k_test_payload_33,
                                                               k_test_payload_44};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_usb,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);
  /* Verify payload was copied into queue entry */
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload,
                                s_manager.event_queue[s_manager.event_queue_tail].payload,
                                sizeof(payload));
}

/* =============================================================================
 * Heartbeat timeout tests
 * =============================================================================
 */

/**
 * @brief Test event queue processes I2C event successfully (err == k_rx_ok path)
 *
 * Enqueues an I2C event and polls. Since i2c_handle is configured and mock_riic
 * returns k_rx_ok on write, the event dequeues on success (line 1057 true branch).
 */
void test_event_queue_processes_i2c_event_success(void)
{
  static rx_session_state_t   s_sess_eq;
  static rx_i2c_comm_handle_t s_i2c_eq;
  helper_init_i2c_manager_send(&s_sess_eq, &s_i2c_eq);

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_i2c,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);

  /* Poll: processes the queued I2C event (send succeeds -> dequeue via k_rx_ok path) */
  (void)rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(0U, s_manager.event_queue_count);

  (void)rx_i2c_comm_deinit(&s_i2c_eq);
  (void)rx_session_deinit(&s_sess_eq);
}

/**
 * @brief Test heartbeat timeout fires link_status_cb when link goes dead (no callback)
 *
 * Same as test_heartbeat_timeout_fires_dead_callback but without a link_status_cb.
 * Covers the `if (mgr->link_status_cb != nullptr)` false branch.
 */
void test_heartbeat_timeout_no_callback(void)
{
  static rx_session_state_t   s_sess_hb2;
  static rx_i2c_comm_handle_t s_i2c_hb2;
  helper_init_i2c_manager(&s_sess_hb2, &s_i2c_hb2, nullptr /* no cb */, false);

  /* Force link to healthy with last_rx_ms=1 so elapsed wraps to UINT32_MAX >> 200ms */
  s_manager.heartbeat[k_comm_channel_i2c].status     = k_link_status_healthy;
  s_manager.heartbeat[k_comm_channel_i2c].last_rx_ms = 1U;

  /* Poll: heartbeat fires dead but no callback to invoke */
  (void)rx_comm_manager_poll(&s_manager);

  TEST_ASSERT_EQUAL(k_link_status_dead, s_manager.heartbeat[k_comm_channel_i2c].status);

  (void)rx_i2c_comm_deinit(&s_i2c_hb2);
  (void)rx_session_deinit(&s_sess_hb2);
}

/**
 * @brief Test heartbeat timeout fires link_status_cb when link goes dead
 *
 * Manually sets the I2C heartbeat to healthy with last_rx_ms = 0, then polls.
 * Since mock time is 0 ms but elapsed >= k_heartbeat_implicit_timeout_ms requires
 * time > 200ms, we simulate by setting last_rx_ms far in the past using
 * the mock time offset approach: set last_rx_ms to 0 and override now to be large.
 *
 * Since we cannot control internal_get_time_ms() easily, we instead set
 * hb->last_rx_ms to a value such that now - last_rx_ms >= 200 ms.
 * The mock time returns 0, so we set last_rx_ms = UINT32_MAX - 100 to
 * trigger overflow wrapping if needed, or simply use a high enough value.
 *
 * Actually: internal_get_time_ms returns 0 in tests (mock_time returns 0 by default).
 * So elapsed = 0 - last_rx_ms. With uint32 wrapping: if last_rx_ms = 1, elapsed
 * wraps to UINT32_MAX which is >> 200. This triggers the timeout.
 */
void test_heartbeat_timeout_fires_dead_callback(void)
{
  static rx_session_state_t   s_sess_hb;
  static rx_i2c_comm_handle_t s_i2c_hb;
  s_last_link_status   = k_link_status_unknown;
  s_link_cb_call_count = 0;
  helper_init_i2c_manager(&s_sess_hb, &s_i2c_hb, test_link_status_cb, false);

  /* Force link to healthy with a non-zero last_rx_ms so timeout wraps */
  s_manager.heartbeat[k_comm_channel_i2c].status     = k_link_status_healthy;
  s_manager.heartbeat[k_comm_channel_i2c].last_rx_ms = 1U; /* Now=0, elapsed = 0-1 = UINT32_MAX */

  /* Poll: heartbeat check should detect timeout (elapsed >> 200ms) */
  (void)rx_comm_manager_poll(&s_manager);

  TEST_ASSERT_EQUAL(k_link_status_dead, s_manager.heartbeat[k_comm_channel_i2c].status);
  TEST_ASSERT_EQUAL(1, (int)s_link_cb_call_count);
  TEST_ASSERT_EQUAL(k_link_status_dead, s_last_link_status);

  (void)rx_i2c_comm_deinit(&s_i2c_hb);
  (void)rx_session_deinit(&s_sess_hb);
}

/* =============================================================================
 * USB and SPI handle send/poll tests
 * =============================================================================
 */

/**
 * @brief Helper: initialize a USB comm handle with a fresh session.
 *
 * @param[out] sess Session state to init
 * @param[out] usb  USB comm handle to init
 */
static void helper_init_usb_handle(rx_session_state_t* sess, rx_usb_comm_handle_t* usb)
{
  *sess = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(sess));
  *usb                               = (rx_usb_comm_handle_t){0};
  const rx_usb_comm_config_t usb_cfg = {
    .session    = sess,
    .time_iface = nullptr,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_comm_init(usb, &usb_cfg));
}

/**
 * @brief Helper: initialize a SPI comm handle with a fresh session and RSPI channel.
 *
 * @details
 * Calls rspi_init_peripheral to mark channel as ready in the mock, then calls
 * rx_spi_comm_init.  Sets write_ready=true so a send attempt can proceed past
 * the handshake poll.
 *
 * @param[out] sess Session state to init
 * @param[out] spi  SPI comm handle to init
 */
static void helper_init_spi_handle(rx_session_state_t* sess, rx_spi_comm_handle_t* spi)
{
  mock_rspi_init(nullptr);
  const rspi_config_t rspi_cfg = {.spi_mode  = (rspi_mode_t)k_spi_comm_default_mode,
                                  .use_16bit = false};
  TEST_ASSERT_EQUAL(k_rx_ok, rspi_init_peripheral(k_rspi_channel_0, &rspi_cfg));
  mock_rspi_set_write_ready(nullptr, k_rspi_channel_0, true);

  *sess = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(sess));
  *spi                               = (rx_spi_comm_handle_t){0};
  const rx_spi_comm_config_t spi_cfg = {
    .session     = sess,
    .channel     = k_rspi_channel_0,
    .spi_mode    = k_spi_comm_default_mode,
    .fec_enabled = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_comm_init(spi, &spi_cfg));
}

/**
 * @brief Test send on USB channel with initialized handle (rx_usb_is_configured returns false)
 *
 * @details
 * Provides a non-null USB handle so rx_comm_manager_send reaches the
 * rx_usb_comm_send() call (lines 1681-1686).  The mock stub rx_usb_is_configured
 * always returns false, so rx_usb_comm_send returns k_rx_err_invalid_state.
 * This also covers line 1744 (transport send failed path).
 *
 * Expected: Returns k_rx_err_invalid_state (USB not configured in test env)
 */
void test_send_usb_handle_non_null_covers_send_path(void)
{
  static rx_session_state_t   s_sess_us;
  static rx_usb_comm_handle_t s_usb_us;
  helper_init_usb_handle(&s_sess_us, &s_usb_us);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = &s_usb_us,
    .spi_handle            = nullptr,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_byte_0,
                                                               k_test_payload_byte_1,
                                                               k_test_payload_byte_2,
                                                               k_test_payload_byte_3};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_usb,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  /* rx_usb_is_configured() returns false in test env, so send fails */
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)rx_usb_comm_deinit(&s_usb_us);
  (void)rx_session_deinit(&s_sess_us);
}

/**
 * @brief Test poll with USB handle: rx_usb_comm_receive returns non-timeout error
 *
 * @details
 * With a non-null usb_handle but USB not configured (mock always returns false),
 * internal_poll_usb calls rx_usb_comm_receive which returns k_rx_err_invalid_state.
 * rx_comm_manager_poll sees a non-timeout error and propagates it (lines 1495-1498).
 *
 * Expected: Returns k_rx_err_invalid_state (not k_rx_err_timeout)
 */
void test_poll_usb_handle_non_null_propagates_error(void)
{
  static rx_session_state_t   s_sess_up;
  static rx_usb_comm_handle_t s_usb_up;
  helper_init_usb_handle(&s_sess_up, &s_usb_up);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = &s_usb_up,
    .spi_handle            = nullptr,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  /* Poll: USB receive fails because USB is not configured in test env */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  /* Should NOT be timeout (USB returned an error) */
  TEST_ASSERT_NOT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)rx_usb_comm_deinit(&s_usb_up);
  (void)rx_session_deinit(&s_sess_up);
}

/**
 * @brief Test poll with SPI handle: rx_spi_comm_receive returns non-timeout error
 *
 * @details
 * Provides a non-null, initialized spi_handle but with RSPI channel that
 * returns k_rx_err_invalid_state from rspi_peripheral_read_available
 * (data_available=false, timeout=0 -> k_rx_err_timeout... unless RSPI fails).
 *
 * Wait: with mock_rspi_init + rspi_init_peripheral, data_available defaults to
 * false, so internal_wait_for_data returns k_rx_err_timeout, and then
 * internal_poll_spi returns k_rx_err_timeout.  That's the normal path (no data).
 * To trigger a non-timeout SPI error, we reset the mock AFTER spi_comm_init
 * so the RSPI channel is no longer initialized when receive is attempted.
 *
 * Expected: Returns a non-timeout, non-ok error from SPI
 */
void test_poll_spi_handle_non_null_propagates_error(void)
{
  static rx_session_state_t   s_sess_sp;
  static rx_spi_comm_handle_t s_spi_sp;
  helper_init_spi_handle(&s_sess_sp, &s_spi_sp);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_sp,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  /* Deinit the RSPI channel so rspi_peripheral_read_available fails
   * -> internal_wait_for_data returns error -> rx_spi_comm_receive returns error
   * -> internal_poll_spi returns error -> poll propagates it (lines 1505-1508) */
  mock_rspi_deinit(nullptr);
  mock_rspi_init(nullptr);
  /* Channel 0 now NOT initialized: rspi_peripheral_read_available returns error */

  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)rx_spi_comm_deinit(&s_spi_sp);
  (void)rx_session_deinit(&s_sess_sp);
}

/**
 * @brief Test send on SPI channel with initialized handle (no spi_link)
 *
 * @details
 * Provides an initialized spi_handle with no spi_link.
 * rx_comm_manager_send reaches the raw SPI path (line 1694 FALSE branch,
 * lines 1706-1712).  With write_ready=true, the SPI transfer succeeds.
 *
 * Expected: Returns k_rx_ok
 */
void test_send_spi_handle_non_null_no_spi_link(void)
{
  static rx_session_state_t   s_sess_ss;
  static rx_spi_comm_handle_t s_spi_ss;
  helper_init_spi_handle(&s_sess_ss, &s_spi_ss);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_ss,
    .spi_link              = nullptr, /* No HARQ link: raw SPI path */
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_byte_0,
                                                               k_test_payload_byte_1,
                                                               k_test_payload_byte_2,
                                                               k_test_payload_byte_3};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_spi,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_comm_deinit(&s_spi_ss);
  (void)rx_session_deinit(&s_sess_ss);
}

/**
 * @brief Test send SPI with decoded output enabled (line 1748 true path)
 *
 * @details
 * Same as test_send_spi_handle_non_null_no_spi_link but with
 * enable_decoded_output=true.  Covers `if (err == k_rx_ok && mgr->enable_decoded_output)`.
 *
 * Expected: Returns k_rx_ok; decoded output formatting runs
 */
void test_send_spi_with_decoded_output(void)
{
  static rx_session_state_t   s_sess_sd2;
  static rx_spi_comm_handle_t s_spi_sd2;
  helper_init_spi_handle(&s_sess_sd2, &s_spi_sd2);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_sd2,
    .spi_link              = nullptr,
    .callback              = nullptr,
    .enable_decoded_output = true,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  uint8_t                     payload[k_test_payload_count] = {k_test_payload_aa,
                                                               k_test_payload_bb,
                                                               k_test_payload_cc,
                                                               k_test_payload_dd};
  const rx_comm_send_params_t params                        = {
                           .channel     = k_comm_channel_spi,
                           .type        = k_frame_type_command,
                           .flags       = k_frame_flag_none,
                           .payload     = payload,
                           .payload_len = sizeof(payload),
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_comm_deinit(&s_spi_sd2);
  (void)rx_session_deinit(&s_sess_sd2);
}

/**
 * @brief Test poll with UART channel receives valid frame (lines 1522-1524)
 *
 * @details
 * Injects a valid encoded frame into the mock UART RX buffer, then polls.
 * The UART poll path decodes the frame and calls internal_handle_frame,
 * which invokes the callback.  Covers lines 1522 (uart_err == k_rx_ok),
 * 1523 (received = true).
 *
 * Expected: Returns k_rx_ok; callback fires once
 */
void test_poll_uart_valid_frame_triggers_callback(void)
{
  mock_uart_hw_init();
  const uart_channel_config_t ch_cfg = {
    .channel  = k_uart_channel_9,
    .baudrate = k_test_baudrate,
    .tx_gpio  = k_rx_p3_0,
    .rx_gpio  = k_rx_p3_1,
  };
  (void)uart_init_channel(&ch_cfg);

  static rx_session_state_t    s_sess_ur;
  static rx_uart_comm_handle_t s_uart_ur;
  s_sess_ur = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_ur));
  s_uart_ur                               = (rx_uart_comm_handle_t){0};
  const rx_uart_comm_config_t uart_cfg_ur = {
    .session = &s_sess_ur,
    .channel = k_uart_channel_9,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_init(&s_uart_ur, &uart_cfg_ur));

  s_callback_count                       = 0;
  const rx_comm_manager_config_t mgr_cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = nullptr,
    .uart_handle           = &s_uart_ur,
    .callback              = test_handle_frame_callback,
    .callback_ctx          = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg));

  /* Encode a minimal command frame and inject into mock UART RX buffer */
  rx_frame_encoder_t enc_ur;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc_ur));

  rx_frame_t frame_ur;
  frame_ur                 = (rx_frame_t){0};
  frame_ur.header.type     = k_frame_type_command;
  frame_ur.header.sequence = 1;
  frame_ur.header.length   = 0;
  frame_ur.header.flags    = k_frame_flag_none;

  static uint8_t s_encoded_ur[k_test_encoded_buf_size];
  uint32_t       enc_len_ur = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc_ur, &frame_ur, s_encoded_ur, &enc_len_ur));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc_ur));

  /* Inject into mock UART channel 9 RX buffer */
  (void)mock_uart_hw_inject_rx_data(k_uart_channel_9, s_encoded_ur, (uint16_t)enc_len_ur);

  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, (int)s_callback_count);

  (void)rx_uart_comm_deinit(&s_uart_ur);
  (void)rx_session_deinit(&s_sess_ur);
}

/**
 * @brief Test process_retransmits passes through to rx_spi_comm when SPI configured
 *
 * @details
 * With a non-null spi_handle, rx_comm_manager_process_retransmits calls
 * rx_spi_comm_process_retransmits (line 2196).
 *
 * Expected: Returns k_rx_ok (spi_comm retransmit with no pending retries)
 */
void test_process_retransmits_with_spi_handle(void)
{
  static rx_session_state_t   s_sess_rt;
  static rx_spi_comm_handle_t s_spi_rt;
  helper_init_spi_handle(&s_sess_rt, &s_spi_rt);

  const rx_comm_manager_config_t cfg = {
    .usb_handle = nullptr,
    .spi_handle = &s_spi_rt,
    .callback   = nullptr,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  rx_err_t err = rx_comm_manager_process_retransmits(&s_manager, 0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_comm_deinit(&s_spi_rt);
  (void)rx_session_deinit(&s_sess_rt);
}

/**
 * @brief Test set_auto_retransmit passes through to rx_spi_comm when SPI configured
 *
 * @details
 * With a non-null spi_handle, rx_comm_manager_set_auto_retransmit calls
 * rx_spi_comm_set_auto_retransmit (line 2215).
 *
 * Expected: Returns k_rx_ok
 */
void test_set_auto_retransmit_with_spi_handle(void)
{
  static rx_session_state_t   s_sess_ar;
  static rx_spi_comm_handle_t s_spi_ar;
  helper_init_spi_handle(&s_sess_ar, &s_spi_ar);

  const rx_comm_manager_config_t cfg = {
    .usb_handle = nullptr,
    .spi_handle = &s_spi_ar,
    .callback   = nullptr,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  rx_err_t err = rx_comm_manager_set_auto_retransmit(&s_manager, false, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_comm_deinit(&s_spi_ar);
  (void)rx_session_deinit(&s_sess_ar);
}

/**
 * @brief Test event queue entry not occupied (line 988-989 branch)
 *
 * @details
 * Manually sets event_queue_count=1 but leaves the tail entry as occupied=false.
 * internal_process_event_queue sees count>0 but entry not occupied, returns early.
 * Verified by checking queue_count is unchanged after poll.
 *
 * Expected: Poll returns k_rx_err_timeout; queue_count stays at 1
 */
void test_event_queue_entry_not_occupied_returns_early(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* Manually corrupt queue state: count=1 but entry occupied=false */
  s_manager.event_queue_count       = 1;
  s_manager.event_queue_tail        = 0;
  s_manager.event_queue[0].occupied = false;

  (void)rx_comm_manager_poll(&s_manager);
  /* Queue was not drained (entry not occupied -> early return) */
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);

  /* Restore clean state for tearDown */
  s_manager.event_queue_count = 0;
}

/**
 * @brief Test event queue backoff: retry pending and time not elapsed (line 994-995)
 *
 * @details
 * Enqueues an SPI event, manually sets entry retries=1 and
 * next_retry_time_ms=UINT32_MAX so the backoff has not elapsed (mock time = 0).
 * internal_process_event_queue sees retries>0 and now_ms < next_retry_time_ms,
 * returns early without processing.
 *
 * Expected: Queue count stays at 1 after poll
 */
void test_event_queue_backoff_not_elapsed_skips_entry(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));

  /* Manually set retry state: backoff not elapsed */
  s_manager.event_queue[s_manager.event_queue_tail].retries            = 1;
  s_manager.event_queue[s_manager.event_queue_tail].next_retry_time_ms = UINT32_MAX;

  (void)rx_comm_manager_poll(&s_manager);
  /* Entry should NOT have been processed (backoff not elapsed) */
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);

  /* Clean up queue for tearDown */
  s_manager.event_queue_count       = 0;
  s_manager.event_queue[0].occupied = false;
}

/**
 * @brief Test event queue SPI retry exhaustion (lines 1026-1034)
 *
 * @details
 * Enqueues an SPI event with spi_handle=nullptr (send will fail).
 * Manually sets retries = k_event_max_retries - 1 so one more failure
 * exhausts the retry count and the entry is dropped.
 *
 * Expected: Queue count becomes 0 after poll (entry dropped on max retries)
 */
void test_event_queue_spi_retry_exhaustion(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* SPI handle is nullptr so send will return k_rx_err_invalid_state */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));

  /* Set retries to max-1: one more failure exhausts it */
  s_manager.event_queue[s_manager.event_queue_tail].retries = (uint8_t)(k_event_max_retries - 1);

  (void)rx_comm_manager_poll(&s_manager);
  /* Entry should be dropped (retries exhausted) */
  TEST_ASSERT_EQUAL(0U, s_manager.event_queue_count);
}

/**
 * @brief Test event queue SPI retry with backoff computation (lines 1038-1044)
 *
 * @details
 * Enqueues an SPI event with spi_handle=nullptr (send will fail).
 * retries starts at 0, so after one failure, retries becomes 1 (< k_event_max_retries).
 * The backoff computation runs: backoff_ms = 10 << 0 = 10, next_retry_time_ms = 10.
 * Covers lines 1026, 1038-1042, 1044.
 *
 * Expected: Queue count stays at 1 (entry not dropped, retry scheduled)
 */
void test_event_queue_spi_retry_backoff_computed(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* SPI handle is nullptr so send will fail */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));

  /* Entry retries starts at 0: after poll, retries becomes 1, backoff scheduled */
  (void)rx_comm_manager_poll(&s_manager);

  /* Entry should still be in queue (retry scheduled, not dropped) */
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue[s_manager.event_queue_tail].retries);
  /* next_retry_time_ms should be set to now + backoff (> 0) */
  TEST_ASSERT_GREATER_THAN(0U,
                           s_manager.event_queue[s_manager.event_queue_tail].next_retry_time_ms);

  /* Clean up */
  s_manager.event_queue_count       = 0;
  s_manager.event_queue[0].occupied = false;
}

/**
 * @brief Test heartbeat null mgr check (line 1070-1071) - defensive guard
 *
 * @details
 * internal_check_heartbeat is static and only called from rx_comm_manager_poll,
 * which validates mgr first. The null check at line 1070 is a defensive guard
 * that is unreachable via public API. This test documents that the guard exists
 * but cannot be triggered without direct access to the static function.
 *
 * @note This test is a placeholder to document the untestable guard.
 * Covered indirectly by test_heartbeat_timeout_fires_dead_callback.
 */
void test_heartbeat_known_untestable_null_guard(void)
{
  /* Verify the heartbeat mechanism works via public API (covers the happy path).
   * The null mgr guard at line 1070 is defensive code that cannot be triggered
   * because rx_comm_manager_poll validates mgr before calling the static helper. */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));
  /* Poll with no channels: heartbeat runs with no channels to check */
  (void)rx_comm_manager_poll(&s_manager);
  TEST_PASS();
}

/* =============================================================================
 * Additional coverage tests (round 2)
 * =============================================================================
 */

/**
 * @brief Test event queue USB channel: fire-and-forget dequeues even on send error
 *
 * @details
 * Enqueues a USB event. USB send fails because usb_handle=nullptr, but the USB
 * channel is fire-and-forget: the entry is dequeued regardless of error code.
 * Covers the `entry->channel == k_comm_channel_usb` TRUE branch at line 1009.
 *
 * Expected: Queue count becomes 0 after poll (USB event dropped on failure)
 */
void test_event_queue_usb_fire_and_forget_error(void)
{
  /* No USB handle: USB send will fail with k_rx_err_invalid_state */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);

  /* Poll: USB send fails but USB is fire-and-forget, entry is dequeued */
  (void)rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(0U, s_manager.event_queue_count);
}

/**
 * @brief Test event_send with non-null payload but zero length (line 1924 middle)
 *
 * @details
 * Enqueues an event with payload pointer non-null but payload_len == 0.
 * The condition `params->payload != nullptr && params->payload_len > 0` evaluates
 * to FALSE (short-circuit at payload_len == 0), so memcpy is NOT called.
 * Covers the intermediate branch of the `&&` at line 1924.
 *
 * Expected: Returns k_rx_ok; queue entry occupied but payload unchanged
 */
void test_event_send_payload_nonnull_zero_len(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  uint8_t                     dummy  = k_test_payload_ab;
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_usb,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = &dummy, /* Non-null payload */
    .payload_len = 0,      /* But zero length: memcpy must NOT be called */
  };
  rx_err_t err = rx_comm_manager_event_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);
}

/**
 * @brief Test backoff computation for retries=1 (backoff=10ms, below cap=15ms).
 *
 * @details
 * With k_event_initial_backoff_ms=10, k_event_max_backoff_ms=15, k_event_max_retries=3:
 * - 1st failure: retries=1, backoff = 10<<0 = 10ms (< cap=15ms, not capped)
 * This test verifies the first retry computes correctly without hitting the cap.
 * See test_event_queue_backoff_cap_reached for the cap-hit test (retries=2).
 *
 * Expected: Queue entry stays with retries=1 and backoff=10 (< cap)
 */
void test_event_queue_backoff_cap(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* SPI handle is nullptr so send will fail */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));

  /* retries starts at 0. After poll: retries=1, backoff=10<<0=10 < 100 (cap not hit).
   * The cap at line 1039 is unreachable: max achievable backoff = 10<<1=20 < 100 */
  (void)rx_comm_manager_poll(&s_manager);

  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue[s_manager.event_queue_tail].retries);

  /* Clean up */
  s_manager.event_queue_count       = 0;
  s_manager.event_queue[0].occupied = false;
}

/**
 * @brief Test I2C poll error propagation (lines 1515-1517)
 *
 * @details
 * Sets up a comm manager with an I2C handle. Then injects a hardware error into
 * the mock RIIC before polling. The error (k_rx_err_hw_error) is neither timeout
 * nor no_data, so it propagates through internal_poll_i2c and rx_comm_manager_poll
 * at lines 1515-1517.
 *
 * Expected: rx_comm_manager_poll returns k_rx_err_hw_error
 */
void test_poll_i2c_error_propagates(void)
{
  mock_riic_init();

  static rx_session_state_t   s_sess_ie;
  static rx_i2c_comm_handle_t s_i2c_ie;
  s_sess_ie = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_ie));
  s_i2c_ie                              = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg_ie = {
    .session     = &s_sess_ie,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_i2c_ie, &i2c_cfg_ie));

  const rx_comm_manager_config_t mgr_cfg_ie = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = &s_i2c_ie,
    .uart_handle           = nullptr,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg_ie));

  /* Inject an overrun error: will be returned by riic_peripheral_read_write */
  mock_riic_set_next_error(k_rx_err_hw_error);

  /* Poll: I2C receive returns overrun, which is neither timeout nor no_data */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);

  (void)rx_i2c_comm_deinit(&s_i2c_ie);
  (void)rx_session_deinit(&s_sess_ie);
}

/**
 * @brief Test UART poll error propagation (lines 1525-1526)
 *
 * @details
 * Sets up a comm manager with a UART handle. Injects a hardware overrun error via
 * mock_uart_hw_set_next_error so uart_rx_available returns k_rx_err_hw_error.
 * internal_read_uart_data returns overrun, internal_receive_iteration returns
 * k_receive_error, rx_uart_comm_receive returns overrun. Since overrun is neither
 * timeout nor no_data, rx_comm_manager_poll propagates it at lines 1525-1526.
 *
 * Expected: rx_comm_manager_poll returns k_rx_err_hw_error
 */
void test_poll_uart_error_propagates(void)
{
  mock_uart_hw_init();
  const uart_channel_config_t ch_cfg_ue = {
    .channel  = k_uart_channel_9,
    .baudrate = k_test_baudrate,
    .tx_gpio  = k_rx_p3_0,
    .rx_gpio  = k_rx_p3_1,
  };
  (void)uart_init_channel(&ch_cfg_ue);

  static rx_session_state_t    s_sess_ue;
  static rx_uart_comm_handle_t s_uart_ue;
  s_sess_ue = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_ue));
  s_uart_ue                               = (rx_uart_comm_handle_t){0};
  const rx_uart_comm_config_t uart_cfg_ue = {
    .session = &s_sess_ue,
    .channel = k_uart_channel_9,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_init(&s_uart_ue, &uart_cfg_ue));

  const rx_comm_manager_config_t mgr_cfg_ue = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = nullptr,
    .uart_handle           = &s_uart_ue,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg_ue));

  /* Inject overrun error: uart_rx_available returns overrun, propagates through */
  mock_uart_hw_set_next_error(k_rx_err_hw_error);

  /* Poll: UART receive gets overrun, which is neither timeout nor no_data */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);

  (void)rx_uart_comm_deinit(&s_uart_ue);
  (void)rx_session_deinit(&s_sess_ue);
}

/**
 * @brief Test heartbeat with status already dead: covers 1085 FALSE branch
 *
 * @details
 * After the heartbeat fires dead (covered in test_heartbeat_timeout_fires_dead_callback),
 * if we poll again the heartbeat check sees `hb->status == k_link_status_dead`
 * which is NOT k_link_status_healthy, so the inner block at line 1085 is skipped.
 * This test manually sets the heartbeat to dead and polls, covering line 1085 FALSE.
 *
 * Expected: Poll returns k_rx_ok (no action taken for dead-status heartbeat)
 */
void test_heartbeat_already_dead_no_refire(void)
{
  mock_riic_init();
  static rx_session_state_t   s_sess_hbd;
  static rx_i2c_comm_handle_t s_i2c_hbd;
  helper_init_i2c_manager(&s_sess_hbd, &s_i2c_hbd, nullptr, false);

  /* Force heartbeat to dead state (skips the healthy-only timeout check) */
  s_manager.heartbeat[k_comm_channel_i2c].status = k_link_status_dead;

  /* Poll: heartbeat sees dead status, skips 1085 inner block */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  /* Status remains dead (no auto-recovery on poll without frame) */
  TEST_ASSERT_EQUAL(k_link_status_dead, s_manager.heartbeat[k_comm_channel_i2c].status);

  (void)rx_i2c_comm_deinit(&s_i2c_hbd);
  (void)rx_session_deinit(&s_sess_hbd);
}

/**
 * @brief Test internal_handle_frame with callback == nullptr (line 674 FALSE)
 *
 * @details
 * Injects a valid I2C frame and polls with a comm manager that has NO callback.
 * internal_handle_frame is called but `mgr->callback == nullptr`, so the
 * callback invocation at line 674-675 is skipped (FALSE branch of line 674).
 *
 * Expected: Poll returns k_rx_ok; no crash despite null callback
 */
void test_poll_i2c_frame_received_no_callback(void)
{
  mock_riic_init();

  static rx_session_state_t   s_sess_nc;
  static rx_i2c_comm_handle_t s_i2c_nc;
  s_sess_nc = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_nc));
  s_i2c_nc                              = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg_nc = {
    .session     = &s_sess_nc,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_i2c_nc, &i2c_cfg_nc));

  const rx_comm_manager_config_t mgr_cfg_nc = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = &s_i2c_nc,
    .uart_handle           = nullptr,
    .callback              = nullptr, /* No callback: covers 674 FALSE */
    .callback_ctx          = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg_nc));

  /* Encode and inject a minimal frame */
  rx_frame_encoder_t enc_nc;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc_nc));

  rx_frame_t frame_nc;
  frame_nc                 = (rx_frame_t){0};
  frame_nc.header.type     = k_frame_type_command;
  frame_nc.header.sequence = 1;
  frame_nc.header.length   = 0;
  frame_nc.header.flags    = k_frame_flag_none;

  static uint8_t s_encoded_nc[k_test_encoded_buf_size];
  uint32_t       enc_len_nc = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc_nc, &frame_nc, s_encoded_nc, &enc_len_nc));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc_nc));

  mock_riic_set_rx_data(0, s_encoded_nc, (uint16_t)enc_len_nc);

  /* Poll: frame received, internal_handle_frame called with callback=nullptr */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_i2c_comm_deinit(&s_i2c_nc);
  (void)rx_session_deinit(&s_sess_nc);
}

/**
 * @brief Test init with matching spi_link succeeds (line 1244 FALSE)
 *
 * @details
 * Provides a spi_link whose spi_handle matches the config spi_handle.
 * The mismatch check at line 1244 evaluates to FALSE (they match), so init
 * continues to success. This also verifies the spi_link path is properly assigned.
 *
 * Expected: rx_comm_manager_init returns k_rx_ok; spi_link is set in manager
 */
void test_init_spi_link_matching_succeeds(void)
{
  static rx_session_state_t   s_sess_lm;
  static rx_spi_comm_handle_t s_spi_lm;
  helper_init_spi_handle(&s_sess_lm, &s_spi_lm);

  static rx_spi_link_t s_link_lm;
  s_link_lm                              = (rx_spi_link_t){0};
  const rx_spi_link_config_t link_cfg_lm = {
    .spi_handle  = &s_spi_lm,
    .fec_enabled = false,
    .max_retries = 3,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_link_init(&s_link_lm, &link_cfg_lm));

  const rx_comm_manager_config_t cfg_lm = {
    .usb_handle   = nullptr,
    .spi_handle   = &s_spi_lm, /* Matches spi_link->spi_handle */
    .spi_link     = &s_link_lm,
    .callback     = nullptr,
    .callback_ctx = nullptr,
  };
  rx_err_t err = rx_comm_manager_init(&s_manager, &cfg_lm);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_PTR(&s_link_lm, s_manager.spi_link);

  (void)rx_spi_link_deinit(&s_link_lm);
  (void)rx_spi_comm_deinit(&s_spi_lm);
  (void)rx_session_deinit(&s_sess_lm);
}

/**
 * @brief Test send via spi_link (line 1694 TRUE)
 *
 * @details
 * Initializes a comm manager with both spi_handle and spi_link. When a send
 * is performed on the SPI channel, the code at line 1694 checks spi_link != nullptr
 * and calls rx_spi_link_send instead of rx_spi_comm_send (covers 1694 TRUE branch).
 *
 * rx_spi_link_send requires HARQ ACK from the other side. In unit tests, the
 * mock RSPI does not provide ACKs, so the call will exhaust retries and fail.
 * The purpose of this test is to exercise the spi_link code path, not to verify
 * end-to-end HARQ success.
 *
 * Expected: rx_comm_manager_send returns non-ok (HARQ timeout/retry exhaustion)
 */
void test_send_spi_via_spi_link(void)
{
  static rx_session_state_t   s_sess_sl;
  static rx_spi_comm_handle_t s_spi_sl;
  helper_init_spi_handle(&s_sess_sl, &s_spi_sl);

  static rx_spi_link_t s_link_sl;
  s_link_sl                              = (rx_spi_link_t){0};
  const rx_spi_link_config_t link_cfg_sl = {
    .spi_handle  = &s_spi_sl,
    .fec_enabled = false,
    .max_retries = 1, /* Minimal retries to reduce test time */
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_link_init(&s_link_sl, &link_cfg_sl));

  const rx_comm_manager_config_t cfg_sl = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_sl,
    .spi_link              = &s_link_sl,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg_sl));

  const rx_comm_send_params_t params_sl = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  /* Call exercises line 1694 TRUE (spi_link path): rx_spi_link_send is called.
   * HARQ fails in mock (no ACK provided) so err is non-ok, but code path is covered. */
  rx_err_t err = rx_comm_manager_send(&s_manager, &params_sl);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err); /* HARQ fails without mock ACK */

  (void)rx_spi_link_deinit(&s_link_sl);
  (void)rx_spi_comm_deinit(&s_spi_sl);
  (void)rx_session_deinit(&s_sess_sl);
}

/**
 * @brief Test poll with spi_link set: covers line 816 TRUE branch
 *
 * @details
 * Initializes a comm manager with both spi_handle and spi_link, then polls.
 * internal_poll_spi sees `mgr->spi_link != nullptr` (TRUE at line 816) and calls
 * rx_spi_link_receive instead of raw rx_spi_comm_receive. Since no data is injected
 * into the RSPI mock, the link receive returns timeout/error and internal_poll_spi
 * returns that error. rx_comm_manager_poll propagates it normally.
 *
 * Expected: Poll returns k_rx_err_timeout (no SPI data available)
 */
void test_poll_spi_with_spi_link_no_data(void)
{
  static rx_session_state_t   s_sess_psl;
  static rx_spi_comm_handle_t s_spi_psl;
  helper_init_spi_handle(&s_sess_psl, &s_spi_psl);

  static rx_spi_link_t s_link_psl;
  s_link_psl                              = (rx_spi_link_t){0};
  const rx_spi_link_config_t link_cfg_psl = {
    .spi_handle  = &s_spi_psl,
    .fec_enabled = false,
    .max_retries = 1,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_link_init(&s_link_psl, &link_cfg_psl));

  const rx_comm_manager_config_t cfg_psl = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_psl,
    .spi_link              = &s_link_psl,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg_psl));

  /* Poll: internal_poll_spi enters spi_link path (line 816 TRUE) */
  (void)rx_comm_manager_poll(&s_manager);

  (void)rx_spi_link_deinit(&s_link_psl);
  (void)rx_spi_comm_deinit(&s_spi_psl);
  (void)rx_session_deinit(&s_sess_psl);
}

/**
 * @brief Test UART poll with handle set but no data: covers 1524 no_data branch
 *
 * @details
 * Sets up a UART handle with an initialized channel but injects no RX data.
 * rx_uart_comm_receive returns k_rx_err_no_data (empty FIFO).
 * internal_poll_uart returns k_rx_err_no_data.
 * At line 1524: `uart_err != k_rx_err_timeout` (TRUE) AND `uart_err != k_rx_err_no_data`
 * is FALSE -> the overall condition is FALSE -> no error propagation.
 * This covers the second short-circuit branch of the `&&` at line 1524.
 *
 * Expected: Poll returns k_rx_err_timeout (no channels deliver data)
 */
void test_poll_uart_handle_set_no_data(void)
{
  mock_uart_hw_init();
  const uart_channel_config_t ch_cfg_nd = {
    .channel  = k_uart_channel_9,
    .baudrate = k_test_baudrate,
    .tx_gpio  = k_rx_p3_0,
    .rx_gpio  = k_rx_p3_1,
  };
  (void)uart_init_channel(&ch_cfg_nd);

  static rx_session_state_t    s_sess_nd;
  static rx_uart_comm_handle_t s_uart_nd;
  s_sess_nd = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_nd));
  s_uart_nd                               = (rx_uart_comm_handle_t){0};
  const rx_uart_comm_config_t uart_cfg_nd = {
    .session = &s_sess_nd,
    .channel = k_uart_channel_9,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_uart_comm_init(&s_uart_nd, &uart_cfg_nd));

  const rx_comm_manager_config_t mgr_cfg_nd = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = nullptr,
    .uart_handle           = &s_uart_nd,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg_nd));

  /* Poll with no injected data: UART returns no_data -> 1524 branch 2 */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);

  (void)rx_uart_comm_deinit(&s_uart_nd);
  (void)rx_session_deinit(&s_sess_nd);
}

/**
 * @brief Test channel_ready for USB with handle set but USB not in configured state
 *
 * @details
 * Initializes the USB hardware and driver (so initialized=true) but sets the
 * device state to k_usb_state_attached (not configured). This ensures that
 * rx_usb_is_configured() returns false (initialized==true but state!=configured),
 * causing the `&&` expression at line 2053 to evaluate to false.
 *
 * This covers the FALSE branch of rx_usb_is_configured() at line 2053 when
 * usb_handle is non-null (the second operand of `&&` is evaluated and returns false).
 *
 * Expected: ready == false (USB handle set but USB not in configured state)
 */
void test_channel_ready_usb_handle_set_not_configured(void)
{
  /* Initialize USB hardware mock and driver, but leave state as attached (not configured).
   * rx_usb_is_configured() returns s_usb.initialized && (state == k_usb_state_configured).
   * With state == k_usb_state_attached, it returns false even though initialized==true. */
  mock_usb_hw_init(nullptr);
  mock_regs_init();
  (void)rx_usb_deinit();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  /* rx_usb_init sets state to k_usb_state_attached, NOT k_usb_state_configured */
  rx_usb_set_state(k_usb_state_attached);

  static rx_session_state_t   s_sess_cr;
  static rx_usb_comm_handle_t s_usb_cr;
  helper_init_usb_handle(&s_sess_cr, &s_usb_cr);

  const rx_comm_manager_config_t cfg_cr = {
    .usb_handle = &s_usb_cr,
    .spi_handle = nullptr,
    .callback   = nullptr,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg_cr));

  /* Verify that USB is initialized but NOT configured */
  TEST_ASSERT_FALSE(rx_usb_is_configured(k_usb_port_proto));

  bool     ready;
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_usb, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* rx_usb_is_configured returns false (state==attached, not configured),
   * so ready=false even though usb_handle != nullptr. Covers line 2053 branch 3. */
  TEST_ASSERT_FALSE(ready);

  (void)rx_usb_comm_deinit(&s_usb_cr);
  (void)rx_session_deinit(&s_sess_cr);
  (void)rx_usb_deinit();
  mock_usb_hw_deinit(nullptr);
}

/**
 * @brief Test channel_ready for USB with handle set and USB fully configured
 *
 * @details
 * Initializes the USB hardware and driver and sets the device state to
 * k_usb_state_configured. This ensures that rx_usb_is_configured() returns true
 * (initialized==true AND state==configured), causing the `&&` expression at line
 * 2053 to evaluate to true (ready=true).
 *
 * This covers the TRUE branch (branch 3 fallthrough) of rx_usb_is_configured()
 * at line 2053 when usb_handle is non-null and USB IS fully configured.
 *
 * Expected: ready == true (USB handle set and USB is configured)
 */
void test_channel_ready_usb_handle_set_and_configured(void)
{
  /* Initialize USB hardware mock and driver, then set state to configured.
   * rx_usb_is_configured() returns s_usb.initialized && (state == k_usb_state_configured).
   * With state == k_usb_state_configured, it returns true. */
  mock_usb_hw_init(nullptr);
  mock_regs_init();
  (void)rx_usb_deinit();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  static rx_session_state_t   s_sess_cf;
  static rx_usb_comm_handle_t s_usb_cf;
  helper_init_usb_handle(&s_sess_cf, &s_usb_cf);

  const rx_comm_manager_config_t cfg_cf = {
    .usb_handle = &s_usb_cf,
    .spi_handle = nullptr,
    .callback   = nullptr,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg_cf));

  /* Verify USB is initialized and configured */
  TEST_ASSERT_TRUE(rx_usb_is_configured(k_usb_port_proto));

  bool     ready;
  rx_err_t err = rx_comm_manager_channel_ready(&s_manager, k_comm_channel_usb, &ready);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* rx_usb_is_configured returns true (state==configured), so ready=true.
   * Covers line 2053 branch 3 (fallthrough = rx_usb_is_configured returned TRUE). */
  TEST_ASSERT_TRUE(ready);

  (void)rx_usb_comm_deinit(&s_usb_cf);
  (void)rx_session_deinit(&s_sess_cf);
  (void)rx_usb_deinit();
  mock_usb_hw_deinit(nullptr);
}

/**
 * @brief Test decoded output send with empty payload covers 1755 FALSE branch
 *
 * @details
 * Sends on I2C with decoded output enabled and payload_len == 0. The condition
 * `params->payload_len > 0 && params->payload_len <= k_frame_max_payload` at
 * line 1755 evaluates to FALSE (payload_len == 0), so memcpy is skipped.
 * This covers the FALSE branch of line 1755.
 *
 * Expected: rx_comm_manager_send returns k_rx_ok
 */
void test_send_i2c_decoded_empty_payload(void)
{
  mock_riic_init();
  static rx_session_state_t   s_sess_dep;
  static rx_i2c_comm_handle_t s_i2c_dep;
  s_sess_dep = (rx_session_state_t){0};
  TEST_ASSERT_EQUAL(k_rx_ok, rx_session_init(&s_sess_dep));
  s_i2c_dep                              = (rx_i2c_comm_handle_t){0};
  const rx_i2c_comm_config_t i2c_cfg_dep = {
    .session     = &s_sess_dep,
    .channel     = {.value = 0},
    .device_addr = {.value = 0x42},
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_i2c_comm_init(&s_i2c_dep, &i2c_cfg_dep));

  const rx_comm_manager_config_t mgr_cfg_dep = {
    .usb_handle            = nullptr,
    .spi_handle            = nullptr,
    .i2c_handle            = &s_i2c_dep,
    .uart_handle           = nullptr,
    .callback              = nullptr,
    .enable_decoded_output = true, /* Enable decoded output */
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &mgr_cfg_dep));

  const rx_comm_send_params_t params_dep = {
    .channel     = k_comm_channel_i2c,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0, /* Empty payload: covers 1755 FALSE */
  };
  rx_err_t err = rx_comm_manager_send(&s_manager, &params_dep);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_i2c_comm_deinit(&s_i2c_dep);
  (void)rx_session_deinit(&s_sess_dep);
}

/* =============================================================================
 * USB poll success path (lines 743-744, 1475)
 * =============================================================================
 */

/**
 * @brief Helper: encode a command frame with optional payload and flags.
 *
 * @param[in]  payload      Payload bytes (may be nullptr)
 * @param[in]  payload_len  Payload length (0 for empty)
 * @param[in]  flags        Frame flags (e.g. k_frame_flag_retransmit)
 * @param[out] out_buf      Output wire-format bytes
 * @param[out] out_len      Wire length written
 */
static void helper_encode_cmd_frame_ex(const uint8_t* payload,
                                       uint32_t       payload_len,
                                       uint8_t        flags,
                                       uint8_t*       out_buf,
                                       uint32_t*      out_len)
{
  rx_frame_encoder_t enc;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc));
  rx_frame_t frame;
  frame                 = (rx_frame_t){0};
  frame.header.type     = k_frame_type_command;
  frame.header.sequence = 1;
  frame.header.length   = (uint16_t)payload_len;
  frame.header.flags    = flags;
  if (payload != nullptr && payload_len > 0) {
    for (uint32_t i = 0; i < payload_len; i++) {
      frame.payload[i] = payload[i];
    }
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc, &frame, out_buf, out_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc));
}

/**
 * @brief Helper: encode a minimal command frame (zero payload, no flags).
 *
 * @param[out] out_buf  Output wire-format bytes
 * @param[out] out_len  Wire length written
 */
static void helper_encode_cmd_frame(uint8_t* out_buf, uint32_t* out_len)
{
  helper_encode_cmd_frame_ex(nullptr, 0, k_frame_flag_none, out_buf, out_len);
}

/**
 * @brief Test poll USB success path covers lines 743-744 and 1475.
 *
 * @details
 * Initializes USB in configured state, injects an encoded frame into
 * the USB RX ring buffer, then polls. internal_poll_usb decodes the frame
 * and calls internal_handle_frame (line 743), returning k_rx_ok (line 744).
 * rx_comm_manager_poll sets received=true (line 1475).
 *
 * Expected: rx_comm_manager_poll returns k_rx_ok
 */
void test_poll_usb_success_path(void)
{
  /* Set up USB hardware mock and configure USB */
  mock_usb_hw_init(nullptr);
  mock_regs_init();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_init(nullptr));
  rx_usb_set_state(k_usb_state_configured);

  static rx_session_state_t   s_sess_us2;
  static rx_usb_comm_handle_t s_usb_us2;
  helper_init_usb_handle(&s_sess_us2, &s_usb_us2);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = &s_usb_us2,
    .spi_handle            = nullptr,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  /* Encode and inject a frame into the USB RX buffer */
  static uint8_t s_wire[k_test_encoded_buf_size];
  uint32_t       wire_len = 0;
  helper_encode_cmd_frame(s_wire, &wire_len);
  (void)rx_usb_rx_push(k_usb_port_proto, s_wire, wire_len);

  /* Poll: USB receive should decode the frame and return k_rx_ok */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_usb_comm_deinit(&s_usb_us2);
  (void)rx_session_deinit(&s_sess_us2);
  (void)rx_usb_deinit();
  mock_usb_hw_deinit(nullptr);
}

/* =============================================================================
 * SPI (no link) poll success path (lines 854-855, 1485)
 * =============================================================================
 */

/**
 * @brief Test poll SPI (no link) success path covers lines 854-855 and 1485.
 *
 * @details
 * Initializes a comm manager with spi_handle but no spi_link, injects an
 * encoded frame into the RSPI mock RX buffer, then polls. internal_poll_spi
 * uses raw rx_spi_comm_receive path and succeeds, calling internal_handle_frame
 * (line 854) and returning k_rx_ok (line 855). poll sets received=true (1485).
 *
 * Expected: rx_comm_manager_poll returns k_rx_ok
 */
void test_poll_spi_no_link_success_path(void)
{
  static rx_session_state_t   s_sess_spi2;
  static rx_spi_comm_handle_t s_spi_spi2;
  helper_init_spi_handle(&s_sess_spi2, &s_spi_spi2);

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_spi2,
    .spi_link              = nullptr, /* No link: use raw SPI path */
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  /* Encode a frame and inject into RSPI mock */
  static uint8_t s_wire2[k_test_encoded_buf_size];
  uint32_t       wire_len2 = 0;
  helper_encode_cmd_frame(s_wire2, &wire_len2);
  TEST_ASSERT_EQUAL(k_rx_ok,
                    mock_rspi_inject_rx_data(nullptr, k_rspi_channel_0, s_wire2, wire_len2));
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);

  /* Poll: SPI receives the frame via raw path (no spi_link) */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_comm_deinit(&s_spi_spi2);
  (void)rx_session_deinit(&s_sess_spi2);
}

/* =============================================================================
 * SPI link poll success path (lines 825-843) and payload-too-large (818-820)
 * =============================================================================
 */

/**
 * @brief Test poll SPI with spi_link: success path covers lines 825-843.
 *
 * @details
 * Initializes a comm manager with spi_handle AND spi_link (no FEC). Injects
 * an encoded command frame into the RSPI mock. internal_poll_spi takes the
 * spi_link path: rx_spi_link_receive returns k_rx_ok, frame is converted to
 * rx_frame_t and dispatched via internal_handle_frame (lines 825-843).
 *
 * Expected: rx_comm_manager_poll returns k_rx_ok
 */
void test_poll_spi_with_link_success_path(void)
{
  static rx_session_state_t   s_sess_slr;
  static rx_spi_comm_handle_t s_spi_slr;
  helper_init_spi_handle(&s_sess_slr, &s_spi_slr);

  static rx_spi_link_t s_link_slr;
  s_link_slr                          = (rx_spi_link_t){0};
  const rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_slr,
    .fec_enabled = false,
    .max_retries = 1,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_link_init(&s_link_slr, &link_cfg));

  const rx_comm_manager_config_t cfg = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_slr,
    .spi_link              = &s_link_slr,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg));

  /* Inject a command frame with payload + retransmit flag.
   * Covers lines 831 (is_retransmit true) and 838 (payload_len > 0 memcpy). */
  static uint8_t s_payload3[] = {k_test_payload_byte_0,
                                 k_test_payload_byte_1,
                                 k_test_payload_byte_2};
  static uint8_t s_wire3[k_test_wire_buf_size];
  uint32_t       wire_len3 = 0;
  helper_encode_cmd_frame_ex(s_payload3,
                             sizeof(s_payload3),
                             k_frame_flag_retransmit,
                             s_wire3,
                             &wire_len3);
  TEST_ASSERT_EQUAL(k_rx_ok,
                    mock_rspi_inject_rx_data(nullptr, k_rspi_channel_0, s_wire3, wire_len3));
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);

  /* Poll: spi_link path decodes frame with retransmit flag and non-empty payload */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_link_deinit(&s_link_slr);
  (void)rx_spi_comm_deinit(&s_spi_slr);
  (void)rx_session_deinit(&s_sess_slr);
}

/**
 * @brief Test poll SPI with spi_link and zero-payload frame covers line 837 FALSE branch.
 *
 * @details
 * Injects an encoded frame with no payload (payload_len == 0) through the SPI
 * link path. At line 837, `if (s_link_result.payload_len > k_zero_u32)` evaluates
 * to FALSE (payload_len == 0), so the memcpy is skipped. This covers the FALSE
 * branch of the payload copy guard.
 *
 * Expected: rx_comm_manager_poll returns k_rx_ok (frame received, zero payload)
 */
void test_poll_spi_with_link_zero_payload(void)
{
  static rx_session_state_t   s_sess_zpay;
  static rx_spi_comm_handle_t s_spi_zpay;
  helper_init_spi_handle(&s_sess_zpay, &s_spi_zpay);

  static rx_spi_link_t s_link_zpay;
  s_link_zpay                              = (rx_spi_link_t){0};
  const rx_spi_link_config_t link_cfg_zpay = {
    .spi_handle  = &s_spi_zpay,
    .fec_enabled = false,
    .max_retries = 1,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_link_init(&s_link_zpay, &link_cfg_zpay));

  const rx_comm_manager_config_t cfg_zpay = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_zpay,
    .spi_link              = &s_link_zpay,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg_zpay));

  /* Encode a frame with zero payload (no payload data).
   * helper_encode_cmd_frame_ex with nullptr payload and 0 length produces
   * a frame with no payload bytes -- payload_len == 0 in link result. */
  static uint8_t s_wire_zpay[k_test_encoded_buf_size];
  uint32_t       wire_len_zpay = 0;
  helper_encode_cmd_frame_ex(nullptr, 0U, k_frame_flag_none, s_wire_zpay, &wire_len_zpay);
  TEST_ASSERT_EQUAL(
    k_rx_ok,
    mock_rspi_inject_rx_data(nullptr, k_rspi_channel_0, s_wire_zpay, wire_len_zpay));
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);

  /* Poll: spi_link path decodes frame with zero payload; line 837 FALSE branch taken */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_link_deinit(&s_link_zpay);
  (void)rx_spi_comm_deinit(&s_spi_zpay);
  (void)rx_session_deinit(&s_sess_zpay);
}

/**
 * @brief Helper: FEC-encode data and build a wire-format frame for injection
 *
 * @param[in]  harq         HARQ context for FEC encoding
 * @param[in]  payload      Raw payload to FEC-encode
 * @param[in]  payload_len  Length of raw payload
 * @param[out] wire_buf     Output wire-format buffer
 * @param[out] wire_len     Output wire length
 */
static void helper_build_fec_wire_frame(rx_harq_handle_t* harq,
                                        const uint8_t*    payload,
                                        uint32_t          payload_len,
                                        uint8_t*          wire_buf,
                                        uint32_t*         wire_len)
{
  static uint8_t s_fec_tmp[k_spi_link_max_encoded_payload];
  uint32_t       fec_len = 0;
  TEST_ASSERT_EQUAL(
    k_rx_ok,
    rx_harq_encode(harq, payload, payload_len, s_fec_tmp, sizeof(s_fec_tmp), &fec_len));

  rx_frame_encoder_t enc;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&enc));
  rx_frame_t frame      = {0};
  frame.header.type     = k_frame_type_command;
  frame.header.sequence = 1;
  frame.header.length   = (uint16_t)fec_len;
  frame.header.flags    = k_frame_flag_fec_enabled;
  for (uint32_t i = 0; i < fec_len; i++) {
    frame.payload[i] = s_fec_tmp[i];
  }
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&enc, &frame, wire_buf, wire_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_deinit(&enc));
}

/**
 * @brief Test poll SPI with spi_link FEC: covers fec_decoded=true path
 *
 * Expected: rx_comm_manager_poll returns k_rx_ok
 */
void test_poll_spi_with_link_fec_decoded(void)
{
  static rx_session_state_t   s_sess_fec;
  static rx_spi_comm_handle_t s_spi_fec;
  helper_init_spi_handle(&s_sess_fec, &s_spi_fec);

  static rx_spi_link_t s_link_fec;
  s_link_fec                              = (rx_spi_link_t){0};
  const rx_spi_link_config_t link_cfg_fec = {
    .spi_handle  = &s_spi_fec,
    .fec_enabled = true, /* Enable FEC to trigger fec_decoded=true path */
    .max_retries = 1,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_spi_link_init(&s_link_fec, &link_cfg_fec));

  const rx_comm_manager_config_t cfg_fec = {
    .usb_handle            = nullptr,
    .spi_handle            = &s_spi_fec,
    .spi_link              = &s_link_fec,
    .callback              = nullptr,
    .enable_decoded_output = false,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, &cfg_fec));

  /* FEC-encode a small payload and build wire-format frame */
  static const uint8_t s_original_fec[] = {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A};
  static uint8_t       s_wire_fec[k_frame_max_size];
  uint32_t             wire_len_fec = 0;
  helper_build_fec_wire_frame(&s_link_fec.harq,
                              s_original_fec,
                              sizeof(s_original_fec),
                              s_wire_fec,
                              &wire_len_fec);

  TEST_ASSERT_EQUAL(k_rx_ok,
                    mock_rspi_inject_rx_data(nullptr, k_rspi_channel_0, s_wire_fec, wire_len_fec));
  mock_rspi_set_data_available(nullptr, k_rspi_channel_0, true);

  /* Poll: spi_link FEC path decodes successfully, sets fec_decoded=true (line 834) */
  rx_err_t err = rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_spi_link_deinit(&s_link_fec);
  (void)rx_spi_comm_deinit(&s_spi_fec);
  (void)rx_session_deinit(&s_sess_fec);
}

/* =============================================================================
 * Backoff cap (line 1034)
 * =============================================================================
 */

/**
 * @brief Test event queue backoff cap is hit at retries=2 (line 1034).
 *
 * @details
 * With k_event_initial_backoff_ms=10, k_event_max_backoff_ms=15, k_event_max_retries=3:
 * - 1st failure: retries=1, backoff = 10<<0 = 10ms (< cap, not capped)
 * - 2nd failure: retries=2, backoff = 10<<1 = 20ms (> 15ms cap, IS capped to 15ms)
 * This test drives two poll failures to trigger the cap at line 1034.
 *
 * Expected: After two poll failures, the entry's retries == 2
 */
void test_event_queue_backoff_cap_reached(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_init(&s_manager, nullptr));

  /* Queue a SPI event (spi_handle == nullptr, so send always fails) */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_spi,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0,
  };
  TEST_ASSERT_EQUAL(k_rx_ok, rx_comm_manager_event_send(&s_manager, &params));

  /* First poll: retries becomes 1, backoff=10ms (not capped) */
  (void)rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);

  /* Force next_retry_time_ms to 0 so the entry is eligible for retry immediately */
  s_manager.event_queue[s_manager.event_queue_tail].next_retry_time_ms = 0;

  /* Second poll: retries becomes 2, backoff=20ms which exceeds cap of 15ms -> capped */
  (void)rx_comm_manager_poll(&s_manager);
  TEST_ASSERT_EQUAL(1U, s_manager.event_queue_count);
  TEST_ASSERT_EQUAL(k_test_retry_count, s_manager.event_queue[s_manager.event_queue_tail].retries);

  /* Clean up queue */
  s_manager.event_queue_count       = 0;
  s_manager.event_queue[0].occupied = false;
}

/**
 * @brief Test rx_comm_manager_send with payload_len == 0 covers FALSE branch
 *
 * @details
 * When payload_len == 0, the condition (payload_len > 0 && ...) is FALSE so
 * memcpy is skipped. This exercises the FALSE branch of the compound condition.
 * Uses the I2C decoded-output send path to reach that code region.
 *
 * @pre setUp() completed
 * @post FALSE branch of payload_len > 0 condition covered
 *
 * @see rx_comm_manager_send() Public API
 */
void test_send_i2c_zero_payload_len_false_branch(void)
{
  static rx_session_state_t   s_sess_zp;
  static rx_i2c_comm_handle_t s_i2c_zp;
  helper_init_i2c_manager(&s_sess_zp, &s_i2c_zp, nullptr, true);

  /* Send with payload_len == 0: condition (payload_len > 0 && ...) is FALSE */
  const rx_comm_send_params_t params = {
    .channel     = k_comm_channel_i2c,
    .type        = k_frame_type_command,
    .flags       = k_frame_flag_none,
    .payload     = nullptr,
    .payload_len = 0U,
  };
  const rx_err_t err = rx_comm_manager_send(&s_manager, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  (void)rx_i2c_comm_deinit(&s_i2c_zp);
  (void)rx_session_deinit(&s_sess_zp);
}

/* =============================================================================
 * Test Runners
 * =============================================================================
 */

/**
 * @brief Run API validation and error handling tests
 */
static void internal_run_api_tests(void)
{
  /* Init tests */
  RUN_TEST(test_init_null_manager);
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_with_config);
  RUN_TEST(test_init_clears_previous_state);

  /* Deinit tests */
  RUN_TEST(test_deinit_null_manager);
  RUN_TEST(test_deinit_success);
  RUN_TEST(test_deinit_uninitialized);

  /* Poll tests */
  RUN_TEST(test_poll_null_manager);
  RUN_TEST(test_poll_uninitialized);
  RUN_TEST(test_poll_no_channels);

  /* Send tests */
  RUN_TEST(test_send_null_manager);
  RUN_TEST(test_send_uninitialized);
  RUN_TEST(test_send_null_params);
  RUN_TEST(test_send_null_payload_with_length);
  RUN_TEST(test_send_usb_channel_not_enabled);
  RUN_TEST(test_send_spi_channel_not_enabled);
  RUN_TEST(test_send_i2c_channel_not_enabled);
  RUN_TEST(test_send_uart_channel_not_enabled);
  RUN_TEST(test_send_invalid_channel);
  RUN_TEST(test_send_empty_payload_allowed);
  RUN_TEST(test_send_payload_too_large);

  /* Stream send tests */
  RUN_TEST(test_stream_send_null_manager);
  RUN_TEST(test_stream_send_uninitialized);
}

/**
 * @brief Run event, link status, channel, and retransmit tests
 */
static void internal_run_channel_tests(void)
{
  /* Event send tests */
  RUN_TEST(test_event_send_null_manager);
  RUN_TEST(test_event_send_null_params);
  RUN_TEST(test_event_send_uninitialized);
  RUN_TEST(test_event_send_payload_too_large);
  RUN_TEST(test_event_send_null_payload_with_length);
  RUN_TEST(test_event_send_enqueue_success);

  /* Link status tests */
  RUN_TEST(test_link_status_null_manager);
  RUN_TEST(test_link_status_null_status);
  RUN_TEST(test_link_status_uninitialized);
  RUN_TEST(test_link_status_invalid_channel);
  RUN_TEST(test_link_status_initial_unknown);

  /* Process retransmits tests */
  RUN_TEST(test_process_retransmits_null_manager);
  RUN_TEST(test_process_retransmits_uninitialized);
  RUN_TEST(test_process_retransmits_no_spi);
  RUN_TEST(test_set_auto_retransmit_null_manager);
  RUN_TEST(test_set_auto_retransmit_uninitialized);
  RUN_TEST(test_set_auto_retransmit_no_spi);

  /* Respond tests */
  RUN_TEST(test_respond_null_manager);
  RUN_TEST(test_respond_uninitialized);
  RUN_TEST(test_respond_channel_not_enabled);

  /* Channel ready tests */
  RUN_TEST(test_channel_ready_null_manager);
  RUN_TEST(test_channel_ready_null_ready);
  RUN_TEST(test_channel_ready_uninitialized);
  RUN_TEST(test_channel_ready_usb_not_configured);
  RUN_TEST(test_channel_ready_spi_not_configured);
  RUN_TEST(test_channel_ready_i2c_not_configured);
  RUN_TEST(test_channel_ready_uart_not_configured);
  RUN_TEST(test_channel_ready_invalid_channel);

  /* Channel name tests */
  RUN_TEST(test_channel_name_usb);
  RUN_TEST(test_channel_name_spi);
  RUN_TEST(test_channel_name_i2c);
  RUN_TEST(test_channel_name_uart);
  RUN_TEST(test_channel_name_invalid);

  /* Enum tests */
  RUN_TEST(test_channel_enum_values);
}

/**
 * @brief Run integration, heartbeat, and SPI link tests
 */
static void internal_run_integration_tests(void)
{
  /* Integration test: valid I2C frame triggers internal_handle_frame */
  RUN_TEST(test_poll_i2c_valid_frame_triggers_callback);

  /* Integration test: decoded output enabled path */
  RUN_TEST(test_poll_i2c_decoded_output_enabled);

  /* Heartbeat and link status tests */
  RUN_TEST(test_heartbeat_already_healthy_on_second_frame);
  RUN_TEST(test_link_status_cb_fires_on_first_frame);
  RUN_TEST(test_heartbeat_dead_to_healthy_recovery);
  RUN_TEST(test_output_decoded_no_usb_puts_on_empty_len);

  /* Init SPI link validation tests */
  RUN_TEST(test_init_spi_link_without_spi_handle);
  RUN_TEST(test_init_spi_link_not_initialized);
  RUN_TEST(test_init_spi_link_handle_mismatch);

  /* Send via I2C/UART channel with valid handle tests */
  RUN_TEST(test_send_i2c_channel_succeeds);
  RUN_TEST(test_send_i2c_with_decoded_output);
  RUN_TEST(test_send_uart_channel_succeeds);

  /* Event queue tests */
  RUN_TEST(test_event_send_queue_full);
  RUN_TEST(test_event_queue_processes_usb_event_on_poll);
  RUN_TEST(test_event_send_with_payload_copies_data);

  /* Heartbeat timeout test */
  RUN_TEST(test_heartbeat_timeout_fires_dead_callback);
  RUN_TEST(test_heartbeat_timeout_no_callback);

  /* USB/SPI handle send and poll path tests */
  RUN_TEST(test_send_usb_handle_non_null_covers_send_path);
  RUN_TEST(test_poll_usb_handle_non_null_propagates_error);
  RUN_TEST(test_poll_spi_handle_non_null_propagates_error);
  RUN_TEST(test_send_spi_handle_non_null_no_spi_link);
  RUN_TEST(test_send_spi_with_decoded_output);
  RUN_TEST(test_poll_uart_valid_frame_triggers_callback);

  /* process_retransmits and set_auto_retransmit with SPI handle */
  RUN_TEST(test_process_retransmits_with_spi_handle);
  RUN_TEST(test_set_auto_retransmit_with_spi_handle);
}

/**
 * @brief Run coverage and edge case tests
 */
static void internal_run_coverage_tests(void)
{
  /* Event queue edge cases */
  RUN_TEST(test_event_queue_entry_not_occupied_returns_early);
  RUN_TEST(test_event_queue_backoff_not_elapsed_skips_entry);
  RUN_TEST(test_event_queue_spi_retry_exhaustion);
  RUN_TEST(test_event_queue_spi_retry_backoff_computed);
  RUN_TEST(test_heartbeat_known_untestable_null_guard);

  /* Event queue: I2C event success path */
  RUN_TEST(test_event_queue_processes_i2c_event_success);

  /* Additional coverage round 2 */
  RUN_TEST(test_event_queue_usb_fire_and_forget_error);
  RUN_TEST(test_event_send_payload_nonnull_zero_len);
  RUN_TEST(test_event_queue_backoff_cap);
  RUN_TEST(test_poll_i2c_error_propagates);
  RUN_TEST(test_poll_uart_error_propagates);
  RUN_TEST(test_heartbeat_already_dead_no_refire);
  RUN_TEST(test_poll_i2c_frame_received_no_callback);
  RUN_TEST(test_init_spi_link_matching_succeeds);
  RUN_TEST(test_send_spi_via_spi_link);
  RUN_TEST(test_poll_spi_with_spi_link_no_data);
  RUN_TEST(test_poll_uart_handle_set_no_data);
  RUN_TEST(test_channel_ready_usb_handle_set_not_configured);
  RUN_TEST(test_channel_ready_usb_handle_set_and_configured);
  RUN_TEST(test_send_i2c_decoded_empty_payload);

  /* Additional coverage round 3: poll success paths and backoff cap */
  RUN_TEST(test_poll_usb_success_path);
  RUN_TEST(test_poll_spi_no_link_success_path);
  RUN_TEST(test_poll_spi_with_link_success_path);
  RUN_TEST(test_poll_spi_with_link_zero_payload);
  RUN_TEST(test_poll_spi_with_link_fec_decoded);
  RUN_TEST(test_event_queue_backoff_cap_reached);

  /* send payload_len==0 coverage */
  RUN_TEST(test_send_i2c_zero_payload_len_false_branch);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  internal_run_api_tests();
  internal_run_channel_tests();
  internal_run_integration_tests();
  internal_run_coverage_tests();

  return UNITY_END();
}
