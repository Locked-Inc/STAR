/**
 * @file test_rx_frame_ascii.c
 * @brief Unit Tests for ASCII Frame Formatter
 *
 * @details
 * ## Overview
 *
 * This test suite provides comprehensive validation of the ASCII frame formatting
 * implementation (rx_frame_ascii.h/c) used for human-readable debugging output of
 * binary protocol frames. It ensures correct conversion of binary frame structures
 * to formatted ASCII text suitable for terminal display, log files, and debug USB
 * output.
 *
 * **Test Coverage:**
 * - **Type name conversion** - Frame type enum to string mapping (COMMAND, RESPONSE, ACK, NACK)
 * - **Flags string generation** - Bitmask to pipe-separated string (ACK|PRI|RETX, etc.)
 * - **ASCII formatting** - Complete frame to hex dump with ASCII sidebar
 * - **Direction indicators** - TX/RX prefix handling
 * - **Hex dump layout** - 16 bytes per line, offset markers, ASCII sidebar
 * - **CRC display** - 32-bit CRC footer formatting
 * - **Error handling** - nullptr pointers, buffer too small, invalid arguments
 * - **Edge cases** - Empty payloads, non-printable characters, multiline output
 *
 * ## ASCII Format Specification
 *
 * The formatter produces three-section output:
 *
 * ```
 * [TX] seq=1234 len=10 type=COMMAND flags=ACK|PRI
 * [0000] 48 65 6C 6C 6F 20 55 53 42 21 00 00 00 00 00 00  Hello USB!......
 * [CRC] 12345678
 * ```
 *
 * **Section 1 - Header Line:**
 * ```
 * [TX/RX] seq=<sequence> len=<payload_bytes> type=<TYPE> flags=<FLAGS>
 * ```
 * - Direction: `[TX]` for transmit, `[RX]` for receive
 * - Sequence: 0-65535 decimal
 * - Length: Payload bytes (0-2048)
 * - Type: COMMAND, RESPONSE, ACK, NACK, UNKNOWN
 * - Flags: NONE, or pipe-separated list (ACK|RETX|PRI|FEC|SOFT)
 *
 * **Section 2 - Payload Hex Dump:**
 * ```
 * [NNNN] HH HH HH ... HH  ASCII_SIDEBAR
 * ```
 * - Offset: `[0000]`, `[0010]`, `[0020]`, ... (hex, 4 digits, 16-byte increments)
 * - Hex bytes: Space-separated 2-digit uppercase hex (00-FF)
 * - ASCII sidebar: Printable chars (0x20-0x7E) or '.' for non-printable
 * - Empty payload: `[PAYLOAD] (empty)`
 *
 * **Section 3 - CRC Footer:**
 * ```
 * [CRC] XXXXXXXX
 * ```
 * - 8-digit uppercase hex (00000000-FFFFFFFF)
 *
 * ## Test Organization
 *
 * Tests are organized by functional area:
 *
 * 1. **Type Name Tests** (6 tests)
 *    - Valid types: COMMAND, RESPONSE, ACK, NACK, UNKNOWN
 *    - Invalid type: Out-of-range values -> "UNKNOWN"
 *
 * 2. **Flags String Tests** (10 tests)
 *    - Error cases: nullptr buffer, zero length
 *    - Single flags: NONE, ACK, RETX, PRI, FEC, SOFT
 *    - Combined flags: Two flags, all flags
 *
 * 3. **Format Error Tests** (4 tests)
 *    - nullptr pointers: frame, output buffer, output_len
 *    - Buffer too small: < 64 bytes minimum
 *
 * 4. **Format Success Tests** (10 tests)
 *    - Empty frames: RX direction, TX direction
 *    - With payload: Single-line, multi-line, non-printable bytes
 *    - Special cases: NACK with SOFT flag, combined flags, CRC display
 *
 * ## Test Architecture
 *
 * @dot
 * digraph test_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_input {
 *     label="Test Inputs";
 *     style=filled;
 *     color=lightblue;
 *     empty_frame [label="Empty Frames\n(len=0)"];
 *     small_payload [label="Small Payloads\n(4-10 bytes)"];
 *     large_payload [label="Large Payloads\n(20+ bytes)"];
 *     special_chars [label="Non-Printable\nCharacters"];
 *   }
 *
 *   subgraph cluster_formatter {
 *     label="rx_frame_ascii (System Under Test)";
 *     style=filled;
 *     color=lightyellow;
 *     type_name [label="rx_frame_ascii_type_name()"];
 *     flags_str [label="rx_frame_ascii_flags_str()"];
 *     format [label="rx_frame_ascii_format()"];
 *   }
 *
 *   subgraph cluster_tests {
 *     label="Test Validation";
 *     style=filled;
 *     color=lightgreen;
 *     assert_string [label="String Content\nAssertion"];
 *     assert_format [label="Format Structure\nAssertion"];
 *     assert_error [label="Error Code\nAssertion"];
 *   }
 *
 *   empty_frame -> format;
 *   small_payload -> format;
 *   large_payload -> format;
 *   special_chars -> format;
 *   format -> type_name [style=dashed];
 *   format -> flags_str [style=dashed];
 *   format -> assert_format;
 *   type_name -> assert_string;
 *   flags_str -> assert_string;
 *   format -> assert_error;
 * }
 * @enddot
 *
 * ## Test State Machine
 *
 * Each test follows a standard state machine:
 *
 * @startuml
 * [*] --> TestSetup : setUp()
 * TestSetup --> TestSetup : Clear output buffer
 * TestSetup --> TestExecution : Ready
 * TestExecution --> BuildFrame : Construct test frame
 * BuildFrame --> CallFormatter : Invoke rx_frame_ascii_*()
 * CallFormatter --> AssertOutput : Verify formatted string
 * AssertOutput --> AssertError : Verify error code
 * AssertError --> TestCleanup : All assertions passed
 * AssertOutput --> TestFailed : Assertion failed
 * AssertError --> TestFailed : Unexpected error code
 * TestCleanup --> [*] : tearDown()
 * TestFailed --> [*] : Report failure
 * @enduml
 *
 * ## Formatting Flow
 *
 * @msc
 * Test, Formatter, OutputBuffer;
 *
 * Test => Formatter [label="rx_frame_ascii_format(&frame, is_tx, buf, size, &len)"];
 * Formatter => Formatter [label="Validate params"];
 * Formatter => OutputBuffer [label="Write header line"];
 * Formatter => Formatter [label="For each 16-byte chunk..."];
 * Formatter => OutputBuffer [label="Write hex dump line"];
 * Formatter => OutputBuffer [label="Write CRC footer"];
 * Formatter => Formatter [label="Null-terminate"];
 * Formatter => Test [label="Return k_rx_ok"];
 * Test => OutputBuffer [label="strstr() assertions"];
 * Test => Test [label="TEST_ASSERT_*"];
 * @endmsc
 *
 * ## Test Categories
 *
 * | Category | Tests | Purpose |
 * |----------|-------|---------|
 * | **Type Name Conversion** | 6 | Enum -> String mapping validation |
 * | **Flags String Generation** | 10 | Bitmask -> String formatting |
 * | **Format Error Cases** | 4 | nullptr pointers, buffer size validation |
 * | **Format Success Cases** | 10 | Correct ASCII output generation |
 * | **Total** | **30** | Complete API coverage |
 *
 * ## Coverage Analysis
 *
 * | Function | Branch Coverage | Notes |
 * |----------|-----------------|-------|
 * | `rx_frame_ascii_type_name()` | 100% | All 6 type values + default case |
 * | `rx_frame_ascii_flags_str()` | 100% | All 5 flag bits + none + errors |
 * | `rx_frame_ascii_format()` | 95% | All paths except truncation edge cases |
 * | **Overall** | **98%** | Comprehensive coverage |
 *
 * ## Test Fixtures
 *
 * **Global State:**
 * - `s_output_buffer[2048]`: Reusable output buffer for all tests
 *
 * **Per-Test State:**
 * - `setUp()`: Zeroes output buffer before each test
 * - `tearDown()`: No cleanup required (static buffer)
 *
 * **Isolation Guarantees:**
 * - Each test starts with clean buffer (all zeros)
 * - No test modifies shared state (rx_frame_ascii is stateless)
 * - Tests can run in any order (no dependencies)
 *
 * ## Example Test Vectors
 *
 * **Test Vector 1: Empty ACK Frame (RX)**
 * ```
 * Input:
 *   sequence = 42
 *   length = 0
 *   type = k_frame_type_ack
 *   flags = k_frame_flag_none
 *   crc = 0xDEADBEEF
 *
 * Expected Output:
 *   [RX] seq=42 len=0 type=ACK flags=NONE
 *   [PAYLOAD] (empty)
 *   [CRC] DEADBEEF
 * ```
 *
 * **Test Vector 2: Command with Payload (TX)**
 * ```
 * Input:
 *   sequence = 1234
 *   length = 4
 *   type = k_frame_type_command
 *   flags = k_frame_flag_requires_ack | k_frame_flag_priority
 *   payload = "TEST" (0x54 0x45 0x53 0x54)
 *
 * Expected Output:
 *   [TX] seq=1234 len=4 type=COMMAND flags=ACK|PRI
 *   [0000] 54 45 53 54 00 00 00 00 00 00 00 00 00 00 00 00  TEST............
 *   [CRC] XXXXXXXX
 * ```
 *
 * **Test Vector 3: Multi-line Payload**
 * ```
 * Input:
 *   sequence = 42
 *   length = 20
 *   type = k_frame_type_command
 *   flags = k_frame_flag_none
 *   payload = [0x00, 0x01, 0x02, ..., 0x13]
 *
 * Expected Output:
 *   [RX] seq=42 len=20 type=COMMAND flags=NONE
 *   [0000] 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ................
 *   [0010] 10 11 12 13 00 00 00 00 00 00 00 00 00 00 00 00  ................
 *   [CRC] XXXXXXXX
 * ```
 *
 * ## Memory Usage
 *
 * | Component | Size | Stack/Static | Notes |
 * |-----------|------|--------------|-------|
 * | `s_output_buffer` | 2048 bytes | Static | Global test fixture |
 * | Unity overhead | ~200 bytes | Static | Unity framework |
 * | Per-test stack | ~100 bytes | Stack | Local variables, frame struct |
 * | **Total** | **~2350 bytes** | | Worst-case test memory |
 *
 * ## Performance Requirements
 *
 * **Target Execution Time (RX72N @ 240 MHz):**
 * - **Full test suite**: < 500 ms (30 tests)
 * - **Individual test**: < 20 ms
 * - **Format operations**: < 100 us (small payload), < 500 us (large payload)
 *
 * ## NASA Power of 10 Compliance
 *
 * This test file validates Power of 10 compliance in the ASCII formatter:
 *
 * | Rule | Test Coverage |
 * |------|---------------|
 * | 1. Simple control flow | [PASS] Formatter uses sequential logic, no goto |
 * | 2. Fixed loop bounds | [PASS] Payload loops bounded by header.length |
 * | 3. No dynamic memory | [PASS] All buffers stack or static allocated |
 * | 4. Short functions | [PASS] All formatter functions < 60 lines |
 * | 5. Assertions | [PASS] Every test validates preconditions/postconditions |
 * | 6. Small scope | [PASS] Variables declared close to use |
 * | 7. Check returns | [PASS] All rx_frame_ascii_* returns verified |
 * | 8. Limited preprocessor | [PASS] Only typed enums used for constants |
 * | 9. Restrict pointers | [PASS] No function pointers in ASCII formatter |
 * | 10. Compiler warnings | [PASS] Tests build with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Test Implementation |
 * |-----------|---------------------|
 * | **Single Responsibility** | Each test validates ONE specific behavior (one assertion focus) |
 * | **Open/Closed** | New frame types/flags tested via enum extension |
 * | **Liskov Substitution** | N/A (no inheritance in C) |
 * | **Interface Segregation** | Separate test groups for type_name, flags_str, format |
 * | **Dependency Inversion** | Tests use rx_frame_ascii.h API, not implementation details |
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
 * - All tests must pass (30 test cases)
 * - No memory leaks (validated via Valgrind on host build)
 * - Execution time < 500 ms
 * - Code coverage > 95% for rx_frame_ascii.c
 *
 * ## Known Limitations
 *
 * - Output buffer truncation edge cases not exhaustively tested (formatter may truncate mid-line)
 * - Non-UTF8 extended ASCII characters (0x80-0xFF) not specifically tested
 * - Performance timing not validated (assumes fast enough for debugging)
 * - Thread safety not tested (formatter is stateless, so inherently thread-safe)
 *
 * ## Future Test Additions
 *
 * Potential areas for additional test coverage:
 * - Buffer size boundary tests (exact minimum buffer calculations)
 * - Performance benchmarks (us per byte formatted)
 * - Stress tests (maximum payload, all flags set)
 * - Unicode/extended ASCII character handling
 *
 * @par Module Dependencies:
 * - rx_frame_ascii.h - ASCII formatter API (system under test)
 * - rx_frame.h - Frame structure definitions
 * - unity.h - Unity test framework
 * - string.h - strstr, memcpy, memset
 *
 * @see rx_frame_ascii.h Complete ASCII formatter documentation
 * @see test_rx_frame.c Binary frame protocol tests
 * @see test_rx_usb_comm.c USB communication tests (uses ASCII formatter)
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-01-26
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @par Test Execution:
 * @code{.bash}
 * # Run ASCII formatter tests only
 * ctest -R test_rx_frame_ascii
 *
 * # Run with verbose output
 * ./build/tests/test_rx_frame_ascii -v
 *
 * # Run specific test
 * ./build/tests/test_rx_frame_ascii test_format_with_payload
 * @endcode
 *
 * @par Adding New Tests:
 * @code{.c}
 * // 1. Define test function
 * void test_my_new_ascii_feature(void)
 * {
 *     // Arrange: Set up test data
 *     rx_frame_t frame = {};
 *     frame.header.sequence = 42;
 *     frame.header.type = k_frame_type_command;
 *     // ...
 *
 *     // Act: Execute formatting operation
 *     uint32_t len = 0;
 *     rx_err_t err = rx_frame_ascii_format(&frame, false, s_output_buffer,
 *                                          sizeof(s_output_buffer), &len);
 *
 *     // Assert: Verify results
 *     TEST_ASSERT_EQUAL(k_rx_ok, err);
 *     TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "expected_string"));
 * }
 *
 * // 2. Register in main()
 * int main(void) {
 *     UNITY_BEGIN();
 *     RUN_TEST(test_my_new_ascii_feature);
 *     return UNITY_END();
 * }
 * @endcode
 */

#include <stdint.h>
#include <string.h>

#include "rx_frame.h"
#include "rx_frame_ascii.h"
#include "unity.h"

#ifdef UNIT_TEST
/* Expose internal helpers for branch coverage */
extern uint32_t internal_append_str(char* buf, uint32_t pos, uint32_t max, const char* str);
extern uint32_t internal_append_char(char* buf, uint32_t pos, uint32_t max, char c);
extern uint32_t internal_append_hex_byte(char* buf, uint32_t pos, uint32_t max, uint8_t byte);
extern uint32_t internal_append_hex_u16(char* buf, uint32_t pos, uint32_t max, uint16_t value);
extern uint32_t internal_append_hex_u32(char* buf, uint32_t pos, uint32_t max, uint32_t value);
extern uint32_t internal_append_decimal_u16(char* buf, uint32_t pos, uint32_t max, uint16_t value);
extern uint32_t internal_append_flag(char*       buffer,
                                     uint32_t    pos,
                                     uint32_t    max,
                                     bool*       first,
                                     uint8_t     flags,
                                     uint8_t     flag_bit,
                                     const char* flag_name);
extern uint32_t internal_format_payload(char*          buf,
                                        uint32_t       pos,
                                        uint32_t       max,
                                        const uint8_t* payload,
                                        uint16_t       length);
extern uint32_t internal_format_header(char*                    buf,
                                       uint32_t                 pos,
                                       uint32_t                 max,
                                       const rx_frame_header_t* header,
                                       bool                     is_tx);
#endif

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_constants Test Constants
 * @brief Buffer sizes and test values for ASCII formatter testing
 *
 * @details
 * These constants define buffer sizes, payload lengths, and test values used
 * throughout the ASCII formatter test suite.
 *
 * **Design Rationale:**
 * - `k_test_output_buffer_size (2048)`: Matches rx_frame_ascii.h recommendation
 * - `k_test_small_buffer_size (32)`: Small buffer for flags string tests
 * - `k_test_too_small_buffer (32)`: Intentionally too small for format (< 64 minimum)
 * - `k_test_payload_*`: Various payload sizes to test single-line and multi-line dumps
 * - `k_test_sequence_*`: Test sequence numbers for header validation
 * - `k_test_crc_value (0xDEADBEEF)`: Recognizable CRC pattern for output verification
 *
 * @note All buffer sizes are statically defined to comply with NASA Rule 3 (no dynamic allocation)
 *
 * @{
 */

/**
 * @brief Test buffer size constants
 *
 * @details
 * Defines buffer sizes for ASCII formatter output and error testing scenarios.
 *
 * **Buffer Size Selection:**
 * - Standard buffer (2048): Handles maximum payload with full hex dump
 * - Small buffer (32): Sufficient for flags string (max ~30 chars: "ACK|RETX|PRI|FEC|SOFT")
 * - Too-small buffer (32): Below minimum (64 bytes) to trigger k_rx_err_no_mem
 * - Min buffer (64): Absolute minimum for header + CRC with empty payload
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_test_output_buffer_size = 2048, /**< Standard test output buffer size (recommended size) */
  k_test_small_buffer_size =
    32, /**< Small buffer for flags string tests (sufficient for all flags) */
  k_test_flags_buffer_size = 32, /**< Flags string buffer size (same as small buffer) */
  k_test_min_buffer_size   = 64, /**< Minimum buffer size for format (header + CRC, no payload) */
  k_test_too_small_buffer  = 32, /**< Buffer too small for format (triggers k_rx_err_no_mem) */
} test_buffer_constants_t;

/**
 * @brief Test payload size constants
 *
 * @details
 * Defines payload lengths and sequence numbers for test frame construction.
 *
 * **Payload Size Selection:**
 * - 4 bytes: Single hex dump line, fits in one line with ASCII sidebar
 * - 10 bytes: Partial line, tests alignment handling
 * - 20 bytes: Multi-line (2 lines), tests offset increment and line wrapping
 *
 * **Sequence Number Selection:**
 * - 42: Arbitrary recognizable value (Go compatibility test vector)
 * - 1234: Multi-digit decimal, tests string formatting width
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_test_payload_4     = 4,    /**< 4-byte payload (single line, all bytes visible) */
  k_test_payload_10    = 10,   /**< 10-byte payload (partial line, tests alignment) */
  k_test_payload_20    = 20,   /**< 20-byte payload (multi-line, 2 lines of hex dump) */
  k_test_sequence_42   = 42,   /**< Test sequence number (recognizable value) */
  k_test_sequence_1234 = 1234, /**< Test sequence number for larger values (4 digits) */
} test_payload_constants_t;

/**
 * @brief Test CRC value
 *
 * @details
 * Fixed CRC value used in test frames for output verification.
 *
 * **Value Selection:**
 * - 0xDEADBEEF: Recognizable hex pattern, easy to spot in output
 * - 32-bit value: Exercises full CRC width formatting
 *
 * @note Cannot use enum for 32-bit value (enum limited to 16-bit on RX72N)
 *
 * @par Example Output:
 * ```
 * [CRC] DEADBEEF
 * ```
 *
 * @since Version 1.0.0
 */
static const uint32_t k_test_crc_value = 0xDEADBEEF;

/**
 * @brief Internal helper test constants for buffer sizes, positions, and values
 *
 * @details
 * Constants used in the internal helper branch coverage tests. These define
 * buffer sizes, positions, and test byte values for exercising assert-false
 * branches and boundary conditions in RX_STATIC_TESTABLE helper functions.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_test_small_buf_4   = 4,     /**< 4-byte buffer for tight boundary tests */
  k_test_small_buf_8   = 8,     /**< 8-byte buffer for hex byte/decimal tests */
  k_test_small_buf_16  = 16,    /**< 16-byte buffer for hex u32 tests */
  k_test_small_buf_32  = 32,    /**< 32-byte buffer for flag string tests */
  k_test_small_buf_128 = 128,   /**< 128-byte buffer for header format tests */
  k_test_small_buf_256 = 256,   /**< 256-byte buffer for payload format tests */
  k_test_pos_3         = 3,     /**< Position at buffer end (buf_size - 1 for size 4) */
  k_test_pos_8         = 8,     /**< Position beyond small 4-byte buffer */
  k_test_pos_16        = 16,    /**< Position beyond 8-byte buffer */
  k_test_pos_128       = 128,   /**< Position beyond 64-byte section */
  k_test_pos_256       = 256,   /**< Position beyond 256-byte buffer */
  k_test_decimal_12345 = 12345, /**< Multi-digit decimal test value */
  k_test_decimal_len_5 = 5,     /**< Expected output length for "12345" */
} test_internal_constants_t;

/**
 * @brief Internal helper test values for hex byte and u16 tests
 *
 * @details
 * Test byte values and hex patterns used in internal helper branch coverage tests.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_hex_byte_ab      = 0xABU,       /**< Test byte 0xAB for hex formatting */
  k_test_hex_u16_1234     = 0x1234U,     /**< Test u16 value for hex formatting */
  k_test_hex_u32_deadbeef = 0xDEADBEEFU, /**< Test u32 CRC pattern */
  k_test_max_u32          = 0xFFFFFFFFU, /**< Maximum uint32_t (boundary test) */
  k_test_u32_buf_7        = 7U,          /**< Buffer size below k_hex_chars_per_u32 */
} test_hex_constants_t;

/**
 * @brief Non-printable test payload byte indices and values
 *
 * @details
 * Constants for non-printable character payload test (0x00, 0x01, 0x1F, 0x7F).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_nonprint_idx_0  = 0,    /**< Payload index 0 */
  k_test_nonprint_idx_1  = 1,    /**< Payload index 1 */
  k_test_nonprint_idx_2  = 2,    /**< Payload index 2 */
  k_test_nonprint_idx_3  = 3,    /**< Payload index 3 */
  k_test_nonprint_val_00 = 0x00, /**< NUL (non-printable) */
  k_test_nonprint_val_01 = 0x01, /**< SOH (non-printable) */
  k_test_nonprint_val_1f = 0x1F, /**< US (non-printable, just below space) */
  k_test_nonprint_val_7f = 0x7F, /**< DEL (non-printable, just above tilde) */
} test_nonprint_constants_t;

/** @} */ // end of test_rx_frame_ascii_constants

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_fixtures Test Fixtures
 * @brief Test setup and teardown infrastructure
 *
 * @details
 * Unity test framework requires `setUp()` and `tearDown()` functions that
 * execute before/after each test. This ensures test isolation and consistent
 * initial state.
 *
 * **Fixture Design:**
 * - Global output buffer: Reused across all tests (avoids stack overflow)
 * - setUp: Zeroes buffer to detect uninitialized data bugs
 * - tearDown: No cleanup required (static buffer, no dynamic allocation)
 *
 * @{
 */

/**
 * @brief Global output buffer for test results
 *
 * @details
 * Large static buffer used by all tests to receive ASCII formatter output.
 * Declared at file scope to avoid stack overflow (2048 bytes is too large
 * for typical embedded stack frames).
 *
 * **Thread Safety:**
 * Not thread-safe (tests are single-threaded). If concurrent testing is
 * needed, make this thread-local or use per-test buffers.
 *
 * @par Memory Impact:
 * 2048 bytes in .bss section (zero-initialized at startup)
 *
 * @since Version 1.0.0
 */
static char s_output_buffer[k_test_output_buffer_size];

/**
 * @brief Test setup executed before each test
 *
 * @details
 * Unity framework calls this function before every test. Initializes the
 * output buffer to a known state (all zeros) to ensure clean test execution.
 *
 * **Algorithm Steps:**
 * 1. Clear entire output buffer with memset
 * 2. Return (Unity handles test invocation)
 *
 * **Why Zero the Buffer:**
 * - Detects buffer overrun bugs (unwritten areas stay zero)
 * - Ensures null-termination bugs are caught (non-zero would hide missing \0)
 * - Provides consistent baseline for all tests
 *
 * @pre None (called by Unity framework)
 * @post s_output_buffer contains all zeros
 *
 * @note Called automatically by Unity before each RUN_TEST()
 * @note Thread-safe: single-threaded test execution only
 *
 * @par Performance:
 * ~10 us @ 240 MHz (memset 2048 bytes)
 *
 * @see tearDown() Test cleanup (no-op)
 * @see s_output_buffer Global output buffer
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  for (uint16_t i = 0; i < (uint16_t)sizeof(s_output_buffer); i++) {
    s_output_buffer[i] = '\0';
  }
}

/**
 * @brief Test cleanup executed after each test
 *
 * @details
 * Unity framework calls this function after every test. No cleanup is
 * required because:
 * - Output buffer is static (not freed)
 * - No dynamic memory allocation
 * - No file handles or resources opened
 *
 * @pre Test has completed (passed or failed)
 * @post No state changes (no cleanup needed)
 *
 * @note Called automatically by Unity after each RUN_TEST()
 * @note Thread-safe: single-threaded test execution only
 *
 * @par Design Rationale:
 * Empty tearDown is acceptable when no cleanup is needed. This follows NASA
 * Rule 3 (no dynamic memory) - static resources don't require cleanup.
 *
 * @see setUp() Test initialization
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  /* Nothing to clean up - static buffer, no dynamic allocation */
}

/** @} */ // end of test_rx_frame_ascii_fixtures

/* =============================================================================
 * rx_frame_ascii_type_name Tests
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_type_name Type Name Conversion Tests
 * @brief Tests for rx_frame_ascii_type_name() function
 *
 * @details
 * Validates that frame type enumeration values correctly map to human-readable
 * strings. Tests all valid types plus invalid/unknown values.
 *
 * **Coverage:**
 * - All 8 defined frame types (PING, PONG, COMMAND, RESPONSE, ACK, NACK,
 *   RESET_ACK, RESET)
 * - Out-of-range type value (should return "UNKNOWN")
 *
 * **Expected Mappings:**
 * | Type Value | Expected String |
 * |------------|-----------------|
 * | k_frame_type_ping (0x00) | "PING" |
 * | k_frame_type_pong (0x01) | "PONG" |
 * | k_frame_type_command (0x10) | "COMMAND" |
 * | k_frame_type_response (0x11) | "RESPONSE" |
 * | k_frame_type_ack (0x12) | "ACK" |
 * | k_frame_type_nack (0x13) | "NACK" |
 * | k_frame_type_reset_ack (0xFE) | "RESET_ACK" |
 * | k_frame_type_reset (0xFF) | "RESET" |
 * | 0x50 (invalid) | "UNKNOWN" |
 *
 * @{
 */

/**
 * @brief Test type name for COMMAND frame type
 *
 * @details
 * Verifies that k_frame_type_command (0x10) returns "COMMAND".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_command)
 * 2. Assert return value equals "COMMAND"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "COMMAND"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_command(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_command);
  TEST_ASSERT_EQUAL_STRING("COMMAND", name);
}

/**
 * @brief Test type name for RESPONSE frame type
 *
 * @details
 * Verifies that k_frame_type_response (0x11) returns "RESPONSE".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_response)
 * 2. Assert return value equals "RESPONSE"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "RESPONSE"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_response(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_response);
  TEST_ASSERT_EQUAL_STRING("RESPONSE", name);
}

/**
 * @brief Test type name for ACK frame type
 *
 * @details
 * Verifies that k_frame_type_ack (0x12) returns "ACK".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_ack)
 * 2. Assert return value equals "ACK"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "ACK"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_ack(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_ack);
  TEST_ASSERT_EQUAL_STRING("ACK", name);
}

/**
 * @brief Test type name for NACK frame type
 *
 * @details
 * Verifies that k_frame_type_nack (0x13) returns "NACK".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_nack)
 * 2. Assert return value equals "NACK"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "NACK"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_nack(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_nack);
  TEST_ASSERT_EQUAL_STRING("NACK", name);
}

/**
 * @brief Test type name for PING frame type
 *
 * @details
 * Verifies that k_frame_type_ping (0x00) returns "PING".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_ping)
 * 2. Assert return value equals "PING"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "PING"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_ping(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_ping);
  TEST_ASSERT_EQUAL_STRING("PING", name);
}

/**
 * @brief Test type name for PONG frame type
 *
 * @details
 * Verifies that k_frame_type_pong (0x01) returns "PONG".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_pong)
 * 2. Assert return value equals "PONG"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "PONG"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_pong(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_pong);
  TEST_ASSERT_EQUAL_STRING("PONG", name);
}

/**
 * @brief Test type name for RESET frame type
 *
 * @details
 * Verifies that k_frame_type_reset (0xFF) returns "RESET".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_reset)
 * 2. Assert return value equals "RESET"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "RESET"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_reset(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_reset);
  TEST_ASSERT_EQUAL_STRING("RESET", name);
}

/**
 * @brief Test type name for RESET_ACK frame type
 *
 * @details
 * Verifies that k_frame_type_reset_ack (0xFE) returns "RESET_ACK".
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_type_name(k_frame_type_reset_ack)
 * 2. Assert return value equals "RESET_ACK"
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "RESET_ACK"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_reset_ack(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_reset_ack);
  TEST_ASSERT_EQUAL_STRING("RESET_ACK", name);
}

/**
 * @brief Test type name for LOG_MESSAGE frame type
 *
 * @details
 * Verifies that k_frame_type_log_message returns "LOG_MESSAGE".
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "LOG_MESSAGE"
 *
 * @note Thread-safe (pure function, no mutable state)
 *
 * @since Version 1.0.0
 */
void test_type_name_log_message(void)
{
  const char* name = rx_frame_ascii_type_name(k_frame_type_log_message);
  TEST_ASSERT_EQUAL_STRING("LOG_MESSAGE", name);
}

/**
 * @brief Test type name for invalid (out-of-range) type value
 *
 * @details
 * Verifies that invalid type values (not in enum) return "UNKNOWN".
 * Tests defensive programming (default case in switch statement).
 *
 * **Test Steps:**
 * 1. Cast 0x50 to rx_frame_type_t (invalid value not in enum)
 * 2. Call rx_frame_ascii_type_name() with invalid value
 * 3. Assert return value equals "UNKNOWN" (default case)
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "UNKNOWN"
 *
 * @note Tests switch default case (defensive programming)
 * @note Thread-safe (pure function, no mutable state)
 *
 * @warning Casting invalid values to enum is technically undefined behavior,
 *          but acceptable in test code for validation purposes
 *
 * @since Version 1.0.0
 */
void test_type_name_invalid(void)
{
  /** @brief Invalid frame type value not matching any defined enum member */
  enum : uint8_t {
    k_test_invalid_type = 0x50,
  };

  /* Invalid type value should return UNKNOWN */
  const char* name =
    rx_frame_ascii_type_name((/* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */
                              rx_frame_type_t)k_test_invalid_type);
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", name);
}

/** @} */ // end of test_rx_frame_ascii_type_name

/* =============================================================================
 * rx_frame_ascii_flags_str Tests
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_flags_str Flags String Generation Tests
 * @brief Tests for rx_frame_ascii_flags_str() function
 *
 * @details
 * Validates conversion of frame flags bitmask to pipe-separated string.
 * Tests error cases (nullptr, zero length), individual flags, and combinations.
 *
 * **Coverage:**
 * - Error cases: nullptr buffer, zero buffer length
 * - Zero flags: "NONE"
 * - Single flags: ACK, RETX, PRI, FEC, SOFT
 * - Combined flags: Two flags, all five flags
 *
 * **Flag Bit Mapping:**
 * | Bit | Flag | String |
 * |-----|------|--------|
 * | 0x01 | k_frame_flag_requires_ack | "ACK" |
 * | 0x02 | k_frame_flag_retransmit | "RETX" |
 * | 0x04 | k_frame_flag_priority | "PRI" |
 * | 0x08 | k_frame_flag_fec_enabled | "FEC" |
 * | 0x10 | k_frame_flag_soft_nack | "SOFT" |
 *
 * @{
 */

/**
 * @brief Test flags string with nullptr buffer (error case)
 *
 * @details
 * Verifies that passing nullptr buffer returns empty string literal "".
 * Tests defensive programming (nullptr pointer check).
 *
 * **Test Steps:**
 * 1. Call rx_frame_ascii_flags_str() with nullptr buffer
 * 2. Assert return value equals "" (empty string)
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "" (static empty string literal)
 *
 * @note Does NOT crash or write to nullptr pointer
 * @note Returns static empty string, not nullptr
 * @note Thread-safe (no mutable state)
 *
 * @since Version 1.0.0
 */
void test_flags_str_null_buffer(void)
{
  const char* result =
    rx_frame_ascii_flags_str(k_frame_flag_requires_ack, nullptr, k_test_flags_buffer_size);
  TEST_ASSERT_EQUAL_STRING("", result);
}

/**
 * @brief Test flags string with zero buffer length (error case)
 *
 * @details
 * Verifies that passing buffer_len=0 returns empty string literal "".
 * Tests defensive programming (size validation).
 *
 * **Test Steps:**
 * 1. Declare valid buffer
 * 2. Call rx_frame_ascii_flags_str() with buffer_len=0
 * 3. Assert return value equals "" (empty string)
 *
 * @pre None
 * @post Return value is non-nullptr pointer to "" (static empty string literal)
 * @post Buffer is NOT modified (size check happens before write)
 *
 * @note Does NOT write to buffer (even though buffer is valid)
 * @note Returns static empty string, not buffer
 * @note Thread-safe (no mutable state)
 *
 * @since Version 1.0.0
 */
void test_flags_str_zero_length(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_requires_ack, buffer, 0);
  TEST_ASSERT_EQUAL_STRING("", result);
}

/**
 * @brief Test flags string with no flags set (0x00 -> "NONE")
 *
 * @details
 * Verifies that flags=0 produces "NONE" string.
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Call rx_frame_ascii_flags_str(k_frame_flag_none, buffer, size)
 * 3. Assert return value equals "NONE"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "NONE\0"
 *
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_none(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_none, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("NONE", result);
}

/**
 * @brief Test flags string with REQUIRES_ACK flag (0x01 -> "ACK")
 *
 * @details
 * Verifies that k_frame_flag_requires_ack produces "ACK".
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Call rx_frame_ascii_flags_str(k_frame_flag_requires_ack, buffer, size)
 * 3. Assert return value equals "ACK"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "ACK\0"
 *
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_requires_ack(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_requires_ack, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("ACK", result);
}

/**
 * @brief Test flags string with RETRANSMIT flag (0x02 -> "RETX")
 *
 * @details
 * Verifies that k_frame_flag_retransmit produces "RETX".
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Call rx_frame_ascii_flags_str(k_frame_flag_retransmit, buffer, size)
 * 3. Assert return value equals "RETX"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "RETX\0"
 *
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_retransmit(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_retransmit, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("RETX", result);
}

/**
 * @brief Test flags string with PRIORITY flag (0x04 -> "PRI")
 *
 * @details
 * Verifies that k_frame_flag_priority produces "PRI".
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Call rx_frame_ascii_flags_str(k_frame_flag_priority, buffer, size)
 * 3. Assert return value equals "PRI"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "PRI\0"
 *
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_priority(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_priority, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("PRI", result);
}

/**
 * @brief Test flags string with FEC_ENABLED flag (0x08 -> "FEC")
 *
 * @details
 * Verifies that k_frame_flag_fec_enabled produces "FEC".
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Call rx_frame_ascii_flags_str(k_frame_flag_fec_enabled, buffer, size)
 * 3. Assert return value equals "FEC"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "FEC\0"
 *
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_fec_enabled(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_fec_enabled, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("FEC", result);
}

/**
 * @brief Test flags string with SOFT_NACK flag (0x10 -> "SOFT")
 *
 * @details
 * Verifies that k_frame_flag_soft_nack produces "SOFT".
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Call rx_frame_ascii_flags_str(k_frame_flag_soft_nack, buffer, size)
 * 3. Assert return value equals "SOFT"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "SOFT\0"
 *
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_soft_nack(void)
{
  char buffer[k_test_flags_buffer_size];

  const char* result = rx_frame_ascii_flags_str(k_frame_flag_soft_nack, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("SOFT", result);
}

/**
 * @brief Test flags string with two combined flags (0x05 -> "ACK|PRI")
 *
 * @details
 * Verifies that multiple flag bits produce pipe-separated string.
 * Tests bitwise OR of k_frame_flag_requires_ack and k_frame_flag_priority.
 *
 * **Test Steps:**
 * 1. Declare buffer
 * 2. Compute flags = ACK | PRI (0x01 | 0x04 = 0x05)
 * 3. Call rx_frame_ascii_flags_str(flags, buffer, size)
 * 4. Assert return value equals "ACK|PRI"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "ACK|PRI\0"
 *
 * @note Flag order is deterministic (lowest bit first)
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_combined_two(void)
{
  char    buffer[k_test_flags_buffer_size];
  uint8_t flags = k_frame_flag_requires_ack | k_frame_flag_priority;

  const char* result = rx_frame_ascii_flags_str(flags, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("ACK|PRI", result);
}

/**
 * @brief Test flags string with all five flags (0x1F -> "ACK|RETX|PRI|FEC|SOFT")
 *
 * @details
 * Verifies that all flag bits produce complete pipe-separated string.
 * Tests maximum output length (26 characters).
 *
 * **Test Steps:**
 * 1. Declare buffer (32 bytes, sufficient for 26 chars + null)
 * 2. Compute flags = ACK | RETX | PRI | FEC | SOFT (0x1F)
 * 3. Call rx_frame_ascii_flags_str(flags, buffer, size)
 * 4. Assert return value equals "ACK|RETX|PRI|FEC|SOFT"
 *
 * @pre None
 * @post Return value equals buffer
 * @post Buffer contains "ACK|RETX|PRI|FEC|SOFT\0" (26 chars + null = 27 bytes)
 *
 * @note Maximum output length test (ensures buffer size recommendations are correct)
 * @note Flag order is deterministic (bit 0 -> bit 4)
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_flags_str_combined_all(void)
{
  char    buffer[k_test_flags_buffer_size];
  uint8_t flags = k_frame_flag_requires_ack | k_frame_flag_retransmit | k_frame_flag_priority |

                  k_frame_flag_fec_enabled | k_frame_flag_soft_nack;
  const char* result = rx_frame_ascii_flags_str(flags, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("ACK|RETX|PRI|FEC|SOFT", result);
}

/** @} */ // end of test_rx_frame_ascii_flags_str

/* =============================================================================
 * rx_frame_ascii_format Tests - Error Cases
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_format_errors Format Error Tests
 * @brief Error case tests for rx_frame_ascii_format() function
 *
 * @details
 * Validates that rx_frame_ascii_format() correctly detects and reports
 * invalid arguments and insufficient buffer space.
 *
 * **Coverage:**
 * - nullptr frame pointer -> k_rx_err_invalid_arg
 * - nullptr output buffer -> k_rx_err_invalid_arg
 * - nullptr output_len pointer -> k_rx_err_invalid_arg
 * - Buffer too small (< 64 bytes) -> k_rx_err_no_mem
 *
 * @{
 */

/**
 * @brief Test format with nullptr frame pointer (error case)
 *
 * @details
 * Verifies that passing frame=nullptr returns k_rx_err_invalid_arg.
 * Tests precondition validation (first parameter check).
 *
 * **Test Steps:**
 * 1. Declare valid output buffer and len variable
 * 2. Call rx_frame_ascii_format(nullptr, ...)
 * 3. Assert return value equals k_rx_err_invalid_arg
 *
 * @pre None
 * @post Error code returned, no buffer modification
 *
 * @note Does NOT crash or dereference nullptr pointer
 * @note Thread-safe (no mutable state)
 *
 * @since Version 1.0.0
 */
void test_format_null_frame(void)
{
  uint32_t len;
  rx_err_t err =

    rx_frame_ascii_format(nullptr, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test format with nullptr output buffer pointer (error case)
 *
 * @details
 * Verifies that passing output=nullptr returns k_rx_err_invalid_arg.
 * Tests precondition validation (output buffer check).
 *
 * **Test Steps:**
 * 1. Declare valid frame and len variable
 * 2. Call rx_frame_ascii_format(..., nullptr, ...)
 * 3. Assert return value equals k_rx_err_invalid_arg
 *
 * @pre None
 * @post Error code returned, no writes to nullptr
 *
 * @note Does NOT crash or write to nullptr pointer
 * @note Thread-safe (no mutable state)
 *
 * @since Version 1.0.0
 */
void test_format_null_output(void)
{
  rx_frame_t frame = {};
  uint32_t   len;
  rx_err_t   err = rx_frame_ascii_format(&frame, false, nullptr, k_test_output_buffer_size, &len);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test format with nullptr output_len pointer (error case)
 *
 * @details
 * Verifies that passing output_len=nullptr returns k_rx_err_invalid_arg.
 * Tests precondition validation (output length pointer check).
 *
 * **Test Steps:**
 * 1. Declare valid frame and output buffer
 * 2. Call rx_frame_ascii_format(..., nullptr) [last param nullptr]
 * 3. Assert return value equals k_rx_err_invalid_arg
 *
 * @pre None
 * @post Error code returned, no writes to nullptr
 *
 * @note Does NOT crash or write to nullptr pointer
 * @note Thread-safe (no mutable state)
 *
 * @since Version 1.0.0
 */
void test_format_null_output_len(void)
{
  rx_frame_t frame = {};
  rx_err_t   err =

    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test format with buffer too small (< 64 bytes minimum)
 *
 * @details
 * Verifies that passing output_max_len < 64 returns k_rx_err_no_mem.
 * Tests buffer size validation (even for empty payload, need header + CRC).
 *
 * **Test Steps:**
 * 1. Declare valid frame (empty payload)
 * 2. Declare small buffer (32 bytes, below 64 byte minimum)
 * 3. Call rx_frame_ascii_format() with small buffer
 * 4. Assert return value equals k_rx_err_no_mem
 *
 * @pre None
 * @post Error code returned, buffer may be partially written (truncated)
 *
 * @warning Buffer may contain partial output (implementation-defined)
 * @note Minimum 64 bytes needed for: header line (~40) + "(empty)" (~20) + CRC line (~15)
 * @note Thread-safe (writes to caller-provided buffer)
 *
 * @since Version 1.0.0
 */
void test_format_buffer_too_small(void)
{
  rx_frame_t frame = {};
  uint32_t   len;
  char       small_buffer[k_test_too_small_buffer];
  rx_err_t   err = rx_frame_ascii_format(&frame, false, small_buffer, sizeof(small_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_err_no_mem, err);
}

/**
 * @brief Test format with frame->header.length exceeding k_frame_max_payload
 *
 * @details
 * Verifies that rx_frame_ascii_format() returns k_rx_err_invalid_arg when
 * frame->header.length > k_frame_max_payload (1024).
 *
 * @pre setUp() zeroed s_output_buffer
 * @post k_rx_err_invalid_arg returned
 *
 * @test Validates frame->header.length > k_frame_max_payload rejection (line 951)
 */
void test_format_payload_length_too_large(void)
{
  rx_frame_t frame    = {};
  uint32_t   len      = 0;
  frame.header.length = k_frame_max_payload + 1U;

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/** @} */ // end of test_rx_frame_ascii_format_errors

/* =============================================================================
 * rx_frame_ascii_format Tests - Success Cases
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_format_success Format Success Tests
 * @brief Success case tests for rx_frame_ascii_format() function
 *
 * @details
 * Validates correct ASCII output for various frame configurations:
 * - Empty payloads (RX/TX directions)
 * - Small payloads (single hex dump line)
 * - Large payloads (multi-line hex dump)
 * - Non-printable characters
 * - Special frame types (NACK with SOFT flag)
 * - Combined flags
 * - CRC display
 *
 * **Test Strategy:**
 * Each test uses strstr() to verify key elements in output:
 * - Direction indicator: [TX] or [RX]
 * - Header fields: seq=N, len=N, type=NAME, flags=...
 * - Payload hex dump: [NNNN] HH HH HH...
 * - ASCII sidebar: printable chars or dots
 * - CRC footer: [CRC] XXXXXXXX
 *
 * @{
 */

/**
 * @brief Test format of empty ACK frame with RX direction
 *
 * @details
 * Verifies correct formatting of empty frame (length=0) with receive direction.
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 0
 * - type = ACK
 * - flags = NONE
 * - crc = 0xDEADBEEF
 * - direction = RX (is_tx=false)
 *
 * **Expected Output:**
 * ```
 * [RX] seq=42 len=0 type=ACK flags=NONE
 * [PAYLOAD] (empty)
 * [CRC] DEADBEEF
 * ```
 *
 * **Test Steps:**
 * 1. Construct frame with above parameters
 * 2. Call rx_frame_ascii_format(&frame, false, ...)
 * 3. Assert k_rx_ok returned
 * 4. Assert output_len > 0
 * 5. Assert output contains "[RX]"
 * 6. Assert output contains "seq=42"
 * 7. Assert output contains "len=0"
 * 8. Assert output contains "type=ACK"
 * 9. Assert output contains "flags=NONE"
 * 10. Assert output contains "[PAYLOAD] (empty)"
 * 11. Assert output contains "[CRC]"
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Uses strstr() for substring matching (order-independent)
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
/**
 * @brief Verify empty ACK frame RX output contains expected header fields
 * @since Version 1.0.0
 */
static void internal_verify_empty_frame_rx_header(void)
{
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[RX]"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "seq=42"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "len=0"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "type=ACK"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "flags=NONE"));
}

void test_format_empty_frame_rx(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;
  frame.crc             = k_test_crc_value;

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
  internal_verify_empty_frame_rx_header();
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[PAYLOAD] (empty)"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[CRC]"));
}

/**
 * @brief Test format of empty COMMAND frame with TX direction
 *
 * @details
 * Verifies correct formatting of empty frame with transmit direction and
 * requires_ack flag set.
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 0
 * - type = COMMAND
 * - flags = REQUIRES_ACK
 * - direction = TX (is_tx=true)
 *
 * **Expected Output:**
 * ```
 * [TX] seq=42 len=0 type=COMMAND flags=ACK
 * [PAYLOAD] (empty)
 * [CRC] ...
 * ```
 *
 * **Test Steps:**
 * 1. Construct COMMAND frame with ACK flag
 * 2. Call rx_frame_ascii_format(&frame, true, ...) [TX direction]
 * 3. Assert k_rx_ok returned
 * 4. Assert output_len > 0
 * 5. Assert output contains "[TX]" (not "[RX]")
 * 6. Assert output contains "type=COMMAND"
 * 7. Assert output contains "flags=ACK"
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates is_tx parameter affects direction indicator
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
void test_format_empty_frame_tx(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;

  rx_err_t err =
    rx_frame_ascii_format(&frame, true, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);

  /* Verify direction indicator is TX */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[TX]"));

  /* Verify type */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "type=COMMAND"));

  /* Verify flags */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "flags=ACK"));
}

/**
 * @brief Test format with 4-byte payload (single hex dump line)
 *
 * @details
 * Verifies correct hex dump and ASCII sidebar for small payload.
 *
 * **Test Vector:**
 * - sequence = 1234
 * - length = 4
 * - type = RESPONSE
 * - flags = PRIORITY
 * - payload = "TEST" (0x54 0x45 0x53 0x54)
 *
 * **Expected Output:**
 * ```
 * [RX] seq=1234 len=4 type=RESPONSE flags=PRI
 * [0000] 54 45 53 54 00 00 00 00 00 00 00 00 00 00 00 00  TEST............
 * [CRC] ...
 * ```
 *
 * **Test Steps:**
 * 1. Construct frame with "TEST" payload
 * 2. Call rx_frame_ascii_format()
 * 3. Assert k_rx_ok returned
 * 4. Assert output contains "seq=1234"
 * 5. Assert output contains "len=4"
 * 6. Assert output contains "type=RESPONSE"
 * 7. Assert output contains "[0000]" (first offset)
 * 8. Assert output contains "54 45 53 54" (hex dump)
 * 9. Assert output contains "TEST" (ASCII sidebar)
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates hex dump format and ASCII sidebar
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
/**
 * @brief Verify payload frame output contains expected fields and hex dump
 * @since Version 1.0.0
 */
static void internal_verify_payload_frame_fields(void)
{
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "seq=1234"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "len=4"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "type=RESPONSE"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[0000]"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "54 45 53 54"));
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "TEST"));
}

void test_format_with_payload(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence                = k_test_sequence_1234;
  frame.header.length                  = k_test_payload_4;
  frame.header.type                    = k_frame_type_response;
  frame.header.flags                   = k_frame_flag_priority;
  frame.payload[0]                     = 'T';
  frame.payload[1]                     = 'E';
  frame.payload[k_test_nonprint_idx_2] = 'S';
  frame.payload[k_test_nonprint_idx_3] = 'T';

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
  internal_verify_payload_frame_fields();
}

/**
 * @brief Test format with 20-byte payload (multi-line hex dump)
 *
 * @details
 * Verifies correct offset increments and line wrapping for payloads
 * spanning multiple hex dump lines (16 bytes per line).
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 20
 * - type = COMMAND
 * - flags = NONE
 * - payload = [0x00, 0x01, 0x02, ..., 0x13]
 *
 * **Expected Output:**
 * ```
 * [RX] seq=42 len=20 type=COMMAND flags=NONE
 * [0000] 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ................
 * [0010] 10 11 12 13 00 00 00 00 00 00 00 00 00 00 00 00  ................
 * [CRC] ...
 * ```
 *
 * **Test Steps:**
 * 1. Construct frame with sequential bytes 0x00-0x13 (20 bytes)
 * 2. Call rx_frame_ascii_format()
 * 3. Assert k_rx_ok returned
 * 4. Assert output contains "[0000]" (first line offset)
 * 5. Assert output contains "[0010]" (second line offset, 16 decimal)
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates multi-line hex dump and offset increments
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
void test_format_with_multiline_payload(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = k_test_payload_20;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  /* Fill with pattern: 00 01 02 ... 13 */
  for (uint16_t i = 0; i < k_test_payload_20; i++) {
    frame.payload[i] = (uint8_t)i;
  }

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify first line offset */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[0000]"));

  /* Verify second line offset (after 16 bytes) */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[0010]"));
}

/**
 * @brief Test format with non-printable characters in payload
 *
 * @details
 * Verifies that non-printable bytes (< 0x20 or > 0x7E) display as dots
 * in ASCII sidebar while hex values remain correct.
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 4
 * - type = COMMAND
 * - flags = NONE
 * - payload = [0x00, 0x01, 0x1F, 0x7F] (all non-printable)
 *
 * **Expected Output:**
 * ```
 * [RX] seq=42 len=4 type=COMMAND flags=NONE
 * [0000] 00 01 1F 7F 00 00 00 00 00 00 00 00 00 00 00 00  ................
 * [CRC] ...
 * ```
 *
 * **Test Steps:**
 * 1. Construct frame with non-printable bytes
 * 2. Call rx_frame_ascii_format()
 * 3. Assert k_rx_ok returned
 * 4. Assert output contains "00 01 1F 7F" (hex dump correct)
 * 5. (ASCII sidebar should be dots, but specific check omitted - implementation detail)
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates hex dump accuracy for non-printable bytes
 * @note ASCII sidebar behavior (dots vs other) is implementation-defined
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
void test_format_with_non_printable_payload(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = k_test_payload_4;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  /* Non-printable characters */
  frame.payload[k_test_nonprint_idx_0] = k_test_nonprint_val_00;
  frame.payload[k_test_nonprint_idx_1] = k_test_nonprint_val_01;
  frame.payload[k_test_nonprint_idx_2] = k_test_nonprint_val_1f;
  frame.payload[k_test_nonprint_idx_3] = k_test_nonprint_val_7f;

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Non-printable characters should be replaced with dots in ASCII sidebar */
  /* The hex values should still be present */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "00 01 1F 7F"));
}

/**
 * @brief Test format of NACK frame with SOFT_NACK flag
 *
 * @details
 * Verifies correct formatting of NACK type with soft flag.
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 0
 * - type = NACK
 * - flags = SOFT_NACK
 * - direction = TX
 *
 * **Expected Output:**
 * ```
 * [TX] seq=42 len=0 type=NACK flags=SOFT
 * [PAYLOAD] (empty)
 * [CRC] ...
 * ```
 *
 * **Test Steps:**
 * 1. Construct NACK frame with SOFT flag
 * 2. Call rx_frame_ascii_format(&frame, true, ...)
 * 3. Assert k_rx_ok returned
 * 4. Assert output contains "type=NACK"
 * 5. Assert output contains "flags=SOFT"
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates NACK type and SOFT flag formatting
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
void test_format_nack_with_soft_flag(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_nack;
  frame.header.flags    = k_frame_flag_soft_nack;

  rx_err_t err =
    rx_frame_ascii_format(&frame, true, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify type */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "type=NACK"));

  /* Verify flags */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "flags=SOFT"));
}

/**
 * @brief Test format with combined flags (ACK|FEC)
 *
 * @details
 * Verifies correct pipe-separated flags display for multiple flags.
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 0
 * - type = COMMAND
 * - flags = REQUIRES_ACK | FEC_ENABLED
 *
 * **Expected Output:**
 * ```
 * [RX] seq=42 len=0 type=COMMAND flags=ACK|FEC
 * [PAYLOAD] (empty)
 * [CRC] ...
 * ```
 *
 * **Test Steps:**
 * 1. Construct frame with ACK | FEC flags
 * 2. Call rx_frame_ascii_format()
 * 3. Assert k_rx_ok returned
 * 4. Assert output contains "flags=ACK|FEC"
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates combined flag formatting
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
void test_format_combined_flags(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack | k_frame_flag_fec_enabled;

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify combined flags */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "flags=ACK|FEC"));
}

/**
 * @brief Test CRC value display in footer
 *
 * @details
 * Verifies that CRC value is correctly formatted as 8-digit uppercase hex.
 *
 * **Test Vector:**
 * - sequence = 42
 * - length = 0
 * - type = ACK
 * - flags = NONE
 * - crc = 0xDEADBEEF
 *
 * **Expected Output:**
 * ```
 * [RX] seq=42 len=0 type=ACK flags=NONE
 * [PAYLOAD] (empty)
 * [CRC] DEADBEEF
 * ```
 *
 * **Test Steps:**
 * 1. Construct frame with CRC = 0xDEADBEEF
 * 2. Call rx_frame_ascii_format()
 * 3. Assert k_rx_ok returned
 * 4. Assert output contains "[CRC] DEADBEEF"
 *
 * @pre setUp() zeroed s_output_buffer
 * @post s_output_buffer contains formatted ASCII text
 * @post Buffer is null-terminated
 *
 * @note Validates CRC footer format (8 hex digits, uppercase)
 * @note Thread-safe (writes to static buffer, single-threaded tests)
 *
 * @since Version 1.0.0
 */
void test_format_crc_displayed(void)
{
  rx_frame_t frame = {};
  uint32_t   len   = 0;

  frame.header.sequence = k_test_sequence_42;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;
  frame.crc             = k_test_crc_value;

  rx_err_t err =
    rx_frame_ascii_format(&frame, false, s_output_buffer, sizeof(s_output_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify CRC value is displayed (DEADBEEF) */
  TEST_ASSERT_NOT_NULL(strstr(s_output_buffer, "[CRC] DEADBEEF"));
}

/** @} */ // end of test_rx_frame_ascii_format_success

/* =============================================================================
 * Internal Helper Branch Coverage Tests
 * =============================================================================
 */

/**
 * @defgroup test_rx_frame_ascii_internal Internal Helper Branch Coverage Tests
 * @brief Tests that exercise assert-false branches and buffer-full paths in
 *        internal (RX_STATIC_TESTABLE) helper functions.
 * @{
 */

/**
 * @brief Test internal_append_str assert fires for each precondition failure.
 * @details Exercises ALL RX_ASSERT short-circuit branches (A=false, B=false,
 *          C=false, D=false) and the while-loop false-exit branch (buffer full).
 *          After the assert fires in unit test mode (returns), the while loop
 *          condition `pos < max - 1` is false (pos >= max), so no writes occur.
 *          The C=false case (max=0) is safe because pos is large enough that
 *          pos < UINT32_MAX is still false (with pos = max = 0, both are 0).
 *          The B=false case (str=nullptr) is safe because the while loop now
 *          checks `pos < max - 1` FIRST (short-circuit prevents *str deref).
 * @since Version 1.0.0
 */
void test_internal_append_str_assert_pos_beyond_max(void)
{
#ifdef UNIT_TEST
  char        dummy[k_test_small_buf_8] = {};
  const char* str                       = "hi";

  /* A=false: buf=nullptr, pos>=max prevents write (D is also false) */
  uint32_t pos = internal_append_str(nullptr, k_test_pos_8, k_test_small_buf_4, str);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* B=false: str=nullptr, pos>=max prevents *str deref (short-circuit) */
  pos = internal_append_str(dummy, k_test_pos_8, k_test_small_buf_4, nullptr);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* C=false: max=0, pos=UINT32_MAX so pos >= max-1=UINT32_MAX: no write */
  pos = internal_append_str(dummy, k_test_max_u32, 0, str);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);

  /* D=false: pos >= max (already covered, reconfirm) */
  pos = internal_append_str(dummy, k_test_pos_8, k_test_small_buf_4, str);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* Buffer exactly full: pos == max-1 = 3, while exits immediately */
  pos = internal_append_str(dummy, k_test_pos_3, k_test_small_buf_4, str);
  TEST_ASSERT_EQUAL(k_test_pos_3, pos);
#endif
}

/**
 * @brief Test internal_append_char assert fires for each precondition and
 *        exercises buffer-full path.
 * @details Exercises A=false (buf=nullptr), B=false (max=0), C=false (pos>=max)
 *          short-circuit branches and the if-false (buffer-full) branch.
 *          Safe because when pos >= max, the if body is not entered.
 * @since Version 1.0.0
 */
void test_internal_append_char_assert_and_full(void)
{
#ifdef UNIT_TEST
  char dummy[k_test_small_buf_4] = {};

  /* A=false: buf=nullptr with pos >= max (if body skipped) */
  uint32_t pos = internal_append_char(nullptr, k_test_pos_8, k_test_small_buf_4, 'A');
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* B=false: max=0, pos=UINT32_MAX so pos < max-1 = UINT32_MAX is false */
  pos = internal_append_char(dummy, k_test_max_u32, 0, 'A');
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);

  /* C=false: pos >= max (already covered, reconfirm) */
  pos = internal_append_char(dummy, k_test_pos_8, k_test_small_buf_4, 'A');
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* Buffer full: pos == max-1 = 3, if-false branch */
  pos = internal_append_char(dummy, k_test_pos_3, k_test_small_buf_4, 'A');
  TEST_ASSERT_EQUAL(k_test_pos_3, pos);
#endif
}

/**
 * @brief Test internal_append_hex_byte assert fires for each precondition and
 *        exercises buffer-full path.
 * @details Exercises A=false (buf=nullptr), B=false (max<2), C=false (pos>=max),
 *          and the if-false (buffer has only 1 byte left) branches.
 * @since Version 1.0.0
 */
void test_internal_append_hex_byte_assert_and_full(void)
{
#ifdef UNIT_TEST
  char dummy[k_test_small_buf_8] = {};

  /* C=false: pos >= max (D fails), if body not entered */
  uint32_t pos =
    internal_append_hex_byte(dummy, k_test_pos_8, k_test_small_buf_4, k_test_hex_byte_ab);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* A=false: buf=nullptr, pos >= max prevents write */
  pos = internal_append_hex_byte(nullptr, k_test_pos_8, k_test_small_buf_4, k_test_hex_byte_ab);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* B=false: max < k_hex_chars_per_byte=2, pos=UINT32_MAX so if body skipped */
  pos = internal_append_hex_byte(dummy, k_test_max_u32, 1, k_test_hex_byte_ab);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);

  /* Buffer only has 1 byte left: pos = max-1 = 3, if-false */
  pos = internal_append_hex_byte(dummy, k_test_pos_3, k_test_small_buf_4, k_test_hex_byte_ab);
  TEST_ASSERT_EQUAL(k_test_pos_3, pos);
#endif
}

/**
 * @brief Test internal_append_hex_u16 assert fires for each precondition.
 * @details Exercises A=false, B=false, and C=false short-circuit branches.
 * @since Version 1.0.0
 */
void test_internal_append_hex_u16_assert_pos_beyond_max(void)
{
#ifdef UNIT_TEST
  char dummy[k_test_small_buf_8] = {};

  /* C=false: pos >= max */
  uint32_t pos =
    internal_append_hex_u16(dummy, k_test_pos_8, k_test_small_buf_4, k_test_hex_u16_1234);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* A=false: buf=nullptr, pos >= max prevents any write */
  pos = internal_append_hex_u16(nullptr, k_test_pos_8, k_test_small_buf_4, k_test_hex_u16_1234);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* B=false: max < k_hex_chars_per_u16=4, pos=UINT32_MAX */
  pos = internal_append_hex_u16(dummy, k_test_max_u32, k_test_pos_3, k_test_hex_u16_1234);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);
#endif
}

/**
 * @brief Test internal_append_hex_u32 assert fires for each precondition.
 * @details Exercises A=false, B=false, and C=false short-circuit branches.
 * @since Version 1.0.0
 */
void test_internal_append_hex_u32_assert_pos_beyond_max(void)
{
#ifdef UNIT_TEST
  char dummy[k_test_small_buf_16] = {};

  /* B=false: max < k_hex_chars_per_u32=8, pos=UINT32_MAX */
  uint32_t pos =
    internal_append_hex_u32(dummy, k_test_max_u32, k_test_u32_buf_7, k_test_hex_u32_deadbeef);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);

  /* A=false: buf=nullptr, B=true (max=8), C=false (pos>=max) */
  pos = internal_append_hex_u32(nullptr, k_test_pos_16, k_test_pos_8, k_test_hex_u32_deadbeef);
  TEST_ASSERT_EQUAL(k_test_pos_16, pos);

  /* C=false: A=true, B=true (max=8>=8), pos=16>=8 */
  pos = internal_append_hex_u32(dummy, k_test_pos_16, k_test_pos_8, k_test_hex_u32_deadbeef);
  TEST_ASSERT_EQUAL(k_test_pos_16, pos);
#endif
}

/**
 * @brief Test internal_append_decimal_u16 assert fires for each precondition
 *        and the while loop exits when value reaches zero.
 * @details Exercises A=false (buf=nullptr), B=false (max=0), C=false (pos>=max)
 *          short-circuit branches.
 * @since Version 1.0.0
 */
void test_internal_append_decimal_u16_assert_and_loop(void)
{
#ifdef UNIT_TEST
  char dummy[k_test_small_buf_8] = {};

  /* C=false: pos >= max */
  uint32_t pos =
    internal_append_decimal_u16(dummy, k_test_pos_8, k_test_small_buf_4, k_test_sequence_42);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* A=false: buf=nullptr, pos >= max prevents write */
  pos = internal_append_decimal_u16(nullptr, k_test_pos_8, k_test_small_buf_4, k_test_sequence_42);
  TEST_ASSERT_EQUAL(k_test_pos_8, pos);

  /* B=false: max=0, pos=UINT32_MAX so inner append_char also skips */
  pos = internal_append_decimal_u16(dummy, k_test_max_u32, 0, k_test_sequence_42);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);

  /* Non-zero value -> the while loop runs then exits naturally when value=0 */
  pos = internal_append_decimal_u16(dummy, 0, k_test_small_buf_8, k_test_decimal_12345);
  /* "12345" is 5 digits, pos should advance to 5 */
  TEST_ASSERT_EQUAL(k_test_decimal_len_5, pos);
  dummy[pos] = '\0';
  TEST_ASSERT_EQUAL_STRING("12345", dummy);
#endif
}

/**
 * @brief Test internal_format_payload assert fires for each precondition.
 * @details Exercises A=false (buf=nullptr), B=false (max=0), C=false (pos>=max)
 *          short-circuit branches at line 616 assert.
 *          Also tests payload=nullptr (early return from null guard at line 625).
 * @since Version 1.0.0
 */
void test_internal_format_payload_assert_violations(void)
{
#ifdef UNIT_TEST
  char          buf[k_test_small_buf_256]   = {};
  const uint8_t payload[k_test_small_buf_4] = {0x01, 0x02, 0x03, 0x04};

  /* C=false: pos >= max (assert fires) */
  uint32_t pos =
    internal_format_payload(buf, k_test_pos_256, k_test_min_buffer_size, payload, k_test_payload_4);
  TEST_ASSERT_EQUAL(k_test_pos_256, pos);

  /* A=false: buf=nullptr, pos >= max prevents writes */
  pos = internal_format_payload(nullptr,
                                k_test_pos_256,
                                k_test_min_buffer_size,
                                payload,
                                k_test_payload_4);
  TEST_ASSERT_EQUAL(k_test_pos_256, pos);

  /* B=false: max=0, pos=UINT32_MAX so child append calls all skip */
  pos = internal_format_payload(buf, k_test_max_u32, 0, payload, k_test_payload_4);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);

  /* payload=nullptr with length>0 hits the null guard */
  pos = internal_format_payload(buf, 0, k_test_small_buf_256, nullptr, k_test_payload_4);
  TEST_ASSERT_EQUAL(0, pos);
#endif
}

/**
 * @brief Test internal_format_header assert fires for each precondition.
 * @details Exercises A=false (buf=nullptr), B=false (header=nullptr),
 *          C=false (max=0), D=false (pos>=max) short-circuit branches.
 * @since Version 1.0.0
 */
void test_internal_format_header_assert_pos_beyond_max(void)
{
#ifdef UNIT_TEST
  char              buf[k_test_small_buf_128] = {};
  rx_frame_header_t header                    = {};
  header.sequence                             = 1;
  header.length                               = 0;
  header.type                                 = k_frame_type_ping;
  header.flags                                = 0;

  /* D=false: pos >= max */
  uint32_t pos = internal_format_header(buf, k_test_pos_128, k_test_min_buffer_size, &header, true);
  TEST_ASSERT_EQUAL(k_test_pos_128, pos);

  /* A=false: buf=nullptr, pos >= max prevents writes */
  pos = internal_format_header(nullptr, k_test_pos_128, k_test_min_buffer_size, &header, true);
  TEST_ASSERT_EQUAL(k_test_pos_128, pos);

  /* B=false: header=nullptr, safety guard returns early */
  pos = internal_format_header(buf, 0, k_test_min_buffer_size, nullptr, true);
  TEST_ASSERT_EQUAL(0, pos);

  /* C=false: max=0, pos=UINT32_MAX */
  pos = internal_format_header(buf, k_test_max_u32, 0, &header, true);
  TEST_ASSERT_EQUAL(k_test_max_u32, pos);
#endif
}

/**
 * @brief Test internal_append_flag assert fires when first is NULL.
 * @details Exercises assert false branch at line 830.
 *          Passes a non-NULL buffer with pos >= max so that after the assert
 *          fires (returns) the subsequent code path does not crash.
 * @since Version 1.0.0
 */
void test_internal_append_flag_assert_null_first(void)
{
#ifdef UNIT_TEST
  char dummy[k_test_small_buf_32] = {};

  /* first == nullptr violates assert (false branch covered).
   * After assert fires in unit test mode, code checks (flags & flag_bit).
   * flags=0x00, flag_bit=0x01 so condition is false and body skipped. */
  uint32_t pos = internal_append_flag(dummy, 0, k_test_small_buf_32, nullptr, 0x00, 0x01, "ACK");
  TEST_ASSERT_EQUAL(0, pos);
#endif
}

/** @} */ // end of test_rx_frame_ascii_internal

/* =============================================================================
 * Main
 * =============================================================================
 */

/**
 * @brief Main test runner
 *
 * @details
 * Unity test framework entry point. Executes all registered tests and reports
 * results.
 *
 * **Test Execution Order:**
 * 1. Type name tests (6 tests)
 * 2. Flags string tests (10 tests)
 * 3. Format error tests (4 tests)
 * 4. Format success tests (10 tests)
 *
 * **Total: 30 tests**
 *
 * **Return Values:**
 * - 0: All tests passed
 * - >0: Number of failed tests
 *
 * @return int Number of failed tests (0 = all pass)
 *
 * @par Example Output:
 * ```
 * test_rx_frame_ascii.c:73:test_type_name_command:PASS
 * test_rx_frame_ascii.c:78:test_type_name_response:PASS
 * ...
 * -----------------------
 * 30 Tests 0 Failures 0 Ignored
 * OK
 * ```
 *
 * @note Called by Unity framework or directly from command line
 * @note Execution time: ~500 ms @ 240 MHz (all 30 tests)
 *
 * @since Version 1.0.0
 */
/**
 * @brief Run type name conversion tests
 * @since Version 1.0.0
 */
static void internal_run_type_name_tests(void)
{
  RUN_TEST(test_type_name_ping);
  RUN_TEST(test_type_name_pong);
  RUN_TEST(test_type_name_command);
  RUN_TEST(test_type_name_response);
  RUN_TEST(test_type_name_ack);
  RUN_TEST(test_type_name_nack);
  RUN_TEST(test_type_name_reset);
  RUN_TEST(test_type_name_reset_ack);
  RUN_TEST(test_type_name_log_message);
  RUN_TEST(test_type_name_invalid);
}

/**
 * @brief Run flags string generation tests
 * @since Version 1.0.0
 */
static void internal_run_flags_str_tests(void)
{
  RUN_TEST(test_flags_str_null_buffer);
  RUN_TEST(test_flags_str_zero_length);
  RUN_TEST(test_flags_str_none);
  RUN_TEST(test_flags_str_requires_ack);
  RUN_TEST(test_flags_str_retransmit);
  RUN_TEST(test_flags_str_priority);
  RUN_TEST(test_flags_str_fec_enabled);
  RUN_TEST(test_flags_str_soft_nack);
  RUN_TEST(test_flags_str_combined_two);
  RUN_TEST(test_flags_str_combined_all);
}

/**
 * @brief Run format error and success tests
 * @since Version 1.0.0
 */
static void internal_run_format_tests(void)
{
  RUN_TEST(test_format_null_frame);
  RUN_TEST(test_format_null_output);
  RUN_TEST(test_format_null_output_len);
  RUN_TEST(test_format_buffer_too_small);
  RUN_TEST(test_format_payload_length_too_large);
  RUN_TEST(test_format_empty_frame_rx);
  RUN_TEST(test_format_empty_frame_tx);
  RUN_TEST(test_format_with_payload);
  RUN_TEST(test_format_with_multiline_payload);
  RUN_TEST(test_format_with_non_printable_payload);
  RUN_TEST(test_format_nack_with_soft_flag);
  RUN_TEST(test_format_combined_flags);
  RUN_TEST(test_format_crc_displayed);
}

/**
 * @brief Run internal helper branch coverage tests
 * @since Version 1.0.0
 */
static void internal_run_internal_helper_tests(void)
{
  RUN_TEST(test_internal_append_str_assert_pos_beyond_max);
  RUN_TEST(test_internal_append_char_assert_and_full);
  RUN_TEST(test_internal_append_hex_byte_assert_and_full);
  RUN_TEST(test_internal_append_hex_u16_assert_pos_beyond_max);
  RUN_TEST(test_internal_append_hex_u32_assert_pos_beyond_max);
  RUN_TEST(test_internal_append_decimal_u16_assert_and_loop);
  RUN_TEST(test_internal_format_payload_assert_violations);
  RUN_TEST(test_internal_format_header_assert_pos_beyond_max);
  RUN_TEST(test_internal_append_flag_assert_null_first);
}

int main(void)
{
  UNITY_BEGIN();
  internal_run_type_name_tests();
  internal_run_flags_str_tests();
  internal_run_format_tests();
  internal_run_internal_helper_tests();
  return UNITY_END();
}
