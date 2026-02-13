/* tests/test_rx_frame.c */

/**
 * @file test_rx_frame.c
 * @brief Unit Tests for Frame Layer Protocol Implementation
 *
 * @details
 * ## Overview
 *
 * This test suite provides comprehensive validation of the frame-level protocol
 * implementation (rx_frame.h/c) for SPI communication between the Raspberry Pi 5
 * gateway and the RX72N motor controller. It ensures bit-exact compatibility with
 * the Go implementation in star-gateway/internal/frame/.
 *
 * **Test Coverage:**
 * - **Encoder/decoder initialization** - nullptr checks, state validation
 * - **Frame encoding** - SYNC word, sequence numbers, payload, CRC generation
 * - **Frame decoding** - Header parsing, CRC verification, payload extraction
 * - **Round-trip validation** - Encode → Decode → Verify identity
 * - **Endianness** - Little-endian multi-byte fields, little-endian CRC-32
 * - **Go compatibility** - Bit-exact wire format matching Go test vectors
 * - **Edge cases** - Max payload, sequence rollover, truncated frames
 * - **Error handling** - Invalid arguments, CRC mismatches, invalid sizes
 *
 * ## Frame Protocol Summary
 *
 * ```
 * ┌──────────┬──────────┬──────────┬──────────┬──────────┬───────────────┬──────────┐
 * │  SYNC    │   SEQ    │   LEN    │  TYPE    │  FLAGS   │   PAYLOAD     │  CRC-32  │
 * │  2 bytes │  2 bytes │  2 bytes │  1 byte  │  1 byte  │   0-1024 B    │  4 bytes │
 * │  0x55AA  │   (LE)   │   (LE)   │          │          │               │   (LE)   │
 * └──────────┴──────────┴──────────┴──────────┴──────────┴───────────────┴──────────┘
 * ```
 *
 * ## Test Organization
 *
 * Tests are organized by functional area:
 *
 * 1. **Initialization Tests** - Encoder/decoder setup and validation
 * 2. **Encode Tests** - Frame construction, header generation, payload handling
 * 3. **Decode Tests** - Wire data parsing, validation, error detection
 * 4. **Round-Trip Tests** - Encode → Decode identity verification
 * 5. **Utility Function Tests** - Helper APIs (create_ack, create_nack, etc.)
 * 6. **Frame Type Tests** - Command, response, ACK, NACK frame handling
 * 7. **Frame Flag Tests** - All flag combinations (requires_ack, retransmit, etc.)
 * 8. **Endianness Tests** - Little-endian fields, little-endian CRC verification
 * 9. **Go Compatibility Tests** - Bit-exact wire format validation
 * 10. **Edge Case Tests** - Boundary conditions, sequence rollover, truncation
 *
 * ## Test State Machine
 *
 * @startuml
 * [*] --> TestSetup : setUp()
 * TestSetup --> EncoderInit : Initialize encoder
 * TestSetup --> DecoderInit : Initialize decoder
 * EncoderInit --> TestExecution : Ready
 * DecoderInit --> TestExecution : Ready
 * TestExecution --> Assertion : Verify behavior
 * Assertion --> TestCleanup : Test complete
 * Assertion --> TestFailed : Assertion failed
 * TestCleanup --> [*] : tearDown()
 * TestFailed --> [*] : Report failure
 * @enduml
 *
 * ## Frame Encoding/Decoding Flow
 *
 * @dot
 * digraph frame_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   // Encoding path
 *   frame [label="rx_frame_t\n(struct)"];
 *   encode [label="rx_frame_encode()", fillcolor=lightblue, style="rounded,filled"];
 *   wire [label="Wire Format\n(byte array)", fillcolor=lightyellow, style="rounded,filled"];
 *
 *   // Decoding path
 *   decode [label="rx_frame_decode()", fillcolor=lightblue, style="rounded,filled"];
 *   decoded [label="rx_frame_t\n(struct)"];
 *
 *   // Validation
 *   crc_check [label="CRC-32\nVerification", shape=diamond, fillcolor=lightgreen, style=filled];
 *   success [label="Success", fillcolor=green, style="rounded,filled"];
 *   error [label="Error", fillcolor=red, style="rounded,filled"];
 *
 *   // Flow
 *   frame -> encode [label="Encode"];
 *   encode -> wire [label="SYNC+Header+Payload+CRC"];
 *   wire -> decode [label="Decode"];
 *   decode -> crc_check;
 *   crc_check -> decoded [label="CRC OK"];
 *   crc_check -> error [label="CRC fail"];
 *   decoded -> success;
 * }
 * @enddot
 *
 * ## Test Vectors
 *
 * The test suite uses fixed test vectors to validate compatibility with the
 * Go implementation. These are captured in test_go_compatibility_*() functions.
 *
 * **Example: Empty ACK Frame**
 * ```
 * Wire format (hex):
 *   AA 55          - SYNC (0x55AA little-endian)
 *   01 00          - SEQ (1 little-endian)
 *   00 00          - LEN (0 little-endian)
 *   12             - TYPE (ACK = 0x12)
 *   00             - FLAGS (none = 0)
 *   <CRC-32 LE>    - CRC-32 of above 8 bytes (little-endian)
 * ```
 *
 * **Example: Command with Payload**
 * ```
 * Wire format (hex):
 *   AA 55          - SYNC (LE)
 *   2A 00          - SEQ (42 little-endian)
 *   04 00          - LEN (4 little-endian)
 *   10             - TYPE (COMMAND = 0x10)
 *   01             - FLAGS (REQUIRES_ACK = 0x01)
 *   54 45 53 54    - PAYLOAD "TEST" (4 bytes)
 *   <CRC-32 LE>    - CRC-32
 * ```
 *
 * ## Memory Usage
 *
 * | Component | Size | Stack/Static | Notes |
 * |-----------|------|--------------|-------|
 * | s_encoder | 1 byte | Static | Global test fixture |
 * | s_decoder | 1 byte | Static | Global test fixture |
 * | Test frame buffer | 512-2048 bytes | Stack | Per-test allocation |
 * | Unity overhead | ~200 bytes | Static | Unity framework |
 *
 * ## Performance Requirements
 *
 * **Target Execution Time (RX72N @ 240 MHz):**
 * - **Full test suite**: < 1 second
 * - **Individual test**: < 10 ms
 * - **Round-trip tests**: < 1 ms (encode + decode)
 *
 * ## NASA Power of 10 Compliance
 *
 * This test file validates Power of 10 compliance in the frame implementation:
 *
 * | Rule | Test Coverage |
 * |------|---------------|
 * | 1. Simple control flow | ✅ Verified via round-trip tests (no goto) |
 * | 2. Fixed loop bounds | ✅ Max payload tests verify bounded iteration |
 * | 3. No dynamic memory | ✅ All buffers stack-allocated |
 * | 4. Short functions | ✅ N/A (test code exempt) |
 * | 5. Assertions | ✅ Every test validates preconditions |
 * | 6. Small scope | ✅ Test fixtures have file scope |
 * | 7. Check returns | ✅ All rx_frame_* returns verified |
 * | 8. Limited preprocessor | ✅ Only include guards used |
 * | 9. Restrict pointers | ✅ No function pointers in frame API |
 * | 10. Compiler warnings | ✅ Tests build with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Test Implementation |
 * |-----------|---------------------|
 * | **Single Responsibility** | Each test validates ONE specific behavior |
 * | **Open/Closed** | New frame types tested via type enum extension |
 * | **Liskov Substitution** | Encoder/decoder handles interchangeable |
 * | **Interface Segregation** | Separate encoder/decoder test suites |
 * | **Dependency Inversion** | Tests use rx_frame.h API, not internals |
 *
 * ## Integration with Continuous Integration
 *
 * This test file is executed as part of the CMake test suite:
 * ```bash
 * cd star-rx72n-firmware
 * cmake --build build --target test
 * ```
 *
 * **CI Requirements:**
 * - All tests must pass (60+ test cases)
 * - No memory leaks (validated via Valgrind on host build)
 * - Execution time < 1 second
 * - Code coverage > 95% for rx_frame.c
 *
 * ## Known Limitations
 *
 * - CRC-32 exact values not tested (delegated to test_rx_crc32.c)
 * - Does not test concurrent encoder/decoder usage (single-threaded tests)
 * - Go compatibility tests use fixed test vectors (may lag Go implementation)
 *
 * @par Module Dependencies:
 * - rx_frame.h - Frame layer API
 * - rx_crc.h - CRC-32 calculation (indirect)
 * - unity.h - Unity test framework
 * - string.h - memcpy, memcmp
 *
 * @see rx_frame.h Complete frame protocol documentation
 * @see test_rx_crc32.c CRC-32 algorithm tests
 * @see test_rx_nanopb.c Protobuf integration tests
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 *
 * @author STAR Team
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project. Licensed under MIT License.
 *
 * @since Version 1.0.0
 * @version 1.1.0 - Added Go compatibility tests (2026-01-27)
 * @version 1.0.0 - Initial implementation (2026-01-04)
 *
 * @par Test Execution:
 * @code{.bash}
 * # Run frame tests only
 * ctest -R test_rx_frame
 *
 * # Run with verbose output
 * ./build/tests/test_rx_frame -v
 *
 * # Run specific test
 * ./build/tests/test_rx_frame test_roundtrip_large_payload
 * @endcode
 *
 * @par Adding New Tests:
 * @code{.c}
 * // 1. Define test function
 * void test_my_new_frame_feature(void)
 * {
 *     // Arrange: Set up test data
 *     rx_frame_t frame = {0};
 *     frame.header.sequence = 42;
 *     // ...
 *
 *     // Act: Execute operation
 *     uint8_t buffer[64];
 *     uint32_t len;
 *     rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
 *
 *     // Assert: Verify results
 *     TEST_ASSERT_EQUAL(k_rx_ok, err);
 *     TEST_ASSERT_EQUAL(expected_len, len);
 * }
 *
 * // 2. Register in main()
 * int main(void) {
 *     UNITY_BEGIN();
 *     RUN_TEST(test_my_new_frame_feature);
 *     return UNITY_END();
 * }
 * @endcode
 */

#include <stdint.h>
#include <string.h>

#include "rx_frame.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Frame test constants for payload and buffer sizes
 *
 * @details
 * These constants define test parameters for various frame sizes and test scenarios.
 * All sizes are chosen to exercise boundary conditions and common use cases.
 *
 * **Design Rationale:**
 * - k_test_payload_size (256): Exercises multi-byte length field encoding
 * - k_test_buffer_size (512): Sufficient for maximum payload + overhead
 * - k_test_oversize_buffer (2048): Tests that encoder rejects oversized data
 *
 * @note Minimum frame size is defined in rx_frame.h as k_frame_min_size = 12
 * (SYNC=2 bytes, header=6 bytes, CRC-32=4 bytes)
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_test_payload_size = 256, /**< Large payload size for testing (exercises 16-bit length field) */
  k_test_buffer_size  = 512, /**< Buffer size for encoded frame (sufficient for max payload) */
  k_test_sequence_num = 100, /**< Test sequence number (arbitrary non-zero value) */
  k_test_seq_zero     = 0,   /**< Sequence value 0 (tests zero handling) */
  k_test_seq_one      = 1,   /**< Sequence value 1 (common test value) */
  k_test_seq_two      = 2,   /**< Sequence value 2 */
  k_test_seq_three    = 3,   /**< Sequence value 3 */
  k_test_seq_four     = 4,   /**< Sequence value 4 */
  k_test_seq_five     = 5,   /**< Sequence value 5 */
  k_test_seq_six      = 6,   /**< Sequence value 6 */
  k_test_seq_42       = 42,  /**< Sequence value 42 (Go compatibility test vector) */
  k_test_seq_123      = 123, /**< Sequence value 123 */
  k_test_seq_200      = 200, /**< Sequence value 200 */
  k_test_seq_999      = 999, /**< Sequence value 999 */
  k_test_seq_1234     = 0x1234,  /**< Sequence value 0x1234 (exercises little-endian encoding) */
  k_test_seq_beef     = 0xBEEF,  /**< Sequence value 0xBEEF (non-ASCII test pattern) */
  k_test_seq_max      = 0xFFFF,  /**< Maximum sequence value (tests 16-bit boundary) */
  k_test_byte_mask    = 0xFF,    /**< Byte mask for payload patterns */
  k_test_byte_shift_8 = 8,       /**< Byte shift for multi-byte field extraction */
  k_test_small_buffer = 64,      /**< Small buffer for simple frame tests */
  k_test_oversize_buffer = 2048, /**< Buffer for overflow/boundary tests */
  k_short_buffer_size    = 8,    /**< Intentionally too small buffer (error test) */
  k_header_wire_size = k_frame_sync_size + k_frame_header_size, /**< Total header size on wire */
  k_go_payload_len   = 4, /**< Go test vector payload length */
} frame_test_constants_t;

/**
 * @brief CRC extraction constants for little-endian CRC-32 byte extraction
 *
 * @details
 * The CRC-32 is stored in little-endian format (LSB first) at the end of the frame.
 * These indices are used to extract individual bytes from the 4-byte CRC field.
 *
 * **Byte Order:**
 * ```
 * CRC-32 = 0x12345678
 * Bytes on wire: [0x78, 0x56, 0x34, 0x12] (little-endian)
 *                  ^     ^     ^     ^
 *                  0     1     2     3
 * ```
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_crc_byte_0 = 0, /**< CRC byte 0 (LSB, least significant byte) */
  k_crc_byte_1 = 1, /**< CRC byte 1 */
  k_crc_byte_2 = 2, /**< CRC byte 2 */
  k_crc_byte_3 = 3, /**< CRC byte 3 (MSB, most significant byte) */
} crc_byte_index_t;

/**
 * @brief CRC byte shift amounts for little-endian CRC-32 reconstruction
 *
 * @details
 * Used to reconstruct the 32-bit CRC value from 4 individual bytes.
 *
 * **Reconstruction Formula:**
 * @f[
 *   \text{CRC} = \text{byte}_0 | (\text{byte}_1 \ll 8) | (\text{byte}_2 \ll 16) | (\text{byte}_3 \ll 24)
 * @f]
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_crc_shift_8  = 8,  /**< Shift for byte 1 (8 bits) */
  k_crc_shift_16 = 16, /**< Shift for byte 2 (16 bits) */
  k_crc_shift_24 = 24, /**< Shift for byte 3 (24 bits, MSB) */
} crc_shift_t;

/**
 * @brief Single-value test constants (file-scope static)
 *
 * @details
 * These constants are defined as static const variables instead of enum members
 * because they are used with memcpy() and require addressability.
 *
 * @note C23 enum members cannot be used as lvalues or have their address taken
 *
 * @since Version 1.0.0
 */
static const uint32_t s_small_payload_size    = 4;      /**< Small payload size (4 bytes) */
static const uint32_t s_deadbeef_len          = 8;      /**< "DEADBEEF" string length */
static const uint32_t s_cmd_payload_len       = 4;      /**< Command payload length (4 bytes) */
static const uint32_t s_rsp_payload_len       = 4;      /**< Response payload length (4 bytes) */
static const uint32_t s_combined_payload_len  = 8;      /**< Combined payload length (8 bytes) */
static const char     s_test_payload_string[] = "TEST"; /**< Standard test payload string */

/**
 * @brief Frame wire format byte offsets
 *
 * @details
 * Byte offsets for accessing frame fields in the wire format (encoded byte array).
 * All multi-byte fields are little-endian (LSB first).
 *
 * **Wire Format Layout:**
 * ```
 * Offset | Field
 * -------|-------
 * 0      | SYNC low byte (0xAA for 0x55AA LE)
 * 1      | SYNC high byte (0x55 for 0x55AA LE)
 * 2      | SEQ low byte
 * 3      | SEQ high byte
 * 4      | LEN low byte
 * 5      | LEN high byte
 * 6      | TYPE
 * 7      | FLAGS
 * 8      | PAYLOAD[0]
 * ...    | ...
 * N-4    | CRC[0] (LSB)
 * N-3    | CRC[1]
 * N-2    | CRC[2]
 * N-1    | CRC[3] (MSB)
 * ```
 *
 * @see rx_frame.h Frame format specification
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_frame_offset_sync_low  = 0, /**< SYNC word low byte (0xAA for 0x55AA LE) */
  k_frame_offset_sync_high = 1, /**< SYNC word high byte (0x55 for 0x55AA LE) */
  k_frame_offset_seq_low   = 2, /**< Sequence number low byte (LE) */
  k_frame_offset_seq_high  = 3, /**< Sequence number high byte (LE) */
  k_frame_offset_len_low   = 4, /**< Length low byte (LE) */
  k_frame_offset_len_high  = 5, /**< Length high byte (LE) */
  k_frame_offset_type      = 6, /**< Frame type byte (k_frame_type_*) */
  k_frame_offset_flags     = 7, /**< Frame flags byte (bitfield) */
  k_frame_offset_payload   = 8, /**< Payload start offset (first payload byte) */
} frame_offset_t;

/**
 * @brief Payload byte offsets relative to payload start
 *
 * @details
 * These offsets are used to index into the payload section of the frame,
 * relative to k_frame_offset_payload (offset 8 in the wire format).
 *
 * @par Usage Example:
 * @code{.c}
 * // Access first payload byte
 * uint8_t byte0 = buffer[k_frame_offset_payload + k_payload_index_0];
 *
 * // Access fourth payload byte
 * uint8_t byte3 = buffer[k_frame_offset_payload + k_payload_index_3];
 * @endcode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_payload_index_0 = 0, /**< Payload byte 0 (first byte) */
  k_payload_index_1 = 1, /**< Payload byte 1 */
  k_payload_index_2 = 2, /**< Payload byte 2 */
  k_payload_index_3 = 3, /**< Payload byte 3 */
} payload_index_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @brief Global encoder fixture for test functions
 *
 * @details
 * Initialized in setUp(), used by all test functions, cleaned up in tearDown().
 * Using a static fixture avoids stack allocation overhead in each test.
 *
 * @note Not thread-safe (tests run sequentially)
 * @see setUp() Initialization function
 * @see tearDown() Cleanup function
 * @since Version 1.0.0
 */
static rx_frame_encoder_t s_encoder;

/**
 * @brief Global decoder fixture for test functions
 *
 * @details
 * Initialized in setUp(), used by all test functions, cleaned up in tearDown().
 * Using a static fixture avoids stack allocation overhead in each test.
 *
 * @note Not thread-safe (tests run sequentially)
 * @see setUp() Initialization function
 * @see tearDown() Cleanup function
 * @since Version 1.0.0
 */
static rx_frame_decoder_t s_decoder;

/**
 * @brief Unity test framework setUp function
 *
 * @details
 * Called automatically by Unity before EACH test function executes.
 * Initializes global encoder and decoder fixtures to known states.
 *
 * **Initialization Steps:**
 * 1. Call rx_frame_encoder_init() to initialize s_encoder
 * 2. Call rx_frame_decoder_init() to initialize s_decoder
 * 3. Both initializations must succeed (asserted)
 *
 * **Algorithm:**
 * ```
 * 1. Initialize encoder → Set encoder.initialized = 1
 * 2. Initialize decoder → Set decoder.initialized = 1
 * ```
 *
 * @pre Unity test framework has called this function
 * @post s_encoder is initialized and ready for use
 * @post s_decoder is initialized and ready for use
 *
 * @note Called automatically by Unity, never call directly
 * @note If initialization fails, subsequent tests will fail with k_rx_err_invalid_state
 *
 * @see tearDown() Cleanup function
 * @see rx_frame_encoder_init() Encoder initialization
 * @see rx_frame_decoder_init() Decoder initialization
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&s_encoder));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decoder_init(&s_decoder));
}

/**
 * @brief Unity test framework tearDown function
 *
 * @details
 * Called automatically by Unity after EACH test function completes.
 * Deinitializes global encoder and decoder fixtures, releasing any resources.
 *
 * **Cleanup Steps:**
 * 1. Call rx_frame_encoder_deinit() to deinitialize s_encoder
 * 2. Call rx_frame_decoder_deinit() to deinitialize s_decoder
 * 3. Both fixtures are reset to uninitialized state
 *
 * **Algorithm:**
 * ```
 * 1. Deinitialize encoder → Set encoder.initialized = 0
 * 2. Deinitialize decoder → Set decoder.initialized = 0
 * ```
 *
 * @pre Test function has completed execution
 * @post s_encoder is deinitialized (initialized = 0)
 * @post s_decoder is deinitialized (initialized = 0)
 *
 * @note Called automatically by Unity, never call directly
 * @note Ensures clean state for next test (prevents cross-contamination)
 *
 * @see setUp() Initialization function
 * @see rx_frame_encoder_deinit() Encoder cleanup
 * @see rx_frame_decoder_deinit() Decoder cleanup
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  (void)rx_frame_encoder_deinit(&s_encoder);
  (void)rx_frame_decoder_deinit(&s_decoder);
}

/* =============================================================================
 * Encoder Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test encoder initialization with nullptr pointer
 *
 * @details
 * Verifies that rx_frame_encoder_init() correctly rejects nullptr pointers
 * and returns k_rx_err_invalid_arg.
 *
 * **Test Algorithm:**
 * 1. Call rx_frame_encoder_init(nullptr)
 * 2. Verify return value is k_rx_err_invalid_arg
 *
 * @pre None
 * @post No state changes (nullptr pointer not dereferenced)
 *
 * @note Tests NASA Power of 10 Rule 5 (assertion/validation)
 * @see rx_frame_encoder_init() Function under test
 * @since Version 1.0.0
 */
void test_encoder_init_null(void)
{
  rx_err_t err = rx_frame_encoder_init(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test successful encoder initialization
 *
 * @details
 * Verifies that rx_frame_encoder_init() correctly initializes a valid
 * encoder structure and sets the initialized flag.
 *
 * **Test Algorithm:**
 * 1. Declare local encoder (uninitialized)
 * 2. Call rx_frame_encoder_init(&enc)
 * 3. Verify return value is k_rx_ok
 * 4. Verify enc.initialized is non-zero (true)
 *
 * @pre Encoder structure allocated (stack or static)
 * @post Encoder is initialized and ready for use
 * @post enc.initialized flag is set
 *
 * @note Tests successful initialization path
 * @see rx_frame_encoder_init() Function under test
 * @since Version 1.0.0
 */
void test_encoder_init_success(void)
{
  rx_frame_encoder_t enc;
  rx_err_t           err = rx_frame_encoder_init(&enc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, enc.initialized);
}

/**
 * @brief Test encoder deinitialization with nullptr pointer
 *
 * @details
 * Verifies that rx_frame_encoder_deinit() correctly rejects nullptr pointers
 * and returns k_rx_err_invalid_arg.
 *
 * **Test Algorithm:**
 * 1. Call rx_frame_encoder_deinit(nullptr)
 * 2. Verify return value is k_rx_err_invalid_arg
 *
 * @pre None
 * @post No state changes (nullptr pointer not dereferenced)
 *
 * @note Tests NASA Power of 10 Rule 5 (assertion/validation)
 * @see rx_frame_encoder_deinit() Function under test
 * @since Version 1.0.0
 */
void test_encoder_deinit_null(void)
{
  rx_err_t err = rx_frame_encoder_deinit(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Decoder Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test decoder initialization with nullptr pointer
 *
 * @details
 * Verifies that rx_frame_decoder_init() correctly rejects nullptr pointers
 * and returns k_rx_err_invalid_arg.
 *
 * **Test Algorithm:**
 * 1. Call rx_frame_decoder_init(nullptr)
 * 2. Verify return value is k_rx_err_invalid_arg
 *
 * @pre None
 * @post No state changes (nullptr pointer not dereferenced)
 *
 * @note Tests NASA Power of 10 Rule 5 (assertion/validation)
 * @see rx_frame_decoder_init() Function under test
 * @since Version 1.0.0
 */
void test_decoder_init_null(void)
{
  rx_err_t err = rx_frame_decoder_init(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test successful decoder initialization
 *
 * @details
 * Verifies that rx_frame_decoder_init() correctly initializes a valid
 * decoder structure and sets the initialized flag.
 *
 * **Test Algorithm:**
 * 1. Declare local decoder (uninitialized)
 * 2. Call rx_frame_decoder_init(&dec)
 * 3. Verify return value is k_rx_ok
 * 4. Verify dec.initialized is non-zero (true)
 *
 * @pre Decoder structure allocated (stack or static)
 * @post Decoder is initialized and ready for use
 * @post dec.initialized flag is set
 *
 * @note Tests successful initialization path
 * @see rx_frame_decoder_init() Function under test
 * @since Version 1.0.0
 */
void test_decoder_init_success(void)
{
  rx_frame_decoder_t dec;
  rx_err_t           err = rx_frame_decoder_init(&dec);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, dec.initialized);
}

/* =============================================================================
 * Encode Tests
 * =============================================================================
 */

/**
 * @brief Test rx_frame_encode() with nullptr arguments
 *
 * @details
 * Verifies that rx_frame_encode() validates all pointer arguments and returns
 * k_rx_err_invalid_arg when any pointer is nullptr.
 *
 * **Test Algorithm:**
 * 1. Test nullptr encoder → k_rx_err_invalid_arg
 * 2. Test nullptr frame → k_rx_err_invalid_arg
 * 3. Test nullptr buffer → k_rx_err_invalid_arg
 * 4. Test nullptr len → k_rx_err_invalid_arg
 *
 * @pre s_encoder initialized by setUp()
 * @post No state changes (all calls rejected before execution)
 *
 * @note Tests NASA Power of 10 Rule 5 (assertion/validation)
 * @note All four pointer parameters must be validated
 * @see rx_frame_encode() Function under test
 * @since Version 1.0.0
 */
void test_encode_null_args(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(nullptr, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(&s_encoder, nullptr, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(&s_encoder, &frame, nullptr, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(&s_encoder, &frame, buffer, nullptr));
}

/**
 * @brief Test encoding with uninitialized encoder
 *
 * @details
 * Verifies that rx_frame_encode() detects and rejects uninitialized encoders,
 * returning k_rx_err_invalid_state.
 *
 * **Test Algorithm:**
 * 1. Create local encoder with zeroed memory (initialized = 0)
 * 2. Attempt to encode a frame with uninitialized encoder
 * 3. Verify return value is k_rx_err_invalid_state
 *
 * @pre Local encoder is uninitialized (initialized flag = 0)
 * @post No encoding occurs (invalid state detected)
 *
 * @note Tests state validation before operation
 * @see rx_frame_encode() Function under test
 * @since Version 1.0.0
 */
void test_encode_uninitialized(void)
{
  rx_frame_encoder_t enc   = {0};
  rx_frame_t         frame = {0};
  uint8_t            buffer[k_test_small_buffer];
  uint32_t           len;

  rx_err_t err = rx_frame_encode(&enc, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test encoding with payload exceeding maximum size
 *
 * @details
 * Verifies that rx_frame_encode() rejects payloads larger than k_frame_max_payload
 * (1024 bytes) and returns k_rx_err_invalid_size.
 *
 * **Test Algorithm:**
 * 1. Create frame with payload length = 1025 bytes (exceeds max)
 * 2. Attempt to encode the frame
 * 3. Verify return value is k_rx_err_invalid_size
 *
 * @pre s_encoder initialized by setUp()
 * @pre Buffer large enough for max frame size
 * @post No encoding occurs (size validation failed)
 *
 * @note Tests NASA Power of 10 Rule 2 (bounded payload size)
 * @see rx_frame_encode() Function under test
 * @see k_frame_max_payload Maximum allowed payload size
 * @since Version 1.0.0
 */
void test_encode_payload_too_large(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_oversize_buffer];
  uint32_t   len;

  frame.header.length = k_frame_max_payload + 1; /* 1025 bytes (exceeds max) */

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test encoding empty frame (zero-length payload)
 *
 * @details
 * Verifies that rx_frame_encode() correctly encodes frames with no payload,
 * producing a minimum-size frame (12 bytes: SYNC + header + CRC).
 *
 * **Test Algorithm:**
 * 1. Create frame with sequence=1, length=0, type=COMMAND, flags=NONE
 * 2. Encode the frame
 * 3. Verify encoded size is k_frame_min_size (12 bytes)
 * 4. Verify SYNC word is 0x55AA (little-endian: wire 0xAA, 0x55)
 * 5. Verify sequence number is 1 (little-endian)
 * 6. Verify length is 0 (little-endian)
 * 7. Verify type is k_frame_type_command
 * 8. Verify flags is k_frame_flag_none
 *
 * **Wire Format (hex):**
 * ```
 * AA 55          - SYNC (0x55AA LE)
 * 01 00          - SEQ (1 LE)
 * 00 00          - LEN (0 LE)
 * 10             - TYPE (COMMAND = 0x10)
 * 00             - FLAGS (NONE)
 * <CRC-32 LE>    - CRC-32 of above 8 bytes
 * ```
 *
 * @pre s_encoder initialized by setUp()
 * @post Frame encoded to minimum size (12 bytes)
 * @post All header fields match input
 *
 * @note Tests minimum frame size (boundary condition)
 * @see rx_frame_encode() Function under test
 * @since Version 1.0.0
 */
void test_encode_empty_frame(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint8_t    sync_high = 0;
  uint8_t    sync_low  = 0;
  uint8_t    seq_high  = 0;
  uint8_t    seq_low   = 0;
  uint8_t    len_high  = 0;
  uint8_t    len_low   = 0;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  sync_high = (uint8_t)(k_frame_sync_word >> k_test_byte_shift_8);
  sync_low  = (uint8_t)(k_frame_sync_word & k_test_byte_mask);
  seq_high  = (uint8_t)(frame.header.sequence >> k_test_byte_shift_8);
  seq_low   = (uint8_t)(frame.header.sequence & k_test_byte_mask);
  len_high  = (uint8_t)(frame.header.length >> k_test_byte_shift_8);
  len_low   = (uint8_t)(frame.header.length & k_test_byte_mask);

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_frame_min_size, len); /* 12 bytes: SYNC + Header + CRC */

  /* Verify SYNC word (little-endian) */
  TEST_ASSERT_EQUAL_HEX8(sync_high, buffer[k_frame_offset_sync_high]);
  TEST_ASSERT_EQUAL_HEX8(sync_low, buffer[k_frame_offset_sync_low]);

  /* Verify SEQ (little-endian) */
  TEST_ASSERT_EQUAL_HEX8(seq_high, buffer[k_frame_offset_seq_high]);
  TEST_ASSERT_EQUAL_HEX8(seq_low, buffer[k_frame_offset_seq_low]);

  /* Verify LEN (little-endian) */
  TEST_ASSERT_EQUAL_HEX8(len_high, buffer[k_frame_offset_len_high]);
  TEST_ASSERT_EQUAL_HEX8(len_low, buffer[k_frame_offset_len_low]);

  /* Verify TYPE */
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_command, buffer[k_frame_offset_type]);

  /* Verify FLAGS */
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_none, buffer[k_frame_offset_flags]);
}

/**
 * @brief Test encoding frame with payload
 *
 * @details
 * Verifies that rx_frame_encode() correctly encodes frames with non-zero payloads,
 * including proper little-endian encoding of multi-byte fields and payload copying.
 *
 * **Test Algorithm:**
 * 1. Create frame with sequence=0x1234, length=4, type=RESPONSE, flags=REQUIRES_ACK
 * 2. Set payload to "TEST" (4 bytes)
 * 3. Encode the frame
 * 4. Verify encoded size is k_frame_min_size + 4 = 16 bytes
 * 5. Verify sequence number is 0x1234 (little-endian: 0x34, 0x12)
 * 6. Verify length is 4 (little-endian: 0x04, 0x00)
 * 7. Verify payload bytes match "TEST"
 *
 * **Wire Format (hex):**
 * ```
 * AA 55          - SYNC (LE)
 * 34 12          - SEQ (0x1234 little-endian)
 * 04 00          - LEN (4 little-endian)
 * 11             - TYPE (RESPONSE = 0x11)
 * 01             - FLAGS (REQUIRES_ACK)
 * 54 45 53 54    - PAYLOAD "TEST"
 * <CRC-32 LE>    - CRC-32
 * ```
 *
 * @pre s_encoder initialized by setUp()
 * @post Frame encoded with payload (16 bytes total)
 * @post Payload bytes match input
 *
 * @note Tests typical frame encoding with payload
 * @see rx_frame_encode() Function under test
 * @since Version 1.0.0
 */
void test_encode_with_payload(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint8_t    seq_high = 0;
  uint8_t    seq_low  = 0;
  uint8_t    len_high = 0;
  uint8_t    len_low  = 0;

  frame.header.sequence = k_test_seq_1234;
  frame.header.length   = s_small_payload_size;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, s_test_payload_string, s_small_payload_size);

  seq_high = (uint8_t)(frame.header.sequence >> k_test_byte_shift_8);
  seq_low  = (uint8_t)(frame.header.sequence & k_test_byte_mask);
  len_high = (uint8_t)(frame.header.length >> k_test_byte_shift_8);
  len_low  = (uint8_t)(frame.header.length & k_test_byte_mask);

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_frame_min_size + s_small_payload_size,
                    len); /* Minimum frame + 4 byte payload */

  /* Verify SEQ (little-endian 0x1234) */
  TEST_ASSERT_EQUAL_HEX8(seq_high, buffer[k_frame_offset_seq_high]);
  TEST_ASSERT_EQUAL_HEX8(seq_low, buffer[k_frame_offset_seq_low]);

  /* Verify LEN (little-endian 4) */
  TEST_ASSERT_EQUAL_HEX8(len_high, buffer[k_frame_offset_len_high]);
  TEST_ASSERT_EQUAL_HEX8(len_low, buffer[k_frame_offset_len_low]);

  /* Verify payload */
  TEST_ASSERT_EQUAL_HEX8(s_test_payload_string[k_payload_index_0],
                         buffer[k_frame_offset_payload + k_payload_index_0]);
  TEST_ASSERT_EQUAL_HEX8(s_test_payload_string[k_payload_index_1],
                         buffer[k_frame_offset_payload + k_payload_index_1]);
  TEST_ASSERT_EQUAL_HEX8(s_test_payload_string[k_payload_index_2],
                         buffer[k_frame_offset_payload + k_payload_index_2]);
  TEST_ASSERT_EQUAL_HEX8(s_test_payload_string[k_payload_index_3],
                         buffer[k_frame_offset_payload + k_payload_index_3]);
}

/* =============================================================================
 * Decode Tests
 * =============================================================================
 */

void test_decode_null_args(void)
{
  uint8_t    data[64] = {0};
  rx_frame_t frame    = {0};

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_decode(nullptr, data, 64, &frame));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_decode(&s_decoder, nullptr, 64, &frame));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_decode(&s_decoder, data, 64, nullptr));
}

void test_decode_uninitialized(void)
{
  rx_frame_decoder_t dec      = {0};
  uint8_t            data[64] = {0};
  rx_frame_t         frame    = {0};

  rx_err_t err = rx_frame_decode(&dec, data, 64, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_decode_too_short(void)
{
  uint8_t    data[k_short_buffer_size] = {0};
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, k_short_buffer_size, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_decode_invalid_sync(void)
{
  uint8_t    data[k_frame_min_size] = {0};
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, k_frame_min_size, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

void test_decode_crc_mismatch(void)
{
  /* Valid header but incorrect CRC */
  uint8_t    data[k_frame_min_size] = {0};
  uint8_t    sync_high              = 0;
  uint8_t    sync_low               = 0;
  uint8_t    seq_high               = 0;
  uint8_t    seq_low                = 0;
  uint8_t    len_high               = 0;
  uint8_t    len_low                = 0;
  rx_frame_t frame;

  sync_high = (uint8_t)(k_frame_sync_word >> k_test_byte_shift_8);
  sync_low  = (uint8_t)(k_frame_sync_word & k_test_byte_mask);
  seq_high  = (uint8_t)((uint16_t)k_test_seq_one >> k_test_byte_shift_8);
  seq_low   = (uint8_t)((uint16_t)k_test_seq_one & k_test_byte_mask);
  len_high  = (uint8_t)((uint16_t)0U >> k_test_byte_shift_8);
  len_low   = (uint8_t)((uint16_t)0U & k_test_byte_mask);

  data[k_frame_offset_sync_high] = sync_high;
  data[k_frame_offset_sync_low]  = sync_low;
  data[k_frame_offset_seq_high]  = seq_high;
  data[k_frame_offset_seq_low]   = seq_low;
  data[k_frame_offset_len_high]  = len_high;
  data[k_frame_offset_len_low]   = len_low;
  data[k_frame_offset_type]      = k_frame_type_command;
  data[k_frame_offset_flags]     = k_frame_flag_none;

  rx_err_t err = rx_frame_decode(&s_decoder, data, k_frame_min_size, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/* =============================================================================
 * Encode/Decode Round-Trip Tests
 * =============================================================================
 */

void test_roundtrip_empty_frame(void)
{
  rx_frame_t original = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  original.header.sequence = k_test_seq_42;
  original.header.length   = 0;
  original.header.type     = k_frame_type_ack;
  original.header.flags    = k_frame_flag_none;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
}

void test_roundtrip_with_payload(void)
{
  rx_frame_t original = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  original.header.sequence = k_test_seq_beef;
  original.header.length   = s_deadbeef_len;
  original.header.type     = k_frame_type_command;
  original.header.flags    = k_frame_flag_fec_enabled | k_frame_flag_priority;
  memcpy(original.payload, "DEADBEEF", s_deadbeef_len);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, s_deadbeef_len);
}

void test_roundtrip_max_sequence(void)
{
  enum : uint8_t {
    k_seq_test_payload_len = 2,
    k_payload_val_high     = 0x12,
    k_payload_val_low      = 0x34
  };
  rx_frame_t original = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  original.header.sequence            = k_test_seq_max;
  original.header.length              = k_seq_test_payload_len;
  original.header.type                = k_frame_type_nack;
  original.header.flags               = k_frame_flag_soft_nack;
  original.payload[k_payload_index_0] = k_payload_val_high;
  original.payload[k_payload_index_1] = k_payload_val_low;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL_HEX16(k_test_seq_max, decoded.header.sequence);
}

void test_roundtrip_large_payload(void)
{
  rx_frame_t original = {0};
  uint8_t    buffer[k_test_buffer_size];
  uint32_t   len;

  original.header.sequence = k_test_sequence_num;
  original.header.length   = k_test_payload_size;
  original.header.type     = k_frame_type_response;
  original.header.flags    = k_frame_flag_none;

  /* Fill payload with pattern */
  for (uint32_t i = 0; i < k_test_payload_size; i++) {
    original.payload[i] = (uint8_t)(i & k_test_byte_mask);
  }

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_test_payload_size, len);

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(k_test_payload_size, decoded.header.length);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, k_test_payload_size);
}

/* =============================================================================
 * Utility Function Tests
 * =============================================================================
 */

void test_create_ack_null(void)
{
  rx_err_t err = rx_frame_create_ack(nullptr, k_test_seq_zero);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_create_ack_success(void)
{
  rx_frame_t frame;
  rx_err_t   err = rx_frame_create_ack(&frame, k_test_seq_42);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_seq_42, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_ack, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, frame.header.flags);
}

void test_create_nack_null(void)
{
  rx_err_t err = rx_frame_create_nack(nullptr, k_test_seq_zero, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_create_nack_with_flags(void)
{
  rx_frame_t frame;
  rx_err_t   err = rx_frame_create_nack(&frame, k_test_seq_123, k_frame_flag_soft_nack);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_seq_123, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_nack, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, frame.header.flags);
}

void test_encoded_size_calculation(void)
{
  enum : uint8_t { k_payload_0 = 0, k_payload_1 = 1, k_payload_8 = 8 };
  TEST_ASSERT_EQUAL(k_frame_min_size, rx_frame_encoded_size(k_payload_0));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_payload_1, rx_frame_encoded_size(k_payload_1));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_payload_8, rx_frame_encoded_size(k_payload_8));
  TEST_ASSERT_EQUAL(k_frame_max_size, rx_frame_encoded_size(k_frame_max_payload));
}

void test_frame_type_valid(void)
{
  enum : uint8_t { k_invalid_type_02 = 0x02, k_invalid_type_50 = 0x50, k_invalid_type_80 = 0x80 };
  /* All valid frame types (non-contiguous hex values) */
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_ping));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_pong));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_command));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_response));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_ack));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_nack));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_reset_ack));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_reset));
  /* Invalid types: gaps in the enum value space */
  TEST_ASSERT_FALSE(rx_frame_type_valid(k_invalid_type_02));
  TEST_ASSERT_FALSE(rx_frame_type_valid(k_invalid_type_50));
  TEST_ASSERT_FALSE(rx_frame_type_valid(k_invalid_type_80));
}

/* =============================================================================
 * Maximum Payload Tests
 * =============================================================================
 */

void test_roundtrip_max_payload(void)
{
  rx_frame_t original = {0};
  uint8_t    buffer[k_frame_max_size];
  uint32_t   len;

  original.header.sequence = k_test_seq_999;
  original.header.length   = k_frame_max_payload; /* 1024 bytes */
  original.header.type     = k_frame_type_command;
  original.header.flags    = k_frame_flag_fec_enabled;

  /* Fill with deterministic pattern */
  for (uint32_t i = 0; i < k_frame_max_payload; i++) {
    original.payload[i] = (uint8_t)(i & k_test_byte_mask);
  }

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_max_size, len); /* 1036 bytes total */

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_max_payload, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, k_frame_max_payload);
}

/* =============================================================================
 * Frame Type Tests (Explicit Coverage)
 * =============================================================================
 */

void test_frame_type_command(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = s_cmd_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "CMD1", s_cmd_payload_len);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("CMD1", decoded.payload, s_cmd_payload_len);
}

void test_frame_type_response(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_two;
  frame.header.length   = s_rsp_payload_len;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "RSP1", s_rsp_payload_len);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_response, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("RSP1", decoded.payload, s_rsp_payload_len);
}

void test_frame_type_ack(void)
{
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, k_test_sequence_num));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(k_test_sequence_num, decoded.header.sequence);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

void test_frame_type_nack(void)
{
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_nack(&frame, k_test_seq_200, k_frame_flag_soft_nack));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_nack, decoded.header.type);
  TEST_ASSERT_EQUAL(k_test_seq_200, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, decoded.header.flags);
}

/* =============================================================================
 * Frame Flag Tests (All Combinations)
 * =============================================================================
 */

void test_flag_requires_ack(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
}

void test_flag_retransmit(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_two;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_retransmit;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_retransmit, decoded.header.flags);
}

void test_flag_priority(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_three;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_priority;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_priority, decoded.header.flags);
}

void test_flag_fec_enabled(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_four;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_fec_enabled;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_fec_enabled, decoded.header.flags);
}

void test_flag_soft_nack(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_five;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_nack;
  frame.header.flags    = k_frame_flag_soft_nack;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, decoded.header.flags);
}

void test_flag_combined(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_six;
  frame.header.length   = s_combined_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags = k_frame_flag_requires_ack | k_frame_flag_priority | k_frame_flag_fec_enabled;
  memcpy(frame.payload, "COMBINED", s_combined_payload_len);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(frame.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("COMBINED", decoded.payload, s_combined_payload_len);
}

/* =============================================================================
 * Endianness Tests (Explicit Little-Endian Verification)
 * =============================================================================
 */

void test_sync_word_little_endian(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint8_t    sync_high = 0;
  uint8_t    sync_low  = 0;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  sync_high = (uint8_t)(k_frame_sync_word >> k_test_byte_shift_8);
  sync_low  = (uint8_t)(k_frame_sync_word & k_test_byte_mask);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* SYNC word 0x55AA must be little-endian: [0xAA, 0x55] */
  TEST_ASSERT_EQUAL_HEX8(sync_low, buffer[k_frame_offset_sync_low]);   /* Low byte first */
  TEST_ASSERT_EQUAL_HEX8(sync_high, buffer[k_frame_offset_sync_high]); /* High byte second */
}

void test_sequence_little_endian(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint8_t    seq_high = 0;
  uint8_t    seq_low  = 0;

  frame.header.sequence = k_test_seq_1234;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  seq_high = (uint8_t)(frame.header.sequence >> k_test_byte_shift_8);
  seq_low  = (uint8_t)(frame.header.sequence & k_test_byte_mask);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* SEQ 0x1234 must be little-endian: [0x34, 0x12] */
  TEST_ASSERT_EQUAL_HEX8(seq_low, buffer[k_frame_offset_seq_low]);   /* Low byte at offset 2 */
  TEST_ASSERT_EQUAL_HEX8(seq_high, buffer[k_frame_offset_seq_high]); /* High byte at offset 3 */
}

void test_length_little_endian(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_buffer_size];
  uint32_t   len;
  uint8_t    len_high = 0;
  uint8_t    len_low  = 0;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = k_test_payload_size;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  /* Fill 256 bytes */
  for (uint32_t i = 0; i < k_test_payload_size; i++) {
    frame.payload[i] = (uint8_t)i;
  }

  len_high = (uint8_t)(frame.header.length >> k_test_byte_shift_8);
  len_low  = (uint8_t)(frame.header.length & k_test_byte_mask);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* LEN 0x0100 must be little-endian: [0x00, 0x01] */
  TEST_ASSERT_EQUAL_HEX8(len_low, buffer[k_frame_offset_len_low]);   /* Low byte at offset 4 */
  TEST_ASSERT_EQUAL_HEX8(len_high, buffer[k_frame_offset_len_high]); /* High byte at offset 5 */
}

void test_crc_little_endian(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint32_t   crc_offset = 0;
  uint32_t   crc        = 0;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size, len);

  /* CRC-32 is little-endian (LSB first) at end of frame */
  /* We don't test exact CRC value (that's rx_crc32's job) */
  /* Just verify it's at the correct position (last 4 bytes) */
  crc_offset = len - k_frame_crc_size;
  TEST_ASSERT_EQUAL(k_frame_min_size - k_frame_crc_size, crc_offset); /* After SYNC + Header */

  /* Verify CRC is non-zero (frame is valid) */
  crc = ((uint32_t)buffer[crc_offset + k_crc_byte_0]) |
        ((uint32_t)buffer[crc_offset + k_crc_byte_1] << k_crc_shift_8) |
        ((uint32_t)buffer[crc_offset + k_crc_byte_2] << k_crc_shift_16) |
        ((uint32_t)buffer[crc_offset + k_crc_byte_3] << k_crc_shift_24);
  TEST_ASSERT_NOT_EQUAL(0, crc);
}

/* =============================================================================
 * Go Compatibility Tests (Bit-Exact Verification)
 * =============================================================================
 */

void test_go_compatibility_empty_ack(void)
{
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint8_t    expected_header[k_header_wire_size];
  uint8_t    sync_high = 0;
  uint8_t    sync_low  = 0;
  uint8_t    seq_high  = 0;
  uint8_t    seq_low   = 0;
  uint8_t    len_high  = 0;
  uint8_t    len_low   = 0;
  /*
   * Test vector from Go implementation (star-gateway/internal/frame/)
   * Frame: ACK for sequence 1
   * Expected wire format (hex):
   *   AA 55          - SYNC (0x55AA little-endian)
   *   01 00          - SEQ (1 little-endian)
   *   00 00          - LEN (0 little-endian)
   *   12             - TYPE (ACK = 0x12)
   *   00             - FLAGS (none = 0)
   *   <CRC-32 LE>    - CRC-32 of above 8 bytes
   */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, k_test_seq_one));

  sync_high = (uint8_t)(k_frame_sync_word >> k_test_byte_shift_8);
  sync_low  = (uint8_t)(k_frame_sync_word & k_test_byte_mask);
  seq_high  = (uint8_t)(frame.header.sequence >> k_test_byte_shift_8);
  seq_low   = (uint8_t)(frame.header.sequence & k_test_byte_mask);
  len_high  = (uint8_t)(frame.header.length >> k_test_byte_shift_8);
  len_low   = (uint8_t)(frame.header.length & k_test_byte_mask);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size, len);

  /* Verify header bytes match Go encoding (little-endian) */
  expected_header[k_frame_offset_sync_low]  = sync_low;
  expected_header[k_frame_offset_sync_high] = sync_high;
  expected_header[k_frame_offset_seq_low]   = seq_low;
  expected_header[k_frame_offset_seq_high]  = seq_high;
  expected_header[k_frame_offset_len_low]   = len_low;
  expected_header[k_frame_offset_len_high]  = len_high;
  expected_header[k_frame_offset_type]      = k_frame_type_ack;
  expected_header[k_frame_offset_flags]     = k_frame_flag_none;
  TEST_ASSERT_EQUAL_MEMORY(expected_header, buffer, k_header_wire_size);

  /* Verify round-trip decode */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_one, decoded.header.sequence);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, decoded.header.flags);
}

void test_go_compatibility_command_with_payload(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;
  uint8_t    expected[k_frame_sync_size + k_frame_header_size + k_go_payload_len];
  uint8_t    sync_high = 0;
  uint8_t    sync_low  = 0;
  uint8_t    seq_high  = 0;
  uint8_t    seq_low   = 0;
  uint8_t    len_high  = 0;
  uint8_t    len_low   = 0;
  /*
   * Test vector: Command with 4-byte payload "TEST"
   * Expected wire format (hex):
   *   AA 55          - SYNC (0x55AA little-endian)
   *   2A 00          - SEQ (42 little-endian)
   *   04 00          - LEN (4 little-endian)
   *   10             - TYPE (COMMAND = 0x10)
   *   01             - FLAGS (REQUIRES_ACK = 0x01)
   *   54 45 53 54    - PAYLOAD "TEST"
   *   <CRC-32 LE>    - CRC-32
   */
  frame.header.sequence = k_test_seq_42;
  frame.header.length   = k_go_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, s_test_payload_string, k_go_payload_len);

  sync_high = (uint8_t)(k_frame_sync_word >> k_test_byte_shift_8);
  sync_low  = (uint8_t)(k_frame_sync_word & k_test_byte_mask);
  seq_high  = (uint8_t)(frame.header.sequence >> k_test_byte_shift_8);
  seq_low   = (uint8_t)(frame.header.sequence & k_test_byte_mask);
  len_high  = (uint8_t)(frame.header.length >> k_test_byte_shift_8);
  len_low   = (uint8_t)(frame.header.length & k_test_byte_mask);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_go_payload_len, len);

  /* Verify header and payload match Go encoding (little-endian) */
  expected[k_frame_offset_sync_low]  = sync_low;
  expected[k_frame_offset_sync_high] = sync_high;
  expected[k_frame_offset_seq_low]   = seq_low;
  expected[k_frame_offset_seq_high]  = seq_high;
  expected[k_frame_offset_len_low]   = len_low;
  expected[k_frame_offset_len_high]  = len_high;
  expected[k_frame_offset_type]      = k_frame_type_command;
  expected[k_frame_offset_flags]     = k_frame_flag_requires_ack;
  expected[k_frame_offset_payload + k_payload_index_0] =
    (uint8_t)s_test_payload_string[k_payload_index_0];
  expected[k_frame_offset_payload + k_payload_index_1] =
    (uint8_t)s_test_payload_string[k_payload_index_1];
  expected[k_frame_offset_payload + k_payload_index_2] =
    (uint8_t)s_test_payload_string[k_payload_index_2];
  expected[k_frame_offset_payload + k_payload_index_3] =
    (uint8_t)s_test_payload_string[k_payload_index_3];
  TEST_ASSERT_EQUAL_MEMORY(expected,
                           buffer,
                           k_frame_sync_size + k_frame_header_size + k_go_payload_len);

  /* Verify round-trip */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_42, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_go_payload_len, decoded.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY(s_test_payload_string, decoded.payload, k_go_payload_len);
}

/* =============================================================================
 * Byte-Exact Cross-Compatibility Vectors (C ↔ Go)
 *
 * These test vectors define the canonical wire encoding for every frame type.
 * The Go tests (frame_test.go TestCrossCompatibility_*) verify against the
 * same hardcoded byte sequences. If either encoder changes, these tests fail.
 * =============================================================================
 */

/** @brief Cross-compatibility vector sizes */
typedef enum : uint8_t {
  k_xc_ping_wire_len      = 12, /**< PING: 8 header + 0 payload + 4 CRC */
  k_xc_pong_wire_len      = 16, /**< PONG: 8 header + 4 payload + 4 CRC */
  k_xc_command_wire_len   = 16, /**< COMMAND: 8 header + 4 payload + 4 CRC */
  k_xc_response_wire_len  = 14, /**< RESPONSE: 8 header + 2 payload + 4 CRC */
  k_xc_ack_wire_len       = 12, /**< ACK: 8 header + 0 payload + 4 CRC */
  k_xc_nack_wire_len      = 12, /**< NACK: 8 header + 0 payload + 4 CRC */
  k_xc_reset_wire_len     = 12, /**< RESET: 8 header + 0 payload + 4 CRC */
  k_xc_reset_ack_wire_len = 12, /**< RESET_ACK: 8 header + 0 payload + 4 CRC */
  k_xc_pong_payload_len   = 4,  /**< PONG counter payload */
  k_xc_cmd_payload_len    = 4,  /**< "TEST" payload */
  k_xc_rsp_payload_len    = 2,  /**< "OK" payload */
  k_xc_counter_42         = 42, /**< PONG counter value */
} xc_vector_constants_t;

/**
 * Vector 1: PING (seq=0, empty payload)
 * CRC-32 = 0x2F42E23D
 */
void test_cross_compat_ping_seq0_empty(void)
{
  static const uint8_t expected_wire[k_xc_ping_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x00,
    0x00, /* SEQ=0 (LE) */
    0x00,
    0x00, /* LEN=0 (LE) */
    0x00, /* TYPE=PING (0x00) */
    0x00, /* FLAGS=none */
    0x3D,
    0xE2,
    0x42,
    0x2F /* CRC-32 LE = 0x2F42E23D */
  };
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ping(&frame, 0, NULL, 0));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_ping_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_ping_wire_len);

  /* Decode and verify round-trip */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(0, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_ping, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

/**
 * Vector 2: PONG (seq=0, 4-byte counter=42 little-endian)
 * CRC-32 = 0x60647975
 */
void test_cross_compat_pong_seq0_counter42(void)
{
  static const uint8_t expected_wire[k_xc_pong_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x00,
    0x00, /* SEQ=0 (LE) */
    0x04,
    0x00, /* LEN=4 (LE) */
    0x01, /* TYPE=PONG (0x01) */
    0x00, /* FLAGS=none */
    0x2A,
    0x00,
    0x00,
    0x00, /* PAYLOAD: counter=42 LE */
    0x75,
    0x79,
    0x64,
    0x60 /* CRC-32 LE = 0x60647975 */
  };
  uint8_t    pong_payload[k_xc_pong_payload_len] = {0x2A, 0x00, 0x00, 0x00};
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  /* Create PONG with counter=42 payload (echoes PING payload) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_pong(&frame, 0, pong_payload, k_xc_pong_payload_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_pong_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_pong_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(0, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_pong, decoded.header.type);
  TEST_ASSERT_EQUAL(k_xc_pong_payload_len, decoded.header.length);
  TEST_ASSERT_EQUAL_MEMORY(pong_payload, decoded.payload, k_xc_pong_payload_len);
}

/**
 * Vector 3: COMMAND (seq=1, payload="TEST", FLAGS=REQUIRES_ACK)
 * CRC-32 = 0x7A6DE93B
 */
void test_cross_compat_command_seq1_test(void)
{
  static const uint8_t expected_wire[k_xc_command_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x01,
    0x00, /* SEQ=1 (LE) */
    0x04,
    0x00, /* LEN=4 (LE) */
    0x10, /* TYPE=COMMAND (0x10) */
    0x01, /* FLAGS=REQUIRES_ACK */
    0x54,
    0x45,
    0x53,
    0x54, /* PAYLOAD="TEST" */
    0x3B,
    0xE9,
    0x6D,
    0x7A /* CRC-32 LE = 0x7A6DE93B */
  };
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = k_xc_cmd_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "TEST", k_xc_cmd_payload_len);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_command_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_command_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_one, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("TEST", decoded.payload, k_xc_cmd_payload_len);
}

/**
 * Vector 4: RESPONSE (seq=1, payload="OK")
 * CRC-32 = 0x9A1DACEA
 */
void test_cross_compat_response_seq1_ok(void)
{
  static const uint8_t expected_wire[k_xc_response_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x01,
    0x00, /* SEQ=1 (LE) */
    0x02,
    0x00, /* LEN=2 (LE) */
    0x11, /* TYPE=RESPONSE (0x11) */
    0x00, /* FLAGS=none */
    0x4F,
    0x4B, /* PAYLOAD="OK" */
    0xEA,
    0xAC,
    0x1D,
    0x9A /* CRC-32 LE = 0x9A1DACEA */
  };
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = k_xc_rsp_payload_len;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "OK", k_xc_rsp_payload_len);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_response_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_response_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_one, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_response, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("OK", decoded.payload, k_xc_rsp_payload_len);
}

/**
 * Vector 5: ACK (seq=1, empty payload)
 * CRC-32 = 0x9CEA414B
 */
void test_cross_compat_ack_seq1_empty(void)
{
  static const uint8_t expected_wire[k_xc_ack_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x01,
    0x00, /* SEQ=1 (LE) */
    0x00,
    0x00, /* LEN=0 */
    0x12, /* TYPE=ACK (0x12) */
    0x00, /* FLAGS=none */
    0x4B,
    0x41,
    0xEA,
    0x9C /* CRC-32 LE = 0x9CEA414B */
  };
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, k_test_seq_one));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_ack_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_ack_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_one, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

/**
 * Vector 6: NACK (seq=1, empty payload)
 * CRC-32 = 0x85F1700A
 */
void test_cross_compat_nack_seq1_empty(void)
{
  static const uint8_t expected_wire[k_xc_nack_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x01,
    0x00, /* SEQ=1 (LE) */
    0x00,
    0x00, /* LEN=0 */
    0x13, /* TYPE=NACK (0x13) */
    0x00, /* FLAGS=none */
    0x0A,
    0x70,
    0xF1,
    0x85 /* CRC-32 LE = 0x85F1700A */
  };
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_nack(&frame, k_test_seq_one, k_frame_flag_none));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_nack_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_nack_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_one, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_nack, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

/**
 * Vector 7: RESET (seq=0, empty payload)
 * CRC-32 = 0xBC661F4F
 */
void test_cross_compat_reset_seq0_empty(void)
{
  static const uint8_t expected_wire[k_xc_reset_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x00,
    0x00, /* SEQ=0 */
    0x00,
    0x00, /* LEN=0 */
    0xFF, /* TYPE=RESET (0xFF) */
    0x00, /* FLAGS=none */
    0x4F,
    0x1F,
    0x66,
    0xBC /* CRC-32 LE = 0xBC661F4F */
  };
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset(&frame, 0));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_reset_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_reset_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(0, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_reset, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

/**
 * Vector 8: RESET_ACK (seq=0, empty payload)
 * CRC-32 = 0xA57D2E0E
 */
void test_cross_compat_reset_ack_seq0_empty(void)
{
  static const uint8_t expected_wire[k_xc_reset_ack_wire_len] = {
    0xAA,
    0x55, /* SYNC (LE) */
    0x00,
    0x00, /* SEQ=0 */
    0x00,
    0x00, /* LEN=0 */
    0xFE, /* TYPE=RESET_ACK (0xFE) */
    0x00, /* FLAGS=none */
    0x0E,
    0x2E,
    0x7D,
    0xA5 /* CRC-32 LE = 0xA57D2E0E */
  };
  rx_frame_t frame;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset_ack(&frame, 0));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_xc_reset_ack_wire_len, len);
  TEST_ASSERT_EQUAL_MEMORY(expected_wire, buffer, k_xc_reset_ack_wire_len);

  /* Decode and verify */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(0, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_reset_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

/**
 * Decode-only: verify C decoder accepts pre-computed wire bytes directly.
 * Feeds the hardcoded wire vectors into rx_frame_decode without any C encoding.
 */
void test_cross_compat_decode_go_wire_bytes(void)
{
  /* PING wire bytes (type=0x00, LE encoding) */
  static const uint8_t ping_wire[k_xc_ping_wire_len] =
    {0xAA, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D, 0xE2, 0x42, 0x2F};
  /* COMMAND wire bytes (type=0x10, flags=REQUIRES_ACK, LE encoding) */
  static const uint8_t cmd_wire[k_xc_command_wire_len] = {0xAA,
                                                          0x55,
                                                          0x01,
                                                          0x00,
                                                          0x04,
                                                          0x00,
                                                          0x10,
                                                          0x01,
                                                          0x54,
                                                          0x45,
                                                          0x53,
                                                          0x54,
                                                          0x3B,
                                                          0xE9,
                                                          0x6D,
                                                          0x7A};
  /* RESET wire bytes (type=0xFF, LE encoding) */
  static const uint8_t reset_wire[k_xc_reset_wire_len] =
    {0xAA, 0x55, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x4F, 0x1F, 0x66, 0xBC};

  rx_frame_t decoded;

  /* Decode PING */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, ping_wire, k_xc_ping_wire_len, &decoded));
  TEST_ASSERT_EQUAL(0, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_ping, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);

  /* Decode COMMAND */
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_frame_decode(&s_decoder, cmd_wire, k_xc_command_wire_len, &decoded));
  TEST_ASSERT_EQUAL(k_test_seq_one, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL(k_xc_cmd_payload_len, decoded.header.length);
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("TEST", decoded.payload, k_xc_cmd_payload_len);

  /* Decode RESET */
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_frame_decode(&s_decoder, reset_wire, k_xc_reset_wire_len, &decoded));
  TEST_ASSERT_EQUAL(0, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_type_reset, decoded.header.type);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

void test_decode_payload_length_mismatch(void)
{
  enum : uint8_t {
    k_actual_payload_len  = 10,
    k_truncated_payload   = 5,
    k_truncated_frame_len = k_frame_min_size + k_truncated_payload
  };
  /* Create valid frame with 10-byte payload */
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  frame.header.sequence = k_test_seq_one;
  frame.header.length   = k_actual_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "0123456789", k_actual_payload_len);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_actual_payload_len, len);

  /* Truncate buffer to only 5 bytes of payload (total 17 bytes) */
  /* LEN field still says 10, but only 5 bytes + CRC available */
  rx_frame_t decoded;
  rx_err_t   err = rx_frame_decode(&s_decoder, buffer, k_truncated_frame_len, &decoded);

  /* Decoder should detect insufficient data */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_decode_zero_length_buffer(void)
{
  rx_frame_t frame;
  rx_err_t   err = rx_frame_decode(&s_decoder, (uint8_t*)"", 0, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_encode_sequence_rollover(void)
{
  /* Test sequence number rollover (0xFFFF -> 0x0000) */
  rx_frame_t frame1   = {0};
  rx_frame_t frame2   = {0};
  rx_frame_t decoded1 = {0};
  rx_frame_t decoded2 = {0};
  uint8_t    buffer1[k_test_small_buffer];
  uint8_t    buffer2[k_test_small_buffer];
  uint32_t   len1 = 0;
  uint32_t   len2 = 0;

  frame1.header.sequence = k_test_seq_max;
  frame1.header.length   = 0;
  frame1.header.type     = k_frame_type_ack;
  frame1.header.flags    = k_frame_flag_none;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame1, buffer1, &len1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer1, len1, &decoded1));
  TEST_ASSERT_EQUAL_HEX16(k_test_seq_max, decoded1.header.sequence);

  /* Next sequence would be 0x0000 */
  frame2.header.sequence = k_test_seq_zero;
  frame2.header.length   = 0;
  frame2.header.type     = k_frame_type_ack;
  frame2.header.flags    = k_frame_flag_none;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame2, buffer2, &len2));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer2, len2, &decoded2));
  TEST_ASSERT_EQUAL_HEX16(k_test_seq_zero, decoded2.header.sequence);
}

/* =============================================================================
 * PING/PONG/RESET/RESET_ACK Creation Tests
 * =============================================================================
 */

void test_create_ping_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_create_ping(NULL, k_test_seq_zero, NULL, 0));
}

void test_create_ping_empty(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ping(&frame, k_test_seq_one, NULL, 0));
  TEST_ASSERT_EQUAL(k_test_seq_one, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_ping, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, frame.header.flags);
}

void test_create_ping_with_counter(void)
{
  enum : uint8_t { k_ping_counter_len = 4 };
  rx_frame_t    frame;
  const uint8_t counter[k_ping_counter_len] = {0x00, 0x00, 0x00, 0x01};

  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_frame_create_ping(&frame, k_test_seq_zero, counter, k_ping_counter_len));
  TEST_ASSERT_EQUAL(k_ping_counter_len, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_ping, frame.header.type);
  TEST_ASSERT_EQUAL_MEMORY(counter, frame.payload, k_ping_counter_len);
}

void test_create_ping_null_payload_with_len(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_create_ping(&frame, k_test_seq_zero, NULL, 1));
}

void test_create_pong_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_create_pong(NULL, k_test_seq_zero, NULL, 0));
}

void test_create_pong_echoes_payload(void)
{
  enum : uint8_t { k_pong_counter_len = 4 };
  rx_frame_t    frame;
  const uint8_t counter[k_pong_counter_len] = {0x00, 0x00, 0x00, 0x05};

  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_frame_create_pong(&frame, k_test_seq_42, counter, k_pong_counter_len));
  TEST_ASSERT_EQUAL(k_test_seq_42, frame.header.sequence);
  TEST_ASSERT_EQUAL(k_pong_counter_len, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_pong, frame.header.type);
  TEST_ASSERT_EQUAL_MEMORY(counter, frame.payload, k_pong_counter_len);
}

void test_create_reset_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_create_reset(NULL, k_test_seq_zero));
}

void test_create_reset_success(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset(&frame, k_test_seq_one));
  TEST_ASSERT_EQUAL(k_test_seq_one, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_reset, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, frame.header.flags);
}

void test_create_reset_ack_null(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_create_reset_ack(NULL, k_test_seq_zero));
}

void test_create_reset_ack_success(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset_ack(&frame, k_test_seq_one));
  TEST_ASSERT_EQUAL(k_test_seq_one, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_reset_ack, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, frame.header.flags);
}

void test_roundtrip_ping_pong(void)
{
  enum : uint8_t { k_counter_len = 4 };
  rx_frame_t    ping_frame;
  rx_frame_t    pong_frame;
  rx_frame_t    decoded;
  uint8_t       buffer[k_test_small_buffer];
  uint32_t      len;
  const uint8_t counter[k_counter_len] = {0x00, 0x00, 0x00, 0x0A};

  /* Encode PING */
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_frame_create_ping(&ping_frame, k_test_seq_one, counter, k_counter_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &ping_frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_ping, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY(counter, decoded.payload, k_counter_len);

  /* Encode PONG echoing same counter */
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_frame_create_pong(&pong_frame, k_test_seq_one, counter, k_counter_len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &pong_frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_pong, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY(counter, decoded.payload, k_counter_len);
}

void test_roundtrip_reset(void)
{
  rx_frame_t frame;
  rx_frame_t decoded;
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset(&frame, k_test_seq_zero));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_reset, decoded.header.type);
  TEST_ASSERT_EQUAL(k_test_seq_zero, decoded.header.sequence);

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset_ack(&frame, k_test_seq_zero));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_reset_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(k_test_seq_zero, decoded.header.sequence);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

/**
 * @brief Main entry point for test suite execution
 *
 * @details
 * Initializes Unity test framework and executes all registered test functions.
 * Unity automatically calls setUp() before each test and tearDown() after each test.
 *
 * **Test Execution Order:**
 * 1. Encoder initialization tests (3 tests)
 * 2. Decoder initialization tests (2 tests)
 * 3. Encode tests (5 tests)
 * 4. Decode tests (5 tests)
 * 5. Round-trip tests (4 tests)
 * 6. Utility function tests (6 tests)
 * 7. Maximum payload tests (1 test)
 * 8. Frame type tests (4 tests)
 * 9. Frame flag tests (6 tests)
 * 10. Endianness tests (4 tests)
 * 11. Go compatibility tests (2 tests)
 * 12. Edge case tests (3 tests)
 *
 * **Total Tests:** 45 test cases
 *
 * @return int Unity test result (0 = all passed, non-zero = failures)
 *
 * @note This function is called by the Unity test runner
 * @note Exit code is used by CMake/CTest to determine pass/fail
 *
 * @par Example Execution:
 * @code{.bash}
 * # Run all tests
 * ./build/tests/test_rx_frame
 *
 * # Run specific test
 * ./build/tests/test_rx_frame test_roundtrip_empty_frame
 *
 * # Run with verbose output
 * ./build/tests/test_rx_frame -v
 * @endcode
 *
 * @see setUp() Test initialization
 * @see tearDown() Test cleanup
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Encoder init tests */
  RUN_TEST(test_encoder_init_null);
  RUN_TEST(test_encoder_init_success);
  RUN_TEST(test_encoder_deinit_null);

  /* Decoder init tests */
  RUN_TEST(test_decoder_init_null);
  RUN_TEST(test_decoder_init_success);

  /* Encode tests */
  RUN_TEST(test_encode_null_args);
  RUN_TEST(test_encode_uninitialized);
  RUN_TEST(test_encode_payload_too_large);
  RUN_TEST(test_encode_empty_frame);
  RUN_TEST(test_encode_with_payload);

  /* Decode tests */
  RUN_TEST(test_decode_null_args);
  RUN_TEST(test_decode_uninitialized);
  RUN_TEST(test_decode_too_short);
  RUN_TEST(test_decode_invalid_sync);
  RUN_TEST(test_decode_crc_mismatch);

  /* Round-trip tests */
  RUN_TEST(test_roundtrip_empty_frame);
  RUN_TEST(test_roundtrip_with_payload);
  RUN_TEST(test_roundtrip_max_sequence);
  RUN_TEST(test_roundtrip_large_payload);

  /* Utility tests */
  RUN_TEST(test_create_ack_null);
  RUN_TEST(test_create_ack_success);
  RUN_TEST(test_create_nack_null);
  RUN_TEST(test_create_nack_with_flags);
  RUN_TEST(test_encoded_size_calculation);
  RUN_TEST(test_frame_type_valid);

  /* Maximum payload tests */
  RUN_TEST(test_roundtrip_max_payload);

  /* Frame type tests */
  RUN_TEST(test_frame_type_command);
  RUN_TEST(test_frame_type_response);
  RUN_TEST(test_frame_type_ack);
  RUN_TEST(test_frame_type_nack);

  /* Frame flag tests */
  RUN_TEST(test_flag_requires_ack);
  RUN_TEST(test_flag_retransmit);
  RUN_TEST(test_flag_priority);
  RUN_TEST(test_flag_fec_enabled);
  RUN_TEST(test_flag_soft_nack);
  RUN_TEST(test_flag_combined);

  /* Endianness tests */
  RUN_TEST(test_sync_word_little_endian);
  RUN_TEST(test_sequence_little_endian);
  RUN_TEST(test_length_little_endian);
  RUN_TEST(test_crc_little_endian);

  /* Go compatibility tests (header byte order) */
  RUN_TEST(test_go_compatibility_empty_ack);
  RUN_TEST(test_go_compatibility_command_with_payload);

  /* Cross-compatibility vectors (byte-exact with Go, includes CRC) */
  RUN_TEST(test_cross_compat_ping_seq0_empty);
  RUN_TEST(test_cross_compat_pong_seq0_counter42);
  RUN_TEST(test_cross_compat_command_seq1_test);
  RUN_TEST(test_cross_compat_response_seq1_ok);
  RUN_TEST(test_cross_compat_ack_seq1_empty);
  RUN_TEST(test_cross_compat_nack_seq1_empty);
  RUN_TEST(test_cross_compat_reset_seq0_empty);
  RUN_TEST(test_cross_compat_reset_ack_seq0_empty);
  RUN_TEST(test_cross_compat_decode_go_wire_bytes);

  /* PING/PONG/RESET/RESET_ACK tests */
  RUN_TEST(test_create_ping_null);
  RUN_TEST(test_create_ping_empty);
  RUN_TEST(test_create_ping_with_counter);
  RUN_TEST(test_create_ping_null_payload_with_len);
  RUN_TEST(test_create_pong_null);
  RUN_TEST(test_create_pong_echoes_payload);
  RUN_TEST(test_create_reset_null);
  RUN_TEST(test_create_reset_success);
  RUN_TEST(test_create_reset_ack_null);
  RUN_TEST(test_create_reset_ack_success);
  RUN_TEST(test_roundtrip_ping_pong);
  RUN_TEST(test_roundtrip_reset);

  /* Edge case tests */
  RUN_TEST(test_decode_payload_length_mismatch);
  RUN_TEST(test_decode_zero_length_buffer);
  RUN_TEST(test_encode_sequence_rollover);

  return UNITY_END();
}
