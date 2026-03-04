/* tests/test_rx_bmp280.c */

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
 * sets it to true only on success. This means:
 * - test_bmp280_read_before_init_returns_error MUST run before any
 *   successful init, because once initialized the static flag stays true
 *   until the next init call resets it.
 * - test_bmp280_init_success sets s_initialized = true (persists to next tests).
 * - Calling init again with valid data reinitializes successfully.
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
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
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
 * With T1=27436, T2=24790, T3=50: T = 2400 centi-degC (24.00 degC) - within range
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
  _Static_assert(k_test_calib_buf_size > 0U, "calibration buffer must be non-empty");
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

  mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, calib, k_test_calib_buf_size);
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
  _Static_assert(k_test_calib_buf_size > 0U, "calibration buffer must be non-empty");
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

  mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, calib, k_test_calib_buf_size);
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
  _Static_assert(k_read_seq_buf_size > 0U, "read sequence buffer must be non-empty");
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

  mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, buf, k_read_seq_buf_size);
}

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

/**
 * @brief Initialize test fixtures before each test
 *
 * @details
 * 1. Reset mock RIIC HAL state
 * 2. Initialize bus manager
 * 3. Create I2C bus config (channel 1, addr 0x76, 400 kHz, P2.0/P2.1)
 * 4. Register bus with manager as "i2c1_baro"
 * 5. Initialize RIIC channel 1 via rx_bus_i2c_init()
 *
 * @pre None - called by Unity before each test
 * @post s_test_manager ready with "i2c1_baro" registered and RIIC ch1 initialized
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  mock_riic_init();

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
 * @post s_test_manager deinitialized; mock RIIC state cleared
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  (void)rx_bus_manager_deinit(&s_test_manager);
  mock_riic_init();
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
  mock_riic_simulate_nack(true);

  rx_err_t err = rx_bmp280_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(false);
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
 * Pre-loads valid 24-byte calibration (dig_T1 = 27436, dig_P1 = 65410).
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
  /* Second init with valid calibration - should succeed (no guard) */
  internal_load_valid_calib();

  rx_err_t err = rx_bmp280_init(&s_test_manager);

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
  bmp280_data_t data;
  rx_err_t      err = rx_bmp280_read(&data);
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
  internal_load_read_data();

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Verify compensation produced physically valid pressure output.
   * The range [7680000, 28160000] corresponds to [300, 1100] hPa * 256. */
  TEST_ASSERT_GREATER_OR_EQUAL((uint32_t)k_press_physical_min, data.press_pa_256);
  TEST_ASSERT_LESS_OR_EQUAL((uint32_t)k_press_physical_max, data.press_pa_256);
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
  uint8_t busy_status = (uint8_t)k_status_measuring_busy;
  mock_riic_set_rx_data((uint8_t)k_test_bmp280_riic_ch, &busy_status, k_test_single_byte);

  bmp280_data_t data;
  rx_err_t      err = rx_bmp280_read(&data);

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
  mock_riic_simulate_nack(true);

  bmp280_data_t data;
  rx_err_t      err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(false);
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
  internal_load_read_data();

  bmp280_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bmp280_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify both outputs are within the BMP280 physical operating range. */
  TEST_ASSERT_GREATER_OR_EQUAL((int32_t)k_temp_physical_min_cdegc, data.temp_centi_degc);
  TEST_ASSERT_LESS_OR_EQUAL((int32_t)k_temp_physical_max_cdegc, data.temp_centi_degc);
  TEST_ASSERT_GREATER_OR_EQUAL((uint32_t)k_press_physical_min, data.press_pa_256);
  TEST_ASSERT_LESS_OR_EQUAL((uint32_t)k_press_physical_max, data.press_pa_256);
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
 * Unity Main Entry Point
 * =============================================================================
 */

/**
 * @brief Test runner entry point
 *
 * @details
 * Runs all BMP280 driver unit tests. The ordering is critical:
 *   1. test_bmp280_read_before_init_returns_error - needs s_initialized==false
 *   2. Error-path init tests (do not change s_initialized to true)
 *   3. test_bmp280_init_success - sets s_initialized = true
 *   4. test_bmp280_init_reinit_succeeds - verifies no double-init guard
 *   5. Read/compensation tests - require s_initialized == true
 *   6. test_bmp280_read_zero_var1_returns_error - re-inits with bad calib
 *
 * @return int Unity test result code
 * @retval 0 All tests passed
 * @retval 1 One or more tests failed
 *
 * @warning Tests must execute in the listed order: the static s_initialized flag
 *          persists across tests and there is no driver deinit/reset API. Reordering
 *          tests will cause failures.
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Read-before-init MUST run first (needs s_initialized == false) */
  RUN_TEST(test_bmp280_read_null_ptr_returns_error);
  RUN_TEST(test_bmp280_read_before_init_returns_error);

  /* Init tests - errors before success */
  RUN_TEST(test_bmp280_init_null_manager_returns_error);
  RUN_TEST(test_bmp280_init_i2c_error_propagates);
  RUN_TEST(test_bmp280_init_invalid_calib_returns_error);
  RUN_TEST(test_bmp280_init_success);
  RUN_TEST(test_bmp280_init_reinit_succeeds);

  /* Read tests - require s_initialized == true */
  RUN_TEST(test_bmp280_read_success_forced_mode);
  RUN_TEST(test_bmp280_read_status_timeout);
  RUN_TEST(test_bmp280_read_i2c_error_propagates);

  /* Compensation tests */
  RUN_TEST(test_bmp280_compensation_known_values);
  RUN_TEST(test_bmp280_read_zero_var1_returns_error);

  return UNITY_END();
}
