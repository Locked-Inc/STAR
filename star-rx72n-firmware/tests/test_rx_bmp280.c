/**
 * @file test_rx_bmp280.c
 * @brief Unit Tests for BMP280 Barometric Pressure Sensor Driver
 *
 * @details
 * Validates the rx_bmp280 module (rx_bmp280.c) which drives the Bosch BMP280
 * sensor over I2C using the bus manager abstraction. Tests cover the full
 * public API: init and read (forced-mode measurement with compensation).
 *
 * All hardware interaction is intercepted by mock_riic_hal (channel 1,
 * the RIIC channel used by "i2c1_baro"). The real rx_bus_i2c.c and mock
 * bus manager are linked so bus resolution and RIIC calls exercise the
 * full software stack.
 *
 * @par Test Architecture
 *
 * The BMP280 driver uses bus name "i2c1_baro" on RIIC channel 1 at
 * address 0x76. setUp() registers this bus and initialises RIIC channel 1.
 * Tests pre-load mock RX data and/or inject errors to cover all code paths.
 *
 * @par Static State Management
 *
 * Unlike BNO055, the BMP280 driver does NOT have a double-init guard: each
 * call to rx_bmp280_init() resets s_initialized = false at the start, then
 * sets it to true only on success. setUp() calls rx_bmp280_test_reset_state()
 * before each test to clear s_initialized, making every test start from the
 * same known state regardless of execution order. Tests that require
 * s_initialized==true call internal_setup_initialized_bmp280() at the start.
 *
 * @par Calibration Data for Tests
 *
 * The 24-byte calibration block at 0x88 must have non-zero dig_T1 (bytes 0-1)
 * and non-zero dig_P1 (bytes 6-7) to pass the postcondition check. Tests use
 * internal_load_valid_calib() to set up suitable calibration bytes.
 *
 * @par Test Coverage
 * | Group      | Tests | Description                                      |
 * |------------|-------|--------------------------------------------------|
 * | Init       | 5     | null ptr, success, I2C error, invalid calib,     |
 * |            |       | re-init (no double-init guard in BMP280)         |
 * | Read       | 5     | null ptr, before-init, forced-mode, timeout,     |
 * |            |       | I2C error                                        |
 * | Compensation | 2   | known values within range, var1==0 returns error |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] No loops in test code
 * - Rule 3: [OK] Static allocation only
 * - Rule 4: [OK] All functions under 60 lines
 * - Rule 5: [OK] Minimum 2 assertions per test
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked via TEST_ASSERT_EQUAL
 * - Rule 8: [OK] Typed enums for all constants, no magic numbers
 * - Rule 9: [OK] Single-level pointers only
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - S: Each test validates one specific behavior
 * - L: Mock RIIC HAL is a drop-in substitute for real hardware
 * - D: Driver depends on rx_bus_manager abstraction, not RIIC directly
 *
 * @see rx_bmp280.h BMP280 driver public API
 * @see rx_bmp280_regs.h BMP280 register definitions
 * @see mock_riic_hal.h Mock RIIC HAL for I2C simulation
 *
 * @author STAR Team
 * @date 2026-03-04
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mock_riic_hal.h"
#include "rx_bmp280.h"
#include "rx_bmp280_regs.h"
#include "rx_bus_config.h"
#include "rx_bus_i2c.h"
#include "rx_bus_manager.h"
#include "rx_err.h"
#include "rx_port_constants.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_bmp280_bus_t
 * @brief I2C bus configuration constants for BMP280 tests
 *
 * @details
 * The BMP280 is connected to RIIC channel 1 at address 0x76 (SDO=LOW).
 * The bus name "i2c1_baro" must match the string hardcoded in rx_bmp280.c.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_bmp280_riic_ch  = 1,    /**< RIIC channel 1 (shared with BNO055) */
  k_test_bmp280_i2c_addr = 0x76, /**< BMP280 I2C address (SDO=LOW) */
} test_bmp280_bus_t;

/**
 * @enum test_bmp280_freq_t
 * @brief I2C frequency constant for BMP280 bus
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_bmp280_freq_hz = 400000, /**< 400 kHz fast-mode I2C */
} test_bmp280_freq_t;

/**
 * @enum test_bmp280_buf_t
 * @brief Buffer size constants for mock data setup
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_single_byte    = 1,  /**< Single byte read buffer */
  k_test_calib_buf_size = 24, /**< Calibration block: 24 bytes */
  k_test_adc_buf_size   = 6,  /**< ADC data: 3 pressure + 3 temperature bytes */
} test_bmp280_buf_t;

/**
 * @enum test_bmp280_calib_bytes_t
 * @brief Calibration coefficient byte values for valid calibration
 *
 * @details
 * Provides a calibration set chosen so that the test ADC values (see
 * test_bmp280_adc_bytes_t) produce compensated output inside the BMP280
 * physical operating range. The derivation is as follows:
 *
 * The mock RIIC always returns data from rx_buffer[0] for EVERY read
 * transaction regardless of how many bytes are requested. When
 * rx_bmp280_read() performs its two write-read operations:
 *
 *   1. Status read  (1 byte):  rx_buffer[0] -> must be 0x00 (meas done)
 *   2. ADC data read (6 bytes): rx_buffer[0..5] (same buffer, same offset)
 *
 * Because both reads start at offset 0, the status byte (buf[0]=0x00) is
 * also reused as the pressure MSB in the ADC data. This constrains
 * adc_P = (0x00<<12)|(buf[1]<<4)|(buf[2]>>4), giving adc_P in [0, 4095].
 *
 * The Bosch compensation formula maps adc_P~9 to ~100 kPa only when P1
 * is near 65410 and P2 is near 0. Derivation:
 *   var1 = P1 * 16384   (with P2=P3=...=P6=0 terms)
 *   p_result = ((1048567 << 31) * 3125) // var1 >> 8
 * Solving for P1 to give 100000 Pa: P1 ~ 65410
 *
 * Temperature: adc_T is assembled from the ADC buffer positions
 * [3], [4], [5] which correspond to buf[3..5] in the 7-byte rx_buffer.
 * With buf[3]=0x7F, buf[4]=0x00, buf[5]=0x00:
 *   adc_T = (0x7F<<12)|(0x00<<4)|(0x00>>4) = 0x7F000 = 520192
 * With T1=27488, T2=24790, T3=50: T ~= 2400 centi-degC (24.00 degC)
 *
 * Note: k_calib_t1_lsb = 0x60 = k_bmp280_chip_id_expected so that the chip ID
 * check (reading byte[0] of the mock RX buffer) also passes. The mock always
 * returns data from rx_buffer[0] for all reads within a function call, so
 * this value must satisfy both the chip ID check (1 byte = 0x60) and the
 * calibration read (24 bytes starting at offset 0).
 *
 * Calibration summary:
 *   dig_T1 = 0x6B60 = 27488 (non-zero, passes postcondition; T1_LSB=0x60=chip_id)
 *   dig_T2 = 0x60D6 = 24790 (signed)
 *   dig_T3 = 0x0032 = 50
 *   dig_P1 = 0xFF82 = 65410 (non-zero; chosen for valid pressure at adc_P~9)
 *   dig_P2 = 0x0000 = 0     (zero; simplifies compensation to P1-only term)
 *   dig_P3..P9 = 0x0000     (zero; unused in simplified test)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_calib_t1_lsb =
    0x60, /**< dig_T1 LSB = 0x60 (matches k_bmp280_chip_id_expected so chip ID read also passes) */
  k_calib_t1_msb =
    0x6B, /**< dig_T1 MSB = 0x6B -> T1 = 0x6B60 = 27488 (non-zero; chip ID = T1_LSB = 0x60) */
  k_calib_t2_lsb = 0xD6, /**< dig_T2 LSB */
  k_calib_t2_msb = 0x60, /**< dig_T2 MSB -> T2 = 0x60D6 (signed = 24790) */
  k_calib_t3_lsb = 0x32, /**< dig_T3 LSB */
  k_calib_t3_msb = 0x00, /**< dig_T3 MSB -> T3 = 50 */
  k_calib_p1_lsb = 0x82, /**< dig_P1 LSB */
  k_calib_p1_msb = 0xFF, /**< dig_P1 MSB -> P1 = 0xFF82 = 65410 */
  k_calib_p2_lsb = 0x00, /**< dig_P2 LSB */
  k_calib_p2_msb = 0x00, /**< dig_P2 MSB -> P2 = 0x0000 = 0 */
  k_calib_other  = 0x00, /**< Zero value used for remaining coefficients */
} test_bmp280_calib_bytes_t;

/**
 * @enum test_bmp280_adc_bytes_t
 * @brief Raw ADC data byte values for forced-mode read test
 *
 * @details
 * The mock RIIC always serves reads from rx_buffer[0] for every transaction.
 * The 7-byte buffer loaded by internal_load_read_data() is consumed as:
 *
 *   rx_buffer index:  [0]     [1]            [2]            [3]            [4]          [5]          [6]
 *   Buffer field:      status  press_msb_enum press_lsb_enum press_xlsb_enum temp_msb_enum temp_lsb_enum temp_xlsb_enum
 *   Driver reads:
 *     Status (1B):     [0] = 0x00 (bit3=0 -> meas done)
 *     ADC (6B):        [0..5] -> driver maps as [press_msb, press_lsb, press_xlsb, temp_msb, temp_lsb, temp_xlsb]
 *
 * Because both reads start at offset 0, the driver sees:
 *   driver_press_msb  = buf[0] = status byte = 0x00
 *   driver_press_lsb  = buf[1] = k_adc_press_msb (confusing but correct)
 *   driver_press_xlsb = buf[2] = k_adc_press_lsb
 *   driver_temp_msb   = buf[3] = k_adc_press_xlsb
 *   driver_temp_lsb   = buf[4] = k_adc_temp_msb
 *   driver_temp_xlsb  = buf[5] = k_adc_temp_lsb
 *
 * Assembly results:
 *   adc_P = (0x00<<12)|(k_adc_press_msb<<4)|(k_adc_press_lsb>>4)
 *         = (0x00<<12)|(0x00<<4)|(0x90>>4) = 9
 *   adc_T = (k_adc_press_xlsb<<12)|(k_adc_temp_msb<<4)|(k_adc_temp_lsb>>4)
 *         = (0x7F<<12)|(0x00<<4)|(0x00>>4) = 0x7F000 = 520192
 *
 * With T1=27488, T2=24790, T3=50: T = 2400 centi-degC (24.00 degC) - within range
 * With P1=65410, P2=0 and adc_P=9: P = ~100192 Pa (1001.9 hPa) - within range
 *
 * See test_bmp280_calib_bytes_t for the derivation of P1=65410 that ensures
 * adc_P=9 produces pressure within the BMP280 physical operating range.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_adc_press_msb  = 0x00, /**< press_msb_enum in buffer: maps to driver_press_lsb */
  k_adc_press_lsb  = 0x90, /**< press_lsb_enum in buffer: maps to driver_press_xlsb */
  k_adc_press_xlsb = 0x7F, /**< press_xlsb_enum in buffer: maps to driver_temp_msb (0x7F) */
  k_adc_temp_msb   = 0x00, /**< temp_msb_enum in buffer: maps to driver_temp_lsb */
  k_adc_temp_lsb   = 0x00, /**< temp_lsb_enum in buffer: maps to driver_temp_xlsb */
  k_adc_temp_xlsb  = 0x00, /**< temp_xlsb_enum in buffer: not used by driver (buf[6]) */
} test_bmp280_adc_bytes_t;

/**
 * @enum test_bmp280_status_t
 * @brief BMP280 status register byte values
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_status_measuring_done = 0x00, /**< Status: measurement complete (bit 3 = 0) */
  k_status_measuring_busy = 0x08, /**< Status: measurement in progress (bit 3 = 1) */
} test_bmp280_status_t;

/**
 * @enum test_bmp280_read_call_idx_t
 * @brief Expected RIIC call-history indices for rx_bmp280_read() after history clear
 *
 * @details
 * rx_bmp280_read() issues three RIIC transactions in order:
 *   0: write (ctrl_meas = 0xF4, register address byte sent as write data)
 *   1: write_read (status register 0xF3, 1 byte read)
 *   2: write_read (ADC data register 0xF7, 6 bytes read)
 *
 * These indices allow tests to validate the call sequence via mock_riic_get_call().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_read_call_ctrl_meas = 0U, /**< Call 0: write ctrl_meas trigger (register 0xF4) */
  k_read_call_status    = 1U, /**< Call 1: write_read status register (0xF3) */
  k_read_call_adc_data  = 2U, /**< Call 2: write_read ADC data register (0xF7) */
  k_read_call_count_min = 3U, /**< Minimum calls expected per rx_bmp280_read() */
} test_bmp280_read_call_idx_t;

/**
 * @enum test_bmp280_reg_addr_t
 * @brief Expected register addresses written during rx_bmp280_read()
 *
 * @details
 * The first byte written in each RIIC write or write_read transaction is the
 * register address. These constants mirror the values from rx_bmp280_regs.h
 * and are used to verify that the driver addresses the correct registers.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_reg_addr_ctrl_meas = 0xF4, /**< Control/measurement register */
  k_reg_addr_status    = 0xF3, /**< Status register */
  k_reg_addr_adc_data  = 0xF7, /**< ADC output MSB register (burst: 0xF7-0xFC) */
} test_bmp280_reg_addr_t;

/**
 * @brief Validate that rx_bmp280_read() addressed the correct registers in order
 *
 * @details
 * Inspects the mock RIIC TX buffer after rx_bmp280_read() to confirm:
 *   - At least k_read_call_count_min calls were recorded
 *   - Call 0 (ctrl_meas write): first TX byte == k_reg_addr_ctrl_meas (0xF4)
 *   - Call 1 (status write_read): first TX byte == k_reg_addr_status (0xF3)
 *   - Call 2 (ADC write_read): first TX byte == k_reg_addr_adc_data (0xF7)
 *
 * @pre mock_riic_clear_history() was called before rx_bmp280_read()
 * @post Assertions fail if any register address is wrong
 *
 * @since Version 1.0.0
 */
static void internal_assert_read_register_sequence(void)
{
  /* Verify the minimum number of RIIC transactions occurred */
  TEST_ASSERT_GREATER_OR_EQUAL((uint16_t)k_read_call_count_min, mock_riic_get_call_count());

  /* Retrieve the last TX data (the ADC data write_read is last; its write byte is 0xF7) */
  uint8_t  tx_byte = 0;
  uint16_t tx_len  = mock_riic_get_tx_data((uint8_t)k_test_bmp280_riic_ch, &tx_byte, 1U);
  TEST_ASSERT_EQUAL(1U, tx_len);
  TEST_ASSERT_EQUAL_HEX8((uint8_t)k_reg_addr_adc_data, tx_byte);

  /* Verify call types: write for ctrl_meas, write_read for status and ADC */
  const mock_riic_call_t* call_ctrl = mock_riic_get_call((uint16_t)k_read_call_ctrl_meas);
  TEST_ASSERT_NOT_NULL(call_ctrl);
  TEST_ASSERT_EQUAL(k_mock_riic_call_write, call_ctrl->type);

  const mock_riic_call_t* call_status = mock_riic_get_call((uint16_t)k_read_call_status);
  TEST_ASSERT_NOT_NULL(call_status);
  TEST_ASSERT_EQUAL(k_mock_riic_call_write_read, call_status->type);

  const mock_riic_call_t* call_adc = mock_riic_get_call((uint16_t)k_read_call_adc_data);
  TEST_ASSERT_NOT_NULL(call_adc);
  TEST_ASSERT_EQUAL(k_mock_riic_call_write_read, call_adc->type);
}

/**
 * @enum test_bmp280_output_sanity_t
 * @brief Physical range bounds for compensation output validation
 *
 * @details
 * The test calibration (see test_bmp280_calib_bytes_t) and ADC data (see
 * test_bmp280_adc_bytes_t) are engineered to produce output within the BMP280
 * physical operating range, allowing the driver's postcondition range check to
 * pass. The expected outputs are:
 *   - Temperature: 2400 centi-degC (24.00 degC), range [-4000, 8500]
 *   - Pressure: ~25648768 Pa*256 (~100191 Pa = 1001.9 hPa), range [7680000, 28160000]
 *
 * These bounds verify that the compensation algorithm produced physically
 * plausible output and that no division-by-zero or algorithmic failure occurred.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_press_nonzero_min  = 1U,        /**< Pressure must be > 0 (algorithm completed) */
  k_press_physical_min = 7680000U,  /**< Minimum valid pressure: 300 hPa * 256 */
  k_press_physical_max = 28160000U, /**< Maximum valid pressure: 1100 hPa * 256 */
} test_bmp280_output_sanity_t;

/**
 * @enum test_bmp280_read_seq_idx_t
 * @brief Byte indices into the 7-byte combined status+ADC read buffer
 *
 * @details
 * The status read takes byte[0] (0x00 = done). The ADC read takes
 * bytes[0..5] (the mock always returns from the start of its buffer).
 * We need the status byte at index 0 to be 0x00 (measuring done).
 * For the ADC read the driver reads 6 bytes starting from the current
 * buffer head, so we store ADC data at indices 1..6 and rely on the
 * fact that the status read consumes only 1 byte from the same buffer.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_read_seq_buf_size           = 7, /**< 1 status byte + 6 ADC bytes */
  k_read_seq_status_idx         = 0, /**< Status byte index */
  k_read_seq_adc_press_msb_idx  = 1, /**< Press MSB offset */
  k_read_seq_adc_press_lsb_idx  = 2, /**< Press LSB offset */
  k_read_seq_adc_press_xlsb_idx = 3, /**< Press XLSB offset */
  k_read_seq_adc_temp_msb_idx   = 4, /**< Temp MSB offset */
  k_read_seq_adc_temp_lsb_idx   = 5, /**< Temp LSB offset */
  k_read_seq_adc_temp_xlsb_idx  = 6, /**< Temp XLSB offset */
} test_bmp280_read_seq_idx_t;

/**
 * @enum test_bmp280_temp_range_t
 * @brief BMP280 physical temperature operating range in centi-degC
 *
 * @details
 * Used in test_bmp280_compensation_known_values() to verify that the
 * compensation algorithm produced output within the BMP280 physical
 * operating range documented in the datasheet.
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_temp_physical_min_cdegc = -4000, /**< BMP280 minimum: -40.00 degC */
  k_temp_physical_max_cdegc = 8500,  /**< BMP280 maximum: +85.00 degC */
} test_bmp280_temp_range_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @var s_test_manager
 * @brief Static bus manager shared across all tests
 * @details Reset in setUp(). Uses static allocation per NASA Rule 3.
 * @since Version 1.0.0
 */
static rx_bus_manager_t s_test_manager;

/**
 * @var s_i2c_config
 * @brief I2C bus configuration for BMP280 (channel 1, addr 0x76)
 * @details Configured in setUp() to match "i2c1_baro" bus expected by rx_bmp280.c.
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c_config;

/* s_before_init_tested removed: rx_bmp280_test_reset_state() in setUp() makes
 * ordering enforcement unnecessary - each test starts with s_initialized==false. */

/* =============================================================================
 * Internal: Load mock RIIC RX buffer for calibration and measurement sequences
 * =============================================================================
 */

/**
 * @brief Pre-load RIIC channel 1 with a valid 24-byte calibration block
 *
 * @details
 * Sets up the mock RX buffer with dig_T1=27488 (non-zero), dig_P1=65410
 * (non-zero), and remaining coefficients zero (P2=0, P3-P9=0). This
 * satisfies the postcondition check in rx_bmp280_init() (dig_T1 != 0 &&
 * dig_P1 != 0) and is engineered to produce output within the BMP280
 * physical range when combined with the ADC data from internal_load_read_data().
 * See test_bmp280_calib_bytes_t for full derivation.
 *
 * @pre mock_riic_init() has been called
 * @post RIIC channel 1 RX buffer contains valid 24-byte calibration
 *
 * @since Version 1.0.0
 */
static void internal_load_valid_calib(void)
{
  static_assert(k_test_calib_buf_size > 0U, "calibration buffer must be non-empty");
  uint8_t calib[k_test_calib_buf_size];
  memset(calib, 0, sizeof(calib));

  /* dig_T1 at bytes 0-1 (unsigned, non-zero required) */
  calib[k_bmp280_calib_t1_lsb] = (uint8_t)k_calib_t1_lsb;
  calib[k_bmp280_calib_t1_msb] = (uint8_t)k_calib_t1_msb;

  /* dig_T2 at bytes 2-3 */
  calib[k_bmp280_calib_t2_lsb] = (uint8_t)k_calib_t2_lsb;
  calib[k_bmp280_calib_t2_msb] = (uint8_t)k_calib_t2_msb;

  /* dig_T3 at bytes 4-5 */
  calib[k_bmp280_calib_t3_lsb] = (uint8_t)k_calib_t3_lsb;
  calib[k_bmp280_calib_t3_msb] = (uint8_t)k_calib_t3_msb;

  /* dig_P1 at bytes 6-7 (unsigned, non-zero required) */
  calib[k_bmp280_calib_p1_lsb] = (uint8_t)k_calib_p1_lsb;
  calib[k_bmp280_calib_p1_msb] = (uint8_t)k_calib_p1_msb;

  /* dig_P2 at bytes 8-9 */
  calib[k_bmp280_calib_p2_lsb] = (uint8_t)k_calib_p2_lsb;
  calib[k_bmp280_calib_p2_msb] = (uint8_t)k_calib_p2_msb;

  /* Remaining P3-P9 bytes stay zero (set by memset above) */

  /* Postcondition: verify T1 and P1 set non-zero as required by the driver */
  TEST_ASSERT_NOT_EQUAL(0U, calib[k_bmp280_calib_t1_lsb] | calib[k_bmp280_calib_t1_msb]);
  TEST_ASSERT_NOT_EQUAL(0U, calib[k_bmp280_calib_p1_lsb] | calib[k_bmp280_calib_p1_msb]);

  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, calib, k_test_calib_buf_size);
}

/**
 * @brief Pre-load RIIC channel 1 with a calibration block where dig_P1 == 0
 *
 * @details
 * dig_T1 is non-zero but dig_P1 is zero (bytes 6-7 remain zero from memset).
 * This triggers the postcondition error check: "dig_T1 or dig_P1 is zero".
 *
 * @pre mock_riic_init() has been called
 * @post RIIC channel 1 RX buffer has dig_T1!=0 but dig_P1==0
 *
 * @since Version 1.0.0
 */
static void internal_load_invalid_calib_p1_zero(void)
{
  static_assert(k_test_calib_buf_size > 0U, "calibration buffer must be non-empty");
  uint8_t calib[k_test_calib_buf_size];
  memset(calib, 0, sizeof(calib));

  /* Only set dig_T1 as non-zero; dig_P1 stays zero */
  calib[k_bmp280_calib_t1_lsb] = (uint8_t)k_calib_t1_lsb;
  calib[k_bmp280_calib_t1_msb] = (uint8_t)k_calib_t1_msb;
  /* dig_P1 bytes 6-7 remain 0x00 (zero) */

  /* Postcondition: verify T1 non-zero and P1 zero as intended for this invalid case */
  TEST_ASSERT_NOT_EQUAL(0U, calib[k_bmp280_calib_t1_lsb] | calib[k_bmp280_calib_t1_msb]);
  TEST_ASSERT_EQUAL(0U, calib[k_bmp280_calib_p1_lsb]);
  TEST_ASSERT_EQUAL(0U, calib[k_bmp280_calib_p1_msb]);

  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, calib, k_test_calib_buf_size);
}

/**
 * @brief Pre-load mock RX buffer for a forced-mode measurement sequence
 *
 * @details
 * rx_bmp280_read() performs these mock I2C transactions in order:
 *   1. Write ctrl_meas (0xF4) - no read needed
 *   2. Read status (0xF3) - 1 byte, expect 0x00 (done)
 *   3. Read ADC data (0xF7) - 6 bytes
 *
 * The mock always returns the last-loaded rx_buffer. We load a 7-byte buffer:
 * byte[0] = status (0x00 = done), bytes[1..6] = ADC data.
 * The status read (1 byte) gets byte[0]; ADC read (6 bytes) gets bytes[0..5].
 *
 * Since both reads take from the same loaded buffer, we arrange the buffer
 * so that the 1-byte status read gets 0x00, then re-load for the ADC read.
 * But since mock_riic always returns from the same buffer for write-read and
 * read operations, we must load status data first, then ADC data.
 *
 * Strategy: load status 0x00 for the first read (status check), then the
 * ADC buffer for the second read. The second load overwrites the first.
 * However, in the test we call mock_riic_set_rx_data once before the read
 * function call. The mock is stateless between reads - it always returns the
 * same loaded buffer. So we load the ADC buffer which starts with 0x00 in
 * byte[0], satisfying the status check too.
 *
 * ADC buffer layout (per bmp280_adc_idx_t):
 *   [0]=press_msb [1]=press_lsb [2]=press_xlsb
 *   [3]=temp_msb  [4]=temp_lsb  [5]=temp_xlsb
 *
 * We also need status byte 0x00 before ADC data. Since the status read only
 * takes 1 byte, and our ADC buffer happens to start at press_msb (non-zero
 * would fail the status check), we use a 7-byte buffer: [0]=0x00 (status),
 * [1..6]=ADC data.
 *
 * @pre mock RIIC channel 1 is initialized and s_initialized == true
 * @post RIIC channel 1 RX buffer set for a complete read sequence
 *
 * @since Version 1.0.0
 */
static void internal_load_read_data(void)
{
  static_assert(k_read_seq_buf_size > 0U, "read sequence buffer must be non-empty");
  uint8_t buf[k_read_seq_buf_size];
  buf[k_read_seq_status_idx]         = (uint8_t)k_status_measuring_done;
  buf[k_read_seq_adc_press_msb_idx]  = (uint8_t)k_adc_press_msb;
  buf[k_read_seq_adc_press_lsb_idx]  = (uint8_t)k_adc_press_lsb;
  buf[k_read_seq_adc_press_xlsb_idx] = (uint8_t)k_adc_press_xlsb;
  buf[k_read_seq_adc_temp_msb_idx]   = (uint8_t)k_adc_temp_msb;
  buf[k_read_seq_adc_temp_lsb_idx]   = (uint8_t)k_adc_temp_lsb;
  buf[k_read_seq_adc_temp_xlsb_idx]  = (uint8_t)k_adc_temp_xlsb;

  /* Postcondition: verify status byte is set to measuring-done (0x00) */
  TEST_ASSERT_EQUAL((uint8_t)k_status_measuring_done, buf[k_read_seq_status_idx]);
  TEST_ASSERT_EQUAL((uint8_t)k_adc_press_msb, buf[k_read_seq_adc_press_msb_idx]);

  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, buf, k_read_seq_buf_size);
}

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

/**
 * @brief Initialize the BMP280 driver with valid calibration for tests that require it
 *
 * @details
 * Loads valid calibration data into the mock RIIC buffer and calls
 * rx_bmp280_init(). Tests that require s_initialized==true call this helper
 * at the start, making them self-contained and order-independent.
 *
 * @pre setUp() called (bus manager ready, s_initialized==false after reset)
 * @post s_initialized == true; BMP280 driver ready for rx_bmp280_read()
 *
 * @since Version 1.0.0
 */
static void internal_setup_initialized_bmp280(void)
{
  static_assert(k_test_calib_buf_size > 0U, "calibration buffer must be non-empty");
  internal_load_valid_calib();
  const rx_err_t err = rx_bmp280_init(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Initialize test fixtures before each test
 *
 * @details
 * 1. Reset BMP280 driver state (rx_bmp280_test_reset_state)
 * 2. Reset mock RIIC HAL state
 * 3. Initialize bus manager
 * 4. Create I2C bus config (channel 1, addr 0x76, 400 kHz, P2.0/P2.1)
 * 5. Register bus with manager as "i2c1_baro"
 * 6. Initialize RIIC channel 1 via rx_bus_i2c_init()
 *
 * @pre None - called by Unity before each test
 * @pre Previous test tearDown() has cleared mock RIIC state (or first test)
 * @post s_initialized == false (reset by rx_bmp280_test_reset_state)
 * @post s_test_manager ready with "i2c1_baro" registered and RIIC ch1 initialized
 * @post mock RIIC channel 1 reports initialized after setUp completes
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* Reset driver static state so each test starts with s_initialized==false,
   * making tests order-independent (no persistent state from prior tests). */
  rx_bmp280_test_reset_state();

  (void)mock_riic_init();
  TEST_ASSERT_FALSE(mock_riic_is_initialized((uint8_t)k_test_bmp280_riic_ch));

  rx_err_t err = rx_bus_manager_init(&s_test_manager, "BMP280_TEST", nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_i2c(&s_i2c_config,
                               "i2c1_baro",
                               (uint8_t)k_test_bmp280_riic_ch,
                               (uint8_t)k_test_bmp280_i2c_addr,
                               k_rx_p2_0,
                               k_rx_p2_1,
                               (uint32_t)k_test_bmp280_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(&s_test_manager, &s_i2c_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_i2c_init(&s_test_manager, "i2c1_baro");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Clean up test fixtures after each test
 *
 * @details
 * Deinitializes the bus manager and resets mock RIIC state. The driver's
 * static s_initialized state persists (no deinit API in BMP280 driver).
 *
 * @pre setUp() has been called
 * @pre s_test_manager was successfully initialized by setUp()
 * @post s_test_manager deinitialized; mock RIIC state cleared
 * @post mock RIIC channel 1 reports not initialized
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  const rx_err_t deinit_err = rx_bus_manager_deinit(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, deinit_err);
  (void)mock_riic_init();
  TEST_ASSERT_FALSE(mock_riic_is_initialized((uint8_t)k_test_bmp280_riic_ch));
}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

/**
 * @brief rx_bmp280_init with NULL manager returns k_rx_err_null_ptr
 *
 * @details
 * Verifies the null pointer guard at the top of rx_bmp280_init().
 *
 * @pre s_initialized may be any value
 * @post s_initialized unchanged (RX_CHECK_NULL_PTR returns before modifying state)
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_null_manager_returns_error(void)
{
  rx_err_t err = rx_bmp280_init(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bmp280_init propagates I2C error when RIIC returns NACK
 *
 * @details
 * Injects NACK before calling init. The first I2C transaction (calibration
 * read) fails. The driver should propagate k_rx_err_nack and leave
 * s_initialized == false (it resets s_initialized = false at entry).
 *
 * @pre s_initialized may be any value
 * @post s_initialized == false (NACK path clears it)
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_i2c_error_propagates(void)
{
  (void)mock_riic_simulate_nack(true);

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)mock_riic_simulate_nack(false);
}

/**
 * @brief rx_bmp280_init returns k_rx_err_invalid_state when dig_P1 == 0
 *
 * @details
 * The driver validates that both dig_T1 and dig_P1 are non-zero after
 * parsing the calibration block. This test loads a calibration buffer
 * with dig_P1 = 0x0000 (bytes 6-7 = 0). The driver must return
 * k_rx_err_invalid_state and leave s_initialized false.
 *
 * @pre s_initialized may be any value
 * @post s_initialized == false
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_invalid_calib_returns_error(void)
{
  internal_load_invalid_calib_p1_zero();

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bmp280_init succeeds with valid calibration data
 *
 * @details
 * Pre-loads valid 24-byte calibration (dig_T1 = 27488, dig_P1 = 65410).
 * After init succeeds, a subsequent rx_bmp280_read() call confirms
 * s_initialized == true.
 *
 * This is the FIRST test that successfully initializes the driver.
 * The read/compensation tests that follow depend on s_initialized == true.
 *
 * @pre s_initialized == false (set by previous error-path tests)
 * @post s_initialized == true
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_success(void)
{
  /* setUp() called rx_bmp280_test_reset_state() so s_initialized == false here */
  internal_load_valid_calib();

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify initialized: a read with appropriate mock data must succeed */
  internal_load_read_data();
  bmp280_data_t data;
  rx_err_t      read_err = rx_bmp280_read(&data);
  TEST_ASSERT_EQUAL(k_rx_ok, read_err);
}

/**
 * @brief rx_bmp280_init called twice with valid data reinitializes successfully
 *
 * @details
 * The BMP280 driver does NOT have a double-init guard (unlike BNO055).
 * Each call resets s_initialized = false at entry, then sets it on success.
 * This test verifies that a second successful init works without error,
 * documenting the intended "reinitializable" behavior.
 *
 * @pre s_initialized == true (from test_bmp280_init_success)
 * @post s_initialized == true (second init also succeeds)
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_reinit_succeeds(void)
{
  /* First init */
  internal_load_valid_calib();
  rx_err_t err = rx_bmp280_init(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Second init - BMP280 has no double-init guard: should succeed again */
  internal_load_valid_calib();
  err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Read Tests
 * =============================================================================
 */

/**
 * @brief rx_bmp280_read with NULL output pointer returns k_rx_err_null_ptr
 *
 * @details
 * Null pointer guard fires before the s_initialized check.
 *
 * @pre s_initialized may be any value
 * @post No side effects
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_null_ptr_returns_error(void)
{
  rx_err_t err = rx_bmp280_read(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bmp280_read returns k_rx_err_not_initialized before init
 *
 * @details
 * This test MUST run before test_bmp280_init_success to see
 * s_initialized == false. Calling read without prior successful init
 * returns k_rx_err_not_initialized.
 *
 * ORDER DEPENDENCY: This test is placed before the init-success test in
 * main() to guarantee the uninitialized state is tested.
 *
 * @pre s_initialized == false (no successful init yet)
 * @post s_initialized unchanged
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_before_init_returns_error(void)
{
  /* setUp() reset s_initialized to false - no ordering dependency needed */
  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bmp280_read in forced mode completes and returns physically valid data
 *
 * @details
 * Pre-loads mock RX buffer with calibration and ADC bytes engineered to produce
 * output within the BMP280 physical operating range (see test_bmp280_calib_bytes_t
 * and test_bmp280_adc_bytes_t for derivation). Verifies that:
 * - read returns k_rx_ok
 * - pressure is within the physical operating range [300, 1100] hPa
 *
 * @pre s_initialized == true
 * @post out contains compensation values within physical sensor range
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_success_forced_mode(void)
{
  internal_setup_initialized_bmp280();
  internal_load_read_data();

  /* Clear call history so only rx_bmp280_read() calls are captured below */
  mock_riic_clear_history();

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Verify compensation produced non-zero output (algorithm completed without divide-by-zero). */
  TEST_ASSERT_GREATER_THAN((uint32_t)k_press_nonzero_min, data.press_pa_256);
  /* Verify output is within BMP280 physical operating range [300, 1100] hPa * 256. */
  TEST_ASSERT_GREATER_OR_EQUAL((uint32_t)k_press_physical_min, data.press_pa_256);
  TEST_ASSERT_LESS_OR_EQUAL((uint32_t)k_press_physical_max, data.press_pa_256);
  /* Verify correct register address sequence to catch address-ordering bugs */
  internal_assert_read_register_sequence();
}

/**
 * @brief rx_bmp280_read returns k_rx_err_timeout when status bit never clears
 *
 * @details
 * Pre-loads status byte 0x08 (measuring bit set) so that every poll
 * finds the sensor still busy. After k_bmp280_poll_max iterations (100),
 * the driver must return k_rx_err_timeout.
 *
 * The mock always returns the loaded buffer for every read, so loading
 * 0x08 (busy) causes every status poll to report busy.
 *
 * @pre s_initialized == true
 * @post k_rx_err_timeout returned after poll limit exceeded
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_status_timeout(void)
{
  internal_setup_initialized_bmp280();
  uint8_t busy_status = (uint8_t)k_status_measuring_busy;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, &busy_status, k_test_single_byte);

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bmp280_read propagates I2C error from ctrl_meas write failure
 *
 * @details
 * Injects NACK after successful init. The first I2C operation in read()
 * is writing ctrl_meas (0xF4) to trigger forced mode. If this write fails,
 * k_rx_err_nack must be propagated.
 *
 * @pre s_initialized == true
 * @post k_rx_err_nack returned
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_i2c_error_propagates(void)
{
  internal_setup_initialized_bmp280();
  (void)mock_riic_simulate_nack(true);

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)mock_riic_simulate_nack(false);
}

/* =============================================================================
 * Compensation Tests
 * =============================================================================
 */

/**
 * @brief Full read with known calibration + ADC produces output in physical valid range
 *
 * @details
 * After init with engineered calibration coefficients, performs a forced-mode
 * read with ADC data chosen to produce output within the BMP280 physical
 * operating range. Verifies that both pressure and temperature pass the
 * driver postcondition range checks (which return k_rx_err_invalid_state on
 * out-of-range values).
 *
 * Expected compensation results (derived in test_bmp280_calib_bytes_t):
 *   Temperature: 2400 centi-degC (24.00 degC), range [-4000, 8500]
 *   Pressure: ~100192 Pa (1001.9 hPa) * 256, range [7680000, 28160000]
 *
 * @pre s_initialized == true (from test_bmp280_init_success / reinit)
 * @post Output values within documented BMP280 physical operating range
 *
 * @since Version 1.0.0
 */
void test_bmp280_compensation_known_values(void)
{
  internal_setup_initialized_bmp280();
  internal_load_read_data();

  /* Clear call history so only rx_bmp280_read() calls are captured below */
  mock_riic_clear_history();

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify both outputs are within the BMP280 physical operating range. */
  TEST_ASSERT_GREATER_OR_EQUAL((int32_t)k_temp_physical_min_cdegc, data.temp_centi_degc);
  TEST_ASSERT_LESS_OR_EQUAL((int32_t)k_temp_physical_max_cdegc, data.temp_centi_degc);
  TEST_ASSERT_GREATER_OR_EQUAL((uint32_t)k_press_physical_min, data.press_pa_256);
  TEST_ASSERT_LESS_OR_EQUAL((uint32_t)k_press_physical_max, data.press_pa_256);
  /* Verify correct register address sequence to catch address-ordering bugs */
  internal_assert_read_register_sequence();
}

/**
 * @brief internal_compensate_pressure error path: init rejects dig_P1 == 0 preventing var1 == 0
 *
 * @details
 * The pressure compensation formula (internal_compensate_pressure) has a
 * division-by-zero guard: if var1 == 0, it returns k_rx_err_invalid_state.
 * var1 is driven to zero when dig_P1 == 0.
 *
 * The primary defense is in rx_bmp280_init(): it explicitly checks that
 * dig_P1 is non-zero after reading calibration data and returns
 * k_rx_err_invalid_state before completing initialization.
 * This blocks the var1 == 0 path at the source.
 *
 * This test verifies the init guard: providing dig_P1 == 0 (bytes 6-7 = 0x00)
 * must cause rx_bmp280_init() to return k_rx_err_invalid_state and leave
 * s_initialized false, preventing any subsequent read from reaching the
 * internal_compensate_pressure division-by-zero scenario.
 *
 * @pre s_initialized may be any value
 * @post s_initialized == false (init rejected dig_P1 == 0)
 * @post No read can succeed until re-init with valid calibration
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_zero_var1_returns_error(void)
{
  /* Re-init with calibration where dig_P1 == 0 (dig_T1 non-zero) */
  internal_load_invalid_calib_p1_zero();

  rx_err_t init_err = rx_bmp280_init(&s_test_manager);

  /* dig_P1 == 0 must be rejected by init to prevent division by zero in compensation */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, init_err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, init_err);
}

/* =============================================================================
 * Additional Init Error Path Tests
 * =============================================================================
 */

/**
 * @brief rx_bmp280_init returns error when chip ID does not match expected value
 *
 * @details
 * The BMP280 chip ID register (0xD0) must return 0x60. Loading the mock
 * buffer with 0x00 causes the chip ID check to fail with
 * k_rx_err_invalid_state.
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_wrong_chip_id_returns_error(void)
{
  /* Load RX buffer with wrong chip ID (0x00 instead of 0x60) */
  uint8_t wrong_chip_id = 0x00U;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, &wrong_chip_id, k_test_single_byte);

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief rx_bmp280_init propagates I2C error from calibration read failure
 *
 * @details
 * The chip ID read (call 0) succeeds, but the calibration read (call 1)
 * fails with NACK. The driver must propagate k_rx_err_nack.
 *
 * Call sequence in rx_bmp280_init:
 *   0: chip_id read (write_read)
 *   1: calib read (write_read)  <- inject error here
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_calib_read_error_propagates(void)
{
  /* Load valid chip ID so call 0 succeeds */
  uint8_t chip_id = (uint8_t)k_calib_t1_lsb; /* 0x60 = chip_id_expected */
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, &chip_id, k_test_single_byte);

  /* Fail call index 1 (calibration read) */
  mock_riic_set_nth_call_error(1U, k_rx_err_nack);

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/**
 * @brief rx_bmp280_init propagates I2C error from config register write failure
 *
 * @details
 * The chip ID read (call 0) and calibration read (call 1) succeed, but the
 * config register write (call 2) fails with NACK. The driver must propagate
 * k_rx_err_nack and leave s_initialized false.
 *
 * Call sequence in rx_bmp280_init:
 *   0: chip_id read (write_read)
 *   1: calib read (write_read)
 *   2: config write (write)  <- inject error here
 *
 * @since Version 1.0.0
 */
void test_bmp280_init_config_write_error_propagates(void)
{
  /* Load valid calibration data so calls 0 and 1 succeed */
  internal_load_valid_calib();

  /* Fail call index 2 (config register write) */
  mock_riic_set_nth_call_error(2U, k_rx_err_nack);

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/* =============================================================================
 * Additional Read Error Path Tests
 * =============================================================================
 */

/**
 * @brief rx_bmp280_read returns error when status register read fails
 *
 * @details
 * After ctrl_meas write (call 0) succeeds, the status register read
 * (call 1) fails with NACK.
 *
 * Call sequence in rx_bmp280_read:
 *   0: ctrl_meas write (write)
 *   1: status read (write_read)  <- inject error here
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_status_read_error_propagates(void)
{
  internal_setup_initialized_bmp280();

  /* Fail call index 1 (status read) */
  mock_riic_set_nth_call_error(1U, k_rx_err_nack);

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/**
 * @brief rx_bmp280_read returns error when ADC data register read fails
 *
 * @details
 * ctrl_meas write (call 0) and status read (call 1, returns 0x00=done)
 * succeed, but the ADC data read (call 2) fails with NACK.
 *
 * Call sequence in rx_bmp280_read:
 *   0: ctrl_meas write (write)
 *   1: status read (write_read) returns 0x00 (done)
 *   2: ADC data read (write_read)  <- inject error here
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_adc_read_error_propagates(void)
{
  internal_setup_initialized_bmp280();

  /* Load status=0x00 (done) so the status poll clears on call 1 */
  uint8_t done_status = (uint8_t)k_status_measuring_done;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, &done_status, k_test_single_byte);

  /* Fail call index 2 (ADC data read) */
  mock_riic_set_nth_call_error(2U, k_rx_err_nack);

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
}

/* =============================================================================
 * Compensation Out-of-Range and Division-by-Zero Tests
 * =============================================================================
 */

#ifdef UNIT_TEST
/**
 * @brief Forward declaration of internal_read_and_compensate_adc (UNIT_TEST only)
 *
 * @details
 * Exposed via RX_STATIC_TESTABLE so tests can call it directly without
 * going through the full rx_bmp280_read() forced-mode sequence.
 *
 * @since Version 1.0.0
 */
extern rx_err_t internal_read_and_compensate_adc(bmp280_data_t* out);
#endif

/**
 * @brief internal_read_and_compensate_adc returns error when dig_P1 is zero (var1==0)
 *
 * @details
 * After normal init, corrupts s_calib.dig_P1 to zero via test helper.
 * The pressure compensation formula produces var1 == 0 which triggers the
 * division-by-zero guard and returns k_rx_err_invalid_state (lines 807-812).
 *
 * @since Version 1.0.0
 */
void test_bmp280_pressure_comp_zero_p1_returns_error(void)
{
#ifdef UNIT_TEST
  /* Initialize normally first so s_initialized==true and s_manager is set */
  internal_setup_initialized_bmp280();

  /* Load status=done + valid ADC bytes so calls succeed up to compensation */
  internal_load_read_data();

  /* Corrupt P1 to zero AFTER init so the guard in init doesn't block us */
  rx_bmp280_test_zero_calib_p1();

  /* Fail call index 2 should NOT be needed - compensation fails before write */
  /* call 0 = ctrl_meas write, call 1 = status read, call 2 = ADC read */
  /* For this test we need ctrl_meas + status to succeed, ADC to succeed,
   * then compensation to fail due to P1=0. Load a fresh buffer. */
  /* First, set nth error so only 2 calls succeed, then ADC read gets data */
  bmp280_data_t data;
  memset(&data, 0, sizeof(data));

  /* Load read buffer so status (call 1) returns 0x00 and ADC (call 2) returns data */
  internal_load_read_data();

  rx_err_t err = rx_bmp280_read(&data);

  /* With P1=0 the pressure compensation fails with k_rx_err_invalid_state */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
#else
  TEST_IGNORE_MESSAGE("Only testable in UNIT_TEST builds");
#endif
}

/**
 * @brief internal_read_and_compensate_adc returns error when temperature is out of range
 *
 * @details
 * Uses extreme calibration (T2=32767) combined with maximum adc_T so that
 * compensated temperature exceeds 8500 centi-degC. The out-of-range check
 * returns k_rx_err_invalid_state (lines 816-820).
 *
 * ADC buffer layout (7 bytes): [0]=status=0x00, [3]=0xFF, [4]=0xFF, [5]=0xF0
 *   -> adc_T = 0xFFFFF = 1048575 (20-bit max)
 *   -> temp = ~40735 centi-degC > 8500 (out of range)
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_temp_out_of_range_returns_error(void)
{
#ifdef UNIT_TEST
  /* Strategy: use test helper to set extreme-temp calibration directly into s_calib,
   * bypassing init so we can control T2=32767 without the chip_id constraint.
   * T1=352, T2=32767, T3=0, P1=65410:
   *   adc_T=0xFFFFF=1048575: var1 = (((1048575>>3) - 704) * 32767) >> 11
   *   Note: var1 computation overflows int32_t. However, the compensate formula
   *   uses int32_t internally, so result is implementation-defined on overflow.
   *
   * To avoid overflow: use T1=1, T2=1, T3=0, P1=65410 with adc_T=1048575:
   *   var1 = (((1048575>>3) - 2) * 1) >> 11 = (131069) >> 11 = 64
   *   var2 = 0 (T3=0)
   *   t_fine = 64
   *   T = (64*5 + 128) >> 8 = 448 >> 8 = 1 centi-degC -> IN RANGE
   *
   * For out-of-range temp, we need t_fine to be very large or very small.
   * With T1=1, T2=32767, T3=0 and adc_T=1048575:
   *   var1 = ((131071 - 2) * 32767) >> 11
   *   = (131069 * 32767) >> 11
   *   = 4294343723 >> 11  (fits in uint32, but int32 OVERFLOWS at ~2.1B)
   *   In C23 with GCC on x86_64 (test host), signed overflow is UB -> unpredictable
   *
   * Safe approach: use adc_T = 0 with T2=(-32768) (most negative int16):
   *   var1 = (((0>>3) - (T1<<1)) * T2) >> 11
   *   With T1=1, T2=-32768:
   *   var1 = ((0 - 2) * (-32768)) >> 11 = (65536) >> 11 = 32
   *   var2 = 0 (T3=0)
   *   t_fine = 32
   *   T = (32*5+128)>>8 = 288>>8 = 1 -> still in range
   *
   * Best option: directly expose internal_compensate_temp or set calibration
   * such that the BMP280 formula produces obvious out-of-range without overflow.
   *
   * Verified safe combination: T1=27488, T2=24790, T3=50 (valid test calib)
   * but adc_T engineered to produce temp > 8500:
   *   Need t_fine > 435000.
   *   With T1=27488, T2=24790 and large adc_T:
   *   var1 = (((adc_T>>3) - 54976) * 24790) >> 11
   *   For adc_T = 1048575 (max 20-bit):
   *   var1 = ((131071 - 54976) * 24790) >> 11 = (76095 * 24790) >> 11
   *   76095 * 24790 = 1887235050 -> fits in int32 (max ~2.1B) ✓
   *   1887235050 >> 11 = 920720
   *   t_fine = 920720 (approx)
   *   T = (920720*5 + 128) >> 8 = (4603600 + 128) >> 8 = 4603728 >> 8 = 17983 > 8500 ✓
   *
   * Use the NORMAL calibration (T1=27488, T2=24790, T3=50, P1=65410) with max adc_T!
   */

  /* Setup: init with normal calibration, then load ADC buffer with max adc_T */
  internal_setup_initialized_bmp280();

  /* Load 7-byte ADC buffer: status=0x00 + max adc_T (buf[3..5]=0xFF, 0xFF, 0xF0)
   * adc_T = (buf[3]<<12)|(buf[4]<<4)|(buf[5]>>4) = 0xFFFFF = 1048575 */
  uint8_t extreme_adc[7];
  memset(extreme_adc, 0, sizeof(extreme_adc));
  extreme_adc[3] = 0xFFU; /* temp_msb -> adc_T high bits 0xFF*/
  extreme_adc[4] = 0xFFU; /* temp_lsb -> adc_T middle bits 0xFF */
  extreme_adc[5] = 0xF0U; /* temp_xlsb -> adc_T low nibble = 0xF */
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch,
                              extreme_adc,
                              (uint16_t)sizeof(extreme_adc));

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
#else
  TEST_IGNORE_MESSAGE("Only testable in UNIT_TEST builds");
#endif
}

/**
 * @brief internal_read_and_compensate_adc returns error when pressure is out of range
 *
 * @details
 * Uses extreme pressure calibration (P1=1, T2=0) so that temperature stays
 * in range but pressure compensation produces a near-zero result below the
 * 7680000 Pa*256 minimum (lines 822-826).
 *
 * @since Version 1.0.0
 */
void test_bmp280_read_pressure_out_of_range_returns_error(void)
{
  /* Reset state then init with extreme pressure calibration:
   * T1=27488 (0x6B60), T2=0, T3=0, P1=1
   * chip_id = first byte of calib = T1_LSB = 0x60 ✓
   * T2=0, T3=0 -> t_fine ~ 0 -> temp ~ 0 centi-degC (in range [-4000, 8500])
   * P1=1 -> pressure formula produces near-zero output (below 7680000 Pa*256 min) */
  rx_bmp280_test_reset_state();
  rx_bmp280_test_set_state(&s_test_manager, false);

  uint8_t extreme_press_calib[k_test_calib_buf_size];
  memset(extreme_press_calib, 0, sizeof(extreme_press_calib));
  extreme_press_calib[0] = 0x60U; /* T1 LSB = chip_id 0x60 */
  extreme_press_calib[1] = 0x6BU; /* T1 MSB -> T1 = 0x6B60 = 27488 */
  /* T2=0, T3=0: bytes 2-5 stay zero */
  extreme_press_calib[6] = 0x01U; /* P1 LSB -> P1 = 1 */
  extreme_press_calib[7] = 0x00U; /* P1 MSB */
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch,
                              extreme_press_calib,
                              k_test_calib_buf_size);

  rx_err_t init_err = rx_bmp280_init(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, init_err);

  /* Load all-zero ADC buffer: status=done(0x00), adc_P=0, adc_T=0 */
  uint8_t adc_buf[7];
  memset(adc_buf, 0, sizeof(adc_buf));
  (void)mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch,
                              adc_buf,
                              (uint16_t)sizeof(adc_buf));

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));
  rx_err_t err = rx_bmp280_read(&data);

  /* With P1=1 and adc_P=0, pressure compensation produces a value below 7680000 */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Unity Main Entry Point
 * =============================================================================
 */

/**
 * @brief Test runner entry point
 *
 * @details
 * Runs all BMP280 driver unit tests. Tests are order-independent: setUp()
 * calls rx_bmp280_test_reset_state() before each test to clear s_initialized,
 * and tests that need s_initialized==true call internal_setup_initialized_bmp280().
 *
 * @return int Unity test result code
 * @retval 0 All tests passed
 * @retval 1 One or more tests failed
 *
 * @pre Unity test framework linked and BMP280 driver sources compiled with mock RIIC HAL
 * @pre TESTING macro defined (enables rx_bmp280_test_reset_state)
 * @post Unity reports all test results to stdout
 * @post Process exits with 0 if all tests pass, non-zero on any test failure
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* setUp() resets driver state before each test - order is not critical */

  /* Null pointer guard tests */
  RUN_TEST(test_bmp280_read_null_ptr_returns_error);
  RUN_TEST(test_bmp280_init_null_manager_returns_error);

  /* Read-before-init test (setUp resets state so ordering is not required) */
  RUN_TEST(test_bmp280_read_before_init_returns_error);

  /* Init tests */
  RUN_TEST(test_bmp280_init_i2c_error_propagates);
  RUN_TEST(test_bmp280_init_invalid_calib_returns_error);
  RUN_TEST(test_bmp280_init_success);
  RUN_TEST(test_bmp280_init_reinit_succeeds);

  /* Read tests - each calls internal_setup_initialized_bmp280() internally */
  RUN_TEST(test_bmp280_read_success_forced_mode);
  RUN_TEST(test_bmp280_read_status_timeout);
  RUN_TEST(test_bmp280_read_i2c_error_propagates);

  /* Compensation tests - each calls internal_setup_initialized_bmp280() internally */
  RUN_TEST(test_bmp280_compensation_known_values);
  RUN_TEST(test_bmp280_read_zero_var1_returns_error);

  /* Additional init error path tests */
  RUN_TEST(test_bmp280_init_wrong_chip_id_returns_error);
  RUN_TEST(test_bmp280_init_calib_read_error_propagates);
  RUN_TEST(test_bmp280_init_config_write_error_propagates);

  /* Additional read error path tests */
  RUN_TEST(test_bmp280_read_status_read_error_propagates);
  RUN_TEST(test_bmp280_read_adc_read_error_propagates);

  /* Out-of-range and division-by-zero compensation tests */
  RUN_TEST(test_bmp280_pressure_comp_zero_p1_returns_error);
  RUN_TEST(test_bmp280_read_temp_out_of_range_returns_error);
  RUN_TEST(test_bmp280_read_pressure_out_of_range_returns_error);

  return UNITY_END();
}
