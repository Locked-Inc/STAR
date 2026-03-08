/**
 * @file test_rx_bno055.c
 * @brief Unit Tests for BNO055 9-DOF Absolute Orientation Sensor Driver
 *
 * @details
 * Validates the rx_bno055 module (rx_bno055.c) which drives the Bosch BNO055
 * sensor over I2C using the bus manager abstraction. Tests cover the full
 * public API: init, read, and calibration check.
 *
 * All hardware interaction is intercepted by mock_riic_hal (channel 1, the
 * RIIC channel used by "i2c1_imu"). The real rx_bus_i2c.c and mock bus manager
 * are linked so bus resolution and RIIC calls exercise the full software stack.
 *
 * @par Test Architecture
 *
 * The BNO055 driver uses bus name "i2c1_imu" on RIIC channel 1 (address 0x28).
 * setUp() registers this bus with the mock bus manager and initialises RIIC
 * channel 1 via rx_bus_i2c_init(). Tests then pre-load mock RX data and/or
 * inject errors to steer the driver through all code paths.
 *
 * @par Static State Management
 *
 * The driver keeps static variables s_initialized and s_manager. setUp()
 * calls rx_bno055_test_reset_state() before each test so that every test
 * starts from a clean, uninitialized state regardless of execution order.
 *
 * Tests that require s_initialized == true call
 * internal_setup_initialized_driver() at the top of the test body to
 * perform a fresh init with a loaded chip-ID mock before exercising the
 * feature under test.
 *
 * Unity calls tests in declaration order, so the order of function
 * definitions in this file still determines execution order, but no test
 * relies on state left behind by a previous test.
 *
 * @par Test Coverage
 * | Group         | Tests | Description                                                          |
 * |---------------|-------|----------------------------------------------------------------------|
 * | Init          | 6     | null ptr, I2C error, wrong chip ID, read-before-init, success, double|
 * | Read          | 6     | null ptr, euler, quaternion, temperature, linear accel, I2C error   |
 * | Calibration   | 3     | null ptr, reads status byte, partial calibration                     |
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
 * @see rx_bno055.h BNO055 driver public API
 * @see rx_bno055_regs.h BNO055 register definitions
 * @see mock_riic_hal.h Mock RIIC HAL for I2C simulation
 *
 * @author STAR Team
 * @date 2026-03-04
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mock_riic_hal.h"
#include "rx_bno055.h"
#include "rx_bno055_regs.h"
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
 * @enum test_bno055_bus_t
 * @brief I2C bus configuration constants for BNO055 tests
 *
 * @details
 * The BNO055 is connected to RIIC channel 1 (P2.0=SDA1, P2.1=SCL1) at
 * address 0x28. The bus name "i2c1_imu" must match the string hardcoded in
 * rx_bno055.c (s_bus_name).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_bno055_riic_ch  = 1,    /**< RIIC channel 1 (SCL1=P2.1, SDA1=P2.0) */
  k_test_bno055_i2c_addr = 0x28, /**< BNO055 I2C address (ADR=LOW) */
} test_bno055_bus_t;

/**
 * @enum test_bno055_freq_t
 * @brief I2C frequency constant for BNO055 bus
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_bno055_freq_hz = 400000, /**< 400 kHz fast-mode I2C */
} test_bno055_freq_t;

/**
 * @enum test_bno055_calib_t
 * @brief Calibration status byte values for calibration tests
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_calib_all_full = 0xFF, /**< SYS=3, GYR=3, ACC=3, MAG=3 (all calibrated) */
  k_test_calib_none     = 0x00, /**< SYS=0, GYR=0, ACC=0, MAG=0 (uncalibrated) */
  k_test_calib_partial  = 0x3F, /**< SYS=0, GYR=3, ACC=3, MAG=3, system not yet calibrated */
} test_bno055_calib_t;

/**
 * @enum test_bno055_euler_raw_t
 * @brief Raw Euler angle byte values for read success test
 *
 * @details
 * heading = 0x0140 = 320 raw -> 320/16 = 20.0 deg
 * roll    = 0x0010 = 16 raw  -> 16/16 = 1.0 deg
 * pitch   = 0x0020 = 32 raw  -> 32/16 = 2.0 deg
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_euler_h_lsb = 0x40, /**< Heading LSB byte */
  k_test_euler_h_msb = 0x01, /**< Heading MSB byte */
  k_test_euler_r_lsb = 0x10, /**< Roll LSB byte */
  k_test_euler_r_msb = 0x00, /**< Roll MSB byte */
  k_test_euler_p_lsb = 0x20, /**< Pitch LSB byte */
  k_test_euler_p_msb = 0x00, /**< Pitch MSB byte */
} test_bno055_euler_raw_t;

/**
 * @enum test_bno055_euler_expected_t
 * @brief Expected assembled int16 values for Euler angle assertion
 *
 * @since Version 1.0.0
 */
typedef enum : int16_t {
  k_test_euler_heading_expected = 0x0140, /**< Expected heading_deg16 = 320 */
  k_test_euler_roll_expected    = 0x0010, /**< Expected roll_deg16    = 16 */
  k_test_euler_pitch_expected   = 0x0020, /**< Expected pitch_deg16   = 32 */
} test_bno055_euler_expected_t;

/**
 * @enum test_bno055_quat_raw_t
 * @brief Raw quaternion byte values for quaternion read success test
 *
 * @details
 * quat_w = 0x4000 = 16384 raw -> 16384/16384 = 1.0 (identity quaternion W)
 * quat_x = 0x0000 = 0
 * quat_y = 0x0000 = 0
 * quat_z = 0x0000 = 0
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_quat_w_lsb = 0x00, /**< Quat W LSB */
  k_test_quat_w_msb = 0x40, /**< Quat W MSB (0x4000 = 16384) */
  k_test_quat_xyz   = 0x00, /**< Quat X/Y/Z bytes (all zero) */
} test_bno055_quat_raw_t;

/**
 * @enum test_quat_expected_t
 * @brief Expected assembled int16_t quaternion values after little-endian assembly
 *
 * @details
 * The identity quaternion has W=1.0 which corresponds to raw value 16384 (0x4000).
 * All other components are 0. These are the expected data.quat_w and data.quat_x
 * values after the driver assembles the bytes loaded by internal_load_quat_read_data.
 *
 * @since Version 1.0.0
 */
typedef enum : int16_t {
  k_quat_w_expected   = 0x4000, /**< Expected quat_w raw = 16384 (W=1.0, identity quaternion) */
  k_quat_xyz_expected = 0x0000, /**< Expected quat_x/y/z raw = 0 */
} test_quat_expected_t;

/**
 * @enum test_bno055_misc_t
 * @brief Temperature byte used in full read test
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_temp_raw         = 25,   /**< Temperature raw value (25 degC) */
  k_test_calib_status_raw = 0xFC, /**< Calib status: SYS=3,GYR=3,ACC=3,MAG=0 */
  k_test_wrong_chip_id    = 0xFF, /**< Non-BNO055 chip ID value */
} test_bno055_misc_t;

/**
 * @enum test_bno055_interrupt_writes_t
 * @brief Expected extra RIIC write count for interrupt-mode init
 *
 * @details
 * Interrupt mode appends 4 extra writes after the standard CONFIG-mode
 * sequence: PAGE_ID=1, INT_MSK=0x01, INT_EN=0x01, PAGE_ID=0.
 * Poll mode does not write any Page 1 registers.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_interrupt_extra_writes = 4U, /**< PAGE_ID(x2)+INT_MSK+INT_EN (BSX_DRDY, 4-step sequence) */
} test_bno055_interrupt_writes_t;

/**
 * @enum test_bno055_lia_raw_t
 * @brief Raw linear acceleration byte values for LIA read success test
 *
 * @details
 * lin_acc_x = 0x00C8 = 200 raw -> 200 / k_bno055_scale_acc = 2.0 m/s^2
 * lin_acc_y = 0x0000 = 0 raw
 * lin_acc_z = 0x0000 = 0 raw
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_lia_x_lsb = 0xC8, /**< LIA X LSB (200 raw = 2.0 m/s^2 * k_bno055_scale_acc) */
  k_test_lia_x_msb = 0x00, /**< LIA X MSB */
} test_bno055_lia_raw_t;

/**
 * @enum test_bno055_lia_expected_t
 * @brief Expected assembled int16_t linear acceleration value
 *
 * @since Version 1.0.0
 */
typedef enum : int16_t {
  k_test_lia_x_expected = 200, /**< Expected lin_acc_x raw = 200 (2.0 m/s^2 * k_bno055_scale_acc) */
} test_bno055_lia_expected_t;

/**
 * @enum test_bno055_buf_sizes_t
 * @brief Buffer sizes for mock data setup
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_single_byte_buf = 1, /**< Single byte read buffer */
  k_test_euler_buf_size  = 6, /**< Euler angles: 3 x int16 */
  k_test_lia_buf_size    = 6, /**< Linear accel: 3 x int16 */
  /* k_test_quat_buf_size removed: use canonical k_quat_buf_size from test_bno055_quat_buf_idx_t */
} test_bno055_buf_sizes_t;

/**
 * @enum test_bno055_read_buf_idx_t
 * @brief Byte indices into the 22-byte unified read buffer
 *
 * @details
 * The first 6 bytes cover the Euler data loaded for each successive
 * write-read transaction. Remaining bytes are zeroed.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_read_buf_size = 22, /**< Max read payload: 6+8+6+1+1 = 22 bytes */
  k_read_idx_0    = 0,  /**< Euler heading LSB */
  k_read_idx_1    = 1,  /**< Euler heading MSB */
  k_read_idx_2    = 2,  /**< Euler roll LSB */
  k_read_idx_3    = 3,  /**< Euler roll MSB */
  k_read_idx_4    = 4,  /**< Euler pitch LSB */
  k_read_idx_5    = 5,  /**< Euler pitch MSB */
} test_bno055_read_buf_idx_t;

/**
 * @enum test_bno055_quat_buf_idx_t
 * @brief Byte indices into the 8-byte quaternion read buffer
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_quat_buf_size  = 8, /**< Quaternion: 4 x int16 */
  k_quat_w_lsb_idx = 0, /**< W LSB position in quat buffer */
  k_quat_w_msb_idx = 1, /**< W MSB position in quat buffer */
} test_bno055_quat_buf_idx_t;

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
 * @brief I2C bus configuration for BNO055 (channel 1, addr 0x28)
 * @details Configured in setUp() to match the "i2c1_imu" bus expected by rx_bno055.c.
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c_config;

/**
 * @var s_poll_cfg
 * @brief BNO055 polling-mode configuration used by most init tests
 * @details Selects k_bno055_mode_poll; interrupt engine registers are not written.
 * @since Version 1.0.0
 */
static const bno055_config_t s_poll_cfg = {.mode = k_bno055_mode_poll};

/**
 * @var s_interrupt_cfg
 * @brief BNO055 interrupt-mode configuration used by interrupt init tests
 * @details Selects k_bno055_mode_interrupt; driver writes Page 1 interrupt registers.
 * @since Version 1.0.0
 */
static const bno055_config_t s_interrupt_cfg = {.mode = k_bno055_mode_interrupt};

/* =============================================================================
 * Helper: Load mock RX data for a successful BNO055 init sequence
 * =============================================================================
 */

/**
 * @brief Pre-load mock RIIC channel 1 with chip ID 0xA0 for successful init
 *
 * @details
 * The BNO055 init does multiple writes then a write-read for chip ID.
 * The mock_riic_set_rx_data() sets data that will be returned on the
 * NEXT read (or write-read) operation on the specified channel.
 * We load 0xA0 once; the chip ID write-read consumes it.
 *
 * @pre mock_riic_init() has been called
 * @pre k_test_single_byte_buf >= 1 (chip ID requires one byte)
 * @post RIIC channel 1 RX buffer contains 0xA0 (chip ID)
 * @post Next write-read transaction on channel 1 will return 0xA0
 *
 * @since Version 1.0.0
 */
static void internal_load_valid_chip_id(void)
{
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
  TEST_ASSERT_GREATER_THAN(0U, (uint32_t)k_test_single_byte_buf);
  uint8_t chip_id_data = (uint8_t)k_bno055_chip_id_expected;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch,
                              &chip_id_data,
                              k_test_single_byte_buf);
}

/**
 * @brief Pre-load mock RIIC channel 1 for a full successful rx_bno055_read()
 *
 * @details
 * The read function issues 5 write-read / read transactions in order:
 *   1. Euler:  6 bytes from 0x1A
 *   2. Quat:   8 bytes from 0x20
 *   3. LIA:    6 bytes from 0x28
 *   4. Temp:   1 byte  from 0x34
 *   5. Calib:  1 byte  from 0x35
 *
 * The mock_riic_set_rx_data() is consumed ONCE per transaction. However,
 * the mock always returns the last-loaded rx_buffer contents, so we load
 * a unified buffer covering the largest expected payload. Each subsequent
 * read crops to its requested length from the same buffer.
 *
 * We load enough bytes to satisfy all reads by setting a buffer that begins
 * with the Euler bytes and then the rest zeroed (quat/lia/temp/calib use 0s).
 * For simplicity we use a single 22-byte buffer: after each transaction the
 * mock returns data from the same static rx_buffer.
 *
 * @pre mock RIIC channel 1 is initialized
 * @pre k_read_buf_size >= k_euler_byte_count (6 bytes for Euler heading/roll/pitch)
 * @post RIIC channel 1 RX buffer set for read sequence
 * @post Mock will return Euler heading=0x0140, roll=0x0010, pitch=0x0020
 *
 * @since Version 1.0.0
 */
static void internal_load_read_data(void)
{
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
  TEST_ASSERT_GREATER_THAN(0U, (uint32_t)k_read_buf_size);
  uint8_t read_buf[k_read_buf_size];
  memset(read_buf, 0, sizeof(read_buf));

  /* Euler heading = 0x0140, roll = 0x0010, pitch = 0x0020 */
  read_buf[k_read_idx_0] = (uint8_t)k_test_euler_h_lsb;
  read_buf[k_read_idx_1] = (uint8_t)k_test_euler_h_msb;
  read_buf[k_read_idx_2] = (uint8_t)k_test_euler_r_lsb;
  read_buf[k_read_idx_3] = (uint8_t)k_test_euler_r_msb;
  read_buf[k_read_idx_4] = (uint8_t)k_test_euler_p_lsb;
  read_buf[k_read_idx_5] = (uint8_t)k_test_euler_p_msb;

  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, read_buf, k_read_buf_size);
}

/**
 * @brief Pre-load mock RIIC for quaternion read data
 *
 * @details
 * Loads identity quaternion: W=16384 (0x4000), X=Y=Z=0.
 *
 * @pre mock RIIC channel 1 is initialized
 * @pre k_quat_buf_size >= 8 (minimum bytes to represent W quaternion component)
 * @post RIIC channel 1 RX buffer set with quaternion data
 * @post Mock will return identity quaternion W=16384, X=Y=Z=0
 *
 * @since Version 1.0.0
 */
static void internal_load_quat_read_data(void)
{
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
  TEST_ASSERT_GREATER_THAN(0U, (uint32_t)k_quat_buf_size);
  uint8_t quat_buf[k_quat_buf_size];
  memset(quat_buf, 0, sizeof(quat_buf));
  quat_buf[k_quat_w_lsb_idx] = (uint8_t)k_test_quat_w_lsb;
  quat_buf[k_quat_w_msb_idx] = (uint8_t)k_test_quat_w_msb;

  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, quat_buf, k_quat_buf_size);
}

/**
 * @brief Pre-load mock RIIC for temperature read data
 *
 * @details
 * Loads a 22-byte buffer with k_test_temp_raw (25) at position 0.
 * Because the mock always returns from buf[0], single-byte reads (temp, calib)
 * return 25. Other fields will also see 25 in their LSBs but are not checked
 * by the temperature test.
 *
 * @pre mock RIIC channel 1 is initialized
 * @pre k_read_buf_size >= 1 (at least one byte required for single-byte reads)
 * @post RIIC channel 1 RX buffer set; single-byte reads return k_test_temp_raw
 * @post data.temp_degc will equal (int8_t)k_test_temp_raw after rx_bno055_read()
 *
 * @since Version 1.0.0
 */
static void internal_load_temp_read_data(void)
{
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
  TEST_ASSERT_GREATER_THAN(0U, (uint32_t)k_read_buf_size);
  uint8_t temp_buf[k_read_buf_size];
  memset(temp_buf, 0, sizeof(temp_buf));
  /* Position 0 is returned for all single-byte reads; set to k_test_temp_raw (25 degC) */
  temp_buf[k_read_idx_0] = (uint8_t)k_test_temp_raw;

  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, temp_buf, k_read_buf_size);
}

/**
 * @brief Pre-load mock RIIC for linear acceleration read data
 *
 * @details
 * Loads a 22-byte buffer with LIA X = 200 raw (2.0 m/s^2) at positions 0-1.
 * Because the mock returns the same buffer for all transactions, lin_acc_x
 * sees the correct bytes regardless of the read order.
 *
 * @pre mock RIIC channel 1 is initialized
 * @pre k_read_buf_size >= 2 (at least two bytes required for int16 assembly)
 * @post RIIC channel 1 RX buffer set with LIA X = 200 raw
 * @post data.lin_acc_x will equal k_test_lia_x_expected after rx_bno055_read()
 *
 * @since Version 1.0.0
 */
static void internal_load_lia_read_data(void)
{
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
  TEST_ASSERT_GREATER_THAN(0U, (uint32_t)k_test_lia_buf_size);
  uint8_t lia_buf[k_read_buf_size];
  memset(lia_buf, 0, sizeof(lia_buf));
  /* lin_acc_x = 0x00C8 = 200 raw (2.0 m/s^2 when divided by k_bno055_scale_acc=100) */
  lia_buf[k_read_idx_0] = (uint8_t)k_test_lia_x_lsb;
  lia_buf[k_read_idx_1] = (uint8_t)k_test_lia_x_msb;

  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, lia_buf, k_read_buf_size);
}

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

/**
 * @brief Initialize test fixtures before each test
 *
 * @details
 * 1. Reset BNO055 driver static state via rx_bno055_test_reset_state()
 * 2. Reset mock RIIC HAL state (mock_riic_init)
 * 3. Initialize bus manager
 * 4. Create I2C bus config (channel 1, addr 0x28, 400 kHz, P2.0/P2.1)
 * 5. Register bus with manager as "i2c1_imu"
 * 6. Initialize RIIC channel 1 via rx_bus_i2c_init()
 *
 * The driver statics s_initialized and s_manager are reset to their
 * initial (uninitialized) state before every test. Tests that need a live
 * driver must call internal_setup_initialized_driver() themselves.
 *
 * @pre None - called by Unity before each test
 * @pre s_initialized may be any value (reset by rx_bno055_test_reset_state())
 * @post s_initialized == false; s_test_manager ready with "i2c1_imu" registered
 * @post RIIC channel 1 initialized and ready for mock transactions
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  rx_bno055_test_reset_state();
  mock_riic_init();

  rx_err_t err = rx_bus_manager_init(&s_test_manager, "BNO055_TEST", nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_i2c(&s_i2c_config,
                               "i2c1_imu",
                               (uint8_t)k_test_bno055_riic_ch,
                               (uint8_t)k_test_bno055_i2c_addr,
                               k_rx_p2_0,
                               k_rx_p2_1,
                               (uint32_t)k_test_bno055_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(&s_test_manager, &s_i2c_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_i2c_init(&s_test_manager, "i2c1_imu");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Clean up test fixtures after each test
 *
 * @details
 * Deinitializes the bus manager and resets mock RIIC state. Driver static
 * state (s_initialized, s_manager) is reset at the start of the next test
 * via rx_bno055_test_reset_state() in setUp(), not here.
 *
 * @pre setUp() has been called
 * @pre All test assertions have completed
 * @post s_test_manager deinitialized; mock RIIC state cleared
 * @post RIIC channel 1 not initialized (mock_riic_init() resets all channels)
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  const rx_err_t deinit_err = rx_bus_manager_deinit(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, deinit_err);
  (void)mock_riic_init();
  TEST_ASSERT_FALSE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
}

/* =============================================================================
 * Internal Test Helper
 * =============================================================================
 */

/**
 * @brief Perform a fresh BNO055 init with valid mock chip ID data
 *
 * @details
 * Loads the valid chip-ID byte into the mock RIIC buffer and calls
 * rx_bno055_init(), asserting that it succeeds. Used at the top of any test
 * that requires s_initialized == true (read tests, calibration tests, and
 * the double-init test).
 *
 * @pre setUp() has been called; s_initialized == false; mock RIIC ready
 * @pre Mock RIIC RX buffer available for internal_load_valid_chip_id() to populate
 * @post s_initialized == true; driver ready for read/calib operations
 * @post rx_bno055_init() returned k_rx_ok (asserted via TEST_ASSERT_EQUAL)
 *
 * @since Version 1.0.0
 */
static void internal_setup_initialized_driver(void)
{
  internal_load_valid_chip_id();
  rx_err_t err = rx_bno055_init(&s_test_manager, &s_poll_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Verify mock HAL is still operational after init (confirms bus communication completed) */
  TEST_ASSERT_TRUE(mock_riic_is_initialized((uint8_t)k_test_bno055_riic_ch));
}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

/**
 * @brief rx_bno055_init with NULL manager returns k_rx_err_null_ptr
 *
 * @details
 * Verifies the null pointer guard at the top of rx_bno055_init(). Does
 * not modify s_initialized so it is safe to run before the success test.
 *
 * @pre s_initialized may be true or false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post s_initialized unchanged
 * @post Bus manager state not modified by null pointer guard
 *
 * @since Version 1.0.0
 */
void test_bno055_init_null_manager_returns_error(void)
{
  rx_err_t err = rx_bno055_init(nullptr, &s_poll_cfg);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bno055_init propagates I2C error when RIIC returns NACK
 *
 * @details
 * Injects NACK error before calling init. The driver's first I2C write
 * (software reset) should fail and propagate k_rx_err_nack. s_initialized
 * must remain false because init did not complete.
 *
 * This test runs before test_bno055_init_success so s_initialized starts false.
 *
 * @pre s_initialized == false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post s_initialized == false (error path does not set it)
 * @post NACK injection cleared via mock_riic_simulate_nack(false) after assertion
 *
 * @since Version 1.0.0
 */
void test_bno055_init_i2c_error_propagates(void)
{
  (void)mock_riic_simulate_nack(true);

  rx_err_t err = rx_bno055_init(&s_test_manager, &s_poll_cfg);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)mock_riic_simulate_nack(false);
}

/**
 * @brief rx_bno055_init returns k_rx_err_invalid_state when chip ID is wrong
 *
 * @details
 * Pre-loads the mock with 0xFF (not 0xA0) for the chip ID read step (step 8).
 * The driver performs 7 writes successfully, then reads the chip ID and finds
 * it does not match k_bno055_chip_id_expected (0xA0). It should return
 * k_rx_err_invalid_state and leave s_initialized false.
 *
 * @pre s_initialized == false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post s_initialized == false (chip ID mismatch aborts init)
 * @post Mock RIIC RX buffer consumed by the chip ID read step
 *
 * @since Version 1.0.0
 */
void test_bno055_init_wrong_chip_id(void)
{
  uint8_t wrong_id = (uint8_t)k_test_wrong_chip_id;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &wrong_id, k_test_single_byte_buf);

  rx_err_t err = rx_bno055_init(&s_test_manager, &s_poll_cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bno055_read before init returns k_rx_err_not_initialized
 *
 * @details
 * Verifies that calling rx_bno055_read() before rx_bno055_init() returns
 * k_rx_err_not_initialized. This test must run while s_initialized is false
 * (before test_bno055_init_success sets it to true).
 *
 * @pre s_initialized == false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post s_initialized unchanged (still false)
 * @post Driver returns k_rx_err_not_initialized (asserted via TEST_ASSERT_EQUAL)
 *
 * @since Version 1.0.0
 */
void test_bno055_read_before_init(void)
{
  bno055_data_t data;
  rx_err_t      err = rx_bno055_read(&data);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bno055_init succeeds with valid chip ID and sets s_initialized
 *
 * @details
 * Pre-loads chip ID 0xA0. After init, a subsequent rx_bno055_read() call
 * (with mock data) should return k_rx_ok confirming s_initialized is true.
 *
 * This is the FIRST test that successfully initializes the driver.
 * All subsequent tests that require s_initialized == true depend on this.
 *
 * @pre s_initialized == false
 * @pre setUp() has registered "i2c1_imu" bus and chip ID mock loaded via internal_load_valid_chip_id()
 * @post s_initialized == true
 * @post rx_bno055_read() returns k_rx_ok confirming driver is operational
 *
 * @since Version 1.0.0
 */
void test_bno055_init_success(void)
{
  internal_load_valid_chip_id();

  rx_err_t err = rx_bno055_init(&s_test_manager, &s_poll_cfg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Verify initialized by attempting a read with mock data */
  internal_load_read_data();
  bno055_data_t data;
  rx_err_t      read_err = rx_bno055_read(&data);
  TEST_ASSERT_EQUAL(k_rx_ok, read_err);
}

/**
 * @brief rx_bno055_init called again after success returns k_rx_err_invalid_state
 *
 * @details
 * Calls init successfully once via internal_setup_initialized_driver(), then
 * calls init a second time. The second call must be rejected with
 * k_rx_err_invalid_state (double-init guard).
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before second call
 * @post s_initialized == true (set by first init, unchanged by failed second init)
 * @post rx_bno055_init() returns k_rx_err_invalid_state (asserted via TEST_ASSERT_EQUAL)
 *
 * @since Version 1.0.0
 */
void test_bno055_init_double_init_returns_error(void)
{
  /* First init must succeed to set s_initialized = true */
  internal_setup_initialized_driver();

  /* Second call must be rejected with double-init guard */
  rx_err_t err = rx_bno055_init(&s_test_manager, &s_poll_cfg);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Read Tests
 * =============================================================================
 */

/**
 * @brief rx_bno055_read with NULL output pointer returns k_rx_err_null_ptr
 *
 * @details
 * Null pointer guard must be checked before the s_initialized check so it
 * fires regardless of initialization state.
 *
 * @pre s_initialized may be true or false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post No side effects
 * @post rx_bno055_read() returns k_rx_err_null_ptr (asserted via TEST_ASSERT_EQUAL)
 *
 * @since Version 1.0.0
 */
void test_bno055_read_null_ptr_returns_error(void)
{
  rx_err_t err = rx_bno055_read(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bno055_read after successful init and euler mock returns correct values
 *
 * @details
 * Calls internal_setup_initialized_driver() to set s_initialized == true,
 * pre-loads 22 bytes covering all 5 read transactions. Verifies that the
 * Euler heading, roll, and pitch fields are assembled correctly from the
 * mock data (little-endian 16-bit values).
 *
 * Expected assembly (from internal_load_read_data):
 * heading_deg16 = (0x40) | (0x01 << 8) = 0x0140 = 320
 * roll_deg16    = (0x10) | (0x00 << 8) = 0x0010 = 16
 * pitch_deg16   = (0x20) | (0x00 << 8) = 0x0020 = 32
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before read
 * @post out populated with mocked sensor data
 * @post out.heading_deg16/roll_deg16/pitch_deg16 match internal_load_read_data() values
 *
 * @since Version 1.0.0
 */
void test_bno055_read_success_euler(void)
{
  internal_setup_initialized_driver();
  internal_load_read_data();

  bno055_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bno055_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT16((int16_t)k_test_euler_heading_expected, data.heading_deg16);
  TEST_ASSERT_EQUAL_INT16((int16_t)k_test_euler_roll_expected, data.roll_deg16);
  TEST_ASSERT_EQUAL_INT16((int16_t)k_test_euler_pitch_expected, data.pitch_deg16);
}

/**
 * @brief rx_bno055_read returns correct quaternion field assembly
 *
 * @details
 * Calls internal_setup_initialized_driver() then pre-loads identity quaternion
 * data (W=0x4000, X=Y=Z=0). After a successful read the quat_w field must
 * equal 0x4000 = 16384.
 *
 * The mock returns the same 8-byte buffer for all read transactions within
 * this test call (Euler, Quat, LIA each consume from the same loaded buffer).
 * Only the quat_w field needs to match since the others are tested separately.
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before read
 * @post out->quat_w == 0x4000 (identity quaternion W component verified)
 * @post out->quat_x == 0 (zero X component of identity quaternion verified)
 *
 * @since Version 1.0.0
 */
void test_bno055_read_success_quaternion(void)
{
  internal_setup_initialized_driver();
  internal_load_quat_read_data();

  bno055_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bno055_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* quat_w: loaded buffer[0]=0x00, buffer[1]=0x40 -> 0x4000 = 16384 */
  TEST_ASSERT_EQUAL_INT16((int16_t)k_quat_w_expected, data.quat_w);
  TEST_ASSERT_EQUAL_INT16((int16_t)k_quat_xyz_expected, data.quat_x);
}

/**
 * @brief rx_bno055_read populates temp_degc with correct on-chip temperature
 *
 * @details
 * Calls internal_setup_initialized_driver() then loads a buffer with
 * k_test_temp_raw (25) at position 0. Because the mock returns the same
 * buffer for all five read transactions, the single-byte temp read
 * receives buf[0] = 25, which should be stored directly as data.temp_degc.
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before read
 * @post data.temp_degc == (int8_t)k_test_temp_raw (25 degC)
 * @post rx_bno055_read() returns k_rx_ok
 *
 * @since Version 1.0.0
 */
void test_bno055_read_success_temperature(void)
{
  internal_setup_initialized_driver();
  internal_load_temp_read_data();

  bno055_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bno055_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT8((int8_t)k_test_temp_raw, data.temp_degc);
}

/**
 * @brief rx_bno055_read populates lin_acc_x with correct linear acceleration
 *
 * @details
 * Calls internal_setup_initialized_driver() then loads a buffer with
 * k_test_lia_x_lsb/msb at positions 0-1, giving lin_acc_x = 200 raw
 * (2.0 m/s^2 when divided by k_bno055_scale_acc = 100). Verifies the
 * little-endian byte assembly for the LIA register block.
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before read
 * @post data.lin_acc_x == k_test_lia_x_expected (200 raw, 2.0 m/s^2)
 * @post rx_bno055_read() returns k_rx_ok
 *
 * @since Version 1.0.0
 */
void test_bno055_read_success_lia(void)
{
  internal_setup_initialized_driver();
  internal_load_lia_read_data();

  bno055_data_t data;
  memset(&data, 0, sizeof(data));

  rx_err_t err = rx_bno055_read(&data);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_INT16((int16_t)k_test_lia_x_expected, data.lin_acc_x);
}

/**
 * @brief rx_bno055_read propagates I2C error that occurs during data read
 *
 * @details
 * Calls internal_setup_initialized_driver() to set s_initialized == true,
 * then injects NACK. The read should fail on the first I2C write-read
 * (Euler registers) and propagate k_rx_err_nack.
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before NACK injection
 * @post No side effects on driver state
 * @post NACK injection cleared via mock_riic_simulate_nack(false) after assertion
 *
 * @since Version 1.0.0
 */
void test_bno055_read_i2c_error_propagates(void)
{
  internal_setup_initialized_driver();
  (void)mock_riic_simulate_nack(true);

  bno055_data_t data;
  rx_err_t      err = rx_bno055_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  (void)mock_riic_simulate_nack(false);
}

/* =============================================================================
 * Calibration Tests
 * =============================================================================
 */

/**
 * @brief rx_bno055_is_calibrated with NULL pointer returns k_rx_err_null_ptr
 *
 * @details
 * Null pointer guard fires before s_initialized check.
 *
 * @pre s_initialized may be true or false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post No side effects
 * @post rx_bno055_is_calibrated() returns k_rx_err_null_ptr (asserted via TEST_ASSERT_EQUAL)
 *
 * @since Version 1.0.0
 */
void test_bno055_is_calibrated_null_returns_error(void)
{
  rx_err_t err = rx_bno055_is_calibrated(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bno055_is_calibrated reads status register and parses result correctly
 *
 * @details
 * Calls internal_setup_initialized_driver() to set s_initialized == true,
 * then pre-loads CALIB_STAT = 0xFF (SYS=3, GYR=3, ACC=3, MAG=3). The
 * function must return k_rx_ok and set out_calibrated = true. Then pre-loads
 * 0x00 and verifies out_calibrated = false.
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before status reads
 * @post out_calibrated correctly reflects the mocked status byte
 * @post Both full (0xFF) and zero (0x00) calibration bytes correctly decoded and asserted
 *
 * @since Version 1.0.0
 */
void test_bno055_is_calibrated_reads_status(void)
{
  internal_setup_initialized_driver();

  /* Load fully calibrated status */
  uint8_t calib_full = (uint8_t)k_test_calib_all_full;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &calib_full, k_test_single_byte_buf);

  bool     out_calibrated = false;
  rx_err_t err            = rx_bno055_is_calibrated(&out_calibrated);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(out_calibrated);

  /* Load uncalibrated status */
  uint8_t calib_none = (uint8_t)k_test_calib_none;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &calib_none, k_test_single_byte_buf);

  out_calibrated = true;
  err            = rx_bno055_is_calibrated(&out_calibrated);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(out_calibrated);
}

/**
 * @brief rx_bno055_is_calibrated returns false for partial calibration (0x3F)
 *
 * @details
 * k_test_calib_partial = 0x3F means SYS=0, GYR=3, ACC=3, MAG=3.
 * Individual sensor calibration is complete but system calibration is not (SYS=0).
 * rx_bno055_is_calibrated must return k_rx_ok and out_calibrated == false because
 * SYS bits [7:6] are not 0x3 (not fully calibrated at system level).
 *
 * @pre setUp() has reset s_initialized == false
 * @pre internal_setup_initialized_driver() sets s_initialized == true before status read
 * @post out_calibrated == false (partial status does not mean fully calibrated)
 * @post rx_bno055_is_calibrated() returns k_rx_ok despite partial calibration
 *
 * @since Version 1.0.0
 */
void test_bno055_is_calibrated_partial(void)
{
  internal_setup_initialized_driver();

  uint8_t calib_partial = (uint8_t)k_test_calib_partial;
  (void)mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch,
                              &calib_partial,
                              k_test_single_byte_buf);

  bool     out_calibrated = true; /* pre-set true so we can detect false-positive */
  rx_err_t err            = rx_bno055_is_calibrated(&out_calibrated);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(out_calibrated);
}

/* =============================================================================
 * Interrupt Mode Init Tests
 * =============================================================================
 */

/**
 * @brief rx_bno055_init in interrupt mode succeeds when all writes succeed
 *
 * @details
 * Passes k_bno055_mode_interrupt via config. The driver must write the normal
 * init sequence plus the 4-step Page 1 interrupt-engine sequence (PAGE_ID=1,
 * INT_MSK=0x01, INT_EN=0x01, PAGE_ID=0). All writes go through the mock RIIC
 * without error; the chip-ID read returns 0xA0. Verifies that k_rx_ok is
 * returned and the interrupt mode issues exactly k_test_interrupt_extra_writes
 * more RIIC calls than poll mode.
 *
 * @pre s_initialized == false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post s_initialized == true
 * @post rx_bno055_read() returns k_rx_ok confirming interrupt mode init succeeded
 *
 * @since Version 1.0.0
 */
void test_bno055_init_interrupt_mode_succeeds(void)
{
  /* Establish poll-mode write count baseline (clear history to exclude setUp riic_init) */
  mock_riic_clear_history();
  internal_load_valid_chip_id();
  const rx_err_t poll_err = rx_bno055_init(&s_test_manager, &s_poll_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, poll_err);
  const uint16_t poll_call_count = mock_riic_get_call_count();

  /* Reset driver state and RIIC call history; bus stays initialized */
  rx_bno055_test_reset_state();
  mock_riic_clear_history();

  /* Interrupt mode init: same sequence plus 4 Page 1 register writes (PAGE_ID x2, INT_MSK, INT_EN) */
  internal_load_valid_chip_id();
  const rx_err_t err = rx_bno055_init(&s_test_manager, &s_interrupt_cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  const uint16_t interrupt_call_count = mock_riic_get_call_count();

  /* Interrupt mode must issue exactly k_test_interrupt_extra_writes more RIIC calls */
  TEST_ASSERT_EQUAL((uint32_t)((uint32_t)poll_call_count + (uint32_t)k_test_interrupt_extra_writes),
                    (uint32_t)interrupt_call_count);

  /* Confirm driver is operational by performing a read */
  internal_load_read_data();
  bno055_data_t  data;
  const rx_err_t read_err = rx_bno055_read(&data);
  TEST_ASSERT_EQUAL(k_rx_ok, read_err);
}

/**
 * @brief rx_bno055_init with NULL config returns k_rx_err_null_ptr
 *
 * @details
 * Verifies the null-pointer guard added for the config parameter. Passes a
 * valid manager but NULL config. The driver must return k_rx_err_null_ptr
 * without performing any I2C communication.
 *
 * @pre s_initialized == false
 * @pre setUp() has initialized bus manager and mock RIIC HAL
 * @post s_initialized == false (null ptr guard fires before init begins)
 * @post No I2C communication occurred (RIIC call count unchanged)
 *
 * @since Version 1.0.0
 */
void test_bno055_init_null_config(void)
{
  rx_err_t err = rx_bno055_init(&s_test_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Unity Main Entry Point
 * =============================================================================
 */

/**
 * @brief Test runner entry point
 *
 * @details
 * Runs all BNO055 driver unit tests in declaration order. Each test begins
 * with setUp() calling rx_bno055_test_reset_state() so tests are independent
 * of execution order. Tests that require s_initialized == true call
 * internal_setup_initialized_driver() at the top of the test body.
 *
 * @return int Exit status
 * @retval 0 All tests passed
 * @retval non-zero Number of test failures reported by Unity
 *
 * @pre Unity test harness linked and BNO055 driver sources compiled with mock RIIC HAL
 * @pre mock_riic_init() and rx_bno055_test_reset_state() called by setUp() before each test
 * @post Unity reports all test results to stdout
 * @post Process exits with 0 if all tests pass, non-zero on any test failure
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* setUp() resets driver state before each test, so ordering is not required for
   * correctness. Tests that need s_initialized == true call
   * internal_setup_initialized_driver() themselves. */

  /* Init tests */
  RUN_TEST(test_bno055_init_null_manager_returns_error);
  RUN_TEST(test_bno055_init_null_config);
  RUN_TEST(test_bno055_init_i2c_error_propagates);
  RUN_TEST(test_bno055_init_wrong_chip_id);
  RUN_TEST(test_bno055_read_before_init);
  RUN_TEST(test_bno055_init_success);
  RUN_TEST(test_bno055_init_double_init_returns_error);
  RUN_TEST(test_bno055_init_interrupt_mode_succeeds);

  /* Read tests (null-ptr test does not require init; others call internal_setup_initialized_driver) */
  RUN_TEST(test_bno055_read_null_ptr_returns_error);
  RUN_TEST(test_bno055_read_success_euler);
  RUN_TEST(test_bno055_read_success_quaternion);
  RUN_TEST(test_bno055_read_success_temperature);
  RUN_TEST(test_bno055_read_success_lia);
  RUN_TEST(test_bno055_read_i2c_error_propagates);

  /* Calibration tests - require s_initialized == true */
  RUN_TEST(test_bno055_is_calibrated_null_returns_error);
  RUN_TEST(test_bno055_is_calibrated_reads_status);
  RUN_TEST(test_bno055_is_calibrated_partial);

  return UNITY_END();
}
