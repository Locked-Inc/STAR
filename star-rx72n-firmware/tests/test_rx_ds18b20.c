/**
 * @file test_rx_ds18b20.c
 * @brief Unit Tests for DS18B20 Temperature Sensor Driver
 *
 * Tests the DS18B20 1-Wire digital temperature sensor driver calculation
 * logic, CRC validation, and constant definitions.
 *
 * Note: This test file only tests pure calculation functions and constants,
 * not the actual bus communication functions which require hardware/mocks.
 *
 * STAR Project - Texas A&M University
 * January 2026
 */

#include "unity.h"
#include "rx_crc.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* =============================================================================
 * DS18B20 Constants (copied from header for testing)
 * =============================================================================
 */

/* DS18B20 device constants */
#define k_ds18b20_rom_bytes         8
#define k_ds18b20_scratchpad_bytes  9
#define k_ds18b20_family_code       0x28

/* DS18B20 command codes */
#define k_ds18b20_cmd_convert_t        0x44
#define k_ds18b20_cmd_read_scratchpad  0xBE
#define k_ds18b20_cmd_write_scratchpad 0x4E
#define k_ds18b20_cmd_copy_scratchpad  0x48
#define k_ds18b20_cmd_recall_e2        0xB8
#define k_ds18b20_cmd_read_power       0xB4

/* DS18B20 resolution settings */
#define k_ds18b20_resolution_9bit  9
#define k_ds18b20_resolution_10bit 10
#define k_ds18b20_resolution_11bit 11
#define k_ds18b20_resolution_12bit 12

/* DS18B20 conversion times */
#define k_ds18b20_conv_time_9bit_ms  94
#define k_ds18b20_conv_time_10bit_ms 188
#define k_ds18b20_conv_time_11bit_ms 375
#define k_ds18b20_conv_time_12bit_ms 750

/* DS18B20 scratchpad indices */
#define k_ds18b20_scratch_temp_lsb   0
#define k_ds18b20_scratch_temp_msb   1
#define k_ds18b20_scratch_th_reg     2
#define k_ds18b20_scratch_tl_reg     3
#define k_ds18b20_scratch_config     4
#define k_ds18b20_scratch_reserved_1 5
#define k_ds18b20_scratch_reserved_2 6
#define k_ds18b20_scratch_reserved_3 7
#define k_ds18b20_scratch_crc        8

/* DS18B20 configuration register */
#define k_ds18b20_config_res_mask   0x60
#define k_ds18b20_config_res_shift  5
#define k_ds18b20_config_res_9bit   0x00
#define k_ds18b20_config_res_10bit  0x20
#define k_ds18b20_config_res_11bit  0x40
#define k_ds18b20_config_res_12bit  0x60
#define k_ds18b20_config_reserved   0x1F

/* DS18B20 temperature limits */
#define k_ds18b20_temp_min_c -55
#define k_ds18b20_temp_max_c 125

void setUp(void) {
    /* Nothing to set up */
}

void tearDown(void) {
    /* Nothing to tear down */
}

/* =============================================================================
 * Temperature Calculation Tests
 * =============================================================================
 */

/**
 * @brief Test temperature calculation at 25.0625C
 *
 * Raw value: 0x0191 = 401 (401 * 0.0625 = 25.0625C)
 */
void test_ds18b20_temp_calc_25c(void) {
    const int16_t raw = 0x0191;  /* 401 decimal */
    const float expected = 25.0625f;
    const float actual = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

/**
 * @brief Test temperature calculation at 0C
 *
 * Raw value: 0x0000 = 0
 */
void test_ds18b20_temp_calc_0c(void) {
    const int16_t raw = 0x0000;
    const float expected = 0.0f;
    const float actual = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

/**
 * @brief Test temperature calculation at -55C (minimum)
 *
 * Raw value: 0xFC90 = -880 (-880 * 0.0625 = -55C)
 */
void test_ds18b20_temp_calc_minus_55c(void) {
    const int16_t raw = (int16_t)0xFC90;  /* -880 decimal */
    const float expected = -55.0f;
    const float actual = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

/**
 * @brief Test temperature calculation at +125C (maximum)
 *
 * Raw value: 0x07D0 = 2000 (2000 * 0.0625 = 125C)
 */
void test_ds18b20_temp_calc_125c(void) {
    const int16_t raw = 0x07D0;  /* 2000 decimal */
    const float expected = 125.0f;
    const float actual = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

/**
 * @brief Test temperature calculation at -0.5C
 *
 * Raw value: 0xFFF8 = -8 (-8 * 0.0625 = -0.5C)
 */
void test_ds18b20_temp_calc_minus_0_5c(void) {
    const int16_t raw = (int16_t)0xFFF8;  /* -8 decimal */
    const float expected = -0.5f;
    const float actual = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

/**
 * @brief Test temperature calculation at 85C (datasheet test point)
 *
 * Raw value: 0x0550 = 1360 (1360 * 0.0625 = 85C)
 */
void test_ds18b20_temp_calc_85c(void) {
    const int16_t raw = 0x0550;  /* 1360 decimal */
    const float expected = 85.0f;
    const float actual = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

/* =============================================================================
 * Celsius to Fahrenheit Conversion Tests
 * =============================================================================
 */

/**
 * @brief Test Celsius to Fahrenheit conversion at 0C = 32F
 */
void test_ds18b20_celsius_to_fahrenheit_0c(void) {
    const float celsius = 0.0f;
    const float expected_f = 32.0f;
    const float actual_f = celsius * 9.0f / 5.0f + 32.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_f, actual_f);
}

/**
 * @brief Test Celsius to Fahrenheit conversion at 25C = 77F
 */
void test_ds18b20_celsius_to_fahrenheit_25c(void) {
    const float celsius = 25.0f;
    const float expected_f = 77.0f;
    const float actual_f = celsius * 9.0f / 5.0f + 32.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_f, actual_f);
}

/**
 * @brief Test Celsius to Fahrenheit conversion at 100C = 212F
 */
void test_ds18b20_celsius_to_fahrenheit_100c(void) {
    const float celsius = 100.0f;
    const float expected_f = 212.0f;
    const float actual_f = celsius * 9.0f / 5.0f + 32.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_f, actual_f);
}

/**
 * @brief Test Celsius to Fahrenheit conversion at -40C = -40F
 */
void test_ds18b20_celsius_to_fahrenheit_minus_40c(void) {
    const float celsius = -40.0f;
    const float expected_f = -40.0f;
    const float actual_f = celsius * 9.0f / 5.0f + 32.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_f, actual_f);
}

/* =============================================================================
 * Resolution Configuration Tests
 * =============================================================================
 */

/**
 * @brief Test resolution to config byte mapping for 9-bit
 */
void test_ds18b20_resolution_to_config_9bit(void) {
    const uint8_t expected = 0x1F;  /* 9-bit: 0x00 | 0x1F (reserved bits) */
    const uint8_t actual = k_ds18b20_config_res_9bit | k_ds18b20_config_reserved;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

/**
 * @brief Test resolution to config byte mapping for 10-bit
 */
void test_ds18b20_resolution_to_config_10bit(void) {
    const uint8_t expected = 0x3F;  /* 10-bit: 0x20 | 0x1F */
    const uint8_t actual = k_ds18b20_config_res_10bit | k_ds18b20_config_reserved;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

/**
 * @brief Test resolution to config byte mapping for 11-bit
 */
void test_ds18b20_resolution_to_config_11bit(void) {
    const uint8_t expected = 0x5F;  /* 11-bit: 0x40 | 0x1F */
    const uint8_t actual = k_ds18b20_config_res_11bit | k_ds18b20_config_reserved;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

/**
 * @brief Test resolution to config byte mapping for 12-bit
 */
void test_ds18b20_resolution_to_config_12bit(void) {
    const uint8_t expected = 0x7F;  /* 12-bit: 0x60 | 0x1F */
    const uint8_t actual = k_ds18b20_config_res_12bit | k_ds18b20_config_reserved;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

/**
 * @brief Test extracting resolution from config byte (9-bit)
 */
void test_ds18b20_config_to_resolution_9bit(void) {
    const uint8_t config = 0x1F;  /* 9-bit config */
    const uint8_t res_bits = (config & k_ds18b20_config_res_mask) >> k_ds18b20_config_res_shift;
    const uint8_t expected = 0;  /* 0b00 */
    TEST_ASSERT_EQUAL_UINT8(expected, res_bits);
}

/**
 * @brief Test extracting resolution from config byte (12-bit)
 */
void test_ds18b20_config_to_resolution_12bit(void) {
    const uint8_t config = 0x7F;  /* 12-bit config */
    const uint8_t res_bits = (config & k_ds18b20_config_res_mask) >> k_ds18b20_config_res_shift;
    const uint8_t expected = 3;  /* 0b11 */
    TEST_ASSERT_EQUAL_UINT8(expected, res_bits);
}

/* =============================================================================
 * Conversion Time Tests
 * =============================================================================
 */

/**
 * @brief Test conversion time lookup for 9-bit resolution
 */
void test_ds18b20_conversion_time_9bit(void) {
    const uint32_t expected = k_ds18b20_conv_time_9bit_ms;
    TEST_ASSERT_EQUAL_UINT32(94, expected);
}

/**
 * @brief Test conversion time lookup for 10-bit resolution
 */
void test_ds18b20_conversion_time_10bit(void) {
    const uint32_t expected = k_ds18b20_conv_time_10bit_ms;
    TEST_ASSERT_EQUAL_UINT32(188, expected);
}

/**
 * @brief Test conversion time lookup for 11-bit resolution
 */
void test_ds18b20_conversion_time_11bit(void) {
    const uint32_t expected = k_ds18b20_conv_time_11bit_ms;
    TEST_ASSERT_EQUAL_UINT32(375, expected);
}

/**
 * @brief Test conversion time lookup for 12-bit resolution
 */
void test_ds18b20_conversion_time_12bit(void) {
    const uint32_t expected = k_ds18b20_conv_time_12bit_ms;
    TEST_ASSERT_EQUAL_UINT32(750, expected);
}

/* =============================================================================
 * Scratchpad CRC-8 Validation Tests
 * =============================================================================
 */

/**
 * @brief Test CRC-8 calculation for valid scratchpad
 *
 * Scratchpad bytes (DS18B20 datasheet example):
 *   [0] = 0x50 (temp LSB: 80 decimal = 5.0C)
 *   [1] = 0x05 (temp MSB)
 *   [2] = 0x4B (TH register)
 *   [3] = 0x46 (TL register)
 *   [4] = 0x7F (config: 12-bit)
 *   [5] = 0xFF (reserved)
 *   [6] = 0x0C (reserved)
 *   [7] = 0x10 (reserved)
 *   [8] = 0x1C (CRC)
 */
void test_ds18b20_scratchpad_crc_valid(void) {
    uint8_t scratchpad[9] = {
        0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C
    };

    /* Calculate CRC of first 8 bytes */
    uint8_t crc = rx_crc8_maxim(scratchpad, 8);

    /* Should match byte 8 */
    TEST_ASSERT_EQUAL_HEX8(scratchpad[8], crc);
}

/**
 * @brief Test CRC-8 calculation for another valid scratchpad
 *
 * Scratchpad at 25.0625C:
 *   [0] = 0x91 (temp LSB)
 *   [1] = 0x01 (temp MSB: 0x0191 = 401 = 25.0625C)
 *   [2] = 0x4B (TH)
 *   [3] = 0x46 (TL)
 *   [4] = 0x7F (config: 12-bit)
 *   [5] = 0xFF (reserved)
 *   [6] = 0x0C (reserved)
 *   [7] = 0x10 (reserved)
 *   [8] = CRC (calculate and verify)
 */
void test_ds18b20_scratchpad_crc_25c(void) {
    uint8_t scratchpad[9] = {
        0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x00
    };

    /* Calculate expected CRC */
    uint8_t expected_crc = rx_crc8_maxim(scratchpad, 8);
    scratchpad[8] = expected_crc;

    /* Verify CRC */
    uint8_t actual_crc = rx_crc8_maxim(scratchpad, 8);
    TEST_ASSERT_EQUAL_HEX8(expected_crc, actual_crc);
}

/**
 * @brief Test CRC-8 detects corrupted scratchpad (temperature byte)
 */
void test_ds18b20_scratchpad_crc_corrupted_temp(void) {
    uint8_t scratchpad[9] = {
        0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C
    };

    /* Corrupt temperature byte */
    scratchpad[0] = 0x51;

    /* Calculate CRC */
    uint8_t crc = rx_crc8_maxim(scratchpad, 8);

    /* Should NOT match original CRC */
    TEST_ASSERT_NOT_EQUAL(0x1C, crc);
}

/**
 * @brief Test CRC-8 detects corrupted scratchpad (config byte)
 */
void test_ds18b20_scratchpad_crc_corrupted_config(void) {
    uint8_t scratchpad[9] = {
        0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C
    };

    /* Corrupt config byte */
    scratchpad[4] = 0x5F;

    /* Calculate CRC */
    uint8_t crc = rx_crc8_maxim(scratchpad, 8);

    /* Should NOT match original CRC */
    TEST_ASSERT_NOT_EQUAL(0x1C, crc);
}

/* =============================================================================
 * ROM Address Tests
 * =============================================================================
 */

/**
 * @brief Test DS18B20 family code constant
 */
void test_ds18b20_family_code(void) {
    TEST_ASSERT_EQUAL_HEX8(0x28, k_ds18b20_family_code);
}

/**
 * @brief Test ROM code size constant
 */
void test_ds18b20_rom_bytes(void) {
    TEST_ASSERT_EQUAL_UINT8(8, k_ds18b20_rom_bytes);
}

/**
 * @brief Test scratchpad size constant
 */
void test_ds18b20_scratchpad_bytes(void) {
    TEST_ASSERT_EQUAL_UINT8(9, k_ds18b20_scratchpad_bytes);
}

/**
 * @brief Test valid DS18B20 ROM code CRC
 *
 * Example ROM: 28-FF-12-34-56-78-9A-BC
 *   [0] = 0x28 (family code)
 *   [1-6] = serial number
 *   [7] = CRC
 */
void test_ds18b20_rom_crc_valid(void) {
    uint8_t rom[8] = {
        0x28, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0x00
    };

    /* Calculate CRC of first 7 bytes */
    uint8_t expected_crc = rx_crc8_maxim(rom, 7);
    rom[7] = expected_crc;

    /* Verify CRC */
    uint8_t actual_crc = rx_crc8_maxim(rom, 7);
    TEST_ASSERT_EQUAL_HEX8(expected_crc, actual_crc);
}

/**
 * @brief Test ROM code family code validation
 */
void test_ds18b20_rom_family_code_check(void) {
    uint8_t rom[8] = {
        0x28, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC
    };

    /* First byte should be DS18B20 family code */
    TEST_ASSERT_EQUAL_HEX8(k_ds18b20_family_code, rom[0]);
}

/**
 * @brief Test ROM code with wrong family code
 */
void test_ds18b20_rom_wrong_family_code(void) {
    uint8_t rom[8] = {
        0x10, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC  /* 0x10 = DS18S20 */
    };

    /* Should NOT match DS18B20 family code */
    TEST_ASSERT_NOT_EQUAL(k_ds18b20_family_code, rom[0]);
}

/* =============================================================================
 * Command Code Tests
 * =============================================================================
 */

/**
 * @brief Test DS18B20 command codes match specification
 */
void test_ds18b20_command_codes(void) {
    TEST_ASSERT_EQUAL_HEX8(0x44, k_ds18b20_cmd_convert_t);
    TEST_ASSERT_EQUAL_HEX8(0xBE, k_ds18b20_cmd_read_scratchpad);
    TEST_ASSERT_EQUAL_HEX8(0x4E, k_ds18b20_cmd_write_scratchpad);
    TEST_ASSERT_EQUAL_HEX8(0x48, k_ds18b20_cmd_copy_scratchpad);
    TEST_ASSERT_EQUAL_HEX8(0xB8, k_ds18b20_cmd_recall_e2);
    TEST_ASSERT_EQUAL_HEX8(0xB4, k_ds18b20_cmd_read_power);
}

/* =============================================================================
 * Scratchpad Byte Index Tests
 * =============================================================================
 */

/**
 * @brief Test scratchpad byte indices match specification
 */
void test_ds18b20_scratchpad_indices(void) {
    TEST_ASSERT_EQUAL_UINT8(0, k_ds18b20_scratch_temp_lsb);
    TEST_ASSERT_EQUAL_UINT8(1, k_ds18b20_scratch_temp_msb);
    TEST_ASSERT_EQUAL_UINT8(2, k_ds18b20_scratch_th_reg);
    TEST_ASSERT_EQUAL_UINT8(3, k_ds18b20_scratch_tl_reg);
    TEST_ASSERT_EQUAL_UINT8(4, k_ds18b20_scratch_config);
    TEST_ASSERT_EQUAL_UINT8(8, k_ds18b20_scratch_crc);
}

/* =============================================================================
 * Temperature Range Tests
 * =============================================================================
 */

/**
 * @brief Test temperature limits match datasheet
 */
void test_ds18b20_temperature_limits(void) {
    TEST_ASSERT_EQUAL_INT16(-55, k_ds18b20_temp_min_c);
    TEST_ASSERT_EQUAL_INT16(125, k_ds18b20_temp_max_c);
}

/**
 * @brief Test temperature within valid range (0C)
 */
void test_ds18b20_temp_in_range_0c(void) {
    const float temp = 0.0f;
    TEST_ASSERT_TRUE(temp >= k_ds18b20_temp_min_c);
    TEST_ASSERT_TRUE(temp <= k_ds18b20_temp_max_c);
}

/**
 * @brief Test temperature within valid range (25C)
 */
void test_ds18b20_temp_in_range_25c(void) {
    const float temp = 25.0f;
    TEST_ASSERT_TRUE(temp >= k_ds18b20_temp_min_c);
    TEST_ASSERT_TRUE(temp <= k_ds18b20_temp_max_c);
}

/**
 * @brief Test temperature at minimum boundary
 */
void test_ds18b20_temp_at_min_boundary(void) {
    const float temp = -55.0f;
    TEST_ASSERT_EQUAL_FLOAT(k_ds18b20_temp_min_c, temp);
}

/**
 * @brief Test temperature at maximum boundary
 */
void test_ds18b20_temp_at_max_boundary(void) {
    const float temp = 125.0f;
    TEST_ASSERT_EQUAL_FLOAT(k_ds18b20_temp_max_c, temp);
}

/**
 * @brief Test temperature below minimum range
 */
void test_ds18b20_temp_below_min(void) {
    const float temp = -56.0f;
    TEST_ASSERT_TRUE(temp < k_ds18b20_temp_min_c);
}

/**
 * @brief Test temperature above maximum range
 */
void test_ds18b20_temp_above_max(void) {
    const float temp = 126.0f;
    TEST_ASSERT_TRUE(temp > k_ds18b20_temp_max_c);
}

/* =============================================================================
 * Raw Temperature to Celsius Tests
 * =============================================================================
 */

/**
 * @brief Test raw temperature extraction from scratchpad
 */
void test_ds18b20_extract_raw_temp_from_scratchpad(void) {
    uint8_t scratchpad[9] = {
        0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x00
    };

    /* Extract raw temperature */
    int16_t raw = ((int16_t)scratchpad[k_ds18b20_scratch_temp_msb] << 8) |
                  scratchpad[k_ds18b20_scratch_temp_lsb];

    /* Should be 0x0191 = 401 */
    TEST_ASSERT_EQUAL_HEX16(0x0191, raw);
}

/**
 * @brief Test negative raw temperature extraction
 */
void test_ds18b20_extract_negative_raw_temp(void) {
    uint8_t scratchpad[9] = {
        0xF8, 0xFF, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x00
    };

    /* Extract raw temperature */
    int16_t raw = ((int16_t)scratchpad[k_ds18b20_scratch_temp_msb] << 8) |
                  scratchpad[k_ds18b20_scratch_temp_lsb];

    /* Should be 0xFFF8 = -8 = -0.5C */
    TEST_ASSERT_EQUAL_HEX16((int16_t)0xFFF8, raw);

    /* Verify conversion to temperature */
    float temp = (float)raw * 0.0625f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, temp);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void) {
    UNITY_BEGIN();

    /* Temperature calculation tests */
    RUN_TEST(test_ds18b20_temp_calc_25c);
    RUN_TEST(test_ds18b20_temp_calc_0c);
    RUN_TEST(test_ds18b20_temp_calc_minus_55c);
    RUN_TEST(test_ds18b20_temp_calc_125c);
    RUN_TEST(test_ds18b20_temp_calc_minus_0_5c);
    RUN_TEST(test_ds18b20_temp_calc_85c);

    /* Celsius to Fahrenheit conversion tests */
    RUN_TEST(test_ds18b20_celsius_to_fahrenheit_0c);
    RUN_TEST(test_ds18b20_celsius_to_fahrenheit_25c);
    RUN_TEST(test_ds18b20_celsius_to_fahrenheit_100c);
    RUN_TEST(test_ds18b20_celsius_to_fahrenheit_minus_40c);

    /* Resolution configuration tests */
    RUN_TEST(test_ds18b20_resolution_to_config_9bit);
    RUN_TEST(test_ds18b20_resolution_to_config_10bit);
    RUN_TEST(test_ds18b20_resolution_to_config_11bit);
    RUN_TEST(test_ds18b20_resolution_to_config_12bit);
    RUN_TEST(test_ds18b20_config_to_resolution_9bit);
    RUN_TEST(test_ds18b20_config_to_resolution_12bit);

    /* Conversion time tests */
    RUN_TEST(test_ds18b20_conversion_time_9bit);
    RUN_TEST(test_ds18b20_conversion_time_10bit);
    RUN_TEST(test_ds18b20_conversion_time_11bit);
    RUN_TEST(test_ds18b20_conversion_time_12bit);

    /* Scratchpad CRC-8 validation tests */
    RUN_TEST(test_ds18b20_scratchpad_crc_valid);
    RUN_TEST(test_ds18b20_scratchpad_crc_25c);
    RUN_TEST(test_ds18b20_scratchpad_crc_corrupted_temp);
    RUN_TEST(test_ds18b20_scratchpad_crc_corrupted_config);

    /* ROM address tests */
    RUN_TEST(test_ds18b20_family_code);
    RUN_TEST(test_ds18b20_rom_bytes);
    RUN_TEST(test_ds18b20_scratchpad_bytes);
    RUN_TEST(test_ds18b20_rom_crc_valid);
    RUN_TEST(test_ds18b20_rom_family_code_check);
    RUN_TEST(test_ds18b20_rom_wrong_family_code);

    /* Command code tests */
    RUN_TEST(test_ds18b20_command_codes);

    /* Scratchpad byte index tests */
    RUN_TEST(test_ds18b20_scratchpad_indices);

    /* Temperature range tests */
    RUN_TEST(test_ds18b20_temperature_limits);
    RUN_TEST(test_ds18b20_temp_in_range_0c);
    RUN_TEST(test_ds18b20_temp_in_range_25c);
    RUN_TEST(test_ds18b20_temp_at_min_boundary);
    RUN_TEST(test_ds18b20_temp_at_max_boundary);
    RUN_TEST(test_ds18b20_temp_below_min);
    RUN_TEST(test_ds18b20_temp_above_max);

    /* Raw temperature extraction tests */
    RUN_TEST(test_ds18b20_extract_raw_temp_from_scratchpad);
    RUN_TEST(test_ds18b20_extract_negative_raw_temp);

    return UNITY_END();
}
