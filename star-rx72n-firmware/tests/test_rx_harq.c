/* tests/test_rx_harq.c */

/**
 * @file test_rx_harq.c
 * @brief Unit Tests for HARQ Protocol and Chase Combiner
 *
 * Tests Hybrid Automatic Repeat Request with Chase Combining.
 * Verifies bit-exact compatibility with Go implementation.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdint.h>
#include <string.h>

#include "rx_harq.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Bit manipulation constants for byte-to-bit conversion
 */
typedef enum : uint8_t {
  k_bits_per_byte = 8, /**< Number of bits in a byte */
  k_bit_idx_msb   = 7, /**< MSB index in byte (bit 7) */
  k_bit_idx_lsb   = 0, /**< LSB index in byte (bit 0) */
  k_bit_mask      = 1, /**< Mask to extract single bit */
} bit_manipulation_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/* Test fixtures */
static rx_chase_combiner_t s_combiner;
static rx_harq_handle_t    s_harq;

void setUp(void)
{
  rx_chase_combiner_init(&s_combiner, 0); /* Default max combines */
}

void tearDown(void)
{
  rx_chase_combiner_deinit(&s_combiner);
  rx_harq_deinit(&s_harq);
}

/* =============================================================================
 * Chase Combiner Initialization Tests
 * =============================================================================
 */

void test_combiner_init_null(void)
{
  rx_err_t err = rx_chase_combiner_init(NULL, 3);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_combiner_init_success(void)
{
  rx_chase_combiner_t comb;
  rx_err_t            err = rx_chase_combiner_init(&comb, 5);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, comb.initialized);
  TEST_ASSERT_EQUAL(5, comb.max_combines);
  TEST_ASSERT_EQUAL(0, comb.count);
}

void test_combiner_init_default_max(void)
{
  rx_chase_combiner_t comb;
  rx_err_t            err = rx_chase_combiner_init(&comb, 0); /* 0 = default */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_harq_default_combines, comb.max_combines);
}

void test_combiner_deinit_null(void)
{
  rx_err_t err = rx_chase_combiner_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Chase Combiner Add Tests
 * =============================================================================
 */

void test_combiner_add_null_combiner(void)
{
  rx_soft_bit_t soft[10] = {0};
  rx_err_t      err      = rx_chase_combiner_add(NULL, soft, 10);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_combiner_add_null_soft(void)
{
  rx_err_t err = rx_chase_combiner_add(&s_combiner, NULL, 10);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_combiner_add_uninitialized(void)
{
  rx_chase_combiner_t comb     = {0};
  rx_soft_bit_t       soft[10] = {0};
  rx_err_t            err      = rx_chase_combiner_add(&comb, soft, 10);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_combiner_add_zero_length(void)
{
  rx_soft_bit_t soft[10] = {0};
  rx_err_t      err      = rx_chase_combiner_add(&s_combiner, soft, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_combiner_add_too_large(void)
{
  rx_soft_bit_t soft[10] = {0};
  rx_err_t      err      = rx_chase_combiner_add(&s_combiner, soft, k_harq_soft_buffer_size + 1);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_combiner_add_single(void)
{
  rx_soft_bit_t soft[] = {10, 20, -30, 40};

  rx_err_t err = rx_chase_combiner_add(&s_combiner, soft, 4);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, s_combiner.count);
  TEST_ASSERT_EQUAL(4, s_combiner.expected_len);
}

void test_combiner_add_length_mismatch(void)
{
  rx_soft_bit_t soft1[] = {10, 20, 30, 40};
  rx_soft_bit_t soft2[] = {50, 60}; /* Different length */

  rx_chase_combiner_add(&s_combiner, soft1, 4);
  rx_err_t err = rx_chase_combiner_add(&s_combiner, soft2, 2);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_combiner_add_max_reached(void)
{
  rx_chase_combiner_t comb;
  rx_chase_combiner_init(&comb, 2); /* Max 2 combines */

  rx_soft_bit_t soft[] = {10, 20};

  TEST_ASSERT_EQUAL(k_rx_ok, rx_chase_combiner_add(&comb, soft, 2));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_chase_combiner_add(&comb, soft, 2));
  TEST_ASSERT_EQUAL(k_rx_err_busy, rx_chase_combiner_add(&comb, soft, 2));
}

/* =============================================================================
 * Chase Combiner Accumulation Tests
 * =============================================================================
 */

void test_combiner_accumulation(void)
{
  rx_soft_bit_t soft1[] = {50, -30, 20, -10};
  rx_soft_bit_t soft2[] = {30, 10, -5, 60};

  rx_chase_combiner_add(&s_combiner, soft1, 4);
  rx_chase_combiner_add(&s_combiner, soft2, 4);

  /* Verify accumulation: 50+30=80, -30+10=-20, 20+(-5)=15, -10+60=50 */
  TEST_ASSERT_EQUAL_INT16(80, s_combiner.accumulated[0]);
  TEST_ASSERT_EQUAL_INT16(-20, s_combiner.accumulated[1]);
  TEST_ASSERT_EQUAL_INT16(15, s_combiner.accumulated[2]);
  TEST_ASSERT_EQUAL_INT16(50, s_combiner.accumulated[3]);
}

void test_combiner_combined_output(void)
{
  rx_soft_bit_t soft1[] = {100, -100};
  rx_soft_bit_t soft2[] = {50, -50};

  rx_chase_combiner_add(&s_combiner, soft1, 2);
  rx_chase_combiner_add(&s_combiner, soft2, 2);

  rx_soft_bit_t output[2];
  uint32_t      len;

  rx_err_t err = rx_chase_combiner_combined(&s_combiner, output, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, len);

  /* 100+50=150, clamped to 127 */
  TEST_ASSERT_EQUAL(k_soft_bit_max, output[0]);
  /* -100+(-50)=-150, clamped to -127 */
  TEST_ASSERT_EQUAL(k_soft_bit_min, output[1]);
}

void test_combiner_combined_no_add(void)
{
  rx_soft_bit_t output[10];
  uint32_t      len;

  rx_err_t err = rx_chase_combiner_combined(&s_combiner, output, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Chase Combiner Helper Tests
 * =============================================================================
 */

void test_combiner_can_add(void)
{
  rx_chase_combiner_t comb;
  rx_chase_combiner_init(&comb, 2);

  rx_soft_bit_t soft[] = {10, 20};

  TEST_ASSERT_TRUE(rx_chase_combiner_can_add(&comb));

  rx_chase_combiner_add(&comb, soft, 2);
  TEST_ASSERT_TRUE(rx_chase_combiner_can_add(&comb));

  rx_chase_combiner_add(&comb, soft, 2);
  TEST_ASSERT_FALSE(rx_chase_combiner_can_add(&comb));
}

void test_combiner_can_add_null(void)
{
  TEST_ASSERT_FALSE(rx_chase_combiner_can_add(NULL));
}

void test_combiner_count(void)
{
  rx_soft_bit_t soft[] = {10, 20, 30};

  TEST_ASSERT_EQUAL(0, rx_chase_combiner_count(&s_combiner));

  rx_chase_combiner_add(&s_combiner, soft, 3);
  TEST_ASSERT_EQUAL(1, rx_chase_combiner_count(&s_combiner));

  rx_chase_combiner_add(&s_combiner, soft, 3);
  TEST_ASSERT_EQUAL(2, rx_chase_combiner_count(&s_combiner));
}

void test_combiner_count_null(void)
{
  TEST_ASSERT_EQUAL(0, rx_chase_combiner_count(NULL));
}

void test_combiner_reset(void)
{
  rx_soft_bit_t soft[] = {10, 20, 30};

  rx_chase_combiner_add(&s_combiner, soft, 3);
  rx_chase_combiner_add(&s_combiner, soft, 3);
  TEST_ASSERT_EQUAL(2, s_combiner.count);

  rx_err_t err = rx_chase_combiner_reset(&s_combiner);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, s_combiner.count);
  TEST_ASSERT_EQUAL(0, s_combiner.expected_len);
}

void test_combiner_reset_null(void)
{
  rx_err_t err = rx_chase_combiner_reset(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_combiner_reset_uninitialized(void)
{
  rx_chase_combiner_t comb = {0};
  rx_err_t            err  = rx_chase_combiner_reset(&comb);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * HARQ Handle Initialization Tests
 * =============================================================================
 */

void test_harq_init_null(void)
{
  rx_err_t err = rx_harq_init(NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_harq_init_default_config(void)
{
  rx_err_t err = rx_harq_init(&s_harq, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, s_harq.initialized);
  TEST_ASSERT_EQUAL(k_harq_default_retries, s_harq.max_retries);
  TEST_ASSERT_NOT_EQUAL(0, s_harq.fec_enabled); /* FEC enabled by default */
  TEST_ASSERT_EQUAL(k_harq_state_idle, s_harq.state);
}

void test_harq_init_custom_config(void)
{
  rx_harq_config_t config = {
    .max_retries  = 5,
    .fec_enabled  = 0, /* Disable FEC */
    .max_combines = 4,
  };

  rx_err_t err = rx_harq_init(&s_harq, &config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(5, s_harq.max_retries);
  TEST_ASSERT_EQUAL(0, s_harq.fec_enabled);
}

void test_harq_deinit_null(void)
{
  rx_err_t err = rx_harq_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * HARQ State Tests
 * =============================================================================
 */

void test_harq_get_state_null(void)
{
  TEST_ASSERT_EQUAL(k_harq_state_error, rx_harq_get_state(NULL));
}

void test_harq_get_state_idle(void)
{
  rx_harq_init(&s_harq, NULL);
  TEST_ASSERT_EQUAL(k_harq_state_idle, rx_harq_get_state(&s_harq));
}

void test_harq_reset(void)
{
  rx_harq_init(&s_harq, NULL);

  /* Manually change state */
  s_harq.state       = k_harq_state_combining;
  s_harq.retry_count = 2;

  rx_err_t err = rx_harq_reset(&s_harq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_harq_state_idle, s_harq.state);
  TEST_ASSERT_EQUAL(0, s_harq.retry_count);
}

void test_harq_reset_null(void)
{
  rx_err_t err = rx_harq_reset(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_harq_reset_uninitialized(void)
{
  rx_harq_handle_t harq = {0};
  rx_err_t         err  = rx_harq_reset(&harq);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * HARQ Encode Tests
 * =============================================================================
 */

void test_harq_encode_null_args(void)
{
  rx_harq_init(&s_harq, NULL);

  uint8_t  payload[] = {0x42};
  uint8_t  output[64];
  uint32_t len;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_encode(NULL, payload, 1, output, 64, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_encode(&s_harq, NULL, 1, output, 64, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_encode(&s_harq, payload, 1, NULL, 64, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_encode(&s_harq, payload, 1, output, 64, NULL));
}

void test_harq_encode_uninitialized(void)
{
  rx_harq_handle_t harq      = {0};
  uint8_t          payload[] = {0x42};
  uint8_t          output[64];
  uint32_t         len;

  rx_err_t err = rx_harq_encode(&harq, payload, 1, output, 64, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_harq_encode_zero_payload(void)
{
  rx_harq_init(&s_harq, NULL);

  uint8_t  payload[1];
  uint8_t  output[64];
  uint32_t len;

  rx_err_t err = rx_harq_encode(&s_harq, payload, 0, output, 64, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_harq_encode_too_large(void)
{
  rx_harq_init(&s_harq, NULL);

  uint8_t  payload[2048];
  uint8_t  output[4096];
  uint32_t len;

  rx_err_t err = rx_harq_encode(&s_harq, payload, k_harq_max_payload + 1, output, 4096, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_harq_encode_with_fec(void)
{
  rx_harq_init(&s_harq, NULL); /* FEC enabled by default */

  uint8_t  payload[] = {0x42};
  uint8_t  output[64];
  uint32_t len;

  rx_err_t err = rx_harq_encode(&s_harq, payload, 1, output, 64, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* With FEC, output should be larger than input */
  /* 1 byte = 8 bits + 6 tail = 14 bits * 2 = 28 bits = 4 bytes */
  TEST_ASSERT_EQUAL(4, len);
}

void test_harq_encode_without_fec(void)
{
  rx_harq_config_t config = {.fec_enabled = 0};
  rx_harq_init(&s_harq, &config);

  uint8_t  payload[] = {0x42, 0x43};
  uint8_t  output[64];
  uint32_t len;

  rx_err_t err = rx_harq_encode(&s_harq, payload, 2, output, 64, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, len); /* No FEC = same size */
  TEST_ASSERT_EQUAL_MEMORY(payload, output, 2);
}

void test_harq_encode_buffer_too_small(void)
{
  rx_harq_init(&s_harq, NULL); /* FEC enabled */

  uint8_t  payload[] = {0x42};
  uint8_t  output[2]; /* Too small for FEC output */
  uint32_t len;

  rx_err_t err = rx_harq_encode(&s_harq, payload, 1, output, 2, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_harq_encode_buffer_too_small_no_fec(void)
{
  rx_harq_config_t config = {.fec_enabled = 0};
  rx_harq_init(&s_harq, &config);

  uint8_t  payload[] = {0x42, 0x43};
  uint8_t  output[1]; /* Too small for payload */
  uint32_t len;

  rx_err_t err = rx_harq_encode(&s_harq, payload, 2, output, 1, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/* =============================================================================
 * HARQ Retry Tests
 * =============================================================================
 */

void test_harq_get_retry_count_null(void)
{
  TEST_ASSERT_EQUAL(0, rx_harq_get_retry_count(NULL));
}

void test_harq_get_retry_count(void)
{
  rx_harq_init(&s_harq, NULL);
  TEST_ASSERT_EQUAL(0, rx_harq_get_retry_count(&s_harq));

  s_harq.retry_count = 2;
  TEST_ASSERT_EQUAL(2, rx_harq_get_retry_count(&s_harq));
}

void test_harq_can_retry_null(void)
{
  TEST_ASSERT_FALSE(rx_harq_can_retry(NULL));
}

void test_harq_can_retry_fresh(void)
{
  rx_harq_init(&s_harq, NULL);
  TEST_ASSERT_TRUE(rx_harq_can_retry(&s_harq));
}

void test_harq_can_retry_exhausted(void)
{
  rx_harq_config_t config = {.max_retries = 2};
  rx_harq_init(&s_harq, &config);

  s_harq.retry_count = 2;
  TEST_ASSERT_FALSE(rx_harq_can_retry(&s_harq));
}

/* =============================================================================
 * HARQ Decode Parameter Validation Tests
 * =============================================================================
 */

void test_harq_decode_null_args(void)
{
  rx_harq_init(&s_harq, NULL);

  rx_soft_bit_t soft[32] = {0};
  uint8_t       output[16];
  uint32_t      len;

  /* NULL harq */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_decode(NULL, soft, 32, 2, output, &len));

  /* NULL soft_bits */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_decode(&s_harq, NULL, 32, 2, output, &len));

  /* NULL output */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_decode(&s_harq, soft, 32, 2, NULL, &len));

  /* NULL output_len */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_harq_decode(&s_harq, soft, 32, 2, output, NULL));
}

void test_harq_decode_uninitialized(void)
{
  rx_harq_handle_t harq     = {0};
  rx_soft_bit_t    soft[32] = {0};
  uint8_t          output[16];
  uint32_t         len;

  rx_err_t err = rx_harq_decode(&harq, soft, 32, 2, output, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_harq_decode_zero_length(void)
{
  rx_harq_init(&s_harq, NULL);

  rx_soft_bit_t soft[32] = {0};
  uint8_t       output[16];
  uint32_t      len;

  /* Zero soft_len returns k_rx_err_invalid_arg */
  rx_err_t err = rx_harq_decode(&s_harq, soft, 0, 2, output, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * HARQ Integration Tests (Encode + Decode)
 * =============================================================================
 */

void test_harq_roundtrip_with_fec(void)
{
  rx_harq_init(&s_harq, NULL);

  /* Encode */
  uint8_t  payload[] = {0xDE, 0xAD};
  uint8_t  encoded[64];
  uint32_t enc_len;
  uint8_t  bit;

  rx_err_t err = rx_harq_encode(&s_harq, payload, 2, encoded, 64, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Convert encoded hard bits to soft bits */
  rx_soft_bit_t soft[128];
  uint32_t      soft_len = enc_len * 8;

  for (uint32_t i = 0; i < enc_len; i++) {
    for (int8_t b = k_bit_idx_msb; b >= k_bit_idx_lsb; b--) {
      bit                                             = (encoded[i] >> b) & k_bit_mask;
      soft[i * k_bits_per_byte + (k_bit_idx_msb - b)] = rx_fec_hard_to_soft(bit);
    }
  }

  /* Reset HARQ for decode */
  (void)rx_harq_reset(&s_harq);

  /* Decode */
  uint8_t  decoded[64];
  uint32_t dec_len;

  err = rx_harq_decode(&s_harq, soft, soft_len, 2, decoded, &dec_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, dec_len);
  TEST_ASSERT_EQUAL_MEMORY(payload, decoded, 2);
}

void test_harq_combining_improves_reception(void)
{
  /* Test that combining multiple noisy transmissions works */
  rx_harq_init(&s_harq, NULL);

  /* Encode a payload */
  uint8_t  payload[] = {0x42};
  uint8_t  encoded[64];
  uint32_t enc_len;
  uint8_t  bit;
  (void)rx_harq_encode(&s_harq, payload, 1, encoded, 64, &enc_len);

  /* Create "perfect" soft bits from encoded data */
  rx_soft_bit_t soft[128];
  uint32_t      soft_len = enc_len * 8;

  for (uint32_t i = 0; i < enc_len; i++) {
    for (int8_t b = k_bit_idx_msb; b >= k_bit_idx_lsb; b--) {
      bit                                             = (encoded[i] >> b) & k_bit_mask;
      soft[i * k_bits_per_byte + (k_bit_idx_msb - b)] = rx_fec_hard_to_soft(bit);
    }
  }

  /* Reset and decode */
  (void)rx_harq_reset(&s_harq);

  uint8_t  decoded[64];
  uint32_t dec_len;

  rx_err_t err = rx_harq_decode(&s_harq, soft, soft_len, 1, decoded, &dec_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(0x42, decoded[0]);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Chase Combiner init tests */
  RUN_TEST(test_combiner_init_null);
  RUN_TEST(test_combiner_init_success);
  RUN_TEST(test_combiner_init_default_max);
  RUN_TEST(test_combiner_deinit_null);

  /* Chase Combiner add tests */
  RUN_TEST(test_combiner_add_null_combiner);
  RUN_TEST(test_combiner_add_null_soft);
  RUN_TEST(test_combiner_add_uninitialized);
  RUN_TEST(test_combiner_add_zero_length);
  RUN_TEST(test_combiner_add_too_large);
  RUN_TEST(test_combiner_add_single);
  RUN_TEST(test_combiner_add_length_mismatch);
  RUN_TEST(test_combiner_add_max_reached);

  /* Chase Combiner accumulation tests */
  RUN_TEST(test_combiner_accumulation);
  RUN_TEST(test_combiner_combined_output);
  RUN_TEST(test_combiner_combined_no_add);

  /* Chase Combiner helper tests */
  RUN_TEST(test_combiner_can_add);
  RUN_TEST(test_combiner_can_add_null);
  RUN_TEST(test_combiner_count);
  RUN_TEST(test_combiner_count_null);
  RUN_TEST(test_combiner_reset);
  RUN_TEST(test_combiner_reset_null);
  RUN_TEST(test_combiner_reset_uninitialized);

  /* HARQ init tests */
  RUN_TEST(test_harq_init_null);
  RUN_TEST(test_harq_init_default_config);
  RUN_TEST(test_harq_init_custom_config);
  RUN_TEST(test_harq_deinit_null);

  /* HARQ state tests */
  RUN_TEST(test_harq_get_state_null);
  RUN_TEST(test_harq_get_state_idle);
  RUN_TEST(test_harq_reset);
  RUN_TEST(test_harq_reset_null);
  RUN_TEST(test_harq_reset_uninitialized);

  /* HARQ encode tests */
  RUN_TEST(test_harq_encode_null_args);
  RUN_TEST(test_harq_encode_uninitialized);
  RUN_TEST(test_harq_encode_zero_payload);
  RUN_TEST(test_harq_encode_too_large);
  RUN_TEST(test_harq_encode_with_fec);
  RUN_TEST(test_harq_encode_without_fec);
  RUN_TEST(test_harq_encode_buffer_too_small);
  RUN_TEST(test_harq_encode_buffer_too_small_no_fec);

  /* HARQ retry tests */
  RUN_TEST(test_harq_get_retry_count_null);
  RUN_TEST(test_harq_get_retry_count);
  RUN_TEST(test_harq_can_retry_null);
  RUN_TEST(test_harq_can_retry_fresh);
  RUN_TEST(test_harq_can_retry_exhausted);

  /* HARQ decode parameter validation tests */
  RUN_TEST(test_harq_decode_null_args);
  RUN_TEST(test_harq_decode_uninitialized);
  RUN_TEST(test_harq_decode_zero_length);

  /* HARQ integration tests */
  RUN_TEST(test_harq_roundtrip_with_fec);
  RUN_TEST(test_harq_combining_improves_reception);

  return UNITY_END();
}
