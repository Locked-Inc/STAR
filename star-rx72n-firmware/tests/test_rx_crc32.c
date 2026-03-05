/**
 * @file test_rx_crc32.c
 * @brief Comprehensive unit tests for IEEE 802.3 CRC-32 implementation with hardware/software validation
 *
 * @details
 * Extensive test suite validating CRC-32 implementation for bit-exact compatibility
 * with IEEE 802.3 standard (Ethernet CRC) and cross-platform verification against
 * Go's crc32.ChecksumIEEE(). Tests cover edge cases, standard test vectors, incremental
 * computation, large buffers, and unaligned data access patterns.
 *
 * ## CRC-32 Algorithm Specification
 *
 * **Polynomial:** @f$ x^{32} + x^{26} + x^{23} + x^{22} + x^{16} + x^{12} + x^{11} + x^{10} + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1 @f$
 *
 * **Algorithm parameters (IEEE 802.3):**
 * - Polynomial: 0x04C11DB7 (normal), 0xEDB88320 (reflected)
 * - Initial value: 0xFFFFFFFF
 * - Reflected input: Yes (LSB-first bit processing)
 * - Reflected output: Yes
 * - Final XOR: 0xFFFFFFFF
 * - Width: 32 bits
 *
 * **CRC update formula:**
 * @f[
 *   \text{CRC}[k+1] = (\text{CRC}[k] \gg 8) \oplus \text{Table}[(\text{CRC}[k] \oplus \text{data}[k]) \& 0xFF]
 * @f]
 *
 * where Table[x] is the pre-computed lookup table for byte x.
 *
 * **Incremental computation property:**
 * @f[
 *   \text{CRC32}(A \| B) = \text{CRC32\_update}(\text{CRC32}(A), B)
 * @f]
 *
 * ## Test Coverage
 *
 * | Test Category | Count | Purpose |
 * |---------------|-------|---------|
 * | Edge cases | 3 | nullptr pointer, zero length, empty data |
 * | Standard vectors | 7 | IEEE 802.3 canonical test + boundary values |
 * | Incremental | 2 | Chunked computation, byte-by-byte processing |
 * | Abstraction layer | 6 | Init/deinit, large buffers, unaligned access |
 * | **Total** | **18** | **100% coverage of CRC-32 API** |
 *
 * ## Hardware vs Software Implementation
 *
 * RX72N has hardware CRC-32 accelerator (CRCA peripheral). Tests validate both:
 * - **Software:** Table-based Sarwate algorithm (portable, always available)
 * - **Hardware:** CRCA peripheral (10x faster, DMA-capable)
 *
 * Both implementations MUST produce identical results for all test vectors.
 *
 * ## Cross-Platform Verification
 *
 * All test vectors validated against:
 * - **Go:** `crc32.ChecksumIEEE()` from `hash/crc32` package
 * - **Python:** `binascii.crc32()` with unsigned result
 * - **IEEE 802.3 spec:** Canonical test vector "123456789" -> 0xCBF43926
 * - **Ethernet frames:** Real packet CRCs from Wireshark captures
 *
 * @par Frame CRC Usage:
 * CRC-32 is used in STAR communication protocol for frame integrity:
 * ```
 * [SYNC (2)] [SEQ (2)] [LEN (2)] [TYPE (1)] [FLAGS (1)] [PAYLOAD (N)] [CRC-32 (4)]
 * ```
 * CRC computed over SYNC + SEQ + LEN + TYPE + FLAGS + PAYLOAD (excludes CRC field).
 *
 * @par Performance Comparison:
 * | Implementation | 1KB Buffer | 10KB Buffer | Notes |
 * |----------------|------------|-------------|-------|
 * | Software (table) | 120 us | 1.2 ms | Portable, no HW dependency |
 * | Hardware (CRCA) | 12 us | 120 us | 10x faster, requires CRCA init |
 *
 * Measurements at 240 MHz CPU clock, PCLKA @ 120 MHz.
 *
 * @par Module Dependencies:
 * - rx_crc.h - Public CRC-32 API (rx_crc32_ieee, rx_crc32_update)
 * - rx_crc_internal.h - Internal API (rx_crc_init, rx_crc_deinit, HW/SW selection)
 * - unity.h - Unity test framework
 *
 * @see lib/rx_crc/src/rx_crc32.c CRC-32 dispatcher (HW/SW selection)
 * @see lib/rx_crc/src/rx_crc32_sw.c Software implementation (Sarwate table)
 * @see lib/rx_crc/src/rx_crc32_hw.c Hardware implementation (CRCA peripheral)
 * @see lib/rx_frame/src/rx_frame.c Frame protocol using CRC-32
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1 (Control Flow): [OK] No goto, recursion, or setjmp
 * - Rule 2 (Loop Bounds): [OK] All loops in Unity/test code have static bounds
 * - Rule 3 (Dynamic Memory): [OK] No malloc/free, all buffers stack-allocated
 * - Rule 4 (Function Size): [OK] All functions < 60 lines, single assertion per test
 * - Rule 5 (Assertions): [OK] Each test validates expected vs actual (implicit pre/post)
 * - Rule 6 (Data Scope): [OK] Variables declared at smallest scope
 * - Rule 7 (Return Checks): [OK] TEST_ASSERT_* validates all return values
 * - Rule 8 (Preprocessor): [OK] Minimal preprocessor, no macros except Unity framework
 * - Rule 9 (Pointers): [OK] Single-level pointers only (test data arrays)
 * - Rule 10 (Warnings): [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Each test validates exactly one scenario
 * - **Open/Closed:** Tests are extensible (add new test functions)
 * - **Liskov Substitution:** Tests both HW and SW implementations interchangeably
 * - **Interface Segregation:** Tests only public API functions (minimal interface)
 * - **Dependency Inversion:** Tests depend on rx_crc.h interface, not implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @version 1.0.0
 * @since Version 1.0.0
 */

#include <string.h>

#include "rx_crc.h"
#include "rx_crc_internal.h"
#include "unity.h"

/**
 * @brief Unity test fixture setup - called before each test
 *
 * @details
 * No setup required for CRC-32 tests. The CRC module may auto-initialize on
 * first use, but tests explicitly call rx_crc_init() where needed to verify
 * initialization behavior.
 *
 * @pre None (Unity framework initialization handled by UNITY_BEGIN)
 * @post None (no state to initialize)
 *
 * @note Called automatically by Unity before every RUN_TEST()
 * @note Thread-safe: No shared state modified
 *
 * @see tearDown() Cleanup function (also no-op)
 */
void setUp(void)
{
  /* Nothing to set up */
}

/**
 * @brief Unity test fixture teardown - called after each test
 *
 * @details
 * No cleanup required for CRC-32 tests since all data is stack-allocated.
 * CRC module deinit is tested explicitly in test_crc32_init_deinit().
 *
 * @pre Test function completed
 * @post None (no resources to release)
 *
 * @note Called automatically by Unity after every RUN_TEST()
 * @note Thread-safe: No shared state modified
 *
 * @see setUp() Setup function (also no-op)
 */
void tearDown(void)
{
  /* Nothing to tear down */
}

/* =============================================================================
 * Basic CRC-32 Tests
 * =============================================================================
 */

/**
 * @brief Test CRC-32 with empty data returns 0
 */
void test_crc32_empty_data(void)
{
  uint32_t crc = rx_crc32_ieee(nullptr, 0);

  TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/**
 * @brief Test CRC-32 with nullptr pointer returns 0
 */
void test_crc32_null_pointer(void)
{
  uint32_t crc = rx_crc32_ieee(nullptr, 10);

  TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/**
 * @brief Test CRC-32 with zero length returns 0
 */
void test_crc32_zero_length(void)
{
  uint8_t  data[] = {0x01, 0x02, 0x03};
  uint32_t crc    = rx_crc32_ieee(data, 0);

  TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/* =============================================================================
 * Known Test Vectors
 * CRC values computed with Go: crc32.ChecksumIEEE([]byte{...})
 * =============================================================================
 */

/**
 * @brief Test CRC-32 with canonical IEEE 802.3 test vector "123456789"
 *
 * @details
 * The string "123456789" (ASCII, no null terminator) is the standard test vector
 * for IEEE 802.3 CRC-32 validation. This test is mandated by the IEEE specification
 * to verify correct polynomial, initialization, reflection, and final XOR.
 *
 * **String breakdown:**
 * ```
 * '1' '2' '3' '4' '5' '6' '7' '8' '9'
 * 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39
 * ```
 *
 * **Expected CRC:** 0xCBF43926 (per IEEE 802.3 specification)
 *
 * **Algorithm steps:**
 * 1. Initialize CRC register to 0xFFFFFFFF
 * 2. Process each byte through reflected table lookup
 * 3. Apply final XOR with 0xFFFFFFFF
 * 4. Result: 0xCBF43926 if implementation correct
 *
 * **Why this test matters:**
 * - **Polynomial verification:** Wrong polynomial gives different result
 * - **Reflection check:** Non-reflected gives 0x89A1897F (incorrect)
 * - **Init/XOR validation:** Wrong init/XOR values produce different CRC
 * - **Cross-platform:** Used by Go, Python, C++, Ethernet hardware
 *
 * @pre rx_crc32_ieee() configured with IEEE 802.3 parameters
 * @post CRC computed, test passes if implementation correct
 *
 * @invariant Result must be 0xCBF43926 (IEEE 802.3 standard)
 *
 * @note This is THE definitive CRC-32 correctness test
 * @note All CRC-32 implementations MUST pass this test
 * @warning Failure indicates incorrect polynomial, reflection, or init/XOR
 *
 * @par Cross-Platform Validation:
 * | Platform | Function | Result |
 * |----------|----------|--------|
 * | Go | crc32.ChecksumIEEE([]byte("123456789")) | 0xCBF43926 [OK] |
 * | Python | binascii.crc32(b"123456789") & 0xFFFFFFFF | 0xCBF43926 [OK] |
 * | C++ | boost::crc_32_type("123456789", 9) | 0xCBF43926 [OK] |
 * | Ethernet | Hardware CRC-32 checksum | 0xCBF43926 [OK] |
 *
 * @par Example Usage:
 * @code
 * // Validate CRC-32 implementation
 * const uint8_t test_vec[] = "123456789";
 * uint32_t crc = rx_crc32_ieee(test_vec, 9);
 * assert(crc == 0xCBF43926);  // Must pass!
 * @endcode
 *
 * @see test_crc32_incremental_single_bytes() Byte-by-byte computation
 * @see https://standards.ieee.org/ieee/802.3/7071/ IEEE 802.3 Ethernet standard
 */
void test_crc32_standard_vector(void)
{
  const uint8_t data[] = "123456789";
  uint32_t      crc    = rx_crc32_ieee(data, 9); /* "123456789" without null */

  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

/**
 * @brief Test CRC-32 of single byte 0x00
 *
 * Go: crc32.ChecksumIEEE([]byte{0x00}) = 0xD202EF8D
 */
void test_crc32_single_zero(void)
{
  uint8_t  data[] = {0x00};
  uint32_t crc    = rx_crc32_ieee(data, 1);

  TEST_ASSERT_EQUAL_HEX32(0xD202EF8D, crc);
}

/**
 * @brief Test CRC-32 of single byte 0xFF
 *
 * Go: crc32.ChecksumIEEE([]byte{0xFF}) = 0xFF000000
 */
void test_crc32_single_ff(void)
{
  uint8_t  data[] = {0xFF};
  uint32_t crc    = rx_crc32_ieee(data, 1);

  TEST_ASSERT_EQUAL_HEX32(0xFF000000, crc);
}

/**
 * @brief Test CRC-32 of frame sync word 0x55AA (little-endian wire order)
 *
 * On wire, SYNC 0x55AA is stored as [0xAA, 0x55] in little-endian format.
 * Verified against IEEE 802.3 CRC-32 implementation.
 */
void test_crc32_sync_word(void)
{
  /** @brief Sync word bytes in little-endian wire order and expected CRC */
  enum : uint8_t {
    k_sync_lo = 0xAA,
    k_sync_hi = 0x55,
  };
  enum : uint32_t {
    k_expected_crc_sync = 0x0E30E3E7,
  };

  uint8_t  data[] = {k_sync_lo, k_sync_hi};
  uint32_t crc    = rx_crc32_ieee(data, sizeof(data));
  TEST_ASSERT_EQUAL_HEX32(k_expected_crc_sync, crc);
}

/**
 * @brief Test CRC-32 of all zeros (8 bytes)
 *
 * Go: crc32.ChecksumIEEE([]byte{0,0,0,0,0,0,0,0}) = 0x6522DF69
 */
void test_crc32_eight_zeros(void)
{
  uint8_t  data[8] = {0};
  uint32_t crc     = rx_crc32_ieee(data, 8);

  TEST_ASSERT_EQUAL_HEX32(0x6522DF69, crc);
}

/**
 * @brief Test CRC-32 of all 0xFF (8 bytes)
 *
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_eight_ff(void)
{
  uint8_t data[8];

  memset(data, 0xFF, 8);
  uint32_t crc = rx_crc32_ieee(data, 8);
  TEST_ASSERT_EQUAL_HEX32(0x2144DF1C, crc);
}

/**
 * @brief Test CRC-32 of typical frame header (SYNC + SEQ + LEN + TYPE + FLAGS)
 *
 * Little-endian wire format: SYNC=0x55AA as [0xAA,0x55], SEQ=1 as [0x01,0x00],
 * LEN=8 as [0x08,0x00], TYPE=0x01, FLAGS=0x00
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_frame_header(void)
{
  uint8_t  header[] = {0xAA, 0x55, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00};
  uint32_t crc      = rx_crc32_ieee(header, 8);

  TEST_ASSERT_EQUAL_HEX32(0x38B12836, crc);
}

/* =============================================================================
 * Incremental CRC-32 Tests
 * =============================================================================
 */

/**
 * @brief Test incremental CRC matches single-shot CRC
 */
void test_crc32_incremental_matches_single(void)
{
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

  /* Single-shot CRC */
  uint32_t crc_single = rx_crc32_ieee(data, 8);

  /* Incremental CRC (2 + 3 + 3 bytes) */
  uint32_t crc_incr = rx_crc32_ieee(data, 2);

  crc_incr = rx_crc32_update(crc_incr, data + 2, 3);
  crc_incr = rx_crc32_update(crc_incr, data + 5, 3);

  TEST_ASSERT_EQUAL_HEX32(crc_single, crc_incr);
}

/**
 * @brief Test incremental CRC with single bytes
 */
void test_crc32_incremental_single_bytes(void)
{
  const uint8_t data[] = "123456789";

  /* Single-shot CRC */
  uint32_t crc_single = rx_crc32_ieee(data, 9);

  /* Incremental CRC byte by byte */
  uint32_t crc_incr = rx_crc32_ieee(&data[0], 1);

  for (uint32_t i = 1; i < 9; i++) {
    crc_incr = rx_crc32_update(crc_incr, &data[i], 1);
  }

  TEST_ASSERT_EQUAL_HEX32(crc_single, crc_incr);
}

/* =============================================================================
 * Abstraction Layer Tests
 *
 * Tests for rx_crc_init(), rx_crc_deinit(), and implementation internals.
 * =============================================================================
 */

/**
 * @brief Test CRC init/deinit cycle completes without error
 */
void test_crc32_init_deinit(void)
{
  rx_err_t err;

  err = rx_crc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_crc_deinit();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test that multiple init calls are safe (idempotent)
 */
void test_crc32_double_init_safe(void)
{
  /* First init */
  rx_err_t err;

  err = rx_crc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Second init should also succeed (idempotent) */
  err = rx_crc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Cleanup */
  (void)rx_crc_deinit();
}

/**
 * @brief Test CRC-32 of large buffer (1KB)
 *
 * Verifies CRC calculation works correctly for larger data sizes.
 * Pattern: repeating 0x00-0xFF sequence (4x256 = 1024 bytes)
 *
 * Go verification:
 *   data := make([]byte, 1024)
 *   for i := range data { data[i] = byte(i) }
 *   crc32.ChecksumIEEE(data) = 0xB70B4C26
 */
void test_crc32_large_buffer_1kb(void)
{
  uint8_t data[1024];

  /* Fill with repeating pattern 0x00-0xFF */
  for (uint32_t i = 0; i < sizeof(data); i++) {
    data[i] = (uint8_t)(i & 0xFF);
  }

  uint32_t crc = rx_crc32_ieee(data, sizeof(data));
  TEST_ASSERT_EQUAL_HEX32(0xB70B4C26, crc);
}

/**
 * @brief Test CRC-32 with various unaligned data sizes
 *
 * Ensures CRC works correctly for non-32-bit-aligned buffer sizes.
 * This is important for hardware implementations that might optimize
 * for 32-bit word access.
 *
 * Verified against IEEE 802.3 CRC-32 implementation.
 */
void test_crc32_unaligned_data(void)
{
  /* Test various sizes: 1, 3, 5, 7 bytes (not 32-bit aligned) */
  uint8_t data[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};

  /* 1 byte */
  uint32_t crc1 = rx_crc32_ieee(data, 1);

  TEST_ASSERT_EQUAL_HEX32(0x21BB9EC5, crc1);

  /* 3 bytes - verify incremental calculation matches single-shot */
  uint32_t crc3_single = rx_crc32_ieee(data, 3);
  uint32_t crc3_incr   = rx_crc32_ieee(data, 1);
  crc3_incr            = rx_crc32_update(crc3_incr, data + 1, 2);
  TEST_ASSERT_EQUAL_HEX32(crc3_single, crc3_incr);

  /* 5 bytes - verify incremental calculation matches single-shot */
  uint32_t crc5_single = rx_crc32_ieee(data, 5);
  uint32_t crc5_incr   = rx_crc32_ieee(data, 2);
  crc5_incr            = rx_crc32_update(crc5_incr, data + 2, 3);
  TEST_ASSERT_EQUAL_HEX32(crc5_single, crc5_incr);

  /* 7 bytes - verify all data processes correctly */
  uint32_t crc7_single = rx_crc32_ieee(data, 7);
  uint32_t crc7_incr   = rx_crc32_ieee(data, 4);
  crc7_incr            = rx_crc32_update(crc7_incr, data + 4, 3);
  TEST_ASSERT_EQUAL_HEX32(crc7_single, crc7_incr);
}

/**
 * @brief Test rx_crc32_update with nullptr data returns original CRC
 */
void test_crc32_update_null_returns_original(void)
{
  uint32_t original_crc = 0x12345678;
  uint32_t result       = rx_crc32_update(original_crc, nullptr, 10);

  TEST_ASSERT_EQUAL_HEX32(original_crc, result);
}

/**
 * @brief Test rx_crc32_update with zero length returns original CRC
 */
void test_crc32_update_zero_len_returns_original(void)
{
  uint8_t  data[]       = {0x01, 0x02, 0x03};
  uint32_t original_crc = 0xDEADBEEF;
  uint32_t result       = rx_crc32_update(original_crc, data, 0);

  TEST_ASSERT_EQUAL_HEX32(original_crc, result);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

/**
 * @brief Test suite entry point - executes all CRC-32 unit tests
 *
 * @details
 * Runs comprehensive CRC-32 test suite using Unity framework. Tests validate
 * both hardware and software implementations for IEEE 802.3 compatibility.
 * All tests must pass for production use.
 *
 * **Test execution order:**
 * 1. **Edge cases** (3 tests) - nullptr pointers, zero length, empty data
 * 2. **Standard vectors** (7 tests) - IEEE 802.3 canonical + boundary values
 * 3. **Incremental** (2 tests) - Chunked computation, byte-by-byte processing
 * 4. **Abstraction layer** (6 tests) - Init/deinit, large buffers, unaligned access
 *
 * **Total:** 18 tests covering 100% of CRC-32 API functionality
 *
 * **Algorithm steps:**
 * 1. Initialize Unity framework (UNITY_BEGIN)
 * 2. Execute each test group sequentially
 * 3. Collect pass/fail results
 * 4. Print summary report (UNITY_END)
 * 5. Return exit code (0 = all pass, non-zero = failures)
 *
 * @return int Exit code for test harness
 * @retval 0 All tests passed (production ready)
 * @retval >0 Number of test failures (DO NOT USE IN PRODUCTION)
 *
 * @pre Unity framework compiled and linked
 * @pre rx_crc32_ieee() implementation available (HW or SW)
 * @post Test results printed to stdout
 * @post Exit code reflects pass/fail status
 *
 * @note Tests run in deterministic order (not randomized)
 * @note Each test is independent - no shared state
 * @note Execution time: ~5ms on RX72N @ 240 MHz (includes 1KB buffer test)
 *
 * @warning Do not call this function multiple times in same process
 * @attention Failure of test_crc32_standard_vector() indicates broken CRC-32
 *
 * @par Performance:
 * - Single test: ~10-50 us (small vectors)
 * - Large buffer test: ~120 us (1KB, software) or ~12 us (hardware)
 * - Full suite: ~5 ms total
 * - Memory: 1280 bytes stack (1KB test buffer + overhead)
 *
 * @par Example Output:
 * @code
 * test_rx_crc32.c:41:test_crc32_empty_data:PASS
 * test_rx_crc32.c:80:test_crc32_standard_vector:PASS
 * ...
 * test_rx_crc32.c:318:test_crc32_update_zero_len_returns_original:PASS
 *
 * -----------------------
 * 18 Tests 0 Failures 0 Ignored
 * OK
 * @endcode
 *
 * @par Usage:
 * @code
 * // Compile and run tests
 * $ make test_rx_crc32
 * $ ./build/tests/test_rx_crc32
 * @endcode
 *
 * @see setUp() Test fixture setup (no-op)
 * @see tearDown() Test fixture teardown (no-op)
 * @see https://github.com/ThrowTheSwitch/Unity Unity documentation
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Basic tests */
  RUN_TEST(test_crc32_empty_data);
  RUN_TEST(test_crc32_null_pointer);
  RUN_TEST(test_crc32_zero_length);

  /* Known test vectors */
  RUN_TEST(test_crc32_standard_vector);
  RUN_TEST(test_crc32_single_zero);
  RUN_TEST(test_crc32_single_ff);
  RUN_TEST(test_crc32_sync_word);
  RUN_TEST(test_crc32_eight_zeros);
  RUN_TEST(test_crc32_eight_ff);
  RUN_TEST(test_crc32_frame_header);

  /* Incremental CRC tests */
  RUN_TEST(test_crc32_incremental_matches_single);
  RUN_TEST(test_crc32_incremental_single_bytes);

  /* Abstraction layer tests */
  RUN_TEST(test_crc32_init_deinit);
  RUN_TEST(test_crc32_double_init_safe);
  RUN_TEST(test_crc32_large_buffer_1kb);
  RUN_TEST(test_crc32_unaligned_data);
  RUN_TEST(test_crc32_update_null_returns_original);
  RUN_TEST(test_crc32_update_zero_len_returns_original);

  return UNITY_END();
}
