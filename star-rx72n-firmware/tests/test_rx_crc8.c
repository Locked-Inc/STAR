/* tests/test_rx_crc8.c */

/**
 * @file test_rx_crc8.c
 * @brief Unit Tests for Dallas/Maxim CRC-8 Implementation
 *
 * Tests the CRC-8 implementation for bit-exact compatibility with
 * Dallas/Maxim OneWire devices (DS18B20, DS2431, etc.).
 *
 * Algorithm characteristics:
 * - Polynomial: x^8 + x^5 + x^4 + 1 (0x31, reflected 0x8C)
 * - Initial value: 0x00
 * - LSB-first processing
 * - No final XOR
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "rx_crc.h"
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
 * Constants for Test Clarity
 * =============================================================================
 */

/**
 * @brief Test vector size constants
 */
typedef enum {
  k_rom_code_size        = 8, /**< OneWire ROM code: family + serial + CRC */
  k_scratchpad_size      = 9, /**< DS18B20 scratchpad with CRC */
  k_rom_data_size        = 7, /**< ROM code without CRC byte */
  k_scratchpad_data_size = 8, /**< Scratchpad without CRC byte */
} test_size_constants_t;

/**
 * @brief DS18B20 family code
 */
typedef enum {
  k_ds18b20_family_code = 0x28,
} onewire_family_codes_t;

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

/**
 * @brief Test CRC-8 with NULL pointer returns 0
 */
void test_crc8_null_pointer(void)
{
  uint8_t crc = rx_crc8_maxim(NULL, 10);
  TEST_ASSERT_EQUAL_HEX8(0x00, crc);
}

/**
 * @brief Test CRC-8 with zero length returns 0
 */
void test_crc8_zero_length(void)
{
  uint8_t data[] = {0x01, 0x02, 0x03};
  uint8_t crc    = rx_crc8_maxim(data, 0);
  TEST_ASSERT_EQUAL_HEX8(0x00, crc);
}

/**
 * @brief Test CRC-8 with NULL pointer and zero length returns 0
 */
void test_crc8_null_zero_length(void)
{
  uint8_t crc = rx_crc8_maxim(NULL, 0);
  TEST_ASSERT_EQUAL_HEX8(0x00, crc);
}

/* =============================================================================
 * Single Byte Tests
 * =============================================================================
 */

/**
 * @brief Test CRC-8 of single byte 0x00
 *
 * CRC-8 Maxim of 0x00 is 0x00 (initial value XOR with 0x00, no shifts trigger
 * polynomial XOR since all bits are 0).
 */
void test_crc8_single_zero(void)
{
  uint8_t data[] = {0x00};
  uint8_t crc    = rx_crc8_maxim(data, 1);
  TEST_ASSERT_EQUAL_HEX8(0x00, crc);
}

/**
 * @brief Test CRC-8 of single byte 0xFF
 *
 * Verified against Maxim reference implementation.
 */
void test_crc8_single_ff(void)
{
  uint8_t data[] = {0xFF};
  uint8_t crc    = rx_crc8_maxim(data, 1);
  TEST_ASSERT_EQUAL_HEX8(0x35, crc);
}

/**
 * @brief Test CRC-8 of single byte 0x01 (LSB set)
 *
 * With 0x01, LSB=1 triggers polynomial XOR: 0x01 XOR 0x8C = 0x8C, then shift.
 */
void test_crc8_single_lsb_set(void)
{
  uint8_t data[] = {0x01};
  uint8_t crc    = rx_crc8_maxim(data, 1);
  TEST_ASSERT_EQUAL_HEX8(0x5E, crc);
}

/**
 * @brief Test CRC-8 of DS18B20 family code (0x28)
 *
 * Family code is the first byte of every DS18B20 ROM.
 */
void test_crc8_ds18b20_family_code(void)
{
  uint8_t data[] = {k_ds18b20_family_code};
  uint8_t crc    = rx_crc8_maxim(data, 1);
  TEST_ASSERT_EQUAL_HEX8(0xE1, crc);
}

/* =============================================================================
 * Known DS18B20 ROM Code Test Vectors
 *
 * ROM format: [Family(1)] [Serial(6)] [CRC(1)]
 * CRC is calculated over first 7 bytes.
 * =============================================================================
 */

/**
 * @brief Test CRC-8 validation for DS18B20 ROM code #1
 *
 * ROM: 28-FF-64-1E-81-16-05-DB
 * Family: 0x28 (DS18B20)
 * Serial: FF-64-1E-81-16-05
 * Expected CRC: 0xDB
 */
void test_crc8_ds18b20_rom_1(void)
{
  uint8_t rom[] = {0x28, 0xFF, 0x64, 0x1E, 0x81, 0x16, 0x05};
  uint8_t crc   = rx_crc8_maxim(rom, k_rom_data_size);
  TEST_ASSERT_EQUAL_HEX8(0xDB, crc);
}

/**
 * @brief Test CRC-8 validation for DS18B20 ROM code #2
 *
 * ROM: 28-00-00-00-00-00-00-1E
 * Family: 0x28 (DS18B20)
 * Serial: 00-00-00-00-00-00 (all zeros test case)
 * Expected CRC: 0x1E
 */
void test_crc8_ds18b20_rom_2(void)
{
  uint8_t rom[] = {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t crc   = rx_crc8_maxim(rom, k_rom_data_size);
  TEST_ASSERT_EQUAL_HEX8(0x1E, crc);
}

/**
 * @brief Test CRC-8 validation for DS18B20 ROM code #3
 *
 * ROM: 28-FF-FF-FF-FF-FF-FF-0C
 * Family: 0x28 (DS18B20)
 * Serial: FF-FF-FF-FF-FF-FF (all ones test case)
 * Expected CRC: 0x0C
 */
void test_crc8_ds18b20_rom_3(void)
{
  uint8_t rom[] = {0x28, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t crc   = rx_crc8_maxim(rom, k_rom_data_size);
  TEST_ASSERT_EQUAL_HEX8(0x0C, crc);
}

/**
 * @brief Test CRC-8 validation for DS18B20 ROM code #4
 *
 * ROM: 28-AA-BB-CC-DD-EE-01-67
 * Family: 0x28 (DS18B20)
 * Serial: AA-BB-CC-DD-EE-01
 * Expected CRC: 0x67
 */
void test_crc8_ds18b20_rom_4(void)
{
  uint8_t rom[] = {0x28, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
  uint8_t crc   = rx_crc8_maxim(rom, k_rom_data_size);
  TEST_ASSERT_EQUAL_HEX8(0x67, crc);
}

/**
 * @brief Test full ROM validation (CRC over all 8 bytes should equal 0)
 *
 * When CRC-8 is computed over all 8 bytes including the CRC byte,
 * the result should be 0x00 if the data is valid.
 */
void test_crc8_full_rom_validation(void)
{
  uint8_t rom_with_crc[] = {0x28, 0xFF, 0x64, 0x1E, 0x81, 0x16, 0x05, 0xDB};
  uint8_t crc            = rx_crc8_maxim(rom_with_crc, k_rom_code_size);
  TEST_ASSERT_EQUAL_HEX8(0x00, crc);
}

/* =============================================================================
 * DS18B20 Scratchpad Test Vectors
 *
 * Scratchpad format (9 bytes):
 * [TempLSB] [TempMSB] [TH] [TL] [Config] [Reserved] [Reserved] [Reserved] [CRC]
 * CRC is calculated over first 8 bytes.
 * =============================================================================
 */

/**
 * @brief Test CRC-8 for DS18B20 scratchpad at +25.0625C (power-on default)
 *
 * Temperature: 0x0191 = 25.0625C
 * Scratchpad: 91 01 4B 46 7F FF 0F 10 [CRC]
 */
void test_crc8_scratchpad_25c(void)
{
  uint8_t scratchpad[] = {0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0F, 0x10};
  uint8_t crc          = rx_crc8_maxim(scratchpad, k_scratchpad_data_size);
  TEST_ASSERT_EQUAL_HEX8(0x25, crc);
}

/**
 * @brief Test CRC-8 for DS18B20 scratchpad at +85C (power-on reset value)
 *
 * Temperature: 0x0550 = 85.0C (DS18B20 power-on reset value)
 * Scratchpad: 50 05 4B 46 7F FF 0C 10 [CRC]
 */
void test_crc8_scratchpad_85c_reset(void)
{
  uint8_t scratchpad[] = {0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10};
  uint8_t crc          = rx_crc8_maxim(scratchpad, k_scratchpad_data_size);
  TEST_ASSERT_EQUAL_HEX8(0x1C, crc);
}

/**
 * @brief Test CRC-8 for DS18B20 scratchpad at -10.125C
 *
 * Temperature: 0xFF5E = -10.125C (two's complement)
 * Scratchpad: 5E FF 4B 46 7F FF 02 10 [CRC]
 */
void test_crc8_scratchpad_negative_temp(void)
{
  uint8_t scratchpad[] = {0x5E, 0xFF, 0x4B, 0x46, 0x7F, 0xFF, 0x02, 0x10};
  uint8_t crc          = rx_crc8_maxim(scratchpad, k_scratchpad_data_size);
  TEST_ASSERT_EQUAL_HEX8(0xB6, crc);
}

/**
 * @brief Test full scratchpad validation (CRC over all 9 bytes should equal 0)
 */
void test_crc8_full_scratchpad_validation(void)
{
  uint8_t scratchpad_with_crc[] = {0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0F, 0x10, 0x25};
  uint8_t crc                   = rx_crc8_maxim(scratchpad_with_crc, k_scratchpad_size);
  TEST_ASSERT_EQUAL_HEX8(0x00, crc);
}

/* =============================================================================
 * Maxim Application Note Reference Vectors
 *
 * These test vectors are from Maxim application notes AN27 and AN187.
 * =============================================================================
 */

/**
 * @brief Test CRC-8 with ascending byte sequence
 *
 * Sequence: 0x01 0x02 0x03 0x04 0x05 0x06 0x07
 * Verified against reference implementation.
 */
void test_crc8_ascending_sequence(void)
{
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  uint8_t crc    = rx_crc8_maxim(data, sizeof(data));
  TEST_ASSERT_EQUAL_HEX8(0x0F, crc);
}

/**
 * @brief Test CRC-8 with descending byte sequence
 *
 * Sequence: 0x07 0x06 0x05 0x04 0x03 0x02 0x01
 * Demonstrates order sensitivity.
 */
void test_crc8_descending_sequence(void)
{
  uint8_t data[] = {0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
  uint8_t crc    = rx_crc8_maxim(data, sizeof(data));
  TEST_ASSERT_EQUAL_HEX8(0xF6, crc);
}

/**
 * @brief Test CRC-8 with alternating pattern 0xAA
 *
 * 0xAA = 10101010 binary, tests alternating bit pattern handling.
 */
void test_crc8_alternating_aa(void)
{
  uint8_t data[] = {0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t crc    = rx_crc8_maxim(data, sizeof(data));
  TEST_ASSERT_EQUAL_HEX8(0xF6, crc);
}

/**
 * @brief Test CRC-8 with alternating pattern 0x55
 *
 * 0x55 = 01010101 binary, complementary to 0xAA pattern.
 */
void test_crc8_alternating_55(void)
{
  uint8_t data[] = {0x55, 0x55, 0x55, 0x55};
  uint8_t crc    = rx_crc8_maxim(data, sizeof(data));
  TEST_ASSERT_EQUAL_HEX8(0x7B, crc);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Edge case tests */
  RUN_TEST(test_crc8_null_pointer);
  RUN_TEST(test_crc8_zero_length);
  RUN_TEST(test_crc8_null_zero_length);

  /* Single byte tests */
  RUN_TEST(test_crc8_single_zero);
  RUN_TEST(test_crc8_single_ff);
  RUN_TEST(test_crc8_single_lsb_set);
  RUN_TEST(test_crc8_ds18b20_family_code);

  /* DS18B20 ROM code tests */
  RUN_TEST(test_crc8_ds18b20_rom_1);
  RUN_TEST(test_crc8_ds18b20_rom_2);
  RUN_TEST(test_crc8_ds18b20_rom_3);
  RUN_TEST(test_crc8_ds18b20_rom_4);
  RUN_TEST(test_crc8_full_rom_validation);

  /* DS18B20 scratchpad tests */
  RUN_TEST(test_crc8_scratchpad_25c);
  RUN_TEST(test_crc8_scratchpad_85c_reset);
  RUN_TEST(test_crc8_scratchpad_negative_temp);
  RUN_TEST(test_crc8_full_scratchpad_validation);

  /* Maxim application note reference vectors */
  RUN_TEST(test_crc8_ascending_sequence);
  RUN_TEST(test_crc8_descending_sequence);
  RUN_TEST(test_crc8_alternating_aa);
  RUN_TEST(test_crc8_alternating_55);

  return UNITY_END();
}
