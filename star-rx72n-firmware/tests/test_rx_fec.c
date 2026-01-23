/* tests/test_rx_fec.c */

/**
 * @file test_rx_fec.c
 * @brief Unit Tests for FEC Codec (K=7 Convolutional + Viterbi)
 *
 * Tests the Forward Error Correction encoder and decoder.
 * Verifies bit-exact compatibility with Go implementation.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "rx_fec.h"
#include "unity.h"

/* Test fixtures */
static rx_fec_encoder_t s_encoder;
static rx_fec_decoder_t s_decoder;

/* Decoder survivors buffer (large enough for max test) */
static uint64_t s_survivors[k_fec_max_symbols];

/* Soft bits working buffer for rx_fec_decode_hard */
static rx_soft_bit_t s_soft_bits_buffer[k_fec_max_symbols * k_fec_num_outputs];

void setUp(void)
{
  rx_fec_encoder_init(&s_encoder);
  rx_fec_decoder_init(&s_decoder, s_survivors, k_fec_max_symbols);
}

void tearDown(void)
{
  rx_fec_encoder_deinit(&s_encoder);
  rx_fec_decoder_deinit(&s_decoder);
}

/* =============================================================================
 * Encoder Initialization Tests
 * =============================================================================
 */

void test_encoder_init_null(void)
{
  rx_err_t err = rx_fec_encoder_init(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_init_success(void)
{
  rx_fec_encoder_t enc;
  rx_err_t         err = rx_fec_encoder_init(&enc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, enc.initialized);
}

void test_encoder_deinit_null(void)
{
  rx_err_t err = rx_fec_encoder_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Decoder Initialization Tests
 * =============================================================================
 */

void test_decoder_init_null_dec(void)
{
  rx_err_t err = rx_fec_decoder_init(NULL, s_survivors, 100);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_decoder_init_null_buffer(void)
{
  rx_fec_decoder_t dec;
  rx_err_t         err = rx_fec_decoder_init(&dec, NULL, 100);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_decoder_init_zero_length(void)
{
  rx_fec_decoder_t dec;
  rx_err_t         err = rx_fec_decoder_init(&dec, s_survivors, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_decoder_init_success(void)
{
  rx_fec_decoder_t dec;
  rx_err_t         err = rx_fec_decoder_init(&dec, s_survivors, 100);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, dec.initialized);
}

/* =============================================================================
 * Encoded Length Calculation Tests
 * =============================================================================
 */

void test_encoded_len_zero(void)
{
  TEST_ASSERT_EQUAL(0, rx_fec_encoded_len(0));
}

void test_encoded_len_one_byte(void)
{
  /* 1 byte = 8 bits, + 6 tail bits = 14 bits, * 2 = 28 bits = 4 bytes */
  TEST_ASSERT_EQUAL(4, rx_fec_encoded_len(1));
}

void test_encoded_len_two_bytes(void)
{
  /* 2 bytes = 16 bits, + 6 tail bits = 22 bits, * 2 = 44 bits = 6 bytes */
  TEST_ASSERT_EQUAL(6, rx_fec_encoded_len(2));
}

void test_encoded_len_max_payload(void)
{
  /* 1024 bytes = 8192 bits, + 6 tail bits = 8198 bits, * 2 = 16396 bits */
  /* 16396 / 8 = 2049.5, rounded up = 2050 bytes */
  TEST_ASSERT_EQUAL(2050, rx_fec_encoded_len(1024));
}

/* =============================================================================
 * Encode Tests
 * =============================================================================
 */

void test_encode_null_args(void)
{
  uint8_t  input[] = {0x42};
  uint8_t  output[16];
  uint32_t len;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_encode(NULL, input, 1, output, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_encode(&s_encoder, NULL, 1, output, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_encode(&s_encoder, input, 1, NULL, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_encode(&s_encoder, input, 1, output, NULL));
}

void test_encode_uninitialized(void)
{
  rx_fec_encoder_t enc     = {0};
  uint8_t          input[] = {0x42};
  uint8_t          output[16];
  uint32_t         len;

  rx_err_t err = rx_fec_encode(&enc, input, 1, output, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encode_zero_length(void)
{
  uint8_t  input[] = {0x42};
  uint8_t  output[16];
  uint32_t len;

  rx_err_t err = rx_fec_encode(&s_encoder, input, 0, output, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encode_single_byte(void)
{
  uint8_t  input[] = {0x00};
  uint8_t  output[16];
  uint32_t len;

  rx_err_t err = rx_fec_encode(&s_encoder, input, 1, output, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(4, len); /* (8+6)*2/8 = 4 bytes */
}

void test_encode_deterministic(void)
{
  /* Encoding the same data twice should produce identical output */
  uint8_t  input[] = {0xDE, 0xAD, 0xBE, 0xEF};
  uint8_t  output1[16];
  uint8_t  output2[16];
  uint32_t len1, len2;

  rx_fec_encode(&s_encoder, input, 4, output1, &len1);

  /* Re-init encoder to reset state */
  rx_fec_encoder_deinit(&s_encoder);
  rx_fec_encoder_init(&s_encoder);

  rx_fec_encode(&s_encoder, input, 4, output2, &len2);

  TEST_ASSERT_EQUAL(len1, len2);
  TEST_ASSERT_EQUAL_MEMORY(output1, output2, len1);
}

/* =============================================================================
 * Soft/Hard Bit Conversion Tests
 * =============================================================================
 */

void test_hard_to_soft(void)
{
  TEST_ASSERT_EQUAL(k_soft_bit_min, rx_fec_hard_to_soft(0));
  TEST_ASSERT_EQUAL(k_soft_bit_max, rx_fec_hard_to_soft(1));
}

void test_soft_to_hard(void)
{
  TEST_ASSERT_EQUAL(0, rx_fec_soft_to_hard(-127));
  TEST_ASSERT_EQUAL(0, rx_fec_soft_to_hard(-1));
  TEST_ASSERT_EQUAL(1, rx_fec_soft_to_hard(0));
  TEST_ASSERT_EQUAL(1, rx_fec_soft_to_hard(1));
  TEST_ASSERT_EQUAL(1, rx_fec_soft_to_hard(127));
}

/* =============================================================================
 * Decode Tests
 * =============================================================================
 */

void test_decode_null_args(void)
{
  rx_soft_bit_t               soft[32];
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_soft_params_t params = {
    .soft_bits           = soft,
    .soft_len            = 32,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
  };

  /* NULL decoder */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_soft(NULL, &params));

  /* NULL params */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_soft(&s_decoder, NULL));

  /* NULL soft_bits */
  params.soft_bits = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_soft(&s_decoder, &params));
  params.soft_bits = soft;

  /* NULL output */
  params.output = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_soft(&s_decoder, &params));
  params.output = output;

  /* NULL output_len */
  params.output_len = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_soft(&s_decoder, &params));
}

void test_decode_uninitialized(void)
{
  rx_fec_decoder_t            dec = {0};
  rx_soft_bit_t               soft[32];
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_soft_params_t params = {
    .soft_bits           = soft,
    .soft_len            = 32,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
  };

  rx_err_t err = rx_fec_decode_soft(&dec, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_decode_odd_soft_length(void)
{
  rx_soft_bit_t               soft[33];
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_soft_params_t params = {
    .soft_bits           = soft,
    .soft_len            = 33,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
  };

  /* Soft bits must come in pairs */
  rx_err_t err = rx_fec_decode_soft(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_decode_zero_length(void)
{
  rx_soft_bit_t               soft[32];
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_soft_params_t params = {
    .soft_bits           = soft,
    .soft_len            = 0,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
  };

  rx_err_t err = rx_fec_decode_soft(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Decode Hard Decision Parameter Validation Tests
 * =============================================================================
 */

void test_decode_hard_null_args(void)
{
  uint8_t                     hard[16] = {0};
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_hard_params_t params = {
    .data                = hard,
    .data_len            = 16,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };

  /* NULL decoder */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_hard(NULL, &params));

  /* NULL params */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_hard(&s_decoder, NULL));

  /* NULL data */
  params.data = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_hard(&s_decoder, &params));
  params.data = hard;

  /* NULL output */
  params.output = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_hard(&s_decoder, &params));
  params.output = output;

  /* NULL output_len */
  params.output_len = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_hard(&s_decoder, &params));
  params.output_len = &len;

  /* NULL soft_bits_buffer */
  params.soft_bits_buffer = NULL;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_fec_decode_hard(&s_decoder, &params));
}

void test_decode_hard_uninitialized(void)
{
  rx_fec_decoder_t            dec      = {0};
  uint8_t                     hard[16] = {0};
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_hard_params_t params = {
    .data                = hard,
    .data_len            = 16,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };

  rx_err_t err = rx_fec_decode_hard(&dec, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_decode_hard_zero_length(void)
{
  uint8_t                     hard[16] = {0};
  uint8_t                     output[16];
  uint32_t                    len;
  rx_fec_decode_hard_params_t params = {
    .data                = hard,
    .data_len            = 0,
    .expected_output_len = 2,
    .output              = output,
    .output_len          = &len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };

  rx_err_t err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Encode/Decode Round-Trip Tests
 * =============================================================================
 */

void test_roundtrip_single_byte(void)
{
  uint8_t  input[] = {0x42};
  uint8_t  encoded[16];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  /* Encode */
  rx_err_t err = rx_fec_encode(&s_encoder, input, 1, encoded, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Decode using hard decision */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 1,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, dec_len);
  TEST_ASSERT_EQUAL_HEX8(0x42, decoded[0]);
}

void test_roundtrip_multi_byte(void)
{
  uint8_t  input[] = {0xDE, 0xAD, 0xBE, 0xEF};
  uint8_t  encoded[32];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  /* Encode */
  rx_err_t err = rx_fec_encode(&s_encoder, input, 4, encoded, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Decode */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 4,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(4, dec_len);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, 4);
}

void test_roundtrip_all_zeros(void)
{
  uint8_t  input[8];
  uint8_t  encoded[32];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  memset(input, 0x00, 8);

  /* Encode */
  rx_err_t err = rx_fec_encode(&s_encoder, input, 8, encoded, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Decode */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 8,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(8, dec_len);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, 8);
}

void test_roundtrip_all_ones(void)
{
  uint8_t  input[8];
  uint8_t  encoded[32];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  memset(input, 0xFF, 8);

  /* Encode */
  rx_err_t err = rx_fec_encode(&s_encoder, input, 8, encoded, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Decode */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 8,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(8, dec_len);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, 8);
}

void test_roundtrip_alternating_pattern(void)
{
  uint8_t  input[] = {0xAA, 0x55, 0xAA, 0x55};
  uint8_t  encoded[32];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  /* Encode */
  rx_err_t err = rx_fec_encode(&s_encoder, input, 4, encoded, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Decode */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 4,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(4, dec_len);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, 4);
}

void test_roundtrip_larger_payload(void)
{
  uint8_t  input[32];
  uint8_t  encoded[128];
  uint8_t  decoded[64];
  uint32_t enc_len, dec_len;

  /* Fill with ascending pattern */
  for (uint32_t i = 0; i < 32; i++) {
    input[i] = (uint8_t)i;
  }

  /* Encode */
  rx_err_t err = rx_fec_encode(&s_encoder, input, 32, encoded, &enc_len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Decode */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 32,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(32, dec_len);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, 32);
}

/* =============================================================================
 * Error Correction Tests
 * =============================================================================
 */

void test_single_bit_error_correction(void)
{
  uint8_t  input[] = {0x42};
  uint8_t  encoded[16];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  /* Encode */
  rx_fec_encode(&s_encoder, input, 1, encoded, &enc_len);

  /* Flip a single bit in the encoded data */
  encoded[0] ^= 0x01;

  /* Decode should still recover the original */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 1,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  rx_err_t err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(0x42, decoded[0]);
}

void test_multiple_bit_error_correction(void)
{
  uint8_t  input[] = {0xAB, 0xCD};
  uint8_t  encoded[16];
  uint8_t  decoded[16];
  uint32_t enc_len, dec_len;

  /* Encode */
  rx_fec_encode(&s_encoder, input, 2, encoded, &enc_len);

  /* Flip a couple bits in different positions */
  encoded[0] ^= 0x02;
  encoded[1] ^= 0x08;

  /* K=7 Viterbi can typically correct multiple scattered errors */
  rx_fec_decode_hard_params_t params = {
    .data                = encoded,
    .data_len            = enc_len,
    .expected_output_len = 2,
    .output              = decoded,
    .output_len          = &dec_len,
    .soft_bits_buffer    = s_soft_bits_buffer,
    .soft_buffer_len     = sizeof(s_soft_bits_buffer) / sizeof(s_soft_bits_buffer[0]),
  };
  rx_err_t err = rx_fec_decode_hard(&s_decoder, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, 2);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Encoder init tests */
  RUN_TEST(test_encoder_init_null);
  RUN_TEST(test_encoder_init_success);
  RUN_TEST(test_encoder_deinit_null);

  /* Decoder init tests */
  RUN_TEST(test_decoder_init_null_dec);
  RUN_TEST(test_decoder_init_null_buffer);
  RUN_TEST(test_decoder_init_zero_length);
  RUN_TEST(test_decoder_init_success);

  /* Encoded length tests */
  RUN_TEST(test_encoded_len_zero);
  RUN_TEST(test_encoded_len_one_byte);
  RUN_TEST(test_encoded_len_two_bytes);
  RUN_TEST(test_encoded_len_max_payload);

  /* Encode tests */
  RUN_TEST(test_encode_null_args);
  RUN_TEST(test_encode_uninitialized);
  RUN_TEST(test_encode_zero_length);
  RUN_TEST(test_encode_single_byte);
  RUN_TEST(test_encode_deterministic);

  /* Soft/hard bit conversion tests */
  RUN_TEST(test_hard_to_soft);
  RUN_TEST(test_soft_to_hard);

  /* Decode soft tests */
  RUN_TEST(test_decode_null_args);
  RUN_TEST(test_decode_uninitialized);
  RUN_TEST(test_decode_odd_soft_length);
  RUN_TEST(test_decode_zero_length);

  /* Decode hard tests */
  RUN_TEST(test_decode_hard_null_args);
  RUN_TEST(test_decode_hard_uninitialized);
  RUN_TEST(test_decode_hard_zero_length);

  /* Round-trip tests */
  RUN_TEST(test_roundtrip_single_byte);
  RUN_TEST(test_roundtrip_multi_byte);
  RUN_TEST(test_roundtrip_all_zeros);
  RUN_TEST(test_roundtrip_all_ones);
  RUN_TEST(test_roundtrip_alternating_pattern);
  RUN_TEST(test_roundtrip_larger_payload);

  /* Error correction tests */
  RUN_TEST(test_single_bit_error_correction);
  RUN_TEST(test_multiple_bit_error_correction);

  return UNITY_END();
}
