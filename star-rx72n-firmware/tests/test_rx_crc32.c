/* tests/test_rx_crc32.c */

/**
 * @file test_rx_crc32.c
 * @brief Unit Tests for IEEE 802.3 CRC-32 Implementation
 *
 * Tests the CRC-32 implementation for bit-exact compatibility with
 * Go's crc32.ChecksumIEEE().
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "rx_crc.h"
#include "rx_crc_internal.h"
#include "unity.h"

void setUp(void)
{
  /* Nothing to set up */
}

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
  uint32_t crc = rx_crc32_ieee(NULL, 0);
  TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/**
 * @brief Test CRC-32 with NULL pointer returns 0
 */
void test_crc32_null_pointer(void)
{
  uint32_t crc = rx_crc32_ieee(NULL, 10);
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
 * @brief Test CRC-32 of "123456789" (standard test vector)
 *
 * This is the canonical IEEE 802.3 CRC-32 test vector.
 * Expected: 0xCBF43926
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
 * @brief Test CRC-32 of frame sync word 0x55AA (big-endian)
 *
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_sync_word(void)
{
  uint8_t  data[] = {0x55, 0xAA};
  uint32_t crc    = rx_crc32_ieee(data, 2);
  TEST_ASSERT_EQUAL_HEX32(0xB016F118, crc);
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
 * Frame header bytes: 0x55 0xAA 0x00 0x01 0x00 0x08 0x01 0x00
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_frame_header(void)
{
  uint8_t  header[] = {0x55, 0xAA, 0x00, 0x01, 0x00, 0x08, 0x01, 0x00};
  uint32_t crc      = rx_crc32_ieee(header, 8);
  TEST_ASSERT_EQUAL_HEX32(0xB157E7A2, crc);
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
  crc_incr          = rx_crc32_update(crc_incr, data + 2, 3);
  crc_incr          = rx_crc32_update(crc_incr, data + 5, 3);

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
  rx_err_t err;

  /* First init */
  err = rx_crc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Second init should also succeed (idempotent) */
  err = rx_crc_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Cleanup */
  rx_crc_deinit();
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
 * @brief Test rx_crc32_update with NULL data returns original CRC
 */
void test_crc32_update_null_returns_original(void)
{
  uint32_t original_crc = 0x12345678;
  uint32_t result       = rx_crc32_update(original_crc, NULL, 10);
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
