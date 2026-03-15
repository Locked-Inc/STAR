/**
 * @file test_rx_crc.c
 * @brief Unified CRC API tests: all 5 polynomials x 3 backends
 *
 * @details
 * Tests rx_crc_compute() with all combinations of polynomial and backend,
 * plus lifecycle (init/deinit), input validation, and convenience wrappers.
 *
 * ## Test Vectors
 *
 * All values computed over the canonical string "123456789" (9 bytes, 0x31-0x39)
 * and cross-validated against the CRC Catalogue (crccalc.com):
 *
 * | Polynomial     | Expected    | Algorithm ID        |
 * |----------------|-------------|---------------------|
 * | CRC-8/Maxim    | 0xA1        | CRC-8/MAXIM-DOW     |
 * | CRC-16/IBM     | 0xBB3D      | CRC-16/ARC          |
 * | CRC-CCITT      | 0x2189      | CRC-16/KERMIT       |
 * | CRC-32/IEEE    | 0xCBF43926  | CRC-32/ISO-HDLC     |
 * | CRC-32C        | 0xE3069283  | CRC-32/ISCSI        |
 *
 * ## Backend behaviour on host (no __RX__)
 *
 * Hardware backends (hw_cpu, hw_dma) fall back to software in host builds
 * via the #else branch in rx_crc_compute(). Tests verify correct CRC values
 * for all three backends on host; mock_rx_dmaca captures DMA call counts.
 *
 * @see rx_crc.h     Public API
 * @see rx_crc_sw.c  Software backend
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mock_rx_dmaca.h"
#include "rx_crc.h"
#include "rx_crc_internal.h"
#include "unity.h"

/* =============================================================================
 * Test Vectors
 * =============================================================================
 */

/** @brief Canonical test string "123456789" (9 bytes) */
static const uint8_t s_test_vec_9[] = {
  0x31,
  0x32,
  0x33,
  0x34,
  0x35,
  0x36,
  0x37,
  0x38,
  0x39,
};

typedef enum : uint32_t {
  k_test_vec_9_len = 9U,          /**< Length of canonical 9-byte vector */
  k_crc8_maxim_9   = 0xA1U,       /**< CRC-8/MAXIM-DOW of "123456789" */
  k_crc16_ibm_9    = 0xBB3DU,     /**< CRC-16/ARC of "123456789" */
  k_crc_ccitt_9    = 0x2189U,     /**< CRC-16/KERMIT of "123456789" */
  k_crc32_ieee_9   = 0xCBF43926U, /**< CRC-32/ISO-HDLC of "123456789" */
  k_crc32c_9       = 0xE3069283U, /**< CRC-32/ISCSI of "123456789" */
} crc_test_vectors_t;

/** @brief Boundary byte values used in single-byte test vectors */
typedef enum : uint8_t {
  k_byte_min = 0x00U, /**< Minimum byte value (all zeros) */
  k_byte_max = 0xFFU, /**< Maximum byte value (all ones) */
} crc_boundary_bytes_t;

/** @brief Single zero byte */
static const uint8_t s_single_zero[] = {k_byte_min};

/** @brief Single byte 0xFF */
static const uint8_t s_single_ff[] = {k_byte_max};

typedef enum : uint32_t {
  k_single_zero_len = 1U, /**< Length of single-byte buffers */
  /* SW reference values for single-byte inputs */
  k_crc32_ieee_zero = 0xD202EF8DU, /**< CRC-32/ISO-HDLC of {0x00} */
  k_crc32_ieee_ff   = 0xFF000000U, /**< CRC-32/ISO-HDLC of {0xFF} */
} crc_single_byte_vectors_t;

/** @brief Chunk sizes for incremental CRC-32 update test */
typedef enum : uint32_t {
  k_chunk_first_len  = 4U, /**< First chunk: bytes 0-3 of "123456789" */
  k_chunk_second_len = 5U, /**< Second chunk: bytes 4-8 of "123456789" (4+5=9) */
} crc_chunk_sizes_t;

/** @brief Mock DMAC address constants (uintptr_t for hardware addresses) */
typedef enum : uintptr_t {
  k_mock_crc_dst_addr = 0x00088284U, /**< CRCDIR register address (DMA destination) */
} crc_mock_addr_constants_t;

/** @brief Mock DMAC non-address constants for direct mock interaction tests */
typedef enum : uint32_t {
  k_mock_timeout_cycles = 50000U, /**< Timeout cycles for mock DMA transfer test */
  k_mock_dma_channel    = 0U,     /**< DMA channel index used in mock transfer config */
} crc_mock_constants_t;

/** @brief Sentinel value for pass-through tests (arbitrary non-zero CRC) */
typedef enum : uint32_t {
  k_test_crc32_sentinel =
    0xDEADBEEFU, /**< Arbitrary sentinel; verifies pass-through returns input unchanged */
} crc_sentinel_constants_t;

/* =============================================================================
 * Unity Fixtures
 * =============================================================================
 */

/**
 * @brief Unity fixture: reset all mock state before each test
 *
 * @details
 * Resets the DMAC mock to its uninitialized state so each test starts from a
 * clean baseline without residual state from a previous test.
 *
 * @pre None -- called automatically by the Unity framework before every test
 * @post mock_rx_dmaca is in its uninitialized state (s_is_initialized == false)
 *
 * @note Not thread-safe; Unity test runner is single-threaded
 * @since Version 1.0.0
 */
void setUp(void)
{
  mock_rx_dmaca_reset();
}

/**
 * @brief Unity fixture: clean up module and mock state after each test
 *
 * @details
 * Calls rx_crc_deinit() (ignoring the return value) to handle tests that
 * initialized the CRC module but may not have deinitialized it, then resets
 * the DMAC mock so the next test starts clean.
 *
 * @pre None -- called automatically by the Unity framework after every test
 * @post rx_crc module is in its uninitialized state
 * @post mock_rx_dmaca is in its uninitialized state (s_is_initialized == false)
 *
 * @note Not thread-safe; Unity test runner is single-threaded
 * @since Version 1.0.0
 */
void tearDown(void)
{
  /* Ensure module is deinitialized between tests that call rx_crc_init() */
  (void)rx_crc_deinit();
  mock_rx_dmaca_reset();
}

/* =============================================================================
 * Helper
 * =============================================================================
 */

/**
 * @brief Build a config and call rx_crc_compute(); assert k_rx_ok.
 *
 * @details
 * Constructs an rx_crc_config_t with the given polynomial and backend,
 * LSB-first bit order, and zero DMA timeout, then calls rx_crc_compute()
 * and asserts that it returns k_rx_ok. Writes the computed CRC to *result.
 *
 * @param[in]  poly      Polynomial selection (e.g. k_rx_crc_poly_crc8)
 * @param[in]  backend   Backend selection (e.g. k_rx_crc_backend_sw)
 * @param[in]  data      Input buffer; must be non-NULL and at least len bytes
 * @param[in]  len       Number of bytes to compute CRC over; must be > 0
 * @param[out] result    Pointer to store the computed CRC value; must be non-NULL
 *
 * @pre data must point to at least len valid bytes
 * @pre result must point to a valid uint32_t storage location
 * @post *result contains the CRC-computed value for the given input
 * @post Unity test is failed (via TEST_ASSERT_EQUAL) if rx_crc_compute() != k_rx_ok
 *
 * @note Not thread-safe; call only from the Unity test thread
 * @since Version 1.0.0
 */
static void internal_compute_ok(rx_crc_poly_t    poly,
                                rx_crc_backend_t backend,
                                const uint8_t*   data,
                                uint32_t         len,
                                uint32_t*        result)
{
  const rx_crc_config_t cfg = {
    .poly      = poly,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = backend,
    .dma       = {.timeout_cycles = 0U},
  };
  rx_err_t err = rx_crc_compute(&cfg, data, len, result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Lifecycle Tests
 * =============================================================================
 */

/**
 * @brief Init then deinit succeeds.
 */
void test_crc_init_deinit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_init());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_deinit());
}

/**
 * @brief Double init returns invalid_state.
 */
void test_crc_double_init_invalid_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_init());
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_crc_init());
  (void)rx_crc_deinit();
}

/**
 * @brief Deinit without init returns invalid_state.
 */
void test_crc_deinit_without_init_invalid_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_crc_deinit());
}

/**
 * @brief Double deinit returns invalid_state.
 */
void test_crc_double_deinit_invalid_state(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_init());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_deinit());
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_crc_deinit());
}

/**
 * @brief Reinit after deinit succeeds.
 */
void test_crc_reinit_after_deinit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_init());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_deinit());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_init());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc_deinit());
}

/* =============================================================================
 * Input Validation Tests
 * =============================================================================
 */

/**
 * @brief NULL config pointer returns null_ptr error.
 */
void test_crc_compute_null_config(void)
{
  uint32_t result = 0U;
  rx_err_t err    = rx_crc_compute(nullptr, s_test_vec_9, k_test_vec_9_len, &result);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief NULL data pointer returns null_ptr error.
 */
void test_crc_compute_null_data(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_crc_compute(&cfg, nullptr, 1U, &result));
}

/**
 * @brief NULL result pointer returns null_ptr error.
 */
void test_crc_compute_null_result(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, nullptr));
}

/**
 * @brief Zero length returns invalid_arg.
 */
void test_crc_compute_zero_len(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_crc_compute(&cfg, s_test_vec_9, 0U, &result));
}

/**
 * @brief Length exceeding maximum returns invalid_arg.
 */
void test_crc_compute_len_too_large(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, (uint32_t)k_crc_len_max + 1U, &result));
}

/**
 * @brief Out-of-range poly value returns invalid_arg.
 */
void test_crc_compute_invalid_poly(void)
{
  const rx_crc_config_t cfg = {
    .poly      = (rx_crc_poly_t)0xFFU,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, &result));
}

/**
 * @brief Out-of-range bit_order value returns invalid_arg.
 */
void test_crc_compute_invalid_bit_order(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = (rx_crc_bit_order_t)0xFFU,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, &result));
}

/**
 * @brief Software backend with msb_first bit order returns invalid_arg.
 */
void test_crc_compute_sw_msb_first_invalid(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_msb_first,
    .backend   = k_rx_crc_backend_software,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, &result));
}

/**
 * @brief hw_cpu backend with msb_first bit order returns invalid_arg on host.
 */
void test_crc_compute_hw_cpu_msb_first_invalid(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_msb_first,
    .backend   = k_rx_crc_backend_hw_cpu,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, &result));
}

/**
 * @brief hw_dma backend with msb_first bit order returns invalid_arg on host.
 */
void test_crc_compute_hw_dma_msb_first_invalid(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_msb_first,
    .backend   = k_rx_crc_backend_hw_dma,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, &result));
}

/**
 * @brief Invalid backend value returns invalid_arg (default switch case).
 */
void test_crc_compute_invalid_backend(void)
{
  const rx_crc_config_t cfg = {
    .poly      = k_rx_crc_poly_crc32,
    .bit_order = k_rx_crc_bit_order_lsb_first,
    .backend   = (rx_crc_backend_t)0xFFU,
    .dma       = {.timeout_cycles = 0U},
  };
  uint32_t result = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_crc_compute(&cfg, s_test_vec_9, k_test_vec_9_len, &result));
}

/* =============================================================================
 * Software Backend Tests - All 5 Polynomials
 * =============================================================================
 */

/**
 * @brief CRC-8/Maxim of "123456789" via software backend.
 */
void test_crc_sw_crc8_maxim(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc8,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc8_maxim_9, result);
}

/**
 * @brief CRC-16/IBM of "123456789" via software backend.
 */
void test_crc_sw_crc16_ibm(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc16,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc16_ibm_9, result);
}

/**
 * @brief CRC-CCITT/Kermit of "123456789" via software backend.
 */
void test_crc_sw_crc_ccitt(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc_ccitt,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc_ccitt_9, result);
}

/**
 * @brief CRC-32/IEEE 802.3 of "123456789" via software backend.
 */
void test_crc_sw_crc32_ieee(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32_ieee_9, result);
}

/**
 * @brief CRC-32C/Castagnoli of "123456789" via software backend.
 */
void test_crc_sw_crc32c(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32c,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32c_9, result);
}

/* =============================================================================
 * hw_cpu Backend Tests - All 5 Polynomials
 * (Host build: hw_cpu falls back to software; same results expected)
 * =============================================================================
 */

/**
 * @brief CRC-8/Maxim via hw_cpu backend produces correct result.
 */
void test_crc_hw_cpu_crc8_maxim(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc8,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc8_maxim_9, result);
}

/**
 * @brief CRC-16/IBM via hw_cpu backend produces correct result.
 */
void test_crc_hw_cpu_crc16_ibm(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc16,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc16_ibm_9, result);
}

/**
 * @brief CRC-CCITT/Kermit via hw_cpu backend produces correct result.
 */
void test_crc_hw_cpu_crc_ccitt(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc_ccitt,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc_ccitt_9, result);
}

/**
 * @brief CRC-32/IEEE via hw_cpu backend produces correct result.
 */
void test_crc_hw_cpu_crc32_ieee(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32_ieee_9, result);
}

/**
 * @brief CRC-32C via hw_cpu backend produces correct result.
 */
void test_crc_hw_cpu_crc32c(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32c,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32c_9, result);
}

/* =============================================================================
 * hw_dma Backend Tests - All 5 Polynomials
 * (Host build: hw_dma falls back to software; DMA mock is NOT called)
 * =============================================================================
 */

/**
 * @brief CRC-8/Maxim via hw_dma backend produces correct result on host.
 */
void test_crc_hw_dma_crc8_maxim(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc8,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc8_maxim_9, result);
  /* Host build: DMA not used - transfer_poll not called */
  TEST_ASSERT_EQUAL(0U, mock_rx_dmaca_get_transfer_call_count());
}

/**
 * @brief CRC-16/IBM via hw_dma backend produces correct result on host.
 */
void test_crc_hw_dma_crc16_ibm(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc16,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc16_ibm_9, result);
  TEST_ASSERT_EQUAL(0U, mock_rx_dmaca_get_transfer_call_count());
}

/**
 * @brief CRC-CCITT/Kermit via hw_dma backend produces correct result on host.
 */
void test_crc_hw_dma_crc_ccitt(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc_ccitt,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc_ccitt_9, result);
  TEST_ASSERT_EQUAL(0U, mock_rx_dmaca_get_transfer_call_count());
}

/**
 * @brief CRC-32/IEEE via hw_dma backend produces correct result on host.
 */
void test_crc_hw_dma_crc32_ieee(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32_ieee_9, result);
  TEST_ASSERT_EQUAL(0U, mock_rx_dmaca_get_transfer_call_count());
}

/**
 * @brief CRC-32C via hw_dma backend produces correct result on host.
 */
void test_crc_hw_dma_crc32c(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32c,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32c_9, result);
  TEST_ASSERT_EQUAL(0U, mock_rx_dmaca_get_transfer_call_count());
}

/* =============================================================================
 * Backend Consistency Tests
 * (All three backends must produce identical results)
 * =============================================================================
 */

/**
 * @brief CRC-32/IEEE: SW, hw_cpu, hw_dma all match.
 */
void test_crc32_all_backends_match(void)
{
  uint32_t sw_result  = 0U;
  uint32_t cpu_result = 0U;
  uint32_t dma_result = 0U;

  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &sw_result);
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &cpu_result);
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &dma_result);

  TEST_ASSERT_EQUAL_HEX32(sw_result, cpu_result);
  TEST_ASSERT_EQUAL_HEX32(sw_result, dma_result);
}

/**
 * @brief CRC-32C: SW, hw_cpu, hw_dma all match.
 */
void test_crc32c_all_backends_match(void)
{
  uint32_t sw_result  = 0U;
  uint32_t cpu_result = 0U;
  uint32_t dma_result = 0U;

  internal_compute_ok(k_rx_crc_poly_crc32c,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &sw_result);
  internal_compute_ok(k_rx_crc_poly_crc32c,
                      k_rx_crc_backend_hw_cpu,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &cpu_result);
  internal_compute_ok(k_rx_crc_poly_crc32c,
                      k_rx_crc_backend_hw_dma,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &dma_result);

  TEST_ASSERT_EQUAL_HEX32(sw_result, cpu_result);
  TEST_ASSERT_EQUAL_HEX32(sw_result, dma_result);
}

/* =============================================================================
 * Convenience Wrapper Tests
 * =============================================================================
 */

/**
 * @brief rx_crc32_ieee() matches rx_crc_compute() with sw backend.
 */
void test_crc32_ieee_wrapper_matches_compute(void)
{
  uint32_t compute_result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &compute_result);

  uint32_t wrapper_result = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc32_ieee(s_test_vec_9, k_test_vec_9_len, &wrapper_result));
  TEST_ASSERT_EQUAL_HEX32(compute_result, wrapper_result);
}

/**
 * @brief rx_crc32_ieee() of canonical "123456789" matches known value.
 */
void test_crc32_ieee_wrapper_canonical(void)
{
  uint32_t crc = 0U;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc32_ieee(s_test_vec_9, k_test_vec_9_len, &crc));
  TEST_ASSERT_EQUAL_HEX32(k_crc32_ieee_9, crc);
}

/**
 * @brief rx_crc32_ieee() returns k_rx_err_null_ptr for NULL data.
 */
void test_crc32_ieee_wrapper_null_data(void)
{
  uint32_t crc = 0U;

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_crc32_ieee(nullptr, k_test_vec_9_len, &crc));
}

/**
 * @brief rx_crc32_ieee() returns k_rx_err_invalid_arg for zero length.
 */
void test_crc32_ieee_wrapper_zero_len(void)
{
  uint32_t crc = 0U;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_crc32_ieee(s_test_vec_9, 0U, &crc));
}

/**
 * @brief rx_crc8_maxim() matches rx_crc_compute() with sw backend.
 */
void test_crc8_maxim_wrapper_matches_compute(void)
{
  uint32_t compute_result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc8,
                      k_rx_crc_backend_software,
                      s_test_vec_9,
                      k_test_vec_9_len,
                      &compute_result);

  uint32_t wrapper_result = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc8_maxim(s_test_vec_9, k_test_vec_9_len, &wrapper_result));
  TEST_ASSERT_EQUAL_HEX32((uint8_t)compute_result, (uint8_t)wrapper_result);
}

/**
 * @brief rx_crc8_maxim() of canonical "123456789" matches known value.
 */
void test_crc8_maxim_wrapper_canonical(void)
{
  uint32_t crc = 0U;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc8_maxim(s_test_vec_9, k_test_vec_9_len, &crc));
  TEST_ASSERT_EQUAL_HEX32(k_crc8_maxim_9, (uint8_t)crc);
}

/**
 * @brief rx_crc8_maxim() returns k_rx_err_null_ptr for NULL data.
 */
void test_crc8_maxim_wrapper_null_data(void)
{
  uint32_t crc = 0U;

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_crc8_maxim(nullptr, k_test_vec_9_len, &crc));
}

/**
 * @brief rx_crc8_maxim() returns k_rx_err_invalid_arg for zero length.
 */
void test_crc8_maxim_wrapper_zero_len(void)
{
  uint32_t crc = 0U;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_crc8_maxim(s_test_vec_9, 0U, &crc));
}

/* =============================================================================
 * Incremental CRC-32 Tests
 * =============================================================================
 */

/**
 * @brief rx_crc32_update() single-pass matches rx_crc32_ieee().
 */
void test_crc32_update_single_pass(void)
{
  uint32_t expected = 0U;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc32_ieee(s_test_vec_9, k_test_vec_9_len, &expected));

  /* Use update to compute same result: start from 0 seed */
  uint32_t crc = rx_crc32_update(0U, s_test_vec_9, k_test_vec_9_len);
  TEST_ASSERT_EQUAL_HEX32(expected, crc);
}

/**
 * @brief rx_crc32_update() chunked matches single-pass.
 */
void test_crc32_update_chunked_matches_single(void)
{
  uint32_t single = 0U;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_crc32_ieee(s_test_vec_9, k_test_vec_9_len, &single));

  /* Split 9 bytes as 4 + 5 */
  uint32_t crc = rx_crc32_update(0U, s_test_vec_9, k_chunk_first_len);
  crc          = rx_crc32_update(crc, s_test_vec_9 + k_chunk_first_len, k_chunk_second_len);
  TEST_ASSERT_EQUAL_HEX32(single, crc);
}

/**
 * @brief rx_crc32_update() NULL data returns input crc unchanged.
 */
void test_crc32_update_null_returns_original(void)
{
  uint32_t crc = (uint32_t)k_test_crc32_sentinel;
  TEST_ASSERT_EQUAL_HEX32(crc, rx_crc32_update(crc, nullptr, k_test_vec_9_len));
}

/**
 * @brief rx_crc32_update() with len > k_crc_len_max returns crc unchanged.
 */
void test_crc32_update_len_too_large_returns_original(void)
{
  uint32_t crc = (uint32_t)k_test_crc32_sentinel;
  TEST_ASSERT_EQUAL_HEX32(crc, rx_crc32_update(crc, s_test_vec_9, (uint32_t)k_crc_len_max + 1U));
}

/**
 * @brief rx_crc32_update() zero length returns input crc unchanged.
 */
void test_crc32_update_zero_len_returns_original(void)
{
  uint32_t crc = (uint32_t)k_test_crc32_sentinel;
  TEST_ASSERT_EQUAL_HEX32(crc, rx_crc32_update(crc, s_test_vec_9, 0U));
}

/* =============================================================================
 * Single-Byte Tests
 * =============================================================================
 */

/**
 * @brief CRC-32/IEEE of {0x00} matches known value.
 */
void test_crc32_ieee_single_zero(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_software,
                      s_single_zero,
                      k_single_zero_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32_ieee_zero, result);
}

/**
 * @brief CRC-32/IEEE of {0xFF} matches known value.
 */
void test_crc32_ieee_single_ff(void)
{
  uint32_t result = 0U;
  internal_compute_ok(k_rx_crc_poly_crc32,
                      k_rx_crc_backend_software,
                      s_single_ff,
                      k_single_zero_len,
                      &result);
  TEST_ASSERT_EQUAL_HEX32(k_crc32_ieee_ff, result);
}

/* =============================================================================
 * Mock DMA Interaction Tests
 * =============================================================================
 */

/**
 * @brief Verify mock_rx_dmaca_reset() clears transfer count.
 */
void test_mock_dmaca_reset_clears_state(void)
{
  /* Simulate a previous transfer call tracking by resetting */
  mock_rx_dmaca_reset();
  TEST_ASSERT_EQUAL(0U, mock_rx_dmaca_get_transfer_call_count());
  TEST_ASSERT_NULL(mock_rx_dmaca_get_last_config());
}

/**
 * @brief After mock set_transfer_result(k_rx_err_timeout), confirm it is returned
 *        by mock rx_dmaca_transfer_poll() directly.
 */
void test_mock_dmaca_preset_timeout_result(void)
{
  mock_rx_dmaca_set_transfer_result(k_rx_err_timeout);

  /* Initialize mock so transfer_poll accepts calls (mirrors real DMACA semantics) */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_dmaca_init());

  /* Call the mock directly to verify preset behavior */
  const rx_dmaca_config_t cfg = {
    .channel        = (uint8_t)k_mock_dma_channel,
    .src            = s_test_vec_9,
    .len            = k_test_vec_9_len,
    .dst_addr       = (uintptr_t)k_mock_crc_dst_addr,
    .timeout_cycles = (uint32_t)k_mock_timeout_cycles,
  };
  rx_err_t result = rx_dmaca_transfer_poll(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, result);
  TEST_ASSERT_EQUAL(1U, mock_rx_dmaca_get_transfer_call_count());
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/** @brief Run lifecycle and input validation tests */
static void internal_run_lifecycle_and_validation_tests(void)
{
  /* Lifecycle */
  RUN_TEST(test_crc_init_deinit);
  RUN_TEST(test_crc_double_init_invalid_state);
  RUN_TEST(test_crc_deinit_without_init_invalid_state);
  RUN_TEST(test_crc_double_deinit_invalid_state);
  RUN_TEST(test_crc_reinit_after_deinit);

  /* Input validation */
  RUN_TEST(test_crc_compute_null_config);
  RUN_TEST(test_crc_compute_null_data);
  RUN_TEST(test_crc_compute_null_result);
  RUN_TEST(test_crc_compute_zero_len);
  RUN_TEST(test_crc_compute_len_too_large);
  RUN_TEST(test_crc_compute_invalid_poly);
  RUN_TEST(test_crc_compute_invalid_bit_order);
  RUN_TEST(test_crc_compute_sw_msb_first_invalid);
  RUN_TEST(test_crc_compute_hw_cpu_msb_first_invalid);
  RUN_TEST(test_crc_compute_hw_dma_msb_first_invalid);
  RUN_TEST(test_crc_compute_invalid_backend);
}

/** @brief Run all backend polynomial tests */
static void internal_run_backend_tests(void)
{
  /* Software backend - all 5 polynomials */
  RUN_TEST(test_crc_sw_crc8_maxim);
  RUN_TEST(test_crc_sw_crc16_ibm);
  RUN_TEST(test_crc_sw_crc_ccitt);
  RUN_TEST(test_crc_sw_crc32_ieee);
  RUN_TEST(test_crc_sw_crc32c);

  /* hw_cpu backend - all 5 polynomials */
  RUN_TEST(test_crc_hw_cpu_crc8_maxim);
  RUN_TEST(test_crc_hw_cpu_crc16_ibm);
  RUN_TEST(test_crc_hw_cpu_crc_ccitt);
  RUN_TEST(test_crc_hw_cpu_crc32_ieee);
  RUN_TEST(test_crc_hw_cpu_crc32c);

  /* hw_dma backend - all 5 polynomials */
  RUN_TEST(test_crc_hw_dma_crc8_maxim);
  RUN_TEST(test_crc_hw_dma_crc16_ibm);
  RUN_TEST(test_crc_hw_dma_crc_ccitt);
  RUN_TEST(test_crc_hw_dma_crc32_ieee);
  RUN_TEST(test_crc_hw_dma_crc32c);

  /* Backend consistency */
  RUN_TEST(test_crc32_all_backends_match);
  RUN_TEST(test_crc32c_all_backends_match);
}

/** @brief Run wrapper, incremental, boundary, and mock tests */
static void internal_run_wrapper_and_edge_tests(void)
{
  /* Convenience wrappers */
  RUN_TEST(test_crc32_ieee_wrapper_matches_compute);
  RUN_TEST(test_crc32_ieee_wrapper_canonical);
  RUN_TEST(test_crc32_ieee_wrapper_null_data);
  RUN_TEST(test_crc32_ieee_wrapper_zero_len);
  RUN_TEST(test_crc8_maxim_wrapper_matches_compute);
  RUN_TEST(test_crc8_maxim_wrapper_canonical);
  RUN_TEST(test_crc8_maxim_wrapper_null_data);
  RUN_TEST(test_crc8_maxim_wrapper_zero_len);

  /* Incremental CRC-32 */
  RUN_TEST(test_crc32_update_single_pass);
  RUN_TEST(test_crc32_update_chunked_matches_single);
  RUN_TEST(test_crc32_update_null_returns_original);
  RUN_TEST(test_crc32_update_zero_len_returns_original);
  RUN_TEST(test_crc32_update_len_too_large_returns_original);

  /* Single-byte edge cases */
  RUN_TEST(test_crc32_ieee_single_zero);
  RUN_TEST(test_crc32_ieee_single_ff);

  /* Mock DMA interaction */
  RUN_TEST(test_mock_dmaca_reset_clears_state);
  RUN_TEST(test_mock_dmaca_preset_timeout_result);
}

int main(void)
{
  UNITY_BEGIN();
  internal_run_lifecycle_and_validation_tests();
  internal_run_backend_tests();
  internal_run_wrapper_and_edge_tests();
  return UNITY_END();
}
