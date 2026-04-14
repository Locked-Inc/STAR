/**
 * @file test_crc32.c
 * @brief Unit tests for bb_crc32 library.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "bb_crc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------------------*/

static uint32_t s_failures  = 0U;
static uint32_t s_tests_run = 0U;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            (void)fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            s_failures++; \
        } \
    } while (0)

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

/**
 * @brief IEEE 802.3 check value: CRC-32("123456789") == 0xCBF43926.
 */
static void test_crc32_check_value(void)
{
    s_tests_run++;
    const uint8_t data[] = "123456789";
    uint32_t crc = 0U;

    bb_err_t err = bb_crc32_compute(data, 9U, &crc);

    TEST_ASSERT(err == k_bb_err_ok, "crc32 check value: err != ok");
    TEST_ASSERT(crc == 0xCBF43926U, "crc32 check value: wrong CRC");
}

/**
 * @brief Empty data (len=0) should produce 0x00000000.
 */
static void test_crc32_empty_data(void)
{
    s_tests_run++;
    const uint8_t data[] = {0};
    uint32_t crc = 0xDEADBEEFU;

    bb_err_t err = bb_crc32_compute(data, 0U, &crc);

    TEST_ASSERT(err == k_bb_err_ok, "crc32 empty: err != ok");
    TEST_ASSERT(crc == 0x00000000U, "crc32 empty: wrong CRC");
}

/**
 * @brief CRC-32 of single byte 0x00 == 0xD202EF8D.
 */
static void test_crc32_single_byte(void)
{
    s_tests_run++;
    const uint8_t data[] = {0x00};
    uint32_t crc = 0U;

    bb_err_t err = bb_crc32_compute(data, 1U, &crc);

    TEST_ASSERT(err == k_bb_err_ok, "crc32 single byte: err != ok");
    TEST_ASSERT(crc == 0xD202EF8DU, "crc32 single byte: wrong CRC");
}

/**
 * @brief Incremental CRC matches one-shot.
 */
static void test_crc32_incremental(void)
{
    s_tests_run++;
    const uint8_t data[] = "123456789";
    uint32_t crc_oneshot = 0U;
    (void)bb_crc32_compute(data, 9U, &crc_oneshot);

    /* Compute incrementally: "12345" then "6789" */
    uint32_t crc = 0xFFFFFFFFU;
    (void)bb_crc32_update(crc, data, 5U, &crc);
    (void)bb_crc32_update(crc, &data[5], 4U, &crc);
    crc ^= 0xFFFFFFFFU; /* finalize */

    TEST_ASSERT(crc == crc_oneshot, "crc32 incremental: mismatch");
}

/**
 * @brief NULL pointer guards return k_bb_err_null_ptr.
 */
static void test_crc32_null_ptr_guards(void)
{
    s_tests_run++;
    uint32_t crc = 0U;
    const uint8_t data[] = {0};

    TEST_ASSERT(bb_crc32_compute(NULL, 1U, &crc) == k_bb_err_null_ptr,
                "crc32 null data");
    TEST_ASSERT(bb_crc32_compute(data, 1U, NULL) == k_bb_err_null_ptr,
                "crc32 null out_crc");
    TEST_ASSERT(bb_crc32_update(0, NULL, 1U, &crc) == k_bb_err_null_ptr,
                "crc32_update null data");
    TEST_ASSERT(bb_crc32_update(0, data, 1U, NULL) == k_bb_err_null_ptr,
                "crc32_update null out_crc");
}

/**
 * @brief CRC-32 of 4 bytes all 0xFF == 0xFFFFFFFF ^ final_xor.
 */
static void test_crc32_all_ones(void)
{
    s_tests_run++;
    const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t crc = 0U;

    bb_err_t err = bb_crc32_compute(data, 4U, &crc);

    TEST_ASSERT(err == k_bb_err_ok, "crc32 all-FF: err != ok");
    TEST_ASSERT(crc == 0xFFFFFFFFU, "crc32 all-FF: wrong CRC");
}

/**
 * @brief CRC-32 of 4 bytes all 0x00 == 0x2144DF1C.
 */
static void test_crc32_all_zeros(void)
{
    s_tests_run++;
    const uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint32_t crc = 0U;

    bb_err_t err = bb_crc32_compute(data, 4U, &crc);

    TEST_ASSERT(err == k_bb_err_ok, "crc32 all-00: err != ok");
    TEST_ASSERT(crc == 0x2144DF1CU, "crc32 all-00: wrong CRC");
}

/**
 * @brief CRC-32 of a 256-byte buffer (sequential 0x00..0xFF).
 */
static void test_crc32_large_buffer(void)
{
    s_tests_run++;
    uint8_t data[256];
    for (uint32_t i = 0U; i < 256U; i++) {
        data[i] = (uint8_t)i;
    }

    uint32_t crc = 0U;
    bb_err_t err = bb_crc32_compute(data, 256U, &crc);

    TEST_ASSERT(err == k_bb_err_ok, "crc32 large: err != ok");
    /* Known CRC-32 of bytes 0x00..0xFF */
    TEST_ASSERT(crc == 0x29058C73U, "crc32 large: wrong CRC");
}

/* ---------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------------*/

int main(void)
{
    test_crc32_check_value();
    test_crc32_empty_data();
    test_crc32_single_byte();
    test_crc32_incremental();
    test_crc32_null_ptr_guards();
    test_crc32_all_ones();
    test_crc32_all_zeros();
    test_crc32_large_buffer();

    if (s_failures == 0U) {
        (void)fprintf(stdout, "test_crc32: ALL %u TESTS PASSED\n",
                      (unsigned)s_tests_run);
    } else {
        (void)fprintf(stderr, "test_crc32: %u FAILURES\n", (unsigned)s_failures);
    }

    return (int)s_failures;
}
